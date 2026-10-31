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
    b.setColour (juce::TextButton::textColourOffId, on);
    b.getProperties().set ("outline", (int) on.withAlpha (0.55f).getARGB());
    b.setColour (juce::TextButton::buttonColourId, Theme::deep);              // recessed when off, filled with its colour when on
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
    nameLabel.onTextChange = [this]
    {
        const auto before = lastName, after = nameLabel.getText();
        track().name = after; lastName = after;
        if (before != after)
            push ("Rename track",
                  [this, after] { track().name = after; lastName = after; nameLabel.setText (after, juce::dontSendNotification); },
                  [this, before] { track().name = before; lastName = before; nameLabel.setText (before, juce::dontSendNotification); });
    };
    nameLabel.onEditorHide = [this] { if (onReleaseFocus) onReleaseFocus(); };
    addAndMakeVisible (nameLabel);

    input.onChange = [this] { track().inputCode = input.getSelectedId() - 1; };
    input.setWantsKeyboardFocus (false);
    input.setColour (juce::ComboBox::backgroundColourId, Theme::deep);               // recessed, with an accent outline for contrast
    input.setColour (juce::ComboBox::outlineColourId, accent.withAlpha (0.55f));
    input.setColour (juce::ComboBox::arrowColourId, accent);
    input.setColour (juce::ComboBox::textColourId, Theme::text);
    input.onChange = [this] { track().inputCode = input.getSelectedId() - 1; if (onReleaseFocus) onReleaseFocus(); };
    addAndMakeVisible (input);
    inMeter.setHorizontal (true);
    addAndMakeVisible (inMeter);

    styleToggle (arm, Theme::red);
    styleToggle (mute, Theme::yellow);
    styleToggle (solo, Theme::green);
    arm.onClick  = [this] { track().armed = arm.getToggleState(); repaint(); if (onArmChanged) onArmChanged(); };
    mute.onClick = [this]
    {
        const bool now = mute.getToggleState();
        track().mute = now;
        push ("Mute", [this, now] { track().mute = now; mute.setToggleState (now, juce::dontSendNotification); },
                      [this, now] { track().mute = ! now; mute.setToggleState (! now, juce::dontSendNotification); });
    };
    solo.onClick = [this]
    {
        const bool now = solo.getToggleState();
        track().solo = now;
        push ("Solo", [this, now] { track().solo = now; solo.setToggleState (now, juce::dontSendNotification); },
                      [this, now] { track().solo = ! now; solo.setToggleState (! now, juce::dontSendNotification); });
    };
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
    pan.onDragStart = [this] { draggingPan = true; };
    pan.onDragEnd = [this] { draggingPan = false; commitPan(); };
    pan.onValueChange = [this] { track().pan = (float) pan.getValue(); if (! draggingPan) commitPan(); };
    addAndMakeVisible (pan);

    styleFader (fader, accent, true);
    fader.onDragStart = [this] { draggingFader = true; };
    fader.onDragEnd = [this] { draggingFader = false; commitVol(); };
    fader.onValueChange = [this] { track().volume = faderToGain ((float) fader.getValue()); updateDb(); if (! draggingFader) commitVol(); };
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
    hitsBtn.setWantsKeyboardFocus (false);
    hitsBtn.setColour (juce::TextButton::buttonColourId, Theme::deep.withAlpha (0.7f));
    hitsBtn.setColour (juce::TextButton::buttonOnColourId, Theme::yellow);
    hitsBtn.setColour (juce::TextButton::textColourOnId, Theme::deep);
    hitsBtn.setTooltip ("Edit individual hits: drag markers, quantize");
    hitsBtn.onClick = [this] { showHitsMenu(); };
    addChildComponent (hitsBtn);
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

void TrackRow::commitVol()
{
    const float after = track().volume.load(), before = lastVol;
    if (after == before) return;
    lastVol = after;
    push ("Volume",
          [this, after] { track().volume = after; lastVol = after; fader.setValue (gainToFader (after), juce::dontSendNotification); updateDb(); },
          [this, before] { track().volume = before; lastVol = before; fader.setValue (gainToFader (before), juce::dontSendNotification); updateDb(); });
}

void TrackRow::commitPan()
{
    const float after = track().pan.load(), before = lastPan;
    if (after == before) return;
    lastPan = after;
    push ("Pan",
          [this, after] { track().pan = after; lastPan = after; pan.setValue (after, juce::dontSendNotification); },
          [this, before] { track().pan = before; lastPan = before; pan.setValue (before, juce::dontSendNotification); });
}

