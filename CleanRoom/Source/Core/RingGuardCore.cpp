#include "RingGuardCore.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ringguard
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float minimumProbeFrequencyHz = 125.0f;
constexpr float maximumProbeFrequencyHz = 12000.0f;
constexpr float probeQ = 18.0f;
constexpr float notchQ = 10.0f;
constexpr float epsilon = 1.0e-20f;

float decibelsToPower (float decibels) noexcept { return std::pow (10.0f, decibels * 0.1f); }
float sanitise (float value) noexcept { return std::isfinite (value) ? value : 0.0f; }
}

void RealtimeProcessor::StateVariableFilter::configure (double sampleRate,
                                                         float frequencyHz,
                                                         float q) noexcept
{
    const auto safeRate = static_cast<float> (std::max (sampleRate, 8000.0));
    const auto safeFrequency = std::clamp (frequencyHz, 20.0f, safeRate * 0.45f);
    const auto safeQ = std::clamp (q, 0.25f, 100.0f);
    const auto g = std::tan (pi * safeFrequency / safeRate);
    k = 1.0f / safeQ;
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

void RealtimeProcessor::StateVariableFilter::reset() noexcept { ic1eq = ic2eq = 0.0f; }

float RealtimeProcessor::StateVariableFilter::processBandPass (float input) noexcept
{
    const auto v3 = input - ic2eq;
    const auto v1 = a1 * ic1eq + a2 * v3;
    const auto v2 = ic2eq + a2 * ic1eq + a3 * v3;
    ic1eq = sanitise (2.0f * v1 - ic1eq);
    ic2eq = sanitise (2.0f * v2 - ic2eq);
    return sanitise (k * v1);
}

float RealtimeProcessor::StateVariableFilter::processNotch (float input) noexcept
{
    const auto v3 = input - ic2eq;
    const auto v1 = a1 * ic1eq + a2 * v3;
    const auto v2 = ic2eq + a2 * ic1eq + a3 * v3;
    ic1eq = sanitise (2.0f * v1 - ic1eq);
    ic2eq = sanitise (2.0f * v2 - ic2eq);
    return sanitise (input - k * v1);
}

void RealtimeProcessor::Probe::reset() noexcept
{
    filter.reset();
    accumulatedEnergy = previousEnergy = currentEnergy = 0.0f;
    persistence = 0;
}

void RealtimeProcessor::NotchSlot::prepare (double newSampleRate) noexcept
{
    sampleRate = std::max (newSampleRate, 8000.0);
    attackStep = 1.0f - std::exp (-1.0f / static_cast<float> (0.004 * sampleRate));
    releaseStep = 1.0f - std::exp (-1.0f / static_cast<float> (0.650 * sampleRate));
    reset();
}

void RealtimeProcessor::NotchSlot::reset() noexcept
{
    for (auto& filter : filters) filter.reset();
    frequencyHz = pendingFrequencyHz = 0.0f;
    currentDepth = targetDepth = pendingDepth = 0.0f;
    score = 0.0f;
    holdFramesRemaining = 0;
    refreshedThisFrame = retunePending = false;
}

void RealtimeProcessor::NotchSlot::tuneNow (float newFrequencyHz) noexcept
{
    frequencyHz = std::clamp (newFrequencyHz, 40.0f, static_cast<float> (sampleRate * 0.45));
    for (auto& filter : filters)
    {
        filter.configure (sampleRate, frequencyHz, notchQ);
        filter.reset();
    }
}

void RealtimeProcessor::NotchSlot::assign (float newFrequencyHz,
                                            float depth,
                                            float newScore,
                                            int holdFrames) noexcept
{
    const auto safeDepth = std::clamp (depth, 0.0f, 0.985f);
    refreshedThisFrame = true;
    holdFramesRemaining = std::max (holdFramesRemaining, holdFrames);
    score = std::max (score * 0.92f, newScore);

    if (frequencyHz <= 0.0f || currentDepth < 0.001f)
    {
        tuneNow (newFrequencyHz);
        targetDepth = pendingDepth = safeDepth;
        retunePending = false;
    }
    else if (matches (newFrequencyHz))
    {
        targetDepth = std::max (targetDepth, safeDepth);
        pendingDepth = targetDepth;
    }
    else
    {
        pendingFrequencyHz = newFrequencyHz;
        pendingDepth = safeDepth;
        targetDepth = 0.0f;
        retunePending = true;
    }
}

void RealtimeProcessor::NotchSlot::beginAnalysisFrame() noexcept
{
    if (! refreshedThisFrame)
    {
        if (holdFramesRemaining > 0) --holdFramesRemaining;
        else if (! retunePending) targetDepth = 0.0f;
        score *= 0.96f;
    }
    refreshedThisFrame = false;
}

void RealtimeProcessor::NotchSlot::beginAudioFrame() noexcept
{
    const auto smoothing = targetDepth > currentDepth ? attackStep : releaseStep;
    currentDepth += smoothing * (targetDepth - currentDepth);
    if (! std::isfinite (currentDepth)) currentDepth = 0.0f;

    if (retunePending && currentDepth < 0.0005f)
    {
        tuneNow (pendingFrequencyHz);
        targetDepth = pendingDepth;
        retunePending = false;
    }

    if (! retunePending && targetDepth <= 0.0f && currentDepth < 0.00005f)
    {
        currentDepth = frequencyHz = score = 0.0f;
        for (auto& filter : filters) filter.reset();
    }
}

float RealtimeProcessor::NotchSlot::process (int channel, float input) noexcept
{
    if (frequencyHz <= 0.0f || channel < 0 || channel >= maximumChannels) return input;
    const auto filtered = filters[static_cast<std::size_t> (channel)].processNotch (input);
    return sanitise (input + currentDepth * (filtered - input));
}

bool RealtimeProcessor::NotchSlot::matches (float otherFrequencyHz) const noexcept
{
    if (frequencyHz <= 0.0f || otherFrequencyHz <= 0.0f) return false;
    const auto ratio = std::max (frequencyHz, otherFrequencyHz) / std::min (frequencyHz, otherFrequencyHz);
    return ratio < 1.055f;
}

bool RealtimeProcessor::NotchSlot::isAvailable() const noexcept
{
    return frequencyHz <= 0.0f || (targetDepth <= 0.0f && currentDepth < 0.002f);
}

bool RealtimeProcessor::NotchSlot::isAudiblyActive() const noexcept
{
    return frequencyHz > 0.0f && currentDepth > 0.03f;
}

void RealtimeProcessor::prepare (double newSampleRate,
                                 std::size_t maximumBlockSize,
                                 int channelCount) noexcept
{
    sampleRateHz = std::clamp (newSampleRate, 8000.0, 192000.0);
    maxBlockSize = maximumBlockSize;
    preparedChannels = std::clamp (channelCount, 1, maximumChannels);
    analysisHopSamples = std::max (64, static_cast<int> (std::lround (sampleRateHz * 0.004)));
    probeFrequencyRatio = std::pow (maximumProbeFrequencyHz / minimumProbeFrequencyHz,
                                    1.0f / static_cast<float> (probeCount - 1));

    auto frequency = minimumProbeFrequencyHz;
    const auto maximumForRate = static_cast<float> (sampleRateHz * 0.44);
    for (auto& probe : probes)
    {
        probe.frequencyHz = std::min (frequency, maximumForRate);
        probe.filter.configure (sampleRateHz, probe.frequencyHz, probeQ);
        frequency *= probeFrequencyRatio;
    }
    for (auto& slot : notchSlots) slot.prepare (sampleRateHz);
    reset();
}

void RealtimeProcessor::reset() noexcept
{
    samplesIntoAnalysisFrame = 0;
    broadbandEnergy = 0.0f;
    analysedFrames = 0;
    for (auto& probe : probes) probe.reset();
    for (auto& slot : notchSlots) slot.reset();
    publishTelemetry (0.0f, 0.0f);
}

void RealtimeProcessor::setSettings (Settings newSettings) noexcept
{
    newSettings.strength = clamp01 (newSettings.strength);
    newSettings.sensitivity = clamp01 (newSettings.sensitivity);
    newSettings.maxNotches = std::clamp (newSettings.maxNotches, 1, maximumNotches);
    settings = newSettings;
    for (int index = settings.maxNotches; index < maximumNotches; ++index)
    {
        auto& slot = notchSlots[static_cast<std::size_t> (index)];
        slot.targetDepth = 0.0f;
        slot.holdFramesRemaining = 0;
    }
}

void RealtimeProcessor::process (float* const* channels,
                                 int channelCount,
                                 std::size_t sampleCount) noexcept
{
    if (channels == nullptr || sampleCount == 0) return;
    const auto channelsToProcess = std::clamp (channelCount, 1, preparedChannels);

    for (std::size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        std::array<float, maximumChannels> frame { 0.0f, 0.0f };
        float analysisInput = 0.0f;
        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            auto* data = channels[channel];
            const auto input = data != nullptr ? sanitise (data[sampleIndex]) : 0.0f;
            frame[static_cast<std::size_t> (channel)] = input;
            analysisInput += input;
        }
        analysisInput /= static_cast<float> (channelsToProcess);
        analyseSample (analysisInput);
        for (auto& slot : notchSlots) slot.beginAudioFrame();

        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            auto output = frame[static_cast<std::size_t> (channel)];
            for (int slotIndex = 0; slotIndex < settings.maxNotches; ++slotIndex)
                output = notchSlots[static_cast<std::size_t> (slotIndex)].process (channel, output);
            if (channels[channel] != nullptr) channels[channel][sampleIndex] = sanitise (output);
        }
    }
}

