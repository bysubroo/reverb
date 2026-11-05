#include "Materials.h"
#include <map>

namespace srr
{
namespace
{
    struct Entry { const char* name; BandArray values; };

    const Entry absorptionTable[] = {
        { "concrete",        { 0.01f, 0.02f, 0.02f, 0.02f, 0.03f, 0.04f, 0.05f } },
        { "plaster",         { 0.02f, 0.02f, 0.03f, 0.04f, 0.05f, 0.05f, 0.06f } },
        { "drywall",         { 0.29f, 0.10f, 0.05f, 0.04f, 0.07f, 0.09f, 0.10f } },
        { "brick",           { 0.03f, 0.03f, 0.03f, 0.04f, 0.05f, 0.07f, 0.07f } },
        { "stone",           { 0.02f, 0.02f, 0.03f, 0.04f, 0.05f, 0.05f, 0.05f } },
        { "tile",            { 0.01f, 0.01f, 0.01f, 0.02f, 0.02f, 0.02f, 0.03f } },
        { "parquet",         { 0.04f, 0.04f, 0.07f, 0.06f, 0.06f, 0.07f, 0.07f } },
        { "wood_floor",      { 0.15f, 0.11f, 0.10f, 0.07f, 0.06f, 0.07f, 0.07f } },
        { "wood_panel",      { 0.28f, 0.22f, 0.17f, 0.09f, 0.10f, 0.11f, 0.11f } },
        { "carpet",          { 0.08f, 0.24f, 0.57f, 0.69f, 0.71f, 0.73f, 0.75f } },
        { "carpet_thin",     { 0.02f, 0.06f, 0.14f, 0.37f, 0.60f, 0.65f, 0.65f } },
        { "linoleum",        { 0.02f, 0.03f, 0.03f, 0.03f, 0.03f, 0.02f, 0.02f } },
        { "glass",           { 0.35f, 0.25f, 0.18f, 0.12f, 0.07f, 0.04f, 0.03f } },
        { "curtains",        { 0.07f, 0.31f, 0.49f, 0.75f, 0.70f, 0.60f, 0.55f } },
        { "acoustic_panel",  { 0.30f, 0.65f, 0.95f, 1.00f, 1.00f, 0.95f, 0.95f } },
        { "acoustic_tile",   { 0.50f, 0.70f, 0.60f, 0.70f, 0.70f, 0.50f, 0.50f } },
        { "metal",           { 0.04f, 0.04f, 0.05f, 0.05f, 0.05f, 0.05f, 0.05f } },
        { "car_interior",    { 0.15f, 0.25f, 0.40f, 0.55f, 0.60f, 0.62f, 0.65f } },
        { "car_headliner",   { 0.10f, 0.30f, 0.55f, 0.70f, 0.75f, 0.75f, 0.75f } },
        { "grass",           { 0.11f, 0.26f, 0.60f, 0.69f, 0.92f, 0.99f, 0.99f } },
        { "asphalt",         { 0.02f, 0.03f, 0.03f, 0.03f, 0.03f, 0.04f, 0.05f } },
        { "gravel",          { 0.25f, 0.60f, 0.65f, 0.70f, 0.75f, 0.80f, 0.80f } },
        { "open",            { 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f } },
    };

