#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"

class RiffCatchApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "RiffCatch"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }

    void initialise (const juce::String&) override { window = std::make_unique<MainWindow> (getApplicationName()); }
    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { quit(); }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, juce::Colours::darkgrey, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

private:
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (RiffCatchApp)
