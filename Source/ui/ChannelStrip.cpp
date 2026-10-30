#include "ChannelStrip.h"

static juce::String dbText (float pos)
{
    const float g = faderToGain (pos);
    return g <= 0.0f ? juce::String ("-inf") : juce::String (juce::Decibels::gainToDecibels (g), 1) + " dB";
}

static void styleFader (juce::Slider& s, juce::Colour accent)
{
    s.setSliderStyle (juce::Slider::LinearVertical);
    s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    s.setRange (0.0, 1.0, 0.0);
    s.setDoubleClickReturnValue (true, gainToFader (1.0f));
    s.setColour (juce::Slider::trackColourId, accent);
    s.setWantsKeyboardFocus (false);
}

static void styleToggle (juce::TextButton& b, juce::Colour on)
{
    b.setClickingTogglesState (true);
    b.setColour (juce::TextButton::buttonOnColourId, on);
    b.setColour (juce::TextButton::textColourOnId, Theme::deep);
    b.setWantsKeyboardFocus (false);
}

ChannelStrip::ChannelStrip (AudioEngine& e, int idx)
    : engine (e), index (idx), accent (Theme::trackColour (idx))
{
    nameLabel.setEditable (false, true);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    nameLabel.onTextChange = [this] { track().name = nameLabel.getText(); };
    addAndMakeVisible (nameLabel);

    input.onChange = [this] { track().inputCode = input.getSelectedId() - 1; };
    addAndMakeVisible (input);

    styleToggle (arm, Theme::red);
    styleToggle (mute, Theme::yellow);
    styleToggle (solo, Theme::green);
    arm.onClick  = [this] { track().armed = arm.getToggleState(); };
    mute.onClick = [this] { track().mute = mute.getToggleState(); };
    solo.onClick = [this] { track().solo = solo.getToggleState(); };
    arm.setTooltip ("Arm for recording / input monitoring");
    addAndMakeVisible (arm); addAndMakeVisible (mute); addAndMakeVisible (solo);

    clear.setColour (juce::TextButton::buttonColourId, Theme::panel);
    clear.setColour (juce::TextButton::textColourOffId, Theme::dim);
    clear.setWantsKeyboardFocus (false);
    clear.onClick = [this] { track().clearAudio(); };
    addAndMakeVisible (clear);

    pan.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    pan.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    pan.setRange (-1.0, 1.0, 0.01);
    pan.setDoubleClickReturnValue (true, 0.0);
    pan.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 60, 16);
    pan.textFromValueFunction = [] (double v)
    {
        const int p = (int) std::round (std::abs (v) * 100.0);
        return p == 0 ? juce::String ("C") : (v < 0 ? "L" : "R") + juce::String (p);
    };
    pan.setColour (juce::Slider::rotarySliderFillColourId, accent);
    pan.setWantsKeyboardFocus (false);
    pan.onValueChange = [this] { track().pan = (float) pan.getValue(); };
    addAndMakeVisible (pan);

    styleFader (fader, accent);
    fader.onValueChange = [this]
    {
        track().volume = faderToGain ((float) fader.getValue());
        updateDb();
    };
    addAndMakeVisible (fader);
    addAndMakeVisible (meter);

    dbLabel.setJustificationType (juce::Justification::centred);
    dbLabel.setColour (juce::Label::textColourId, Theme::dim);
    dbLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (dbLabel);

    setInputChoices (2);
    refreshFromTrack();
}

void ChannelStrip::setInputChoices (int n)
{
    const int cur = track().inputCode.load();
    input.clear (juce::dontSendNotification);
    for (int i = 0; i < n; ++i) input.addItem ("In " + juce::String (i + 1), i + 1);
    for (int i = 0; i + 1 < n; i += 2) input.addItem ("In " + juce::String (i + 1) + "+" + juce::String (i + 2) + " (stereo)", 101 + i);
    if (input.indexOfItemId (cur + 1) < 0 && n > 0) { input.setSelectedId (1, juce::dontSendNotification); track().inputCode = 0; }
    else input.setSelectedId (cur + 1, juce::dontSendNotification);
}