void TrackRow::pushHitEdit (const juce::String& name, const std::vector<Hit>& before)
{
    auto& t = track();
    const auto after = t.hits;
    bool same = before.size() == after.size();
    for (size_t i = 0; same && i < after.size(); ++i) same = before[i].src == after[i].src && before[i].dst == after[i].dst;
    if (same) return;
    const auto gen = t.editGen;
    auto apply = [this, gen] (const std::vector<Hit>& hs)
    {
        auto& tr = track();
        if (tr.editGen != gen || tr.editSource.getNumSamples() == 0) return;   // take changed since: nothing to restore
        tr.hits = hs;
        tr.applyEdit (engine.getSampleRate());
    };
    push (name, [apply, after] { apply (after); }, [apply, before] { apply (before); });
}

void TrackRow::takeStep (int d)
{
    auto& t = track();
    const int from = t.currentTake, to = juce::jlimit (0, (int) t.takes.size() - 1, t.currentTake + d);
    if (from != to)
        push ("Select take",
              [this, to] { track().applyTake (to); },
              [this, from] { track().applyTake (from); });
    t.applyTake (to);
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
    lastName = t.name; lastVol = t.volume.load(); lastPan = t.pan.load();
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
    {
        const int glowState = (t.armed.load() ? 1 : 0) + ((t.recActive.load() && engine.getState() == AudioEngine::Recording) ? 2 : 0);
        if (glowState != lastGlowState) { lastGlowState = glowState; repaint(); }
    }
    const int nt = (int) t.takes.size(), ct = t.currentTake;
    if (nt != shownTakes || ct != shownCurrent)
    {
        shownTakes = nt; shownCurrent = ct;
        const bool show = nt > 1;
        prevTake.setVisible (show); nextTake.setVisible (show); takeLabel.setVisible (show);
        prevTake.setEnabled (ct > 0); nextTake.setEnabled (ct < nt - 1);
        takeLabel.setText ({}, juce::dontSendNotification);
    }
    const bool canEdit = t.length.load() > 0 && ! (t.recActive.load() && engine.getState() == AudioEngine::Recording);
    if (hitsBtn.isVisible() != canEdit) hitsBtn.setVisible (canEdit);
    hitsBtn.setToggleState (t.editing, juce::dontSendNotification);
    repaint (wave);
}

