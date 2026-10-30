#include "MainComponent.h"
#include "project/ProjectIO.h"
#include "project/Exporter.h"
#include "WinTitleBar.h"

MainComponent::MainComponent()
{
    juce::PropertiesFile::Options opts;
    opts.applicationName = "RiffCatch";
    opts.filenameSuffix = ".settings";
    opts.osxLibrarySubFolder = "Application Support";
    props.setStorageParameters (opts);

    std::unique_ptr<juce::XmlElement> saved;
    if (auto* f = props.getUserSettings()) saved = f->getXmlValue ("audioDeviceState");
    deviceManager.initialise (8, 2, saved.get(), true);
    deviceManager.addAudioCallback (&engine);
    deviceManager.addChangeListener (this);

    for (int i = 0; i < AudioEngine::maxTracks; ++i)
    {
        auto* r = new TrackRow (engine, i);
        r->onReleaseFocus = [this] { grabKeyboardFocus(); };
        r->onUndoable  = [this] (const juce::String& n, std::function<void()> a, std::function<void()> b) { record (n, std::move (a), std::move (b)); };
        r->onArmChanged = [this] { warnIfMultipleArmed(); };
        r->onDelete    = [this] (TrackRow* row) { deleteTrack (row); };
        r->onDuplicate = [this] (TrackRow* row) { duplicateTrack (row->slot()); };
        r->onStatus    = [this] (const juce::String& s) { setStatus (s); };
        r->onSelectionStarted = [this] (TrackRow* row)
        {
            selSlot = row->slot();
            for (auto* o : rows) if (o != row) o->clearSelection();
        };
        r->onDragStart = [this] (TrackRow* row) { list.dragStart (row); };
        r->onDragMove  = [this] (TrackRow* row, int dy) { list.dragMove (row, dy); };
        r->onDragEnd   = [this] (TrackRow* row) { list.dragEnd (row); };
        rows.add (r);
        list.addChildComponent (r);
    }
    master.onUndoable = [this] (const juce::String& n, std::function<void()> a, std::function<void()> b) { record (n, std::move (a), std::move (b)); };
    list.onReordered = [this] (std::vector<int> before, std::vector<int> after)
    {
        record ("Reorder tracks",
                [this, after] { engine.order = after; list.layoutRows(); },
                [this, before] { engine.order = before; list.layoutRows(); });
    };
    engine.onTakeCommitted = [this] (int slot, int prev, int cur)
    {
        record ("Recording",
                [this, slot, cur] { engine.tracks[slot].applyTake (cur); },
                [this, slot, prev] { auto& t = engine.tracks[slot]; if (prev >= 0) t.applyTake (prev); else t.silence(); });
    };
    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (false, false, true, false);   // wheel scrolling, no scrollbar that would narrow the rows
    addAndMakeVisible (viewport);
    addAndMakeVisible (master);
    addAndMakeVisible (ruler);

    titleLabel.setText ("RiffCatch", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, Theme::blue);
    addAndMakeVisible (titleLabel);

    for (auto* b : { &recBtn, &playBtn, &stopBtn }) addAndMakeVisible (b);
    recBtn.setTooltip ("Record (R)");
    playBtn.setTooltip ("Play / Stop (Space)");
    stopBtn.setTooltip ("Stop and rewind (Enter)");
    saveBtn.setTooltip ("Save (Ctrl+S, Ctrl+Shift+S = Save as)");
    openBtn.setTooltip ("Open (Ctrl+O)");
    newBtn.setTooltip ("New (Ctrl+N)");
    recBtn.onClick  = [this] { toggleRecord(); };
    playBtn.onClick = [this] { togglePlay(); };
    stopBtn.onClick = [this]
    {
        if (engine.getState() == AudioEngine::Stopped) engine.setCursor (0);   // second press rewinds to the start
        else stopTransport();
    };
    ruler.onSeek = [this] (juce::int64 p) { engine.seek (p); };
    ruler.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    ruler.setTooltip ("Click or drag to move the playhead (snaps to beats, Shift = bars, Alt = free)");
    list.onAdd = [this] { addTrack(); };

    for (auto* b : { &exportBtn, &newBtn, &openBtn, &saveBtn, &audioBtn })
    {
        b->setWantsKeyboardFocus (false);
        b->setColour (juce::TextButton::buttonColourId, Theme::raised);
        addAndMakeVisible (b);
    }
    saveBtn.setColour (juce::TextButton::buttonColourId, Theme::blue.withAlpha (0.25f));
    exportBtn.setTooltip ("Bounce master to WAV or MP3");
    exportBtn.onClick = [this] { exportMenu(); };
    newBtn.onClick   = [this] { newProject(); };
    openBtn.onClick  = [this] { openProject(); };
    saveBtn.onClick  = [this] { saveProject (false); };
    audioBtn.onClick = [this] { showAudioSettings(); };

    timeLabel.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 26.0f, juce::Font::plain)));
    timeLabel.setJustificationType (juce::Justification::centred);
    timeLabel.setColour (juce::Label::textColourId, Theme::text);
    addAndMakeVisible (timeLabel);

    projectLabel.setJustificationType (juce::Justification::centredRight);
    projectLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
    projectLabel.setColour (juce::Label::textColourId, Theme::dim);
    addAndMakeVisible (projectLabel);

    statusLabel.setColour (juce::Label::textColourId, Theme::yellow);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    // ---- tempo / loop / click / count-in controls
    bpmCaption.setText ("BPM", juce::dontSendNotification);
    countCaption.setText ("Count-in", juce::dontSendNotification);
    for (auto* l : { &bpmCaption, &countCaption }) { l->setColour (juce::Label::textColourId, Theme::dim); addAndMakeVisible (l); }

    bpmLabel.setEditable (true);
    bpmLabel.setJustificationType (juce::Justification::centred);
    bpmLabel.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 20.0f, juce::Font::bold)));
    bpmLabel.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    bpmLabel.setColour (juce::Label::textColourId, Theme::cyan);
    bpmLabel.setTooltip ("Tempo (double-click to type, 30-300)");
    bpmLabel.onTextChange = [this] { setBpm (bpmLabel.getText().getDoubleValue()); grabKeyboardFocus(); };
    addAndMakeVisible (bpmLabel);

    for (auto* b : { &undoBtn, &redoBtn })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    undoBtn.onClick = [this] { doUndo (false); };
    redoBtn.onClick = [this] { doUndo (true); };

    tapBtn.setTooltip ("Tap tempo (T)");
    tapBtn.setWantsKeyboardFocus (false);
    tapBtn.onClick = [this] { tapTempo(); };
    addAndMakeVisible (tapBtn);

    for (auto* b : { &loopBtn, &clickBtn })
    {
        b->setClickingTogglesState (true);
        b->setWantsKeyboardFocus (false);
        b->setColour (juce::TextButton::buttonOnColourId, Theme::teal);
        addAndMakeVisible (b);
    }
    loopBtn.setTooltip ("Loop the project region while playing/recording (L)");
    clickBtn.setTooltip ("Metronome during play/record (C). Count-in always clicks.");
    loopBtn.onClick  = [this] { engine.loopEnabled = loopBtn.getToggleState(); };
    clickBtn.onClick = [this] { engine.clickEnabled = clickBtn.getToggleState(); };

    for (int b : { 1, 2, 4, 8, 16 }) barsBox.addItem (juce::String (b) + (b == 1 ? " bar" : " bars"), b);
    barsBox.setWantsKeyboardFocus (false);
    barsBox.setTooltip ("Loop length in bars (4/4)");
    barsBox.onChange = [this] { engine.loopBars = barsBox.getSelectedId(); grabKeyboardFocus(); };
    addAndMakeVisible (barsBox);

    clickVol.setSliderStyle (juce::Slider::LinearHorizontal);
    clickVol.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    clickVol.setRange (0.0, 1.0, 0.01);
    clickVol.setColour (juce::Slider::trackColourId, Theme::teal);
    clickVol.setWantsKeyboardFocus (false);
    clickVol.setTooltip ("Click volume");
    clickVol.onValueChange = [this] { engine.clickVolume = (float) clickVol.getValue(); };
    addAndMakeVisible (clickVol);

    countInBox.addItem ("Off", 1);
    countInBox.addItem ("1 bar", 11);
    countInBox.addItem ("2 bars", 12);
    countInBox.setWantsKeyboardFocus (false);
    countInBox.setTooltip ("Count-in before recording starts (4 beats per bar)");
    countInBox.onChange = [this]
    {
        const int id = countInBox.getSelectedId();
        engine.countInBars = id == 1 ? 0 : id - 10;
        grabKeyboardFocus();
    };
    addAndMakeVisible (countInBox);

    latLabel.setColour (juce::Label::textColourId, Theme::dim);
    latLabel.setJustificationType (juce::Justification::centredLeft);
    latLabel.setTooltip ("Round-trip latency reported by the audio driver. Recordings are shifted earlier by this amount (plus the adjustment) so they sit on the grid.");
    latAdjust.setSliderStyle (juce::Slider::LinearBar);
    latAdjust.setRange (-50.0, 50.0, 0.5);
    latAdjust.setNumDecimalPlacesToDisplay (1);
    latAdjust.setTextValueSuffix (" ms");
    latAdjust.setDoubleClickReturnValue (true, 0.0);
    latAdjust.setColour (juce::Slider::trackColourId, Theme::teal.withAlpha (0.35f));
    latAdjust.setColour (juce::Slider::textBoxTextColourId, Theme::text);
    latAdjust.setColour (juce::Slider::textBoxOutlineColourId, Theme::border);
    latAdjust.setWantsKeyboardFocus (false);
    latAdjust.setTooltip ("Extra latency compensation in ms (fine-tune until recordings sit on the click)");
    latAdjust.onValueChange = [this] { saveLatencyAdjust(); };
    if (auto* f = props.getUserSettings()) latAdjust.setValue (f->getDoubleValue ("latencyAdjustMs", 0.0), juce::sendNotification);

    barLabel.setJustificationType (juce::Justification::centredLeft);
    barLabel.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::plain)));
    barLabel.setColour (juce::Label::textColourId, Theme::cyan);
    addAndMakeVisible (barLabel);

    engine.addTrack();
    engine.addTrack();
    changeListenerCallback (nullptr);
    refreshStrips();
    setWantsKeyboardFocus (true);
    setSize (1280, 760);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    if (audioDialog != nullptr) delete audioDialog.getComponent();
    deviceManager.removeChangeListener (this);
    deviceManager.removeAudioCallback (&engine);
    if (auto xml = deviceManager.createStateXml())
        if (auto* f = props.getUserSettings()) { f->setValue ("audioDeviceState", xml.get()); f->saveIfNeeded(); }
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (auto xml = deviceManager.createStateXml())
        if (auto* f = props.getUserSettings()) { f->setValue ("audioDeviceState", xml.get()); f->saveIfNeeded(); }
    const int n = engine.getNumInputs();
    for (auto* r : rows) r->setInputChoices (juce::jmax (1, n));
}

