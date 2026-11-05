#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

namespace srr::params
{
inline constexpr const char* location   = "location";
inline constexpr const char* source     = "source";
inline constexpr const char* mic        = "mic";
inline constexpr const char* doors      = "doors";
inline constexpr const char* direct     = "direct";
inline constexpr const char* micSystem  = "micSystem";
inline constexpr const char* angleMode  = "angleMode";
inline constexpr const char* angle      = "angle";
inline constexpr const char* levelRange = "levelRange";
inline constexpr const char* spread     = "spread";
inline constexpr const char* mix        = "mix";
inline constexpr const char* predelay   = "predelay";
inline constexpr const char* size       = "size";
inline constexpr const char* decay      = "decay";
inline constexpr const char* lowEq      = "lowEq";
inline constexpr const char* highEq     = "highEq";
inline constexpr const char* output     = "output";

inline constexpr int kMaxLocations = 64;
inline constexpr int kMaxSpots = 32;

struct NameProviders
{
    std::function<juce::String (int)> locationName;
    std::function<juce::String (int)> sourceName;
    std::function<juce::String (int)> micName;
};

juce::AudioProcessorValueTreeState::ParameterLayout createLayout (const NameProviders& names);

// parameters whose change requires a new impulse response
bool needsRender (const juce::String& paramID);
} // namespace srr::params
