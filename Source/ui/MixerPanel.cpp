#include "MixerPanel.h"

static juce::String dbStr (float pos)
{
    const float g = faderToGain (pos);
    return g <= 0.0f ? juce::String ("-inf") : juce::String (juce::Decibels::gainToDecibels (g), 1);
}

static void styleFlatToggle (juce::TextButton& b, juce::Colour on)
{
    b.setClickingTogglesState (true);
    b.setColour (juce::TextButton::buttonColourId, Theme::raised);
    b.setColour (juce::TextButton::buttonOnColourId, on);
    b.setColour (juce::TextButton::textColourOffId, Theme::text);
    b.setColour (juce::TextButton::textColourOnId, Theme::onAccent);
    b.setWantsKeyboardFocus (false);
}

ChannelStrip::ChannelStrip (AudioEngine& e, int s, int orderPos) : engine (e), slotIndex (s), pos (orderPos)
{
    const auto fillCol = isMaster() ? Theme::purple : Theme::accent;

    meter.setHorizontal (false);
    addAndMakeVisible (meter);

    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    fader.setRange (0.0, 1.0, 0.0);
    fader.setDoubleClickReturnValue (true, gainToFader (1.0f));
    fader.setColour (juce::Slider::trackColourId, fillCol);
    fader.setWantsKeyboardFocus (false);
    fader.setTooltip ("Volume");
    fader.onDragStart = [this] { dragging = true; };
    fader.onDragEnd = [this] { dragging = false; if (isMaster()) commitMaster(); };
    fader.onValueChange = [this]
    {
        const float v = (float) fader.getValue(), g = faderToGain (v);
        dbLabel.setText (dbStr (v) + " dB", juce::dontSendNotification);
        if (isMaster())
        {
            engine.masterVolume = g;
            if (! dragging) commitMaster();
            info ("Master volume", dbStr (v) + " dB");
        }
        else
        {
            track().volume = g;
            if (onChanged) onChanged (slotIndex);
            info (track().name + " volume", dbStr (v) + " dB");
        }
    };
    addAndMakeVisible (fader);

    dbLabel.setJustificationType (juce::Justification::centred);
    dbLabel.setColour (juce::Label::textColourId, Theme::dim);
    dbLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    dbLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (dbLabel);

    if (! isMaster())
    {
        pan.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        pan.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        pan.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        pan.setRange (-1.0, 1.0, 0.01);
        pan.setDoubleClickReturnValue (true, 0.0);
        pan.setColour (juce::Slider::rotarySliderFillColourId, Theme::accent);
        pan.setWantsKeyboardFocus (false);
        pan.setTooltip ("Pan");
        pan.onValueChange = [this]
        {
            track().pan = (float) pan.getValue();
            if (onChanged) onChanged (slotIndex);
            const int pct = (int) std::lround (std::abs (pan.getValue()) * 100.0);
            info (track().name + " pan", pct == 0 ? juce::String ("center") : (pan.getValue() < 0 ? "left " : "right ") + juce::String (pct) + "%");
        };
        addAndMakeVisible (pan);

        styleFlatToggle (muteBtn, Theme::accent);
        styleFlatToggle (soloBtn, Theme::accent);
        muteBtn.setTooltip ("Mute");
        soloBtn.setTooltip ("Solo");
        muteBtn.onClick = [this] { track().mute = muteBtn.getToggleState(); if (onChanged) onChanged (slotIndex); };
        soloBtn.onClick = [this] { track().solo = soloBtn.getToggleState(); if (onChanged) onChanged (slotIndex); };
        addAndMakeVisible (muteBtn);
        addAndMakeVisible (soloBtn);

        echoBtn.setButtonText ("Echo");
        reverbBtn.setButtonText ("Rev");
        styleFlatToggle (echoBtn, Theme::accent);
        styleFlatToggle (reverbBtn, Theme::accent);
        echoBtn.setTooltip ("Echo on/off. Right-click to choose the timing (1/2, 1/4, 1/8 dotted, 1/8, 1/16)");
        reverbBtn.setTooltip ("Reverb on/off");
        echoBtn.onClick = [this]
        {
            track().echoOn = echoBtn.getToggleState();
            info (track().name + " echo", echoBtn.getToggleState() ? juce::String (TrackFx::divName (track().echoDiv.load())) : juce::String ("off"));
        };
        echoBtn.onRight = [this] { showEchoMenu(); };
        reverbBtn.onClick = [this]
        {
            track().reverbOn = reverbBtn.getToggleState();
            info (track().name + " reverb", reverbBtn.getToggleState() ? juce::String ("on") : juce::String ("off"));
        };
        addAndMakeVisible (echoBtn);
        addAndMakeVisible (reverbBtn);
    }
    syncFromEngine();
}

