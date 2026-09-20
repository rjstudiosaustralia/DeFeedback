#include "../Source/AudioEngine.h"
#include "../Source/ModeParameters.h"
#include <iostream>
#include <numeric>

namespace defeedback
{
namespace
{
int failures = 0;
void expect (bool condition, const juce::String& message)
{
    if (! condition) { ++failures; std::cerr << "FAIL: " << message << std::endl; }
}
void dispatch() { juce::MessageManager::getInstance()->runDispatchLoopUntil (20); }

// Exercises the production AudioDeviceManager -> player -> graph callback with
// real installed plugins. This device never opens hardware or emits sound.
class MemoryDevice final : public juce::AudioIODevice
{
public:
    MemoryDevice() : AudioIODevice ("Offline", "Offline") {}
    juce::StringArray getOutputChannelNames() override
    {
        juce::StringArray names;
        for (int i = 0; i < 16; ++i) names.add (juce::String (i + 1));
        return names;
    }
    juce::StringArray getInputChannelNames() override { return getOutputChannelNames(); }
    juce::Array<double> getAvailableSampleRates() override { return { 44100, 48000, 88200, 96000, 176400, 192000 }; }
    juce::Array<int> getAvailableBufferSizes() override { return { 32, 64, 128, 256, 512 }; }
    int getDefaultBufferSize() override { return 64; }
    juce::String open (const juce::BigInteger& in, const juce::BigInteger& out, double sr, int bs) override
    {
        inputs = in; outputs = out; inputs.setRange (16, 256, false); outputs.setRange (16, 256, false);
        sampleRate = sr; blockSize = bs; opened = true; return {};
    }
    void close() override { stop(); opened = false; }
    bool isOpen() override { return opened; }
    void start (juce::AudioIODeviceCallback* c) override { callback = c; if (c) c->audioDeviceAboutToStart (this); }
    void stop() override { if (callback) callback->audioDeviceStopped(); callback = nullptr; }
    bool isPlaying() override { return callback != nullptr; }
    juce::String getLastError() override { return {}; }
    int getCurrentBufferSizeSamples() override { return blockSize; }
    double getCurrentSampleRate() override { return sampleRate; }
    int getCurrentBitDepth() override { return 32; }
    juce::BigInteger getActiveOutputChannels() const override { return outputs; }
    juce::BigInteger getActiveInputChannels() const override { return inputs; }
    int getOutputLatencyInSamples() override { return blockSize; }
    int getInputLatencyInSamples() override { return blockSize; }
    juce::AudioIODeviceCallback* callback = nullptr;
    juce::BigInteger inputs, outputs;
    double sampleRate = 48000;
    int blockSize = 64;
    bool opened = false;
};
class MemoryDeviceType final : public juce::AudioIODeviceType
{
public:
    MemoryDeviceType() : AudioIODeviceType ("Offline") {}
    void scanForDevices() override {}
    juce::StringArray getDeviceNames (bool) const override { return { "Offline" }; }
    int getDefaultDeviceIndex (bool) const override { return 0; }
    int getIndexOfDevice (juce::AudioIODevice*, bool) const override { return 0; }
    bool hasSeparateInputsAndOutputs() const override { return false; }
    juce::AudioIODevice* createDevice (const juce::String&, const juce::String&) override { return new MemoryDevice(); }
};
}

class AudioEngineIntegrationTest
{
public:
    static int runMixed()
    {
        AppConfig config;
        config.inputDeviceName = config.outputDeviceName = "Offline";
        config.autoStart = config.launchAtLogin = false;
        config.lanes.clear();
        config.lanes.add ({ 1, "Feedback", 0, 3, false, {}, false, {} });
        config.lanes.add ({ 2, "Clear", 1, 4, false, {}, false, {} });
        config.lanes.add ({ 3, "ISO", 2, 5, false, {}, false, {} });
        config.lanes.getReference (1).selectPlugin (PluginMode::clearVoice);
        config.lanes.getReference (2).selectPlugin (PluginMode::voiceIso);
        {
            AudioEngine engine;
            engine.deviceManager.addAudioDeviceType (std::make_unique<MemoryDeviceType>());
            expect (engine.initialise (config).isEmpty(), "initialise mixed rack");
            dispatch();
            auto statuses = engine.getLaneStatuses();
            expect (statuses[0].pluginMuteAvailable && ! statuses[0].dereverbAvailable,
                    "De-Feedback line exposes only its own controls");
            expect (statuses[1].dereverbAvailable && statuses[1].cpuModeAvailable && ! statuses[1].pluginMuteAvailable,
                    "Clear Voice line exposes its controls in mixed rack");
            expect (statuses[2].strengthAvailable && ! statuses[2].pluginMuteAvailable
                    && ! statuses[2].dereverbAvailable, "VoiceISO line exposes only Amount");
            if (! statuses[0].strengthAvailable || ! statuses[1].strengthAvailable || ! statuses[2].strengthAvailable)
                return 1;
            for (int i = 0; i < 3; ++i) engine.setLaneStrength (i, 0.2f + static_cast<float> (i) * 0.2f);
            expect (engine.setLaneClearVoiceControl (0, "dereverb", 0.5f).isNotEmpty(), "wrong lane rejects CV control");
            expect (engine.setLaneClearVoiceControl (1, "dereverb", 0.73f).isEmpty(), "CV control targets correct lane");
            expect (engine.setSampleRate (44100).isNotEmpty(), "mixed CV rack rejects unsupported rate");
            expect (engine.start().isEmpty(), "start all three plugins together");
            dispatch();
            pump (engine, 200);
            auto changed = engine.captureLanesWithPluginState();
            changed.getReference (0).selectPlugin (PluginMode::voiceIso);
            expect (engine.setLanes (changed).isNotEmpty(), "running rack rejects plugin reassignment");
            for (int i = 0; i < 3; ++i) engine.setLanePluginEnabled (i, false);
            pump (engine, 100, false, false, false, true);
            engine.setEmergencyMuted (true);
            pump (engine, 100, true);
            engine.stop();
            for (int i = 0; i < 3; ++i) engine.setLanePluginEnabled (i, true);
            for (int cycle = 0; cycle < 3; ++cycle)
            {
                auto next = engine.captureLanesWithPluginState();
                next.getReference (0).selectPlugin (PluginMode::voiceIso);
                expect (engine.setLanes (next).isEmpty(), "change one plugin in mixed rack");
                dispatch();
                auto status = engine.getLaneStatuses();
                expect (std::abs (status[1].dereverbNormalized - 0.73f) < 0.001f
                        && std::abs (status[2].strengthNormalized - 0.6f) < 0.001f,
                        "unaffected plugins retain latest controls");
                next = engine.captureLanesWithPluginState();
                next.getReference (0).selectPlugin (PluginMode::deFeedback);
                expect (engine.setLanes (next).isEmpty(), "restore line's original plugin");
                dispatch();
                expect (std::abs (engine.getLaneStatuses()[0].strengthNormalized - 0.2f) < 0.001f,
                        "per-line saved plugin controls restored");
            }
            auto reordered = engine.captureLanesWithPluginState();
            reordered.swap (0, 2);
            expect (engine.setLanes (reordered).isEmpty(), "reorder mixed lanes by stable id");
            dispatch();
            expect (std::abs (engine.getLaneStatuses()[0].strengthNormalized - 0.6f) < 0.001f,
                    "reorder cannot cross-write plugin state");
            config.lanes = engine.captureLanesWithPluginState();
        }
        config = AppConfig::fromXml (*config.toXml());
        AudioEngine restored;
        restored.deviceManager.addAudioDeviceType (std::make_unique<MemoryDeviceType>());
        expect (restored.initialise (config).isEmpty(), "restart mixed rack from XML");
        dispatch();
        expect (std::abs (restored.getLaneStatuses()[0].strengthNormalized - 0.6f) < 0.001f
                && std::abs (restored.getLaneStatuses()[1].dereverbNormalized - 0.73f) < 0.001f,
                "fresh engine restores mixed plugin controls");
        config.lanes = restored.captureLanesWithPluginState();
        config.switchPluginMode (PluginMode::voiceIso);
        expect (restored.setLanes (config.lanes).isEmpty(), "apply one plugin to all lanes");
        expect (restored.setSampleRate (44100).isEmpty(), "removing CV restores other supported rates");
        std::cout << (failures == 0 ? "All mixed-plugin integration tests passed.\n" : "Mixed-plugin failures.\n");
        return failures == 0 ? 0 : 1;
    }

