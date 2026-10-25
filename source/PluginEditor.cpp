#include "PluginEditor.h"
#include "BinaryData.h"

namespace dub
{
namespace
{
const juce::Colour kFace {0xff2d4626};   // green faceplate
const juce::Colour kBlack {0xff0c0f0d};  // black section fields
const juce::Colour kInk {0xffeef1ec};    // titles
const juce::Colour kLabel {0xffe6ece4};  // knob labels
const juce::Colour kMint {0xff7fe0ac};   // read-outs, LEDs

juce::Typeface::Ptr regularFace()
{
    static auto tf = juce::Typeface::createSystemTypefaceFor(BinaryData::TitilliumWebRegular_ttf,
                                                              BinaryData::TitilliumWebRegular_ttfSize);
    return tf;
}
juce::Typeface::Ptr boldFace()
{
    static auto tf = juce::Typeface::createSystemTypefaceFor(BinaryData::TitilliumWebSemiBold_ttf,
                                                              BinaryData::TitilliumWebSemiBold_ttfSize);
    return tf;
}
juce::Font font(bool bold, float size, float tracking = 0.f)
{
    return juce::Font(juce::FontOptions(bold ? boldFace() : regularFace()).withHeight(size)
                          .withKerningFactor(tracking));
}

int paramIndex(const char *id)
{
    for(int i = 0; i < kNumParams; i++)
        if(!std::strcmp(kParams[i].id, id))
            return i;
    jassertfalse;
    return 0;
}

class KnobLook : public juce::LookAndFeel_V4
{
  public:
    KnobLook()
    {
        strip_ = juce::ImageCache::getFromMemory(BinaryData::moog_knob_voice_png, BinaryData::moog_knob_voice_pngSize);
    }
    void drawRotarySlider(juce::Graphics &g, int x, int y, int w, int h, float pos, float, float,
                          juce::Slider &) override
    {
        const int frames = strip_.getHeight() / strip_.getWidth();
        const int fw     = strip_.getWidth();
        const int f      = juce::jlimit(0, frames - 1, (int)std::lround(pos * (frames - 1)));
        const int side   = juce::jmin(w, h);
        // the knob fills ~92% of its 112 px frame; draw so the visible knob is `side` wide
        const float sz = side * 112.f / 103.f;
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(strip_, (int)(x + (w - sz) * .5f), (int)(y + (h - sz) * .5f + 1.f), (int)sz, (int)sz,
                    0, f * fw, fw, fw);
    }

  private:
    juce::Image strip_;
};

KnobLook &knobLook()
{
    static KnobLook l;
    return l;
}
} // namespace

// ---------------------------------------------------------------------------------------------------------
Knob::Knob(DubStationProcessor &proc, int paramIndex, juce::String label)
    : proc_(proc), index_(paramIndex), label_(std::move(label))
{
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setLookAndFeel(&knobLook());
    slider.setMouseDragSensitivity(220);
    slider.setVelocityBasedMode(false);
    slider.setPopupDisplayEnabled(false, false, nullptr);
    slider.onValueChange = [this] { repaint(); };
    slider.addMouseListener(this, false);
    addAndMakeVisible(slider);
    attach_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, kParams[paramIndex].id, slider);
    slider.setDoubleClickReturnValue(true, kParams[paramIndex].def);
}

void Knob::resized()
{
    slider.setBounds(getWidth() / 2 - 46, 2, 92, 92);
}

void Knob::paint(juce::Graphics &g)
{
    const bool  active = slider.isMouseOverOrDragging();
    const auto  area   = juce::Rectangle<float>(0.f, 102.f, (float)getWidth(), 22.f);
    if(active)
    {
        const auto text = proc_.valueText(index_, (float)slider.getValue());
        g.setFont(font(true, 15.f));
        const float tw = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), text) + 14.f;
        const auto  box = area.withSizeKeepingCentre(juce::jmin((float)getWidth(), tw), 22.f);
        g.setColour(juce::Colour(0xff0a0f0c).withAlpha(0.92f));
        g.fillRoundedRectangle(box, 5.f);
        g.setColour(juce::Colour(0x59cfd4d0));
        g.drawRoundedRectangle(box, 5.f, 1.2f);
        g.setColour(kMint);
        g.drawText(text, box, juce::Justification::centred, false);
    }
    else
    {
        g.setColour(kLabel);
        g.setFont(font(true, 17.f, 0.03f));
        g.drawText(label_, area, juce::Justification::centred, false);
    }
}

