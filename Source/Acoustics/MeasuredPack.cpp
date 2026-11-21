#include "MeasuredPack.h"

namespace srr
{
juce::File MeasuredIRCache::irFileFor (const Location& loc, int source, int mic, uint32_t doors)
{
    if (source < 0 || source >= (int) loc.sources.size() || mic < 0 || mic >= (int) loc.mics.size()) return {};
    const auto& spot = loc.sources[(size_t) source];
    if (spot.derive)   // derived spots use the recording of their base spot
        if (auto b = loc.findSource (spot.derive->from); b >= 0 && b != source)
            return irFileFor (loc, b, mic, doors);
    auto dir = loc.packDir.getChildFile (loc.irFolder);
    auto base = spot.id + "__" + loc.mics[(size_t) mic].id + "__";
    auto exact = dir.getChildFile (base + loc.doorStateKey (doors) + ".wav");
    if (exact.existsAsFile()) return exact;

    // nearest door state (Hamming distance on the door key)
    auto want = loc.doorStateKey (doors);
    juce::File best;
    int bestDist = 1 << 20;
    for (const auto& e : juce::RangedDirectoryIterator (dir, false, base + "*.wav", juce::File::findFiles))
    {
        auto key = e.getFile().getFileNameWithoutExtension().fromLastOccurrenceOf ("__", false, false);
        int dist = 0;
        for (int i = 0; i < std::max (key.length(), want.length()); ++i)
            dist += (i < key.length() && i < want.length() && key[i] == want[i]) ? 0 : 1;
        if (dist < bestDist) { bestDist = dist; best = e.getFile(); }
    }
    return best;
}

//==============================================================================
void applyDerive (const SourceSpot::Derive& d, const std::vector<RigChannel>& rig, juce::AudioBuffer<float>& ir, double fs)
{
    const int C = ir.getNumChannels(), N = ir.getNumSamples();
    if (d.mirror && C > 1)
    {
        // swap every rig channel with its left/right mirror image (azimuth -> -azimuth)
        juce::AudioBuffer<float> src (ir);
        for (int c = 0; c < C && c < (int) rig.size(); ++c)
        {
            int best = c;
            float bestErr = 1.0e9f;
            for (int k = 0; k < C && k < (int) rig.size(); ++k)
            {
                const float err = std::abs (wrapDegrees (rig[(size_t) k].azDeg + rig[(size_t) c].azDeg))
                                + std::abs (rig[(size_t) k].elDeg - rig[(size_t) c].elDeg);
                if (err < bestErr) { bestErr = err; best = k; }
            }
            ir.copyFrom (c, 0, src, best, 0, N);
        }
    }
    if (d.delayMs > 0.0f)
    {
        const int shift = std::min (N, (int) std::lround (d.delayMs * 0.001 * fs));
        for (int c = 0; c < C; ++c)
        {
            auto* x = ir.getWritePointer (c);
            std::memmove (x + shift, x, sizeof (float) * (size_t) (N - shift));
            std::fill (x, x + shift, 0.0f);
        }
    }
    if (d.lowpassHz > 20.0f && d.lowpassHz < fs * 0.45)
        for (int c = 0; c < C; ++c)
        {
            juce::IIRFilter lp;
            lp.setCoefficients (juce::IIRCoefficients::makeLowPass (fs, d.lowpassHz, 0.7071));
            lp.processSamples (ir.getWritePointer (c), N);
        }
    if (d.gainDb != 0.0f) ir.applyGain (juce::Decibels::decibelsToGain (d.gainDb));
}

std::shared_ptr<const MeasuredIR> MeasuredIRCache::load (const Location& loc, int source, int mic, uint32_t doors, double fs)
{
    auto file = irFileFor (loc, source, mic, doors);
    if (! file.existsAsFile()) return {};

    const auto& spot = loc.sources[(size_t) source];
    const auto key = file.getFullPathName() + "@" + juce::String (fs) + "#" + spot.id;
    std::lock_guard<std::mutex> g (lock);
    if (auto it = cache.find (key); it != cache.end()) return it->second;

    if (! formatsReady) { formats.registerBasicFormats(); formatsReady = true; }
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) return {};

