#pragma once

// Turns a Scene (world-frame arrivals + late energy envelopes) into a
// multichannel impulse response for a given virtual microphone rig and
// orientation. The late field is synthesised as a set of plane waves with
// fixed world directions and a seeded random realisation, so rotating the
// rig keeps the same "sound field" and crossfades stay seamless.

#include "Scene.h"
#include "MicRig.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace srr
{
struct EarlyOptions
{
    double sampleRate = 48000.0;
    bool includeDirect = true;
    int maxSamples = 1 << 16;     // arrivals later than this are ignored
};

// Accumulates discrete arrivals into 'out' (channels == rig.size()).
// 'yawsDeg' are averaged (used for Spread); each entry is the world yaw of the rig front.
void renderEarly (const Scene& scene, const std::vector<Capsule>& rig, const std::vector<float>& yawsDeg,
                  const EarlyOptions& opt, juce::AudioBuffer<float>& out, float gain = 1.0f);

// Accumulates the late field into 'out'.
void renderLate (const Scene& scene, const std::vector<Capsule>& rig, float yawDeg, double sampleRate,
                 uint64_t seed, juce::AudioBuffer<float>& out, float gain = 1.0f);

int irLengthFor (const Scene& scene, double sampleRate, int multiple);

} // namespace srr
