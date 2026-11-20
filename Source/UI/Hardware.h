#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Drawing helpers for the hardware-style faceplate (rack unit look).
// Everything is drawn in code, so it scales cleanly on any display.
namespace srr::ui::hw
{
namespace pal
{
    const juce::Colour faceTop     { 0xff3c3e43 };   // graphite anodised faceplate
    const juce::Colour faceBottom  { 0xff2b2c30 };
    const juce::Colour ear         { 0xff505257 };
    const juce::Colour recess      { 0xff1d1e21 };
    const juce::Colour silk        { 0xffece6d6 };   // warm white silkscreen
    const juce::Colour silkDim     { 0xffa8a294 };
    const juce::Colour amber       { 0xffffb54a };   // lamps, readouts
    const juce::Colour amberDeep   { 0xffd9822b };
    const juce::Colour green       { 0xff9df0a8 };
    const juce::Colour red         { 0xffe8483a };
    const juce::Colour glass       { 0xff0b0f12 };
    const juce::Colour vuFace      { 0xfff2e3b6 };   // backlit meter face
}

enum class Weight { medium, semibold, bold };

juce::Font font (float height, Weight w = Weight::semibold, float tracking = 0.04f);

// Procedural brushed-metal texture (cached by the caller)
juce::Image makeBrushed (int w, int h, juce::Colour top, juce::Colour bottom, uint32_t seed, float grain = 1.0f);

void drawScrew (juce::Graphics&, juce::Point<float> centre, float radius, float slotAngle);
void drawRackEar (juce::Graphics&, juce::Rectangle<float> area, bool left);
void drawRecess (juce::Graphics&, juce::Rectangle<float> area, float corner, juce::Colour fill);
void drawBezel (juce::Graphics&, juce::Rectangle<float> outer, float thickness, float corner);
void drawRoundBezel (juce::Graphics&, juce::Rectangle<float> outer, float thickness);
void drawEngraved (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Justification,
                   float height, juce::Colour = pal::silk, Weight = Weight::semibold, float tracking = 0.08f);
void drawSectionFrame (juce::Graphics&, juce::Rectangle<float>, const juce::String& title);
void drawLamp (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour, bool on);

// Bakelite knob with a metal skirt and an engraved scale around it.
void drawKnob (juce::Graphics&, juce::Rectangle<float> area, float pos, float startAngle, float endAngle,
               bool bipolar, bool enabled, bool hot);

// Lit push-button cap (for switch rows)
void drawPushCap (juce::Graphics&, juce::Rectangle<float>, const juce::String&, bool on, bool hover, juce::Colour lamp);

// Small metal push button
void drawMetalButton (juce::Graphics&, juce::Rectangle<float>, bool down, bool hover);

// Analog VU meter
class VUMeter : public juce::Component
{
public:
    void setLevel (float rms);           // linear RMS of the output
    void paint (juce::Graphics&) override;
private:
    float needle = 0.0f;                 // ballistic VU value in dB
    float peakHold = -60.0f;
    int peakTimer = 0;
};
} // namespace srr::ui::hw
