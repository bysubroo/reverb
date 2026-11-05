// Loads the built VST3 through JUCE's plug-in hosting (like a DAW) and
// exercises layouts, processing, latency, programs and state.
// usage: SubRooRoomsHostCheck "<path to SubRoo Rooms.vst3>"

#include <juce_audio_processors/juce_audio_processors.h>

static void pump (int ms)
{
    auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (juce::Time::getMillisecondCounter() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf ("usage: SubRooRoomsHostCheck <plugin.vst3>\n"); return 2; }
    int failures = 0;

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> types;
    format.findAllTypesForFile (types, juce::String (argv[1]));
    if (types.isEmpty()) { std::printf ("FAIL: no plug-in found\n"); return 1; }
    auto desc = *types[0];
    std::printf ("found: %s by %s, category '%s', ins %d outs %d\n", desc.name.toRawUTF8(), desc.manufacturerName.toRawUTF8(),
                 desc.category.toRawUTF8(), desc.numInputChannels, desc.numOutputChannels);

    juce::String err;
    auto inst = format.createInstanceFromDescription (desc, 48000.0, 512, err);
    if (inst == nullptr) { std::printf ("FAIL: %s\n", err.toRawUTF8()); return 1; }

    std::printf ("parameters: %d, programs: %d\n", inst->getParameters().size(), inst->getNumPrograms());
    if (inst->getNumPrograms() < 10) ++failures;

    using ACS = juce::AudioChannelSet;
    struct L { ACS in, out; const char* name; };
    for (auto& l : { L { ACS::stereo(), ACS::stereo(), "stereo" }, L { ACS::mono(), ACS::stereo(), "mono>stereo" },
                     L { ACS::create5point1(), ACS::create5point1(), "5.1" }, L { ACS::create7point0point2(), ACS::create7point0point2(), "7.0.2" },
                     L { ACS::create7point1point2(), ACS::create7point1point2(), "7.1.2" } })
    {
        juce::AudioProcessor::BusesLayout bl;
        bl.inputBuses.add (l.in); bl.outputBuses.add (l.out);
        const bool ok = inst->setBusesLayout (bl);
        inst->prepareToPlay (48000.0, 512);
        const int nch = std::max (l.in.size(), l.out.size());
        juce::AudioBuffer<float> buf (nch, 512);
        juce::MidiBuffer midi;
        for (int i = 0; i < 150; ++i) { buf.clear(); inst->processBlock (buf, midi); pump (4); }
        float peak = 0; bool finite = true;
        for (int b = 0; b < 94; ++b)
        {
            buf.clear();
            if (b == 0) for (int c = 0; c < l.in.size(); ++c) buf.setSample (c, 0, 1.0f);
            inst->processBlock (buf, midi);
            for (int c = 0; c < l.out.size(); ++c)
            {
                peak = std::max (peak, buf.getMagnitude (c, 0, 512));
                for (int n = 0; n < 512; ++n) finite = finite && std::isfinite (buf.getSample (c, n));
            }
        }
        inst->releaseResources();
        std::printf ("layout %-12s accepted=%d latency=%d peak=%.4f finite=%d\n", l.name, (int) ok, inst->getLatencySamples(), peak, (int) finite);
        if (! ok || peak <= 0.0f || ! finite) ++failures;
    }

    // programs + state
    inst->setCurrentProgram (20);
    pump (50);
    juce::MemoryBlock mb;
    inst->getStateInformation (mb);
    auto inst2 = format.createInstanceFromDescription (desc, 48000.0, 512, err);
    inst2->setStateInformation (mb.getData(), (int) mb.getSize());
    pump (50);
    int diffs = 0;
    for (int i = 0; i < inst->getParameters().size(); ++i)
        if (std::abs (inst->getParameters()[i]->getValue() - inst2->getParameters()[i]->getValue()) > 1e-4f) ++diffs;
    std::printf ("program 20 '%s', state %d bytes, %d differing params after reload\n",
                 inst->getProgramName (20).toRawUTF8(), (int) mb.getSize(), diffs);
    if (diffs) ++failures;

    // editor
    if (auto* ed = inst->createEditorIfNeeded())
    {
        std::printf ("editor: %d x %d, resizable=%d\n", ed->getWidth(), ed->getHeight(), (int) ed->isResizable());
        pump (200);
        delete ed;
    }
    else { std::printf ("FAIL: no editor\n"); ++failures; }

    inst2.reset();
    inst.reset();
    std::printf (failures == 0 ? "HOST CHECK PASSED\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
