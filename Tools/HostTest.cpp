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
    inst->releaseResources();
    return failed ? 1 : 0;
}
