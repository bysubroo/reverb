#pragma once

// Background thread that turns the current plug-in state into impulse
// responses and hands them to the convolver. Every parameter change only
// bumps an atomic counter (real-time safe); the thread renders the latest
// state as fast as it can, and the convolver crossfades to each new set.

#include "Scene.h"
#include "MicRig.h"
#include "MeasuredPack.h"
#include "../DSP/MatrixConvolver.h"
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <functional>

namespace srr
{
struct RenderRequest
{
    LocationPtr location;
    int source = 0, mic = 0;
    uint32_t doors = 0;
    float sizeScale = 1.0f, decayScale = 1.0f;
    bool directOn = true;
    bool customAngle = false;
    float angleDeg = 0.0f;
    MicSystem micSystem = MicSystem::Spaced;
    float spread = 0.3f;
    juce::AudioChannelSet outLayout = juce::AudioChannelSet::stereo();
    juce::AudioChannelSet inLayout = juce::AudioChannelSet::stereo();
    double sampleRate = 48000.0;
    bool valid = false;
};

struct RenderInfo
{
    juce::String locationId;
    int source = -1, mic = -1;
    uint32_t doors = 0;
    float recordedAngleDeg = 0;   // where the first arrival appears with the recorded mic orientation
    float appliedAngleDeg = 0;    // where it appears now
    float micYawDeg = 90;         // applied mic yaw in the plan (for drawing)
    float rt1k = 0, irSeconds = 0, levelDb = -100;
    float renderMs = 0;
    bool measured = false;
    bool measuredFallback = false;
    bool derived = false;            // speaker spot derived from another recording
    bool throughClosed = false;
    std::vector<Vec3> path;
    uint64_t serial = 0;
};

class RenderEngine : private juce::Thread
{
public:
    RenderEngine (MatrixConvolver& convolver, juce::CriticalSection& engineLock);
    ~RenderEngine() override;

    void setRequestProvider (std::function<RenderRequest()> provider);
    void startRendering();
    void stopRendering();

    void markDirty() noexcept { dirty.fetch_add (1, std::memory_order_relaxed); }
    bool isBusy() const noexcept { return busy.load() || dirty.load() != rendered.load(); }

    RenderInfo getInfo() const;

private:
    void run() override;
    std::unique_ptr<IRSet> render (const RenderRequest& rq, RenderInfo& info);
    std::unique_ptr<IRSet> renderSimulated (const RenderRequest& rq, RenderInfo& info);
    std::unique_ptr<IRSet> renderMeasured (const RenderRequest& rq, RenderInfo& info);
    std::shared_ptr<const Scene> sceneFor (const RenderRequest& rq);

    MatrixConvolver& conv;
    juce::CriticalSection& convLock;
    std::function<RenderRequest()> provider;
    std::atomic<uint64_t> dirty { 1 }, rendered { 0 };
    std::atomic<bool> busy { false };
    uint64_t serialCounter = 0;

    mutable juce::SpinLock infoLock;
    RenderInfo info;

    struct CacheEntry { uint64_t key; std::shared_ptr<const Scene> scene; };
    std::vector<CacheEntry> sceneCache;
    MeasuredIRCache measuredCache;
};

std::vector<float> inputAzimuths (const juce::AudioChannelSet& inLayout);

} // namespace srr
