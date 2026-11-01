#include "BassPanel.h"
#include "audio/BassTemplates.h"

static juce::String dbText (float gain)
{
    return gain <= 0.001f ? juce::String ("-inf dB") : juce::String (juce::Decibels::gainToDecibels (gain), 1) + " dB";
}

void BassPanel::info (const juce::String& what, const juce::String& value)
{
    if (onInfo) onInfo (what + ": " + value);
}

BassPanel::BassPanel (AudioEngine& e) : engine (e)
{
    auto& bm = engine.bass;
    addAndMakeVisible (chevron);
    chevron.open = expanded;
    chevron.onClick = [this] { setExpanded (! expanded); };

    tabs.onSelect = [this] (int i) { if (onSelectTab) onSelectTab (i); };
    tabs.hasData = [this] (int i) { return i == 0 ? engine.drums.hasAnySteps() : engine.bass.hasAnySteps(); };

    auto release = [this] { if (onReleaseFocus) onReleaseFocus(); };

    for (int b = 1; b <= BassMachine::maxBars; ++b) barsBox.addItem (juce::String (b) + (b == 1 ? " bar" : " bars"), b);
    barsBox.setWantsKeyboardFocus (false);
    barsBox.setTooltip ("Pattern length. It repeats over the loop and keeps its phase");
    barsBox.onChange = [this, release]
    {
        const int nb = barsBox.getSelectedId();
        if (nb < 1 || nb == engine.bass.bars.load()) return;
        auto before = engine.bass.snapshot();
        auto after = before;
        after.bars = nb;
        const int bs = before.barSteps;
        for (int bar = before.bars; bar < nb; ++bar)       // growing: bar 1 is copied into every new, empty bar
        {
            bool empty = true;
            for (int k = 0; k < bs; ++k) empty &= ! BassMachine::isNote (after.steps[bar * bs + k]);
            if (empty) for (int k = 0; k < bs; ++k) after.steps[bar * bs + k] = after.steps[k];
        }
        pushPattern ("Bass pattern length", before, after);
        release();
    };
    addAndMakeVisible (barsBox);

    for (int m = BassMachine::rootMin; m <= BassMachine::rootMax; ++m) rootBox.addItem (BassMachine::noteName (m), m - BassMachine::rootMin + 1);
    rootBox.setWantsKeyboardFocus (false);
    rootBox.setTooltip ("Root note of the key");
    rootBox.onChange = [this, release]
    {
        const int r = BassMachine::rootMin + rootBox.getSelectedId() - 1;
        if (r == engine.bass.root.load()) return;
        auto before = engine.bass.snapshot(); auto after = before; after.root = r;
        pushPattern ("Bass root", before, after);
        release();
    };
    addAndMakeVisible (rootBox);

    for (int s = 0; s < BassMachine::numScales; ++s) scaleBox.addItem (BassMachine::scaleName (s), s + 1);
    scaleBox.setWantsKeyboardFocus (false);
    scaleBox.setTooltip ("Scale: the grid rows are the notes of this scale");
    scaleBox.onChange = [this, release]
    {
        const int s = scaleBox.getSelectedId() - 1;
        if (s == engine.bass.scale.load()) return;
        auto before = engine.bass.snapshot(); auto after = before; after.scale = s;
        pushPattern ("Bass scale", before, after);
        release();
    };
    addAndMakeVisible (scaleBox);

    templateBox.setTextWhenNothingSelected ("Template");
    templateBox.setWantsKeyboardFocus (false);
    templateBox.setTooltip ("Replace the pattern with a ready-made bass line (undoable). 'Lock to kick' puts the root on every kick");
    for (int i = 0; i <= BassTemplates::numPatterns; ++i) templateBox.addItem (BassTemplates::name (i), i + 1);
    templateBox.onChange = [this, release]
    {
        const int id = templateBox.getSelectedId() - 1;
        if (id < 0) return;
        const auto before = engine.bass.snapshot();
        const auto dr = engine.drums.snapshot();
        const auto after = BassTemplates::apply (before, id, &dr, engine.tsDen.load() == 8 ? 2 : 4);
        if (id == BassTemplates::lockToKickId && ! engine.drums.hasAnySteps()) info ("Lock to kick", "the drum pattern has no kicks yet");
        page = 0;
        pushPattern (juce::String ("Bass template: ") + BassTemplates::name (id), before, after);
        templateBox.setSelectedId (0, juce::dontSendNotification);
        release();
    };
    addAndMakeVisible (templateBox);

    progBox.setTextWhenNothingSelected ("Chords");
    progBox.setWantsKeyboardFocus (false);
    progBox.setTooltip ("Chord progression: transposes the pattern bar by bar (needs 4 bars; a shorter pattern is extended)");
    for (int i = 0; i < BassTemplates::numProgressions; ++i) progBox.addItem (BassTemplates::progression (i).name, i + 1);
    progBox.onChange = [this, release]
    {
        const int id = progBox.getSelectedId() - 1;
        if (id < 0) return;
        const auto before = engine.bass.snapshot();
        pushPattern (juce::String ("Bass chords: ") + BassTemplates::progression (id).name, before, BassTemplates::applyProgression (before, id));
        progBox.setSelectedId (0, juce::dontSendNotification);
        release();
    };
    addAndMakeVisible (progBox);

    clearBtn.setWantsKeyboardFocus (false);
    clearBtn.setTooltip ("Clear the bass pattern");
    clearBtn.onClick = [this, release]
    {
        auto before = engine.bass.snapshot(); auto after = before;
        std::memset (after.steps, 0, sizeof (after.steps));
        pushPattern ("Clear bass pattern", before, after);
        release();
    };
    addAndMakeVisible (clearBtn);

    bakeBtn.setWantsKeyboardFocus (false);
    bakeBtn.setTooltip ("Render the bass line to a normal audio track (and mute the synth so it does not play twice)");
    bakeBtn.onClick = [this, release] { if (onBake) onBake(); release(); };
    addAndMakeVisible (bakeBtn);
    liveBtn.setWantsKeyboardFocus (false);
    liveBtn.setClickingTogglesState (false);
    liveBtn.setColour (juce::TextButton::buttonOnColourId, Theme::red);
    liveBtn.setTooltip ("Record the bass for one loop pass while you play (turn knobs such as Cutoff): it becomes a new audio track");
    liveBtn.onClick = [this, release] { if (onBakeLive) onBakeLive(); release(); };
    addAndMakeVisible (liveBtn);

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
    volume.setColour (juce::Slider::rotarySliderFillColourId, Theme::cyan);
    volume.setWantsKeyboardFocus (false);
    volume.setTooltip ("Bass volume");
    volume.onValueChange = [this] { engine.bass.volume = (float) volume.getValue(); info ("Bass volume", dbText ((float) volume.getValue())); };
    addAndMakeVisible (volume);

    auto styleToggle = [] (juce::TextButton& b)
    {
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonColourId, Theme::raised);
        b.setColour (juce::TextButton::buttonOnColourId, Theme::accent);
        b.setColour (juce::TextButton::textColourOffId, Theme::text);
        b.setColour (juce::TextButton::textColourOnId, Theme::onAccent);
        b.setWantsKeyboardFocus (false);
    };
    styleToggle (muteBtn); styleToggle (soloBtn);
    styleToggle (echoBtn); styleToggle (reverbBtn);
    echoBtn.setButtonText ("Echo"); reverbBtn.setButtonText ("Rev");
    echoBtn.setTooltip ("Echo on/off. Right-click to choose the timing (1/2, 1/4, 1/8 dotted, 1/8, 1/16)");
    reverbBtn.setTooltip ("Reverb on/off");
    echoBtn.onClick = [this, release]
    {
        engine.bass.echoOn = echoBtn.getToggleState();
        info ("Bass echo", echoBtn.getToggleState() ? juce::String (TrackFx::divName (engine.bass.echoDiv.load())) : juce::String ("off"));
        release();
    };
    echoBtn.onRight = [this]
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Echo timing");
        for (int d = 0; d < TrackFx::numDivs; ++d) m.addItem (d + 1, TrackFx::divName (d), true, d == engine.bass.echoDiv.load());
        m.showMenuAsync (juce::PopupMenu::Options(), [this] (int id)
        {
            if (id <= 0) return;
            engine.bass.echoDiv = id - 1; engine.bass.echoOn = true;
            echoBtn.setToggleState (true, juce::dontSendNotification);
            info ("Bass echo", TrackFx::divName (id - 1));
        });
    };
    reverbBtn.onClick = [this, release]
    {
        engine.bass.reverbOn = reverbBtn.getToggleState();
        info ("Bass reverb", reverbBtn.getToggleState() ? juce::String ("on") : juce::String ("off"));
        release();
    };
    addAndMakeVisible (echoBtn); addAndMakeVisible (reverbBtn);
    muteBtn.onClick = [this, release] { engine.bass.mute = muteBtn.getToggleState(); release(); };
    soloBtn.onClick = [this, release] { engine.bass.solo = soloBtn.getToggleState(); release(); };
    addAndMakeVisible (muteBtn); addAndMakeVisible (soloBtn);

    meter.setHorizontal (true);
    addAndMakeVisible (meter);

    randBtn.setWantsKeyboardFocus (false);
    randBtn.setTooltip ("Random notes in the key and scale. Click to set grid, density, lengths and range; right-click rolls again with the same settings");
    randBtn.onClick = [this] { showRandomOptions(); };
    randBtn.addMouseListener (this, false);          // right-click: roll again with the same settings
    addAndMakeVisible (randBtn);

    // ---- sound
    presetBox.setTextWhenNothingSelected ("Sound");
    presetBox.setWantsKeyboardFocus (false);
    presetBox.setTooltip ("Load a ready-made bass sound");
    for (int i = 0; i < BassMachine::numPresets; ++i) presetBox.addItem (BassMachine::preset (i).name, i + 1);
    presetBox.onChange = [this, release]
    {
        const int id = presetBox.getSelectedId() - 1;
        if (id < 0) return;
        engine.bass.applyPreset (id);
        refreshFromEngine();
        info ("Bass sound", BassMachine::preset (id).name);
        presetBox.setSelectedId (0, juce::dontSendNotification);
        release();
    };
    addAndMakeVisible (presetBox);

    for (int w = 0; w < 3; ++w) waveBox.addItem (BassMachine::waveName (w), w + 1);
    waveBox.setWantsKeyboardFocus (false);
    waveBox.setTooltip ("Oscillator waveform");
    waveBox.onChange = [this, release] { engine.bass.wave = waveBox.getSelectedId() - 1; release(); };
    addAndMakeVisible (waveBox);

    struct Def { const char* name; double lo, hi, def; std::atomic<float>* target; const char* unit; double skewMid; };
    const Def defs[8] = {
        { "Sub",     0.0,   1.0,   0.5,   &bm.sub,     "",    0.0 },
        { "Cutoff",  30.0,  12000.0, 400.0, &bm.cutoff, " Hz", 500.0 },
        { "Reso",    0.0,   0.97,  0.2,   &bm.reso,    "",    0.0 },
        { "Env",     0.0,   1.0,   0.4,   &bm.envAmt,  "",    0.0 },
        { "Decay",   0.03,  1.5,   0.3,   &bm.decay,   " s",  0.0 },
        { "Sustain", 0.0,   1.0,   0.7,   &bm.sustain, "",    0.0 },
        { "Glide",   0.0,   300.0, 0.0,   &bm.glide,   " ms", 0.0 },
        { "Drive",   0.0,   1.0,   0.2,   &bm.drive,   "",    0.0 } };
    for (const auto& d : defs)
    {
        auto* k = knobs.add (new Knob());
        k->name = d.name;
        auto& s = k->s;
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRange (d.lo, d.hi, 0.0);
        if (d.skewMid > 0.0) s.setSkewFactorFromMidPoint (d.skewMid);
        s.setDoubleClickReturnValue (true, d.def);
        s.getProperties().set ("uni", d.lo >= 0.0);
        s.setColour (juce::Slider::rotarySliderFillColourId, Theme::cyan);
        s.setWantsKeyboardFocus (false);
        s.setTooltip (d.name);
        const juce::String unit (d.unit);
        auto* target = d.target;
        const juce::String nm (d.name);
        k->fmt = [unit] (double v) { return (unit == " Hz" ? juce::String ((int) std::lround (v)) : juce::String (v, unit == " ms" ? 0 : 2)) + unit; };
        auto* kp = k;
        s.onValueChange = [this, kp, target, nm] { *target = (float) kp->s.getValue(); info ("Bass " + nm.toLowerCase(), kp->fmt (kp->s.getValue())); };
        addChildComponent (s);
    }
    refreshFromEngine();
}