    static int runPassThrough()
    {
        AudioEngine engine;
        engine.deviceManager.addAudioDeviceType (std::make_unique<MemoryDeviceType>());
        AppConfig config;
        config.inputDeviceName = config.outputDeviceName = "Offline";
        config.autoStart = config.launchAtLogin = false;
        config.lanes = { { 1, "Pass-through", 2, 3, false, {}, false, {} } };
        expect (engine.initialise (config).isEmpty(), "initialise dry pass tests");
        dispatch();
        for (auto mode : { PluginMode::deFeedback, PluginMode::clearVoice, PluginMode::voiceIso })
        {
            config.switchPluginMode (mode);
            expect (engine.setPluginMode (mode, config.lanes).isEmpty(), "select mode for OFF/BYPASS test");
            dispatch();
            expect (engine.getLaneStatuses()[0].strengthAvailable, "real plugin loaded for OFF/BYPASS test");
            if (mode == PluginMode::clearVoice)
                expect (engine.setLaneClearVoiceControl (0, "latency", 1.0f).isEmpty(), "test OFF with LOW latency compensation");
            engine.setLanePluginMuted (0, true);
            expect (engine.start().isEmpty(), "start pass-through tests");
            dispatch();
            auto node = engine.laneRuntimes[0].pluginNode;
            for (int cycle = 0; cycle < 4; ++cycle)
            {
                engine.setLanePluginEnabled (0, false);
                expect (node == engine.laneRuntimes[0].pluginNode && node->getProcessor()->isSuspended(),
                        "OFF suspends original instance without graph reconstruction");
                auto status = engine.getLaneStatuses()[0];
                expect (status.isDryFallback && ! status.pluginMuted && status.text == "PLUGIN OFF - dry pass",
                        "OFF reports dry audio even if the plugin's Mute is set");
                pump (engine, 100, false, false, false, true);
                expect (engine.getLaneStatuses()[0].outputPeak > 0.0f, "OFF output meter remains live");
                engine.setEmergencyMuted (true);
                pump (engine, 40, true);
                engine.setEmergencyMuted (false);
                pump (engine, 40, false, false, false, true);
                engine.setLanePluginEnabled (0, true);
                expect (! node->getProcessor()->isSuspended(), "ON resumes original instance");
                pump (engine, 20);
            }
            engine.setLanePluginEnabled (0, false);
            engine.stop();
            expect (engine.start().isEmpty(), "restore OFF across graph reconstruction");
            dispatch();
            pump (engine, 100, false, false, false, true);
            engine.stop();
            config.lanes = engine.captureLanesWithPluginState (true);
            config.lanes.getReference (0).pluginEnabled = true;
            config.lanes.getReference (0).dry = true;
            expect (engine.setLanes (config.lanes).isEmpty() && engine.start().isEmpty(), "BYPASS routes dry with plugin Mute retained");
            dispatch();
            pump (engine, 100, false, false, false, true);
            expect (! engine.getLaneStatuses()[0].pluginMuted, "BYPASS status does not claim plugin mute silences dry audio");
            engine.setLanePluginEnabled (0, false);
            pump (engine, 40, false, false, false, true);
            engine.setLanePluginEnabled (0, true);
            pump (engine, 40, false, false, false, true);
            engine.stop();
            config.lanes = engine.captureLanesWithPluginState (true);
            std::cout << pluginModeName (mode) << ": OFF, BYPASS, retained state, dry meters and master mute passed.\n";
        }
        return failures == 0 ? 0 : 1;
    }