void TrackRow::paint (juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState cardState (g);
    auto b = getLocalBounds().toFloat().reduced (0.0f, 4.0f);          // flush with the panels left/right
    const bool armedNow = track().armed.load();
    const bool recNow = track().recActive.load() && engine.getState() == AudioEngine::Recording;
    const auto glow = (recNow || armedNow) ? Theme::red : accent;      // armed tracks turn red
    juce::Path card;
    card.addRoundedRectangle (b, Theme::radiusLg);
    g.reduceClipRegion (card);                                          // everything below is clipped to a rounded card
    g.setColour (Theme::panel);
    g.fillRect (b);
    g.setColour (Theme::panel.brighter (0.05f));
    g.fillRect (b.withRight ((float) meter.getX() - 3.0f));            // header block
    g.setColour (accent.withAlpha (0.92f));
    g.fillRect (nameBar.toFloat());                                     // coloured name bar (Live style)
    g.setColour (accent);
    g.fillRect (b.withWidth (4.0f));                                    // track colour stripe
    {   // line along the top edge: strongest in the middle, fading out to both sides
        juce::ColourGradient line (glow.withAlpha (0.0f), b.getX(), b.getY(), glow.withAlpha (0.0f), b.getRight(), b.getY(), false);
        line.addColour (0.5, glow.withAlpha (0.9f));
        g.setGradientFill (line);
        g.fillRect (b.getX(), b.getY(), b.getWidth(), 1.0f);
    }
    // ---- clip lane
    auto w = wave.toFloat();
    g.setColour (Theme::deep);
    g.fillRoundedRectangle (w, Theme::radius);
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
        const float nudgePx = live ? 0.0f : (float) (tr.nudge.load() / juce::jmax (1.0, sr) * pxPerSec);
        auto drawClip = [&] (float dx)
        {
        auto clip = w.withWidth (clipW).translated (dx, 0.0f);
        const auto col = live ? Theme::red : accent;
        g.setColour (col.withMultipliedBrightness (0.92f));
        g.fillRoundedRectangle (clip, Theme::radius - 1.0f);                        // solid clip body
        auto strip = clip.removeFromTop ((float) clipStripH);
        juce::Path sp;
        sp.addRoundedRectangle (strip.getX(), strip.getY(), strip.getWidth(), strip.getHeight(), Theme::radius - 1.0f, Theme::radius - 1.0f, true, true, false, false);
        g.setColour (col);
        g.fillPath (sp);
        g.setColour (Theme::deep);
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        juce::String title = live ? juce::String ("Recording") : tr.name;
        if (! live && tr.takes.size() > 1) title << "   take " << (tr.currentTake + 1) << "/" << (int) tr.takes.size();
        if (! live && tr.nudge.load() != 0)
            title << "   " << (tr.nudge.load() > 0 ? "+" : "") << juce::String (tr.nudge.load() * 1000.0 / juce::jmax (1.0, sr), 1) << " ms";
        g.drawText (title, strip.reduced (5.0f, 0.0f), juce::Justification::centredLeft);


        if (done > 0 && ! (live && tr.recToStage.load()))
        {
            g.setColour (col.withMultipliedBrightness (0.30f).withAlpha (0.95f));
            const double spp = sr * viewSeconds / w.getWidth();     // samples per pixel
            // auto-zoom: scale the drawing so the loudest peak of the take fills the lane height
            float peak = 0.0f;
            for (juce::int64 k = 0; k < done; ++k)
            {
                peak = juce::jmax (peak, mx[(size_t) k], -mn[(size_t) k]);
                if (stereo) peak = juce::jmax (peak, mx2[(size_t) k], -mn2[(size_t) k]);
            }
            const float gain = juce::jlimit (1.0f, 40.0f, 0.95f / juce::jmax (0.001f, peak));
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
                    const float y1 = mid - juce::jlimit (0.0f, 1.0f, mxv * gain) * half;
                    const float y2 = mid - juce::jlimit (-1.0f, 0.0f, mnv * gain) * half;
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
        };
        drawClip (nudgePx);
        // a one-loop take wraps around when nudged, exactly like it plays back
        if (! live && nudgePx != 0.0f && engine.loopEnabled.load() && clipW >= w.getWidth() - 3.0f)
            drawClip (nudgePx + (nudgePx > 0.0f ? -clipW : clipW));
    }
    else
    {
        g.setColour (Theme::dim);
        g.setFont (12.0f);
        g.drawText (armed ? "Armed - press R to record" : "Empty", wave, juce::Justification::centred);
    }

    if (selEnd > len) selEnd = len;
    if (selStart >= selEnd) selStart = selEnd = 0;
    if (hasSelection() && ! tr.editing)
    {
        const int nd = tr.nudge.load();
        const float x0 = sampleToX (selStart + nd), x1 = sampleToX (selEnd + nd);
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.fillRect (x0, w.getY(), x1 - x0, w.getHeight());
        g.setColour (Theme::text.withAlpha (0.75f));
        g.fillRect (x0, w.getY(), 1.0f, w.getHeight());
        g.fillRect (x1 - 1.0f, w.getY(), 1.0f, w.getHeight());
    }

    if (tr.editing)
    {
        const float top = w.getY() + (float) clipStripH;
        for (size_t i = 0; i < tr.hits.size(); ++i)
        {
            const auto& h = tr.hits[i];
            const float xs = sampleToX (h.src), xd = sampleToX (h.dst);
            const bool moved = h.dst != h.src;
            if (moved)
            {
                g.setColour (Theme::text.withAlpha (0.25f));
                g.fillRect (xs, top, 1.0f, w.getBottom() - top);
            }
            const bool active = (int) i == dragHit;
            g.setColour (moved || active ? Theme::yellow : Theme::text.withAlpha (0.85f));
            g.fillRect (xd - 0.5f, top, 1.5f, w.getBottom() - top);
            juce::Path tri;
            tri.addTriangle (xd - 5.0f, top, xd + 6.0f, top, xd + 0.5f, top + 8.0f);
            g.fillPath (tri);
        }
    }

    const float px = (float) (w.getX() + engine.getPosition() / juce::jmax (1.0, sr) * pxPerSec);
    if (engine.getState() != AudioEngine::Stopped || engine.getPosition() > 0)
    {
        g.setColour (Theme::text);
        g.fillRect (px, w.getY(), 1.5f, w.getHeight());
    }
    g.restoreState();
    g.setColour (glow.withAlpha (recNow ? 0.8f : armedNow ? 0.55f : 0.22f));
    g.drawRoundedRectangle (b.reduced (0.5f), Theme::radiusLg, 1.0f);              // solid 1 px border, low opacity
}

// ---------------------------------------------------------------- hit editing

