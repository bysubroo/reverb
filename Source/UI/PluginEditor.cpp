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
    levelRange.setColour (juce::Slider::trackColourId, colours::accent);
    levelRange.setTooltip ("Flat: every position has the same loudness. Natural: real level differences (closing a door makes it quieter).");
    levelAttachment = std::make_unique<SliderAttachment> (st, params::levelRange, levelRange);

    struct K { Knob* k; const char* id; juce::Colour c; };
    for (auto& k : { K { &spread, params::spread, colours::accent }, K { &mix, params::mix, colours::accent },
                     K { &predelay, params::predelay, colours::accent }, K { &size, params::size, colours::mic },
                     K { &decay, params::decay, colours::mic }, K { &output, params::output, colours::text },
                     K { &low, params::lowEq, colours::warn }, K { &high, params::highEq, colours::warn } })
    {
        addAndMakeVisible (*k.k);
        k.k->slider.setColour (juce::Slider::rotarySliderFillColourId, k.c);
        attachments.push_back (std::make_unique<SliderAttachment> (st, k.id, k.k->slider));
    }
    spread.slider.setTooltip ("How tightly the reverb follows each panned input (narrow) or envelops it (wide)");
    mix.slider.setTooltip ("Wet/dry balance (Direct Off)");
    predelay.slider.setTooltip ("Delays all reverb channels (Direct Off)");
    size.slider.setTooltip ("Scales the whole location");
    decay.slider.setTooltip ("Reverb time multiplier");

    setResizable (true, true);
    setResizeLimits (920, 600, 2400, 1500);
    setSize (1180, 720);

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
    auto folder = LocationLibrary::getPacksFolder();
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
}

void SubRooRoomsEditor::paint (juce::Graphics& g)
{
    using namespace colours;
    g.fillAll (bg);
    g.setColour (panel);
    g.fillRect (panelArea);
    g.setColour (line);
    g.drawVerticalLine (panelArea.getRight(), 0.0f, (float) getHeight());

    // brand
    auto head = panelArea.reduced (18, 0).removeFromTop (58);
    g.setFont (uiFont (26.0f, true));
    g.setColour (accent);
    g.drawText ("SUBROO", head.removeFromLeft (118), juce::Justification::centredLeft);
    g.setColour (text);
    g.drawText ("ROOMS", head.removeFromLeft (90), juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (uiFont (10.0f));
    g.drawText ("v" JucePlugin_VersionString, head, juce::Justification::centredRight);

    auto sectionLabel = [&] (juce::Rectangle<int> r, const juce::String& s)
    {
        g.setColour (textDim);
        g.setFont (uiFont (10.5f, true));
        g.drawText (s, r, juce::Justification::bottomLeft);
    };
    sectionLabel ({ directSeg.getX(), directSeg.getY() - 18, 200, 16 }, "SIGNAL");
    sectionLabel ({ micSeg.getX(), micSeg.getY() - 18, 200, 16 }, "MIC SYSTEM");
    if (proc.isDirectOn())
    {
        sectionLabel ({ angleSeg.getX(), angleSeg.getY() - 18, 200, 16 }, "ANGLE");
        auto lr = levelRange.getBounds();
        sectionLabel ({ lr.getX(), lr.getY() - 14, 200, 14 }, "LEVEL RANGE");
        g.setFont (uiFont (10.0f, true));
        g.setColour (textDim);
        g.drawText ("FLAT", lr.getX(), lr.getBottom() - 2, 60, 12, juce::Justification::centredLeft);
        g.drawText ("NATURAL", lr.getRight() - 70, lr.getBottom() - 2, 70, 12, juce::Justification::centredRight);
    }
    else
    {
        sectionLabel ({ spread.getX(), angleArea.getY() - 18, 200, 16 }, "REVERB");
    }
    sectionLabel ({ size.getX(), size.getY() - 18, 200, 16 }, "ROOM & OUTPUT");
    sectionLabel ({ low.getX(), low.getY() - 18, 200, 16 }, "TONE");
}

void SubRooRoomsEditor::resized()
{
    auto r = getLocalBounds();
    panelArea = r.removeFromLeft (304);
    roomView.setBounds (r);

    auto vb = r.reduced (16, 14);
    auto top = vb.removeFromTop (26);
    planButton.setBounds (top.removeFromRight (44));
    top.removeFromRight (6);
    isoButton.setBounds (top.removeFromRight (44));
    auto bottom = vb.removeFromBottom (26).withTrimmedBottom (0);
    juce::ignoreUnused (bottom);
    auto doorRow = r.reduced (16, 14).withTrimmedTop (34).removeFromTop (26).removeFromRight (190);
    closeAllButton.setBounds (doorRow.removeFromRight (92));
    doorRow.removeFromRight (6);
    openAllButton.setBounds (doorRow.removeFromRight (92));

    auto p = panelArea.reduced (18, 0);
    p.removeFromTop (58);
    auto row = p.removeFromTop (30);
    packsButton.setBounds (row.removeFromRight (62));
    row.removeFromRight (6);
    locationBox.setBounds (row);
    p.removeFromTop (8);
    presetBox.setBounds (p.removeFromTop (28));

    p.removeFromTop (26);
    directSeg.setBounds (p.removeFromTop (30));
    p.removeFromTop (26);
    micSeg.setBounds (p.removeFromTop (30));
    p.removeFromTop (26);

    // mode-dependent block
    auto modeBlock = p.removeFromTop (juce::jlimit (160, 260, getHeight() - 500));
    angleArea = modeBlock;
    {
        auto a = modeBlock;
        angleSeg.setBounds (a.removeFromTop (28));
        a.removeFromTop (4);
        auto lr = a.removeFromBottom (34).withTrimmedBottom (10);
        levelRange.setBounds (lr.withTrimmedTop (6));
        a.removeFromBottom (10);
        angleDial.setBounds (a);
    }
    {
        auto a = modeBlock.withSizeKeepingCentre (modeBlock.getWidth(), std::min (modeBlock.getHeight(), 110));
        const int w = a.getWidth() / 3;
        spread.setBounds (a.removeFromLeft (w));
        mix.setBounds (a.removeFromLeft (w));
        predelay.setBounds (a);
    }

    p.removeFromTop (22);
    const int kh = juce::jlimit (70, 96, (p.getHeight() - 60) / 2);
    auto k1 = p.removeFromTop (kh);
    const int kw = k1.getWidth() / 3;
    size.setBounds (k1.removeFromLeft (kw));
    decay.setBounds (k1.removeFromLeft (kw));
    output.setBounds (k1);
    p.removeFromTop (22);
    auto k2 = p.removeFromTop (kh);
    low.setBounds (k2.removeFromLeft (kw));
    high.setBounds (k2.removeFromLeft (kw));
}
} // namespace srr
