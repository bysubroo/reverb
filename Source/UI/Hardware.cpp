#include "Hardware.h"
#include "BinaryData.h"

namespace srr::ui::hw
{
//==============================================================================
static juce::Typeface::Ptr loadFace (const char* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
}

juce::Font font (float height, Weight w, float tracking)
{
    static juce::Typeface::Ptr medium   = loadFace (SubRooData::BarlowCondensedMedium_ttf, SubRooData::BarlowCondensedMedium_ttfSize);
    static juce::Typeface::Ptr semibold = loadFace (SubRooData::BarlowCondensedSemiBold_ttf, SubRooData::BarlowCondensedSemiBold_ttfSize);
    static juce::Typeface::Ptr bold     = loadFace (SubRooData::BarlowCondensedBold_ttf, SubRooData::BarlowCondensedBold_ttfSize);
    auto face = w == Weight::bold ? bold : (w == Weight::medium ? medium : semibold);
    return juce::Font (juce::FontOptions (face).withHeight (height).withKerningFactor (tracking));
}

//==============================================================================
juce::Image makeBrushed (int w, int h, juce::Colour top, juce::Colour bottom, uint32_t seed, float grain)
{
    w = std::max (1, w);
    h = std::max (1, h);
    juce::Image img (juce::Image::RGB, w, h, false);
    juce::Random rnd ((juce::int64) seed);

    // per-row brightness drift + long horizontal streaks = brushed look
    std::vector<float> row ((size_t) h);
    float drift = 0.0f;
    for (int y = 0; y < h; ++y)
    {
        drift = drift * 0.82f + (rnd.nextFloat() - 0.5f) * 0.9f;
        row[(size_t) y] = drift;
    }

    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
    {
        const auto base = top.interpolatedWith (bottom, (float) y / (float) h);
        const float br = (float) base.getRed(), bg = (float) base.getGreen(), bb = (float) base.getBlue();
        float streak = 0.0f;
        int streakLeft = 0;
        for (int x = 0; x < w; ++x)
        {
            if (streakLeft-- <= 0)
            {
                streakLeft = 20 + rnd.nextInt (220);
                streak = (rnd.nextFloat() - 0.5f) * 5.0f;
            }
            const float n = (row[(size_t) y] * 3.0f + streak + (rnd.nextFloat() - 0.5f) * 3.0f) * grain;
            bd.setPixelColour (x, y, juce::Colour ((juce::uint8) juce::jlimit (0.0f, 255.0f, br + n),
                                                   (juce::uint8) juce::jlimit (0.0f, 255.0f, bg + n),
                                                   (juce::uint8) juce::jlimit (0.0f, 255.0f, bb + n * 1.05f)));
        }
    }
    return img;
}

//==============================================================================
void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float slotAngle)
{
    // countersink shadow
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (c.x - r * 1.18f, c.y - r * 1.05f, r * 2.36f, r * 2.36f);

    juce::ColourGradient head (juce::Colour (0xffe6e6e3), c.x - r * 0.6f, c.y - r * 0.7f,
                               juce::Colour (0xff5d5e60), c.x + r * 0.7f, c.y + r * 0.8f, true);
    g.setGradientFill (head);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colour (0xff2a2a2c));
    g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, std::max (0.6f, r * 0.08f));

    // phillips slot
    for (int i = 0; i < 2; ++i)
    {
        const float a = slotAngle + (float) i * juce::MathConstants<float>::halfPi;
        juce::Path slot;
        slot.addRoundedRectangle (-r * 0.72f, -r * 0.13f, r * 1.44f, r * 0.26f, r * 0.1f);
        slot.applyTransform (juce::AffineTransform::rotation (a).translated (c));
        g.setColour (juce::Colour (0xff232325));
        g.fillPath (slot);
    }
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawEllipse (c.x - r * 0.75f, c.y - r * 0.82f, r * 1.2f, r * 1.0f, std::max (0.5f, r * 0.05f));
}

