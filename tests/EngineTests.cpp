// Offline checks for the SubRoo Rooms engine:
//  1. partitioned convolver == direct convolution
//  2. IR swaps are click-free
//  3. every built-in location/spot combination renders sane scenes
//  4. renders example IRs to WAV for listening / analysis
//
// usage: SubRooRoomsTests <output-folder>

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Acoustics/LocationLibrary.h"
#include "Acoustics/Scene.h"
#include "Acoustics/Synth.h"
#include "Acoustics/RenderEngine.h"
#include "DSP/MatrixConvolver.h"

using namespace srr;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("  FAIL: %s\n", msg); } } while (0)

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double fs)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    auto opts = juce::AudioFormatWriterOptions().withSampleRate (fs).withNumChannels (b.getNumChannels()).withBitsPerSample (32)
                    .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    if (auto w = wav.createWriterFor (os, opts))
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

//==============================================================================
static void testConvolver()
{
    std::printf ("[1] convolver vs direct convolution\n");
    const int O = 3, F = 2, L = 21000, N = 30000;
    juce::Random rnd (42);

    juce::AudioBuffer<float> ir (O, L);
    for (int c = 0; c < O; ++c)
        for (int n = 0; n < L; ++n)
            ir.setSample (c, n, (rnd.nextFloat() * 2 - 1) * std::exp (-(float) n / 4000.0f));

    std::vector<float> x ((size_t) N);
    for (auto& v : x) v = rnd.nextFloat() * 2 - 1;

    MatrixConvolver conv;
    conv.prepare (F, O, 16);
    auto set = IRSet::build ({ { 0, &ir } }, &ir, O, 16);
    conv.submit (std::move (set));

    // let the set become active (first period boundary), feeding silence
    std::vector<std::vector<float>> out ((size_t) O, std::vector<float> ((size_t) N + 20000, 0.0f));
    {
        std::vector<float> z (4096, 0.0f), dump (4096);
        const float* in[2] = { z.data(), z.data() };
        float* o3[3] = { dump.data(), dump.data(), dump.data() };
        conv.process (in, o3, 4096, 1.0f);
        conv.process (in, o3, 4096, 1.0f);
    }
    // the fade-in from "nothing" takes one period: run silence through it
    {
        std::vector<float> z (8192, 0.0f), dump (8192);
        const float* in[2] = { z.data(), z.data() };
        float* o3[3] = { dump.data(), dump.data(), dump.data() };
        conv.process (in, o3, 8192, 1.0f);
    }

    std::vector<float> zero ((size_t) N + 20000, 0.0f);
    std::vector<float> xin ((size_t) N + 20000, 0.0f);
    std::copy (x.begin(), x.end(), xin.begin());
    int pos = 0;
    while (pos < (int) xin.size())
    {
        int bs = std::min ((int) xin.size() - pos, 1 + rnd.nextInt (700));
        const float* in[2] = { xin.data() + pos, zero.data() + pos };
        float* o[3] = { out[0].data() + pos, out[1].data() + pos, out[2].data() + pos };
        conv.process (in, o, bs, 1.0f);
        pos += bs;
    }

    const int lat = conv.getLatencySamples();
    double maxErr = 0, maxRef = 0;
    for (int c = 0; c < O; ++c)
        for (int n = 0; n < 26000; n += 7)
        {
            double ref = 0;
            const float* h = ir.getReadPointer (c);
            for (int k = 0; k < L && k <= n; ++k)
                if (n - k < N) ref += (double) h[k] * x[(size_t) (n - k)];
            auto got = (double) out[(size_t) c][(size_t) (n + lat)];
            maxErr = std::max (maxErr, std::abs (got - ref));
            maxRef = std::max (maxRef, std::abs (ref));
        }
    std::printf ("  max |ref| %.3f  max error %.2e\n", maxRef, maxErr);
    CHECK (maxErr < 1.0e-3 * std::max (1.0, maxRef), "convolver output differs from direct convolution");
}

