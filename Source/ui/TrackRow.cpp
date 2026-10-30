#include "TrackRow.h"

static juce::String dbText (float pos)
{
    const float g = faderToGain (pos);
    return g <= 0.0f ? juce::String ("-inf") : juce::String (juce::Decibels::gainToDecibels (g), 1) + " dB";
}

static void styleFader (juce::Slider& s, juce::Colour accent, bool horizontal)
{
    s.setSliderStyle (horizontal ? juce::Slider::LinearHorizontal : juce::Slider::LinearVertical);
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

void TrackRow::Grip::paint (juce::Graphics& g)
{
    g.setColour (Theme::dim);
    auto b = getLocalBounds().toFloat().withSizeKeepingCentre (8.0f, 22.0f);
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 2; ++c)
            g.fillEllipse (b.getX() + c * 6.0f, b.getY() + r * 6.0f, 2.5f, 2.5f);
}

TrackRow::TrackRow (AudioEngine& e, int slot) : engine (e), index (slot), accent (Theme::trackColour (slot))
{
    addAndMakeVisible (grip);

    nameLabel.setEditable (false, true);
    nameLabel.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    nameLabel.setColour (juce::Label::textColourId, Theme::deep);
    nameLabel.onTextChange = [this] { track().name = nameLabel.getText(); };
    nameLabel.onEditorHide = [this] { if (onReleaseFocus) onReleaseFocus(); };
    addAndMakeVisible (nameLabel);

    input.onChange = [this] { track().inputCode = input.getSelectedId() - 1; };
    input.setWantsKeyboardFocus (false);
    input.onChange = [this] { track().inputCode = input.getSelectedId() - 1; if (onReleaseFocus) onReleaseFocus(); };
    addAndMakeVisible (input);
    inMeter.setHorizontal (true);
    addAndMakeVisible (inMeter);

    styleToggle (arm, Theme::red);
    styleToggle (mute, Theme::yellow);
    styleToggle (solo, Theme::green);
    arm.onClick  = [this] { track().armed = arm.getToggleState(); };
    mute.onClick = [this] { track().mute = mute.getToggleState(); };
    solo.onClick = [this] { track().solo = solo.getToggleState(); };
    arm.setTooltip ("Arm for recording / input monitoring");
    addAndMakeVisible (arm); addAndMakeVisible (mute); addAndMakeVisible (solo);

    del.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    del.setColour (juce::TextButton::textColourOffId, Theme::dim);
    del.setWantsKeyboardFocus (false);
    del.setTooltip ("Delete track");
    del.onClick = [this] { if (onDelete) onDelete (this); };
    addAndMakeVisible (del);

    pan.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    pan.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    pan.setRange (-1.0, 1.0, 0.01);
    pan.setDoubleClickReturnValue (true, 0.0);
    pan.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    pan.textFromValueFunction = [] (double v)
    {
        const int p = (int) std::round (std::abs (v) * 100.0);
        return p == 0 ? juce::String ("Pan C") : "Pan " + juce::String (v < 0 ? "L" : "R") + juce::String (p);
    };
    pan.setPopupDisplayEnabled (true, true, nullptr);
    pan.setColour (juce::Slider::rotarySliderFillColourId, accent);
    pan.setWantsKeyboardFocus (false);
    pan.onValueChange = [this] { track().pan = (float) pan.getValue(); };
    addAndMakeVisible (pan);

    styleFader (fader, accent, true);
    fader.onValueChange = [this] { track().volume = faderToGain ((float) fader.getValue()); updateDb(); };
    addAndMakeVisible (fader);
    addAndMakeVisible (meter);

    dbLabel.setJustificationType (juce::Justification::centredRight);
    dbLabel.setColour (juce::Label::textColourId, Theme::dim);
    dbLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (dbLabel);

    for (auto* b : { &prevTake, &nextTake })
    {
        b->setWantsKeyboardFocus (false);
        b->setColour (juce::TextButton::buttonColourId, Theme::raised.withAlpha (0.9f));
        addChildComponent (b);
    }
    prevTake.onClick = [this] { takeStep (-1); };
    nextTake.onClick = [this] { takeStep (+1); };
    prevTake.setTooltip ("Previous take");
    nextTake.setTooltip ("Next take");
    takeLabel.setJustificationType (juce::Justification::centred);
    takeLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    takeLabel.setColour (juce::Label::textColourId, Theme::text);
    takeLabel.setInterceptsMouseClicks (false, false);
    addChildComponent (takeLabel);

    setInputChoices (2);
    refreshFromTrack();
}