void MainComponent::refreshStrips()
{
    for (auto* r : rows)
        if (engine.tracks[r->slot()].active.load()) r->refreshFromTrack();
    master.refreshFromEngine();
    updateSettingsFromEngine();
    projectLabel.setText (projectDir == juce::File() ? "Untitled" : projectDir.getFileName(), juce::dontSendNotification);
    resized();
    list.layoutRows();
    repaint();
}

void MainComponent::updateSettingsFromEngine()
{
    const double b = engine.bpm.load();
    bpmLabel.setText (std::abs (b - std::round (b)) < 0.05 ? juce::String ((int) std::round (b)) : juce::String (b, 1), juce::dontSendNotification);
    loopBtn.setToggleState (engine.loopEnabled.load(), juce::dontSendNotification);
    clickBtn.setToggleState (engine.clickEnabled.load(), juce::dontSendNotification);
    barsBox.setSelectedId (engine.loopBars.load(), juce::dontSendNotification);
    clickVol.setValue (engine.clickVolume.load(), juce::dontSendNotification);
    const int cb = engine.countInBars.load();
    countInBox.setSelectedId (cb <= 0 ? 1 : 10 + cb, juce::dontSendNotification);
}

void MainComponent::setBpm (double v)
{
    if (engine.getState() == AudioEngine::Stopped)
        engine.bpm = juce::jlimit (30.0, 300.0, std::round (v * 10.0) / 10.0);
    updateSettingsFromEngine();
}

