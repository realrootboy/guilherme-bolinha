// Cure synthesis engine (Guilherme Bolinha): 16 multitimbral parts, each playing a Cure patch
// (1..n layers, melody or drum), with arpeggiator, step sequencer and IFX/SFX.
#pragma once
#include "CureData.h"
#include "Effects.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <functional>

namespace cure
{

// ---------------------------------------------------------------------------
// Decoded parameters of one layer (normalized values -> engine units).
struct DrumKey
{
    int wave = 0;
    bool reverse = false;
    float semis = 0, cents = 0;
    float cutoff = 1, reso = 0, filtVel = 0;
    float gain = 1, pan = 0.5f;
    float attack = 0, decay = 20;
    bool hold = false;
    float velAmt = 0.95f;
    int muteGroup = 0;     // 0 none, 1 A, 2 B
    int toIfx = 0;         // 0 dry, 1 IFX1, 2 IFX2
    float chorus = 0, reverb = 0;
};

struct SeqLine
{
    bool on = false;
    int base = 128;        // 0..127 fixed note, >=128 => held note #(base-128)
    std::array<uint8_t, 64> code {};   // 0..96 offset (+48), 97 tie, 98 tie+end, 99 cut, 100 empty
    std::array<float, 64> vel {};
};

struct LayerParams
{
    bool drum = false, on = true;
    float gain = 1, pan = 0.5f;
    int voiceLimit = 16;   // 0 mono legato, 1 mono retrigger
    int transpose = 0;
    float chorusSend = 0, reverbSend = 0;
    bool ifxOn[2] {};
    int ifxType[2] {};
    float ifxP[2][3] {};

    bool seqOn = false;
    int seqOrder = 0, seqRate = 3, seqLength = 16, seqSync = 0, seqRoot = 128;
    bool seqTriplet = false, seqLatch = false;
    float seqSwing = 0, seqVelAmt = 0.5f;
    std::array<SeqLine, 5> lines;

    // melody
    int keyLo = 0, keyHi = 127, velLo = 0, velHi = 127;
    float glideTime = 0; bool glideAuto = false;
    int pbRange = 2;
    int lfoWave = 0; float lfoRate = 1; bool lfoKeySync = false, lfoBpm = false; int lfoBpmIdx = 0;
    float lfoLevel = 0, lfoDelay = 0, lfoAttack = 0;
    int osc[2] {}; float oscSemis[2] {}, oscCents[2] {};
    float oscMix = -1, oscLfo = 0;
    bool slope24 = true;
    float cutoff = 1, reso = 0, filtMix = 0;
    float fA = 0, fD = 0, fS = 1, fR = 0, fAmt = 0, fVel = 0, fLfo = 0, fKbd = 0;
    float aA = 0, aD = 0, aS = 1, aR = 0, aVel = 0.8f;
    bool arpOn = false, arpKeySync = false, arpLatch = false;
    float arpGate = 0.75f; int arpPattern = 0, arpRate = 10, arpOct = 1;

    // drum
    std::array<DrumKey, 65> keys;   // notes 24..88

