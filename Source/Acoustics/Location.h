#pragma once

#include <optional>

#include "Core.h"
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <memory>
#include <limits>
#include <vector>

namespace srr
{
constexpr int kMaxDoorBits = 12;

struct Room
{
    enum Side { West = 0, East = 1, South = 2, North = 3 };

    juce::String id, name;
    float x0 = 0, y0 = 0, x1 = 1, y1 = 1;
    float height = 2.6f;          // acoustic ceiling height
    bool exterior = false;
    BandArray floorA {}, ceilingA {};
    std::array<BandArray, 4> wallA {};
    float furnishing = 0.3f;      // 0..1, adds soft absorption (sofas, beds, people)
    float scattering = 0.3f;      // 0..1
    float outdoorLevelDb = -100.0f;
    float outdoorRt = 1.0f;
    juce::String style;           // drawing hint: wood, tile, carpet, concrete, grass, asphalt, car ...

    float width() const  { return x1 - x0; }
    float depth() const  { return y1 - y0; }
    float floorArea() const { return width() * depth(); }
    juce::Point<float> centre() const { return { (x0 + x1) * 0.5f, (y0 + y1) * 0.5f }; }
    bool contains (float x, float y, float margin = 0.0f) const
    {
        return x >= x0 - margin && x <= x1 + margin && y >= y0 - margin && y <= y1 + margin;
    }
};

struct Portal
{
    enum class Kind { Door, Window, Opening };

    juce::String id, name;
    Kind kind = Kind::Door;
    int roomA = -1, roomB = -1;
    float cx = 0, cy = 0;         // centre on the wall line
    bool wallAlongX = true;       // true: wall is horizontal in plan (y = const)
    float width = 0.9f, height = 2.05f, sill = 0.0f;
    BandArray tlClosedDb {};
    bool toggleable = true;
    bool defaultOpen = false;
    int doorBit = -1;
    juce::String style;           // door, glass, window, garage, car, lift, studio, trunk

    float area() const   { return width * height; }
    Vec3 centre() const  { return { cx, cy, sill + height * 0.5f }; }
};

struct WallLink
{
    int roomA = -1, roomB = -1;
    float cx = 0, cy = 0, cz = 1.3f;
    bool wallAlongX = true;
    float length = 0, area = 0;
    BandArray tlDb {};
    bool openAir = false;          // exterior <-> exterior: no wall at all
};

struct SourceSpot
{
    juce::String id, name;
    int room = 0;
    std::vector<Vec3> positions;  // >1 for PA / distributed systems
    float yawDeg = std::numeric_limits<float>::quiet_NaN();
    juce::String kind = "talker";
    BandArray deviceEqDb {};

    // Measured packs: a spot without its own recording can be derived from a
    // recorded one (mirrored left/right, level, extra delay, darker).
    struct Derive
    {
        juce::String from;            // id of the recorded source spot
        bool mirror = false;          // swap left/right of the mic rig
        float gainDb = 0, delayMs = 0, lowpassHz = 0;
    };
    std::optional<Derive> derive;

    bool isDevice() const { return kind != "talker"; }
    bool isDerived() const { return derive.has_value(); }
    Vec3 position() const { return positions.empty() ? Vec3() : positions.front(); }
};

struct MicSpot
{
    juce::String id, name;
    int room = 0;
    Vec3 pos;
    float yawDeg = std::numeric_limits<float>::quiet_NaN();
};

struct Furniture
{
    int room = -1;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0, h = 0.5f;
    juce::String kind;
};

struct RigChannel
{
    juce::String name;
    float azDeg = 0, elDeg = 0;
};

struct Location
{
    juce::String id, name, description, category;
    std::vector<Room> rooms;
    std::vector<Portal> portals;
    std::vector<WallLink> walls;
    std::vector<SourceSpot> sources;
    std::vector<MicSpot> mics;
    std::vector<Furniture> furniture;
    int defaultSource = 0, defaultMic = 0;
    uint32_t defaultDoors = 0;
    int numDoorBits = 0;

    // measured location pack
    bool isPack = false;
    juce::File packDir;
    juce::String irFolder = "ir";
    std::vector<RigChannel> rig;
    float packCalibrationDb = 0.0f;

    juce::Rectangle<float> planBounds() const;
    int findRoom (const juce::String& rid) const;
    int findSource (const juce::String& sid) const;
    int findMic (const juce::String& mid) const;
    int roomAt (float x, float y) const;

    bool isPortalOpen (const Portal& p, uint32_t doors) const
    {
        if (p.kind == Portal::Kind::Opening) return true;
        if (p.doorBit < 0) return p.defaultOpen;
        return ((doors >> (uint32_t) p.doorBit) & 1u) != 0;
    }

    juce::String doorStateKey (uint32_t doors) const;

    static std::shared_ptr<Location> fromJson (const juce::var& json, juce::String& error,
                                               const juce::File& packDir = {});
    static std::shared_ptr<Location> fromJsonText (const juce::String& text, juce::String& error,
                                                   const juce::File& packDir = {});
};

using LocationPtr = std::shared_ptr<const Location>;

} // namespace srr
