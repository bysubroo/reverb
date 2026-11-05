#include "MeasuredPack.h"

namespace srr
{
juce::File MeasuredIRCache::irFileFor (const Location& loc, int source, int mic, uint32_t doors)
{
    if (source < 0 || source >= (int) loc.sources.size() || mic < 0 || mic >= (int) loc.mics.size()) return {};
    auto dir = loc.packDir.getChildFile (loc.irFolder);
    auto base = loc.sources[(size_t) source].id + "__" + loc.mics[(size_t) mic].id + "__";
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

std::shared_ptr<const MeasuredIR> MeasuredIRCache::load (const Location& loc, int source, int mic, uint32_t doors, double fs)
{
    auto file = irFileFor (loc, source, mic, doors);
    if (! file.existsAsFile()) return {};

    const auto key = file.getFullPathName() + "@" + juce::String (fs);
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
} // namespace srr
