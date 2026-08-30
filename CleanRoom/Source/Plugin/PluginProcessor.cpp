#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace
{
constexpr auto parameterTreeName = "RINGGUARD_PARAMETERS";
constexpr auto strengthId = "strength";
constexpr auto sensitivityId = "sensitivity";
constexpr auto maxNotchesId = "maxNotches";
constexpr auto bypassId = "bypass";
constexpr auto muteId = "mute";
}

RingGuardAudioProcessor::RingGuardAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, parameterTreeName, createParameterLayout())
{
    strength = parameters.getRawParameterValue (strengthId);
    sensitivity = parameters.getRawParameterValue (sensitivityId);
    maxNotches = parameters.getRawParameterValue (maxNotchesId);
    bypass = parameters.getRawParameterValue (bypassId);
    mute = parameters.getRawParameterValue (muteId);
    setLatencySamples (0);
}

juce::AudioProcessorValueTreeState::ParameterLayout RingGuardAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> result;

    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { strengthId, 1 }, "Strength",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float value, int) { return juce::String (juce::roundToInt (value * 100.0f)) + "%"; })));

    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { sensitivityId, 1 }, "Sensitivity",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.65f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float value, int) { return juce::String (juce::roundToInt (value * 100.0f)) + "%"; })));

    result.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { maxNotchesId, 1 }, "Maximum Notches", 1,
        ringguard::RealtimeProcessor::maximumNotches, 4));
    result.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { bypassId, 1 }, "Bypass", false));
    result.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { muteId, 1 }, "Mute", false));

    return { result.begin(), result.end() };
}

void RingGuardAudioProcessor::prepareToPlay (double sampleRate,
                                              int maximumExpectedSamplesPerBlock)
{
    core.prepare (sampleRate,
                  static_cast<std::size_t> (juce::jmax (1, maximumExpectedSamplesPerBlock)),
                  juce::jlimit (1, ringguard::RealtimeProcessor::maximumChannels,
                                getTotalNumInputChannels()));
    setLatencySamples (0);
}

void RingGuardAudioProcessor::releaseResources()
{
    core.reset();
}

void RingGuardAudioProcessor::reset()
{
    core.reset();
}

bool RingGuardAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    if (input != output)
        return false;
    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

void RingGuardAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;

    const auto inputChannels = getTotalNumInputChannels();
    const auto outputChannels = getTotalNumOutputChannels();
    for (auto channel = inputChannels; channel < outputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (mute != nullptr && mute->load (std::memory_order_relaxed) >= 0.5f)
    {
        buffer.clear();
        return;
    }

    if (bypass != nullptr && bypass->load (std::memory_order_relaxed) >= 0.5f)
        return;

    core.setSettings ({
        strength != nullptr ? strength->load (std::memory_order_relaxed) : 1.0f,
        sensitivity != nullptr ? sensitivity->load (std::memory_order_relaxed) : 0.65f,
        maxNotches != nullptr ? juce::roundToInt (maxNotches->load (std::memory_order_relaxed)) : 4
    });

    core.process (buffer.getArrayOfWritePointers(),
                  juce::jmin (inputChannels, ringguard::RealtimeProcessor::maximumChannels),
                  static_cast<std::size_t> (buffer.getNumSamples()));
}

juce::AudioProcessorEditor* RingGuardAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void RingGuardAudioProcessor::getStateInformation (juce::MemoryBlock& destinationData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destinationData);
}

void RingGuardAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RingGuardAudioProcessor();
}
