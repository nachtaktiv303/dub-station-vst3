#pragma once
// The plugin parameters. IDs, ranges and defaults are the engine's keys (dsp/engine.cpp, the MPC's
// vst/params.json), so a value means exactly the same on the MPC and in the desktop plugin.
#include <juce_core/juce_core.h>

namespace dub
{
enum class Kind
{
    Float,
    Toggle
};

struct ParamInfo
{
    const char *id;
    const char *name;
    float       min, max, def;
    const char *unit;
    Kind        kind;
};

// order = host automation list order
inline const ParamInfo kParams[] = {
    {"time",      "A Time",          10.f,   2000.f, 1000.f, "ms", Kind::Float},
    {"sync",      "A Sync",          0.f,    1.f,    1.f,    "",   Kind::Toggle},
    {"feedback",  "A Feedback",      0.f,    120.f,  45.f,   "%",  Kind::Float},
    {"fbfilter",  "A Filter",        -100.f, 100.f,  0.f,    "",   Kind::Float},
    {"fbreso",    "A Resonance",     0.f,    100.f,  20.f,   "%",  Kind::Float},
    {"width",     "A Width",         0.f,    100.f,  100.f,  "%",  Kind::Float},
    {"delay",     "A Level",         0.f,    100.f,  75.f,   "%",  Kind::Float},
    {"time2",     "B Time",          10.f,   2000.f, 500.f,  "ms", Kind::Float},
    {"sync2",     "B Sync",          0.f,    1.f,    1.f,    "",   Kind::Toggle},
    {"feedback2", "B Feedback",      0.f,    120.f,  35.f,   "%",  Kind::Float},
    {"tone2",     "B Filter",        -100.f, 100.f,  0.f,    "",   Kind::Float},
    {"flanger",   "B Flanger",       0.f,    100.f,  0.f,    "%",  Kind::Float},
    {"width2",    "B Width",         0.f,    100.f,  100.f,  "%",  Kind::Float},
    {"delay2",    "B Level",         0.f,    100.f,  0.f,    "%",  Kind::Float},
    {"sat",       "Saturation",      0.f,    100.f,  0.f,    "%",  Kind::Float},
    {"duck",      "Ducking",         0.f,    100.f,  0.f,    "%",  Kind::Float},
    {"mix",       "Mix",             0.f,    100.f,  50.f,   "%",  Kind::Float},
    {"panic",     "Panic",           0.f,    1.f,    0.f,    "",   Kind::Toggle},
    {"size",      "Reverb Size",     0.f,    100.f,  55.f,   "%",  Kind::Float},
    {"reverb",    "Reverb Amount",   0.f,    100.f,  30.f,   "%",  Kind::Float},
    {"damp",      "Reverb Damp",     0.f,    100.f,  20.f,   "%",  Kind::Float},
    {"hipass",    "Reverb Hi-Pass",  0.f,    100.f,  0.f,    "%",  Kind::Float},
    {"drift",     "Reverb Drift",    0.f,    100.f,  40.f,   "%",  Kind::Float},
    {"drive",     "Tape Drive",      0.f,    100.f,  20.f,   "%",  Kind::Float},
    {"warble",    "Tape Warble",     0.f,    100.f,  15.f,   "%",  Kind::Float},
    {"degrade",   "Tape Degrade",    0.f,    100.f,  0.f,    "%",  Kind::Float},
    {"noise",     "Noise",           0.f,    100.f,  0.f,    "%",  Kind::Float},
    {"noisefilt", "Noise Tone",      0.f,    100.f,  50.f,   "%",  Kind::Float},
    {"noisemod",  "Noise Motion",    0.f,    100.f,  30.f,   "%",  Kind::Float},
    {"crackle",   "Crackle",         0.f,    100.f,  0.f,    "%",  Kind::Float},
    {"phzrate",   "Phaser Rate",     0.f,    100.f,  30.f,   "%",  Kind::Float},
    {"phzdepth",  "Phaser Depth",    0.f,    100.f,  0.f,    "%",  Kind::Float},
};
constexpr int kNumParams = (int)(sizeof(kParams) / sizeof(kParams[0]));

// --- value read-outs (mirror the engine's get_param texts) ---------------------------------------------------
inline const float       kDivBeats[7]  = {0.25f, 0.5f, 0.75f, 1.f, 1.5f, 2.f, 4.f};
inline const char *const kDivLabels[7] = {"1/16", "1/8", "1/8.", "1/4", "1/4.", "1/2", "1"};

inline int syncDivIndex(float timeMs)
{
    const int d = (int)((timeMs - 10.f) / 1990.f * 7.f);
    return d < 0 ? 0 : d > 6 ? 6 : d;
}

inline juce::String timeText(float ms, bool synced, double bpm)
{
    if(synced)
    {
        const int   d   = syncDivIndex(ms);
        const float div = kDivBeats[d] * 60000.f / (float)(bpm > 1.0 ? bpm : 120.0);
        return juce::String(kDivLabels[d]) + " (" + juce::String((int)(div + 0.5f)) + " ms)";
    }
    return juce::String((int)(ms + 0.5f)) + " ms";
}

inline juce::String hzText(const char *mode, float fc)
{
    if(fc >= 1000.f)
        return juce::String(mode) + " " + juce::String(fc / 1000.f, 1) + " kHz";
    return juce::String(mode) + " " + juce::String((int)(fc + 0.5f)) + " Hz";
}

// Delay A DJ filter (mirrors DJFilter::SetControl in the engine)
inline juce::String filterAText(float v)
{
    const float sr   = 44100.f;
    const float ctrl = 0.5f + (v * .01f) * 0.35f;
    float glp = juce::jlimit(0.f, .90f, .01f + ctrl * 2.f);
    glp       = glp * glp * glp;
    float ghp = juce::jlimit(0.f, 1.f, ctrl * 1.9f - 1.f);
    ghp       = ghp * ghp * ghp;
    if(ghp > 1e-4f)
        return hzText("HP", -(sr / 6.2831853f) * std::log(1.f - ghp));
    if(glp < .728f)
        return hzText("LP", -(sr / 6.2831853f) * std::log(1.f - glp));
    return "Flat";
}

// Delay B tilt filter
inline juce::String filterBText(float v)
{
    const float tv = v * .01f;
    if(tv < -0.02f)
        return hzText("LP", 12000.f * std::pow(100.f / 12000.f, -tv));
    if(tv > 0.02f)
        return hzText("HP", 20.f * std::pow(500.f, tv));
    return "Flat";
}
} // namespace dub
