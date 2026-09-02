#include "AudioEngine.h"

#include "PluginRegistration.h"

#include <set>

namespace defeedback
{
namespace
{
// Enough to bootstrap every interface currently in scope. Once Core Audio has
// opened the device, enableAllAvailableChannels() replaces this with the exact
// channel counts reported by the driver.
constexpr int bootstrapHardwareChannels = 256;
const juce::String deFeedbackIdentifier { "AudioUnit:Effects/aufx,FbTI,jDSP" };

juce::AudioProcessor::BusesLayout monoLayout()
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::mono());
    layout.outputBuses.add (juce::AudioChannelSet::mono());
    return layout;
}
}

AudioEngine::AudioEngine()
{
    pluginFormats.addFormat (std::make_unique<juce::AudioUnitPluginFormat>());
    deviceManager.addChangeListener (this);
}

AudioEngine::~AudioEngine()
{
    cancelPendingUpdate();
    stop();
    closePluginWindows();
    player.setProcessor (nullptr);
    graph.clear();
    deviceManager.removeChangeListener (this);
    deviceManager.closeAudioDevice();
}

juce::String AudioEngine::initialise (const AppConfig& config)
{
    lanes = config.lanes;
    locatePlugin();

    juce::AudioDeviceManager::AudioDeviceSetup preferred;
    preferred.inputDeviceName = config.inputDeviceName;
    preferred.outputDeviceName = config.outputDeviceName;
    preferred.sampleRate = config.sampleRate;
    preferred.bufferSize = config.bufferSize;
    configureAllChannels (preferred);

    suppressDeviceNotifications = true;
    auto error = deviceManager.initialise (bootstrapHardwareChannels,
                                           bootstrapHardwareChannels,
                                           nullptr,
                                           true,
                                           {},
                                           &preferred);
    suppressDeviceNotifications = false;

    if (error.isNotEmpty())
        return "Audio device: " + error;

    if (error = enableAllAvailableChannels(); error.isNotEmpty())
        return "Audio device channels: " + error;

    xRunBaseline = deviceManager.getXRunCount();
    return rebuildGraph();
}

juce::String AudioEngine::start()
{
    if (running)
        return {};

    if (deviceManager.getCurrentAudioDevice() == nullptr)
        return "No Core Audio device is open.";

    auto error = rebuildGraph();
    if (error.isNotEmpty())
        return error;

    player.setProcessor (&graph);

    deviceManager.addAudioCallback (&player);
    running = true;
    sendChangeMessage();
    return {};
}

void AudioEngine::stop()
{
    if (! running)
        return;

    deviceManager.removeAudioCallback (&player);
    player.setProcessor (nullptr);
    running = false;
    sendChangeMessage();
}

void AudioEngine::setEmergencyMuted (bool shouldMute)
{
    if (emergencyMuted.load (std::memory_order_relaxed) == shouldMute)
        return;

    emergencyMuted.store (shouldMute, std::memory_order_relaxed);

    sendChangeMessage();
}

juce::String AudioEngine::setLanes (const juce::Array<LaneConfig>& newLanes)
{
    updateSavedPluginStates();
    capturePluginWindowStates();
    auto updated = newLanes;

    // Routing edits originate in the UI, while the freshest opaque AU state lives
    // in the active instances. Preserve that state by stable lane id when a graph
    // edit causes the instances to be rebuilt.
    for (auto& lane : updated)
    {
        for (const auto& current : lanes)
        {
            if (lane.id == current.id)
            {
                lane.pluginStateBase64 = current.pluginStateBase64;
                lane.editorOpen = current.editorOpen;
                lane.editorWindowState = current.editorWindowState;
                break;
            }
        }
    }

    lanes = std::move (updated);

    if (lanes.isEmpty())
        lanes.add ({ 1, "Vocal 1", 0, 0, false, {}, false, {} });

    return rebuildGraph();
}

juce::Array<LaneConfig> AudioEngine::captureLanesWithPluginState()
{
    updateSavedPluginStates();
    capturePluginWindowStates();
    return lanes;
}