void TrackRow::takeStep (int d)
{
    auto& t = track();
    t.applyTake (juce::jlimit (0, (int) t.takes.size() - 1, t.currentTake + d));
    if (onReleaseFocus) onReleaseFocus();
}

void TrackRow::setInputChoices (int n)
{
    const int cur = track().inputCode.load();
    input.clear (juce::dontSendNotification);
    for (int i = 0; i < n; ++i) input.addItem ("In " + juce::String (i + 1), i + 1);
    for (int i = 0; i + 1 < n; i += 2) input.addItem ("In " + juce::String (i + 1) + "+" + juce::String (i + 2), 101 + i);
    if (input.indexOfItemId (cur + 1) < 0 && n > 0) { input.setSelectedId (1, juce::dontSendNotification); track().inputCode = 0; }
    else input.setSelectedId (cur + 1, juce::dontSendNotification);
}

void TrackRow::refreshFromTrack()
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
    mn.clear(); mx.clear(); mn2.clear(); mx2.clear(); stereo = false; done = 0; rev = ~0u;
    repaint();
}

void TrackRow::updateDb()
{
    dbLabel.setText (dbText ((float) fader.getValue()), juce::dontSendNotification);
}

void TrackRow::updateCache()
{
    auto& t = track();
    juce::SpinLock::ScopedTryLockType lk (t.lock);
    if (! lk.isLocked()) return;

    const auto len = juce::jmin<juce::int64> (t.length.load(), t.data.getNumSamples());
    const auto r = t.revision.load();
    if (r != rev || len < done * binSize) { mn.clear(); mx.clear(); mn2.clear(); mx2.clear(); stereo = false; done = 0; rev = r; }

    const auto target = len / binSize;
    if (target > done)
    {
        const float* l = t.data.getReadPointer (0);
        const float* rr = t.data.getReadPointer (1);
        for (auto b = done; b < target; ++b)
        {
            auto a = juce::FloatVectorOperations::findMinAndMax (l + b * binSize, binSize);
            auto c = juce::FloatVectorOperations::findMinAndMax (rr + b * binSize, binSize);
            mn.push_back (a.getStart()); mx.push_back (a.getEnd());
            mn2.push_back (c.getStart()); mx2.push_back (c.getEnd());
            if (std::abs (a.getStart() - c.getStart()) > 1.0e-4f || std::abs (a.getEnd() - c.getEnd()) > 1.0e-4f)
                stereo = true;
        }
        done = target;
    }
}

void TrackRow::tick()
{
    auto& t = track();
    const int code = t.inputCode.load();
    const int l = code >= 100 ? code - 100 : code;
    const int r = code >= 100 ? l + 1 : l;
    auto lvl = [&] (int ch) { return ch >= 0 && ch < 16 ? engine.inputLevel[ch].load() : 0.0f; };
    inMeter.setLevels (lvl (l), lvl (r));
    meter.setLevels (t.peakL.exchange (0.0f), t.peakR.exchange (0.0f));
    updateCache();
    const int nt = (int) t.takes.size(), ct = t.currentTake;
    if (nt != shownTakes || ct != shownCurrent)
    {
        shownTakes = nt; shownCurrent = ct;
        const bool show = nt > 1;
        prevTake.setVisible (show); nextTake.setVisible (show); takeLabel.setVisible (show);
        prevTake.setEnabled (ct > 0); nextTake.setEnabled (ct < nt - 1);
        takeLabel.setText ({}, juce::dontSendNotification);
    }
    repaint (wave);
}

