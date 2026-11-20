#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Acoustics/LocationLibrary.h"
#include "Acoustics/RenderEngine.h"
#include "DSP/MatrixConvolver.h"
#include "Parameters.h"
#include "Presets.h"

namespace srr
{
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0; }
    void setShelf (double fs, double f0, double gainDb, bool high) noexcept;
};

class SubRooRoomsProcessor : public juce::AudioProcessor,
                             private juce::AudioProcessorValueTreeState::Listener
{
public:
    SubRooRoomsProcessor();
    ~SubRooRoomsProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SubRoo Rooms"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.6; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // editor helpers (message thread)
    LocationPtr getCurrentLocation() const;
    int getLocationIndex() const;
    int getSourceIndex() const;
    int getMicIndex() const;
    uint32_t getDoors() const;
    bool isDirectOn() const;
    bool isCustomAngle() const;

    void selectLocation (int index);
    void selectSource (int index);
    void selectMic (int index);
    void toggleDoor (int bit);
    void setAllDoors (bool open);
    void setParam (const char* id, float plainValue);
    void applyPreset (const FactoryPreset& p);

    float estimateLevelDb (int source, int mic, uint32_t doors) const;
    RenderInfo getRenderInfo() const { return renderer->getInfo(); }
    bool isRendering() const { return renderer->isBusy(); }
    void rescanPacks();
    float getOutputRms() const { return std::sqrt (outputMeanSquare.load()); }

    juce::AudioProcessorValueTreeState& getState() { return apvts; }
    LocationLibrary& getLibrary() { return *library; }

private:
    void parameterChanged (const juce::String& id, float newValue) override;
    RenderRequest makeRequest();
    void updateDryMatrix();
    int paramInt (const std::atomic<float>* p) const { return (int) std::lround (p->load()); }

    std::atomic<float> outputMeanSquare { 0.0f };
    juce::SharedResourcePointer<LocationLibrary> library;
    juce::AudioProcessorValueTreeState apvts;

    MatrixConvolver convolver;
    juce::CriticalSection convLock;
    std::unique_ptr<RenderEngine> renderer;

    std::atomic<float>* pLocation {};
    std::atomic<float>* pSource {};
    std::atomic<float>* pMic {};
    std::atomic<float>* pDoors {};
    std::atomic<float>* pDirect {};
    std::atomic<float>* pMicSystem {};
    std::atomic<float>* pAngleMode {};
    std::atomic<float>* pAngle {};
    std::atomic<float>* pLevelRange {};
    std::atomic<float>* pSpread {};
    std::atomic<float>* pMix {};
    std::atomic<float>* pPredelay {};
    std::atomic<float>* pSize {};
    std::atomic<float>* pDecay {};
    std::atomic<float>* pLow {};
    std::atomic<float>* pHigh {};
    std::atomic<float>* pOutput {};

    // layout snapshot for the renderer
    juce::SpinLock layoutLock;
    juce::AudioChannelSet snapIn = juce::AudioChannelSet::stereo(), snapOut = juce::AudioChannelSet::stereo();
    double snapRate = 48000.0;
    std::atomic<bool> prepared { false };

    // processing state
    double fs = 48000.0;
    int blockCap = 512;
    int numIn = 2, numOut = 2;
    juce::AudioBuffer<float> fdlBuf, wetBuf, dryBuf;
    std::vector<const float*> fdlPtrs;
    std::vector<float*> wetPtrs;

    juce::AudioBuffer<float> dryRing;
    int dryRingPos = 0;
    std::vector<std::vector<float>> dryMatrix;   // [out][in]

    juce::AudioBuffer<float> preRing;
    int preRingPos = 0;
    juce::SmoothedValue<float> preSamples;

    std::vector<Biquad> lowShelf, highShelf;
    float lastLow = 1000, lastHigh = 1000;
    juce::SmoothedValue<float> wetGain, dryGain, outGain, levelRangeSm;

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubRooRoomsProcessor)
};
} // namespace srr
