#include "Location.h"
#include "Materials.h"
#include <map>
#include <tuple>

namespace srr
{
namespace
{
    juce::var prop (const juce::var& v, const char* name) { return v.getProperty (juce::Identifier (name), juce::var()); }

    float num (const juce::var& v, const char* name, float def)
    {
        auto p = prop (v, name);
        return (p.isDouble() || p.isInt() || p.isInt64()) ? (float) (double) p : def;
    }

    bool boolean (const juce::var& v, const char* name, bool def)
    {
        auto p = prop (v, name);
        return p.isBool() ? (bool) p : ((p.isInt() || p.isDouble()) ? ((double) p != 0.0) : def);
    }

    juce::String str (const juce::var& v, const char* name, const juce::String& def = {})
    {
        auto p = prop (v, name);
        return p.isString() ? p.toString() : def;
    }

    bool readVec3 (const juce::var& v, Vec3& out, float defaultZ)
    {
        if (auto* a = v.getArray())
        {
            if (a->size() < 2) return false;
            out.x = (float) (double) a->getReference (0);
            out.y = (float) (double) a->getReference (1);
            out.z = a->size() > 2 ? (float) (double) a->getReference (2) : defaultZ;
            return true;
        }
        return false;
    }

    struct MaterialResolver
    {
        std::map<juce::String, BandArray> custom;

        bool absorption (const juce::var& v, BandArray& out) const
        {
            if (v.isString())
            {
                auto it = custom.find (v.toString());
                if (it != custom.end()) { out = it->second; return true; }
            }
            return parseBandValue (v, out, &lookupAbsorption);
        }
    };

    // Shared edge between two axis-aligned rectangles.
    struct Adjacency { bool found = false; bool alongX = true; float line = 0, from = 0, to = 0; };

