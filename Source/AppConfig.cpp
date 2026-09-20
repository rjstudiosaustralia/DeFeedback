#include "AppConfig.h"

namespace defeedback
{
juce::String pluginModeId (PluginMode mode)
{
    return mode == PluginMode::voiceIso ? "voiceiso"
         : mode == PluginMode::clearVoice ? "clearvoice" : "defeedback";
}

juce::String pluginModeName (PluginMode mode)
{
    return mode == PluginMode::voiceIso ? "VoiceISO"
         : mode == PluginMode::clearVoice ? "Clear Voice Live" : "De-Feedback";
}

bool supportsSampleRate (PluginMode mode, double rate)
{
    return mode == PluginMode::deFeedback
        || (mode == PluginMode::voiceIso && std::isfinite (rate) && rate > 0.0)
        || rate == 48000.0 || rate == 96000.0 || rate == 192000.0;
}

namespace
{
PluginMode readMode (const juce::String& id)
{
    return id == "voiceiso" ? PluginMode::voiceIso
         : id == "clearvoice" ? PluginMode::clearVoice : PluginMode::deFeedback;
}

void writeState (juce::XmlElement& node, const LanePluginState& state)
{
    node.setAttribute ("dry", state.dry);
    node.setAttribute ("pluginState", state.pluginStateBase64);
    node.setAttribute ("editorOpen", state.editorOpen);
    node.setAttribute ("editorWindowState", state.editorWindowState);
    node.setAttribute ("pluginEnabled", state.pluginEnabled);
    node.setAttribute ("modeParameters", state.modeParameterState);
}

LanePluginState readState (const juce::XmlElement& node)
{
    return { node.getBoolAttribute ("dry"), node.getStringAttribute ("pluginState"),
             node.getBoolAttribute ("editorOpen"), node.getStringAttribute ("editorWindowState"),
             node.getBoolAttribute ("pluginEnabled", true), node.getStringAttribute ("modeParameters") };
}

void applyState (LaneConfig& lane, const LanePluginState& state)
{
    lane.dry = state.dry;
    lane.pluginStateBase64 = state.pluginStateBase64;
    lane.editorOpen = state.editorOpen;
    lane.editorWindowState = state.editorWindowState;
    lane.pluginEnabled = state.pluginEnabled;
    lane.modeParameterState = state.modeParameterState;
}

void writeLanes (juce::XmlElement& parent, const juce::Array<LaneConfig>& lanes)
{
    for (const auto& lane : lanes)
    {
        auto* child = parent.createNewChildElement ("LANE");
        child->setAttribute ("id", lane.id);
        child->setAttribute ("name", lane.name);
        child->setAttribute ("input", lane.inputChannel);
        child->setAttribute ("output", lane.outputChannel);
        child->setAttribute ("pluginMode", pluginModeId (lane.pluginMode));
        writeState (*child, lane.capturePluginState());
        for (const auto& [mode, state] : lane.inactivePluginStates)
        {
            auto* saved = child->createNewChildElement ("PLUGIN_STATE");
            saved->setAttribute ("pluginMode", pluginModeId (mode));
            writeState (*saved, state);
        }
    }
}

juce::Array<LaneConfig> readLanes (const juce::XmlElement& parent, PluginMode defaultMode)
{
    juce::Array<LaneConfig> lanes;
    for (auto* child : parent.getChildIterator())
    {
        if (! child->hasTagName ("LANE")) continue;
        LaneConfig lane;
        lane.id = child->getIntAttribute ("id", lanes.size() + 1);
        lane.name = child->getStringAttribute ("name", "Vocal " + juce::String (lane.id));
        lane.inputChannel = juce::jmax (0, child->getIntAttribute ("input"));
        lane.outputChannel = juce::jmax (0, child->getIntAttribute ("output"));
        lane.pluginMode = readMode (child->getStringAttribute ("pluginMode", pluginModeId (defaultMode)));
        applyState (lane, readState (*child));
        for (auto* saved : child->getChildIterator())
            if (saved->hasTagName ("PLUGIN_STATE"))
            {
                const auto mode = readMode (saved->getStringAttribute ("pluginMode"));
                if (mode != lane.pluginMode) lane.inactivePluginStates[mode] = readState (*saved);
            }
        lanes.add (std::move (lane));
    }
    return lanes;
}
}

LanePluginState LaneConfig::capturePluginState() const
{
    return { dry, pluginStateBase64, editorOpen, editorWindowState, pluginEnabled, modeParameterState };
}

void LaneConfig::selectPlugin (PluginMode mode)
{
    if (mode == pluginMode) return;
    inactivePluginStates[pluginMode] = capturePluginState();
    const auto found = inactivePluginStates.find (mode);
    applyState (*this, found != inactivePluginStates.end() ? found->second : LanePluginState {});
    inactivePluginStates.erase (mode);
    pluginMode = mode;
}

void AppConfig::switchPluginMode (PluginMode mode)
{
    for (auto& lane : lanes) lane.selectPlugin (mode);
    pluginMode = mode; // Default for subsequently added lanes.
}

juce::String AppConfig::rackModeId() const
{
    if (lanes.isEmpty()) return pluginModeId (pluginMode);
    for (const auto& lane : lanes)
        if (lane.pluginMode != lanes[0].pluginMode) return "mixed";
    return pluginModeId (lanes[0].pluginMode);
}

juce::String AppConfig::rackModeName() const
{
    return rackModeId() == "mixed" ? "Mixed plugins"
        : pluginModeName (lanes.isEmpty() ? pluginMode : lanes[0].pluginMode);
}

bool supportsSampleRate (const juce::Array<LaneConfig>& lanes, double rate)
{
    for (const auto& lane : lanes)
        if (! supportsSampleRate (lane.pluginMode, rate)) return false;
    return std::isfinite (rate) && rate > 0.0;
}

AppConfig::AppConfig()
{
    lanes.add ({ 1, "Vocal 1", 0, 0, false, {}, false, {} });
}

std::unique_ptr<juce::XmlElement> AppConfig::toXml() const
{
    auto root = std::make_unique<juce::XmlElement> ("DEFEEDBACK_LIVE_CONFIG");
    root->setAttribute ("formatVersion", 8);
    root->setAttribute ("pluginMode", "defeedback"); // Safe downgrade projection.
    root->setAttribute ("defaultPluginMode", pluginModeId (pluginMode));
    root->setAttribute ("inputDevice", inputDeviceName);
    root->setAttribute ("outputDevice", outputDeviceName);
    root->setAttribute ("sampleRate", sampleRate);
    root->setAttribute ("bufferSize", bufferSize);
    root->setAttribute ("autoStart", autoStart);
    root->setAttribute ("launchAtLogin", launchAtLogin);
    root->setAttribute ("remoteControlEnabled", remoteControlEnabled);
    root->setAttribute ("remoteControlPort", remoteControlPort);
    root->setAttribute ("remoteAccessCodeRequired", remoteAccessCodeRequired);
    root->setAttribute ("remoteAccessCode", remoteAccessCode);
    root->setAttribute ("remoteBrowserToken", remoteBrowserToken);
    root->setAttribute ("mainWindowState", mainWindowState);
    writeLanes (*root->createNewChildElement ("RACK_LANES"), lanes);
    auto legacy = lanes;
    for (auto& lane : legacy)
    {
        lane.selectPlugin (PluginMode::deFeedback);
        lane.inactivePluginStates.clear();
    }
    writeLanes (*root, legacy);
    // Preserve unmatched lanes/routes from earlier whole-rack mode banks.
    for (const auto& [mode, bank] : inactiveModeLanes)
    {
        auto* archived = root->createNewChildElement ("LEGACY_BANK");
        archived->setAttribute ("pluginMode", pluginModeId (mode));
        writeLanes (*archived, bank);
    }
    return root;
}

AppConfig AppConfig::fromXml (const juce::XmlElement& root)
{
    AppConfig result;
    if (! root.hasTagName ("DEFEEDBACK_LIVE_CONFIG")) return result;
    result.inputDeviceName = root.getStringAttribute ("inputDevice");
    result.outputDeviceName = root.getStringAttribute ("outputDevice");
    result.sampleRate = root.getDoubleAttribute ("sampleRate", 48000.0);
    result.bufferSize = juce::jmax (16, root.getIntAttribute ("bufferSize", 64));
    result.autoStart = root.getBoolAttribute ("autoStart", true);
    result.launchAtLogin = root.getBoolAttribute ("launchAtLogin", true);
    result.remoteControlEnabled = root.getBoolAttribute ("remoteControlEnabled", false);
    result.remoteControlPort = juce::jlimit (1024, 65535, root.getIntAttribute ("remoteControlPort", 8765));
    result.remoteAccessCodeRequired = root.getBoolAttribute ("remoteAccessCodeRequired", true);
    result.remoteAccessCode = root.getStringAttribute ("remoteAccessCode");
    result.remoteBrowserToken = root.getStringAttribute ("remoteBrowserToken");
    result.mainWindowState = root.getStringAttribute ("mainWindowState");
    if (const auto* rack = root.getChildByName ("RACK_LANES"))
    {
        result.pluginMode = readMode (root.getStringAttribute ("defaultPluginMode"));
        result.lanes = readLanes (*rack, result.pluginMode);
        for (auto* child : root.getChildIterator())
            if (child->hasTagName ("LEGACY_BANK"))
            {
                const auto mode = readMode (child->getStringAttribute ("pluginMode"));
                result.inactiveModeLanes[mode] = readLanes (*child, mode);
            }
    }
    else
    {
        result.pluginMode = readMode (root.getStringAttribute ("pluginMode"));
        auto& banks = result.inactiveModeLanes;
        banks[PluginMode::deFeedback] = readLanes (root, PluginMode::deFeedback);
        if (const auto* bank = root.getChildByName ("CLEAR_VOICE_LANES"))
            banks[PluginMode::clearVoice] = readLanes (*bank, PluginMode::clearVoice);
        if (const auto* bank = root.getChildByName ("VOICE_ISO_LANES"))
            banks[PluginMode::voiceIso] = readLanes (*bank, PluginMode::voiceIso);
        result.lanes = banks[result.pluginMode];
        if (result.lanes.isEmpty())
        {
            result.lanes = banks[PluginMode::deFeedback];
            for (auto& lane : result.lanes) lane.selectPlugin (result.pluginMode);
        }
        for (auto& lane : result.lanes)
            for (const auto& [mode, bank] : banks)
                if (mode != lane.pluginMode)
                    for (const auto& saved : bank)
                        if (saved.id == lane.id)
                            lane.inactivePluginStates[mode] = saved.capturePluginState();
    }
    if (result.lanes.isEmpty())
    {
        result.lanes.add (LaneConfig {});
        result.lanes.getReference (0).selectPlugin (result.pluginMode);
    }
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