void BassPanel::lookAndFeelChanged()
{
    pageLabel.setColour (juce::Label::textColourId, Theme::dim);
    liveBtn.setColour (juce::TextButton::buttonOnColourId, Theme::red);
    for (auto* b : { &muteBtn, &soloBtn, (juce::TextButton*) &echoBtn, (juce::TextButton*) &reverbBtn })
    {
        b->setColour (juce::TextButton::buttonColourId, Theme::raised);
        b->setColour (juce::TextButton::buttonOnColourId, Theme::accent);
        b->setColour (juce::TextButton::textColourOffId, Theme::text);
        b->setColour (juce::TextButton::textColourOnId, Theme::onAccent);
    }
}

void BassPanel::setLiveState (int state)
{
    liveBtn.setToggleState (state != 0, juce::dontSendNotification);
    liveBtn.setButtonText (state == 1 ? "Wait" : state == 2 ? "REC" : "Rec");
}

void BassPanel::generateRandom()
{
    const auto before = engine.bass.snapshot();
    page = 0;
    pushPattern ("Random bass line", before, BassRandom::generate (before, randOpts, rng));
    refreshFromEngine();
}

void BassPanel::showRandomOptions()
{
    struct Options : juce::Component
    {
        BassPanel& owner;
        juce::ComboBox grid, length, range, scope;
        juce::Slider density, rootBias, accent, slide;
        juce::Label l[8];
        juce::TextButton go { "Generate" };

        explicit Options (BassPanel& p) : owner (p)
        {
            auto& o = p.randOpts;
            const char* const names[8] = { "Grid", "Density", "Lengths", "Range", "Bars", "Root / fifth", "Accents", "Slides" };
            for (int i = 0; i < 8; ++i) { l[i].setText (names[i], juce::dontSendNotification); l[i].setColour (juce::Label::textColourId, Theme::dim); addAndMakeVisible (l[i]); }
            grid.addItem ("1/4 notes", 1); grid.addItem ("1/8 notes", 2); grid.addItem ("1/16 notes", 3); grid.setSelectedId (o.grid + 1, juce::dontSendNotification);
            length.addItem ("Fixed (one grid step)", 1); length.addItem ("Random", 2); length.setSelectedId (o.randomLength ? 2 : 1, juce::dontSendNotification);
            range.addItem ("1 octave", 1); range.addItem ("2 octaves", 2); range.addItem ("3 octaves", 3); range.setSelectedId (o.octaves, juce::dontSendNotification);
            scope.addItem ("Different every bar", 1); scope.addItem ("Repeat bar 1", 2); scope.setSelectedId (o.repeatBar ? 2 : 1, juce::dontSendNotification);
            for (auto* s : { &density, &rootBias, &accent, &slide })
            {
                s->setSliderStyle (juce::Slider::LinearHorizontal);
                s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
                s->setNumDecimalPlacesToDisplay (0);
                addAndMakeVisible (s);
            }
            density.setRange (5.0, 100.0, 1.0); density.setValue (o.density * 100.0, juce::dontSendNotification);
            rootBias.setRange (0.0, 100.0, 1.0); rootBias.setValue (o.rootBias * 100.0, juce::dontSendNotification);
            accent.setRange (0.0, 100.0, 1.0); accent.setValue (o.accent * 100.0, juce::dontSendNotification);
            slide.setRange (0.0, 100.0, 1.0); slide.setValue (o.slide * 100.0, juce::dontSendNotification);
            for (auto* c : { &grid, &length, &range, &scope }) addAndMakeVisible (c);
            go.onClick = [this] { apply(); owner.generateRandom(); };
            addAndMakeVisible (go);
            setSize (320, 12 + 8 * 30 + 44);
        }
        void apply()
        {
            auto& o = owner.randOpts;
            o.grid = grid.getSelectedId() - 1;
            o.randomLength = length.getSelectedId() == 2;
            o.octaves = range.getSelectedId();
            o.repeatBar = scope.getSelectedId() == 2;
            o.density = (float) density.getValue() / 100.0f;
            o.rootBias = (float) rootBias.getValue() / 100.0f;
            o.accent = (float) accent.getValue() / 100.0f;
            o.slide = (float) slide.getValue() / 100.0f;
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (12, 8);
            juce::Component* ctl[8] = { &grid, &density, &length, &range, &scope, &rootBias, &accent, &slide };
            for (int i = 0; i < 8; ++i)
            {
                auto row = r.removeFromTop (30);
                l[i].setBounds (row.removeFromLeft (92));
                ctl[i]->setBounds (row.reduced (0, 3));
            }
            r.removeFromTop (6);
            go.setBounds (r.removeFromTop (28));
        }
        void paint (juce::Graphics& g) override { g.fillAll (Theme::panel); }
    };
    juce::CallOutBox::launchAsynchronously (std::make_unique<Options> (*this), randBtn.getScreenBounds(), nullptr);
}

