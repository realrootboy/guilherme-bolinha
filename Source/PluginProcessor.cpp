#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ContentImport.h"

namespace
{
    constexpr int kMaxPatches = 4096;

    juce::StringArray velocityCurves() { return { "Linear", "Soft", "Hard", "S-Curve", "Fixed" }; }
    juce::StringArray tuningScales()
    {
        juce::StringArray s;
        for (int i = 0; i < cure::kNumTuningScales; ++i) s.add (cure::tuningScaleName (i));
        return s;
    }
}

BolinhaProcessor::BolinhaProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Main", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Sub", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "GuilhermeBolinha", createLayout())
{
    for (auto& s : userScales) for (auto& c : s) c = 0.0f;
    std::string err;
    library = cure::getSharedLibrary (err);
    loadError = err;
    engine.setLibrary (library);

    patchParam = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter ("patch"));
    selectedParam = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter ("channel"));
    for (auto* p : getParameters())
        if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            raw[wp->paramID.toStdString()] = apvts.getRawParameterValue (wp->paramID);

    if (library != nullptr) engine.gmReset();
}

std::atomic<float>* BolinhaProcessor::rp (const char* id)
{
    auto it = raw.find (id);
    jassert (it != raw.end());
    return it->second;
}

juce::AudioProcessorValueTreeState::ParameterLayout BolinhaProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto f = [&] (const char* id, const char* name, float lo, float hi, float def)
    { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi), def)); };
    auto b = [&] (const char* id, const char* name, bool def)
    { l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto i = [&] (const char* id, const char* name, int lo, int hi, int def)
    { l.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };
    auto c = [&] (const char* id, const char* name, const StringArray& items, int def)
    { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };

    i ("patch", "Patch", 0, kMaxPatches - 1, 0);
    f ("volume", "Master Volume", 0.0f, 1.0f, 0.7071f);
    b ("gm", "GM Multitimbral (16 ch)", false);
    b ("priority", "Priority Mode", false);
    i ("channel", "Selected Channel", 1, 16, 1);
    // global (mixer)
    f ("bpm", "Master BPM", 20.0f, 300.0f, 120.0f);
    b ("sync", "BPM Sync to Host", true);
    i ("voices", "Master Voice Limit", 1, 256, 256);
    i ("mtranspose", "Master Transpose", -48, 48, 0);
    f ("mtune", "Master Tune (Hz)", 415.3f, 466.2f, 440.0f);
    b ("ifx", "IFXs Master", true);
    // send-return
    b ("chorusOn", "Chorus On", true);
    f ("chorus", "Chorus Return", 0.0f, 1.0f, 1.0f);
    f ("chorusRate", "Chorus Rate", 0.0f, 1.0f, 0.3f);
    f ("chorusWidth", "Chorus Width", 0.0f, 1.0f, 0.5f);
    f ("chorusSpread", "Chorus Spread", 0.0f, 1.0f, 1.0f);
    b ("reverbOn", "Reverb On", true);
    f ("reverb", "Reverb Return", 0.0f, 1.0f, 1.0f);
    f ("reverbPre", "Reverb Pre-delay", 0.0f, 1.0f, 0.05f);
    f ("reverbTime", "Reverb Time", 0.0f, 1.0f, 0.7f);
    f ("reverbTone", "Reverb Tone", 0.0f, 1.0f, 0.55f);
    // setup
    c ("velCurve", "Velocity Curve", velocityCurves(), 0);
    b ("transposeDrum", "Master Transpose to Drum", false);
    b ("tuneDrum", "Master Tune to Drum", false);
    b ("ignoreBpm", "Ignore Patch BPM", false);
    b ("mixdown", "Stereo Mix-down", true);
    b ("acceptPC", "Accept Program Change", true);
    b ("acceptCC", "Accept Controllers", true);
    c ("scale", "Tuning Scale", tuningScales(), 0);
    return l;
}

