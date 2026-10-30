// Insert (IFX) and send (SFX) effects for the Cure engine.
// Parameter layouts follow the original effect classes (CIFX*): 3 normalized params each.
#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

namespace cure
{

enum IfxType
{
    IFX_OFF = 0, IFX_SFILTER12 = 2, IFX_SFILTER24 = 3, IFX_EQ3 = 4, IFX_PEAKCOMP = 6, IFX_RMSCOMP = 7,
    IFX_OVERDRIVE = 9, IFX_DISTORTION = 10, IFX_AMPDIST = 11, IFX_CRUSHER = 13, IFX_LPNOISE = 14,
    IFX_STEREOIMAGE = 16, IFX_AUTOWAH = 17, IFX_CHORUS = 19, IFX_FLANGER = 20, IFX_PHASER = 21,
    IFX_TREMOLO = 23, IFX_AUTOPAN = 24, IFX_DELAY = 26, IFX_STEREODELAY = 27, IFX_PINGPONG = 28,
    IFX_GATEREVERB = 30, IFX_ROOMREVERB = 31, IFX_HALLREVERB = 32, IFX_SPRINGREVERB = 33
};

// Tempo-synced note lengths in beats, longest first: 8/1, 4/1D, 8/1T, 4/1, ... (27 entries)
inline double noteBeatsDesc (int i)
{
    static const double f[3] = { 1.0, 0.75, 2.0 / 3.0 };
    i = juce::jlimit (0, 26, i);
    return 32.0 / std::pow (2.0, i / 3) * f[i % 3];
}
inline double noteBeatsAsc (int i) { return noteBeatsDesc (26 - juce::jlimit (0, 26, i)); }

// Shared LFO wave shapes (same order as Cure's LFO.Wave list)
inline float lfoShape (int wave, double phase, float rnd0, float rnd1)
{
    const double ph = phase - std::floor (phase);
    switch (wave)
    {
        case 0: return (float) std::cos (juce::MathConstants<double>::twoPi * ph);          // SIN hi-lo
        case 1: return (float) -std::cos (juce::MathConstants<double>::twoPi * ph);         // SIN lo-hi
        case 2: return (float) (ph < 0.5 ? 1.0 - 4.0 * ph : -3.0 + 4.0 * ph);                  // TRI hi-lo
        case 3: return (float) (ph < 0.5 ? -1.0 + 4.0 * ph : 3.0 - 4.0 * ph);                  // TRI lo-hi
        case 4: return (float) (2.0 * ph - 1.0);                                              // SAW up
        case 5: return (float) (1.0 - 2.0 * ph);                                              // SAW down
        case 6: return ph < 0.5 ? 1.0f : -1.0f;                                               // SQR hi-lo
        case 7: return ph < 0.5 ? -1.0f : 1.0f;                                               // SQR lo-hi
        case 8:                                                                               // RND glide
        {
            const float t = (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * ph));
            return rnd0 + (rnd1 - rnd0) * t;
        }
        default: return rnd1;                                                                 // RND hold
    }
}

struct Svf
{
    float ic1 = 0, ic2 = 0, g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
    void set (float fc, float fs, float res)
    {
        fc = juce::jlimit (16.0f, fs * 0.45f, fc);
        g = std::tan (juce::MathConstants<float>::pi * fc / fs);
        k = 2.0f - 1.96f * juce::jlimit (0.0f, 1.0f, res);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }
    // returns lp, writes bp/hp
    inline float tick (float x, float& bp, float& hp)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1;
        ic2 = 2 * v2 - ic2;
        bp = v1;
        hp = x - k * v1 - v2;
        return v2;
    }
};

