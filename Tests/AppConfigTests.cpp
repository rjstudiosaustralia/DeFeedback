#include "../Source/AppConfig.h"

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
    original.remoteControlEnabled = true;
    original.remoteControlPort = 9876;
    original.remoteAccessCode = "12345678";
    original.mainWindowState = "1080 720 40 60";
    original.lanes.clear();
    original.lanes.add ({ 1, "Lead", 7, 11, false, "YWJj", true, "420 560 80 90" });
    original.lanes.add ({ 2, "MC", 3, 5, true, {}, false, {} });

    auto restored = AppConfig::fromXml (*original.toXml());
    expect (restored.inputDeviceName == original.inputDeviceName, "input device round-trip");
    expect (restored.outputDeviceName == original.outputDeviceName, "output device round-trip");
    expect (restored.bufferSize == 32, "buffer round-trip");
    expect (restored.remoteControlEnabled, "remote enabled round-trip");
    expect (restored.remoteControlPort == 9876, "remote port round-trip");
    expect (restored.remoteAccessCode == "12345678", "remote access code round-trip");
    expect (restored.mainWindowState == original.mainWindowState, "main window state round-trip");
    expect (restored.lanes == original.lanes, "lane configuration round-trip");

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
    expect (AppConfig::fromXml (empty).lanes.size() == 1, "empty config restores one safe lane");

    juce::Array<LaneConfig> exclusiveRoutes;
    exclusiveRoutes.add ({ 1, "One", 0, 0, false, {}, false, {} });
    exclusiveRoutes.add ({ 2, "Two", 1, 1, false, {}, false, {} });
    expect (validateExclusiveRoutes (exclusiveRoutes).isEmpty(), "unique routes are accepted");
    exclusiveRoutes.getReference (1).inputChannel = 0;
    expect (validateExclusiveRoutes (exclusiveRoutes).startsWith ("Input"), "duplicate input is rejected");
    exclusiveRoutes.getReference (1).inputChannel = 1;
    exclusiveRoutes.getReference (1).outputChannel = 0;
    expect (validateExclusiveRoutes (exclusiveRoutes).startsWith ("Output"), "duplicate output is rejected");

    if (failures == 0)
        std::cout << "All DeFeedback configuration tests passed.\n";

    return failures == 0 ? 0 : 1;
}