void TrackRow::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Theme::panel);
    g.fillRect (b);
    g.setColour (Theme::panel.brighter (0.05f));
    g.fillRect (b.withRight ((float) wave.getX() - 4.0f));            // header block
    g.setColour (accent.withAlpha (0.92f));
    g.fillRect (nameBar.toFloat());                                     // coloured name bar (Live style)
    g.setColour (accent);
    g.fillRect (b.withWidth (4.0f));                                    // track colour stripe
    g.setColour (Theme::deep);
    g.fillRect (b.withTop (b.getBottom() - 1.0f));                      // row separator

    // ---- clip lane
    auto w = wave.toFloat();
    g.setColour (Theme::deep);
    g.fillRect (w);
    if (w.isEmpty()) return;

    g.saveState();
    g.reduceClipRegion (wave);
    const double sr = engine.getSampleRate();
    const double pxPerSec = w.getWidth() / viewSeconds;

    if (barSeconds > 0.0)
    {
        const double beat = barSeconds / 4.0;
        int k = 0;
        for (double s = 0.0; s < viewSeconds - 0.001; s += beat, ++k)
        {
            const bool bar = (k % 4 == 0);
            g.setColour (bar ? Theme::border.withAlpha (0.8f) : Theme::border.withAlpha (0.25f));
            g.fillRect ((float) (w.getX() + s * pxPerSec), w.getY(), 1.0f, w.getHeight());
        }
    }
    else
    {
        g.setColour (Theme::border.withAlpha (0.3f));
        for (double s = 5.0; s < viewSeconds; s += 5.0)
            g.fillRect ((float) (w.getX() + s * pxPerSec), w.getY(), 1.0f, w.getHeight());
    }

    auto& tr = track();
    const bool armed = tr.armed.load();
    const bool recording = tr.recActive.load() && engine.getState() == AudioEngine::Recording;
    const auto len = tr.length.load();

    // clip rectangle: recorded take, or the in-progress loop pass
    double clipSecs = len / juce::jmax (1.0, sr);
    bool live = false;
    if (recording && tr.recToStage.load()) { clipSecs = tr.stagePos.load() / juce::jmax (1.0, sr); live = true; }
    else if (recording) live = true;
    const float clipW = juce::jmin (w.getWidth(), (float) (clipSecs * pxPerSec));

    if (clipW > 1.0f && (done > 0 || live))
    {
        auto clip = w.withWidth (clipW);
        const auto col = live ? Theme::red : accent;
        auto strip = clip.removeFromTop ((float) clipStripH);
        g.setColour (col);
        g.fillRect (strip);
        g.setColour (Theme::deep);
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        juce::String title = live ? juce::String ("Recording") : tr.name;
        if (! live && tr.takes.size() > 1) title << "   take " << (tr.currentTake + 1) << "/" << (int) tr.takes.size();
        g.drawText (title, strip.reduced (5.0f, 0.0f), juce::Justification::centredLeft);

        g.setColour (col.withMultipliedBrightness (0.30f).withMultipliedSaturation (0.8f));
        g.fillRect (clip);

        if (done > 0 && ! (live && tr.recToStage.load()))
        {
            g.setColour (col.brighter (0.35f));
            const double spp = sr * viewSeconds / w.getWidth();     // samples per pixel
            auto drawLane = [&] (const std::vector<float>& lo, const std::vector<float>& hi, juce::Rectangle<float> area)
            {
                const float mid = area.getCentreY(), half = area.getHeight() * 0.5f - 2.0f;
                for (int x = 0; x < (int) area.getWidth(); ++x)
                {
                    const auto b0 = (juce::int64) (x * spp / binSize);
                    if (b0 >= done) break;
                    const auto b1 = juce::jmin (done, juce::jmax (b0 + 1, (juce::int64) ((x + 1) * spp / binSize)));
                    float mnv = 0.0f, mxv = 0.0f;
                    for (auto k = b0; k < b1; ++k) { mnv = juce::jmin (mnv, lo[(size_t) k]); mxv = juce::jmax (mxv, hi[(size_t) k]); }
                    const float y1 = mid - juce::jlimit (0.0f, 1.0f, mxv) * half;
                    const float y2 = mid - juce::jlimit (-1.0f, 0.0f, mnv) * half;
                    g.fillRect ((float) (area.getX() + x), y1, 1.0f, juce::jmax (1.0f, y2 - y1));
                }
            };
            if (stereo)
            {
                const float h2 = clip.getHeight() * 0.5f;
                drawLane (mn, mx, clip.withHeight (h2));
                drawLane (mn2, mx2, clip.withTrimmedTop (h2));
                g.setColour (Theme::deep.withAlpha (0.6f));
                g.fillRect (clip.getX(), clip.getY() + h2, clip.getWidth(), 1.0f);
            }
            else
                drawLane (mn, mx, clip);
        }
    }
    else
    {
        g.setColour (Theme::dim);
        g.setFont (12.0f);
        g.drawText (armed ? "Armed - press R to record" : "Empty", wave, juce::Justification::centred);
    }

    const float px = (float) (w.getX() + engine.getPosition() / juce::jmax (1.0, sr) * pxPerSec);
    if (engine.getState() != AudioEngine::Stopped || engine.getPosition() > 0)
    {
        g.setColour (Theme::text);
        g.fillRect (px, w.getY(), 1.5f, w.getHeight());
    }
    g.restoreState();
}
void TrackRow::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromLeft (4);
    grip.setBounds (r.removeFromLeft (16));
    r.removeFromLeft (6);
    auto header = r.removeFromLeft (336);
    r.removeFromLeft (8);
    meter.setBounds (r.removeFromRight (14));
    r.removeFromRight (6);
    wave = r;
    prevTake.setBounds (wave.getRight() - 62, wave.getY(), 18, clipStripH);
    nextTake.setBounds (wave.getRight() - 20, wave.getY(), 18, clipStripH);
    takeLabel.setBounds (wave.getRight() - 44, wave.getY(), 24, clipStripH);

    auto l1 = header.removeFromTop (26);
    del.setBounds (l1.removeFromRight (24));
    l1.removeFromRight (4);
    inMeter.setBounds (l1.removeFromRight (40).withSizeKeepingCentre (40, 12));
    l1.removeFromRight (4);
    input.setBounds (l1.removeFromRight (96));
    l1.removeFromRight (6);
    nameLabel.setBounds (l1.reduced (4, 0));
    nameBar = l1;

    header.removeFromTop (8);
    auto l2 = header;
    const int bw = 30;
    arm.setBounds (l2.removeFromLeft (bw).withSizeKeepingCentre (bw, 30)); l2.removeFromLeft (4);
    mute.setBounds (l2.removeFromLeft (bw).withSizeKeepingCentre (bw, 30)); l2.removeFromLeft (4);
    solo.setBounds (l2.removeFromLeft (bw).withSizeKeepingCentre (bw, 30)); l2.removeFromLeft (8);
    pan.setBounds (l2.removeFromLeft (44));
    l2.removeFromLeft (6);
    dbLabel.setBounds (l2.removeFromRight (56));
    fader.setBounds (l2);
}

