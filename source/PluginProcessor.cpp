#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <cstdio>

DubStationProcessor::DubStationProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "DubStation", createLayout())
{
    api_ = mpc_engine();
    for(int i = 0; i < dub::kNumParams; i++)
        raw_[i] = apvts.getRawParameterValue(dub::kParams[i].id);
    createEngine();
}

DubStationProcessor::~DubStationProcessor()
{
    if(engine_)
        api_->destroy(engine_);
}

juce::String DubStationProcessor::valueText(int index, float v) const
{
    const auto &p  = dub::kParams[index];
    const juce::String id(p.id);
    if(id == "time" || id == "time2")
    {
        const auto *sync = raw_[id == "time" ? 1 : 8];
        return dub::timeText(v, sync != nullptr && sync->load() >= .5f, bpm_.load());
    }
    if(id == "fbfilter")
        return dub::filterAText(v);
    if(id == "tone2")
        return dub::filterBText(v);
    if(id == "sync" || id == "sync2")
        return v >= .5f ? "Sync" : "Free";
    if(id == "panic")
        return v >= .5f ? "On" : "Off";
    return juce::String((int)std::lround(v)) + " " + p.unit;
}

juce::AudioProcessorValueTreeState::ParameterLayout DubStationProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for(int i = 0; i < dub::kNumParams; i++)
    {
        const auto &p = dub::kParams[i];
        if(p.kind == dub::Kind::Toggle)
        {
            layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID {p.id, 1}, p.name, p.def >= .5f,
                juce::AudioParameterBoolAttributes().withStringFromValueFunction(
                    [this, i](bool b, int) { return valueText(i, b ? 1.f : 0.f); })));
        }
        else
        {
            juce::NormalisableRange<float> range(p.min, p.max, 0.f);
            if(juce::String(p.id) == "time" || juce::String(p.id) == "time2")
                range.setSkewForCentre(500.f); // more knob travel for the short times
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID {p.id, 1}, p.name, range, p.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                    [this, i](float v, int) { return valueText(i, v); })));
        }
    }
    return layout;
}

void DubStationProcessor::createEngine()
{
    if(engine_)
        api_->destroy(engine_);
    engine_     = api_->create(nullptr);
    forceSend_  = true;
    sentBpm_    = 0.0;
}

void DubStationProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    createEngine(); // fresh tails, all parameters re-sent on the next block

    maxChunk_ = juce::jmax(64, samplesPerBlock);
    resample_ = std::abs(sampleRate - dub::kEngineRate) > 0.5;
    if(!resample_)
    {
        setLatencySamples(0);
        return;
    }

    const double toEngine = dub::kEngineRate / sampleRate;
    down_.prepare(sampleRate, dub::kEngineRate, maxChunk_);
    up_.prepare(dub::kEngineRate, sampleRate, (int)std::ceil(maxChunk_ * toEngine) + 8);

    // Output FIFO pre-filled with silence: the resamplers need to look ahead by half their kernel, so the
    // output runs `latency` samples behind the input. Reported to the host for delay compensation.
    const int latency = (int)std::ceil(down_.halfWidth() + up_.halfWidth() / toEngine) + 4;
    setLatencySamples(latency);

    const size_t engCap = (size_t)std::ceil(maxChunk_ * toEngine) + 16;
    engL_.assign(engCap, 0.f);
    engR_.assign(engCap, 0.f);
    upL_.assign((size_t)maxChunk_ + 32, 0.f);
    upR_.assign((size_t)maxChunk_ + 32, 0.f);
    inL_.assign((size_t)maxChunk_, 0.f);
    inR_.assign((size_t)maxChunk_, 0.f);

    const size_t fifoCap = (size_t)(latency + 4 * maxChunk_ + 64);
    fifoL_.assign(fifoCap, 0.f);
    fifoR_.assign(fifoCap, 0.f);
    fifoRead_  = 0;
    fifoWrite_ = latency;
    fifoCount_ = latency;
}

void DubStationProcessor::releaseResources() {}

bool DubStationProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if(out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    if(in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono())
        return false;
    return !(out == juce::AudioChannelSet::mono() && in == juce::AudioChannelSet::stereo());
}

