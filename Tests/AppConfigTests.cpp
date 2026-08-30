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
    original.lanes.clear();
    original.lanes.add ({ 1, "Lead", 7, 11, false, "YWJj" });
    original.lanes.add ({ 2, "MC", 3, 5, true, {} });

    auto restored = AppConfig::fromXml (*original.toXml());
    expect (restored.inputDeviceName == original.inputDeviceName, "input device round-trip");
    expect (restored.outputDeviceName == original.outputDeviceName, "output device round-trip");
    expect (restored.bufferSize == 32, "buffer round-trip");
    expect (restored.lanes == original.lanes, "lane configuration round-trip");

    const auto latency = estimateRoundTripMilliseconds (48000.0, 64, 32, 32);
    expect (std::abs (latency - 1.3333333333) < 0.0001, "latency estimate");
    expect (estimateRoundTripMilliseconds (0.0, 64, 32, 32) == 0.0, "invalid rate is safe");
    expect (std::abs (estimateRoundTripMilliseconds (48000.0, 64, 0, 0) - 2.6666666667) < 0.0001,
            "zero-latency driver fallback uses two buffers");

    juce::XmlElement oversized ("DEFEEDBACK_LIVE_CONFIG");
    for (int index = 0; index < maxLanes + 3; ++index)
        oversized.createNewChildElement ("LANE")->setAttribute ("id", index + 1);
    expect (AppConfig::fromXml (oversized).lanes.size() == maxLanes, "config is capped at ten lanes");

    juce::XmlElement empty ("DEFEEDBACK_LIVE_CONFIG");
    expect (AppConfig::fromXml (empty).lanes.size() == 1, "empty config restores one safe lane");

    if (failures == 0)
        std::cout << "All DeFeedback configuration tests passed.\n";

    return failures == 0 ? 0 : 1;
}