    const int maxLen = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 4.0));
    juce::AudioBuffer<float> raw ((int) reader->numChannels, maxLen);
    reader->read (&raw, 0, maxLen, 0, true, true);

    auto ir = std::make_shared<MeasuredIR>();
    ir->fileName = file.getFileName();
    ir->exactDoorMatch = file.getFileNameWithoutExtension().endsWith ("__" + loc.doorStateKey (doors));
    ir->derived = spot.derive.has_value();

    if (std::abs (reader->sampleRate - fs) < 1.0)
    {
        ir->rigIR = std::move (raw);
    }
    else
    {
        const double ratio = reader->sampleRate / fs;
        const int outLen = (int) std::ceil (maxLen / ratio);
        ir->rigIR.setSize (raw.getNumChannels(), outLen);
        ir->rigIR.clear();
        for (int c = 0; c < raw.getNumChannels(); ++c)
        {
            juce::WindowedSincInterpolator interp;
            interp.process (ratio, raw.getReadPointer (c), ir->rigIR.getWritePointer (c), outLen, maxLen, 0);
        }
    }

    if (spot.derive) applyDerive (*spot.derive, loc.rig, ir->rigIR, fs);

    // onset and first-arrival direction
    float peak = 0;
    for (int c = 0; c < ir->rigIR.getNumChannels(); ++c)
        peak = std::max (peak, ir->rigIR.getMagnitude (c, 0, ir->rigIR.getNumSamples()));
    int onset = 0;
    for (int n = 0; n < ir->rigIR.getNumSamples() && onset == 0; ++n)
        for (int c = 0; c < ir->rigIR.getNumChannels(); ++c)
            if (std::abs (ir->rigIR.getSample (c, n)) > peak * 0.3f) { onset = std::max (1, n); break; }
    ir->onsetSample = onset;

    const int win = (int) (0.0025 * fs);
    float vx = 0, vy = 0;
    for (int c = 0; c < ir->rigIR.getNumChannels() && c < (int) loc.rig.size(); ++c)
    {
        float e = 0;
        for (int n = onset; n < std::min (ir->rigIR.getNumSamples(), onset + win); ++n)
            e += ir->rigIR.getSample (c, n) * ir->rigIR.getSample (c, n);
        auto az = degToRad (loc.rig[(size_t) c].azDeg);
        vx += e * std::cos (az); vy += e * std::sin (az);
    }
    ir->firstArrivalAzDeg = (vx * vx + vy * vy) > 1.0e-20f ? radToDeg (std::atan2 (vy, vx)) : 0.0f;

    // reverberation time in the 1 kHz octave (Schroeder integral, T20 extrapolated to 60 dB)
    {
        const int n = ir->rigIR.getNumSamples();
        std::vector<float> band ((size_t) n, 0.0f);
        for (int c = 0; c < ir->rigIR.getNumChannels(); ++c)
            juce::FloatVectorOperations::add (band.data(), ir->rigIR.getReadPointer (c), n);
        juce::IIRFilter bp;
        bp.setCoefficients (juce::IIRCoefficients::makeBandPass (fs, 1000.0, 1.41));
        bp.processSamples (band.data(), n);
        std::vector<double> edc ((size_t) n + 1, 0.0);
        for (int i = n - 1; i >= 0; --i) edc[(size_t) i] = edc[(size_t) i + 1] + (double) band[(size_t) i] * band[(size_t) i];
        if (edc[0] > 0)
        {
            int i5 = -1, i25 = -1;
            for (int i = 0; i < n && i25 < 0; ++i)
            {
                const double db = 10.0 * std::log10 (edc[(size_t) i] / edc[0] + 1.0e-30);
                if (i5 < 0 && db < -5.0) i5 = i;
                if (db < -25.0) i25 = i;
            }
            if (i5 >= 0 && i25 > i5) ir->rt60At1k = (float) (3.0 * (i25 - i5) / fs);
        }
    }

    if (cache.size() > 24) cache.clear();
    cache[key] = ir;
    return ir;
}