// ---------------------------------------------------------------------------------------------------------
SyncLed::SyncLed(DubStationProcessor &proc, const char *paramId)
{
    button.setClickingTogglesState(true);
    button.setAlpha(0.f); // invisible hit area; painted by this component
    button.onStateChange = [this] { repaint(); };
    addAndMakeVisible(button);
    attach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, paramId, button);
}

void SyncLed::resized()
{
    button.setBounds(getLocalBounds());
}

void SyncLed::paint(juce::Graphics &g)
{
    const bool on = button.getToggleState();
    const auto c  = juce::Point<float>(getWidth() * .5f, 15.f);
    if(on)
    {
        juce::ColourGradient glow(kMint.withAlpha(0.55f), c, kMint.withAlpha(0.f), c.translated(20.f, 0.f), true);
        g.setGradientFill(glow);
        g.fillEllipse(juce::Rectangle<float>(40.f, 40.f).withCentre(c));
    }
    g.setColour(juce::Colour(0xff0d120f));
    g.fillEllipse(juce::Rectangle<float>(28.f, 28.f).withCentre(c));
    g.setColour(juce::Colour(0xff2f3a34));
    g.drawEllipse(juce::Rectangle<float>(28.f, 28.f).withCentre(c), 1.5f);
    const auto led = juce::Rectangle<float>(18.f, 18.f).withCentre(c);
    if(on)
    {
        g.setGradientFill(juce::ColourGradient(juce::Colours::white, c.translated(-3.f, -4.f), juce::Colour(0xff3fae6f),
                                               c.translated(6.f, 8.f), true));
    }
    else
    {
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a524c), c.translated(-3.f, -4.f), juce::Colour(0xff1d221e),
                                               c.translated(6.f, 8.f), true));
    }
    g.fillEllipse(led);
    g.setColour(kLabel);
    g.setFont(font(true, 14.f, 0.03f));
    g.drawText("SYNC", juce::Rectangle<float>(0.f, 36.f, (float)getWidth(), 18.f), juce::Justification::centred, false);
}

// ---------------------------------------------------------------------------------------------------------
PanicButton::PanicButton(DubStationProcessor &proc) : juce::Button("Panic")
{
    param_ = proc.apvts.getParameter("panic");
    on_    = juce::ImageCache::getFromMemory(BinaryData::panic_on_png, BinaryData::panic_on_pngSize);
    off_   = juce::ImageCache::getFromMemory(BinaryData::panic_off_png, BinaryData::panic_off_pngSize);
    setTooltip("Kills all delay and reverb tails at once");
}

void PanicButton::paintButton(juce::Graphics &g, bool highlighted, bool down)
{
    const auto &img = (lit_ || down) ? on_ : off_;
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(img, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    if(highlighted && !down && !lit_)
    {
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(4.f), 8.f);
    }
}

void PanicButton::clicked()
{
    // momentary: the parameter goes on (the processor flushes on that edge) and drops back off after ~300 ms
    param_->beginChangeGesture();
    param_->setValueNotifyingHost(1.f);
    param_->endChangeGesture();
    lit_ = true;
    repaint();
    startTimer(300);
}

void PanicButton::timerCallback()
{
    stopTimer();
    param_->beginChangeGesture();
    param_->setValueNotifyingHost(0.f);
    param_->endChangeGesture();
    lit_ = false;
    repaint();
}

// ---------------------------------------------------------------------------------------------------------
VuMeter::VuMeter(DubStationProcessor &proc) : proc_(proc)
{
    startTimerHz(30);
}

void VuMeter::timerCallback()
{
    // level -> needle 0..1 over -20..+3 dB, light smoothing for a lazy analog swing
    const float lvl    = proc_.getOutputLevel();
    const float db     = juce::Decibels::gainToDecibels(lvl, -60.f);
    const float target = juce::jlimit(0.f, 1.f, (db + 20.f) / 23.f);
    needle_ += (target - needle_) * 0.35f;
    repaint();
}