void MainComponent::tapTempo()
{
    if (engine.getState() != AudioEngine::Stopped) return;
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! taps.empty() && now - taps.back() > 2000.0) taps.clear();
    taps.push_back (now);
    if (taps.size() > 6) taps.erase (taps.begin());
    if (taps.size() >= 2)   // average of the last intervals
        setBpm (60000.0 / ((taps.back() - taps.front()) / (double) (taps.size() - 1)));
    else
        setStatus ("Keep tapping...");
}

void MainComponent::saveLatencyAdjust()
{
    engine.latencyAdjustMs = (float) latAdjust.getValue();
    if (auto* f = props.getUserSettings()) { f->setValue ("latencyAdjustMs", latAdjust.getValue()); f->saveIfNeeded(); }
}

// Menu item id = kind * 100 + loops. kind: 1 = WAV, 2..4 = MP3 192/256/320 kbps
void MainComponent::exportMenu()
{
    const bool looping = engine.loopEnabled.load();
    auto addFormat = [&] (juce::PopupMenu& m, int kind, const juce::String& title)
    {
        if (looping)
        {
            juce::PopupMenu sub;
            for (int loops : { 1, 2, 4, 8 })
                sub.addItem (kind * 100 + loops, juce::String (loops) + (loops == 1 ? " loop" : " loops"));
            m.addSubMenu (title, sub);
        }
        else
            m.addItem (kind * 100 + 1, title + " (full length)");
    };

    juce::PopupMenu m;
    addFormat (m, 1, "WAV 24-bit");
    if (Exporter::mp3Available)
    {
        m.addSeparator();
        addFormat (m, 2, "MP3 192 kbps");
        addFormat (m, 3, "MP3 256 kbps");
        addFormat (m, 4, "MP3 320 kbps");
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exportBtn), [this] (int id)
    {
        if (id <= 0) return;
        const int kind = id / 100, loops = id % 100;
        const bool mp3 = kind > 1;
        const juce::String ext = mp3 ? "mp3" : "wav";
        const auto base = projectDir == juce::File() ? juce::String ("RiffCatch") : projectDir.getFileName();
        chooser = std::make_unique<juce::FileChooser> (mp3 ? "Export MP3" : "Export WAV",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (base + "." + ext), "*." + ext);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, kind, loops, ext] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f != juce::File()) doExport (f.withFileExtension (ext), kind, loops);
                              });
    });
}