bool BolinhaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.outputBuses.size() > 1)
    {
        const auto& sub = layouts.outputBuses[1];
        if (! sub.isDisabled() && sub != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void BolinhaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    keyboardState.reset();
    const juce::ScopedLock sl (editLock);
    for (int p = 0; p < kParts; ++p) reapplyEdits (p);
}

void BolinhaProcessor::reapplyEdits (int p)
{
    auto& e = edits[(size_t) p];
    if (e.patch != engine.part (p).patch) return;
    for (auto& [key, value] : e.values)
        engine.applyEdit (p, key >> 16, key & 0xffff, value);
}

void BolinhaProcessor::syncGlobals()
{
    auto& g = engine.g;
    const float v = rp ("volume")->load();
    engine.masterGain = 2.0f * v * v;
    engine.chorusLevel = rp ("chorus")->load();
    engine.reverbLevel = rp ("reverb")->load();
    engine.multitimbral = rp ("gm")->load() >= 0.5f;
    g.priority = rp ("priority")->load() >= 0.5f;
    g.selectedPart = juce::jlimit (0, 15, (int) rp ("channel")->load() - 1);
    g.bpm = rp ("bpm")->load();
    g.hostSync = rp ("sync")->load() >= 0.5f;
    g.masterVoiceLimit = (int) rp ("voices")->load();
    g.masterTranspose = (int) rp ("mtranspose")->load();
    g.masterTuneHz = rp ("mtune")->load();
    g.ifxMaster = rp ("ifx")->load() >= 0.5f;
    g.chorusOn = rp ("chorusOn")->load() >= 0.5f;
    g.chorusRate = rp ("chorusRate")->load();
    g.chorusWidth = rp ("chorusWidth")->load();
    g.chorusSpread = rp ("chorusSpread")->load();
    g.reverbOn = rp ("reverbOn")->load() >= 0.5f;
    g.reverbPredelay = rp ("reverbPre")->load();
    g.reverbTime = rp ("reverbTime")->load();
    g.reverbTone = rp ("reverbTone")->load();
    g.velocityCurve = (int) rp ("velCurve")->load();
    g.transposeToDrum = rp ("transposeDrum")->load() >= 0.5f;
    g.tuneToDrum = rp ("tuneDrum")->load() >= 0.5f;
    g.ignorePatchBpm = rp ("ignoreBpm")->load() >= 0.5f;
    g.stereoMixdown = rp ("mixdown")->load() >= 0.5f;
    g.acceptProgramChange = rp ("acceptPC")->load() >= 0.5f;
    g.acceptControllers = rp ("acceptCC")->load() >= 0.5f;
    g.tuningScale = (int) rp ("scale")->load();
    for (size_t s = 0; s < 8; ++s)
        for (size_t k = 0; k < 12; ++k) g.userScales[s][k] = userScales[s][k].load();
}

void BolinhaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (library == nullptr) { buffer.clear(); return; }
    syncGlobals();

    // on-screen keyboard and wheels play the selected channel
    const int ch = engine.g.selectedPart + 1;
    juce::MidiBuffer screen;
    keyboardState.processNextMidiBuffer (screen, 0, n, false);
    for (const auto meta : screen)
    {
        auto m = meta.getMessage();
        m.setChannel (ch);
        midi.addEvent (m, meta.samplePosition);
    }
    if (const int b = pendingBend.exchange (-1); b >= 0) midi.addEvent (juce::MidiMessage::pitchWheel (ch, b), 0);
    if (const int m = pendingMod.exchange (-1); m >= 0) midi.addEvent (juce::MidiMessage::controllerEvent (ch, 1, m), 0);

    if (panicRequest.exchange (false)) engine.panic();
    if (gmResetRequest.exchange (false)) engine.gmReset();

    const int wanted = juce::jlimit (0, numPatches() - 1, (int) *patchParam);
    if (wanted != currentPatch)
    {
        currentPatch = wanted;
        engine.setPartPatch (0, wanted);
    }
    for (int p = 0; p < kParts; ++p)
    {
        const int req = engine.part (p).requestedPatch.exchange (-1);
        if (req >= 0) engine.setPartPatch (p, req);
    }
    {
        int s1, n1, s2, n2;
        editFifo.prepareToRead (editFifo.getNumReady(), s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) { auto& e = editQueue[(size_t) (s1 + i)]; engine.applyEdit (e.part, e.layer, e.index, e.value); }
        for (int i = 0; i < n2; ++i) { auto& e = editQueue[(size_t) (s2 + i)]; engine.applyEdit (e.part, e.layer, e.index, e.value); }
        editFifo.finishedRead (n1 + n2);
    }

    double bpm = 0, ppq = 0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bp = pos->getBpm()) bpm = *bp;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }

    float* subL = nullptr;
    float* subR = nullptr;
    juce::AudioBuffer<float> mainBuf (buffer.getArrayOfWritePointers(), 2, n);
    if (getBusCount (false) > 1 && getBus (false, 1)->isEnabled() && buffer.getNumChannels() >= 4)
    {
        subL = buffer.getWritePointer (2);
        subR = buffer.getWritePointer (3);
    }
    engine.process (mainBuf, subL, subR, midi, bpm, ppq, playing);
}