void BassPanel::setExpanded (bool b)
{
    expanded = b;
    chevron.open = b;
    chevron.repaint();
    updateControls();
    if (onLayoutChanged) onLayoutChanged();
}

void BassPanel::setView (double seconds, double barSecs, double start, double full)
{
    viewSeconds = seconds; barSeconds = barSecs; viewStart = start;
    fullSeconds = full > 0.0 ? full : seconds;
    updateControls();
}

void BassPanel::refreshFromEngine()
{
    auto& bm = engine.bass;
    barsBox.setSelectedId (bm.bars.load(), juce::dontSendNotification);
    rootBox.setSelectedId (bm.root.load() - BassMachine::rootMin + 1, juce::dontSendNotification);
    scaleBox.setSelectedId (bm.scale.load() + 1, juce::dontSendNotification);
    waveBox.setSelectedId (bm.wave.load() + 1, juce::dontSendNotification);
    volume.setValue (bm.volume.load(), juce::dontSendNotification);
    muteBtn.setToggleState (bm.mute.load(), juce::dontSendNotification);
    soloBtn.setToggleState (bm.solo.load(), juce::dontSendNotification);
    echoBtn.setToggleState (bm.echoOn.load(), juce::dontSendNotification);
    reverbBtn.setToggleState (bm.reverbOn.load(), juce::dontSendNotification);
    const float vals[8] = { bm.sub.load(), bm.cutoff.load(), bm.reso.load(), bm.envAmt.load(), bm.decay.load(), bm.sustain.load(), bm.glide.load(), bm.drive.load() };
    for (int i = 0; i < knobs.size(); ++i) knobs[i]->s.setValue (vals[i], juce::dontSendNotification);
    updateControls();
    repaint();
}