void VuMeter::paint(juce::Graphics &g)
{
    // drawn in the 240 x 120 coordinates of the MPC skin's vu.svg
    g.addTransform(juce::AffineTransform::scale(getWidth() / 240.f, getHeight() / 120.f));
    g.setColour(juce::Colour(0xff0c110e));
    g.fillRoundedRectangle(2.f, 2.f, 236.f, 116.f, 8.f);
    g.setColour(juce::Colour(0xff2c352e));
    g.drawRoundedRectangle(2.f, 2.f, 236.f, 116.f, 8.f, 1.5f);
    const juce::Rectangle<float> face(14.f, 12.f, 212.f, 78.f);
    g.setColour(juce::Colour(0xffe9e2cc));
    g.fillRoundedRectangle(face, 4.f);
    g.setColour(juce::Colours::white.withAlpha(0.18f));
    g.fillRoundedRectangle(14.f, 12.f, 212.f, 24.f, 4.f);

    const juce::Point<float> pivot(120.f, 112.f);
    const float              R = 92.f;
    auto arc = [&](float a0, float a1) {
        juce::Path p;
        p.addCentredArc(pivot.x, pivot.y, R, R, 0.f, a0, a1, true);
        return p;
    };
    const float a0 = -juce::MathConstants<float>::pi * 0.25f, a1 = -a0;
    g.setColour(juce::Colour(0xff3a3a3a));
    g.strokePath(arc(a0, a1), juce::PathStrokeType(1.6f));
    g.setColour(juce::Colour(0xffc0392b));
    g.strokePath(arc(a1 * 0.48f, a1), juce::PathStrokeType(3.5f));
    g.setColour(juce::Colour(0xff3a3a3a));
    for(int i = 0; i < 5; i++)
    {
        const float a  = a0 + (a1 - a0) * i / 4.f;
        const auto  p0 = pivot.getPointOnCircumference(R, a);
        const auto  p1 = pivot.getPointOnCircumference(R - 7.f, a);
        g.drawLine({p0, p1}, 1.3f);
    }
    g.setFont(font(true, 15.f, 0.2f));
    g.drawText("VU", juce::Rectangle<float>(90.f, 62.f, 60.f, 20.f), juce::Justification::centred, false);
    g.setColour(juce::Colour(0xffc0392b));
    g.setFont(font(true, 9.f));
    g.drawText("+3", juce::Rectangle<float>(190.f, 24.f, 18.f, 11.f), juce::Justification::centredRight, false);
    g.setColour(juce::Colour(0xff6b7a70));
    g.setFont(font(true, 8.f, 0.25f));
    g.drawText("OUTPUT", juce::Rectangle<float>(74.f, 98.f, 60.f, 10.f), juce::Justification::centred, false);

    {
        juce::Graphics::ScopedSaveState s(g);
        g.reduceClipRegion(face.toNearestInt());
        const float a = a0 - 0.06f + (a1 - a0 + 0.12f) * needle_;
        g.setColour(juce::Colour(0xff1a1a1a));
        g.drawLine({pivot, pivot.getPointOnCircumference(R - 4.f, a)}, 1.8f);
    }
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillEllipse(juce::Rectangle<float>(10.f, 10.f).withCentre(pivot));
}

// ---------------------------------------------------------------------------------------------------------
namespace
{
const char *const kTabNames[3] = {"DELAY", "SPACE / TAPE", "AIR / MOD"};
juce::Rectangle<float> tabRect(int i)
{
    const float w[3] = {104.f, 160.f, 130.f};
    float       x    = 1260.f;
    for(int k = 2; k >= i; k--)
        x -= w[k] + (k < 2 ? 10.f : 0.f);
    return {x, 22.f, w[i], 32.f};
}
} // namespace