    static LayerParams decode (const std::vector<float>& p);
};

// ---------------------------------------------------------------------------
struct Env
{
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Idle;
    float level = 0, a = 0, d = 0, s = 1, r = 0, fs = 44100;
    float attInc = 1, decCoef = 0, relCoef = 0;
    void setup (float A, float D, float S, float R, float sampleRate)
    {
        a = A; d = D; s = S; r = R; fs = sampleRate;
        attInc = A <= 0.0005f ? 1.0f : 1.0f / (A * fs);
        decCoef = coef (D);
        relCoef = coef (R);
    }
    // exponential segment reaching -60 dB of its distance after t seconds
    float coef (float t) const { return t <= 0.0005f ? 0.0f : std::exp (-6.9f / (t * fs)); }
    void start (bool fromZero) { if (fromZero) level = 0; stage = Attack; }
    void release() { if (stage != Idle) stage = Release; }
    inline float tick()
    {
        switch (stage)
        {
            case Attack: level += attInc; if (level >= 1) { level = 1; stage = Decay; } break;
            case Decay: level = s + (level - s) * decCoef; if (std::abs (level - s) < 1e-4f) { level = s; stage = Sustain; } break;
            case Sustain: level = s; break;
            case Release: level *= relCoef; if (level < 1e-5f) { level = 0; stage = Idle; } break;
            default: break;
        }
        return level;
    }
};

struct Osc
{
    const Sample* smp = nullptr;
    double pos = 0;
    float rootOffset = 0;   // semitones: target - root
    double rateRatio = 1;   // sample rate / host rate
    float gain = 0;
    bool reverse = false, done = true, loop = false;
};

struct PartState;
struct LayerRuntime;

struct Voice
{
    bool active = false;
    int part = 0, layer = 0;
    int key = 0;            // MIDI key that owns the voice (for note-off), -1 for internal
    int note = 60;          // sounding note
    int source = 0;         // 0 direct, 1 arp, 2 seq line N (source-2)
    float vel = 1;
    bool drum = false, released = false, sustained = false, sostenuto = false;
    int drumIndex = 0;
    uint64_t age = 0;
    float pitch = 60, pitchTarget = 60, glideCoef = 0;
    Osc osc[2];
    Env amp, filt;
    Svf svf[2];
    double lfoPhase = 0; float lfoTime = 0; float rnd0 = 0, rnd1 = 0;
    float panL = 0.7071f, panR = 0.7071f;
    float gain = 1;
    float sendCh = 0, sendRv = 0;
    int bus = 0;            // 0 -> IFX chain from IFX1, 1 -> IFX2 only, 2 dry
    float fadeOut = 1; bool killing = false;
};

struct LayerRuntime
{
    std::vector<float> raw;         // normalized parameters (patch values + edits)
    LayerParams lp;
    InsertEffect ifx[2];
    std::vector<float> busA[2], busB[2], busD[2];
    double lfoPhase = 0; float lfoTime = 0; float rnd0 = 0, rnd1 = 0;
    int lastNote = -1;
    // arp / seq
    std::vector<int> held;          // held keys in press order
    std::vector<int> heldVel;
    std::vector<int> latched, latchedVel;
    double clockBeats = 0;          // internal clock when host is stopped
    int arpIndex = 0, arpNote = -1; double arpOffAt = -1, arpNextAt = 0; bool arpRunning = false;
    int seqStep = -1; bool seqRunning = false; double seqNextAt = 0;
    std::array<int, 5> seqNote { -1, -1, -1, -1, -1 };
    std::array<bool, 5> seqEndAfter {};
};

// One of the 16 mixer channels. Atomics are shared with the editor (mixer view).
struct PartState
{
    int patch = -1;
    std::vector<std::unique_ptr<LayerRuntime>> layers;
    std::atomic<float> volume { 100.0f / 127.0f }, panOffset { 0 };   // CC7 / CC10 and mixer faders
    std::atomic<float> reverbCC { -1 }, chorusCC { -1 };              // send levels, -1 = patch value
    std::atomic<bool> on { true }, link { false };
    std::atomic<int> bus { 0 };                                       // 0 main, 1 sub
    std::atomic<int> transpose { 0 };                                 // -48..48
    std::atomic<int> voiceLimit { -1 };                               // -1 patch, 0 legato, 1 retrigger, 2..64
    std::atomic<int> requestedPatch { -1 };                           // set by the editor, applied on the audio thread
    std::atomic<int> patchSerial { 0 }, shownPatch { -1 };
    std::atomic<float> meter { 0 };
    std::atomic<int> midiActivity { 0 }, lastKey { -1 };
    float expression = 1, modWheel = 0;
    float bend = 0;                 // -1..1
    bool sustain = false, sostenuto = false;
    int bankSelect = 0, bankLsb = 0;
    int rpnMsb = 127, rpnLsb = 127, pbRange = -1;
    float patchGain = 1;
    double patchTempo = 0;
};

// Global settings (Mixer global / Send-Return / Setup views), written by the processor every block.
struct GlobalSettings
{
    int masterTranspose = 0;
    float masterTuneHz = 440.0f;
    bool transposeToDrum = false, tuneToDrum = false;
    int tuningScale = 0;                            // 0 equal, 1..7 presets, 8..15 user scales 1..8
    std::array<std::array<float, 12>, 8> userScales {};
    int velocityCurve = 0;                          // 0 linear, 1 soft, 2 hard, 3 S-curve, 4 fixed
    bool ifxMaster = true;
    bool priority = false;
    int selectedPart = 0;
    bool hostSync = true, ignorePatchBpm = false;
    double bpm = 120.0;
    bool stereoMixdown = true;
    int masterVoiceLimit = 256;
    bool acceptProgramChange = true, acceptControllers = true;
    bool chorusOn = true, reverbOn = true;
    float chorusRate = 0.3f, chorusWidth = 0.5f, chorusSpread = 1.0f;
    float reverbPredelay = 0.05f, reverbTime = 0.7f, reverbTone = 0.55f;
};

const char* tuningScaleName (int index);
constexpr int kNumTuningScales = 16;

class Engine
{
public:
    void setLibrary (std::shared_ptr<const Library> lib) { library = std::move (lib); }
    const Library* getLibrary() const { return library.get(); }
    void prepare (double sampleRate, int maxBlock);
    void reset();

