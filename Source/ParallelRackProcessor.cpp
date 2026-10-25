#include "ParallelRackProcessor.h"

#if JUCE_MAC
 #include <mach/mach.h>
 #include <mach/semaphore.h>
 #include <mach/sync_policy.h>
 #include <sys/sysctl.h>
#endif

namespace defeedback
{
namespace
{
// Mach semaphores are allocated during setup; signaling/waiting requires no
// application mutex, allocation or polling loop. Each worker has one producer
// and one consumer on each event, and acknowledges every dispatched cycle.
class AudioEvent
{
public:
   #if JUCE_MAC
    AudioEvent() { valid = semaphore_create (mach_task_self(), &event, SYNC_POLICY_FIFO, 0) == KERN_SUCCESS; }
    ~AudioEvent() { if (valid) semaphore_destroy (mach_task_self(), event); }
    bool isValid() const { return valid; }
    void signal() { const auto r = semaphore_signal (event); juce::ignoreUnused (r); jassert (r == KERN_SUCCESS); }
    void wait()
    {
        kern_return_t result;
        do { result = semaphore_wait (event); } while (result == KERN_ABORTED);
        jassert (result == KERN_SUCCESS);
    }
   private:
    semaphore_t event {};
    bool valid = false;
   #else
    bool isValid() const { return true; }
    void signal() { event.signal(); }
    void wait() { event.wait(); }
   private:
    juce::WaitableEvent event;
   #endif
};

int hardwareThreadLimit()
{
   #if JUCE_MAC
    // Apple Silicon performance cores are the conservative starting point.
    // Core Audio's workgroup recommendation may lower this further.
    int performanceCores = 0;
    size_t bytes = sizeof (performanceCores);
    if (sysctlbyname ("hw.perflevel0.physicalcpu", &performanceCores, &bytes, nullptr, 0) == 0
        && performanceCores > 0)
        return performanceCores;
   #endif
    return juce::jmax (1, juce::SystemStats::getNumPhysicalCpus());
}
}

struct ParallelRackProcessor::Lane : private juce::AudioProcessorListener
{
    explicit Lane (std::atomic<bool>& invalidated) : latencyInvalidated (invalidated) {}
    ~Lane() override { stopWatchingLatency(); }

    void watchLatency()
    {
        stopWatchingLatency();
        for (const auto& node : graph->getNodes())
            watchedProcessors.emplace_back (node->getProcessor(), node->getProcessor()->getLatencySamples());
        watchedProcessors.emplace_back (graph.get(), graph->getLatencySamples());
        for (const auto& entry : watchedProcessors) entry.first->addListener (this);
    }

    void stopWatchingLatency()
    {
        for (const auto& entry : watchedProcessors) entry.first->removeListener (this);
        watchedProcessors.clear();
    }

    std::unique_ptr<juce::AudioProcessorGraph> graph;
    int input = 0, output = 0, graphLatency = 0, delayPosition = 0;
    juce::AudioBuffer<float> buffer;
    juce::MidiBuffer midi;
    std::vector<float> alignmentDelay;

private:
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged (juce::AudioProcessor* processor, const ChangeDetails& details) override
    {
        if (details.latencyChanged)
            for (const auto& entry : watchedProcessors)
                if (entry.first == processor && entry.second != processor->getLatencySamples())
                    latencyInvalidated.store (true, std::memory_order_release);
    }
    std::atomic<bool>& latencyInvalidated;
    std::vector<std::pair<juce::AudioProcessor*, int>> watchedProcessors;
};

class ParallelRackProcessor::Worker final : private juce::Thread
{
public:
    explicit Worker (ParallelRackProcessor& processor, int index)
        : Thread ("DeFeedback audio " + juce::String (index)), owner (processor) {}

    bool start (int blockSize, double sampleRate)
    {
        return ready.isValid() && done.isValid()
            && startRealtimeThread (RealtimeOptions{}.withApproximateAudioProcessingTime (blockSize, sampleRate));
    }

    ~Worker() override
    {
        signalThreadShouldExit();
        if (isThreadRunning())
        {
            ready.signal();
            // Only on teardown, after the device callback has been detached.
            stopThread (-1);
        }
    }

