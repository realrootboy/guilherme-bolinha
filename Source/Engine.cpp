#include "Engine.h"
#include <algorithm>
#include <cmath>

namespace cure
{

namespace
{
    constexpr int kSub = 32;                    // control-rate sub-block size
    constexpr float kCutOct = 14.28634f;        // cutoff Hz = 2^(v * kCutOct) + 19
    constexpr int kDrumBase = 24;               // drum key slots cover notes 24..88

    inline float cube (float v) { return v * v * v; }
    inline int iround (float v) { return (int) std::lround (v); }
    inline float cutoffHz (float norm) { return std::pow (2.0f, juce::jlimit (0.0f, 1.0f, norm) * kCutOct) + 19.0f; }

    double arpRateBeats (int i)
    {
        static const double base[6] = { 4, 2, 1, 0.5, 0.25, 0.125 };
        // 18 rates (original table): 1/1D..1/32D, 1/1..1/32, 1/1T..1/32T
        i = juce::jlimit (0, 17, i);
        if (i < 6) return base[i] * 1.5;
        if (i < 12) return base[i - 6];
        return base[i - 12] * 2.0 / 3.0;
    }

    inline float hermite (const int16_t* d, int frames, double pos, bool loop, int loopStart)
    {
        const int i = (int) pos;
        const float t = (float) (pos - i);
        auto at = [&] (int k) -> float
        {
            if (k >= frames)
            {
                if (! loop) return 0.0f;
                k = loopStart + (k - frames) % juce::jmax (1, frames - loopStart);
            }
            if (k < 0) return 0.0f;
            return (float) d[k] * (1.0f / 32768.0f);
        };
        const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }
}

// ---------------------------------------------------------------------------
// Tuning scales (cents relative to equal temperament, C..B), as listed in the Setup view.
namespace
{
    const char* const kScaleNames[kNumTuningScales] = {
        "Equal Temperament", "Well Temperament Vallotti-Young", "Well Temperament Werckmeister III", "Meantone Scale",
        "Pure Temperament C Major", "Pure Temperament C Minor", "Arabic Scale", "Pythagorean Tuning",
        "User Tuning Scale 1", "User Tuning Scale 2", "User Tuning Scale 3", "User Tuning Scale 4",
        "User Tuning Scale 5", "User Tuning Scale 6", "User Tuning Scale 7", "User Tuning Scale 8" };
    const float kScales[8][12] = {
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, -5.9f, -3.9f, -2.0f, -7.8f, 2.0f, -7.8f, -2.0f, -3.9f, -5.9f, 0, -9.8f },
        { 0, -9.8f, -7.8f, -5.9f, -9.8f, -2.0f, -11.7f, -3.9f, -7.8f, -11.7f, -3.9f, -7.8f },
        { 0, -24.0f, -6.8f, 10.3f, -13.7f, 3.4f, -20.5f, -3.4f, -27.4f, -10.3f, 6.8f, -17.1f },
        { 0, 11.7f, 3.9f, 15.6f, -13.7f, -2.0f, -9.8f, 2.0f, 13.7f, -15.6f, 17.6f, -11.7f },
        { 0, 11.7f, 3.9f, 15.6f, -13.7f, -2.0f, -9.8f, 2.0f, 13.7f, -15.6f, -3.9f, -11.7f },
        { 0, 0, 0, 0, -50.0f, 0, 0, 0, 0, 0, 0, -50.0f },
        { 0, 13.7f, 3.9f, -5.9f, 7.8f, -2.0f, 11.7f, 2.0f, 15.6f, 5.9f, -3.9f, 9.8f } };
}

const char* tuningScaleName (int index)
{
    return kScaleNames[juce::jlimit (0, kNumTuningScales - 1, index)];
}

