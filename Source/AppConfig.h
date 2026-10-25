#pragma once

#include <juce_core/juce_core.h>
#include <map>

namespace defeedback
{
enum class PluginMode { deFeedback, clearVoice, voiceIso };

juce::String pluginModeId (PluginMode);
juce::String pluginModeName (PluginMode);
bool supportsSampleRate (PluginMode, double);

struct LanePluginState
{
    bool dry = false;
    juce::String pluginStateBase64;
    bool editorOpen = false;
    juce::String editorWindowState;
    bool pluginEnabled = true;
    juce::String modeParameterState;
    bool operator== (const LanePluginState&) const = default;
};

struct LaneConfig
{
    int id = 1;
    juce::String name { "Vocal 1" };
    int inputChannel = 0;
    int outputChannel = 0;
    bool dry = false;
    juce::String pluginStateBase64;
    bool editorOpen = false;
    juce::String editorWindowState;
    bool pluginEnabled = true;
    juce::String modeParameterState {};

    PluginMode pluginMode = PluginMode::deFeedback;
    std::map<PluginMode, LanePluginState> inactivePluginStates {};

    LanePluginState capturePluginState() const;
    void selectPlugin (PluginMode);
    bool operator== (const LaneConfig&) const = default;
};

struct AppConfig
{
    juce::String inputDeviceName;
    juce::String outputDeviceName;
    double sampleRate = 48000.0;
    int bufferSize = 64;
    bool parallelProcessing = true;
    int maxProcessingThreads = 0; // Total audio threads including the device callback; 0 = automatic.
    bool autoStart = true;
    bool launchAtLogin = true;
    bool remoteControlEnabled = false;
    int remoteControlPort = 8765;
    bool remoteAccessCodeRequired = true;
    juce::String remoteAccessCode;
    juce::String remoteBrowserToken;
    juce::String mainWindowState;
    juce::Array<LaneConfig> lanes;
    PluginMode pluginMode = PluginMode::deFeedback;
    // Original pre-v8 racks retained for recovery after migration.
    std::map<PluginMode, juce::Array<LaneConfig>> inactiveModeLanes;

    AppConfig();

    std::unique_ptr<juce::XmlElement> toXml() const;
    static AppConfig fromXml (const juce::XmlElement&);
    void switchPluginMode (PluginMode);
    juce::String rackModeId() const;
    juce::String rackModeName() const;
};

bool supportsSampleRate (const juce::Array<LaneConfig>&, double);

juce::String validateExclusiveRoutes (const juce::Array<LaneConfig>&);

double estimateRoundTripMilliseconds (double sampleRate,
                                      int bufferSize,
                                      int inputLatencySamples,
                                      int outputLatencySamples);
}
