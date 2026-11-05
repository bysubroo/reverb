#include "RoomView.h"
#include "LookAndFeel.h"
#include "Plugin/PluginProcessor.h"

namespace srr::ui
{
namespace
{
    constexpr float kCos30 = 0.8660254f;
    constexpr float kSin30 = 0.5f;

    struct FloorStyle { juce::Colour base, line; int pattern; };   // 0 none, 1 planks, 2 tiles, 3 speckle, 4 grass, 5 asphalt

    FloorStyle floorStyle (const juce::String& s, bool exterior)
    {
        if (s == "wood" || s == "parquet")   return { juce::Colour (0xff3b2d23), juce::Colour (0xff4a3a2d), 1 };
        if (s == "tile")                     return { juce::Colour (0xff27303a), juce::Colour (0xff334050), 2 };
        if (s == "carpet")                   return { juce::Colour (0xff2c2536), juce::Colour (0xff352d41), 0 };
        if (s == "concrete")                 return { juce::Colour (0xff2b2b2f), juce::Colour (0xff36363c), 3 };
        if (s == "metal")                    return { juce::Colour (0xff262b30), juce::Colour (0xff323a42), 2 };
        if (s == "grass")                    return { juce::Colour (0xff17261b), juce::Colour (0xff1f3324), 4 };
        if (s == "asphalt")                  return { juce::Colour (0xff1b1c20), juce::Colour (0xff3a3a30), 5 };
        if (s == "pavement")                 return { juce::Colour (0xff24252a), juce::Colour (0xff2f3036), 2 };
        if (s == "car")                      return { juce::Colour (0xff23232b), juce::Colour (0xff2d2d37), 0 };
        if (s == "studio")                   return { juce::Colour (0xff35281f), juce::Colour (0xff433327), 1 };
        return exterior ? FloorStyle { juce::Colour (0xff1c1f24), juce::Colour (0xff262a30), 0 }
                        : FloorStyle { juce::Colour (0xff30302f), juce::Colour (0xff3a3a39), 0 };
    }

    juce::Colour furnitureColour (const juce::String& k)
    {
        if (k == "sofa")    return juce::Colour (0xff4b3f63);
        if (k == "bed")     return juce::Colour (0xff3f4d66);
        if (k == "table")   return juce::Colour (0xff5b4532);
        if (k == "counter") return juce::Colour (0xff4c4f56);
        if (k == "bath")    return juce::Colour (0xffb9c4cc);
        if (k == "seat")    return juce::Colour (0xff3a3a46);
        if (k == "shelf")   return juce::Colour (0xff4a4038);
        if (k == "car")     return juce::Colour (0xff545a66);
        if (k == "desk")    return juce::Colour (0xff53443a);
        if (k == "drums")   return juce::Colour (0xff6a3048);
        if (k == "console") return juce::Colour (0xff2e3036);
        if (k == "bar")     return juce::Colour (0xff5d3a2a);
        return juce::Colour (0xff42424e);
    }

    juce::Colour portalColour (const juce::String& style)
    {
        if (style == "glass" || style == "window") return juce::Colour (0xff9fd8ff);
        if (style == "car" || style == "trunk")    return juce::Colour (0xff5a6270);
        if (style == "garage" || style == "lift")  return juce::Colour (0xff7d848d);
        if (style == "studio")                     return juce::Colour (0xff6d4a35);
        return colours::door;
    }

