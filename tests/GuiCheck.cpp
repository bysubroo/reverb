// Instantiates SubRoo Rooms directly, runs it through a few layouts and
// saves editor snapshots (PNG) for visual checks.
// usage: SubRooRoomsGuiCheck <output-folder>

#include <juce_audio_processors/juce_audio_processors.h>
#include "Plugin/PluginProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static void pump (int ms)
{
    auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (juce::Time::getMillisecondCounter() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
}

static float runImpulse (juce::AudioProcessor& p, int numIn, int numOut, int seconds, juce::AudioBuffer<float>* capture = nullptr)
{
    const int bs = 512;
    juce::AudioBuffer<float> buf (std::max (numIn, numOut), bs);
    juce::MidiBuffer midi;
    // wait for the renderer
    for (int i = 0; i < 200; ++i) { buf.clear(); p.processBlock (buf, midi); pump (5); }
    float peak = 0;
    if (capture) capture->setSize (numOut, 48000 * seconds);
    for (int b = 0; b < 48000 * seconds / bs; ++b)
    {
        buf.clear();
        if (b == 0) for (int c = 0; c < numIn; ++c) buf.setSample (c, 10, 1.0f);
        p.processBlock (buf, midi);
        for (int c = 0; c < numOut; ++c)
        {
            peak = std::max (peak, buf.getMagnitude (c, 0, bs));
            if (capture) capture->copyFrom (c, b * bs, buf, c, 0, bs);
        }
    }
    return peak;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    auto out = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "gui-check");
    out.createDirectory();
    int failures = 0;

    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    auto& srp = dynamic_cast<srr::SubRooRoomsProcessor&> (*proc);

    using ACS = juce::AudioChannelSet;
    struct L { ACS in, out; const char* name; };
    for (auto& l : { L { ACS::stereo(), ACS::stereo(), "stereo" }, L { ACS::mono(), ACS::stereo(), "mono>stereo" },
                     L { ACS::stereo(), ACS::create5point1(), "stereo>5.1" }, L { ACS::create7point1point2(), ACS::create7point1point2(), "7.1.2" } })
    {
        juce::AudioProcessor::BusesLayout bl;
        bl.inputBuses.add (l.in); bl.outputBuses.add (l.out);
        const bool ok = proc->setBusesLayout (bl);
        proc->prepareToPlay (48000.0, 512);
        auto t0 = juce::Time::getMillisecondCounterHiRes();
        float peak = runImpulse (*proc, l.in.size(), l.out.size(), 2);
        auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        std::printf ("layout %-12s supported=%d peak=%.4f  (%.0f ms incl. waiting)\n", l.name, (int) ok, peak, ms);
        if (! ok || ! std::isfinite (peak) || peak <= 0.0f) ++failures;
    }

    // CPU: stereo, process 20 s of noise
    {
        juce::AudioProcessor::BusesLayout bl;
        bl.inputBuses.add (ACS::stereo()); bl.outputBuses.add (ACS::stereo());
        proc->setBusesLayout (bl);
        proc->prepareToPlay (48000.0, 256);
        runImpulse (*proc, 2, 2, 1);
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        juce::Random r;
        double total = 0;
        const int blocks = 48000 * 20 / 256;
        for (int b = 0; b < blocks; ++b)
        {
            for (int c = 0; c < 2; ++c) for (int n = 0; n < 256; ++n) buf.setSample (c, n, r.nextFloat() * 0.2f - 0.1f);
            auto t0 = juce::Time::getMillisecondCounterHiRes();
            proc->processBlock (buf, midi);
            total += juce::Time::getMillisecondCounterHiRes() - t0;
        }
        std::printf ("CPU stereo Direct On: %.2f %% of one core (avg %.3f ms per 256-sample block)\n",
                     100.0 * total / (blocks * 256.0 / 48.0), total / blocks);

        srp.setParam (srr::params::direct, 0.0f);
        runImpulse (*proc, 2, 2, 1);
        total = 0;
        for (int b = 0; b < blocks; ++b)
        {
            for (int c = 0; c < 2; ++c) for (int n = 0; n < 256; ++n) buf.setSample (c, n, r.nextFloat() * 0.2f - 0.1f);
            auto t0 = juce::Time::getMillisecondCounterHiRes();
            proc->processBlock (buf, midi);
            total += juce::Time::getMillisecondCounterHiRes() - t0;
        }
        std::printf ("CPU stereo Direct Off: %.2f %% of one core\n", 100.0 * total / (blocks * 256.0 / 48.0));
        srp.setParam (srr::params::direct, 1.0f);
    }

    // angle automation sweep: must stay finite and free of clicks
    {
        juce::AudioProcessor::BusesLayout bl;
        bl.inputBuses.add (ACS::mono()); bl.outputBuses.add (ACS::stereo());
        proc->setBusesLayout (bl);
        proc->prepareToPlay (48000.0, 512);
        srp.setParam (srr::params::angleMode, 1.0f);
        runImpulse (*proc, 1, 2, 1);
        juce::AudioBuffer<float> buf (2, 512), rec (2, 48000 * 6);
        juce::MidiBuffer midi;
        for (int b = 0; b < 48000 * 6 / 512; ++b)
        {
            for (int n = 0; n < 512; ++n) buf.setSample (0, n, 0.3f * std::sin (2.0f * 3.14159f * 330.0f * (float) (b * 512 + n) / 48000.0f));
            buf.clear (1, 0, 512);
            srp.setParam (srr::params::angle, -180.0f + 360.0f * (float) b / (48000.0f * 6 / 512));
            proc->processBlock (buf, midi);
            pump (1);
            for (int c = 0; c < 2; ++c) rec.copyFrom (c, b * 512, buf, c, 0, 512);
        }
        float worst = 0, typical = 0;
        for (int n = 48000 + 2; n < (48000 * 6 / 512) * 512; ++n)
        {
            auto d2 = std::abs (rec.getSample (0, n) - 2 * rec.getSample (0, n - 1) + rec.getSample (0, n - 2));
            worst = std::max (worst, d2); typical += d2;
        }
        typical /= (float) ((48000 * 6 / 512) * 512 - 48002);
        std::printf ("angle sweep: max curvature %.5f vs mean %.5f (ratio %.1f)\n", worst, typical, worst / std::max (1e-9f, typical));
        juce::WavAudioFormat wav;
        auto f = out.getChildFile ("angle_sweep.wav"); f.deleteFile();
        std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
        if (auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (48000).withNumChannels (2).withBitsPerSample (24)))
            w->writeFromAudioSampleBuffer (rec, 0, rec.getNumSamples());
        srp.setParam (srr::params::angleMode, 0.0f);
    }

    // every location: render and process
    {
        juce::AudioProcessor::BusesLayout bl;
        bl.inputBuses.add (ACS::stereo()); bl.outputBuses.add (ACS::stereo());
        proc->setBusesLayout (bl);
        proc->prepareToPlay (48000.0, 512);
        for (int li = 0; li < srp.getLibrary().size(); ++li)
        {
            srp.selectLocation (li);
            float peak = runImpulse (*proc, 2, 2, 1);
            std::printf ("  location %-22s peak %.4f\n", srp.getLibrary().get (li)->name.toRawUTF8(), peak);
            if (! std::isfinite (peak) || peak <= 0.0f) ++failures;
        }
    }

    // state round trip
    {
        srp.setCurrentProgram (12);
        pump (50);
        juce::MemoryBlock mb;
        proc->getStateInformation (mb);
        std::unique_ptr<juce::AudioProcessor> p2 (createPluginFilter());
        p2->setStateInformation (mb.getData(), (int) mb.getSize());
        int diffs = 0;
        auto params1 = proc->getParameters();
        auto params2 = p2->getParameters();
        for (int i = 0; i < params1.size(); ++i)
            if (std::abs (params1[i]->getValue() - params2[i]->getValue()) > 1e-4f) ++diffs;
        std::printf ("state round trip: %d differing parameters (preset '%s')\n", diffs, proc->getProgramName (12).toRawUTF8());
        if (diffs != 0) ++failures;
    }

    // editor snapshots
    {
        juce::AudioProcessor::BusesLayout bl;
        bl.inputBuses.add (ACS::stereo()); bl.outputBuses.add (ACS::stereo());
        proc->setBusesLayout (bl);
        proc->prepareToPlay (48000.0, 512);
        struct Shot { int preset; bool iso; bool directOff; const char* name; };
        for (auto& s : { Shot { 0, true, false, "editor_stan_iso" }, Shot { 0, false, false, "editor_stan_2d" },
                         Shot { 19, true, false, "editor_auto_iso" }, Shot { 25, true, false, "editor_studio_iso" },
                         Shot { 32, true, true, "editor_directoff_iso" }, Shot { 12, true, false, "editor_kafana_iso" },
                         Shot { 15, true, false, "editor_hotel_iso" } })
        {
            srp.setCurrentProgram (s.preset);
            if (s.directOff) srp.setParam (srr::params::direct, 0.0f);
            runImpulse (*proc, 2, 2, 1);
            std::unique_ptr<juce::AudioProcessorEditor> ed (srp.createEditor());
            ed->setSize (1180, 720);
            pump (400);
            if (! s.iso)
                for (auto* c : ed->getChildren())
                    if (auto* b = dynamic_cast<juce::TextButton*> (c))
                        if (b->getButtonText() == "2D") b->triggerClick();
            pump (300);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f = out.getChildFile (juce::String (s.name) + ".png");
            f.deleteFile();
            juce::FileOutputStream fos (f);
            juce::PNGImageFormat png;
            png.writeImageToStream (img, fos);
            std::printf ("snapshot %s\n", f.getFullPathName().toRawUTF8());
            ed.reset();
            pump (50);
        }
    }

    std::printf (failures == 0 ? "GUI/PROCESS CHECK PASSED\n" : "%d FAILURE(S)\n", failures);
    proc.reset();
    return failures == 0 ? 0 : 1;
}