Canvas::Canvas(DubStationProcessor &proc) : proc_(proc)
{
    grunge_ = juce::ImageCache::getFromMemory(BinaryData::grunge_png, BinaryData::grunge_pngSize);
    grime_  = juce::ImageCache::getFromMemory(BinaryData::grime_panel_png, BinaryData::grime_panel_pngSize);

    // --- page 1: DELAY ---
    addTitle(0, {20.f, 72.f, 860.f, 238.f}, "DELAY A", false);
    addTitle(0, {20.f, 318.f, 860.f, 238.f}, "DELAY B", false);
    addTitle(0, {892.f, 72.f, 140.f, 484.f}, "SAT (A+B)", true, 20.f);
    addTitle(0, {1044.f, 72.f, 216.f, 484.f}, "OUTPUT", true);
    const int xs[6] = {150, 282, 414, 546, 678, 810};
    addKnob(0, "time", "TIME", xs[0], 202);
    addKnob(0, "feedback", "FEEDBACK", xs[1], 202);
    addKnob(0, "fbfilter", "FILTER", xs[2], 202);
    addKnob(0, "fbreso", "RESONANCE", xs[3], 202);
    addKnob(0, "width", "WIDTH", xs[4], 202);
    addKnob(0, "delay", "LEVEL", xs[5], 202);
    addKnob(0, "time2", "TIME", xs[0], 448);
    addKnob(0, "feedback2", "FEEDBACK", xs[1], 448);
    addKnob(0, "tone2", "FILTER", xs[2], 448);
    addKnob(0, "flanger", "FLANGER", xs[3], 448);
    addKnob(0, "width2", "WIDTH", xs[4], 448);
    addKnob(0, "delay2", "LEVEL", xs[5], 448);
    addKnob(0, "sat", "SATURATION", 962, 314);
    addKnob(0, "duck", "DUCKING", 1152, 180);
    addKnob(0, "mix", "MIX", 1152, 436);

    syncA_ = std::make_unique<SyncLed>(proc, "sync");
    syncB_ = std::make_unique<SyncLed>(proc, "sync2");
    syncA_->setBounds(52 - 28, 202 - 15, 56, 56);
    syncB_->setBounds(52 - 28, 448 - 15, 56, 56);
    addAndMakeVisible(*syncA_);
    addAndMakeVisible(*syncB_);
    panic_ = std::make_unique<PanicButton>(proc);
    panic_->setBounds(1152 - 85, 339 - 33, 170, 66);
    addAndMakeVisible(*panic_);

    // --- page 2: SPACE / TAPE ---
    addTitle(1, {20.f, 72.f, 760.f, 500.f}, "SPACE", true);
    addTitle(1, {800.f, 72.f, 460.f, 500.f}, "TAPE", false);
    addKnob(1, "size", "SIZE", 100, 304);
    addKnob(1, "reverb", "AMOUNT", 245, 304);
    addKnob(1, "damp", "DAMP", 390, 304);
    addKnob(1, "hipass", "HI-PASS", 535, 304);
    addKnob(1, "drift", "DRIFT", 680, 304);
    addKnob(1, "drive", "DRIVE", 880, 304);
    addKnob(1, "warble", "WARBLE", 1025, 304);
    addKnob(1, "degrade", "DEGRADE", 1170, 304);

    // --- page 3: AIR / MOD ---
    addTitle(2, {20.f, 72.f, 730.f, 500.f}, "AMBIENCE", true);
    addTitle(2, {760.f, 72.f, 500.f, 500.f}, "PHASER", false);
    addKnob(2, "noise", "NOISE", 110, 304);
    addKnob(2, "noisefilt", "TONE", 290, 304);
    addKnob(2, "noisemod", "MOTION", 470, 304);
    addKnob(2, "crackle", "CRACKLE", 640, 304);
    addKnob(2, "phzrate", "RATE", 870, 304);
    addKnob(2, "phzdepth", "DEPTH", 1080, 304);
    vu_ = std::make_unique<VuMeter>(proc);
    vu_->setBounds(890, 442, 210, 105);
    addChildComponent(*vu_);

    for(int i = 0; i < 3; i++)
    {
        auto *b = tabs_.add(new juce::TextButton(kTabNames[i]));
        b->setAlpha(0.f); // hit area only; the tabs are painted on the silver strip
        b->setBounds(tabRect(i).toNearestInt());
        b->onClick = [this, i] { showPage(i); };
        addAndMakeVisible(b);
    }
    showPage(0);
}

void Canvas::addKnob(int page, const char *id, const char *label, int cx, int cy)
{
    auto k = std::make_unique<Knob>(proc_, paramIndex(id), label);
    k->setBounds(cx - 66, cy - 48, 132, 128);
    addChildComponent(*k);
    knobs_.push_back(std::move(k));
    knobPage_.push_back(page);
}

void Canvas::addTitle(int page, juce::Rectangle<float> r, juce::String title, bool black, float size)
{
    panels_[page].push_back({r, std::move(title), size, black});
}

void Canvas::showPage(int page)
{
    page_ = page;
    for(size_t i = 0; i < knobs_.size(); i++)
        knobs_[i]->setVisible(knobPage_[i] == page);
    syncA_->setVisible(page == 0);
    syncB_->setVisible(page == 0);
    panic_->setVisible(page == 0);
    vu_->setVisible(page == 2);
    repaint();
}

