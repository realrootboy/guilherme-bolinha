// BolinhaArpTest <contentDir>: drives the engine's arpeggiator and checks the played notes against
// the expected patterns, step timing, octave range, gate, latch, key sync and host grid alignment.
#include "../Source/Engine.h"
#include <cstdio>

namespace
{
    constexpr int kM = 681;                         // first melody parameter
    constexpr int kArpOn = kM + 39, kKeySync = kM + 40, kGate = kM + 41, kLatch = kM + 42;
    constexpr int kPattern = kM + 43, kRate = kM + 44, kOct = kM + 45;
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 480;

    struct Ev { int note; bool on; double beat; };

    struct Rig
    {
        cure::Engine engine;
        std::vector<Ev> evs;
        double ppq = 0;
        juce::AudioBuffer<float> buf { 2, kBlock };

        Rig (std::shared_ptr<const cure::Library> lib, int patch)
        {
            engine.setLibrary (lib);
            engine.prepare (kSr, kBlock);
            engine.setPartPatch (0, patch);
            engine.trace = [this] (int, int, int note, bool on, int source, double beat)
            {
                if (source == 1) evs.push_back ({ note, on, beat });
            };
        }
        void set (int index, float v) { engine.applyEdit (0, 0, index, v); }
        // runs `beats` of host time at 120 BPM, sending the given MIDI at the start
        void run (double beats, std::vector<juce::MidiMessage> msgs = {}, double bpm = 120.0)
        {
            const int blocks = (int) std::lround (beats * 60.0 / bpm * kSr / kBlock);
            for (int b = 0; b < blocks; ++b)
            {
                juce::MidiBuffer midi;
                if (b == 0) for (auto& m : msgs) midi.addEvent (m, 0);
                engine.process (buf, nullptr, nullptr, midi, bpm, ppq, true);
                ppq += kBlock / kSr * bpm / 60.0;
            }
        }
        std::vector<int> onNotes() const
        {
            std::vector<int> r;
            for (auto& e : evs) if (e.on) r.push_back (e.note);
            return r;
        }
        std::vector<int> firstOnNotes (size_t n) const
        {
            auto r = onNotes();
            if (r.size() > n) r.resize (n);
            return r;
        }
    };

