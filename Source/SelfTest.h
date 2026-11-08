#pragma once
#include "audio/AudioEngine.h"
#include "project/Exporter.h"
#include "ui/Undo.h"
#include "audio/DrumTemplates.h"
#include "audio/BassTemplates.h"
#include "audio/PitchDetector.h"
#include "ui/BassPanel.h"
#include "audio/BassSuggest.h"
#include "project/AudioImport.h"
#include "ui/ThemeStore.h"
#include "ui/TunerCard.h"
#include "project/ProjectIO.h"
#include "ui/SeqTabs.h"
#include "ui/MixerPanel.h"
#include "MainComponent.h"
#include "DrumMaterialSelfTest.h"
#include "BassLightPocSelfTest.h"

// kept out of runEngineSelfTest: that function's stack frame is already large
inline void patternSelfTest (const std::function<void (bool, const juce::String&)>& check)
{
    auto peP = std::make_unique<AudioEngine>(); auto& pe = *peP;
    pe.prepareForTest (44100.0, 2);
    pe.addTrack(); pe.addTrack();
    juce::AudioBuffer<float> a (2, 44100); a.clear(); a.setSample (0, 10, 0.5f);
    pe.tracks[0].setAudio (std::move (a));
    pe.tracks[0].offset = 123;
    pe.tracks[0].name = "Guitar";
    pe.tracks[1].volume = 0.7f;
    pe.drums.steps[0][0] = 1;
    pe.drums.volume = 0.6f;
    pe.bass.steps[0] = BassMachine::pack (3, 4, false, false);
    pe.loopBars = 2;
    auto& bank = pe.patterns;
    bank.storeCurrent (pe);
    const int n = bank.addBlank (pe);
    check (n == 1 && bank.numUsed() == 2, "pattern: a new pattern is added");
    bank.current = n; bank.loadCurrent (pe);
    check (bank.pats[(size_t) n].drumRef == bank.pats[0].drumRef && bank.pats[(size_t) n].bassRef == bank.pats[0].bassRef, "pattern: a new pattern starts on the same drum and bass patterns");
    bank.selectItem (pe, PatternBank::Drums, 1); bank.selectItem (pe, PatternBank::Bass, 1);       // give it its own
    check (pe.order.size() == 1 && pe.tracks[pe.order[0]].length.load() == 0 && pe.drums.steps[0][0].load() == 0 && ! pe.bass.hasAnySteps(),
           "pattern: the new pattern has one empty track and no notes");
    for (int i = 0; i < 4; ++i) pe.addTrack();
    juce::AudioBuffer<float> b (2, 22050); b.clear(); b.setSample (1, 5, 0.25f);
    pe.tracks[pe.order[1]].setAudio (std::move (b));
    pe.tracks[pe.order[0]].volume = 0.3f;
    pe.tracks[pe.order[0]].name = "Chorus gtr";
    pe.drums.steps[1][4] = 2;
    pe.drums.volume = 0.9f;
    pe.loopBars = 3;
    bank.pats[(size_t) n].name = "Chorus";
    bank.storeCurrent (pe);
    bank.current = 0; bank.loadCurrent (pe);
    check (pe.order.size() == 2 && pe.tracks[pe.order[0]].length.load() == 44100 && std::abs (pe.tracks[pe.order[0]].data.getSample (0, 10) - 0.5f) < 1.0e-6f
           && pe.tracks[pe.order[0]].offset.load() == 123 && pe.tracks[pe.order[0]].name == "Guitar" && std::abs (pe.tracks[pe.order[1]].volume.load() - 0.7f) < 1.0e-6f
           && pe.drums.steps[0][0].load() == 1 && pe.drums.steps[1][4].load() == 0 && std::abs (pe.drums.volume.load() - 0.6f) < 1.0e-6f
           && pe.bass.hasAnySteps() && pe.loopBars.load() == 2, "pattern: switching back restores the tracks, mix, drums, bass and loop length");

    auto pdir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("riffcatch-pattern-test");
    pdir.deleteRecursively();
    juce::String perr;
    check (ProjectIO::save (pe, ProjectIO::fileInFolder (pdir), perr), "pattern: project saved " + perr);
    auto pe2P = std::make_unique<AudioEngine>(); auto& pe2 = *pe2P;
    pe2.prepareForTest (44100.0, 2);
    check (ProjectIO::load (pe2, ProjectIO::fileInFolder (pdir), perr), "pattern: project loaded " + perr);
    auto& b2 = pe2.patterns;
    check (b2.numUsed() == 2 && b2.current == 0 && b2.pats[1].name == "Chorus" && b2.pats[1].loopBars == 3 && b2.pats[1].tracks.size() == 5
           && b2.pats[1].tracks[1].audio.getNumSamples() == 22050 && std::abs (b2.pats[1].tracks[1].audio.getSample (1, 5) - 0.25f) < 1.0e-5f
           && std::abs (b2.pats[1].tracks[0].volume - 0.3f) < 1.0e-4f && b2.pats[1].tracks[0].name == "Chorus gtr"
           && b2.drumLib[(size_t) b2.pats[1].drumRef].steps[1][4] == 2 && std::abs (b2.pats[1].drumMix.volume - 0.9f) < 1.0e-4f && pe2.order.size() == 2,
           "pattern: patterns with their own tracks round-trip through the project file");
    b2.storeCurrent (pe2);
    b2.current = 1; b2.loadCurrent (pe2);
    check (pe2.order.size() == 5 && pe2.tracks[pe2.order[1]].length.load() == 22050 && pe2.drums.steps[1][4].load() == 2 && pe2.loopBars.load() == 3,
           "pattern: the second pattern has 5 tracks and its own content");
    b2.current = 0; b2.loadCurrent (pe2);
    check (pe2.order.size() == 2 && pe2.tracks[pe2.order[0]].length.load() == 44100, "pattern: back to the first pattern's 2 tracks");
    bank.clearPattern (pe, 0);
    check (pe.order.size() == 2 && pe.tracks[pe.order[0]].length.load() == 0 && pe.drums.hasAnySteps(), "pattern: clearing empties the audio clips but keeps the tracks and the shared drum and bass patterns");
    check (bank.remove (pe, 1) && bank.numUsed() == 1 && ! bank.remove (pe, 0), "pattern: the last pattern cannot be deleted");
    {   // libraries: shared drum and bass patterns
        auto p3P = std::make_unique<AudioEngine>(); auto& p3 = *p3P;
        p3.prepareForTest (44100.0, 2);
        p3.addTrack();
        p3.drums.steps[0][0] = 1;
        auto& bk = p3.patterns;
        bk.storeCurrent (p3);
        const int d2 = bk.newItem (p3, PatternBank::Drums, false);
        check (d2 == 1 && bk.currentRef (PatternBank::Drums) == 1 && ! p3.drums.hasAnySteps(), "library: a new drum pattern is empty and selected");
        p3.drums.steps[2][3] = 2;
        bk.selectItem (p3, PatternBank::Drums, 0);
        check (p3.drums.steps[0][0].load() == 1 && p3.drums.steps[2][3].load() == 0 && bk.drumLib[1].steps[2][3] == 2, "library: switching keeps each drum pattern's own notes");
        const int np = bk.addBlank (p3);
        check (np == 1 && bk.pats[1].drumRef == bk.pats[0].drumRef && bk.pats[1].bassRef == bk.pats[0].bassRef, "library: a new pattern starts on the same drum and bass patterns");
        bk.pats[1].drumRef = 0;
        check (bk.usedBy (PatternBank::Drums, 0).size() == 2 && ! bk.deleteItem (PatternBank::Drums, 0), "library: a drum pattern used by two patterns is shared and cannot be deleted");
        const int dup = bk.duplicate (p3, 0);
        check (dup >= 0 && bk.pats[(size_t) dup].drumRef == bk.pats[0].drumRef, "library: duplicating a pattern shares its drum pattern");
        check (bk.deleteItem (PatternBank::Drums, 1) && ! bk.itemUsed (PatternBank::Drums, 1), "library: an unused drum pattern can be deleted");
        bk.current = 1; bk.loadCurrent (p3);
        check (p3.drums.steps[0][0].load() == 1, "library: the second pattern plays the shared drum pattern");
        {   // the counter: a number that does not exist starts empty, an empty unplayed one disappears
            auto p4P = std::make_unique<AudioEngine>(); auto& p4 = *p4P;
            p4.prepareForTest (44100.0, 2);
            p4.addTrack();
            p4.drums.steps[0][0] = 1;
            auto& b4 = p4.patterns;
            b4.storeCurrent (p4);
            b4.selectItem (p4, PatternBank::Drums, 1);
            check (b4.itemUsed (PatternBank::Drums, 1) && ! p4.drums.hasAnySteps() && b4.currentRef (PatternBank::Drums) == 1, "counter: counting up to a new number starts an empty pattern");
            b4.selectItem (p4, PatternBank::Drums, 0);
            check (! b4.itemUsed (PatternBank::Drums, 1) && p4.drums.steps[0][0].load() == 1, "counter: leaving an empty, unplayed pattern removes it");
            b4.selectItem (p4, PatternBank::Drums, 3);
            p4.drums.steps[1][1] = 1;
            b4.selectItem (p4, PatternBank::Drums, 0);
            check (b4.itemUsed (PatternBank::Drums, 3) && b4.drumLib[3].steps[1][1] == 1, "counter: a pattern with notes stays in the library");
        }    }    pdir.deleteRecursively();
}
inline void songSelfTest (const std::function<void (bool, const juce::String&)>& check)
{
    {   // song: patterns rendered one after the other
        auto p5P = std::make_unique<AudioEngine>(); auto& p5 = *p5P;
        p5.prepareForTest (44100.0, 2);
        p5.bpm = 120.0; p5.loopBars = 1;
        p5.addTrack();
        juce::AudioBuffer<float> a1 (2, (int) p5.getBarLength()); a1.clear();
        for (int i = 0; i < a1.getNumSamples(); ++i) { a1.setSample (0, i, 0.25f); a1.setSample (1, i, 0.25f); }
        p5.tracks[0].setAudio (std::move (a1));
        auto& b5 = p5.patterns;
        b5.storeCurrent (p5);
        const int second = b5.addBlank (p5);
        b5.current = second; b5.loadCurrent (p5);
        juce::AudioBuffer<float> a2 (2, (int) p5.getBarLength()); a2.clear();
        for (int i = 0; i < a2.getNumSamples(); ++i) { a2.setSample (0, i, 0.5f); a2.setSample (1, i, 0.5f); }
        p5.tracks[p5.order[0]].setAudio (std::move (a2));
        b5.song = { { 0, 1 }, { second, 2 } };
        juce::AudioBuffer<float> sm;
        juce::String serr;
        const bool okSong = Exporter::renderSongBuffer (p5, sm, serr, false);
        const int bar = (int) p5.getBarLength();
        check (okSong && sm.getNumSamples() >= 3 * bar && sm.getNumSamples() < 3 * bar + 44100 * 2, "song: length is the sum of the patterns' passes (" + juce::String (sm.getNumSamples()) + ")");
        check (okSong && std::abs (sm.getSample (0, bar / 2) - 0.25f * 0.70711f) < 1.0e-3f && std::abs (sm.getSample (0, bar + bar / 2) - 0.5f * 0.70711f) < 1.0e-3f && std::abs (sm.getSample (1, 2 * bar + bar / 2) - 0.5f * 0.70711f) < 1.0e-3f,
               "song: pattern 1 plays first, then pattern 2 twice");
        auto buf = std::make_shared<juce::AudioBuffer<float>> (sm);
        p5.setSong (buf);
        p5.stop();
        float zin[1] = { 0.0f }; const float* zi[2] = { zin, zin };
        std::vector<float> o0 (1024), o1 (1024); float* zo[2] = { o0.data(), o1.data() };
        juce::AudioIODeviceCallbackContext ctx;
        p5.seek (bar + 100);
        p5.play();
        p5.audioDeviceIOCallbackWithContext (zi, 1, zo, 2, 1024, ctx);
        check (p5.getState() == AudioEngine::Playing && std::abs (o0[10] - 0.5f * 0.70711f) < 0.02f, "song: the transport plays the rendered song from the cursor");
        p5.seek (3 * bar - 10);
        p5.audioDeviceIOCallbackWithContext (zi, 1, zo, 2, 1024, ctx);
        p5.clearSong();
        check (! p5.songLoaded(), "song: can be cleared");
    }
}

// Offscreen regression only: no window, settings, captures or audio-device access.
inline void bassKnobSelfTest (BassPanel& panel, const std::function<void (bool, const juce::String&)>& check)
{
    RiffLookAndFeel laf;
    const juce::StringArray names { "Sub", "Cutoff", "Reso", "Env", "Decay", "Sustain", "Glide", "Drive" };
    const double lo[] { 0.0, 30.0, 0.0, 0.0, 0.03, 0.0, 0.0, 0.0 };
    const double hi[] { 1.0, 12000.0, 0.97, 1.0, 1.5, 1.0, 300.0, 1.0 };
    const double defaults[] { 0.5, 400.0, 0.2, 0.4, 0.3, 0.7, 0.0, 0.2 };
    int marked = 0, unmarked = 0;
    bool scopeOK = true, parametersOK = true;
    for (auto* child : panel.getChildren())
        if (auto* s = dynamic_cast<juce::Slider*> (child))
        {
            const bool optedIn = (bool) s->getProperties().getWithDefault ("bassSoundKnob", false);
            const int i = names.indexOf (s->getTooltip());
            scopeOK &= optedIn == (i >= 0);
            if (optedIn) ++marked; else ++unmarked;
            if (i < 0) continue;
            const auto value = s->getValue();
            juce::Image image (juce::Image::ARGB, 38, 38, true);
            juce::Graphics g (image);
            panel.lookAndFeelChanged();
            laf.drawRotarySlider (g, 0, 0, 38, 38, (float) s->valueToProportionOfLength (value),
                                  juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, *s);
            parametersOK &= s->getValue() == value && s->getMinimum() == lo[i] && s->getMaximum() == hi[i]
                         && s->getDoubleClickReturnValue() == defaults[i] && ! s->getWantsKeyboardFocus()
                         && s->getWidth() == 38 && s->getHeight() == 38;
            if (i == 1) parametersOK &= std::abs (s->proportionOfLengthToValue (0.5) - 500.0) < 0.001;
        }
    check (scopeOK && marked == 8 && unmarked == 1, "bass knobs: only eight sound sliders opt in, not volume");
    check (parametersOK, "bass knobs: painting/restyle preserve values, ranges, resets, Cutoff curve, size and focus policy");

    const Theme::Preset saved { "live", Theme::blue.getARGB(), Theme::purple.getARGB(), Theme::cyan.getARGB(),
                               Theme::bg.getARGB(), Theme::panel.getARGB(), Theme::text.getARGB() };
    const bool legacy = Theme::legacyTracks;
    const int selected = Theme::current;
    juce::Slider s;
    s.setRange (0.0, 1.0);
    s.setValue (0.5, juce::dontSendNotification);
    s.getProperties().set ("uni", true);
    int callbacks = 0;
    s.onValueChange = [&] { ++callbacks; };
    auto render = [&] (float proportion = 0.5f)
    {
        juce::Image image (juce::Image::ARGB, 38, 38, true);
        juce::Graphics g (image);
        laf.drawRotarySlider (g, 0, 0, 38, 38, proportion,
                              juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, s);
        return image;
    };
    for (int t = 0; t < Theme::builtinCount + 2; ++t)
    {
        const auto preset = t < Theme::builtinCount ? Theme::presets[(size_t) t]
            : Theme::generate ("test custom", juce::Colour (0xff2563eb), juce::Colour (0xff06b6d4), t == Theme::builtinCount);
        Theme::applyPreset (preset, t == 0);
        s.setColour (juce::Slider::rotarySliderFillColourId, Theme::cyan);
        s.getProperties().remove ("bassSoundKnob");
        const auto plain = render();
        s.getProperties().set ("bassSoundKnob", false);
        const auto optedOut = render();
        s.getProperties().set ("bassSoundKnob", true);
        const auto metal = render();
        s.setEnabled (false);
        const auto disabled = render();
        s.setEnabled (true);
        bool legacyOK = true, outsideOK = true, faceDiffers = false, disabledDiffers = false;
        for (int y = 0; y < 38; ++y)
            for (int x = 0; x < 38; ++x)
            {
                legacyOK &= plain.getPixelAt (x, y) == optedOut.getPixelAt (x, y);
                const bool differs = plain.getPixelAt (x, y) != metal.getPixelAt (x, y);
                faceDiffers |= differs;
                disabledDiffers |= disabled.getPixelAt (x, y) != metal.getPixelAt (x, y);
                // v0.71 deliberately lights the value arc and adds a bounded shadow.
                if (juce::Point<float> (x + 0.5f, y + 0.5f).getDistanceFrom ({ 19.0f, 19.0f }) > 18.5f)
                    outsideOK &= ! differs;
            }
        check (legacyOK && outsideOK && faceDiffers && disabledDiffers && metal.getPixelAt (19, 13) == Theme::text,
               "bass knob rendering: opt-out, bounded halo/shadow, face, disabled state and pointer - " + preset.name);
        const auto zero = render (0.0f);
        s.setColour (juce::Slider::rotarySliderFillColourId, Theme::red);
        const auto zeroOtherColour = render (0.0f);
        bool zeroOK = true;
        for (int y = 0; y < 38; ++y)
            for (int x = 0; x < 38; ++x)
                zeroOK &= zero.getPixelAt (x, y) == zeroOtherColour.getPixelAt (x, y);
        s.setColour (juce::Slider::rotarySliderFillColourId, Theme::cyan);
        s.getProperties().remove ("bassSoundKnob");
        s.getProperties().set ("bassVolumeKnob", true);
        const auto volume = render();
        bool volumeOK = true;
        for (int y = 0; y < 38; ++y)
            for (int x = 0; x < 38; ++x)
                volumeOK &= volume.getPixelAt (x, y) == metal.getPixelAt (x, y);
        s.getProperties().remove ("bassVolumeKnob");
        check (zeroOK, "bass arcs: no fabricated zero arc - " + preset.name);
        check (volumeOK, "bass arcs: volume shares sound-knob glow - " + preset.name);
        check (metal.getPixelAt (31, 19) == plain.getPixelAt (31, 19)
               && metal.getPixelAt (19, 3).getAlpha() >= 20
               && metal.getPixelAt (19, 3).getAlpha() > plain.getPixelAt (19, 3).getAlpha() + 15
               && metal.getPixelAt (19, 3).getSaturation() > 0.20f,
               "bass arcs: unlit arc retained; visible coloured halo outside solid stroke - " + preset.name);
        check (Theme::bassKnobLight().getPerceivedBrightness() > Theme::bassKnobShade().getPerceivedBrightness()
               && std::abs (Theme::bassKnobLabel().getPerceivedBrightness() - Theme::panel.getPerceivedBrightness())
                  > std::abs (Theme::dim.getPerceivedBrightness() - Theme::panel.getPerceivedBrightness())
               && Theme::blue.getARGB() == preset.primary && Theme::cyan.getARGB() == preset.accent
               && Theme::panel.getARGB() == preset.surface && Theme::text.getARGB() == preset.text
               && Theme::current == selected && s.getValue() == 0.5 && callbacks == 0,
               "bass knob roles: live theme/label contrast, palette and value preserved - " + preset.name);
    }
    Theme::applyPreset (saved, legacy);
}

