#include "MainComponent.h"
#include "project/ProjectIO.h"
#include "project/Exporter.h"
#include "WinTitleBar.h"
#include "project/AudioImport.h"

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
    r->onAnalyze = [this] (TrackRow* row) { analyzeTrack (row->slot()); };
        r->onStatus    = [this] (const juce::String& s) { setStatus (s); };
        r->onClipGrew  = [this] (juce::int64 end)
        {   // a duplicate past the loop end makes the loop longer, so the copy is heard
            const auto barLen = juce::jmax<juce::int64> (1, engine.getBarLength());
            const int bars = (int) ((end + barLen - 1) / barLen);
            if (engine.loopEnabled.load() && bars > engine.loopBars.load() && bars <= 64)
            {
                engine.loopBars = bars;
                barsBox.setSelectedId (bars, juce::dontSendNotification);
            }
        };
        r->onCropped   = [this] (juce::int64 len)
        {   // the cropped part becomes the loop: set the loop length to its number of bars
            const int bars = (int) std::lround ((double) len / (double) juce::jmax<juce::int64> (1, engine.getBarLength()));
            if (bars >= 1 && bars <= 64)
            {
                engine.loopBars = bars;
                barsBox.setSelectedId (bars, juce::dontSendNotification);
                setStatus ("Cropped to " + juce::String (bars) + (bars == 1 ? " bar" : " bars") + " and looping from bar 1");
            }
            zoom = 1.0; zoomStart = 0.0;
        };
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
    drumPanel.onUndoable = [this] (const juce::String& n, std::function<void()> a, std::function<void()> b) { record (n, std::move (a), std::move (b)); };
    drumPanel.onLayoutChanged = [this]
    {
        if (bassPanel.isExpanded() != drumPanel.isExpanded()) bassPanel.setExpanded (drumPanel.isExpanded());
        else resized();
        if (auto* f = props.getUserSettings()) { f->setValue ("drumsExpanded", drumPanel.isExpanded()); f->saveIfNeeded(); }
    };
    drumPanel.onReleaseFocus = [this] { grabKeyboardFocus(); };
    drumPanel.onInfo = [this] (const juce::String& s) { setStatus (s, 45); };
    addAndMakeVisible (drumPanel);
    drumPanel.onSelectTab = bassPanel.onSelectTab = [this] (int t) { showSeqTab (t); };
    bassPanel.onUndoable = [this] (const juce::String& n, std::function<void()> a, std::function<void()> b) { record (n, std::move (a), std::move (b)); };
    bassPanel.onLayoutChanged = [this]
    {
        if (drumPanel.isExpanded() != bassPanel.isExpanded()) drumPanel.setExpanded (bassPanel.isExpanded());
        else resized();
    };
    bassPanel.onReleaseFocus = [this] { grabKeyboardFocus(); };
    bassPanel.onInfo = [this] (const juce::String& s) { setStatus (s, 45); };
    bassPanel.onBake = [this] { bakeBass(); };
    bassPanel.onBakeLive = [this] { bakeBassLive(); };
    addChildComponent (bassPanel);
    addChildComponent (drumPanel.seqTabs());
    addChildComponent (bassPanel.seqTabs());
    addMouseListener (this, true);     // hover hints go to the info bar
    if (auto* f = props.getUserSettings()) drumPanel.setExpanded (f->getBoolValue ("drumsExpanded", true));
    if (auto* f = props.getUserSettings()) showSeqTab (f->getIntValue ("seqTab", 0));
    mixer.onUndoable = [this] (const juce::String& n, std::function<void()> a, std::function<void()> b) { record (n, std::move (a), std::move (b)); };
    mixer.onInfo = [this] (const juce::String& s) { setStatus (s, 45); };
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
    list.onEmptyClick = [this] { grabKeyboardFocus(); };
    list.onEmptyMenu = [this]
    {
        juce::PopupMenu m;
        const bool room = engine.getNumTracks() < AudioEngine::maxTracks;
        m.addItem (1, "Import audio to new track...", room);
        m.addItem (2, "Add new track", room);
        m.showMenuAsync (juce::PopupMenu::Options(), [this] (int id)
        {
            if (id == 1) importAudio();
            else if (id == 2) addTrack();
        });
    };
    viewport.setScrollBarsShown (false, false, true, false);   // wheel scrolling, no scrollbar that would narrow the rows
    addAndMakeVisible (viewport);
    addAndMakeVisible (mixer);
    addAndMakeVisible (ruler);
    addAndMakeVisible (patternTabs);
    setupPatternTabs();
    addChildComponent (songBar);
    songBar.onChanged = [this] { ++editCounter; };
    songBar.onOpenPattern = [this] (int i) { switchPattern (i); };
    songBtn.setClickingTogglesState (false);
    songBtn.setWantsKeyboardFocus (false);
    songBtn.setTooltip ("Song mode: Play plays the patterns of the song one after the other. Click again to go back to editing one pattern");
    songBtn.onClick = [this] { setSongMode (! engine.patterns.songMode); };
    addAndMakeVisible (songBtn);
    setupLibraryCounter();

    titleLabel.setText ("RiffCatch", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, Theme::blue);
    addAndMakeVisible (titleLabel);

    for (auto* b : { &recBtn, &playBtn, &stopBtn }) addAndMakeVisible (b);
    recBtn.setTooltip ("Record (R)");
    playBtn.setTooltip ("Play / Stop (Space)");
    stopBtn.setTooltip ("Stop and rewind (Enter)");

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
    addTrackBtn.onClick = [this] { addTrack(); };
    addAndMakeVisible (addTrackBtn);

    fileBtn.setWantsKeyboardFocus (false);

    fileBtn.setTooltip ("Menu: new, open, save, export, audio settings");
    fileBtn.onClick = [this] { fileMenu(); };
    addAndMakeVisible (fileBtn);

    timeLabel.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 20.0f, juce::Font::plain)));
    timeLabel.setBorderSize ({ 0, 0, 0, 0 });
    timeLabel.setJustificationType (juce::Justification::centred);
    timeLabel.setColour (juce::Label::textColourId, Theme::text.withMultipliedBrightness (0.72f));   // softer than pure white
    addChildComponent (timeLabel);                        // only holds the text; the clock below draws it in seven segments
    addAndMakeVisible (clock);

    projectLabel.setJustificationType (juce::Justification::centredRight);
    projectLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
    projectLabel.setColour (juce::Label::textColourId, Theme::dim);
    addAndMakeVisible (projectLabel);
    catchBtn.setWantsKeyboardFocus (false);
    catchBtn.setTooltip ("Save what you just played into a new track (Shift+C). The app always listens to the armed input.");
    catchBtn.onClick = [this] { catchNow(); grabKeyboardFocus(); };
    addAndMakeVisible (catchBtn);
    for (int s : { 15, 30, 60 }) catchLenBox.addItem ("Last " + juce::String (s) + " s", s);
    catchLenBox.setWantsKeyboardFocus (false);
    catchLenBox.setTooltip ("How far back Catch reaches");
    {
        int secs = 30;
        if (auto* f = props.getUserSettings()) secs = f->getIntValue ("catchSeconds", 30);
        catchLenBox.setSelectedId (secs == 15 || secs == 60 ? secs : 30, juce::dontSendNotification);
    }
    catchLenBox.onChange = [this]
    {
        if (auto* f = props.getUserSettings()) { f->setValue ("catchSeconds", catchLenBox.getSelectedId()); f->saveIfNeeded(); }
        grabKeyboardFocus();
    };
    addAndMakeVisible (catchLenBox);

    looperBtn.setClickingTogglesState (true);
    looperBtn.setWantsKeyboardFocus (false);
    looperBtn.setTooltip ("Looper mode: Shift+Space (or a MIDI foot switch) records the first loop, closes it, then adds a new track for each overdub");
    looperBtn.onClick = [this]
    {
        if (looperBtn.getToggleState()) setStatus ("Looper: Shift+Space or your pedal. First press records, second press closes the loop", 200);
        else looperReset ("Looper off");
        grabKeyboardFocus();
    };
    addAndMakeVisible (looperBtn);
    if (auto* f = props.getUserSettings()) mixer.setTunerRef (f->getIntValue ("tunerRef", 440));
    mixer.setOnTunerRefChanged ([this] (int hz)
    {
        if (auto* f = props.getUserSettings()) { f->setValue ("tunerRef", hz); f->saveIfNeeded(); }
    });
    recent.setMaxNumberOfItems (10);
    if (auto* f = props.getUserSettings()) recent.restoreFromString (f->getValue ("recentProjects"));
    pedal.onTrigger = [this] { looperPress(); };
    pedal.onLearned = [this] (const juce::String& d)
    {
        if (auto* f = props.getUserSettings()) { f->setValue ("pedalType", pedal.getType()); f->setValue ("pedalNumber", pedal.getNumber()); f->saveIfNeeded(); }
        setStatus ("Pedal learned: " + d, 200);
    };
    if (auto* f = props.getUserSettings()) pedal.set (f->getIntValue ("pedalType", 0), f->getIntValue ("pedalNumber", -1));
    pedal.open();

    folderBtn.onClick = [this] { openCurrentFolder(); grabKeyboardFocus(); };
    addAndMakeVisible (folderBtn);
    refreshFolderTip();
    notesBtn.setClickingTogglesState (false);
    notesBtn.setWantsKeyboardFocus (false);
    notesBtn.setTooltip ("Project notes (a dot of colour means there are notes)");
    notesBtn.onClick = [this] { editNotes(); };
    addAndMakeVisible (notesBtn);

    statusLabel.setColour (juce::Label::textColourId, Theme::yellow);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (statusLabel);

    // ---- tempo / loop / click / count-in controls
    bpmCaption.setText ("BPM", juce::dontSendNotification);
    countCaption.setText ("Count-in", juce::dontSendNotification);
    for (auto* l : { &bpmCaption, &countCaption }) { l->setColour (juce::Label::textColourId, Theme::dim); addAndMakeVisible (l); }

    bpmLabel.setEditable (true);
    bpmLabel.setJustificationType (juce::Justification::centred);
    bpmLabel.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 20.0f, juce::Font::bold)));
    bpmLabel.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    bpmLabel.setColour (juce::Label::textColourId, Theme::accent);
    bpmLabel.setTooltip ("Tempo (double-click to type, 30-300)");
    bpmLabel.onTextChange = [this] { setBpm (bpmLabel.getText().getDoubleValue()); grabKeyboardFocus(); };
    addAndMakeVisible (bpmLabel);

    for (auto* b : { &undoBtn, &redoBtn, &cutBtn, &copyBtn, &pasteBtn, &silenceBtn, &fadeInBtn, &fadeOutBtn, &dupBtn, &reverseBtn, &selectAllBtn, &cropBtn })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    auto selCmd = [this] (TrackRow::SelCmd c)
    {
        if (auto* r = selectionRow()) r->selectionCommand (c);
        else setStatus ("Select a region first: drag in a waveform");
        grabKeyboardFocus();
    };
    cutBtn.setTooltip ("Cut selection (Ctrl+X)");               cutBtn.onClick = [selCmd] { selCmd (TrackRow::SelCut); };
    copyBtn.setTooltip ("Copy selection (Ctrl+C)");             copyBtn.onClick = [selCmd] { selCmd (TrackRow::SelCopy); };
    pasteBtn.setTooltip ("Paste at selection / playhead (Ctrl+V)"); pasteBtn.onClick = [selCmd] { selCmd (TrackRow::SelPaste); };
    silenceBtn.setTooltip ("Silence selection (Del)");          silenceBtn.onClick = [selCmd] { selCmd (TrackRow::SelSilence); };
    fadeInBtn.setTooltip ("Fade in over selection");            fadeInBtn.onClick = [selCmd] { selCmd (TrackRow::SelFadeIn); };
    fadeOutBtn.setTooltip ("Fade out over selection");
    reverseBtn.setTooltip ("Reverse selection (whole clip if nothing is selected)");
    reverseBtn.onClick = [selCmd] { selCmd (TrackRow::SelReverse); };
    cropBtn.setTooltip ("Crop: keep only the selection and move it to bar 1 (Alt+drag selects whole bars)");
    cropBtn.onClick = [this]
    {
        if (auto* r = selectionRow()) r->cropToSelection(); else setStatus ("Select the part to keep first: drag in a waveform");
        grabKeyboardFocus();
    };
    selectAllBtn.setTooltip ("Select the whole clip (Ctrl+A)");
    selectAllBtn.onClick = [this]
    {
        if (auto* r = selectionRow()) r->selectAll(); else setStatus ("Click a track first");
    };          fadeOutBtn.onClick = [selCmd] { selCmd (TrackRow::SelFadeOut); };
    dupBtn.setTooltip ("Duplicate track (Ctrl+D)");             dupBtn.onClick = [this] { if (auto* r = selectionRow()) duplicateTrack (r->slot()); grabKeyboardFocus(); };
    undoBtn.onClick = [this] { doUndo (false); };
    redoBtn.onClick = [this] { doUndo (true); };

    tapBtn.setTooltip ("Tap tempo (T)");
    tapBtn.setWantsKeyboardFocus (false);
    tapBtn.onClick = [this] { tapTempo(); };
    addAndMakeVisible (tapBtn);

    loopBtn.setClickingTogglesState (true);
    addAndMakeVisible (loopBtn);
    for (auto* b : { &skipStartBtn, &skipEndBtn }) addAndMakeVisible (b);
    skipStartBtn.setTooltip ("Go to the start");
    skipEndBtn.setTooltip ("Go to the end of the loop");
    skipStartBtn.onClick = [this] { engine.seek (0); zoomStart = 0.0; };
    skipEndBtn.onClick = [this]
    {
        juce::int64 end = engine.getLoopLength() - 1;
        if (! engine.loopEnabled.load())
        {
            end = 0;
            for (int slot : engine.order) end = juce::jmax<juce::int64> (end, engine.tracks[slot].length.load() + engine.tracks[slot].offset.load());
        }
        engine.seek (juce::jmax<juce::int64> (0, end));
    };
    for (auto* b : { &zoomInBtn, &zoomOutBtn, &zoomFitBtn }) addAndMakeVisible (b);
    zoomInBtn.setTooltip ("Zoom in (Ctrl + mouse wheel)");
    zoomOutBtn.setTooltip ("Zoom out (Ctrl + mouse wheel)");
    zoomFitBtn.setTooltip ("Show the whole loop. Shift + wheel pans when zoomed");
    zoomInBtn.onClick = [this] { zoomAt (0.5, 1.6); };
    zoomOutBtn.onClick = [this] { zoomAt (0.5, 1.0 / 1.6); };
    zoomFitBtn.onClick = [this] { zoom = 1.0; zoomStart = 0.0; };
    viewport.onZoomWheel = [this] (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) { handleWheel (e, w, false); };
    ruler.onWheel = [this] (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) { handleWheel (e, w, true); };
    drumPanel.onWheel = bassPanel.onWheel = [this] (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) { handleWheel (e, w, false); };
    for (auto* b : { &clickBtn })
    {
        b->setClickingTogglesState (true);
        b->setWantsKeyboardFocus (false);
        b->setColour (juce::TextButton::buttonOnColourId, Theme::accent);
        addAndMakeVisible (b);
    }
    loopBtn.setTooltip ("Loop the project region while playing/recording (L)");
    clickBtn.setTooltip ("Metronome during play/record (C). Count-in always clicks.");
    loopBtn.onClick  = [this] { engine.loopEnabled = loopBtn.getToggleState(); };
    clickBtn.onClick = [this] { engine.clickEnabled = clickBtn.getToggleState(); };

    punchBtn.setClickingTogglesState (true);
    punchBtn.setWantsKeyboardFocus (false);
    punchBtn.setColour (juce::TextButton::buttonOnColourId, Theme::red);
    punchBtn.setColour (juce::TextButton::textColourOnId, Theme::deep);
    punchBtn.setTooltip ("Punch in/out: select a region in a waveform, arm the track and record - only that region is replaced (loop mode)");
    punchBtn.onClick = [this]
    {
        if (! punchBtn.getToggleState()) { engine.punchEnabled = false; ruler.repaint(); grabKeyboardFocus(); return; }
        auto* r = selectionRow();
        if (r == nullptr) { punchBtn.setToggleState (false, juce::dontSendNotification); setStatus ("Punch: select a region in a waveform first"); grabKeyboardFocus(); return; }
        const auto off = (juce::int64) r->clipOffset();
        engine.punchStart = off + r->selectionStart();
        engine.punchEnd = off + r->selectionEnd();
        engine.punchEnabled = true;
        if (! engine.loopEnabled.load()) setStatus ("Punch works in loop mode - turn Loop on");
        else setStatus ("Punch region set - arm a track and record");
        ruler.repaint();
        grabKeyboardFocus();
    };
    addAndMakeVisible (punchBtn);

    for (int b : { 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 24, 32, 48, 64 }) barsBox.addItem (juce::String (b) + (b == 1 ? " bar" : " bars"), b);
    barsBox.setWantsKeyboardFocus (false);
    barsBox.setTooltip ("Loop length in bars");
    {
        static const char* const sigs[] = { "2/4", "3/4", "4/4", "5/4", "6/8", "7/8" };
        for (int i = 0; i < 6; ++i) timeSigBox.addItem (sigs[i], i + 1);
        timeSigBox.setWantsKeyboardFocus (false);
        timeSigBox.setTooltip ("Time signature. BPM counts quarter notes");
        timeSigBox.onChange = [this]
        {
            static const int nums[] = { 2, 3, 4, 5, 6, 7 }, dens[] = { 4, 4, 4, 4, 8, 8 };
            const int i = juce::jlimit (1, 6, timeSigBox.getSelectedId()) - 1;
            engine.setTimeSignature (nums[i], dens[i]);
            ++editCounter;
            drumPanel.refreshTemplates();
            drumPanel.repaint();
        bassPanel.repaint();
            grabKeyboardFocus();
        };
        addAndMakeVisible (timeSigBox);
    }
    barsBox.onChange = [this] { engine.loopBars = barsBox.getSelectedId(); grabKeyboardFocus(); };
    addAndMakeVisible (barsBox);

    clickVol.setSliderStyle (juce::Slider::LinearHorizontal);
    clickVol.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    clickVol.setRange (0.0, 1.0, 0.01);
    clickVol.setColour (juce::Slider::trackColourId, Theme::accent);
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
    latAdjust.setColour (juce::Slider::trackColourId, Theme::accent.withAlpha (0.35f));
    latAdjust.setColour (juce::Slider::textBoxTextColourId, Theme::text);
    latAdjust.setColour (juce::Slider::textBoxOutlineColourId, Theme::border);
    latAdjust.setWantsKeyboardFocus (false);
    latAdjust.setTooltip ("Extra latency compensation in ms (fine-tune until recordings sit on the click)");
    latAdjust.onValueChange = [this] { saveLatencyAdjust(); };
    if (auto* f = props.getUserSettings()) latAdjust.setValue (f->getDoubleValue ("latencyAdjustMs", 0.0), juce::sendNotification);

    barLabel.setJustificationType (juce::Justification::centredLeft);
    barLabel.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::plain)));
    barLabel.setColour (juce::Label::textColourId, Theme::accent);
    addAndMakeVisible (barLabel);

    engine.addTrack();
    engine.addTrack();
    changeListenerCallback (nullptr);
    refreshStrips();
    setWantsKeyboardFocus (true);
    {
        const auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
        setSize (juce::jmin (1400, area.getWidth() - 40), juce::jmin (940, area.getHeight() - 60));
    }
    startTimerHz (30);
    {   // a .riffcatch file passed on the command line (double-click in Explorer) is opened at start
        const auto arg = juce::JUCEApplication::getCommandLineParameters().removeCharacters ("\"").trim();
        if (arg.isNotEmpty() && ! arg.startsWith ("--") && ProjectIO::isProjectFile (juce::File (arg)) && juce::File (arg).existsAsFile()) doOpen (juce::File (arg));
        else juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainComponent> (this)] { if (safe != nullptr) safe->offerRestore(); });
    }
}

