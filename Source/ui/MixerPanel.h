#pragma once
#include "TunerCard.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include "audio/AudioEngine.h"
#include "Theme.h"
#include "Undo.h"

// One vertical mixer channel: name, VU, volume fader, pan, mute/solo and the echo/reverb inserts.
// slot < 0 is the master channel (VU + volume only).
class ChannelStrip : public juce::Component
{
public:
    ChannelStrip (AudioEngine& e, int slot, int orderPos);
    int slot() const noexcept { return slotIndex; }
    void tick();
    void syncFromEngine();
    void paint (juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override;

    std::function<void (int)> onChanged;                        // a track value was changed here (rows follow)
    std::function<void (const juce::String&)> onInfo;           // value read-out for the info bar
    UndoPush onUndoable;                                        // master volume undo

private:
    struct FxButton : juce::TextButton
    {
        std::function<void()> onRight;
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) { if (onRight) onRight(); return; }
            juce::TextButton::mouseDown (e);
        }
    };

    // Ableton-style track number button: filled with the track colour while the track is on, dark when muted (toggle ON = muted)
    struct NumberButton : juce::Button
    {
        NumberButton() : juce::Button ({}) { setClickingTogglesState (true); setWantsKeyboardFocus (false); }
        int number = 1;
        juce::Colour colour;
        void paintButton (juce::Graphics& g, bool over, bool) override
        {
            const bool muted = getToggleState();
            Theme::framedTile (g, getLocalBounds().toFloat(), muted ? Theme::raised : (over ? colour.brighter (0.12f) : colour), 2.0f);
            g.setColour (muted ? Theme::dim : (colour.getPerceivedBrightness() > 0.6f ? juce::Colours::black.withAlpha (0.8f) : juce::Colours::white));
            g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
            g.drawText (juce::String (number), getLocalBounds(), juce::Justification::centred);
        }
    };

    bool isMaster() const noexcept { return slotIndex < 0; }
    Track& track() { return engine.tracks[slotIndex]; }
    void info (const juce::String& what, const juce::String& value) { if (onInfo) onInfo (what + ": " + value); }
    void commitMaster();
    void showEchoMenu();

    AudioEngine& engine;
    int slotIndex, pos;
    juce::String shownName;
    float lastMaster = 1.0f;
    bool dragging = false;
    juce::Rectangle<int> wellArea, scaleArea;                   // dark well holding VU, dB scale and fader

    LevelMeter meter;
    juce::Slider fader, pan;
    juce::Label dbLabel;
    NumberButton muteBtn;
    juce::TextButton soloBtn { "S" };
    FxButton echoBtn, reverbBtn;
};

// The card at the bottom right: master channel and the tuner. Track levels live in the track rows (per pattern).
class MixerPanel : public juce::Component
{
public:
    static constexpr int panelH = 206, panelW = 440, pad = 5;
    explicit MixerPanel (AudioEngine& e);

    int preferredHeight() const { return panelH; }
    juce::Rectangle<float> enclosureBounds() const { return getLocalBounds().toFloat().reduced (1.0f); }
    void refreshFromEngine() { master.syncFromEngine(); }
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (const juce::String&)> onInfo;
    UndoPush onUndoable;
    void setTunerRef (int hz) { tuner.setRef (hz); }
    void setOnTunerRefChanged (std::function<void (int)> f) { tuner.onRefChanged = std::move (f); }

private:
    AudioEngine& engine;
    ChannelStrip master;
    TunerCard tuner;
};