//==============================================================================
void mapRigToLayout (const juce::AudioBuffer<float>& rigIR, const std::vector<RigChannel>& rig,
                     const juce::AudioChannelSet& layout, float rotationDeg,
                     juce::AudioBuffer<float>& out, float gain, int maxSamples)
{
    const int numOut = layout.size();
    const int R = std::min ((int) rig.size(), rigIR.getNumChannels());
    const int len = std::min ({ rigIR.getNumSamples(), out.getNumSamples(), maxSamples });
    if (R == 0 || numOut == 0 || len <= 0) return;

    struct OutCh { int index; float az; };
    std::vector<OutCh> ring, tops;
    for (int o = 0; o < numOut; ++o)
    {
        auto nd = nominalDirection (layout.getTypeOfChannel (o));
        if (nd.lfe) continue;
        (nd.elDeg > 30.0f ? tops : ring).push_back ({ o, nd.azDeg });
    }
    std::sort (ring.begin(), ring.end(), [] (auto& a, auto& b) { return a.az < b.az; });

    std::vector<float> g ((size_t) numOut);
    auto panHorizontal = [&] (float az, float level)
    {
        if (ring.empty()) return;
        if (ring.size() == 1) { g[(size_t) ring[0].index] += level; return; }
        az = wrapDegrees (az);
        // find the bracketing pair on the circle
        for (size_t i = 0; i < ring.size(); ++i)
        {
            auto a = ring[i], b = ring[(i + 1) % ring.size()];
            float span = wrapDegrees (b.az - a.az);
            if (ring.size() == 2 && span < 0) span += 360.0f;
            if (span <= 0) span += 360.0f;
            float off = wrapDegrees (az - a.az);
            if (off < 0) off += 360.0f;
            if (off <= span)
            {
                auto t = off / span;
                g[(size_t) a.index] += level * std::cos (t * kPi * 0.5f);
                g[(size_t) b.index] += level * std::sin (t * kPi * 0.5f);
                return;
            }
        }
        g[(size_t) ring[0].index] += level;
    };

    for (int m = 0; m < R; ++m)
    {
        std::fill (g.begin(), g.end(), 0.0f);
        auto az = rig[(size_t) m].azDeg + rotationDeg;
        if (rig[(size_t) m].elDeg > 30.0f)
        {
            if (! tops.empty())
            {
                auto best = tops[0];
                for (auto& t : tops)
                    if (std::abs (wrapDegrees (t.az - az)) < std::abs (wrapDegrees (best.az - az))) best = t;
                g[(size_t) best.index] += 1.0f;
            }
            else panHorizontal (az, 0.707f);
        }
        else
        {
            panHorizontal (az, 1.0f);
            for (auto& t : tops)   // light fill of height channels
                if (std::abs (wrapDegrees (t.az - az)) < 100.0f) g[(size_t) t.index] += 0.35f;
        }

        for (int o = 0; o < numOut && o < out.getNumChannels(); ++o)
            if (g[(size_t) o] != 0.0f)
                out.addFrom (o, 0, rigIR, m, 0, len, g[(size_t) o] * gain);
    }
}

void removeDirectSound (juce::AudioBuffer<float>& ir, int onset, double fs)
{
    const int pre = (int) (0.0005 * fs), post = (int) (0.003 * fs), fade = std::max (1, (int) (0.0005 * fs));
    const int a = std::max (0, onset - pre), b = std::min (ir.getNumSamples(), onset + post);
    for (int c = 0; c < ir.getNumChannels(); ++c)
    {
        auto* d = ir.getWritePointer (c);
        for (int n = a; n < b; ++n) d[n] = 0.0f;
        for (int n = 0; n < fade && b + n < ir.getNumSamples(); ++n) d[b + n] *= (float) n / (float) fade;
        for (int n = 0; n < fade && a - 1 - n >= 0; ++n) d[a - 1 - n] *= (float) n / (float) fade;
    }
}


