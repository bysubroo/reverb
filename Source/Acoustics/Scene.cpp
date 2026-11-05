#include "Scene.h"
#include "Materials.h"
#include <functional>

namespace srr
{
namespace
{
    constexpr float kPortalKappa = 0.5f;     // incidence factor (between diffuse 1/4 and normal 1)
    constexpr float kMinDistance = 0.25f;
    constexpr float kMaxLengthSec = 3.5f;
    constexpr int   kMaxPaths = 5;

    //==========================================================================
    struct RoomAc
    {
        float x0, y0, x1, y1, h;
        std::array<BandArray, 6> beta;  // W, E, S, N, floor, ceiling (specular amplitude)
        BandArray rt {}, rtPhys {};
        BandArray lateGain {};          // steady-state late energy per unit radiated power
        float mfp = 2.0f;
        bool exterior = false;
        bool hasLate = true;
        int maxOrder = 3;
        float volume = 1, surface = 1;
    };

    struct Edge
    {
        int a = -1, b = -1;
        Vec3 point;          // scaled world point on the wall
        bool alongX = true;
        float area = 1;
        BandArray tau {};    // energy transmission
        bool open = false;   // open aperture (diffraction applies)
        int portal = -1;     // -1: wall link
        float lossDb = 0;
        Vec3 planPoint;      // unscaled (for UI)
    };

    struct Ctx
    {
        const Location& loc;
        SceneParams prm;
        Vec3 centre;
        float s = 1;
        std::vector<RoomAc> rooms;
        std::vector<Edge> edges;
        int frames = 0;

        Vec3 scaled (Vec3 p) const { return { centre.x + (p.x - centre.x) * s, centre.y + (p.y - centre.y) * s, p.z * s }; }
    };

    using Env = std::array<std::vector<float>, kNumBands>;

    void envInit (Env& e, int n) { for (auto& b : e) b.assign ((size_t) n, 0.0f); }

    BandArray airAmp (float d)
    {
        BandArray r;
        for (int b = 0; b < kNumBands; ++b) r[(size_t) b] = std::exp (-0.5f * kAirAbsorption[(size_t) b] * d);
        return r;
    }

    BandArray patternAmp (const BandArray& dirBlend, float cosTheta)
    {
        BandArray r;
        auto card = 0.5f + 0.5f * cosTheta;
        for (int b = 0; b < kNumBands; ++b) r[(size_t) b] = (1.0f - dirBlend[(size_t) b]) + dirBlend[(size_t) b] * card;
        return r;
    }

    // mean of pattern^2 over the sphere -> radiated power relative to omni
    BandArray patternPower (const BandArray& d)
    {
        BandArray r;
        for (int b = 0; b < kNumBands; ++b)
        {
            auto x = d[(size_t) b];
            r[(size_t) b] = (1 - x) * (1 - x) + x * (1 - x) + x * x / 3.0f;
        }
        return r;
    }

