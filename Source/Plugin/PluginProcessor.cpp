#include "PluginProcessor.h"
#include "UI/PluginEditor.h"

namespace srr
{
//==============================================================================
void Biquad::setShelf (double sr, double f0, double gainDb, bool high) noexcept
{
    // RBJ cookbook shelf, S = 1
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / sr;
    const double cw = std::cos (w0), sw = std::sin (w0);
    const double alpha = sw / 2.0 * std::sqrt (2.0);
    const double sqA = 2.0 * std::sqrt (A) * alpha;
    double nb0, nb1, nb2, na0, na1, na2;
    if (! high)
    {
        nb0 = A * ((A + 1) - (A - 1) * cw + sqA);
        nb1 = 2 * A * ((A - 1) - (A + 1) * cw);
        nb2 = A * ((A + 1) - (A - 1) * cw - sqA);
        na0 = (A + 1) + (A - 1) * cw + sqA;
        na1 = -2 * ((A - 1) + (A + 1) * cw);
        na2 = (A + 1) + (A - 1) * cw - sqA;
    }
    else
    {
        nb0 = A * ((A + 1) + (A - 1) * cw + sqA);
        nb1 = -2 * A * ((A - 1) + (A + 1) * cw);
        nb2 = A * ((A + 1) + (A - 1) * cw - sqA);
        na0 = (A + 1) - (A - 1) * cw + sqA;
        na1 = 2 * ((A - 1) - (A + 1) * cw);
        na2 = (A + 1) - (A - 1) * cw - sqA;
    }
    b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
    a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
}

//==============================================================================
SubRooRoomsProcessor::SubRooRoomsProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SubRooRooms",
             params::createLayout ({
                 [this] (int i) { auto l = library->get (i); return l ? l->name : juce::String (i); },
                 [this] (int i)
                 {
                     auto l = getCurrentLocation();
                     return (l && i < (int) l->sources.size()) ? l->sources[(size_t) i].name : juce::String (i + 1);
                 },
                 [this] (int i)
                 {
                     auto l = getCurrentLocation();
                     return (l && i < (int) l->mics.size()) ? l->mics[(size_t) i].name : juce::String (i + 1);
                 } }))
{
    pLocation   = apvts.getRawParameterValue (params::location);
    pSource     = apvts.getRawParameterValue (params::source);
    pMic        = apvts.getRawParameterValue (params::mic);
    pDoors      = apvts.getRawParameterValue (params::doors);
    pDirect     = apvts.getRawParameterValue (params::direct);
    pMicSystem  = apvts.getRawParameterValue (params::micSystem);
    pAngleMode  = apvts.getRawParameterValue (params::angleMode);
    pAngle      = apvts.getRawParameterValue (params::angle);
    pLevelRange = apvts.getRawParameterValue (params::levelRange);
    pSpread     = apvts.getRawParameterValue (params::spread);
    pMix        = apvts.getRawParameterValue (params::mix);
    pPredelay   = apvts.getRawParameterValue (params::predelay);
    pSize       = apvts.getRawParameterValue (params::size);
    pDecay      = apvts.getRawParameterValue (params::decay);
    pLow        = apvts.getRawParameterValue (params::lowEq);
    pHigh       = apvts.getRawParameterValue (params::highEq);
    pOutput     = apvts.getRawParameterValue (params::output);

    for (auto* id : { params::location, params::source, params::mic, params::doors, params::direct, params::micSystem,
                      params::angleMode, params::angle, params::spread, params::size, params::decay })
        apvts.addParameterListener (id, this);

    renderer = std::make_unique<RenderEngine> (convolver, convLock);
    renderer->setRequestProvider ([this] { return makeRequest(); });

    // start with the first factory preset's location defaults
    if (auto loc = library->get (0))
    {
        if (auto* p = apvts.getParameter (params::source)) p->setValueNotifyingHost (p->convertTo0to1 ((float) loc->defaultSource));
        if (auto* p = apvts.getParameter (params::mic)) p->setValueNotifyingHost (p->convertTo0to1 ((float) loc->defaultMic));
        if (auto* p = apvts.getParameter (params::doors)) p->setValueNotifyingHost (p->convertTo0to1 ((float) loc->defaultDoors));
    }
}

SubRooRoomsProcessor::~SubRooRoomsProcessor()
{
    renderer->stopRendering();
    for (auto* id : { params::location, params::source, params::mic, params::doors, params::direct, params::micSystem,
                      params::angleMode, params::angle, params::spread, params::size, params::decay })
        apvts.removeParameterListener (id, this);
}