double BassPanel::stepSec() const
{
    return barSeconds > 0.0 ? barSeconds / engine.bass.barSteps.load() : fullSeconds / (4.0 * engine.bass.barSteps.load());
}

int BassPanel::viewSteps() const
{
    return barSeconds > 0.0 ? juce::jmax (1, (int) std::lround (fullSeconds / barSeconds) * engine.bass.barSteps.load()) : 4 * engine.bass.barSteps.load();
}

void BassPanel::updateControls()
{
    const int ps = engine.bass.patternSteps(), vs = viewSteps();
    const int pages = juce::jmax (1, (ps + vs - 1) / vs);
    page = juce::jlimit (0, pages - 1, page);
    const bool paged = expanded && pages > 1;
    for (auto* k : knobs) k->s.setVisible (expanded);
    presetBox.setVisible (expanded); waveBox.setVisible (expanded);
    const bool pagedChanged = prevPage.isVisible() != paged;
    prevPage.setVisible (paged); nextPage.setVisible (paged); pageLabel.setVisible (paged);
    if (pagedChanged) resized();
    prevPage.setEnabled (page > 0); nextPage.setEnabled (page < pages - 1);
    if (paged)
    {
        const int bs = engine.bass.barSteps.load();
        const int b0 = page * vs / bs + 1, b1 = juce::jmin (engine.bass.bars.load(), b0 + vs / bs - 1);
        pageLabel.setText ("bars " + juce::String (b0) + "-" + juce::String (b1), juce::dontSendNotification);
    }
}

