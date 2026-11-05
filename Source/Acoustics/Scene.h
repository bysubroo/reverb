#pragma once

// Geometric/statistical room acoustics.
//
// A Scene is the acoustic "answer" for one speaker spot / mic spot / door
// state: a list of discrete arrivals (direct sound + early specular
// reflections, also through doors/walls), plus late energy envelopes per
// octave band, split into an isotropic (diffuse) part and directional parts
// arriving through portals. Directions are in the world frame, so the same
// Scene can be rendered for any mic orientation or channel layout.

#include "Location.h"
#include <vector>

namespace srr
{
struct Arrival
{
    float delay = 0;     // seconds
    Vec3 dir;            // unit vector from mic toward the arriving sound (world frame)
    BandArray amp {};    // band amplitudes (linear)
    bool isDirect = false;
};

struct DirectionalLate
{
    Vec3 dir;
    std::array<std::vector<float>, kNumBands> energy;  // energy density per second
};

struct SceneParams
{
    int source = 0, mic = 0;
    uint32_t doors = 0;
    float sizeScale = 1.0f;
    float decayScale = 1.0f;
};

struct Scene
{
    static constexpr float envRate = 1000.0f;   // envelope frames per second

    std::vector<Arrival> arrivals;
    int envFrames = 0;
    std::array<std::vector<float>, kNumBands> diffuse;  // isotropic late energy density per second
    std::vector<DirectionalLate> directional;

    Vec3 firstDir { 1, 0, 0 };
    float firstDelay = 0;
    float micYawDeg = 90;
    BandArray micRoomRt {};
    float lengthSec = 1.0f;
    float energy = 0;               // rough broadband energy (relative to direct @ 1 m)
    std::vector<Vec3> mainPath;     // unscaled plan points: source, portals..., mic
    bool throughClosedStructure = false;
    bool valid = false;
};

Scene buildScene (const Location& loc, const SceneParams& p);

// Fast level estimate in dB (same scale as Scene::energy), for UI warnings
float estimateLevelDb (const Location& loc, const SceneParams& p);

} // namespace srr
