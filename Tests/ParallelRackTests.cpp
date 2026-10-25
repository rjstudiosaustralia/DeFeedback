#include "../Source/ParallelRackProcessor.h"
#include <iostream>
#include <thread>

namespace
{
int failures = 0;
void expect (bool ok, const char* message)
{
    if (! ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
struct Probe
{
    std::atomic<int> active { 0 }, maximum { 0 }, calls { 0 }, reentries { 0 };
};
class TestProcessor final : public juce::AudioProcessor
{
public:
    TestProcessor (int samples, float gainIn, Probe& probeIn)
        : AudioProcessor (BusesProperties().withInput ("In", juce::AudioChannelSet::mono(), true)
                                          .withOutput ("Out", juce::AudioChannelSet::mono(), true)),
          delaySamples (samples), gain (gainIn), probe (probeIn) { setLatencySamples (samples); }
    const juce::String getName() const override { return "Deterministic test delay"; }
    void prepareToPlay (double, int) override { delay.assign (static_cast<size_t> (delaySamples), 0.0f); position = 0; }
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        if (entered.exchange (true)) ++probe.reentries;
        const auto concurrent = probe.active.fetch_add (1) + 1;
        auto max = probe.maximum.load();
        while (max < concurrent && ! probe.maximum.compare_exchange_weak (max, concurrent)) {}
        // Enough deterministic work to expose actual overlap, without sleeps.
        double result = seed;
        for (int i = 0; i < 12000; ++i) result = std::sin (result + 0.001);
        seed = result;
        auto* data = buffer.getWritePointer (0);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            auto sample = data[i] * gain;
            if (! delay.empty())
            {
                std::swap (sample, delay[static_cast<size_t> (position)]);
                if (++position == static_cast<int> (delay.size())) position = 0;
            }
            data[i] = sample;
        }
        ++probe.calls;
        --probe.active;
        entered.store (false);
    }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
private:
    int delaySamples, position = 0;
    float gain;
    double seed = 0.123;
    Probe& probe;
    std::vector<float> delay;
    std::atomic<bool> entered { false };
};

std::unique_ptr<juce::AudioProcessorGraph> makeGraph (int latency, float gain, Probe& probe,
                                                     TestProcessor** processor = nullptr)
{
    using G = juce::AudioProcessorGraph;
    auto graph = std::make_unique<G>();
    graph->setPlayConfigDetails (1, 1, 48000, 64);
    const auto in = graph->addNode (std::make_unique<G::AudioGraphIOProcessor> (G::AudioGraphIOProcessor::audioInputNode));
    auto test = std::make_unique<TestProcessor> (latency, gain, probe);
    if (processor) *processor = test.get();
    const auto node = graph->addNode (std::move (test));
    const auto out = graph->addNode (std::make_unique<G::AudioGraphIOProcessor> (G::AudioGraphIOProcessor::audioOutputNode));
    expect (graph->addConnection ({ { in->nodeID, 0 }, { node->nodeID, 0 } }), "test input connected");
    expect (graph->addConnection ({ { node->nodeID, 0 }, { out->nodeID, 0 } }), "test output connected");
    return graph;
}

float inputSample (int channel, int sample) { return static_cast<float> ((channel + 1) * 10000 + sample) / 100000.0f; }

std::vector<float> render (bool parallel, int blockSize, Probe& probe)
{
    std::atomic<bool> muted { false };
    defeedback::ParallelRackProcessor rack (&muted);
    rack.setPlayConfigDetails (6, 6, 48000, blockSize);
    for (int lane = 0; lane < 4; ++lane)
        rack.addLane (makeGraph (lane * 11, static_cast<float> (lane + 1), probe), lane, 3 - lane);
    rack.configure (parallel, {}, 4);
    rack.prepareToPlay (48000, blockSize);
    expect (rack.getLatencySamples() == 33, "rack reports longest plugin latency without extra block");
    expect (parallel || rack.getProcessingThreadCount() == 1, "serial creates no helper threads");
    if (parallel && juce::SystemStats::getNumPhysicalCpus() > 1)
        expect (rack.getProcessingThreadCount() > 1, "parallel starts real-time workers");
    std::vector<float> rendered;
    int elapsed = 0, chunks = 0;
    juce::MidiBuffer midi;
    for (int cycle = 0; cycle < 90; ++cycle)
    {
        const auto samples = cycle % 5 == 0 ? blockSize * 3 + 1 : cycle % 5 == 1 ? 1 : blockSize;
        juce::AudioBuffer<float> buffer (6, samples);
        for (int c = 0; c < 6; ++c)
            for (int i = 0; i < samples; ++i) buffer.setSample (c, i, inputSample (c, elapsed + i));
        muted.store (cycle >= 30 && cycle < 45);
        rack.processBlock (buffer, midi);
        for (int c = 0; c < 6; ++c)
            for (int i = 0; i < samples; ++i)
            {
                const auto value = buffer.getSample (c, i);
                const auto lane = 3 - c;
                const auto expected = muted.load() || c >= 4 || elapsed + i < 33 ? 0.0f
                    : inputSample (lane, elapsed + i - 33) * static_cast<float> (lane + 1);
                if (std::memcmp (&value, &expected, sizeof (value)) != 0) { expect (false, "aligned, sample-exact routing and final mute"); return {}; }
                rendered.push_back (value);
            }
        elapsed += samples;
        chunks += (samples + blockSize - 1) / blockSize;
    }
    expect (probe.calls.load() == chunks * 4, "every lane processed exactly once per chunk");
    expect (probe.reentries.load() == 0, "no concurrent calls to the same plugin instance");
    juce::AudioBuffer<float> empty (6, 0);
    rack.processBlock (empty, midi);
    rack.releaseResources();
    expect (rack.getProcessingThreadCount() == 1, "workers stopped on release");
    return rendered;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (int blockSize : { 32, 64, 128, 512 })
    {
        Probe serial, parallel;
        const auto reference = render (false, blockSize, serial);
        const auto result = render (true, blockSize, parallel);
        expect (! reference.empty() && result == reference, "parallel output is bit-identical to serial");
        expect (serial.maximum.load() == 1, "serial runs only one lane at a time");
        if (juce::SystemStats::getNumPhysicalCpus() > 1)
            expect (parallel.maximum.load() > 1, "plugin processing overlaps on multiple threads");
        std::cout << blockSize << " samples: maximum simultaneous lanes " << parallel.maximum.load() << '\n';
    }
    for (int cycle = 0; cycle < 12; ++cycle)
    {
        Probe probe;
        defeedback::ParallelRackProcessor rack;
        rack.setPlayConfigDetails (1, 1, 48000, 64);
        TestProcessor* processor = nullptr;
        rack.addLane (makeGraph (0, 1.0f, probe, &processor), 0, 0);
        rack.configure (true, {});
        rack.prepareToPlay (48000, 64);
        expect (rack.getProcessingThreadCount() == 1, "one lane avoids worker overhead");
        juce::AudioBuffer<float> audio (1, 64);
        juce::MidiBuffer midi;
        audio.clear();
        rack.processBlock (audio, midi);
        processor->setLatencySamples (20);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
        for (int i = 0; i < 64; ++i) audio.setSample (0, i, 1.0f);
        rack.processBlock (audio, midi);
        expect (rack.needsLatencyRebuild() && audio.getMagnitude (0, 64) == 0.0f,
                "unexpected plugin latency silences output until rebuilt");
        rack.clear();
        for (int lane = 0; lane < 4; ++lane)
            rack.addLane (makeGraph (lane * 3, 2.0f, probe), lane, lane);
        rack.setPlayConfigDetails (4, 4, 96000, 128);
        rack.configure (cycle % 2 == 0, {}, 4);
        rack.prepareToPlay (96000, 128);
        audio.setSize (4, 128);
        for (int block = 0; block < 20; ++block)
        {
            audio.clear();
            rack.processBlock (audio, midi);
        }
        expect (! rack.needsLatencyRebuild(), "reprepare clears latency invalidation");
        expect (probe.reentries.load() == 0, "mode and sample-rate changes never overlap plugin calls");
    }
    std::cout << (failures == 0 ? "All parallel rack tests passed.\n" : "Parallel rack failures.\n");
    return failures == 0 ? 0 : 1;
}