    void dispatch()
    {
        cycleReady.store (true, std::memory_order_release);
        ready.signal();
    }
    void joinCycle()
    {
        done.wait();
        // Publish all lane output writes before the callback reads them.
        const auto completed = cycleDone.exchange (false, std::memory_order_acquire);
        juce::ignoreUnused (completed);
        jassert (completed);
    }

private:
    void run() override
    {
        juce::ScopedNoDenormals noDenormals;
        // The retained group outlives its token; both are destroyed on this
        // worker, never on the audio or UI thread.
        juce::AudioWorkgroup joinedGroup;
        juce::WorkgroupToken token;
        for (;;)
        {
            ready.wait();
            if (threadShouldExit()) break;
            if (! cycleReady.exchange (false, std::memory_order_acquire)) continue;
            if (joinedGroup != owner.workgroup)
            {
                token.reset();
                joinedGroup = owner.workgroup;
                joinedGroup.join (token);
            }
            owner.runAvailableLanes();
            cycleDone.store (true, std::memory_order_release);
            done.signal();
        }
    }

    ParallelRackProcessor& owner;
    AudioEvent ready, done;
    std::atomic<bool> cycleReady { false }, cycleDone { false };
};

ParallelRackProcessor::ParallelRackProcessor (std::atomic<bool>* mute)
    : AudioProcessor (BusesProperties().withInput ("Inputs", juce::AudioChannelSet::mono(), true)
                                      .withOutput ("Outputs", juce::AudioChannelSet::mono(), true)),
      outputMute (mute)
{
}

ParallelRackProcessor::~ParallelRackProcessor() { clear(); }

void ParallelRackProcessor::clear()
{
    releaseResources();
    laneGraphs.clear();
    setLatencySamples (0);
}

void ParallelRackProcessor::addLane (std::unique_ptr<juce::AudioProcessorGraph> graph, int input, int output)
{
    jassert (preparedBlockSize == 0);
    auto lane = std::make_unique<Lane> (latencyInvalidated);
    lane->graph = std::move (graph);
    lane->input = input;
    lane->output = output;
    laneGraphs.push_back (std::move (lane));
}

void ParallelRackProcessor::configure (bool parallel, const juce::AudioWorkgroup& group, int maximumThreads)
{
    jassert (preparedBlockSize == 0);
    parallelEnabled = parallel;
    requestedThreads = maximumThreads;
    workgroup = group;
    deviceWorkgroupActive.store (static_cast<bool> (group));
}

int ParallelRackProcessor::parallelThreadLimit() const
{
    auto count = juce::jmin (static_cast<int> (laneGraphs.size()), hardwareThreadLimit());
    if (requestedThreads > 0) count = juce::jmin (count, requestedThreads);
    if (const auto recommended = workgroup.getMaxParallelThreadCount(); recommended > 0)
        count = juce::jmin (count, static_cast<int> (recommended));
    return parallelEnabled ? juce::jmax (1, count) : 1;
}

void ParallelRackProcessor::prepareToPlay (double sampleRate, int blockSize)
{
    releaseResources();
    if (blockSize <= 0 || sampleRate <= 0.0) return;
    preparedBlockSize = blockSize;
    latencyInvalidated.store (false);
    auto longestLatency = 0;
    for (auto& lane : laneGraphs)
    {
        lane->buffer.setSize (1, blockSize);
        lane->buffer.clear();
        lane->midi.ensureSize (2048);
        lane->graph->setPlayConfigDetails (1, 1, sampleRate, blockSize);
        lane->graph->prepareToPlay (sampleRate, blockSize);
        lane->graphLatency = juce::jmax (0, lane->graph->getLatencySamples());
        longestLatency = juce::jmax (longestLatency, lane->graphLatency);
        lane->watchLatency();
    }
    for (auto& lane : laneGraphs)
    {
        lane->alignmentDelay.assign (static_cast<size_t> (longestLatency - lane->graphLatency), 0.0f);
        lane->delayPosition = 0;
    }
    setLatencySamples (longestLatency);
    audioWorkgroupContextChanged (workgroup);
    const auto limit = parallelThreadLimit();
    for (int i = 1; i < limit; ++i)
    {
        auto worker = std::make_unique<Worker> (*this, i);
        if (! worker->start (blockSize, sampleRate)) break;
        workers.push_back (std::move (worker));
    }
    processingThreads.store (static_cast<int> (workers.size()) + 1);
}

void ParallelRackProcessor::releaseResources()
{
    workers.clear();
    if (preparedBlockSize != 0)
        for (auto& lane : laneGraphs)
        {
            lane->stopWatchingLatency();
            lane->graph->releaseResources();
        }
    preparedBlockSize = 0;
    processingThreads.store (1);
}

void ParallelRackProcessor::audioWorkgroupContextChanged (const juce::AudioWorkgroup& group)
{
    workgroup = group;
    deviceWorkgroupActive.store (static_cast<bool> (group));
    // The previous cycle has joined. Workers read this only after dispatch.
    for (auto& lane : laneGraphs)
        for (const auto& node : lane->graph->getNodes())
            node->getProcessor()->audioWorkgroupContextChanged (group);
}

void ParallelRackProcessor::runAvailableLanes()
{
    for (;;)
    {
        const auto index = nextLane.fetch_add (1, std::memory_order_relaxed);
        if (index >= laneGraphs.size()) return;
        processLane (*laneGraphs[index]);
    }
}

void ParallelRackProcessor::processLane (Lane& lane)
{
    auto* data = lane.buffer.getWritePointer (0);
    juce::AudioBuffer<float> block (&data, 1, cycleSamples);
    lane.midi.clear();
    lane.graph->setPlayHead (&blockPlayHead);
    lane.graph->processBlock (block, lane.midi);
    lane.graph->setPlayHead (nullptr);
    if (lane.graph->getLatencySamples() != lane.graphLatency)
        latencyInvalidated.store (true, std::memory_order_release);
    if (! lane.alignmentDelay.empty())
        for (int i = 0; i < cycleSamples; ++i)
        {
            std::swap (data[i], lane.alignmentDelay[static_cast<size_t> (lane.delayPosition)]);
            if (++lane.delayPosition == static_cast<int> (lane.alignmentDelay.size())) lane.delayPosition = 0;
        }
}

void ParallelRackProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    if (preparedBlockSize <= 0 || latencyInvalidated.load (std::memory_order_acquire))
    {
        audio.clear();
        return;
    }
    // No extra buffering cycle: all workers finish this same hardware block.
    // Oversized device callbacks are split using the existing prepared buffers.
    const auto position = getPlayHead() != nullptr ? getPlayHead()->getPosition() : juce::nullopt;
    auto activeWorkers = static_cast<int> (workers.size());
    if (const auto recommended = workgroup.getMaxParallelThreadCount(); recommended > 0)
        activeWorkers = juce::jmin (activeWorkers, juce::jmax (0, static_cast<int> (recommended) - 1));
    processingThreads.store (activeWorkers + 1, std::memory_order_relaxed);
    for (int offset = 0; offset < audio.getNumSamples(); offset += cycleSamples)
    {
        cycleSamples = juce::jmin (preparedBlockSize, audio.getNumSamples() - offset);
        blockPlayHead.position = position;
        if (blockPlayHead.position.hasValue())
        {
            if (auto samples = blockPlayHead.position->getTimeInSamples())
                blockPlayHead.position->setTimeInSamples (*samples + offset);
            if (auto seconds = blockPlayHead.position->getTimeInSeconds())
                blockPlayHead.position->setTimeInSeconds (*seconds + offset / getSampleRate());
        }
        // Copy every input before overwriting any output (including crossed routes).
        for (auto& lane : laneGraphs)
            if (juce::isPositiveAndBelow (lane->input, audio.getNumChannels()))
                lane->buffer.copyFrom (0, 0, audio, lane->input, offset, cycleSamples);
            else
                lane->buffer.clear (0, cycleSamples);
        nextLane.store (0, std::memory_order_relaxed);
        for (int i = 0; i < activeWorkers; ++i) workers[static_cast<size_t> (i)]->dispatch();
        runAvailableLanes(); // The hardware callback contributes a processing thread.
        for (int i = 0; i < activeWorkers; ++i) workers[static_cast<size_t> (i)]->joinCycle();
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            audio.clear (channel, offset, cycleSamples);
        const auto muted = outputMute != nullptr && outputMute->load (std::memory_order_relaxed);
        if (! muted && ! latencyInvalidated.load (std::memory_order_acquire))
            for (auto& lane : laneGraphs)
                if (juce::isPositiveAndBelow (lane->output, audio.getNumChannels()))
                    audio.copyFrom (lane->output, offset, lane->buffer, 0, 0, cycleSamples);
    }
    // If mute/latency changes during an oversized callback, also gate chunks
    // that completed earlier in this same callback.
    if ((outputMute != nullptr && outputMute->load (std::memory_order_relaxed))
        || latencyInvalidated.load (std::memory_order_acquire)) audio.clear();
}
}