juce::Array<DeviceChoice> AudioEngine::getDeviceChoices()
{
    juce::Array<DeviceChoice> result;

    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        type->scanForDevices();
        const auto inputs = type->getDeviceNames (true);
        const auto outputs = type->getDeviceNames (false);

        for (const auto& output : outputs)
        {
            if (inputs.contains (output))
                result.addIfNotAlreadyThere ({ output, output, output });
        }
    }

    const auto current = getCurrentDeviceChoice();
    if (current.inputName.isNotEmpty() && current.outputName.isNotEmpty())
        result.addIfNotAlreadyThere (current);

    return result;
}

juce::Array<double> AudioEngine::getAvailableSampleRates() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getAvailableSampleRates();

    return {};
}

juce::Array<int> AudioEngine::getAvailableBufferSizes() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getAvailableBufferSizes();

    return {};
}

juce::StringArray AudioEngine::getInputChannelNames() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getInputChannelNames();

    return {};
}

juce::StringArray AudioEngine::getOutputChannelNames() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getOutputChannelNames();

    return {};
}

void AudioEngine::refreshDeviceList()
{
    for (auto* type : deviceManager.getAvailableDeviceTypes())
        type->scanForDevices();

    sendChangeMessage();
}

juce::String AudioEngine::selectDevice (const DeviceChoice& choice)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.inputDeviceName = choice.inputName;
    setup.outputDeviceName = choice.outputName;
    configureAllChannels (setup);

    suppressDeviceNotifications = true;
    auto error = deviceManager.setAudioDeviceSetup (setup, true);
    suppressDeviceNotifications = false;

    if (error.isNotEmpty())
        return error;

    if (error = enableAllAvailableChannels(); error.isNotEmpty())
        return error;

    resetXRunCount();
    error = rebuildGraph();
    sendChangeMessage();
    return error;
}

juce::String AudioEngine::setSampleRate (double rate)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.sampleRate = rate;

    suppressDeviceNotifications = true;
    auto error = deviceManager.setAudioDeviceSetup (setup, true);
    suppressDeviceNotifications = false;

    if (error.isNotEmpty())
        return error;

    resetXRunCount();
    error = rebuildGraph();
    sendChangeMessage();
    return error;
}

juce::String AudioEngine::setBufferSize (int size)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.bufferSize = size;

    suppressDeviceNotifications = true;
    auto error = deviceManager.setAudioDeviceSetup (setup, true);
    suppressDeviceNotifications = false;

    if (error.isNotEmpty())
        return error;

    resetXRunCount();
    error = rebuildGraph();
    sendChangeMessage();
    return error;
}

DeviceChoice AudioEngine::getCurrentDeviceChoice() const
{
    const auto setup = deviceManager.getAudioDeviceSetup();
    DeviceChoice result;
    result.inputName = setup.inputDeviceName;
    result.outputName = setup.outputDeviceName;
    result.displayName = result.inputName == result.outputName
                       ? result.inputName
                       : result.inputName + " -> " + result.outputName;
    return result;
}

double AudioEngine::getCurrentSampleRate() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentSampleRate();

    return 0.0;
}

int AudioEngine::getCurrentBufferSize() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentBufferSizeSamples();

    return 0;
}

double AudioEngine::getEstimatedRoundTripMilliseconds() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return estimateRoundTripMilliseconds (device->getCurrentSampleRate(),
                                              device->getCurrentBufferSizeSamples(),
                                              device->getInputLatencyInSamples(),
                                              device->getOutputLatencyInSamples());

    return 0.0;
}

int AudioEngine::getXRunCount() const noexcept
{
    return juce::jmax (0, deviceManager.getXRunCount() - xRunBaseline);
}

void AudioEngine::resetXRunCount()
{
    xRunBaseline = deviceManager.getXRunCount();
    sendChangeMessage();
}

juce::Array<LaneStatus> AudioEngine::getLaneStatuses()
{
    juce::Array<LaneStatus> result;

    for (const auto& runtime : laneRuntimes)
    {
        LaneStatus status;
        status.text = runtime.status;
        status.isDryFallback = runtime.dryFallback;
        status.editorAvailable = runtime.pluginNode != nullptr;
        status.strengthAvailable = runtime.strengthParameter != nullptr;
        status.pluginMuteAvailable = runtime.muteParameter != nullptr;
        status.pluginMuted = runtime.muteParameter != nullptr && runtime.muteParameter->getValue() >= 0.5f;
        status.strengthNormalized = runtime.strengthParameter != nullptr
                                  ? runtime.strengthParameter->getValue()
                                  : 1.0f;
        status.inputPeak = runtime.inputMeter != nullptr ? runtime.inputMeter->consumePeak() : 0.0f;
        status.outputPeak = runtime.outputMeter != nullptr ? runtime.outputMeter->consumePeak() : 0.0f;
        result.add (std::move (status));
    }

    return result;
}

