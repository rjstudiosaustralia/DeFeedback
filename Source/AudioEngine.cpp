#include "AudioEngine.h"

#include "PluginRegistration.h"
#include "ModeParameters.h"

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
    pluginMode = config.pluginMode;
    parallelProcessing = config.parallelProcessing;
    maxProcessingThreads = config.maxProcessingThreads;
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

    resetXRunCount();
    return rebuildGraph();
}

juce::String AudioEngine::start()
{
    if (running)
        return {};

    if (deviceManager.getCurrentAudioDevice() == nullptr)
        return "No Core Audio device is open.";

    if (! supportsSampleRate (lanes, getCurrentSampleRate()))
        return "Clear Voice Live requires 48, 96 or 192 kHz. Select a supported sample rate before starting.";

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
    if (newLanes.isEmpty()) return "At least one lane must remain in the rack.";
    if (const auto error = validateExclusiveRoutes (newLanes); error.isNotEmpty()) return error;
    for (const auto& lane : newLanes)
    {
        for (const auto& current : lanes)
            if (lane.id == current.id && lane.pluginMode != current.pluginMode && running)
                return "Stop the audio engine before changing a lane's plugin.";
        if (! plugins.contains (lane.pluginMode)) locatePlugin (lane.pluginMode);
        // A missing new selection must leave the existing rack intact.
        for (const auto& current : lanes)
            if (lane.id == current.id && lane.pluginMode != current.pluginMode
                && ! plugins[lane.pluginMode].available)
                return plugins[lane.pluginMode].diagnostic;
    }
    const auto wasRunning = running;
    stop();
    updateSavedPluginStates (true);
    capturePluginWindowStates();
    auto updated = newLanes;
    for (auto& lane : updated)
        for (const auto& current : lanes)
            if (lane.id == current.id)
            {
                auto fresh = current;
                fresh.selectPlugin (lane.pluginMode);
                fresh.name = lane.name;
                fresh.inputChannel = lane.inputChannel;
                fresh.outputChannel = lane.outputChannel;
                if (lane.pluginMode == current.pluginMode)
                {
                    fresh.dry = lane.dry;
                    fresh.pluginEnabled = lane.pluginEnabled;
                }
                lane = std::move (fresh);
                break;
            }
    // Retire old nodes before changing lane order or plugin identity. Never
    // capture an old instance's state into a different lane or vendor's state.
    player.setProcessor (nullptr);
    closePluginWindows();
    graph.clear();
    laneRuntimes.clear();
    lanes = std::move (updated);
    locatePlugin();
    running = wasRunning;
    return rebuildGraph();
}

juce::Array<LaneConfig> AudioEngine::captureLanesWithPluginState (bool preserveModeControls)
{
    updateSavedPluginStates (preserveModeControls);
    capturePluginWindowStates();
    return lanes;
}

juce::String AudioEngine::setPluginMode (PluginMode mode, const juce::Array<LaneConfig>& nextLanes)
{
    if (running) return "Stop the audio engine before changing plugin mode.";
    auto selected = nextLanes;
    for (auto& lane : selected) lane.selectPlugin (mode);
    const auto error = setLanes (selected);
    if (error.isEmpty()) pluginMode = mode;
    return error;
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
    {
        auto rates = device->getAvailableSampleRates();
        for (int index = rates.size(); --index >= 0;)
            if (! supportsSampleRate (lanes, rates[index]))
                rates.remove (index);
        return rates;
    }

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
    if (! supportsSampleRate (lanes, rate))
        return "Clear Voice Live requires 48, 96 or 192 kHz.";
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

juce::String AudioEngine::setParallelProcessing (bool enabled, int maximumThreads)
{
    if (running) return "Stop the engine before changing parallel processing.";
    if (maximumThreads < 0 || maximumThreads > 64) return "Thread limit must be between 0 (automatic) and 64.";
    parallelProcessing = enabled;
    maxProcessingThreads = maximumThreads;
    return {};
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
        return (device->getCurrentSampleRate() > 0.0
                    ? 1000.0 * graph.getLatencySamples() / device->getCurrentSampleRate() : 0.0)
             + estimateRoundTripMilliseconds (device->getCurrentSampleRate(),
                                              device->getCurrentBufferSizeSamples(),
                                              device->getInputLatencyInSamples(),
                                              device->getOutputLatencyInSamples());

    return 0.0;
}

int AudioEngine::getXRunCount() const noexcept
{
    return juce::jmax (0, deviceManager.getXRunCount() - xRunBaseline);
}

int AudioEngine::getDeviceXRunCount() const noexcept
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto count = device->getXRunCount();
        return count < 0 ? -1 : juce::jmax (0, count - deviceXRunBaseline);
    }
    return -1;
}