class DelayLine
{
public:
    void prepare (int maxSamples) { buf.assign ((size_t) maxSamples + 4, 0.0f); w = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void push (float x) { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }
    inline float read (float delaySamples) const   // fractional, delay >= 1
    {
        const int n = (int) buf.size();
        delaySamples = juce::jlimit (1.0f, (float) (n - 3), delaySamples);
        float rp = (float) w - delaySamples;
        while (rp < 0) rp += (float) n;
        const int i0 = (int) rp;
        const float fr = rp - (float) i0;
        const int i1 = (i0 + 1) % n;
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * fr;
    }
private:
    std::vector<float> buf;
    int w = 0;
};

class InsertEffect
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        dl[0].prepare ((int) (fs * 4.2f));
        dl[1].prepare ((int) (fs * 4.2f));
        reverb.setSampleRate (sampleRate);
        reset();
    }

    void reset()
    {
        dl[0].clear(); dl[1].clear();
        for (auto& f : filt) f.reset();
        for (auto& s : allpass) s = 0;
        env = 0; phase = 0; crushHold[0] = crushHold[1] = 0; crushCount = 0; fb[0] = fb[1] = 0;
        reverb.reset();
        eqState = {};
    }

    void configure (int newType, float a, float b, float c)
    {
        if (newType != type)
        {
            type = newType;
            reset();
        }
        p0 = a; p1 = b; p2 = c;
        if (type >= IFX_GATEREVERB && type <= IFX_SPRINGREVERB)
        {
            juce::Reverb::Parameters rp;
            const float sizes[4] = { 0.25f, 0.55f, 0.88f, 0.7f };
            rp.roomSize = juce::jlimit (0.0f, 1.0f, sizes[type - IFX_GATEREVERB] * (0.5f + 0.5f * p0) + 0.1f * p0);
            rp.damping = 1.0f - p1;
            rp.wetLevel = 1.0f;
            rp.dryLevel = 0.0f;
            rp.width = 1.0f;
            reverb.setParameters (rp);
        }
    }

    // Processes a stereo block in place. bpm is used for tempo-synced params.
    void process (float* L, float* R, int n, double bpm)
    {
        switch (type)
        {
            case IFX_SFILTER12: case IFX_SFILTER24: filter (L, R, n, type == IFX_SFILTER24); break;
            case IFX_EQ3: eq3 (L, R, n); break;
            case IFX_PEAKCOMP: case IFX_RMSCOMP: compressor (L, R, n, type == IFX_RMSCOMP); break;
            case IFX_OVERDRIVE: case IFX_DISTORTION: case IFX_AMPDIST: drive (L, R, n); break;
            case IFX_CRUSHER: crusher (L, R, n); break;
            case IFX_LPNOISE: lpnoise (L, R, n); break;
            case IFX_STEREOIMAGE: stereoImage (L, R, n); break;
            case IFX_AUTOWAH: autowah (L, R, n, bpm); break;
            case IFX_CHORUS: case IFX_FLANGER: modDelay (L, R, n, bpm, type == IFX_FLANGER); break;
            case IFX_PHASER: phaser (L, R, n, bpm); break;
            case IFX_TREMOLO: case IFX_AUTOPAN: tremPan (L, R, n, bpm, type == IFX_AUTOPAN); break;
            case IFX_DELAY: case IFX_STEREODELAY: case IFX_PINGPONG: delay (L, R, n, bpm); break;
            case IFX_GATEREVERB: case IFX_ROOMREVERB: case IFX_HALLREVERB: case IFX_SPRINGREVERB: rev (L, R, n); break;
            default: break;
        }
    }