// ---------------------------------------------------------------- master

MasterStrip::MasterStrip (AudioEngine& e) : engine (e)
{
    title.setText ("MASTER", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    addAndMakeVisible (title);

    styleFader (fader, Theme::purple, true);
    fader.onValueChange = [this]
    {
        engine.masterVolume = faderToGain ((float) fader.getValue());
        dbLabel.setText (dbText ((float) fader.getValue()), juce::dontSendNotification);
    };
    addAndMakeVisible (fader);
    meter.setHorizontal (true);
    addAndMakeVisible (meter);

    dbLabel.setJustificationType (juce::Justification::centredRight);
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
    g.setColour (Theme::panel.brighter (0.05f));
    g.fillRect (b);
    g.setColour (Theme::purple);
    g.fillRect (b.withWidth (4.0f));
    g.setColour (Theme::deep);
    g.drawRect (b, 1.0f);
}

void MasterStrip::resized()
{
    auto r = getLocalBounds().reduced (14, 10);
    r.removeFromLeft (4);
    title.setBounds (r.removeFromLeft (80));
    fader.setBounds (r.removeFromLeft (260));
    dbLabel.setBounds (r.removeFromLeft (60));
    r.removeFromLeft (16);
    meter.setBounds (r.withSizeKeepingCentre (r.getWidth(), 20));
}

// ---------------------------------------------------------------- input bar

InputBar::InputBar (AudioEngine& e) : engine (e)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Click an input to assign it to the armed tracks");
    setNumInputs (2);
}