juce::Rectangle<int> BassPanel::gridArea() const
{
    return { TrackRow::waveLeft, headerH + shiftH, juce::jmax (1, getWidth() - TrackRow::waveLeft - TrackRow::waveRightInset), gridH };
}

juce::Rectangle<int> BassPanel::stripArea() const
{
    return { TrackRow::waveLeft, headerH, juce::jmax (1, getWidth() - TrackRow::waveLeft - TrackRow::waveRightInset), shiftH };
}

int BassPanel::cellAt (float x, bool clamp) const
{
    const auto a = gridArea();
    if (! clamp && (x < (float) a.getX() || x >= (float) a.getRight())) return -1;
    const double secs = (double) (x - (float) a.getX()) / (double) a.getWidth() * viewSeconds + viewStart;
    return juce::jlimit (0, viewSteps() - 1, (int) std::floor (secs / stepSec()));
}

int BassPanel::stepForCell (int cell) const
{
    return (page * viewSteps() + cell) % engine.bass.patternSteps();
}

int BassPanel::rowAt (int y) const
{
    const int r = (y - headerH - shiftH) / rowHeight();
    if (y < headerH + shiftH || r < 0 || r >= rowsNow()) return -1;
    return rowsNow() - 1 - r;
}

int BassPanel::effectiveLen (int step) const
{
    const int ps = engine.bass.patternSteps();
    const auto c = engine.bass.steps[step].load();
    int len = BassMachine::lenOf (c);
    for (int j = 1; j < len; ++j)
        if (BassMachine::isNote (engine.bass.steps[(step + j) % ps].load())) { len = j; break; }
    return len;
}

void BassPanel::pushPattern (const juce::String& name, const BassMachine::Snapshot& before, const BassMachine::Snapshot& after)
{
    if (before == after) return;
    auto apply = [this] (const BassMachine::Snapshot& s) { engine.bass.apply (s); refreshFromEngine(); };
    if (onUndoable) onUndoable (name, [apply, after] { apply (after); }, [apply, before] { apply (before); });
    else apply (after);
}

void BassPanel::resized()
{
    auto r = juce::Rectangle<int> (0, 0, getWidth(), headerH).reduced (12, 7);
    const int h = r.getHeight(), y = r.getY();
    // right group first (it never gets squeezed): volume, M, S, Clear, Echo, Rev, Rec, Bake, then the VU meter if there is room
    const int rightFixed = 390, leftFixed = 6 + 24 + 64 + 8 + 30 + (prevPage.isVisible() ? 134 : 0), comboBase = 496;
    const int avail = r.getWidth();
    const int meterRoom = avail - (leftFixed + comboBase) - rightFixed - 16;
    const int meterW = meterRoom >= 60 ? juce::jmin (190, meterRoom) : 0;
    meter.setVisible (meterW > 0);
    int x = r.getRight() - 4;
    if (meterW > 0) { meter.setBounds (juce::Rectangle<int> (x - meterW, y, meterW, h).withSizeKeepingCentre (meterW, 14)); x -= meterW + 16; }
    auto put = [&] (juce::Component& c, int w, int gap) { x -= w; c.setBounds (x, y, w, h); x -= gap; };
    put (bakeBtn, 52, 4); put (liveBtn, 44, 6); put (reverbBtn, 40, 4); put (echoBtn, 48, 12);
    put (clearBtn, 56, 12); put (soloBtn, 30, 4); put (muteBtn, 30, 12);
    x -= 32; volume.setBounds (juce::Rectangle<int> (x, y, 32, h).withSizeKeepingCentre (32, 32));
    const int rightEdge = x - 8;

    // left group; the combo boxes shrink when the window is narrow
    const float f = juce::jlimit (0.5f, 1.0f, (float) (rightEdge - r.getX() - leftFixed) / (float) comboBase);
    auto w = [f] (int v) { return (int) std::lround (v * f); };
    juce::Rectangle<int> l (r.getX(), y, juce::jmax (0, rightEdge - r.getX()), h);
    l.removeFromLeft (6);
    chevron.setBounds (l.removeFromLeft (24));
    barsBox.setBounds (l.removeFromLeft (w (84))); l.removeFromLeft (6);
    rootBox.setBounds (l.removeFromLeft (w (54))); l.removeFromLeft (6);
    scaleBox.setBounds (l.removeFromLeft (w (128))); l.removeFromLeft (6);
    templateBox.setBounds (l.removeFromLeft (w (112))); l.removeFromLeft (6);
    progBox.setBounds (l.removeFromLeft (w (118))); l.removeFromLeft (6);
    randBtn.setBounds (l.removeFromLeft (64)); l.removeFromLeft (8);
    prevPage.setBounds (l.removeFromLeft (26)); l.removeFromLeft (4);
    pageLabel.setBounds (l.removeFromLeft (72)); l.removeFromLeft (4);
    nextPage.setBounds (l.removeFromLeft (26));
    presetBox.setBounds (14, headerH + 2, 150, 24);
    waveBox.setBounds (172, headerH + 2, 96, 24);
    for (int i = 0; i < knobs.size(); ++i)
    {
        const int col = i % 4, row = i / 4;
        knobs[i]->s.setBounds (14 + col * 92 + 26, headerH + 34 + row * 60, 38, 38);
    }
}