inline void bassLightingSelfTest (const std::function<void (bool, const juce::String&)>& check)
{
    const Theme::Preset saved { "live", Theme::blue.getARGB(), Theme::purple.getARGB(), Theme::cyan.getARGB(),
                               Theme::bg.getARGB(), Theme::panel.getARGB(), Theme::text.getARGB() };
    const bool legacy = Theme::legacyTracks;
    auto grey = [] (juce::Colour c) { return (c.getRed() * 299 + c.getGreen() * 587 + c.getBlue() * 114) / 1000; };
    const Theme::Preset lightingThemes[] {
        Theme::presets[2], Theme::presets[0],
        { "Neo-Retro", 0xffff780a, 0xfff2ab24, 0xfff6c343, 0xff0b1018, 0xff202630, 0xffdce3ec }
    };
    for (const auto& preset : lightingThemes)
    {
        Theme::applyPreset (preset, preset.name == "Tokyo Night");
        const auto name = preset.name;
        for (bool tab : { false, true })
        {
            const int width = tab ? 124 : 608, height = tab ? 28 : 236;
            juce::Image image (juce::Image::ARGB, width, height, true);
            {
                juce::Graphics g (image);
                Theme::bassEnclosure (g, { 0.0f, 0.0f, (float) width, (float) height }, tab);
            } // Flush the native renderer before sampling.
            const int hot = grey (image.getPixelAt (12, 1));
            const int contact = grey (image.getPixelAt (12, 3));
            const int far = grey (image.getPixelAt (width - 16, 1));
            check (hot - contact >= 40 && (tab ? std::abs (hot - far) <= 3 : hot - far >= 40),
                   "bass lighting: core/contact >=40, tab sustained / panel far subdued - " + name + (tab ? " tab " : " panel ")
                   + juce::String (hot) + "/" + juce::String (contact) + "/" + juce::String (far));
            check (image.getPixelAt (0, height / 2).getAlpha() == 0,
                   "bass lighting: enclosure retains outer bounds - " + name + (tab ? " tab" : " panel"));
            // Check the actual halo band hole, not just a difference from an old
            // image. Its inset-6 corner must curve between (6,6) and (9,9).
            const auto edge = juce::Rectangle<float> (0, 0, (float) width, (float) height);
            const auto band = Theme::bassEnclosureBand (edge, 6.0f, tab);
            check (band.contains (6.25f, 6.25f) && band.contains (6.5f, 7.0f)
                   && ! band.contains (8.5f, 8.5f) && ! band.contains (12.0f, 6.25f)
                   && band.contains (12.0f, 5.75f) && ! band.contains (0.25f, 0.25f),
                   "bass corners: rounded halo hole, straight-edge depth and outer radius - " + name + (tab ? " tab" : " panel"));
            juce::Image mask (juce::Image::ARGB, width, height, true);
            {
                juce::Graphics mg (mask);
                mg.setColour (juce::Colours::white);
                mg.fillPath (band);
            }
            const auto curveAlpha = mask.getPixelAt (6, 7).getAlpha();
            check (mask.getPixelAt (5, 7).getAlpha() == 255 && curveAlpha > 0 && curveAlpha < 255
                   && mask.getPixelAt (9, 9).getAlpha() == 0 && mask.getPixelAt (12, 5).getAlpha() == 255,
                   "bass corners: antialiased curved inner transition, not square/blurred - " + name + (tab ? " tab" : " panel"));
            if (! tab)
            {
                bool solid = Theme::bassLeftRibbon() == Theme::bassAccent();
                for (int y = 8; y < height - 8; ++y)
                    for (int x = 1; x < 5; ++x)
                        solid &= image.getPixelAt (x, y) == Theme::bassAccent();
                check (solid && image.getPixelAt (5, height / 2) != Theme::bassAccent()
                       && grey (image.getPixelAt (4, height / 2)) - grey (image.getPixelAt (5, height / 2)) >= 40
                       && std::abs (grey (image.getPixelAt (8, height / 2))
                                    - grey (image.getPixelAt (14, height / 2))) <= 3,
                       "bass stripe: tuner-style 4px full-opacity live Secondary, full height and no left halo - " + name);
                juce::Image stripeMask (juce::Image::ARGB, width, height, true);
                {
                    juce::Graphics g (stripeMask);
                    juce::Path card;
                    const auto b = image.getBounds().toFloat().reduced (1.0f);
                    card.addRoundedRectangle (b, Theme::radiusLg);
                    g.reduceClipRegion (card);
                    g.setColour (Theme::bassAccent());
                    g.fillPath (Theme::bassStripePath (b));
                }
                bool corners = true;
                for (int y : { 1, 2, 3, 4, 5, height - 6, height - 5, height - 4, height - 3, height - 2 })
                    for (int x = 1; x < 5; ++x)
                    {
                        const auto coverage = stripeMask.getPixelAt (x, y).getAlpha();
                        const auto pixel = image.getPixelAt (x, y);
                        // Existing material beneath the stripe also contributes
                        // at antialiased pixels; compare footprint and solid interior.
                        if (coverage == 255) corners &= pixel == Theme::bassAccent();
                    }
                const auto stripe = Theme::bassStripePath (image.getBounds().toFloat().reduced (1.0f));
                corners &= ! stripe.contains (4.9f, 1.1f) && stripe.contains (4.9f, 3.1f)
                           && ! stripe.contains (4.9f, height - 1.1f) && stripe.contains (4.9f, height - 3.1f)
                           && stripe.contains (4.99f, 7.0f) && ! stripe.contains (5.01f, 7.0f);
                check (corners && image.getPixelAt (0, 1).getAlpha() == 0
                       && stripeMask.getPixelAt (4, 1).getAlpha() < stripeMask.getPixelAt (4, 3).getAlpha(),
                       "bass stripe: tangent radius-2 inner ends, outer radius-6 clip, unchanged 4px middle - " + name);
                const int y = height - 4;
                const int peakX = juce::roundToInt (13.0f + (width * 0.23f - 12.0f) * 0.12f);
                int largestStep = 0;
                for (int x = 11; x <= peakX + 2; ++x)
                    largestStep = juce::jmax (largestStep, std::abs (grey (image.getPixelAt (x, y))
                                                                   - grey (image.getPixelAt (x - 1, y))));
                const int start = grey (image.getPixelAt (12, y));
                const int peak = grey (image.getPixelAt (peakX, y));
                const int end = grey (image.getPixelAt (juce::roundToInt (width * 0.23f), y));
                check (largestStep <= 4 && peak - start >= 10 && peak - end >= 10,
                       "bass reflection: gradual dark/light start and preserved fading highlight - " + name
                       + " step/start/peak/end=" + juce::String (largestStep) + "/" + juce::String (start)
                       + "/" + juce::String (peak) + "/" + juce::String (end));
            }
            else
            {
                check (grey (image.getPixelAt (width / 2, 4)) > grey (image.getPixelAt (width / 2, 12)) + 25
                       && grey (image.getPixelAt (width / 2, 7)) > grey (image.getPixelAt (width / 2, 12)) + 8,
                       "bass tab: visible top halo beyond the solid core, bounded before caption - " + name);
            }
            auto shadow = [&] (bool enabled, bool down)
            {
                juce::Image image (juce::Image::ARGB, 80, 48, true);
                juce::Graphics g (image);
                Theme::bassControlShadow (g, { 8.0f, 8.0f, 64.0f, 28.0f }, enabled, down);
                return image;
            };
            const auto normalShadow = shadow (true, false), pressedShadow = shadow (true, true), disabledShadow = shadow (false, false);
            check (normalShadow.getPixelAt (40, 36).getAlpha() > pressedShadow.getPixelAt (40, 36).getAlpha()
                   && pressedShadow.getPixelAt (40, 36).getAlpha() > disabledShadow.getPixelAt (40, 36).getAlpha()
                   && normalShadow.getPixelAt (40, 36).getAlpha() > normalShadow.getPixelAt (40, 39).getAlpha()
                   && normalShadow.getPixelAt (40, 41).getAlpha() == 0,
                   "bass shadows: downward falloff, weaker pressed/disabled, bounded extent - " + name);
        }
        SeqTabs activeBass (1, Theme::cyan);
        activeBass.setSize (251, 28);
        juce::Image actualTab (juce::Image::ARGB, 251, 28, true);
        juce::Image bareTab (juce::Image::ARGB, 251, 28, true);
        {
            juce::Graphics g (actualTab), base (bareTab);
            activeBass.paint (g);
            Theme::tabFace (base, { 127.0f, 0.0f, 124.0f, 28.0f }, Theme::bassTabRibbon(), true);
        }
        bool noReflection = true;
        for (int y = 24; y <= 26; ++y)
            for (int x = 162; x <= 216; ++x)
                noReflection &= actualTab.getPixelAt (x, y) == bareTab.getPixelAt (x, y);
        check (noReflection, "bass tab: no separate reflection beneath label in actual SeqTabs paint - " + name);
        auto tile = [&] (bool enabled, bool over, bool down, bool focus, int width = 104)
        {
            juce::Image image (juce::Image::ARGB, width, 28, true);
            juce::Graphics g (image);
            Theme::bassControlTile (g, { 0.0f, 0.0f, (float) width, 28.0f }, Theme::raised, 2.0f, enabled, over, down, focus);
            return image;
        };
        const auto normal = tile (true, false, false, false);
        const int hot = grey (normal.getPixelAt (10, 2)), contact = grey (normal.getPixelAt (10, 1));
        check (hot - contact >= 40 && hot > grey (normal.getPixelAt (88, 2)) + 40
               && grey (tile (true, true, false, false).getPixelAt (10, 2)) > hot
               && grey (tile (true, false, true, false).getPixelAt (10, 2)) < hot
               && grey (tile (false, false, false, false).getPixelAt (10, 2)) < grey (tile (true, false, true, false).getPixelAt (10, 2))
               && grey (tile (true, false, false, true).getPixelAt (10, 2)) > hot,
               "bass lighting: control contrast/taper and hover/pressed/disabled/focus ordering - " + name
               + " core/contact=" + juce::String (hot) + "/" + juce::String (contact));
        check (normal.getPixelAt (52, 14) == Theme::raised
               && Theme::bassControlLight().getSaturation() < 0.10f
               && Theme::bassControlLight().getBrightness() >= 0.80f,
               "bass lighting: semantic centre fill preserved, neutral high-value metal - " + name);
        for (int width : { 44, 52, 104 })
        {
            const auto image = tile (true, false, false, false, width);
            int largestStep = 0;
            bool fading = true;
            for (int y = 9; y <= 19; ++y)
            {
                const int previous = grey (image.getPixelAt (2, y - 1));
                const int current = grey (image.getPixelAt (2, y));
                largestStep = juce::jmax (largestStep, std::abs (current - previous));
                fading &= current <= previous + 1;
            }
            const int upper = grey (image.getPixelAt (2, 9));
            const int middle = grey (image.getPixelAt (2, 13));
            const int lower = grey (image.getPixelAt (2, 19));
            check (fading && largestStep <= 16 && upper > middle + 10 && middle > lower + 10,
                   "bass controls: left highlight fades through old end-cap, width=" + juce::String (width)
                   + " - " + name + " step/upper/middle/lower=" + juce::String (largestStep)
                   + "/" + juce::String (upper) + "/" + juce::String (middle) + "/" + juce::String (lower));
        }

        juce::Image well (juce::Image::ARGB, 220, 180, true);
        {
            juce::Graphics wg (well);
            Theme::bassWellRim (wg, { 10.0f, 10.0f, 200.0f, 160.0f });
        }
        bool bounded = true;
        for (int y = 0; y < 180; ++y)
            for (int x = 0; x < 220; ++x)
                if (x < 7 || x >= 213 || y < 7 || y >= 173)
                    bounded &= well.getPixelAt (x, y).getAlpha() == 0;
        check (bounded && grey (well.getPixelAt (30, 7)) - grey (well.getPixelAt (30, 9)) >= 40
               && well.getPixelAt (100, 80) == Theme::deep.darker (0.65f),
               "bass lighting: well 3px extent, >=40 top/inner contrast and untouched dark interior - " + name);

        // Unified tab material: the active Drums tab uses the orange rim, the
        // inactive Bass tab stays grey with the same face.
        SeqTabs tabs (0, Theme::cyan);
        tabs.setSize (251, 28);
        const auto image = tabs.createComponentSnapshot (tabs.getLocalBounds());
        juce::Image drumTop (juce::Image::ARGB, 124, 28, true);
        { juce::Graphics dg (drumTop); Theme::tabFace (dg, { 0, 0, 124, 28 }, Theme::drumAccent(), true); }
        juce::Image bassTop (juce::Image::ARGB, 124, 28, true);
        { juce::Graphics bg (bassTop); Theme::tabFace (bg, { 0, 0, 124, 28 }, Theme::dim, false); }
        check (image.getPixelAt (12, 1) == drumTop.getPixelAt (12, 1)
               && image.getPixelAt (139, 1) == bassTop.getPixelAt (12, 1),
               "bass lighting: active Drums uses the tab colour; inactive Bass is grey - " + name);
    }
    Theme::applyPreset (saved, legacy);
}

inline void bassControlSelfTest (const std::function<void (bool, const juce::String&)>& check)
{
    auto engine = std::make_unique<AudioEngine>();
    engine->prepareForTest (44100.0, 2);
    RiffLookAndFeel laf; // outlives the panel
    BassPanel panel (*engine);
    panel.setLookAndFeel (&laf);
    panel.setView (8.0, 2.0, 0.0, 8.0);
    for (bool paged : { false, true })
        for (bool expanded : { true, false })
            for (int width : { 608, 999, 1000, 1008, 1133, 1134, 1500 })
            {
                engine->bass.bars = paged ? 8 : 1;
                panel.setExpanded (expanded);
                panel.refreshFromEngine();
                panel.setSize (width, panel.preferredHeight (width));
                bool fits = true, separated = true, readable = true;
                int combos = 0, buttons = 0, sliders = 0;
                for (auto* a : panel.getChildren())
                {
                    if (! a->isVisible()) continue;
                    fits &= ! a->getBounds().isEmpty() && panel.getLocalBounds().contains (a->getBounds());
                    if (auto* c = dynamic_cast<juce::ComboBox*> (a))
                    {
                        ++combos;
                        readable &= c->getHeight() >= 24 && c->getWidth() >= (c->getTooltip() == "Root note of the key" ? 42 : 80);
                    }
                    if (dynamic_cast<juce::Button*> (a) != nullptr) ++buttons;
                    if (dynamic_cast<juce::Slider*> (a) != nullptr) ++sliders;
                    for (auto* b : panel.getChildren())
                        if (a != b && b->isVisible()) separated &= ! a->getBounds().intersects (b->getBounds());
                }
                check (fits && separated && readable && combos == (expanded ? 7 : 5)
                       && buttons == (expanded ? 9 : 7) + (paged && expanded ? 2 : 0)
                       && sliders == (expanded ? 9 : 1),
                       "bass layout: all controls contained/readable/non-overlapping width=" + juce::String (width)
                       + " header=" + juce::String (panel.headerHeight()) + " expanded=" + juce::String ((int) expanded)
                       + " paged=" + juce::String ((int) paged));
            }
    panel.setExpanded (true);
    engine->bass.bars = 1;
    panel.refreshFromEngine();
    panel.setSize (608, panel.preferredHeight (608));
    const auto before = engine->bass.snapshot();
    int combos = 0, buttons = 0, volumes = 0;
    juce::String scopeDetail;
    bool scope = (bool) panel.counter().getProperties()["bassControl"], volumeOK = false;
    for (auto* c : panel.getChildren())
    {
        const bool marked = (bool) c->getProperties().getWithDefault ("bassControl", false);
        if (auto* box = dynamic_cast<juce::ComboBox*> (c))
        {
            ++combos; scope &= marked;
            if (! marked) scopeDetail << " combo:" << box->getText();
        }
        if (auto* b = dynamic_cast<juce::TextButton*> (c))
        {
            ++buttons; scope &= marked && ! b->getWantsKeyboardFocus();
            if (! marked || b->getWantsKeyboardFocus()) scopeDetail << " button:" << b->getButtonText();
        }
        if (auto* s = dynamic_cast<juce::Slider*> (c))
        {
            const bool volume = (bool) s->getProperties().getWithDefault ("bassVolumeKnob", false);
            scope &= volume == (s->getTooltip() == "Bass volume");
            if (volume != (s->getTooltip() == "Bass volume")) scopeDetail << " slider:" << s->getTooltip();
            if (volume)
            {
                ++volumes;
                volumeOK = s->getMinimum() == 0.0 && s->getMaximum() == 1.2
                    && s->getDoubleClickReturnValue() == 0.8 && ! s->getWantsKeyboardFocus();
            }
        }
    }
    panel.createComponentSnapshot (panel.getLocalBounds());
    panel.lookAndFeelChanged();
    check (scope && combos == 7 && buttons == 10 && volumes == 1 && volumeOK && before == engine->bass.snapshot(),
           "bass controls: explicit opt-in, volume 0..1.2/reset 0.8, focus and pattern preserved (scope="
           + juce::String ((int) scope) + ", combos=" + juce::String (combos) + ", buttons=" + juce::String (buttons)
           + ", volumes=" + juce::String (volumes) + ", volumeOK=" + juce::String ((int) volumeOK)
           + ", pattern=" + juce::String ((int) (before == engine->bass.snapshot())) + scopeDetail + ")");
    bool liveOK = true;
    for (int state = 0; state < 3; ++state)
    {
        panel.setLiveState (state);
        bool found = false;
        for (auto* c : panel.getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (c))
                if (b->getButtonText() == (state == 0 ? "Rec" : state == 1 ? "Wait" : "REC"))
                    found = b->getToggleState() == (state != 0) && b->findColour (juce::TextButton::buttonOnColourId) == Theme::red;
        liveOK &= found;
    }
    check (liveOK, "bass controls: Rec/Wait/REC retain toggle and record-red semantics");
    panel.setLiveState (0);

    const Theme::Preset saved { "live", Theme::blue.getARGB(), Theme::purple.getARGB(), Theme::cyan.getARGB(),
                               Theme::bg.getARGB(), Theme::panel.getARGB(), Theme::text.getARGB() };
    const bool legacy = Theme::legacyTracks;
    auto equal = [] (const juce::Image& a, const juce::Image& b)
    {
        for (int y = 0; y < a.getHeight(); ++y)
            for (int x = 0; x < a.getWidth(); ++x)
                if (a.getPixelAt (x, y) != b.getPixelAt (x, y)) return false;
        return true;
    };
    for (int theme : { 2, 0 }) // new visual scope: Sunset and Tokyo Night only
    {
        Theme::applyPreset (Theme::presets[(size_t) theme], theme == 0);
        laf.applyColours();
        for (bool combo : { false, true })
        {
            juce::ComboBox box;
            juce::TextButton button;
            button.setSize (104, 28);
            box.setSize (104, 28);
            auto render = [&] (int opt, bool enabled, bool over, bool down, bool active)
            {
                juce::Component& c = combo ? (juce::Component&) box : (juce::Component&) button;
                c.setLookAndFeel (&laf);
                c.setEnabled (enabled);
                if (opt < 0) c.getProperties().remove ("bassControl"); else c.getProperties().set ("bassControl", opt != 0);
                juce::Image image (juce::Image::ARGB, 104, 28, true);
                juce::Graphics g (image);
                if (combo) laf.drawComboBox (g, 104, 28, down, 0, 0, 0, 0, box);
                else laf.drawButtonBackground (g, button, active ? Theme::accent : Theme::raised, over, down);
                c.setLookAndFeel (nullptr);
                return image;
            };
            const auto plain = render (-1, true, false, false, false);
            const auto optedOut = render (0, true, false, false, false);
            const auto normal = render (1, true, false, false, false);
            check (equal (plain, optedOut) && ! equal (plain, normal)
                   && ! equal (normal, render (1, false, false, false, false))
                   && ! equal (normal, render (1, true, false, true, false))
                   && (combo || (! equal (normal, render (1, true, true, false, false))
                                 && ! equal (normal, render (1, true, false, false, true)))),
                   "bass material: opt-out unchanged, opt-in and states differ - " + Theme::presets[(size_t) theme].name
                   + (combo ? " combo" : " button"));
        }
        panel.counter().getProperties().set ("bassControl", false);
        const auto plainCounter = panel.counter().createComponentSnapshot (panel.counter().getLocalBounds());
        panel.counter().getProperties().set ("bassControl", true);
        const auto bassCounter = panel.counter().createComponentSnapshot (panel.counter().getLocalBounds());
        bool displaySame = true, arrowsDiffer = false;
        for (int y = 0; y < bassCounter.getHeight(); ++y)
            for (int x = 0; x < bassCounter.getWidth(); ++x)
                // The display housing is always full width and ends at x=70.
                // Arrow shadow may occupy the previously empty 2px gap.
                if (x < 70) displaySame &= plainCounter.getPixelAt (x, y) == bassCounter.getPixelAt (x, y);
                else arrowsDiffer |= plainCounter.getPixelAt (x, y) != bassCounter.getPixelAt (x, y);
        check (displaySame && arrowsDiffer && Theme::bassNoteLabel().getPerceivedBrightness() > Theme::dim.getPerceivedBrightness(),
               "bass material: blue library display unchanged, arrows opt in, note label contrast - " + Theme::presets[(size_t) theme].name);
    }
    Theme::applyPreset (saved, legacy);
    panel.setLookAndFeel (nullptr);

    // Same note edit in actual minimum/normal Bass bounds, including the local
    // header offset. No device, settings or project files are touched.
    for (int width : { 608, 1008 })
    {
        engine->bass.clearPattern();
        panel.setSize (width, panel.preferredHeight (width));
        juce::UndoManager undo;
        panel.onUndoable = [&] (const juce::String& name, std::function<void()> apply, std::function<void()> revert)
        {
            undo.beginNewTransaction (name);
            undo.perform (new LambdaAction (std::move (apply), std::move (revert)));
        };
        const float step = (float) (width - TrackRow::waveLeft - TrackRow::waveRightInset) / 64.0f;
        const juce::Point<float> start ((float) TrackRow::waveLeft + step * 0.5f,
                                       (float) (panel.headerHeight() + BassPanel::shiftH + 5 * BassPanel::rowH + 6));
        const auto finish = start.translated (step * 3.0f, 0.0f);
        auto event = [&] (juce::Point<float> pos, bool drag)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), pos,
                                    juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                    &panel, &panel, juce::Time::getCurrentTime(), start, juce::Time::getCurrentTime(), 1, drag);
        };
        panel.mouseDown (event (start, false));
        panel.mouseDrag (event (finish, true));
        panel.mouseUp (event (finish, true));
        const auto note = engine->bass.steps[0].load();
        const bool added = BassMachine::isNote (note) && BassMachine::lenOf (note) == 4;
        undo.undo();
        const bool removed = ! engine->bass.hasAnySteps();
        undo.redo();
        check (added && removed && engine->bass.steps[0].load() == note,
               "bass layout: note create/length/audition mapping and undo/redo width=" + juce::String (width));
        panel.onUndoable = {};
    }
}