    //==========================================================================
    RoomAc makeRoomAcoustics (const Ctx& c, const Room& r)
    {
        RoomAc a;
        auto p0 = c.scaled ({ r.x0, r.y0, 0 });
        auto p1 = c.scaled ({ r.x1, r.y1, r.height });
        a.x0 = p0.x; a.y0 = p0.y; a.x1 = p1.x; a.y1 = p1.y; a.h = p1.z;
        a.exterior = r.exterior;

        auto lx = a.x1 - a.x0, ly = a.y1 - a.y0, lz = a.h;
        a.volume = lx * ly * lz;
        float areas[6] = { ly * lz, ly * lz, lx * lz, lx * lz, lx * ly, lx * ly };
        a.surface = 2.0f * (lx * ly + lx * lz + ly * lz);

        const BandArray scatterShape { 0.1f, 0.2f, 0.3f, 0.5f, 0.7f, 0.85f, 1.0f };
        // furniture, people, curtains cover walls and floor; doors, windows and
        // light partitions add some low-frequency (panel) absorption indoors
        const BandArray furnShape { 0.15f, 0.25f, 0.35f, 0.42f, 0.45f, 0.45f, 0.42f };
        const BandArray lfExtra   { 0.09f, 0.05f, 0.02f, 0.0f, 0.0f, 0.0f, 0.0f };
        BandArray absArea = bandsFill (0.0f);

        for (int sIdx = 0; sIdx < 6; ++sIdx)
        {
            const BandArray& alpha = sIdx < 4 ? r.wallA[(size_t) sIdx] : (sIdx == 4 ? r.floorA : r.ceilingA);
            for (int b = 0; b < kNumBands; ++b)
            {
                auto al = std::clamp (alpha[(size_t) b], 0.0f, 1.0f);
                if (! r.exterior)
                {
                    al = 1.0f - (1.0f - al) * (1.0f - lfExtra[(size_t) b]);
                    if (sIdx < 5)
                        al = 1.0f - (1.0f - al) * (1.0f - std::clamp (r.furnishing * furnShape[(size_t) b] * 0.7f, 0.0f, 0.9f));
                }
                auto sc = std::clamp (r.scattering * scatterShape[(size_t) b], 0.0f, 0.95f);
                a.beta[(size_t) sIdx][(size_t) b] = std::sqrt ((1.0f - al) * (1.0f - sc));
                absArea[(size_t) b] += areas[sIdx] * al;
            }
        }

        if (r.exterior)
        {
            a.hasLate = r.outdoorLevelDb > -90.0f;
            a.rtPhys = bandsFill (std::max (0.1f, r.outdoorRt));
            a.rt = a.rtPhys * c.prm.decayScale;
            auto lvl = std::pow (10.0f, r.outdoorLevelDb / 10.0f);
            const BandArray outdoorTilt { 1.0f, 1.0f, 0.9f, 0.8f, 0.6f, 0.4f, 0.2f };
            a.lateGain = outdoorTilt * (lvl / (4.0f * kPi));
            a.mfp = 10.0f;
            a.maxOrder = 1;
            return a;
        }

        // open portals let energy out of the room

        auto roomIndex = (int) (&r - c.loc.rooms.data());
        for (auto& p : c.loc.portals)
            if ((p.roomA == roomIndex || p.roomB == roomIndex) && c.loc.isPortalOpen (p, c.prm.doors))
                absArea += bandsFill (p.area() * c.s * c.s);

        a.mfp = 4.0f * a.volume / a.surface;
        for (int b = 0; b < kNumBands; ++b)
        {
            auto meanAlpha = std::clamp (absArea[(size_t) b] / a.surface, 0.01f, 0.95f);
            auto eyringA = -a.surface * std::log (1.0f - meanAlpha);
            auto t = 0.161f * a.volume / (eyringA + 4.0f * kAirAbsorption[(size_t) b] * a.volume);
            a.rtPhys[(size_t) b] = std::clamp (t, 0.05f, 6.0f);
            a.rt[(size_t) b] = a.rtPhys[(size_t) b] * c.prm.decayScale;
            a.lateGain[(size_t) b] = 4.0f * (1.0f - meanAlpha) / eyringA;
        }
        a.maxOrder = std::clamp ((int) std::round (0.07f * kSpeedOfSound / a.mfp), 2, 4);
        return a;
    }