//==============================================================================
static void testSwap()
{
    std::printf ("[2] IR swap is click free\n");
    const int O = 2, L = 48000;
    juce::AudioBuffer<float> a (O, L), b (O, L);
    juce::Random rnd (7);
    for (int c = 0; c < O; ++c)
        for (int n = 0; n < L; ++n)
        {
            auto env = std::exp (-(float) n / 6000.0f);
            a.setSample (c, n, (rnd.nextFloat() * 2 - 1) * env * 0.05f);
            b.setSample (c, n, (rnd.nextFloat() * 2 - 1) * env * 0.05f);
        }
    a.setSample (0, 100, 1.0f); b.setSample (0, 300, 0.5f);

    MatrixConvolver conv;
    conv.prepare (1, O, 16);
    conv.submit (IRSet::build ({ { 0, &a } }, &a, O, 16));

    const int total = 48000 * 4, bs = 256;
    std::vector<float> sine ((size_t) total), outL ((size_t) total), outR ((size_t) total);
    for (int n = 0; n < total; ++n) sine[(size_t) n] = 0.5f * std::sin (2.0f * kPi * 220.0f * (float) n / 48000.0f);

    float maxJump = 0;
    for (int p = 0; p < total; p += bs)
    {
        if (p == 48000 * 2) conv.submit (IRSet::build ({ { 0, &b } }, &b, O, 16));
        const float* in[1] = { sine.data() + p };
        float* o[2] = { outL.data() + p, outR.data() + p };
        conv.process (in, o, bs, 1.0f);
    }
    // second difference after the swap compared with before
    auto maxD2 = [&] (int from, int to)
    {
        float m = 0;
        for (int n = from + 2; n < to; ++n)
            m = std::max (m, std::abs (outL[(size_t) n] - 2 * outL[(size_t) n - 1] + outL[(size_t) n - 2]));
        return m;
    };
    auto before = maxD2 (48000, 48000 * 2);
    auto during = maxD2 (48000 * 2, 48000 * 2 + 20000);
    maxJump = during / std::max (1.0e-9f, before);
    std::printf ("  curvature before %.4f  during swap %.4f  (ratio %.2f)\n", before, during, maxJump);
    CHECK (maxJump < 2.5f, "swap produced a discontinuity");
}

//==============================================================================
static void testAllScenes (LocationLibrary& lib)
{
    std::printf ("[3] all built-in locations\n");
    for (int li = 0; li < lib.getNumBuiltIn(); ++li)
    {
        auto loc = lib.get (li);
        double worstMs = 0, sumMs = 0;
        int n = 0, bad = 0;
        float minDb = 1e9f, maxDb = -1e9f, maxLen = 0;
        for (int s = 0; s < (int) loc->sources.size(); ++s)
            for (int m = 0; m < (int) loc->mics.size(); ++m)
            {
                SceneParams sp; sp.source = s; sp.mic = m; sp.doors = loc->defaultDoors;
                auto t0 = juce::Time::getMillisecondCounterHiRes();
                auto sc = buildScene (*loc, sp);
                auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
                worstMs = std::max (worstMs, ms); sumMs += ms; ++n;
                auto db = 10.0f * std::log10 (std::max (1e-12f, sc.energy));
                if (! sc.valid || ! std::isfinite (sc.energy) || sc.energy <= 0 || sc.arrivals.empty()) ++bad;
                minDb = std::min (minDb, db); maxDb = std::max (maxDb, db);
                maxLen = std::max (maxLen, sc.lengthSec);
            }
        std::printf ("  %-22s %3d combos  scene %.1f ms avg / %.1f max  level %.1f .. %.1f dB  len<=%.2fs  bad=%d  RT(mic0,1k)=%.2fs\n",
                     loc->id.toRawUTF8(), n, sumMs / n, worstMs, minDb, maxDb, maxLen, bad,
                     buildScene (*loc, { loc->defaultSource, loc->defaultMic, loc->defaultDoors, 1, 1 }).micRoomRt[3]);
        CHECK (bad == 0, "invalid scene");
    }
    auto errs = lib.getErrors();
    for (auto& e : errs) std::printf ("  library error: %s\n", e.toRawUTF8());
}

//==============================================================================
struct Example { const char* name; const char* loc; const char* src; const char* mic; juce::StringArray open; juce::AudioChannelSet layout; MicSystem sys; };

