#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace srr
{
struct FactoryPreset
{
    juce::String name, location, source, mic;
    juce::StringArray openDoors;       // portal ids that are open (others closed)
    bool direct = true;
    int micSystem = 0;                 // 0 spaced, 1 coincident, 2 hybrid
    int angleMode = 0;                 // 0 recorded, 1 custom
    float angle = 0;
    float levelRange = 100;
    float size = 100, decay = 100;
    float mix = 100, spread = 30, predelay = 0;
    float low = 0, high = 0, output = 0;
};

const std::vector<FactoryPreset>& factoryPresets();
} // namespace srr
