#include "AngleDial.h"
#include "LookAndFeel.h"
#include <cmath>

namespace srr::ui
{
AngleDial::AngleDial()
{
    setTooltip ("Where the direct sound appears around the mic. Drag in Custom mode, double-click to centre.");
}

void AngleDial::setState (float a, float r, bool e)
{
    if (std::abs (a - apparent) > 0.05f || std::abs (r - recorded) > 0.05f || e != editable)
    {
        apparent = a; recorded = r; editable = e;
        repaint();
    }
}

static juce::Point<float> onCircle (juce::Point<float> c, float radius, float deg)
{
    auto a = deg * juce::MathConstants<float>::pi / 180.0f;
    return { c.x - radius * std::sin (a), c.y - radius * std::cos (a) };
}

void AngleDial::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat().reduced (6.0f);
    auto side = std::min (b.getWidth(), b.getHeight() - 18.0f);
    auto area = juce::Rectangle<float> (side, side).withCentre ({ b.getCentreX(), b.getY() + side * 0.5f });
    auto c = area.getCentre();
    auto r = side * 0.5f - 13.0f;

    // round display window in a metal bezel
    hw::drawRoundBezel (g, area.reduced (1.0f), 8.0f);
    auto glassArea = area.reduced (9.0f);
    juce::ColourGradient glass (juce::Colour (0xff17202a), c.x, c.y - side * 0.1f, juce::Colour (0xff070a0d), c.x + side * 0.5f, c.y + side * 0.5f, true);
    g.setGradientFill (glass);
    g.fillEllipse (glassArea);
    g.setColour (line.withAlpha (0.6f));
    g.drawEllipse (glassArea.reduced (r * 0.28f), 0.8f);
    g.drawEllipse (glassArea.reduced (r * 0.56f), 0.8f);

    for (int i = 0; i < 24; ++i)
    {
        auto deg = (float) i * 15.0f;
        auto len = (i % 6 == 0) ? 7.0f : 3.5f;
        g.setColour (i % 6 == 0 ? textDim : line.brighter (0.2f));
        g.drawLine ({ onCircle (c, r, deg), onCircle (c, r - len, deg) }, 1.2f);
    }

    g.setFont (uiFont (10.0f, true));
    g.setColour (textDim);
    g.drawText ("FRONT", juce::Rectangle<float> (60, 12).withCentre (onCircle (c, r - 16, 0)), juce::Justification::centred);
    g.drawText ("L", juce::Rectangle<float> (14, 12).withCentre (onCircle (c, r - 14, 90)), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (14, 12).withCentre (onCircle (c, r - 14, -90)), juce::Justification::centred);

    // listener head (top view)
    auto head = r * 0.2f;
    g.setColour (mic.withAlpha (0.18f));
    g.fillEllipse (c.x - head * 1.6f, c.y - head * 1.6f, head * 3.2f, head * 3.2f);
    g.setColour (mic);
    g.fillEllipse (c.x - head, c.y - head, head * 2, head * 2);
    juce::Path nose;
    nose.addTriangle (c.x - head * 0.45f, c.y - head * 0.85f, c.x + head * 0.45f, c.y - head * 0.85f, c.x, c.y - head * 1.55f);
    g.fillPath (nose);

    // recorded position (ghost) when editing
    if (editable)
    {
        auto pr = onCircle (c, r * 0.72f, recorded);
        g.setColour (accent.withAlpha (0.35f));
        g.drawEllipse (pr.x - 6, pr.y - 6, 12, 12, 1.2f);
    }

    auto p = onCircle (c, r * 0.72f, apparent);
    juce::Path ray;
    ray.startNewSubPath (p);
    ray.lineTo (c);
    float dash[] = { 3.0f, 4.0f };
    juce::PathStrokeType (1.2f).createDashedStroke (ray, ray, dash, 2);
    g.setColour (accent.withAlpha (editable ? 0.8f : 0.45f));
    g.fillPath (ray);

    g.setColour (accent.withAlpha (0.25f));
    g.fillEllipse (p.x - 12, p.y - 12, 24, 24);
    g.setColour (editable ? accent : accent.withAlpha (0.65f));
    g.fillRoundedRectangle (p.x - 7, p.y - 7, 14, 14, 3.0f);

    // glass highlight
    juce::ColourGradient glare (juce::Colours::white.withAlpha (0.07f), glassArea.getX(), glassArea.getY(),
                                juce::Colours::transparentWhite, glassArea.getCentreX(), glassArea.getCentreY(), false);
    g.setGradientFill (glare);
    g.fillEllipse (glassArea);

    const juce::String deg (juce::CharPointer_UTF8 ("\xc2\xb0"));
    auto label = juce::String (juce::roundToInt (apparent)) + deg;
    if (apparent > 0.5f) label = "L " + label;
    else if (apparent < -0.5f) label = "R " + juce::String (juce::roundToInt (-apparent)) + deg;
    auto win = juce::Rectangle<float> (64.0f, 17.0f).withCentre (getLocalBounds().toFloat().removeFromBottom (18).getCentre());
    hw::drawRecess (g, win, 2.5f, hw::pal::glass);
    g.setColour (hw::pal::amber);
    g.setFont (hw::font (14.0f, hw::Weight::semibold, 0.04f));
    g.drawText (label, win, juce::Justification::centred);
}

float AngleDial::angleFromMouse (juce::Point<float> p) const
{
    auto b = getLocalBounds().toFloat().reduced (6.0f);
    auto side = std::min (b.getWidth(), b.getHeight() - 18.0f);
    juce::Point<float> c { b.getCentreX(), b.getY() + side * 0.5f };
    auto d = p - c;
    return std::atan2 (-d.x, -d.y) * 180.0f / juce::MathConstants<float>::pi;
}

void AngleDial::mouseDown (const juce::MouseEvent& e)
{
    if (! editable) return;
    dragging = true;
    if (onGestureStart) onGestureStart();
    if (onDrag) onDrag (angleFromMouse (e.position));
}

void AngleDial::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging && onDrag) onDrag (angleFromMouse (e.position));
}

void AngleDial::mouseUp (const juce::MouseEvent&)
{
    if (dragging && onGestureEnd) onGestureEnd();
    dragging = false;
}

void AngleDial::mouseDoubleClick (const juce::MouseEvent&)
{
    if (! editable) return;
    if (onGestureStart) onGestureStart();
    if (onDrag) onDrag (0.0f);
    if (onGestureEnd) onGestureEnd();
}
} // namespace srr::ui