//==============================================================================
bool SubRooRoomsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    using ACS = juce::AudioChannelSet;
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    const ACS outs[] = { ACS::mono(), ACS::stereo(), ACS::createLCR(), ACS::quadraphonic(), ACS::create5point0(),
                         ACS::create5point1(), ACS::create7point0(), ACS::create7point1(), ACS::create7point0point2(),
                         ACS::create7point1point2() };
    bool okOut = false;
    for (auto& o : outs) okOut = okOut || o == out;
    const bool okIn = in == ACS::mono() || in == ACS::stereo() || in == out;
    return okOut && okIn;
}

void SubRooRoomsProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    blockCap = std::max (64, samplesPerBlock);
    numIn = std::max (1, getTotalNumInputChannels());
    numOut = std::max (1, getTotalNumOutputChannels());

    {
        const juce::ScopedLock sl (convLock);
        const int maxParts = (int) std::ceil ((3.6 * fs - kHeadLength) / kTailBlock) + 1;
        convolver.prepare (numIn + 1, numOut, maxParts);
    }

    fdlBuf.setSize (numIn + 1, blockCap);
    wetBuf.setSize (numOut, blockCap);
    dryBuf.setSize (numOut, blockCap);
    fdlPtrs.assign ((size_t) numIn + 1, nullptr);
    wetPtrs.assign ((size_t) numOut, nullptr);

    dryRing.setSize (numIn, kHeadBlock + blockCap + 1);
    dryRing.clear();
    dryRingPos = 0;

    preRing.setSize (numIn + 1, (int) (0.26 * fs) + blockCap + 4);
    preRing.clear();
    preRingPos = 0;
    preSamples.reset (fs, 0.05);
    preSamples.setCurrentAndTargetValue (0.0f);

    lowShelf.assign ((size_t) numOut, {});
    highShelf.assign ((size_t) numOut, {});
    lastLow = lastHigh = 1000.0f;

    wetGain.reset (fs, 0.03); dryGain.reset (fs, 0.03); outGain.reset (fs, 0.03); levelRangeSm.reset (fs, 0.1);
    const bool directOn = pDirect->load() > 0.5f;
    wetGain.setCurrentAndTargetValue (directOn ? 1.0f : pMix->load() / 100.0f);
    dryGain.setCurrentAndTargetValue (directOn ? 0.0f : 1.0f - pMix->load() / 100.0f);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    levelRangeSm.setCurrentAndTargetValue (pLevelRange->load() / 100.0f);

    updateDryMatrix();
    setLatencySamples (convolver.getLatencySamples());

    {
        const juce::SpinLock::ScopedLockType sl (layoutLock);
        snapIn = getChannelLayoutOfBus (true, 0);
        snapOut = getChannelLayoutOfBus (false, 0);
        snapRate = fs;
    }
    prepared = true;
    renderer->markDirty();
    renderer->startRendering();
}

void SubRooRoomsProcessor::updateDryMatrix()
{
    const auto in = getChannelLayoutOfBus (true, 0);
    const auto out = getChannelLayoutOfBus (false, 0);
    dryMatrix.assign ((size_t) numOut, std::vector<float> ((size_t) numIn, 0.0f));

    for (int o = 0; o < numOut; ++o)
    {
        auto t = out.getTypeOfChannel (o);
        int same = in.getChannelIndexForType (t);
        if (same >= 0 && same < numIn) { dryMatrix[(size_t) o][(size_t) same] = 1.0f; continue; }

        if (numIn == 1)
        {
            const bool hasCentre = out.getChannelIndexForType (juce::AudioChannelSet::centre) >= 0;
            if (numOut == 1) dryMatrix[(size_t) o][0] = 1.0f;
            else if (t == juce::AudioChannelSet::centre) dryMatrix[(size_t) o][0] = 1.0f;
            else if (! hasCentre && (t == juce::AudioChannelSet::left || t == juce::AudioChannelSet::right))
                dryMatrix[(size_t) o][0] = 0.7071f;
        }
        else if (numOut == 1)
        {
            for (int i = 0; i < numIn; ++i) dryMatrix[0][(size_t) i] = 1.0f / (float) numIn;
        }
    }
}

void SubRooRoomsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    const int inCh = std::min (getTotalNumInputChannels(), numIn);
    const int outCh = std::min (buffer.getNumChannels(), numOut);
    if (total == 0 || ! prepared) return;

    const bool directOn = pDirect->load() > 0.5f;
    const float mix = pMix->load() / 100.0f;
    wetGain.setTargetValue (directOn ? 1.0f : mix);
    dryGain.setTargetValue (directOn ? 0.0f : 1.0f - mix);
    outGain.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    levelRangeSm.setTargetValue (pLevelRange->load() / 100.0f);
    preSamples.setTargetValue (directOn ? 0.0f : (float) (pPredelay->load() * 0.001 * fs));

    const float lowDb = pLow->load(), highDb = pHigh->load();
    if (std::abs (lowDb - lastLow) > 0.01f)
    {
        for (auto& b : lowShelf) { auto z1 = b.z1, z2 = b.z2; b.setShelf (fs, 250.0, lowDb, false); b.z1 = z1; b.z2 = z2; }
        lastLow = lowDb;
    }
    if (std::abs (highDb - lastHigh) > 0.01f)
    {
        for (auto& b : highShelf) { auto z1 = b.z1, z2 = b.z2; b.setShelf (fs, 4000.0, highDb, true); b.z1 = z1; b.z2 = z2; }
        lastHigh = highDb;
    }
    const bool eqActive = std::abs (lowDb) > 0.01f || std::abs (highDb) > 0.01f;

    const int preLen = preRing.getNumSamples();
    const int dryLen = dryRing.getNumSamples();

    for (int start = 0; start < total; start += blockCap)
    {
        const int n = std::min (blockCap, total - start);

        // ---- engine inputs: 0 = mono average, 1..N = individual channels
        auto* mono = fdlBuf.getWritePointer (0);
        juce::FloatVectorOperations::clear (mono, n);
        for (int c = 0; c < numIn; ++c)
        {
            auto* dst = fdlBuf.getWritePointer (c + 1);
            if (c < inCh) juce::FloatVectorOperations::copy (dst, buffer.getReadPointer (c, start), n);
            else juce::FloatVectorOperations::clear (dst, n);
            juce::FloatVectorOperations::addWithMultiply (mono, dst, 1.0f / (float) numIn, n);
        }

        // ---- dry signal, delayed by the convolver latency
        for (int c = 0; c < numIn; ++c)
        {
            auto* ring = dryRing.getWritePointer (c);
            const auto* src = fdlBuf.getReadPointer (c + 1);
            int w = dryRingPos;
            for (int i = 0; i < n; ++i) { ring[w] = src[i]; if (++w == dryLen) w = 0; }
        }
        dryBuf.clear (0, n);
        for (int o = 0; o < numOut; ++o)
        {
            auto* dst = dryBuf.getWritePointer (o);
            for (int c = 0; c < numIn; ++c)
            {
                const float g = dryMatrix[(size_t) o][(size_t) c];
                if (g == 0.0f) continue;
                const auto* ring = dryRing.getReadPointer (c);
                int r = dryRingPos - kHeadBlock;
                if (r < 0) r += dryLen;
                for (int i = 0; i < n; ++i) { dst[i] += ring[r] * g; if (++r == dryLen) r = 0; }
            }
        }
        dryRingPos = (dryRingPos + n) % dryLen;

        // ---- pre-delay (Direct Off) on the engine inputs
        if (preSamples.isSmoothing() || preSamples.getTargetValue() > 0.0f)
        {
            for (int i = 0; i < n; ++i)
            {
                const float d = preSamples.getNextValue();
                const int di = (int) d;
                const float fr = d - (float) di;
                for (int c = 0; c <= numIn; ++c)
                {
                    auto* ring = preRing.getWritePointer (c);
                    auto* io = fdlBuf.getWritePointer (c);
                    ring[preRingPos] = io[i];
                    int r0 = preRingPos - di; if (r0 < 0) r0 += preLen;
                    int r1 = r0 - 1;          if (r1 < 0) r1 += preLen;
                    io[i] = ring[r0] * (1.0f - fr) + ring[r1] * fr;
                }
                if (++preRingPos == preLen) preRingPos = 0;
            }
        }
        else
        {
            // keep the ring warm so turning pre-delay up does not glitch
            for (int c = 0; c <= numIn; ++c)
            {
                auto* ring = preRing.getWritePointer (c);
                const auto* io = fdlBuf.getReadPointer (c);
                int w = preRingPos;
                for (int i = 0; i < n; ++i) { ring[w] = io[i]; if (++w == preLen) w = 0; }
            }
            preRingPos = (preRingPos + n) % preLen;
        }

        // ---- convolution
        for (int c = 0; c <= numIn; ++c) fdlPtrs[(size_t) c] = fdlBuf.getReadPointer (c);
        for (int o = 0; o < numOut; ++o) wetPtrs[(size_t) o] = wetBuf.getWritePointer (o);
        levelRangeSm.skip (n);
        convolver.process (fdlPtrs.data(), wetPtrs.data(), n, levelRangeSm.getCurrentValue());

        if (eqActive)
            for (int o = 0; o < numOut; ++o)
            {
                auto* d = wetBuf.getWritePointer (o);
                auto& lo = lowShelf[(size_t) o];
                auto& hi = highShelf[(size_t) o];
                for (int i = 0; i < n; ++i) d[i] = hi.process (lo.process (d[i]));
            }

        // ---- output
        for (int i = 0; i < n; ++i)
        {
            const float wg = wetGain.getNextValue(), dg = dryGain.getNextValue(), og = outGain.getNextValue();
            for (int o = 0; o < outCh; ++o)
                buffer.getWritePointer (o, start)[i] = (wetBuf.getSample (o, i) * wg + dryBuf.getSample (o, i) * dg) * og;
        }
    }

    for (int o = outCh; o < buffer.getNumChannels(); ++o)
        buffer.clear (o, 0, total);
}

