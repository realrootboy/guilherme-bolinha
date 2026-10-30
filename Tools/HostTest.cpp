// BolinhaHostTest <plugin path>: loads the built plugin like a DAW would, plays notes, prints levels.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: BolinhaHostTest <plugin>\n"); return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    juce::AudioPluginFormatManager fm;
    fm.addDefaultFormats();
    juce::OwnedArray<juce::PluginDescription> types;
    for (auto* f : fm.getFormats())
        f->findAllTypesForFile (types, argv[1]);
    if (types.isEmpty()) { std::printf ("no plugin found in %s\n", argv[1]); return 1; }
    juce::String err;
    auto inst = fm.createPluginInstance (*types[0], 44100, 512, err);
    if (! inst) { std::printf ("load failed: %s\n", err.toRawUTF8()); return 1; }
    std::printf ("loaded: %s (%s), programs=%d\n", inst->getName().toRawUTF8(), types[0]->pluginFormatName.toRawUTF8(), inst->getNumPrograms());
    inst->setPlayConfigDetails (0, 2, 44100, 512);
    inst->prepareToPlay (44100, 512);

    bool failed = inst->getNumPrograms() <= 0;
    std::vector<int> programs;
    for (int prog : { 0, 128, 300, 1000 })
        if (prog < inst->getNumPrograms()) programs.push_back (prog);
    for (int i = 0; i < inst->getNumPrograms(); ++i)
        if (inst->getProgramName (i).containsIgnoreCase ("Arpeggiator"))
        { programs.push_back (i); break; }
    for (int prog : programs)
    {
        inst->setCurrentProgram (prog);
        juce::AudioBuffer<float> buf (2, 512);
        double sum = 0; float peak = 0;
        for (int b = 0; b < 200; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 2) for (int n : { 36, 60, 64, 67 }) midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
            if (b == 120) for (int n : { 36, 60, 64, 67 }) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
            buf.clear();
            inst->processBlock (buf, midi);
            for (int c = 0; c < buf.getNumChannels(); ++c)
                for (int n = 0; n < buf.getNumSamples(); ++n)
                    if (! std::isfinite (buf.getSample (c, n))) failed = true;
            peak = juce::jmax (peak, buf.getMagnitude (0, 512));
            sum += buf.getRMSLevel (0, 0, 512);
        }
        if (peak < 1.0e-5f || ! std::isfinite (sum)) failed = true;
        std::printf ("program %4d %-34s peak=%.3f avgRMS=%.4f\n", prog, inst->getProgramName (prog).toRawUTF8(), peak, sum / 200);
    }

    // Program sync through the host's program parameter, as FL Studio's preset name and arrows use it.
    auto findParam = [&] (const juce::String& name) -> juce::AudioProcessorParameter*
    {
        for (auto* p : inst->getParameters()) if (p->getName (64) == name) return p;
        return nullptr;
    };
    auto* channel = findParam ("Selected Channel");
    auto* patch = findParam ("Patch");
    constexpr float maxPatch = 4095.0f; // the "Patch" parameter's range is 0..4095
    auto settle = [&]
    {
        for (int i = 0; i < 6; ++i)
        {
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            inst->processBlock (buf, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        }
    };
    auto selectChannel = [&] (int ch) { channel->setValueNotifyingHost ((float) (ch - 1) / 15.0f); settle(); };
    auto expectProgram = [&] (int expected, const char* what)
    {
        const int got = inst->getCurrentProgram();
        std::printf ("program sync: %-44s host=%4d expected=%4d %s\n", what, got, expected, got == expected ? "ok" : "FAILED");
        if (got != expected) failed = true;
    };
    if (channel == nullptr || patch == nullptr || inst->getNumPrograms() < 200) { std::printf ("program sync: parameters missing\n"); failed = true; }
    else
    {
        selectChannel (1);
        inst->setCurrentProgram (55); settle();
        expectProgram (55, "host picks program on channel 1");
        if (juce::roundToInt (patch->getValue() * maxPatch) != 55) { std::printf ("program sync: channel 1 patch not loaded\n"); failed = true; }
        selectChannel (5);
        inst->setCurrentProgram (66); settle();
        expectProgram (66, "host picks program on channel 5");
        selectChannel (1);
        expectProgram (55, "selecting channel 1 shows its patch");
        selectChannel (5);
        expectProgram (66, "selecting channel 5 shows its patch");
        selectChannel (1);
        patch->setValueNotifyingHost (77.0f / maxPatch); settle();
        expectProgram (77, "plugin changes channel 1 patch");
        inst->setCurrentProgram (inst->getCurrentProgram() + 1); settle();
        expectProgram (78, "host next-preset arrow");

        // A program change that only reaches the processor's parameter queue (no message loop,
        // so the edit controller never sees it) must still load the patch.
        if (types[0]->pluginFormatName != "VST3") {}
        else if (auto* program = findParam ("Program"))
        {
            program->setValue (90.0f / (float) (inst->getNumPrograms() - 1));
            for (int i = 0; i < 4; ++i)
            {
                juce::AudioBuffer<float> buf (2, 512);
                juce::MidiBuffer midi;
                inst->processBlock (buf, midi);
            }
            const int loaded = juce::roundToInt (patch->getValue() * maxPatch);
            std::printf ("program sync: %-44s patch=%4d expected=%4d %s\n", "program change via processor queue only", loaded, 90, loaded == 90 ? "ok" : "FAILED");
            if (loaded != 90) failed = true;
        }
        else { std::printf ("program sync: host program parameter missing\n"); failed = true; }
    }

    inst->releaseResources();
    return failed ? 1 : 0;
}
