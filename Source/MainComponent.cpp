#include "MainComponent.h"
#include "project/ProjectIO.h"

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

    for (auto* b : { &addBtn, &newBtn, &openBtn, &saveBtn, &audioBtn })
    {
        b->setWantsKeyboardFocus (false);
        b->setColour (juce::TextButton::buttonColourId, Theme::raised);
        addAndMakeVisible (b);
    }
    saveBtn.setColour (juce::TextButton::buttonColourId, Theme::blue.withAlpha (0.25f));
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

    engine.addTrack();
    engine.addTrack();
    changeListenerCallback (nullptr);
    refreshStrips();
    setWantsKeyboardFocus (true);
    setSize (1120, 680);
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
    projectLabel.setText (projectDir == juce::File() ? "Untitled" : projectDir.getFileName(), juce::dontSendNotification);
    addBtn.setEnabled (engine.getNumTracks() < AudioEngine::maxTracks);
    resized();
    repaint();
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
    if (engine.getState() == AudioEngine::Recording) { engine.play(); return; }
    if (! engine.record()) setStatus ("Arm a track (R) before recording");
}

void MainComponent::togglePlay()
{
    if (engine.getState() == AudioEngine::Playing) engine.stop();
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
    const auto st = engine.getState();
    recBtn.setToggleState (st == AudioEngine::Recording, juce::dontSendNotification);
    playBtn.setToggleState (st == AudioEngine::Playing, juce::dontSendNotification);

    const double secs = engine.getPosition() / juce::jmax (1.0, engine.getSampleRate());
    const int m = (int) (secs / 60.0);
    timeLabel.setText (juce::String::formatted ("%02d:%02d.%03d", m, (int) secs % 60, (int) ((secs - std::floor (secs)) * 1000.0)),
                       juce::dontSendNotification);

    juce::int64 longest = engine.getPosition();
    const double sr = juce::jmax (1.0, engine.getSampleRate());
    for (int slot : engine.order)
    {
        longest = juce::jmax (longest, engine.tracks[slot].length.load());
        rows[slot]->tick();
    }
    const double view = juce::jmax (30.0, std::ceil ((longest / sr + 1.0) / 30.0) * 30.0);
    for (auto* r : rows) r->setViewSeconds (view);
    inputBar.tick();
    master.tick();

    if (statusTicks > 0 && --statusTicks == 0) statusLabel.setText ({}, juce::dontSendNotification);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (barArea.toFloat(), 14.0f);
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
    timeLabel.setBounds (bar.removeFromLeft (160));
    bar.removeFromLeft (10);

    audioBtn.setBounds (bar.removeFromRight (70)); bar.removeFromRight (6);
    saveBtn.setBounds (bar.removeFromRight (64));  bar.removeFromRight (6);
    openBtn.setBounds (bar.removeFromRight (64));  bar.removeFromRight (6);
    newBtn.setBounds (bar.removeFromRight (56));   bar.removeFromRight (12);
    addBtn.setBounds (bar.removeFromRight (84));   bar.removeFromRight (12);
    projectLabel.setBounds (bar.removeFromRight (130));
    statusLabel.setBounds (bar);

    r.removeFromTop (10);
    inputBar.setBounds (r.removeFromTop (38));
    r.removeFromTop (10);
    master.setBounds (r.removeFromBottom (56));
    r.removeFromBottom (10);

    viewport.setBounds (r);
    const int w = viewport.getMaximumVisibleWidth();
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
    engine.addTrack();
    engine.addTrack();
    projectDir = juce::File();
    bpm = 120.0;
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
    if (ProjectIO::save (engine, dir, bpm, err))
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
    if (ProjectIO::load (engine, dir, bpm, err))
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
