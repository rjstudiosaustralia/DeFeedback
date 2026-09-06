#include "SleepInhibitor.h"

#include <juce_events/juce_events.h>

#import <IOKit/pwr_mgt/IOPMLib.h>

namespace defeedback
{
class SleepInhibitor::Pimpl final
{
public:
    Pimpl()
    {
        const auto result = IOPMAssertionCreateWithName (
            kIOPMAssertionTypePreventUserIdleSystemSleep,
            kIOPMAssertionLevelOn,
            CFSTR ("DeFeedback Live is protecting a live audio session"),
            &assertionId);

        if (result == kIOReturnSuccess)
            active = true;
        else
            error = "macOS rejected the system-sleep assertion (IOKit error "
                  + juce::String (static_cast<int> (result)) + ").";
    }

    ~Pimpl()
    {
        if (active)
            IOPMAssertionRelease (assertionId);
    }

    juce::ScopedLowPowerModeDisabler appNapDisabler;
    IOPMAssertionID assertionId = kIOPMNullAssertionID;
    juce::String error;
    bool active = false;
};

SleepInhibitor::SleepInhibitor()
    : pimpl (std::make_unique<Pimpl>())
{
}

SleepInhibitor::~SleepInhibitor() = default;

bool SleepInhibitor::isActive() const noexcept
{
    return pimpl->active;
}

juce::String SleepInhibitor::getError() const
{
    return pimpl->error;
}
}
