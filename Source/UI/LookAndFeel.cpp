#include "LookAndFeel.h"

namespace srr::ui
{
using namespace hw;

juce::Font uiFont (float height, bool bold)
{
    return hw::font (height * 1.12f, bold ? Weight::bold : Weight::medium, 0.02f);
}

SubRooLookAndFeel::SubRooLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, pal::faceBottom);
    setColour (juce::ComboBox::backgroundColourId, pal::glass);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::black);
    setColour (juce::ComboBox::textColourId, pal::amber);
    setColour (juce::ComboBox::arrowColourId, pal::amberDeep);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1c1d20));
    setColour (juce::PopupMenu::textColourId, pal::silk);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, pal::amber.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff1d1307));
    setColour (juce::PopupMenu::headerTextColourId, pal::amber);
    setColour (juce::Label::textColourId, pal::silk);
    setColour (juce::Slider::textBoxTextColourId, pal::amber);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, pal::faceBottom);
    setColour (juce::TextButton::textColourOffId, pal::silkDim);
    setColour (juce::TextButton::textColourOnId, pal::silk);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1c1d20));
    setColour (juce::TooltipWindow::textColourId, pal::silk);
    setColour (juce::TooltipWindow::outlineColourId, juce::Colours::black);
}

void SubRooLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float start, float end, juce::Slider& s)
{
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const bool hot = s.isMouseButtonDown() || s.isMouseOverOrDragging();
    hw::drawKnob (g, juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f), pos, start, end, bipolar, s.isEnabled(), hot);
}

void SubRooLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                          juce::Slider::SliderStyle, juce::Slider& s)
{
    // hardware fader: recessed slot, centre detent marks, metal cap
    auto b = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float cy = b.getCentreY();
    auto slot = juce::Rectangle<float> (b.getX() + 4.0f, cy - 3.0f, b.getWidth() - 8.0f, 6.0f);
    drawRecess (g, slot, 3.0f, juce::Colour (0xff0b0b0c));

    for (int i = 0; i <= 10; ++i)
    {
        const float tx = slot.getX() + slot.getWidth() * (float) i / 10.0f;
        g.setColour (pal::silk.withAlpha (i % 5 == 0 ? 0.7f : 0.35f));
        g.fillRect (tx - 0.5f, cy + 7.0f, 1.0f, i % 5 == 0 ? 5.0f : 3.0f);
    }

    const float capW = 14.0f, capH = std::min (b.getHeight() - 2.0f, 24.0f);
    auto cap = juce::Rectangle<float> (capW, capH).withCentre ({ pos, cy });
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (cap.translated (0, 2.0f), 2.5f);
    juce::ColourGradient grad (juce::Colour (0xffd9d9d6), cap.getX(), cap.getY(), juce::Colour (0xff6c6d70), cap.getX(), cap.getBottom(), false);
    grad.addColour (0.48, juce::Colour (0xffa9aaab));
    grad.addColour (0.52, juce::Colour (0xff7d7e80));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 2.5f);
    g.setColour (juce::Colour (0xff1a1a1c));
    g.drawRoundedRectangle (cap, 2.5f, 1.0f);
    g.setColour (s.isMouseOverOrDragging() ? pal::amber : juce::Colour (0xff1a1a1c));
    g.fillRect (cap.getCentreX() - 1.0f, cap.getY() + 3.0f, 2.0f, cap.getHeight() - 6.0f);
}

void SubRooLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    if (down) r = r.translated (0, 1.0f);
    hw::drawPushCap (g, r, b.getButtonText(), b.getToggleState(), over, pal::amber);
}

void SubRooLookAndFeel::drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) {}

juce::Font SubRooLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return hw::font (std::min (14.0f, h * 0.52f), Weight::bold); }

void SubRooLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    // amber LCD window
    auto r = juce::Rectangle<int> (0, 0, w, h).toFloat();
    drawRecess (g, r, 3.0f, pal::glass);
    juce::ColourGradient glow (pal::amber.withAlpha (0.06f), r.getX(), r.getY(), juce::Colours::transparentBlack, r.getX(), r.getBottom(), false);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (r.reduced (1.0f), 3.0f);

    // fine scanlines
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    for (float yy = r.getY() + 2.0f; yy < r.getBottom() - 1.0f; yy += 2.0f)
        g.fillRect (r.getX() + 2.0f, yy, r.getWidth() - 4.0f, 0.6f);

    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.isMouseOver (true) ? pal::amber : pal::amberDeep);
    g.fillPath (arrow);
}