void InputBar::setNumInputs (int n)
{
    n = juce::jlimit (0, 16, n);
    if (n == meters.size()) return;
    meters.clear();
    for (int i = 0; i < n; ++i)
    {
        auto* m = new LevelMeter();
        m->setHorizontal (true);
        m->setBars (1);
        m->setInterceptsMouseClicks (false, false);
        meters.add (m);
        addAndMakeVisible (m);
    }
    resized();
    repaint();
}

juce::Rectangle<int> InputBar::cell (int i) const
{
    auto r = getLocalBounds().reduced (12, 6);
    r.removeFromLeft (70);
    const int n = juce::jmax (1, meters.size());
    const int w = juce::jmin (140, r.getWidth() / n);
    return { r.getX() + i * w, r.getY(), w - 8, r.getHeight() };
}

void InputBar::resized()
{
    for (int i = 0; i < meters.size(); ++i)
    {
        auto c = cell (i);
        c.removeFromLeft (22);
        meters[i]->setBounds (c.withSizeKeepingCentre (c.getWidth(), 10));
    }
}

void InputBar::tick()
{
    for (int i = 0; i < meters.size(); ++i)
    {
        const float v = engine.inputLevel[i].load();
        meters[i]->setLevels (v, v);
    }
}

void InputBar::paint (juce::Graphics& g)
{
    g.setColour (Theme::panel.brighter (0.05f));
    g.fillRect (getLocalBounds().toFloat());
    g.setColour (Theme::deep);
    g.drawRect (getLocalBounds().toFloat(), 1.0f);
    g.setColour (Theme::dim);
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    g.drawText ("INPUTS", getLocalBounds().reduced (14, 0).withWidth (60), juce::Justification::centredLeft);
    g.setFont (13.0f);
    g.setColour (Theme::text);
    for (int i = 0; i < meters.size(); ++i)
        g.drawText (juce::String (i + 1), cell (i).withWidth (18), juce::Justification::centredLeft);
    if (meters.isEmpty())
        g.drawText ("No input channels active - open Audio settings", getLocalBounds().withTrimmedLeft (80), juce::Justification::centredLeft);
}

void InputBar::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < meters.size(); ++i)
        if (cell (i).expanded (4, 4).contains (e.getPosition()) && onPick) { onPick (i); return; }
}

// ---------------------------------------------------------------- list / drag reorder

int TrackList::targetSlot (int y) const
{
    const int n = engine.getNumTracks();
    return juce::jlimit (0, juce::jmax (0, n - 1), (int) std::round (y / (double) (rowHeight + gap)));
}

