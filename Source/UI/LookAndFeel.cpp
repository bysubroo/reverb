#include "LookAndFeel.h"

namespace srr::ui
{
juce::Font uiFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

SubRooLookAndFeel::SubRooLookAndFeel()
{
    using namespace colours;
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::ComboBox::backgroundColourId, panel2);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, panel2);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, accent);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, panel2);
    setColour (juce::TextButton::textColourOffId, textDim);
    setColour (juce::TextButton::textColourOnId, text);
    setColour (juce::TooltipWindow::backgroundColourId, panel2);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, line);
}

void SubRooLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float start, float end, juce::Slider& s)
{
    using namespace colours;
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    auto r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto c = bounds.getCentre();
    const float thick = std::max (3.0f, r * 0.13f);
    const float angle = start + pos * (end - start);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - thick, r - thick, 0, start, end, true);
    g.setColour (line);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    float from = start;
    if (bipolar) from = start + (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) * (end - start);

    auto col = s.findColour (juce::Slider::rotarySliderFillColourId);
    if (s.isEnabled())
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, r - thick, r - thick, 0, std::min (from, angle), std::max (from, angle), true);
        g.setColour (col.withAlpha (0.25f));
        g.strokePath (val, juce::PathStrokeType (thick * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col);
        g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    auto inner = r - thick * 2.4f;
    g.setColour (panel2);
    g.fillEllipse (c.x - inner, c.y - inner, inner * 2, inner * 2);
    juce::Point<float> tip (c.x + std::sin (angle) * (inner - 2), c.y - std::cos (angle) * (inner - 2));
    g.setColour (s.isEnabled() ? text : textDim);
    g.drawLine ({ c.getPointOnCircumference (inner * 0.6f, angle), tip }, 2.0f);

    g.setColour (s.isEnabled() ? text : textDim);
    g.setFont (uiFont (std::max (10.0f, r * 0.30f)));
    g.drawText (s.getTextFromValue (s.getValue()), bounds.withY (c.y + inner * 0.25f).withHeight (inner * 0.9f).toNearestInt(),
                juce::Justification::centredTop);
}

void SubRooLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                          juce::Slider::SliderStyle, juce::Slider& s)
{
    using namespace colours;
    auto b = juce::Rectangle<int> (x, y, w, h).toFloat();
    auto cy = b.getCentreY();
    g.setColour (line);
    g.fillRoundedRectangle (b.getX(), cy - 2.5f, b.getWidth(), 5.0f, 2.5f);
    auto col = s.findColour (juce::Slider::trackColourId);
    g.setColour (col);
    g.fillRoundedRectangle (b.getX(), cy - 2.5f, pos - b.getX(), 5.0f, 2.5f);
    g.setColour (col.withAlpha (0.25f));
    g.fillEllipse (pos - 11, cy - 11, 22, 22);
    g.setColour (text);
    g.fillEllipse (pos - 7, cy - 7, 14, 14);
}

void SubRooLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    using namespace colours;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (down ? panel2.brighter (0.15f) : (over ? panel2.brighter (0.07f) : panel2));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (b.getToggleState() ? accent : line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

void SubRooLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId));
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

juce::Font SubRooLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return uiFont (std::min (13.0f, h * 0.5f), true); }

void SubRooLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace colours;
    auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.hasKeyboardFocus (true) ? accent : line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    juce::Path arrow;
    auto ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (textDim);
    g.fillPath (arrow);
}

juce::Font SubRooLookAndFeel::getComboBoxFont (juce::ComboBox& b) { return uiFont (std::min (14.0f, b.getHeight() * 0.5f), true); }
juce::Font SubRooLookAndFeel::getPopupMenuFont() { return uiFont (14.0f); }

void SubRooLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

//==============================================================================
Knob::Knob (const juce::String& c) : caption (c)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
    slider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (slider);
}

void Knob::resized()
{
    auto b = getLocalBounds();
    b.removeFromBottom (16);
    slider.setBounds (b);
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (colours::textDim);
    g.setFont (uiFont (11.0f, true));
    g.drawText (caption.toUpperCase(), getLocalBounds().removeFromBottom (16), juce::Justification::centred);
}

//==============================================================================
Segmented::Segmented (juce::StringArray l, juce::Colour a) : labels (std::move (l)), accent (a) {}

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
    using namespace colours;
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (panel2);
    g.fillRoundedRectangle (r, 7.0f);
    const float w = r.getWidth() / (float) std::max (1, labels.size());
    for (int i = 0; i < labels.size(); ++i)
    {
        auto cell = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight()).reduced (2.0f);
        if (i == selected)
        {
            g.setColour (accent.withAlpha (0.18f));
            g.fillRoundedRectangle (cell, 5.0f);
            g.setColour (accent);
            g.drawRoundedRectangle (cell, 5.0f, 1.2f);
        }
        else if (i == hover)
        {
            g.setColour (panel2.brighter (0.1f));
            g.fillRoundedRectangle (cell, 5.0f);
        }
        g.setColour (i == selected ? text : textDim);
        g.setFont (uiFont (std::min (12.5f, r.getHeight() * 0.42f), true));
        g.drawFittedText (labels[i], cell.toNearestInt(), juce::Justification::centred, 1);
    }
    g.setColour (line);
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
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
