#include "AppConfig.h"

namespace defeedback
{
AppConfig::AppConfig()
{
    lanes.add ({ 1, "Vocal 1", 0, 0, false, {}, false, {} });
}

std::unique_ptr<juce::XmlElement> AppConfig::toXml() const
{
    auto root = std::make_unique<juce::XmlElement> ("DEFEEDBACK_LIVE_CONFIG");
    root->setAttribute ("formatVersion", 2);
    root->setAttribute ("inputDevice", inputDeviceName);
    root->setAttribute ("outputDevice", outputDeviceName);
    root->setAttribute ("sampleRate", sampleRate);
    root->setAttribute ("bufferSize", bufferSize);
    root->setAttribute ("autoStart", autoStart);
    root->setAttribute ("launchAtLogin", launchAtLogin);
    root->setAttribute ("mainWindowState", mainWindowState);

    for (const auto& lane : lanes)
    {
        auto* child = root->createNewChildElement ("LANE");
        child->setAttribute ("id", lane.id);
        child->setAttribute ("name", lane.name);
        child->setAttribute ("input", lane.inputChannel);
        child->setAttribute ("output", lane.outputChannel);
        child->setAttribute ("dry", lane.dry);
        child->setAttribute ("pluginState", lane.pluginStateBase64);
        child->setAttribute ("editorOpen", lane.editorOpen);
        child->setAttribute ("editorWindowState", lane.editorWindowState);
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
    result.mainWindowState = root.getStringAttribute ("mainWindowState");
    result.lanes.clear();

    for (auto* child : root.getChildIterator())
    {
        if (! child->hasTagName ("LANE"))
            continue;

        LaneConfig lane;
        lane.id = child->getIntAttribute ("id", result.lanes.size() + 1);
        lane.name = child->getStringAttribute ("name", "Vocal " + juce::String (lane.id));
        lane.inputChannel = juce::jmax (0, child->getIntAttribute ("input"));
        lane.outputChannel = juce::jmax (0, child->getIntAttribute ("output"));
        lane.dry = child->getBoolAttribute ("dry", false);
        lane.pluginStateBase64 = child->getStringAttribute ("pluginState");
        lane.editorOpen = child->getBoolAttribute ("editorOpen", false);
        lane.editorWindowState = child->getStringAttribute ("editorWindowState");
        result.lanes.add (std::move (lane));
    }

    if (result.lanes.isEmpty())
        result.lanes.add ({ 1, "Vocal 1", 0, 0, false, {}, false, {} });

    return result;
}

juce::String validateExclusiveRoutes (const juce::Array<LaneConfig>& lanes)
{
    for (int first = 0; first < lanes.size(); ++first)
    {
        for (int second = first + 1; second < lanes.size(); ++second)
        {
            if (lanes[first].inputChannel == lanes[second].inputChannel)
                return "Input channel " + juce::String (lanes[first].inputChannel + 1)
                     + " is already assigned to another lane.";

            if (lanes[first].outputChannel == lanes[second].outputChannel)
                return "Output channel " + juce::String (lanes[first].outputChannel + 1)
                     + " is already assigned to another lane.";
        }
    }

    return {};
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
