#pragma once

// SubRoo Rooms — shared math/acoustic primitives.
// World frame: x = east, y = north, z = up (metres). Yaw is measured
// counter-clockwise from +x when seen from above. Azimuth of a channel is
// positive to the LEFT of the listener's front.

#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace srr
{
constexpr float kSpeedOfSound = 343.0f;
constexpr float kPi = 3.14159265358979323846f;
constexpr int kNumBands = 7;

using BandArray = std::array<float, kNumBands>;

constexpr std::array<float, kNumBands> kBandCentres { 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f };

// Air absorption, energy attenuation coefficient m [1/m] (20 C, 50 % RH, ISO 9613-1)
constexpr std::array<float, kNumBands> kAirAbsorption { 1.0e-4f, 3.0e-4f, 6.2e-4f, 1.08e-3f, 2.28e-3f, 7.58e-3f, 2.69e-2f };

inline BandArray bandsFill (float v) { BandArray a; a.fill (v); return a; }

inline BandArray operator* (const BandArray& a, const BandArray& b) { BandArray r; for (int i = 0; i < kNumBands; ++i) r[(size_t) i] = a[(size_t) i] * b[(size_t) i]; return r; }
inline BandArray operator* (const BandArray& a, float s)            { BandArray r; for (int i = 0; i < kNumBands; ++i) r[(size_t) i] = a[(size_t) i] * s; return r; }
inline BandArray operator+ (const BandArray& a, const BandArray& b) { BandArray r; for (int i = 0; i < kNumBands; ++i) r[(size_t) i] = a[(size_t) i] + b[(size_t) i]; return r; }
inline BandArray& operator+= (BandArray& a, const BandArray& b)     { for (int i = 0; i < kNumBands; ++i) a[(size_t) i] += b[(size_t) i]; return a; }
inline BandArray& operator*= (BandArray& a, const BandArray& b)     { for (int i = 0; i < kNumBands; ++i) a[(size_t) i] *= b[(size_t) i]; return a; }

inline BandArray dbToGain (const BandArray& db)   { BandArray r; for (int i = 0; i < kNumBands; ++i) r[(size_t) i] = std::pow (10.0f, db[(size_t) i] / 20.0f); return r; }
inline BandArray dbToPower (const BandArray& db)  { BandArray r; for (int i = 0; i < kNumBands; ++i) r[(size_t) i] = std::pow (10.0f, db[(size_t) i] / 10.0f); return r; }
inline float bandMean (const BandArray& a)        { float s = 0; for (auto v : a) s += v; return s / (float) kNumBands; }
inline float bandMax (const BandArray& a)         { return *std::max_element (a.begin(), a.end()); }

struct Vec3
{
    float x = 0, y = 0, z = 0;

    Vec3() = default;
    constexpr Vec3 (float X, float Y, float Z) : x (X), y (Y), z (Z) {}

    Vec3 operator+ (Vec3 o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator- (Vec3 o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator* (float s) const { return { x * s, y * s, z * s }; }
    Vec3 operator- () const { return { -x, -y, -z }; }
    float dot (Vec3 o) const { return x * o.x + y * o.y + z * o.z; }
    float length() const { return std::sqrt (x * x + y * y + z * z); }
    Vec3 normalised() const { auto l = length(); return l > 1.0e-9f ? Vec3 { x / l, y / l, z / l } : Vec3 { 1, 0, 0 }; }

    // rotate around z by angle (radians), counter-clockwise
    Vec3 rotatedZ (float a) const
    {
        auto c = std::cos (a), s = std::sin (a);
        return { c * x - s * y, s * x + c * y, z };
    }
};

inline float wrapDegrees (float d)
{
    while (d > 180.0f)  d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

inline float degToRad (float d) { return d * kPi / 180.0f; }
inline float radToDeg (float r) { return r * 180.0f / kPi; }

inline float smoothstep (float e0, float e1, float x)
{
    if (e1 <= e0) return x >= e1 ? 1.0f : 0.0f;
    auto t = std::clamp ((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Small, fast deterministic RNG (xorshift128+), used for reproducible tails
struct FastRandom
{
    uint64_t s0, s1;
    explicit FastRandom (uint64_t seed = 0x9E3779B97F4A7C15ull)
    {
        s0 = seed ^ 0xA0761D6478BD642Full;
        s1 = (seed * 0xE7037ED1A0B428DBull) | 1ull;
        for (int i = 0; i < 8; ++i) next();
    }
    uint64_t next()
    {
        auto x = s0; auto const y = s1;
        s0 = y; x ^= x << 23; s1 = x ^ y ^ (x >> 17) ^ (y >> 26);
        return s1 + y;
    }
    // uniform in [-1, 1)
    float bipolar() { return (float) ((next() >> 40) * (1.0 / 8388608.0)) - 1.0f; }
};

inline uint64_t hashCombine (uint64_t h, uint64_t v)
{
    h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    return h;
}

} // namespace srr
