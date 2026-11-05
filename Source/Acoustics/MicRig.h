#pragma once

#include "Core.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace srr
{
enum class MicSystem { Spaced = 0, Coincident = 1, Hybrid = 2 };

// One virtual capsule per output channel. Mic frame: +x front, +y left, +z up.
struct Capsule
{
    Vec3 pos;                 // metres, relative to rig centre
    Vec3 facing { 1, 0, 0 };
    bool baffledOmni = true;  // spaced omni with baffle shadowing, otherwise first-order
    float firstOrderA = 0.5f; // pattern = A + (1 - A) cos(theta)
    bool silent = false;      // LFE
    float azDeg = 0, elDeg = 0;
};

struct ChannelDirection { float azDeg = 0, elDeg = 0; bool lfe = false; };

ChannelDirection nominalDirection (juce::AudioChannelSet::ChannelType t);

std::vector<Capsule> makeRig (const juce::AudioChannelSet& layout, MicSystem system);

float capsuleGain (const Capsule& c, Vec3 dirMicFrame, int band);

} // namespace srr