    //==========================================================================
    // Allen & Berkley image-source enumeration in a shoebox room.
    template <typename Fn>
    void forEachImage (const RoomAc& R, Vec3 src, Vec3 rcv, int maxOrder, Fn&& fn, int excludeSide = -1)
    {
        const float lx = R.x1 - R.x0, ly = R.y1 - R.y0, lz = R.h;
        const Vec3 s { src.x - R.x0, src.y - R.y0, src.z };
        const Vec3 r { rcv.x - R.x0, rcv.y - R.y0, rcv.z };

        // exterior zones: only the ground reflection exists
        const int nx = R.exterior ? 0 : maxOrder;
        const int ny = R.exterior ? 0 : maxOrder;
        const int nz = R.exterior ? 1 : maxOrder;

        for (int u = 0; u < 2; ++u)
        for (int l = -nx; l <= nx; ++l)
        {
            if (R.exterior && u == 1) continue;
            int cx0 = std::abs (l - u), cx1 = std::abs (l);
            if (cx0 + cx1 > maxOrder) continue;
            float ix = (1 - 2 * u) * s.x + 2.0f * (float) l * lx;

            for (int v = 0; v < 2; ++v)
            for (int m = -ny; m <= ny; ++m)
            {
                if (R.exterior && v == 1) continue;
                int cy0 = std::abs (m - v), cy1 = std::abs (m);
                if (cx0 + cx1 + cy0 + cy1 > maxOrder) continue;
                float iy = (1 - 2 * v) * s.y + 2.0f * (float) m * ly;

                for (int w = 0; w < 2; ++w)
                for (int n = -nz; n <= nz; ++n)
                {
                    int cz0 = std::abs (n - w), cz1 = std::abs (n);
                    int order = cx0 + cx1 + cy0 + cy1 + cz0 + cz1;
                    if (order > maxOrder) continue;
                    if (R.exterior && cz1 > 0) continue;   // no ceiling outside
                    float iz = (1 - 2 * w) * s.z + 2.0f * (float) n * lz;

                    const int counts[6] = { cx0, cx1, cy0, cy1, cz0, cz1 };
                    if (excludeSide >= 0 && counts[excludeSide] > 0) continue;
                    BandArray g = bandsFill (1.0f);
                    bool dead = false;
                    for (int sIdx = 0; sIdx < 6 && ! dead; ++sIdx)
                    {
                        if (counts[sIdx] == 0) continue;
                        for (int b = 0; b < kNumBands; ++b)
                            g[(size_t) b] *= std::pow (R.beta[(size_t) sIdx][(size_t) b], (float) counts[sIdx]);
                        dead = bandMax (g) < 1.0e-4f;
                    }
                    if (dead) continue;

                    Vec3 img { ix, iy, iz };
                    Vec3 d = r - img;
                    Vec3 emit { u ? -d.x : d.x, v ? -d.y : d.y, w ? -d.z : d.z };
                    fn (Vec3 { img.x + R.x0, img.y + R.y0, img.z }, order, g, emit.normalised());
                }
            }
        }
    }

    //==========================================================================
    void addDiffuseImpulse (Env& e, const RoomAc& R, const BandArray& power, float t0, float onset0, float onset1, int frames)
    {
        if (! R.hasLate) return;
        const float dt = 1.0f / Scene::envRate;
        for (int b = 0; b < kNumBands; ++b)
        {
            auto T = R.rt[(size_t) b], Tp = R.rtPhys[(size_t) b];
            auto e0 = power[(size_t) b] * R.lateGain[(size_t) b] * 13.82f / Tp;
            auto k = 13.82f / T;
            auto start = std::max (0, (int) (t0 * Scene::envRate));
            for (int f = start; f < frames; ++f)
            {
                auto t = (float) f * dt;
                e[(size_t) b][(size_t) f] += e0 * std::exp (-k * (t - t0)) * smoothstep (onset0, onset1, t);
            }
        }
    }

    // convolve a radiated power envelope with the room's late response (one-pole)
    void addDiffuseDriven (Env& out, const RoomAc& R, const Env& powerIn, int frames)
    {
        if (! R.hasLate) return;
        const float dt = 1.0f / Scene::envRate;
        for (int b = 0; b < kNumBands; ++b)
        {
            auto T = R.rt[(size_t) b], Tp = R.rtPhys[(size_t) b];
            auto a = std::exp (-13.82f * dt / T);
            auto g = R.lateGain[(size_t) b] * 13.82f / Tp * dt;
            float y = 0;
            for (int f = 0; f < frames; ++f)
            {
                y = y * a + powerIn[(size_t) b][(size_t) f] * g;
                out[(size_t) b][(size_t) f] += y;
            }
        }
    }

    void addShifted (Env& out, const Env& in, float delaySec, const BandArray& gain, int frames)
    {
        auto shift = (int) std::round (delaySec * Scene::envRate);
        for (int b = 0; b < kNumBands; ++b)
            for (int f = shift; f < frames; ++f)
                out[(size_t) b][(size_t) f] += in[(size_t) b][(size_t) (f - shift)] * gain[(size_t) b];
    }

    void binEnergy (Env& e, float delay, const BandArray& amp, int frames)
    {
        auto f = (int) (delay * Scene::envRate);
        if (f < 0 || f >= frames) return;
        for (int b = 0; b < kNumBands; ++b)
            e[(size_t) b][(size_t) f] += amp[(size_t) b] * amp[(size_t) b] * Scene::envRate;
    }