    bool isGlassy (const juce::String& style) { return style == "glass" || style == "window"; }
}

juce::String upperLatin (const juce::String& in)
{
    // towupper() is locale dependent; make sure Serbian latin letters survive
    juce::String out;
    for (auto t = in.getCharPointer(); ! t.isEmpty(); ++t)
    {
        auto c = *t;
        switch (c)
        {
            case 0x10D: c = 0x10C; break;   // č
            case 0x107: c = 0x106; break;   // ć
            case 0x161: c = 0x160; break;   // š
            case 0x17E: c = 0x17D; break;   // ž
            case 0x111: c = 0x110; break;   // đ
            default: c = juce::CharacterFunctions::toUpperCase (c); break;
        }
        out += juce::String::charToString (c);
    }
    return out;
}

//==============================================================================
RoomView::RoomView (SubRooRoomsProcessor& p) : proc (p)
{
    setRepaintsOnMouseActivity (false);
}

void RoomView::setIsometric (bool b)
{
    if (iso == b) return;
    iso = b;
    computeTransform();
    buildBoxes();
    repaint();
}

void RoomView::resized()
{
    computeTransform();
    buildBoxes();
}

juce::Point<float> RoomView::project (Vec3 p) const
{
    float px, py;
    if (iso) { px = (p.x - p.y) * kCos30; py = -(p.x + p.y) * kSin30 - p.z; }
    else     { px = p.x; py = -p.y; }
    return { offset.x + px * scale, offset.y + py * scale };
}

void RoomView::computeTransform()
{
    if (loc == nullptr || getWidth() <= 0) return;
    auto b = loc->planBounds();
    wallVis = juce::jlimit (0.3f, 0.62f, 0.055f * std::max (b.getWidth(), b.getHeight()));

    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    auto acc = [&] (float x, float y, float z)
    {
        float px, py;
        if (iso) { px = (x - y) * kCos30; py = -(x + y) * kSin30 - z; }
        else     { px = x; py = -y; }
        minX = std::min (minX, px); maxX = std::max (maxX, px);
        minY = std::min (minY, py); maxY = std::max (maxY, py);
    };
    for (auto& r : loc->rooms)
        for (auto z : { 0.0f, iso ? wallVis : 0.0f })
        {
            acc (r.x0, r.y0, z); acc (r.x1, r.y0, z); acc (r.x0, r.y1, z); acc (r.x1, r.y1, z);
        }

    auto area = getLocalBounds().toFloat().reduced (28.0f);
    area.removeFromTop (58.0f);
    area.removeFromBottom (26.0f);
    const float w = std::max (0.1f, maxX - minX), h = std::max (0.1f, maxY - minY);
    scale = std::min (area.getWidth() / w, area.getHeight() / h);
    offset = { area.getCentreX() - (minX + w * 0.5f) * scale, area.getCentreY() - (minY + h * 0.5f) * scale };
}

juce::Path RoomView::floorQuad (float x0, float y0, float x1, float y1, float z) const
{
    juce::Path p;
    p.startNewSubPath (project (x0, y0, z));
    p.lineTo (project (x1, y0, z));
    p.lineTo (project (x1, y1, z));
    p.lineTo (project (x0, y1, z));
    p.closeSubPath();
    return p;
}

juce::Path RoomView::portalFootprint (int pi, float grow) const
{
    const auto& P = loc->portals[(size_t) pi];
    const float hw = P.width * 0.5f;
    if (P.wallAlongX) return floorQuad (P.cx - hw, P.cy - grow, P.cx + hw, P.cy + grow);
    return floorQuad (P.cx - grow, P.cy - hw, P.cx + grow, P.cy + hw);
}

//==============================================================================
void RoomView::buildBoxes()
{
    boxes.clear();
    if (loc == nullptr) return;
    const float t = 0.13f;
    const juce::Colour wallCol (0xff7d7f8c);

    auto interiorNeighbourOnSide = [&] (int ri, int side)
    {
        const auto& r = loc->rooms[(size_t) ri];
        for (int j = 0; j < (int) loc->rooms.size(); ++j)
        {
            if (j == ri) continue;
            const auto& o = loc->rooms[(size_t) j];
            if (o.exterior) continue;
            const float e = 0.06f;
            bool ov = false;
            if (side == Room::South) ov = std::abs (o.y1 - r.y0) < e && std::min (o.x1, r.x1) - std::max (o.x0, r.x0) > 0.1f;
            if (side == Room::West)  ov = std::abs (o.x1 - r.x0) < e && std::min (o.y1, r.y1) - std::max (o.y0, r.y0) > 0.1f;
            if (ov) return true;
        }
        return false;
    };

    for (int ri = 0; ri < (int) loc->rooms.size(); ++ri)
    {
        const auto& r = loc->rooms[(size_t) ri];
        if (r.exterior) continue;
        const float h = std::min (r.height, wallVis);

        for (int side = 0; side < 4; ++side)
        {
            const bool alongX = side == Room::South || side == Room::North;
            const float line = side == Room::West ? r.x0 : side == Room::East ? r.x1 : side == Room::South ? r.y0 : r.y1;
            const float a = alongX ? r.x0 : r.y0, b = alongX ? r.x1 : r.y1;
            const bool low = (side == Room::South || side == Room::West) && ! interiorNeighbourOnSide (ri, side);
            const float hh = (iso && low) ? std::min (h, 0.22f) : h;

            // gaps from portals on this wall
            std::vector<std::pair<float, float>> gaps;
            for (int pi = 0; pi < (int) loc->portals.size(); ++pi)
            {
                const auto& P = loc->portals[(size_t) pi];
                if (P.roomA != ri && P.roomB != ri) continue;
                if (P.wallAlongX != alongX) continue;
                const float pl = alongX ? P.cy : P.cx;
                if (std::abs (pl - line) > 0.1f) continue;
                const float c = alongX ? P.cx : P.cy;
                gaps.push_back ({ c - P.width * 0.5f, c + P.width * 0.5f });

                // the door / window itself (only once: from roomA)
                if (P.roomA != ri || P.kind == Portal::Kind::Opening) continue;
                const bool open = loc->isPortalOpen (P, doors);
                const auto col = portalColour (P.style);
                const float alpha = isGlassy (P.style) ? 0.45f : 1.0f;
                const float leafH = std::min (hh < h ? std::max (hh, wallVis * 0.85f) : h, wallVis);
                const float z0 = P.kind == Portal::Kind::Window ? std::min (leafH * 0.45f, P.sill) : 0.0f;
                if (! open)
                {
                    const float lt = 0.05f;
                    BoxItem bx = alongX ? BoxItem { c - P.width * 0.5f, line - lt, c + P.width * 0.5f, line + lt, z0, leafH, col, alpha, pi, 0 }
                                        : BoxItem { line - lt, c - P.width * 0.5f, line + lt, c + P.width * 0.5f, z0, leafH, col, alpha, pi, 0 };
                    boxes.push_back (bx);
                }
                else if (P.kind == Portal::Kind::Door)
                {
                    // leaf swung 90 degrees into room A
                    const auto& RA = loc->rooms[(size_t) P.roomA];
                    const float into = alongX ? ((RA.y0 + RA.y1) * 0.5f > line ? 1.0f : -1.0f)
                                              : ((RA.x0 + RA.x1) * 0.5f > line ? 1.0f : -1.0f);
                    const float hinge = c - P.width * 0.5f;
                    const float len = P.width * 0.92f, lt = 0.03f;
                    BoxItem bx = alongX ? BoxItem { hinge - lt, std::min (line, line + into * len), hinge + lt, std::max (line, line + into * len), 0.0f, leafH * 0.95f, col, alpha * 0.9f, pi, 0 }
                                        : BoxItem { std::min (line, line + into * len), hinge - lt, std::max (line, line + into * len), hinge + lt, 0.0f, leafH * 0.95f, col, alpha * 0.9f, pi, 0 };
                    boxes.push_back (bx);
                }
            }
            std::sort (gaps.begin(), gaps.end());

            float cur = a;
            auto emit = [&] (float s0, float s1)
            {
                if (s1 - s0 < 0.02f) return;
                BoxItem bx = alongX ? BoxItem { s0, line - t * 0.5f, s1, line + t * 0.5f, 0.0f, hh, wallCol, 1.0f, -1, 0 }
                                    : BoxItem { line - t * 0.5f, s0, line + t * 0.5f, s1, 0.0f, hh, wallCol, 1.0f, -1, 0 };
                boxes.push_back (bx);
            };
            for (auto& g : gaps)
            {
                emit (cur, std::max (cur, g.first));
                cur = std::max (cur, g.second);
            }
            emit (cur, b);
        }
    }

    for (auto& f : loc->furniture)
        boxes.push_back ({ f.x0, f.y0, f.x1, f.y1, 0.0f, std::min (f.h, wallVis * 0.9f), furnitureColour (f.kind), 1.0f, -1, 0 });

    for (auto& bx : boxes) bx.depth = (bx.x0 + bx.x1) * 0.5f + (bx.y0 + bx.y1) * 0.5f;
    std::sort (boxes.begin(), boxes.end(), [] (const BoxItem& a, const BoxItem& b) { return a.depth > b.depth; });
}

//==============================================================================
void RoomView::refresh()
{
    auto newLoc = proc.getCurrentLocation();
    auto newSrc = proc.getSourceIndex();
    auto newMic = proc.getMicIndex();
    auto newDoors = proc.getDoors();
    auto newInfo = proc.getRenderInfo();
    auto newRendering = proc.isRendering();

    bool geometryChanged = newLoc != loc;
    bool stateChanged = geometryChanged || newSrc != src || newMic != mic || newDoors != doors;
    bool infoChanged = newInfo.serial != info.serial || newRendering != rendering;

    if (! (stateChanged || infoChanged)) return;

    loc = newLoc; src = newSrc; mic = newMic; doors = newDoors;
    info = newInfo; rendering = newRendering;
    if (stateChanged)
    {
        levelValid = false;
        if (geometryChanged) computeTransform();
        buildBoxes();
    }
    repaint();
}

//==============================================================================
void RoomView::drawFloors (juce::Graphics& g)
{
    for (int pass = 0; pass < 2; ++pass)
        for (auto& r : loc->rooms)
        {
            if (r.exterior != (pass == 0)) continue;
            auto st = floorStyle (r.style, r.exterior);
            auto quad = floorQuad (r.x0, r.y0, r.x1, r.y1);
            g.setColour (st.base);
            g.fillPath (quad);

            g.saveState();
            g.reduceClipRegion (quad);
            g.setColour (st.line);
            const float step = st.pattern == 1 ? 0.22f : st.pattern == 2 ? 0.45f : st.pattern == 4 ? 0.6f : 1.0f;
            if (st.pattern == 1 || st.pattern == 2 || st.pattern == 4)
            {
                for (float y = r.y0 + step; y < r.y1; y += step)
                    g.drawLine ({ project (r.x0, y, 0), project (r.x1, y, 0) }, 0.8f);
                if (st.pattern == 2)
                    for (float x = r.x0 + step; x < r.x1; x += step)
                        g.drawLine ({ project (x, r.y0, 0), project (x, r.y1, 0) }, 0.8f);
            }
            else if (st.pattern == 3)
            {
                juce::Random rnd (r.id.hashCode());
                for (int i = 0; i < (int) (r.floorArea() * 6); ++i)
                {
                    auto pt = project (r.x0 + rnd.nextFloat() * r.width(), r.y0 + rnd.nextFloat() * r.depth(), 0);
                    g.fillEllipse (pt.x, pt.y, 1.5f, 1.5f);
                }
            }
            else if (st.pattern == 5)
            {
                const bool alongX = r.width() >= r.depth();
                const float mid = alongX ? (r.y0 + r.y1) * 0.5f : (r.x0 + r.x1) * 0.5f;
                juce::Path dash;
                if (alongX) { dash.startNewSubPath (project (r.x0, mid, 0)); dash.lineTo (project (r.x1, mid, 0)); }
                else        { dash.startNewSubPath (project (mid, r.y0, 0)); dash.lineTo (project (mid, r.y1, 0)); }
                float d[] = { 8.0f, 8.0f };
                juce::PathStrokeType (2.0f).createDashedStroke (dash, dash, d, 2);
                g.setColour (juce::Colour (0xff5a5a40));
                g.fillPath (dash);
            }
            g.restoreState();

            if (r.exterior)
            {
                juce::Path outline;
                float d[] = { 4.0f, 5.0f };
                juce::PathStrokeType (1.0f).createDashedStroke (outline, quad, d, 2);
                g.setColour (colours::line.brighter (0.2f));
                g.fillPath (outline);
            }
        }

}

void RoomView::drawRoomNames (juce::Graphics& g)
{
    auto font = uiFont (std::clamp (scale * 0.22f, 9.0f, 13.0f), true);
    g.setFont (font);
    for (auto& r : loc->rooms)
    {
        auto c = r.centre();
        auto pt = iso ? project (c.x, c.y, 0) : project (c.x, r.y1 - 0.3f, 0);
        auto txt = upperLatin (r.name);
        auto w = juce::GlyphArrangement::getStringWidth (font, txt) + 12.0f;
        auto box = juce::Rectangle<float> (w, 17.0f).withCentre (pt);
        if (iso)
        {
            g.setColour (colours::bg.withAlpha (0.55f));
            g.fillRoundedRectangle (box, 4.0f);
        }
        g.setColour ((r.exterior ? colours::textDim : colours::text).withAlpha (iso ? 0.8f : 0.55f));
        g.drawText (txt, box, juce::Justification::centred);
    }
}

void RoomView::drawBox (juce::Graphics& g, const BoxItem& b)
{
    const bool hl = hover.kind == Hit::Door && hover.index == b.portal && b.portal >= 0;
    auto col = hl ? b.colour.interpolatedWith (colours::accent, 0.45f) : b.colour;

    if (! iso)
    {
        g.setColour (col.withAlpha (b.alpha));
        g.fillPath (floorQuad (b.x0, b.y0, b.x1, b.y1));
        return;
    }

    juce::Path top = floorQuad (b.x0, b.y0, b.x1, b.y1, b.z1);
    juce::Path west, south;
    west.startNewSubPath (project (b.x0, b.y0, b.z0)); west.lineTo (project (b.x0, b.y1, b.z0));
    west.lineTo (project (b.x0, b.y1, b.z1)); west.lineTo (project (b.x0, b.y0, b.z1)); west.closeSubPath();
    south.startNewSubPath (project (b.x0, b.y0, b.z0)); south.lineTo (project (b.x1, b.y0, b.z0));
    south.lineTo (project (b.x1, b.y0, b.z1)); south.lineTo (project (b.x0, b.y0, b.z1)); south.closeSubPath();

    g.setColour (col.darker (0.15f).withAlpha (b.alpha));
    g.fillPath (west);
    g.setColour (col.darker (0.45f).withAlpha (b.alpha));
    g.fillPath (south);
    g.setColour (col.brighter (0.35f).withAlpha (b.alpha));
    g.fillPath (top);
    g.setColour (juce::Colours::black.withAlpha (0.25f * b.alpha));
    g.strokePath (top, juce::PathStrokeType (0.6f));
}

void RoomView::drawPath (juce::Graphics& g)
{
    if (info.path.size() < 2 || info.locationId != loc->id || info.source != src || info.mic != mic) return;
    juce::Path p;
    p.startNewSubPath (project (info.path[0].x, info.path[0].y, 0.02f));
    for (size_t i = 1; i < info.path.size(); ++i) p.lineTo (project (info.path[i].x, info.path[i].y, 0.02f));

    auto a = project (info.path.front().x, info.path.front().y, 0);
    auto b = project (info.path.back().x, info.path.back().y, 0);
    juce::ColourGradient grad (colours::accent, a, colours::mic, b, false);

    juce::Path glow;
    juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (glow, p);
    g.setGradientFill (grad);
    g.setOpacity (0.12f);
    g.fillPath (glow);

    juce::Path dashed;
    float d[] = { 5.0f, 6.0f };
    juce::PathStrokeType (1.6f).createDashedStroke (dashed, p, d, 2);
    g.setOpacity (info.throughClosed ? 0.55f : 0.9f);
    g.fillPath (dashed);
    g.setOpacity (1.0f);
}

void RoomView::drawSpots (juce::Graphics& g)
{
    const float sq = 0.22f;
    auto font = uiFont (10.0f, true);

    for (int i = 0; i < (int) loc->sources.size(); ++i)
    {
        const auto& s = loc->sources[(size_t) i];
        const bool active = i == src, hov = hover.kind == Hit::Source && hover.index == i;
        if (s.positions.size() > 1)
        {
            juce::Path link;
            link.startNewSubPath (project (s.positions[0].x, s.positions[0].y, 0));
            for (size_t k = 1; k < s.positions.size(); ++k) link.lineTo (project (s.positions[k].x, s.positions[k].y, 0));
            g.setColour (colours::accent.withAlpha (active ? 0.5f : 0.2f));
            g.strokePath (link, juce::PathStrokeType (1.0f));
        }
        for (auto& p : s.positions)
        {
            auto marker = floorQuad (p.x - sq, p.y - sq, p.x + sq, p.y + sq);
            if (active)
            {
                auto glow = floorQuad (p.x - sq * 2.2f, p.y - sq * 2.2f, p.x + sq * 2.2f, p.y + sq * 2.2f);
                g.setColour (colours::accent.withAlpha (0.18f));
                g.fillPath (glow);
            }
            g.setColour (active ? colours::accent : colours::bg.withAlpha (0.75f));
            g.fillPath (marker);
            g.setColour (colours::accent.withAlpha (active ? 1.0f : (hov ? 0.95f : 0.6f)));
            g.strokePath (marker, juce::PathStrokeType (hov ? 2.2f : 1.4f));

            if (s.isDevice())
            {
                auto c = project (p.x, p.y, 0);
                g.setFont (font);
                g.setColour (active ? colours::bg : colours::accent);
                auto tag = s.kind == "tv" ? "TV" : s.kind == "radio" ? "FM" : s.kind == "phone" ? "TEL"
                         : s.kind == "pa" ? "PA" : s.kind == "laptop" ? "PC" : s.kind.substring (0, 2).toUpperCase();
                g.drawText (tag, juce::Rectangle<float> (30, 12).withCentre (c), juce::Justification::centred);
            }
        }
    }

    for (int i = 0; i < (int) loc->mics.size(); ++i)
    {
        const auto& m = loc->mics[(size_t) i];
        const bool active = i == mic, hov = hover.kind == Hit::Mic && hover.index == i;
        juce::Path egg;
        for (int k = 0; k <= 24; ++k)
        {
            const float a = (float) k / 24.0f * juce::MathConstants<float>::twoPi;
            const float rx = 0.2f, ry = (std::sin (a) > 0 ? 0.28f : 0.22f);
            auto pt = project (m.pos.x + rx * std::cos (a), m.pos.y + ry * std::sin (a), 0);
            if (k == 0) egg.startNewSubPath (pt); else egg.lineTo (pt);
        }
        egg.closeSubPath();
        if (active)
        {
            g.setColour (colours::mic.withAlpha (0.18f));
            g.fillEllipse (juce::Rectangle<float> (scale * 0.9f, scale * 0.9f * (iso ? 0.6f : 1.0f)).withCentre (project (m.pos.x, m.pos.y, 0)));
        }
        g.setColour (active ? colours::mic : colours::bg.withAlpha (0.75f));
        g.fillPath (egg);
        g.setColour (colours::mic.withAlpha (active ? 1.0f : (hov ? 0.95f : 0.6f)));
        g.strokePath (egg, juce::PathStrokeType (hov ? 2.2f : 1.4f));

        if (active)
        {
            const float yaw = (info.locationId == loc->id && info.mic == mic) ? info.micYawDeg : (std::isnan (m.yawDeg) ? 90.0f : m.yawDeg);
            const float a = degToRad (yaw);
            auto from = project (m.pos.x, m.pos.y, 0);
            auto to = project (m.pos.x + std::cos (a) * 0.75f, m.pos.y + std::sin (a) * 0.75f, 0);
            juce::Path arrow;
            arrow.addArrow ({ from, to }, 2.0f, 9.0f, 8.0f);
            g.setColour (colours::mic);
            g.fillPath (arrow);
        }
    }
}

void RoomView::drawHover (juce::Graphics& g)
{
    if (hover.kind == Hit::None) return;
    juce::Point<float> target;
    juce::String name;
    juce::Point<float> from;
    bool haveFrom = false;
    juce::Colour col = colours::accent;

    if (hover.kind == Hit::Source)
    {
        const auto& s = loc->sources[(size_t) hover.index];
        target = project (s.position().x, s.position().y, 0);
        name = s.name;
        if (hover.index != src) { auto p = loc->sources[(size_t) src].position(); from = project (p.x, p.y, 0); haveFrom = true; }
    }
    else if (hover.kind == Hit::Mic)
    {
        const auto& m = loc->mics[(size_t) hover.index];
        target = project (m.pos.x, m.pos.y, 0);
        name = m.name;
        col = colours::mic;
        if (hover.index != mic) { auto p = loc->mics[(size_t) mic].pos; from = project (p.x, p.y, 0); haveFrom = true; }
    }
    else
    {
        const auto& P = loc->portals[(size_t) hover.index];
        target = project (P.cx, P.cy, 0);
        name = P.name + (loc->isPortalOpen (P, doors) ? " (open)" : " (closed)");
        col = colours::door;
        g.setColour (colours::accent.withAlpha (0.35f));
        g.fillPath (portalFootprint (hover.index, 0.18f));
    }

    if (haveFrom)
    {
        juce::Path line;
        line.startNewSubPath (from);
        line.lineTo (target);
        float d[] = { 2.0f, 4.0f };
        juce::PathStrokeType (1.4f).createDashedStroke (line, line, d, 2);
        g.setColour (col.withAlpha (0.85f));
        g.fillPath (line);
    }

    juce::String txt = name;
    const bool showDelta = levelValid && (hover.kind == Hit::Door || haveFrom);
    if (showDelta)
        txt << "   " << (hoverDelta >= 0 ? "+" : "") << juce::String (hoverDelta, 1) << " dB";

    auto font = uiFont (12.0f, true);
    g.setFont (font);
    const float w = juce::GlyphArrangement::getStringWidth (font, txt) + (showDelta && std::abs (hoverDelta) > 12.0f ? 40.0f : 20.0f);
    auto box = juce::Rectangle<float> (w, 22.0f).withCentre (target.translated (0, -24));
    box = box.constrainedWithin (getLocalBounds().toFloat().reduced (4));
    g.setColour (colours::panel.withAlpha (0.94f));
    g.fillRoundedRectangle (box, 5.0f);
    g.setColour (col.withAlpha (0.8f));
    g.drawRoundedRectangle (box, 5.0f, 1.0f);

    auto textArea = box.reduced (8, 0);
    if (showDelta && std::abs (hoverDelta) > 12.0f)
    {
        auto tri = textArea.removeFromLeft (16).withSizeKeepingCentre (14, 12);
        juce::Path p;
        p.addTriangle (tri.getCentreX(), tri.getY(), tri.getRight(), tri.getBottom(), tri.getX(), tri.getBottom());
        g.setColour (colours::warn);
        g.fillPath (p);
        g.setColour (colours::bg);
        g.setFont (uiFont (10.0f, true));
        g.drawText ("!", tri.translated (0, 1), juce::Justification::centred);
        textArea.removeFromLeft (4);
        g.setFont (font);
    }
    g.setColour (colours::text);
    g.drawText (txt, textArea, juce::Justification::centredLeft);
}

void RoomView::drawOverlay (juce::Graphics& g)
{
    auto b = getLocalBounds().reduced (20, 14);
    auto title = b.removeFromTop (24);
    g.setColour (colours::text);
    g.setFont (uiFont (19.0f, true));
    g.drawText (loc->name, title, juce::Justification::centredLeft);

    g.setColour (colours::textDim);
    g.setFont (uiFont (12.0f));
    g.drawFittedText (loc->description, b.removeFromTop (30).withTrimmedRight (200), juce::Justification::topLeft, 2);

    // status badge
    auto badge = getLocalBounds().reduced (16, 16).removeFromBottom (18).removeFromRight (360);
    juce::String st = info.measured ? (info.measuredFallback ? "MEASURED (nearest door state)" : "MEASURED")
                                    : (loc->isPack ? "SIMULATED (no measurement)" : "SIMULATED");
    st << "  -  RT60 1k " << juce::String (info.rt1k, 2) << " s  -  IR " << juce::String (info.irSeconds, 1) << " s";
    g.setColour (rendering ? colours::accent : colours::textDim);
    g.setFont (uiFont (11.0f, true));
    g.drawText ((rendering ? "RENDERING...  " : "") + st, badge, juce::Justification::centredRight);

    auto legend = getLocalBounds().reduced (20, 16).removeFromBottom (18);
    g.setColour (colours::textDim);
    g.setFont (uiFont (11.0f));
    g.drawText ("Click a square to move the speaker, an egg to move the mic, a door to open/close it",
                legend, juce::Justification::centredLeft);
}

void RoomView::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (bg.brighter (0.06f), b.getCentreX(), b.getY(), bg, b.getCentreX(), b.getBottom(), false));
    g.fillRect (b);

    // subtle dot grid
    g.setColour (line.withAlpha (0.35f));
    for (float x = 12; x < b.getRight(); x += 24)
        for (float y = 12; y < b.getBottom(); y += 24)
            g.fillRect (x, y, 1.0f, 1.0f);

    if (loc == nullptr) return;
    drawFloors (g);
    drawPath (g);
    for (auto& bx : boxes) drawBox (g, bx);
    drawRoomNames (g);
    drawSpots (g);
    drawHover (g);
    drawOverlay (g);
}

