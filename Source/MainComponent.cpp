#include "MainComponent.h"
#include "project/ProjectIO.h"
#include "project/Exporter.h"

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
        r->onDelete    = [this] (TrackRow* row) { deleteTrack (row); };
        r->onDragStart = [this] (TrackRow* row) { list.dragStart (row); };
        r->onDragMove  = [this] (TrackRow* row, int dy) { list.dragMove (row, dy); };
        r->onDragEnd   = [this] (TrackRow* row) { list.dragEnd (row); };
        rows.add (r);
        list.addChildComponent (r);
    }
    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    addAndMakeVisible (master);
    addAndMakeVisible (inputBar);
    addAndMakeVisible (ruler);
    inputBar.onPick = [this] (int ch)
    {
        int n = 0;
        for (int slot : engine.order)
            if (engine.tracks[slot].armed.load()) { engine.tracks[slot].inputCode = ch; rows[slot]->refreshFromTrack(); ++n; }
        setStatus (n > 0 ? "Input " + juce::String (ch + 1) + " assigned to " + juce::String (n) + " armed track(s)"
                         : "Arm a track first, then click an input");
    };

    titleLabel.setText ("RiffCatch", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, Theme::blue);
    addAndMakeVisible (titleLabel);

    for (auto* b : { &recBtn, &playBtn, &stopBtn }) addAndMakeVisible (b);
    recBtn.setTooltip ("Record (R)");
    playBtn.setTooltip ("Play / Stop (Space)");
    stopBtn.setTooltip ("Stop and rewind (Enter)");
    addBtn.setTooltip ("Add track (Ctrl+T)");
    saveBtn.setTooltip ("Save (Ctrl+S, Ctrl+Shift+S = Save as)");
    openBtn.setTooltip ("Open (Ctrl+O)");
    newBtn.setTooltip ("New (Ctrl+N)");
    recBtn.onClick  = [this] { toggleRecord(); };
    playBtn.onClick = [this] { togglePlay(); };
    stopBtn.onClick = [this] { engine.stop(); };
    addBtn.onClick  = [this] { addTrack(); };

    for (auto* b : { &addBtn, &exportBtn, &newBtn, &openBtn, &saveBtn, &audioBtn })
    {
        b->setWantsKeyboardFocus (false);
        b->setColour (juce::TextButton::buttonColourId, Theme::raised);
        addAndMakeVisible (b);
    }
    saveBtn.setColour (juce::TextButton::buttonColourId, Theme::blue.withAlpha (0.25f));
    exportBtn.setTooltip ("Bounce master to WAV");
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
    bpmLabel.setColour (juce::Label::backgroundColourId, Theme::deep);
    bpmLabel.setColour (juce::Label::textColourId, Theme::cyan);
    bpmLabel.setTooltip ("Tempo (double-click to type, 30-300)");
    bpmLabel.onTextChange = [this] { setBpm (bpmLabel.getText().getDoubleValue()); grabKeyboardFocus(); };
    addAndMakeVisible (bpmLabel);

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
    for (int s : { 1, 2, 3, 4, 6, 8 }) countInBox.addItem (juce::String (s) + " s", 10 + s);
    countInBox.setWantsKeyboardFocus (false);
    countInBox.setTooltip ("Count-in length, rounded to whole beats");
    countInBox.onChange = [this]
    {
        const int id = countInBox.getSelectedId();
        engine.countInSeconds = id == 1 ? 0.0f : (float) (id - 10);
        grabKeyboardFocus();
    };
    addAndMakeVisible (countInBox);

    latLabel.setColour (juce::Label::textColourId, Theme::dim);
    latLabel.setJustificationType (juce::Justification::centredRight);
    latLabel.setTooltip ("Round-trip latency reported by the audio driver. Recordings are shifted earlier by this amount (plus the adjustment) so they sit on the grid.");
    addAndMakeVisible (latLabel);
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
    addAndMakeVisible (latAdjust);
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
    setSize (1140, 760);
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
    inputBar.setNumInputs (n);
}

void MainComponent::refreshStrips()
{
    for (auto* r : rows)
        if (engine.tracks[r->slot()].active.load()) r->refreshFromTrack();
    master.refreshFromEngine();
    updateSettingsFromEngine();
    projectLabel.setText (projectDir == juce::File() ? "Untitled" : projectDir.getFileName(), juce::dontSendNotification);
    addBtn.setEnabled (engine.getNumTracks() < AudioEngine::maxTracks);
    resized();
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
    const int s = (int) std::lround (engine.countInSeconds.load());
    countInBox.setSelectedId (s <= 0 ? 1 : 10 + s, juce::dontSendNotification);
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

void MainComponent::exportMenu()
{
    juce::PopupMenu m;
    if (engine.loopEnabled.load())
    {
        m.addItem (1, "WAV - 1 loop");
        m.addItem (2, "WAV - 2 loops");
        m.addItem (4, "WAV - 4 loops");
        m.addItem (8, "WAV - 8 loops");
    }
    else
        m.addItem (1, "WAV - full length");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exportBtn), [this] (int loops)
    {
        if (loops <= 0) return;
        chooser = std::make_unique<juce::FileChooser> ("Export WAV",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (
                (projectDir == juce::File() ? juce::String ("RiffCatch") : projectDir.getFileName()) + ".wav"), "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, loops] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f != juce::File()) doExportWav (f.withFileExtension ("wav"), loops);
                              });
    });
}

void MainComponent::doExportWav (const juce::File& f, int loops)
{
    juce::String err;
    if (Exporter::bounceWav (engine, f, loops, err)) setStatus ("Exported " + f.getFullPathName());
    else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export failed", err);
}