void RealtimeProcessor::analyseSample (float input) noexcept
{
    broadbandEnergy += input * input;
    for (auto& probe : probes)
    {
        const auto band = probe.filter.processBandPass (input);
        probe.accumulatedEnergy += band * band;
    }
    if (++samplesIntoAnalysisFrame >= analysisHopSamples) finishAnalysisFrame();
}

void RealtimeProcessor::finishAnalysisFrame() noexcept
{
    const auto inverseFrameLength = 1.0f / static_cast<float> (analysisHopSamples);
    broadbandEnergy *= inverseFrameLength;
    for (auto& probe : probes)
    {
        probe.previousEnergy = probe.currentEnergy;
        probe.currentEnergy = sanitise (probe.accumulatedEnergy * inverseFrameLength);
        probe.accumulatedEnergy = 0.0f;
    }

    std::array<Candidate, maximumNotches> candidates {};
    int candidateCount = 0;
    buildCandidates (candidates, candidateCount);
    updateNotches (candidates, candidateCount);
    for (auto& slot : notchSlots) slot.beginAnalysisFrame();
    ++analysedFrames;
    publishTelemetry (candidateCount > 0 ? candidates[0].frequencyHz : 0.0f,
                      candidateCount > 0 ? candidates[0].score : 0.0f);
    broadbandEnergy = 0.0f;
    samplesIntoAnalysisFrame = 0;
}

