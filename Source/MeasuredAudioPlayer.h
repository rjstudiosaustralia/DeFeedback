#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

namespace defeedback
{
// Measures the complete player callback, including copying and worker waits.
// Counters are independent of the driver's underrun counter and JUCE's average.
class MeasuredAudioPlayer final : public juce::AudioProcessorPlayer
{
public:
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        ticksPerSample = static_cast<double> (juce::Time::getHighResolutionTicksPerSecond())
                       / device->getCurrentSampleRate();
        juce::AudioProcessorPlayer::audioDeviceAboutToStart (device);
    }

    void audioDeviceIOCallbackWithContext (const float* const* inputs, int inputCount,
                                          float* const* outputs, int outputCount, int samples,
                                          const juce::AudioIODeviceCallbackContext& context) override
    {
        const auto before = juce::Time::getHighResolutionTicks();
        juce::AudioProcessorPlayer::audioDeviceIOCallbackWithContext (
            inputs, inputCount, outputs, outputCount, samples, context);
        const auto elapsed = juce::Time::getHighResolutionTicks() - before;
        if (samples <= 0) return;
        auto previous = peakTicks.load (std::memory_order_relaxed);
        while (elapsed > previous && ! peakTicks.compare_exchange_weak (
            previous, elapsed, std::memory_order_relaxed)) {}
        if (static_cast<double> (elapsed) > ticksPerSample * samples)
            overruns.fetch_add (1, std::memory_order_relaxed);
        blocks.fetch_add (1, std::memory_order_relaxed);
    }

    double getPeakMilliseconds() const
    {
        return 1000.0 * juce::Time::highResolutionTicksToSeconds (peakTicks.load (std::memory_order_relaxed));
    }
    int getOverruns() const { return overruns.load (std::memory_order_relaxed); }
    int getBlocks() const { return blocks.load (std::memory_order_relaxed); }
    void resetMeasurements() { peakTicks.store (0); overruns.store (0); blocks.store (0); }

private:
    double ticksPerSample = 0.0;
    std::atomic<juce::int64> peakTicks { 0 };
    std::atomic<int> overruns { 0 }, blocks { 0 };
};
}
