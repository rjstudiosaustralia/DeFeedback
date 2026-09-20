#pragma once
#include "AppConfig.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace defeedback
{
// Some AUs restore an unchanged opaque state block but reset exposed controls.
// Each lane therefore carries a separate snapshot of published parameter IDs
// for each plugin it has used. Snapshots never cross plugin identities.
inline juce::String captureModeParameters (juce::AudioProcessor& processor, PluginMode mode)
{
    juce::XmlElement state ("MODE_PARAMETERS");
    state.setAttribute ("pluginMode", pluginModeId (mode));
    for (auto* parameter : processor.getParameters())
        if (auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*> (parameter))
        {
            const auto value = hosted->getValue();
            if (! std::isfinite (value)) continue;
            auto* item = state.createNewChildElement ("PARAM");
            item->setAttribute ("id", hosted->getParameterID());
            item->setAttribute ("name", hosted->getName (128));
            item->setAttribute ("value", static_cast<double> (value));
        }
    return state.toString();
}

inline void restoreModeParameters (juce::AudioProcessor& processor, PluginMode mode, const juce::String& saved)
{
    auto state = juce::XmlDocument::parse (saved);
    if (state == nullptr || ! state->hasTagName ("MODE_PARAMETERS")
        || state->getStringAttribute ("pluginMode") != pluginModeId (mode)) return;
    for (auto* parameter : processor.getParameters())
        if (auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*> (parameter))
            for (auto* item : state->getChildIterator())
            {
                if (item->getStringAttribute ("id") != hosted->getParameterID()
                    || item->getStringAttribute ("name") != hosted->getName (128)) continue;
                const auto value = item->getDoubleAttribute ("value", -1.0);
                if (std::isfinite (value) && value >= 0.0 && value <= 1.0)
                    hosted->setValueNotifyingHost (static_cast<float> (value));
                break;
            }
}
}