// ---------------------------------------------------------------------------
LayerParams LayerParams::decode (const std::vector<float>& p)
{
    auto P = [&] (int i) { return i < (int) p.size() ? p[(size_t) i] : 0.0f; };
    LayerParams lp;
    lp.drum = P (0) >= 0.5f && p.size() > 727;
    lp.on = P (1) >= 0.5f;
    lp.gain = 2.0f * P (3) * P (3);
    lp.pan = P (4);
    lp.voiceLimit = iround (P (5) * 64);
    lp.transpose = iround ((P (6) - 0.5f) * 96);
    lp.chorusSend = P (8);
    lp.reverbSend = P (9);
    for (int k = 0; k < 2; ++k)
    {
        lp.ifxOn[k] = P (10 + k) >= 0.5f;
        lp.ifxType[k] = iround (P (12 + k) * 33);
        for (int j = 0; j < 3; ++j)
            lp.ifxP[k][j] = P (14 + 3 * k + j);
    }

    lp.seqOn = P (20) >= 0.5f;
    lp.seqOrder = iround (P (21) * 2);
    lp.seqRate = iround (P (22) * 7);
    lp.seqTriplet = P (23) >= 0.5f;
    lp.seqSwing = P (24);
    lp.seqLength = iround (P (25) * 63) + 1;
    lp.seqSync = iround (P (27) * 2);
    lp.seqLatch = P (28) >= 0.5f;
    lp.seqRoot = iround (P (29) * 128);
    lp.seqVelAmt = P (30);
    for (int l = 0; l < 5; ++l)
    {
        const int b = 31 + 130 * l;
        auto& line = lp.lines[(size_t) l];
        for (int s = 0; s < 64; ++s)
        {
            line.code[(size_t) s] = (uint8_t) juce::jlimit (0, 100, iround (P (b + 2 * s) * 100));
            line.vel[(size_t) s] = P (b + 2 * s + 1) * 0.99f + 0.01f;
        }
        line.on = P (b + 128) >= 0.5f;
        line.base = iround (P (b + 129) * 132);
    }

    constexpr int m = 681;
    if (! lp.drum)
    {
        lp.keyLo = iround (P (m) * 127);      lp.keyHi = iround (P (m + 1) * 127);
        lp.velLo = iround (P (m + 2) * 127);  lp.velHi = iround (P (m + 3) * 127);
        lp.glideTime = cube (P (m + 4)) * 5.0f;
        lp.glideAuto = P (m + 5) >= 0.5f;
        lp.pbRange = iround (P (m + 6) * 24);
        lp.lfoWave = juce::jlimit (0, 9, iround (P (m + 7) * 9));          // 10 waves
        lp.lfoRate = cube (P (m + 8)) * 49.99f + 0.01f;
        lp.lfoBpmIdx = juce::jlimit (0, 26, iround (P (m + 8) * 26));     // 8/1 .. 1/32T
        lp.lfoKeySync = P (m + 9) >= 0.5f;
        lp.lfoBpm = P (m + 10) >= 0.5f;
        lp.lfoLevel = P (m + 11);
        lp.lfoDelay = cube (P (m + 12)) * 10.0f;
        lp.lfoAttack = cube (P (m + 13)) * 10.0f;
        for (int k = 0; k < 2; ++k)
        {
            lp.osc[k] = iround (P (m + 14 + k) * 16384);
            lp.oscSemis[k] = (float) iround ((P (m + 16 + k) - 0.5f) * 96);
            lp.oscCents[k] = P (m + 18 + k) * 200.0f - 100.0f;
        }
        lp.oscMix = P (m + 20) * 2.0f - 1.0f;
        lp.oscLfo = P (m + 21);
        lp.slope24 = P (m + 22) < 0.5f;
        lp.cutoff = P (m + 23);
        lp.reso = P (m + 24);
        lp.filtMix = P (m + 25);
        lp.fA = cube (P (m + 26)) * 10.0f;
        lp.fD = cube (P (m + 27)) * 29.99f + 0.01f;
        lp.fS = P (m + 28);
        lp.fR = cube (P (m + 29)) * 19.99f + 0.01f;
        lp.fAmt = P (m + 30) * 2.0f - 1.0f;
        lp.fVel = P (m + 31);
        lp.fLfo = P (m + 32);
        lp.fKbd = P (m + 33) * 2.0f - 1.0f;
        lp.aA = cube (P (m + 34)) * 10.0f;
        lp.aD = cube (P (m + 35)) * 29.99f + 0.01f;
        lp.aS = P (m + 36);
        lp.aR = cube (P (m + 37)) * 19.99f + 0.01f;
        lp.aVel = P (m + 38);
        lp.arpOn = P (m + 39) >= 0.5f;
        lp.arpKeySync = P (m + 40) >= 0.5f;
        lp.arpGate = P (m + 41);
        lp.arpLatch = P (m + 42) >= 0.5f;
        lp.arpPattern = juce::jlimit (0, 7, iround (P (m + 43) * 7));
        lp.arpRate = juce::jlimit (0, 17, iround (P (m + 44) * 17));
        lp.arpOct = 1 + juce::jlimit (0, 5, iround (P (m + 45) * 5));
    }
    else
    {
        for (int k = 0; k < 65; ++k)
        {
            const int b = m + 17 * k;
            auto& dk = lp.keys[(size_t) k];
            dk.wave = iround (P (b) * 16384);
            dk.reverse = P (b + 1) >= 0.5f;
            dk.semis = (float) iround ((P (b + 2) - 0.5f) * 96);
            dk.cents = P (b + 3) * 200.0f - 100.0f;
            dk.cutoff = P (b + 4);
            dk.reso = P (b + 5);
            dk.filtVel = P (b + 6);
            dk.gain = 2.0f * P (b + 7) * P (b + 7);
            dk.pan = P (b + 8);
            dk.attack = cube (P (b + 9)) * 10.0f;
            dk.decay = cube (P (b + 10)) * 19.99f + 0.01f;
            dk.hold = P (b + 11) >= 0.5f;
            dk.velAmt = P (b + 12);
            dk.muteGroup = iround (P (b + 13) * 2);
            dk.toIfx = iround (P (b + 14) * 2);
            dk.chorus = P (b + 15);
            dk.reverb = P (b + 16);
        }
        lp.keyLo = iround (P (m + 17 * 65) * 127);
        lp.keyHi = iround (P (m + 17 * 65 + 1) * 127);
        if (lp.keyHi < lp.keyLo) { lp.keyLo = 0; lp.keyHi = 127; }
    }
    return lp;
}

// ---------------------------------------------------------------------------
void Engine::prepare (double sampleRate, int block)
{
    fs = sampleRate;
    maxBlock = juce::jmax (block, kSub);
    sfx.prepare (fs, kSub);
    for (int p = 0; p < 16; ++p)
        if (parts[(size_t) p].patch >= 0)
            setPartPatch (p, parts[(size_t) p].patch);
    reset();
}

void Engine::reset()
{
    for (auto& v : voices) v.active = false;
    for (auto& part : parts)
        for (auto& lr : part.layers)
        {
            lr->held.clear(); lr->heldVel.clear(); lr->latched.clear(); lr->latchedVel.clear();
            lr->arpRunning = lr->seqRunning = false;
            for (auto& f : lr->ifx) f.reset();
        }
}

void Engine::setPartPatch (int partIndex, int patchIndex)
{
    if (library == nullptr || partIndex < 0 || partIndex >= 16)
        return;
    patchIndex = juce::jlimit (0, (int) library->patches.size() - 1, patchIndex);
    auto& part = parts[(size_t) partIndex];
    for (auto& v : voices)
        if (v.active && v.part == partIndex)
            v.active = false;
    part.patch = patchIndex;
    part.layers.clear();
    const auto& patch = library->patches[(size_t) patchIndex];
    part.patchGain = 2.0f * patch.glob[3] * patch.glob[3];
    if (part.patchGain <= 0.0f) part.patchGain = 1.0f;
    part.patchTempo = patch.glob[8];
    for (auto& layer : patch.layers)
    {
        auto lr = std::make_unique<LayerRuntime>();
        lr->raw = layer.p;
        lr->lp = LayerParams::decode (lr->raw);
        for (auto* b : { lr->busA, lr->busB, lr->busD })
            for (int c = 0; c < 2; ++c)
                b[c].assign ((size_t) kSub, 0.0f);
        for (auto& f : lr->ifx) f.prepare (fs);
        configureLayerIfx (*lr);
        lr->rnd0 = rng.nextFloat() * 2 - 1;
        lr->rnd1 = rng.nextFloat() * 2 - 1;
        part.layers.push_back (std::move (lr));
    }
    part.shownPatch = patchIndex;
    ++part.patchSerial;
}

void Engine::configureLayerIfx (LayerRuntime& lr)
{
    for (int k = 0; k < 2; ++k)
        if (lr.lp.ifxOn[k] && lr.lp.ifxType[k] > 0)
            lr.ifx[k].configure (lr.lp.ifxType[k], lr.lp.ifxP[k][0], lr.lp.ifxP[k][1], lr.lp.ifxP[k][2]);
}

void Engine::applyEdit (int partIndex, int layer, int index, float value)
{
    if (partIndex < 0 || partIndex >= 16) return;
    auto& part = parts[(size_t) partIndex];
    if (layer < 0 || layer >= (int) part.layers.size()) return;
    auto& lr = *part.layers[(size_t) layer];
    if (index < 0 || index >= (int) lr.raw.size()) return;
    lr.raw[(size_t) index] = juce::jlimit (0.0f, 1.0f, value);
    const bool wasSeq = lr.lp.seqOn, wasArp = lr.lp.arpOn;
    lr.lp = LayerParams::decode (lr.raw);
    configureLayerIfx (lr);
    if ((wasSeq && ! lr.lp.seqOn) || (wasArp && ! lr.lp.arpOn))
    {
        lr.seqRunning = lr.arpRunning = false;
        for (int l = 0; l < 5; ++l)
            if (lr.seqNote[(size_t) l] >= 0) { layerNoteOff (partIndex, layer, -1, 2 + l, true); lr.seqNote[(size_t) l] = -1; }
        if (lr.arpNote >= 0) { layerNoteOff (partIndex, layer, -1, 1, true); lr.arpNote = -1; }
    }
}