private:
    float fs = 44100;
    int type = -1;
    float p0 = 0, p1 = 0, p2 = 0;
    DelayLine dl[2];
    Svf filt[4];
    float allpass[12] {};
    float env = 0, fb[2] {};
    double phase = 0;
    float crushHold[2] {};
    int crushCount = 0;
    juce::Random rng;
    juce::Reverb reverb;
    struct { float z[3][2][2] {}; } eqState;

    static inline float mixWet (float dry, float wet, float m) { return dry * (1.0f - m) + wet * m; }
    static double cutoffHz (float v) { return std::pow (2.0, v * 14.28634) + 19.0; }
    double rateHz (float v, double bpm) const { return bpm / 60.0 / noteBeatsDesc ((int) std::lround (v * 26)); }

    void filter (float* L, float* R, int n, bool steep)
    {
        for (int i = 0; i < 4; ++i) filt[i].set ((float) cutoffHz (p0), fs, p1 * (steep && i % 2 ? 0.3f : 1.0f));
        for (int i = 0; i < n; ++i)
        {
            float bp, hp;
            float l = filt[0].tick (L[i], bp, hp), r = filt[1].tick (R[i], bp, hp);
            if (steep) { l = filt[2].tick (l, bp, hp); r = filt[3].tick (r, bp, hp); }
            L[i] = mixWet (L[i], l, p2); R[i] = mixWet (R[i], r, p2);
        }
    }

    void eq3 (float* L, float* R, int n)
    {
        // simple 3-band split (200 Hz / 4 kHz) with +/-12 dB band gains
        const float gl = juce::Decibels::decibelsToGain ((p0 - 0.5f) * 24.0f);
        const float gm = juce::Decibels::decibelsToGain ((p1 - 0.5f) * 24.0f);
        const float gh = juce::Decibels::decibelsToGain ((p2 - 0.5f) * 24.0f);
        filt[0].set (200, fs, 0); filt[1].set (200, fs, 0); filt[2].set (4000, fs, 0); filt[3].set (4000, fs, 0);
        for (int i = 0; i < n; ++i)
        {
            float bp, hp;
            for (int c = 0; c < 2; ++c)
            {
                float* x = c ? R : L;
                const float lo = filt[c].tick (x[i], bp, hp);
                const float rest = x[i] - lo;
                const float mid = filt[2 + c].tick (rest, bp, hp);
                const float hi = rest - mid;
                x[i] = lo * gl + mid * gm + hi * gh;
            }
        }
    }

    void compressor (float* L, float* R, int n, bool rms)
    {
        const float thr = juce::Decibels::decibelsToGain (-60.0f * (1.0f - p0));
        const float ratio = 1.0f + p1 * 19.0f;
        const float rel = std::exp (-1.0f / (fs * (0.01f + p2 * 1.0f)));
        const float att = std::exp (-1.0f / (fs * (rms ? 0.01f : 0.0005f)));
        const float makeup = std::pow (1.0f / thr, (1.0f - 1.0f / ratio) * 0.5f);
        for (int i = 0; i < n; ++i)
        {
            float x = rms ? std::sqrt (0.5f * (L[i] * L[i] + R[i] * R[i])) : std::max (std::abs (L[i]), std::abs (R[i]));
            env = x > env ? att * env + (1 - att) * x : rel * env + (1 - rel) * x;
            float g = 1.0f;
            if (env > thr) g = std::pow (env / thr, 1.0f / ratio - 1.0f);
            L[i] *= g * makeup; R[i] *= g * makeup;
        }
    }

    void drive (float* L, float* R, int n)
    {
        const float gain = 1.0f + p0 * 7.0f;
        filt[0].set ((float) cutoffHz (0.5f + 0.5f * p1), fs, 0);
        filt[1].set ((float) cutoffHz (0.5f + 0.5f * p1), fs, 0);
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 2; ++c)
            {
                float* x = c ? R : L;
                float d = x[i] * gain, y;
                if (type == IFX_OVERDRIVE) y = d / (1.0f + std::abs (d));
                else if (type == IFX_DISTORTION) y = juce::jlimit (-0.7f, 0.7f, d) * 1.2f;
                else y = std::tanh (d + 0.2f * d * d) - 0.04f;
                float bp, hp;
                y = filt[c].tick (y, bp, hp);
                x[i] = mixWet (x[i], y, p2);
            }
    }

    void crusher (float* L, float* R, int n)
    {
        const int hold = 1 + (int) (p0 * 31.0f);
        const float levels = std::pow (2.0f, 16.0f - p1 * 14.0f);
        for (int i = 0; i < n; ++i)
        {
            if (++crushCount >= hold)
            {
                crushCount = 0;
                crushHold[0] = std::round (L[i] * levels) / levels;
                crushHold[1] = std::round (R[i] * levels) / levels;
            }
            L[i] = mixWet (L[i], crushHold[0], p2); R[i] = mixWet (R[i], crushHold[1], p2);
        }
    }

    void lpnoise (float* L, float* R, int n)
    {
        const float fc = (float) cutoffHz (1.0f - 0.45f * p0);
        filt[0].set (fc, fs, 0.2f); filt[1].set (fc, fs, 0.2f);
        for (int i = 0; i < n; ++i)
        {
            float bp, hp;
            const float noise = (rng.nextFloat() * 2 - 1) * 0.02f * p1;
            const float crackle = rng.nextFloat() < p2 * 0.0008f ? (rng.nextFloat() * 2 - 1) * 0.5f : 0.0f;
            L[i] = filt[0].tick (L[i], bp, hp) + noise + crackle;
            R[i] = filt[1].tick (R[i], bp, hp) + noise + crackle;
        }
    }

    void stereoImage (float* L, float* R, int n)
    {
        const float haas = std::abs (p0 - 0.5f) * 0.020f * fs;   // up to 10 ms
        const bool delayRight = p0 < 0.5f;
        const float balL = juce::jmin (1.0f, 2.0f - 2.0f * p1), balR = juce::jmin (1.0f, 2.0f * p1);
        const float width = p2 * 2.0f;
        for (int i = 0; i < n; ++i)
        {
            dl[0].push (L[i]); dl[1].push (R[i]);
            float l = L[i], r = R[i];
            if (haas >= 1.0f) { if (delayRight) r = dl[1].read (haas); else l = dl[0].read (haas); }
            const float m = 0.5f * (l + r), s = 0.5f * (l - r) * width;
            L[i] = (m + s) * balL; R[i] = (m - s) * balR;
        }
    }

    void autowah (float* L, float* R, int n, double bpm)
    {
        const double inc = rateHz (p0, bpm) / fs;
        for (int i = 0; i < n; i += 16)
        {
            const int m = juce::jmin (16, n - i);
            const float lfo = 0.5f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * phase);
            phase += inc * m;
            const float fc = (float) cutoffHz (0.35f + 0.5f * p1 * lfo);
            filt[0].set (fc, fs, 0.5f + 0.45f * p2); filt[1].set (fc, fs, 0.5f + 0.45f * p2);
            for (int k = i; k < i + m; ++k)
            {
                float bp, hp;
                filt[0].tick (L[k], bp, hp); L[k] = bp * 1.5f;
                filt[1].tick (R[k], bp, hp); R[k] = bp * 1.5f;
            }
        }
    }

    void modDelay (float* L, float* R, int n, double bpm, bool flanger)
    {
        const double inc = rateHz (p0, bpm) / fs;
        const float base = flanger ? 0.0015f * fs : 0.012f * fs;
        const float depth = (flanger ? 0.0035f : 0.008f) * fs * p1;
        const float feedback = flanger ? (p2 - 0.5f) * 1.9f : 0.0f;
        const float mix = flanger ? 0.5f : p2;
        for (int i = 0; i < n; ++i)
        {
            const float s = (float) std::sin (juce::MathConstants<double>::twoPi * phase);
            const float c = (float) std::cos (juce::MathConstants<double>::twoPi * phase);
            phase += inc;
            dl[0].push (L[i] + fb[0] * feedback); dl[1].push (R[i] + fb[1] * feedback);
            const float wl = dl[0].read (base + depth * (0.5f + 0.5f * s));
            const float wr = dl[1].read (base + depth * (0.5f + 0.5f * c));
            fb[0] = wl; fb[1] = wr;
            L[i] = mixWet (L[i], wl, mix); R[i] = mixWet (R[i], wr, mix);
        }
        phase -= std::floor (phase);
    }

    void phaser (float* L, float* R, int n, double bpm)
    {
        const double inc = rateHz (p0, bpm) / fs;
        const float feedback = p2 * 0.9f;
        for (int i = 0; i < n; ++i)
        {
            const float lfo = 0.5f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * phase);
            phase += inc;
            const float fc = 300.0f * std::pow (2.0f, 4.0f * p1 * lfo);
            const float t = std::tan (juce::MathConstants<float>::pi * juce::jmin (fc, fs * 0.45f) / fs);
            const float a = (t - 1) / (t + 1);
            for (int c = 0; c < 2; ++c)
            {
                float* x = c ? R : L;
                float y = x[i] + fb[c] * feedback;
                for (int s = 0; s < 6; ++s)
                {
                    float& z = allpass[c * 6 + s];
                    const float out = a * y + z;
                    z = y - a * out;
                    y = out;
                }
                fb[c] = y;
                x[i] = 0.5f * (x[i] + y);
            }
        }
        phase -= std::floor (phase);
    }

    void tremPan (float* L, float* R, int n, double bpm, bool pan)
    {
        const int wave = juce::jlimit (0, 8, (int) std::lround (p0 * 8));
        const double inc = rateHz (p1, bpm) / fs;
        for (int i = 0; i < n; ++i)
        {
            const float lfo = lfoShape (wave, phase, 0, 0);
            phase += inc;
            if (pan)
            {
                const float pos = 0.5f + 0.5f * lfo * p2;
                L[i] *= std::sqrt (2.0f * (1.0f - pos)) ; R[i] *= std::sqrt (2.0f * pos);
            }
            else
            {
                const float g = 1.0f - p2 * (0.5f - 0.5f * lfo);
                L[i] *= g; R[i] *= g;
            }
        }
        phase -= std::floor (phase);
    }

    void delay (float* L, float* R, int n, double bpm)
    {
        const float t = (float) (noteBeatsAsc ((int) std::lround (p0 * 26)) * 60.0 / bpm * fs);
        const float tl = juce::jlimit (1.0f, fs * 4.0f, t);
        const float tr = juce::jlimit (1.0f, fs * 4.0f, type == IFX_STEREODELAY ? t * 1.5f : t);
        const float feedback = p1 * 0.95f;
        for (int i = 0; i < n; ++i)
        {
            const float wl = dl[0].read (tl), wr = dl[1].read (tr);
            if (type == IFX_PINGPONG)
            {
                dl[0].push (0.5f * (L[i] + R[i]) + wr * feedback);
                dl[1].push (wl * feedback);
            }
            else if (type == IFX_STEREODELAY)
            {
                dl[0].push (L[i] + wr * feedback);
                dl[1].push (R[i] + wl * feedback);
            }
            else
            {
                dl[0].push (L[i] + wl * feedback);
                dl[1].push (R[i] + wr * feedback);
            }
            L[i] = mixWet (L[i], wl, p2);
            R[i] = mixWet (R[i], wr, p2);
        }
    }

    void rev (float* L, float* R, int n)
    {
        std::vector<float>& tl = tmp[0];
        std::vector<float>& tr = tmp[1];
        if ((int) tl.size() < n) { tl.resize ((size_t) n); tr.resize ((size_t) n); }
        std::copy (L, L + n, tl.begin()); std::copy (R, R + n, tr.begin());
        reverb.processStereo (tl.data(), tr.data(), n);
        const float mix = p2;
        for (int i = 0; i < n; ++i)
        {
            float wl = tl[(size_t) i], wr = tr[(size_t) i];
            if (type == IFX_GATEREVERB)
            {
                const float x = std::abs (L[i]) + std::abs (R[i]);
                env = x > env ? x : env * 0.9995f;
                const float gate = env > 0.02f ? 1.0f : 0.0f;
                wl *= gate; wr *= gate;
            }
            L[i] = mixWet (L[i], wl * 0.6f, mix); R[i] = mixWet (R[i], wr * 0.6f, mix);
        }
    }

    std::vector<float> tmp[2];
};