MainComponent::~MainComponent()
{
    stopTimer();
    autosaveDir().deleteRecursively();
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
    patternTabs.refresh();
    songBar.repaint();
    mixer.refreshFromEngine();
    drumPanel.refreshFromEngine();
    bassPanel.refreshFromEngine();
    updateSettingsFromEngine();
    projectLabel.setText (projectFile == juce::File() ? "Untitled" : projectFile.getFileNameWithoutExtension(), juce::dontSendNotification);
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
    songBtn.setToggleState (engine.patterns.songMode, juce::dontSendNotification);
    barsBox.setSelectedId (engine.loopBars.load(), juce::dontSendNotification);
    syncTimeSigBox();
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

void MainComponent::fileMenu()
{
    juce::PopupMenu m;
    auto addKey = [&m] (int id, const juce::String& text, const juce::String& key)
    {
        juce::PopupMenu::Item it (text); it.itemID = id; it.shortcutKeyDescription = key; m.addItem (it);
    };
    addKey (1, "New project", "Ctrl+N");
    addKey (2, "Open...", "Ctrl+O");
    {
        recent.removeNonExistentFiles();
        juce::PopupMenu rm;
        int shown = 0;
        for (int i = 0; i < recent.getNumFiles() && shown < 10; ++i)
        {
            const auto f = recent.getFile (i);
            if (! f.existsAsFile() || ! ProjectIO::isProjectFile (f)) continue;      // folders from before v0.47 are skipped
            rm.addItem (700 + i, f.getFileNameWithoutExtension() + "    (" + f.getParentDirectory().getFullPathName() + ")");
            ++shown;
        }
        if (shown == 0) rm.addItem (699, "No recent projects", false);
        else { rm.addSeparator(); rm.addItem (698, "Clear list"); }
        m.addSubMenu ("Open recent", rm);
    }
    m.addSeparator();
    addKey (3, "Save", "Ctrl+S");
    addKey (4, "Save as...", "Ctrl+Shift+S");
    m.addSeparator();
    m.addItem (5, "Export...");
    m.addSeparator();
    m.addItem (7, "Import audio...");
    m.addItem (6, "Audio settings...");
    m.addItem (9, "Work folder...  (" + workDir().getFileName() + ")");
    m.addItem (8, "Open .riffcatch files with RiffCatch (register)");
    m.addSeparator();
    juce::PopupMenu themes;
    for (int i = 0; i < Theme::numPresets(); ++i)
    {
        if (i == Theme::builtinCount) themes.addSeparator();
        themes.addItem (300 + i, Theme::presets[(size_t) i].name, true, i == Theme::current);
    }
    themes.addSeparator();
    themes.addItem (290, "Custom theme...");
    if (Theme::numPresets() > Theme::builtinCount)
    {
        juce::PopupMenu del;
        for (int i = Theme::builtinCount; i < Theme::numPresets(); ++i) del.addItem (500 + i, Theme::presets[(size_t) i].name);
        themes.addSubMenu ("Delete custom theme", del);
    }
    m.addSubMenu ("Theme", themes);
    {
        juce::PopupMenu pm;
        pm.addItem (600, pedal.isLearning() ? "Press the pedal now..." : "Learn MIDI foot switch...");
        pm.addItem (601, "Forget pedal (now: " + pedal.describe() + ")", pedal.isSet());
        m.addSubMenu ("Looper pedal", pm);
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fileBtn), [this] (int id)
    {
        switch (id)
        {
            case 1: newProject(); break;
            case 2: openProject(); break;
            case 3: saveProject (false); break;
            case 4: saveProject (true); break;
            case 5: exportMenu(); break;
            case 6: showAudioSettings(); break;
            case 7: importAudio(); break;
            case 9: chooseWorkDir(); break;
            case 8: setStatus (FileAssoc::registerForUser() ? ".riffcatch files now open with this RiffCatch" : "Could not register the file type", 200); break;
            case 290: openThemeEditor(); break;
            case 698:
                recent.clear();
                if (auto* f = props.getUserSettings()) { f->setValue ("recentProjects", recent.toString()); f->saveIfNeeded(); }
                break;
            case 600:
                pedal.startLearning();
                setStatus (pedal.getNumInputs() > 0 ? "Press your MIDI foot switch now (note or controller)..." : "No MIDI input found - connect the pedal and try again", 300);
                break;
            case 601:
                pedal.forget();
                if (auto* f = props.getUserSettings()) { f->setValue ("pedalType", 0); f->setValue ("pedalNumber", -1); f->saveIfNeeded(); }
                setStatus ("Pedal forgotten");
                break;
            default:
                if (id >= 700 && id < 720) { const auto f = recent.getFile (id - 700); if (f != juce::File()) doOpen (f); }
                else if (id >= 300 && id < 300 + Theme::numPresets()) applyTheme (id - 300);
                else if (id >= 500 && id < 500 + Theme::numPresets())
                {
                    const int idx = id - 500, cur = Theme::current;
                    const auto name = Theme::presets[(size_t) idx].name;
                    ThemeStore::remove (idx);
                    applyTheme (cur == idx ? 3 : ThemeStore::indexOf (Theme::presets[(size_t) juce::jlimit (0, Theme::numPresets() - 1, cur)].name));
                    setStatus ("Deleted theme: " + name, 60);
                }
                break;
        }
        grabKeyboardFocus();
    });
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
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fileBtn), [this] (int id)
    {
        if (id <= 0) return;
        const int kind = id / 100, loops = id % 100;
        const bool mp3 = kind > 1;
        const juce::String ext = mp3 ? "mp3" : "wav";
        const auto base = projectFile == juce::File() ? juce::String ("RiffCatch") : projectFile.getFileNameWithoutExtension();
        chooser = std::make_unique<juce::FileChooser> (mp3 ? "Export MP3" : "Export WAV",
            workDir().getChildFile (base + "." + ext), "*." + ext);
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
    ++editCounter;
    undoManager.beginNewTransaction (name);
    undoManager.perform (new LambdaAction (std::move (redo), std::move (undo)));
}