void Engine::panic()
{
    for (int p = 0; p < 16; ++p)
    {
        allNotesOff (p, true);
        auto& part = parts[(size_t) p];
        part.sustain = part.sostenuto = false;
    }
}

void Engine::gmReset()
{
    if (library == nullptr) return;
    int rhythm = 0;
    for (size_t b = 0; b < library->bankNames.size(); ++b)
        if (juce::String (library->bankNames[b]).containsIgnoreCase ("Rhythm"))
            rhythm = library->bankFirstPatch[b];
    for (int p = 0; p < 16; ++p)
    {
        auto& part = parts[(size_t) p];
        setPartPatch (p, p == 9 ? rhythm : 0);
        part.volume = 100.0f / 127.0f; part.panOffset = 0; part.expression = 1;
        part.reverbCC = -1; part.chorusCC = -1; part.transpose = 0; part.voiceLimit = -1;
        part.bus = 0; part.on = true; part.link = false; part.pbRange = -1;
        part.bankSelect = part.bankLsb = 0; part.modWheel = 0; part.bend = 0;
    }
}

float Engine::applyVelocityCurve (float v) const
{
    v = juce::jlimit (0.0f, 1.0f, v);
    switch (g.velocityCurve)
    {
        case 1: return std::sqrt (v);                                   // soft: louder at low velocity
        case 2: return v * v;                                           // hard
        case 3: return v * v * (3.0f - 2.0f * v);                       // S-curve
        case 4: return 100.0f / 127.0f;                                 // fixed
        default: return v;
    }
}

float Engine::tuningOffset (int note, bool drum) const
{
    float semis = 0;
    if (! drum || g.tuneToDrum)
        semis += 12.0f * std::log2 (juce::jlimit (415.3f, 466.2f, g.masterTuneHz) / 440.0f);
    if (! drum)
    {
        const int pc = ((note % 12) + 12) % 12;
        const int sc = juce::jlimit (0, kNumTuningScales - 1, g.tuningScale);
        semis += 0.01f * (sc < 8 ? kScales[sc][pc] : g.userScales[(size_t) (sc - 8)][(size_t) pc]);
    }
    return semis;
}

int Engine::effectiveKey (int partIndex, int key, bool drum) const
{
    int k = key + parts[(size_t) partIndex].transpose.load();
    if (! drum || g.transposeToDrum) k += g.masterTranspose;
    return k;
}

// ---------------------------------------------------------------------------
Voice* Engine::allocVoice()
{
    Voice* oldest = nullptr;
    Voice* freeVoice = nullptr;
    int active = 0;
    for (auto& v : voices)
    {
        if (! v.active) { if (freeVoice == nullptr) freeVoice = &v; continue; }
        if (! v.killing) ++active;
        if (oldest == nullptr || (v.released && ! oldest->released) || (v.released == oldest->released && v.age < oldest->age))
            oldest = &v;
    }
    if (freeVoice != nullptr && active < juce::jlimit (1, (int) voices.size(), g.masterVoiceLimit))
        return freeVoice;
    if (freeVoice != nullptr && oldest != nullptr) oldest->killing = true;   // global limit: steal, fade out
    return freeVoice != nullptr ? freeVoice : oldest;
}

int Engine::heldNoteFor (LayerRuntime& lr, int index) const
{
    const auto& src = lr.held.empty() ? lr.latched : lr.held;
    if (index < 0 || index >= (int) src.size())
        return -1;
    std::vector<int> order (src);
    if (lr.lp.seqOrder == 1) std::sort (order.begin(), order.end());
    else if (lr.lp.seqOrder == 2) std::sort (order.rbegin(), order.rend());
    return order[(size_t) index];
}

void Engine::noteOn (int partIndex, int inKey, float vel)
{
    auto& part = parts[(size_t) partIndex];
    if (! part.on) return;
    ++part.midiActivity;
    part.lastKey = inKey;
    vel = applyVelocityCurve (vel);
    const int v127 = juce::jlimit (1, 127, iround (vel * 127));
    for (int l = 0; l < (int) part.layers.size(); ++l)
    {
        auto& lr = *part.layers[(size_t) l];
        auto& lp = lr.lp;
        if (! lp.on || inKey < lp.keyLo || inKey > lp.keyHi) continue;
        if (! lp.drum && (v127 < lp.velLo || v127 > lp.velHi)) continue;
        const int key = effectiveKey (partIndex, inKey, lp.drum);
        if (key < 0 || key > 127) continue;

        if (lp.seqOn || (lp.arpOn && ! lp.drum))
        {
            if (lr.held.empty() && (lp.seqOn ? lp.seqLatch : lp.arpLatch))
            {
                lr.latched.clear(); lr.latchedVel.clear();
            }
            lr.held.push_back (key);
            lr.heldVel.push_back (v127);
            if (lp.seqLatch || lp.arpLatch) { lr.latched = lr.held; lr.latchedVel = lr.heldVel; }
            const double now = hostPlaying ? ppqNow : lr.clockBeats;
            if (lp.seqOn)
            {
                if (! lr.seqRunning || lp.seqSync == 2)
                {
                    lr.seqRunning = true;
                    lr.seqStep = -1;
                    lr.seqNextAt = now;
                }
            }
            else if (! lr.arpRunning || lp.arpKeySync)
            {
                const double step = arpRateBeats (lp.arpRate);
                // key sync: the pattern restarts on every new note; otherwise it starts
                // on the next step of the host's beat grid
                const bool wasRunning = lr.arpRunning;
                lr.arpRunning = true;
                lr.arpIndex = 0;
                if (lp.arpKeySync || ! hostPlaying)
                {
                    lr.arpNextAt = now;
                    if (wasRunning && lr.arpNote >= 0) { layerNoteOff (partIndex, l, -1, 1, true); lr.arpNote = -1; }
                }
                else
                    lr.arpNextAt = std::ceil (now / step - 1e-6) * step;
            }
            continue;
        }
        layerNoteOn (partIndex, l, inKey, key, vel, 0);
    }
}