    //==========================================================================
    Vec3 insidePoint (const Ctx& c, const Edge& e, int room)
    {
        const auto& R = c.rooms[(size_t) room];
        Vec3 p = e.point;
        const float off = 0.08f;
        if (e.alongX) p.y += ((R.y0 + R.y1) * 0.5f < e.point.y ? -off : off);
        else          p.x += ((R.x0 + R.x1) * 0.5f < e.point.x ? -off : off);
        p.z = std::clamp (p.z, 0.2f, R.h - 0.1f);
        return p;
    }

    // which wall of 'room' an edge sits on (0 W, 1 E, 2 S, 3 N)
    int sideOf (const Ctx& c, const Edge& e, int room)
    {
        const auto& R = c.rooms[(size_t) room];
        if (e.alongX) return std::abs (e.point.y - R.y0) < std::abs (e.point.y - R.y1) ? 2 : 3;
        return std::abs (e.point.x - R.x0) < std::abs (e.point.x - R.x1) ? 0 : 1;
    }

    bool segmentThroughAperture (const Ctx& c, Vec3 a, Vec3 b, const Edge& e)
    {
        if (e.portal < 0) return false;
        const auto& P = c.loc.portals[(size_t) e.portal];
        auto halfW = P.width * c.s * 0.5f;
        auto z0 = P.sill * c.s, z1 = (P.sill + P.height) * c.s;

        if (e.alongX)
        {
            auto denom = b.y - a.y;
            if (std::abs (denom) < 1.0e-6f) return false;
            auto t = (e.point.y - a.y) / denom;
            if (t <= 0 || t >= 1) return false;
            auto x = a.x + (b.x - a.x) * t, z = a.z + (b.z - a.z) * t;
            return std::abs (x - e.point.x) < halfW && z > z0 && z < z1;
        }
        auto denom = b.x - a.x;
        if (std::abs (denom) < 1.0e-6f) return false;
        auto t = (e.point.x - a.x) / denom;
        if (t <= 0 || t >= 1) return false;
        auto y = a.y + (b.y - a.y) * t, z = a.z + (b.z - a.z) * t;
        return std::abs (y - e.point.y) < halfW && z > z0 && z < z1;
    }

    BandArray diffraction (float cosBend)
    {
        BandArray r;
        auto bend = std::max (0.0f, 1.0f - cosBend);
        for (int b = 0; b < kNumBands; ++b)
            r[(size_t) b] = 1.0f / std::sqrt (1.0f + kBandCentres[(size_t) b] / 400.0f * bend * 1.5f);
        return r;
    }

    //==========================================================================
    struct Path { std::vector<int> rooms; std::vector<int> edges; float cost = 0; };

    void findPaths (const Ctx& c, int from, int to, std::vector<Path>& out)
    {
        Path cur;
        cur.rooms.push_back (from);
        std::function<void()> dfs = [&]
        {
            int here = cur.rooms.back();
            if (here == to) { out.push_back (cur); return; }
            if ((int) cur.edges.size() >= 5) return;
            for (int ei = 0; ei < (int) c.edges.size(); ++ei)
            {
                auto& e = c.edges[(size_t) ei];
                int next = e.a == here ? e.b : (e.b == here ? e.a : -1);
                if (next < 0) continue;
                if (std::find (cur.rooms.begin(), cur.rooms.end(), next) != cur.rooms.end()) continue;
                cur.rooms.push_back (next); cur.edges.push_back (ei);
                dfs();
                cur.rooms.pop_back(); cur.edges.pop_back();
            }
        };
        dfs();
    }

    //==========================================================================
    struct SourceSide { float delay; Vec3 pos; BandArray amp; int order; Vec3 image; };