// ---- autosave: a copy of the work in progress is written every minute (only when something changed and nothing is recording).
// It is removed on a normal exit and after a normal save, so a leftover copy means the last session ended unexpectedly.
juce::File MainComponent::autosaveDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("RiffCatch").getChildFile ("autosave");
}

void MainComponent::autosaveTick()
{
    if (++autosaveTicks < 30 * 60) return;
    autosaveTicks = 0;
    if (engine.getState() == AudioEngine::Recording || engine.getState() == AudioEngine::CountIn) return;
    juce::int64 sig = (juce::int64) editCounter * 1000003 + (juce::int64) (engine.bpm.load() * 100.0);
    bool any = engine.drums.hasAnySteps() || engine.bass.hasAnySteps();
    for (int slot : engine.order) { const auto len = engine.tracks[slot].length.load(); sig += len; any = any || len > 0; }
    if (! any || sig == lastAutosaveSig) return;
    juce::String err;
    if (ProjectIO::save (engine, autosaveFile(), err)) lastAutosaveSig = sig;
}

void MainComponent::offerRestore()
{
    const auto file = autosaveFile();
    if (! file.existsAsFile()) return;
    auto opts = juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::QuestionIcon, "Restore unsaved work?",
                    "RiffCatch did not close normally last time. Restore the automatic backup?", "Restore", "Discard");
    juce::AlertWindow::showAsync (opts, [this, file] (int result)
    {
        if (result == 1)
        {
            doOpen (file);
            projectFile = juce::File();
            projectLabel.setText ("restored", juce::dontSendNotification);
            setStatus ("Restored the automatic backup - save it to keep it");
        }
        else
            autosaveDir().deleteRecursively();
    });
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
    s.mute = t.mute.load(); s.solo = t.solo.load(); s.input = t.inputCode.load(); s.offset = t.offset.load(); s.echoOn = t.echoOn.load(); s.reverbOn = t.reverbOn.load(); s.echoDiv = t.echoDiv.load();
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
    t.inputCode = s.input; t.offset = s.offset; t.echoOn = s.echoOn; t.reverbOn = s.reverbOn; t.echoDiv = s.echoDiv;
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
    if (engine.patterns.songMode) { setStatus ("Song mode plays the whole song. Turn Song off to record"); return; }
    if (engine.getState() == AudioEngine::Recording) { engine.play(); disarmAll(); return; }   // stop recording, keep playing
    if (engine.getState() == AudioEngine::CountIn) { stopTransport(); return; }
    warnIfMultipleArmed();
    if (! engine.record()) setStatus ("Arm a track (R) before recording");
    else if (engine.getRecordStart() > 0)
        setStatus ("Recording from bar " + juce::String ((int) (engine.getRecordStart() / juce::jmax<juce::int64> (1, engine.getBarLength())) + 1), 150);
}

