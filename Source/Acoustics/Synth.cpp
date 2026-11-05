#include "Synth.h"
#include "../DSP/FFT.h"
#include <map>
#include <mutex>

namespace srr
{
namespace
{
    const RealFFT& fftFor (int n)
    {
        static std::mutex lock;
        static std::map<int, std::unique_ptr<RealFFT>> cache;
        std::lock_guard<std::mutex> g (lock);
        auto& p = cache[n];
        if (p == nullptr) p = std::make_unique<RealFFT> (n);
        return *p;
    }

    struct BinBands { int lo = 0, hi = 0; float frac = 0; };

    BinBands bandsForFrequency (float f)
    {
        BinBands r;
        if (f <= kBandCentres[0]) { r.lo = r.hi = 0; return r; }
        if (f >= kBandCentres[kNumBands - 1]) { r.lo = r.hi = kNumBands - 1; return r; }
        auto x = std::log2 (f / kBandCentres[0]);
        r.lo = std::clamp ((int) std::floor (x), 0, kNumBands - 1);
        r.hi = std::min (r.lo + 1, kNumBands - 1);
        r.frac = x - (float) r.lo;
        return r;
    }

    std::array<Vec3, 16> lateDirections()
    {
        std::array<Vec3, 16> d;
        const float golden = 2.39996323f;
        for (int i = 0; i < 16; ++i)
        {
            auto z = 1.0f - (2.0f * (float) i + 1.0f) / 16.0f;
            auto r = std::sqrt (std::max (0.0f, 1.0f - z * z));
            auto phi = (float) i * golden + 0.3f;
            d[(size_t) i] = { r * std::cos (phi), r * std::sin (phi), z };
        }
        return d;
    }

