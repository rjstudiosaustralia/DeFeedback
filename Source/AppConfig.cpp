#include "AppConfig.h"

namespace defeedback
{
AppConfig::AppConfig()
{
    lanes.add ({ 1, "Vocal 1", 0, 0, false, {} });
}

std::unique_ptr<juce::XmlElement> AppConfig::toXml() const
{
    auto root = std::make_unique<juce::XmlElement> ("DEFEEDBACK_LIVE_CONFIG");
    root->setAttribute ("formatVersion", 1);
    root->setAttribute ("inputDevice", inputDeviceName);
    root->setAttribute ("outputDevice", outputDeviceName);
    root->setAttribute ("sampleRate", sampleRate);
    root->setAttribute ("bufferSize", bufferSize);
    root->setAttribute ("autoStart", autoStart);
    root->setAttribute ("launchAtLogin", launchAtLogin);

    for (const auto& lane : lanes)
    {
        auto* child = root->createNewChildElement ("LANE");
        child->setAttribute ("id", lane.id);
        child->setAttribute ("name", lane.name);
        child->setAttribute ("input", lane.inputChannel);
        child->setAttribute ("output", lane.outputChannel);
        child->setAttribute ("dry", lane.dry);
        child->setAttribute ("pluginState", lane.pluginStateBase64);
    }

    return root;
}

AppConfig AppConfig::fromXml (const juce::XmlElement& root)
{
    AppConfig result;

    if (! root.hasTagName ("DEFEEDBACK_LIVE_CONFIG"))
        return result;

    result.inputDeviceName = root.getStringAttribute ("inputDevice");
    result.outputDeviceName = root.getStringAttribute ("outputDevice");
    result.sampleRate = root.getDoubleAttribute ("sampleRate", 48000.0);
    result.bufferSize = juce::jmax (16, root.getIntAttribute ("bufferSize", 64));
    result.autoStart = root.getBoolAttribute ("autoStart", true);
    result.launchAtLogin = root.getBoolAttribute ("launchAtLogin", true);
    result.lanes.clear();

    for (auto* child : root.getChildIterator())
    {
        if (! child->hasTagName ("LANE") || result.lanes.size() >= maxLanes)
            continue;

        LaneConfig lane;
        lane.id = child->getIntAttribute ("id", result.lanes.size() + 1);
        lane.name = child->getStringAttribute ("name", "Vocal " + juce::String (lane.id));
        lane.inputChannel = juce::jmax (0, child->getIntAttribute ("input"));
        lane.outputChannel = juce::jmax (0, child->getIntAttribute ("output"));
        lane.dry = child->getBoolAttribute ("dry", false);
        lane.pluginStateBase64 = child->getStringAttribute ("pluginState");
        result.lanes.add (std::move (lane));
    }

    if (result.lanes.isEmpty())
        result.lanes.add ({ 1, "Vocal 1", 0, 0, false, {} });

    return result;
}

double estimateRoundTripMilliseconds (double sampleRate,
                                      int bufferSize,
                                      int inputLatencySamples,
                                      int outputLatencySamples)
{
    if (sampleRate <= 0.0)
        return 0.0;

    auto totalSamples = juce::jmax (0, inputLatencySamples)
                      + juce::jmax (0, outputLatencySamples);

    // Core Audio device-reported latencies normally include each side's buffer.
    // Some virtual drivers report zero; use two buffers as a conservative fallback.
    if (totalSamples == 0)
        totalSamples = 2 * juce::jmax (0, bufferSize);
    return 1000.0 * static_cast<double> (totalSamples) / sampleRate;
}
}
