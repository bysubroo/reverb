#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Hardware.h"

namespace srr::ui
{
namespace colours
{
    // screen (room view) colours
    const juce::Colour bg        { 0xff0a0d10 };
    const juce::Colour panel     { 0xff12161a };
    const juce::Colour panel2    { 0xff1a1f24 };
    const juce::Colour line      { 0xff2a3138 };
    const juce::Colour text      { 0xffeeeef3 };
    const juce::Colour textDim   { 0xff8b919a };
    const juce::Colour accent    { 0xfffe20d7 };   // SubRoo pink: speakers / sources
    const juce::Colour mic       { 0xff2ef2ff };   // microphones
    const juce::Colour warn      { 0xffffc23d };
    const juce::Colour door      { 0xffc08a55 };
}

class SubRooLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SubRooLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
};

juce::Font uiFont (float height, bool bold = false);

// Bakelite knob with an engraved caption and a small amber readout below.
class Knob : public juce::Component
{
public:
    explicit Knob (const juce::String& caption);
    juce::Slider slider;
    void resized() override;
    void paint (juce::Graphics&) override;
    static constexpr int captionH = 16, readoutH = 17;
private:
    juce::String caption;
};

// Row of lit push buttons bound to a choice/bool parameter via callbacks
class Segmented : public juce::Component
{
public:
    Segmented (juce::StringArray labels, juce::Colour lampColour = hw::pal::amber);
    std::function<void (int)> onSelect;
    void setSelected (int index);
    int getSelected() const { return selected; }
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
private:
    int indexAt (juce::Point<int>) const;
    juce::StringArray labels;
    juce::Colour lamp;
    int selected = 0, hover = -1;
};
} // namespace srr::ui
