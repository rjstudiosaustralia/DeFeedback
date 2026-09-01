#pragma once

#include "AppConfig.h"
#include "MeterProcessor.h"
#include "PluginWindow.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace defeedback
{
struct DeviceChoice
{
    juce::String displayName;
    juce::String inputName;
    juce::String outputName;

    bool operator== (const DeviceChoice&) const = default;
};

struct LaneStatus
{
    juce::String text;
    bool isDryFallback = false;
    bool editorAvailable = false;
    bool strengthAvailable = false;
    bool pluginMuteAvailable = false;
    bool pluginMuted = false;
    float strengthNormalized = 1.0f;
    float inputPeak = 0.0f;
    float outputPeak = 0.0f;
};

class AudioEngine final : public juce::ChangeBroadcaster,
                          private juce::ChangeListener,
                          private juce::AsyncUpdater
{
public:
    AudioEngine();
    ~AudioEngine() override;

    juce::String initialise (const AppConfig&);
    juce::String start();
    void stop();
    bool isRunning() const noexcept { return running; }

    void setEmergencyMuted (bool);
    bool isEmergencyMuted() const noexcept { return emergencyMuted.load (std::memory_order_relaxed); }

    juce::String setLanes (const juce::Array<LaneConfig>&);
    juce::Array<LaneConfig> captureLanesWithPluginState();

    juce::Array<DeviceChoice> getDeviceChoices();
    juce::Array<double> getAvailableSampleRates() const;
    juce::Array<int> getAvailableBufferSizes() const;
    juce::StringArray getInputChannelNames() const;
    juce::StringArray getOutputChannelNames() const;
    void refreshDeviceList();

    juce::String selectDevice (const DeviceChoice&);
    juce::String setSampleRate (double);
    juce::String setBufferSize (int);

    DeviceChoice getCurrentDeviceChoice() const;
    double getCurrentSampleRate() const;
    int getCurrentBufferSize() const;
    double getEstimatedRoundTripMilliseconds() const;
    double getCpuUsage() const { return deviceManager.getCpuUsage(); }
    int getXRunCount() const noexcept;
    void resetXRunCount();

    bool isPluginAvailable() const noexcept { return pluginAvailable; }
    juce::String getPluginDiagnostic() const { return pluginDiagnostic; }
    juce::Array<LaneStatus> getLaneStatuses();
    void openPluginEditor (int laneIndex);
    void restorePluginWindows();
    void setLaneStrength (int laneIndex, float normalizedValue);
    void setLanePluginMuted (int laneIndex, bool shouldMute);

private:
    struct LaneRuntime
    {
        juce::AudioProcessorGraph::Node::Ptr pluginNode;
        MeterProcessor* inputMeter = nullptr;
        MeterProcessor* outputMeter = nullptr;
        juce::AudioProcessorParameter* strengthParameter = nullptr;
        juce::AudioProcessorParameter* muteParameter = nullptr;
        juce::String status;
        bool dryFallback = false;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void handleAsyncUpdate() override;
    void locatePlugin();
    juce::String rebuildGraph();
    void closePluginWindows();
    void capturePluginWindowStates();
    void reopenSavedPluginWindows();
    void bindPluginParameters (LaneRuntime&);
    void updateSavedPluginStates();
    int getActiveInputSpan() const;
    int getActiveOutputSpan() const;
    void configureAllChannels (juce::AudioDeviceManager::AudioDeviceSetup&) const;
    juce::String enableAllAvailableChannels();

    juce::AudioDeviceManager deviceManager;
    juce::AudioPluginFormatManager pluginFormats;
    juce::AudioProcessorGraph graph;
    juce::AudioProcessorPlayer player;
    juce::PluginDescription pluginDescription;
    juce::Array<LaneConfig> lanes;
    std::vector<LaneRuntime> laneRuntimes;
    juce::OwnedArray<PluginWindow> pluginWindows;

    bool running = false;
    std::atomic<bool> emergencyMuted { false };
    bool pluginAvailable = false;
    bool suppressDeviceNotifications = false;
    int xRunBaseline = 0;
    juce::String pluginDiagnostic;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
}