inline void bottomRowSelfTest (const std::function<void (bool, const juce::String&)>& check)
{
    auto engine = std::make_unique<AudioEngine>();
    engine->prepareForTest (44100.0, 2);
    RiffLookAndFeel laf;
    juce::Component host;
    DrumPanel drums (*engine);
    BassPanel bass (*engine);
    MixerPanel mixer (*engine);
    host.addChildComponent (drums); host.addChildComponent (bass); host.addChildComponent (mixer);
    drums.setLookAndFeel (&laf); bass.setLookAndFeel (&laf); mixer.setLookAndFeel (&laf);
    bass.setView (8.0, 2.0, 0.0, 8.0);
    int width = 1484, height = 941, tab = 1, layoutCalls = 0;
    juce::Rectangle<int> remaining;
    auto layout = [&]
    {
        remaining = juce::Rectangle<int> (width, height).reduced (12).withTrimmedTop (110);
        MainComponent::layoutBottomRow (remaining, drums, bass, mixer, tab);
        ++layoutCalls;
    };
    // Same collapse/page and tab routing as the real parent, without settings or devices.
    drums.onLayoutChanged = [&]
    {
        if (bass.isExpanded() != drums.isExpanded()) bass.setExpanded (drums.isExpanded());
        else layout();
    };
    bass.onLayoutChanged = [&]
    {
        if (drums.isExpanded() != bass.isExpanded()) drums.setExpanded (bass.isExpanded());
        else layout();
    };
    drums.onSelectTab = bass.onSelectTab = [&] (int t)
    {
        tab = t;
        drums.setVisible (tab == 0); bass.setVisible (tab == 1);
        drums.seqTabs().setVisible (tab == 0); bass.seqTabs().setVisible (tab == 1);
        layout();
    };
    auto paintedEdges = [] (juce::Component& c)
    {
        juce::Image image (juce::Image::ARGB, c.getWidth(), c.getHeight(), true);
        {
            juce::Graphics g (image);
            c.paint (g); // actual production enclosure paint, excluding parent shadow
        } // flush the Windows renderer before reading pixels
        int top = -1, bottom = -1;
        for (int y = 0; y < image.getHeight(); ++y)
            if (image.getPixelAt (100, y).getAlpha() != 0)
            {
                if (top < 0) top = y;
                bottom = y;
            }
        return std::pair<int, int> { c.getY() + top, c.getY() + bottom };
    };
    for (int w : { 1484, 1084, 1473, 1474, 1607, 1608, 1484 })
        for (bool expanded : { true, false })
            for (bool paged : { false, true })
              for (bool drumPaged : { false, true })
                for (int selected : { 0, 1 })
                {
                    width = w; height = w == 1084 ? 581 : 941;
                    layout();
                    const int before = layoutCalls;
                    bass.setExpanded (expanded);
                    check (layoutCalls > before, "bottom row: collapse/expand callback relayout");
                    engine->bass.bars = paged ? 8 : 1;
                    bass.refreshFromEngine();
                    engine->drums.bars = drumPaged ? 8 : 1;
                    drums.refreshFromEngine();
                    bass.onSelectTab (selected);
                    auto& left = selected == 1 ? static_cast<juce::Component&> (bass)
                                               : static_cast<juce::Component&> (drums);
                    const int extra = bass.headerHeight() - BassPanel::headerH;
                    const juce::String context = " width=" + juce::String (w) + " tab=" + juce::String (selected)
                        + " expanded=" + juce::String ((int) expanded) + " paged=" + juce::String ((int) paged)
                        + " drumPaged=" + juce::String ((int) drumPaged);
                    const auto leftEdges = paintedEdges (left), rightEdges = paintedEdges (mixer);
                    const juce::Rectangle<int> normalMixer (width - 12 - MixerPanel::panelW,
                                                           height - 12 - MixerPanel::panelH,
                                                           MixerPanel::panelW, MixerPanel::panelH);
                    const std::pair<int, int> normalMixerEdges { normalMixer.getY() + 1, normalMixer.getBottom() - 2 };
                    check (expanded ? leftEdges == rightEdges
                                    : mixer.getBounds() == normalMixer && rightEdges == normalMixerEdges
                                      && leftEdges.second < rightEdges.second,
                           (expanded ? "bottom row: expanded painted top AND bottom equal"
                                     : "bottom row: folded left ends early; full normal mixer position/inset retained") + context
                           + " left=" + juce::String (leftEdges.first) + "," + juce::String (leftEdges.second)
                           + " right=" + juce::String (rightEdges.first) + "," + juce::String (rightEdges.second));
                    const int budget = 208 + extra;
                    check (expanded
                               ? remaining.getBottom() == height - 12 - 28 - budget
                                 && bass.getHeight() == budget && drums.getHeight() == budget - 2
                                 && bass.getBounds().reduced (1) == drums.getBounds()
                                 && bass.seqTabs().getBounds() == drums.seqTabs().getBounds()
                                 && bass.seqTabs().getBottom() == bass.getY() + 1
                                 && mixer.getY() == bass.getY() && mixer.getHeight() == budget
                               : remaining.getBottom() == height - 12 - 234 - extra
                                 && bass.getHeight() == bass.preferredHeight (bass.getWidth())
                                 && drums.getHeight() == bass.getHeight() - 2
                                 && bass.getHeight() == BassPanel::headerH + extra
                                 && bass.getY() == normalMixer.getY() - extra && drums.getY() == bass.getY() + 1,
                           "bottom row: shared budget/contours/tab baseline; folded header-only/full mixer retained" + context);
                    bool commonEqual = true, controlsFit = true, controlsSeparate = true;
                    const auto bc = bass.headerControls(), dc = drums.headerControls();
                    for (size_t i = 0; i < bc.size(); ++i)
                    {
                        commonEqual &= host.getLocalArea (bc[i], bc[i]->getLocalBounds())
                                    == host.getLocalArea (dc[i], dc[i]->getLocalBounds());
                    }
                    for (auto* panel : std::initializer_list<juce::Component*> { &bass, &drums })
                        for (auto* c : panel->getChildren())
                        {
                            if (! c->isVisible() || c->getBounds().isEmpty()) continue;
                            controlsFit &= panel->getLocalBounds().contains (c->getBounds());
                            for (auto* other : panel->getChildren())
                                if (other != c && other->isVisible() && ! other->getBounds().isEmpty())
                                    controlsSeparate &= ! c->getBounds().intersects (other->getBounds());
                        }
                    check (commonEqual && controlsFit && controlsSeparate,
                           "shared header: ALL 11 parent-coordinate bounds equal, visible children contained/non-overlapping" + context);
                    check (bass.headerHeight() == SeqHeaderLayout::height (bass.getWidth(), expanded && (paged || drumPaged))
                           && drums.headerHeight() == bass.headerHeight()
                           && (int) bass.hasPages() == (int) (expanded && paged)
                           && (int) drums.hasPages() == (int) (expanded && drumPaged),
                           "shared header: wrap uses BOTH paging states without changing own page visibility" + context);
                    {
                        std::array<juce::Rectangle<int>, SeqHeaderLayout::numCommon> anchors;
                        for (size_t i = 0; i < bc.size(); ++i) anchors[i] = host.getLocalArea (bc[i], bc[i]->getLocalBounds());
                        const auto foldedOrExpandedRemaining = remaining, foldedOrExpandedMixer = mixer.getBounds();
                        drums.onSelectTab (1 - selected);
                        bool stable = remaining == foldedOrExpandedRemaining && mixer.getBounds() == foldedOrExpandedMixer;
                        for (size_t i = 0; i < bc.size(); ++i)
                            stable &= anchors[i] == host.getLocalArea (bc[i], bc[i]->getLocalBounds())
                                   && anchors[i] == host.getLocalArea (dc[i], dc[i]->getLocalBounds());
                        bass.onSelectTab (selected);
                        check (stable, "shared header: actual tab callback preserves all common anchors, folded/expanded" + context);
                    }
                    if (expanded)
                    {
                        const auto frameBefore = bass.getBounds().reduced (1);
                        const auto tabsBefore = bass.seqTabs().getBounds(), mixerBefore = mixer.getBounds(), tracksBefore = remaining;
                        const auto bassBounds = bass.getBounds(), drumsBounds = drums.getBounds();
                        // Exercise each panel's actual onSelectTab callback, not a
                        // separately copied formula. Then return through the other.
                        drums.onSelectTab (1 - selected);
                        check (frameBefore == bass.getBounds().reduced (1) && frameBefore == drums.getBounds()
                               && tabsBefore == bass.seqTabs().getBounds() && tabsBefore == drums.seqTabs().getBounds()
                               && mixerBefore == mixer.getBounds() && tracksBefore == remaining
                               && bassBounds == bass.getBounds() && drumsBounds == drums.getBounds()
                               && paintedEdges (bass) == paintedEdges (drums) && paintedEdges (drums) == paintedEdges (mixer),
                               "bottom row: actual tab switch has ZERO frame/tab/mixer/track-reservation movement" + context);
                        bass.onSelectTab (selected);
                        check (mixerBefore == mixer.getBounds() && tracksBefore == remaining
                               && tabsBefore == bass.seqTabs().getBounds() && tabsBefore == drums.seqTabs().getBounds(),
                               "bottom row: return tab callback restores identical geometry" + context);
                        if (! paged && ! drumPaged && (w == 1484 || w == 1084))
                        {
                            const auto name = juce::String (selected == 0 ? "Drums" : "Bass") + "-layout-width-" + juce::String (w);
                            DrumMaterialTest::artifact (left.createComponentSnapshot (left.getLocalBounds()), name);
                            if (selected == 0)
                                DrumMaterialTest::artifact (drums.seqTabs().createComponentSnapshot (drums.seqTabs().getLocalBounds()),
                                                            "active-tab-width-" + juce::String (w));
                        }
                    }
                    bool contained = true;
                    for (auto* child : mixer.getChildren())
                        contained &= mixer.getLocalBounds().contains (child->getBounds()) && child->getHeight() >= 196;
                    for (auto* child : bass.getChildren())
                        if (child->isVisible()) contained &= bass.getLocalBounds().contains (child->getBounds());
                    for (auto* child : drums.getChildren())
                        if (child->isVisible() && ! child->getBounds().isEmpty())
                            contained &= drums.getLocalBounds().contains (child->getBounds());
                    mixer.setEnabled (false); drums.setEnabled (false); bass.setEnabled (false);
                    check (contained && paintedEdges (left) == leftEdges && paintedEdges (mixer) == rightEdges,
                           "bottom row: disabled paint and all visible children contained" + context);
                    mixer.setEnabled (true); drums.setEnabled (true); bass.setEnabled (true);
                }
    // Page visibility alone must resize an expanded Bass at the normal window width.
    width = 1484; tab = 0; bass.setExpanded (true);
    engine->drums.bars = 1; drums.refreshFromEngine();
    engine->bass.bars = 1; bass.refreshFromEngine(); layout();
    const int beforePages = layoutCalls;
    engine->bass.bars = 8; bass.refreshFromEngine();
    check (layoutCalls > beforePages && mixer.getHeight() == 240
           && paintedEdges (bass) == paintedEdges (mixer) && paintedEdges (drums) == paintedEdges (mixer),
           "bottom row: hidden Bass page callback updates shared wrapped row/mixer to 240px while Drums selected");
    engine->bass.bars = 1; bass.refreshFromEngine();
    check (mixer.getHeight() == 208 && bass.getHeight() == 208 && drums.getHeight() == 206,
           "bottom row: hidden Bass unpaging restores common 208px budget, intact 206px Drums content");
    for (bool collapseViaDrums : { true, false })
    {
        if (collapseViaDrums) drums.setExpanded (false); else bass.setExpanded (false);
        check (! drums.isExpanded() && ! bass.isExpanded() && drums.getHeight() == 40
               && bass.getHeight() == bass.preferredHeight() && mixer.getHeight() == 206,
               "bottom row: collapse from either panel synchronizes original preferred-height frames");
        if (collapseViaDrums) drums.setExpanded (true); else bass.setExpanded (true);
        check (drums.isExpanded() && bass.isExpanded() && paintedEdges (drums) == paintedEdges (bass)
               && bass.seqTabs().getBounds() == drums.seqTabs().getBounds(),
               "bottom row: expand from either panel restores shared frame/tab baseline");
    }

    // Real gestures in the shared parent layout, including hidden-other-panel
    // paging forcing a wrap. No screen input, settings, devices or project I/O.
    for (int w : { 1084, 1473, 1474, 1484, 1607, 1608 })
        for (bool bp : { false, true })
            for (bool dp : { false, true })
            {
                width = w; bass.setExpanded (true);
                engine->drums.clearPattern(); engine->bass.clearPattern();
                engine->drums.bars = dp ? 8 : 1; engine->bass.bars = bp ? 8 : 1;
                drums.refreshFromEngine(); bass.refreshFromEngine(); layout();
                const juce::String tag = " width=" + juce::String (w) + " bassPage=" + juce::String ((int) bp)
                    + " drumPage=" + juce::String ((int) dp);
                juce::UndoManager undo;
                auto push = [&] (const juce::String& name, std::function<void()> apply, std::function<void()> revert)
                { undo.beginNewTransaction (name); undo.perform (new LambdaAction (std::move (apply), std::move (revert))); };
                drums.onUndoable = push; bass.onUndoable = push;
                auto pageButton = [] (juce::Component* c) { dynamic_cast<juce::TextButton*> (c)->onClick(); };
                // Return to page one first, then exercise the real page callback.
                if (dp) { pageButton (drums.headerControls()[SeqHeaderLayout::Previous]); pageButton (drums.headerControls()[SeqHeaderLayout::Next]); }
                if (bp) { pageButton (bass.headerControls()[SeqHeaderLayout::Previous]); pageButton (bass.headerControls()[SeqHeaderLayout::Next]); }
                auto event = [] (juce::Component& c, juce::Point<float> pos, int modifiers, bool drag = false)
                {
                    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), pos,
                        juce::ModifierKeys (modifiers), 1, 0, 0, 0, 0, &c, &c,
                        juce::Time::getCurrentTime(), pos, juce::Time::getCurrentTime(), 1, drag);
                };
                const int left = juce::ModifierKeys::leftButtonModifier, right = juce::ModifierKeys::rightButtonModifier;
                const float dw = (float) (drums.getWidth() - TrackRow::waveLeft - TrackRow::waveRightInset) / 64.0f;
                auto point = [&] (int cell, int voice)
                { return juce::Point<float> (TrackRow::waveLeft + (cell + 0.5f) * dw, (float) drums.bodyTop() + voice * 26 + 13); };
                const auto empty = drums.createComponentSnapshot (drums.getLocalBounds());
                bool cells = true, laneHeaders = true;
                for (int voice = 0; voice < 6; ++voice)
                {
                    const int base = dp ? 64 : 0;
                    const auto a = point (1, voice), b = point (4, voice);
                    drums.mouseDown (event (drums, a, left));
                    drums.mouseDrag (event (drums, b, left, true));
                    drums.mouseUp (event (drums, b, left, true));
                    cells &= engine->drums.steps[voice][base + 1] == 1 && engine->drums.steps[voice][base + 4] == 1;
                    const auto drawn = engine->drums.snapshot();
                    undo.undo();
                    cells &= engine->drums.steps[voice][base + 1] == 0 && engine->drums.steps[voice][base + 4] == 0;
                    undo.redo(); cells &= engine->drums.snapshot() == drawn;
                    drums.mouseDown (event (drums, a, right)); drums.mouseUp (event (drums, a, right));
                    cells &= engine->drums.steps[voice][base + 1] == 2;
                    drums.mouseDown (event (drums, a, left | juce::ModifierKeys::shiftModifier));
                    drums.mouseUp (event (drums, a, left | juce::ModifierKeys::shiftModifier));
                    cells &= engine->drums.steps[voice][base + 1] == 1;
                    if (! dp)
                    {
                        // Repeated display cell 17 maps back to pattern step 1.
                        const auto repeated = point (17, voice);
                        drums.mouseDown (event (drums, repeated, right)); drums.mouseUp (event (drums, repeated, right));
                        cells &= engine->drums.steps[voice][1] == 2;
                    }
                    const bool mute = engine->drums.voiceMute[voice].load();
                    drums.mouseDown (event (drums, { 25, a.y }, left));
                    laneHeaders &= engine->drums.voiceMute[voice].load() != mute;
                    drums.mouseDown (event (drums, { 25, a.y }, left));
                    laneHeaders &= engine->drums.voiceMute[voice].load() == mute;
                    engine->drums.audition[voice] = false;
                    drums.mouseDown (event (drums, { 65, a.y }, left));
                    laneHeaders &= engine->drums.audition[voice].exchange (false);
                }
                const auto populated = drums.createComponentSnapshot (drums.getLocalBounds());
                bool layers = true;
                for (int voice = 0; voice < 6; ++voice)
                {
                    const auto p = point (1, voice).toInt();
                    layers &= empty.getPixelAt (p.x, p.y) != populated.getPixelAt (p.x, p.y);
                }
                const auto patternBeforeHeader = engine->drums.snapshot();
                drums.mouseDown (event (drums, { point (1, 0).x, (float) drums.bodyTop() - 1 }, left));
                drums.mouseUp (event (drums, { point (1, 0).x, (float) drums.bodyTop() - 1 }, left));
                check (cells && layers && patternBeforeHeader == engine->drums.snapshot(),
                       "shared header gestures: six Drum lanes, drag, accents, repeat/page mapping, undo/redo, paint and header exclusion" + tag);
                check (laneHeaders, "shared header gestures: all six shifted lane mute/audition hit areas" + tag);

                undo.clearUndoHistory();
                const float bw = (float) (bass.getWidth() - TrackRow::waveLeft - TrackRow::waveRightInset) / 64.0f;
                const juce::Point<float> start (TrackRow::waveLeft + bw * 0.5f,
                    (float) bass.headerHeight() + BassPanel::shiftH + 5 * BassPanel::rowH + 6);
                const auto finish = start.translated (bw * 3, 0);
                bass.mouseDown (event (bass, start, left));
                bass.mouseDrag (event (bass, finish, left, true)); bass.mouseUp (event (bass, finish, left, true));
                const int step = bp ? 64 : 0;
                const auto note = engine->bass.steps[step].load();
                bool notes = BassMachine::isNote (note) && BassMachine::lenOf (note) == 4;
                undo.undo(); notes &= ! engine->bass.hasAnySteps();
                undo.redo(); notes &= engine->bass.steps[step].load() == note;
                bass.mouseDown (event (bass, start, right)); bass.mouseUp (event (bass, start, right));
                notes &= BassMachine::accentOf (engine->bass.steps[step].load());
                bass.mouseDown (event (bass, start, left | juce::ModifierKeys::ctrlModifier));
                bass.mouseUp (event (bass, start, left | juce::ModifierKeys::ctrlModifier));
                notes &= BassMachine::slideOf (engine->bass.steps[step].load());
                const auto bassBeforeHeader = engine->bass.snapshot();
                bass.mouseDown (event (bass, { start.x, (float) bass.headerHeight() - 1 }, left));
                bass.mouseUp (event (bass, { start.x, (float) bass.headerHeight() - 1 }, left));
                check (notes && bassBeforeHeader == engine->bass.snapshot(),
                       "shared header gestures: Bass fixed-grid note/length/accent/slide/page mapping, undo/redo and header exclusion" + tag);
                drums.onUndoable = {}; bass.onUndoable = {};
            }

    // Outside silhouette coverage and a fully faded extent, without a shadow gutter/hit-area change.
    juce::Image shadow (juce::Image::ARGB, 160, 100, true);
    {
        juce::Graphics g (shadow);
        Theme::tunerShadow (g, { 20, 20, 120, 60 });
    }
    check (shadow.getPixelAt (80, 80).getAlpha() > shadow.getPixelAt (80, 82).getAlpha()
           && shadow.getPixelAt (80, 82).getAlpha() > 0
           && shadow.getPixelAt (80, 84).getAlpha() == 0
           && shadow.getPixelAt (16, 50).getAlpha() == 0,
           "tuner shadow: real outside pixels, downward taper ends inside existing 4px padding");
    juce::Image outer (juce::Image::ARGB, 500, 280, true);
    {
        juce::Graphics g (outer);
        Theme::cardShadow (g, { 20, 20, 440, 238 });
    }
    check (outer.getPixelAt (240, 260).getAlpha() > 0
           && outer.getPixelAt (240, 271).getAlpha() == 0,
           "mixer outer shadow: outside silhouette, fully faded within parent bottom margin");
    drums.onLayoutChanged = {}; bass.onLayoutChanged = {};
    drums.setLookAndFeel (nullptr); bass.setLookAndFeel (nullptr); mixer.setLookAndFeel (nullptr);
}