void MainComponent::doExport (const juce::File& f, int kind, int loops)
{
    juce::String err;
    const int kbps = kind == 2 ? 192 : kind == 3 ? 256 : 320;
    const bool ok = kind == 1 ? Exporter::bounceWav (engine, f, loops, err)
                              : Exporter::bounceMp3 (engine, f, loops, kbps, err);
    if (ok) setStatus ("Exported " + f.getFullPathName());
    else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export failed", err);
}
void MainComponent::record (const juce::String& name, std::function<void()> redo, std::function<void()> undo)
{
    undoManager.beginNewTransaction (name);
    undoManager.perform (new LambdaAction (std::move (redo), std::move (undo)));
}

void MainComponent::syncRows()
{
    changeListenerCallback (nullptr);   // input choices for (re)activated rows
    refreshStrips();
}

MainComponent::TrackSnap MainComponent::captureTrack (int slot)
{
    TrackSnap s;
    auto& t = engine.tracks[slot];
    s.slot = slot; s.valid = true;
    s.name = t.name; s.vol = t.volume.load(); s.pan = t.pan.load();
    s.mute = t.mute.load(); s.solo = t.solo.load(); s.input = t.inputCode.load(); s.nudge = t.nudge.load();
    s.orderPos = (int) (std::find (engine.order.begin(), engine.order.end(), slot) - engine.order.begin());
    const auto len = (int) t.length.load();
    if (len > 0)
    {
        s.audio.setSize (2, len);
        juce::SpinLock::ScopedLockType l (t.lock);
        for (int c = 0; c < 2; ++c) s.audio.copyFrom (c, 0, t.data, c, 0, len);
    }
    return s;
}

int MainComponent::restoreTrack (const TrackSnap& s)
{
    const int slot = engine.addTrackAt (s.slot);
    if (slot < 0) return -1;
    auto& t = engine.tracks[slot];
    t.name = s.name; t.volume = s.vol; t.pan = s.pan; t.mute = s.mute; t.solo = s.solo;
    t.inputCode = s.input; t.nudge = s.nudge;
    if (s.audio.getNumSamples() > 0) { auto copy = s.audio; t.setAudio (std::move (copy)); }
    engine.order.erase (std::remove (engine.order.begin(), engine.order.end(), slot), engine.order.end());
    engine.order.insert (engine.order.begin() + juce::jmin (s.orderPos, (int) engine.order.size()), slot);
    return slot;
}

