// BolinhaRender <dataDir> <outDir> [patchIndex ...]
// Renders a short phrase for each patch to 44.1 kHz WAV and prints peak/RMS levels.
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/Engine.h"
#include "../Source/ContentImport.h"
#include <cmath>

int main (int argc, char** argv)
{
    if (argc == 4 && juce::String (argv[1]) == "--test-user-bank")
    {
        cure::Library factory;
        std::string error;
        if (! factory.load (argv[2], error)) { std::fprintf (stderr, "%s\n", error.c_str()); return 1; }
        auto file = juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]).getChildFile ("099 User.pth");
        if (file.existsAsFile()) { std::fprintf (stderr, "Use an empty test directory\n"); return 1; }
        auto patch = factory.patches.front();
        patch.name = "Bolinha user bank test";
        patch.layers[0].p[3] = 0.123f;
        const auto path = file.getFullPathName().toStdString();
        if (! cure::appendUserPatch (patch, error, path)) { std::fprintf (stderr, "%s\n", error.c_str()); return 1; }
        patch.name = "Bolinha user bank two";
        if (! cure::appendUserPatch (patch, error, path)) { std::fprintf (stderr, "%s\n", error.c_str()); return 1; }
        cure::Library restored;
        if (! restored.load (argv[2], error, path)
            || restored.patches.size() != factory.patches.size() + 2
            || restored.patches.back().name != patch.name
            || restored.patches.back().layers[0].p[3] != 0.123f
            || restored.bankNames.back() != "099 User")
        { std::fprintf (stderr, "User bank round trip failed: %s\n", error.c_str()); return 1; }
        std::printf ("User bank saved separately and restored: %d factory + 2 user patches\n", (int) factory.patches.size());
        return 0;
    }
    if (argc == 4 && juce::String (argv[1]) == "--import")
    {
        juce::String err;
        int n = 0;
        const bool ok = cure::importContent (juce::File (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2])),
                                             juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]), err, n);
        std::printf ("%s: %d files%s%s\n", ok ? "imported" : "failed", n, ok ? "" : " - ", err.toRawUTF8());
        return ok ? 0 : 1;
    }
    if (argc < 3) { std::printf ("usage: BolinhaRender <dataDir> <outDir> [patch...]\n       BolinhaRender --import <setup.exe|folder> <destDir>\n"); return 1; }
    auto lib = std::make_shared<cure::Library>();
    std::string err;
    if (! lib->load (argv[1], err)) { std::printf ("load failed: %s\n", err.c_str()); return 1; }
    std::printf ("loaded %d patches in %d banks\n", (int) lib->patches.size(), (int) lib->bankNames.size());

    std::vector<int> list;
    for (int i = 3; i < argc; ++i) list.push_back (std::atoi (argv[i]));
    if (list.empty()) for (int i = 0; i < (int) lib->patches.size(); ++i) list.push_back (i);

    juce::File outDir (argv[2]);
    outDir.createDirectory();
    const double sr = 44100;
    const int block = 256;
    int silent = 0, nearFullScale = 0, invalid = 0;

    for (int idx : list)
    {
        if (idx < 0 || idx >= (int) lib->patches.size())
        { std::fprintf (stderr, "invalid patch index: %d\n", idx); return 1; }
        cure::Engine engine;
        engine.setLibrary (lib);
        engine.setPartPatch (0, idx);
        engine.prepare (sr, block);
        const auto& patch = lib->patches[(size_t) idx];
        const bool drum = ! patch.layers.empty() && patch.layers[0].p.size() > 727;

        const int total = (int) (sr * 4.0);
        juce::AudioBuffer<float> out (2, total), buf (2, block);
        // phrase: chord C-E-G for 1.5 s, then single notes; drums get a beat
        struct Ev { double t; int note; bool on; };
        std::vector<Ev> evs;
        if (drum)
        {
            const int pattern[8] = { 36, 42, 38, 42, 36, 36, 38, 46 };
            for (int i = 0; i < 16; ++i) { evs.push_back ({ i * 0.2, pattern[i % 8], true }); evs.push_back ({ i * 0.2 + 0.1, pattern[i % 8], false }); }
        }
        else
        {
            for (int n : { 60, 64, 67 }) { evs.push_back ({ 0.0, n, true }); evs.push_back ({ 1.5, n, false }); }
            const int mel[4] = { 72, 69, 65, 48 };
            for (int i = 0; i < 4; ++i) { evs.push_back ({ 1.8 + i * 0.4, mel[i], true }); evs.push_back ({ 2.1 + i * 0.4, mel[i], false }); }
        }
        double ppq = 0;
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            buf.setSize (2, n, false, false, true);
            juce::MidiBuffer midi;
            for (auto& e : evs)
            {
                const int s = (int) (e.t * sr) - pos;
                if (s >= 0 && s < n)
                    midi.addEvent (e.on ? juce::MidiMessage::noteOn (1, e.note, (juce::uint8) 100) : juce::MidiMessage::noteOff (1, e.note), s);
            }
            engine.process (buf, nullptr, nullptr, midi, 120.0, ppq, true);
            ppq += n * 2.0 / sr;
            for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, n);
        }
        const float peak = out.getMagnitude (0, total);
        const float rms = 0.5f * (out.getRMSLevel (0, 0, total) + out.getRMSLevel (1, 0, total));
        if (peak < 1e-4f) ++silent;
        if (peak > 0.99f) ++nearFullScale;
        bool valid = true;
        for (int c = 0; c < out.getNumChannels(); ++c)
            for (int n = 0; n < total; ++n)
                if (! std::isfinite (out.getSample (c, n)) || std::abs (out.getSample (c, n)) > 1.00001f) valid = false;
        if (! valid) ++invalid;
        std::printf ("%4d  %-22s %-24s layers=%d peak=%.3f rms=%.4f%s\n", idx, patch.bank.c_str(), patch.name.c_str(),
                     (int) patch.layers.size(), peak, rms, peak < 1e-4f ? "  SILENT" : "");
        if (argc > 3 || idx % 25 == 0)
        {
            auto f = outDir.getChildFile (juce::String (idx).paddedLeft ('0', 4) + " " + juce::File::createLegalFileName (patch.name) + ".wav");
            f.deleteFile();
            juce::WavAudioFormat wav;
            if (auto* os = f.createOutputStream().release())
                if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os, sr, 2, 16, {}, 0)))
                    w->writeFromAudioSampleBuffer (out, 0, total);
        }
    }
    std::printf ("rendered %d patches, silent=%d, nearFullScale=%d, invalid=%d\n", (int) list.size(), silent, nearFullScale, invalid);
    return silent == 0 && invalid == 0 ? 0 : 1;
}