void DubStationProcessor::sendParams(bool force)
{
    char buf[32];
    for(int i = 0; i < dub::kNumParams; i++)
    {
        const float v  = raw_[i]->load();
        const char *id = dub::kParams[i].id;
        if(i == 17) // panic: only a rising edge fires the flush (the engine flushes on any write)
        {
            const bool on = v >= .5f;
            if(on && !lastPanic_)
                api_->set_param(engine_, id, "1");
            lastPanic_ = on;
            continue;
        }
        if(!force && v == sent_[i])
            continue;
        sent_[i] = v;
        std::snprintf(buf, sizeof buf, "%.6g", (double)v);
        api_->set_param(engine_, id, buf);
    }
}

void DubStationProcessor::runEngine(float *l, float *r, int n)
{
    if(!resample_)
    {
        dub_engine_process_float(engine_, l, r, n);
        return;
    }

    std::copy(l, l + n, inL_.data());
    std::copy(r, r + n, inR_.data());
    down_.push(inL_.data(), inR_.data(), n);
    const int m = down_.pull(engL_.data(), engR_.data(), (int)engL_.size());
    if(m > 0)
        dub_engine_process_float(engine_, engL_.data(), engR_.data(), m);
    up_.push(engL_.data(), engR_.data(), m);

    const int cap = (int)fifoL_.size();
    for(;;)
    {
        const int k = up_.pull(upL_.data(), upR_.data(), (int)upL_.size());
        for(int i = 0; i < k && fifoCount_ < cap; i++)
        {
            fifoL_[(size_t)fifoWrite_] = upL_[(size_t)i];
            fifoR_[(size_t)fifoWrite_] = upR_[(size_t)i];
            fifoWrite_                 = (fifoWrite_ + 1) % cap;
            fifoCount_++;
        }
        if(k < (int)upL_.size())
            break;
    }

    for(int i = 0; i < n; i++)
    {
        if(fifoCount_ > 0)
        {
            l[i]      = fifoL_[(size_t)fifoRead_];
            r[i]      = fifoR_[(size_t)fifoRead_];
            fifoRead_ = (fifoRead_ + 1) % cap;
            fifoCount_--;
        }
        else
            l[i] = r[i] = 0.f;
    }
}

void DubStationProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &)
{
    juce::ScopedNoDenormals noDenormals;

    if(auto *ph = getPlayHead())
        if(auto pos = ph->getPosition())
            if(auto b = pos->getBpm())
                if(*b > 1.0)
                    bpm_.store(*b);

    if(engine_ == nullptr)
    {
        buffer.clear();
        return;
    }

    sendParams(forceSend_.exchange(false));
    const double bpm = bpm_.load();
    if(std::abs(bpm - sentBpm_) > 1e-3)
    {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.4f", bpm);
        api_->set_param(engine_, "lfo_bpm", buf);
        sentBpm_ = bpm;
    }

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int total  = buffer.getNumSamples();

    // a stereo working pair (mono input is copied to both sides)
    float *l = buffer.getWritePointer(0);
    float *r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    float  monoR[512];

    float peak = 0.f;
    for(int start = 0; start < total;)
    {
        const int n  = juce::jmin(total - start, maxChunk_, 512);
        float    *cl = l + start;
        float    *cr = r != nullptr ? r + start : monoR;
        if(numIn < 2 || r == nullptr)
            std::copy(cl, cl + n, cr);

        runEngine(cl, cr, n);

        if(numOut < 2) // mono out: fold the stereo image down
            for(int i = 0; i < n; i++)
                cl[i] = 0.5f * (cl[i] + cr[i]);
        for(int i = 0; i < n; i++)
            peak = juce::jmax(peak, std::abs(cl[i]), std::abs(cr[i]));
        start += n;
    }
    for(int ch = numOut; ch < buffer.getNumChannels(); ch++)
        buffer.clear(ch, 0, total);

    // VU ballistics (~300 ms)
    const float prev = outLevel_.load();
    const float a    = (float)std::exp(-(double)total / (getSampleRate() * 0.3));
    outLevel_.store(peak > prev ? prev + (peak - prev) * 0.5f : prev * a + peak * (1.f - a));
}

void DubStationProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = apvts.copyState();
    state.setProperty("version", 1, nullptr);
    if(auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void DubStationProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    if(auto xml = getXmlFromBinary(data, sizeInBytes))
        if(xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
    forceSend_ = true;
}

juce::AudioProcessorEditor *DubStationProcessor::createEditor()
{
    return new DubStationEditor(*this);
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new DubStationProcessor();
}
