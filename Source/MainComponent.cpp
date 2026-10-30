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
        engine.tracks[i].resetToDefaults (i);
        strips.add (new ChannelStrip (engine, i));
        addChildComponent (strips.getLast());
    }
    addAndMakeVisible (master);

    titleLabel.setText ("RiffCatch", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, Theme::blue);
    addAndMakeVisible (titleLabel);

    for (auto* b : { &recBtn, &playBtn, &stopBtn, &addBtn }) addAndMakeVisible (b);
    recBtn.onClick  = [this] { toggleRecord(); };
    playBtn.onClick = [this] { togglePlay(); };
    stopBtn.onClick = [this] { engine.stop(); };
    addBtn.onClick  = [this]
    {
        const int n = engine.getNumTracks();
        if (n < AudioEngine::maxTracks) { engine.setNumTracks (n + 1); refreshStrips(); }
    };

    for (auto* b : { &newBtn, &openBtn, &saveBtn, &audioBtn })
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

    engine.setNumTracks (2);
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
    const int n = juce::jmax (1, engine.getNumInputs());
    for (auto* s : strips) s->setInputChoices (n);
}

void MainComponent::refreshStrips()
{
    const int n = engine.getNumTracks();
    for (int i = 0; i < strips.size(); ++i)
    {
        strips[i]->setVisible (i < n);
        if (i < n) strips[i]->refreshFromTrack();
    }
    master.refreshFromEngine();
    projectLabel.setText (projectDir == juce::File() ? "Untitled" : projectDir.getFileName(), juce::dontSendNotification);
    resized();
    repaint();
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
    if (k == juce::KeyPress::spaceKey) { togglePlay(); return true; }
    if (k.getTextCharacter() == 'r' || k.getTextCharacter() == 'R') { toggleRecord(); return true; }
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

    for (int i = 0; i < engine.getNumTracks(); ++i) strips[i]->tick();
    master.tick();

    if (statusTicks > 0 && --statusTicks == 0) statusLabel.setText ({}, juce::dontSendNotification);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (barArea.toFloat(), 14.0f);

    if (addBtn.isVisible())
    {
        g.setColour (Theme::border.withAlpha (0.6f));
        g.drawRoundedRectangle (addArea.toFloat().reduced (0.5f), 12.0f, 1.5f);
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
    timeLabel.setBounds (bar.removeFromLeft (160));
    bar.removeFromLeft (10);

    audioBtn.setBounds (bar.removeFromRight (70)); bar.removeFromRight (6);
    saveBtn.setBounds (bar.removeFromRight (64));  bar.removeFromRight (6);
    openBtn.setBounds (bar.removeFromRight (64));  bar.removeFromRight (6);
    newBtn.setBounds (bar.removeFromRight (56));   bar.removeFromRight (12);
    projectLabel.setBounds (bar.removeFromRight (150));
    statusLabel.setBounds (bar);

    r.removeFromTop (12);
    master.setBounds (r.removeFromRight (140));
    r.removeFromRight (12);

    const int n = engine.getNumTracks();
    const int gap = 10;
    const int maxW = 150;
    const int w = juce::jmin (maxW, (r.getWidth() - gap * AudioEngine::maxTracks) / juce::jmax (1, n + 1));
    for (int i = 0; i < n; ++i)
    {
        strips[i]->setBounds (r.removeFromLeft (w));
        r.removeFromLeft (gap);
    }
    const bool canAdd = n < AudioEngine::maxTracks;
    addBtn.setVisible (canAdd);
    if (canAdd)
    {
        addArea = r.removeFromLeft (w);
        addBtn.setBounds (addArea.withSizeKeepingCentre (48, 48));
    }
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
    engine.stop();
    engine.setNumTracks (0);
    for (int i = 0; i < AudioEngine::maxTracks; ++i) engine.tracks[i].resetToDefaults (i);
    engine.masterVolume = 1.0f;
    engine.setNumTracks (2);
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