void MainComponent::togglePlay()
{
    if (engine.getState() != AudioEngine::Stopped) { stopTransport(); return; }
    if (engine.patterns.songMode) { if (! startSong()) return; }
    else engine.clearSong();
    engine.play();
}

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    const int rawCode = k.getKeyCode();
    // JUCE reports letters in either case depending on modifiers/path; normalise to upper case.
    int code = (rawCode >= 'a' && rawCode <= 'z') ? rawCode - 32 : rawCode;
    const bool ctrl = mods.isCtrlDown() || mods.isCommandDown();
    if (ctrl && code >= 1 && code <= 26) code += 64;   // Windows delivers Ctrl+letter as a control character (Ctrl+T = 20)

    if (ctrl)
    {
        if (mods.isShiftDown() && (code == 'O' || code == 'P'))     // previous / next theme
        {
            applyTheme ((Theme::current + (code == 'P' ? 1 : Theme::numPresets() - 1)) % Theme::numPresets());
            return true;
        }
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
        if (code == 'A') { if (auto* r = selectionRow()) r->selectAll(); else setStatus ("Click a track first"); return true; }
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

    if (code == 32 && mods.isShiftDown()) { looperPress(); return true; }
    if (code == 'C' && mods.isShiftDown()) { catchNow(); return true; }
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
// Colours set per instance (labels, accent buttons) are re-applied whenever the theme changes.
void MainComponent::restyle()
{
    titleLabel.setColour (juce::Label::textColourId, Theme::blue);
    timeLabel.setColour (juce::Label::textColourId, Theme::text.withMultipliedBrightness (0.72f));
    clock.setColour (Theme::accent);
    projectLabel.setColour (juce::Label::textColourId, Theme::dim);
    for (auto* l : { &bpmCaption, &countCaption }) l->setColour (juce::Label::textColourId, Theme::dim);
    bpmLabel.setColour (juce::Label::textColourId, Theme::accent);
    barLabel.setColour (juce::Label::textColourId, Theme::accent);
    clickBtn.setColour (juce::TextButton::buttonOnColourId, Theme::accent);
    clickBtn.setColour (juce::TextButton::textColourOnId, Theme::onAccent);
    clickVol.setColour (juce::Slider::trackColourId, Theme::accent);
    recBtn.setIconColour (Theme::red);
    playBtn.setIconColour (Theme::green);
    stopBtn.setIconColour (Theme::text);
    skipStartBtn.setIconColour (Theme::text);
    skipEndBtn.setIconColour (Theme::text);
    loopBtn.setIconColour (Theme::accent);
    repaint();
}

void MainComponent::lookAndFeelChanged() { restyle(); }

void MainComponent::applyTheme (int index)
{
    Theme::apply (index);
    refreshTheme();
    if (auto* f = props.getUserSettings())
    {
        f->setValue ("theme", Theme::current);
        f->setValue ("themeName", Theme::presets[(size_t) Theme::current].name);
        f->saveIfNeeded();
    }
    setStatus (juce::String ("Theme: ") + Theme::presets[(size_t) Theme::current].name, 60);
}

void MainComponent::refreshTheme()
{
    if (auto* laf = dynamic_cast<RiffLookAndFeel*> (&getLookAndFeel())) laf->applyColours();
    if (auto* top = getTopLevelComponent()) top->sendLookAndFeelChange(); else sendLookAndFeelChange();
    if (auto* peer = getPeer()) applyDarkTitleBar (peer, Theme::bg, Theme::text);
}

void MainComponent::openThemeEditor()
{
    const int before = Theme::current;
    auto* ed = new ThemeEditor (Theme::blue, Theme::cyan, Theme::bg.getPerceivedBrightness() < 0.5f);
    juce::Component::SafePointer<MainComponent> self (this);
    ed->onDiscard = [self, before] { if (self != nullptr) self->applyTheme (before); };
    ed->onPreview = [this] (const Theme::Preset& p) { Theme::applyPreset (p, false); refreshTheme(); };

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (ed);
    o.dialogTitle = "Custom theme";
    o.dialogBackgroundColour = Theme::bg;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    auto* win = o.launchAsync();
    juce::Component::SafePointer<juce::DialogWindow> sp (win);
    ed->onCancel = [sp] { if (sp != nullptr) sp->exitModalState (0); };
    ed->onSave = [self, sp] (const Theme::Preset& p)
    {
        ThemeStore::save (p);
        ThemeStore::reload();
        const int idx = ThemeStore::indexOf (p.name);
        if (self != nullptr) self->applyTheme (idx >= 0 ? idx : 3);
        if (sp != nullptr) sp->exitModalState (1);
    };
}

// The info bar (the yellow field at the top right) also shows hover hints instead of pop-up tooltips.
void MainComponent::mouseEnter (const juce::MouseEvent& e)
{
    juce::String tip;
    if (auto* tc = dynamic_cast<juce::TooltipClient*> (e.eventComponent)) tip = tc->getTooltip();
    if (tip.isNotEmpty()) { statusLabel.setText (tip, juce::dontSendNotification); statusTicks = 0; hoverTip = true; }
    else if (hoverTip) { statusLabel.setText ({}, juce::dontSendNotification); hoverTip = false; }
}

void MainComponent::mouseExit (const juce::MouseEvent&)
{
    if (hoverTip) { statusLabel.setText ({}, juce::dontSendNotification); hoverTip = false; }
}

void MainComponent::clampZoom()
{
    zoom = juce::jlimit (1.0, 64.0, zoom);
    zoomStart = juce::jlimit (0.0, juce::jmax (0.0, fullView - fullView / zoom), zoomStart);
}

void MainComponent::zoomAt (double frac, double factor)
{
    frac = juce::jlimit (0.0, 1.0, frac);
    const double anchor = zoomStart + frac * fullView / zoom;
    zoom = juce::jlimit (1.0, 64.0, zoom * factor);
    zoomStart = anchor - frac * fullView / zoom;
    clampZoom();
}

void MainComponent::handleWheel (const juce::MouseEvent& e, const juce::MouseWheelDetails& w, bool plainZooms)
{
    const double laneW = juce::jmax (1.0, (double) (viewport.getMaximumVisibleWidth() - TrackRow::waveLeft - TrackRow::waveRightInset));
    const double frac = juce::jlimit (0.0, 1.0, (e.x - TrackRow::waveLeft) / laneW);
    const double d = w.deltaY != 0.0f ? w.deltaY : w.deltaX;
    if (d == 0.0) return;
    if (e.mods.isCtrlDown() || (plainZooms && ! e.mods.isShiftDown()))
        zoomAt (frac, d > 0.0 ? 1.3 : 1.0 / 1.3);
    else
    {
        zoomStart -= d * 0.6 * fullView / zoom;      // pan
        clampZoom();
    }
}

void MainComponent::timerCallback()
{
    engine.pollRecording();
    {
        const int cs = engine.bassCaptureState();
        if (cs != lastCapState)
        {
            lastCapState = cs;
            bassPanel.setLiveState (cs == 3 ? 0 : cs);
            if (cs == 2) setStatus ("Recording the bass for one loop pass - turn the knobs now", 400);
        }
        if (cs == 3)
        {
            auto snap = std::make_shared<TrackSnap>();
            if (engine.takeBassCapture (snap->audio))
            {
                snap->valid = true; snap->slot = -1; snap->orderPos = 1000;
                snap->name = "Bass live";
                snap->echoDiv = (int) TrackFx::Quarter;
                addBassTrack (snap, "Record bass live");
                setStatus ("Recorded the bass pass into a new track. The synth is muted so it does not play twice", 250);
            }
        }
    }
    engine.updateCaptureChannels();
    looperTick();
    autosaveTick();
    // dark title bar on every top-level window we open (audio settings, alert boxes, ...)
    for (int i = 0; i < juce::TopLevelWindow::getNumTopLevelWindows(); ++i)
        if (auto* w = juce::TopLevelWindow::getTopLevelWindow (i))
            if (w->isOnDesktop() && ! w->getProperties().contains ("darkTitle"))
            {
                applyDarkTitleBar (w->getPeer(), Theme::bg, Theme::text);
                w->getProperties().set ("darkTitle", true);
            }
    {
        bool anySel = false;
        for (auto* r : rows) anySel = anySel || r->hasSelection();
        const bool idle = engine.getState() != AudioEngine::Recording && engine.getState() != AudioEngine::CountIn;
        for (auto* b : { &cutBtn, &copyBtn, &silenceBtn, &fadeInBtn, &fadeOutBtn }) b->setEnabled (anySel && idle);
    for (auto* b : { &reverseBtn, &selectAllBtn, &cropBtn }) b->setEnabled (idle && selectionRow() != nullptr);
        pasteBtn.setEnabled (TrackRow::hasClipboard() && selectionRow() != nullptr && idle);
        dupBtn.setEnabled (selectionRow() != nullptr);
    }
    addTrackBtn.setEnabled (engine.getNumTracks() < AudioEngine::maxTracks);
    undoBtn.setEnabled (undoManager.canUndo());
    redoBtn.setEnabled (undoManager.canRedo());
    undoBtn.setTooltip (undoManager.canUndo() ? "Undo " + undoManager.getUndoDescription() + " (Ctrl+Z)" : "Nothing to undo");
    redoBtn.setTooltip (undoManager.canRedo() ? "Redo " + undoManager.getRedoDescription() + " (Ctrl+Shift+Z)" : "Nothing to redo");

    const auto st = engine.getState();
    recBtn.setToggleState (st == AudioEngine::Recording || st == AudioEngine::CountIn, juce::dontSendNotification);
    if (recBtn.getToggleState()) recBtn.repaint();   // pulse animation
    playBtn.setToggleState (st == AudioEngine::Playing, juce::dontSendNotification);

    const bool idle = st == AudioEngine::Stopped;
    for (auto* c : std::initializer_list<juce::Component*> { &bpmLabel, &tapBtn, &loopBtn, &barsBox, &timeSigBox })
        c->setEnabled (idle);

    const double sr = juce::jmax (1.0, engine.getSampleRate());
    latLabel.setText ("Latency " + juce::String (engine.deviceLatency.load() * 1000.0 / sr, 1) + " ms  +  manual adjustment", juce::dontSendNotification);
    const double spb = engine.getSamplesPerBeat();
    const double secs = engine.getPosition() / sr;
    const int m = (int) (secs / 60.0);
    timeLabel.setText (juce::String::formatted ("%02d:%02d.%03d", m, (int) secs % 60, (int) ((secs - std::floor (secs)) * 1000.0)),
                       juce::dontSendNotification);
    clock.setText (timeLabel.getText());

    if (st == AudioEngine::CountIn)
    {
        const int beat = (int) (engine.getCountInPosition() / engine.getPulseSamples()) % engine.getPulsesPerBar() + 1;     // 1-2-3-4
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
        // the whole project is visible, also a clip that is longer than the loop (e.g. a freshly imported file)
        juce::int64 clipEnd = 0;
        for (int slot : engine.order) clipEnd = juce::jmax (clipEnd, engine.tracks[slot].length.load() + juce::jmax (0, engine.tracks[slot].offset.load()));
        const auto barLen = juce::jmax<juce::int64> (1, engine.getBarLength());
        view = (double) juce::jmax (engine.getLoopLength(), ((clipEnd + barLen - 1) / barLen) * barLen) / sr;
        barSecs = engine.getBarBeats() * spb / sr;
    }
    else
        view = juce::jmax (30.0, std::ceil ((longest / sr + 1.0) / 30.0) * 30.0);
    fullView = view;
    if (zoom > 1.0 && st != AudioEngine::Stopped)   // follow the playhead while zoomed in
    {
        const double ph = engine.getPosition() / sr, vis0 = fullView / zoom;
        if (ph < zoomStart || ph > zoomStart + vis0) zoomStart = ph - 0.05 * vis0;
    }
    clampZoom();
    const double vis = fullView / zoom;
    for (auto* r : rows) r->setView (vis, barSecs, zoomStart);
    ruler.setView (vis, barSecs, zoomStart);
    drumPanel.setView (vis, barSecs, zoomStart, view);
    bassPanel.setView (vis, barSecs, zoomStart, view);
    if (vis != lastVis || zoomStart != lastStart || fullView != lastFull)
    {
        lastVis = vis; lastStart = zoomStart; lastFull = fullView;
        for (auto* r : rows) r->repaint();
        drumPanel.repaint();
        bassPanel.repaint();
    }
    ruler.repaint();
    mixer.tick();
    songBar.setPlayhead (engine.patterns.songMode && engine.songLoaded() && engine.getState() == AudioEngine::Playing ? engine.getPosition() : -1);
    drumPanel.tick();
    bassPanel.tick();

    if (statusTicks > 0 && --statusTicks == 0) statusLabel.setText ({}, juce::dontSendNotification);
}
void MainComponent::showSeqTab (int tab)
{
    seqTab = tab == 1 ? 1 : 0;
    drumPanel.setVisible (seqTab == 0);
    bassPanel.setVisible (seqTab == 1);
    drumPanel.seqTabs().setVisible (seqTab == 0);
    bassPanel.seqTabs().setVisible (seqTab == 1);
    if (auto* f = props.getUserSettings()) { f->setValue ("seqTab", seqTab); f->saveIfNeeded(); }
    repaint();
}

// Renders one pass of the bass line to a normal track, then mutes the synth so it does not play twice.
void MainComponent::addBassTrack (std::shared_ptr<TrackSnap> snap, const juce::String& undoName)
{
    auto slot = std::make_shared<int> (-1);
    const bool wasMuted = engine.bass.mute.load();
    record (undoName,
            [this, slot, snap] { snap->slot = *slot; *slot = restoreTrack (*snap); engine.bass.mute = true; bassPanel.refreshFromEngine(); syncRows(); },
            [this, slot, snap, wasMuted] { *snap = captureTrack (*slot); engine.removeTrack (*slot); engine.bass.mute = wasMuted; bassPanel.refreshFromEngine(); syncRows(); });
}

// Live bake: plays the loop and records the bass output of one pass, so knob moves (cutoff...) are captured.
void MainComponent::bakeBassLive()
{
    if (engine.bassCaptureState() != 0) { engine.cancelBassCapture(); setStatus ("Bass recording cancelled"); return; }
    const auto st = engine.getState();
    if (st == AudioEngine::Recording || st == AudioEngine::CountIn) { setStatus ("Stop recording first"); return; }
    if (! engine.bass.hasAnySteps()) { setStatus ("The bass pattern is empty"); return; }
    if (engine.getNumTracks() >= AudioEngine::maxTracks) { setStatus ("All tracks are in use"); return; }
    if (engine.bass.mute.load()) { setStatus ("Unmute the bass first"); return; }
    if (! engine.startBassCapture()) { setStatus ("Turn the loop on first - the pass is recorded over one loop"); return; }
    setStatus (st == AudioEngine::Stopped && engine.countInBars.load() > 0 ? "Count-in, then recording the bass for one loop pass..." : "Waiting for the next loop start...", 200);
}

void MainComponent::bakeBass()
{
    const auto st = engine.getState();
    if (st == AudioEngine::Recording || st == AudioEngine::CountIn) { setStatus ("Stop recording first"); return; }
    if (! engine.bass.hasAnySteps()) { setStatus ("The bass pattern is empty"); return; }
    if (engine.getNumTracks() >= AudioEngine::maxTracks) { setStatus ("All tracks are in use"); return; }
    const double sr = juce::jmax (1.0, engine.getSampleRate()), spb = engine.getSamplesPerBeat();
    const juce::int64 len = engine.loopEnabled.load() ? engine.getLoopLength() : (juce::int64) std::ceil (engine.bass.patternSteps() * spb / 4.0);
    if (len <= 0 || len > (juce::int64) (sr * 600.0)) { setStatus ("Nothing to bake"); return; }
    BassMachine bm;
    bm.copyFrom (engine.bass);
    bm.mute = false; bm.solo = false;
    bm.prepare (sr);
    bm.fx.prepare (sr);
    auto snap = std::make_shared<TrackSnap>();
    snap->audio.setSize (2, (int) len);
    snap->audio.clear();
    for (juce::int64 p = 0; p < len; p += 4096)
    {
        const int n = (int) juce::jmin<juce::int64> (4096, len - p);
        bm.render (p, n, spb / 4.0, bm.volume.load(), snap->audio.getWritePointer (0) + p, snap->audio.getWritePointer (1) + p, true, spb);
    }
    snap->valid = true; snap->slot = -1; snap->orderPos = 1000;
    snap->name = "Bass";
    snap->echoDiv = (int) TrackFx::Quarter;
    addBassTrack (snap, "Bake bass");
    setStatus ("Baked the bass line into a new track. The synth is muted so it does not play twice", 250);
}

// Analyze a recorded riff (tempo, key / mode, chords, rhythm) and offer rule-based bass lines.
void MainComponent::analyzeTrack (int slot)
{
    if (slot < 0 || slot >= AudioEngine::maxTracks) return;
    auto& t = engine.tracks[slot];
    const auto len = t.length.load();
    if (len <= 0) { setStatus ("Nothing to analyze - the track is empty"); return; }
    juce::AudioBuffer<float> copy;
    {
        juce::SpinLock::ScopedLockType lk (t.lock);
        const int n = (int) juce::jmin<juce::int64> (len, t.data.getNumSamples());
        copy.setSize (2, n);
        for (int c = 0; c < 2; ++c) copy.copyFrom (c, 0, t.data, c, 0, n);
    }
    AnalysisCard::Setup s;
    s.result = Analysis::analyse (copy, copy.getNumSamples(), t.offset.load(), engine.getSampleRate(), engine.getSamplesPerBeat(), engine.getBarBeats(), BassMachine::maxBars);
    if (! s.result.valid) { setStatus ("Could not find a key - is there enough audio in the track?"); return; }
    s.trackName = t.name;
    s.projectBpm = engine.bpm.load();
    s.stopped = engine.getState() == AudioEngine::Stopped;
    s.barSteps = (int) std::lround (engine.getBarBeats() * 4.0);
    s.beat = engine.tsDen.load() == 8 ? 2 : 4;
    s.haveDrums = engine.drums.hasAnySteps();
    if (s.haveDrums) s.drums = engine.drums.snapshot();

    auto* card = new AnalysisCard (std::move (s));
    card->onSetTempo = [this] (double bpm) { setBpm (bpm); setStatus ("Project tempo set to " + juce::String (engine.bpm.load(), 1) + " BPM"); };
    card->onUse = [this] (const juce::String& name, const BassMachine::Snapshot& snap)
    {
        const auto before = engine.bass.snapshot();
        auto apply = [this] (const BassMachine::Snapshot& x) { engine.bass.apply (x); bassPanel.refreshFromEngine(); };
        record ("Bass from riff: " + name, [apply, snap] { apply (snap); }, [apply, before] { apply (before); });
        showSeqTab (1);
        setStatus (engine.bass.mute.load() ? "Bass line '" + name + "' added - the bass is muted" : "Bass line '" + name + "' added in the Bass tab", 200);
    };
    juce::CallOutBox::launchAsynchronously (std::unique_ptr<juce::Component> (card), rows[slot] != nullptr ? rows[slot]->getScreenBounds() : getScreenBounds(), nullptr);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);
    Theme::cardShadow (g, barArea.getUnion (settingsArea).toFloat());
    Theme::cardShadow (g, drumPanel.getBounds().toFloat());
    Theme::cardShadow (g, mixer.getBounds().toFloat());
    {
        const auto f = barArea.getUnion (settingsArea).toFloat();     // one merged top panel, settings row slightly darker
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (f, Theme::radiusLg);
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path p;
            p.addRoundedRectangle (f, Theme::radiusLg);
            g.reduceClipRegion (p);
            g.setColour (Theme::panel.darker (0.28f));
            g.fillRect (settingsArea);
        }
        g.setColour (Theme::border.withAlpha (0.55f));
        g.drawRoundedRectangle (f.reduced (0.5f), Theme::radiusLg, 1.0f);
    }
    g.setColour (Theme::raised);
    g.fillRoundedRectangle (bpmLabel.getBounds().toFloat(), Theme::radius);
    g.setColour (Theme::deep);                                            // clock in a dark display box
    g.fillRoundedRectangle (timeLabel.getBounds().toFloat(), Theme::radius);
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (12);
    barArea = r.removeFromTop (56);
    auto bar = barArea.reduced (12, 8);
    titleLabel.setBounds (bar.removeFromLeft (110));
    bar.removeFromLeft (4);
    recBtn.setBounds (bar.removeFromLeft (44));  bar.removeFromLeft (8);
    playBtn.setBounds (bar.removeFromLeft (44)); bar.removeFromLeft (8);
    stopBtn.setBounds (bar.removeFromLeft (44)); bar.removeFromLeft (8);
    skipStartBtn.setBounds (bar.removeFromLeft (44)); bar.removeFromLeft (8);
    skipEndBtn.setBounds (bar.removeFromLeft (44)); bar.removeFromLeft (8);
    loopBtn.setBounds (bar.removeFromLeft (44)); bar.removeFromLeft (14);
    timeLabel.setBounds (bar.removeFromLeft (160).reduced (0, 2));
    clock.setBounds (timeLabel.getBounds());
    barLabel.setBounds (bar.removeFromLeft (140));

    fileBtn.setBounds (bar.removeFromRight (34).withSizeKeepingCentre (34, 30));
    bar.removeFromRight (14);
    for (auto* b : { &redoBtn, &undoBtn })
        { b->setBounds (bar.removeFromRight (32).withSizeKeepingCentre (32, 28)); bar.removeFromRight (4); }
    bar.removeFromRight (12);
    for (auto* b : { &dupBtn, &fadeOutBtn, &fadeInBtn, &reverseBtn, &silenceBtn, &pasteBtn, &copyBtn, &cutBtn, &selectAllBtn, &cropBtn })
        { b->setBounds (bar.removeFromRight (32).withSizeKeepingCentre (32, 28)); bar.removeFromRight (4); }
    bar.removeFromRight (12);
    folderBtn.setBounds (bar.removeFromRight (32).withSizeKeepingCentre (32, 28));
    bar.removeFromRight (6);
    notesBtn.setBounds (bar.removeFromRight (64).withSizeKeepingCentre (64, 28));
    bar.removeFromRight (8);
    projectLabel.setBounds (bar);   // whatever is left of the top row
    settingsArea = r.removeFromTop (44);
    auto sb = settingsArea.reduced (14, 8);
    bpmCaption.setBounds (sb.removeFromLeft (36));
    bpmLabel.setBounds (sb.removeFromLeft (64));  sb.removeFromLeft (6);
    tapBtn.setBounds (sb.removeFromLeft (48));    sb.removeFromLeft (18);
    barsBox.setBounds (sb.removeFromLeft (80));   sb.removeFromLeft (6);
    timeSigBox.setBounds (sb.removeFromLeft (64)); sb.removeFromLeft (18);
    clickBtn.setBounds (sb.removeFromLeft (56));  sb.removeFromLeft (6);
    clickVol.setBounds (sb.removeFromLeft (90));  sb.removeFromLeft (18);
    punchBtn.setBounds (sb.removeFromLeft (56));  sb.removeFromLeft (18);
    countCaption.setBounds (sb.removeFromLeft (64));
    countInBox.setBounds (sb.removeFromLeft (70)); sb.removeFromLeft (18);
    catchBtn.setBounds (sb.removeFromLeft (58));   sb.removeFromLeft (4);
    catchLenBox.setBounds (sb.removeFromLeft (92)); sb.removeFromLeft (14);
    looperBtn.setBounds (sb.removeFromLeft (64));  sb.removeFromLeft (18);
    for (auto* b : { &zoomInBtn, &zoomOutBtn, &zoomFitBtn })
        { b->setBounds (sb.removeFromRight (32).withSizeKeepingCentre (32, 28)); sb.removeFromRight (4); }
    sb.removeFromRight (10);
    statusLabel.setBounds (sb);

    r.removeFromTop (10);
    {   // bottom row: Drums | Bass tabs above the card, master + tuner on the right, same height as the card
        const int tabsH = 28;
        auto row = r.removeFromBottom (tabsH + MixerPanel::panelH);
        const auto tabsRow = row.removeFromTop (tabsH);
        mixer.setBounds (row.removeFromRight (MixerPanel::panelW));
        row.removeFromRight (10);
        for (auto* t : { &drumPanel.seqTabs(), &bassPanel.seqTabs() }) t->setBounds (tabsRow.withWidth (250));
        const auto pr = row.withHeight (juce::jmin (row.getHeight(), drumPanel.preferredHeight()));
        drumPanel.setBounds (pr);
        bassPanel.setBounds (pr);
    }    r.removeFromBottom (10);

    {
        auto tabsRowR = r.removeFromTop (PatternTabs::tabH + 3);
        songBtn.setBounds (tabsRowR.removeFromRight (74).withTrimmedBottom (3).withTrimmedTop (2));
        tabsRowR.removeFromRight (8);
        patternTabs.setBounds (tabsRowR);
    }
    r.removeFromTop (4);
    songBar.setVisible (engine.patterns.songMode);
    if (engine.patterns.songMode) { songBar.setBounds (r.removeFromTop (SongBar::barH)); r.removeFromTop (4); }
    auto rulerRow = r.removeFromTop (26);
    viewport.setBounds (r);
    const int w = viewport.getMaximumVisibleWidth();
    ruler.setBounds (rulerRow.withWidth (w));
    addTrackBtn.setBounds (rulerRow.withWidth (8 + 4 + 16 + 6 + 336 + 12).withTrimmedLeft (4));   // sits above the track headers
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

