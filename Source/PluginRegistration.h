#pragma once

#include <juce_core/juce_core.h>

namespace defeedback
{
bool ensureDeFeedbackAudioUnitRegistered (juce::String& diagnostic);
bool ensureClearVoiceAudioUnitRegistered (juce::String& diagnostic);
bool ensureVoiceIsoAudioUnitRegistered (juce::String& diagnostic);
}