// Global send effects (SFX): one chorus and one reverb shared by all parts.
class SendEffects
{
public:
    void prepare (double sr, int maxBlock)
    {
        fs = (float) sr;
        for (auto& d : cd) d.prepare ((int) (fs * 0.05f));
        for (auto& d : pre) d.prepare ((int) (fs * 0.25f));
        reverb.setSampleRate (sr);
        lastTime = lastTone = -1;
        setReverb (predelay, time, tone);
        for (auto& b : bus) b.assign ((size_t) juce::jmax (maxBlock, 32), 0.0f);
    }
    // rate 0..1 -> 0.05..5 Hz, width = modulation depth, spread = L/R phase difference (0..180 deg)
    void setChorus (float rate, float width, float spreadAmt)
    {
        chRate = 0.05f + 4.95f * rate * rate;
        chWidth = juce::jlimit (0.0f, 1.0f, width);
        spread = juce::jlimit (0.0f, 1.0f, spreadAmt);
    }
    // pre-delay 0..1 -> 0..200 ms, time -> room size, tone -> brightness
    void setReverb (float predelayAmt, float timeAmt, float toneAmt)
    {
        predelay = predelayAmt; time = timeAmt; tone = toneAmt;
        if (std::abs (time - lastTime) < 1e-4f && std::abs (tone - lastTone) < 1e-4f) return;
        lastTime = time; lastTone = tone;
        juce::Reverb::Parameters rp;
        rp.roomSize = juce::jlimit (0.0f, 1.0f, 0.3f + 0.69f * time);
        rp.damping = juce::jlimit (0.0f, 1.0f, 1.0f - tone);
        rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 1.0f;
        reverb.setParameters (rp);
    }
    void clear (int n) { for (auto& b : bus) std::fill (b.begin(), b.begin() + n, 0.0f); }
    float* chorusL() { return bus[0].data(); }
    float* chorusR() { return bus[1].data(); }
    float* reverbL() { return bus[2].data(); }
    float* reverbR() { return bus[3].data(); }

