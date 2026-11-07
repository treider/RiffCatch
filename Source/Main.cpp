#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"
#include "ui/Theme.h"
#include "SelfTest.h"
#include "WinTitleBar.h"
#include "WinFileAssoc.h"

class RiffCatchApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "RiffCatch"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }

    void initialise (const juce::String& cmd) override
    {
        if (cmd.contains ("--selftest"))
        {
            juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("selftest.txt")
                .replaceWithText (runEngineSelfTest());
            quit();
            return;
        }
        startApp();
    }
    void startApp()
    {
        {
            juce::ApplicationProperties tmp;
            juce::PropertiesFile::Options opts;
            opts.applicationName = "RiffCatch"; opts.filenameSuffix = ".settings"; opts.osxLibrarySubFolder = "Application Support";
            tmp.setStorageParameters (opts);
            ThemeStore::reload();
            if (auto* f = tmp.getUserSettings())
            {
                const auto savedName = f->getValue ("themeName");
                int themeIndex = ThemeStore::indexOf (savedName);
                if (themeIndex < 0) // old selection of a removed theme falls back to the default
                {
                    themeIndex = savedName.isNotEmpty() ? ThemeStore::indexOf ("Neo-Retro")
                                                       : f->getIntValue ("theme", Theme::current);
                    if (themeIndex < 0) themeIndex = Theme::current;
                }
                Theme::apply (themeIndex);
            }
            laf.applyColours();
        }
        juce::LookAndFeel::setDefaultLookAndFeel (&laf); window = std::make_unique<MainWindow> (getApplicationName() + " v" + getApplicationVersion());
        if (! FileAssoc::isRegistered()) FileAssoc::registerForUser();   // makes .riffcatch files open here; the exe path is kept up to date
    }
    // one window only: starting a project file again (double-click) opens it in the running app instead of a second instance
    bool moreThanOneInstanceAllowed() override { return false; }
    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        if (window == nullptr) return;
        if (auto* mc = dynamic_cast<MainComponent*> (window->getContentComponent())) mc->openFromCommandLine (commandLine);
        window->toFront (true);
    }
    void shutdown() override { window.reset(); juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }
    void systemRequestedQuit() override { quit(); }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, Theme::bg, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            setResizeLimits (1100, 620, 3000, 2000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
            applyDarkTitleBar (getPeer(), Theme::bg, Theme::text);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

private:
    RiffLookAndFeel laf;
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (RiffCatchApp)