juce::Font SubRooLookAndFeel::getComboBoxFont (juce::ComboBox& b)
{
    return hw::font (std::min (17.0f, b.getHeight() * 0.62f), Weight::semibold, 0.05f);
}

juce::Font SubRooLookAndFeel::getPopupMenuFont() { return hw::font (16.0f, Weight::medium, 0.02f); }

void SubRooLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void SubRooLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (juce::Colours::black);
    g.drawRect (0, 0, w, h, 1);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine (1, 1.0f, (float) w - 1.0f);
}

void SubRooLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    g.setFont (hw::font (13.0f, Weight::bold, 0.12f));
    g.setColour (findColour (juce::PopupMenu::headerTextColourId));
    g.drawFittedText (name.toUpperCase(), area.getX() + 12, area.getY(), area.getWidth() - 16, (int) ((float) area.getHeight() * 0.8f),
                      juce::Justification::bottomLeft, 1);
}

void SubRooLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.fillAll (findColour (juce::TooltipWindow::backgroundColourId));
    g.setColour (juce::Colours::black);
    g.drawRect (0, 0, w, h, 1);
    g.setColour (pal::silk);
    g.setFont (hw::font (15.0f, Weight::medium, 0.02f));
    g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (8, 4), juce::Justification::centredLeft, 4);
}

//==============================================================================
Knob::Knob (const juce::String& c) : caption (c)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityModeParameters (0.9, 1, 0.05, true, juce::ModifierKeys::shiftModifier);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void Knob::resized()
{
    auto b = getLocalBounds();
    b.removeFromBottom (captionH + readoutH + 2);
    const int side = std::min (b.getWidth(), b.getHeight());
    slider.setBounds (b.withSizeKeepingCentre (side, side));
}

void Knob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto readout = b.removeFromBottom ((float) readoutH);
    auto cap = b.removeFromBottom ((float) captionH);
    drawEngraved (g, caption.toUpperCase(), cap, juce::Justification::centred, 14.0f, pal::silk, Weight::bold, 0.12f);

    const auto txt = slider.getTextFromValue (slider.getValue());
    const auto f = hw::font (14.0f, Weight::semibold, 0.04f);
    const float tw = std::max (46.0f, juce::GlyphArrangement::getStringWidth (f, txt) + 12.0f);
    auto win = juce::Rectangle<float> (tw, (float) readoutH - 2.0f).withCentre (readout.getCentre());
    drawRecess (g, win, 2.5f, pal::glass);
    g.setFont (f);
    g.setColour (isEnabled() ? pal::amber : pal::amber.withAlpha (0.35f));
    g.drawText (txt, win, juce::Justification::centred, false);
}

//==============================================================================
Segmented::Segmented (juce::StringArray l, juce::Colour c) : labels (std::move (l)), lamp (c) {}

void Segmented::setSelected (int i)
{
    if (i != selected) { selected = i; repaint(); }
}

void Segmented::resized() {}

int Segmented::indexAt (juce::Point<int> p) const
{
    if (labels.isEmpty()) return -1;
    return juce::jlimit (0, labels.size() - 1, p.x * labels.size() / std::max (1, getWidth()));
}

void Segmented::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawRecess (g, r, 4.0f, juce::Colour (0xff17181a));
    const float w = r.getWidth() / (float) std::max (1, labels.size());
    for (int i = 0; i < labels.size(); ++i)
    {
        auto cell = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight()).reduced (3.0f, 3.0f);
        if (i == selected) cell = cell.translated (0, 0.5f);
        drawPushCap (g, cell, labels[i], i == selected, i == hover, lamp);
    }
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    auto i = indexAt (e.getPosition());
    if (i < 0) return;
    setSelected (i);
    if (onSelect) onSelect (i);
}

void Segmented::mouseMove (const juce::MouseEvent& e)
{
    auto i = indexAt (e.getPosition());
    if (i != hover) { hover = i; repaint(); }
}

void Segmented::mouseExit (const juce::MouseEvent&) { hover = -1; repaint(); }
} // namespace srr::ui