static void renderExamples (LocationLibrary& lib, const juce::File& dir)
{
    std::printf ("[4] example impulse responses -> %s\n", dir.getFullPathName().toRawUTF8());
    dir.createDirectory();
    using ACS = juce::AudioChannelSet;
    std::vector<Example> ex = {
        { "stan_dnevna_dijalog", "stan", "s_kauc", "m_dnevna", { "d_dnevna", "d_kuhinja" }, ACS::stereo(), MicSystem::Spaced },
        { "stan_dnevna_dijalog_MS", "stan", "s_kauc", "m_dnevna", { "d_dnevna", "d_kuhinja" }, ACS::stereo(), MicSystem::Coincident },
        { "stan_kuhinja_u_dnevnu", "stan", "s_kuhinja", "m_dnevna", { "d_dnevna", "d_kuhinja" }, ACS::stereo(), MicSystem::Spaced },
        { "stan_kuhinja_u_dnevnu_zatvoreno", "stan", "s_kuhinja", "m_dnevna", {}, ACS::stereo(), MicSystem::Spaced },
        { "stan_spavaca_iza_vrata", "stan", "s_spavaca", "m_hodnik", {}, ACS::stereo(), MicSystem::Spaced },
        { "stan_spavaca_otvorena_vrata", "stan", "s_spavaca", "m_hodnik", { "d_spavaca" }, ACS::stereo(), MicSystem::Spaced },
        { "stan_kupatilo", "stan", "s_kupatilo", "m_kupatilo", {}, ACS::stereo(), MicSystem::Spaced },
        { "stan_stepeniste", "stan", "s_stepeniste_gore", "m_stepeniste", {}, ACS::stereo(), MicSystem::Spaced },
        { "stan_komsija_na_stepenistu", "stan", "s_stepeniste", "m_predsoblje", {}, ACS::stereo(), MicSystem::Spaced },
        { "stan_garderober", "stan", "s_garderober", "m_garderober", {}, ACS::stereo(), MicSystem::Spaced },
        { "kafana_sto", "kafana", "s_sto1", "m_sto1", { "d_kuhinja_sank" }, ACS::stereo(), MicSystem::Spaced },
        { "kafana_bend_iz_toaleta", "kafana", "s_bina", "m_wc", {}, ACS::stereo(), MicSystem::Spaced },
        { "hotel_tv_komsija", "hotel", "s_susedi_tv", "m_krevet", {}, ACS::stereo(), MicSystem::Spaced },
        { "hotel_hodnik", "hotel", "s_hodnik_far", "m_hodnik", {}, ACS::stereo(), MicSystem::Spaced },
        { "auto_suvozac", "auto", "s_suvozac", "m_vozac", {}, ACS::stereo(), MicSystem::Spaced },
        { "auto_radio", "auto", "s_radio", "m_vozac", {}, ACS::stereo(), MicSystem::Spaced },
        { "auto_trotoar_zatvoreno", "auto", "s_trotoar", "m_vozac", {}, ACS::stereo(), MicSystem::Spaced },
        { "auto_trotoar_otvoreno", "auto", "s_trotoar", "m_vozac", { "d_vozac" }, ACS::stereo(), MicSystem::Spaced },
        { "auto_gepek", "auto", "s_gepek", "m_zadnje", {}, ACS::stereo(), MicSystem::Spaced },
        { "studio_kabina", "studio", "s_vokal", "m_kabina", {}, ACS::stereo(), MicSystem::Coincident },
        { "studio_bubanj_room", "studio", "s_bubanj", "m_room", {}, ACS::stereo(), MicSystem::Spaced },
        { "radionica_hala", "radionica", "s_lift", "m_hala", { "d_rolo" }, ACS::stereo(), MicSystem::Spaced },
        { "kancelarija_telefon", "kancelarija", "s_telefon", "m_sredina", { "d_ulaz" }, ACS::stereo(), MicSystem::Spaced },
        { "stan_dnevna_7_1_2", "stan", "s_kauc", "m_dnevna", { "d_dnevna", "d_kuhinja" }, ACS::create7point1point2(), MicSystem::Hybrid },
        { "stan_dnevna_5_1", "stan", "s_kauc", "m_dnevna", { "d_dnevna", "d_kuhinja" }, ACS::create5point1(), MicSystem::Spaced },
    };

    MatrixConvolver dummy;
    juce::CriticalSection lock;
    RenderEngine engine (dummy, lock);

    juce::String csv ("name,channels,length_s,energy_db,render_ms,rt1k\n");
    for (auto& e : ex)
    {
        int li = lib.indexOf (e.loc);
        auto loc = lib.get (li);
        RenderRequest rq;
        rq.location = loc;
        rq.source = loc->findSource (e.src);
        rq.mic = loc->findMic (e.mic);
        uint32_t mask = 0;
        for (auto& p : loc->portals) if (p.doorBit >= 0 && e.open.contains (p.id)) mask |= 1u << (uint32_t) p.doorBit;
        rq.doors = mask;
        rq.outLayout = e.layout;
        rq.inLayout = ACS::mono();
        rq.micSystem = e.sys;
        rq.sampleRate = 48000.0;
        rq.valid = true;

        SceneParams sp; sp.source = rq.source; sp.mic = rq.mic; sp.doors = mask;
        auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto sc = buildScene (*loc, sp);
        auto rig = makeRig (e.layout, e.sys);
        const int L = irLengthFor (sc, 48000.0, 4096);
        juce::AudioBuffer<float> ir ((int) rig.size(), L);
        ir.clear();
        EarlyOptions eo; eo.sampleRate = 48000.0; eo.maxSamples = L;
        renderEarly (sc, rig, { sc.micYawDeg }, eo, ir);
        renderLate (sc, rig, sc.micYawDeg, 48000.0, 1234, ir);
        auto ms = juce::Time::getMillisecondCounterHiRes() - t0;

        double en = 0;
        for (int c = 0; c < ir.getNumChannels(); ++c)
            for (int n = 0; n < L; ++n) en += (double) ir.getSample (c, n) * ir.getSample (c, n);
        bool finite = true;
        for (int c = 0; c < ir.getNumChannels(); ++c)
            for (int n = 0; n < L; ++n) finite = finite && std::isfinite (ir.getSample (c, n));
        CHECK (finite, "non-finite IR sample");

        writeWav (dir.getChildFile (juce::String (e.name) + ".wav"), ir, 48000.0);
        csv << e.name << "," << ir.getNumChannels() << "," << (float) L / 48000.0f << ","
            << 10.0 * std::log10 (std::max (1e-12, en / ir.getNumChannels())) << "," << ms << "," << sc.micRoomRt[3] << "\n";
        std::printf ("  %-34s %2dch %.2fs  %6.1f dB  %6.1f ms\n", e.name, ir.getNumChannels(), (float) L / 48000.0f,
                     10.0 * std::log10 (std::max (1e-12, en / ir.getNumChannels())), ms);
    }
    dir.getChildFile ("examples.csv").replaceWithText (csv);
}

