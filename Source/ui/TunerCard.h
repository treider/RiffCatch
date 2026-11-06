#pragma once
#include "Theme.h"
#include "audio/AudioEngine.h"
#include "audio/PitchDetector.h"

// Chromatic tuner for the input of the armed track (or track 1). A scrolling cent ruler, the detected note
// is held under the centre marker, like a stroboscope-free analogue tuner.
class TunerCard : public juce::Component
{
public:
    explicit TunerCard (AudioEngine& e) : engine (e) { buf.resize (analysisSize); }
    ~TunerCard() override { engine.tunerChannel = -1; }

    void setRunning (bool on)
    {
        running = on;
        if (! on) engine.tunerChannel = -1;
    }

    void tick()
    {
        if (! running) return;
        engine.tunerChannel = pickChannel();
        if (++phase & 1) return;                       // analyse at half the timer rate

        engine.readTunerSamples (buf.data(), analysisSize);
        const auto r = det.detect (buf.data(), analysisSize, engine.getSampleRate());
        if (r.valid && r.clarity > 0.8f)
        {
            const float midi = 69.0f + 12.0f * std::log2 (r.freq / refHz);
            if (! active || std::abs (midi - target) > 1.5f) shown = midi;
            target = midi;
            freq = r.freq;
            active = true;
            silent = 0;
        }
        else if (active && ++silent > 14) active = false;   // about a second without a clear note

        shown += (target - shown) * 0.45f;
        if (active || wasActive || std::abs (target - shown) > 0.02f) repaint();     // an idle tuner needs no repaints
        wasActive = active;
    }

