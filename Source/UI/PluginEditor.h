#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"
#include "RoomView.h"
#include "AngleDial.h"

namespace srr
{
class SubRooRoomsProcessor;

class SubRooRoomsEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SubRooRoomsEditor (SubRooRoomsProcessor&);
    ~SubRooRoomsEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void rebuildLocationMenu();
    void rebuildPresetMenu();
    void updateModeVisibility();
    void showPacksMenu();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    SubRooRoomsProcessor& proc;
    ui::SubRooLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 500 };

    ui::RoomView roomView;
    juce::ComboBox locationBox, presetBox;
    juce::TextButton packsButton { "PACKS" };
    juce::TextButton isoButton { "ISO" }, planButton { "2D" };
    juce::TextButton openAllButton { "OPEN ALL" }, closeAllButton { "CLOSE ALL" };

    ui::Segmented directSeg { { "DIRECT ON", "DIRECT OFF" } };
    ui::Segmented micSeg { { "SPACED AB", "COINC. MS", "HYBRID" } };
    ui::Segmented angleSeg { { "RECORDED", "CUSTOM" } };
    ui::AngleDial angleDial;
    juce::Slider levelRange;

    ui::Knob spread { "Spread" }, mix { "Mix" }, predelay { "Pre-delay" };
    ui::Knob size { "Size" }, decay { "Decay" }, output { "Output" };
    ui::Knob low { "Low" }, high { "High" };
    ui::hw::VUMeter vu;

    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::unique_ptr<SliderAttachment> levelAttachment;

    // faceplate geometry (set in resized, used by paint)
    juce::Rectangle<int> faceArea, headerArea, nameArea, lampArea, sideArea, screenOuter, stripArea, footerArea;
    juce::Rectangle<int> secSignal, secMic, secMode, secRoom, secTone, secOut;
    juce::Rectangle<int> lcdLocLabel, lcdPresetLabel, viewLabel, doorsLabel;
    int lastLocationCount = -1;
    bool ignoreCombo = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubRooRoomsEditor)
};
} // namespace srr