inline void levelMeterSelfTest (const std::function<void (bool, const juce::String&)>& check)
{
    const Theme::Preset saved { "live", Theme::blue.getARGB(), Theme::purple.getARGB(), Theme::cyan.getARGB(),
                               Theme::bg.getARGB(), Theme::panel.getARGB(), Theme::text.getARGB() };
    const bool legacy = Theme::legacyTracks;
    const Theme::Preset themes[] { Theme::presets[0], Theme::presets[2], Theme::presets[7] };
    for (const auto& preset : themes)
    {
        Theme::applyPreset (preset, preset.name == "Tokyo Night");
        for (bool horizontal : { false, true })
        {
            LevelMeter meter;
            meter.setSize (horizontal ? 120 : 24, horizontal ? 24 : 120);
            meter.setHorizontal (horizontal);
            meter.setBars (1);
            meter.setLevels (0.5f, 0.5f);
            const auto image = meter.createComponentSnapshot (meter.getLocalBounds());
            bool fillFound = false, peakFound = false, zoneFound = false;
            for (int i = 0; i < 120; ++i)
            {
                const auto px = horizontal ? image.getPixelAt (i, 12) : image.getPixelAt (12, i);
                fillFound |= px == Theme::meterFill();
                peakFound |= px == Theme::meterPeak();
                zoneFound |= px == Theme::green || px == Theme::red;
            }
            check (Theme::meterFill() == juce::Colour (0xffbb9af7)
                   && Theme::meterPeak() == juce::Colour (0xff7aa2f7)
                   && fillFound && peakFound && ! zoneFound,
                   "level meter: flat fixed Tokyo lilac fill, blue peak and no zone colours - " + preset.name
                   + (horizontal ? " horizontal" : " vertical"));
        }
    }
    Theme::applyPreset (saved, legacy);
}

inline void mixerMaterialSelfTest (const std::function<void (bool, const juce::String&)>& check)
{
    const Theme::Preset saved { "live", Theme::blue.getARGB(), Theme::purple.getARGB(), Theme::cyan.getARGB(),
                               Theme::bg.getARGB(), Theme::panel.getARGB(), Theme::text.getARGB() };
    const bool legacy = Theme::legacyTracks;
    auto themes = Theme::presets;
    themes.push_back ({ "Neo-Retro", 0xffff780a, 0xfff2ab24, 0xfff6c343, 0xff0b1018, 0xff202630, 0xffdce3ec });
    for (const auto& theme : themes)
    {
        Theme::applyPreset (theme, theme.name == "Tokyo Night");
        RiffLookAndFeel laf;
        juce::Slider fader;
        fader.setColour (juce::Slider::trackColourId, Theme::purple);
        for (bool enabled : { true, false })
            for (bool vertical : { true, false })
                for (float pos : { 12.0f, 50.0f, 88.0f })
                {
                    fader.setEnabled (enabled);
                    auto render = [&] (bool marked, bool old)
                    {
                        juce::Image image (juce::Image::ARGB, 100, 100, true);
                        juce::Graphics g (image);
                        fader.getProperties().set ("masterOutputFader", marked);
                        if (! old)
                            laf.drawLinearSlider (g, 12, 12, vertical ? 24 : 76, vertical ? 76 : 24, pos, 0, 0,
                                                  vertical ? juce::Slider::LinearVertical : juce::Slider::LinearHorizontal, fader);
                        else
                        {
                            const juce::Rectangle<float> groove = vertical ? juce::Rectangle<float> (22, 12, 4, 76)
                                                                          : juce::Rectangle<float> (12, 22, 76, 4);
                            g.setColour (Theme::deep); g.fillRoundedRectangle (groove, 1.0f);
                            g.setColour (vertical ? Theme::purple.withAlpha (enabled ? 0.9f : 0.3f) : Theme::purple);
                            g.fillRoundedRectangle (vertical ? groove.withTop (pos) : groove.withRight (pos), 1.0f);
                            g.setColour (Theme::text);
                            g.fillEllipse (vertical ? 17.0f : pos - 7, vertical ? pos - 7 : 17.0f, 14, 14);
                        }
                        return image;
                    };
                    const auto plain = render (false, false), old = render (false, true), marked = render (true, false);
                    bool unchanged = true, bounded = true, different = false;
                    for (int y = 0; y < 100; ++y)
                        for (int x = 0; x < 100; ++x)
                        {
                            unchanged &= plain.getPixelAt (x, y) == old.getPixelAt (x, y);
                            const bool diff = marked.getPixelAt (x, y) != plain.getPixelAt (x, y);
                            different |= diff;
                            if (! vertical || x < 17 || x >= 31 || y < pos - 7 || y >= pos + 7) bounded &= ! diff;
                        }
                    check (unchanged && bounded && (different == vertical),
                           "master fader: opt-in only, legacy exact, 14px cap/4px slot/travel retained - " + theme.name
                           + (vertical ? " vertical " : " horizontal ") + juce::String (pos) + (enabled ? " enabled" : " disabled"));
                }
        check (Theme::mixerCoolLight().getBlue() > Theme::mixerCoolLight().getRed(),
               "mixer material: live cool role independent of Accent hue - " + theme.name);
    }
    Theme::applyPreset (saved, legacy);

    auto engine = std::make_unique<AudioEngine>();
    engine->prepareForTest (44100.0, 2);
    const int slot = engine->addTrack();
    RiffLookAndFeel laf;
    MixerPanel panel (*engine);
    panel.setLookAndFeel (&laf);
    panel.setVisible (true);
    panel.setSize (440, 206);
    ChannelStrip* master = nullptr;
    TunerCard* tuner = nullptr;
    for (auto* child : panel.getChildren())
    {
        if (auto* c = dynamic_cast<ChannelStrip*> (child)) master = c;
        if (auto* c = dynamic_cast<TunerCard*> (child)) tuner = c;
    }
    juce::Slider* fader = nullptr;
    juce::Label* db = nullptr;
    if (master != nullptr)
        for (auto* child : master->getChildren())
        {
            if (auto* c = dynamic_cast<juce::Slider*> (child)) fader = c;
            if (auto* c = dynamic_cast<juce::Label*> (child)) db = c;
        }
    check (master != nullptr && master->getBounds() == juce::Rectangle<int> (331, 5, 104, 196)
           && tuner != nullptr && tuner->getBounds() == juce::Rectangle<int> (13, 5, 310, 196),
           "mixer geometry: 440x206, pad5, master104 and tuner310 unchanged");
    if (fader != nullptr && db != nullptr)
    {
        check ((bool) fader->getProperties()["masterOutputFader"] && fader->getBounds() == juce::Rectangle<int> (61, 30, 24, 133)
               && db->getBounds() == juce::Rectangle<int> (14, 168, 76, 20)
               && fader->getMinimum() == 0 && fader->getMaximum() == 1 && fader->getInterval() == 0
               && fader->getDoubleClickReturnValue() == gainToFader (1.0f) && ! fader->getWantsKeyboardFocus()
               && db->getText() == "0.0 dB", "master: range/default/unity/focus/bounds unchanged");
        std::function<void()> undo, redo;
        int commits = 0;
        panel.onUndoable = [&] (const juce::String&, std::function<void()> r, std::function<void()> u)
        { ++commits; redo = std::move (r); undo = std::move (u); };
        fader->onDragStart();
        fader->setValue (0, juce::sendNotificationSync);
        check (engine->masterVolume == 0 && db->getText() == "-inf dB" && commits == 0,
               "master: drag updates gain/dB, defers undo commit");
        fader->onDragEnd();
        check (commits == 1 && undo && redo, "master: drag end commits one undo");
        if (undo && redo)
        {
            undo();
            check (engine->masterVolume == 1 && db->getText() == "0.0 dB", "master: undo restores unity");
            redo();
            check (engine->masterVolume == 0 && db->getText() == "-inf dB", "master: redo restores silence");
        }
        fader->setValue (1, juce::sendNotificationSync);
        check (std::abs (engine->masterVolume.load() - faderToGain (1)) < 0.0001f && commits == 2,
               "master: non-drag value commits, full-range gain mapping unchanged");
        panel.onUndoable = {};
    }
    ChannelStrip track (*engine, slot, 0);
    bool unmarked = true;
    for (auto* child : track.getChildren())
        if (auto* s = dynamic_cast<juce::Slider*> (child))
            unmarked &= ! s->getProperties().contains ("masterOutputFader");
    check (unmarked, "master opt-in: track fader and pan remain unmarked");
    if (tuner != nullptr)
    {
        tuner->setRef (429); check (tuner->getRef() == 430, "tuner: reference lower clamp unchanged");
        tuner->setRef (451); check (tuner->getRef() == 450, "tuner: reference upper clamp unchanged");
        tuner->setRef (440);
        const auto image = tuner->createComponentSnapshot (tuner->getLocalBounds());
        check (image.getPixelAt (20, 5) == Theme::deep
               && image.getPixelAt (20, 190) == Theme::deep
               && image.getPixelAt (20, 0) != image.getPixelAt (20, 195),
               "tuner production paint: deep well unchanged, light confined to 3px directional surround");
        std::array<float, 4096> samples;
        auto feed = [&] (float hz)
        {
            for (int i = 0; i < (int) samples.size(); ++i)
                samples[(size_t) i] = hz == 0 ? 0.0f : 0.4f * std::sin (juce::MathConstants<float>::twoPi * hz * i / 44100.0f);
            engine->pushTunerSamples (samples.data(), (int) samples.size());
            for (int i = 0; i < 40; ++i) tuner->tick();
            return tuner->createComponentSnapshot (tuner->getLocalBounds());
        };
        const auto tuned = feed (440.0f), sharp = feed (448.0f), silent = feed (0);
        check (tuned.getPixelAt (155, 100) == Theme::green
               && sharp.getPixelAt (155, 100) == Theme::orange
               && silent.getPixelAt (155, 100) == Theme::dim,
               "tuner production paint via real detector: in-tune green, out-of-tune orange and timed silence marker states");
        bool sameRim = true;
        for (int x = 0; x < 310; ++x)
            for (int y : { 0, 1, 2, 193, 194, 195 })
                sameRim &= image.getPixelAt (x, y) == tuned.getPixelAt (x, y)
                           && image.getPixelAt (x, y) == sharp.getPixelAt (x, y)
                           && image.getPixelAt (x, y) == silent.getPixelAt (x, y);
        check (sameRim, "tuner production paint: material independent of active/inactive content");
    }
    if (master != nullptr && tuner != nullptr && fader != nullptr && db != nullptr)
        for (int h : { 238, 208, 206 })
        {
            panel.setSize (440, h);
            const int fh = h - 73;
            check (master->getBounds() == juce::Rectangle<int> (331, 5, 104, h - 10)
                   && tuner->getBounds() == juce::Rectangle<int> (13, 5, 310, h - 10)
                   && fader->getBounds() == juce::Rectangle<int> (61, 30, 24, fh)
                   && db->getBounds() == juce::Rectangle<int> (14, h - 38, 76, 20)
                   && fader->getPositionOfValue (0) > fader->getPositionOfValue (1)
                   && fader->getPositionOfValue (0) <= fh && fader->getPositionOfValue (1) >= 0,
                   "mixer resized: actual fader travel/readout and tuner bounds h=" + juce::String (h));
            auto hit = panel.getLocalPoint (fader, fader->getLocalBounds().getCentre());
            check (panel.getComponentAt (hit) == fader
                   && panel.getComponentAt (tuner->getBounds().getTopLeft().translated (260, 16)) == tuner,
                   "mixer resized: real master and tuner-reference hit targets h=" + juce::String (h));
            for (double value : { 0.0, 0.5, 1.0 })
            {
                fader->setValue (value, juce::sendNotificationSync);
                check (std::abs (engine->masterVolume.load() - faderToGain ((float) value)) < 0.0001f,
                       "mixer resized: real gain callback mapping h=" + juce::String (h) + " value=" + juce::String (value));
            }
            const auto image = tuner->createComponentSnapshot (tuner->getLocalBounds());
            check (image.getPixelAt (155, 100) == Theme::dim && tuner->getRef() == 440
                   && image.getPixelAt (20, h - 16) == Theme::deep,
                   "mixer resized: tuner marker/reference retained, full-height well h=" + juce::String (h));
        }
    panel.setLookAndFeel (nullptr);
}