float TrackRow::sampleToX (juce::int64 s) const
{
    return (float) wave.getX() + (float) (s / juce::jmax (1.0, engine.getSampleRate()) * (wave.getWidth() / viewSeconds));
}

juce::int64 TrackRow::xToSample (float x) const
{
    return (juce::int64) std::llround ((x - (float) wave.getX()) / (wave.getWidth() / viewSeconds) * engine.getSampleRate());
}

int TrackRow::hitAt (int x) const
{
    const auto& hs = engine.tracks[index].hits;
    int best = -1; float bd = 8.0f;
    for (size_t i = 0; i < hs.size(); ++i)
        if (const float d = std::abs (sampleToX (hs[i].dst) - (float) x); d < bd) { bd = d; best = (int) i; }
    return best;
}

void TrackRow::showHitsMenu()
{
    auto& t = track();
    juce::PopupMenu m;
    m.addItem (1, "Edit hits", true, t.editing);
    m.addSeparator();
    m.addItem (2, "Quantize to 1/4", t.editing);
    m.addItem (3, "Quantize to 1/8", t.editing);
    m.addItem (4, "Quantize to 1/16", t.editing);
    m.addItem (5, "Quantize to 1/8 (50%)", t.editing);
    m.addSeparator();
    m.addItem (6, "Detect more hits", t.editing);
    m.addItem (7, "Detect fewer hits", t.editing);
    m.addItem (8, "Reset all hits to original", t.editing);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&hitsBtn), [this] (int id)
    {
        auto& tr = track();
        const double sr = engine.getSampleRate();
        if (id == 1 || id == 6 || id == 7) clearSelection();
        switch (id)
        {
            case 1:
                if (tr.editing) tr.editing = false;                 // leave edit mode, keep the edited audio
                else if (engine.getState() == AudioEngine::Stopped) tr.beginEdit (sr, riseDb);
                break;
            case 2: quantize (1.0, 1.0f); break;
            case 3: quantize (2.0, 1.0f); break;
            case 4: quantize (4.0, 1.0f); break;
            case 5: quantize (2.0, 0.5f); break;
            case 6: riseDb = juce::jmax (3.0f, riseDb - 1.5f); tr.beginEdit (sr, riseDb); tr.applyEdit (sr); break;
            case 7: riseDb = juce::jmin (15.0f, riseDb + 1.5f); tr.beginEdit (sr, riseDb); tr.applyEdit (sr); break;
            case 8: { const auto before = tr.hits; for (auto& h : tr.hits) h.dst = h.src; tr.applyEdit (sr); pushHitEdit ("Reset hits", before); break; }
            default: break;
        }
        if (onReleaseFocus) onReleaseFocus();
        repaint();
    });
}

// Snap hit targets to the beat grid (division 1 = quarter notes, 2 = eighths, 4 = sixteenths).
void TrackRow::quantize (double division, float strength)
{
    auto& t = track();
    if (! t.editing || t.hits.empty()) return;
    const auto hitsBefore = t.hits;
    const double sr = engine.getSampleRate();
    const double step = engine.getSamplesPerBeat() / division;
    const juce::int64 minGap = (juce::int64) (0.02 * sr), len = t.editSource.getNumSamples();

    for (auto& h : t.hits)
    {
        const double snapped = std::round ((double) h.src / step) * step;
        h.dst = (juce::int64) std::llround ((double) h.src + (snapped - (double) h.src) * strength);
    }
    for (size_t i = 0; i < t.hits.size(); ++i)   // keep order and a minimum distance
    {
        const auto lo = (i == 0 ? 0 : t.hits[i - 1].dst) + minGap;
        t.hits[i].dst = juce::jmax (lo, t.hits[i].dst);
    }
    for (size_t i = t.hits.size(); i-- > 0;)
    {
        const auto hi = (i + 1 == t.hits.size() ? len : t.hits[i + 1].dst) - minGap;
        t.hits[i].dst = juce::jmin (hi, t.hits[i].dst);
    }
    t.applyEdit (sr);
    pushHitEdit ("Quantize", hitsBefore);
}

juce::MouseCursor TrackRow::getMouseCursor()
{
    if (! track().editing && track().length.load() > 0 && wave.contains (getMouseXYRelative()))
        return juce::MouseCursor::IBeamCursor;
    return juce::Component::getMouseCursor();
}

