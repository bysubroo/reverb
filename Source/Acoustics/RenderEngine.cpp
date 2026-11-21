#include "RenderEngine.h"
#include "Synth.h"

namespace srr
{
namespace
{
    constexpr float kFlatTargetEnergy = 0.5f;

    float bufferEnergyPerChannel (const juce::AudioBuffer<float>& b, int activeChannels)
    {
        double e = 0;
        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            auto* d = b.getReadPointer (c);
            for (int n = 0; n < b.getNumSamples(); ++n) e += (double) d[n] * d[n];
        }
        return (float) (e / std::max (1, activeChannels));
    }

    void fadeOutEnd (juce::AudioBuffer<float>& b, int fadeLen)
    {
        const int n = b.getNumSamples();
        fadeLen = std::min (fadeLen, n);
        for (int c = 0; c < b.getNumChannels(); ++c)
            b.applyGainRamp (c, n - fadeLen, fadeLen, 1.0f, 0.0f);
    }

    int activeOutputs (const juce::AudioChannelSet& layout)
    {
        int n = 0;
        for (int i = 0; i < layout.size(); ++i)
            if (! nominalDirection (layout.getTypeOfChannel (i)).lfe) ++n;
        return std::max (1, n);
    }

    float flatGainFor (float energyPerChannel)
    {
        auto g = std::sqrt (kFlatTargetEnergy / std::max (1.0e-12f, energyPerChannel));
        return juce::jlimit (0.01f, 100.0f, g);
    }

    int maxTailPartsFor (double fs)
    {
        return (int) std::ceil ((kMaxIRSeconds * fs - kHeadLength) / kTailBlock) + 1;
    }

    float azimuthOf (Vec3 d) { return radToDeg (std::atan2 (d.y, d.x)); }
}

std::vector<float> inputAzimuths (const juce::AudioChannelSet& in)
{
    std::vector<float> az;
    if (in.size() <= 1) { az.push_back (0.0f); return az; }
    for (int i = 0; i < in.size(); ++i)
    {
        auto nd = nominalDirection (in.getTypeOfChannel (i));
        az.push_back (nd.lfe ? std::numeric_limits<float>::quiet_NaN() : nd.azDeg);
    }
    return az;
}

//==============================================================================
RenderEngine::RenderEngine (MatrixConvolver& c, juce::CriticalSection& l)
    : juce::Thread ("SubRoo Rooms renderer"), conv (c), convLock (l)
{
}

RenderEngine::~RenderEngine()
{
    stopRendering();
}

void RenderEngine::setRequestProvider (std::function<RenderRequest()> p)
{
    provider = std::move (p);
}

void RenderEngine::startRendering()
{
    if (! isThreadRunning()) startThread (juce::Thread::Priority::normal);
}

void RenderEngine::stopRendering()
{
    stopThread (4000);
}

RenderInfo RenderEngine::getInfo() const
{
    const juce::SpinLock::ScopedLockType sl (infoLock);
    return info;
}

void RenderEngine::run()
{
    while (! threadShouldExit())
    {
        {
            const juce::ScopedLock sl (convLock);
            conv.collectGarbage();
        }

        const auto want = dirty.load();
        if (want == rendered.load() || ! provider)
        {
            wait (12);
            continue;
        }

        auto rq = provider();
        if (! rq.valid || rq.location == nullptr)
        {
            rendered = want;
            wait (20);
            continue;
        }

        busy = true;
        RenderInfo newInfo;
        auto t0 = juce::Time::getMillisecondCounterHiRes();
        std::unique_ptr<IRSet> set;
        try { set = render (rq, newInfo); }
        catch (...) { set.reset(); }
        newInfo.renderMs = (float) (juce::Time::getMillisecondCounterHiRes() - t0);

        if (set != nullptr && ! threadShouldExit())
        {
            set->serial = ++serialCounter;
            newInfo.serial = set->serial;
            const juce::ScopedLock sl (convLock);
            if (set->numOutputs == conv.getNumOutputs())
                conv.submit (std::move (set));
        }

        {
            const juce::SpinLock::ScopedLockType sl (infoLock);
            info = std::move (newInfo);
        }
        rendered = want;
        busy = false;
    }
}