// ---------------------------------------------------------------------------
void BolinhaProcessor::setCurrentProgram (int index)
{
    if (patchParam != nullptr)
        *patchParam = juce::jlimit (0, juce::jmax (0, numPatches() - 1), index);
}

juce::String BolinhaProcessor::patchDisplayName (int index) const
{
    if (library == nullptr || index < 0 || index >= numPatches())
        return "<no content>";
    const auto& p = library->patches[(size_t) index];
    return juce::String (p.bank).fromFirstOccurrenceOf (" ", false, false) + " - " + juce::String (p.name);
}

const juce::String BolinhaProcessor::getProgramName (int index)
{
    return patchDisplayName (index);
}

int BolinhaProcessor::partPatch (int p) const
{
    return engine.part (p).shownPatch.load();
}

void BolinhaProcessor::loadPatch (int p, int patchIndex)
{
    if (numPatches() == 0) return;
    patchIndex = juce::jlimit (0, numPatches() - 1, patchIndex);
    if (p == 0)
    {
        patchParam->beginChangeGesture();
        *patchParam = patchIndex;
        patchParam->endChangeGesture();
        updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true));
    }
    else
        engine.part (p).requestedPatch = patchIndex;
    engine.part (p).shownPatch = patchIndex;
    const juce::ScopedLock sl (editLock);
    edits[(size_t) p] = { patchIndex, {} };
}

void BolinhaProcessor::selectPart (int p)
{
    selectedParam->beginChangeGesture();
    *selectedParam = juce::jlimit (1, 16, p + 1);
    selectedParam->endChangeGesture();
}

// ---------------------------------------------------------------------------
int BolinhaProcessor::numLayers (int p) const
{
    const int patch = partPatch (p);
    if (library == nullptr || patch < 0 || patch >= numPatches()) return 0;
    return (int) library->patches[(size_t) patch].layers.size();
}

bool BolinhaProcessor::layerIsDrum (int p, int layer) const
{
    const int patch = partPatch (p);
    if (layer < 0 || layer >= numLayers (p)) return false;
    const auto& l = library->patches[(size_t) patch].layers[(size_t) layer];
    return l.p.size() > 727 && l.p[0] >= 0.5f;
}

juce::String BolinhaProcessor::layerName (int p, int layer) const
{
    if (layer < 0 || layer >= numLayers (p)) return {};
    return library->patches[(size_t) partPatch (p)].layers[(size_t) layer].name;
}