void drawRackEar (juce::Graphics& g, juce::Rectangle<float> a, bool left)
{
    juce::ColourGradient grad (pal::ear.brighter (0.12f), a.getX(), a.getY(), pal::ear.darker (0.25f), a.getX(), a.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (a);

    // fold edge towards the faceplate
    auto edge = left ? a.removeFromRight (3.0f) : a.removeFromLeft (3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (edge);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.fillRect (left ? a.removeFromRight (1.0f) : a.removeFromLeft (1.0f));

    // rack slots with screws
    const float sw = a.getWidth() * 0.62f, sh = sw * 1.55f;
    for (float fy : { 0.085f, 0.915f })
    {
        auto slot = juce::Rectangle<float> (sw, sh).withCentre ({ a.getCentreX(), a.getY() + a.getHeight() * fy });
        g.setColour (juce::Colour (0xff121214));
        g.fillRoundedRectangle (slot, sw * 0.5f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (slot.translated (0, 1.0f), sw * 0.5f, 1.0f);
        drawScrew (g, slot.getCentre(), sw * 0.42f, fy < 0.5f ? 0.35f : 1.1f);
    }
}

void drawRecess (juce::Graphics& g, juce::Rectangle<float> a, float corner, juce::Colour fill)
{
    g.setColour (fill);
    g.fillRoundedRectangle (a, corner);
    // inner shadow from the top, light catch at the bottom edge
    juce::ColourGradient sh (juce::Colours::black.withAlpha (0.55f), a.getX(), a.getY(),
                             juce::Colours::transparentBlack, a.getX(), a.getY() + std::min (10.0f, a.getHeight() * 0.4f), false);
    g.setGradientFill (sh);
    g.fillRoundedRectangle (a, corner);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.drawRoundedRectangle (a, corner, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.drawLine (a.getX() + corner, a.getBottom() + 0.5f, a.getRight() - corner, a.getBottom() + 0.5f, 1.0f);
}

void drawBezel (juce::Graphics& g, juce::Rectangle<float> o, float t, float corner)
{
    // drop shadow on the faceplate
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (o.translated (0, 3.0f).expanded (1.0f), corner + 1.0f);

    juce::ColourGradient grad (juce::Colour (0xff6a6c71), o.getX(), o.getY(), juce::Colour (0xff222326), o.getX(), o.getBottom(), false);
    grad.addColour (0.08, juce::Colour (0xff55575c));
    grad.addColour (0.55, juce::Colour (0xff3a3c40));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (o, corner);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (o.reduced (0.5f), corner, 1.0f);

    auto inner = o.reduced (t);
    juce::ColourGradient lip (juce::Colour (0xff17181a), inner.getX(), inner.getY(), juce::Colour (0xff4a4c50), inner.getX(), inner.getBottom(), false);
    g.setGradientFill (lip);
    g.fillRoundedRectangle (inner.expanded (2.5f), std::max (1.0f, corner - t * 0.5f));
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (inner.expanded (0.5f), std::max (1.0f, corner - t * 0.6f), 1.5f);
}

void drawRoundBezel (juce::Graphics& g, juce::Rectangle<float> o, float t)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (o.translated (0, 3.0f).expanded (1.0f));
    juce::ColourGradient grad (juce::Colour (0xff76787d), o.getX(), o.getY(), juce::Colour (0xff1f2023), o.getRight(), o.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (o);
    auto inner = o.reduced (t);
    juce::ColourGradient lip (juce::Colour (0xff141517), inner.getX(), inner.getY(), juce::Colour (0xff505257), inner.getRight(), inner.getBottom(), false);
    g.setGradientFill (lip);
    g.fillEllipse (inner.expanded (2.0f));
    g.setColour (juce::Colours::black);
    g.drawEllipse (inner, 1.5f);
}

void drawEngraved (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Justification j,
                   float h, juce::Colour col, Weight w, float tracking)
{
    g.setFont (font (h, w, tracking));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawText (s, r.translated (0, 1.0f), j, false);
    g.setColour (col);
    g.drawText (s, r, j, false);
}

void drawSectionFrame (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
{
    const auto f = font (12.0f, Weight::bold, 0.14f);
    const float tw = juce::GlyphArrangement::getStringWidth (f, title) + 12.0f;
    const float x0 = r.getX(), y0 = r.getY(), x1 = r.getRight(), y1 = r.getBottom(), c = 4.0f;

    juce::Path p;
    p.startNewSubPath (x0 + 10.0f + tw, y0);
    p.lineTo (x1 - c, y0);
    p.quadraticTo (x1, y0, x1, y0 + c);
    p.lineTo (x1, y1 - c);
    p.quadraticTo (x1, y1, x1 - c, y1);
    p.lineTo (x0 + c, y1);
    p.quadraticTo (x0, y1, x0, y1 - c);
    p.lineTo (x0, y0 + c);
    p.quadraticTo (x0, y0, x0 + c, y0);
    p.lineTo (x0 + 8.0f, y0);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.strokePath (p, juce::PathStrokeType (1.0f), juce::AffineTransform::translation (0, 1.0f));
    g.setColour (pal::silk.withAlpha (0.42f));
    g.strokePath (p, juce::PathStrokeType (1.0f));

    drawEngraved (g, title, { x0 + 12.0f, y0 - 7.0f, tw, 14.0f }, juce::Justification::centredLeft, 12.0f,
                  pal::silk, Weight::bold, 0.14f);
}

void drawLamp (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour col, bool on)
{
    // chrome bezel
    juce::ColourGradient ring (juce::Colour (0xffe0e0e0), c.x - r, c.y - r, juce::Colour (0xff4a4a4c), c.x + r, c.y + r, false);
    g.setGradientFill (ring);
    g.fillEllipse (c.x - r * 1.35f, c.y - r * 1.35f, r * 2.7f, r * 2.7f);
    g.setColour (juce::Colours::black);
    g.fillEllipse (c.x - r * 1.05f, c.y - r * 1.05f, r * 2.1f, r * 2.1f);

    if (on)
    {
        g.setColour (col.withAlpha (0.22f));
        g.fillEllipse (c.x - r * 2.6f, c.y - r * 2.6f, r * 5.2f, r * 5.2f);
        juce::ColourGradient jewel (col.brighter (0.9f), c.x - r * 0.3f, c.y - r * 0.4f, col.darker (0.4f), c.x + r, c.y + r, true);
        g.setGradientFill (jewel);
    }
    else
    {
        juce::ColourGradient jewel (col.withMultipliedBrightness (0.45f), c.x - r * 0.3f, c.y - r * 0.4f,
                                    col.withMultipliedBrightness (0.12f), c.x + r, c.y + r, true);
        g.setGradientFill (jewel);
    }
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.7f : 0.25f));
    g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.6f, r * 0.5f, r * 0.35f);
}

//==============================================================================
void drawKnob (juce::Graphics& g, juce::Rectangle<float> area, float pos, float start, float end,
               bool bipolar, bool enabled, bool hot)
{
    const auto c = area.getCentre();
    const float R = std::min (area.getWidth(), area.getHeight()) * 0.5f;
    const float angle = start + pos * (end - start);

    // engraved scale on the faceplate
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (float) i / 10.0f * (end - start);
        const bool major = (i % 5 == 0) || (bipolar && i == 5);
        const float r0 = R * (major ? 0.83f : 0.88f), r1 = R * 0.98f;
        juce::Line<float> l (c.getPointOnCircumference (r0, a), c.getPointOnCircumference (r1, a));
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawLine (l.getStartX(), l.getStartY() + 1.0f, l.getEndX(), l.getEndY() + 1.0f, major ? 1.6f : 1.0f);
        g.setColour (pal::silk.withAlpha (major ? 0.9f : 0.6f));
        g.drawLine (l, major ? 1.6f : 1.0f);
    }

    const float rk = R * 0.76f;    // skirt
    const float rb = rk * 0.80f;   // knurled body
    const float rc = rb * 0.82f;   // cap

    juce::Path disc;
    disc.addEllipse (c.x - rk, c.y - rk, rk * 2.0f, rk * 2.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.7f), (int) std::max (3.0f, rk * 0.35f), { 0, (int) std::max (2.0f, rk * 0.16f) })
        .drawForPath (g, disc);

    // metal skirt
    juce::ColourGradient skirt (juce::Colour (0xffeeeeec), c.x - rk * 0.7f, c.y - rk * 0.8f,
                                juce::Colour (0xff5a5b5e), c.x + rk * 0.7f, c.y + rk * 0.9f, false);
    skirt.addColour (0.5, juce::Colour (0xffa3a4a6));
    g.setGradientFill (skirt);
    g.fillPath (disc);
    g.setColour (juce::Colour (0xff1c1c1e));
    g.drawEllipse (c.x - rk, c.y - rk, rk * 2.0f, rk * 2.0f, 1.0f);

    // pointer notch on the skirt
    g.setColour (juce::Colour (0xff1a1a1a));
    g.drawLine ({ c.getPointOnCircumference (rb * 1.02f, angle), c.getPointOnCircumference (rk * 0.97f, angle) }, std::max (1.6f, rk * 0.07f));

    // knurled bakelite body
    g.setColour (juce::Colour (0xff0d0d0e));
    g.fillEllipse (c.x - rb, c.y - rb, rb * 2.0f, rb * 2.0f);
    const int teeth = 40;
    for (int i = 0; i < teeth; ++i)
    {
        const float a = (float) i / (float) teeth * juce::MathConstants<float>::twoPi + angle;
        const float light = 0.5f + 0.5f * std::cos (a + juce::MathConstants<float>::pi * 0.25f);
        g.setColour (juce::Colour (0xff1a1a1c).interpolatedWith (juce::Colour (0xff4a4a4e), light * 0.75f));
        g.drawLine ({ c.getPointOnCircumference (rb * 0.86f, a), c.getPointOnCircumference (rb * 0.99f, a) }, std::max (1.0f, rb * 0.045f));
    }

    // cap
    juce::ColourGradient cap (juce::Colour (0xff3c3c40), c.x, c.y - rc, juce::Colour (0xff0c0c0d), c.x, c.y + rc, false);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - rc, c.y - rc, rc * 2.0f, rc * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawEllipse (c.x - rc, c.y - rc, rc * 2.0f, rc * 2.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.fillEllipse (c.x - rc * 0.75f, c.y - rc * 0.85f, rc * 1.3f, rc * 0.75f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawEllipse (c.x - rc * 0.55f, c.y - rc * 0.55f, rc * 1.1f, rc * 1.1f, 1.0f);

    // indicator line
    const auto lineCol = hot ? pal::amber : pal::silk;
    g.setColour (lineCol);
    juce::Path ind;
    ind.startNewSubPath (c.getPointOnCircumference (rc * 0.22f, angle));
    ind.lineTo (c.getPointOnCircumference (rb * 0.96f, angle));
    g.strokePath (ind, juce::PathStrokeType (std::max (1.8f, rk * 0.075f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (! enabled)
    {
        g.setColour (pal::faceBottom.withAlpha (0.55f));
        g.fillEllipse (c.x - rk, c.y - rk, rk * 2.0f, rk * 2.0f);
    }
}

void drawPushCap (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool on, bool hover, juce::Colour lamp)
{
    g.setColour (juce::Colour (0xff111113));
    g.fillRoundedRectangle (r, 3.5f);
    auto cap = r.reduced (2.0f);

    if (on)
    {
        g.setColour (lamp.withAlpha (0.20f));
        g.fillRoundedRectangle (r.expanded (2.0f), 5.0f);
        juce::ColourGradient lit (lamp.brighter (0.55f), cap.getX(), cap.getY(), lamp.darker (0.15f), cap.getX(), cap.getBottom(), false);
        g.setGradientFill (lit);
        g.fillRoundedRectangle (cap, 2.5f);
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawLine (cap.getX() + 2.0f, cap.getY() + 1.0f, cap.getRight() - 2.0f, cap.getY() + 1.0f, 1.0f);
    }
    else
    {
        juce::ColourGradient off (juce::Colour (hover ? 0xff4a4b50 : 0xff3e3f44), cap.getX(), cap.getY(),
                                  juce::Colour (0xff222326), cap.getX(), cap.getBottom(), false);
        g.setGradientFill (off);
        g.fillRoundedRectangle (cap, 2.5f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawLine (cap.getX() + 2.0f, cap.getY() + 1.0f, cap.getRight() - 2.0f, cap.getY() + 1.0f, 1.0f);
    }

    const float h = std::min (14.0f, cap.getHeight() * 0.52f);
    g.setFont (font (h, Weight::bold, 0.08f));
    if (on)
    {
        g.setColour (juce::Colour (0xff2b1a06));
        g.drawFittedText (text, cap.toNearestInt().reduced (3, 0), juce::Justification::centred, 1, 0.8f);
    }
    else
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawFittedText (text, cap.toNearestInt().reduced (3, 0).translated (0, 1), juce::Justification::centred, 1, 0.8f);
        g.setColour (hover ? pal::silk : pal::silkDim);
        g.drawFittedText (text, cap.toNearestInt().reduced (3, 0), juce::Justification::centred, 1, 0.8f);
    }
}

void drawMetalButton (juce::Graphics& g, juce::Rectangle<float> r, bool down, bool hover)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0, 1.5f), 3.0f);
    juce::ColourGradient grad (juce::Colour (down ? 0xff3a3b3f : (hover ? 0xff6c6e73 : 0xff606267)), r.getX(), r.getY(),
                               juce::Colour (down ? 0xff505257 : 0xff38393d), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colour (0xff141416));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    if (! down)
    {
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawLine (r.getX() + 3.0f, r.getY() + 1.0f, r.getRight() - 3.0f, r.getY() + 1.0f, 1.0f);
    }
}

//==============================================================================
void VUMeter::setLevel (float rms)
{
    const float dbfs = 20.0f * std::log10 (std::max (1.0e-6f, rms));
    const float vu = juce::jlimit (-24.0f, 4.0f, dbfs + 18.0f);       // 0 VU = -18 dBFS
    needle += (vu - needle) * 0.11f;                                   // ~300 ms integration at 30 Hz
    if (dbfs > -3.0f) { peakTimer = 30; }
    else if (peakTimer > 0) --peakTimer;
    repaint();
}

static float vuPos (float db)
{
    // classic VU scale: deflection proportional to voltage between -20 and +3 VU
    auto v = [] (float d) { return std::pow (10.0f, d / 20.0f); };
    return (v (juce::jlimit (-20.0f, 3.0f, db)) - v (-20.0f)) / (v (3.0f) - v (-20.0f));
}

void VUMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawBezel (g, b, 5.0f, 5.0f);
    auto face = b.reduced (5.0f);

    juce::ColourGradient back (pal::vuFace, face.getCentreX(), face.getBottom(), pal::vuFace.darker (0.35f), face.getX(), face.getY(), true);
    g.setGradientFill (back);
    g.fillRoundedRectangle (face, 2.0f);

    g.saveState();
    g.reduceClipRegion (face.toNearestInt());

    const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.38f);
    const float R = face.getHeight() * 1.05f;
    const float a0 = juce::degreesToRadians (-46.0f), a1 = juce::degreesToRadians (46.0f);
    auto angleFor = [&] (float db) { return a0 + vuPos (db) * (a1 - a0); };

    // red zone
    juce::Path red;
    red.addCentredArc (pivot.x, pivot.y, R * 0.93f, R * 0.93f, 0, angleFor (0.0f), angleFor (3.0f), true);
    g.setColour (juce::Colour (0xffc8321f));
    g.strokePath (red, juce::PathStrokeType (face.getHeight() * 0.07f));
    juce::Path arc;
    arc.addCentredArc (pivot.x, pivot.y, R * 0.89f, R * 0.89f, 0, a0, a1, true);
    g.setColour (juce::Colour (0xff1e1a14));
    g.strokePath (arc, juce::PathStrokeType (1.0f));

    const float ticks[] = { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 };
    for (auto t : ticks)
    {
        const float a = angleFor (t);
        const bool major = (t == -20 || t == -10 || t == -7 || t == -5 || t == -3 || t == 0 || t == 3);
        g.setColour (t > 0 ? juce::Colour (0xffb32818) : juce::Colour (0xff1e1a14));
        g.drawLine ({ pivot.getPointOnCircumference (R * 0.89f, a), pivot.getPointOnCircumference (R * (major ? 0.99f : 0.95f), a) }, major ? 1.4f : 1.0f);
        if (major)
        {
            auto p = pivot.getPointOnCircumference (R * 1.07f, a);
            g.setFont (font (std::max (8.0f, face.getHeight() * 0.13f), Weight::semibold, 0.0f));
            g.drawText (t > 0 ? "+" + juce::String ((int) t) : juce::String ((int) std::abs (t)),
                        juce::Rectangle<float> (24, 12).withCentre (p), juce::Justification::centred);
        }
    }

    g.setColour (juce::Colour (0xff1e1a14));
    g.setFont (font (face.getHeight() * 0.24f, Weight::bold, 0.06f));
    g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.42f).withHeight (face.getHeight() * 0.3f), juce::Justification::centred);

    // needle
    const float na = a0 + vuPos (needle) * (a1 - a0);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawLine ({ pivot.translated (2.0f, 2.0f), pivot.getPointOnCircumference (R * 0.97f, na).translated (2.0f, 2.0f) }, 1.4f);
    g.setColour (juce::Colour (0xff141210));
    g.drawLine ({ pivot, pivot.getPointOnCircumference (R * 0.97f, na) }, 1.4f);

    // pivot cover
    g.setColour (juce::Colour (0xff17171a));
    g.fillEllipse (juce::Rectangle<float> (face.getHeight() * 0.9f, face.getHeight() * 0.9f).withCentre (pivot.translated (0, -face.getHeight() * 0.05f)));
    g.restoreState();

    // peak lamp
    drawLamp (g, { face.getRight() - 9.0f, face.getY() + 9.0f }, 3.2f, pal::red, peakTimer > 0);

    // glass reflection
    juce::ColourGradient glare (juce::Colours::white.withAlpha (0.16f), face.getX(), face.getY(),
                                juce::Colours::transparentWhite, face.getX() + face.getWidth() * 0.45f, face.getBottom(), false);
    g.setGradientFill (glare);
    g.fillRoundedRectangle (face, 2.0f);
}
} // namespace srr::ui::hw