std::shared_ptr<const Scene> RenderEngine::sceneFor (const RenderRequest& rq)
{
    uint64_t key = 1469598103934665603ull;
    key = hashCombine (key, (uint64_t) std::hash<std::string>{} (rq.location->id.toStdString()));
    key = hashCombine (key, (uint64_t) rq.source);
    key = hashCombine (key, (uint64_t) rq.mic);
    key = hashCombine (key, (uint64_t) rq.doors);
    key = hashCombine (key, (uint64_t) std::lround (rq.sizeScale * 1000.0f));
    key = hashCombine (key, (uint64_t) std::lround (rq.decayScale * 1000.0f));
    key = hashCombine (key, (uint64_t) (uintptr_t) rq.location.get());

    for (auto& e : sceneCache)
        if (e.key == key) return e.scene;

    SceneParams sp;
    sp.source = rq.source; sp.mic = rq.mic; sp.doors = rq.doors;
    sp.sizeScale = rq.sizeScale; sp.decayScale = rq.decayScale;
    auto scene = std::make_shared<const Scene> (buildScene (*rq.location, sp));

    sceneCache.insert (sceneCache.begin(), { key, scene });
    if (sceneCache.size() > 6) sceneCache.pop_back();
    return scene;
}

std::unique_ptr<IRSet> RenderEngine::render (const RenderRequest& rq, RenderInfo& inf)
{
    inf.locationId = rq.location->id;
    inf.source = juce::jlimit (0, (int) rq.location->sources.size() - 1, rq.source);
    inf.mic = juce::jlimit (0, (int) rq.location->mics.size() - 1, rq.mic);
    inf.doors = rq.doors;

    if (rq.location->isPack)
    {
        if (auto s = renderMeasured (rq, inf)) return s;
        inf.measuredFallback = true;
    }
    return renderSimulated (rq, inf);
}

