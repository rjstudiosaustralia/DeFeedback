#pragma once

#include "AppConfig.h"
#include "AudioEngine.h"
#include "SettingsStore.h"

#include <juce_gui_extra/juce_gui_extra.h>

namespace defeedback
{
class LaneRow;
class LaneHeader;

class MainComponent final : public juce::Component,
                            private juce::Timer,
                            private juce::ChangeListener
{
public:
    explicit MainComponent (bool safeLaunch = false);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    juce::String getSavedMainWindowState() const { return config.mainWindowState; }
    void storeMainWindowState (const juce::String&);
    void restorePluginWindows() { engine.restorePluginWindows(); }

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refreshAllControls();
    void refreshDeviceControls();
    void rebuildLaneRows();
    void applyLanes();
    void saveConfig();
    void showMessage (const juce::String&, bool isError);
    void startOrStop();
    void updateRuntimeStatus();

    SettingsStore settings;
    AppConfig config;
    AudioEngine engine;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label deviceLabel;
    juce::Label rateLabel;
    juce::Label bufferLabel;
    juce::Label engineLabel;
    juce::Label outputSafetyLabel;
    juce::Label metricsLabel;
    juce::Label pluginLabel;
    juce::Label alertLabel;
    juce::Label routingHintLabel;

    juce::ComboBox deviceCombo;
    juce::ComboBox rateCombo;
    juce::ComboBox bufferCombo;
    juce::TextButton startStopButton { "START ENGINE" };
    juce::TextButton emergencyButton { "MUTE ALL OUTPUTS" };
    juce::TextButton refreshDevicesButton { "REFRESH" };
    juce::TextButton resetXRunsButton { "RESET XRUNS" };
    juce::TextButton addLaneButton { "+ ADD LANE" };
    juce::ToggleButton autoStartToggle { "Auto-start audio" };
    juce::ToggleButton launchAtLoginToggle { "Launch at login" };

    juce::Viewport laneViewport;
    juce::Component laneContainer;
    std::unique_ptr<LaneHeader> laneHeader;
    juce::OwnedArray<LaneRow> laneRows;
    juce::Array<DeviceChoice> deviceChoices;
    juce::Array<double> sampleRates;
    juce::Array<int> bufferSizes;

    bool refreshingControls = false;
    bool safeLaunch = false;
   #if DEFEEDBACK_UI_PREVIEW
    bool previewEngineRunning = true;
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
}
