#pragma once

#include <juce_core/juce_core.h>

namespace defeedback
{
enum class LoginItemStatus
{
    disabled,
    enabled,
    requiresApproval,
    unavailable
};

LoginItemStatus getLoginItemStatus();
bool setLaunchAtLogin (bool enabled, juce::String& error);
}