    static int runVoiceIso (bool editor)
    {
        AudioEngine engine;
        engine.deviceManager.addAudioDeviceType (std::make_unique<MemoryDeviceType>());
        AppConfig config;
        config.inputDeviceName = config.outputDeviceName = "Offline";
        config.autoStart = config.launchAtLogin = false;
        config.switchPluginMode (PluginMode::voiceIso);
        expect (engine.initialise (config).isEmpty(), "initialise VoiceISO on memory device");
        dispatch();
        auto status = engine.getLaneStatuses()[0];
        expect (engine.isPluginAvailable() && status.strengthAvailable && status.editorAvailable,
                "VoiceISO Amount and native editor bound");
        expect (! status.pluginMuteAvailable && ! status.cpuModeAvailable && ! status.dereverbAvailable,
                "VoiceISO does not inherit other vendors' controls");
        if (! status.strengthAvailable) return 1;
        std::cout << engine.getPluginDiagnostic() << std::endl;
        expect (std::abs (status.strengthNormalized * 120.0f - 100.0f) < 0.01f,
                "VoiceISO factory Amount maps to 100%, not full normalized range");
        if (editor)
        {
            engine.openPluginEditor (0);
            std::cout << "Offline editor QA: touch /private/tmp/voiceiso-rebuild to save and restore via other modes; touch /private/tmp/voiceiso-finish to exit." << std::endl;
            const auto deadline = juce::Time::getMillisecondCounterHiRes() + 600000.0;
            while (juce::Time::getMillisecondCounterHiRes() < deadline)
            {
                dispatch();
                const juce::File rebuild ("/private/tmp/voiceiso-rebuild"), finish ("/private/tmp/voiceiso-finish");
                if (finish.existsAsFile()) { finish.deleteFile(); break; }
                if (! rebuild.existsAsFile()) continue;
                rebuild.deleteFile();
                config.lanes = engine.captureLanesWithPluginState (true);
                juce::File ("/private/tmp/voiceiso-editor-state.txt").replaceWithText (config.lanes[0].pluginStateBase64);
                for (auto mode : { PluginMode::deFeedback, PluginMode::clearVoice, PluginMode::voiceIso })
                {
                    config.switchPluginMode (mode);
                    expect (engine.setPluginMode (mode, config.lanes).isEmpty(), "editor QA mode swap");
                    dispatch();
                    config.lanes = engine.captureLanesWithPluginState (true);
                }
                std::cout << "Saved and restored VoiceISO through both other modes." << std::endl;
            }
            return failures == 0 ? 0 : 1;
        }
        for (float amount : { 0.0f, 0.5f, 100.0f / 120.0f, 1.0f })
        {
            engine.setLaneStrength (0, amount);
            dispatch();
            expect (std::abs (engine.getLaneStatuses()[0].strengthNormalized - amount) < 0.001f,
                    "VoiceISO Amount supports complete 0..120% range");
        }
        auto* bypass = engine.laneRuntimes[0].bypassParameter;
        expect (bypass != nullptr, "VoiceISO native Bypass bound separately from Mute");
        if (! bypass) return 1;
        bypass->setValueNotifyingHost (1.0f);
        expect (engine.getLaneStatuses()[0].text == "DRY - plugin bypass", "native bypass is reported");
        expect (engine.start().isEmpty(), "start bypassed VoiceISO");
        dispatch();
        pump (engine, 100, false, false, false, true);
        engine.stop();
        engine.laneRuntimes[0].bypassParameter->setValueNotifyingHost (0.0f);
        engine.setLaneStrength (0, 0.0f);
        expect (engine.getLaneStatuses()[0].isDryFallback, "zero Amount is reported dry");
        expect (engine.start().isEmpty(), "start zero-strength VoiceISO");
        dispatch();
        pump (engine, 100, false, false, false, true);
        engine.stop();
        engine.setLaneStrength (0, 0.625f);
        expect (engine.start().isEmpty(), "start VoiceISO processing");
        dispatch();
        pump (engine, 1000, false, false, true);
        expect (engine.getLaneStatuses()[0].pluginLatencySamples == 0, "installed VoiceISO reports zero plugin latency");
        expect (engine.setPluginMode (PluginMode::deFeedback, AppConfig().lanes).isNotEmpty(),
                "running VoiceISO rejects mode switch");
        engine.setEmergencyMuted (true);
        pump (engine, 100, true);
        engine.setEmergencyMuted (false);
        engine.setLanePluginEnabled (0, false);
        pump (engine, 100, false, false, false, true);
        expect (engine.laneRuntimes[0].pluginNode->getProcessor()->isSuspended(), "VoiceISO off suspends DSP");
        engine.setLanePluginEnabled (0, true);
        engine.stop();
        config.lanes = engine.captureLanesWithPluginState (true);
        config.lanes.getReference (0).name = "VoiceISO test";
        config.lanes.getReference (0).inputChannel = 2;
        config.lanes.getReference (0).outputChannel = 3;
        expect (engine.setLanes (config.lanes).isEmpty(), "VoiceISO repatch");
        dispatch();
        expect (std::abs (engine.getLaneStatuses()[0].strengthNormalized - 0.625f) < 0.001f,
                "VoiceISO repatch retains Amount");
        config.lanes = engine.captureLanesWithPluginState (true);
        for (int cycle = 0; cycle < 3; ++cycle)
            for (auto mode : { PluginMode::deFeedback, PluginMode::clearVoice, PluginMode::voiceIso })
            {
                config.switchPluginMode (mode);
                config = AppConfig::fromXml (*config.toXml());
                expect (engine.setPluginMode (mode, config.lanes).isEmpty(), "three real AU modes restore after XML restart");
                dispatch();
                if (mode == PluginMode::voiceIso)
                    expect (std::abs (engine.getLaneStatuses()[0].strengthNormalized - 0.625f) < 0.001f
                            && engine.lanes[0].inputChannel == 2 && engine.lanes[0].outputChannel == 3,
                            "VoiceISO routing and Amount retained across both other vendors");
                config.lanes = engine.captureLanesWithPluginState (true);
            }
        for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
        {
            expect (engine.setSampleRate (rate).isEmpty(), "VoiceISO rate rebuild");
            for (int buffer : { 32, 64, 128, 512 })
            {
                expect (engine.setBufferSize (buffer).isEmpty(), "VoiceISO buffer rebuild");
                expect (engine.start().isEmpty(), "VoiceISO rate/buffer start");
                dispatch();
                expect (! engine.getLaneStatuses()[0].isDryFallback, "VoiceISO rate/buffer supports mono processing");
                pump (engine, 30);
                engine.stop();
            }
        }
        engine.setSampleRate (48000);
        engine.setBufferSize (64);
        juce::Array<LaneConfig> rack;
        for (int i = 0; i < 4; ++i) rack.add ({ i + 1, "VoiceISO", i, i, false, {}, false, {} });
        for (auto& lane : rack) lane.selectPlugin (PluginMode::voiceIso);
        expect (engine.setLanes (rack).isEmpty(), "four independent VoiceISO lanes");
        engine.setLaneStrength (1, 0.25f);
        expect (std::abs (engine.getLaneStatuses()[0].strengthNormalized - 0.625f) < 0.001f,
                "changing lane 2 does not change lane 1");
        expect (engine.start().isEmpty(), "start VoiceISO rack");
        dispatch();
        pump (engine, 200);
        engine.stop();
        rack = engine.captureLanesWithPluginState (true);
        for (auto& lane : rack) lane.dry = true;
        expect (engine.setLanes (rack).isEmpty() && engine.start().isEmpty(), "host dry bypass for VoiceISO rack");
        dispatch();
        pump (engine, 100, false, false, false, true);
        engine.stop();
        std::cout << (failures == 0 ? "All VoiceISO offline integration tests passed.\n" : "VoiceISO integration failures.\n");
        return failures == 0 ? 0 : 1;
    }