    void processSourcePosition (Ctx& c, Scene& sc, const SourceSpot& spot, Vec3 srcPos, const MicSpot& mic,
                                Vec3 micPos, const BandArray& deviceGain, Env& diffuse,
                                std::vector<DirectionalLate>& dirLate, bool collectPath)
    {
        const auto dirBlend = sourceDirectivityFor (spot.kind);
        const auto power = patternPower (dirBlend) * deviceGain * deviceGain * (4.0f * kPi);
        const int srcRoom = spot.room, micRoom = mic.room;

        auto facingToward = [&] (Vec3 target)
        {
            if (! std::isnan (spot.yawDeg))
            {
                auto y = degToRad (spot.yawDeg);
                return Vec3 { std::cos (y), std::sin (y), srcPos.z > 2.2f ? -0.6f : 0.0f }.normalised();
            }
            if (spot.kind == "pa" && srcPos.z > 2.2f) return Vec3 { 0, 0, -1 };
            auto d = target - srcPos; d.z *= 0.3f;
            return d.normalised();
        };

        // ------------------------------------------------------------------ same room
        if (srcRoom == micRoom)
        {
            const auto& R = c.rooms[(size_t) srcRoom];
            auto facing = facingToward (micPos);
            float directDelay = std::max (kMinDistance, (micPos - srcPos).length()) / kSpeedOfSound;

            forEachImage (R, srcPos, micPos, R.maxOrder, [&] (Vec3 img, int order, const BandArray& g, Vec3 emit)
            {
                auto d = std::max (kMinDistance, (img - micPos).length());
                Arrival a;
                a.delay = d / kSpeedOfSound;
                if (a.delay > 0.6f) return;
                a.dir = (img - micPos).normalised();
                a.amp = g * patternAmp (dirBlend, emit.dot (facing)) * airAmp (d) * deviceGain * (1.0f / d);
                a.isDirect = order == 0;
                sc.arrivals.push_back (a);
            });

            auto on0 = directDelay + 0.3f * R.mfp / kSpeedOfSound;
            auto on1 = directDelay + (float) (R.maxOrder + 1) * 0.8f * R.mfp / kSpeedOfSound;
            addDiffuseImpulse (diffuse, R, power, directDelay, on0, on1, c.frames);

            if (collectPath) sc.mainPath = { spot.position(), mic.pos };
            return;
        }

        // ------------------------------------------------------------------ through portals / walls
        std::vector<Path> paths;
        findPaths (c, srcRoom, micRoom, paths);
        if (paths.empty()) return;

        for (auto& p : paths)
        {
            float len = 0; Vec3 prev = srcPos;
            p.cost = 0;
            for (auto ei : p.edges)
            {
                auto& e = c.edges[(size_t) ei];
                len += (e.point - prev).length(); prev = e.point;
                p.cost += e.lossDb;
            }
            len += (micPos - prev).length();
            p.cost += 20.0f * std::log10 (std::max (1.0f, len));
        }
        std::sort (paths.begin(), paths.end(), [] (auto& a, auto& b) { return a.cost < b.cost; });
        const float best = paths.front().cost;
        paths.erase (std::remove_if (paths.begin(), paths.end(), [best] (auto& p) { return p.cost > best + 24.0f; }), paths.end());
        if ((int) paths.size() > kMaxPaths) paths.resize (kMaxPaths);

        if (collectPath)
        {
            sc.mainPath.clear();
            sc.mainPath.push_back (spot.position());
            for (auto ei : paths.front().edges) sc.mainPath.push_back (c.edges[(size_t) ei].planPoint);
            sc.mainPath.push_back (mic.pos);
            for (auto ei : paths.front().edges)
                if (! c.edges[(size_t) ei].open) sc.throughClosedStructure = true;
        }

        for (auto& path : paths)
        {
            const int k = (int) path.edges.size();
            const auto& e1 = c.edges[(size_t) path.edges[0]];
            const auto& R0 = c.rooms[(size_t) path.rooms[0]];
            const Vec3 P1 = insidePoint (c, e1, path.rooms[0]);
            const auto facing = facingToward (P1);

            // --- stage 0: energy and discrete arrivals at the first portal (inside source room)
            Env inc, incLate;
            envInit (inc, c.frames); envInit (incLate, c.frames);
            std::vector<SourceSide> discrete;
            float directDelayP1 = std::max (kMinDistance, (P1 - srcPos).length()) / kSpeedOfSound;

            forEachImage (R0, srcPos, P1, 3, [&] (Vec3 img, int order, const BandArray& g, Vec3 emit)
            {
                auto d = std::max (kMinDistance, (img - P1).length());
                auto amp = g * patternAmp (dirBlend, emit.dot (facing)) * airAmp (d) * deviceGain * (1.0f / d);
                auto delay = d / kSpeedOfSound;
                binEnergy (inc, delay, amp, c.frames);
                if (order <= 2 && k == 1) discrete.push_back ({ delay, P1, amp, order, img });
                else binEnergy (incLate, delay, amp, c.frames);
                if (order == 0 && k > 1) discrete.push_back ({ delay, P1, amp, 0, img });
            }, sideOf (c, e1, path.rooms[0]));

            {
                auto on0 = directDelayP1 + 0.3f * R0.mfp / kSpeedOfSound;
                auto on1 = directDelayP1 + (float) (R0.maxOrder + 1) * 0.8f * R0.mfp / kSpeedOfSound;
                Env d0; envInit (d0, c.frames);
                addDiffuseImpulse (d0, R0, power, directDelayP1, on0, on1, c.frames);
                for (int b = 0; b < kNumBands; ++b)
                    for (int f = 0; f < c.frames; ++f)
                    {
                        inc[(size_t) b][(size_t) f] += d0[(size_t) b][(size_t) f];
                        incLate[(size_t) b][(size_t) f] += d0[(size_t) b][(size_t) f];
                    }
            }

            // radiated power through first portal
            auto transmit = [] (const Edge& e) { return e.tau * (e.area * kPortalKappa); };
            Env W, Wlate;
            envInit (W, c.frames); envInit (Wlate, c.frames);
            addShifted (W, inc, 0.0f, transmit (e1), c.frames);
            addShifted (Wlate, incLate, 0.0f, transmit (e1), c.frames);

            // discrete chain amplitude factors (k > 1 keeps only the chain direct)
            BandArray chainAmp = bandsFill (1.0f);
            float chainDelay = 0;
            Vec3 lastInDir = (P1 - srcPos).normalised();

            // --- intermediate rooms
            for (int i = 1; i < k; ++i)
            {
                const auto& Ein = c.edges[(size_t) path.edges[(size_t) (i - 1)]];
                const auto& Eout = c.edges[(size_t) path.edges[(size_t) i]];
                const auto& Ri = c.rooms[(size_t) path.rooms[(size_t) i]];
                auto S = insidePoint (c, Ein, path.rooms[(size_t) i]);
                auto Q = insidePoint (c, Eout, path.rooms[(size_t) i]);
                auto r = std::max (kMinDistance, (Q - S).length());

                Env incNext; envInit (incNext, c.frames);
                auto directGain = airAmp (r) * airAmp (r) * (2.0f / (4.0f * kPi * r * r));
                addShifted (incNext, W, r / kSpeedOfSound, directGain, c.frames);
                addDiffuseDriven (incNext, Ri, W, c.frames);

                // chain direct amplitude through Ein, travelling to Eout
                auto inDir = lastInDir;
                auto outDir = (Q - S).normalised();
                auto tEin = transmit (Ein);
                for (int b = 0; b < kNumBands; ++b)
                    chainAmp[(size_t) b] *= std::sqrt (tEin[(size_t) b] * 2.0f / (4.0f * kPi)) / r * airAmp (r)[(size_t) b];
                if (Ein.open) chainAmp *= diffraction (inDir.dot (outDir));
                chainDelay += r / kSpeedOfSound;
                lastInDir = outDir;

                envInit (W, c.frames); envInit (Wlate, c.frames);
                addShifted (W, incNext, 0.0f, transmit (Eout), c.frames);
                addShifted (Wlate, incNext, 0.0f, transmit (Eout), c.frames);
            }

            // --- mic room
            const auto& Elast = c.edges[(size_t) path.edges[(size_t) (k - 1)]];
            const auto& Rk = c.rooms[(size_t) path.rooms[(size_t) k]];
            const Vec3 S = insidePoint (c, Elast, path.rooms[(size_t) k]);
            const float rS = std::max (kMinDistance, (micPos - S).length());

            DirectionalLate dl;
            dl.dir = (S - micPos).normalised();
            envInit (dl.energy, c.frames);
            auto dgain = airAmp (rS) * airAmp (rS) * (2.0f / (4.0f * kPi * rS * rS));
            addShifted (dl.energy, Wlate, rS / kSpeedOfSound, dgain, c.frames);
            dirLate.push_back (std::move (dl));

            addDiffuseDriven (diffuse, Rk, W, c.frames);

            // discrete arrivals: source-side arrivals x mic-side images of the last portal
            const auto tLast = transmit (Elast);
            BandArray radiate;
            for (int b = 0; b < kNumBands; ++b) radiate[(size_t) b] = std::sqrt (tLast[(size_t) b] * 2.0f / (4.0f * kPi));

            const bool losCandidate = k == 1 && Elast.open && segmentThroughAperture (c, srcPos, micPos, Elast);

            struct MicImage { Vec3 img; BandArray g; int order; };
            std::vector<MicImage> micImages;
            forEachImage (Rk, S, micPos, 2, [&] (Vec3 img, int order, const BandArray& g, Vec3)
            {
                micImages.push_back ({ img, g, order });
            }, sideOf (c, Elast, path.rooms[(size_t) k]));

            for (auto& sside : discrete)
            {
                auto inDir = k == 1 ? (S - sside.image).normalised() : lastInDir;
                for (auto& mi : micImages)
                {
                    auto r = std::max (kMinDistance, (micPos - mi.img).length());
                    auto outDir = (micPos - mi.img).normalised();
                    Arrival a;
                    a.amp = sside.amp * chainAmp * radiate * mi.g * airAmp (r) * (1.0f / r);
                    if (Elast.open) a.amp *= diffraction (inDir.dot (outDir));
                    a.delay = sside.delay + chainDelay + r / kSpeedOfSound;
                    a.dir = (mi.img - micPos).normalised();
                    a.isDirect = sside.order == 0 && mi.order == 0;

                    if (a.isDirect && losCandidate)
                    {
                        auto d = std::max (kMinDistance, (micPos - srcPos).length());
                        a.delay = d / kSpeedOfSound;
                        a.dir = (srcPos - micPos).normalised();
                        a.amp = patternAmp (dirBlend, (micPos - srcPos).normalised().dot (facingToward (micPos)))
                                * airAmp (d) * deviceGain * (1.0f / d);
                    }
                    if (a.delay < 0.8f) sc.arrivals.push_back (a);
                }
            }
        }
    }