    Adjacency findAdjacency (const Room& a, const Room& b, float eps = 0.06f)
    {
        Adjacency r;
        auto overlapY0 = std::max (a.y0, b.y0), overlapY1 = std::min (a.y1, b.y1);
        auto overlapX0 = std::max (a.x0, b.x0), overlapX1 = std::min (a.x1, b.x1);

        if (overlapY1 - overlapY0 > 0.05f)
        {
            if (std::abs (a.x1 - b.x0) < eps) { r = { true, false, (a.x1 + b.x0) * 0.5f, overlapY0, overlapY1 }; return r; }
            if (std::abs (a.x0 - b.x1) < eps) { r = { true, false, (a.x0 + b.x1) * 0.5f, overlapY0, overlapY1 }; return r; }
        }
        if (overlapX1 - overlapX0 > 0.05f)
        {
            if (std::abs (a.y1 - b.y0) < eps) { r = { true, true, (a.y1 + b.y0) * 0.5f, overlapX0, overlapX1 }; return r; }
            if (std::abs (a.y0 - b.y1) < eps) { r = { true, true, (a.y0 + b.y1) * 0.5f, overlapX0, overlapX1 }; return r; }
        }
        return r;
    }
}

juce::Rectangle<float> Location::planBounds() const
{
    if (rooms.empty()) return { 0, 0, 1, 1 };
    float x0 = rooms[0].x0, y0 = rooms[0].y0, x1 = rooms[0].x1, y1 = rooms[0].y1;
    for (auto& r : rooms)
    {
        x0 = std::min (x0, r.x0); y0 = std::min (y0, r.y0);
        x1 = std::max (x1, r.x1); y1 = std::max (y1, r.y1);
    }
    return { x0, y0, x1 - x0, y1 - y0 };
}

int Location::findRoom (const juce::String& rid) const
{
    for (size_t i = 0; i < rooms.size(); ++i) if (rooms[i].id == rid) return (int) i;
    return -1;
}

int Location::findSource (const juce::String& sid) const
{
    for (size_t i = 0; i < sources.size(); ++i) if (sources[i].id == sid) return (int) i;
    return -1;
}

int Location::findMic (const juce::String& mid) const
{
    for (size_t i = 0; i < mics.size(); ++i) if (mics[i].id == mid) return (int) i;
    return -1;
}

int Location::roomAt (float x, float y) const
{
    // prefer interior rooms over exterior zones
    for (int pass = 0; pass < 2; ++pass)
        for (size_t i = 0; i < rooms.size(); ++i)
            if (rooms[i].exterior == (pass == 1) && rooms[i].contains (x, y, 0.01f))
                return (int) i;
    return -1;
}

juce::String Location::doorStateKey (uint32_t doors) const
{
    juce::String s;
    for (int b = 0; b < numDoorBits; ++b)
        s << (((doors >> (uint32_t) b) & 1u) ? "1" : "0");
    return s.isEmpty() ? juce::String ("-") : s;
}

std::shared_ptr<Location> Location::fromJsonText (const juce::String& text, juce::String& error, const juce::File& packDir)
{
    juce::var json;
    auto res = juce::JSON::parse (text, json);
    if (res.failed()) { error = "JSON: " + res.getErrorMessage(); return {}; }
    return fromJson (json, error, packDir);
}

std::shared_ptr<Location> Location::fromJson (const juce::var& json, juce::String& error, const juce::File& packDirIn)
{
    auto loc = std::make_shared<Location>();
    loc->id = str (json, "id");
    loc->name = str (json, "name", loc->id);
    loc->description = str (json, "description");
    loc->category = str (json, "category");

    if (loc->id.isEmpty()) { error = "location has no id"; return {}; }

    MaterialResolver mat;
    if (auto* customMats = prop (json, "materials").getDynamicObject())
    {
        for (auto& p : customMats->getProperties())
        {
            BandArray a;
            if (parseBandValue (p.value, a, &lookupAbsorption))
                mat.custom[p.name.toString()] = a;
        }
    }

    BandArray tlInterior, tlExterior;
    lookupTransmissionLoss ("brick", tlInterior);
    lookupTransmissionLoss ("facade", tlExterior);
    if (auto wt = prop (json, "wallTL"); wt.isObject())
    {
        parseBandValue (prop (wt, "interior"), tlInterior, &lookupTransmissionLoss);
        parseBandValue (prop (wt, "exterior"), tlExterior, &lookupTransmissionLoss);
    }

    // ---------------------------------------------------------------- rooms
    if (auto* roomsArr = prop (json, "rooms").getArray())
    {
        for (auto& rv : *roomsArr)
        {
            Room r;
            r.id = str (rv, "id");
            r.name = str (rv, "name", r.id);
            r.exterior = boolean (rv, "exterior", false);

            auto* rect = prop (rv, "rect").getArray();
            if (rect == nullptr || rect->size() != 4) { error = "room '" + r.id + "' needs rect [x0,y0,x1,y1]"; return {}; }
            r.x0 = (float) (double) rect->getReference (0);
            r.y0 = (float) (double) rect->getReference (1);
            r.x1 = (float) (double) rect->getReference (2);
            r.y1 = (float) (double) rect->getReference (3);
            if (r.x1 < r.x0) std::swap (r.x0, r.x1);
            if (r.y1 < r.y0) std::swap (r.y0, r.y1);

            r.height = num (rv, "height", r.exterior ? 12.0f : 2.6f);
            r.furnishing = num (rv, "furnishing", r.exterior ? 0.0f : 0.3f);
            r.scattering = num (rv, "scattering", 0.3f);
            r.style = str (rv, "style", r.exterior ? "grass" : "wood");

            BandArray defWall, defFloor, defCeil;
            lookupAbsorption (r.exterior ? "open" : "plaster", defWall);
            lookupAbsorption (r.exterior ? "grass" : "parquet", defFloor);
            lookupAbsorption (r.exterior ? "open" : "plaster", defCeil);

            r.floorA = defFloor; r.ceilingA = defCeil;
            mat.absorption (prop (rv, "floor"), r.floorA);
            if (! r.exterior) mat.absorption (prop (rv, "ceiling"), r.ceilingA);
            BandArray walls = defWall;
            if (! r.exterior) mat.absorption (prop (rv, "walls"), walls);
            r.wallA.fill (walls);

            if (auto ov = prop (rv, "wallOverrides"); ov.isObject())
            {
                const char* names[] = { "west", "east", "south", "north" };
                for (int s = 0; s < 4; ++s)
                    mat.absorption (prop (ov, names[s]), r.wallA[(size_t) s]);
            }

            if (auto od = prop (rv, "outdoorReverb"); od.isObject())
            {
                r.outdoorLevelDb = num (od, "level", -100.0f);
                r.outdoorRt = num (od, "rt", 1.0f);
            }

            if (r.id.isEmpty()) { error = "room without id"; return {}; }
            loc->rooms.push_back (r);
        }
    }

    if (loc->rooms.empty()) { error = "location '" + loc->id + "' has no rooms"; return {}; }

    // ---------------------------------------------------------------- portals
    if (auto* portalsArr = prop (json, "portals").getArray())
    {
        for (auto& pv : *portalsArr)
        {
            Portal p;
            p.id = str (pv, "id");
            p.name = str (pv, "name", p.id);
            auto type = str (pv, "type", "door");
            p.kind = type == "window" ? Portal::Kind::Window : (type == "opening" ? Portal::Kind::Opening : Portal::Kind::Door);
            p.width = num (pv, "width", p.kind == Portal::Kind::Window ? 1.2f : 0.9f);
            p.height = num (pv, "height", p.kind == Portal::Kind::Window ? 1.3f : 2.05f);
            p.sill = num (pv, "sill", p.kind == Portal::Kind::Window ? 0.9f : 0.0f);
            p.style = str (pv, "style", p.kind == Portal::Kind::Window ? "window" : "door");

            lookupTransmissionLoss (p.kind == Portal::Kind::Window ? "window" : "interior", p.tlClosedDb);
            parseBandValue (prop (pv, "tl"), p.tlClosedDb, &lookupTransmissionLoss);

            p.toggleable = p.kind != Portal::Kind::Opening && boolean (pv, "toggle", true);
            p.defaultOpen = p.kind == Portal::Kind::Opening || boolean (pv, "open", false);

            auto* rr = prop (pv, "rooms").getArray();
            if (rr == nullptr || rr->size() != 2) { error = "portal '" + p.id + "' needs two rooms"; return {}; }
            p.roomA = loc->findRoom (rr->getReference (0).toString());
            p.roomB = loc->findRoom (rr->getReference (1).toString());
            if (p.roomA < 0 || p.roomB < 0) { error = "portal '" + p.id + "' references unknown room"; return {}; }

            Vec3 at;
            readVec3 (prop (pv, "at"), at, 0.0f);

            auto adj = findAdjacency (loc->rooms[(size_t) p.roomA], loc->rooms[(size_t) p.roomB]);
            if (! adj.found) { error = "portal '" + p.id + "': rooms do not share a wall"; return {}; }

            p.wallAlongX = adj.alongX;
            auto half = p.width * 0.5f;
            if (adj.alongX)
            {
                p.cy = adj.line;
                p.cx = juce::jlimit (adj.from + half, std::max (adj.from + half, adj.to - half), at.x);
            }
            else
            {
                p.cx = adj.line;
                p.cy = juce::jlimit (adj.from + half, std::max (adj.from + half, adj.to - half), at.y);
            }
            p.width = std::min (p.width, adj.to - adj.from);

            if (p.toggleable && loc->numDoorBits < kMaxDoorBits)
            {
                p.doorBit = loc->numDoorBits++;
                if (p.defaultOpen) loc->defaultDoors |= (1u << (uint32_t) p.doorBit);
            }
            loc->portals.push_back (p);
        }
    }

    // ---------------------------------------------------------------- walls between rooms
    std::vector<std::tuple<juce::String, juce::String, BandArray>> wallOverrides;
    if (auto* wo = prop (json, "wallOverrides").getArray())
    {
        for (auto& w : *wo)
        {
            auto* rr = prop (w, "rooms").getArray();
            BandArray tl;
            if (rr != nullptr && rr->size() == 2 && parseBandValue (prop (w, "tl"), tl, &lookupTransmissionLoss))
                wallOverrides.emplace_back (rr->getReference (0).toString(), rr->getReference (1).toString(), tl);
        }
    }

    for (size_t i = 0; i < loc->rooms.size(); ++i)
    {
        for (size_t j = i + 1; j < loc->rooms.size(); ++j)
        {
            auto& a = loc->rooms[i];
            auto& b = loc->rooms[j];
            auto adj = findAdjacency (a, b);
            if (! adj.found) continue;

            WallLink w;
            w.roomA = (int) i; w.roomB = (int) j;
            w.wallAlongX = adj.alongX;
            w.length = adj.to - adj.from;
            auto mid = (adj.from + adj.to) * 0.5f;
            if (adj.alongX) { w.cx = mid; w.cy = adj.line; } else { w.cx = adj.line; w.cy = mid; }

            float h = std::min (a.exterior ? b.height : a.height, b.exterior ? a.height : b.height);
            if (a.exterior && b.exterior) { w.openAir = true; h = 3.0f; }
            w.cz = h * 0.5f;
            w.area = w.length * h;

            for (auto& p : loc->portals)
                if ((p.roomA == (int) i && p.roomB == (int) j) || (p.roomA == (int) j && p.roomB == (int) i))
                    w.area -= p.area();
            w.area = std::max (w.area, 0.2f);

            w.tlDb = (a.exterior || b.exterior) ? tlExterior : tlInterior;
            if (w.openAir) w.tlDb = bandsFill (0.0f);
            for (auto& [ra, rb, tl] : wallOverrides)
                if ((ra == a.id && rb == b.id) || (ra == b.id && rb == a.id))
                    w.tlDb = tl;

            loc->walls.push_back (w);
        }
    }

    // ---------------------------------------------------------------- sources
    if (auto* srcArr = prop (json, "sources").getArray())
    {
        for (auto& sv : *srcArr)
        {
            SourceSpot s;
            s.id = str (sv, "id");
            s.name = str (sv, "name", s.id);
            s.kind = str (sv, "kind", "talker");
            if (! lookupDeviceEq (s.kind, s.deviceEqDb)) s.deviceEqDb = bandsFill (0.0f);
            parseBandValue (prop (sv, "eq"), s.deviceEqDb, &lookupDeviceEq);

            auto yaw = prop (sv, "yaw");
            if (yaw.isDouble() || yaw.isInt()) s.yawDeg = (float) (double) yaw;

            Vec3 p;
            if (readVec3 (prop (sv, "pos"), p, 1.6f)) s.positions.push_back (p);
            if (auto* plist = prop (sv, "positions").getArray())
                for (auto& pp : *plist)
                    if (readVec3 (pp, p, 2.4f)) s.positions.push_back (p);

            if (s.positions.empty()) { error = "source '" + s.id + "' has no position"; return {}; }

            auto roomName = str (sv, "room");
            s.room = roomName.isNotEmpty() ? loc->findRoom (roomName) : loc->roomAt (s.positions[0].x, s.positions[0].y);
            if (s.room < 0) { error = "source '" + s.id + "' is outside every room"; return {}; }
            loc->sources.push_back (s);
        }
    }

    // ---------------------------------------------------------------- mics
    if (auto* micArr = prop (json, "mics").getArray())
    {
        for (auto& mv : *micArr)
        {
            MicSpot m;
            m.id = str (mv, "id");
            m.name = str (mv, "name", m.id);
            if (! readVec3 (prop (mv, "pos"), m.pos, 1.6f)) { error = "mic '" + m.id + "' has no position"; return {}; }
            auto yaw = prop (mv, "yaw");
            if (yaw.isDouble() || yaw.isInt()) m.yawDeg = (float) (double) yaw;

            auto roomName = str (mv, "room");
            m.room = roomName.isNotEmpty() ? loc->findRoom (roomName) : loc->roomAt (m.pos.x, m.pos.y);
            if (m.room < 0) { error = "mic '" + m.id + "' is outside every room"; return {}; }

            if (std::isnan (m.yawDeg))
            {
                auto c = loc->rooms[(size_t) m.room].centre();
                auto dx = c.x - m.pos.x, dy = c.y - m.pos.y;
                m.yawDeg = (dx * dx + dy * dy) > 0.25f ? radToDeg (std::atan2 (dy, dx)) : 90.0f;
            }
            loc->mics.push_back (m);
        }
    }

    if (loc->sources.empty() || loc->mics.empty()) { error = "location '" + loc->id + "' needs sources and mics"; return {}; }

    // ---------------------------------------------------------------- furniture (drawing only)
    if (auto* fArr = prop (json, "furniture").getArray())
    {
        for (auto& fv : *fArr)
        {
            Furniture f;
            auto* rect = prop (fv, "rect").getArray();
            if (rect == nullptr || rect->size() != 4) continue;
            f.x0 = (float) (double) rect->getReference (0);
            f.y0 = (float) (double) rect->getReference (1);
            f.x1 = (float) (double) rect->getReference (2);
            f.y1 = (float) (double) rect->getReference (3);
            if (f.x1 < f.x0) std::swap (f.x0, f.x1);
            if (f.y1 < f.y0) std::swap (f.y0, f.y1);
            f.h = num (fv, "h", 0.5f);
            f.kind = str (fv, "kind", "box");
            f.room = loc->roomAt ((f.x0 + f.x1) * 0.5f, (f.y0 + f.y1) * 0.5f);
            loc->furniture.push_back (f);
        }
    }

    // ---------------------------------------------------------------- defaults
    if (auto d = prop (json, "defaults"); d.isObject())
    {
        loc->defaultSource = std::max (0, loc->findSource (str (d, "source")));
        loc->defaultMic = std::max (0, loc->findMic (str (d, "mic")));
    }

    // ---------------------------------------------------------------- measured pack
    if (auto pk = prop (json, "pack"); pk.isObject())
    {
        loc->isPack = true;
        loc->packDir = packDirIn;
        loc->irFolder = str (pk, "irFolder", "ir");
        loc->packCalibrationDb = num (pk, "calibrationDb", 0.0f);
        if (auto* rig = prop (pk, "rig").getArray())
            for (auto& c : *rig)
                loc->rig.push_back ({ str (c, "name"), num (c, "az", 0.0f), num (c, "el", 0.0f) });
        if (loc->rig.empty())
            loc->rig = { { "L", 30.0f, 0.0f }, { "R", -30.0f, 0.0f } };
    }

    return loc;
}

} // namespace srr