    int failures = 0;
    void check (bool ok, const juce::String& what)
    {
        std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what.toRawUTF8());
        if (! ok) ++failures;
    }
    juce::String join (const std::vector<int>& v)
    {
        juce::StringArray s;
        for (int n : v) s.add (juce::String (n));
        return s.joinIntoString (" ");
    }
    std::vector<juce::MidiMessage> chord (std::initializer_list<int> notes, bool on)
    {
        std::vector<juce::MidiMessage> m;
        for (int n : notes) m.push_back (on ? juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) : juce::MidiMessage::noteOff (1, n));
        return m;
    }

    constexpr int rate16 = 10;        // index of 1/16 in the 18-entry rate table
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: BolinhaArpTest <contentDir>\n"); return 1; }
    auto lib = std::make_shared<cure::Library>();
    std::string err;
    if (! lib->load (argv[1], err)) { std::printf ("load failed: %s\n", err.c_str()); return 1; }

    // first single-layer melody patch
    int patch = -1;
    for (int i = 0; i < (int) lib->patches.size() && patch < 0; ++i)
        if (lib->patches[(size_t) i].layers.size() == 1 && lib->patches[(size_t) i].layers[0].p.size() == 727) patch = i;
    std::printf ("using patch %d: %s\n", patch, lib->patches[(size_t) patch].name.c_str());

    auto setup = [&] (Rig& r, int pattern, int octaves, int rate = rate16)
    {
        r.set (kArpOn, 1); r.set (kKeySync, 1); r.set (kLatch, 0); r.set (kGate, 0.5f);
        r.set (kPattern, pattern / 7.0f); r.set (kOct, (octaves - 1) / 5.0f); r.set (kRate, rate / 17.0f);
    };

    std::printf ("patterns (held C4 E4 G4, pressed E4 C4 G4):\n");
    const char* names[8] = { "Up", "Down", "Up-Down", "Up-Down 2", "Down-Up", "Down-Up 2", "As Played", "Random" };
    const std::vector<int> expected[7] = {
        { 60, 64, 67, 60, 64, 67, 60, 64 },
        { 67, 64, 60, 67, 64, 60, 67, 64 },
        { 60, 64, 67, 64, 60, 64, 67, 64 },
        { 60, 64, 67, 67, 64, 60, 60, 64 },
        { 67, 64, 60, 64, 67, 64, 60, 64 },
        { 67, 64, 60, 60, 64, 67, 67, 64 },
        { 64, 60, 67, 64, 60, 67, 64, 60 } };
    for (int pat = 0; pat < 8; ++pat)
    {
        Rig r (lib, patch);
        setup (r, pat, 1);
        r.run (2.0, chord ({ 64, 60, 67 }, true));   // 8 sixteenths
        auto got = r.firstOnNotes (8);
        if (pat < 7)
            check (got == expected[pat], juce::String (names[pat]) + ": " + join (got));
        else
        {
            bool inSet = got.size() == 8;
            for (int n : got) inSet = inSet && (n == 60 || n == 64 || n == 67);
            check (inSet, juce::String (names[pat]) + ": " + join (got));
        }
    }

    std::printf ("octaves:\n");
    for (int oct = 1; oct <= 6; ++oct)
    {
        Rig r (lib, patch);
        setup (r, 0, oct);
        r.run (oct * 2 * 0.25 * 2, chord ({ 60, 67 }, true));   // two cycles
        auto got = r.firstOnNotes ((size_t) oct * 4);
        std::vector<int> exp;
        for (int c = 0; c < 2; ++c) for (int o = 0; o < oct; ++o) { exp.push_back (60 + 12 * o); exp.push_back (67 + 12 * o); }
        check (got == exp, juce::String (oct) + " oct: " + join (got));
    }

    std::printf ("rates (step length in beats at 120 BPM):\n");
    const double rateBeats[18] = { 6, 3, 1.5, 0.75, 0.375, 0.1875, 4, 2, 1, 0.5, 0.25, 0.125,
                                   8.0 / 3, 4.0 / 3, 2.0 / 3, 1.0 / 3, 1.0 / 6, 1.0 / 12 };
    const char* rateNames[18] = { "1/1D", "1/2D", "1/4D", "1/8D", "1/16D", "1/32D", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                                  "1/1T", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T" };
    for (int ri = 0; ri < 18; ++ri)
    {
        Rig r (lib, patch);
        setup (r, 0, 1, ri);
        r.run (rateBeats[ri] * 4 + 1e-3, chord ({ 60, 64 }, true));
        std::vector<double> on;
        for (auto& e : r.evs) if (e.on) on.push_back (e.beat);
        bool ok = on.size() >= 4;
        const double tol = 64.0 / kSr * 2.0;   // two engine sub-blocks
        for (size_t i = 1; ok && i < on.size(); ++i) ok = std::abs ((on[i] - on[i - 1]) - rateBeats[ri]) < tol;
        check (ok, juce::String (rateNames[ri]) + juce::String::formatted (": %d notes, step %.4f", (int) on.size(),
                                                                           on.size() > 1 ? on[1] - on[0] : 0.0));
    }

    std::printf ("gate:\n");
    for (float gate : { 0.25f, 0.5f, 0.9f })
    {
        Rig r (lib, patch);
        setup (r, 0, 1);
        r.set (kGate, gate);
        r.run (1.0, chord ({ 60, 64 }, true));
        double onAt = -1, len = -1;
        for (auto& e : r.evs)
        {
            if (e.on && onAt < 0) onAt = e.beat;
            else if (! e.on && onAt >= 0 && len < 0) len = e.beat - onAt;
        }
        check (std::abs (len - 0.25 * gate) < 0.01, juce::String::formatted ("gate %.2f -> note length %.3f beats (expected %.3f)", gate, len, 0.25 * gate));
    }

    std::printf ("latch / release:\n");
    {
        Rig r (lib, patch);
        setup (r, 0, 1);
        r.run (1.0, chord ({ 60, 64 }, true));
        r.run (0.0, {});
        const size_t before = r.onNotes().size();
        r.run (1.0, chord ({ 60, 64 }, false));
        check (r.onNotes().size() <= before + 1, "without latch the arp stops on key release");
    }
    {
        Rig r (lib, patch);
        setup (r, 0, 1);
        r.set (kLatch, 1);
        r.run (1.0, chord ({ 60, 64 }, true));
        r.run (1.0, chord ({ 60, 64 }, false));
        const auto n1 = r.onNotes().size();
        r.run (1.0, {});
        check (r.onNotes().size() >= n1 + 3, "latch keeps playing after release");
        r.evs.clear();
        r.run (1.0 - 1e-3, chord ({ 72 }, true));
        auto got = r.onNotes();
        bool only72 = ! got.empty();
        for (int n : got) only72 = only72 && n == 72;
        check (only72, "new chord after release replaces the latched notes: " + join (got));
    }

    std::printf ("sync:\n");
    {
        // key sync off: a note pressed mid-step starts on the next 1/16 of the host grid
        Rig r (lib, patch);
        setup (r, 0, 1);
        r.set (kKeySync, 0);
        r.run (0.1);
        r.run (1.0, chord ({ 60 }, true));
        const double first = r.evs.empty() ? -1 : r.evs.front().beat;
        check (std::abs (first - 0.25) < 0.005, juce::String::formatted ("key sync off: first step at beat %.4f (grid 0.25)", first));
    }
    {
        // key sync on: every new note restarts the pattern immediately
        Rig r (lib, patch);
        setup (r, 0, 1);
        r.run (0.6, chord ({ 60, 64, 67 }, true));
        r.evs.clear();
        r.run (0.5, { juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100) });
        auto got = r.onNotes();
        check (! got.empty() && got.front() == 60, "key sync on: pattern restarts from the lowest note: " + join (got));
    }
    {
        // tempo follows the host
        Rig r (lib, patch);
        setup (r, 0, 1);
        r.run (2.0, chord ({ 60, 64 }, true), 90.0);
        check (r.onNotes().size() == 8, juce::String::formatted ("90 BPM host: %d sixteenths in 2 beats", (int) r.onNotes().size()));
    }

    std::printf (failures == 0 ? "ALL ARP TESTS PASSED\n" : "%d ARP TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