// ---------------------------------------------------------------- Catch: save what was just played

void MainComponent::catchNow()
{
    const auto st = engine.getState();
    if (st == AudioEngine::Recording || st == AudioEngine::CountIn) { setStatus ("Stop recording first"); return; }
    if (engine.getNumTracks() >= AudioEngine::maxTracks) { setStatus ("All tracks are in use"); return; }
    const int secs = catchLenBox.getSelectedId() > 0 ? catchLenBox.getSelectedId() : 30;
    auto buf = std::make_shared<juce::AudioBuffer<float>>();
    if (! engine.readCatch (*buf, (double) secs)) { setStatus ("Nothing buffered yet - play something first"); return; }
    const double sr = juce::jmax (1.0, engine.getSampleRate());
    const int len = buf->getNumSamples();
    if (buf->getMagnitude (0, len) < 0.003f && buf->getMagnitude (1, 0, len) < 0.003f)
    {
        setStatus ("Only silence in the last " + juce::String (secs) + " s - is the right input armed?");
        return;
    }
    juce::int64 a = 0, z = 0;
    if (SilenceTrim::findRange (*buf, len, sr, a, z))
    {
        juce::AudioBuffer<float> kept (2, (int) (z - a));
        for (int c = 0; c < 2; ++c) kept.copyFrom (c, 0, *buf, c, (int) a, (int) (z - a));
        *buf = std::move (kept);
    }
    const double caught = buf->getNumSamples() / sr;

    auto slot = std::make_shared<int> (-1);
    auto snap = std::make_shared<TrackSnap>();
    snap->valid = true; snap->slot = -1; snap->orderPos = 1000;
    snap->name = "Catch " + juce::Time::getCurrentTime().formatted ("%H:%M:%S");
    snap->echoDiv = (int) TrackFx::Quarter;
    snap->audio = std::move (*buf);
    record ("Catch",
            [this, slot, snap] { snap->slot = *slot; *slot = restoreTrack (*snap); syncRows(); },
            [this, slot, snap] { *snap = captureTrack (*slot); engine.removeTrack (*slot); syncRows(); });
    zoom = 1.0; zoomStart = 0.0;
    viewport.setViewPosition (0, list.contentHeight());
    setStatus ("Caught " + juce::String (caught, 1) + " s into a new track. Select the part you want and Crop to loop it", 250);
}