inline void trackMaterialSelfTest (const std::function<void (bool, const juce::String&)>& check)
{
    const Theme::Preset saved { "live", Theme::blue.getARGB(), Theme::purple.getARGB(), Theme::cyan.getARGB(),
                               Theme::bg.getARGB(), Theme::panel.getARGB(), Theme::text.getARGB() };
    const bool legacy = Theme::legacyTracks;
    for (int theme : { 2, 0 }) // Sunset and Tokyo Night
    {
        Theme::applyPreset (Theme::presets[(size_t) theme], theme == 0);
        auto engine = std::make_unique<AudioEngine>();
        engine->prepareForTest (48000.0, 2);
        const int slot = engine->addTrack();
        RiffLookAndFeel laf;
        TrackRow row (*engine, slot);
        row.setLookAndFeel (&laf);
        row.setSize (900, 96);
        int trackControls = 0, trackKnobs = 0;
        for (auto* c : row.getChildren())
        {
            if (c->getProperties().getWithDefault ("trackControl", false)) ++trackControls;
            if (c->getProperties().getWithDefault ("trackKnob", false)) ++trackKnobs;
        }
        check (trackControls == 7 && trackKnobs == 2,
               "track panel: header and take controls opt into material - " + Theme::presets[(size_t) theme].name);
        auto render = [&] { return row.createComponentSnapshot (row.getLocalBounds()); };
        const auto image = render();
        const int rimX = image.getWidth() - 1;
        const auto rim = image.getPixelAt (rimX, 10);
        const auto dark = image.getPixelAt (rimX - 2, 10);
        const auto plateTop = image.getPixelAt (180, 11);
        const auto plateBottom = image.getPixelAt (180, 35);
        const auto plateShadow = image.getPixelAt (180, 37);
        const auto header = image.getPixelAt (180, 41);
        check (rim.getPerceivedBrightness() > dark.getPerceivedBrightness()
               && rim.getPerceivedBrightness() > header.getPerceivedBrightness(),
               "track rim: localized track-colour edge with darker neighbour - " + Theme::presets[(size_t) theme].name);
        check (plateTop.getPerceivedBrightness() > plateBottom.getPerceivedBrightness()
               && plateShadow.getPerceivedBrightness() < header.getPerceivedBrightness(),
               "track nameplate: top highlight and contact shadow - " + Theme::presets[(size_t) theme].name);
        check (image.getPixelAt (30, 15).getPerceivedBrightness() > image.getPixelAt (30, 80).getPerceivedBrightness(),
               "track panel: card face keeps a restrained top-to-bottom shading - " + Theme::presets[(size_t) theme].name);
        check (image.getPixelAt (83, 53).getPerceivedBrightness() > image.getPixelAt (83, 77).getPerceivedBrightness(),
               "track controls: raised top bevel and lower contact - " + Theme::presets[(size_t) theme].name);
        check (image.getPixelAt (154, 56).getPerceivedBrightness() > image.getPixelAt (170, 74).getPerceivedBrightness(),
               "track knobs: metallic face with directional light - " + Theme::presets[(size_t) theme].name);
        if (theme == 0) // Tokyo Night track 1 is blue, so armed red is unambiguous
        {
            check (rim.getBlue() > rim.getRed(), "track rim: unarmed uses the live track colour - Tokyo Night");
            engine->tracks[slot].armed = true;
            row.syncArm();
            const auto armed = render();
            juce::Colour armedRim = juce::Colours::black;
            float redDest = -1.0f;
            for (int x = 8; x < armed.getWidth() - 8; ++x)     // glint positions follow the panel width
            {
                const auto c = armed.getPixelAt (x, 6);
                const float d = (float) c.getRed() - (float) c.getBlue();
                if (d > redDest) { redDest = d; armedRim = c; }
            }
            check (redDest > 20.0f && armedRim != rim,
                   "track rim: armed card keeps the semantic red - Tokyo Night");
            engine->tracks[slot].armed = false;
        }
        row.setLookAndFeel (nullptr);
    }
    Theme::applyPreset (saved, legacy);
}

inline void toolbarMaterialSelfTest (const std::function<void (bool, const juce::String&)>& check)
{
    const Theme::Preset saved { "live", Theme::blue.getARGB(), Theme::purple.getARGB(), Theme::cyan.getARGB(),
                               Theme::bg.getARGB(), Theme::panel.getARGB(), Theme::text.getARGB() };
    const bool legacy = Theme::legacyTracks;
    for (int theme : { 2, 0 }) // Sunset and Tokyo Night
    {
        Theme::applyPreset (Theme::presets[(size_t) theme], theme == 0);
        const auto name = Theme::presets[(size_t) theme].name;
        juce::Image image (juce::Image::ARGB, 600, 130, true);
        {
            juce::Graphics g (image);
            Theme::toolbarEnclosure (g, { 10.0f, 10.0f, 580.0f, 110.0f }, { 10.0f, 58.0f, 580.0f, 62.0f });
            Theme::toolbarReadoutWell (g, { 30.0f, 22.0f, 64.0f, 24.0f }, 5.0f);
        }
        const auto topEdge = image.getPixelAt (300, 11);
        const auto face = image.getPixelAt (300, 40);
        const auto row = image.getPixelAt (300, 75);
        check (topEdge.getPerceivedBrightness() > face.getPerceivedBrightness()
               && face.getPerceivedBrightness() > row.getPerceivedBrightness(),
               "toolbar: pale upper rim, lit face and darker settings row - " + name);
        float glintMin = 1.0f, glintMax = 0.0f;
        for (int x = 14; x <= 586; ++x)
        {
            const float v = image.getPixelAt (x, 11).getPerceivedBrightness();
            glintMin = juce::jmin (glintMin, v);
            glintMax = juce::jmax (glintMax, v);
        }
        check (glintMax - glintMin > 0.08f && glintMax > face.getPerceivedBrightness(),
               "toolbar: localized glints instead of a uniform top line - " + name);
        check (Theme::toolbarRim().getBlue() >= Theme::toolbarRim().getRed()
               && Theme::toolbarRim().getPerceivedBrightness() > Theme::toolbarContact().getPerceivedBrightness() + 0.25f,
               "toolbar: cool exposed rim is separated from the dark contact - " + name);
        check (image.getPixelAt (62, 34).getPerceivedBrightness() < face.getPerceivedBrightness(),
               "toolbar: BPM readout is a recessed dark well - " + name);

        RiffLookAndFeel laf;
        IconButton play (IconButton::Play, Theme::green);
        play.setLookAndFeel (&laf);
        play.setSize (44, 40);
        const auto off = play.createComponentSnapshot (play.getLocalBounds());
        play.setToggleState (true, juce::dontSendNotification);
        const auto on = play.createComponentSnapshot (play.getLocalBounds());
        check (Theme::materialControl (play)
               && off.getPixelAt (22, 3).getPerceivedBrightness() > off.getPixelAt (22, 37).getPerceivedBrightness(),
               "toolbar transport: material opt-in with raised top bevel and lower contact - " + name);
        check (on.getPixelAt (34, 20).getGreen() > on.getPixelAt (34, 20).getRed() + 40
               && on.getPixelAt (34, 20) != off.getPixelAt (34, 20),
               "toolbar transport: active fill keeps its semantic colour - " + name);

        ToolButton undo (ToolButton::Undo);
        undo.setLookAndFeel (&laf);
        undo.setSize (32, 28);
        const auto undoShot = undo.createComponentSnapshot (undo.getLocalBounds());
        check (Theme::materialControl (undo)
               && undoShot.getPixelAt (16, 2).getPerceivedBrightness() > undoShot.getPixelAt (16, 26).getPerceivedBrightness(),
               "toolbar tools: material opt-in with directional light - " + name);

        juce::TextButton tap ("Tap");
        tap.getProperties().set ("toolbarControl", true);
        tap.setLookAndFeel (&laf);
        tap.setSize (48, 28);
        const auto tapShot = tap.createComponentSnapshot (tap.getLocalBounds());
        check (Theme::materialControl (tap)
               && tapShot.getPixelAt (24, 2).getPerceivedBrightness() > tapShot.getPixelAt (24, 26).getPerceivedBrightness(),
               "toolbar text buttons: material opt-in with directional light - " + name);
        tap.setLookAndFeel (nullptr);
        undo.setLookAndFeel (nullptr);
        play.setLookAndFeel (nullptr);
    }
    Theme::applyPreset (saved, legacy);
}