    float envAt (const std::vector<float>& e, float frame)
    {
        if (e.empty()) return 0;
        auto i0 = (int) std::floor (frame);
        if (i0 < 0) return 0;
        if (i0 >= (int) e.size() - 1) return i0 < (int) e.size() ? e[(size_t) i0] : 0.0f;
        auto fr = frame - (float) i0;
        return e[(size_t) i0] * (1 - fr) + e[(size_t) i0 + 1] * fr;
    }
}

int irLengthFor (const Scene& scene, double sampleRate, int multiple)
{
    auto n = (int) std::ceil (scene.lengthSec * sampleRate);
    n = std::max (n, multiple);
    return ((n + multiple - 1) / multiple) * multiple;
}

//==============================================================================
void renderEarly (const Scene& scene, const std::vector<Capsule>& rig, const std::vector<float>& yawsDeg,
                  const EarlyOptions& opt, juce::AudioBuffer<float>& out, float gain)
{
    const int C = std::min ((int) rig.size(), out.getNumChannels());
    if (C == 0 || yawsDeg.empty() || scene.arrivals.empty()) return;

    const double fs = opt.sampleRate;
    const int pad = 512;

    float maxDelay = 0;
    for (auto& a : scene.arrivals)
    {
        if (! opt.includeDirect && a.isDirect) continue;
        if (a.delay * fs >= opt.maxSamples) continue;
        maxDelay = std::max (maxDelay, a.delay);
    }

    const int extent = std::min ({ out.getNumSamples(), opt.maxSamples, (int) (maxDelay * fs) + 640 });
    if (extent <= 0) return;

    const int nfft = nextPow2 (extent + pad + 1024);
    const auto& fft = fftFor (nfft);
    const int nb = nfft / 2;

    // band weights per bin (triangular in log frequency, sum to one)
    std::vector<std::array<float, kNumBands>> w ((size_t) nb + 1);
    for (int k = 0; k <= nb; ++k)
    {
        auto f = (float) (k * fs / nfft);
        auto bb = bandsForFrequency (std::max (f, 1.0f));
        w[(size_t) k].fill (0.0f);
        w[(size_t) k][(size_t) bb.lo] += 1.0f - bb.frac;
        w[(size_t) k][(size_t) bb.hi] += bb.frac;
    }

    AlignedBuffer bands ((size_t) (kNumBands * nfft)), spec ((size_t) nfft), acc ((size_t) nfft), work ((size_t) nfft), tmp ((size_t) nfft);
    const float wYaw = gain / (float) yawsDeg.size();
    std::vector<float> yawRad;
    for (auto y : yawsDeg) yawRad.push_back (degToRad (y));

    for (int c = 0; c < C; ++c)
    {
        const auto& cap = rig[(size_t) c];
        if (cap.silent) continue;
        bands.clear();
        bool any = false;

        for (auto& a : scene.arrivals)
        {
            if (! opt.includeDirect && a.isDirect) continue;
            if (a.delay * fs >= opt.maxSamples) continue;

            for (auto yr : yawRad)
            {
                auto dm = a.dir.rotatedZ (-yr);
                auto t = (a.delay - dm.dot (cap.pos) / kSpeedOfSound) * (float) fs + (float) pad;
                auto i0 = (int) std::floor (t);
                if (i0 < 0 || i0 + 1 >= nfft) continue;
                auto fr = t - (float) i0;
                for (int b = 0; b < kNumBands; ++b)
                {
                    auto g = a.amp[(size_t) b] * capsuleGain (cap, dm, b) * wYaw;
                    bands[(size_t) (b * nfft + i0)] += g * (1.0f - fr);
                    bands[(size_t) (b * nfft + i0 + 1)] += g * fr;
                }
                any = true;
            }
        }
        if (! any) continue;

        acc.clear();
        for (int b = 0; b < kNumBands; ++b)
        {
            fft.forwardOrdered (bands.get() + b * nfft, spec.get(), work.get());
            acc[0] += spec[0] * w[0][(size_t) b];
            acc[1] += spec[1] * w[(size_t) nb][(size_t) b];
            for (int k = 1; k < nb; ++k)
            {
                auto wk = w[(size_t) k][(size_t) b];
                acc[(size_t) (2 * k)]     += spec[(size_t) (2 * k)] * wk;
                acc[(size_t) (2 * k + 1)] += spec[(size_t) (2 * k + 1)] * wk;
            }
        }
        fft.inverseOrdered (acc.get(), tmp.get(), work.get());

        auto* dst = out.getWritePointer (c);
        const float norm = 1.0f / (float) nfft;
        for (int n = 0; n < extent; ++n)
            dst[n] += tmp[(size_t) (pad + n)] * norm;
    }
}

//==============================================================================
void renderLate (const Scene& scene, const std::vector<Capsule>& rig, float yawDeg, double fs,
                 uint64_t seed, juce::AudioBuffer<float>& out, float gain)
{
    const int C = std::min ((int) rig.size(), out.getNumChannels());
    const int L = out.getNumSamples();
    if (C == 0 || L <= 0) return;

    constexpr int N = 512, H = 256, NB = N / 2, K = 16;
    const auto& fft = fftFor (N);
    static const auto dirs = lateDirections();

    // window (sqrt periodic Hann: OLA of w^2 at 50 % overlap == 1)
    std::array<float, N> win;
    for (int n = 0; n < N; ++n) win[(size_t) n] = std::sqrt (0.5f - 0.5f * std::cos (2.0f * kPi * (float) n / (float) N));

    // per-bin band interpolation
    std::array<BinBands, NB> bins;
    for (int k = 0; k < NB; ++k) bins[(size_t) k] = bandsForFrequency (std::max (1.0f, (float) (k * fs / N)));

    // capsule response per (channel, direction, bin) incl. propagation delay across the array
    const float yawRad = degToRad (yawDeg);
    std::vector<float> hre ((size_t) (C * K * NB)), him ((size_t) (C * K * NB));
    for (int c = 0; c < C; ++c)
        for (int q = 0; q < K; ++q)
        {
            auto dm = dirs[(size_t) q].rotatedZ (-yawRad);
            auto tau = -dm.dot (rig[(size_t) c].pos) / kSpeedOfSound;
            std::array<float, kNumBands> g;
            for (int b = 0; b < kNumBands; ++b) g[(size_t) b] = capsuleGain (rig[(size_t) c], dm, b);
            for (int k = 0; k < NB; ++k)
            {
                auto& bb = bins[(size_t) k];
                auto amp = g[(size_t) bb.lo] * (1 - bb.frac) + g[(size_t) bb.hi] * bb.frac;
                auto ph = -2.0f * kPi * (float) (k * fs / N) * tau;
                auto idx = (size_t) ((c * K + q) * NB + k);
                hre[idx] = amp * std::cos (ph);
                him[idx] = amp * std::sin (ph);
            }
        }

    // directional weights of the late components
    const int D = (int) scene.directional.size();
    std::vector<float> dw ((size_t) (D * K));
    for (int d = 0; d < D; ++d)
    {
        float sum = 0;
        for (int q = 0; q < K; ++q)
        {
            auto v = std::exp (3.0f * (dirs[(size_t) q].dot (scene.directional[(size_t) d].dir) - 1.0f));
            dw[(size_t) (d * K + q)] = v; sum += v;
        }
        for (int q = 0; q < K; ++q) dw[(size_t) (d * K + q)] /= std::max (sum, 1.0e-9f);
    }

    AlignedBuffer spec (N), frame (N), work (N);
    std::vector<float> sq ((size_t) (K * NB)), rr ((size_t) (K * NB)), ri ((size_t) (K * NB));
    const float randScale = std::sqrt (1.5f);
    const int frames = L / H + 2;

    for (int f = 0; f < frames; ++f)
    {
        const int start = f * H - N / 2;
        const float tCentre = (float) (f * H) / (float) fs;
        const float ef = tCentre * Scene::envRate;

        std::array<float, kNumBands> eDiff;
        std::vector<std::array<float, kNumBands>> eDir ((size_t) D);
        float maxE = 0;
        for (int b = 0; b < kNumBands; ++b)
        {
            eDiff[(size_t) b] = envAt (scene.diffuse[(size_t) b], ef);
            maxE = std::max (maxE, eDiff[(size_t) b]);
            for (int d = 0; d < D; ++d)
            {
                eDir[(size_t) d][(size_t) b] = envAt (scene.directional[(size_t) d].energy[(size_t) b], ef);
                maxE = std::max (maxE, eDir[(size_t) d][(size_t) b]);
            }
        }
        if (maxE <= 1.0e-14f) continue;

        // per-direction, per-bin standard deviation
        const float toSample = 1.0f / ((float) fs * (float) N);
        for (int q = 0; q < K; ++q)
            for (int k = 1; k < NB; ++k)
            {
                auto& bb = bins[(size_t) k];
                auto interp = [&] (const std::array<float, kNumBands>& e)
                {
                    // geometric (dB-linear) interpolation keeps decay rates smooth across bands
                    const float a = std::max (e[(size_t) bb.lo], 1.0e-24f), b = std::max (e[(size_t) bb.hi], 1.0e-24f);
                    if (bb.frac <= 0.0f) return a;
                    return a * std::pow (b / a, bb.frac);
                };
                float v = interp (eDiff) / (float) K;
                for (int d = 0; d < D; ++d) v += interp (eDir[(size_t) d]) * dw[(size_t) (d * K + q)];
                sq[(size_t) (q * NB + k)] = std::sqrt (v * toSample) * gain;
            }

        FastRandom rng (hashCombine (seed, (uint64_t) f * 0x51ED27ull + 7));
        for (size_t i = 0; i < rr.size(); ++i) { rr[i] = rng.bipolar() * randScale; ri[i] = rng.bipolar() * randScale; }

        for (int c = 0; c < C; ++c)
        {
            if (rig[(size_t) c].silent) continue;
            spec.clear();
            for (int q = 0; q < K; ++q)
            {
                const auto base = (size_t) ((c * K + q) * NB);
                const auto qb = (size_t) (q * NB);
                for (int k = 1; k < NB; ++k)
                {
                    auto s = sq[qb + (size_t) k];
                    auto a = rr[qb + (size_t) k] * s, bI = ri[qb + (size_t) k] * s;
                    auto hr = hre[base + (size_t) k], hi = him[base + (size_t) k];
                    spec[(size_t) (2 * k)]     += hr * a - hi * bI;
                    spec[(size_t) (2 * k + 1)] += hr * bI + hi * a;
                }
            }
            fft.inverseOrdered (spec.get(), frame.get(), work.get());

            auto* dst = out.getWritePointer (c);
            for (int n = 0; n < N; ++n)
            {
                auto idx = start + n;
                if (idx < 0 || idx >= L) continue;
                dst[idx] += frame[(size_t) n] * win[(size_t) n];
            }
        }
    }
}

} // namespace srr