void AudioEngine::openPluginEditor (int laneIndex)
{
    if (! juce::isPositiveAndBelow (laneIndex, static_cast<int> (laneRuntimes.size())))
        return;

    auto node = laneRuntimes[static_cast<size_t> (laneIndex)].pluginNode;
    if (node == nullptr)
        return;

    for (auto* window : pluginWindows)
    {
        if (window->getNode() == node)
        {
            window->setVisible (true);
            window->toFront (true);
            return;
        }
    }

    auto& lane = lanes.getReference (laneIndex);
    lane.editorOpen = true;
    pluginWindows.add (new PluginWindow (
        node,
        pluginWindows,
        laneIndex + 1,
        lane.id,
        lane.editorWindowState,
        [this] (int laneId, const juce::String& windowState)
        {
            for (auto& item : lanes)
            {
                if (item.id == laneId)
                {
                    item.editorOpen = false;
                    item.editorWindowState = windowState;
                    break;
                }
            }

            sendChangeMessage();
        }));
}

void AudioEngine::restorePluginWindows()
{
    reopenSavedPluginWindows();
}

void AudioEngine::setLaneStrength (int laneIndex, float normalizedValue)
{
    if (! juce::isPositiveAndBelow (laneIndex, static_cast<int> (laneRuntimes.size())))
        return;

    if (auto* parameter = laneRuntimes[static_cast<size_t> (laneIndex)].strengthParameter)
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalizedValue));
        parameter->endChangeGesture();
    }
}

void AudioEngine::setLanePluginMuted (int laneIndex, bool shouldMute)
{
    if (! juce::isPositiveAndBelow (laneIndex, static_cast<int> (laneRuntimes.size())))
        return;

    if (auto* parameter = laneRuntimes[static_cast<size_t> (laneIndex)].muteParameter)
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (shouldMute ? 1.0f : 0.0f);
        parameter->endChangeGesture();
    }
}

void AudioEngine::setLaneName (int laneIndex, const juce::String& name)
{
    if (juce::isPositiveAndBelow (laneIndex, lanes.size()))
        lanes.getReference (laneIndex).name = name;
}

void AudioEngine::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (! suppressDeviceNotifications)
        triggerAsyncUpdate();
}

void AudioEngine::handleAsyncUpdate()
{
    if (running)
        rebuildGraph();

    sendChangeMessage();
}

void AudioEngine::locatePlugin()
{
    pluginAvailable = false;
    pluginDiagnostic.clear();

    if (! ensureDeFeedbackAudioUnitRegistered (pluginDiagnostic))
        return;

    auto* format = pluginFormats.getFormat (0);
    if (format == nullptr)
    {
        pluginDiagnostic = "Audio Unit hosting format is unavailable.";
        return;
    }

    juce::OwnedArray<juce::PluginDescription> descriptions;
    format->findAllTypesForFile (descriptions, deFeedbackIdentifier);

    for (const auto* description : descriptions)
    {
        if (description->name.containsIgnoreCase ("De-Feedback")
            || description->fileOrIdentifier.containsIgnoreCase ("FbTI"))
        {
            pluginDescription = *description;
            pluginAvailable = true;
            pluginDiagnostic += " Native Audio Unit " + description->version + " is ready.";
            return;
        }
    }

    pluginDiagnostic += " The AU could not be described by the host; lanes will pass dry.";
}