void RealtimeProcessor::buildCandidates (std::array<Candidate, maximumNotches>& candidates,
                                         int& candidateCount) noexcept
{
    candidateCount = 0;
    const auto sensitivity = settings.sensitivity;
    const auto minimumPower = decibelsToPower (-34.0f - 26.0f * sensitivity);
    const auto minimumNeighbourRatio = 3.4f - 1.4f * sensitivity;
    const auto minimumConcentration = 0.120f - 0.060f * sensitivity;
    const auto persistenceNeeded = sensitivity > 0.85f ? 2 : (sensitivity > 0.65f ? 3 : 4);

    for (int index = 1; index < probeCount - 1; ++index)
    {
        auto& probe = probes[static_cast<std::size_t> (index)];
        const auto energy = probe.currentEnergy;
        const auto left = probes[static_cast<std::size_t> (index - 1)].currentEnergy;
        const auto right = probes[static_cast<std::size_t> (index + 1)].currentEnergy;
        const auto neighbourRatio = energy / (0.5f * (left + right) + epsilon);
        const auto concentration = energy / (broadbandEnergy + epsilon);
        const auto growth = energy / (probe.previousEnergy + epsilon);
        const auto looksTonal = energy > minimumPower
                             && neighbourRatio > minimumNeighbourRatio
                             && concentration > minimumConcentration;
        probe.persistence = looksTonal ? std::min (probe.persistence + 1, 1000)
                                       : std::max (probe.persistence - 1, 0);
        const auto fastGrowth = growth > (3.2f - 1.2f * sensitivity)
                             && probe.persistence >= 2
                             && neighbourRatio > minimumNeighbourRatio * 1.25f
                             && concentration > minimumConcentration * 1.5f;
        if (! looksTonal || (probe.persistence < persistenceNeeded && ! fastGrowth)) continue;

        auto interpolatedFrequency = probe.frequencyHz;
        const auto denominator = left - 2.0f * energy + right;
        if (std::abs (denominator) > epsilon)
        {
            const auto offset = std::clamp (0.5f * (left - right) / denominator, -0.5f, 0.5f);
            interpolatedFrequency *= std::pow (probeFrequencyRatio, offset);
        }

        const auto score = neighbourRatio * std::sqrt (std::max (concentration, 0.0f))
                         * (1.0f + 0.08f * static_cast<float> (probe.persistence));
        const auto normalisedScore = std::clamp ((score - 1.0f) / 12.0f, 0.0f, 1.0f);
        const auto depth = settings.strength
                         * std::clamp (0.42f + 0.56f * normalisedScore, 0.0f, 0.985f);
        Candidate candidate { interpolatedFrequency, score, depth };

        auto insertion = std::min (candidateCount, maximumNotches - 1);
        while (insertion > 0
               && candidates[static_cast<std::size_t> (insertion - 1)].score < candidate.score)
        {
            if (insertion < maximumNotches)
                candidates[static_cast<std::size_t> (insertion)] =
                    candidates[static_cast<std::size_t> (insertion - 1)];
            --insertion;
        }
        if (insertion < maximumNotches)
        {
            candidates[static_cast<std::size_t> (insertion)] = candidate;
            candidateCount = std::min (candidateCount + 1, maximumNotches);
        }
    }
}