void MainComponent::addTrack()
{
    if (engine.getNumTracks() >= AudioEngine::maxTracks) return;
    auto slot = std::make_shared<int> (-1);
    auto snap = std::make_shared<TrackSnap>();
    record ("Add track",
            [this, slot, snap]
            {
                *slot = snap->valid ? restoreTrack (*snap) : engine.addTrackAt (*slot);
                syncRows();
            },
            [this, slot, snap]
            {
                *snap = captureTrack (*slot);
                engine.removeTrack (*slot);
                syncRows();
            });
    viewport.setViewPosition (0, list.contentHeight());
}

void MainComponent::deleteTrack (TrackRow* row)
{
    const int slot = row->slot();
    auto doDelete = [this, slot]
    {
        auto snap = std::make_shared<TrackSnap>();
        record ("Delete track",
                [this, slot, snap]
                {
                    *snap = captureTrack (slot);
                    engine.removeTrack (slot);
                    syncRows();
                },
                [this, snap]
                {
                    restoreTrack (*snap);
                    syncRows();
                });
    };
    if (engine.tracks[slot].length.load() == 0) { doDelete(); return; }
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Delete track",
                                        "Delete \"" + engine.tracks[slot].name + "\" and its recording? You can undo this with Ctrl+Z.",
                                        "Delete", "Cancel", this,
                                        juce::ModalCallbackFunction::create ([doDelete] (int r) { if (r == 1) doDelete(); }));
}

void MainComponent::duplicateTrack (int src)
{
    if (engine.getNumTracks() >= AudioEngine::maxTracks) { setStatus ("Track limit reached"); return; }
    auto snap = std::make_shared<TrackSnap> (captureTrack (src));
    snap->name += " copy";
    snap->orderPos += 1;     // right below the original
    auto slot = std::make_shared<int> (-1);
    record ("Duplicate track",
            [this, slot, snap] { snap->slot = *slot; *slot = restoreTrack (*snap); syncRows(); },
            [this, slot] { engine.removeTrack (*slot); syncRows(); });
}

TrackRow* MainComponent::selectionRow()
{
    if (selSlot < 0 || selSlot >= rows.size() || ! engine.tracks[selSlot].active.load()) return nullptr;
    return rows[selSlot];
}

void MainComponent::doUndo (bool redo)
{
    const auto st = engine.getState();
    if (st == AudioEngine::Recording || st == AudioEngine::CountIn) { setStatus ("Stop recording first"); return; }
    if (redo ? ! undoManager.canRedo() : ! undoManager.canUndo()) return;
    const auto what = redo ? undoManager.getRedoDescription() : undoManager.getUndoDescription();
    if (redo) undoManager.redo(); else undoManager.undo();
    refreshStrips();
    setStatus ((redo ? "Redo: " : "Undo: ") + what);
}
void MainComponent::setStatus (const juce::String& s, int ticks)
{
    statusLabel.setText (s, juce::dontSendNotification);
    statusTicks = ticks;
}

// A finished take leaves nothing armed, so the next playback/overdub starts from a clean state.
void MainComponent::stopTransport()
{
    const auto st = engine.getState();
    engine.stop();
    if (st == AudioEngine::Recording || st == AudioEngine::CountIn) disarmAll();
}

void MainComponent::disarmAll()
{
    for (int slot : engine.order)
    {
        engine.tracks[slot].armed = false;
        rows[slot]->syncArm();
    }
}

void MainComponent::warnIfMultipleArmed()
{
    int n = 0;
    for (int slot : engine.order) n += engine.tracks[slot].armed.load() ? 1 : 0;
    if (n > 1) setStatus (juce::String (n) + " tracks armed - all will record", 150);
}

void MainComponent::toggleRecord()
{
    if (engine.getState() == AudioEngine::Recording) { engine.play(); disarmAll(); return; }   // stop recording, keep playing
    if (engine.getState() == AudioEngine::CountIn) { stopTransport(); return; }
    warnIfMultipleArmed();
    if (! engine.record()) setStatus ("Arm a track (R) before recording");
}