void ChannelStrip::showEchoMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Echo timing");
    for (int d = 0; d < TrackFx::numDivs; ++d)
        m.addItem (d + 1, TrackFx::divName (d), true, d == track().echoDiv.load());
    m.showMenuAsync (juce::PopupMenu::Options(), [this] (int id)
    {
        if (id <= 0) return;
        track().echoDiv = id - 1;
        track().echoOn = true;
        echoBtn.setToggleState (true, juce::dontSendNotification);
        info (track().name + " echo", TrackFx::divName (id - 1));
    });
}

void ChannelStrip::commitMaster()
{
    const float after = engine.masterVolume.load(), before = lastMaster;
    if (after == before) return;
    lastMaster = after;
    auto set = [this] (float g)
    {
        engine.masterVolume = g; lastMaster = g;
        fader.setValue (gainToFader (g), juce::dontSendNotification);
        dbLabel.setText (dbStr ((float) fader.getValue()) + " dB", juce::dontSendNotification);
    };
    if (onUndoable) onUndoable ("Master volume", [set, after] { set (after); }, [set, before] { set (before); });
}

void ChannelStrip::syncFromEngine()
{
    if (isMaster())
    {
        lastMaster = engine.masterVolume.load();
        if (! fader.isMouseButtonDown()) fader.setValue (gainToFader (lastMaster), juce::dontSendNotification);
    }
    else
    {
        auto& t = track();
        if (! fader.isMouseButtonDown()) fader.setValue (gainToFader (t.volume.load()), juce::dontSendNotification);
        if (! pan.isMouseButtonDown()) pan.setValue (t.pan.load(), juce::dontSendNotification);
        muteBtn.setToggleState (t.mute.load(), juce::dontSendNotification);
        soloBtn.setToggleState (t.solo.load(), juce::dontSendNotification);
        echoBtn.setToggleState (t.echoOn.load(), juce::dontSendNotification);
        reverbBtn.setToggleState (t.reverbOn.load(), juce::dontSendNotification);
        if (t.name != shownName) { shownName = t.name; repaint(); }
    }
    dbLabel.setText (dbStr ((float) fader.getValue()) + " dB", juce::dontSendNotification);
}

void ChannelStrip::tick()
{
    if (isMaster()) meter.setLevels (engine.masterPeakL.exchange (0.0f), engine.masterPeakR.exchange (0.0f));
    else            meter.setLevels (track().mixPeakL.exchange (0.0f), track().mixPeakR.exchange (0.0f));
    syncFromEngine();
}

void ChannelStrip::lookAndFeelChanged()
{
    fader.setColour (juce::Slider::trackColourId, isMaster() ? Theme::purple : Theme::accent);
    dbLabel.setColour (juce::Label::textColourId, Theme::dim);
    if (! isMaster())
    {
        pan.setColour (juce::Slider::rotarySliderFillColourId, Theme::accent);
        for (auto* b : { &muteBtn, &soloBtn, (juce::TextButton*) &echoBtn, (juce::TextButton*) &reverbBtn }) styleFlatToggle (*b, Theme::accent);
    }
    repaint();
}

void ChannelStrip::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto accent = isMaster() ? Theme::purple : Theme::trackColour (slotIndex);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path card;
        card.addRoundedRectangle (b, Theme::radiusLg);
        g.reduceClipRegion (card);
        g.setColour (Theme::panel);
        g.fillRect (b);
        g.setColour (accent);
        g.fillRect (isMaster() ? b.withWidth (4.0f) : b.withHeight (3.0f));   // master: side stripe like Drums
    }
    g.setColour (isMaster() ? accent.withAlpha (0.35f) : Theme::border.withAlpha (0.55f));
    g.drawRoundedRectangle (b.reduced (0.5f), Theme::radiusLg, 1.0f);

    {   // dark well around VU + fader, with a lighter ring
        const auto well = meter.getBounds().getUnion (fader.getBounds()).toFloat().expanded (4.0f, 2.0f);
        Theme::wellRing (g, well, 2.5f);
        g.setColour (Theme::deep);
        g.fillRoundedRectangle (well, Theme::radius);
    }
    g.setColour (Theme::text);
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    const juce::String label = isMaster() ? juce::String ("MASTER") : juce::String (pos + 1) + "  " + shownName;
    g.drawText (label, 8, 6, getWidth() - 16, 18, juce::Justification::centredLeft, true);
}

