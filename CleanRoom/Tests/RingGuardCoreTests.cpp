#include "../Source/Core/RingGuardCore.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr std::size_t blockSize = 64;
int failures = 0;

void expect (bool condition, const char* message)
{
    if (! condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}

struct LoopBandPass
{
    explicit LoopBandPass (double frequencyHz, double q)
    {
        constexpr double pi = 3.14159265358979323846;
        const auto omega = 2.0 * pi * frequencyHz / sampleRate;
        const auto alpha = std::sin (omega) / (2.0 * q);
        const auto inverseA0 = 1.0 / (1.0 + alpha);
        b0 = alpha * inverseA0;
        b2 = -alpha * inverseA0;
        a1 = -2.0 * std::cos (omega) * inverseA0;
        a2 = (1.0 - alpha) * inverseA0;
    }

    float process (float input) noexcept
    {
        const auto output = b0 * input + z1;
        z1 = -a1 * output + z2;
        z2 = b2 * input - a2 * output;
        return static_cast<float> (output);
    }

    double b0 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
};

float simulateClosedLoop (bool enableRingGuard)
{
    constexpr std::size_t delaySamples = 173;
    constexpr std::size_t totalSamples = static_cast<std::size_t> (sampleRate * 2.0);
    constexpr std::size_t excitationSamples = 4800;
    constexpr float loopGain = 2.5f;

    ringguard::RealtimeProcessor processor;
    processor.prepare (sampleRate, blockSize, 1);
    processor.setSettings ({ 1.0f, 0.90f, 6 });

    LoopBandPass loudspeakerRoomPath (997.0, 14.0);
    std::array<float, delaySamples> delay {};
    std::mt19937 generator (0x4c4f4f50u);
    std::normal_distribution<float> excitation (0.0f, 0.002f);
    std::size_t delayIndex = 0;
    double lateEnergy = 0.0;
    std::size_t lateSamples = 0;

    for (std::size_t index = 0; index < totalSamples; ++index)
    {
        const auto returned = loudspeakerRoomPath.process (delay[delayIndex]);
        const auto source = index < excitationSamples ? excitation (generator) : 0.0f;
        auto output = source + loopGain * returned;

        if (enableRingGuard)
        {
            float* channel[] { &output };
            processor.process (channel, 1, 1);
        }

        delay[delayIndex] = std::clamp (output, -4.0f, 4.0f);
        delayIndex = (delayIndex + 1) % delaySamples;

        if (index >= totalSamples - static_cast<std::size_t> (sampleRate * 0.5))
        {
            lateEnergy += static_cast<double> (output) * output;
            ++lateSamples;
        }
    }

    return static_cast<float> (std::sqrt (lateEnergy / static_cast<double> (lateSamples)));
}

float rms (const std::vector<float>& signal, std::size_t first, std::size_t count)
{
    double energy = 0.0;
    const auto end = std::min (signal.size(), first + count);
    for (auto index = first; index < end; ++index)
        energy += static_cast<double> (signal[index]) * signal[index];
    return end > first ? static_cast<float> (std::sqrt (energy / static_cast<double> (end - first))) : 0.0f;
}

void processMono (ringguard::RealtimeProcessor& processor, std::vector<float>& signal)
{
    for (std::size_t offset = 0; offset < signal.size(); offset += blockSize)
    {
        const auto count = std::min (blockSize, signal.size() - offset);
        float* channels[] { signal.data() + offset };
        processor.process (channels, 1, count);
    }
}

void testImpulseIsSampleSynchronous()
{
    ringguard::RealtimeProcessor processor;
    processor.prepare (sampleRate, blockSize, 1);
    std::vector<float> signal (512, 0.0f);
    signal[0] = 1.0f;
    processMono (processor, signal);
    expect (processor.getLatencySamples() == 0, "core reports zero samples of latency");
    expect (std::abs (signal[0] - 1.0f) < 1.0e-6f, "first impulse sample is not delayed");
    for (const auto sample : signal) expect (std::isfinite (sample), "impulse output remains finite");
}

void testSilenceAndInvalidInputAreSafe()
{
    ringguard::RealtimeProcessor processor;
    processor.prepare (sampleRate, blockSize, 1);
    std::vector<float> signal (2048, 0.0f);
    signal[100] = std::numeric_limits<float>::quiet_NaN();
    signal[200] = std::numeric_limits<float>::infinity();
    processMono (processor, signal);
    for (const auto sample : signal) expect (std::isfinite (sample), "non-finite input is sanitised");
    expect (processor.getTelemetry().activeNotches == 0, "silence does not create notches");
}

void testSustainedNarrowToneIsReduced()
{
    ringguard::RealtimeProcessor processor;
    processor.prepare (sampleRate, blockSize, 1);
    processor.setSettings ({ 1.0f, 0.82f, 4 });
    const auto sampleCount = static_cast<std::size_t> (sampleRate * 2.0);
    std::vector<float> signal (sampleCount);
    for (std::size_t index = 0; index < sampleCount; ++index)
        signal[index] = 0.25f * std::sin (2.0f * 3.14159265358979323846f * 1000.0f
                                          * static_cast<float> (index / sampleRate));
    const auto inputRms = rms (signal, 0, static_cast<std::size_t> (sampleRate * 0.02));
    processMono (processor, signal);
    const auto outputRms = rms (signal, static_cast<std::size_t> (sampleRate * 1.5),
                                static_cast<std::size_t> (sampleRate * 0.4));
    const auto telemetry = processor.getTelemetry();
    expect (telemetry.activeNotches >= 1, "a sustained narrow tone activates a notch");
    expect (outputRms < inputRms * 0.55f, "the notch materially reduces a narrow tone");
}

void testBroadbandNoiseDoesNotRunAway()
{
    ringguard::RealtimeProcessor processor;
    processor.prepare (sampleRate, blockSize, 1);
    processor.setSettings ({ 1.0f, 0.65f, 4 });
    std::mt19937 generator (0x52474u);
    std::normal_distribution<float> distribution (0.0f, 0.08f);
    std::vector<float> signal (static_cast<std::size_t> (sampleRate));
    for (auto& sample : signal) sample = distribution (generator);
    processMono (processor, signal);
    expect (processor.getTelemetry().activeNotches <= 1, "broadband noise does not fill the notch pool");
}

void testClosedLoopGainBeforeFeedbackBaseline()
{
    const auto unprocessedLateRms = simulateClosedLoop (false);
    const auto processedLateRms = simulateClosedLoop (true);
    expect (unprocessedLateRms > 1.0f, "the reference loop reaches sustained feedback");
    expect (processedLateRms < unprocessedLateRms * 0.10f,
            "RingGuard increases stability in the deterministic closed-loop fixture");
}

void testStereoUsesLinkedControl()
{
    ringguard::RealtimeProcessor processor;
    processor.prepare (sampleRate, blockSize, 2);
    processor.setSettings ({ 1.0f, 0.82f, 3 });
    const auto sampleCount = static_cast<std::size_t> (sampleRate);
    std::vector<float> left (sampleCount), right (sampleCount);
    for (std::size_t index = 0; index < sampleCount; ++index)
    {
        const auto tone = 0.2f * std::sin (2.0f * 3.14159265358979323846f * 1600.0f
                                           * static_cast<float> (index / sampleRate));
        left[index] = tone; right[index] = tone * 0.8f;
    }
    for (std::size_t offset = 0; offset < sampleCount; offset += blockSize)
    {
        const auto count = std::min (blockSize, sampleCount - offset);
        float* channels[] { left.data() + offset, right.data() + offset };
        processor.process (channels, 2, count);
    }
    const auto leftLate = rms (left, sampleCount - 12000, 10000);
    const auto rightLate = rms (right, sampleCount - 12000, 10000);
    expect (leftLate > 0.0f && rightLate > 0.0f, "stereo output remains present");
    expect (std::abs ((rightLate / leftLate) - 0.8f) < 0.08f,
            "linked control preserves the stereo level relationship");
}
}

int main()
{
    testImpulseIsSampleSynchronous();
    testSilenceAndInvalidInputAreSafe();
    testSustainedNarrowToneIsReduced();
    testBroadbandNoiseDoesNotRunAway();
    testClosedLoopGainBeforeFeedbackBaseline();
    testStereoUsesLinkedControl();
    if (failures == 0) std::cout << "All RingGuard core tests passed.\n";
    return failures == 0 ? 0 : 1;
}