void AudioEngine::resetXRunCount()
{
    xRunBaseline = deviceManager.getXRunCount();
    if (auto* device = deviceManager.getCurrentAudioDevice())
        deviceXRunBaseline = juce::jmax (0, device->getXRunCount());
    player.resetMeasurements();
    sendChangeMessage();
}

juce::Array<LaneStatus> AudioEngine::getLaneStatuses()
{
    // A plugin can change latency from its editor. Do not emit misaligned
    // samples while the message thread prepares new delay buffers.
    if (graph.needsLatencyRebuild())
    {
        setEmergencyMuted (true);
        triggerAsyncUpdate();
    }
    juce::Array<LaneStatus> result;

    for (int index = 0; index < static_cast<int> (laneRuntimes.size()); ++index)
    {
        const auto& runtime = laneRuntimes[static_cast<size_t> (index)];
        const auto pluginEnabled = juce::isPositiveAndBelow (index, lanes.size())
                                 ? lanes[index].pluginEnabled
                                 : true;
        LaneStatus status;
        status.text = pluginEnabled ? runtime.status : "PLUGIN OFF - dry pass";
        status.isDryFallback = runtime.dryFallback || ! pluginEnabled;
        status.editorAvailable = pluginEnabled && runtime.pluginNode != nullptr;
        status.strengthAvailable = pluginEnabled && runtime.strengthParameter != nullptr;
        status.pluginMuteAvailable = pluginEnabled && runtime.muteParameter != nullptr;
        status.pluginMuted = pluginEnabled && ! lanes[index].dry && runtime.muteParameter != nullptr && runtime.muteParameter->getValue() >= 0.5f;
        status.pluginEnabled = pluginEnabled;
        status.strengthNormalized = runtime.strengthParameter != nullptr
                                  ? runtime.strengthParameter->getValue()
                                  : 1.0f;
        status.inputPeak = runtime.inputMeter != nullptr ? runtime.inputMeter->consumePeak() : 0.0f;
        status.outputPeak = runtime.outputMeter != nullptr ? runtime.outputMeter->consumePeak() : 0.0f;
        status.dereverbAvailable = pluginEnabled && runtime.dereverbParameter != nullptr;
        status.cpuModeAvailable = pluginEnabled && runtime.cpuParameter != nullptr;
        status.latencyModeAvailable = pluginEnabled && runtime.latencyParameter != nullptr;
        status.dereverbNormalized = runtime.dereverbParameter != nullptr ? runtime.dereverbParameter->getValue() : 0.0f;
        status.highCpu = runtime.cpuParameter != nullptr && runtime.cpuParameter->getValue() >= 0.5f;
        status.lowLatency = runtime.latencyParameter != nullptr && runtime.latencyParameter->getValue() >= 0.5f;
        status.pluginLatencySamples = runtime.pluginNode != nullptr
            ? runtime.pluginNode->getProcessor()->getLatencySamples() : 0;
        if (lanes[index].pluginMode != PluginMode::deFeedback && pluginEnabled && ! runtime.dryFallback
            && runtime.strengthParameter != nullptr && status.strengthNormalized <= 0.0f)
        {
            status.text = "DRY - Strength is 0";
            status.isDryFallback = true;
        }
        if (lanes[index].pluginMode == PluginMode::voiceIso && pluginEnabled && ! runtime.dryFallback
            && runtime.bypassParameter != nullptr && runtime.bypassParameter->getValue() >= 0.5f)
        {
            status.text = "DRY - plugin bypass";
            status.isDryFallback = true;
        }
        result.add (std::move (status));
    }

    return result;
}

