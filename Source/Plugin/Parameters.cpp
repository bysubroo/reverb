#include "Parameters.h"

namespace srr::params
{
using namespace juce;

AudioProcessorValueTreeState::ParameterLayout createLayout (const NameProviders& names)
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto intAttr = [] (std::function<String (int)> fn)
    {
        return AudioParameterIntAttributes().withStringFromValueFunction ([fn] (int v, int) { return fn ? fn (v) : String (v); });
    };

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { location, 1 }, "Location", 0, kMaxLocations - 1, 0,
                                                     intAttr (names.locationName)));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { source, 1 }, "Speaker", 0, kMaxSpots - 1, 0,
                                                     intAttr (names.sourceName)));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { mic, 1 }, "Mic", 0, kMaxSpots - 1, 0,
                                                     intAttr (names.micName)));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { doors, 1 }, "Doors", 0, 4095, 0,
                                                     AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                                                     {
                                                         String s;
                                                         for (int b = 0; b < 12; ++b) s << (((v >> b) & 1) ? "O" : "-");
                                                         return s;
                                                     })));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { direct, 1 }, "Direct", true,
                                                      AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return v ? "On" : "Off"; })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { micSystem, 1 }, "Mic System",
                                                        StringArray { "Spaced (AB)", "Coincident (MS)", "Hybrid" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { angleMode, 1 }, "Angle Mode",
                                                        StringArray { "Recorded", "Custom" }, 0));

    auto degAttr = AudioParameterFloatAttributes().withLabel ("deg")
                     .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + String (juce::CharPointer_UTF8 ("\xc2\xb0")); });
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { angle, 1 }, "Angle", NormalisableRange<float> (-180.0f, 180.0f, 0.1f), 0.0f, degAttr));

    auto pct = [] { return AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; }); };
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { levelRange, 1 }, "Level Range", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { spread, 1 }, "Spread", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, pct()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct()));

    auto msAttr = AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " ms"; });
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { predelay, 1 }, "Pre-delay", NormalisableRange<float> (0.0f, 250.0f, 0.1f, 0.5f), 0.0f, msAttr));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { size, 1 }, "Size", NormalisableRange<float> (70.0f, 140.0f, 0.1f), 100.0f, pct()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { decay, 1 }, "Decay", NormalisableRange<float> (50.0f, 200.0f, 0.1f, 0.7f), 100.0f, pct()));

    auto dbAttr = AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; });
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { lowEq, 1 }, "Low", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { highEq, 1 }, "High", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, dbAttr));

    return layout;
}

bool needsRender (const String& id)
{
    return id == location || id == source || id == mic || id == doors || id == direct || id == micSystem
        || id == angleMode || id == angle || id == spread || id == size || id == decay;
}
} // namespace srr::params
