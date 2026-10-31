#pragma once
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

    LevelMeter meter;
    juce::Slider fader, pan;
    juce::Label dbLabel;
    juce::TextButton muteBtn { "M" }, soloBtn { "S" };
    FxButton echoBtn, reverbBtn;
};

class MixerPanel : public juce::Component
{
public:
    static constexpr int headerH = 26, stripH = 238;
    explicit MixerPanel (AudioEngine& e);

    int preferredHeight() const { return expanded ? headerH + stripH + 8 : headerH; }
    bool isExpanded() const noexcept { return expanded; }
    void setExpanded (bool b);
    void refreshFromEngine();
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (int)> onTrackChanged;
    std::function<void (const juce::String&)> onInfo;
    UndoPush onUndoable;
    std::function<void()> onLayoutChanged;

private:
    struct Chevron : juce::Button
    {
        Chevron() : juce::Button ({}) { setWantsKeyboardFocus (false); }
        bool open = true;
        void paintButton (juce::Graphics& g, bool over, bool) override
        {
            g.setColour (over ? Theme::text : Theme::dim);
            const auto c = getLocalBounds().toFloat().getCentre();
            juce::Path p;
            if (open) p.addTriangle (c.x - 5.0f, c.y - 2.5f, c.x + 5.0f, c.y - 2.5f, c.x, c.y + 3.5f);
            else      p.addTriangle (c.x - 2.5f, c.y - 5.0f, c.x - 2.5f, c.y + 5.0f, c.x + 3.5f, c.y);
            g.fillPath (p);
        }
    };

    void rebuild();
    void layoutStrips();

    AudioEngine& engine;
    bool expanded = true;
    std::vector<int> shownOrder;
    Chevron chevron;
    juce::Label title;
    juce::Viewport stripView;
    juce::Component holder;
    juce::OwnedArray<ChannelStrip> strips;
    ChannelStrip master;
};