float BolinhaProcessor::layerValue (int p, int layer, int index) const
{
    if (layer < 0 || layer >= numLayers (p)) return 0.0f;
    {
        const juce::ScopedLock sl (editLock);
        const auto& e = edits[(size_t) p];
        if (e.patch == partPatch (p))
            if (auto it = e.values.find ((layer << 16) | index); it != e.values.end())
                return it->second;
    }
    const auto& v = library->patches[(size_t) partPatch (p)].layers[(size_t) layer].p;
    return index >= 0 && index < (int) v.size() ? v[(size_t) index] : 0.0f;
}

void BolinhaProcessor::setLayerValue (int p, int layer, int index, float value)
{
    if (layer < 0 || layer >= numLayers (p)) return;
    value = juce::jlimit (0.0f, 1.0f, value);
    {
        const juce::ScopedLock sl (editLock);
        auto& e = edits[(size_t) p];
        if (e.patch != partPatch (p)) { e.values.clear(); e.patch = partPatch (p); }
        e.values[(layer << 16) | index] = value;
    }
    int s1, n1, s2, n2;
    editFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) editQueue[(size_t) s1] = { p, layer, index, value };
    else if (n2 > 0) editQueue[(size_t) s2] = { p, layer, index, value };
    editFifo.finishedWrite (n1 + n2);
}

bool BolinhaProcessor::saveToUserBank (int p, const juce::String& name, juce::String& message)
{
    const int patch = partPatch (p);
    if (library == nullptr || patch < 0) { message = "Nothing to save"; return false; }
    cure::Patch out = library->patches[(size_t) patch];
    out.name = name.isEmpty() ? out.name : name.toStdString();
    for (int l = 0; l < (int) out.layers.size(); ++l)
        for (size_t i = 0; i < out.layers[(size_t) l].p.size(); ++i)
            out.layers[(size_t) l].p[i] = layerValue (p, l, (int) i);
    std::string err;
    if (! cure::appendUserPatch (out, err)) { message = err; return false; }
    reloadLibrary();
    loadPatch (p, numPatches() - 1);
    message = "Saved \"" + juce::String (out.name) + "\" to " + cure::kUserBank;
    return true;
}

// ---------------------------------------------------------------------------
juce::File BolinhaProcessor::contentDir() const
{
    return cure::findContentDir();
}

void BolinhaProcessor::reloadLibrary()
{
    std::string err;
    auto lib = cure::reloadSharedLibrary (err);
    suspendProcessing (true);
    {
        const juce::ScopedLock sl (getCallbackLock());
        std::array<int, kParts> patches {};
        for (int p = 0; p < kParts; ++p) patches[(size_t) p] = engine.part (p).patch;
        const bool hadLibrary = library != nullptr;
        library = lib;
        loadError = err;
        engine.setLibrary (library);
        if (library != nullptr)
        {
            if (! hadLibrary) engine.gmReset();
            else
                for (int p = 0; p < kParts; ++p)
                    if (patches[(size_t) p] >= 0) engine.setPartPatch (p, juce::jmin (patches[(size_t) p], numPatches() - 1));
        }
        currentPatch = -1;
        engine.reset();
    }
    suspendProcessing (false);
    updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true));
    sendChangeMessage();
}

bool BolinhaProcessor::importContent (const juce::File& source, juce::String& message)
{
    int n = 0;
    juce::String err;
    const auto dest = cure::userContentDir();
    if (! cure::importContent (source, dest, err, n)) { message = err; return false; }
    cure::setContentDirOverride ({});
    reloadLibrary();
    message = juce::String (n) + " files imported to " + dest.getFullPathName();
    return library != nullptr;
}

bool BolinhaProcessor::useContentFolder (const juce::File& folder, juce::String& message)
{
    cure::setContentDirOverride (folder);
    reloadLibrary();
    message = library != nullptr ? "Using " + folder.getFullPathName() : loadError;
    return library != nullptr;
}

