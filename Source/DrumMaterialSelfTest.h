#pragma once
#include "ui/DrumPanel.h"

// Production paint/interaction regressions; no device, settings or project I/O.
// PNG artifacts are explicitly synthetic and cannot replace HWND evidence.
namespace DrumMaterialTest
{
    inline bool equal (const juce::Image& a, const juce::Image& b, juce::Rectangle<int> area = {})
    {
        if (a.getBounds() != b.getBounds()) return false;
        if (area.isEmpty()) area = a.getBounds();
        for (int y = area.getY(); y < area.getBottom(); ++y)
            for (int x = area.getX(); x < area.getRight(); ++x)
                if (a.getPixelAt (x, y) != b.getPixelAt (x, y)) return false;
        return true;
    }
    template <typename Paint>
    juce::Image render (int w, int h, Paint paint)
    {
        juce::Image image (juce::Image::ARGB, w, h, true);
        { juce::Graphics g (image); paint (g); } // flush before accessing pixels
        return image;
    }
    inline void artifact (const juce::Image& image, const juce::String& name)
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("riffcatch-material-selftest");
        if (dir.createDirectory().failed()) return;
        if (auto out = dir.getChildFile ("synthetic-drums-" + name + ".png").createOutputStream())
        {
            out->setPosition (0); out->truncate();
            juce::PNGImageFormat().writeImageToStream (image, *out);
        }
    }
}