//==============================================================================
// Every installed pack (Documents/SubRoo Rooms/Packs): each source must resolve to a
// measured IR for every door state, and map cleanly onto stereo and 7.1.2.
static void testInstalledPacks (LocationLibrary& lib)
{
    std::printf ("[5] installed packs in %s\n", LocationLibrary::getPacksFolder().getFullPathName().toRawUTF8());
    MeasuredIRCache cache;
    int packs = 0, irs = 0, missing = 0, bad = 0;
    for (int li = lib.getNumBuiltIn(); li < lib.size(); ++li)
    {
        auto loc = lib.get (li);
        ++packs;
        const uint32_t states = 1u << (uint32_t) std::min (loc->numDoorBits, 4);
        float minDb = 1e9f, maxDb = -1e9f, maxLen = 0;
        for (int s = 0; s < (int) loc->sources.size(); ++s)
            for (int m = 0; m < (int) loc->mics.size(); ++m)
                for (uint32_t d = 0; d < states; ++d)
                {
                    auto ir = cache.load (*loc, s, m, d, 44100.0);
                    if (ir == nullptr) { ++missing; continue; }
                    ++irs;
                    double en = 0;
                    bool finite = true;
                    for (int c = 0; c < ir->rigIR.getNumChannels(); ++c)
                        for (int n = 0; n < ir->rigIR.getNumSamples(); ++n)
                        {
                            auto v = ir->rigIR.getSample (c, n);
                            finite = finite && std::isfinite (v);
                            en += (double) v * v;
                        }
                    juce::AudioBuffer<float> st (2, ir->rigIR.getNumSamples()), imm (10, ir->rigIR.getNumSamples());
                    st.clear(); imm.clear();
                    mapRigToLayout (ir->rigIR, loc->rig, juce::AudioChannelSet::stereo(), 0.0f, st, 1.0f, st.getNumSamples());
                    mapRigToLayout (ir->rigIR, loc->rig, juce::AudioChannelSet::create7point1point2(), 90.0f, imm, 1.0f, imm.getNumSamples());
                    if (! finite || en <= 0 || st.getMagnitude (0, st.getNumSamples()) <= 0) ++bad;
                    auto db = (float) (10.0 * std::log10 (std::max (1e-12, en)));
                    minDb = std::min (minDb, db); maxDb = std::max (maxDb, db);
                    maxLen = std::max (maxLen, (float) ir->rigIR.getNumSamples() / 44100.0f);
                }
        std::printf ("  %-48s %2d src  %d doors  %6.1f .. %5.1f dB  len<=%.2fs\n", loc->name.toRawUTF8(),
                     (int) loc->sources.size(), loc->numDoorBits, minDb, maxDb, maxLen);
    }
    for (auto& e : lib.getErrors()) { std::printf ("  pack error: %s\n", e.toRawUTF8()); ++bad; }
    std::printf ("  %d packs, %d IR loads, %d missing, %d bad\n", packs, irs, missing, bad);
    CHECK (missing == 0 && bad == 0, "installed pack problem");
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    auto outDir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "test-output");
    LocationLibrary lib;

    testConvolver();
    testSwap();
    testAllScenes (lib);
    renderExamples (lib, outDir);
    testInstalledPacks (lib);

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
