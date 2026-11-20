#include "PluginEditor.h"
#include <map>
#include "Plugin/PluginProcessor.h"

namespace srr
{
using namespace ui;

SubRooRoomsEditor::SubRooRoomsEditor (SubRooRoomsProcessor& p)
    : AudioProcessorEditor (p), proc (p), roomView (p)
{
    setLookAndFeel (&lnf);
    auto& st = proc.getState();

    addAndMakeVisible (roomView);
    roomView.setOpaque (true);
    addAndMakeVisible (vu);

    for (auto* c : { &locationBox, &presetBox })
    {
        addAndMakeVisible (*c);
        c->setJustificationType (juce::Justification::centredLeft);
    }
    locationBox.setTooltip ("Location");
    presetBox.setTextWhenNothingSelected ("Factory presets");
    rebuildLocationMenu();
    rebuildPresetMenu();

    locationBox.onChange = [this]
    {
        if (ignoreCombo) return;
        auto id = locationBox.getSelectedId();
        if (id > 0) proc.selectLocation (id - 1);
    };
    presetBox.onChange = [this]
    {
        if (ignoreCombo) return;
        auto id = presetBox.getSelectedId();
        if (id > 0) proc.setCurrentProgram (id - 1);
    };

    addAndMakeVisible (packsButton);
    packsButton.setTooltip ("Measured location packs");
    packsButton.onClick = [this] { showPacksMenu(); };

    for (auto* b : { &isoButton, &planButton })
    {
        addAndMakeVisible (*b);
        b->setClickingTogglesState (false);
    }
    isoButton.setToggleState (true, juce::dontSendNotification);
    isoButton.onClick = [this] { roomView.setIsometric (true); isoButton.setToggleState (true, juce::dontSendNotification); planButton.setToggleState (false, juce::dontSendNotification); };
    planButton.onClick = [this] { roomView.setIsometric (false); planButton.setToggleState (true, juce::dontSendNotification); isoButton.setToggleState (false, juce::dontSendNotification); };

    addAndMakeVisible (openAllButton);
    addAndMakeVisible (closeAllButton);
    openAllButton.onClick = [this] { proc.setAllDoors (true); };
    closeAllButton.onClick = [this] { proc.setAllDoors (false); };

    addAndMakeVisible (directSeg);
    directSeg.onSelect = [this] (int i) { proc.setParam (params::direct, i == 0 ? 1.0f : 0.0f); updateModeVisibility(); };
    addAndMakeVisible (micSeg);
    micSeg.onSelect = [this] (int i) { proc.setParam (params::micSystem, (float) i); };
    addAndMakeVisible (angleSeg);
    angleSeg.onSelect = [this] (int i) { proc.setParam (params::angleMode, (float) i); };

    addAndMakeVisible (angleDial);
    angleDial.onGestureStart = [this] { if (auto* q = proc.getState().getParameter (params::angle)) q->beginChangeGesture(); };
    angleDial.onGestureEnd = [this] { if (auto* q = proc.getState().getParameter (params::angle)) q->endChangeGesture(); };
    angleDial.onDrag = [this] (float deg)
    {
        if (auto* q = proc.getState().getParameter (params::angle))
            q->setValueNotifyingHost (q->convertTo0to1 (std::round (deg)));
    };

    addAndMakeVisible (levelRange);
    levelRange.setSliderStyle (juce::Slider::LinearHorizontal);
    levelRange.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    levelRange.setTooltip ("Flat: every position has the same loudness. Natural: real level differences (closing a door makes it quieter).");
    levelAttachment = std::make_unique<SliderAttachment> (st, params::levelRange, levelRange);

    struct K { Knob* k; const char* id; };
    for (auto& k : { K { &spread, params::spread }, K { &mix, params::mix }, K { &predelay, params::predelay },
                     K { &size, params::size }, K { &decay, params::decay }, K { &output, params::output },
                     K { &low, params::lowEq }, K { &high, params::highEq } })
    {
        addAndMakeVisible (*k.k);
        attachments.push_back (std::make_unique<SliderAttachment> (st, k.id, k.k->slider));
    }
    spread.slider.setTooltip ("How tightly the reverb follows each panned input (narrow) or envelops it (wide)");
    mix.slider.setTooltip ("Wet/dry balance (Direct Off)");
    predelay.slider.setTooltip ("Delays all reverb channels (Direct Off)");
    size.slider.setTooltip ("Scales the whole location");
    decay.slider.setTooltip ("Reverb time multiplier");

    setResizable (true, true);
    setResizeLimits (980, 640, 2400, 1500);
    setSize (1240, 780);

    updateModeVisibility();
    startTimerHz (30);
}

SubRooRoomsEditor::~SubRooRoomsEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SubRooRoomsEditor::rebuildLocationMenu()
{
    juce::ScopedValueSetter<bool> svs (ignoreCombo, true);
    locationBox.clear (juce::dontSendNotification);
    auto names = proc.getLibrary().getNames();
    auto cats = proc.getLibrary().getCategories();
    const int numBuiltIn = proc.getLibrary().getNumBuiltIn();

    // built-in locations: flat list with category headings
    juce::String lastCat;
    for (int i = 0; i < std::min (numBuiltIn, names.size()); ++i)
    {
        if (cats[i] != lastCat && cats[i].isNotEmpty())
        {
            locationBox.addSectionHeading (cats[i]);
            lastCat = cats[i];
        }
        locationBox.addItem (names[i], i + 1);
    }

    // packs: one sub-menu per category (a pack library can hold hundreds of rooms)
    if (names.size() > numBuiltIn)
    {
        juce::StringArray order;
        std::map<juce::String, juce::PopupMenu> menus;
        for (int i = numBuiltIn; i < names.size(); ++i)
        {
            auto cat = cats[i].isNotEmpty() ? cats[i] : juce::String ("Packs");
            if (! order.contains (cat)) order.add (cat);
            menus[cat].addItem (i + 1, names[i]);
        }
        auto* root = locationBox.getRootMenu();
        root->addSeparator();
        root->addSectionHeader ("Packs");
        for (auto& cat : order)
            root->addSubMenu (cat + "  (" + juce::String (menus[cat].getNumItems()) + ")", menus[cat]);
    }
    lastLocationCount = names.size();
    locationBox.setSelectedId (proc.getLocationIndex() + 1, juce::dontSendNotification);
}

void SubRooRoomsEditor::rebuildPresetMenu()
{
    juce::ScopedValueSetter<bool> svs (ignoreCombo, true);
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
}

void SubRooRoomsEditor::showPacksMenu()
{
    juce::PopupMenu m;
    auto folder = LocationLibrary::getSharedPacksFolder().isDirectory() ? LocationLibrary::getSharedPacksFolder()
                                                                         : LocationLibrary::getPacksFolder();
    m.addSectionHeader ("Measured location packs");
    m.addItem (1, "Rescan packs");
    m.addItem (2, "Open packs folder");
    auto errs = proc.getLibrary().getErrors();
    if (! errs.isEmpty())
    {
        m.addSeparator();
        m.addSectionHeader ("Problems");
        for (auto& e : errs) m.addItem (-1, e.substring (0, 90), false);
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (packsButton), [this, folder] (int r)
    {
        if (r == 1) { proc.rescanPacks(); rebuildLocationMenu(); }
        if (r == 2) { folder.createDirectory(); folder.startAsProcess(); }
    });
}

void SubRooRoomsEditor::updateModeVisibility()
{
    const bool directOn = proc.isDirectOn();
    angleSeg.setVisible (directOn);
    angleDial.setVisible (directOn);
    levelRange.setVisible (directOn);
    spread.setVisible (! directOn);
    mix.setVisible (! directOn);
    predelay.setVisible (! directOn);
    repaint();
}

void SubRooRoomsEditor::timerCallback()
{
    if (proc.getLibrary().size() != lastLocationCount) rebuildLocationMenu();
    if (locationBox.getSelectedId() != proc.getLocationIndex() + 1)
    {
        juce::ScopedValueSetter<bool> svs (ignoreCombo, true);
        locationBox.setSelectedId (proc.getLocationIndex() + 1, juce::dontSendNotification);
    }

    const bool directOn = proc.isDirectOn();
    directSeg.setSelected (directOn ? 0 : 1);
    if (angleDial.isVisible() != directOn) updateModeVisibility();

    auto& st = proc.getState();
    micSeg.setSelected ((int) std::lround (st.getRawParameterValue (params::micSystem)->load()));
    angleSeg.setSelected (proc.isCustomAngle() ? 1 : 0);

    auto info = proc.getRenderInfo();
    const float angle = st.getRawParameterValue (params::angle)->load();
    angleDial.setState (proc.isCustomAngle() ? angle : info.recordedAngleDeg, info.recordedAngleDeg, proc.isCustomAngle());

    roomView.refresh();
    vu.setLevel (proc.getOutputRms());
}

//==============================================================================
static const juce::Image& faceplateTexture()
{
    static juce::Image img = hw::makeBrushed (1800, 1100, hw::pal::faceTop, hw::pal::faceBottom, 0x5b7a11u, 1.0f);
    return img;
}

void SubRooRoomsEditor::paint (juce::Graphics& g)
{
    using namespace hw;
    g.fillAll (juce::Colours::black);

    // brushed faceplate
    const auto& tex = faceplateTexture();
    g.drawImage (tex, faceArea.toFloat(), juce::RectanglePlacement::fillDestination);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawHorizontalLine (faceArea.getY(), (float) faceArea.getX(), (float) faceArea.getRight());
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawHorizontalLine (faceArea.getBottom() - 1, (float) faceArea.getX(), (float) faceArea.getRight());

    drawRackEar (g, getLocalBounds().removeFromLeft (faceArea.getX()).toFloat(), true);
    drawRackEar (g, getLocalBounds().withLeft (faceArea.getRight()).toFloat(), false);

    // header: nameplate, LCD labels, power lamp
    {
        auto n = nameArea.toFloat();
        auto plate = n.reduced (0, 6);
        drawRecess (g, plate, 4.0f, juce::Colour (0xff141517));
        auto inner = plate.reduced (14, 0);
        auto top = inner.removeFromTop (inner.getHeight() * 0.62f);
        const auto big = font (top.getHeight() * 0.92f, Weight::bold, 0.10f);
        const float w1 = juce::GlyphArrangement::getStringWidth (big, "SUBROO");
        g.setFont (big);
        g.setColour (colours::accent);
        g.drawText ("SUBROO", top.removeFromLeft (w1 + 4).translated (0, 3), juce::Justification::bottomLeft, false);
        top.removeFromLeft (6);
        g.setColour (pal::silk);
        g.drawText ("ROOMS", top.translated (0, 3), juce::Justification::bottomLeft, false);
        drawEngraved (g, juce::String (juce::CharPointer_UTF8 ("LOCATION REVERB  \xc2\xb7  MEASURED SPACES")), inner, juce::Justification::centredLeft,
                      std::min (13.0f, inner.getHeight() * 0.8f), pal::silkDim, Weight::semibold, 0.16f);
    }
    drawEngraved (g, "LOCATION", lcdLocLabel.toFloat(), juce::Justification::bottomLeft, 12.0f, pal::silkDim, Weight::bold, 0.16f);
    drawEngraved (g, "PRESET", lcdPresetLabel.toFloat(), juce::Justification::bottomLeft, 12.0f, pal::silkDim, Weight::bold, 0.16f);
    {
        auto l = lampArea.toFloat();
        drawLamp (g, { l.getCentreX(), l.getY() + l.getHeight() * 0.38f }, 5.0f, pal::amber, true);
        drawEngraved (g, "POWER", l.withTrimmedTop (l.getHeight() * 0.62f), juce::Justification::centredTop, 11.0f, pal::silkDim, Weight::bold, 0.14f);
    }
    // header divider (engraved)
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawHorizontalLine (headerArea.getBottom(), (float) faceArea.getX() + 14.0f, (float) faceArea.getRight() - 14.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine (headerArea.getBottom() + 1, (float) faceArea.getX() + 14.0f, (float) faceArea.getRight() - 14.0f);

    // side column sections
    drawSectionFrame (g, secSignal.toFloat(), "SIGNAL");
    drawSectionFrame (g, secMic.toFloat(), "MIC SYSTEM");
    drawSectionFrame (g, secMode.toFloat(), proc.isDirectOn() ? "SOURCE ANGLE" : "REVERB");
    if (proc.isDirectOn())
    {
        auto lr = levelRange.getBounds().toFloat();
        drawEngraved (g, "LEVEL RANGE", lr.withY (lr.getY() - 16.0f).withHeight (14.0f), juce::Justification::centred, 12.0f, pal::silk, Weight::bold, 0.14f);
        drawEngraved (g, "FLAT", { lr.getX() + 2.0f, lr.getBottom() + 1.0f, 60.0f, 13.0f }, juce::Justification::centredLeft, 11.0f, pal::silkDim, Weight::bold, 0.1f);
        drawEngraved (g, "NATURAL", { lr.getRight() - 62.0f, lr.getBottom() + 1.0f, 60.0f, 13.0f }, juce::Justification::centredRight, 11.0f, pal::silkDim, Weight::bold, 0.1f);
    }

    // screen bezel + strip labels
    drawBezel (g, screenOuter.toFloat(), 9.0f, 7.0f);
    drawEngraved (g, "VIEW", viewLabel.toFloat(), juce::Justification::centredLeft, 12.0f, pal::silkDim, Weight::bold, 0.16f);
    drawEngraved (g, "DOORS", doorsLabel.toFloat(), juce::Justification::centredRight, 12.0f, pal::silkDim, Weight::bold, 0.16f);

    // footer sections
    drawSectionFrame (g, secRoom.toFloat(), "ROOM");
    drawSectionFrame (g, secTone.toFloat(), "TONE");
    drawSectionFrame (g, secOut.toFloat(), "OUTPUT");

    // faceplate screws
    for (auto p : { juce::Point<float> ((float) faceArea.getX() + 10.0f, (float) faceArea.getY() + 10.0f),
                    juce::Point<float> ((float) faceArea.getRight() - 10.0f, (float) faceArea.getY() + 10.0f),
                    juce::Point<float> ((float) faceArea.getX() + 10.0f, (float) faceArea.getBottom() - 10.0f),
                    juce::Point<float> ((float) faceArea.getRight() - 10.0f, (float) faceArea.getBottom() - 10.0f) })
        drawScrew (g, p, 4.0f, p.x * 0.013f + p.y * 0.021f);

    drawEngraved (g, "v" JucePlugin_VersionString, footerArea.toFloat().removeFromBottom (12.0f).withTrimmedRight (6.0f).translated (0, 9.0f),
                  juce::Justification::centredRight, 10.0f, pal::silkDim.withAlpha (0.7f), Weight::semibold, 0.1f);
}

void SubRooRoomsEditor::paintOverChildren (juce::Graphics& g)
{
    // faint glass reflection over the display
    auto s = roomView.getBounds().toFloat();
    juce::ColourGradient glare (juce::Colours::white.withAlpha (0.035f), s.getX(), s.getY(),
                                juce::Colours::transparentWhite, s.getX() + s.getWidth() * 0.35f, s.getY() + s.getHeight() * 0.6f, false);
    g.setGradientFill (glare);
    g.fillRect (s);
}

void SubRooRoomsEditor::resized()
{
    const int earW = 30;
    auto r = getLocalBounds();
    faceArea = r.withTrimmedLeft (earW).withTrimmedRight (earW);
    auto f = faceArea.reduced (22, 0);

    // ------------------------------------------------ header
    headerArea = f.removeFromTop (76);
    {
        auto h = headerArea.withTrimmedTop (8).withTrimmedBottom (6);
        nameArea = h.removeFromLeft (std::min (330, h.getWidth() / 3));
        h.removeFromLeft (22);
        lampArea = h.removeFromRight (54);
        h.removeFromRight (14);
        auto labels = h.removeFromTop (16);
        auto combos = h.withSizeKeepingCentre (h.getWidth(), 32);
        const int presetW = juce::jlimit (180, 300, combos.getWidth() / 3);
        auto presetCol = combos.removeFromRight (presetW);
        lcdPresetLabel = labels.removeFromRight (presetW);
        presetBox.setBounds (presetCol);
        combos.removeFromRight (12);
        labels.removeFromRight (12);
        packsButton.setBounds (combos.removeFromRight (72).reduced (0, 1));
        combos.removeFromRight (8);
        locationBox.setBounds (combos);
        lcdLocLabel = labels;
    }

    // ------------------------------------------------ footer knob row
    f.removeFromBottom (14);
    footerArea = f.removeFromBottom (juce::jlimit (150, 176, getHeight() / 5));
    f.removeFromBottom (18);
    {
        auto a = footerArea.withTrimmedTop (8);
        const int gap = 18;
        const int w = a.getWidth() - 2 * gap;
        const int roomW = (int) (w * 0.27f), toneW = (int) (w * 0.27f);
        secRoom = a.removeFromLeft (roomW);
        a.removeFromLeft (gap);
        secTone = a.removeFromLeft (toneW);
        a.removeFromLeft (gap);
        secOut = a;

        auto knobRow = [] (juce::Rectangle<int> sec, std::initializer_list<Knob*> knobs)
        {
            auto in = sec.reduced (10, 0).withTrimmedTop (16).withTrimmedBottom (6);
            const int kw = in.getWidth() / (int) knobs.size();
            for (auto* k : knobs)
                k->setBounds (in.removeFromLeft (kw).withSizeKeepingCentre (std::min (kw, in.getHeight() + 10), in.getHeight()));
        };
        knobRow (secRoom, { &size, &decay });
        knobRow (secTone, { &low, &high });
        auto o = secOut.reduced (12, 0).withTrimmedTop (16).withTrimmedBottom (6);
        auto vuArea = o.removeFromRight (std::min (220, o.getWidth() / 2 + 20));
        vu.setBounds (vuArea.withSizeKeepingCentre (vuArea.getWidth(), std::min (vuArea.getHeight() - 4, (int) (vuArea.getWidth() * 0.55f))));
        output.setBounds (o.withSizeKeepingCentre (std::min (o.getWidth(), o.getHeight() + 10), o.getHeight()));
    }

    // ------------------------------------------------ middle: side column + screen
    f.removeFromTop (16);
    sideArea = f.removeFromLeft (318);
    f.removeFromLeft (20);
    {
        auto s = sideArea;
        secSignal = s.removeFromTop (58);
        s.removeFromTop (18);
        secMic = s.removeFromTop (58);
        s.removeFromTop (18);
        secMode = s;

        directSeg.setBounds (secSignal.reduced (10, 0).withTrimmedTop (14).withTrimmedBottom (8));
        micSeg.setBounds (secMic.reduced (10, 0).withTrimmedTop (14).withTrimmedBottom (8));

        auto m = secMode.reduced (12, 0).withTrimmedTop (16).withTrimmedBottom (8);
        {
            auto a = m;
            angleSeg.setBounds (a.removeFromTop (34));
            a.removeFromTop (6);
            auto lr = a.removeFromBottom (54);
            levelRange.setBounds (lr.withTrimmedTop (22).withTrimmedBottom (12).reduced (6, 0));
            angleDial.setBounds (a);
        }
        {
            auto a = m;
            if (a.getHeight() >= 270)
            {
                const int rowH = a.getHeight() / 2;
                auto r1 = a.removeFromTop (rowH);
                spread.setBounds (r1.withSizeKeepingCentre (std::min (150, r1.getWidth()), r1.getHeight() - 8));
                const int kw = a.getWidth() / 2;
                mix.setBounds (a.removeFromLeft (kw).reduced (4, 6));
                predelay.setBounds (a.reduced (4, 6));
            }
            else
            {
                const int kw = a.getWidth() / 3;
                auto row = a.withSizeKeepingCentre (a.getWidth(), std::min (a.getHeight(), 130));
                spread.setBounds (row.removeFromLeft (kw));
                mix.setBounds (row.removeFromLeft (kw));
                predelay.setBounds (row);
            }
        }
    }

    stripArea = f.removeFromBottom (36);
    f.removeFromBottom (10);
    screenOuter = f;
    roomView.setBounds (screenOuter.reduced (9));
    {
        auto s = stripArea.withSizeKeepingCentre (stripArea.getWidth(), 30);
        viewLabel = s.removeFromLeft (44);
        isoButton.setBounds (s.removeFromLeft (56));
        s.removeFromLeft (6);
        planButton.setBounds (s.removeFromLeft (56));
        closeAllButton.setBounds (s.removeFromRight (104));
        s.removeFromRight (6);
        openAllButton.setBounds (s.removeFromRight (104));
        doorsLabel = s.removeFromRight (60).withTrimmedRight (10);
    }
}
} // namespace srr