inline void drumMaterialSelfTest (const std::function<void (bool, const juce::String&)>& check)
{
    using namespace DrumMaterialTest;
    const Theme::Preset saved { "live", Theme::blue.getARGB(), Theme::purple.getARGB(), Theme::cyan.getARGB(),
                               Theme::bg.getARGB(), Theme::panel.getARGB(), Theme::text.getARGB() };
    const bool legacy = Theme::legacyTracks;
    auto themes = Theme::presets;
    themes.push_back ({ "Neo-Retro", 0xffff780a, 0xfff2ab24, 0xfff6c343, 0xff0b1018, 0xff202630, 0xffdce3ec });
    auto engine = std::make_unique<AudioEngine>();
    engine->prepareForTest (44100.0, 2);
    auto cursor = [&] (juce::int64 position)
    {
        // The real stopped audio callback transfers cursorPos to playPos.
        // Merely setting cursorPos does not exercise the playhead paint path.
        engine->setCursor (position);
        float silence[64] {}, left[64] {}, right[64] {};
        const float* inputs[] { silence, silence };
        float* outputs[] { left, right };
        engine->audioDeviceIOCallbackWithContext (inputs, 2, outputs, 2, 64, {});
    };
    for (const auto& theme : themes)
    {
        Theme::applyPreset (theme, theme.name == "Tokyo Night");
        RiffLookAndFeel laf;
        const auto tag = " - " + theme.name;
        juce::TextButton button;
        button.setLookAndFeel (&laf); button.setSize (80, 28);
        auto paint = [&] (const char* opt, bool enabled, bool over, bool down, bool on)
        {
            button.getProperties().clear(); button.getProperties().set (opt, true); button.setEnabled (enabled);
            return render (80, 28, [&] (juce::Graphics& g)
            { laf.drawButtonBackground (g, button, on ? Theme::accent : Theme::raised, over, down); });
        };
        const auto normal = paint ("drumControl", true, false, false, false);
        check (equal (normal, paint ("bassControl", true, false, false, false))
               && ! equal (normal, paint ("unmarked", true, false, false, false))
               && ! equal (normal, paint ("drumControl", false, false, false, false))
               && ! equal (normal, paint ("drumControl", true, true, false, false))
               && ! equal (normal, paint ("drumControl", true, false, true, false))
               && ! equal (normal, paint ("drumControl", true, false, false, true)),
               "Drums controls: opt-in reuses Bass exactly; normal/disabled/hover/pressed/toggled distinct" + tag);
        juce::ComboBox combo;
        combo.setLookAndFeel (&laf); combo.setSize (80, 28);
        auto drawCombo = [&] (const char* opt, bool enabled, bool down)
        {
            combo.getProperties().clear(); combo.getProperties().set (opt, true); combo.setEnabled (enabled);
            return render (80, 28, [&] (juce::Graphics& g) { laf.drawComboBox (g, 80, 28, down, 0, 0, 0, 0, combo); });
        };
        const auto marked = drawCombo ("drumControl", true, false);
        check (equal (marked, drawCombo ("bassControl", true, false))
               && ! equal (marked, drawCombo ("unmarked", true, false))
               && ! equal (marked, drawCombo ("drumControl", false, false))
               && ! equal (marked, drawCombo ("drumControl", true, true)),
               "Drums combos: production opt-in, disabled and pressed paint paths" + tag);
        button.setLookAndFeel (nullptr); combo.setLookAndFeel (nullptr);

        for (int size : { 26, 32, 38 })
        {
            juce::Slider knob;
            knob.getProperties().set ("drumKnob", true); knob.getProperties().set ("uni", true);
            knob.setColour (juce::Slider::rotarySliderFillColourId, Theme::accent);
            auto draw = [&] (float value)
            {
                return render (size, size, [&] (juce::Graphics& g)
                { laf.drawRotarySlider (g, 0, 0, size, size, value, juce::MathConstants<float>::pi * 1.25f,
                                        juce::MathConstants<float>::pi * 2.75f, knob); });
            };
            const auto half = draw (0.5f), zero = draw (0);
            knob.setColour (juce::Slider::rotarySliderFillColourId, Theme::red);
            const auto zeroRed = draw (0);
            knob.setColour (juce::Slider::rotarySliderFillColourId, Theme::accent);
            knob.setEnabled (false); const auto disabled = draw (0.5f);
            knob.setEnabled (true); knob.getProperties().remove ("drumKnob");
            const auto plain = draw (0.5f);
            check (! equal (half, plain) && ! equal (half, disabled) && equal (zero, zeroRed)
                   && half.getPixelAt (size / 2, 3).getAlpha() > plain.getPixelAt (size / 2, 3).getAlpha(),
                   "Drums knobs: metal/disabled/real arc halo and empty zero size=" + juce::String (size) + tag);
            knob.getProperties().set ("bassVolumeKnob", true);
            const auto bass = draw (0.5f);
            check (size == 26 ? bass.getPixelAt (size / 2, 3).getAlpha() > half.getPixelAt (size / 2, 3).getAlpha()
                              : equal (bass, half),
                   "Drums knobs: small lane halo tighter/weaker; larger face matches frozen Bass helper size=" + juce::String (size) + tag);
        }
        const juce::Rectangle<float> well (10, 10, 180, 70);
        const auto oldWell = render (200, 90, [&] (juce::Graphics& g) { Theme::wellRing (g, well); });
        const auto newWell = render (200, 90, [&] (juce::Graphics& g) { Theme::materialWellRim (g, well); });
        check (equal (oldWell, newWell, well.toNearestInt()) && ! equal (oldWell, newWell)
               && newWell.getPixelAt (80, 6).getAlpha() == 0
               && Theme::materialCoolLight().getBlue() > Theme::materialCoolLight().getRed(),
               "Drums well: unchanged entire interior/gutters, directional 3px surround and independent cool role" + tag);

        engine->drums.clearPattern(); engine->drums.bars = 1; cursor (0);
        DrumPanel drums (*engine);
        drums.setLookAndFeel (&laf); drums.setVisible (true); drums.setSize (1008, 206); drums.setView (8, 2, 0, 8);
        int controls = 0, knobs = 0; bool ranges = true, geometry = true;
        for (auto* c : drums.getChildren())
        {
            if ((bool) c->getProperties().getWithDefault ("drumControl", false)) ++controls;
            if (auto* s = dynamic_cast<juce::Slider*> (c))
            {
                ++knobs;
                const auto tip = s->getTooltip();
                ranges &= (bool) s->getProperties()["drumKnob"] && ! s->getWantsKeyboardFocus() && s->getInterval() == 0.0;
                if (tip == "Drum volume") ranges &= s->getMinimum() == 0 && s->getMaximum() == 1.2 && s->getDoubleClickReturnValue() == 0.8;
                else if (tip == "Lane volume") ranges &= s->getMinimum() == 0 && s->getMaximum() == 1.5 && s->getDoubleClickReturnValue() == 1;
                else if (tip == "Lane pan") ranges &= s->getMinimum() == -1 && s->getMaximum() == 1 && s->getDoubleClickReturnValue() == 0;
                else ranges &= tip == "Cymbal pitch (semitones)" && s->getMinimum() == -12 && s->getMaximum() == 12 && s->getDoubleClickReturnValue() == 0;
                geometry &= tip == "Drum volume" ? s->getWidth() == 32 && s->getHeight() == 32 : s->getWidth() == 26 && s->getHeight() == 26;
            }
        }
        const auto before = engine->drums.snapshot();
        const auto empty = drums.createComponentSnapshot (drums.getLocalBounds());
        check (controls == 8 && knobs == 14 && ranges && geometry && before == engine->drums.snapshot()
               && empty.getPixelAt (2, 100) == Theme::drumAccent(),
               "Drums production: 8 control opt-ins/14 knobs, bounds/ranges/defaults, edge stripe, paint-only invariant" + tag);
        drums.counter().getProperties().set ("drumControl", false);
        const auto oldCounter = drums.counter().createComponentSnapshot (drums.counter().getLocalBounds());
        drums.counter().getProperties().set ("drumControl", true);
        const auto newCounter = drums.counter().createComponentSnapshot (drums.counter().getLocalBounds());
        check (equal (oldCounter, newCounter, { 0, 0, 70, newCounter.getHeight() }) && ! equal (oldCounter, newCounter),
               "Drums library: blue housing/digits exact; only local arrow tiles/shadows change" + tag);
        for (int v = 0; v < 6; ++v) { engine->drums.steps[v][1] = 1; engine->drums.steps[v][2] = 2; engine->drums.voiceMute[v] = false; }
        drums.refreshFromEngine();
        const auto populated = drums.createComponentSnapshot (drums.getLocalBounds());
        const float cellW = 606.0f / 64.0f;
        const int normalX = (int) (394 + 1.5f * cellW), accentX = (int) (394 + 2.5f * cellW), repeatX = (int) (394 + 17.5f * cellW);
        bool cells = true, accents = true, repeats = true;
        for (int v = 0; v < 6; ++v)
        {
            const int y = 42 + 26 * v + 13;
            cells &= empty.getPixelAt (normalX, y) != populated.getPixelAt (normalX, y);
            accents &= populated.getPixelAt (accentX, y) != populated.getPixelAt (accentX, 42 + 26 * v + 3);
            repeats &= populated.getPixelAt (normalX, y).getPerceivedBrightness() > populated.getPixelAt (repeatX, y).getPerceivedBrightness();
        }
        engine->drums.voiceMute[0] = true;
        const auto muted = drums.createComponentSnapshot (drums.getLocalBounds());
        cursor (44100);
        const auto playhead = drums.createComponentSnapshot (drums.getLocalBounds());
        check (cells, "Drums production: all six voice cell layers" + tag);
        check (accents, "Drums production: accent strokes above all six voice cells" + tag);
        check (repeats, "Drums production: all six repeated patterns remain dimmed" + tag);
        check (muted.getPixelAt (normalX, 55) != populated.getPixelAt (normalX, 55),
               "Drums production: voice mute affects real cell layer" + tag);
        check (playhead.getPixelAt (470, 55) == Theme::text && engine->getPosition() == 44100,
               "Drums production: actual callback position paints playhead over cells" + tag);
        check (populated.getPixelAt (697, 55) != populated.getPixelAt (700, 55),
               "Drums production: bar line above unchanged cells" + tag);
        if (theme.name == "Neo-Retro" || theme.name == "Sunset" || theme.name == "Tokyo Night")
            artifact (playhead, theme.name + "-normal-populated-muted-playhead");
        cursor (0); engine->drums.voiceMute[0] = false;
        drums.setExpanded (false); drums.setSize (1008, 42);
        const auto folded = drums.createComponentSnapshot (drums.getLocalBounds());
        check (folded.getPixelAt (2, 20) == Theme::drumAccent() && Theme::materialControl (drums.counter()) && drums.preferredHeight() == 42,
               "Drums folded: original 42px collapse, header opt-ins before early return" + tag);
        drums.setExpanded (true); drums.setSize (608, 238);
        const auto minimum = drums.createComponentSnapshot (drums.getLocalBounds());
        bool bounds = true;
        for (auto* c : drums.getChildren())
            if (c->isVisible() && ! c->getBounds().isEmpty()) bounds &= drums.getLocalBounds().contains (c->getBounds());
        check (bounds && drums.preferredHeight() == 238 && minimum.getPixelAt (2, 100) == Theme::drumAccent(),
               "Drums minimum: shared wrapped header, intact 26px lanes and contained children" + tag);
        if (theme.name == "Neo-Retro") artifact (minimum, "Neo-Retro-minimum-populated");

        // Real cell gesture and undo through the production callback, at minimum width.
        engine->drums.clearPattern();
        juce::UndoManager undo;
        drums.onUndoable = [&] (const juce::String& name, std::function<void()> apply, std::function<void()> revert)
        { undo.beginNewTransaction (name); undo.perform (new LambdaAction (std::move (apply), std::move (revert))); };
        const juce::Point<float> pos (399, (float) drums.bodyTop() + 13);
        const juce::MouseEvent event (juce::Desktop::getInstance().getMainMouseSource(), pos,
                                      juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier),
                                      1, 0, 0, 0, 0, &drums, &drums, juce::Time::getCurrentTime(), pos, juce::Time::getCurrentTime(), 1, false);
        drums.mouseDown (event); drums.mouseUp (event);
        const auto edited = engine->drums.snapshot();
        undo.undo(); const bool cleared = ! engine->drums.hasAnySteps();
        undo.redo();
        check (edited.steps[0][1] == 2 && cleared && engine->drums.snapshot() == edited,
               "Drums minimum: real Shift-click accent cell mapping and undo/redo retained" + tag);
        int laneVolume = 0, lanePan = 0; bool callbacks = true;
        for (auto* c : drums.getChildren())
            if (auto* s = dynamic_cast<juce::Slider*> (c))
            {
                const auto tip = s->getTooltip();
                const auto old = s->getValue();
                const double value = tip == "Cymbal pitch (semitones)" ? 7 : 0.37;
                s->setValue (value, juce::sendNotificationSync);
                if (tip == "Drum volume") callbacks &= std::abs (engine->drums.volume.load() - value) < 0.00001;
                else if (tip == "Lane volume") callbacks &= std::abs (engine->drums.level[laneVolume++].load() - value) < 0.00001;
                else if (tip == "Lane pan") callbacks &= std::abs (engine->drums.pan[lanePan++].load() - value) < 0.00001;
                else callbacks &= std::abs (engine->drums.cymbalPitch.load() - value) < 0.00001;
                s->setValue (old, juce::sendNotificationSync);
            }
        check (callbacks && laneVolume == 6 && lanePan == 6,
               "Drums controls: actual volume, six lane volume/pan and cymbal pitch callbacks unchanged" + tag);
        engine->drums.bars = 8; drums.refreshFromEngine(); drums.resized();
        juce::TextButton* next = nullptr; juce::TextButton* prev = nullptr;
        for (auto* c : drums.getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (c))
            {
                if (b->getButtonText() == ">") next = b;
                if (b->getButtonText() == "<") prev = b;
            }
        const bool firstPage = next != nullptr && prev != nullptr && next->isVisible() && next->isEnabled()
                            && prev->isVisible() && ! prev->isEnabled();
        if (next != nullptr) next->onClick();
        check (firstPage && ! next->isEnabled() && prev->isEnabled()
               && Theme::materialControl (*next) && Theme::materialControl (*prev),
               "Drums pages: original visibility/enabling/callback and local material opt-ins" + tag);
        drums.onUndoable = {};
        drums.setLookAndFeel (nullptr);
    }
    Theme::applyPreset (saved, legacy);
}