void Canvas::paint(juce::Graphics &g)
{
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

    // faceplate
    g.fillAll(kFace);
    juce::ColourGradient vignette(juce::Colours::transparentBlack, 640.f, 320.f, juce::Colours::black.withAlpha(0.28f),
                                  0.f, 320.f, true);
    g.setGradientFill(vignette);
    g.fillRect(0, 72, 1280, 500);

    // section fields
    for(const auto &p : panels_[page_])
    {
        if(!p.black)
            continue;
        g.setColour(kBlack);
        g.fillRoundedRectangle(p.r, 8.f);
        juce::Graphics::ScopedSaveState s(g);
        juce::Path clip;
        clip.addRoundedRectangle(p.r, 8.f);
        g.reduceClipRegion(clip);
        g.drawImage(grime_, p.r, juce::RectanglePlacement::stretchToFit);
    }

    // silver strips
    juce::ColourGradient top(juce::Colour(0xffe0e2dc), 0.f, 0.f, juce::Colour(0xffb6b8b2), 0.f, 72.f, false);
    top.addColour(0.5, juce::Colour(0xffcccec8));
    g.setGradientFill(top);
    g.fillRect(0, 0, 1280, 72);
    juce::ColourGradient bot(juce::Colour(0xffb6b8b2), 0.f, 572.f, juce::Colour(0xffd4d6d0), 0.f, 628.f, false);
    bot.addColour(0.5, juce::Colour(0xffcccec8));
    g.setGradientFill(bot);
    g.fillRect(0, 572, 1280, 56);
    g.setColour(juce::Colour(0xff7f817a));
    g.fillRect(0.f, 71.25f, 1280.f, 1.5f);
    g.fillRect(0.f, 571.25f, 1280.f, 1.5f);
    g.setColour(juce::Colours::white.withAlpha(0.45f));
    g.fillRect(0.f, 73.f, 1280.f, 1.f);

    g.drawImage(grunge_, juce::Rectangle<float>(0.f, 0.f, 1280.f, 628.f), juce::RectanglePlacement::stretchToFit);

    // logo
    g.setFont(font(true, 42.f, 0.05f));
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.drawText("DUB STATION", juce::Rectangle<float>(55.f, 6.f, 400.f, 46.f), juce::Justification::centredLeft, false);
    g.setColour(juce::Colour(0xff4a4d48));
    g.drawText("DUB STATION", juce::Rectangle<float>(55.f, 4.8f, 400.f, 46.f), juce::Justification::centredLeft, false);
    g.setFont(font(true, 12.f, 0.32f));
    g.setColour(juce::Colour(0xff2e7d4f));
    g.drawText("BY NACHTAKTIV", juce::Rectangle<float>(57.f, 44.f, 300.f, 16.f), juce::Justification::centredLeft, false);

    // page tabs, engraved into the top strip; the active one gets a lit LED bar
    for(int i = 0; i < 3; i++)
    {
        const auto r      = tabRect(i);
        const bool active = i == page_;
        g.setColour(juce::Colour(active ? 0xff1e2a1f : 0xff8d908a).withAlpha(active ? 0.9f : 0.55f));
        g.fillRoundedRectangle(r, 5.f);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawRoundedRectangle(r.translated(0.f, 1.f), 5.f, 1.f);
        g.setColour(juce::Colour(0xff5d605a));
        g.drawRoundedRectangle(r, 5.f, 1.f);
        g.setFont(font(true, 15.f, 0.12f));
        g.setColour(active ? kMint : juce::Colour(0xff26291f));
        g.drawText(kTabNames[i], r, juce::Justification::centred, false);
    }

    // section titles
    for(const auto &p : panels_[page_])
    {
        g.setColour(kInk);
        g.setFont(font(true, p.titleSize, p.titleSize > 21.f ? 0.2f : 0.06f));
        const float dy = p.black ? 22.f : 30.f;
        g.drawText(p.title, juce::Rectangle<float>(p.r.getX() + (p.titleSize > 21.f ? 20.f : 14.f), p.r.getY() + dy,
                                                   p.r.getWidth() - 20.f, 30.f),
                   juce::Justification::centredLeft, false);
    }
}
} // namespace dub

// ---------------------------------------------------------------------------------------------------------
DubStationEditor::DubStationEditor(DubStationProcessor &p) : AudioProcessorEditor(&p), canvas_(p)
{
    addAndMakeVisible(canvas_);
    canvas_.setBounds(0, 0, kW, kH);
    setResizable(true, true);
    setResizeLimits(kW / 2, kH / 2, kW * 3 / 2, kH * 3 / 2);
    if(auto *c = getConstrainer())
        c->setFixedAspectRatio((double)kW / kH);
    setSize(kW * 4 / 5, kH * 4 / 5);
}

void DubStationEditor::resized()
{
    canvas_.setTransform(juce::AffineTransform::scale((float)getWidth() / kW));
}

void DubStationEditor::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colours::black);
}