//==============================================================================
std::unique_ptr<IRSet> RenderEngine::renderSimulated (const RenderRequest& rq, RenderInfo& inf)
{
    auto scene = sceneFor (rq);
    if (scene == nullptr || ! scene->valid) return {};

    const double fs = rq.sampleRate;
    const auto rig = makeRig (rq.outLayout, rq.micSystem);
    const int O = (int) rig.size();
    const int L = std::max (kHeadLength + kTailBlock, irLengthFor (*scene, fs, kTailBlock));
    const int maxParts = maxTailPartsFor (fs);
    const int activeO = activeOutputs (rq.outLayout);

    const uint64_t seed = hashCombine (hashCombine (hashCombine ((uint64_t) std::hash<std::string>{} (rq.location->id.toStdString()),
                                                                 (uint64_t) rq.source), (uint64_t) rq.mic), (uint64_t) rq.doors);

    const float firstAz = azimuthOf (scene->firstDir);
    const float recordedYaw = scene->micYawDeg;
    inf.recordedAngleDeg = wrapDegrees (firstAz - recordedYaw);
    inf.rt1k = scene->micRoomRt[3];
    inf.path = scene->mainPath;
    inf.throughClosed = scene->throughClosedStructure;

    std::unique_ptr<IRSet> set;

    if (rq.directOn)
    {
        const float yaw = rq.customAngle ? wrapDegrees (firstAz - rq.angleDeg) : recordedYaw;
        inf.micYawDeg = yaw;
        inf.appliedAngleDeg = wrapDegrees (firstAz - yaw);

        juce::AudioBuffer<float> ir (O, L);
        ir.clear();
        EarlyOptions eo; eo.sampleRate = fs; eo.includeDirect = true; eo.maxSamples = L;
        renderEarly (*scene, rig, { yaw }, eo, ir);
        renderLate (*scene, rig, yaw, fs, seed, ir);
        fadeOutEnd (ir, (int) (0.03 * fs));

        const float e = bufferEnergyPerChannel (ir, activeO);
        set = IRSet::build ({ { 0, &ir } }, &ir, O, maxParts);
        set->flatGain = flatGainFor (e);
        inf.levelDb = 10.0f * std::log10 (std::max (1.0e-12f, e));
        inf.irSeconds = (float) L / (float) fs;
        return set;
    }

    // ---------------------------------------------------------------- Direct Off
    const auto inAz = inputAzimuths (rq.inLayout);
    int numIn = 0;
    for (auto a : inAz) if (! std::isnan (a)) ++numIn;
    numIn = std::max (1, numIn);
    const float rowGain = 1.0f / std::sqrt ((float) numIn);
    const float frontYaw = wrapDegrees (firstAz - 0.0f);
    inf.micYawDeg = frontYaw;
    inf.appliedAngleDeg = 0;

    // offsets used to widen each input's reflections (Spread)
    std::vector<float> offsets { 0.0f };
    if (rq.spread > 0.02f)
    {
        const float w = rq.spread * 150.0f;
        offsets = { -w, -w * 0.5f, 0.0f, w * 0.5f, w };
    }

    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> rows;
    std::vector<std::pair<int, const juce::AudioBuffer<float>*>> rowRefs;
    float energy = 0;

    for (size_t i = 0; i < inAz.size(); ++i)
    {
        if (std::isnan (inAz[i])) continue;
        auto buf = std::make_unique<juce::AudioBuffer<float>> (O, kHeadLength);
        buf->clear();
        std::vector<float> yaws;
        for (auto off : offsets) yaws.push_back (wrapDegrees (firstAz - (inAz[i] + off)));
        EarlyOptions eo; eo.sampleRate = fs; eo.includeDirect = false; eo.maxSamples = kHeadLength;
        renderEarly (*scene, rig, yaws, eo, *buf, rowGain);
        energy += bufferEnergyPerChannel (*buf, activeO);
        rowRefs.push_back ({ (int) i + 1, buf.get() });
        rows.push_back (std::move (buf));
    }

    // shared late field (and any discrete arrival later than the head)
    juce::AudioBuffer<float> late (O, L);
    late.clear();
    renderLate (*scene, rig, frontYaw, fs, seed, late);
    {
        juce::AudioBuffer<float> lateEarly (O, L);
        lateEarly.clear();
        EarlyOptions eo; eo.sampleRate = fs; eo.includeDirect = false; eo.maxSamples = L;
        renderEarly (*scene, rig, { frontYaw }, eo, lateEarly);
        for (int c = 0; c < O; ++c)
            late.addFrom (c, kHeadLength, lateEarly, c, kHeadLength, L - kHeadLength);
    }
    fadeOutEnd (late, (int) (0.03 * fs));
    energy += bufferEnergyPerChannel (late, activeO);

    const float tailScale = std::sqrt ((float) numIn);
    juce::AudioBuffer<float> noiseHead (O, kHeadLength);
    for (int c = 0; c < O; ++c)
    {
        noiseHead.copyFrom (c, 0, late, c, 0, kHeadLength);
        noiseHead.applyGain (c, 0, kHeadLength, tailScale);
    }
    rowRefs.push_back ({ 0, &noiseHead });

    set = IRSet::build (rowRefs, &late, O, maxParts, tailScale);
    set->flatGain = flatGainFor (energy);
    inf.levelDb = 10.0f * std::log10 (std::max (1.0e-12f, energy));
    inf.irSeconds = (float) L / (float) fs;
    return set;
}