void RealtimeProcessor::updateNotches (const std::array<Candidate, maximumNotches>& candidates,
                                       int candidateCount) noexcept
{
    const auto holdFrames = std::max (1, static_cast<int> (std::lround (0.45 * sampleRateHz
                                                                       / analysisHopSamples)));
    const auto usableSlots = settings.maxNotches;
    for (int candidateIndex = 0; candidateIndex < candidateCount && candidateIndex < usableSlots;
         ++candidateIndex)
    {
        const auto& candidate = candidates[static_cast<std::size_t> (candidateIndex)];
        int selectedSlot = -1;
        for (int slotIndex = 0; slotIndex < usableSlots; ++slotIndex)
            if (notchSlots[static_cast<std::size_t> (slotIndex)].matches (candidate.frequencyHz))
            { selectedSlot = slotIndex; break; }

        if (selectedSlot < 0)
            for (int slotIndex = 0; slotIndex < usableSlots; ++slotIndex)
                if (notchSlots[static_cast<std::size_t> (slotIndex)].isAvailable())
                { selectedSlot = slotIndex; break; }

        if (selectedSlot < 0)
        {
            auto weakestScore = std::numeric_limits<float>::max();
            for (int slotIndex = 0; slotIndex < usableSlots; ++slotIndex)
            {
                const auto slotScore = notchSlots[static_cast<std::size_t> (slotIndex)].score;
                if (slotScore < weakestScore) { weakestScore = slotScore; selectedSlot = slotIndex; }
            }
            if (selectedSlot >= 0
                && candidate.score < notchSlots[static_cast<std::size_t> (selectedSlot)].score * 1.20f)
                selectedSlot = -1;
        }
        if (selectedSlot >= 0)
            notchSlots[static_cast<std::size_t> (selectedSlot)].assign (
                candidate.frequencyHz, candidate.targetDepth, candidate.score, holdFrames);
    }
}

void RealtimeProcessor::publishTelemetry (float strongestFrequency, float strongestScore) noexcept
{
    auto active = 0;
    for (const auto& slot : notchSlots) if (slot.isAudiblyActive()) ++active;
    telemetryActiveNotches.store (active, std::memory_order_relaxed);
    telemetryStrongestFrequency.store (strongestFrequency, std::memory_order_relaxed);
    telemetryStrongestScore.store (strongestScore, std::memory_order_relaxed);
    telemetryFrames.store (analysedFrames, std::memory_order_relaxed);
}

Telemetry RealtimeProcessor::getTelemetry() const noexcept
{
    return { telemetryActiveNotches.load (std::memory_order_relaxed),
             telemetryStrongestFrequency.load (std::memory_order_relaxed),
             telemetryStrongestScore.load (std::memory_order_relaxed),
             telemetryFrames.load (std::memory_order_relaxed) };
}

float RealtimeProcessor::clamp01 (float value) noexcept
{
    return std::clamp (std::isfinite (value) ? value : 0.0f, 0.0f, 1.0f);
}
}
