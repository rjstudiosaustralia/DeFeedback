#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace ringguard
{
struct Settings
{
    float strength = 1.0f;
    float sensitivity = 0.65f;
    int maxNotches = 4;
};

struct Telemetry
{
    int activeNotches = 0;
    float strongestFrequencyHz = 0.0f;
    float strongestScore = 0.0f;
    std::uint64_t analysedFrames = 0;
};

/** Deterministic, zero-lookahead feedback-risk reducer. */
class RealtimeProcessor
{
public:
    static constexpr int maximumChannels = 2;
    static constexpr int maximumNotches = 8;
    static constexpr int probeCount = 48;

    void prepare (double sampleRate, std::size_t maximumBlockSize, int channelCount) noexcept;
    void reset() noexcept;
    void setSettings (Settings newSettings) noexcept;
    [[nodiscard]] Settings getSettings() const noexcept { return settings; }
    void process (float* const* channels, int channelCount, std::size_t sampleCount) noexcept;
    [[nodiscard]] Telemetry getTelemetry() const noexcept;
    [[nodiscard]] double getSampleRate() const noexcept { return sampleRateHz; }
    [[nodiscard]] int getLatencySamples() const noexcept { return 0; }

private:
    struct StateVariableFilter
    {
        void configure (double sampleRate, float frequencyHz, float q) noexcept;
        void reset() noexcept;
        float processBandPass (float input) noexcept;
        float processNotch (float input) noexcept;
        float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f;
        float ic1eq = 0.0f, ic2eq = 0.0f;
    };

    struct Probe
    {
        StateVariableFilter filter;
        float frequencyHz = 0.0f;
        float accumulatedEnergy = 0.0f;
        float previousEnergy = 0.0f;
        float currentEnergy = 0.0f;
        int persistence = 0;
        void reset() noexcept;
    };

    struct Candidate
    {
        float frequencyHz = 0.0f;
        float score = 0.0f;
        float targetDepth = 0.0f;
    };

    struct NotchSlot
    {
        void prepare (double sampleRate) noexcept;
        void reset() noexcept;
        void assign (float frequencyHz, float depth, float score, int holdFrames) noexcept;
        void beginAnalysisFrame() noexcept;
        void beginAudioFrame() noexcept;
        float process (int channel, float input) noexcept;
        [[nodiscard]] bool matches (float frequencyHz) const noexcept;
        [[nodiscard]] bool isAvailable() const noexcept;
        [[nodiscard]] bool isAudiblyActive() const noexcept;
        void tuneNow (float frequencyHz) noexcept;

        std::array<StateVariableFilter, maximumChannels> filters;
        double sampleRate = 48000.0;
        float frequencyHz = 0.0f, pendingFrequencyHz = 0.0f;
        float currentDepth = 0.0f, targetDepth = 0.0f, pendingDepth = 0.0f;
        float score = 0.0f, attackStep = 1.0f, releaseStep = 1.0f;
        int holdFramesRemaining = 0;
        bool refreshedThisFrame = false, retunePending = false;
    };

    void analyseSample (float input) noexcept;
    void finishAnalysisFrame() noexcept;
    void buildCandidates (std::array<Candidate, maximumNotches>& candidates,
                          int& candidateCount) noexcept;
    void updateNotches (const std::array<Candidate, maximumNotches>& candidates,
                        int candidateCount) noexcept;
    void publishTelemetry (float strongestFrequency, float strongestScore) noexcept;
    static float clamp01 (float value) noexcept;

    std::array<Probe, probeCount> probes;
    std::array<NotchSlot, maximumNotches> notchSlots;
    Settings settings;
    double sampleRateHz = 48000.0;
    std::size_t maxBlockSize = 0;
    int preparedChannels = 1, analysisHopSamples = 96, samplesIntoAnalysisFrame = 0;
    float broadbandEnergy = 0.0f, probeFrequencyRatio = 1.0f;
    std::uint64_t analysedFrames = 0;
    std::atomic<int> telemetryActiveNotches { 0 };
    std::atomic<float> telemetryStrongestFrequency { 0.0f };
    std::atomic<float> telemetryStrongestScore { 0.0f };
    std::atomic<std::uint64_t> telemetryFrames { 0 };
};
}