//==============================================================================
namespace
{
    struct Bands3
    {
        // low (< 500 Hz), mid (500 Hz - 4 kHz), high (> 4 kHz)
        static void split (const float* x, int n, double fs, std::vector<float> (&out)[3])
        {
            for (auto& v : out) v.assign (x, x + n);
            juce::IIRFilter a, b1, b2, c;
            a.setCoefficients (juce::IIRCoefficients::makeLowPass (fs, 500.0, 0.7071));
            b1.setCoefficients (juce::IIRCoefficients::makeHighPass (fs, 500.0, 0.7071));
            b2.setCoefficients (juce::IIRCoefficients::makeLowPass (fs, 4000.0, 0.7071));
            c.setCoefficients (juce::IIRCoefficients::makeHighPass (fs, 4000.0, 0.7071));
            a.processSamples (out[0].data(), n);
            b1.processSamples (out[1].data(), n);
            b2.processSamples (out[1].data(), n);
            c.processSamples (out[2].data(), n);
        }
    };

    float rms (const std::vector<float>& v, int a, int b)
    {
        a = std::max (0, a); b = std::min ((int) v.size(), b);
        if (b <= a) return 0.0f;
        double e = 0;
        for (int i = a; i < b; ++i) e += (double) v[(size_t) i] * v[(size_t) i];
        return (float) std::sqrt (e / (b - a));
    }
}