void Engine::noteOff (int partIndex, int inKey)
{
    auto& part = parts[(size_t) partIndex];
    for (int l = 0; l < (int) part.layers.size(); ++l)
    {
        auto& lr = *part.layers[(size_t) l];
        auto& lp = lr.lp;
        if (lp.seqOn || (lp.arpOn && ! lp.drum))
        {
            const int key = effectiveKey (partIndex, inKey, lp.drum);
            for (size_t i = 0; i < lr.held.size(); ++i)
                if (lr.held[i] == key)
                {
                    lr.held.erase (lr.held.begin() + (long) i);
                    lr.heldVel.erase (lr.heldVel.begin() + (long) i);
                    break;
                }
            const bool latch = lp.seqOn ? lp.seqLatch : lp.arpLatch;
            if (lr.held.empty() && ! latch)
            {
                lr.seqRunning = lr.arpRunning = false;
                for (int s = 0; s < 5; ++s)
                    if (lr.seqNote[(size_t) s] >= 0) { layerNoteOff (partIndex, l, -1, 2 + s); lr.seqNote[(size_t) s] = -1; }
                if (lr.arpNote >= 0) { layerNoteOff (partIndex, l, -1, 1); lr.arpNote = -1; }
            }
            continue;
        }
        layerNoteOff (partIndex, l, inKey, 0);
    }
}

void Engine::layerNoteOn (int partIndex, int layer, int key, int note, float vel, int source)
{
    auto& lr = *parts[(size_t) partIndex].layers[(size_t) layer];
    if (trace) trace (partIndex, layer, note, true, source, hostPlaying ? ppqNow : lr.clockBeats);
    if (lr.lp.drum)
        startDrumVoice (partIndex, layer, key, note, vel);
    else
        startVoice (partIndex, layer, key, note, vel, source);
}

void Engine::layerNoteOff (int partIndex, int layer, int key, int source, bool force)
{
    auto& part = parts[(size_t) partIndex];
    if (trace && source > 0)
        trace (partIndex, layer, -1, false, source, hostPlaying ? ppqNow : part.layers[(size_t) layer]->clockBeats);
    for (auto& v : voices)
    {
        if (! v.active || v.part != partIndex || v.layer != layer || v.released || v.source != source) continue;
        if (source == 0 && v.key != key) continue;
        if (v.drum)
        {
            const auto& dk = part.layers[(size_t) layer]->lp.keys[(size_t) v.drumIndex];
            if (! dk.hold) continue;   // one-shot drums ignore note-off
        }
        if ((part.sustain || (part.sostenuto && v.sostenuto)) && ! force && source == 0) { v.sustained = true; continue; }
        v.released = true;
        v.amp.release();
        v.filt.release();
    }
}

void Engine::startVoice (int partIndex, int layer, int key, int note, float vel, int source)
{
    auto& part = parts[(size_t) partIndex];
    auto& lr = *part.layers[(size_t) layer];
    const auto& lp = lr.lp;

    // mono handling (mixer voice limit overrides the patch value)
    const int voiceLimit = part.voiceLimit >= 0 ? part.voiceLimit.load() : lp.voiceLimit;
    bool legato = false;
    if (voiceLimit <= 1)
    {
        for (auto& v : voices)
            if (v.active && v.part == partIndex && v.layer == layer && ! v.killing)
            {
                if (voiceLimit == 0 && ! v.released)
                {
                    legato = true;
                    v.key = key; v.note = note; v.source = source;
                    v.pitchTarget = (float) note;
                    if (lp.glideTime <= 0.0f) v.pitch = (float) note;
                    lr.lastNote = note;
                    return;
                }
                v.killing = true;
            }
    }
    else
    {
        int count = 0;
        Voice* oldest = nullptr;
        for (auto& v : voices)
            if (v.active && v.part == partIndex && v.layer == layer && ! v.killing)
            {
                ++count;
                if (oldest == nullptr || v.age < oldest->age) oldest = &v;
            }
        if (count >= voiceLimit && oldest != nullptr)
            oldest->killing = true;
    }

    Voice* v = allocVoice();
    *v = Voice();
    v->active = true;
    v->part = partIndex; v->layer = layer; v->key = key; v->note = note; v->source = source;
    v->vel = vel; v->drum = false; v->age = ++voiceCounter;
    v->pitchTarget = (float) note;
    const bool canGlide = lp.glideTime > 0.0f && lr.lastNote >= 0 && (! lp.glideAuto || legato);
    v->pitch = canGlide ? (float) lr.lastNote : (float) note;
    v->glideCoef = lp.glideTime > 0.0f ? std::exp (-6.9f / (lp.glideTime * (float) fs)) : 0.0f;
    lr.lastNote = note;

    const bool both = lp.osc[0] != 0 && lp.osc[1] != 0;
    const float tune = tuningOffset (note, false);
    for (int k = 0; k < 2; ++k)
    {
        auto& o = v->osc[k];
        o.done = true;
        if (lp.osc[k] == 0) continue;
        const Wavetable* wt = library->wavetable (lp.osc[k]);
        if (wt == nullptr) continue;
        const int zNote = juce::jlimit (0, 127, note + lp.transpose + (int) lp.oscSemis[k]);
        const Zone* z = wt->zoneFor (zNote);
        if (z == nullptr) continue;
        o.smp = library->sample (z->bank, z->sample);
        if (o.smp == nullptr || o.smp->frames < 2) continue;
        o.done = false;
        o.pos = 0;
        o.loop = o.smp->loopStart > 0 && wt->loopFlag;
        o.rootOffset = (float) lp.transpose + lp.oscSemis[k] + lp.oscCents[k] * 0.01f - (float) z->root + tune;
        o.rateRatio = (double) o.smp->rate / fs;
        if (both) o.gain = k == 0 ? (lp.oscMix <= 0 ? 1.0f : 1.0f - lp.oscMix) : (lp.oscMix >= 0 ? 1.0f : 1.0f + lp.oscMix);
        else o.gain = 1.0f;
    }
    if (v->osc[0].done && v->osc[1].done) { v->active = false; return; }

    v->amp.setup (lp.aA, lp.aD, lp.aS, lp.aR, (float) fs);
    v->filt.setup (lp.fA, lp.fD, lp.fS, lp.fR, (float) fs);
    v->amp.start (true);
    v->filt.start (true);
    v->gain = lp.gain * ((1.0f - lp.aVel) + lp.aVel * vel * vel);
    v->bus = 0;
    v->rnd0 = rng.nextFloat() * 2 - 1; v->rnd1 = rng.nextFloat() * 2 - 1;
}