void BassPanel::paint (juce::Graphics& g)
{
    const auto accent = Theme::cyan;
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

    const auto& bm = engine.bass;
    const int vs = viewSteps(), ps = bm.patternSteps(), bs = bm.barSteps.load();
    const auto ga = gridArea();
    const int n = BassMachine::scaleSize (bm.scale.load());
    Theme::wellRing (g, juce::Rectangle<int> (ga.getX(), headerH, ga.getWidth(), shiftH + ga.getHeight()).toFloat(), 3.0f);
    const double pps = (double) ga.getWidth() / viewSeconds;
    const double stepS = stepSec();
    const float cellW = (float) (stepS * pps);
    const float xOrigin = (float) ga.getX() - (float) (viewStart * pps);
    const int c0 = juce::jmax (0, (int) std::floor (viewStart / stepS)), c1 = juce::jmin (vs, (int) std::ceil ((viewStart + viewSeconds) / stepS));

    // left column captions
    g.setColour (Theme::dim);
    g.setFont (juce::Font (juce::FontOptions (10.5f)));
    for (int i = 0; i < knobs.size(); ++i)
    {
        const int col = i % 4, row = i / 4;
        g.drawText (knobs[i]->name, 14 + col * 92, headerH + 34 + row * 60 + 38, 92, 13, juce::Justification::centred);
    }

    // bar transposition strip
    {
        g.setColour (Theme::deep);
        g.fillRect (stripArea());
        g.saveState();
        g.reduceClipRegion (stripArea());
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        for (int c = (c0 / bs) * bs; c < c1; c += bs)
        {
            const int s = page * vs + c, bar = s / bs;
            if (bar >= bm.bars.load()) continue;
            const int sh = bm.shift[bar].load();
            g.setColour (sh == 0 ? Theme::dim : Theme::cyan);
            g.drawText (BassTemplates::shiftName (sh), (int) (xOrigin + c * cellW) + 4, headerH, 60, shiftH, juce::Justification::centredLeft);
        }
        g.restoreState();
    }

    g.saveState();
    g.reduceClipRegion (ga);
    const bool chromatic = bm.scale.load() == 6;
    for (int row = 0; row < rowsNow(); ++row)
    {
        const int d = baseDegree() + row;
        const bool isRoot = ((d % n) + n) % n == 0;
        const int pc = ((bm.root.load() + BassMachine::semitones (bm.scale.load(), d)) % 12 + 12) % 12;
        const bool blackKey = chromatic && (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);   // piano colouring when every note is shown
        const juce::Rectangle<float> lr ((float) ga.getX(), rowY (row), (float) ga.getWidth(), (float) rowHeight());
        g.setColour (isRoot ? Theme::raised : blackKey ? Theme::deep.darker (0.35f) : (chromatic ? Theme::deep.brighter (0.06f) : Theme::deep));
        g.fillRect (lr);
        g.setColour (Theme::border.withAlpha (0.35f));
        g.fillRect (lr.withHeight (1.0f));
    }
    for (int c = c0; c < c1; ++c)           // beat shading
    {
        if ((c / 4) % 2 != 0) continue;
        g.setColour (juce::Colours::white.withAlpha (0.03f));
        g.fillRect (xOrigin + c * cellW, (float) ga.getY(), cellW, (float) ga.getHeight());
    }
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    for (int row = 0; row < rowsNow(); ++row)    // note names
    {
        const int d = baseDegree() + row;
        g.setColour (Theme::dim.withAlpha (0.75f));
        const int midi = bm.root.load() + BassMachine::semitones (bm.scale.load(), d);
        const bool isC = ((d % n) + n) % n == 0;
        g.drawText (juce::String (BassMachine::noteName (midi)) + (isC ? juce::String (midi / 12 - 1) : juce::String()), ga.getX() + 3, (int) rowY (row), 26, rowHeight(), juce::Justification::centredLeft);
    }
    for (int c = c0; c < c1; ++c)
    {
        const int s = page * vs + c;
        const bool inside = s < ps;
        const auto cell = bm.steps[s % ps].load();
        if (! BassMachine::isNote (cell)) continue;
        const int row = BassMachine::degreeOf (cell) - baseDegree();
        if (row < 0 || row >= rowsNow()) continue;
        const int len = effectiveLen (s % ps);
        const float x = xOrigin + c * cellW;
        const juce::Rectangle<float> nr (x + 0.5f, rowY (row) + 1.5f, juce::jmax (2.0f, cellW * (float) len - 1.0f), (float) rowHeight() - 3.0f);
        auto col = BassMachine::accentOf (cell) ? accent.brighter (0.3f) : accent.withMultipliedBrightness (0.85f);
        if (! inside || bm.mute.load()) col = col.withAlpha (inside ? 0.45f : 0.3f);
        g.setColour (col);
        g.fillRoundedRectangle (nr, Theme::radius - 1.0f);
        if (nr.getWidth() > 9.0f && inside) { g.setColour (juce::Colours::white.withAlpha (0.28f)); g.fillRect (nr.getRight() - 3.0f, nr.getY() + 2.0f, 1.5f, nr.getHeight() - 4.0f); }   // grab handle: drag to change the length
        if (BassMachine::accentOf (cell) && inside) { g.setColour (juce::Colours::white.withAlpha (0.8f)); g.fillRect (nr.withHeight (2.0f).reduced (1.0f, 0.0f)); }
        if (BassMachine::slideOf (cell) && inside)   // slide: a small notch at the start of the note
        {
            juce::Path p;
            p.addTriangle (nr.getX(), nr.getBottom(), nr.getX() + 5.0f, nr.getBottom(), nr.getX(), nr.getBottom() - 5.0f);
            g.setColour (Theme::onAccent);
            g.fillPath (p);
        }
    }
    for (int c = (c0 / bs) * bs; c < c1; c += bs)   // bar lines
    {
        g.setColour (Theme::border.withAlpha (0.9f));
        g.fillRect (xOrigin + c * cellW - 0.5f, (float) ga.getY(), 1.0f, (float) ga.getHeight());
    }
    if (engine.getState() != AudioEngine::Stopped || engine.getPosition() > 0)
    {
        const float px = (float) ga.getX() + (float) ((engine.getPosition() / juce::jmax (1.0, engine.getSampleRate()) - viewStart) / viewSeconds * (double) ga.getWidth());
        g.setColour (Theme::text);
        g.fillRect (px, (float) ga.getY(), 1.5f, (float) ga.getHeight());
    }
    g.restoreState();
}

