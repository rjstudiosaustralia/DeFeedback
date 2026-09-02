#include "MainComponent.h"

#include <juce_gui_extra/juce_gui_extra.h>

namespace defeedback
{
class MainWindow final : public juce::DocumentWindow
{
public:
    explicit MainWindow (bool safeLaunch)
        : DocumentWindow ("DeFeedback Live",
                          juce::Colour (0xff101316),
                          allButtons)
    {
        setUsingNativeTitleBar (true);
        mainContent = new MainComponent (safeLaunch);
        setContentOwned (mainContent, true);
        setResizable (true, true);
        setResizeLimits (1260, 660, 1800, 1200);

        if (mainContent->getSavedMainWindowState().isEmpty()
            || ! restoreWindowStateFromString (mainContent->getSavedMainWindowState()))
            centreWithSize (getWidth(), getHeight());

        setVisible (true);
        mainContent->restorePluginWindows();
    }

    ~MainWindow() override
    {
        if (mainContent != nullptr)
            mainContent->storeMainWindowState (getWindowStateAsString());
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    MainComponent* mainContent = nullptr;
};

class Application final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override
    {
       #if DEFEEDBACK_UI_PREVIEW
        return "DeFeedback Live QA";
       #else
        return "DeFeedback Live";
       #endif
    }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& commandLine) override
    {
        lookAndFeel.setColourScheme (juce::LookAndFeel_V4::getDarkColourScheme());
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
       #if JUCE_DEBUG
        constexpr auto debugBuild = true;
       #else
        constexpr auto debugBuild = false;
       #endif
        mainWindow = std::make_unique<MainWindow> (debugBuild || commandLine.contains ("--safe"));
    }

    void shutdown() override
    {
        mainWindow.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override
    {
        if (mainWindow != nullptr)
            mainWindow->toFront (true);
    }

private:
    juce::LookAndFeel_V4 lookAndFeel;
    std::unique_ptr<MainWindow> mainWindow;
};
}

START_JUCE_APPLICATION (defeedback::Application)
