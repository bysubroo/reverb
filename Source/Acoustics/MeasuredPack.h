#pragma once

// Measured location packs: real impulse responses recorded with a
// multichannel rig, one WAV per (speaker spot, mic spot, door state):
//
//   <pack>/ir/<sourceId>__<micId>__<doorKey>.wav
//
// doorKey is one character per toggleable door in location.json order
// ('1' open, '0' closed), or '-' if the location has no doors. Missing door
// states fall back to the closest measured one.

#include "Location.h"
#include "MicRig.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <mutex>

namespace srr
{
struct MeasuredIR
{
    juce::AudioBuffer<float> rigIR;   // one channel per rig channel, at the session rate
    juce::String fileName;
    bool exactDoorMatch = true;
    int onsetSample = 0;
    float firstArrivalAzDeg = 0;      // estimated from early energy over the rig
};

class MeasuredIRCache
{
public:
    std::shared_ptr<const MeasuredIR> load (const Location& loc, int source, int mic, uint32_t doors, double sampleRate);

    static juce::File irFileFor (const Location& loc, int source, int mic, uint32_t doors);

private:
    std::mutex lock;
    std::map<juce::String, std::shared_ptr<const MeasuredIR>> cache;   // key: path@rate
    juce::AudioFormatManager formats;
    bool formatsReady = false;
};

// Pan the rig channels (rotated by rotationDeg) onto the output layout.
void mapRigToLayout (const juce::AudioBuffer<float>& rigIR, const std::vector<RigChannel>& rig,
                     const juce::AudioChannelSet& layout, float rotationDeg,
                     juce::AudioBuffer<float>& out, float gain, int maxSamples);

// Silences the direct sound (for Direct Off) around the onset.
void removeDirectSound (juce::AudioBuffer<float>& ir, int onsetSample, double sampleRate);

} // namespace srr
