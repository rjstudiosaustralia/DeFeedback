#include "MeterProcessor.h"

namespace defeedback
{
MeterProcessor::MeterProcessor (std::atomic<bool>* muteFlag,
                                bool measureAfterMute,
                                std::atomic<bool>* dryPass)
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", dryPass != nullptr ? juce::AudioChannelSet::stereo()
                                                                 : juce::AudioChannelSet::mono(), true)
                          .withOutput ("Output", juce::AudioChannelSet::mono(), true)),
      outputMute (muteFlag),
      perLaneDry (dryPass),
      measureAfterGate (measureAfterMute)
{
}

bool MeterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == (perLaneDry != nullptr ? juce::AudioChannelSet::stereo()
                                                                     : juce::AudioChannelSet::mono())
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

template <typename Sample>
void MeterProcessor::updatePeak (const juce::AudioBuffer<Sample>& buffer)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    auto value = static_cast<float> (buffer.getMagnitude (0, 0, buffer.getNumSamples()));
    auto previous = peak.load (std::memory_order_relaxed);
    while (value > previous
           && ! peak.compare_exchange_weak (previous, value, std::memory_order_relaxed))
    {
    }
}

void MeterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (perLaneDry != nullptr && perLaneDry->load (std::memory_order_acquire))
        buffer.copyFrom (0, 0, buffer, 1, 0, buffer.getNumSamples());
    if (! measureAfterGate)
        updatePeak (buffer);

    if (outputMute != nullptr && outputMute->load (std::memory_order_relaxed))
        buffer.clear();

    if (measureAfterGate)
        updatePeak (buffer);
}

void MeterProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&)
{
    if (perLaneDry != nullptr && perLaneDry->load (std::memory_order_acquire))
        buffer.copyFrom (0, 0, buffer, 1, 0, buffer.getNumSamples());
    if (! measureAfterGate)
        updatePeak (buffer);

    if (outputMute != nullptr && outputMute->load (std::memory_order_relaxed))
        buffer.clear();

    if (measureAfterGate)
        updatePeak (buffer);
}
}