//==============================================================================
void SubRooRoomsProcessor::parameterChanged (const juce::String& id, float)
{
    if (params::needsRender (id))
        renderer->markDirty();
}

RenderRequest SubRooRoomsProcessor::makeRequest()
{
    RenderRequest rq;
    rq.location = library->get (paramInt (pLocation));
    if (rq.location == nullptr) return rq;
    rq.source = juce::jlimit (0, (int) rq.location->sources.size() - 1, paramInt (pSource));
    rq.mic = juce::jlimit (0, (int) rq.location->mics.size() - 1, paramInt (pMic));
    rq.doors = (uint32_t) std::max (0, paramInt (pDoors));
    rq.sizeScale = pSize->load() / 100.0f;
    rq.decayScale = pDecay->load() / 100.0f;
    rq.directOn = pDirect->load() > 0.5f;
    rq.customAngle = paramInt (pAngleMode) == 1;
    rq.angleDeg = pAngle->load();
    rq.micSystem = (MicSystem) juce::jlimit (0, 2, paramInt (pMicSystem));
    rq.spread = pSpread->load() / 100.0f;
    {
        const juce::SpinLock::ScopedLockType sl (layoutLock);
        rq.inLayout = snapIn;
        rq.outLayout = snapOut;
        rq.sampleRate = snapRate;
    }
    rq.valid = prepared.load();
    return rq;
}

//==============================================================================
LocationPtr SubRooRoomsProcessor::getCurrentLocation() const
{
    return library->get (pLocation != nullptr ? (int) std::lround (pLocation->load()) : 0);
}

int SubRooRoomsProcessor::getLocationIndex() const { return juce::jlimit (0, std::max (0, library->size() - 1), paramInt (pLocation)); }
int SubRooRoomsProcessor::getSourceIndex() const
{
    auto l = getCurrentLocation();
    return l ? juce::jlimit (0, (int) l->sources.size() - 1, paramInt (pSource)) : 0;
}
int SubRooRoomsProcessor::getMicIndex() const
{
    auto l = getCurrentLocation();
    return l ? juce::jlimit (0, (int) l->mics.size() - 1, paramInt (pMic)) : 0;
}
uint32_t SubRooRoomsProcessor::getDoors() const { return (uint32_t) std::max (0, paramInt (pDoors)); }
bool SubRooRoomsProcessor::isDirectOn() const { return pDirect->load() > 0.5f; }
bool SubRooRoomsProcessor::isCustomAngle() const { return paramInt (pAngleMode) == 1; }

void SubRooRoomsProcessor::setParam (const char* id, float plain)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
        p->endChangeGesture();
    }
}

void SubRooRoomsProcessor::selectLocation (int index)
{
    auto loc = library->get (index);
    if (loc == nullptr) return;
    setParam (params::location, (float) index);
    setParam (params::source, (float) loc->defaultSource);
    setParam (params::mic, (float) loc->defaultMic);
    setParam (params::doors, (float) loc->defaultDoors);
}

void SubRooRoomsProcessor::selectSource (int index) { setParam (params::source, (float) index); }
void SubRooRoomsProcessor::selectMic (int index)    { setParam (params::mic, (float) index); }

