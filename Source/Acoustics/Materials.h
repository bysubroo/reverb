#pragma once

#include "Core.h"
#include <juce_core/juce_core.h>

namespace srr
{
// Surface absorption coefficients (octave bands 125 Hz .. 8 kHz)
bool lookupAbsorption (const juce::String& name, BandArray& out);

// Transmission loss presets in dB (doors, windows, walls)
bool lookupTransmissionLoss (const juce::String& name, BandArray& outDb);

// Device colouration presets in dB (TV, radio, phone, PA ...)
bool lookupDeviceEq (const juce::String& name, BandArray& outDb);

// Directivity blend per band: 0 = omni, 1 = cardioid
BandArray sourceDirectivityFor (const juce::String& kind);

// parse either a preset name or an explicit array of 7 numbers
bool parseBandValue (const juce::var& v, BandArray& out, bool (*lookup) (const juce::String&, BandArray&));

juce::StringArray allMaterialNames();
juce::StringArray allTransmissionNames();
} // namespace srr