juce::String AudioEngine::rebuildGraph()
{
    const auto wasRunning = running;
    if (wasRunning)
        deviceManager.removeAudioCallback (&player);

    player.setProcessor (nullptr);
    updateSavedPluginStates();
    closePluginWindows();
    graph.clear();
    laneRuntimes.clear();

    auto* device = deviceManager.getCurrentAudioDevice();
    if (device == nullptr)
    {
        if (wasRunning)
            running = false;
        return "No Core Audio device is open.";
    }

    const auto inputSpan = getActiveInputSpan();
    const auto outputSpan = getActiveOutputSpan();
    const auto sampleRate = device->getCurrentSampleRate();
    const auto blockSize = device->getCurrentBufferSizeSamples();
    graph.setPlayConfigDetails (inputSpan, outputSpan, sampleRate, blockSize);

    auto inputNode = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    auto outputNode = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));

    if (inputNode == nullptr || outputNode == nullptr)
        return "Could not create the Core Audio graph I/O nodes.";

    juce::String firstError;
    std::set<int> usedInputs;
    std::set<int> usedOutputs;

    for (int laneIndex = 0; laneIndex < lanes.size(); ++laneIndex)
    {
        auto& lane = lanes.getReference (laneIndex);
        LaneRuntime runtime;

        if (! juce::isPositiveAndBelow (lane.inputChannel, inputSpan)
            || ! juce::isPositiveAndBelow (lane.outputChannel, outputSpan))
        {
            runtime.status = "INVALID ROUTE";
            runtime.dryFallback = true;
            laneRuntimes.push_back (std::move (runtime));
            continue;
        }

        const auto duplicateInput = usedInputs.contains (lane.inputChannel);
        const auto duplicateOutput = usedOutputs.contains (lane.outputChannel);
        if (duplicateInput || duplicateOutput)
        {
            runtime.status = duplicateInput ? "DUPLICATE INPUT" : "DUPLICATE OUTPUT";
            runtime.dryFallback = true;
            laneRuntimes.push_back (std::move (runtime));
            if (firstError.isEmpty())
                firstError = "Each input and output channel can be assigned to only one lane.";
            continue;
        }

        usedInputs.insert (lane.inputChannel);
        usedOutputs.insert (lane.outputChannel);

        auto inputMeter = std::make_unique<MeterProcessor>();
        runtime.inputMeter = inputMeter.get();
        auto inputMeterNode = graph.addNode (std::move (inputMeter));

        if (inputMeterNode == nullptr
            || ! graph.addConnection ({ { inputNode->nodeID, lane.inputChannel },
                                         { inputMeterNode->nodeID, 0 } }))
        {
            runtime.status = "INVALID INPUT";
            runtime.dryFallback = true;
            runtime.inputMeter = nullptr;
            laneRuntimes.push_back (std::move (runtime));
            if (firstError.isEmpty())
                firstError = "One or more lane inputs could not be connected.";
            continue;
        }

        auto sourceNode = inputMeterNode;
        auto sourceChannel = 0;

        if (pluginAvailable)
        {
            juce::String pluginError;
            auto instance = pluginFormats.createPluginInstance (pluginDescription,
                                                                 sampleRate,
                                                                 blockSize,
                                                                 pluginError);

            if (instance != nullptr && instance->setBusesLayout (monoLayout()))
            {
                if (lane.pluginStateBase64.isNotEmpty())
                {
                    juce::MemoryBlock state;
                    if (state.fromBase64Encoding (lane.pluginStateBase64))
                        instance->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
                }

                runtime.pluginNode = graph.addNode (std::move (instance));
                bindPluginParameters (runtime);

                const auto connectedToPlugin = runtime.pluginNode != nullptr
                                            && graph.addConnection ({ { inputMeterNode->nodeID, 0 },
                                                                       { runtime.pluginNode->nodeID, 0 } });

                if (lane.dry)
                {
                    runtime.status = "BYPASSED - dry pass";
                    runtime.dryFallback = true;
                }
                else if (connectedToPlugin)
                {
                    sourceNode = runtime.pluginNode;
                    runtime.status = "PROCESSED";
                }
                else
                {
                    runtime.pluginNode = nullptr;
                    runtime.status = "DRY - graph connection failed";
                    runtime.dryFallback = true;
                }
            }
            else
            {
                runtime.status = "DRY - " + (pluginError.isNotEmpty() ? pluginError : "mono layout rejected");
                runtime.dryFallback = true;
            }
        }
        else
        {
            runtime.status = lane.dry ? "BYPASSED - dry pass" : "DRY - plugin unavailable";
            runtime.dryFallback = true;
        }

        auto outputMeter = std::make_unique<MeterProcessor> (&emergencyMuted, true);
        runtime.outputMeter = outputMeter.get();
        auto outputMeterNode = graph.addNode (std::move (outputMeter));

        const auto connectedToMeter = outputMeterNode != nullptr
                                   && graph.addConnection ({ { sourceNode->nodeID, sourceChannel },
                                                              { outputMeterNode->nodeID, 0 } });
        const auto connectedToOutput = connectedToMeter
                                    && graph.addConnection ({ { outputMeterNode->nodeID, 0 },
                                                               { outputNode->nodeID, lane.outputChannel } });

        if (! connectedToOutput)
        {
            runtime.status = "INVALID ROUTE";
            runtime.dryFallback = true;
            runtime.outputMeter = nullptr;
            if (firstError.isEmpty())
                firstError = "One or more lane routes could not be connected.";
        }

        laneRuntimes.push_back (std::move (runtime));
    }

    graph.removeIllegalConnections();

    player.setProcessor (&graph);

    if (wasRunning)
        deviceManager.addAudioCallback (&player);

    running = wasRunning;
    reopenSavedPluginWindows();
    sendChangeMessage();
    return firstError;
}