// ---------------------------------------------------------------- Looper: one button (key or foot switch)
// Idle -> first press records the first loop freely, second press closes it (length rounded to whole bars, BPM follows),
// then it plays; each further press records one pass on a NEW track, starting at the next loop start.

void MainComponent::looperReset (const juce::String& why)
{
    if (looperState == LooperState::FirstTake || looperState == LooperState::Finishing)
    {
        engine.stop();
        engine.countInBars = looperSavedCountIn;
        updateSettingsFromEngine();
    }
    if (looperState == LooperState::Queued || looperState == LooperState::Overdub) { engine.stop(); disarmAll(); }
    looperState = LooperState::Idle;
    if (why.isNotEmpty()) setStatus (why);
}

void MainComponent::looperPress()
{
    if (! looperBtn.getToggleState()) looperBtn.setToggleState (true, juce::dontSendNotification);   // the key or pedal switches Looper mode on
    switch (looperState)
    {
        case LooperState::Idle:
        {
            if (engine.getState() != AudioEngine::Stopped) { setStatus ("Stop the transport first"); return; }
            bool hasAudio = false;
            for (int slot : engine.order) hasAudio |= engine.tracks[slot].length.load() > 0;
            if (hasAudio) { looperState = LooperState::Playing; engine.play(); setStatus ("Looper: playing - press again to overdub on a new track"); }
            else looperStartFirstTake();
            break;
        }
        case LooperState::FirstTake:
            looperFirstSamples = engine.getPosition();
            stopTransport();
            looperState = LooperState::Finishing;
            break;
        case LooperState::Playing:
            if (engine.getState() == AudioEngine::Stopped) { engine.play(); setStatus ("Looper: playing"); }
            else looperQueueOverdub();
            break;
        default:
            setStatus ("Looper: wait for the current pass to finish", 100);
            break;
    }
}

void MainComponent::looperStartFirstTake()
{
    if (engine.getNumTracks() == 0) addTrack();
    int slot = engine.order.front();
    for (int s : engine.order) if (engine.tracks[s].armed.load()) { slot = s; break; }
    disarmAll();
    engine.tracks[slot].armed = true;
    rows[slot]->syncArm();
    looperSavedCountIn = engine.countInBars.load();
    engine.countInBars = 0;
    engine.loopEnabled = false;
    engine.setCursor (0);
    if (! engine.record())
    {
        engine.countInBars = looperSavedCountIn;
        setStatus ("Looper: could not start recording");
        return;
    }
    looperSlot = slot;
    looperState = LooperState::FirstTake;
    updateSettingsFromEngine();
    setStatus ("Looper: recording the first loop - press again to close it", 400);
}