void Engine::startDrumVoice (int partIndex, int layer, int inKey, int key, float vel)
{
    auto& part = parts[(size_t) partIndex];
    auto& lr = *part.layers[(size_t) layer];
    const auto& lp = lr.lp;
    const int idx = key + lp.transpose - kDrumBase;
    if (idx < 0 || idx >= 65) return;
    const auto& dk = lp.keys[(size_t) idx];
    if (dk.wave == 0) return;
    const Wavetable* wt = library->wavetable (dk.wave);
    if (wt == nullptr || wt->zones.empty()) return;
    const Zone* z = wt->zones.size() == 1 ? &wt->zones.front() : wt->zoneFor (key);
    const Sample* smp = library->sample (z->bank, z->sample);
    if (smp == nullptr || smp->frames < 2) return;

    // choke: same key retrigger, and mute groups
    for (auto& v : voices)
        if (v.active && v.part == partIndex && v.layer == layer && v.drum)
        {
            const auto& other = lp.keys[(size_t) v.drumIndex];
            if (v.drumIndex == idx || (dk.muteGroup != 0 && other.muteGroup == dk.muteGroup))
                v.killing = true;
        }

    Voice* v = allocVoice();
    *v = Voice();
    v->active = true;
    v->part = partIndex; v->layer = layer; v->key = inKey; v->note = key; v->source = 0;
    v->vel = vel; v->drum = true; v->drumIndex = idx; v->age = ++voiceCounter;
    auto& o = v->osc[0];
    o.smp = smp;
    o.done = false;
    o.reverse = dk.reverse;
    o.loop = ! dk.reverse && smp->loopStart > 0 && wt->loopFlag && dk.hold;
    o.pos = dk.reverse ? smp->frames - 1 : 0;
    o.rootOffset = dk.semis + dk.cents * 0.01f + tuningOffset (key, true);
    o.rateRatio = (double) smp->rate / fs;
    o.gain = 1.0f;
    v->osc[1].done = true;

    if (dk.hold) v->amp.setup (dk.attack, 0.0f, 1.0f, dk.decay, (float) fs);
    else v->amp.setup (dk.attack, dk.decay, 0.0f, dk.decay, (float) fs);
    v->amp.start (true);
    v->gain = dk.gain * lp.gain * ((1.0f - dk.velAmt) + dk.velAmt * vel * vel);
    const float pan = juce::jlimit (0.0f, 1.0f, dk.pan + (lp.pan - 0.5f));
    v->panL = std::cos (pan * juce::MathConstants<float>::halfPi);
    v->panR = std::sin (pan * juce::MathConstants<float>::halfPi);
    v->bus = dk.toIfx == 1 ? 0 : dk.toIfx == 2 ? 1 : 2;
    const float chSend = part.chorusCC >= 0 ? part.chorusCC.load() : lp.chorusSend;
    const float rvSend = part.reverbCC >= 0 ? part.reverbCC.load() : lp.reverbSend;
    v->sendCh = juce::jmin (1.0f, dk.chorus * chSend * 2.0f);
    v->sendRv = juce::jmin (1.0f, dk.reverb * rvSend * 2.0f);
    // drum filter: cutoff opens with velocity by filtVel
    const float norm = dk.cutoff - dk.filtVel * (1.0f - vel) * 0.5f;
    v->filt.level = norm;   // reused as static cutoff storage for drums
    v->svf[0].set (cutoffHz (norm), (float) fs, dk.reso);
}

void Engine::allNotesOff (int partIndex, bool hard)
{
    auto& part = parts[(size_t) partIndex];
    for (auto& lr : part.layers)
    {
        lr->held.clear(); lr->heldVel.clear(); lr->latched.clear(); lr->latchedVel.clear();
        lr->seqRunning = lr->arpRunning = false;
        lr->seqNote.fill (-1); lr->arpNote = -1;
    }
    for (auto& v : voices)
        if (v.active && v.part == partIndex)
        {
            if (hard) v.killing = true;
            else { v.released = true; v.amp.release(); v.filt.release(); }
        }
}

// ---------------------------------------------------------------------------
int Engine::routeParts (int channel, std::array<int, 16>& out) const
{
    // priority mode: everything goes to the selected channel; otherwise to the MIDI channel's part
    // (or part 0 when GM multitimbral is off). Linked channels respond together.
    const int base = g.priority ? juce::jlimit (0, 15, g.selectedPart) : (multitimbral ? channel - 1 : 0);
    int n = 0;
    out[(size_t) n++] = base;
    if (parts[(size_t) base].link)
        for (int p = 0; p < 16; ++p)
            if (p != base && parts[(size_t) p].link) out[(size_t) n++] = p;
    return n;
}

void Engine::handleMidi (const juce::MidiMessage& m)
{
    const int ch = juce::jlimit (1, 16, m.getChannel());
    std::array<int, 16> targets {};
    const int n = routeParts (ch, targets);
    for (int i = 0; i < n; ++i)
        handlePartMidi (targets[(size_t) i], ch, m);
}

