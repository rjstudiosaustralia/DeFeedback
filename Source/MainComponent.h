#pragma once

#include "AppConfig.h"
#include "AudioEngine.h"
#include "RemoteControlServer.h"
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
    bool applyLaneConfiguration (const juce::Array<LaneConfig>&, const juce::String& successMessage);
    void addLane();
    int findLaneIndexById (int id) const;
    void saveConfig();
    void showMessage (const juce::String&, bool isError);
    void startOrStop();
    void updateRuntimeStatus();
    bool startRemoteControl();
    void stopRemoteControl();
    void updateRemoteControls();
    void publishRemoteState (const juce::Array<LaneStatus>&, bool engineRunning, bool masterMuted);
    void handleRemoteCommand (const juce::var&);

    SettingsStore settings;
    AppConfig config;
    AudioEngine engine;
    RemoteControlServer remoteControlServer;

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
    juce::Label remoteStatusLabel;

    juce::ComboBox deviceCombo;
    juce::ComboBox rateCombo;
    juce::ComboBox bufferCombo;
    juce::TextButton startStopButton { "START ENGINE" };
    juce::TextButton emergencyButton { "MUTE ALL OUTPUTS" };
    juce::TextButton refreshDevicesButton { "REFRESH" };
    juce::TextButton resetXRunsButton { "RESET XRUNS" };
    juce::TextButton addLaneButton { "+ ADD LANE" };
    juce::TextButton aboutButton { "ABOUT / SAFETY" };
    juce::TextButton copyRemoteButton { "COPY DETAILS" };
    juce::TextButton newRemoteCodeButton { "NEW CODE" };
    juce::ToggleButton autoStartToggle { "Auto-start audio" };
    juce::ToggleButton launchAtLoginToggle { "Launch at login" };
    juce::ToggleButton remoteControlToggle { "Enable full-control LAN remote" };

    juce::Viewport laneViewport;
    juce::Component laneContainer;
    std::unique_ptr<LaneHeader> laneHeader;
    juce::OwnedArray<LaneRow> laneRows;
    juce::Array<DeviceChoice> deviceChoices;
    juce::Array<double> sampleRates;
    juce::Array<int> bufferSizes;

    bool refreshingControls = false;
    bool safeLaunch = false;
    juce::String remoteActionMessage;
    bool remoteActionError = false;
    double remoteActionMessageExpiresAtMs = 0.0;
   #if DEFEEDBACK_UI_PREVIEW
    bool previewEngineRunning = true;
    juce::Array<int> previewMutedLaneIds { 2 };
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
}