void MainComponent::looperFinishFirstTake()
{
    auto& t = engine.tracks[looperSlot];
    const double sr = juce::jmax (1.0, engine.getSampleRate());
    const double secs = (double) looperFirstSamples / sr;
    engine.countInBars = looperSavedCountIn;
    if (secs < 0.5 || t.length.load() <= 0)
    {
        t.clearAudio();
        looperState = LooperState::Idle;
        updateSettingsFromEngine();
        setStatus ("Looper: that take was only " + juce::String (secs, 2) + " s - hold the loop open longer (press once to start, once to close)", 300);
        return;
    }

    // whole bars at a BPM close to the current one
    static const int options[] = { 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 24, 32 };
    const double cur = engine.bpm.load(), barBeats = engine.getBarBeats();
    int bars = 0; double bpm = cur, best = 1.0e9;
    for (int pass = 0; pass < 2 && bars == 0; ++pass)
        for (int b : options)
        {
            const double v = b * barBeats * 60.0 / secs;
            if (v < (pass == 0 ? 40.0 : 30.0) || v > (pass == 0 ? 240.0 : 300.0)) continue;
            const double score = std::abs (std::log (v / cur));
            if (score < best) { best = score; bars = b; bpm = v; }
        }
    if (bars == 0)
    {
        t.clearAudio();
        looperState = LooperState::Idle;
        setStatus ("Looper: could not fit that length to a loop - try again");
        return;
    }
    engine.bpm = bpm;
    engine.loopBars = bars;
    engine.loopEnabled = true;
    const auto loopLen = engine.getLoopLength();
    {
        const juce::int64 keep = juce::jmin<juce::int64> (t.length.load(), loopLen);
        auto src = t.copyRegion (0, keep);
        juce::AudioBuffer<float> nb (2, (int) loopLen);
        nb.clear();
        for (int c = 0; c < 2; ++c) nb.copyFrom (c, 0, src, c, 0, (int) keep);
        t.setAudio (std::move (nb));
        t.offset = 0;
    }
    updateSettingsFromEngine();
    for (auto* r : rows) r->repaint();
    engine.setCursor (0);
    engine.play();
    looperState = LooperState::Playing;
    setStatus ("Looper: " + juce::String (bars) + (bars == 1 ? " bar" : " bars") + " at " + juce::String (bpm, 1) + " BPM - press again to overdub on a new track", 400);
}

void MainComponent::looperQueueOverdub()
{
    if (engine.getNumTracks() >= AudioEngine::maxTracks) { setStatus ("Looper: all tracks are in use"); return; }
    addTrack();
    const int slot = engine.order.back();
    engine.tracks[slot].inputCode = engine.tracks[looperSlot >= 0 ? looperSlot : slot].inputCode.load();
    disarmAll();
    engine.tracks[slot].armed = true;
    rows[slot]->syncArm();
    if (! engine.recordAtNextLoop()) { setStatus ("Looper: could not start the overdub"); return; }
    looperLayerSlot = slot;
    looperState = LooperState::Queued;
    setStatus ("Looper: overdub starts at the next loop start", 200);
}

void MainComponent::looperTick()
{
    {   // the button shows what the looper is doing
        const char* label = "Looper";
        switch (looperState)
        {
            case LooperState::FirstTake: label = "REC 1st"; break;
            case LooperState::Finishing: label = "..."; break;
            case LooperState::Playing:   label = "Loop"; break;
            case LooperState::Queued:    label = "Dub next"; break;
            case LooperState::Overdub:   label = "REC dub"; break;
            default: break;
        }
        if (looperBtn.getButtonText() != label) looperBtn.setButtonText (label);
        if (looperState == LooperState::FirstTake && engine.getState() == AudioEngine::Recording)
            setStatus ("Looper: recording the first loop, " + juce::String ((double) engine.getPosition() / juce::jmax (1.0, engine.getSampleRate()), 1)
                           + " s - press again to close it", 15);
    }
    switch (looperState)
    {
        case LooperState::FirstTake:
            if (engine.getState() == AudioEngine::Stopped)       // stopped from elsewhere: give up this take
            {
                engine.tracks[looperSlot].clearAudio();
                engine.countInBars = looperSavedCountIn;
                looperState = LooperState::Idle;
                updateSettingsFromEngine();
            }
            break;
        case LooperState::Finishing:
            if (! engine.tracks[looperSlot].recSession && engine.getState() == AudioEngine::Stopped) looperFinishFirstTake();
            break;
        case LooperState::Queued:
            if (engine.getState() == AudioEngine::Recording) { looperState = LooperState::Overdub; setStatus ("Looper: overdubbing one pass", 200); }
            else if (engine.getState() == AudioEngine::Stopped) { disarmAll(); looperState = LooperState::Playing; }
            break;
        case LooperState::Overdub:
            if (engine.tracks[looperLayerSlot].sessionCommits >= 1)
            {
                engine.play();                                    // keeps playing, stops recording
                disarmAll();
                looperState = LooperState::Playing;
                setStatus ("Looper: layer added - press again for another", 300);
            }
            else if (engine.getState() == AudioEngine::Stopped) { disarmAll(); looperState = LooperState::Playing; }
            break;
        default:
            break;
    }
}

void MainComponent::syncTimeSigBox()
{
    const int n = engine.tsNum.load(), d = engine.tsDen.load();
    int id = 3;
    if (d == 4) id = n == 2 ? 1 : n == 3 ? 2 : n == 5 ? 4 : 3;
    else id = n == 7 ? 6 : 5;
    timeSigBox.setSelectedId (id, juce::dontSendNotification);
    drumPanel.refreshTemplates();
}

juce::File MainComponent::workDir() const
{
    const auto def = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("RiffCatch");
    if (auto* f = const_cast<juce::ApplicationProperties&> (props).getUserSettings())
    {
        const auto s = f->getValue ("workDir");
        if (s.isNotEmpty()) return juce::File (s);
    }
    return def;
}

void MainComponent::setWorkDir (const juce::File& dir)
{
    if (auto* f = props.getUserSettings()) { f->setValue ("workDir", dir.getFullPathName()); f->saveIfNeeded(); }
    refreshFolderTip();
    setStatus ("Work folder: " + dir.getFullPathName(), 200);
}

void MainComponent::chooseWorkDir()
{
    auto start = workDir();
    if (! start.exists()) start = start.getParentDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Choose the work folder (where projects are saved and opened from)", start);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto d = fc.getResult();
                              if (d != juce::File() && d.isDirectory()) setWorkDir (d);
                          });
}

// The folder of the open project, or the work folder while the project is untitled.
void MainComponent::openCurrentFolder()
{
    const auto dir = projectFile != juce::File() ? projectFile.getParentDirectory() : workDir();
    dir.createDirectory();
    if (! dir.startAsProcess()) setStatus ("Could not open " + dir.getFullPathName());
}

void MainComponent::refreshFolderTip()
{
    const bool has = projectFile != juce::File();
    folderBtn.setTooltip ((has ? "Open the project folder: " + projectFile.getParentDirectory().getFullPathName()
                               : "Open the work folder: " + workDir().getFullPathName()));
}

void MainComponent::addRecent (const juce::File& dir)
{
    if (dir == autosaveFile()) return;
    recent.addFile (dir);
    if (auto* f = props.getUserSettings()) { f->setValue ("recentProjects", recent.toString()); f->saveIfNeeded(); }
}

void MainComponent::updateNotesButton()
{
    notesBtn.setToggleState (engine.projectNotes.trim().isNotEmpty(), juce::dontSendNotification);
}

void MainComponent::editNotes()
{
    auto* ed = new juce::TextEditor();
    ed->setMultiLine (true, true);
    ed->setReturnKeyStartsNewLine (true);
    ed->setTextToShowWhenEmpty ("Notes for this project: key, capo, ideas...", Theme::dim);
    ed->setInputRestrictions (ProjectIO::maxNotesLength);
    ed->setText (engine.projectNotes, false);
    ed->setSize (340, 190);
    juce::Component::SafePointer<MainComponent> self (this);
    ed->onTextChange = [self, ed]
    {
        if (self == nullptr) return;
        self->engine.projectNotes = ed->getText();
        ++self->editCounter;
        self->updateNotesButton();
    };
    juce::CallOutBox::launchAsynchronously (std::unique_ptr<juce::Component> (ed), notesBtn.getScreenBounds(), nullptr);
}

void MainComponent::newProject()
{
    undoManager.clearUndoHistory();
    engine.removeAllTracks();
    engine.patterns.reset();
    engine.masterVolume = 1.0f;
    engine.drums.resetAll();
    engine.bass.resetAll();
    engine.bpm = 120.0; engine.loopBars = 4; engine.loopEnabled = true;
    engine.setTimeSignature (4, 4);
    engine.countInBars = 1; engine.clickEnabled = false; engine.clickVolume = 0.5f;
    engine.projectNotes.clear();
    updateNotesButton();
    engine.addTrack();
    engine.addTrack();
    projectFile = juce::File();
    refreshFolderTip();
    refreshStrips();
}

void MainComponent::saveProject (bool saveAs)
{
    if (! saveAs && projectFile != juce::File()) { doSave (projectFile); return; }
    workDir().createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save project as (a folder with this name is created)",
                                                   workDir(),
                                                   "*.riffcatch");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              const auto name = f.getFileNameWithoutExtension();
                              auto parent = f.getParentDirectory();
                              // "song/song.riffcatch": make the folder, unless the chosen place already is that folder
                              const auto folder = parent.getFileName() == name ? parent : parent.getChildFile (name);
                              doSave (folder.getChildFile (name + ProjectIO::extension));
                          });
}

