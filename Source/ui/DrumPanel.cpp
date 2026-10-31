#include "DrumPanel.h"
#include "audio/DrumTemplates.h"

juce::Colour DrumPanel::voiceColour (int v)
{
    const juce::Colour c[] = { Theme::red, Theme::orange, Theme::yellow, Theme::teal, Theme::cyan, Theme::purple };
    return c[v % 6];
}

static juce::String dbText (float gain)
{
    return gain <= 0.001f ? juce::String ("-inf dB") : juce::String (juce::Decibels::gainToDecibels (gain), 1) + " dB";
}

static juce::String panText (float p)
{
    const int pct = (int) std::lround (std::abs (p) * 100.0f);
    return pct == 0 ? juce::String ("center") : (p < 0 ? "left " : "right ") + juce::String (pct) + "%";
}

void DrumPanel::info (const juce::String& what, const juce::String& value)
{
    if (onInfo) onInfo (what + ": " + value);
}

DrumPanel::DrumPanel (AudioEngine& e) : engine (e)
{
    addAndMakeVisible (chevron);
    chevron.open = expanded;
    chevron.onClick = [this] { setExpanded (! expanded); };

    title.setText ("DRUMS", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    title.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (title);

    for (int b = 1; b <= DrumMachine::maxBars; ++b) barsBox.addItem (juce::String (b) + (b == 1 ? " bar" : " bars"), b);
    barsBox.setWantsKeyboardFocus (false);
    barsBox.setTooltip ("Pattern length. It repeats over the loop and keeps its phase (6 bars over a 4-bar loop works)");
    barsBox.onChange = [this]
    {
        const int nb = barsBox.getSelectedId();
        if (nb < 1 || nb == engine.drums.bars.load()) return;
        auto before = engine.drums.snapshot();
        auto after = before;
        after.bars = nb;
        // growing: bar 1 is copied into every new bar, lane by lane, where that lane is still empty in the new bar
        for (int bar = before.bars; bar < nb; ++bar)
            for (int v = 0; v < DrumMachine::numVoices; ++v)
            {
                bool empty = true;
                for (int k = 0; k < DrumMachine::stepsPerBar; ++k)
                    empty &= after.steps[v][bar * DrumMachine::stepsPerBar + k] == 0;
                if (empty)
                    for (int k = 0; k < DrumMachine::stepsPerBar; ++k)
                        after.steps[v][bar * DrumMachine::stepsPerBar + k] = after.steps[v][k];
            }        pushPattern ("Pattern length", before, after);
        if (onReleaseFocus) onReleaseFocus();
    };
    addAndMakeVisible (barsBox);

    {   // ready-made patterns
        int n = 0;
        const auto* t = DrumTemplates::all (n);
        const char* group = "";
        for (int i = 0; i < n; ++i)
        {
            if (juce::String (group) != t[i].group) { group = t[i].group; templateBox.addSectionHeading (group); }
            templateBox.addItem (t[i].name, i + 1);
        }
        templateBox.setTextWhenNothingSelected ("Template");
        templateBox.setWantsKeyboardFocus (false);
        templateBox.setTooltip ("Replace the pattern with a ready-made two-bar groove (undoable)");
        templateBox.onChange = [this]
        {
            const int id = templateBox.getSelectedId();
            if (id < 1) return;
            const auto before = engine.drums.snapshot();
            const auto after = DrumTemplates::apply (before, id - 1);
            int n2 = 0;
            const auto name = juce::String (DrumTemplates::all (n2)[id - 1].name);
            page = 0;
            pushPattern ("Drum template: " + name, before, after);
            templateBox.setSelectedId (0, juce::dontSendNotification);
            if (onReleaseFocus) onReleaseFocus();
        };
        addAndMakeVisible (templateBox);
    }

    clearBtn.setWantsKeyboardFocus (false);
    clearBtn.setTooltip ("Clear the pattern");
    clearBtn.onClick = [this]
    {
        auto before = engine.drums.snapshot();
        auto after = before;
        std::memset (after.steps, 0, sizeof (after.steps));
        pushPattern ("Clear drum pattern", before, after);
        if (onReleaseFocus) onReleaseFocus();
    };
    addAndMakeVisible (clearBtn);

    for (auto* b : { &prevPage, &nextPage })
    {
        b->setWantsKeyboardFocus (false);
        b->onClick = [this, b] { page += (b == &nextPage ? 1 : -1); repaint(); updateControls(); };
        addChildComponent (b);
    }
    pageLabel.setJustificationType (juce::Justification::centred);
    pageLabel.setColour (juce::Label::textColourId, Theme::dim);
    pageLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    addChildComponent (pageLabel);

    volume.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    volume.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    volume.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    volume.setRange (0.0, 1.2, 0.0);
    volume.setDoubleClickReturnValue (true, 0.8);
    volume.getProperties().set ("uni", true);
    volume.setColour (juce::Slider::rotarySliderFillColourId, Theme::orange);
    volume.setWantsKeyboardFocus (false);
    volume.setTooltip ("Drum volume");
    volume.onValueChange = [this] { engine.drums.volume = (float) volume.getValue(); info ("Drums volume", dbText ((float) volume.getValue())); };
    addAndMakeVisible (volume);

    auto styleToggle = [] (juce::TextButton& b, juce::Colour on)
    {
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonColourId, Theme::raised);
        b.setColour (juce::TextButton::buttonOnColourId, on);
        b.setColour (juce::TextButton::textColourOffId, Theme::text);
        b.setColour (juce::TextButton::textColourOnId, Theme::onAccent);
        b.setWantsKeyboardFocus (false);
    };
    styleToggle (muteBtn, Theme::accent);
    styleToggle (soloBtn, Theme::accent);
    muteBtn.onClick = [this] { engine.drums.mute = muteBtn.getToggleState(); if (onReleaseFocus) onReleaseFocus(); };
    soloBtn.onClick = [this] { engine.drums.solo = soloBtn.getToggleState(); if (onReleaseFocus) onReleaseFocus(); };
    addAndMakeVisible (muteBtn);
    addAndMakeVisible (soloBtn);

    meter.setHorizontal (true);
    addAndMakeVisible (meter);

    auto styleLane = [] (juce::Slider& s, double lo, double hi, double def, const juce::String& tip, bool unipolar = false)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRange (lo, hi, 0.0);
        s.setDoubleClickReturnValue (true, def);
        s.getProperties().set ("uni", unipolar);
        s.setColour (juce::Slider::rotarySliderFillColourId, Theme::accent);
        s.setWantsKeyboardFocus (false);
        s.setTooltip (tip);
    };
    for (int v = 0; v < DrumMachine::numVoices; ++v)
    {
        styleLane (laneVol[v], 0.0, 1.5, 1.0, "Lane volume", true);
        laneVol[v].setColour (juce::Slider::rotarySliderFillColourId, voiceColour (v));
        lanePan[v].setColour (juce::Slider::rotarySliderFillColourId, voiceColour (v));
        styleLane (lanePan[v], -1.0, 1.0, 0.0, "Lane pan");
        laneVol[v].onValueChange = [this, v] { engine.drums.level[v] = (float) laneVol[v].getValue(); info (juce::String (DrumMachine::voiceName (v)) + " volume", dbText ((float) laneVol[v].getValue())); };
        lanePan[v].onValueChange = [this, v] { engine.drums.pan[v] = (float) lanePan[v].getValue(); info (juce::String (DrumMachine::voiceName (v)) + " pan", panText ((float) lanePan[v].getValue())); };
        addChildComponent (laneVol[v]);
        addChildComponent (lanePan[v]);
    }
    styleLane (cymPitch, -12.0, 12.0, 0.0, "Cymbal pitch (semitones)");
    cymPitch.setColour (juce::Slider::rotarySliderFillColourId, voiceColour (DrumMachine::Cymbal));
    cymPitch.onValueChange = [this]
    {
        engine.drums.cymbalPitch = (float) cymPitch.getValue();
        const double st = cymPitch.getValue();
        info ("Cymbal pitch", (st > 0.05 ? "+" : "") + juce::String (st, 1) + " semitones");
    };
    addChildComponent (cymPitch);

    refreshFromEngine();
}