void MainComponent::togglePlay()
{
    if (engine.getState() != AudioEngine::Stopped) stopTransport();
    else engine.play();
}

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    const int code = k.getKeyCode();
    const bool ctrl = mods.isCtrlDown() || mods.isCommandDown();

    if (ctrl)
    {
        if (code == 'S') { saveProject (mods.isShiftDown()); return true; }
        if (code == 'O') { openProject(); return true; }
        if (code == 'N') { newProject(); return true; }
        if (code == 'T') { addTrack(); return true; }
        if (code == 'C' || code == 'X' || code == 'V')
        {
            if (auto* r = selectionRow())
                r->selectionCommand (code == 'C' ? TrackRow::SelCopy : code == 'X' ? TrackRow::SelCut : TrackRow::SelPaste);
            else setStatus ("Select a region first: drag in a waveform");
            return true;
        }
        if (code == 'D') { if (auto* r = selectionRow()) duplicateTrack (r->slot()); return true; }
        if (code == 'Z') { doUndo (mods.isShiftDown()); return true; }
        if (code == 'Y') { doUndo (true); return true; }
        if (code >= '1' && code <= '8')
        {
            const int n = code - '1';
            if (n < engine.getNumTracks())
            {
                auto& t = engine.tracks[engine.order[(size_t) n]];
                t.solo = ! t.solo.load();
                rows[engine.order[(size_t) n]]->refreshFromTrack();
            }
            return true;
        }
        return false;
    }

    if (k == juce::KeyPress::spaceKey) { togglePlay(); return true; }
    if (k == juce::KeyPress::returnKey || k == juce::KeyPress::homeKey)
    {
        if (engine.getState() == AudioEngine::Stopped) engine.setCursor (0);
        else if (engine.getState() == AudioEngine::Playing) { engine.stop(); engine.setCursor (0); }
        else stopTransport();
        return true;
    }
    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey)
    {
        if (auto* r = selectionRow()) r->selectionCommand (TrackRow::SelSilence);
        return true;
    }
    if (k == juce::KeyPress::escapeKey) { for (auto* r : rows) r->clearSelection(); return true; }
    if (code == 'R') { toggleRecord(); return true; }
    if (code == 'T') { tapTempo(); return true; }
    if (code == 'C') { clickBtn.setToggleState (! clickBtn.getToggleState(), juce::sendNotification); return true; }
    if (code == 'L' && engine.getState() == AudioEngine::Stopped) { loopBtn.setToggleState (! loopBtn.getToggleState(), juce::sendNotification); return true; }

    if (code >= '1' && code <= '8')    // 1-8 arm, Shift+1-8 mute
    {
        const int n = code - '1';
        if (n < engine.getNumTracks())
        {
            const int slot = engine.order[(size_t) n];
            auto& t = engine.tracks[slot];
            if (mods.isShiftDown()) t.mute = ! t.mute.load();
            else                    t.armed = ! t.armed.load();
            rows[slot]->refreshFromTrack();
            if (! mods.isShiftDown() && t.armed.load()) warnIfMultipleArmed();
        }
        return true;
    }
    return false;
}
void MainComponent::timerCallback()
{
    engine.pollRecording();
    // dark title bar on every top-level window we open (audio settings, alert boxes, ...)
    for (int i = 0; i < juce::TopLevelWindow::getNumTopLevelWindows(); ++i)
        if (auto* w = juce::TopLevelWindow::getTopLevelWindow (i))
            if (w->isOnDesktop() && ! w->getProperties().contains ("darkTitle"))
            {
                applyDarkTitleBar (w->getPeer(), Theme::bg, Theme::text);
                w->getProperties().set ("darkTitle", true);
            }
    undoBtn.setEnabled (undoManager.canUndo());
    redoBtn.setEnabled (undoManager.canRedo());
    undoBtn.setTooltip (undoManager.canUndo() ? "Undo " + undoManager.getUndoDescription() + " (Ctrl+Z)" : "Nothing to undo");
    redoBtn.setTooltip (undoManager.canRedo() ? "Redo " + undoManager.getRedoDescription() + " (Ctrl+Shift+Z)" : "Nothing to redo");

    const auto st = engine.getState();
    recBtn.setToggleState (st == AudioEngine::Recording || st == AudioEngine::CountIn, juce::dontSendNotification);
    if (recBtn.getToggleState()) recBtn.repaint();   // pulse animation
    playBtn.setToggleState (st == AudioEngine::Playing, juce::dontSendNotification);

    const bool idle = st == AudioEngine::Stopped;
    for (auto* c : std::initializer_list<juce::Component*> { &bpmLabel, &tapBtn, &loopBtn, &barsBox })
        c->setEnabled (idle);

    const double sr = juce::jmax (1.0, engine.getSampleRate());
    latLabel.setText ("Latency " + juce::String (engine.deviceLatency.load() * 1000.0 / sr, 1) + " ms  +  manual adjustment", juce::dontSendNotification);
    const double spb = engine.getSamplesPerBeat();
    const double secs = engine.getPosition() / sr;
    const int m = (int) (secs / 60.0);
    timeLabel.setText (juce::String::formatted ("%02d:%02d.%03d", m, (int) secs % 60, (int) ((secs - std::floor (secs)) * 1000.0)),
                       juce::dontSendNotification);

    if (st == AudioEngine::CountIn)
    {
        const int beat = (int) (engine.getCountInPosition() / spb) % 4 + 1;     // 1-2-3-4
        barLabel.setText ("Count " + juce::String (beat), juce::dontSendNotification);
        barLabel.setColour (juce::Label::textColourId, Theme::yellow);
    }
    else
    {
        barLabel.setText ({}, juce::dontSendNotification);   // no bar counter outside the count-in
    }

    // time axis: bars when looping, otherwise seconds in 30 s steps
    juce::int64 longest = engine.getPosition();
    for (int slot : engine.order)
    {
        longest = juce::jmax (longest, engine.tracks[slot].length.load());
        rows[slot]->tick();
    }
    double view, barSecs = 0.0;
    if (engine.loopEnabled.load())
    {
        view = engine.getLoopLength() / sr;
        barSecs = 4.0 * spb / sr;
    }
    else
        view = juce::jmax (30.0, std::ceil ((longest / sr + 1.0) / 30.0) * 30.0);
    for (auto* r : rows) r->setView (view, barSecs);
    ruler.setView (view, barSecs);
    ruler.repaint();
    master.tick();

    if (statusTicks > 0 && --statusTicks == 0) statusLabel.setText ({}, juce::dontSendNotification);
}
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);
    for (auto a : { barArea, settingsArea })
    {
        const auto f = a.toFloat();
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (f, 10.0f);
        g.setColour (Theme::border.withAlpha (0.55f));
        g.drawRoundedRectangle (f.reduced (0.5f), 10.0f, 1.0f);
    }
    g.setColour (Theme::raised);
    g.fillRoundedRectangle (bpmLabel.getBounds().toFloat(), 6.0f);
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (12);
    barArea = r.removeFromTop (64);
    auto bar = barArea.reduced (12, 10);
    titleLabel.setBounds (bar.removeFromLeft (120));
    bar.removeFromLeft (10);
    recBtn.setBounds (bar.removeFromLeft (44));  bar.removeFromLeft (8);
    playBtn.setBounds (bar.removeFromLeft (44)); bar.removeFromLeft (8);
    stopBtn.setBounds (bar.removeFromLeft (44)); bar.removeFromLeft (14);
    timeLabel.setBounds (bar.removeFromLeft (150));
    barLabel.setBounds (bar.removeFromLeft (140));

    audioBtn.setBounds (bar.removeFromRight (70)); bar.removeFromRight (6);
    saveBtn.setBounds (bar.removeFromRight (64));  bar.removeFromRight (6);
    openBtn.setBounds (bar.removeFromRight (64));  bar.removeFromRight (6);
    newBtn.setBounds (bar.removeFromRight (56));   bar.removeFromRight (12);
    exportBtn.setBounds (bar.removeFromRight (70)); bar.removeFromRight (12);
    projectLabel.setBounds (bar);   // whatever is left of the top bar
    r.removeFromTop (8);
    settingsArea = r.removeFromTop (46);
    auto sb = settingsArea.reduced (14, 8);
    bpmCaption.setBounds (sb.removeFromLeft (36));
    bpmLabel.setBounds (sb.removeFromLeft (64));  sb.removeFromLeft (6);
    tapBtn.setBounds (sb.removeFromLeft (48));    sb.removeFromLeft (18);
    loopBtn.setBounds (sb.removeFromLeft (56));   sb.removeFromLeft (6);
    barsBox.setBounds (sb.removeFromLeft (80));   sb.removeFromLeft (18);
    clickBtn.setBounds (sb.removeFromLeft (56));  sb.removeFromLeft (6);
    clickVol.setBounds (sb.removeFromLeft (90));  sb.removeFromLeft (18);
    countCaption.setBounds (sb.removeFromLeft (64));
    countInBox.setBounds (sb.removeFromLeft (70)); sb.removeFromLeft (18);
    redoBtn.setBounds (sb.removeFromRight (56)); sb.removeFromRight (6);
    undoBtn.setBounds (sb.removeFromRight (56)); sb.removeFromRight (10);
    statusLabel.setBounds (sb);

    r.removeFromTop (10);
    master.setBounds (r.removeFromBottom (56));
    r.removeFromBottom (10);

    auto rulerRow = r.removeFromTop (26);
    viewport.setBounds (r);
    const int w = viewport.getMaximumVisibleWidth();
    ruler.setBounds (rulerRow.withWidth (w));
    list.setSize (w, juce::jmax (list.contentHeight(), viewport.getHeight()));
}
// ------------------------------------------------------------- dialogs/projects

