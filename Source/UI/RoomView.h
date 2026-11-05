#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Acoustics/Location.h"
#include "Acoustics/RenderEngine.h"

namespace srr
{
class SubRooRoomsProcessor;

namespace ui
{
// Isometric "doll-house" / 2D plan of the current location. Click a square
// to move the speaker, an egg to move the mic, a door to open/close it.
class RoomView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit RoomView (SubRooRoomsProcessor&);

    void setIsometric (bool shouldBeIso);
    bool isIsometric() const { return iso; }
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Hit
    {
        enum Kind { None, Source, Mic, Door } kind = None;
        int index = -1;
        bool operator== (const Hit& o) const { return kind == o.kind && index == o.index; }
        bool operator!= (const Hit& o) const { return ! (*this == o); }
    };

    struct BoxItem
    {
        float x0, y0, x1, y1, z0, z1;
        juce::Colour colour;
        float alpha = 1.0f;
        int portal = -1;      // for hover highlight
        float depth = 0;
    };

    juce::Point<float> project (Vec3 p) const;
    juce::Point<float> project (float x, float y, float z) const { return project (Vec3 { x, y, z }); }
    void computeTransform();
    void buildBoxes();
    Hit hitTest (juce::Point<float> p) const;
    juce::Path floorQuad (float x0, float y0, float x1, float y1, float z = 0.0f) const;
    juce::Path portalFootprint (int portalIndex, float grow) const;

    void drawFloors (juce::Graphics&);
    void drawRoomNames (juce::Graphics&);
    void drawBox (juce::Graphics&, const BoxItem&);
    void drawPath (juce::Graphics&);
    void drawSpots (juce::Graphics&);
    void drawHover (juce::Graphics&);
    void drawOverlay (juce::Graphics&);

    SubRooRoomsProcessor& proc;
    LocationPtr loc;
    int src = -1, mic = -1;
    uint32_t doors = 0;
    RenderInfo info;
    bool rendering = false;

    bool iso = true;
    float scale = 40.0f;
    juce::Point<float> offset;
    float wallVis = 0.85f;
    std::vector<BoxItem> boxes;

    Hit hover;
    float hoverDelta = 0, currentLevel = 0;
    bool levelValid = false;
};
} // namespace ui
} // namespace srr