    void buildEdges (Ctx& c)
    {
        auto& loc = c.loc;
        for (int pi = 0; pi < (int) loc.portals.size(); ++pi)
        {
            auto& p = loc.portals[(size_t) pi];
            Edge e;
            e.a = p.roomA; e.b = p.roomB; e.portal = pi;
            e.alongX = p.wallAlongX;
            e.planPoint = p.centre();
            e.point = c.scaled (p.centre());
            e.area = p.area() * c.s * c.s;
            e.open = loc.isPortalOpen (p, c.prm.doors);
            e.tau = e.open ? bandsFill (1.0f) : dbToPower (p.tlClosedDb * -1.0f);
            e.lossDb = e.open ? 3.0f : p.tlClosedDb[3] + 3.0f;
            c.edges.push_back (e);
        }
        for (auto& w : loc.walls)
        {
            Edge e;
            e.a = w.roomA; e.b = w.roomB;
            e.alongX = w.wallAlongX;
            e.planPoint = { w.cx, w.cy, w.cz };
            e.point = c.scaled ({ w.cx, w.cy, w.cz });
            e.area = w.area * c.s * c.s;
            e.open = w.openAir;
            e.tau = w.openAir ? bandsFill (1.0f) : dbToPower (w.tlDb * -1.0f);
            e.lossDb = w.openAir ? 3.0f : w.tlDb[3] + 3.0f;
            c.edges.push_back (e);
        }
    }