    static int run (bool measureLoad)
    {
        AudioEngine engine;
        engine.deviceManager.addAudioDeviceType (std::make_unique<MemoryDeviceType>());
        AppConfig config;
        config.inputDeviceName = config.outputDeviceName = "Offline";
        config.autoStart = config.launchAtLogin = false;
        expect (engine.initialise (config).isEmpty(), "initialise legacy De-Feedback graph");
        dispatch();
        auto status = engine.getLaneStatuses()[0];
        expect (status.strengthAvailable && status.pluginMuteAvailable, "De-Feedback controls bound");
        expect (engine.start().isEmpty(), "start legacy graph");
        dispatch();
        engine.setLaneStrength (0, 0.57f);
        engine.setLanePluginMuted (0, true);
        pump (engine, 100);
        engine.setLanePluginEnabled (0, false);
        engine.stop();
        config.lanes = engine.captureLanesWithPluginState (true);
        const auto originalBank = config.lanes;
        const auto originalStatus = engine.getLaneStatuses()[0];
        std::cout << "Original De-Feedback strength=" << originalStatus.strengthNormalized << " muted=" << originalStatus.pluginMuted << " enabled=" << originalStatus.pluginEnabled << std::endl;
        config.switchPluginMode (PluginMode::clearVoice);
        expect (engine.setPluginMode (config.pluginMode, config.lanes).isEmpty(), "switch to Clear Voice");
        dispatch();
        status = engine.getLaneStatuses()[0];
        expect (status.strengthAvailable && status.dereverbAvailable && status.cpuModeAvailable
                && status.latencyModeAvailable && ! status.pluginMuteAvailable, "Clear Voice controls bound exactly");
        expect (std::abs (status.strengthNormalized - 0.3f) < 0.001f
                && std::abs (status.dereverbNormalized - 0.6f) < 0.001f
                && ! status.highCpu && ! status.lowLatency, "documented first-use preset");
        restoreModeParameters (*engine.laneRuntimes[0].pluginNode->getProcessor(), PluginMode::clearVoice, originalBank[0].modeParameterState);
        expect (std::abs (engine.getLaneStatuses()[0].strengthNormalized - 0.3f) < 0.001f, "wrong-vendor parameter snapshot rejected");
        expect (! engine.getAvailableSampleRates().contains (44100), "unsupported rates hidden");
        expect (engine.setSampleRate (44100).isNotEmpty() && std::abs (engine.getCurrentSampleRate() - 48000) < 0.01,
                "unsupported sample rate cannot alter device");
        engine.setLaneStrength (0, 0.42f);
        expect (engine.setLaneClearVoiceControl (0, "dereverb", -1).isNotEmpty()
                && engine.setLaneClearVoiceControl (0, "cpu", 2).isNotEmpty()
                && engine.setLaneClearVoiceControl (0, "unknown", 0).isNotEmpty(), "invalid Clear Voice controls rejected");
        expect (engine.setLaneClearVoiceControl (0, "dereverb", 0.73f).isEmpty(), "set Dereverb");
        expect (engine.setLaneClearVoiceControl (0, "cpu", 1).isEmpty(), "set CPU while stopped");
        expect (engine.setLaneClearVoiceControl (0, "latency", 1).isEmpty(), "set latency while stopped");
        expect (engine.start().isEmpty(), "start Clear Voice");
        dispatch();
        expect (engine.setLaneClearVoiceControl (0, "cpu", 0).isNotEmpty(), "live CPU change rejected");
        expect (engine.setLaneClearVoiceControl (0, "latency", 0).isNotEmpty(), "live latency change rejected");
        expect (engine.setPluginMode (PluginMode::deFeedback, originalBank).isNotEmpty(), "live plugin swap rejected");
        pump (engine, 1000, false, false, true);
        status = engine.getLaneStatuses()[0];
        std::cout << "Clear Voice LOW latency reports " << status.pluginLatencySamples << " samples\n";
        expect (status.pluginLatencySamples > 0, "plugin latency reported to host");
        expect (engine.getEstimatedRoundTripMilliseconds() > 2.67, "host latency includes Clear Voice delay");
        engine.setEmergencyMuted (true);
        pump (engine, 100, true);
        engine.setEmergencyMuted (false);
        engine.setLanePluginEnabled (0, false);
        pump (engine, 100, false, false, false, true);
        expect (engine.laneRuntimes[0].pluginNode->getProcessor()->isSuspended(), "inactive DSP suspended");
        engine.setLanePluginEnabled (0, true);
        pump (engine, 100);
        engine.stop();
        expect (engine.setLaneClearVoiceControl (0, "cpu", 0).isEmpty(), "restore LOW CPU");
        expect (engine.setLaneClearVoiceControl (0, "latency", 0).isEmpty(), "restore ZERO latency");
        auto clearBank = engine.captureLanesWithPluginState();
        auto patch = clearBank;
        patch.getReference (0).inputChannel = 2;
        patch.getReference (0).outputChannel = 3;
        expect (engine.setLanes (patch).isEmpty(), "repatch Clear Voice");
        dispatch();
        status = engine.getLaneStatuses()[0];
        expect (std::abs (status.strengthNormalized - 0.42f) < 0.001f
                && std::abs (status.dereverbNormalized - 0.73f) < 0.001f, "routing rebuild preserves controls");
        clearBank = engine.captureLanesWithPluginState();
        for (int cycle = 0; cycle < 8; ++cycle)
        {
            expect (engine.setPluginMode (PluginMode::deFeedback, originalBank).isEmpty(), "restore De-Feedback");
            dispatch();
            status = engine.getLaneStatuses()[0];
            std::cout << "Restored De-Feedback strength=" << status.strengthNormalized << " muted=" << status.pluginMuted << " enabled=" << status.pluginEnabled << std::endl;
            expect (std::abs (status.strengthNormalized - originalStatus.strengthNormalized) < 0.001f
                    && status.pluginMuted == originalStatus.pluginMuted && status.pluginEnabled == originalStatus.pluginEnabled, "De-Feedback controls/activity restored exactly");
            expect (engine.setPluginMode (PluginMode::clearVoice, clearBank).isEmpty(), "restore Clear Voice");
            dispatch();
            status = engine.getLaneStatuses()[0];
            expect (std::abs (status.dereverbNormalized - 0.73f) < 0.001f, "Clear Voice roundtrip control");
        }
        for (double rate : { 48000.0, 96000.0, 192000.0 })
        {
            expect (engine.setSampleRate (rate).isEmpty(), "supported rate rebuild");
            for (int buffer : { 32, 64, 128, 512 })
            {
                expect (engine.setBufferSize (buffer).isEmpty(), "buffer rebuild");
                expect (engine.start().isEmpty(), "start at supported rate/buffer");
                dispatch();
                pump (engine, 200);
                engine.stop();
            }
        }
        // Exercise a realistic rack and measure callback time, after model warmup.
        engine.setSampleRate (48000);
        engine.setBufferSize (64);
        juce::Array<LaneConfig> rack;
        for (int i = 0; i < 16; ++i) rack.add ({ i + 1, "Test", i, i, false, {}, false, {} });
        for (auto& lane : rack) lane.selectPlugin (PluginMode::clearVoice);
        expect (engine.setLanes (rack).isEmpty(), "16 independent mono instances");
        expect (engine.start().isEmpty(), "start 16-lane rack");
        dispatch();
        if (measureLoad)
        {
            pump (engine, 1000, false, false, true);
            pump (engine, 2000, false, true);
            for (int active : { 8, 4 })
            {
                for (int lane = 0; lane < 16; ++lane) engine.setLanePluginEnabled (lane, lane < active);
                pump (engine, 1000, false, true);
            }
        }
        engine.stop();
        engine.setBufferSize (128);
        for (int lane = 0; lane < 16; ++lane) engine.setLanePluginEnabled (lane, true);
        expect (engine.start().isEmpty(), "16 lanes at 128 samples");
        dispatch();
        if (measureLoad)
        {
            pump (engine, 1000, false, false, true);
            pump (engine, 2000, false, true);
        }
        engine.setEmergencyMuted (true);
        pump (engine, 100, true);
        engine.stop();
        // Dry bypass remains input->output, distinct from plugin off / master mute.
        rack = engine.captureLanesWithPluginState();
        for (auto& lane : rack) lane.dry = true;
        expect (engine.setLanes (rack).isEmpty(), "dry bypass routing");
        engine.setEmergencyMuted (false);
        expect (engine.start().isEmpty(), "start dry rack");
        dispatch();
        pump (engine, 100, false, false, false, true);
        engine.stop();
        std::cout << (failures == 0 ? "All offline audio integration tests passed.\n" : "Audio integration failures.\n");
        return failures == 0 ? 0 : 1;
    }
private:
    static void pump (AudioEngine& engine, int blocks, bool silent = false,
                      bool benchmark = false, bool warmup = false, bool dry = false)
    {
        auto* device = dynamic_cast<MemoryDevice*> (engine.deviceManager.getCurrentAudioDevice());
        expect (device != nullptr && device->callback != nullptr, "memory device active");
        if (! device || ! device->callback) return;
        juce::AudioBuffer<float> input (16, device->blockSize), output (16, device->blockSize);
        const auto delay = engine.graph.getLatencySamples();
        juce::AudioBuffer<float> history (16, delay + device->blockSize);
        history.clear();
        juce::Random random (42);
        std::vector<double> timings;
        timings.reserve (static_cast<size_t> (blocks));
        double inputEnergy = 0.0, outputEnergy = 0.0;
        for (int block = 0; block < blocks; ++block)
        {
            for (int c = 0; c < 16; ++c)
                for (int i = 0; i < device->blockSize; ++i)
                    input.setSample (c, i, (random.nextFloat() - 0.5f) * 0.2f);
            for (int c = 0; c < 16; ++c)
                for (int i = 0; i < device->blockSize; ++i)
                    history.setSample (c, (block * device->blockSize + i) % history.getNumSamples(), input.getSample (c, i));
            const auto start = juce::Time::getMillisecondCounterHiRes();
            device->callback->audioDeviceIOCallbackWithContext (input.getArrayOfReadPointers(), 16,
                output.getArrayOfWritePointers(), 16, device->blockSize, {});
            timings.push_back (juce::Time::getMillisecondCounterHiRes() - start);
            for (int c = 0; c < 16; ++c)
                for (int i = 0; i < device->blockSize; ++i)
                {
                    const auto value = output.getSample (c, i);
                    if (! std::isfinite (value) || std::abs (value) > 10.0f) { expect (false, "finite bounded output"); return; }
                    if (silent && value != 0.0f) { expect (false, "output is hard-muted"); return; }
                    if (dry && block * device->blockSize + i >= delay)
                    {
                        float expected = 0.0f;
                        for (const auto& lane : engine.lanes)
                            if (lane.outputChannel == c)
                                expected = history.getSample (lane.inputChannel,
                                    (block * device->blockSize + i - delay) % history.getNumSamples());
                        if (std::memcmp (&value, &expected, sizeof (value)) != 0) { expect (false, "dry pass is sample exact at routed outputs; unused outputs silent"); return; }
                    }
                }
            if (warmup && block > blocks / 2)
                for (const auto& lane : engine.lanes)
                    if (lane.pluginEnabled && ! lane.dry)
                        for (int i = 0; i < device->blockSize; ++i)
                        {
                            inputEnergy += std::pow (input.getSample (lane.inputChannel, i), 2);
                            outputEnergy += std::pow (output.getSample (lane.outputChannel, i), 2);
                        }
            if (warmup) juce::Thread::sleep (1);
        }
        if (warmup && inputEnergy > 0.0)
        {
            std::cout << "Noise output/input energy ratio: " << outputEnergy / inputEnergy << std::endl;
            expect (outputEnergy / inputEnergy < 0.95, "plugin reduces noise after model warmup");
        }
        if (benchmark)
        {
            std::sort (timings.begin(), timings.end());
            const auto average = std::accumulate (timings.begin(), timings.end(), 0.0) / blocks;
            std::cout << std::count_if (engine.lanes.begin(), engine.lanes.end(), [] (const auto& lane) { return lane.pluginEnabled; })
                << " active lanes, " << device->sampleRate << " Hz / " << device->blockSize << ", LOW CPU / ZERO: mean " << average
                << " ms, p99 " << timings[static_cast<size_t> (blocks * 99 / 100)]
                << " ms, max " << timings.back() << " ms; deadline " << 1000.0 * device->blockSize / device->sampleRate << " ms\n";
        }
    }
};
}
class OfflineEditorApp final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "VoiceISO Offline QA"; }
    const juce::String getApplicationVersion() override { return "1"; }
    void initialise (const juce::String&) override
    {
        juce::Timer::callAfterDelay (100, [this]
        {
            setApplicationReturnValue (defeedback::AudioEngineIntegrationTest::runVoiceIso (true));
            quit();
        });
    }
    void shutdown() override {}
};

int main (int argc, char** argv)
{
    if ((argc > 1 && juce::String (argv[1]) == "--voiceiso-editor")
        || juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFileName() == "VoiceISO Offline QA")
    {
        juce::JUCEApplicationBase::createInstance = []() -> juce::JUCEApplicationBase* { return new OfflineEditorApp(); };
        return juce::JUCEApplicationBase::main (argc, const_cast<const char**> (argv));
    }
    juce::ScopedJuceInitialiser_GUI initialiser;
    if (argc > 1 && juce::String (argv[1]) == "--mixed")
        return defeedback::AudioEngineIntegrationTest::runMixed();
    if (argc > 1 && juce::String (argv[1]) == "--pass-through")
        return defeedback::AudioEngineIntegrationTest::runPassThrough();
    if (argc > 1 && juce::String (argv[1]).startsWith ("--voiceiso"))
        return defeedback::AudioEngineIntegrationTest::runVoiceIso (juce::String (argv[1]) == "--voiceiso-editor");
    const auto measureLoad = argc < 2 || juce::String (argv[1]) != "--functional";
    return defeedback::AudioEngineIntegrationTest::run (measureLoad);
}
