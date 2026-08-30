#pragma once

#include <juce_core/juce_core.h>

namespace defeedback
{
constexpr int maxLanes = 10;

struct LaneConfig
{
    int id = 1;
    juce::String name { "Vocal 1" };
    int inputChannel = 0;
    int outputChannel = 0;
    bool dry = false;
    juce::String pluginStateBase64;

    bool operator== (const LaneConfig&) const = default;
};

struct AppConfig
{
    juce::String inputDeviceName;
    juce::String outputDeviceName;
    double sampleRate = 48000.0;
    int bufferSize = 64;
    bool autoStart = true;
    bool launchAtLogin = true;
    juce::Array<LaneConfig> lanes;

    AppConfig();

    std::unique_ptr<juce::XmlElement> toXml() const;
    static AppConfig fromXml (const juce::XmlElement&);
};

double estimateRoundTripMilliseconds (double sampleRate,
                                      int bufferSize,
                                      int inputLatencySamples,
                                      int outputLatencySamples);
}
