#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <functional>
#include <memory>

// Listens on every MIDI input for one learned note or controller (a foot switch). Callbacks arrive on the message thread.
class MidiPedal : private juce::MidiInputCallback
{
public:
    std::function<void()> onTrigger;
    std::function<void (const juce::String&)> onLearned;

    MidiPedal() = default;
    ~MidiPedal() override { *alive = false; inputs.clear(); }

    void open()
    {
        inputs.clear();
        for (const auto& d : juce::MidiInput::getAvailableDevices())
            if (auto in = juce::MidiInput::openDevice (d.identifier, this)) { in->start(); inputs.push_back (std::move (in)); }
    }
    int getNumInputs() const noexcept { return (int) inputs.size(); }
    void startLearning() { open(); learning = true; }
    void cancelLearning() { learning = false; }
    bool isLearning() const noexcept { return learning.load(); }
    void forget() { type = 0; number = -1; }
    void set (int t, int n) { type = t; number = n; }
    int getType() const noexcept { return type.load(); }
    int getNumber() const noexcept { return number.load(); }
    bool isSet() const noexcept { return type.load() != 0; }
    juce::String describe() const
    {
        if (type.load() == 1) return "note " + juce::MidiMessage::getMidiNoteName (number.load(), true, true, 3);
        if (type.load() == 2) return "controller " + juce::String (number.load());
        return "none";
    }
    static void handleMessage (MidiPedal& p, const juce::MidiMessage& m)
    {
        const bool isNote = m.isNoteOn();
        const bool isCc = m.isController() && m.getControllerValue() >= 64;     // press, not release
        if (! (isNote || isCc)) return;
        const int t = isNote ? 1 : 2, n = isNote ? m.getNoteNumber() : m.getControllerNumber();
        if (p.learning.exchange (false)) { p.type = t; p.number = n; p.fire (true); }
        else if (t == p.type.load() && n == p.number.load())
        {
            // one tap can arrive as several messages (press and release both sent, or the same device listed twice): act once
            const auto now = juce::Time::getMillisecondCounter();
            if (now - p.lastTrigger.load() < 600u) return;
            p.lastTrigger = now;
            p.fire (false);
        }
    }

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override { handleMessage (*this, m); }
    void fire (bool learned)
    {
        auto flag = alive;
        juce::MessageManager::callAsync ([this, flag, learned]
        {
            if (! *flag) return;
            if (learned) { if (onLearned) onLearned (describe()); }
            else if (onTrigger) onTrigger();
        });
    }

    std::vector<std::unique_ptr<juce::MidiInput>> inputs;
    std::atomic<int> type { 0 }, number { -1 };
    std::atomic<bool> learning { false };
    std::atomic<juce::uint32> lastTrigger { 0 };
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};
