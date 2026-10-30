#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"

class BolinhaProcessor : public juce::AudioProcessor, public juce::ChangeBroadcaster, private juce::Timer
{
public:
    BolinhaProcessor();
    ~BolinhaProcessor() override { stopTimer(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Guilherme Bolinha"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return juce::jmax (1, numPatches()); }
    // The host program follows the patch of the channel selected in the editor.
    int getCurrentProgram() override { return juce::jmax (0, partPatch (selectedPart())); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- content ----
    int numPatches() const { return library ? (int) library->patches.size() : 0; }
    const cure::Library* getLibrary() const { return library.get(); }
    juce::String getLoadError() const { return loadError; }
    juce::String patchDisplayName (int index) const;
    juce::File contentDir() const;
    // Imports content from the original installer or a folder, then reloads it.
    bool importContent (const juce::File& source, juce::String& message);
    bool useContentFolder (const juce::File& folder, juce::String& message);
    void reloadLibrary();

    // ---- channels (mixer) ----
    static constexpr int kParts = 16;
    cure::PartState& part (int p) { return engine.part (p); }
    int partPatch (int p) const;
    void loadPatch (int part, int patchIndex);
    int selectedPart() const { return juce::jlimit (0, kParts - 1, (int) *selectedParam - 1); }
    void selectPart (int p);

    // ---- patch editing (edit panel / sequencer) ----
    // Current value of a layer parameter of a part (patch value or edit).
    float layerValue (int part, int layer, int index) const;
    void setLayerValue (int part, int layer, int index, float value);
    int numLayers (int part) const;
    bool layerIsDrum (int part, int layer) const;
    juce::String layerName (int part, int layer) const;
    bool saveToUserBank (int part, const juce::String& name, juce::String& message);

    // ---- performance ----
    juce::MidiKeyboardState keyboardState;
    std::atomic<int> pendingBend { -1 }, pendingMod { -1 };
    void panic() { panicRequest = true; }
    void gmReset() { gmResetRequest = true; }
    float mainLevel() { return engine.mainMeter.exchange (0); }
    float subLevel() { return engine.subMeter.exchange (0); }
    double currentBpm() const { return engine.currentBpm.load(); }

    // user tuning scales (cents per note)
    std::array<std::array<std::atomic<float>, 12>, 8> userScales;

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioParameterInt* patchParam = nullptr;
    juce::AudioParameterInt* selectedParam = nullptr;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void syncGlobals();
    void timerCallback() override;
    void reapplyEdits (int part);

    juce::String loadError;
    std::shared_ptr<const cure::Library> library;
    cure::Engine engine;
    int currentPatch = -1;
    int hostProgram = -1;

    struct Edit { int part, layer, index; float value; };
    juce::AbstractFifo editFifo { 2048 };
    std::array<Edit, 2048> editQueue {};
    // edits per part, keyed by (layer << 16 | index); owned by the message thread
    struct PartEdits { int patch = -1; std::map<int, float> values; };
    std::array<PartEdits, kParts> edits;
    juce::CriticalSection editLock;

    std::atomic<bool> panicRequest { false }, gmResetRequest { false };
    std::unordered_map<std::string, std::atomic<float>*> raw;
    std::atomic<float>* rp (const char* id);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BolinhaProcessor)
};