    void process (float* L, float* R, int n, float chorusLevel, float reverbLevel)
    {
        const float base = 0.010f * fs, depth = (0.001f + 0.006f * chWidth) * fs;
        const double offset = 0.5 * spread;
        for (int i = 0; i < n; ++i)
        {
            const float sL = (float) std::sin (juce::MathConstants<double>::twoPi * phase);
            const float sR = (float) std::sin (juce::MathConstants<double>::twoPi * (phase + offset));
            phase += chRate / fs;
            cd[0].push (bus[0][(size_t) i]); cd[1].push (bus[1][(size_t) i]);
            L[i] += chorusLevel * cd[0].read (base + depth * (0.5f + 0.5f * sL));
            R[i] += chorusLevel * cd[1].read (base + depth * (0.5f + 0.5f * sR));
        }
        phase -= std::floor (phase);
        const float pd = juce::jmax (1.0f, predelay * 0.2f * fs);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                pre[c].push (bus[2 + c][(size_t) i]);
                bus[2 + c][(size_t) i] = pre[c].read (pd);
            }
        reverb.processStereo (bus[2].data(), bus[3].data(), n);
        for (int i = 0; i < n; ++i)
        {
            L[i] += reverbLevel * 0.5f * bus[2][(size_t) i];
            R[i] += reverbLevel * 0.5f * bus[3][(size_t) i];
        }
    }

private:
    float fs = 44100;
    double phase = 0;
    float chRate = 0.55f, chWidth = 0.5f, spread = 1.0f;
    float predelay = 0.1f, time = 0.6f, tone = 0.55f, lastTime = -1, lastTone = -1;
    DelayLine cd[2], pre[2];
    juce::Reverb reverb;
    std::vector<float> bus[4];
};

} // namespace cure