// Headless engine check, run with: RiffCatch.exe --selftest   (writes selftest.txt next to the exe)
inline juce::String runEngineSelfTest()
{
    juce::String log;
    bool ok = true;
    auto check = [&] (bool c, const juce::String& what) { log << (c ? "PASS " : "FAIL ") << what << "\n"; ok &= c; };
    bassLightingSelfTest (check);
    drumMaterialSelfTest (check);
    trackMaterialSelfTest (check);
    toolbarMaterialSelfTest (check);
    BassLightPocTest::run (check);
    bassControlSelfTest (check);
    levelMeterSelfTest (check);
    mixerMaterialSelfTest (check);
    bottomRowSelfTest (check);

    auto eng = std::make_unique<AudioEngine>();
    const double sr = 48000.0;
    eng->prepareForTest (sr, 2);
    eng->bpm = 120.0; eng->loopBars = 1; eng->loopEnabled = true; eng->countInBars = 1;
    eng->clickEnabled = true;
    eng->latencyAdjustMs = 10.0f;                       // 480 samples of compensation @48k
    const int comp = eng->getCompensationSamples();
    check (comp == 480, "latency compensation = 480 samples (got " + juce::String (comp) + ")");
    const int slot = eng->addTrack();
    eng->tracks[slot].armed = true;
    eng->tracks[slot].volume = 0.0f;   // mute monitoring so only click energy is measured on the output

    const int loopLen = (int) eng->getLoopLength();     // 1 bar @120 = 96000
    check (loopLen == 96000, "loop length = 96000 samples (got " + juce::String (loopLen) + ")");
    check (eng->record(), "record() accepted");
    check (eng->getState() == AudioEngine::CountIn, "state is CountIn");
    check (eng->getCountInLength() == 96000, "count-in = 4 beats = 96000 samples");

    constexpr int block = 480;
    std::vector<float> inBuf (block), outL (block), outR (block);
    const float* ins[2] = { inBuf.data(), inBuf.data() };
    float* outs[2] = { outL.data(), outR.data() };
    juce::int64 global = 0;
    double countInEnergy = 0.0;
    bool recordingStartedAtBeat = false;
    juce::AudioIODeviceCallbackContext ctx;

    auto runBlocks = [&] (juce::int64 samples)
    {
        for (juce::int64 done = 0; done < samples; done += block)
        {
            for (int i = 0; i < block; ++i) inBuf[(size_t) i] = (float) (global + i) * 1.0e-7f;   // unique ramp
            eng->audioDeviceIOCallbackWithContext (ins, 2, outs, 2, block, ctx);
            if (eng->getState() == AudioEngine::CountIn)
                for (int i = 0; i < block; ++i) countInEnergy += std::abs (outL[(size_t) i]);
            global += block;
            if ((global / block) % 5 == 0) eng->pollRecording();
        }
    };

    runBlocks (96000 - block);
    check (eng->getState() == AudioEngine::CountIn, "still counting in just before the end");
    check (countInEnergy > 1.0, "click audible during count-in (energy " + juce::String (countInEnergy, 1) + ")");
    runBlocks (block);                                   // last count-in block
    const juce::int64 recStart = global;                 // first recorded sample = global index at start of pass 1
    runBlocks (block);                                   // first recorded block
    recordingStartedAtBeat = eng->getState() == AudioEngine::Recording && eng->getPosition() == block;
    check (recordingStartedAtBeat, "recording starts on the first sample after count-in, at bar 1 (pos " + juce::String (eng->getPosition()) + ")");

    runBlocks ((juce::int64) loopLen * 2 + 24000);       // two full passes + a partial third
    check (eng->getState() == AudioEngine::Recording, "still recording after 2.25 passes");
    eng->stop();
    runBlocks (block * 20);
    eng->pollRecording(); eng->pollRecording(); eng->pollRecording();

    auto& tr = eng->tracks[slot];
    check (tr.length.load() == loopLen, "track length = exactly one loop (" + juce::String (tr.length.load()) + ")");
    check ((int) tr.takes.size() == 2, "two completed passes kept as takes (" + juce::String ((int) tr.takes.size()) + ")");
    const float expected = (float) (recStart + loopLen + comp) * 1.0e-7f;   // pass 2 starts one loop after pass 1
    check (tr.length.load() == loopLen && std::abs (tr.data.getSample (0, 0) - expected) < 1.0e-4f,
           "last completed pass wins, shifted earlier by latency compensation; partial pass 3 discarded");
    tr.applyTake (0);
    check (std::abs (tr.data.getSample (0, 0) - (float) (recStart + comp) * 1.0e-7f) < 1.0e-4f, "take 1 can be restored");

    tr.volume = 1.0f;
    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("rc_selftest_export.wav");
    juce::String err;
    check (Exporter::bounceWav (*eng, tmp, 2, err), "bounceWav succeeds " + err);
    check (tmp.getSize() > 2 * loopLen * 2 * 3, "exported WAV has 2 loops of 24-bit stereo (" + juce::String (tmp.getSize()) + " bytes)");
    tmp.deleteFile();
    if (Exporter::mp3Available)
    {
        auto mp3 = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("rc_selftest_export.mp3");
        juce::String e2;
        check (Exporter::bounceMp3 (*eng, mp3, 2, 192, e2), "bounceMp3 succeeds " + e2);
        juce::MemoryBlock mb; mp3.loadFileAsData (mb);
        const auto* b = (const unsigned char*) mb.getData();
        const bool sync = mb.getSize() > 4 && ((b[0] == 0xFF && (b[1] & 0xE0) == 0xE0) || (b[0] == 'I' && b[1] == 'D' && b[2] == '3'));
        // 2 loops @120 BPM, 1 bar each = 4 s at 192 kbps = ~96 kB
        check (sync && mb.getSize() > 60000 && mb.getSize() < 130000, "MP3 has valid frame sync and plausible size (" + juce::String ((int) mb.getSize()) + " bytes)");
        mp3.deleteFile();
    }

    // ---- hit detection + re-timing
    {
        const int len = (int) (sr * 3.0);
        juce::AudioBuffer<float> take (2, len);
        take.clear();
        const double times[] = { 0.50, 1.04, 1.50, 2.03 };      // two hits are ~35 ms off the 0.5 s grid
        for (double tm : times)
        {
            const int s0 = (int) (tm * sr);
            for (int i = 0; i < (int) (0.4 * sr) && s0 + i < len; ++i)
            {
                const float v = std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) i / (float) sr)
                                * std::exp (-(float) i / (float) (0.12 * sr)) * 0.7f;
                take.setSample (0, s0 + i, v); take.setSample (1, s0 + i, v);
            }
        }
        auto found = HitEditor::detectOnsets (take, sr, 6.0f);
        check (found.size() == 4, "onset detection found 4 hits (" + juce::String ((int) found.size()) + ")");
        bool near = found.size() == 4;
        for (size_t i = 0; i < found.size() && i < 4; ++i)
            near &= std::abs ((double) found[i] / sr - times[i]) < 0.01;
        check (near, "detected positions within 10 ms of the real hits");

        std::vector<Hit> hits;
        const double grid[] = { 0.50, 1.00, 1.50, 2.00 };
        for (size_t i = 0; i < found.size(); ++i) hits.push_back ({ found[i], (juce::int64) (grid[i] * sr) });
        auto edited = HitEditor::render (take, hits, sr);
        check (edited.getNumSamples() == len, "re-timed take keeps its length");
        bool onGrid = true, finite = true;
        for (int i = 0; i < 4; ++i)
        {
            const int g = (int) (grid[i] * sr);
            int first = -1;
            for (int s = g - (int) (0.06 * sr); s < g + (int) (0.06 * sr); ++s)
                if (std::abs (edited.getSample (0, s)) > 0.05f) { first = s; break; }
            onGrid &= first >= 0 && std::abs (first - g) < (int) (0.004 * sr);   // within 4 ms
        }
        for (int s = 0; s < len; ++s) finite &= std::isfinite (edited.getSample (0, s));
        check (onGrid, "every moved hit now sounds on the grid (within 4 ms)");
        check (finite, "re-timed audio has no NaN/inf");
    }

    // ---- playhead cursor / seek
    {
        AudioEngine e3;
        e3.prepareForTest (sr, 2);
        e3.loopBars = 4;
        e3.addTrack();
        std::vector<float> z (block, 0.0f), l (block), r (block);
        const float* zi[2] = { z.data(), z.data() };
        float* zo[2] = { l.data(), r.data() };
        juce::AudioIODeviceCallbackContext c3;
        auto run = [&] { e3.audioDeviceIOCallbackWithContext (zi, 2, zo, 2, block, c3); };
        e3.seek (48000);
        run();
        check (e3.getPosition() == 48000 && e3.getState() == AudioEngine::Stopped, "seek while stopped moves the stopped playhead to the cursor");
        e3.play(); run();
        check (e3.getPosition() == 48000 + block, "play starts from the cursor (pos " + juce::String (e3.getPosition()) + ")");
        e3.seek (1000); run();
        check (e3.getPosition() == 1000 + block, "seek while playing jumps immediately");
        e3.stop(); run();
        check (e3.getPosition() == 1000, "stop returns the playhead to the cursor");
        e3.setCursor (0); run();
        check (e3.getPosition() == 0, "setCursor(0) rewinds");
    }

    // ---- undo building blocks
    {
        AudioEngine e2;
        e2.prepareForTest (sr, 2);
        const int sl = e2.addTrackAt (3);
        check (sl == 3 && e2.order.size() == 1, "addTrackAt reuses the requested slot");
        auto& t = e2.tracks[sl];
        juce::AudioBuffer<float> a (2, 1000), b (2, 1000);
        a.clear(); b.clear();
        for (int i = 0; i < 1000; ++i) { a.setSample (0, i, 0.25f); b.setSample (0, i, 0.5f); }
        const auto p1 = t.commitTake (a, 1000);
        check (p1.first == -1 && p1.second == 0 && t.length.load() == 1000, "first commit reports prev=-1, new=0");
        const auto p2 = t.commitTake (b, 1000);
        check (p2.first == 0 && p2.second == 1 && std::abs (t.data.getSample (0, 500) - 0.5f) < 1.0e-6f, "second commit reports prev=0, new=1");
        t.applyTake (p2.first);
        check (std::abs (t.data.getSample (0, 500) - 0.25f) < 1.0e-6f, "undo of a take restores the previous take");
        t.applyTake (p2.second);
        check (std::abs (t.data.getSample (0, 0)) < 0.05f && std::abs (t.data.getSample (0, 999)) < 0.05f, "loop seam is faded (no click at take start/end)");
        {   // region editing: copy, silence, restore
            auto reg = t.copyRegion (100, 200);
            check (reg.getNumSamples() == 100 && std::abs (reg.getSample (0, 50) - 0.5f) < 1.0e-6f, "copyRegion returns the selected samples");
            juce::AudioBuffer<float> z (2, 100); z.clear();
            t.writeRegion (100, z);
            check (t.data.getSample (0, 150) == 0.0f && std::abs (t.data.getSample (0, 99) - 0.5f) < 1.0e-6f && std::abs (t.data.getSample (0, 200) - 0.5f) < 1.0e-6f, "writeRegion silences only the selection");
            t.writeRegion (100, reg);
            check (std::abs (t.data.getSample (0, 150) - 0.5f) < 1.0e-6f, "writeRegion restores the region (undo)");
            t.writeRegion (950, reg);
            check (t.length.load() == 1000, "writeRegion never extends the take");
        }
        check (std::abs (t.data.getSample (0, 500) - 0.5f) < 1.0e-6f, "redo of a take brings the new take back");

        juce::UndoManager um;
        int value = 1;
        um.beginNewTransaction ("set");
        um.perform (new LambdaAction ([&] { value = 2; }, [&] { value = 1; }));
        check (value == 2, "perform applies the change");
        um.undo();
        check (value == 1, "undo reverts it");
        um.redo();
        check (value == 2, "redo re-applies it");

        t.silence();
        check (t.length.load() == 0 && t.takes.size() == 2, "silence() mutes the track but keeps take history");
        e2.removeTrack (sl);
        check (e2.order.empty() && ! t.active.load(), "removeTrack frees the slot");
    }

    // ---- clips: record from a later bar, whole-bar length, playback without wrap
    {
        std::vector<float> in5 (block, 0.25f), l5 (block), r5 (block);
        const float* ins5[2] = { in5.data(), in5.data() };
        float* outs5[2] = { l5.data(), r5.data() };
        juce::AudioIODeviceCallbackContext c5;

        AudioEngine e5;
        e5.prepareForTest (sr, 2);
        e5.bpm = 120.0; e5.loopBars = 4; e5.loopEnabled = false; e5.countInBars = 0; e5.clickEnabled = false;
        const int s5 = e5.addTrack();
        auto& t5 = e5.tracks[s5];
        t5.armed = true; t5.volume = 0.0f;
        const juce::int64 bar = e5.getBarLength();
        e5.setCursor (2 * bar);
        check (e5.record(), "record from bar 3 accepted");
        check (e5.getRecordStart() == 2 * bar, "recording starts at the bar of the playhead (bar 3)");
        for (juce::int64 i = 0; i * block < bar * 6 / 5; ++i) e5.audioDeviceIOCallbackWithContext (ins5, 2, outs5, 2, block, c5);
        e5.stop();
        for (int i = 0; i < 4; ++i) e5.audioDeviceIOCallbackWithContext (ins5, 2, outs5, 2, block, c5);
        for (int i = 0; i < 3; ++i) e5.pollRecording();
        check (t5.offset.load() == 2 * bar, "clip offset = recording start (" + juce::String (t5.offset.load()) + ")");
        check (t5.length.load() == bar * 2, "1.2 bars recorded are rounded up to 2 whole bars (" + juce::String (t5.length.load()) + ")");
        check (std::abs (t5.data.getSample (0, 5000) - 0.25f) < 1.0e-4f && std::abs (t5.data.getSample (0, (int) (bar * 2) - 10)) < 1.0e-6f,
               "recorded audio is at the start of the clip, padded with silence at the end");

        t5.armed = false; t5.volume = 1.0f;
        e5.setCursor (0);
        e5.play();
        double before = 0.0, after = 0.0;
        for (int i = 0; (juce::int64) (i + 1) * block < 2 * bar + 20000; ++i)
        {
            e5.audioDeviceIOCallbackWithContext (ins5, 2, outs5, 2, block, c5);
            const juce::int64 end = (juce::int64) (i + 1) * block;
            for (int k = 0; k < block; ++k)
            {
                if (end <= 2 * bar) before += std::abs (l5[(size_t) k]);
                else if (end > 2 * bar + 1000) after += std::abs (l5[(size_t) k]);
            }
        }
        check (before == 0.0, "nothing plays before the clip start (no wrapping)");
        check (after > 100.0, "the clip plays from its start position (energy " + juce::String (after, 1) + ")");

        // loop mode: the pass window runs from the chosen bar to the loop end
        AudioEngine e6;
        e6.prepareForTest (sr, 2);
        e6.bpm = 120.0; e6.loopBars = 4; e6.loopEnabled = true; e6.countInBars = 0; e6.clickEnabled = false;
        const int s6 = e6.addTrack();
        auto& t6 = e6.tracks[s6];
        t6.armed = true; t6.volume = 0.0f;
        e6.setCursor (2 * bar);
        e6.record();
        for (juce::int64 i = 0; i < (2 * bar) / block + 4; ++i) e6.audioDeviceIOCallbackWithContext (ins5, 2, outs5, 2, block, c5);
        e6.stop();
        for (int i = 0; i < 4; ++i) e6.audioDeviceIOCallbackWithContext (ins5, 2, outs5, 2, block, c5);
        for (int i = 0; i < 3; ++i) e6.pollRecording();
        check (t6.length.load() == 2 * bar && t6.offset.load() == 2 * bar && t6.takes.size() == 1,
               "loop recording from bar 3 keeps bars 3-4 as one clip (len " + juce::String (t6.length.load()) + ")");
    }

    // ---- live bass capture: one whole loop pass, starting at the loop start
    {
        std::vector<float> zin (block, 0.0f), ol (block), orr (block);
        const float* zi[2] = { zin.data(), zin.data() };
        float* zo[2] = { ol.data(), orr.data() };
        juce::AudioIODeviceCallbackContext cc;
        auto ecP = std::make_unique<AudioEngine>(); auto& ec = *ecP;
        ec.prepareForTest (44100.0, 2);
        ec.bpm = 120.0; ec.loopBars = 1; ec.loopEnabled = true; ec.countInBars = 0; ec.clickEnabled = false;
        ec.bass.volume = 1.0f;
        ec.bass.steps[0] = BassMachine::pack (5, 4, false, false);
        ec.play();
        for (int i = 0; i < 20000 / block; ++i) ec.audioDeviceIOCallbackWithContext (zi, 2, zo, 2, block, cc);   // mid-loop
        check (ec.startBassCapture() && ec.bassCaptureState() == 1, "bass capture arms while looping");
        for (int i = 0; i < 200000 / block && ec.bassCaptureState() != 3; ++i) ec.audioDeviceIOCallbackWithContext (zi, 2, zo, 2, block, cc);
        juce::AudioBuffer<float> got;
        const bool took = ec.takeBassCapture (got);
        check (took && got.getNumSamples() == (int) ec.getLoopLength() && ec.bassCaptureState() == 0, "bass capture delivers exactly one loop");
        float first = 0.0f, quiet = 0.0f;
        for (int i = 2000; i < 6000 && took; ++i) first = std::max (first, std::abs (got.getSample (0, i)));
        for (int i = 40000; i < 44000 && took; ++i) quiet = std::max (quiet, std::abs (got.getSample (0, i)));
        check (first > 0.05f && quiet < 0.001f, "bass capture starts at the loop start and holds the pattern (" + juce::String (first, 3) + " / " + juce::String (quiet, 4) + ")");
        ec.loopEnabled = false;
        check (! ec.startBassCapture(), "bass capture needs the loop");

        // from standstill: count-in first, then the pass is captured from the loop start
        auto ecnP = std::make_unique<AudioEngine>(); auto& ecn = *ecnP;
        ecn.prepareForTest (44100.0, 2);
        ecn.bpm = 120.0; ecn.loopBars = 1; ecn.loopEnabled = true; ecn.countInBars = 1; ecn.clickEnabled = false;
        ecn.bass.volume = 1.0f;
        ecn.bass.steps[0] = BassMachine::pack (5, 4, false, false);
        check (ecn.startBassCapture() && ecn.getState() == AudioEngine::CountIn, "bass capture starts with a count-in from standstill");
        int blocks = 0;
        while (ecn.bassCaptureState() != 3 && blocks++ < 400000 / block) ecn.audioDeviceIOCallbackWithContext (zi, 2, zo, 2, block, cc);
        juce::AudioBuffer<float> got2;
        const bool took2 = ecn.takeBassCapture (got2);
        float f2 = 0.0f;
        for (int i = 2000; i < 6000 && took2; ++i) f2 = std::max (f2, std::abs (got2.getSample (0, i)));
        check (took2 && f2 > 0.05f && blocks * block >= 88200 + 80000 && ecn.getState() == AudioEngine::Playing, "bass capture after the count-in holds a full pass and playback continues");
    }

    // ---- bass panel: click adds a note, dragging right makes it longer
    {
        auto epP = std::make_unique<AudioEngine>(); auto& ep = *epP;
        ep.prepareForTest (44100.0, 2);
        BassPanel panel (ep);
        panel.setSize (1500, 210);
        panel.setView (8.0, 2.0, 0.0, 8.0);
        bassKnobSelfTest (panel, check);
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        auto ev = [&] (juce::Point<float> p, juce::Point<float> down, bool dragged)
        {
            return juce::MouseEvent (src, p, juce::ModifierKeys::currentModifiers.withoutMouseButtons().withFlags (juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                     &panel, &panel, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, dragged);
        };
        const float x0 = (float) TrackRow::waveLeft + 5.0f, y0 = (float) (BassPanel::headerH + BassPanel::shiftH + 5 * BassPanel::rowH + 5);
        const float stepPx = (float) (1500 - TrackRow::waveLeft - TrackRow::waveRightInset) / 64.0f;     // 2 s per bar, 8 s view = 4 bars of 16 steps
        panel.mouseDown (ev ({ x0, y0 }, { x0, y0 }, false));
        panel.mouseDrag (ev ({ x0 + 3.0f * stepPx, y0 }, { x0, y0 }, true));
        panel.mouseUp (ev ({ x0 + 3.0f * stepPx, y0 }, { x0, y0 }, true));
        const auto c = ep.bass.steps[0].load();
        check (BassMachine::isNote (c) && BassMachine::lenOf (c) == 4, "bass panel: dragging right lengthens the new note (len " + juce::String (BassMachine::lenOf (c)) + ")");

        // resize an existing note by its right edge; the pressed note keeps its start
        const float cellRight = (float) TrackRow::waveLeft + 4.0f * stepPx;       // note covers steps 0-3
        panel.mouseDown (ev ({ cellRight - 3.0f, y0 }, { cellRight - 3.0f, y0 }, false));
        panel.mouseDrag (ev ({ (float) TrackRow::waveLeft + 8.5f * stepPx, y0 }, { cellRight, y0 }, true));
        panel.mouseUp (ev ({ (float) TrackRow::waveLeft + 8.5f * stepPx, y0 }, { cellRight, y0 }, true));
        check (BassMachine::lenOf (ep.bass.steps[0].load()) == 9, "bass panel: dragging the right edge of a note resizes it (len " + juce::String (BassMachine::lenOf (ep.bass.steps[0].load())) + ")");

        // wipe: press on an existing note and drag over others in the row
        ep.bass.bars = 4;
        for (int k = 20; k <= 26; k += 2) ep.bass.steps[k] = BassMachine::pack (3, 1, false, false);
        ep.bass.steps[22] = BassMachine::pack (4, 1, false, false);                // other row: must survive
        const float wx = (float) TrackRow::waveLeft;
        panel.mouseDown (ev ({ wx + 20.0f * stepPx + 3.0f, y0 }, { wx, y0 }, false));
        panel.mouseDrag (ev ({ wx + 26.0f * stepPx + 3.0f, y0 }, { wx, y0 }, true));
        panel.mouseUp (ev ({ wx + 26.0f * stepPx + 3.0f, y0 }, { wx, y0 }, true));
        check (! BassMachine::isNote (ep.bass.steps[20].load()) && ! BassMachine::isNote (ep.bass.steps[24].load()) && ! BassMachine::isNote (ep.bass.steps[26].load())
               && BassMachine::isNote (ep.bass.steps[22].load()), "bass panel: dragging from a note wipes notes in that row only");

        // chromatic scale: 13 rows, every semitone from the root up an octave
        ep.bass.scale = 6; ep.bass.root = 36; ep.bass.bars = 4;
        panel.refreshFromEngine();
        const float yBottom = (float) (BassPanel::headerH + BassPanel::shiftH + BassPanel::gridH - 3), yTop = (float) (BassPanel::headerH + BassPanel::shiftH + 3);
        panel.mouseDown (ev ({ x0 + 50.0f * stepPx, yBottom }, { x0, yBottom }, false)); panel.mouseUp (ev ({ x0 + 50.0f * stepPx, yBottom }, { x0, yBottom }, false));
        panel.mouseDown (ev ({ x0 + 52.0f * stepPx, yTop }, { x0, yTop }, false)); panel.mouseUp (ev ({ x0 + 52.0f * stepPx, yTop }, { x0, yTop }, false));
        panel.mouseDown (ev ({ x0 + 54.0f * stepPx, (float) (BassPanel::headerH + BassPanel::shiftH + BassPanel::gridH - 3 - 11 * 3) }, { x0, yTop }, false));
        panel.mouseUp (ev ({ x0 + 54.0f * stepPx, (float) (BassPanel::headerH + BassPanel::shiftH + BassPanel::gridH - 3 - 11 * 3) }, { x0, yTop }, false));
        check (BassMachine::degreeOf (ep.bass.steps[50].load()) == 0 && BassMachine::degreeOf (ep.bass.steps[52].load()) == 12 && BassMachine::degreeOf (ep.bass.steps[54].load()) == 3,
               "bass panel: chromatic scale shows C2 to C3 (degrees " + juce::String (BassMachine::degreeOf (ep.bass.steps[50].load())) + ", " + juce::String (BassMachine::degreeOf (ep.bass.steps[52].load())) + ", " + juce::String (BassMachine::degreeOf (ep.bass.steps[54].load())) + ")");
        check (ep.bass.root.load() + BassMachine::semitones (6, 12) == 48, "chromatic: root C gives C2 up to C3 (MIDI 36 to 48)");
        ep.bass.scale = 0; ep.bass.root = 33; panel.refreshFromEngine();

        // plain click on a note removes it
        ep.bass.steps[40] = BassMachine::pack (3, 3, false, false);
        panel.mouseDown (ev ({ wx + 41.0f * stepPx, y0 }, { wx, y0 }, false));
        panel.mouseUp (ev ({ wx + 41.0f * stepPx, y0 }, { wx, y0 }, false));
        check (! BassMachine::isNote (ep.bass.steps[40].load()), "bass panel: clicking inside a long note removes it");
    }

    // ---- riff analysis: tempo, key, chords, rhythm, bass suggestions (synthetic guitar-like signals)
    {
        const double sr = 44100.0, spb = sr * 0.5;                  // 120 BPM
        const int barLen = (int) (spb * 4.0);
        auto tone = [&] (juce::AudioBuffer<float>& b, double f, int start, int lenS)
        {
            for (int i = 0; i < lenS && start + i < b.getNumSamples(); ++i)
            {
                const double t = i / sr;
                double v = 0.0;
                for (int h = 1; h <= 4; ++h) v += std::sin (2.0 * juce::MathConstants<double>::pi * f * h * t) / h;
                v *= std::exp (-t * 5.0) * std::min (1.0, t / 0.003) * 0.25;
                b.addSample (0, start + i, (float) v); b.addSample (1, start + i, (float) v);
            }
        };
        // chord riff: E5 E5 C5 D5, power chords hit on every quarter note
        juce::AudioBuffer<float> riff (2, barLen * 4);
        riff.clear();
        const double bassHz[4] = { 82.41, 82.41, 65.41, 73.42 }, fifthHz[4] = { 123.47, 123.47, 98.0, 110.0 };
        for (int bar = 0; bar < 4; ++bar)
            for (int q = 0; q < 4; ++q)
            {
                tone (riff, bassHz[bar], bar * barLen + (int) (q * spb), (int) spb);
                tone (riff, fifthHz[bar], bar * barLen + (int) (q * spb), (int) spb);
            }
        const auto ra = Analysis::analyse (riff, riff.getNumSamples(), 0, sr, spb, 4.0, 8);
        check (ra.valid && ra.bars == 4, "analysis: finds four bars of audio");
        check (std::abs (Analysis::foldNear (ra.tempo, 120.0) - 120.0) < 3.0, "analysis: tempo near 120 BPM (" + juce::String (ra.tempo, 1) + ")");
        const auto chords = ra.valid ? Analysis::detectChords (ra, ra.keys[0]) : std::vector<Analysis::Chord>();
        check (chords.size() == 4 && chords[0].rootPc == 4 && chords[1].rootPc == 4 && chords[2].rootPc == 0 && chords[3].rootPc == 2,
               "analysis: chord roots E E C D per bar" + (chords.size() == 4 ? juce::String (" (got ") + Analysis::chordName (chords[0]) + " " + Analysis::chordName (chords[1]) + " " + Analysis::chordName (chords[2]) + " " + Analysis::chordName (chords[3]) + ")" : juce::String()));
        bool onsetsOk = ra.valid;
        for (int bar = 0; bar < 4 && onsetsOk; ++bar)
            onsetsOk = ra.onsets[(size_t) bar][0] > 0.2f && ra.onsets[(size_t) bar][4] > 0.2f && ra.onsets[(size_t) bar][8] > 0.2f && ra.onsets[(size_t) bar][12] > 0.2f
                       && ra.onsets[(size_t) bar][2] < 0.2f && ra.onsets[(size_t) bar][6] < 0.2f;
        check (onsetsOk, "analysis: riff rhythm lands on steps 0, 4, 8, 12");

        // pentatonic melody: E minor pentatonic with E as the home note
        juce::AudioBuffer<float> mel (2, barLen * 4);
        mel.clear();
        const double notes[8] = { 82.41, 98.0, 110.0, 82.41, 123.47, 146.83, 82.41, 164.81 };
        for (int i = 0; i < 32; ++i) tone (mel, notes[i % 8], (int) (i * spb * 0.5), (int) (spb * 0.5));
        const auto ma = Analysis::analyse (mel, mel.getNumSamples(), 0, sr, spb, 4.0, 8);
        check (ma.valid && ma.keys[0].rootPc == 4 && ma.keys[0].scale == 0, "analysis: E minor pentatonic melody -> " + (ma.valid ? Analysis::keyName (ma.keys[0]) : juce::String ("nothing")));

        // bass suggestions follow the chords
        if (ra.valid)
        {
            const auto key = ra.keys[0];
            const auto sg = BassSuggest::suggest (ra, key, chords, 16, 4, nullptr);
            check (sg.size() >= 5, "bass suggestions: at least five lines");
            bool good = ! sg.empty();
            int expectShift = (((0 - key.rootPc) % 12) + 12) % 12; if (expectShift > 6) expectShift -= 12;
            for (const auto& s : sg)
            {
                good &= s.snap.bars == 4 && s.snap.scale == key.scale && s.snap.shift[2] == expectShift;
                int notes2 = 0;
                for (int k = 0; k < 64; ++k)
                    if (BassMachine::isNote (s.snap.steps[k]))
                    {
                        ++notes2;
                        const int row = BassMachine::degreeOf (s.snap.steps[k]) - BassMachine::degreeBase;
                        good &= BassMachine::lenOf (s.snap.steps[k]) >= 1 && row > -20 && row < 40;
                    }
                good &= notes2 >= 4;
            }
            check (good, "bass suggestions: four bars, in key, transposed per chord");
            bool pedal = false;
            for (const auto& s : sg) if (s.name == "Pedal") { int c = 0; for (int k = 0; k < 64; ++k) c += BassMachine::isNote (s.snap.steps[k]) ? 1 : 0; pedal = c == 4 && BassMachine::lenOf (s.snap.steps[0]) == 16; }
            check (pedal, "bass suggestions: Pedal is one bar-long note per bar");
        }
    }

    // ---- random bass line
    {
        juce::Random rnd (1234);
        BassMachine::Snapshot cur; cur.bars = 2; cur.scale = 0; cur.root = 33;
        BassRandom::Options o; o.grid = 1; o.density = 1.0f; o.octaves = 2; o.repeatBar = true; o.randomLength = false;
        const auto full = BassRandom::generate (cur, o, rnd);
        int notes = 0; bool grid = true, inRange = true, same = true;
        for (int k = 0; k < 32; ++k)
        {
            const auto c = full.steps[k];
            if (BassMachine::isNote (c)) { ++notes; grid &= k % 2 == 0; inRange &= BassMachine::degreeOf (c) >= 0 && BassMachine::degreeOf (c) < 10; }
            else grid &= k % 2 == 1;
            if (k >= 16) same &= full.steps[k] == full.steps[k - 16];
        }
        check (notes == 16 && grid && inRange && same, "random bass: every eighth, inside two octaves of the scale, bar 1 repeated (" + juce::String (notes) + " notes)");
        o.density = 0.3f; o.randomLength = true; o.repeatBar = false; o.grid = 2;
        int sparse = 0, dense = 0; bool lensOk = true;
        for (int run = 0; run < 20; ++run)
        {
            const auto s = BassRandom::generate (cur, o, rnd);
            for (int k = 0; k < 32; ++k)
                if (BassMachine::isNote (s.steps[k]))
                {
                    ++sparse;
                    int next = k + 1; while (next < 32 && ! BassMachine::isNote (s.steps[next])) ++next;
                    lensOk &= BassMachine::lenOf (s.steps[k]) >= 1 && k % 16 + BassMachine::lenOf (s.steps[k]) <= 16 && BassMachine::lenOf (s.steps[k]) <= next - k + (next >= 32 ? 16 : 0);
                }
        }
        o.density = 1.0f;
        for (int run = 0; run < 20; ++run) { const auto s = BassRandom::generate (cur, o, rnd); for (int k = 0; k < 32; ++k) dense += BassMachine::isNote (s.steps[k]) ? 1 : 0; }
        check (sparse < dense / 2 && lensOk, "random bass: density thins the line, random lengths stay inside the gaps (" + juce::String (sparse) + " vs " + juce::String (dense) + ")");
        BassMachine::Snapshot chromatic = cur; chromatic.scale = 6;
        const auto cs = BassRandom::generate (chromatic, o, rnd);
        bool chromOk = true; for (int k = 0; k < 32; ++k) if (BassMachine::isNote (cs.steps[k])) chromOk &= BassMachine::degreeOf (cs.steps[k]) >= 0 && BassMachine::degreeOf (cs.steps[k]) < 24;
        check (chromOk, "random bass: chromatic scale stays inside the range");
    }

    // ---- drum machine: sample-accurate steps, phase kept across loop passes
    {
        std::vector<float> zin (block, 0.0f), dl (block), dr (block);
        const float* zi[2] = { zin.data(), zin.data() };
        float* zo[2] = { dl.data(), dr.data() };
        juce::AudioIODeviceCallbackContext cd;
        auto energy = [] (const std::vector<float>& v, juce::int64 a, juce::int64 b)
        {
            double e = 0.0;
            for (juce::int64 i = a; i < b && i < (juce::int64) v.size(); ++i) e += std::abs (v[(size_t) i]);
            return e;
        };

        AudioEngine ed;
        ed.prepareForTest (sr, 2);
        ed.bpm = 120.0; ed.loopBars = 1; ed.loopEnabled = true; ed.countInBars = 0; ed.clickEnabled = false;
        ed.drums.volume = 1.0f;
        ed.drums.steps[DrumMachine::Hat][0] = 2;
        ed.drums.steps[DrumMachine::Hat][4] = 2;        // beat 2 = sample 24000
        ed.play();
        std::vector<float> cap;
        for (int i = 0; i < 96000 / block; ++i)
        {
            ed.audioDeviceIOCallbackWithContext (zi, 2, zo, 2, block, cd);
            cap.insert (cap.end(), dl.begin(), dl.end());
        }
        check (energy (cap, 0, 2000) > 5.0, "drum step 1 sounds at the start of the bar");
        check (energy (cap, 12000, 23990) < 0.01, "silent between hits (hat has decayed)");
        check (energy (cap, 24000, 26000) > 5.0, "drum step 5 sounds exactly on beat 2");
        check (energy (cap, 48000, 96000) < 0.01, "no hits where the pattern has none");

        // two-bar pattern over a one-bar loop: step 16 (bar 2) must sound in the second pass only
        AudioEngine ee;
        ee.prepareForTest (sr, 2);
        ee.bpm = 120.0; ee.loopBars = 1; ee.loopEnabled = true; ee.countInBars = 0; ee.clickEnabled = false;
        ee.drums.volume = 1.0f;
        ee.drums.bars = 2;
        ee.drums.steps[DrumMachine::Snare][16] = 2;
        ee.play();
        std::vector<float> cap2;
        for (int i = 0; i < 192000 / block; ++i)
        {
            ee.audioDeviceIOCallbackWithContext (zi, 2, zo, 2, block, cd);
            cap2.insert (cap2.end(), dl.begin(), dl.end());
        }
        check (energy (cap2, 0, 96000) < 0.01, "6-over-4 style: bar 2 of the pattern is silent in loop pass 1");
        check (energy (cap2, 96000, 98000) > 5.0, "bar 2 of the pattern sounds in loop pass 2 (phase kept across passes)");

        // export renders the same drums
        AudioEngine ex;
        ex.prepareForTest (sr, 2);
        ex.bpm = 120.0; ex.loopBars = 1; ex.loopEnabled = true;
        ex.drums.volume = 1.0f;
        ex.drums.steps[DrumMachine::Kick][0] = 2;
        auto wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("rc_selftest_drums.wav");
        juce::String derr;
        check (Exporter::bounceWav (ex, wav, 1, derr), "export works with drums only " + derr);
        check (wav.getSize() > 96000 * 2 * 3, "exported drum loop has the expected length");
        wav.deleteFile();
    }

    // ---- track echo (tempo synced) and reverb
    {
        std::vector<float> zin (block, 0.0f), fl (block), fr (block);
        const float* zi[2] = { zin.data(), zin.data() };
        float* zo[2] = { fl.data(), fr.data() };
        juce::AudioIODeviceCallbackContext cf;
        auto runFx = [&] (bool echo, bool reverb, std::vector<float>& cap)
        {
            AudioEngine ef;
            ef.prepareForTest (sr, 2);
            ef.bpm = 120.0; ef.loopBars = 1; ef.loopEnabled = true; ef.countInBars = 0; ef.clickEnabled = false;
            const int sf = ef.addTrack();
            auto& tf = ef.tracks[sf];
            juce::AudioBuffer<float> imp (2, 96000);
            imp.clear();
            imp.setSample (0, 5000, 1.0f); imp.setSample (1, 5000, 1.0f);
            tf.setAudio (std::move (imp));
            tf.echoOn = echo; tf.reverbOn = reverb; tf.echoDiv = TrackFx::Quarter;   // 1/4 @120 = 24000 samples
            ef.play();
            for (int i = 0; i < 96000 / block; ++i)
            {
                ef.audioDeviceIOCallbackWithContext (zi, 2, zo, 2, block, cf);
                cap.insert (cap.end(), fl.begin(), fl.end());
            }
        };
        std::vector<float> dry, echo, rev;
        runFx (false, false, dry); runFx (true, false, echo); runFx (false, true, rev);
        float pkDry = 0.0f, pkEcho = 0.0f, tailRev = 0.0f;
        for (int i = 28990; i < 29010; ++i) { pkDry = juce::jmax (pkDry, std::abs (dry[(size_t) i])); pkEcho = juce::jmax (pkEcho, std::abs (echo[(size_t) i])); }
        for (int i = 8000; i < 25000; ++i) tailRev = juce::jmax (tailRev, std::abs (rev[(size_t) i]));
        check (pkDry < 1.0e-6f && pkEcho > 0.1f, "echo 1/4 repeats the hit exactly one beat later (peak " + juce::String (pkEcho, 3) + ")");
        check (std::abs (echo[5000]) > 0.3f && std::abs (dry[5000]) > 0.3f, "the dry hit is unchanged with echo on");
        check (tailRev > 0.001f, "reverb adds a tail after the hit (" + juce::String (tailRev, 4) + ")");
    }

    {   // punch in/out: the old clip stays outside the punch window, the new take replaces it inside
        Track tr;
        juce::AudioBuffer<float> oldB (2, 1000), newB (2, 1000);
        for (int i = 0; i < 1000; ++i) for (int c = 0; c < 2; ++c) { oldB.setSample (c, i, 0.25f); newB.setSample (c, i, 0.75f); }
        tr.data.setSize (2, 1000); tr.data.makeCopyOf (oldB); tr.length = 1000; tr.offset = 0;
        tr.commitTake (newB, 1000, 0, false, 0, 400, 600);
        const auto& d = tr.data;
        check (tr.length.load() == 1000, "punch keeps the clip length");
        check (std::abs (d.getSample (0, 100) - 0.25f) < 1.0e-5f && std::abs (d.getSample (0, 800) - 0.25f) < 1.0e-5f, "punch: old audio kept outside the window");
        check (std::abs (d.getSample (0, 500) - 0.75f) < 1.0e-3f, "punch: new audio inside the window");
    }

    {   // audio import: a mono 44.1 kHz WAV becomes stereo at the device rate
        auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("riffcatch_import_test.wav");
        f.deleteFile();
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), 44100.0, 1, 16, {}, 0));
            juce::AudioBuffer<float> b (1, 44100);
            for (int i = 0; i < 44100; ++i) b.setSample (0, i, 0.5f * std::sin (i * 0.05f));
            if (w) w->writeFromAudioSampleBuffer (b, 0, 44100);
        }
        juce::AudioBuffer<float> out; double fr = 0; juce::String err;
        const bool okRead = AudioImport::readFile (f, 48000.0, out, fr, err);
        check (okRead, "import reads the WAV");
        check (out.getNumChannels() == 2 && std::abs (out.getNumSamples() - 48000) <= 2, "import: stereo, resampled to 48 kHz (" + juce::String (out.getNumSamples()) + " samples)");
        check (out.getNumSamples() > 100 && std::abs (out.getSample (0, 5000) - out.getSample (1, 5000)) < 1.0e-6f && std::abs (out.getSample (0, 5000)) > 0.01f, "import: mono copied to both channels");
        check (! AudioImport::readFile (juce::File ("Z:/does/not/exist.wav"), 48000.0, out, fr, err), "import: missing file is refused");
        f.deleteFile();
    }

    {   // drum templates
        int n = 0;
        const auto* t = DrumTemplates::all (n);
        check (n == 21, "21 drum templates (got " + juce::String (n) + ")");
        {
            bool lengthsOk = true;
            for (int i = 0; i < n; ++i)
            {
                const juce::String sig (t[i].sig);
                const int num = sig.upToFirstOccurrenceOf ("/", false, false).getIntValue(), den = sig.fromFirstOccurrenceOf ("/", false, false).getIntValue();
                const int steps = (int) std::lround (num * 4.0 * 4.0 / den);
                for (int b = 0; b < 2; ++b)
                    for (int v = 0; v < DrumMachine::numVoices; ++v)
                        lengthsOk &= (int) std::strlen (t[i].bar[b][v]) == steps;
            }
            check (lengthsOk, "every template row has exactly one character per step of its time signature");
        }
        bool lenOk = true, kickOk = true;
        for (int i = 0; i < n; ++i)
            for (int b = 0; b < 2; ++b)
            {
                for (int v = 0; v < DrumMachine::numVoices; ++v) lenOk &= juce::String (t[i].sig) != "4/4" || juce::String (t[i].bar[b][v]).length() == DrumMachine::stepsPerBar;
                kickOk &= t[i].bar[b][DrumMachine::Kick][0] == 'x';
            }
        check (lenOk, "every template row has 16 steps");
        check (kickOk, "every template has a kick on the one");
        DrumMachine::Snapshot cur;
        cur.bars = 1;
        cur.steps[DrumMachine::Cymbal][5] = 1;
        auto a = DrumTemplates::apply (cur, 0);
        check (a.bars == 2, "a template makes the pattern two bars");
        check (a.steps[DrumMachine::Snare][4] == 2 && a.steps[DrumMachine::Snare][12] == 2, "Rock 1: accented snare on 2 and 4");
        check (a.steps[DrumMachine::Cymbal][5] == 0, "old steps are replaced");
        cur.bars = 4;
        auto b4 = DrumTemplates::apply (cur, 0);
        check (b4.bars == 4 && std::memcmp (&b4.steps[1][0], &b4.steps[1][32], 16) == 0, "longer patterns repeat the two bars");
    }

    {   // project files from other people: hardening
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("riffcatch-security-test");
        root.deleteRecursively();
        const auto pdir = root.getChildFile ("p");
        check (ProjectIO::isSafeTrackFile (pdir, "track1.wav"), "plain track file name is accepted");
        check (! ProjectIO::isSafeTrackFile (pdir, "..\\evil.wav") && ! ProjectIO::isSafeTrackFile (pdir, "../evil.wav")
               && ! ProjectIO::isSafeTrackFile (pdir, "C:\\Windows\\x.wav") && ! ProjectIO::isSafeTrackFile (pdir, "sub\\a.wav")
               && ! ProjectIO::isSafeTrackFile (pdir, "track1.exe") && ! ProjectIO::isSafeTrackFile (pdir, ""), "paths, traversal and other extensions are rejected");

        // a hand-made project.json with hostile values
        pdir.createDirectory();
        const auto longNotes = juce::String::repeatedString ("x", 50000);
        pdir.getChildFile ("p.riffcatch").replaceWithText (
            "{\"tracks\":[{\"name\":\"" + juce::String::repeatedString ("n", 500) + "\",\"volume\":1e9,\"pan\":-50,\"input\":-5,\"offsetMs\":1e15,\"file\":\"..\\\\..\\\\x.wav\"}],"
            "\"bpm\":99999,\"timeSignature\":\"99/3\",\"notes\":\"" + longNotes + "\"}");
        auto he = std::make_unique<AudioEngine>();
        he->prepareForTest (48000.0, 2);
        juce::String herr;
        check (ProjectIO::load (*he, pdir.getChildFile ("p.riffcatch"), herr), "a hostile project file still loads (values are clamped)");
        const auto& ht = he->tracks[he->order.front()];
        check (ht.volume.load() <= 4.0f && std::abs (ht.pan.load()) <= 1.0f && ht.inputCode.load() >= 0 && ht.name.length() <= ProjectIO::maxNameLength
               && std::abs (ht.offset.load()) <= 48000 * 60 * 20 && ht.length.load() == 0, "hostile track values are clamped and the escaping file is ignored");
        check (he->projectNotes.length() == ProjectIO::maxNotesLength && he->bpm.load() <= 300.0 && he->tsNum.load() <= 16 && he->tsDen.load() == 4,
               "notes, tempo and time signature are limited");

        // saving never touches a folder with someone else's files
        const auto foreign = root.getChildFile ("foreign");
        foreign.createDirectory();
        foreign.getChildFile ("track1.wav").replaceWithText ("my precious file");
        juce::String serr;
        auto se = std::make_unique<AudioEngine>();
        se->prepareForTest (48000.0, 2);
        se->addTrack();
        se->tracks[se->order.front()].setAudio (juce::AudioBuffer<float> (2, 4800));
        check (! ProjectIO::save (*se, foreign.getChildFile ("foreign.riffcatch"), serr) && foreign.getChildFile ("track1.wav").loadFileAsString() == "my precious file", "saving into a foreign non-empty folder is refused");
        // a real project folder: stale track files are removed, unrelated files are kept
        const auto mine = root.getChildFile ("mine");
        check (ProjectIO::save (*se, ProjectIO::fileInFolder (mine), serr), "saving into a new folder works");
        mine.getChildFile ("track7.wav").replaceWithText ("not written by RiffCatch");
        se->tracks[se->order.front()].clearAudio();
        const bool saved2 = ProjectIO::save (*se, ProjectIO::fileInFolder (mine), serr);
        check (saved2 && ! mine.getChildFile ("track1.wav").existsAsFile() && mine.getChildFile ("track7.wav").existsAsFile(),
               "only the files RiffCatch wrote are cleaned up (saved " + juce::String ((int) saved2) + ", err " + serr + ", track1 exists " + juce::String ((int) mine.getChildFile ("track1.wav").existsAsFile()) + ", track7 exists " + juce::String ((int) mine.getChildFile ("track7.wav").existsAsFile()) + ")");
        check (! ProjectIO::save (*se, mine.getChildFile ("other.riffcatch"), serr) && ! mine.getChildFile ("other.riffcatch").existsAsFile(),
               "a second project in the same folder is refused");
        check (ProjectIO::isProjectFile (juce::File ("C:\\music\\song1\\song1.riffcatch")) && ! ProjectIO::isProjectFile (juce::File ("C:\\music\\song1\\project.json")),
               "only .riffcatch files count as projects");
        root.deleteRecursively();
    }
    {   // Catch: always-on ring
        auto ce = std::make_unique<AudioEngine>();
        ce->prepareForTest (48000.0, 2);
        const int total = 48000 * 10;
        std::vector<float> l ((size_t) total), r ((size_t) total);
        for (int i = 0; i < total; ++i) { l[(size_t) i] = (float) i / (float) total; r[(size_t) i] = -(float) i / (float) total; }
        ce->pushCatchForTest (l.data(), r.data(), total);
        juce::AudioBuffer<float> got;
        check (ce->readCatch (got, 2.0) && got.getNumSamples() == 96000, "catch returns the requested 2 s");
        check (std::abs (got.getSample (0, 95999) - l[(size_t) total - 1]) < 1e-6f && std::abs (got.getSample (1, 0) - r[(size_t) total - 96000]) < 1e-6f,
               "catch holds the newest samples, both channels");
        check (ce->readCatch (got, 60.0) && got.getNumSamples() == total, "catch returns only what was recorded so far");
        // wraps around the 70 s ring
        for (int k = 0; k < 8; ++k) ce->pushCatchForTest (l.data(), r.data(), total);
        check (ce->readCatch (got, 30.0) && got.getNumSamples() == 30 * 48000 && std::abs (got.getSample (0, 30 * 48000 - 1) - l[(size_t) total - 1]) < 1e-6f, "catch keeps working after the ring wraps");
    }
    {   // looper building block: record starts exactly when the loop wraps
        auto le = std::make_unique<AudioEngine>();
        le->prepareForTest (48000.0, 1);
        le->bpm = 120.0; le->loopBars = 1; le->loopEnabled = true; le->countInBars = 0;
        const int s = le->addTrack();
        le->tracks[s].volume = 0.0f;
        le->play();
        std::vector<float> in (512, 0.1f), outL (512), outR (512);
        const float* ins[1] = { in.data() };
        float* outs[2] = { outL.data(), outR.data() };
        juce::AudioIODeviceCallbackContext ctx;
        auto run = [&] (int blocks) { for (int i = 0; i < blocks; ++i) { le->audioDeviceIOCallbackWithContext (ins, 1, outs, 2, 512, ctx); if (i % 8 == 0) le->pollRecording(); } };
        run (4);
        check (! le->recordAtNextLoop(), "overdub needs an armed track");
        le->tracks[s].armed = true;
        check (le->recordAtNextLoop(), "overdub can be queued while playing");
        run (100);                                                // 51200 samples, the loop is 96000
        check (le->getState() == AudioEngine::Playing, "still playing before the loop ends");
        run (100);                                                // past the loop end
        check (le->getState() == AudioEngine::Recording, "recording started at the loop wrap");
        run (200);                                                // the whole recorded pass
        for (int i = 0; i < 4; ++i) le->pollRecording();
        auto& lt = le->tracks[s];
        check (lt.length.load() == le->getLoopLength() && std::abs (lt.data.getSample (0, 48000) - 0.1f) < 0.01f,
               "the queued overdub recorded a full pass (length " + juce::String ((juce::int64) lt.length.load()) + ", mid sample " + juce::String (lt.data.getSample (0, 48000), 3) + ")");
        le->stop();
    }
    {   // time signatures
        auto tsP = std::make_unique<AudioEngine>(); auto& ts = *tsP;
        ts.prepareForTest (48000.0, 1);
        ts.bpm = 120.0;
        const double spb = 24000.0;
        check (ts.getBarLength() == (juce::int64) (4 * spb), "4/4 bar = 4 quarters");
        ts.setTimeSignature (3, 4);
        check (ts.getBarLength() == (juce::int64) (3 * spb) && ts.drums.barSteps.load() == 12, "3/4 bar = 3 quarters, 12 drum steps");
        ts.setTimeSignature (6, 8);
        check (ts.getBarLength() == (juce::int64) (3 * spb) && ts.getPulsesPerBar() == 6 && std::abs (ts.getPulseSamples() - spb / 2) < 1e-6, "6/8 bar = 3 quarters, 6 eighth pulses");
        ts.setTimeSignature (7, 8);
        check (ts.getBarLength() == (juce::int64) (3.5 * spb) && ts.drums.barSteps.load() == 14, "7/8 bar = 3.5 quarters, 14 drum steps");
        ts.setTimeSignature (5, 4);
        check (ts.getBarLength() == (juce::int64) (5 * spb) && ts.drums.barSteps.load() == 20, "5/4 bar = 5 quarters, 20 drum steps");
        ts.loopBars = 2;
        check (ts.getLoopLength() == (juce::int64) (10 * spb), "loop length follows the time signature");

        // drum pattern survives a layout change as far as it fits
        auto dm4P = std::make_unique<AudioEngine>(); auto& dm4 = *dm4P;
        dm4.setTimeSignature (4, 4);
        dm4.drums.steps[0][0] = 2; dm4.drums.steps[0][12] = 1; dm4.drums.steps[0][16] = 1;
        dm4.drums.bars = 2;
        dm4.setTimeSignature (3, 4);
        check (dm4.drums.steps[0][0].load() == 2 && dm4.drums.steps[0][12].load() == 1 && dm4.drums.steps[0][13].load() == 0, "3/4: bar 2 starts at step 12 (the cut 4/4 tail is dropped)");
        dm4.setTimeSignature (4, 4);
        check (dm4.drums.barSteps.load() == 16, "back to 4/4: 16 steps");

        // time signature in the project file
        auto peP = std::make_unique<AudioEngine>(); auto& pe = *peP;
        pe.prepareForTest (44100.0, 2);
        pe.addTrack();
        pe.setTimeSignature (6, 8);
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("riffcatch-ts-test");
        dir.deleteRecursively();
        juce::String err;
        check (ProjectIO::save (pe, ProjectIO::fileInFolder (dir), err), "time signature test: saved");
        auto pe2P = std::make_unique<AudioEngine>(); auto& pe2 = *pe2P;
        pe2.prepareForTest (44100.0, 2);
        check (ProjectIO::load (pe2, ProjectIO::fileInFolder (dir), err) && pe2.tsNum.load() == 6 && pe2.tsDen.load() == 8 && pe2.drums.barSteps.load() == 12, "time signature round-trips through the project file");
        dir.deleteRecursively();
    }
    {   // bass synth
        const double sr = 44100.0, stepLen = sr * 60.0 / 120.0 / 4.0;
        auto bmP = std::make_unique<BassMachine>(); auto& bm = *bmP;
        bm.prepare (sr);
        bm.wave = 2; bm.sub = 0.0f; bm.drive = 0.0f; bm.cutoff = 2000.0f; bm.envAmt = 0.0f; bm.sustain = 1.0f; bm.reso = 0.0f; bm.glide = 0.0f;
        bm.root = 33; bm.scale = 0;
        check (BassMachine::semitones (0, 5) == 12 && BassMachine::semitones (0, -1) == -2 && BassMachine::semitones (2, 7) == 12, "bass scales: degrees map to semitones, also below the root");
        check (BassMachine::nearestDegree (0, 7) == 3 && BassMachine::nearestDegree (3, 4) == 2, "bass: nearest degree for fifth and major third");
        const auto cell = BassMachine::pack (-2, 5, true, true);
        check (BassMachine::degreeOf (cell) == -2 && BassMachine::lenOf (cell) == 5 && BassMachine::slideOf (cell) && BassMachine::accentOf (cell), "bass cell packs degree, length and flags");

        bm.steps[2] = BassMachine::pack (5, 4, false, false);          // A2 on step 3
        std::vector<float> L (44100, 0.0f), R (44100, 0.0f);
        bm.process (0, 44100, stepLen, 1.0f, L.data(), R.data(), true);
        float pre = 0.0f, post = 0.0f;
        for (int i = 0; i < 11000; ++i) pre = std::max (pre, std::abs (L[(size_t) i]));
        for (int i = 11025; i < 16000; ++i) post = std::max (post, std::abs (L[(size_t) i]));
        check (pre == 0.0f && post > 0.05f, "bass note starts at its step (sample 11025)");
        float tail = 0.0f;
        for (int i = 40000; i < 44100; ++i) tail = std::max (tail, std::abs (L[(size_t) i]));
        check (tail < 0.001f, "bass note ends after its length");
        PitchDetector det;
        const auto pr = det.detect (L.data() + 14000, 4096, sr);
        const double cents = pr.valid ? 1200.0 * std::log2 (pr.freq / 110.0) : 999.0;
        check (pr.valid && std::abs (cents) < 25.0, "bass plays A2 (110 Hz), off by " + juce::String (cents, 1) + " cents");

        // chunked rendering gives the same audio as one block
        auto bm2P = std::make_unique<BassMachine>(); auto& bm2 = *bm2P;
        bm2.copyFrom (bm); bm2.prepare (sr);
        std::vector<float> L2 (44100, 0.0f), R2 (44100, 0.0f);
        for (int p = 0; p < 44100; p += 777)
        {
            const int n = std::min (777, 44100 - p);
            bm2.process (p, n, stepLen, 1.0f, L2.data() + p, R2.data() + p, true);
        }
        float diff = 0.0f;
        for (int i = 0; i < 44100; ++i) diff = std::max (diff, std::abs (L[(size_t) i] - L2[(size_t) i]));
        check (diff < 1e-4f, "bass renders identically in small chunks");

        // transposition per bar and bar layout
        bm.bars = 2; bm.shift[1] = 5;
        check (bm.midiFor (cell, 1) == bm.midiFor (cell, 0) + 5, "bass: per-bar transposition");
        bm.barSteps = 16; bm.steps[16] = BassMachine::pack (0, 2, false, false);
        bm.setBarSteps (12);
        check (BassMachine::isNote (bm.steps[12].load()) && ! BassMachine::isNote (bm.steps[16].load()), "bass: 3/4 moves bar 2 to step 12");
        bm.setBarSteps (16);

        // templates
        BassMachine::Snapshot cur;
        auto root = BassTemplates::apply (cur, 0, nullptr, 4);
        int cnt = 0; for (int k = 0; k < 16; ++k) cnt += BassMachine::isNote (root.steps[k]) ? 1 : 0;
        check (cnt == 4 && BassMachine::degreeOf (root.steps[4]) == 0, "bass template: root pulse = four root notes");
        DrumMachine::Snapshot ds;
        ds.bars = 2; ds.steps[DrumMachine::Kick][0] = 2; ds.steps[DrumMachine::Kick][8] = 1; ds.steps[DrumMachine::Kick][20] = 1;
        auto lk = BassTemplates::apply (cur, BassTemplates::lockToKickId, &ds, 4);
        check (lk.bars == 2 && BassMachine::isNote (lk.steps[0]) && BassMachine::isNote (lk.steps[8]) && BassMachine::isNote (lk.steps[20])
               && ! BassMachine::isNote (lk.steps[4]) && BassMachine::accentOf (lk.steps[0]) && BassMachine::lenOf (lk.steps[0]) == 4, "bass template: lock to kick follows the drums");
        auto pg = BassTemplates::applyProgression (root, 1);
        check (pg.bars == 4 && pg.shift[1] == 5 && pg.shift[2] == 7 && BassMachine::isNote (pg.steps[3 * 16]), "bass chords: I-IV-V-IV extends to four bars");
        for (int t = 0; t < BassTemplates::numPatterns; ++t)
        {
            auto s = BassTemplates::apply (cur, t, nullptr, 4);
            bool inRange = true;
            for (int k = 0; k < 16; ++k)
                if (BassMachine::isNote (s.steps[k])) { const int r = BassMachine::degreeOf (s.steps[k]) - BassMachine::degreeBase; inRange &= r >= 0 && r < BassMachine::numRows; }
            check (inRange, juce::String ("bass template stays inside the grid: ") + BassTemplates::name (t));
        }

        // project round trip and export
        auto beP = std::make_unique<AudioEngine>(); auto& be = *beP;
        be.prepareForTest (44100.0, 2);
        be.addTrack();
        be.bass.root = 31; be.bass.scale = 4; be.bass.bars = 2; be.bass.cutoff = 777.0f; be.bass.shift[1] = 7; be.bass.mute = true;
        be.bass.steps[3] = BassMachine::pack (4, 3, true, true);
        auto bdir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("riffcatch-bass-test");
        bdir.deleteRecursively();
        juce::String berr;
        check (ProjectIO::save (be, ProjectIO::fileInFolder (bdir), berr), "bass test: project saved");
        auto be2P = std::make_unique<AudioEngine>(); auto& be2 = *be2P;
        be2.prepareForTest (44100.0, 2);
        check (ProjectIO::load (be2, ProjectIO::fileInFolder (bdir), berr), "bass test: project loaded");
        check (be2.bass.snapshot() == be.bass.snapshot() && std::abs (be2.bass.cutoff.load() - 777.0f) < 0.01f && be2.bass.mute.load(), "bass pattern and sound round-trip through the project file");
        bdir.deleteRecursively();

        auto bxP = std::make_unique<AudioEngine>(); auto& bx = *bxP;
        bx.prepareForTest (44100.0, 2);
        bx.bass.volume = 1.0f;
        bx.bass.steps[0] = BassMachine::pack (0, 4, false, false);
        auto bwav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("rc_selftest_bass.wav");
        check (Exporter::bounceWav (bx, bwav, 1, berr), "export works with bass only " + berr);
        bwav.deleteFile();

        // bass insert effects: echo repeats the note after the dry note has ended, reverb adds a tail
        auto tailEnergy = [&] (bool echo, bool rev)
        {
            auto fP = std::make_unique<BassMachine>(); auto& f = *fP;
            f.prepare (sr); f.fx.prepare (sr);
            f.wave = 2; f.sub = 0.0f; f.drive = 0.0f; f.sustain = 1.0f;
            f.steps[0] = BassMachine::pack (5, 1, false, false);
            f.echoOn = echo; f.reverbOn = rev; f.echoDiv = TrackFx::Quarter;
            std::vector<float> a (44100, 0.0f), b (44100, 0.0f);
            for (int p = 0; p < 44100; p += 1024)
            {
                const int n = std::min (1024, 44100 - p);
                f.render (p, n, stepLen, 1.0f, a.data() + p, b.data() + p, true, sr * 0.5);
            }
            float e = 0.0f;
            for (int i = 24000; i < 44100; ++i) e += std::abs (a[(size_t) i]);
            return e;
        };
        const float dry = tailEnergy (false, false), wet = tailEnergy (true, false), rv = tailEnergy (false, true);
        check (wet > dry + 5.0f && rv > dry + 1.0f, "bass echo and reverb add a tail (dry " + juce::String (dry, 2) + ", echo " + juce::String (wet, 2) + ", reverb " + juce::String (rv, 2) + ")");
    }
    {   // silence trimming
        juce::AudioBuffer<float> b (2, 48000);
        b.clear();
        for (int i = 20000; i < 30000; ++i) { b.setSample (0, i, 0.5f); b.setSample (1, i, 0.5f); }
        juce::int64 a = 0, z = 0;
        check (SilenceTrim::findRange (b, 48000, 48000.0, a, z), "silence trim finds audio");
        check (a > 19000 && a <= 20000 && z >= 30000 && z < 33000, "silence trim keeps the audible part with a little padding (" + juce::String (a) + ".." + juce::String (z) + ")");
        juce::AudioBuffer<float> full (2, 1000);
        for (int i = 0; i < 1000; ++i) { full.setSample (0, i, 0.5f); full.setSample (1, i, 0.5f); }
        check (! SilenceTrim::findRange (full, 1000, 48000.0, a, z), "nothing to trim when the clip is full");
        juce::AudioBuffer<float> empty (2, 1000);
        empty.clear();
        check (! SilenceTrim::findRange (empty, 1000, 48000.0, a, z), "all-silent clips are left alone");
    }
    {   // tuner: pitch detection on synthetic guitar-range tones, through the real input ring
        for (double sr : { 44100.0, 48000.0 })
            for (double f : { 82.41, 110.0, 196.0, 329.63, 440.0, 659.26 })
            {
                AudioEngine te;
                te.prepareForTest (sr, 1);
                std::vector<float> sig (4096);
                for (int i = 0; i < 4096; ++i)
                    sig[(size_t) i] = (float) (0.4 * std::sin (2.0 * juce::MathConstants<double>::pi * f * i / sr)
                                             + 0.15 * std::sin (4.0 * juce::MathConstants<double>::pi * f * i / sr));
                te.pushTunerSamples (sig.data(), 4096);
                std::vector<float> got (4096);
                te.readTunerSamples (got.data(), 4096);
                PitchDetector det;
                const auto r = det.detect (got.data(), 4096, sr);
                const double cents = r.valid ? 1200.0 * std::log2 (r.freq / f) : 999.0;
                check (r.valid && std::abs (cents) < 3.0, "tuner detects " + juce::String (f, 2) + " Hz at " + juce::String ((int) sr) + " (off by " + juce::String (cents, 1) + " cents)");
            }
        std::vector<float> quiet (4096, 0.0f);
        PitchDetector det2;
        check (! det2.detect (quiet.data(), 4096, 44100.0).valid, "tuner ignores silence");
        check (TunerCard::noteName (69) == "A" && TunerCard::octaveOf (69) == 4 && TunerCard::noteName (40) == "E" && TunerCard::octaveOf (40) == 2, "tuner note names (A4, E2)");
    }
    {   // project notes survive save and load
        auto neP = std::make_unique<AudioEngine>(); auto& ne = *neP;
        ne.prepareForTest (44100.0, 2);
        ne.addTrack();
        ne.projectNotes = "E minor, capo 2\nidea: slower";
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("riffcatch-notes-test");
        dir.deleteRecursively();
        juce::String err;
        check (ProjectIO::save (ne, ProjectIO::fileInFolder (dir), err), "notes test: project saved");
        auto ne2P = std::make_unique<AudioEngine>(); auto& ne2 = *ne2P;
        ne2.prepareForTest (44100.0, 2);
        check (ProjectIO::load (ne2, ProjectIO::fileInFolder (dir), err), "notes test: project loaded");
        check (ne2.projectNotes == ne.projectNotes, "project notes round-trip");
        dir.deleteRecursively();
    }
    patternSelfTest (check);
    songSelfTest (check);
    {   // custom themes
        const Theme::Preset saved { "live", Theme::blue.getARGB(), Theme::purple.getARGB(), Theme::cyan.getARGB(),
                                   Theme::bg.getARGB(), Theme::panel.getARGB(), Theme::text.getARGB() };
        const bool legacy = Theme::legacyTracks;
        const auto selectedName = Theme::presets[(size_t) Theme::current].name;
        ThemeStore::reload();
        const auto baseline = Theme::presets;
        const auto files = ThemeStore::dir().findChildFiles (juce::File::findFiles, false, "*.json");
        std::vector<juce::MemoryBlock> contents;
        for (const auto& f : files)
        {
            juce::MemoryBlock bytes;
            f.loadFileAsData (bytes);
            contents.push_back (std::move (bytes));
        }
        juce::String testName;
        do { testName = "selftest-theme-" + juce::Uuid().toString(); }
        while (ThemeStore::fileFor (testName).exists() || ThemeStore::indexOf (testName) >= 0);
        const auto dk = Theme::generate (testName, juce::Colour (0xffea580c), juce::Colour (0xfffbbf24), true);
        const auto lt = Theme::generate ("selftest-light", juce::Colour (0xff2563eb), juce::Colour (0xff06b6d4), false);
        auto contrast = [] (juce::uint32 a, juce::uint32 b) { return std::abs (juce::Colour (a).getPerceivedBrightness() - juce::Colour (b).getPerceivedBrightness()); };
        check (contrast (dk.background, dk.text) > 0.6f, "generated dark theme: text contrasts with background");
        check (contrast (lt.background, lt.text) > 0.6f, "generated light theme: text contrasts with background");
        check (juce::Colour (dk.background).getPerceivedBrightness() < juce::Colour (lt.background).getPerceivedBrightness(), "dark base is darker than light base");
        // Six explicit Neo-Retro roles exercise the real store even on machines
        // without the optional installed preset. Never write to its user file.
        const Theme::Preset fixture { testName, 0xffff780a, 0xfff2ab24, 0xfff6c343, 0xff0b1018, 0xff202630, 0xffdce3ec };
        auto same = [] (const Theme::Preset& a, const Theme::Preset& b)
        {
            return a.name == b.name && a.primary == b.primary && a.secondary == b.secondary && a.accent == b.accent
                && a.background == b.background && a.surface == b.surface && a.text == b.text;
        };
        auto paletteMatches = [] (const Theme::Preset& p)
        {
            return Theme::blue.getARGB() == p.primary && Theme::accent.getARGB() == p.primary
                && Theme::purple.getARGB() == p.secondary && Theme::cyan.getARGB() == p.accent
                && Theme::bg.getARGB() == p.background && Theme::panel.getARGB() == p.surface && Theme::text.getARGB() == p.text
                && Theme::red.getARGB() == 0xfff7768e && Theme::green.getARGB() == 0xff9ece6a;
        };
        check (ThemeStore::save (fixture), "custom theme saved to disk");
        ThemeStore::reload();
        const int idx = ThemeStore::indexOf (testName);
        check (idx >= Theme::builtinCount && Theme::numPresets() == (int) baseline.size() + 1, "only test custom theme added after reload");
        if (idx >= 0)
        {
            check (same (Theme::presets[(size_t) idx], fixture), "custom theme: all six Neo-Retro roles round-trip through JSON");
            Theme::apply (idx);
            check (Theme::current == idx && paletteMatches (fixture) && ! Theme::legacyTracks,
                   "custom theme: Neo-Retro selection, six-role mapping and fixed transport colours");
            check (ThemeStore::remove (idx) && ThemeStore::indexOf (testName) < 0, "only test custom theme deleted");
        }
        bool preserved = Theme::presets.size() == baseline.size();
        for (size_t i = 0; i < baseline.size() && i < Theme::presets.size(); ++i)
            preserved &= same (baseline[i], Theme::presets[i]);
        for (size_t i = 0; i < contents.size(); ++i)
        {
            juce::MemoryBlock bytes;
            preserved &= files[(int) i].loadFileAsData (bytes) && bytes == contents[i];
        }
        check (preserved, "existing built-in/custom themes survive reload and cleanup byte-for-byte");
        // Includes the actual installed Neo-Retro when present; no install-file
        // dependency, no settings writes, no save/delete of any existing theme.
        bool selectionOK = true;
        for (int i = Theme::builtinCount; i < Theme::numPresets(); ++i)
        {
            Theme::apply (i);
            selectionOK &= Theme::current == i && paletteMatches (Theme::presets[(size_t) i]);
        }
        check (selectionOK, "installed custom themes: reload/selection/palette preserved (" + juce::String (Theme::numPresets() - Theme::builtinCount) + ")");
        Theme::current = ThemeStore::indexOf (selectedName);
        Theme::applyPreset (saved, legacy);
    }

    log << (ok ? "ALL OK" : "FAILURES") << "\n";



    return log;
}