void SubRooRoomsProcessor::toggleDoor (int bit)
{
    if (bit < 0 || bit >= kMaxDoorBits) return;
    setParam (params::doors, (float) (getDoors() ^ (1u << (uint32_t) bit)));
}

void SubRooRoomsProcessor::setAllDoors (bool open)
{
    auto loc = getCurrentLocation();
    if (loc == nullptr) return;
    uint32_t mask = 0;
    if (open) for (int b = 0; b < loc->numDoorBits; ++b) mask |= (1u << (uint32_t) b);
    setParam (params::doors, (float) mask);
}

float SubRooRoomsProcessor::estimateLevelDb (int source, int mic, uint32_t doors) const
{
    auto loc = getCurrentLocation();
    if (loc == nullptr) return -100.0f;
    SceneParams sp;
    sp.source = source; sp.mic = mic; sp.doors = doors;
    sp.sizeScale = pSize->load() / 100.0f;
    sp.decayScale = pDecay->load() / 100.0f;
    return srr::estimateLevelDb (*loc, sp);
}

void SubRooRoomsProcessor::rescanPacks()
{
    library->rescanPacks();
    renderer->markDirty();
}

//==============================================================================
void SubRooRoomsProcessor::applyPreset (const FactoryPreset& p)
{
    int li = library->indexOf (p.location);
    if (li < 0) return;
    auto loc = library->get (li);
    setParam (params::location, (float) li);
    setParam (params::source, (float) std::max (0, loc->findSource (p.source)));
    setParam (params::mic, (float) std::max (0, loc->findMic (p.mic)));
    uint32_t mask = 0;
    for (auto& portal : loc->portals)
        if (portal.doorBit >= 0 && p.openDoors.contains (portal.id))
            mask |= (1u << (uint32_t) portal.doorBit);
    setParam (params::doors, (float) mask);
    setParam (params::direct, p.direct ? 1.0f : 0.0f);
    setParam (params::micSystem, (float) p.micSystem);
    setParam (params::angleMode, (float) p.angleMode);
    setParam (params::angle, p.angle);
    setParam (params::levelRange, p.levelRange);
    setParam (params::size, p.size);
    setParam (params::decay, p.decay);
    setParam (params::mix, p.mix);
    setParam (params::spread, p.spread);
    setParam (params::predelay, p.predelay);
    setParam (params::lowEq, p.low);
    setParam (params::highEq, p.high);
    setParam (params::output, p.output);
}

int SubRooRoomsProcessor::getNumPrograms() { return std::max (1, (int) factoryPresets().size()); }

void SubRooRoomsProcessor::setCurrentProgram (int index)
{
    auto& presets = factoryPresets();
    if (index < 0 || index >= (int) presets.size()) return;
    currentProgram = index;
    applyPreset (presets[(size_t) index]);
}

const juce::String SubRooRoomsProcessor::getProgramName (int index)
{
    auto& presets = factoryPresets();
    return (index >= 0 && index < (int) presets.size()) ? presets[(size_t) index].name : juce::String ("Default");
}

//==============================================================================
void SubRooRoomsProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto loc = getCurrentLocation())
    {
        state.setProperty ("locationId", loc->id, nullptr);
        auto s = getSourceIndex(), m = getMicIndex();
        state.setProperty ("sourceId", loc->sources[(size_t) s].id, nullptr);
        state.setProperty ("micId", loc->mics[(size_t) m].id, nullptr);
        state.setProperty ("doorKey", loc->doorStateKey (getDoors()), nullptr);
    }
    state.setProperty ("pluginVersion", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SubRooRoomsProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (state);

    // resolve by id so sessions survive added/removed packs
    auto locId = state.getProperty ("locationId").toString();
    if (locId.isEmpty()) return;
    int li = library->indexOf (locId);
    if (li < 0) return;
    auto loc = library->get (li);
    auto setRaw = [this] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    setRaw (params::location, (float) li);
    int si = loc->findSource (state.getProperty ("sourceId").toString());
    int mi = loc->findMic (state.getProperty ("micId").toString());
    if (si >= 0) setRaw (params::source, (float) si);
    if (mi >= 0) setRaw (params::mic, (float) mi);
    renderer->markDirty();
}

juce::AudioProcessorEditor* SubRooRoomsProcessor::createEditor()
{
    return new SubRooRoomsEditor (*this);
}
} // namespace srr

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new srr::SubRooRoomsProcessor();
}
