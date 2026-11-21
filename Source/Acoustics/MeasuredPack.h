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
    bool derived = false;             // spot derived from another recording (no own IR)
    int onsetSample = 0;
    float firstArrivalAzDeg = 0;      // estimated from early energy over the rig
    float rt60At1k = 0;               // T20-based estimate in the 1 kHz octave
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

// Derived spot: mirror / delay / darken / level the base spot's recording in place.
void applyDerive (const SourceSpot::Derive&, const std::vector<RigChannel>& rig, juce::AudioBuffer<float>& ir, double sampleRate);

// Size and decay on a measured response. Both are >= 1 (1 = the recording as is):
// size stretches the response in time (bigger room), decay lengthens the reverb tail
// (the recording's noise floor is replaced by a matching synthetic tail).
// Returns the reshaped response and its onset sample.
juce::AudioBuffer<float> reshapeMeasured (const juce::AudioBuffer<float>& ir, int onsetSample, float rt60,
                                          double sampleRate, float size, float decay, int& newOnset);

// Silences the direct sound (for Direct Off) around the onset.
void removeDirectSound (juce::AudioBuffer<float>& ir, int onsetSample, double sampleRate);

} // namespace srr