void Engine::handlePartMidi (int partIndex, int ch, const juce::MidiMessage& m)
{
    auto& part = parts[(size_t) partIndex];

    if (m.isNoteOn())
        noteOn (partIndex, m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        noteOff (partIndex, m.getNoteNumber());
    else if (m.isPitchWheel())
    {
        if (g.acceptControllers) part.bend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
    }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const int raw = m.getControllerValue();
        const float val = (float) raw / 127.0f;
        if (cc == 120 || cc == 123 || cc == 121 || cc == 0 || cc == 32 || g.acceptControllers)
        switch (cc)
        {
            case 0: part.bankSelect = raw; break;
            case 32: part.bankLsb = raw; break;
            case 1: part.modWheel = val; break;
            case 6:
                if (part.rpnMsb == 0 && part.rpnLsb == 0) part.pbRange = juce::jlimit (0, 24, raw);   // RPN 0: bend range
                break;
            case 7: part.volume = val; break;
            case 10: part.panOffset = val - 0.5f; break;
            case 11: part.expression = val; break;
            case 64:
                part.sustain = val >= 0.5f;
                if (! part.sustain)
                    for (auto& v : voices)
                        if (v.active && v.part == partIndex && v.sustained && ! v.released && ! (part.sostenuto && v.sostenuto))
                        { v.released = true; v.amp.release(); v.filt.release(); }
                break;
            case 66:
                part.sostenuto = val >= 0.5f;
                for (auto& v : voices)
                {
                    if (! v.active || v.part != partIndex) continue;
                    if (part.sostenuto) v.sostenuto = ! v.released && ! v.drum;
                    else if (v.sostenuto)
                    {
                        v.sostenuto = false;
                        if (v.sustained && ! part.sustain && ! v.released) { v.released = true; v.amp.release(); v.filt.release(); }
                    }
                }
                break;
            case 91: part.reverbCC = val; break;
            case 93: part.chorusCC = val; break;
            case 98: case 99: part.rpnMsb = part.rpnLsb = 127; break;   // NRPN deselects RPN
            case 100: part.rpnLsb = raw; break;
            case 101: part.rpnMsb = raw; break;
            case 120: allNotesOff (partIndex, true); break;
            case 121: part.modWheel = 0; part.bend = 0; part.expression = 1; part.sustain = false; part.sostenuto = false;
                      part.rpnMsb = part.rpnLsb = 127; break;
            case 123: allNotesOff (partIndex, false); break;
            default: break;
        }
    }
    else if (m.isProgramChange() && library != nullptr && g.acceptProgramChange)
    {
        const int prog = m.getProgramChangeNumber();
        const int nBanks = (int) library->bankNames.size();
        auto bankPrefix = [&] (int b) { return juce::String (library->bankNames[(size_t) b]).getIntValue(); };
        auto isRhythm = [&] (int b) { return juce::String (library->bankNames[(size_t) b]).containsIgnoreCase ("Rhythm"); };
        int bank = 0;
        if (part.bankSelect == 127)
        {
            // MSB 127 + LSB = category number (e.g. 127 052 005 = 052 Bell & Perc, patch 5)
            for (int b = 0; b < nBanks; ++b) if (bankPrefix (b) == part.bankLsb) bank = b;
        }
        else
            bank = juce::jlimit (0, nBanks - 1, part.bankSelect);
        // GM drum channel: bank 0 on channel 10 selects GM Rhythm kits
        if (multitimbral && ch == 10 && part.bankSelect == 0)
            for (int b = 0; b < nBanks; ++b)
                if (isRhythm (b)) bank = b;
        const int first = library->bankFirstPatch[(size_t) bank];
        const int next = bank + 1 < nBanks ? library->bankFirstPatch[(size_t) bank + 1] : (int) library->patches.size();
        int index = prog;
        if (isRhythm (bank))
        {
            static const int kitProg[9] = { 0, 8, 16, 24, 25, 32, 40, 48, 56 };
            index = 0;
            for (int k = 0; k < 9; ++k) if (prog >= kitProg[k]) index = k;
        }
        setPartPatch (partIndex, first + juce::jlimit (0, juce::jmax (0, next - first - 1), index));
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        allNotesOff (partIndex, m.isAllSoundOff());
}

// ---------------------------------------------------------------------------
void Engine::runClocks (int partIndex, int layer, double start, double len, int /*nSamples*/, bool /*hostSync*/)
{
    auto& lr = *parts[(size_t) partIndex].layers[(size_t) layer];
    const auto& lp = lr.lp;
    const double end = start + len;

    if (lp.seqOn && lr.seqRunning)
    {
        const double stepBeats = 4.0 / std::pow (2.0, lp.seqRate) * (lp.seqTriplet ? 2.0 / 3.0 : 1.0);
        // never let the clock fall far behind (e.g. after transport jumps)
        if (lr.seqNextAt < start - stepBeats || lr.seqNextAt > end + 64 * stepBeats) lr.seqNextAt = start;
        while (lr.seqNextAt < end)
        {
            int step;
            if (lp.seqSync == 0 && hostPlaying)
                step = (int) std::floor (lr.seqNextAt / stepBeats + 1e-6) % lp.seqLength;
            else
                step = (lr.seqStep + 1) % lp.seqLength;
            lr.seqStep = step;
            const int firstHeld = heldNoteFor (lr, 0);
            const auto& velSrc = lr.held.empty() ? lr.latchedVel : lr.heldVel;
            const float playedVel = velSrc.empty() ? 1.0f : (float) velSrc.front() / 127.0f;
            for (int l = 0; l < 5; ++l)
            {
                const auto& line = lp.lines[(size_t) l];
                if (! line.on) continue;
                const int code = line.code[(size_t) step];
                int& cur = lr.seqNote[(size_t) l];
                if (code == 97 || code == 98)
                {
                    if (lr.seqEndAfter[(size_t) l]) { if (cur >= 0) layerNoteOff (partIndex, layer, -1, 2 + l, true); cur = -1; }
                    lr.seqEndAfter[(size_t) l] = code == 98;
                    continue;
                }
                if (cur >= 0) { layerNoteOff (partIndex, layer, -1, 2 + l, true); cur = -1; }
                lr.seqEndAfter[(size_t) l] = false;
                if (code > 96) continue;
                int base = line.base;
                int note;
                if (base >= 128)
                {
                    const int h = heldNoteFor (lr, base - 128);
                    if (h < 0) continue;
                    note = h + code - 48;
                }
                else
                {
                    note = base + code - 48;
                    if (lp.seqRoot < 128 && firstHeld >= 0) note += firstHeld - lp.seqRoot;
                }
                if (note < 0 || note > 127) continue;
                const float vel = juce::jlimit (0.01f, 1.0f, line.vel[(size_t) step] * ((1.0f - lp.seqVelAmt) + lp.seqVelAmt * playedVel));
                layerNoteOn (partIndex, layer, -1, note, vel, 2 + l);
                cur = note;
            }
            double next = stepBeats;
            if (lp.seqSwing > 0 && (step % 2) == 0) next += stepBeats * lp.seqSwing * 0.5;
            else if (lp.seqSwing > 0) next -= stepBeats * lp.seqSwing * 0.5;
            lr.seqNextAt += next;
        }
    }

    if (lp.arpOn && ! lp.drum && lr.arpRunning)
    {
        const double stepBeats = arpRateBeats (lp.arpRate);
        if (lr.arpNextAt < start - stepBeats || lr.arpNextAt > end + 64 * stepBeats) lr.arpNextAt = start;
        if (lr.arpNote >= 0 && lr.arpOffAt >= 0 && lr.arpOffAt < end)
        {
            layerNoteOff (partIndex, layer, -1, 1, true);
            lr.arpNote = -1; lr.arpOffAt = -1;
        }
        while (lr.arpNextAt < end)
        {
            const auto& src = lr.held.empty() ? lr.latched : lr.held;
            const auto& srcVel = lr.held.empty() ? lr.latchedVel : lr.heldVel;
            if (src.empty()) { lr.arpRunning = false; break; }
            std::vector<int> notes;
            std::vector<int> base (src);
            if (lp.arpPattern != 6) std::sort (base.begin(), base.end());
            std::vector<int> up;
            for (int o = 0; o < lp.arpOct; ++o)
                for (int n : base) up.push_back (n + 12 * o);
            std::vector<int> down (up.rbegin(), up.rend());
            switch (lp.arpPattern)
            {
                case 0: case 6: notes = up; break;
                case 1: notes = down; break;
                case 2: notes = up; if (down.size() > 2) notes.insert (notes.end(), down.begin() + 1, down.end() - 1); break;
                case 3: notes = up; notes.insert (notes.end(), down.begin(), down.end()); break;
                case 4: notes = down; if (up.size() > 2) notes.insert (notes.end(), up.begin() + 1, up.end() - 1); break;
                case 5: notes = down; notes.insert (notes.end(), up.begin(), up.end()); break;
                default: notes = up; break;
            }
            int note = lp.arpPattern == 7 ? up[(size_t) rng.nextInt ((int) up.size())]
                                          : notes[(size_t) (lr.arpIndex % (int) notes.size())];
            ++lr.arpIndex;
            if (lr.arpNote >= 0) layerNoteOff (partIndex, layer, -1, 1, true);
            const float vel = srcVel.empty() ? 0.8f : (float) srcVel.back() / 127.0f;
            if (note >= 0 && note <= 127)
            {
                layerNoteOn (partIndex, layer, -1, note, vel, 1);
                lr.arpNote = note;
                lr.arpOffAt = lr.arpNextAt + stepBeats * juce::jlimit (0.05f, 1.0f, lp.arpGate);
            }
            lr.arpNextAt += stepBeats;
        }
    }
}

// ---------------------------------------------------------------------------
void Engine::renderVoice (Voice& v, int n)
{
    auto& part = parts[(size_t) v.part];
    auto& lr = *part.layers[(size_t) v.layer];
    const auto& lp = lr.lp;
    const float sr = (float) fs;

    float* outL; float* outR;
    if (v.bus == 0) { outL = lr.busA[0].data(); outR = lr.busA[1].data(); }
    else if (v.bus == 1) { outL = lr.busB[0].data(); outR = lr.busB[1].data(); }
    else { outL = lr.busD[0].data(); outR = lr.busD[1].data(); }

    const float partGain = part.volume * part.volume * part.expression * part.expression * (127.0f * 127.0f / (100.0f * 100.0f));
    float pitchMod = 0, cutNorm = 1, reso = 0;
    bool useFilter = false;

    if (! v.drum)
    {
        // LFO
        double* phase = lp.lfoKeySync ? &v.lfoPhase : &lr.lfoPhase;
        float* ltime = lp.lfoKeySync ? &v.lfoTime : &lr.lfoTime;
        const double rate = lp.lfoBpm ? bpmNow / 60.0 / noteBeatsDesc (lp.lfoBpmIdx) : lp.lfoRate;
        float fade = 1.0f;
        if (*ltime < lp.lfoDelay) fade = 0.0f;
        else if (lp.lfoAttack > 0) fade = juce::jmin (1.0f, (*ltime - lp.lfoDelay) / lp.lfoAttack);
        const float r0 = lp.lfoKeySync ? v.rnd0 : lr.rnd0, r1 = lp.lfoKeySync ? v.rnd1 : lr.rnd1;
        const float lfo = lfoShape (lp.lfoWave, *phase, r0, r1) * fade * juce::jmin (1.0f, lp.lfoLevel + part.modWheel);
        if (lp.lfoKeySync)   // free-running layer LFO is advanced once per sub-block in process()
        {
            const double before = v.lfoPhase;
            v.lfoPhase += rate * n / fs; v.lfoTime += (float) n / sr;
            if (std::floor (v.lfoPhase) != std::floor (before)) { v.rnd0 = v.rnd1; v.rnd1 = rng.nextFloat() * 2 - 1; }
        }

        // glide
        if (v.glideCoef > 0) v.pitch = v.pitchTarget + (v.pitch - v.pitchTarget) * std::pow (v.glideCoef, (float) n);
        else v.pitch = v.pitchTarget;
        pitchMod = v.pitch + part.bend * (float) (part.pbRange >= 0 ? part.pbRange : lp.pbRange) + lfo * lp.oscLfo * 32.0f;

        // filter
        float fenv = v.filt.level;
        for (int i = 0; i < n; ++i) fenv = v.filt.tick();
        const float velScale = (1.0f - lp.fVel) + lp.fVel * v.vel;
        cutNorm = lp.cutoff + lp.fAmt * fenv * velScale + lp.fKbd * (v.pitch - 60.0f) / 12.0f / kCutOct + lp.fLfo * lfo * 0.5f;
        reso = lp.reso;
        useFilter = cutNorm < 0.995f || lp.filtMix > 0.001f || lp.reso > 0.05f;
        if (useFilter)
        {
            const float hz = cutoffHz (cutNorm);
            v.svf[0].set (hz, sr, reso);
            v.svf[1].set (hz, sr, lp.slope24 ? reso * 0.5f : reso);
        }
        const float pan = juce::jlimit (0.0f, 1.0f, lp.pan + part.panOffset);
        v.panL = std::cos (pan * juce::MathConstants<float>::halfPi);
        v.panR = std::sin (pan * juce::MathConstants<float>::halfPi);
    }
    else
    {
        const auto& dk = lp.keys[(size_t) v.drumIndex];
        cutNorm = v.filt.level;
        useFilter = cutNorm < 0.995f || dk.reso > 0.05f;
    }

    double inc[2] {};
    for (int k = 0; k < 2; ++k)
    {
        auto& o = v.osc[k];
        if (o.done) continue;
        const float semis = v.drum ? o.rootOffset : pitchMod + o.rootOffset;
        inc[k] = std::pow (2.0, semis / 12.0) * o.rateRatio;
    }

    const float baseGain = v.gain * partGain;
    // drum keys feed the send buses directly (scaled like the layer output)
    const float drumSend = (part.on && part.bus == 0) ? part.patchGain * masterGain * 0.5f : 0.0f;
    const float fadeStep = 1.0f / (0.004f * sr);
    const float mixT = lp.filtMix;
    float* chL = sfx.chorusL(); float* chR = sfx.chorusR();
    float* rvL = sfx.reverbL(); float* rvR = sfx.reverbR();

    for (int i = 0; i < n; ++i)
    {
        float x = 0;
        bool alive = false;
        for (int k = 0; k < 2; ++k)
        {
            auto& o = v.osc[k];
            if (o.done) continue;
            const Sample& s = *o.smp;
            x += o.gain * hermite (s.data, s.frames, o.pos, o.loop, s.loopStart);
            if (o.reverse)
            {
                o.pos -= inc[k];
                if (o.pos < 0) o.done = true;
            }
            else
            {
                o.pos += inc[k];
                if (o.pos >= s.frames)
                {
                    if (o.loop) o.pos = s.loopStart + std::fmod (o.pos - s.frames, (double) juce::jmax (1, s.frames - s.loopStart));
                    else o.done = true;
                }
            }
            alive = true;
        }
        if (useFilter)
        {
            float bp, hp;
            float lpo = v.svf[0].tick (x, bp, hp);
            if (! v.drum)
            {
                // Filter.Mix crossfades low-pass (0) -> high-pass (1)
                float y = lpo + (hp - lpo) * mixT;
                if (lp.slope24)
                {
                    float bp2, hp2;
                    const float lp2 = v.svf[1].tick (y, bp2, hp2);
                    y = lp2 + (hp2 - lp2) * mixT;
                }
                x = y;
            }
            else x = lpo;
        }
        const float env = v.amp.tick();
        if (v.killing) { v.fadeOut -= fadeStep; if (v.fadeOut <= 0) { v.active = false; return; } }
        const float out = x * env * baseGain * v.fadeOut;
        outL[i] += out * v.panL;
        outR[i] += out * v.panR;
        if (v.drum)
        {
            const float o = out * drumSend;
            chL[i] += o * v.panL * v.sendCh; chR[i] += o * v.panR * v.sendCh;
            rvL[i] += o * v.panL * v.sendRv; rvR[i] += o * v.panR * v.sendRv;
        }
        if (! alive || v.amp.stage == Env::Idle) { v.active = false; return; }
    }
}

// ---------------------------------------------------------------------------
void Engine::process (juce::AudioBuffer<float>& buffer, float* subL, float* subR, juce::MidiBuffer& midi,
                      double bpm, double ppq, bool playing)
{
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    if (subL != nullptr) juce::FloatVectorOperations::clear (subL, numSamples);
    if (subR != nullptr) juce::FloatVectorOperations::clear (subR, numSamples);
    if (library == nullptr) return;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const bool separateSub = subL != nullptr && subR != nullptr && ! g.stereoMixdown;

    // patch changes requested by the editor
    for (int p = 0; p < 16; ++p)
    {
        const int req = parts[(size_t) p].requestedPatch.exchange (-1);
        if (req >= 0) setPartPatch (p, req);
    }

    // tempo: host (SYNC on) > patch BPM (unless ignored) > master BPM
    if (! g.hostSync) playing = false;
    if (g.hostSync && bpm > 0) bpmNow = bpm;
    else if (! g.ignorePatchBpm && parts[0].patchTempo > 0) bpmNow = parts[0].patchTempo;
    else bpmNow = juce::jlimit (20.0, 300.0, g.bpm);
    currentBpm = bpmNow;
    hostPlaying = playing;
    const double beatsPerSample = bpmNow / 60.0 / fs;

    sfx.setChorus (g.chorusRate, g.chorusWidth, g.chorusSpread);
    sfx.setReverb (g.reverbPredelay, g.reverbTime, g.reverbTone);
    const float chorusReturn = g.chorusOn ? chorusLevel : 0.0f;
    const float reverbReturn = g.reverbOn ? reverbLevel : 0.0f;
    float partPeak[16] {};
    float mainPeak = 0, subPeak = 0;

    auto it = midi.begin();
    for (int pos = 0; pos < numSamples; pos += kSub)
    {
        const int n = juce::jmin (kSub, numSamples - pos);
        while (it != midi.end() && (*it).samplePosition < pos + n)
        {
            handleMidi ((*it).getMessage());
            ++it;
        }
        ppqNow = ppq + pos * beatsPerSample;

        // clocks + clear buses
        for (int p = 0; p < 16; ++p)
            for (int l = 0; l < (int) parts[(size_t) p].layers.size(); ++l)
            {
                auto& lr = *parts[(size_t) p].layers[(size_t) l];
                const double start = playing ? ppqNow : lr.clockBeats;
                runClocks (p, l, start, n * beatsPerSample, n, playing);
                lr.clockBeats += n * beatsPerSample;
                if (! lr.lp.lfoKeySync && ! lr.lp.drum)
                {
                    const double rate = lr.lp.lfoBpm ? bpmNow / 60.0 / noteBeatsDesc (lr.lp.lfoBpmIdx) : lr.lp.lfoRate;
                    const double before = lr.lfoPhase;
                    lr.lfoPhase += rate * n / fs;
                    lr.lfoTime += (float) (n / fs);
                    if (std::floor (lr.lfoPhase) != std::floor (before)) { lr.rnd0 = lr.rnd1; lr.rnd1 = rng.nextFloat() * 2 - 1; }
                }
                for (auto* b : { lr.busA, lr.busB, lr.busD })
                    for (int c = 0; c < 2; ++c) std::fill (b[c].begin(), b[c].begin() + n, 0.0f);
            }
        sfx.clear (n);

        for (auto& v : voices)
            if (v.active) renderVoice (v, n);

        // layer effects and mixing
        for (int p = 0; p < 16; ++p)
        {
            auto& part = parts[(size_t) p];
            const bool toSub = part.bus == 1;
            float* oL = toSub && separateSub ? subL + pos : L + pos;
            float* oR = toSub && separateSub ? subR + pos : (R != nullptr ? R + pos : nullptr);
            const float mute = part.on ? 1.0f : 0.0f;
            for (auto& lrp : part.layers)
            {
                auto& lr = *lrp;
                auto& lp = lr.lp;
                float* aL = lr.busA[0].data(); float* aR = lr.busA[1].data();
                if (g.ifxMaster && lp.ifxOn[0] && lp.ifxType[0] > 0) lr.ifx[0].process (aL, aR, n, bpmNow);
                for (int i = 0; i < n; ++i) { aL[i] += lr.busB[0][(size_t) i]; aR[i] += lr.busB[1][(size_t) i]; }
                if (g.ifxMaster && lp.ifxOn[1] && lp.ifxType[1] > 0) lr.ifx[1].process (aL, aR, n, bpmNow);
                const float gain = part.patchGain * masterGain * 0.5f * mute;   // -6 dB internal headroom
                // the sub bus has no send/return routing
                const float sendScale = toSub ? 0.0f : 1.0f;
                const float chCC = part.chorusCC, rvCC = part.reverbCC;
                const float ch = sendScale * (lp.drum ? 0.0f : (chCC >= 0 ? chCC : lp.chorusSend));   // drums send per key
                const float rv = sendScale * (lp.drum ? 0.0f : (rvCC >= 0 ? rvCC : lp.reverbSend));
                float* cL = sfx.chorusL(); float* cR = sfx.chorusR();
                float* rL = sfx.reverbL(); float* rR = sfx.reverbR();
                for (int i = 0; i < n; ++i)
                {
                    const float l = (aL[i] + lr.busD[0][(size_t) i]) * gain;
                    const float r = (aR[i] + lr.busD[1][(size_t) i]) * gain;
                    oL[i] += l;
                    if (oR) oR[i] += r;
                    cL[i] += l * ch; cR[i] += r * ch;
                    rL[i] += l * rv; rR[i] += r * rv;
                    partPeak[p] = juce::jmax (partPeak[p], std::abs (l), std::abs (r));
                }
            }
        }
        if (R) sfx.process (L + pos, R + pos, n, chorusReturn, reverbReturn);
    }
    for (; it != midi.end(); ++it) handleMidi ((*it).getMessage());
    if (R == nullptr) return;
    // safety soft-clip: transparent below 0.8, saturates smoothly towards 1.0
    auto clip = [] (float x)
    {
        const float a = std::abs (x);
        return a < 0.8f ? x : std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), x);
    };
    for (int i = 0; i < numSamples; ++i)
    {
        L[i] = clip (L[i]);
        R[i] = clip (R[i]);
        mainPeak = juce::jmax (mainPeak, std::abs (L[i]), std::abs (R[i]));
    }
    if (separateSub)
        for (int i = 0; i < numSamples; ++i)
        {
            subL[i] = clip (subL[i]);
            subR[i] = clip (subR[i]);
            subPeak = juce::jmax (subPeak, std::abs (subL[i]), std::abs (subR[i]));
        }
    auto hold = [] (std::atomic<float>& m, float v) { if (v > m.load()) m = v; };
    for (int p = 0; p < 16; ++p) hold (parts[(size_t) p].meter, partPeak[p]);
    hold (mainMeter, mainPeak);
    hold (subMeter, subPeak);
}

} // namespace cure
