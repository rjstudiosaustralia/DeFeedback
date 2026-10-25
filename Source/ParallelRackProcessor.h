#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace defeedback
{
// Each lane owns a mono graph. Topology, workers and buffers may only change
// while the device callback is detached. The player's callback lock covers the
// entire dispatch/join cycle, including plugin ON/OFF transitions.
class ParallelRackProcessor final : public juce::AudioProcessor
{
public:
    explicit ParallelRackProcessor (std::atomic<bool>* masterMute = nullptr);
    ~ParallelRackProcessor() override;

    void clear();
    void addLane (std::unique_ptr<juce::AudioProcessorGraph>, int input, int output);
    void configure (bool parallel, const juce::AudioWorkgroup&, int maximumThreads = 0);
    int getProcessingThreadCount() const noexcept { return processingThreads.load(); }
    bool hasDeviceWorkgroup() const noexcept { return deviceWorkgroupActive.load(); }
    bool needsLatencyRebuild() const noexcept { return latencyInvalidated.load(); }

    const juce::String getName() const override { return "Parallel mono rack"; }
    void prepareToPlay (double, int) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void audioWorkgroupContextChanged (const juce::AudioWorkgroup&) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
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

private:
    struct Lane;
    class Worker;
    class BlockPlayHead final : public juce::AudioPlayHead
    {
    public:
        juce::Optional<PositionInfo> position;
        juce::Optional<PositionInfo> getPosition() const override { return position; }
    };

    void runAvailableLanes();
    void processLane (Lane&);
    int parallelThreadLimit() const;

    std::vector<std::unique_ptr<Lane>> laneGraphs;
    std::vector<std::unique_ptr<Worker>> workers;
    juce::AudioWorkgroup workgroup;
    BlockPlayHead blockPlayHead;
    std::atomic<bool>* outputMute;
    bool parallelEnabled = true;
    int requestedThreads = 0, preparedBlockSize = 0, cycleSamples = 0;
    std::atomic<size_t> nextLane { 0 };
    std::atomic<int> processingThreads { 1 };
    std::atomic<bool> deviceWorkgroupActive { false }, latencyInvalidated { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParallelRackProcessor)
};
}