void DrumPanel::lookAndFeelChanged()
{
    pageLabel.setColour (juce::Label::textColourId, Theme::dim);
    for (auto* b : { &muteBtn, &soloBtn })
    {
        b->setColour (juce::TextButton::buttonColourId, Theme::raised);
        b->setColour (juce::TextButton::buttonOnColourId, Theme::accent);
        b->setColour (juce::TextButton::textColourOffId, Theme::text);
        b->setColour (juce::TextButton::textColourOnId, Theme::onAccent);
    }
}

void DrumPanel::setExpanded (bool b)
{
    expanded = b;
    chevron.open = b;
    chevron.repaint();
    updateControls();
    if (onLayoutChanged) onLayoutChanged();
}

void DrumPanel::setView (double seconds, double barSecs, double start, double full)
{
    viewSeconds = seconds; barSeconds = barSecs; viewStart = start;
    fullSeconds = full > 0.0 ? full : seconds;
    updateControls();
}

void DrumPanel::refreshFromEngine()
{
    barsBox.setSelectedId (engine.drums.bars.load(), juce::dontSendNotification);
    volume.setValue (engine.drums.volume.load(), juce::dontSendNotification);
    muteBtn.setToggleState (engine.drums.mute.load(), juce::dontSendNotification);
    soloBtn.setToggleState (engine.drums.solo.load(), juce::dontSendNotification);
    for (int v = 0; v < DrumMachine::numVoices; ++v)
    {
        laneVol[v].setValue (engine.drums.level[v].load(), juce::dontSendNotification);
        lanePan[v].setValue (engine.drums.pan[v].load(), juce::dontSendNotification);
    }
    cymPitch.setValue (engine.drums.cymbalPitch.load(), juce::dontSendNotification);
    updateControls();
    repaint();
}