void BassPanel::tick()
{
    meter.setLevels (engine.bass.peakL.exchange (0.0f), engine.bass.peakR.exchange (0.0f));
    tabs.refresh();
    if (! expanded || ! isVisible()) return;
    const auto ga = gridArea();
    const float px = (float) ga.getX() + (float) ((engine.getPosition() / juce::jmax (1.0, engine.getSampleRate()) - viewStart) / viewSeconds * (double) ga.getWidth());
    if (std::abs (px - lastPlayX) > 0.4f || engine.getState() != AudioEngine::Stopped)
    {
        repaint (ga.expanded (2, 0));
        lastPlayX = px;
    }
}

void BassPanel::audition (int step, int degree)
{
    const auto& bm = engine.bass;
    const int bar = step / juce::jmax (1, bm.barSteps.load());
    engine.bass.auditionNote = bm.root.load() + BassMachine::semitones (bm.scale.load(), degree) + bm.shift[juce::jlimit (0, BassMachine::maxBars - 1, bar)].load();
}

void BassPanel::showShiftMenu (int bar)
{
    juce::PopupMenu m;
    const int cur = engine.bass.shift[bar].load();
    const int opts[] = { 0, 2, 3, 4, 5, 7, 8, 9, 10, -2, -5, -7 };
    int id = 1;
    for (int o : opts) m.addItem (id++, BassTemplates::shiftName (o) + (o != 0 ? "   (" + juce::String (o > 0 ? "+" : "") + juce::String (o) + ")" : juce::String()), true, o == cur);
    m.showMenuAsync (juce::PopupMenu::Options(), [this, bar, opts] (int r)
    {
        if (r < 1) return;
        auto before = engine.bass.snapshot(); auto after = before;
        after.shift[bar] = opts[r - 1];
        pushPattern ("Bass bar transposition", before, after);
    });
}

float BassPanel::cellX (int cell) const
{
    const auto ga = gridArea();
    const double pps = (double) ga.getWidth() / viewSeconds;
    return (float) ga.getX() - (float) (viewStart * pps) + (float) cell * (float) (stepSec() * pps);
}

int BassPanel::findNoteAt (int step, int degree) const
{
    const int ps = engine.bass.patternSteps();
    for (int j = 0; j < BassMachine::maxLen && j < ps; ++j)
    {
        const int h = ((step - j) % ps + ps) % ps;
        const auto c = engine.bass.steps[h].load();
        if (BassMachine::isNote (c) && BassMachine::degreeOf (c) == degree && j < effectiveLen (h)) return h;
    }
    return -1;
}

void BassPanel::eraseRange (int cellA, int cellB, int row)
{
    if (row < 0) return;
    const int degree = baseDegree() + row;
    for (int c = juce::jmin (cellA, cellB); c <= juce::jmax (cellA, cellB); ++c)
    {
        const int h = findNoteAt (stepForCell (c), degree);
        if (h >= 0) engine.bass.steps[h] = 0u;
    }
}