juce::AudioBuffer<float> reshapeMeasured (const juce::AudioBuffer<float>& in, int onset, float rt60,
                                          double fs, float size, float decay, int& newOnset)
{
    size = juce::jlimit (1.0f, 2.0f, size);
    decay = juce::jlimit (1.0f, 4.0f, decay);
    newOnset = onset;
    const int C = in.getNumChannels();
    if ((size <= 1.001f && decay <= 1.001f) || C == 0 || in.getNumSamples() < 64) return in;

    // 1) size: stretch everything after the onset (a bigger room = later, sparser reflections)
    juce::AudioBuffer<float> a;
    if (size > 1.001f)
    {
        const int N = in.getNumSamples(), post = N - onset;
        const int outPost = (int) std::ceil (post * size);
        a.setSize (C, onset + outPost);
        a.clear();
        for (int c = 0; c < C; ++c)
        {
            a.copyFrom (c, 0, in, c, 0, onset);
            juce::WindowedSincInterpolator interp;
            interp.process (1.0 / size, in.getReadPointer (c, onset), a.getWritePointer (c, onset), outPost, post, 0);
        }
    }
    else
    {
        a.makeCopyOf (in);
    }

    const float rt = std::max (0.05f, (rt60 > 0.0f ? rt60 : 0.5f) * size);
    if (decay <= 1.001f) return a;

    // 2) where the recorded decay meets its noise floor (10 ms energy frames over all channels)
    const int N = a.getNumSamples();
    const int frame = std::max (16, (int) (0.01 * fs));
    const int F = N / frame;
    std::vector<double> env ((size_t) std::max (1, F), 0.0);
    for (int f = 0; f < F; ++f)
        for (int c = 0; c < C; ++c)
        {
            auto* x = a.getReadPointer (c, f * frame);
            for (int i = 0; i < frame; ++i) env[(size_t) f] += (double) x[i] * x[i];
        }
    int peakF = 0;
    for (int f = 0; f < F; ++f) if (env[(size_t) f] > env[(size_t) peakF]) peakF = f;
    const int tailFrames = std::max (3, F / 10);
    double floorE = 0;
    for (int f = std::max (0, F - tailFrames); f < F; ++f) floorE += env[(size_t) f];
    floorE /= tailFrames;
    int endF = F - 1, below = 0;
    for (int f = peakF + 1; f < F; ++f)
    {
        below = env[(size_t) f] < floorE * 4.0 ? below + 1 : 0;   // within 6 dB of the floor
        if (below >= 3) { endF = f - 3; break; }
    }
    const int minEnd = onset + (int) (0.06 * fs);
    int usableEnd = std::max (minEnd, endF * frame);
    usableEnd = std::min (usableEnd, N);

    // 3) new length and synthetic continuation of the tail
    const float newRt = rt * decay;
    const int target = std::min ((int) (10.0 * fs), onset + (int) (newRt * 1.1 * fs));
    const int L = std::max (usableEnd, target);
    juce::AudioBuffer<float> out (C, L);
    out.clear();

    const int refLen = std::min (usableEnd - onset, (int) (0.15 * fs));
    const int xfade = std::min (refLen / 2, (int) (0.05 * fs));
    const float bandRt[3] = { rt * 1.1f, rt, rt * 0.7f };
    juce::Random rnd (0x5b0a);

    for (int c = 0; c < C; ++c)
    {
        out.copyFrom (c, 0, a, c, 0, usableEnd);
        if (L <= usableEnd - xfade || refLen < 32) continue;

        // colour of the late recorded decay
        std::vector<float> ref[3];
        Bands3::split (a.getReadPointer (c, usableEnd - refLen), refLen, fs, ref);

        const int synthStart = usableEnd - xfade;
        const int synthLen = L - synthStart;
        std::vector<float> noise ((size_t) synthLen);
        for (auto& v : noise) v = rnd.nextFloat() * 2.0f - 1.0f;
        std::vector<float> nb[3];
        Bands3::split (noise.data(), synthLen, fs, nb);

        auto* y = out.getWritePointer (c);
        for (int b = 0; b < 3; ++b)
        {
            // level at the end of the reference segment, continuing that band's decay
            const float levelMid = rms (ref[b], 0, refLen);
            const float atEnd = levelMid * std::pow (10.0f, -3.0f * (0.5f * refLen / (float) fs) / bandRt[b]);
            const float noiseRms = std::max (1.0e-9f, rms (nb[b], 0, std::min (synthLen, (int) (0.2 * fs))));
            const float g0 = atEnd / noiseRms;
            for (int i = 0; i < synthLen; ++i)
            {
                const float t = (float) (i - xfade) / (float) fs;
                const float e = g0 * std::pow (10.0f, -3.0f * t / bandRt[b]);
                const float fadeIn = i < xfade ? (float) i / (float) xfade : 1.0f;
                y[synthStart + i] += nb[b][(size_t) i] * e * fadeIn;
            }
        }
        for (int i = 0; i < xfade; ++i)   // fade the recording out under the synthetic tail
        {
            const float w = (float) i / (float) xfade;
            const float rec = a.getSample (c, synthStart + i);
            y[synthStart + i] += rec * (1.0f - w) - rec;   // replace 'rec' by 'rec * (1 - w)'
        }
    }

    // 4) slower decay: raise the level over time so -60 dB is reached after newRt instead of rt
    const int start = onset + (int) (0.03 * fs);
    const float k = 3.0f / rt * (1.0f - 1.0f / decay);   // log10 gain per second
    for (int c = 0; c < C; ++c)
    {
        auto* y = out.getWritePointer (c);
        for (int n = start; n < L; ++n)
        {
            const float t = (float) (n - start) / (float) fs;
            const float ramp = std::min (1.0f, t / 0.05f);   // ease in over 50 ms
            y[n] *= std::pow (10.0f, k * t * ramp);
        }
    }

    // short fade at the very end
    const int fadeLen = std::min (L / 20, (int) (0.2 * fs));
    for (int c = 0; c < C; ++c)
        for (int i = 0; i < fadeLen; ++i)
            out.getWritePointer (c)[L - fadeLen + i] *= 1.0f - (float) i / (float) fadeLen;
    return out;
}
} // namespace srr