double DrumPanel::stepSec() const
{
    return barSeconds > 0.0 ? barSeconds / DrumMachine::stepsPerBar : fullSeconds / (4.0 * DrumMachine::stepsPerBar);
}

int DrumPanel::viewSteps() const
{
    return barSeconds > 0.0 ? juce::jmax (1, (int) std::lround (fullSeconds / barSeconds) * DrumMachine::stepsPerBar) : 4 * DrumMachine::stepsPerBar;
}

void DrumPanel::updateControls()
{
    const int ps = engine.drums.patternSteps(), vs = viewSteps();
    const int pages = juce::jmax (1, (ps + vs - 1) / vs);
    page = juce::jlimit (0, pages - 1, page);
    const bool paged = expanded && pages > 1;
    for (int v = 0; v < DrumMachine::numVoices; ++v) { laneVol[v].setVisible (expanded); lanePan[v].setVisible (expanded); }
    cymPitch.setVisible (expanded);
    prevPage.setVisible (paged); nextPage.setVisible (paged); pageLabel.setVisible (paged);
    prevPage.setEnabled (page > 0); nextPage.setEnabled (page < pages - 1);
    if (paged)
    {
        const int b0 = page * vs / DrumMachine::stepsPerBar + 1, b1 = juce::jmin (engine.drums.bars.load(), b0 + vs / DrumMachine::stepsPerBar - 1);
        pageLabel.setText ("bars " + juce::String (b0) + "-" + juce::String (b1), juce::dontSendNotification);
    }
}

juce::Rectangle<int> DrumPanel::lanesArea() const
{
    return { TrackRow::waveLeft, headerH, juce::jmax (1, getWidth() - TrackRow::waveLeft - TrackRow::waveRightInset), DrumMachine::numVoices * laneH };
}

juce::Rectangle<float> DrumPanel::laneRect (int voice) const
{
    const auto a = lanesArea();
    return { (float) a.getX(), (float) (a.getY() + voice * laneH + 1), (float) a.getWidth(), (float) (laneH - 2) };
}

int DrumPanel::cellAt (float x) const
{
    const auto a = lanesArea();
    if (x < (float) a.getX() || x >= (float) a.getRight()) return -1;
    const double secs = (double) (x - (float) a.getX()) / (double) a.getWidth() * viewSeconds + viewStart;
    return juce::jlimit (0, viewSteps() - 1, (int) std::floor (secs / stepSec()));
}

int DrumPanel::stepForCell (int cell) const
{
    const int ps = engine.drums.patternSteps();
    return (page * viewSteps() + cell) % ps;
}

void DrumPanel::setCell (int voice, int cell, int value)
{
    if (voice < 0 || voice >= DrumMachine::numVoices || cell < 0) return;
    engine.drums.steps[voice][stepForCell (cell)] = (uint8_t) value;
    repaint (lanesArea());
}

void DrumPanel::pushPattern (const juce::String& name, const DrumMachine::Snapshot& before, const DrumMachine::Snapshot& after)
{
    if (before == after) return;
    auto apply = [this] (const DrumMachine::Snapshot& s) { engine.drums.apply (s); refreshFromEngine(); };
    if (onUndoable) onUndoable (name, [apply, after] { apply (after); }, [apply, before] { apply (before); });
    else apply (after);
}

