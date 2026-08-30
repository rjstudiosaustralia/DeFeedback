#pragma once

#include "../Core/RingGuardCore.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <memory>
#include <vector>

class RingGuardAudioProcessor final : public juce::AudioProcessor
{
public:
    RingGuardAudioProcessor();
    ~RingGuardAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destinationData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    [[nodiscard]] ringguard::Telemetry getTelemetry() const noexcept { return core.getTelemetry(); }
    [[nodiscard]] static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    ringguard::RealtimeProcessor core;
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* strength = nullptr;
    std::atomic<float>* sensitivity = nullptr;
    std::atomic<float>* maxNotches = nullptr;
    std::atomic<float>* bypass = nullptr;
    std::atomic<float>* mute = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RingGuardAudioProcessor)
};
