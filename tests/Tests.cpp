// Offline checks for the desktop wrapper (not part of the plugin):
//  1. dry path: with MIX = 0 the output is the input, delayed by exactly the reported latency, at 44.1/48/96 kHz
//  2. the echo of Delay A lands at the set time and with the same level at every host rate
//  3. long noisy run with everything turned up: no NaN/Inf, bounded output
//  4. optional: renders the editor's three pages to PNG files (pass an output folder)
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <cstdio>

#include "PluginProcessor.h"
#include "PluginEditor.h"

static int failures = 0;
static void check(bool ok, const juce::String &what)
{
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    if(!ok)
        failures++;
}

static void setParam(DubStationProcessor &p, const char *id, float v)
{
    auto *prm = p.apvts.getParameter(id);
    prm->setValueNotifyingHost(prm->convertTo0to1(v));
}

static std::vector<float> render(DubStationProcessor &p, double sr, const std::vector<float> &in, int block)
{
    p.setRateAndBufferSizeDetails(sr, block);
    p.prepareToPlay(sr, block);
    std::vector<float> out(in.size());
    juce::AudioBuffer<float> buf(2, block);
    juce::MidiBuffer         midi;
    for(size_t s = 0; s < in.size(); s += (size_t)block)
    {
        const int n = (int)std::min<size_t>((size_t)block, in.size() - s);
        buf.setSize(2, n, false, false, true);
        for(int i = 0; i < n; i++)
            buf.setSample(0, i, in[s + (size_t)i]), buf.setSample(1, i, in[s + (size_t)i]);
        p.processBlock(buf, midi);
        for(int i = 0; i < n; i++)
            out[s + (size_t)i] = buf.getSample(0, i);
    }
    return out;
}

static void dryPathTest(double sr)
{
    DubStationProcessor p;
    setParam(p, "mix", 0.f);
    const int          len = (int)sr;
    std::vector<float> in((size_t)len);
    juce::Random       rnd(1);
    for(int i = 0; i < len; i++) // band-limited test signal: a few sines well below 20 kHz
        in[(size_t)i] = 0.3f * std::sin(2 * M_PI * 220.0 * i / sr) + 0.2f * std::sin(2 * M_PI * 3100.0 * i / sr)
                        + 0.1f * std::sin(2 * M_PI * 12500.0 * i / sr);
    const auto out = render(p, sr, in, 333); // odd block size on purpose
    const int  lat = p.getLatencySamples();
    double     err = 0.0, ref = 0.0;
    for(int i = len / 4; i < len - lat; i++)
    {
        const double d = out[(size_t)(i + lat)] - in[(size_t)i];
        err += d * d;
        ref += (double)in[(size_t)i] * in[(size_t)i];
    }
    const double snr = 10.0 * std::log10(ref / std::max(err, 1e-30));
    check(snr > 60.0, juce::String::formatted("dry path @ %.0f Hz: latency %d samples, accuracy %.1f dB", sr, lat, snr));
}

static void echoTest(double sr, double &levelOut)
{
    DubStationProcessor p;
    setParam(p, "sync", 0.f);
    setParam(p, "time", 250.f);
    setParam(p, "feedback", 0.f);
    setParam(p, "fbfilter", 0.f);
    setParam(p, "fbreso", 0.f);
    setParam(p, "delay", 100.f);
    setParam(p, "delay2", 0.f);
    setParam(p, "reverb", 0.f);
    setParam(p, "drive", 0.f);
    setParam(p, "warble", 0.f);
    setParam(p, "width", 0.f);
    setParam(p, "mix", 100.f);
    const int          len = (int)(sr * 0.6);
    std::vector<float> in((size_t)len, 0.f);
    const int          click = (int)(sr * 0.05);
    for(int i = 0; i < (int)(sr * 0.004); i++) // 4 ms 1 kHz burst
        in[(size_t)(click + i)] = 0.5f * (float)std::sin(2 * M_PI * 1000.0 * i / sr);
    const auto out = render(p, sr, in, 256);
    const int  lat = p.getLatencySamples();
    // find the echo peak after the burst
    int   peakAt = 0;
    float peak   = 0.f;
    for(int i = click + (int)(sr * 0.1); i < len; i++)
        if(std::abs(out[(size_t)i]) > peak)
            peak = std::abs(out[(size_t)i]), peakAt = i;
    const double ms = (peakAt - lat - click) * 1000.0 / sr;
    levelOut        = peak;
    check(std::abs(ms - 252.0) < 6.0,
          juce::String::formatted("echo @ %.0f Hz arrives after %.1f ms (set 250 ms), peak %.3f", sr, ms, peak));
}

static void stressTest(double sr)
{
    DubStationProcessor p;
    for(int i = 0; i < dub::kNumParams; i++)
        if(juce::String(dub::kParams[i].id) != "panic")
            setParam(p, dub::kParams[i].id, dub::kParams[i].max);
    const int          len = (int)(sr * 8);
    std::vector<float> in((size_t)len);
    juce::Random       rnd(7);
    for(auto &x : in)
        x = rnd.nextFloat() * 1.6f - 0.8f;
    const auto out = render(p, sr, in, 512);
    bool       finite = true;
    float      mx     = 0.f;
    for(float x : out)
    {
        if(!std::isfinite(x))
            finite = false;
        mx = std::max(mx, std::abs(x));
    }
    check(finite && mx < 8.f, juce::String::formatted("everything at max @ %.0f Hz for 8 s: finite, peak %.2f", sr, mx));
}

static void snapshotEditor(const juce::File &dir)
{
    DubStationProcessor p;
    p.prepareToPlay(48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
    ed->setSize(DubStationEditor::kW, DubStationEditor::kH);
    auto *canvas = dynamic_cast<dub::Canvas *>(ed->getChildComponent(0));
    for(int page = 0; page < 3; page++)
    {
        canvas->showPage(page);
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = dir.getChildFile("page" + juce::String(page + 1) + ".png");
        f.deleteFile();
        juce::FileOutputStream os(f);
        png.writeImageToStream(img, os);
        std::printf("wrote %s\n", f.getFullPathName().toRawUTF8());
    }
}

int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    for(double sr : {44100.0, 48000.0, 96000.0})
        dryPathTest(sr);
    double l44 = 0, l48 = 0, l96 = 0;
    echoTest(44100.0, l44);
    echoTest(48000.0, l48);
    echoTest(96000.0, l96);
    check(std::abs(l48 / l44 - 1.0) < 0.1 && std::abs(l96 / l44 - 1.0) < 0.1, "echo level matches across rates");
    stressTest(48000.0);
    stressTest(44100.0);
    if(argc > 1)
        snapshotEditor(juce::File(argv[1]));
    std::printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
