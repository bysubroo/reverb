#include "MicRig.h"

namespace srr
{
using CT = juce::AudioChannelSet::ChannelType;

ChannelDirection nominalDirection (CT t)
{
    switch (t)
    {
        case juce::AudioChannelSet::left:               return { 30, 0 };
        case juce::AudioChannelSet::right:              return { -30, 0 };
        case juce::AudioChannelSet::centre:             return { 0, 0 };
        case juce::AudioChannelSet::LFE:                return { 0, 0, true };
        case juce::AudioChannelSet::LFE2:               return { 0, 0, true };
        case juce::AudioChannelSet::leftSurround:       return { 110, 0 };
        case juce::AudioChannelSet::rightSurround:      return { -110, 0 };
        case juce::AudioChannelSet::leftCentre:         return { 15, 0 };
        case juce::AudioChannelSet::rightCentre:        return { -15, 0 };
        case juce::AudioChannelSet::centreSurround:     return { 180, 0 };
        case juce::AudioChannelSet::leftSurroundSide:   return { 90, 0 };
        case juce::AudioChannelSet::rightSurroundSide:  return { -90, 0 };
        case juce::AudioChannelSet::leftSurroundRear:   return { 150, 0 };
        case juce::AudioChannelSet::rightSurroundRear:  return { -150, 0 };
        case juce::AudioChannelSet::wideLeft:           return { 60, 0 };
        case juce::AudioChannelSet::wideRight:          return { -60, 0 };
        case juce::AudioChannelSet::topMiddle:          return { 0, 90 };
        case juce::AudioChannelSet::topFrontLeft:       return { 45, 45 };
        case juce::AudioChannelSet::topFrontCentre:     return { 0, 45 };
        case juce::AudioChannelSet::topFrontRight:      return { -45, 45 };
        case juce::AudioChannelSet::topRearLeft:        return { 135, 45 };
        case juce::AudioChannelSet::topRearCentre:      return { 180, 45 };
        case juce::AudioChannelSet::topRearRight:       return { -135, 45 };
        case juce::AudioChannelSet::topSideLeft:        return { 90, 55 };
        case juce::AudioChannelSet::topSideRight:       return { -90, 55 };
        default: break;
    }
    return { 0, 0 };
}

namespace
{
    Vec3 dirFromAzEl (float az, float el)
    {
        auto a = degToRad (az), e = degToRad (el);
        return { std::cos (a) * std::cos (e), std::sin (a) * std::cos (e), std::sin (e) };
    }

    // capsule azimuths for the spaced (AB) array: corners form a quad
    float spacedAzimuth (CT t, bool isStereoLike)
    {
        switch (t)
        {
            case juce::AudioChannelSet::left:               return isStereoLike ? 45.0f : 45.0f;
            case juce::AudioChannelSet::right:              return isStereoLike ? -45.0f : -45.0f;
            case juce::AudioChannelSet::leftSurround:       return 135.0f;
            case juce::AudioChannelSet::rightSurround:      return -135.0f;
            case juce::AudioChannelSet::leftSurroundRear:   return 140.0f;
            case juce::AudioChannelSet::rightSurroundRear:  return -140.0f;
            case juce::AudioChannelSet::leftSurroundSide:   return 90.0f;
            case juce::AudioChannelSet::rightSurroundSide:  return -90.0f;
            default: break;
        }
        return nominalDirection (t).azDeg;
    }

    float coincidentAzimuth (CT t, bool isStereo)
    {
        switch (t)
        {
            case juce::AudioChannelSet::left:               return isStereo ? 50.0f : 40.0f;
            case juce::AudioChannelSet::right:              return isStereo ? -50.0f : -40.0f;
            case juce::AudioChannelSet::leftSurround:       return 125.0f;
            case juce::AudioChannelSet::rightSurround:      return -125.0f;
            case juce::AudioChannelSet::leftSurroundRear:   return 145.0f;
            case juce::AudioChannelSet::rightSurroundRear:  return -145.0f;
            default: break;
        }
        return nominalDirection (t).azDeg;
    }

    bool isCorner (CT t)
    {
        return t == juce::AudioChannelSet::left || t == juce::AudioChannelSet::right
            || t == juce::AudioChannelSet::leftSurround || t == juce::AudioChannelSet::rightSurround
            || t == juce::AudioChannelSet::leftSurroundRear || t == juce::AudioChannelSet::rightSurroundRear;
    }
}

std::vector<Capsule> makeRig (const juce::AudioChannelSet& layout, MicSystem system)
{
    std::vector<Capsule> rig;
    const int n = layout.size();
    const bool stereo = n == 2;

    for (int i = 0; i < n; ++i)
    {
        auto t = layout.getTypeOfChannel (i);
        auto nd = nominalDirection (t);
        Capsule c;
        c.silent = nd.lfe;

        bool spaced = system == MicSystem::Spaced
                   || (system == MicSystem::Hybrid && (isCorner (t) || nd.elDeg > 30.0f));

        if (n == 1)
        {
            c.azDeg = 0; c.elDeg = 0;
            c.pos = {};
            c.facing = { 1, 0, 0 };
            c.baffledOmni = spaced;
            c.firstOrderA = 0.5f;
            rig.push_back (c);
            continue;
        }

        if (spaced)
        {
            c.azDeg = spacedAzimuth (t, stereo);
            c.elDeg = nd.elDeg > 30.0f ? 60.0f : 0.0f;
            auto horiz = dirFromAzEl (c.azDeg, 0);
            if (nd.elDeg > 30.0f)
                c.pos = Vec3 { horiz.x * 0.2f, horiz.y * 0.2f, 0.45f };
            else
                c.pos = horiz * 0.35f;
            c.facing = dirFromAzEl (c.azDeg, c.elDeg);
            c.baffledOmni = true;
        }
        else
        {
            c.azDeg = coincidentAzimuth (t, stereo);
            c.elDeg = nd.elDeg > 30.0f ? 55.0f : 0.0f;
            c.pos = {};
            c.facing = dirFromAzEl (c.azDeg, c.elDeg);
            c.baffledOmni = false;
            c.firstOrderA = stereo ? 0.5f : 0.42f;
        }
        rig.push_back (c);
    }
    return rig;
}

float capsuleGain (const Capsule& c, Vec3 dirMic, int band)
{
    if (c.silent) return 0.0f;
    auto cosT = dirMic.dot (c.facing);
    if (c.baffledOmni)
    {
        static constexpr float shadow[kNumBands] = { 0.0f, 0.05f, 0.12f, 0.25f, 0.4f, 0.55f, 0.65f };
        auto a = shadow[band];
        return (1.0f - a) + a * (0.5f + 0.5f * cosT);
    }
    return c.firstOrderA + (1.0f - c.firstOrderA) * cosT;
}

} // namespace srr
