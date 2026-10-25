#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

#include "PluginProcessor.h"

namespace dub
{
// Space-Echo look: black Moog knob from the MPC skin's 128-frame filmstrip, label under it that turns into
// the value read-out while the knob is hovered or turned.
class Knob : public juce::Component
{
  public:
    Knob(DubStationProcessor &proc, int paramIndex, juce::String label);
    void resized() override;
    void paint(juce::Graphics &) override;
    void mouseEnter(const juce::MouseEvent &) override { repaint(); }
    void mouseExit(const juce::MouseEvent &) override { repaint(); }

    juce::Slider slider;

  private:
    DubStationProcessor &proc_;
    int                  index_;
    juce::String         label_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach_;
};

class SyncLed : public juce::Component
{
  public:
    SyncLed(DubStationProcessor &proc, const char *paramId);
    void resized() override;
    void paint(juce::Graphics &) override;

    juce::ToggleButton button;

  private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attach_;
};

class PanicButton : public juce::Button, private juce::Timer
{
  public:
    explicit PanicButton(DubStationProcessor &proc);
    void paintButton(juce::Graphics &, bool highlighted, bool down) override;
    void clicked() override;

  private:
    void timerCallback() override;
    juce::RangedAudioParameter *param_;
    juce::Image                 on_, off_;
    bool                        lit_ = false;
};

class VuMeter : public juce::Component, private juce::Timer
{
  public:
    explicit VuMeter(DubStationProcessor &proc);
    void paint(juce::Graphics &) override;

  private:
    void timerCallback() override;
    DubStationProcessor &proc_;
    float                needle_ = 0.f;
};

// all panels are laid out on a fixed 1280 x 628 canvas (the MPC skin's coordinates) and scaled as a whole
class Canvas : public juce::Component
{
  public:
    explicit Canvas(DubStationProcessor &proc);
    void paint(juce::Graphics &) override;
    void showPage(int page);

  private:
    struct Panel
    {
        juce::Rectangle<float> r;
        juce::String           title;
        float                  titleSize;
        bool                   black;
    };
    void addKnob(int page, const char *id, const char *label, int cx, int cy);
    void addTitle(int page, juce::Rectangle<float> r, juce::String title, bool black, float size = 26.f);

    DubStationProcessor                    &proc_;
    int                                     page_ = 0;
    std::vector<std::unique_ptr<Knob>>      knobs_;
    std::vector<int>                        knobPage_;
    std::unique_ptr<SyncLed>                syncA_, syncB_;
    std::unique_ptr<PanicButton>            panic_;
    std::unique_ptr<VuMeter>                vu_;
    std::vector<Panel>                      panels_[3];
    juce::OwnedArray<juce::TextButton>      tabs_;
    juce::Image                             grunge_, grime_;
};
} // namespace dub

class DubStationEditor : public juce::AudioProcessorEditor
{
  public:
    explicit DubStationEditor(DubStationProcessor &);
    void resized() override;
    void paint(juce::Graphics &) override;

    static constexpr int kW = 1280, kH = 628;

  private:
    dub::Canvas canvas_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DubStationEditor)
};