// Drag the clip sideways to nudge the track in time (right-drag always, left-drag when not editing hits).
void TrackRow::mouseDown (const juce::MouseEvent& e)
{
    auto& t = track();
    dragHit = -1;
    nudging = false;
    selecting = false;
    if (! wave.contains (e.getPosition())) return;

    if (t.editing)
    {
        const int h = hitAt (e.x);
        if (h >= 0)
        {
            if (e.mods.isPopupMenu())      // right-click on a hit removes it
            {
                const auto before = t.hits;
                t.hits.erase (t.hits.begin() + h);
                t.applyEdit (engine.getSampleRate());
                pushHitEdit ("Remove hit", before);
                repaint();
            }
            else
            {
                dragHit = h;
                hitsBeforeDrag = t.hits;
            }
            return;
        }
        if (! e.mods.isPopupMenu()) return;   // left-drag in empty space does nothing while editing hits
    }

    if (t.length.load() <= 0) return;
    if (e.mods.isPopupMenu())          // right button: drag = nudge in time, click = clip menu
    {
        nudging = true;
        nudgeStartX = e.x;
        nudgeStartVal = t.nudge.load();
    }
    else if (e.mods.isLeftButtonDown())   // left button: select a region (Shift = snap to 1/16)
    {
        selecting = true;
        selAnchor = xToTake ((float) e.x, e.mods.isShiftDown());
        selStart = selEnd = selAnchor;
        if (onSelectionStarted) onSelectionStarted (this);
        repaint (wave);
    }
}
void TrackRow::mouseDrag (const juce::MouseEvent& e)
{
    auto& t = track();
    if (selecting)
    {
        const auto cur = xToTake ((float) e.x, e.mods.isShiftDown());
        selStart = juce::jmin (selAnchor, cur);
        selEnd = juce::jmax (selAnchor, cur);
        repaint (wave);
        return;
    }
    if (nudging)
    {
        const double sr = engine.getSampleRate();
        double total = nudgeStartVal + (e.x - nudgeStartX) / (wave.getWidth() / viewSeconds) * sr;
        if (e.mods.isShiftDown())        // Shift = snap to 1/16 notes
        {
            const double step = engine.getSamplesPerBeat() / 4.0;
            total = std::round (total / step) * step;
        }
        track().nudge = (int) juce::jlimit (-2.0 * sr, 2.0 * sr, total);
        repaint (wave);
        return;
    }
    if (dragHit < 0 || dragHit >= (int) t.hits.size()) return;
    const juce::int64 gap = (juce::int64) (0.02 * engine.getSampleRate());
    const auto lo = (dragHit == 0 ? 0 : t.hits[(size_t) dragHit - 1].dst) + gap;
    const auto hi = ((size_t) dragHit + 1 == t.hits.size() ? (juce::int64) t.editSource.getNumSamples()
                                                           : t.hits[(size_t) dragHit + 1].dst) - gap;
    t.hits[(size_t) dragHit].dst = juce::jlimit (lo, juce::jmax (lo, hi), xToSample ((float) e.x));
    repaint (wave);
}

void TrackRow::mouseUp (const juce::MouseEvent& e)
{
    if (selecting)
    {
        selecting = false;
        if (std::abs (e.getDistanceFromDragStartX()) < 3) { selStart = selEnd = 0; repaint (wave); }   // a plain click clears
        if (onReleaseFocus) onReleaseFocus();
        return;
    }
    if (nudging)
    {
        const int before = nudgeStartVal, after = track().nudge.load();
        if (before != after)
            push ("Nudge", [this, after] { track().nudge = after; repaint(); }, [this, before] { track().nudge = before; repaint(); });
        else if (! e.mouseWasDraggedSinceMouseDown())
        {
            nudging = false;
            showClipMenu();
        }
    }
    nudging = false;
    if (dragHit >= 0)
    {
        track().applyEdit (engine.getSampleRate());   // re-render once, when the marker is released
        pushHitEdit ("Move hit", hitsBeforeDrag);
        dragHit = -1;
        repaint();
    }
}