void DrumPanel::resized()
{
    auto r = juce::Rectangle<int> (0, 0, getWidth(), headerH).reduced (12, 7);
    r.removeFromLeft (6);
    chevron.setBounds (r.removeFromLeft (24));
    title.setBounds (r.removeFromLeft (64));
    barsBox.setBounds (r.removeFromLeft (84)); r.removeFromLeft (6);
    templateBox.setBounds (r.removeFromLeft (112)); r.removeFromLeft (8);
    prevPage.setBounds (r.removeFromLeft (26)); r.removeFromLeft (4);
    pageLabel.setBounds (r.removeFromLeft (72)); r.removeFromLeft (4);
    nextPage.setBounds (r.removeFromLeft (26)); r.removeFromLeft (16);
    r.removeFromRight (4);                                          // VU right edge lines up with the mixer's
    meter.setBounds (r.removeFromRight (190).withSizeKeepingCentre (190, 14)); r.removeFromRight (16);
    clearBtn.setBounds (r.removeFromRight (56)); r.removeFromRight (12);
    soloBtn.setBounds (r.removeFromRight (30)); r.removeFromRight (4);
    muteBtn.setBounds (r.removeFromRight (30)); r.removeFromRight (12);
    volume.setBounds (r.removeFromRight (32).withSizeKeepingCentre (32, 32));

    for (int v = 0; v < DrumMachine::numVoices; ++v)
    {
        const int y = headerH + v * laneH;
        laneVol[v].setBounds (112, y, laneH, laneH);
        lanePan[v].setBounds (172, y, laneH, laneH);
    }
    cymPitch.setBounds (232, headerH + DrumMachine::Cymbal * laneH, laneH, laneH);
}

void DrumPanel::paint (juce::Graphics& g)
{
    const auto accent = Theme::orange;
    const auto b = getLocalBounds().toFloat();
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path card;
        card.addRoundedRectangle (b, Theme::radiusLg);
        g.reduceClipRegion (card);
        g.setColour (Theme::panel);
        g.fillRect (b);
        g.setColour (accent);
        g.fillRect (b.withWidth (4.0f));
    }
    g.setColour (accent.withAlpha (0.22f));
    g.drawRoundedRectangle (b.reduced (0.5f), Theme::radiusLg, 1.0f);
    if (! expanded) return;

    const auto& dm = engine.drums;
    const int vs = viewSteps(), ps = dm.patternSteps();
    const auto la = lanesArea();
    Theme::wellRing (g, la.toFloat(), 3.0f);
    const double pps = (double) la.getWidth() / viewSeconds;
    const double stepS = stepSec();
    const float cellW = (float) (stepS * pps);
    const float xOrigin = (float) la.getX() - (float) (viewStart * pps);      // x of step 0
    const int c0 = juce::jmax (0, (int) std::floor (viewStart / stepS)), c1 = juce::jmin (vs, (int) std::ceil ((viewStart + viewSeconds) / stepS));

    for (int v = 0; v < DrumMachine::numVoices; ++v)
    {
        const auto col = voiceColour (v);
        const auto lr = laneRect (v);
        const bool vm = dm.voiceMute[v].load();

        // lane header: mute dot + name (click the name to audition)
        const juce::Rectangle<float> dot (18.0f, lr.getCentreY() - 6.0f, 12.0f, 12.0f);
        g.setColour (vm ? Theme::dim.withAlpha (0.5f) : col);
        if (vm) g.drawRoundedRectangle (dot, Theme::radius, 1.4f); else g.fillRoundedRectangle (dot, Theme::radius);
        g.setColour (vm ? Theme::dim : Theme::text);
        g.setFont (juce::Font (juce::FontOptions (12.5f)));
        g.drawText (DrumMachine::voiceName (v), 40, (int) lr.getY(), 74, (int) lr.getHeight(), juce::Justification::centredLeft);

        g.setColour (Theme::dim);                 // knob captions
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        g.drawText ("vol", 112 + laneH, (int) lr.getY(), 30, (int) lr.getHeight(), juce::Justification::centredLeft);
        g.drawText ("pan", 172 + laneH, (int) lr.getY(), 30, (int) lr.getHeight(), juce::Justification::centredLeft);
        if (v == DrumMachine::Cymbal) g.drawText ("pitch", 232 + laneH, (int) lr.getY(), 40, (int) lr.getHeight(), juce::Justification::centredLeft);

        g.setColour (Theme::deep);
        g.fillRoundedRectangle (lr, Theme::radius);
        g.saveState();
        g.reduceClipRegion (lr.toNearestInt());
        for (int c = c0; c < c1; ++c)
        {
            const int s = page * vs + c;
            const bool inside = s < ps;
            const int val = dm.steps[v][s % ps].load();
            const float x = xOrigin + c * cellW;
            const juce::Rectangle<float> cell (x + 0.5f, lr.getY() + 2.0f, juce::jmax (1.0f, cellW - 1.0f), lr.getHeight() - 4.0f);
            const float rad = juce::jmin (Theme::radius - 1.0f, cell.getWidth() * 0.4f);
            if (val > 0)
            {
                auto cc = val >= 2 ? col.brighter (0.25f) : col.withMultipliedBrightness (0.85f);
                if (! inside || vm) cc = cc.withAlpha (inside ? 0.45f : 0.3f);
                g.setColour (cc);
                g.fillRoundedRectangle (cell, rad);
                if (val >= 2 && inside) { g.setColour (juce::Colours::white.withAlpha (0.8f)); g.fillRect (cell.withHeight (2.0f).reduced (1.0f, 0.0f)); }
            }
            else
            {
                g.setColour (Theme::raised.withAlpha (((c / 4) % 2 == 0) ? 0.95f : 0.6f));
                g.fillRoundedRectangle (cell, rad);
            }
            if (! inside)   // beyond the pattern end: shown dimmed, still edits the repeating step
            {
                g.setColour (Theme::deep.withAlpha (0.5f));
                g.fillRoundedRectangle (cell, rad);
            }
        }
        g.restoreState();
    }

    // bar lines
    g.saveState();
    g.reduceClipRegion (la);
    for (int c = (c0 / DrumMachine::stepsPerBar) * DrumMachine::stepsPerBar; c < c1; c += DrumMachine::stepsPerBar)
    {
        g.setColour (Theme::border.withAlpha (0.9f));
        g.fillRect (xOrigin + c * cellW - 0.5f, (float) la.getY(), 1.0f, (float) la.getHeight());
    }

    // playhead
    if (engine.getState() != AudioEngine::Stopped || engine.getPosition() > 0)
    {
        const float px = (float) la.getX() + (float) ((engine.getPosition() / juce::jmax (1.0, engine.getSampleRate()) - viewStart) / viewSeconds * (double) la.getWidth());
        g.setColour (Theme::text);
        g.fillRect (px, (float) la.getY(), 1.5f, (float) la.getHeight());
    }
    g.restoreState();
}

