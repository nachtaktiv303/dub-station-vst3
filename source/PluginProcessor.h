#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>

#include "Engine.h"
#include "Params.h"
#include "Resampler.h"

class DubStationProcessor : public juce::AudioProcessor
{
  public:
    DubStationProcessor();
    ~DubStationProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String &) override {}

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // for the editor
    double getBpm() const { return bpm_.load(); }
    float  getOutputLevel() const { return outLevel_.load(); }
    juce::String valueText(int index, float value) const;

  private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void createEngine();
    void sendParams(bool force);
    void runEngine(float *l, float *r, int n);

    const mpc_engine_t *api_    = nullptr;
    void               *engine_ = nullptr;

    std::atomic<float> *raw_[dub::kNumParams] = {};
    float               sent_[dub::kNumParams] = {};
    std::atomic<bool>   forceSend_ {true};
    bool                lastPanic_ = false;
    std::atomic<double> bpm_ {120.0};
    double              sentBpm_ = 0.0;
    std::atomic<float>  outLevel_ {0.f};

    // host rate <-> 44.1 kHz
    bool                  resample_ = false;
    dub::StreamResampler  down_, up_;
    std::vector<float>    engL_, engR_, upL_, upR_, inL_, inR_;
    std::vector<float>    fifoL_, fifoR_;
    int                   fifoRead_ = 0, fifoWrite_ = 0, fifoCount_ = 0;
    int                   maxChunk_ = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DubStationProcessor)
};