void TrackRow::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto& t = track();
    if (! wave.contains (e.getPosition())) return;
    if (! t.editing)   // double-click resets the nudge
    {
        const int before = t.nudge.load();
        t.nudge = 0;
        if (before != 0) push ("Reset nudge", [this] { track().nudge = 0; repaint(); }, [this, before] { track().nudge = before; repaint(); });
        repaint (wave);
        return;
    }
    const double sr = engine.getSampleRate();
    const int h = hitAt (e.x);
    if (h >= 0)
    {
        const auto before = t.hits;
        t.hits[(size_t) h].dst = t.hits[(size_t) h].src;
        t.applyEdit (sr);
        pushHitEdit ("Reset hit", before);
        repaint();
        return;
    }

    // add a hit where the user clicked (missed by the detector)
    const auto dst = xToSample ((float) e.x);
    if (dst <= 0 || dst >= t.editSource.getNumSamples()) return;
    size_t at = 0;
    while (at < t.hits.size() && t.hits[at].dst < dst) ++at;
    const juce::int64 d0 = at == 0 ? 0 : t.hits[at - 1].dst, s0 = at == 0 ? 0 : t.hits[at - 1].src;
    const juce::int64 d1 = at == t.hits.size() ? t.editSource.getNumSamples() : t.hits[at].dst;
    const juce::int64 s1 = at == t.hits.size() ? t.editSource.getNumSamples() : t.hits[at].src;
    const double f = d1 > d0 ? (double) (dst - d0) / (double) (d1 - d0) : 0.0;
    const auto before = t.hits;
    t.hits.insert (t.hits.begin() + (long) at, { s0 + (juce::int64) std::llround (f * (double) (s1 - s0)), dst });
    pushHitEdit ("Add hit", before);
    repaint();
}

// ---------------------------------------------------------------- selection editing

juce::int64 TrackRow::xToTake (float x, bool snap) const
{
    double t = (double) xToSample (x);
    if (snap)
    {
        const double step = engine.getSamplesPerBeat() / 4.0;     // 1/16 notes
        t = std::round (t / step) * step;
    }
    const auto& tr = engine.tracks[index];
    return juce::jlimit<juce::int64> (0, tr.length.load(), (juce::int64) std::llround (t) - tr.nudge.load());
}

void TrackRow::selectionCommand (SelCmd cmd)
{
    auto& t = track();
    const auto st = engine.getState();
    auto say = [this] (const juce::String& s) { if (onStatus) onStatus (s); };
    if (st == AudioEngine::Recording || st == AudioEngine::CountIn) { say ("Stop recording first"); return; }
    if (t.editing) { say ("Leave hit editing first"); return; }
    const juce::int64 len = t.length.load();
    if (len <= 0) return;

    juce::int64 s0 = selStart, s1 = juce::jmin (selEnd, len);
    if (cmd == SelPaste)
    {
        if (clipboard.getNumSamples() == 0) { say ("Nothing to paste"); return; }
        if (! hasSelection())    // no selection: paste at the playhead
        {
            const juce::int64 pos = st == AudioEngine::Stopped ? engine.getCursor() : engine.getPosition();
            s0 = pos - t.nudge.load();
        }
        if (s0 < 0 || s0 >= len) { say ("Playhead is outside this track"); return; }
        s1 = juce::jmin<juce::int64> (len, s0 + clipboard.getNumSamples());
    }
    else if (! hasSelection() || s1 <= s0) { say ("Select a region first: drag in the waveform"); return; }

    auto before = std::make_shared<juce::AudioBuffer<float>> (t.copyRegion (s0, s1));
    auto after = std::make_shared<juce::AudioBuffer<float>> (2, before->getNumSamples());
    const int n = before->getNumSamples();
    juce::String name;

    switch (cmd)
    {
        case SelCopy:
            clipboard = *before;
            say ("Copied " + juce::String ((double) n / juce::jmax (1.0, engine.getSampleRate()), 2) + " s");
            return;
        case SelSilence: case SelCut:
            if (cmd == SelCut) clipboard = *before;
            after->clear();
            name = cmd == SelCut ? "Cut" : "Silence";
            break;
        case SelFadeIn: case SelFadeOut:
            for (int c = 0; c < 2; ++c)
            {
                after->copyFrom (c, 0, *before, c, 0, n);
                auto* d = after->getWritePointer (c);
                for (int i = 0; i < n; ++i)
                {
                    const float g = (float) (i + 1) / (float) (n + 1);
                    d[i] *= cmd == SelFadeIn ? g : 1.0f - g;
                }
            }
            name = cmd == SelFadeIn ? "Fade in" : "Fade out";
            break;
        case SelPaste:
            for (int c = 0; c < 2; ++c) after->copyFrom (c, 0, clipboard, c, 0, n);
            name = "Paste";
            selStart = s0; selEnd = s1;
            break;
    }

    t.resetEdit();   // older hit-edit undo steps must not touch the changed audio
    push (name,
          [this, s0, after] { track().writeRegion (s0, *after); repaint(); },
          [this, s0, before] { track().writeRegion (s0, *before); repaint(); });
}