void MainComponent::doSave (const juce::File& file)
{
    juce::String err;
    if (ProjectIO::save (engine, file, err))
    {
        projectFile = file;
        refreshFolderTip();
        addRecent (file);
        projectLabel.setText (file.getFileNameWithoutExtension(), juce::dontSendNotification);
        setStatus ("Saved to " + file.getFullPathName());
        autosaveDir().deleteRecursively();
    }
    else
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save failed", err);
}
// ---- import an audio file as a new track (File > Import audio..., or drop a file on the window)
void MainComponent::importAudio()
{
    chooser = std::make_unique<juce::FileChooser> ("Import audio",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f != juce::File()) doImport (f);
                          });
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() > 0 && (AudioImport::isAudioFile (juce::File (files[0])) || ProjectIO::isProjectFile (juce::File (files[0])));
}

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.size() == 0) return;
    if (ProjectIO::isProjectFile (juce::File (files[0]))) doOpen (juce::File (files[0]));
    else doImport (juce::File (files[0]));
}

void MainComponent::openFromCommandLine (const juce::String& commandLine)
{
    const auto arg = commandLine.removeCharacters ("\"").trim();
    if (arg.isNotEmpty() && ! arg.startsWith ("--") && ProjectIO::isProjectFile (juce::File (arg)) && juce::File (arg).existsAsFile())
        doOpen (juce::File (arg));
}

void MainComponent::doImport (const juce::File& file)
{
    if (engine.getState() == AudioEngine::Recording || engine.getState() == AudioEngine::CountIn) { setStatus ("Stop recording first"); return; }
    if (engine.getNumTracks() >= AudioEngine::maxTracks) { setStatus ("All tracks are in use"); return; }
    auto buf = std::make_shared<juce::AudioBuffer<float>>();
    double fileRate = 0.0;
    juce::String err;
    if (! AudioImport::readFile (file, engine.getSampleRate(), *buf, fileRate, err))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Import failed", err);
        return;
    }
    const double secs = buf->getNumSamples() / juce::jmax (1.0, engine.getSampleRate());
    const juce::String len = juce::String ((int) (secs / 60.0)) + ":" + juce::String ((int) std::fmod (secs, 60.0)).paddedLeft ('0', 2);

    auto* aw = new juce::AlertWindow ("Import " + file.getFileName(),
                                      "Length " + len + ". What is the tempo of this file (BPM)? The project is at "
                                          + juce::String (engine.bpm.load(), 1) + " BPM. The audio is not stretched, it plays at its own tempo.",
                                      juce::MessageBoxIconType::QuestionIcon);
    aw->addTextEditor ("bpm", juce::String (engine.bpm.load(), 1), "Tempo of the file (BPM)");
    aw->addButton ("Import", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Import and set project BPM", 2);
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<juce::AlertWindow> safeAw (aw);
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, buf, file, safeAw] (int result)
    {
        if (result == 0) return;
        double fileBpm = engine.bpm.load();
        if (safeAw != nullptr) { const double v = safeAw->getTextEditorContents ("bpm").getDoubleValue(); if (v >= 30.0 && v <= 300.0) fileBpm = v; }
        if (result == 2 && std::abs (fileBpm - engine.bpm.load()) > 0.01) setBpm (fileBpm);

        auto slot = std::make_shared<int> (-1);
        auto snap = std::make_shared<TrackSnap>();
        snap->valid = true; snap->slot = -1; snap->orderPos = 1000;
        snap->name = file.getFileNameWithoutExtension();
        snap->echoDiv = (int) TrackFx::Quarter;
        juce::String trimNote;
        {
            juce::int64 a = 0, z = 0;
            if (SilenceTrim::findRange (*buf, buf->getNumSamples(), engine.getSampleRate(), a, z))
            {
                juce::AudioBuffer<float> kept (2, (int) (z - a));
                for (int c = 0; c < 2; ++c) kept.copyFrom (c, 0, *buf, c, (int) a, (int) (z - a));
                trimNote = " (trimmed " + juce::String ((buf->getNumSamples() - (int) (z - a)) / juce::jmax (1.0, engine.getSampleRate()), 1) + " s of silence)";
                *buf = std::move (kept);
            }
        }
        snap->audio = std::move (*buf);
        record ("Import audio",
                [this, slot, snap] { snap->slot = *slot; *slot = restoreTrack (*snap); syncRows(); },
                [this, slot, snap] { *snap = captureTrack (*slot); engine.removeTrack (*slot); syncRows(); });
        zoom = 1.0; zoomStart = 0.0;
        viewport.setViewPosition (0, list.contentHeight());
        setStatus ("Imported " + file.getFileName() + trimNote + ". Select the part you want (Alt+drag = whole bars), then Crop to loop it");
    }), true);
}

void MainComponent::openProject()
{
    workDir().createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Open project",
                                                   workDir(),
                                                   "*.riffcatch");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f != juce::File()) doOpen (f);
                          });
}

void MainComponent::doOpen (const juce::File& file)
{
    juce::String err;
    undoManager.clearUndoHistory();
    if (ProjectIO::load (engine, file, err))
    {
        projectFile = file;
        refreshFolderTip();
        addRecent (file);
        refreshStrips();
        updateNotesButton();
        setStatus ("Opened " + file.getFileNameWithoutExtension());
    }
    else
    {
        refreshStrips();
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Open failed", err);
    }
}

// ------------------------------------------------------------- patterns

void MainComponent::setupPatternTabs()
{
    patternTabs.onSelect = [this] (int i) { switchPattern (i); };
    patternTabs.onAdd = [this]
    {
        if (engine.getState() != AudioEngine::Stopped) { setStatus ("Stop the transport to change pattern"); return; }
        engine.patterns.storeCurrent (engine);
        const int i = engine.patterns.addBlank (engine);
        if (i < 0) return;
        switchPattern (i);
    };
    patternTabs.onRenamed = [this] (int i, const juce::String& n)
    {
        if (! engine.patterns.isUsed (i)) return;
        engine.patterns.pats[(size_t) i].name = n.isEmpty() ? PatternBank::defaultName (i) : n;
        ++editCounter;
        patternTabs.refresh();
    };
    patternTabs.onColour = [this] (int i, int c) { engine.patterns.pats[i].colour = c; ++editCounter; patternTabs.refresh(); };
    patternTabs.onDuplicate = [this] (int i)
    {
        const int n = engine.patterns.duplicate (engine, i);
        if (n < 0) return;
        ++editCounter;
        patternTabs.refresh();
        setStatus ("Duplicated to pattern " + juce::String (n + 1));
    };
    patternTabs.onClear = [this] (int i)
    {
        if (i == engine.patterns.current && engine.getState() != AudioEngine::Stopped) { setStatus ("Stop the transport first"); return; }
        engine.patterns.clearPattern (engine, i);
        undoManager.clearUndoHistory();
        ++editCounter;
        syncRows();
    };
    patternTabs.onDelete = [this] (int i)
    {
        if (i == engine.patterns.current && engine.getState() != AudioEngine::Stopped) { setStatus ("Stop the transport first"); return; }
        if (engine.patterns.remove (engine, i))
        {
            undoManager.clearUndoHistory();
            ++editCounter;
            syncRows();
        }
    };
}

void MainComponent::switchPattern (int i)
{
    if (i == engine.patterns.current || ! engine.patterns.isUsed (i)) return;
    if (engine.getState() != AudioEngine::Stopped) { setStatus ("Stop the transport to change pattern"); return; }
    engine.patterns.storeCurrent (engine);
    engine.patterns.current = i;
    engine.patterns.loadCurrent (engine);
    undoManager.clearUndoHistory();           // undo steps belong to the pattern they were made in
    ++editCounter;
    syncRows();
}

void MainComponent::setupLibraryCounter()
{
    auto wire = [this] (LibraryCounter& ch, PatternBank::Kind kind)
    {
        auto after = [this]
        {
            undoManager.clearUndoHistory();          // undo steps belong to the pattern they were made in
            ++editCounter;
            drumPanel.refreshFromEngine();
            bassPanel.refreshFromEngine();
            patternTabs.refresh();
            repaint();
        };
        ch.onStep = [this, kind, after] (int delta)
        {
            const int cur = engine.patterns.currentRef (kind);
            const int target = juce::jlimit (0, PatternBank::maxItems - 1, cur + delta);
            if (target == cur) return;
            engine.patterns.selectItem (engine, kind, target);
            after();
        };
        ch.onCopy = [this, kind, after]
        {
            if (engine.patterns.newItem (engine, kind, true) < 0) setStatus ("The library is full (16)");
            else after();
        };
    };
    wire (drumPanel.counter(), PatternBank::Drums);
    wire (bassPanel.counter(), PatternBank::Bass);
}
// ------------------------------------------------------------- song

void MainComponent::setSongMode (bool on)
{
    if (on == engine.patterns.songMode) return;
    if (engine.getState() != AudioEngine::Stopped) { setStatus ("Stop the transport first"); return; }
    auto& bank = engine.patterns;
    bank.songMode = on;
    if (on && bank.song.empty())                       // start with every pattern once, in order
        for (int i = 0; i < PatternBank::maxPatterns; ++i) if (bank.isUsed (i)) bank.song.push_back ({ i, 1 });
    if (! on) engine.clearSong();
    songBtn.setToggleState (on, juce::dontSendNotification);
    ++editCounter;
    resized();
    repaint();
    setStatus (on ? "Song mode: Play plays the whole song" : "Back to editing one pattern");
}

bool MainComponent::startSong()
{
    auto buf = std::make_shared<juce::AudioBuffer<float>>();
    juce::String err;
    if (! Exporter::renderSongBuffer (engine, *buf, err, false)) { setStatus (err); return false; }
    if (engine.getCursor() >= buf->getNumSamples()) engine.setCursor (0);
    engine.setSong (buf);
    return true;
}