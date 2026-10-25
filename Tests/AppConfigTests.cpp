#include "../Source/AppConfig.h"
#include "../Source/MeterProcessor.h"

#include <iostream>

namespace
{
int failures = 0;

void expect (bool condition, const char* message)
{
    if (! condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
}

int main()
{
    using namespace defeedback;

    AppConfig original;
    original.inputDeviceName = "RME Digiface Dante";
    original.outputDeviceName = "RME Digiface Dante";
    original.sampleRate = 48000.0;
    original.bufferSize = 32;
    original.maxProcessingThreads = 6;
    original.remoteControlEnabled = true;
    original.remoteControlPort = 9876;
    original.remoteAccessCodeRequired = false;
    original.remoteAccessCode = "12345678";
    original.remoteBrowserToken = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    original.mainWindowState = "1080 720 40 60";
    original.lanes.clear();
    original.lanes.add ({ 1, "Lead", 7, 11, false, "YWJj", true, "420 560 80 90" });
    original.lanes.add ({ 2, "MC", 3, 5, true, {}, false, {}, false });

    auto restored = AppConfig::fromXml (*original.toXml());
    expect (restored.inputDeviceName == original.inputDeviceName, "input device round-trip");
    expect (restored.outputDeviceName == original.outputDeviceName, "output device round-trip");
    expect (restored.parallelProcessing, "parallel processing survives save/reload");
    expect (restored.maxProcessingThreads == 6, "thread limit survives save/reload");
    {
        auto settings = original.toXml();
        settings->removeAttribute ("maxProcessingThreads");
        expect (AppConfig::fromXml (*settings).maxProcessingThreads == 0, "old settings use automatic thread limit");
        settings->setAttribute ("maxProcessingThreads", -7);
        expect (AppConfig::fromXml (*settings).maxProcessingThreads == 0, "negative thread limit is clamped");
        settings->setAttribute ("maxProcessingThreads", 999);
        expect (AppConfig::fromXml (*settings).maxProcessingThreads == 64, "excessive thread limit is clamped");
    }
    expect (restored.bufferSize == 32, "buffer round-trip");
    expect (restored.remoteControlEnabled, "remote enabled round-trip");
    expect (restored.remoteControlPort == 9876, "remote port round-trip");
    expect (! restored.remoteAccessCodeRequired, "remote access-code mode round-trip");
    expect (restored.remoteAccessCode == "12345678", "remote access code round-trip");
    expect (restored.remoteBrowserToken == original.remoteBrowserToken, "remembered-browser token round-trip");
    expect (restored.mainWindowState == original.mainWindowState, "main window state round-trip");
    expect (restored.lanes == original.lanes, "lane configuration round-trip");

    expect (restored.pluginMode == PluginMode::deFeedback, "legacy mode remains De-Feedback");
    const auto originalState = original.lanes[0].capturePluginState();
    original.lanes.getReference (0).selectPlugin (PluginMode::clearVoice);
    expect (original.lanes[0].inputChannel == 7 && original.lanes[0].outputChannel == 11,
            "per-lane plugin change preserves patching");
    expect (original.lanes[0].pluginStateBase64.isEmpty() && ! original.lanes[0].editorOpen,
            "new plugin never inherits another vendor's state or editor");
    expect (original.lanes[1] == restored.lanes[1], "other lane remains untouched");
    original.lanes.getReference (0).pluginStateBase64 = "Y2xlYXJ2b2ljZQ==";
    original.lanes.getReference (0).modeParameterState = "<MODE_PARAMETERS pluginMode=\"clearvoice\"/>";
    original.lanes.getReference (0).editorWindowState = "800 500 20 40";
    const auto clearState = original.lanes[0].capturePluginState();
    expect (original.rackModeId() == "mixed", "mixed rack reported explicitly");
    auto mixed = AppConfig::fromXml (*original.toXml());
    expect (mixed.lanes == original.lanes, "mixed rack and inactive states survive restart");
    expect (mixed.toXml()->getChildByName ("LANE")->getStringAttribute ("pluginState") == "YWJj",
            "older hosts only receive De-Feedback state");
    for (int cycle = 0; cycle < 3; ++cycle)
        for (auto mode : { PluginMode::voiceIso, PluginMode::deFeedback, PluginMode::clearVoice })
        {
            mixed.lanes.getReference (0).selectPlugin (mode);
            mixed = AppConfig::fromXml (*mixed.toXml());
            expect (mixed.lanes[0].pluginMode == mode, "lane plugin survives restart");
            expect (mixed.lanes[0].name == "Lead" && mixed.lanes[0].inputChannel == 7,
                    "plugin swaps retain lane identity and patch");
            if (mode == PluginMode::deFeedback)
                expect (mixed.lanes[0].capturePluginState() == originalState, "De-Feedback state restored exactly");
            if (mode == PluginMode::clearVoice)
                expect (mixed.lanes[0].capturePluginState() == clearState, "Clear Voice state restored exactly");
        }
    mixed.switchPluginMode (PluginMode::voiceIso);
    expect (mixed.rackModeId() == "voiceiso" && mixed.lanes.size() == 2,
            "apply all updates every existing lane");
    expect (mixed.lanes[1].inputChannel == 3 && mixed.lanes[1].outputChannel == 5,
            "apply all preserves each lane's routing");
    mixed.lanes.getReference (1).selectPlugin (PluginMode::deFeedback);
    expect (! mixed.lanes[1].pluginEnabled && mixed.lanes[1].dry, "switch back restores lane OFF and bypass");
    for (int version : { 6, 7 })
    {
        juce::XmlElement legacyBank ("DEFEEDBACK_LIVE_CONFIG");
        legacyBank.setAttribute ("formatVersion", version);
        legacyBank.setAttribute ("pluginMode", "clearvoice");
        auto* df = legacyBank.createNewChildElement ("LANE");
        df->setAttribute ("id", 1);
        df->setAttribute ("pluginState", "df-state");
        df->setAttribute ("input", 4);
        auto* cv = legacyBank.createNewChildElement ("CLEAR_VOICE_LANES")->createNewChildElement ("LANE");
        cv->setAttribute ("id", 1);
        cv->setAttribute ("name", "Existing lead");
        cv->setAttribute ("pluginState", "cv-state");
        cv->setAttribute ("input", 8);
        auto migrated = AppConfig::fromXml (legacyBank);
        expect (migrated.lanes[0].pluginMode == PluginMode::clearVoice
                && migrated.lanes[0].pluginStateBase64 == "cv-state" && migrated.lanes[0].inputChannel == 8,
                "legacy selected rack and plugin states migrate");
        migrated = AppConfig::fromXml (*migrated.toXml());
        migrated.lanes.getReference (0).selectPlugin (PluginMode::deFeedback);
        expect (migrated.lanes[0].pluginStateBase64 == "df-state" && migrated.lanes[0].inputChannel == 8,
                "legacy inactive state migrates without swapping current routing");
        expect (migrated.inactiveModeLanes[PluginMode::deFeedback][0].inputChannel == 4,
                "original legacy bank retained for recovery");
    }
    expect (! supportsSampleRate (original.lanes, 44100) && supportsSampleRate (original.lanes, 48000),
            "mixed rack uses sample rates supported by every lane");
    expect (supportsSampleRate (PluginMode::deFeedback, 44100), "De-Feedback sample-rate behavior preserved");
    expect (! supportsSampleRate (PluginMode::clearVoice, 44100)
            && ! supportsSampleRate (PluginMode::clearVoice, 88200)
            && ! supportsSampleRate (PluginMode::clearVoice, 0), "unsupported Clear Voice rates rejected");
    expect (supportsSampleRate (PluginMode::clearVoice, 48000)
            && supportsSampleRate (PluginMode::clearVoice, 96000)
            && supportsSampleRate (PluginMode::clearVoice, 192000), "documented Clear Voice rates accepted");

    const auto latency = estimateRoundTripMilliseconds (48000.0, 64, 32, 32);
    expect (std::abs (latency - 1.3333333333) < 0.0001, "latency estimate");
    expect (estimateRoundTripMilliseconds (0.0, 64, 32, 32) == 0.0, "invalid rate is safe");
    expect (std::abs (estimateRoundTripMilliseconds (48000.0, 64, 0, 0) - 2.6666666667) < 0.0001,
            "zero-latency driver fallback uses two buffers");

    juce::XmlElement manyLanes ("DEFEEDBACK_LIVE_CONFIG");
    constexpr int savedLaneCount = 64;
    for (int index = 0; index < savedLaneCount; ++index)
        manyLanes.createNewChildElement ("LANE")->setAttribute ("id", index + 1);
    expect (AppConfig::fromXml (manyLanes).lanes.size() == savedLaneCount,
            "all saved lanes are restored without an application cap");

    juce::XmlElement empty ("DEFEEDBACK_LIVE_CONFIG");
    expect (! AppConfig::fromXml (empty).parallelProcessing, "legacy settings retain serial processing until selected");
    expect (AppConfig::fromXml (empty).lanes.size() == 1, "empty config restores one safe lane");

    juce::XmlElement legacy ("DEFEEDBACK_LIVE_CONFIG");
    legacy.setAttribute ("formatVersion", 3);
    legacy.createNewChildElement ("LANE")->setAttribute ("id", 1);
    expect (AppConfig::fromXml (legacy).lanes[0].pluginEnabled,
            "lanes saved before plugin enable existed remain enabled");

    juce::Array<LaneConfig> exclusiveRoutes;
    exclusiveRoutes.add ({ 1, "One", 0, 0, false, {}, false, {} });
    exclusiveRoutes.add ({ 2, "Two", 1, 1, false, {}, false, {} });
    expect (validateExclusiveRoutes (exclusiveRoutes).isEmpty(), "unique routes are accepted");
    exclusiveRoutes.getReference (1).inputChannel = 0;
    expect (validateExclusiveRoutes (exclusiveRoutes).startsWith ("Input"), "duplicate input is rejected");
    exclusiveRoutes.getReference (1).inputChannel = 1;
    exclusiveRoutes.getReference (1).outputChannel = 0;
    expect (validateExclusiveRoutes (exclusiveRoutes).startsWith ("Output"), "duplicate output is rejected");

    std::atomic<bool> dryPass { false }, masterMuted { false };
    MeterProcessor laneOutputGate (&masterMuted, true, &dryPass);
    juce::AudioBuffer<float> samples (2, 32);
    juce::MidiBuffer midi;
    const auto fill = [&]
    {
        for (int sample = 0; sample < samples.getNumSamples(); ++sample)
        {
            samples.setSample (0, sample, 0.25f);
            samples.setSample (1, sample, -0.5f);
        }
    };
    fill();
    laneOutputGate.processBlock (samples, midi);
    expect (std::abs (samples.getSample (0, 0) - 0.25f) < 0.0001f, "active lane selects processed audio");
    laneOutputGate.consumePeak();
    dryPass.store (true);
    fill();
    laneOutputGate.processBlock (samples, midi);
    expect (std::abs (samples.getSample (0, 0) + 0.5f) < 0.0001f, "plugin OFF selects dry input");
    expect (std::abs (laneOutputGate.consumePeak() - 0.5f) < 0.0001f, "OFF output meter measures dry audio");
    masterMuted.store (true);
    fill();
    laneOutputGate.processBlock (samples, midi);
    expect (samples.getMagnitude (0, 0, samples.getNumSamples()) == 0.0f,
            "master mute overrides OFF dry pass");
    expect (laneOutputGate.consumePeak() == 0.0f, "master-muted output meter reports silence");
    juce::AudioBuffer<double> doubles (2, 16);
    masterMuted.store (false);
    for (int i = 0; i < 16; ++i) { doubles.setSample (0, i, 0.25); doubles.setSample (1, i, 0.75); }
    laneOutputGate.processBlock (doubles, midi);
    expect (std::abs (doubles.getSample (0, 0) - 0.75) < 0.0001, "double precision OFF dry pass");
    masterMuted.store (true);
    laneOutputGate.processBlock (doubles, midi);
    expect (doubles.getMagnitude (0, 0, 16) == 0.0, "double precision master mute overrides OFF");

    if (failures == 0)
        std::cout << "All DeFeedback configuration tests passed.\n";

    return failures == 0 ? 0 : 1;
}