//==============================================================================
std::unique_ptr<IRSet> RenderEngine::renderMeasured (const RenderRequest& rq, RenderInfo& inf)
{
    const auto& loc = *rq.location;
    auto m = measuredCache.load (loc, inf.source, inf.mic, rq.doors, rq.sampleRate);
    if (m == nullptr) return {};

    inf.measured = true;
    inf.measuredFallback = ! m->exactDoorMatch;
    inf.derived = m->derived;
    const double fs = rq.sampleRate;
    const int O = rq.outLayout.size();
    const int activeO = activeOutputs (rq.outLayout);
    const int maxParts = maxTailPartsFor (fs);

    // size / decay on top of the recording (1 = as recorded)
    int onset = m->onsetSample;
    const auto shaped = reshapeMeasured (m->rigIR, m->onsetSample, m->rt60At1k, fs, rq.sizeScale, rq.decayScale, onset);
    const int maxLen = (int) (kMaxIRSeconds * fs) - kTailBlock;
    const int L = std::max (kHeadLength + kTailBlock,
                            ((std::min (shaped.getNumSamples(), maxLen) + kTailBlock - 1) / kTailBlock) * kTailBlock);
    const float calib = juce::Decibels::decibelsToGain (loc.packCalibrationDb);
    const float firstAz = m->firstArrivalAzDeg;

    inf.recordedAngleDeg = firstAz;
    inf.path = { loc.sources[(size_t) inf.source].position(), loc.mics[(size_t) inf.mic].pos };
    auto micYaw = loc.mics[(size_t) inf.mic].yawDeg;
    inf.micYawDeg = std::isnan (micYaw) ? 90.0f : micYaw;
    inf.irSeconds = (float) L / (float) fs;
    inf.rt1k = m->rt60At1k * std::max (1.0f, rq.sizeScale) * std::max (1.0f, rq.decayScale);

    if (rq.directOn)
    {
        const float rot = rq.customAngle ? wrapDegrees (rq.angleDeg - firstAz) : 0.0f;
        inf.appliedAngleDeg = wrapDegrees (firstAz + rot);
        inf.micYawDeg = wrapDegrees (inf.micYawDeg - rot);
        juce::AudioBuffer<float> ir (O, L);
        ir.clear();
        mapRigToLayout (shaped, loc.rig, rq.outLayout, rot, ir, calib, L);
        fadeOutEnd (ir, (int) (0.02 * fs));
        const float e = bufferEnergyPerChannel (ir, activeO);
        auto set = IRSet::build ({ { 0, &ir } }, &ir, O, maxParts);
        set->flatGain = flatGainFor (e);
        inf.levelDb = 10.0f * std::log10 (std::max (1.0e-12f, e));
        return set;
    }

    juce::AudioBuffer<float> noDirect (shaped);
    removeDirectSound (noDirect, onset, fs);

    const auto inAz = inputAzimuths (rq.inLayout);
    int numIn = 0;
    for (auto a : inAz) if (! std::isnan (a)) ++numIn;
    numIn = std::max (1, numIn);
    const float rowGain = 1.0f / std::sqrt ((float) numIn);

    std::vector<float> offsets { 0.0f };
    if (rq.spread > 0.02f)
    {
        const float w = rq.spread * 150.0f;
        offsets = { -w, -w * 0.5f, 0.0f, w * 0.5f, w };
    }

    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> rows;
    std::vector<std::pair<int, const juce::AudioBuffer<float>*>> rowRefs;
    float energy = 0;
    for (size_t i = 0; i < inAz.size(); ++i)
    {
        if (std::isnan (inAz[i])) continue;
        auto buf = std::make_unique<juce::AudioBuffer<float>> (O, kHeadLength);
        buf->clear();
        for (auto off : offsets)
            mapRigToLayout (noDirect, loc.rig, rq.outLayout, wrapDegrees (inAz[i] + off - firstAz), *buf,
                            calib * rowGain / (float) offsets.size(), kHeadLength);
        energy += bufferEnergyPerChannel (*buf, activeO);
        rowRefs.push_back ({ (int) i + 1, buf.get() });
        rows.push_back (std::move (buf));
    }

    juce::AudioBuffer<float> tail (O, L);
    tail.clear();
    mapRigToLayout (noDirect, loc.rig, rq.outLayout, wrapDegrees (-firstAz), tail, calib, L);
    for (int c = 0; c < O; ++c) tail.clear (c, 0, kHeadLength);
    fadeOutEnd (tail, (int) (0.02 * fs));
    energy += bufferEnergyPerChannel (tail, activeO);

    auto set = IRSet::build (rowRefs, &tail, O, maxParts, std::sqrt ((float) numIn));
    set->flatGain = flatGainFor (energy);
    inf.levelDb = 10.0f * std::log10 (std::max (1.0e-12f, energy));
    return set;
}

} // namespace srr
