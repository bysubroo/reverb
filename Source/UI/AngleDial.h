#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace srr::ui
{
// Top view of the listener: front is up, the dot is where the direct sound
// appears (positive angles = left). Draggable in Custom mode.
class AngleDial : public juce::Component, public juce::SettableTooltipClient
{
public:
    AngleDial();

    void setState (float apparentDeg, float recordedDeg, bool editable);
    std::function<void (float)> onDrag;          // degrees
    std::function<void()> onGestureStart, onGestureEnd;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    float angleFromMouse (juce::Point<float> p) const;
    float apparent = 0, recorded = 0;
    bool editable = false, dragging = false;
};
} // namespace srr::ui