void MainComponent::showAudioSettings()
{
    if (audioDialog != nullptr) { audioDialog->toFront (true); return; }
    struct AudioPanel : juce::Component
    {
        AudioPanel (juce::AudioDeviceManager& dm, juce::Label& lab, juce::Slider& adj)
            : selector (dm, 0, 8, 0, 8, false, false, true, false), label (lab), adjust (adj)
        {
            addAndMakeVisible (selector);
            addAndMakeVisible (label);
            addAndMakeVisible (adjust);
            hint.setText ("Recordings are shifted earlier by the driver latency plus this value. Leave at 0 unless takes consistently land early/late.",
                          juce::dontSendNotification);
            hint.setColour (juce::Label::textColourId, Theme::dim);
            hint.setFont (juce::Font (juce::FontOptions (12.0f)));
            addAndMakeVisible (hint);
            setSize (540, 560);
        }
        void resized() override
        {
            auto r = getLocalBounds();
            auto bottom = r.removeFromBottom (84).reduced (14, 6);
            selector.setBounds (r);
            auto row = bottom.removeFromTop (28);
            label.setBounds (row.removeFromLeft (260));
            adjust.setBounds (row.removeFromLeft (100).withSizeKeepingCentre (100, 24));
            bottom.removeFromTop (4);
            hint.setBounds (bottom);
        }
        void paint (juce::Graphics& g) override
        {
            g.setColour (Theme::border);
            g.fillRect (14, getHeight() - 84, getWidth() - 28, 1);
        }
        juce::AudioDeviceSelectorComponent selector;
        juce::Label& label; juce::Slider& adjust; juce::Label hint;
    };
    auto* sel = new AudioPanel (deviceManager, latLabel, latAdjust);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (sel);
    o.dialogTitle = "Audio settings";
    o.dialogBackgroundColour = Theme::bg;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.componentToCentreAround = this;
    audioDialog = o.launchAsync();
}

