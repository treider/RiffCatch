#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"
#include "ui/Theme.h"
#include "SelfTest.h"

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
    void startApp() { juce::LookAndFeel::setDefaultLookAndFeel (&laf); window = std::make_unique<MainWindow> (getApplicationName()); }
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
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

private:
    RiffLookAndFeel laf;
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (RiffCatchApp)