void MainComponent::addTrack()
{
    if (engine.addTrack() < 0) return;
    changeListenerCallback (nullptr);
    refreshStrips();
    viewport.setViewPosition (0, list.contentHeight());
}

void MainComponent::deleteTrack (TrackRow* row)
{
    const int slot = row->slot();
    auto doDelete = [this, slot]
    {
        engine.removeTrack (slot);
        rows[slot]->setVisible (false);
        refreshStrips();
    };
    if (engine.tracks[slot].length.load() == 0) { doDelete(); return; }
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Delete track",
                                        "Delete \"" + engine.tracks[slot].name + "\" and its recording? This cannot be undone.",
                                        "Delete", "Cancel", this,
                                        juce::ModalCallbackFunction::create ([doDelete] (int r) { if (r == 1) doDelete(); }));
}
void MainComponent::setStatus (const juce::String& s)
{
    statusLabel.setText (s, juce::dontSendNotification);
    statusTicks = 90;
}

void MainComponent::toggleRecord()
{
    if (engine.getState() == AudioEngine::Recording) { engine.play(); return; }   // stop recording, keep playing
    if (engine.getState() == AudioEngine::CountIn) { engine.stop(); return; }
    if (! engine.record()) setStatus ("Arm a track (R) before recording");
}

void MainComponent::togglePlay()
{
    if (engine.getState() != AudioEngine::Stopped) engine.stop();
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
    if (k == juce::KeyPress::returnKey || k == juce::KeyPress::homeKey) { engine.stop(); return true; }
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
        }
        return true;
    }
    return false;
}
void MainComponent::timerCallback()
{
    engine.pollRecording();

    const auto st = engine.getState();
    recBtn.setToggleState (st == AudioEngine::Recording || st == AudioEngine::CountIn, juce::dontSendNotification);
    playBtn.setToggleState (st == AudioEngine::Playing, juce::dontSendNotification);

    const bool idle = st == AudioEngine::Stopped;
    for (auto* c : std::initializer_list<juce::Component*> { &bpmLabel, &tapBtn, &loopBtn, &barsBox })
        c->setEnabled (idle);

    const double sr = juce::jmax (1.0, engine.getSampleRate());
    latLabel.setText ("Latency " + juce::String (engine.deviceLatency.load() * 1000.0 / sr, 1) + " ms +", juce::dontSendNotification);
    const double spb = engine.getSamplesPerBeat();
    const double secs = engine.getPosition() / sr;
    const int m = (int) (secs / 60.0);
    timeLabel.setText (juce::String::formatted ("%02d:%02d.%03d", m, (int) secs % 60, (int) ((secs - std::floor (secs)) * 1000.0)),
                       juce::dontSendNotification);

    if (st == AudioEngine::CountIn)
    {
        const auto left = (int) std::ceil ((double) (engine.getCountInLength() - engine.getCountInPosition()) / sr - 1.0e-6);   // seconds remaining
        barLabel.setText ("Count-in " + juce::String (juce::jmax (1, left)), juce::dontSendNotification);
        barLabel.setColour (juce::Label::textColourId, Theme::yellow);
    }
    else
    {
        const double beats = engine.getPosition() / spb;
        barLabel.setText (juce::String::formatted ("Bar %d.%d", (int) (beats / 4.0) + 1, (int) beats % 4 + 1), juce::dontSendNotification);
        barLabel.setColour (juce::Label::textColourId, st == AudioEngine::Recording ? Theme::red : Theme::cyan);
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
    inputBar.tick();
    master.tick();

    if (statusTicks > 0 && --statusTicks == 0) statusLabel.setText ({}, juce::dontSendNotification);
}
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);
    for (auto a : { barArea, settingsArea })
    {
        g.setColour (Theme::panel.brighter (0.05f));
        g.fillRect (a);
        g.setColour (Theme::deep);
        g.drawRect (a, 1);
    }
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
    addBtn.setBounds (bar.removeFromRight (84));   bar.removeFromRight (6);
    exportBtn.setBounds (bar.removeFromRight (70)); bar.removeFromRight (12);
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
    latLabel.setBounds (sb.removeFromLeft (120));
    latAdjust.setBounds (sb.removeFromLeft (80).withSizeKeepingCentre (80, 24)); sb.removeFromLeft (12);
    projectLabel.setBounds (sb.removeFromRight (150));
    statusLabel.setBounds (sb);

    r.removeFromTop (10);
    inputBar.setBounds (r.removeFromTop (38));
    r.removeFromTop (10);
    master.setBounds (r.removeFromBottom (56));
    r.removeFromBottom (10);

    auto rulerRow = r.removeFromTop (22);
    viewport.setBounds (r);
    const int w = viewport.getMaximumVisibleWidth();
    ruler.setBounds (rulerRow.withWidth (w));
    list.setSize (w, juce::jmax (list.contentHeight(), viewport.getHeight()));
}
// ------------------------------------------------------------- dialogs/projects

void MainComponent::showAudioSettings()
{
    if (audioDialog != nullptr) { audioDialog->toFront (true); return; }
    auto* sel = new juce::AudioDeviceSelectorComponent (deviceManager, 0, 8, 0, 8, false, false, true, false);
    sel->setSize (540, 480);
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
    engine.removeAllTracks();
    engine.masterVolume = 1.0f;
    engine.bpm = 120.0; engine.loopBars = 4; engine.loopEnabled = true;
    engine.countInSeconds = 3.0f; engine.clickEnabled = true; engine.clickVolume = 0.5f;
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