void MainComponent::newProject()
{
    undoManager.clearUndoHistory();
    engine.removeAllTracks();
    engine.masterVolume = 1.0f;
    engine.bpm = 120.0; engine.loopBars = 4; engine.loopEnabled = true;
    engine.countInBars = 1; engine.clickEnabled = true; engine.clickVolume = 0.5f;
    engine.addTrack();
    engine.addTrack();
    projectDir = juce::File();
    refreshStrips();
}

void MainComponent::saveProject (bool saveAs)
{
    if (! saveAs && projectDir != juce::File()) { doSave (projectDir); return; }
    chooser = std::make_unique<juce::FileChooser> ("Save project (a folder with this name is created)",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("RiffCatch"),
                                                   "");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f != juce::File()) doSave (f.getParentDirectory().getChildFile (f.getFileNameWithoutExtension()));
                          });
}

void MainComponent::doSave (const juce::File& dir)
{
    juce::String err;
    if (ProjectIO::save (engine, dir, err))
    {
        projectDir = dir;
        projectLabel.setText (dir.getFileName(), juce::dontSendNotification);
        setStatus ("Saved to " + dir.getFullPathName());
    }
    else
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save failed", err);
}

void MainComponent::openProject()
{
    chooser = std::make_unique<juce::FileChooser> ("Open project folder",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("RiffCatch"));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f != juce::File()) doOpen (f);
                          });
}

void MainComponent::doOpen (const juce::File& dir)
{
    juce::String err;
    undoManager.clearUndoHistory();
    if (ProjectIO::load (engine, dir, err))
    {
        projectDir = dir;
        refreshStrips();
        setStatus ("Opened " + dir.getFileName());
    }
    else
    {
        refreshStrips();
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Open failed", err);
    }
}
