#include "MeterProcessor.h"

namespace defeedback
{
MeterProcessor::MeterProcessor (std::atomic<bool>* muteFlag)
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::mono(), true)
                          .withOutput ("Output", juce::AudioChannelSet::mono(), true)),
      outputMute (muteFlag)
{
}

bool MeterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono()
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
    updatePeak (buffer);
    if (outputMute != nullptr && outputMute->load (std::memory_order_relaxed))
        buffer.clear();
}

void MeterProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&)
{
    updatePeak (buffer);
    if (outputMute != nullptr && outputMute->load (std::memory_order_relaxed))
        buffer.clear();
}
}