void ChannelStrip::resized()
{
    const int W = getWidth(), H = getHeight();
    const int below = 14 + 4 + 34 + 4 + 48 + 8;               // db label, pan, two button rows, padding
    const int top = 28, bodyH = juce::jmax (40, H - top - below);
    const int meterW = isMaster() ? 16 : 12;
    const int faderW = 26;
    const int total = meterW + 8 + faderW;
    const int x0 = (W - total) / 2;
    meter.setBounds (x0, top, meterW, bodyH);
    fader.setBounds (x0 + meterW + 8, top, faderW, bodyH);
    dbLabel.setBounds (0, top + bodyH + 2, W, 14);
    if (isMaster()) return;
    pan.setBounds ((W - 34) / 2, top + bodyH + 18, 34, 34);
    const int bw = (W - 16 - 4) / 2;
    int y = H - 8 - 48;
    muteBtn.setBounds (8, y, bw, 22); soloBtn.setBounds (8 + bw + 4, y, bw, 22);
    y += 26;
    echoBtn.setBounds (8, y, bw, 22); reverbBtn.setBounds (8 + bw + 4, y, bw, 22);
}

// ------------------------------------------------------------------ panel

MixerPanel::MixerPanel (AudioEngine& e) : engine (e), master (e, -1, 0)
{
    addAndMakeVisible (chevron);
    chevron.onClick = [this] { setExpanded (! expanded); };
    title.setText ("MIXER", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    title.setColour (juce::Label::textColourId, Theme::dim);
    title.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (title);

    stripView.setViewedComponent (&holder, false);
    stripView.setScrollBarsShown (false, true);
    stripView.setScrollBarThickness (8);
    addAndMakeVisible (stripView);

    master.onUndoable = [this] (const juce::String& n, std::function<void()> a, std::function<void()> b) { if (onUndoable) onUndoable (n, std::move (a), std::move (b)); };
    master.onInfo = [this] (const juce::String& s) { if (onInfo) onInfo (s); };
    addAndMakeVisible (master);
}

void MixerPanel::setExpanded (bool b)
{
    expanded = b;
    chevron.open = b;
    chevron.repaint();
    stripView.setVisible (b);
    master.setVisible (b);
    if (onLayoutChanged) onLayoutChanged();
}

void MixerPanel::refreshFromEngine()
{
    shownOrder.clear();     // force a rebuild
    rebuild();
    master.syncFromEngine();
}

void MixerPanel::rebuild()
{
    if (shownOrder == engine.order) return;
    shownOrder = engine.order;
    strips.clear();
    int i = 0;
    for (int slot : shownOrder)
    {
        auto* s = strips.add (new ChannelStrip (engine, slot, i++));
        s->onChanged = [this] (int sl) { if (onTrackChanged) onTrackChanged (sl); };
        s->onInfo = [this] (const juce::String& t) { if (onInfo) onInfo (t); };
        holder.addAndMakeVisible (s);
    }
    layoutStrips();
}

void MixerPanel::layoutStrips()
{
    const int n = strips.size();
    const int avail = stripView.getWidth();
    const int sw = n > 0 ? juce::jlimit (66, 120, avail / n) : 80;
    holder.setSize (juce::jmax (avail, n * sw), juce::jmax (40, stripView.getMaximumVisibleHeight()));
    for (int i = 0; i < n; ++i)
        strips[i]->setBounds (i * sw, 0, sw - 4, holder.getHeight());
}

void MixerPanel::tick()
{
    rebuild();
    if (! expanded) return;
    for (auto* s : strips) s->tick();
    master.tick();
}

void MixerPanel::paint (juce::Graphics&) {}

void MixerPanel::lookAndFeelChanged() { title.setColour (juce::Label::textColourId, Theme::dim); }

void MixerPanel::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (headerH);
    chevron.setBounds (head.removeFromLeft (30).reduced (2));
    title.setBounds (head.removeFromLeft (100));
    r.removeFromTop (0);
    auto body = r.withHeight (stripH);
    master.setBounds (body.removeFromRight (104));
    body.removeFromRight (6);
    stripView.setBounds (body);
    layoutStrips();
}