    const Entry tlTable[] = {
        { "opening",         { 0, 0, 0, 0, 0, 0, 0 } },
        { "open",            { 0, 0, 0, 0, 0, 0, 0 } },
        { "interior",        { 14, 17, 20, 22, 24, 25, 25 } },
        { "glass_door",      { 16, 20, 24, 27, 28, 27, 28 } },
        { "entrance",        { 24, 28, 32, 35, 37, 38, 38 } },
        { "balcony",         { 20, 24, 28, 32, 33, 32, 33 } },
        { "studio",          { 32, 40, 48, 54, 58, 60, 60 } },
        { "studio_window",   { 28, 36, 44, 50, 54, 55, 56 } },
        { "car",             { 18, 22, 27, 31, 35, 38, 40 } },
        { "car_glass",       { 16, 20, 25, 30, 33, 32, 34 } },
        { "trunk",           { 16, 20, 24, 28, 32, 34, 36 } },
        { "garage",          { 12, 15, 18, 21, 24, 26, 28 } },
        { "lift",            { 18, 22, 26, 30, 32, 33, 34 } },
        { "window",          { 20, 23, 27, 31, 33, 31, 33 } },
        { "drywall",         { 22, 30, 38, 44, 46, 42, 46 } },
        { "brick",           { 36, 40, 45, 50, 55, 58, 60 } },
        { "concrete",        { 40, 45, 50, 55, 60, 63, 65 } },
        { "facade",          { 38, 42, 46, 50, 54, 56, 58 } },
        { "glass_partition", { 22, 26, 30, 34, 36, 34, 36 } },
        { "car_body",        { 16, 20, 25, 30, 34, 37, 40 } },
        { "seat",            {  6,  8, 10, 12, 14, 15, 16 } },
        { "curtain",         {  2,  3,  5,  7,  9, 10, 11 } },
    };

    const Entry deviceTable[] = {
        { "none",    {   0,   0,  0,  0,  0,  0,   0 } },
        { "tv",      { -20,  -9, -2,  1,  2, -1, -10 } },
        { "radio",   {  -6,   1,  2,  0,  1, -2,  -9 } },
        { "phone",   { -30, -20, -6,  2,  3, -4, -30 } },
        { "laptop",  { -24, -14, -4,  1,  2,  0,  -8 } },
        { "pa",      {  -8,  -2,  0,  1,  1, -1,  -6 } },
        { "monitor", {   0,   0,  0,  0,  0,  0,   0 } },
        { "megaphone", { -30, -18, -4, 4,  6, -2, -24 } },
    };

    template <size_t N>
    bool lookupIn (const Entry (&table)[N], const juce::String& name, BandArray& out)
    {
        for (auto& e : table)
            if (name.equalsIgnoreCase (e.name)) { out = e.values; return true; }
        return false;
    }
}

bool lookupAbsorption (const juce::String& name, BandArray& out)       { return lookupIn (absorptionTable, name, out); }
bool lookupTransmissionLoss (const juce::String& name, BandArray& out) { return lookupIn (tlTable, name, out); }
bool lookupDeviceEq (const juce::String& name, BandArray& out)         { return lookupIn (deviceTable, name, out); }

BandArray sourceDirectivityFor (const juce::String& kind)
{
    if (kind == "tv" || kind == "laptop" || kind == "monitor")
        return { 0.0f, 0.1f, 0.3f, 0.5f, 0.7f, 0.8f, 0.9f };
    if (kind == "pa" || kind == "radio")
        return { 0.0f, 0.05f, 0.15f, 0.3f, 0.45f, 0.6f, 0.7f };
    if (kind == "phone" || kind == "omni")
        return bandsFill (0.0f);
    // human talker
    return { 0.0f, 0.1f, 0.2f, 0.4f, 0.6f, 0.75f, 0.85f };
}

bool parseBandValue (const juce::var& v, BandArray& out, bool (*lookup) (const juce::String&, BandArray&))
{
    if (v.isString())
        return lookup (v.toString(), out);

    if (auto* arr = v.getArray())
    {
        if (arr->size() != kNumBands) return false;
        for (int i = 0; i < kNumBands; ++i)
            out[(size_t) i] = (float) (double) arr->getReference (i);
        return true;
    }

    if (v.isDouble() || v.isInt())
    {
        out = bandsFill ((float) (double) v);
        return true;
    }
    return false;
}

juce::StringArray allMaterialNames()
{
    juce::StringArray s;
    for (auto& e : absorptionTable) s.add (e.name);
    return s;
}

juce::StringArray allTransmissionNames()
{
    juce::StringArray s;
    for (auto& e : tlTable) s.add (e.name);
    return s;
}

} // namespace srr