void TrackList::layoutRows()
{
    for (auto* r : rows) if (r != dragging) r->setVisible (false);
    const int n = (int) engine.order.size();
    const int target = dragging != nullptr ? targetSlot (dragY) : -1;
    int p = 0;
    for (int slot : engine.order)
    {
        auto* r = rows[slot];
        if (r == dragging) continue;
        const int pos = (dragging != nullptr && p >= target) ? p + 1 : p;
        r->setBounds (0, pos * (rowHeight + gap), getWidth(), rowHeight);
        r->setVisible (true);
        ++p;
    }
    if (dragging != nullptr)
    {
        dragging->setBounds (0, dragY, getWidth(), rowHeight);
        dragging->setVisible (true);
        dragging->toFront (false);
    }
    juce::ignoreUnused (n);
}

void TrackList::dragStart (TrackRow* r)
{
    dragging = r;
    dragStartY = dragY = r->getY();
}

void TrackList::dragMove (TrackRow*, int dy)
{
    if (dragging == nullptr) return;
    dragY = juce::jlimit (0, juce::jmax (0, contentHeight() - rowHeight), dragStartY + dy);
    layoutRows();
}

void TrackList::dragEnd (TrackRow* r)
{
    if (dragging == nullptr) return;
    const int t = targetSlot (dragY);
    engine.order.erase (std::remove (engine.order.begin(), engine.order.end(), r->slot()), engine.order.end());
    engine.order.insert (engine.order.begin() + juce::jmin (t, (int) engine.order.size()), r->slot());
    dragging = nullptr;
    layoutRows();
}

// ---------------------------------------------------------------- ruler

void Ruler::paint (juce::Graphics& g)
{
    g.setColour (Theme::panel.brighter (0.05f));
    g.fillRect (getLocalBounds());
    g.setColour (Theme::deep);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    const float x0 = (float) TrackRow::waveLeft;
    const float wdt = (float) getWidth() - x0 - (float) TrackRow::waveRightInset;
    if (wdt <= 0.0f) return;
    const double pxPerSec = wdt / viewSeconds;

    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    if (barSeconds > 0.0)
    {
        const double beat = barSeconds / 4.0;
        int k = 0;
        for (double s = 0.0; s < viewSeconds - 0.001; s += beat, ++k)
        {
            const float x = x0 + (float) (s * pxPerSec);
            const bool bar = (k % 4 == 0);
            g.setColour (bar ? Theme::text.withAlpha (0.8f) : Theme::dim);
            g.fillRect (x, bar ? 4.0f : (float) getHeight() - 7.0f, 1.0f, bar ? (float) getHeight() - 5.0f : 6.0f);
            if (bar)
            {
                g.setColour (Theme::text);
                g.drawText (juce::String (k / 4 + 1), (int) x + 4, 2, 40, 14, juce::Justification::centredLeft);
            }
        }
        if (engine.loopEnabled.load())
        {
            g.setColour (Theme::teal);
            g.fillRect (x0, 0.0f, wdt, 3.0f);   // loop region
        }
    }
    else
    {
        for (double s = 0.0; s < viewSeconds; s += 5.0)
        {
            const float x = x0 + (float) (s * pxPerSec);
            g.setColour (Theme::dim);
            g.fillRect (x, 4.0f, 1.0f, (float) getHeight() - 5.0f);
            g.setColour (Theme::text);
            g.drawText (juce::String::formatted ("%d:%02d", (int) s / 60, (int) s % 60), (int) x + 4, 2, 50, 14, juce::Justification::centredLeft);
        }
    }

    if (engine.getState() != AudioEngine::Stopped || engine.getPosition() > 0)
    {
        const float px = x0 + (float) (engine.getPosition() / juce::jmax (1.0, engine.getSampleRate()) * pxPerSec);
        juce::Path tri;
        tri.addTriangle (px - 5.0f, (float) getHeight() - 9.0f, px + 6.0f, (float) getHeight() - 9.0f, px + 0.5f, (float) getHeight() - 1.0f);
        g.setColour (Theme::text);
        g.fillPath (tri);
    }
}