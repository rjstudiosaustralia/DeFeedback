#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace defeedback
{
class MeterProcessor final : public juce::AudioProcessor
{
public:
    explicit MeterProcessor (std::atomic<bool>* outputMute = nullptr,
                             bool measureAfterMute = false);

    const juce::String getName() const override { return "Lane Meter"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }

    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    float consumePeak() noexcept { return peak.exchange (0.0f); }

private:
    template <typename Sample>
    void updatePeak (const juce::AudioBuffer<Sample>&);

    std::atomic<float> peak { 0.0f };
    std::atomic<bool>* outputMute = nullptr;
    bool measureAfterGate = false;
};
}