void TrackRow::showClipMenu()
{
    auto& t = track();
    if (t.editing || t.length.load() <= 0) return;
    const bool sel = hasSelection();
    juce::PopupMenu m;
    m.addItem (1, "Silence selection\tDel", sel);
    m.addItem (2, "Cut\tCtrl+X", sel);
    m.addItem (3, "Copy\tCtrl+C", sel);
    m.addItem (4, "Paste\tCtrl+V", clipboard.getNumSamples() > 0);
    m.addSeparator();
    m.addItem (5, "Fade in", sel);
    m.addItem (6, "Fade out", sel);
    m.addSeparator();
    m.addItem (7, "Duplicate track\tCtrl+D");
    m.showMenuAsync (juce::PopupMenu::Options(), [this] (int id)
    {
        switch (id)
        {
            case 1: selectionCommand (SelSilence); break;
            case 2: selectionCommand (SelCut); break;
            case 3: selectionCommand (SelCopy); break;
            case 4: selectionCommand (SelPaste); break;
            case 5: selectionCommand (SelFadeIn); break;
            case 6: selectionCommand (SelFadeOut); break;
            case 7: if (onDuplicate) onDuplicate (this); break;
            default: break;
        }
        if (onReleaseFocus) onReleaseFocus();
    });
}

void TrackRow::resized()
{
    auto r = getLocalBounds().reduced (8, 10);
    r.removeFromLeft (4);
    grip.setBounds (r.removeFromLeft (16));
    r.removeFromLeft (6);
    auto header = r.removeFromLeft (336);
    r.removeFromLeft (4);
    meter.setBounds (r.removeFromLeft (14));
    r.removeFromLeft (6);
    wave = r;
    prevTake.setBounds (wave.getRight() - 62, wave.getY(), 18, clipStripH);
    nextTake.setBounds (wave.getRight() - 20, wave.getY(), 18, clipStripH);
    takeLabel.setBounds (wave.getRight() - 44, wave.getY(), 24, clipStripH);
    hitsBtn.setBounds (wave.getRight() - 112, wave.getY(), 46, clipStripH);

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
    fader.onDragStart = [this] { dragging = true; };
    fader.onDragEnd = [this] { dragging = false; commitMaster(); };
    fader.onValueChange = [this]
    {
        engine.masterVolume = faderToGain ((float) fader.getValue());
        dbLabel.setText (dbText ((float) fader.getValue()), juce::dontSendNotification);
        if (! dragging) commitMaster();
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

void MasterStrip::commitMaster()
{
    const float after = engine.masterVolume.load(), before = lastMaster;
    if (after == before) return;
    lastMaster = after;
    auto set = [this] (float g)
    {
        engine.masterVolume = g; lastMaster = g;
        fader.setValue (gainToFader (g), juce::dontSendNotification);
        dbLabel.setText (dbText ((float) fader.getValue()), juce::dontSendNotification);
    };
    if (onUndoable) onUndoable ("Master volume", [set, after] { set (after); }, [set, before] { set (before); });
}

void MasterStrip::refreshFromEngine()
{
    lastMaster = engine.masterVolume.load();
    fader.setValue (gainToFader (engine.masterVolume.load()), juce::dontSendNotification);
    dbLabel.setText (dbText ((float) fader.getValue()), juce::dontSendNotification);
}

void MasterStrip::tick() { meter.setLevels (engine.masterPeakL.exchange (0.0f), engine.masterPeakR.exchange (0.0f)); }

void MasterStrip::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    {
        juce::Graphics::ScopedSaveState s (g);
        juce::Path card;
        card.addRoundedRectangle (b, Theme::radiusLg);
        g.reduceClipRegion (card);
        g.setColour (Theme::panel);
        g.fillRect (b);
        g.setColour (Theme::purple);
        g.fillRect (b.withWidth (4.0f));
    }
    g.setColour (Theme::border.withAlpha (0.55f));
    g.drawRoundedRectangle (b.reduced (0.5f), Theme::radiusLg, 1.0f);
}

void MasterStrip::resized()
{
    auto r = getLocalBounds().reduced (8, 10);
    r.removeFromLeft (4);
    title.setBounds (r.removeFromLeft (80));
    fader.setBounds (r.removeFromLeft (260));
    dbLabel.setBounds (r.removeFromLeft (60));
    r.removeFromLeft (16);
    meter.setBounds (r.withSizeKeepingCentre (r.getWidth(), 20));
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
}

void TrackList::dragStart (TrackRow* r)
{
    dragging = r;
    dragStartY = dragY = r->getY();
}

void TrackList::dragMove (TrackRow*, int dy)
{
    if (dragging == nullptr) return;
    dragY = juce::jlimit (0, juce::jmax (0, rowsHeight() - rowHeight), dragStartY + dy);
    layoutRows();
}

void TrackList::dragEnd (TrackRow* r)
{
    if (dragging == nullptr) return;
    const int t = targetSlot (dragY);
    const auto orderBefore = engine.order;
    engine.order.erase (std::remove (engine.order.begin(), engine.order.end(), r->slot()), engine.order.end());
    engine.order.insert (engine.order.begin() + juce::jmin (t, (int) engine.order.size()), r->slot());
    dragging = nullptr;
    layoutRows();
    if (onReordered && orderBefore != engine.order) onReordered (orderBefore, engine.order);
}

// ---------------------------------------------------------------- ruler

// Click or drag in the ruler to move the playhead. Snaps to beats; Shift = bars, Alt = free.
void Ruler::seekTo (const juce::MouseEvent& e)
{
    const float x0 = (float) TrackRow::waveLeft;
    const float wdt = (float) getWidth() - x0 - (float) TrackRow::waveRightInset;
    if (wdt <= 0.0f || ! onSeek) return;
    const double sr = engine.getSampleRate();
    double pos = juce::jlimit (0.0, 1.0, (double) (e.x - x0) / wdt) * viewSeconds * sr;
    if (! e.mods.isAltDown())
    {
        const double step = engine.getSamplesPerBeat() * (e.mods.isShiftDown() ? 4.0 : 1.0);
        pos = std::round (pos / step) * step;
    }
    if (engine.loopEnabled.load()) pos = juce::jmin (pos, (double) (engine.getLoopLength() - 1));
    onSeek ((juce::int64) pos);
}

void Ruler::paint (juce::Graphics& g)
{
    const float x0 = (float) TrackRow::waveLeft;
    const float wdt = (float) getWidth() - x0 - (float) TrackRow::waveRightInset;
    if (wdt <= 0.0f) return;
    const double pxPerSec = wdt / viewSeconds;

    // same rounded dark lane as the clips below, so the ruler reads as part of the track area
    const juce::Rectangle<float> pill (x0, 3.0f, wdt, (float) getHeight() - 6.0f);
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (pill, Theme::radius);

    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (pill.toNearestInt());
    g.setFont (juce::Font (juce::FontOptions (11.0f)));

    if (barSeconds > 0.0)
    {
        const double beat = barSeconds / 4.0;
        int k = 0;
        for (double s = 0.0; s < viewSeconds - 0.001; s += beat, ++k)
        {
            const float x = x0 + (float) (s * pxPerSec);
            const bool bar = (k % 4 == 0);
            g.setColour (bar ? Theme::dim : Theme::border);
            g.fillRect (x, bar ? pill.getY() + 5.0f : pill.getBottom() - 8.0f, 1.0f, bar ? pill.getHeight() - 10.0f : 5.0f);
            if (bar)
            {
                g.setColour (Theme::text);
                g.drawText (juce::String (k / 4 + 1), (int) x + 5, (int) pill.getY(), 40, (int) pill.getHeight() - 4, juce::Justification::centredLeft);
            }
        }
        if (engine.loopEnabled.load())   // loop region as a thin rounded pill along the bottom
        {
            g.setColour (Theme::teal.withAlpha (0.9f));
            g.fillRoundedRectangle (pill.getX() + 4.0f, pill.getBottom() - 4.0f, pill.getWidth() - 8.0f, 2.0f, 1.0f);
        }
    }
    else
    {
        for (double s = 0.0; s < viewSeconds; s += 5.0)
        {
            const float x = x0 + (float) (s * pxPerSec);
            g.setColour (Theme::dim);
            g.fillRect (x, pill.getY() + 5.0f, 1.0f, pill.getHeight() - 10.0f);
            g.setColour (Theme::text);
            g.drawText (juce::String::formatted ("%d:%02d", (int) s / 60, (int) s % 60), (int) x + 5, (int) pill.getY(), 50, (int) pill.getHeight() - 4, juce::Justification::centredLeft);
        }
    }

    if (engine.getState() != AudioEngine::Stopped || engine.getPosition() > 0)
    {
        const float px = x0 + (float) (engine.getPosition() / juce::jmax (1.0, engine.getSampleRate()) * pxPerSec);
        juce::Path tri;
        tri.addTriangle (px - 5.0f, pill.getBottom() - 9.0f, px + 6.0f, pill.getBottom() - 9.0f, px + 0.5f, pill.getBottom() - 1.0f);
        g.setColour (Theme::text);
        g.fillPath (tri);
    }
}