void AudioEngine::closePluginWindows()
{
    capturePluginWindowStates();
    pluginWindows.clear();
}

void AudioEngine::capturePluginWindowStates()
{
    for (auto* window : pluginWindows)
    {
        for (auto& lane : lanes)
        {
            if (lane.id == window->getLaneId())
            {
                lane.editorOpen = true;
                lane.editorWindowState = window->captureWindowState();
                break;
            }
        }
    }
}

void AudioEngine::reopenSavedPluginWindows()
{
    for (int laneIndex = 0; laneIndex < lanes.size(); ++laneIndex)
    {
        if (lanes.getReference (laneIndex).editorOpen)
            openPluginEditor (laneIndex);
    }
}

void AudioEngine::bindPluginParameters (LaneRuntime& runtime)
{
    if (runtime.pluginNode == nullptr)
        return;

    for (auto* parameter : runtime.pluginNode->getProcessor()->getParameters())
    {
        const auto name = parameter->getName (128).trim();

        if (runtime.strengthParameter == nullptr && name.containsIgnoreCase ("strength"))
            runtime.strengthParameter = parameter;

        if (runtime.muteParameter == nullptr && name.containsIgnoreCase ("mute"))
            runtime.muteParameter = parameter;
    }
}

void AudioEngine::updateSavedPluginStates()
{
    const auto count = juce::jmin (lanes.size(), static_cast<int> (laneRuntimes.size()));

    for (int index = 0; index < count; ++index)
    {
        const auto node = laneRuntimes[static_cast<size_t> (index)].pluginNode;
        if (node == nullptr)
            continue;

        juce::MemoryBlock state;
        node->getProcessor()->getStateInformation (state);
        lanes.getReference (index).pluginStateBase64 = state.toBase64Encoding();
    }
}

int AudioEngine::getActiveInputSpan() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return juce::jmax (1, device->getActiveInputChannels().getHighestBit() + 1);

    return 1;
}

int AudioEngine::getActiveOutputSpan() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return juce::jmax (1, device->getActiveOutputChannels().getHighestBit() + 1);

    return 1;
}

void AudioEngine::configureAllChannels (juce::AudioDeviceManager::AudioDeviceSetup& setup) const
{
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = false;
    setup.inputChannels.clear();
    setup.outputChannels.clear();
    setup.inputChannels.setRange (0, bootstrapHardwareChannels, true);
    setup.outputChannels.setRange (0, bootstrapHardwareChannels, true);
}

juce::String AudioEngine::enableAllAvailableChannels()
{
    auto* device = deviceManager.getCurrentAudioDevice();
    if (device == nullptr)
        return "No Core Audio device is open.";

    auto setup = deviceManager.getAudioDeviceSetup();
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = false;
    setup.inputChannels.clear();
    setup.outputChannels.clear();
    setup.inputChannels.setRange (0, device->getInputChannelNames().size(), true);
    setup.outputChannels.setRange (0, device->getOutputChannelNames().size(), true);

    const juce::ScopedValueSetter<bool> guard (suppressDeviceNotifications, true);
    return deviceManager.setAudioDeviceSetup (setup, true);
}
}