void AudioEngine::openPluginEditor (int laneIndex)
{
    if (! juce::isPositiveAndBelow (laneIndex, static_cast<int> (laneRuntimes.size())))
        return;

    if (! juce::isPositiveAndBelow (laneIndex, lanes.size()) || ! lanes[laneIndex].pluginEnabled)
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

juce::String AudioEngine::setLaneClearVoiceControl (int laneIndex, const juce::String& control, float value)
{
    if (! juce::isPositiveAndBelow (laneIndex, static_cast<int> (laneRuntimes.size()))
        || lanes[laneIndex].pluginMode != PluginMode::clearVoice
        || ! std::isfinite (value) || value < 0.0f || value > 1.0f)
        return "Clear Voice control is unavailable or its value is outside 0-1.";
    auto& runtime = laneRuntimes[static_cast<size_t> (laneIndex)];
    auto* parameter = control == "dereverb" ? runtime.dereverbParameter
                    : control == "cpu" ? runtime.cpuParameter
                    : control == "latency" ? runtime.latencyParameter : nullptr;
    if (parameter == nullptr)
        return "The installed Clear Voice AU does not expose this control.";
    if (running && control != "dereverb")
        return "Stop the engine before changing CPU or latency mode.";
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (control == "dereverb" ? juce::jlimit (0.0f, 1.0f, value)
                                                            : (value >= 0.5f ? 1.0f : 0.0f));
    parameter->endChangeGesture();
    if (control != "dereverb") sendChangeMessage();
    return {};
}

void AudioEngine::setLanePluginEnabled (int laneIndex, bool shouldEnable)
{
    if (! juce::isPositiveAndBelow (laneIndex, lanes.size())
        || ! juce::isPositiveAndBelow (laneIndex, static_cast<int> (laneRuntimes.size())))
        return;

    auto& lane = lanes.getReference (laneIndex);
    auto& runtime = laneRuntimes[static_cast<size_t> (laneIndex)];
    if (lane.pluginEnabled == shouldEnable)
        return;

    lane.pluginEnabled = shouldEnable;

    if (runtime.pluginNode != nullptr)
    {
        auto* processor = runtime.pluginNode->getProcessor();
        if (! shouldEnable)
        {
            // Change routing and suspension together between whole callbacks.
            // No state serialization or editor work occurs under this lock.
            {
                const juce::ScopedLock callbackLock (graph.getCallbackLock());
                if (runtime.dryOutput != nullptr)
                    runtime.dryOutput->store (true, std::memory_order_release);
                processor->suspendProcessing (true);
            }
            juce::MemoryBlock state;
            processor->getStateInformation (state);
            lane.pluginStateBase64 = state.toBase64Encoding();
        }
        else
        {
            // Reset while still suspended and dry; release dry only when the
            // next complete graph callback can run the resumed processor.
            processor->reset();
            const juce::ScopedLock callbackLock (graph.getCallbackLock());
            processor->suspendProcessing (false);
            if (runtime.dryOutput != nullptr)
                runtime.dryOutput->store (false, std::memory_order_release);
        }

        if (! shouldEnable)
        {
            for (int windowIndex = pluginWindows.size(); --windowIndex >= 0;)
            {
                auto* window = pluginWindows[windowIndex];
                if (window->getNode() == runtime.pluginNode)
                {
                    lane.editorOpen = true;
                    lane.editorWindowState = window->captureWindowState();
                    pluginWindows.remove (windowIndex);
                }
            }
        }
        else if (lane.editorOpen)
            openPluginEditor (laneIndex);
    }

    else if (runtime.dryOutput != nullptr)
        runtime.dryOutput->store (! shouldEnable, std::memory_order_release);
    sendChangeMessage();
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
    pluginAvailable = true;
    pluginDiagnostic.clear();
    std::set<PluginMode> selected;
    for (const auto& lane : lanes) selected.insert (lane.pluginMode);
    for (const auto mode : selected)
    {
        if (! plugins.contains (mode) || ! plugins[mode].available) locatePlugin (mode);
        pluginAvailable = pluginAvailable && plugins[mode].available;
        if (pluginDiagnostic.isNotEmpty()) pluginDiagnostic += " | ";
        pluginDiagnostic += pluginModeName (mode) + ": " + plugins[mode].diagnostic;
    }
}

void AudioEngine::locatePlugin (PluginMode mode)
{
    auto& info = plugins[mode];
    info.available = false;
    info.diagnostic.clear();

    const auto registered = mode == PluginMode::deFeedback
        ? ensureDeFeedbackAudioUnitRegistered (info.diagnostic)
        : mode == PluginMode::voiceIso ? ensureVoiceIsoAudioUnitRegistered (info.diagnostic)
        : ensureClearVoiceAudioUnitRegistered (info.diagnostic);
    if (! registered)
        return;

    auto* format = pluginFormats.getFormat (0);
    if (format == nullptr)
    {
        info.diagnostic = "Audio Unit hosting format is unavailable.";
        return;
    }

    juce::OwnedArray<juce::PluginDescription> descriptions;
    const auto identifier = mode == PluginMode::deFeedback ? deFeedbackIdentifier
        : mode == PluginMode::voiceIso ? juce::String ("AudioUnit:Effects/aufx,Viso,Maze")
        : juce::String ("AudioUnit:Effects/aufx,Cv11,FabS");
    format->findAllTypesForFile (descriptions, identifier);

    for (const auto* description : descriptions)
    {
        if (mode == PluginMode::deFeedback
                ? (description->name.containsIgnoreCase ("De-Feedback")
                   || description->fileOrIdentifier.containsIgnoreCase ("FbTI"))
                : description->fileOrIdentifier == identifier)
        {
            info.description = *description;
            info.available = true;
            info.diagnostic += " Native Audio Unit " + description->version + " is ready.";
            return;
        }
    }

    info.diagnostic += " The AU could not be described by the host; lanes will pass dry.";
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

    graph.configure (parallelProcessing, deviceManager.getDeviceAudioWorkgroup(), maxProcessingThreads);

    juce::String firstError;
    std::set<int> usedInputs;
    std::set<int> usedOutputs;

    for (int laneIndex = 0; laneIndex < lanes.size(); ++laneIndex)
    {
        auto& lane = lanes.getReference (laneIndex);
        LaneRuntime runtime;
        runtime.dryOutput = std::make_shared<std::atomic<bool>> (! lane.pluginEnabled);

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

        auto monoGraph = std::make_unique<juce::AudioProcessorGraph>();
        auto& laneGraph = *monoGraph;
        laneGraph.setPlayConfigDetails (1, 1, sampleRate, blockSize);
        auto inputNode = laneGraph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
        auto outputNode = laneGraph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));

        auto inputMeter = std::make_unique<MeterProcessor>();
        runtime.inputMeter = inputMeter.get();
        auto inputMeterNode = laneGraph.addNode (std::move (inputMeter));

        if (inputNode == nullptr || outputNode == nullptr || inputMeterNode == nullptr
            || ! laneGraph.addConnection ({ { inputNode->nodeID, 0 },
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

        if (! supportsSampleRate (lane.pluginMode, sampleRate))
        {
            runtime.status = "DRY - unsupported sample rate";
            runtime.dryFallback = true;
        }
        else if (plugins[lane.pluginMode].available)
        {
            juce::String pluginError;
            auto instance = pluginFormats.createPluginInstance (plugins[lane.pluginMode].description,
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

                if (lane.modeParameterState.isNotEmpty())
                    restoreModeParameters (*instance, lane.pluginMode, lane.modeParameterState);

                runtime.pluginNode = laneGraph.addNode (std::move (instance));
                bindPluginParameters (runtime, lane.pluginMode);

                if (lane.pluginMode == PluginMode::clearVoice && lane.pluginStateBase64.isEmpty()
                    && lane.modeParameterState.isEmpty())
                {
                    // Vendor's "Good starting point": LOW CPU, ZERO latency,
                    // Strength 3, Dereverb 6 (the AU values are normalized).
                    const auto set = [] (juce::AudioProcessorParameter* parameter, float value)
                    {
                        if (parameter != nullptr) parameter->setValueNotifyingHost (value);
                    };
                    set (runtime.strengthParameter, 0.3f);
                    set (runtime.dereverbParameter, 0.6f);
                    set (runtime.cpuParameter, 0.0f);
                    set (runtime.latencyParameter, 0.0f);
                }

                if (runtime.pluginNode != nullptr)
                    runtime.pluginNode->getProcessor()->suspendProcessing (! lane.pluginEnabled);

                const auto connectedToPlugin = runtime.pluginNode != nullptr
                                            && laneGraph.addConnection ({ { inputMeterNode->nodeID, 0 },
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

        auto outputMeter = std::make_unique<MeterProcessor> (&emergencyMuted,
                                                              true,
                                                              runtime.dryOutput.get());
        runtime.outputMeter = outputMeter.get();
        auto outputMeterNode = laneGraph.addNode (std::move (outputMeter));

        const auto connectedToMeter = outputMeterNode != nullptr
                                   && laneGraph.addConnection ({ { sourceNode->nodeID, sourceChannel },
                                                              { outputMeterNode->nodeID, 0 } })
                                    && laneGraph.addConnection ({ { inputMeterNode->nodeID, 0 },
                                                              { outputMeterNode->nodeID, 1 } });
        const auto connectedToOutput = connectedToMeter
                                    && laneGraph.addConnection ({ { outputMeterNode->nodeID, 0 },
                                                               { outputNode->nodeID, 0 } });

        if (! connectedToOutput)
        {
            runtime.status = "INVALID ROUTE";
            runtime.dryFallback = true;
            runtime.outputMeter = nullptr;
            if (firstError.isEmpty())
                firstError = "One or more lane routes could not be connected.";
        }

        laneGraph.removeIllegalConnections();
        graph.addLane (std::move (monoGraph), lane.inputChannel, lane.outputChannel);
        laneRuntimes.push_back (std::move (runtime));
    }

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
        if (lanes.getReference (laneIndex).pluginEnabled
            && lanes.getReference (laneIndex).editorOpen)
            openPluginEditor (laneIndex);
    }
}

void AudioEngine::bindPluginParameters (LaneRuntime& runtime, PluginMode mode)
{
    if (runtime.pluginNode == nullptr)
        return;

    for (auto* parameter : runtime.pluginNode->getProcessor()->getParameters())
    {
        const auto name = parameter->getName (128).trim();

        if (mode == PluginMode::voiceIso)
        {
            // VoiceISO 1.0.6 publishes Amount (0..1 = 0..120%) and Bypass.
            // Full/Lite is private editor state; preserve its opaque AU bank.
            if (name == "Amount") runtime.strengthParameter = parameter;
            if (name == "Bypass") runtime.bypassParameter = parameter;
            continue;
        }

        if (runtime.strengthParameter == nullptr && name.containsIgnoreCase ("strength"))
            runtime.strengthParameter = parameter;

        if (runtime.muteParameter == nullptr && name.containsIgnoreCase ("mute"))
            runtime.muteParameter = parameter;

        if (mode == PluginMode::clearVoice)
        {
            // Exact published AU names: legacy Dereverb Boost and Voice Mode
            // are different parameters and must never be used for these controls.
            if (name == "Dereverb") runtime.dereverbParameter = parameter;
            if (name == "Engine") runtime.cpuParameter = parameter;
            if (name == "Latency") runtime.latencyParameter = parameter;
        }
    }
}

void AudioEngine::updateSavedPluginStates (bool /*preserveModeControls*/)
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
        lanes.getReference (index).modeParameterState = captureModeParameters (*node->getProcessor(), lanes[index].pluginMode);
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