    // Assign a patch (global index into Library::patches) to a part.
    void setPartPatch (int part, int patchIndex);
    int getPartPatch (int part) const { return parts[(size_t) part].patch; }
    PartState& part (int index) { return parts[(size_t) index]; }
    const PartState& part (int index) const { return parts[(size_t) index]; }

    // Changes one normalized parameter of a layer of the part's current patch (audio thread).
    void applyEdit (int part, int layer, int index, float value);
    // All notes off, arpeggiators and sequencers stopped.
    void panic();
    // Restores the GM default layout (piano on every channel, standard kit on channel 10).
    void gmReset();

    // subL/subR may be null: the sub bus is then mixed into the main output.
    void process (juce::AudioBuffer<float>& buffer, float* subL, float* subR, juce::MidiBuffer& midi,
                  double bpm, double ppq, bool playing);

    bool multitimbral = false;     // false: all MIDI channels drive part 0
    float masterGain = 1.0f;
    float chorusLevel = 1.0f, reverbLevel = 1.0f;
    GlobalSettings g;
    // Optional note trace (tests): part, layer, note, on, source (0 key, 1 arp, 2+ seq lane), beat position.
    std::function<void (int, int, int, bool, int, double)> trace;
    std::atomic<float> mainMeter { 0 }, subMeter { 0 };
    std::atomic<double> currentBpm { 120.0 };

private:
    void handleMidi (const juce::MidiMessage& m);
    void handlePartMidi (int part, int channel, const juce::MidiMessage& m);
    int routeParts (int channel, std::array<int, 16>& out) const;
    float applyVelocityCurve (float v) const;
    float tuningOffset (int note, bool drum) const;
    int effectiveKey (int part, int key, bool drum) const;
    void configureLayerIfx (LayerRuntime& lr);
    void noteOn (int part, int key, float vel);
    void noteOff (int part, int key);
    void layerNoteOn (int part, int layer, int key, int note, float vel, int source);
    void layerNoteOff (int part, int layer, int key, int source, bool force = false);
    void startVoice (int part, int layer, int key, int note, float vel, int source);
    void startDrumVoice (int part, int layer, int inKey, int key, float vel);
    void allNotesOff (int part, bool hard);
    void runClocks (int part, int layer, double blockStartBeats, double blockBeats, int nSamples, bool hostSync);
    void renderVoice (Voice& v, int n);
    Voice* allocVoice();
    int heldNoteFor (LayerRuntime& lr, int index) const;

    std::shared_ptr<const Library> library;
    double fs = 44100;
    int maxBlock = 512;
    std::array<PartState, 16> parts;
    std::array<Voice, 256> voices;
    uint64_t voiceCounter = 0;
    SendEffects sfx;
    double bpmNow = 120, ppqNow = 0; bool hostPlaying = false;
    juce::Random rng;

    struct Pending { int part, layer, key, note; float vel; int source; bool on; int offset; };
    std::vector<Pending> pending;
};

} // namespace cure