// ---------------------------------------------------------------------------
void BolinhaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree parts ("Parts");
    for (int p = 0; p < kParts; ++p)
    {
        auto& ps = engine.part (p);
        juce::ValueTree t ("Part");
        t.setProperty ("patch", partPatch (p), nullptr);
        t.setProperty ("volume", ps.volume.load(), nullptr);
        t.setProperty ("pan", ps.panOffset.load(), nullptr);
        t.setProperty ("chorus", ps.chorusCC.load(), nullptr);
        t.setProperty ("reverb", ps.reverbCC.load(), nullptr);
        t.setProperty ("on", ps.on.load(), nullptr);
        t.setProperty ("link", ps.link.load(), nullptr);
        t.setProperty ("bus", ps.bus.load(), nullptr);
        t.setProperty ("transpose", ps.transpose.load(), nullptr);
        t.setProperty ("voiceLimit", ps.voiceLimit.load(), nullptr);
        const juce::ScopedLock sl (editLock);
        const auto& e = edits[(size_t) p];
        if (e.patch == partPatch (p))
            for (auto& [key, value] : e.values)
            {
                juce::ValueTree ed ("Edit");
                ed.setProperty ("layer", key >> 16, nullptr);
                ed.setProperty ("index", key & 0xffff, nullptr);
                ed.setProperty ("value", value, nullptr);
                t.appendChild (ed, nullptr);
            }
        parts.appendChild (t, nullptr);
    }
    state.removeChild (state.getChildWithName ("Parts"), nullptr);
    state.appendChild (parts, nullptr);
    juce::String scales;
    for (auto& s : userScales) for (auto& c : s) scales << juce::String (c.load(), 1) << " ";
    state.setProperty ("userScales", scales.trim(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BolinhaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    apvts.replaceState (state);
    auto scales = juce::StringArray::fromTokens (state.getProperty ("userScales").toString(), " ", "");
    for (int i = 0; i < juce::jmin (96, scales.size()); ++i)
        userScales[(size_t) i / 12][(size_t) i % 12] = scales[i].getFloatValue();

    auto parts = state.getChildWithName ("Parts");
    for (int p = 0; p < juce::jmin (kParts, parts.getNumChildren()); ++p)
    {
        auto t = parts.getChild (p);
        auto& ps = engine.part (p);
        ps.volume = (float) t.getProperty ("volume", 100.0 / 127.0);
        ps.panOffset = (float) t.getProperty ("pan", 0.0);
        ps.chorusCC = (float) t.getProperty ("chorus", -1.0);
        ps.reverbCC = (float) t.getProperty ("reverb", -1.0);
        ps.on = (bool) t.getProperty ("on", true);
        ps.link = (bool) t.getProperty ("link", false);
        ps.bus = (int) t.getProperty ("bus", 0);
        ps.transpose = (int) t.getProperty ("transpose", 0);
        ps.voiceLimit = (int) t.getProperty ("voiceLimit", -1);
        const int patch = (int) t.getProperty ("patch", -1);
        if (patch >= 0 && patch < numPatches())
        {
            if (p > 0) ps.requestedPatch = patch;   // part 0 follows the "patch" parameter
            ps.shownPatch = patch;
        }
        {
            const juce::ScopedLock sl (editLock);
            edits[(size_t) p] = { partPatch (p), {} };
        }
        for (auto ed : t)
            if (ed.hasType ("Edit"))
            {
                const int layer = ed.getProperty ("layer"), index = ed.getProperty ("index");
                const float value = ed.getProperty ("value");
                {
                    const juce::ScopedLock sl (editLock);
                    edits[(size_t) p].values[(layer << 16) | index] = value;
                }
                int s1, n1, s2, n2;
                editFifo.prepareToWrite (1, s1, n1, s2, n2);
                if (n1 > 0) editQueue[(size_t) s1] = { p, layer, index, value };
                else if (n2 > 0) editQueue[(size_t) s2] = { p, layer, index, value };
                editFifo.finishedWrite (n1 + n2);
            }
    }
    currentPatch = -1;
}

juce::AudioProcessorEditor* BolinhaProcessor::createEditor()
{
    return new BolinhaEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BolinhaProcessor();
}