void ChannelStrip::refreshFromTrack()
{
    auto& t = track();
    nameLabel.setText (t.name, juce::dontSendNotification);
    arm.setToggleState (t.armed.load(), juce::dontSendNotification);
    mute.setToggleState (t.mute.load(), juce::dontSendNotification);
    solo.setToggleState (t.solo.load(), juce::dontSendNotification);
    pan.setValue (t.pan.load(), juce::dontSendNotification);
    fader.setValue (gainToFader (t.volume.load()), juce::dontSendNotification);
    if (input.indexOfItemId (t.inputCode.load() + 1) >= 0)
        input.setSelectedId (t.inputCode.load() + 1, juce::dontSendNotification);
    updateDb();
    repaint();
}

void ChannelStrip::updateDb() { dbLabel.setText (dbText ((float) fader.getValue()), juce::dontSendNotification); }

void ChannelStrip::tick()
{
    auto& t = track();
    meter.setLevels (t.peakL.exchange (0.0f), t.peakR.exchange (0.0f));
    if (t.length.load() != lastLen) { lastLen = t.length.load(); repaint(); }
}

void ChannelStrip::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (b, 12.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (b.withHeight (4.0f).reduced (14.0f, 0.0f), 2.0f);
    g.setColour (Theme::border.withAlpha (0.5f));
    g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, 1.0f);

    // length indicator
    const auto len = engine.tracks[index].length.load();
    g.setColour (len > 0 ? accent.withAlpha (0.8f) : Theme::dim.withAlpha (0.5f));
    g.setFont (11.0f);
    const double secs = len / juce::jmax (1.0, engine.getSampleRate());
    g.drawText (len > 0 ? juce::String (secs, 1) + " s" : "empty",
                getLocalBounds().removeFromBottom (30).removeFromTop (20), juce::Justification::centred);
}

void ChannelStrip::resized()
{
    auto r = getLocalBounds().reduced (10);
    r.removeFromTop (8);
    nameLabel.setBounds (r.removeFromTop (24));
    r.removeFromTop (4);
    input.setBounds (r.removeFromTop (26));
    r.removeFromTop (8);

    auto row = r.removeFromTop (30);
    const int bw = (row.getWidth() - 12) / 3;
    arm.setBounds (row.removeFromLeft (bw)); row.removeFromLeft (6);
    mute.setBounds (row.removeFromLeft (bw)); row.removeFromLeft (6);
    solo.setBounds (row);
    r.removeFromTop (8);

    pan.setBounds (r.removeFromTop (74));
    r.removeFromTop (4);

    auto bottom = r.removeFromBottom (44);   // clear button + length text (painted)
    clear.setBounds (bottom.removeFromTop (22).reduced (14, 0));
    dbLabel.setBounds (r.removeFromBottom (18));

    auto fa = r.reduced (0, 4);
    meter.setBounds (fa.removeFromRight (18));
    fa.removeFromRight (6);
    fader.setBounds (fa);
}

// ---------------------------------------------------------------- master

MasterStrip::MasterStrip (AudioEngine& e) : engine (e)
{
    title.setText ("MASTER", juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    addAndMakeVisible (title);

    styleFader (fader, Theme::purple);
    fader.onValueChange = [this]
    {
        engine.masterVolume = faderToGain ((float) fader.getValue());
        dbLabel.setText (dbText ((float) fader.getValue()), juce::dontSendNotification);
    };
    addAndMakeVisible (fader);
    addAndMakeVisible (meter);

    dbLabel.setJustificationType (juce::Justification::centred);
    dbLabel.setColour (juce::Label::textColourId, Theme::dim);
    dbLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (dbLabel);
    refreshFromEngine();
}

void MasterStrip::refreshFromEngine()
{
    fader.setValue (gainToFader (engine.masterVolume.load()), juce::dontSendNotification);
    dbLabel.setText (dbText ((float) fader.getValue()), juce::dontSendNotification);
}

void MasterStrip::tick() { meter.setLevels (engine.masterPeakL.exchange (0.0f), engine.masterPeakR.exchange (0.0f)); }

void MasterStrip::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (b, 12.0f);
    g.setColour (Theme::purple);
    g.fillRoundedRectangle (b.withHeight (4.0f).reduced (14.0f, 0.0f), 2.0f);
    g.setColour (Theme::border.withAlpha (0.5f));
    g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, 1.0f);
}

void MasterStrip::resized()
{
    auto r = getLocalBounds().reduced (10);
    r.removeFromTop (8);
    title.setBounds (r.removeFromTop (24));
    r.removeFromTop (8);
    dbLabel.setBounds (r.removeFromBottom (18));
    auto fa = r.reduced (0, 4);
    meter.setBounds (fa.removeFromRight (24));
    fa.removeFromRight (8);
    fader.setBounds (fa);
}
