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
    dbLabel.setColour (juce::Label::textColourId, Theme::text);
    dbLabel.setFont (juce::Font (juce::FontOptions (11.5f)));
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

        muteBtn.number = pos + 1;
        muteBtn.colour = Theme::trackColour (slotIndex);
        styleFlatToggle (soloBtn, Theme::accent);
        muteBtn.setTooltip ("Track on / off (mute)");
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
    dbLabel.setColour (juce::Label::textColourId, Theme::text);
    if (! isMaster())
    {
        pan.setColour (juce::Slider::rotarySliderFillColourId, Theme::accent);
        muteBtn.colour = Theme::trackColour (slotIndex);
        for (auto* b : { &soloBtn, (juce::TextButton*) &echoBtn, (juce::TextButton*) &reverbBtn }) styleFlatToggle (*b, Theme::accent);
    }
    repaint();
}

void ChannelStrip::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto accent = isMaster() ? Theme::purple : Theme::trackColour (slotIndex);
    const auto onHeader = accent.getPerceivedBrightness() > 0.6f ? juce::Colours::black.withAlpha (0.8f) : juce::Colours::white;
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path card;
        card.addRoundedRectangle (b, Theme::radiusLg);
        g.reduceClipRegion (card);
        g.setColour (Theme::panel);
        g.fillRect (b);
        g.setColour (accent);                                           // coloured title block, as in Live
        g.fillRect (b.withHeight (22.0f));
    }
    g.setColour (Theme::border.withAlpha (0.55f));
    g.drawRoundedRectangle (b.reduced (0.5f), Theme::radiusLg, 1.0f);

    {   // one dark well holds VU, dB scale and fader; the read-out sits right under it, same width
        const auto well = wellArea.toFloat();
        Theme::wellRing (g, well, 2.5f);
        g.setColour (Theme::deep);
        g.fillRoundedRectangle (well, Theme::radius);
        if (! scaleArea.isEmpty())
        {
            g.setFont (juce::Font (juce::FontOptions (9.5f)));
            const auto mb = meter.getBounds().toFloat();
            for (int db = 0; db >= -60; db -= 12)
            {
                const float y = mb.getBottom() - mb.getHeight() * (float) (db + 60) / 66.0f;     // same mapping as the VU and the fader
                g.setColour (Theme::dim.withAlpha (0.55f));
                g.fillRect ((float) scaleArea.getX(), y - 0.5f, 3.0f, 1.0f);
                g.fillRect ((float) scaleArea.getRight() - 3.0f, y - 0.5f, 3.0f, 1.0f);
                g.setColour (Theme::dim);
                g.drawText (juce::String (-db), scaleArea.getX() + 2, (int) y - 6, scaleArea.getWidth() - 4, 12, juce::Justification::centred);
            }
        }
        const auto db = dbLabel.getBounds().toFloat();
        Theme::framedTile (g, db, Theme::deep, 1.5f);
    }    g.setColour (onHeader);
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    g.drawText (isMaster() ? juce::String ("Master") : shownName, 8, 2, getWidth() - 16, 18, juce::Justification::centredLeft, true);
}

void ChannelStrip::resized()
{
    const int W = getWidth(), H = getHeight();
    const int top = 30;
    const bool scale = W >= 84;
    const int meterW = isMaster() ? 16 : 12, faderW = 24, scaleW = 20;
    const int wellW = 5 + meterW + (scale ? 3 + scaleW + 3 : 8) + faderW + 5;
    const int wellX = (W - wellW) / 2;
    int yDb;
    if (isMaster()) yDb = H - 8 - 20;
    else
    {
        const int yEcho = H - 8 - 22, yBtn = yEcho - 4 - 26, yPan = yBtn - 6 - 34;
        yDb = yPan - 4 - 20;
        pan.setBounds ((W - 34) / 2, yPan, 34, 34);
        const int bw = (W - 16 - 4) / 2;
        muteBtn.setBounds (8, yBtn, bw, 26); soloBtn.setBounds (8 + bw + 4, yBtn, bw, 26);
        echoBtn.setBounds (8, yEcho, bw, 22); reverbBtn.setBounds (8 + bw + 4, yEcho, bw, 22);
    }
    const int bodyH = juce::jmax (50, yDb - 5 - top);
    wellArea = { wellX, top, wellW, bodyH };
    meter.setBounds (wellX + 5, top + 7, meterW, bodyH - 14);                 // inset so its scale matches the fader travel
    scaleArea = scale ? juce::Rectangle<int> (wellX + 5 + meterW + 3, top, scaleW, bodyH) : juce::Rectangle<int>();
    fader.setBounds (wellX + wellW - 5 - faderW, top, faderW, bodyH);
    dbLabel.setBounds (wellX, yDb, wellW, 20);
}
// ------------------------------------------------------------------ panel

MixerPanel::MixerPanel (AudioEngine& e) : engine (e), master (e, -1, 0), tuner (e)
{
    master.onUndoable = [this] (const juce::String& n, std::function<void()> a, std::function<void()> b) { if (onUndoable) onUndoable (n, std::move (a), std::move (b)); };
    master.onInfo = [this] (const juce::String& s) { if (onInfo) onInfo (s); };
    addAndMakeVisible (master);
    addAndMakeVisible (tuner);
    tuner.setRunning (true);
}

void MixerPanel::tick()
{
    master.tick();
    tuner.tick();
}

void MixerPanel::paint (juce::Graphics& g)
{
    const auto accent = Theme::purple;
    const auto b = getLocalBounds().toFloat();
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path card;
        card.addRoundedRectangle (b, Theme::radiusLg);
        g.reduceClipRegion (card);
        g.setColour (Theme::panel);
        g.fillRect (b);
        {   // the master strip casts a shadow to the sides and below
            juce::Graphics::ScopedSaveState s2 (g);
            const auto r = master.getBounds();
            g.reduceClipRegion (r.expanded (Theme::shadowRadius, 0).withTrimmedBottom (-Theme::shadowRadius).withTop (r.getY()));
            Theme::cardShadow (g, r.toFloat());
        }
        g.setColour (accent);
        g.fillRect (b.withWidth (4.0f));
    }
    g.setColour (accent.withAlpha (0.22f));
    g.drawRoundedRectangle (b.reduced (0.5f), Theme::radiusLg, 1.0f);
}

void MixerPanel::resized()
{
    auto r = getLocalBounds().reduced (pad, pad);
    r.removeFromLeft (8);
    master.setBounds (r.removeFromRight (104));
    r.removeFromRight (8);
    tuner.setBounds (r);
}