    static juce::String noteName (int midi)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return names[((midi % 12) + 12) % 12];
    }
    static int octaveOf (int midi) { return midi / 12 - 1; }
    float getRef() const noexcept { return refHz; }
    void setRef (int hz) { refHz = (float) juce::jlimit (430, 450, hz); repaint(); }
    std::function<void (int)> onRefChanged;      // so the choice can be remembered

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float w = b.getWidth();
        const float cx = w * 0.5f;
        Theme::mixerWellSurround (g, b, Theme::radiusLg, b.reduced (3.0f), Theme::radiusLg - 1.0f);
        g.setColour (Theme::deep);
        g.fillRoundedRectangle (b.reduced (3.0f), Theme::radiusLg - 1.0f);

        const int midiNote = (int) std::lround (shown);
        const float cents = (shown - (float) midiNote) * 100.0f;
        const bool inTune = active && std::abs (cents) <= 5.0f;
        const auto live = inTune ? Theme::green : Theme::accent;

        // header
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.setColour (Theme::dim);
        g.drawText ("TUNER", 14, 10, 80, 14, juce::Justification::left);
        refRect = juce::Rectangle<int> (getWidth() - 94, 8, 82, 18);
        g.drawText ("A = " + juce::String (juce::roundToInt (refHz)) + " Hz", refRect, juce::Justification::right);

        // note name
        const juce::Rectangle<int> noteArea (10, 26, getWidth() - 20, 50);
        if (active)
        {
            g.setColour (live);
            g.setFont (juce::FontOptions (46.0f, juce::Font::bold));
            const auto nm = noteName (midiNote);
            const int nw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), nm);
            g.drawText (nm, noteArea.withWidth (nw + 4).withCentre (noteArea.getCentre().translated (-8, 0)), juce::Justification::centred);
            g.setFont (juce::FontOptions (16.0f));
            g.setColour (live.withAlpha (0.8f));
            g.drawText (juce::String (octaveOf (midiNote)), (int) cx + nw / 2 - 2, noteArea.getBottom() - 24, 24, 18, juce::Justification::left);
        }
        else
        {
            g.setColour (Theme::dim);
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawFittedText ("START PLAYING\nFOR TUNING", noteArea, juce::Justification::centred, 2);
        }

        // ruler: one tick per 10 cents, a note label at every semitone
        const float ppSemi = 66.0f;
        const float yTop = 86.0f, yBase = 116.0f;
        const float half = cx / ppSemi;
        const int k0 = (int) std::floor ((shown - half) * 10.0f) - 1, k1 = (int) std::ceil ((shown + half) * 10.0f) + 1;
        g.setFont (juce::FontOptions (13.0f));
        for (int k = k0; k <= k1; ++k)
        {
            const float m = (float) k / 10.0f;
            const float x = cx + (m - shown) * ppSemi;
            if (x < 10.0f || x > w - 10.0f) continue;
            const bool major = (k % 10) == 0, mid = (k % 5) == 0;
            const float len = major ? 26.0f : (mid ? 16.0f : 10.0f);
            const float edge = 1.0f - juce::jlimit (0.0f, 1.0f, (std::abs (x - cx) - cx * 0.55f) / (cx * 0.45f));
            g.setColour (Theme::text.withAlpha ((major ? 0.85f : 0.4f) * edge * (active ? 1.0f : 0.6f)));
            g.fillRect (x - (major ? 0.9f : 0.6f), yBase - len, major ? 1.8f : 1.2f, len);
            if (major)
            {
                g.setColour (Theme::text.withAlpha (0.65f * edge * (active ? 1.0f : 0.6f)));
                g.drawText (noteName (k / 10), juce::Rectangle<float> (x - 18.0f, yBase + 4.0f, 36.0f, 16.0f), juce::Justification::centred);
            }
        }
        // centre marker: line with a triangle at each end
        g.setColour (active ? live : Theme::dim);
        g.fillRect (cx - 1.0f, yTop - 6.0f, 2.0f, yBase - yTop + 12.0f);
        juce::Path tri;
        tri.addTriangle (cx - 6.0f, yTop - 14.0f, cx + 6.0f, yTop - 14.0f, cx, yTop - 5.0f);
        tri.addTriangle (cx - 6.0f, yBase + 12.0f, cx + 6.0f, yBase + 12.0f, cx, yBase + 3.0f);
        g.fillPath (tri);

        // readout
        g.setColour (active ? Theme::text : Theme::dim);
        g.setFont (juce::FontOptions (30.0f));
        g.drawText (active ? juce::String (freq, 1) + " Hz" : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")) + " Hz",
                    juce::Rectangle<int> (0, 134, getWidth(), 34), juce::Justification::centred);
        g.setColour (Theme::dim);
        g.setFont (juce::FontOptions (14.0f));
        const auto centsText = active ? (cents >= 0.0f ? "+" : "") + juce::String (juce::roundToInt (cents)) + " cents"
                                      : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")) + " cents";
        g.drawText (centsText, juce::Rectangle<int> (0, 166, getWidth(), 20), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! refRect.expanded (4).contains (e.getPosition())) return;
        juce::PopupMenu m;
        for (int hz = 430; hz <= 450; ++hz) m.addItem (hz, "A = " + juce::String (hz) + " Hz", true, hz == juce::roundToInt (refHz));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int id)
        {
            if (id >= 430 && id <= 450) { refHz = (float) id; if (onRefChanged) onRefChanged (id); }
        });
    }

private:
    int pickChannel() const
    {
        int slot = -1;
        for (int s : engine.order) if (engine.tracks[s].armed.load()) { slot = s; break; }
        if (slot < 0 && ! engine.order.empty()) slot = engine.order.front();
        if (slot < 0) return 0;
        const int code = engine.tracks[slot].inputCode.load();
        return code >= 100 ? code - 100 : code;
    }

    static constexpr int analysisSize = 4096;
    AudioEngine& engine;
    PitchDetector det;
    bool wasActive = false;
    std::vector<float> buf;
    bool running = true, active = false;
    int phase = 0, silent = 0;
    float refHz = 440.0f, shown = 69.0f, target = 69.0f, freq = 0.0f;
    juce::Rectangle<int> refRect;
};