void DrumPanel::tick()
{
    meter.setLevels (engine.drums.peakL.exchange (0.0f), engine.drums.peakR.exchange (0.0f));
    if (! expanded) return;
    const auto la = lanesArea();
    const float px = (float) la.getX() + (float) ((engine.getPosition() / juce::jmax (1.0, engine.getSampleRate()) - viewStart) / viewSeconds * (double) la.getWidth());
    if (std::abs (px - lastPlayX) > 0.4f || engine.getState() != AudioEngine::Stopped)
    {
        repaint (la.expanded (2, 0));
        lastPlayX = px;
    }
}

void DrumPanel::mouseDown (const juce::MouseEvent& e)
{
    painting = false;
    if (! expanded || e.y < headerH) return;
    const int voice = (e.y - headerH) / laneH;
    if (voice < 0 || voice >= DrumMachine::numVoices) return;

    if (e.x < lanesArea().getX() - 6)    // lane header
    {
        if (e.x >= 110) return;
        if (e.x < 36) engine.drums.voiceMute[voice] = ! engine.drums.voiceMute[voice].load();
        else          engine.drums.audition[voice] = true;
        repaint();
        return;
    }
    const int cell = cellAt ((float) e.x);
    if (cell < 0) return;

    beforePaint = engine.drums.snapshot();
    const int cur = engine.drums.steps[voice][stepForCell (cell)].load();
    const bool accent = e.mods.isPopupMenu() || e.mods.isShiftDown();
    paintValue = accent ? (cur == 2 ? 1 : 2) : (cur == 0 ? 1 : 0);
    if (cur == 0 && ! accent) paintValue = 1;
    paintVoice = voice;
    painting = true;
    setCell (voice, cell, paintValue);
}

void DrumPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (! painting) return;
    const int cell = cellAt ((float) e.x);
    if (cell >= 0) setCell (paintVoice, cell, paintValue);
}

void DrumPanel::mouseUp (const juce::MouseEvent&)
{
    if (! painting) return;
    painting = false;
    pushPattern ("Drum pattern", beforePaint, engine.drums.snapshot());
    if (onReleaseFocus) onReleaseFocus();
}