//==============================================================================
RoomView::Hit RoomView::hitTest (juce::Point<float> p) const
{
    Hit best;
    if (loc == nullptr) return best;
    float bestD = std::max (12.0f, scale * 0.35f);

    for (int i = 0; i < (int) loc->mics.size(); ++i)
    {
        auto d = project (loc->mics[(size_t) i].pos.x, loc->mics[(size_t) i].pos.y, 0).getDistanceFrom (p);
        if (d < bestD) { bestD = d; best = { Hit::Mic, i }; }
    }
    for (int i = 0; i < (int) loc->sources.size(); ++i)
        for (auto& pos : loc->sources[(size_t) i].positions)
        {
            auto d = project (pos.x, pos.y, 0).getDistanceFrom (p);
            if (d < bestD) { bestD = d; best = { Hit::Source, i }; }
        }
    if (best.kind != Hit::None) return best;

    for (int i = 0; i < (int) loc->portals.size(); ++i)
    {
        const auto& P = loc->portals[(size_t) i];
        if (P.doorBit < 0) continue;
        if (portalFootprint (i, 0.35f).contains (p)) return { Hit::Door, i };
        // also allow clicking the drawn leaf (above the floor in iso)
        if (iso)
        {
            auto c0 = project (P.cx, P.cy, 0), c1 = project (P.cx, P.cy, wallVis);
            juce::Rectangle<float> r (c0, c1);
            if (r.expanded (scale * P.width * 0.4f, 4.0f).contains (p)) return { Hit::Door, i };
        }
    }
    return best;
}

