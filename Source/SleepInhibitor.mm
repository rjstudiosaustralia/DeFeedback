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
        createAssertion (kIOPMAssertionTypePreventUserIdleSystemSleep, systemAssertion, "system");
        createAssertion (kIOPMAssertionTypePreventUserIdleDisplaySleep, displayAssertion, "display");
        active = systemAssertion != kIOPMNullAssertionID && displayAssertion != kIOPMNullAssertionID;
    }

    ~Pimpl()
    {
        if (systemAssertion != kIOPMNullAssertionID) IOPMAssertionRelease (systemAssertion);
        if (displayAssertion != kIOPMNullAssertionID) IOPMAssertionRelease (displayAssertion);
    }

    void createAssertion (CFStringRef type, IOPMAssertionID& assertion, const char* name)
    {
        const auto result = IOPMAssertionCreateWithName (
            type, kIOPMAssertionLevelOn,
            CFSTR ("DeFeedback Live is protecting a live audio session"), &assertion);
        if (result != kIOReturnSuccess)
        {
            assertion = kIOPMNullAssertionID;
            if (error.isNotEmpty()) error += " ";
            error += "macOS rejected the " + juce::String (name) + "-sleep assertion (IOKit error "
                   + juce::String (static_cast<int> (result)) + ").";
        }
    }

    juce::ScopedLowPowerModeDisabler appNapDisabler;
    IOPMAssertionID systemAssertion = kIOPMNullAssertionID;
    IOPMAssertionID displayAssertion = kIOPMNullAssertionID;
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