    Scene buildSceneImpl (const Location& loc, const SceneParams& prmIn, bool full)
    {
        Scene sc;
        if (loc.sources.empty() || loc.mics.empty()) return sc;

        SceneParams prm = prmIn;
        prm.source = juce::jlimit (0, (int) loc.sources.size() - 1, prm.source);
        prm.mic = juce::jlimit (0, (int) loc.mics.size() - 1, prm.mic);
        prm.sizeScale = juce::jlimit (0.5f, 2.0f, prm.sizeScale);
        prm.decayScale = juce::jlimit (0.3f, 3.0f, prm.decayScale);

        Ctx c { loc, prm, {}, prm.sizeScale, {}, {}, 0 };
        auto b = loc.planBounds();
        c.centre = { b.getCentreX(), b.getCentreY(), 0 };
        for (auto& r : loc.rooms) c.rooms.push_back (makeRoomAcoustics (c, r));
        buildEdges (c);

        const auto& mic = loc.mics[(size_t) prm.mic];
        const auto& spot = loc.sources[(size_t) prm.source];
        const auto micPos = c.scaled (mic.pos);

        float maxRt = 0;
        for (size_t i = 0; i < c.rooms.size(); ++i)
            if (! c.rooms[i].exterior || c.rooms[i].hasLate)
                maxRt = std::max (maxRt, bandMax (c.rooms[i].rt));

        c.frames = (int) std::ceil (kMaxLengthSec * Scene::envRate) + 2;
        if (! full) c.frames = std::min (c.frames, (int) ((std::min (kMaxLengthSec, maxRt * 1.3f + 0.5f)) * Scene::envRate) + 2);
        sc.envFrames = c.frames;
        envInit (sc.diffuse, c.frames);

        const auto deviceGain = dbToGain (spot.deviceEqDb);
        bool first = true;
        for (auto& pos : spot.positions)
        {
            processSourcePosition (c, sc, spot, c.scaled (pos), mic, micPos, deviceGain, sc.diffuse, sc.directional, first);
            first = false;
        }

        sc.micYawDeg = std::isnan (mic.yawDeg) ? 90.0f : mic.yawDeg;
        sc.micRoomRt = c.rooms[(size_t) mic.room].rt;

        // ------------------------------------------------------------------ summary values
        float eMax = 0;
        for (auto& a : sc.arrivals)
        {
            auto e = 0.0f;
            for (int bb = 1; bb < 6; ++bb) e += a.amp[(size_t) bb] * a.amp[(size_t) bb];
            eMax = std::max (eMax, e);
        }
        float firstT = 1.0e9f;
        for (auto& a : sc.arrivals)
        {
            auto e = 0.0f;
            for (int bb = 1; bb < 6; ++bb) e += a.amp[(size_t) bb] * a.amp[(size_t) bb];
            if (e >= eMax * 0.1f && a.delay < firstT) { firstT = a.delay; sc.firstDir = a.dir; sc.firstDelay = a.delay; }
        }

        double energy = 0;
        float lastArrival = 0;
        for (auto& a : sc.arrivals)
        {
            for (int bb = 1; bb < 6; ++bb) energy += a.amp[(size_t) bb] * a.amp[(size_t) bb] / 5.0;
            lastArrival = std::max (lastArrival, a.delay);
        }

        float peak = 0;
        std::vector<float> lateTotal ((size_t) c.frames, 0.0f);
        for (int f = 0; f < c.frames; ++f)
        {
            float v = 0;
            for (int bb = 0; bb < kNumBands; ++bb)
            {
                v = std::max (v, sc.diffuse[(size_t) bb][(size_t) f]);
                for (auto& d : sc.directional) v = std::max (v, d.energy[(size_t) bb][(size_t) f]);
            }
            lateTotal[(size_t) f] = v;
            peak = std::max (peak, v);
            for (int bb = 1; bb < 6; ++bb)
            {
                double sum = sc.diffuse[(size_t) bb][(size_t) f];
                for (auto& d : sc.directional) sum += d.energy[(size_t) bb][(size_t) f];
                energy += sum / Scene::envRate / 5.0;
            }
        }

        int lastFrame = 0;
        for (int f = c.frames - 1; f >= 0; --f)
            if (lateTotal[(size_t) f] > peak * 1.0e-6f) { lastFrame = f; break; }

        sc.energy = (float) energy;
        sc.lengthSec = juce::jlimit (0.25f, kMaxLengthSec, std::max (lastArrival + 0.05f, (float) lastFrame / Scene::envRate + 0.02f));
        sc.valid = true;
        return sc;
    }
} // namespace

Scene buildScene (const Location& loc, const SceneParams& p)
{
    return buildSceneImpl (loc, p, true);
}

float estimateLevelDb (const Location& loc, const SceneParams& p)
{
    auto sc = buildSceneImpl (loc, p, false);
    return 10.0f * std::log10 (std::max (1.0e-12f, sc.energy));
}

} // namespace srr
