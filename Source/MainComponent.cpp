#include "MainComponent.h"

MainComponent::MainComponent()
{
    juce::PropertiesFile::Options opts;
    opts.applicationName = "RiffCatch";
    opts.filenameSuffix = ".settings";
    opts.osxLibrarySubFolder = "Application Support";
    juce::ApplicationProperties props;
    props.setStorageParameters (opts);

    std::unique_ptr<juce::XmlElement> saved;
    if (auto* f = props.getUserSettings()) saved = f->getXmlValue ("audioDeviceState");

    deviceManager.initialise (2, 2, saved.get(), true);
    deviceManager.addAudioCallback (&engine);

    selector = std::make_unique<juce::AudioDeviceSelectorComponent> (
        deviceManager, 0, 8, 0, 8, false, false, true, false);
    addAndMakeVisible (*selector);
    addAndMakeVisible (status);
    status.setJustificationType (juce::Justification::centredLeft);

    setSize (640, 520);
    startTimerHz (10);
}

MainComponent::~MainComponent()
{
    juce::PropertiesFile::Options opts;
    opts.applicationName = "RiffCatch";
    opts.filenameSuffix = ".settings";
    opts.osxLibrarySubFolder = "Application Support";
    juce::ApplicationProperties props;
    props.setStorageParameters (opts);
    if (auto xml = deviceManager.createStateXml())
        if (auto* f = props.getUserSettings()) { f->setValue ("audioDeviceState", xml.get()); f->saveIfNeeded(); }

    deviceManager.removeAudioCallback (&engine);
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (8);
    status.setBounds (r.removeFromBottom (24));
    selector->setBounds (r);
}

void MainComponent::timerCallback()
{
    auto* dev = deviceManager.getCurrentAudioDevice();
    status.setText (dev ? dev->getTypeName() + " | " + juce::String (engine.getSampleRate()) + " Hz | samples: "
                              + juce::String (engine.getSamplePosition())
                        : "No device", juce::dontSendNotification);
}