void RoomView::mouseMove (const juce::MouseEvent& e)
{
    auto h = hitTest (e.position);
    if (h == hover) return;
    hover = h;
    setMouseCursor (h.kind == Hit::None ? juce::MouseCursor::NormalCursor : juce::MouseCursor::PointingHandCursor);

    if (h.kind != Hit::None && loc != nullptr)
    {
        if (! levelValid) { currentLevel = proc.estimateLevelDb (src, mic, doors); levelValid = true; }
        float cand = currentLevel;
        if (h.kind == Hit::Source && h.index != src) cand = proc.estimateLevelDb (h.index, mic, doors);
        if (h.kind == Hit::Mic && h.index != mic)    cand = proc.estimateLevelDb (src, h.index, doors);
        if (h.kind == Hit::Door)
        {
            const auto& P = loc->portals[(size_t) h.index];
            cand = proc.estimateLevelDb (src, mic, doors ^ (1u << (uint32_t) P.doorBit));
        }
        hoverDelta = cand - currentLevel;
    }
    repaint();
}

void RoomView::mouseExit (const juce::MouseEvent&)
{
    if (hover.kind != Hit::None) { hover = {}; repaint(); }
}

void RoomView::mouseDown (const juce::MouseEvent& e)
{
    auto h = hitTest (e.position);
    if (h.kind == Hit::Source) proc.selectSource (h.index);
    else if (h.kind == Hit::Mic) proc.selectMic (h.index);
    else if (h.kind == Hit::Door) proc.toggleDoor (loc->portals[(size_t) h.index].doorBit);
    hover = {};
    levelValid = false;
    refresh();
    repaint();
}
} // namespace srr::ui