void BassPanel::mouseDown (const juce::MouseEvent& e)
{
    mode = Mode::None;
    if (e.eventComponent == &randBtn) { if (e.mods.isPopupMenu()) generateRandom(); return; }
    if (! expanded || e.y < headerH) return;
    const int cell = cellAt ((float) e.x, false);
    if (cell < 0) return;

    if (e.y < headerH + shiftH)    // bar transposition strip
    {
        const int bar = (page * viewSteps() + cell) / engine.bass.barSteps.load();
        if (bar < engine.bass.bars.load()) showShiftMenu (bar);
        return;
    }
    const int row = rowAt (e.y);
    if (row < 0) return;

    auto& bm = engine.bass;
    const int ps = bm.patternSteps();
    const int step = stepForCell (cell), degree = baseDegree() + row;
    beforePaint = bm.snapshot();
    const int head = findNoteAt (step, degree);

    if (head >= 0)
    {
        const auto cur = bm.steps[head].load();
        if (e.mods.isPopupMenu() || e.mods.isCtrlDown())      // flags on an existing note
        {
            const bool acc = e.mods.isPopupMenu() ? ! BassMachine::accentOf (cur) : BassMachine::accentOf (cur);
            const bool sl = e.mods.isPopupMenu() ? BassMachine::slideOf (cur) : ! BassMachine::slideOf (cur);
            bm.steps[head] = BassMachine::pack (BassMachine::degreeOf (cur), BassMachine::lenOf (cur), sl, acc);
            pushPattern (e.mods.isPopupMenu() ? "Bass accent" : "Bass slide", beforePaint, bm.snapshot());
            repaint (gridArea());
            return;
        }
        pendingHead = head;
        pendingJ = ((step - head) % ps + ps) % ps;
        drawCell = cell - pendingJ;                           // visible cell of the note's head
        drawDegree = degree;
        lastCell = cell;
        const float right = cellX (drawCell + effectiveLen (head));
        mode = (float) e.x >= right - 7.0f ? Mode::Resize : Mode::Pending;   // grab the right edge to change the length
        return;
    }
    mode = Mode::Create;
    drawStep = step; drawCell = cell; drawDegree = degree;
    drawAccent = e.mods.isShiftDown() ? 1 : 0;
    bm.steps[step] = BassMachine::pack (degree, 1, false, drawAccent != 0);
    audition (step, degree);
    repaint (gridArea());
}

void BassPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (mode == Mode::None) return;
    auto& bm = engine.bass;
    const int cell = cellAt ((float) e.x, true);
    const int row = rowAt (juce::jlimit (headerH + shiftH, headerH + shiftH + gridH - 1, e.y));

    if (mode == Mode::Create)
    {
        const int len = juce::jlimit (1, BassMachine::maxLen, cell - drawCell + 1);
        int degree = drawDegree;
        if (row >= 0) degree = baseDegree() + row;
        if (degree != drawDegree) { drawDegree = degree; audition (drawStep, degree); }
        bm.steps[drawStep] = BassMachine::pack (degree, len, false, drawAccent != 0);
    }
    else if (mode == Mode::Resize)
    {
        const auto cur = bm.steps[pendingHead].load();
        if (! BassMachine::isNote (cur)) return;
        const int len = juce::jlimit (1, BassMachine::maxLen, cell - drawCell + 1);
        bm.steps[pendingHead] = BassMachine::pack (BassMachine::degreeOf (cur), len, BassMachine::slideOf (cur), BassMachine::accentOf (cur));
    }
    else
    {
        if (mode == Mode::Pending)
        {
            if (e.getDistanceFromDragStart() < 3) return;
            mode = Mode::Erase;
            bm.steps[pendingHead] = 0u;                           // the pressed note goes first
        }
        eraseRange (lastCell, cell, row);
        lastCell = cell;
    }
    repaint (gridArea());
}

void BassPanel::mouseUp (const juce::MouseEvent&)
{
    if (mode == Mode::None) return;
    if (mode == Mode::Pending) engine.bass.steps[pendingHead] = 0u;        // a plain click removes the note
    const bool wasErase = mode == Mode::Erase || mode == Mode::Pending;
    const bool wasResize = mode == Mode::Resize;
    mode = Mode::None;
    pushPattern (wasErase ? "Erase bass notes" : wasResize ? "Bass note length" : "Bass pattern", beforePaint, engine.bass.snapshot());
    repaint (gridArea());
    if (onReleaseFocus) onReleaseFocus();
}
void BassPanel::scrollView (int rows)
{
    const int size = BassMachine::scaleSize (engine.bass.scale.load());
    const int old = viewShift;
    viewShift = juce::jlimit (-2 * size, 3 * size, viewShift + rows);
    if (viewShift == old) return;
    const int lo = engine.bass.root.load() + BassMachine::semitones (engine.bass.scale.load(), baseDegree());
    info ("Bass range", juce::String (BassMachine::noteName (lo)) + juce::String (lo / 12 - 1) + " and up");
    repaint();
}
