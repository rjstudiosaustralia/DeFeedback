#pragma once

#include <juce_core/juce_core.h>

namespace defeedback
{
class SleepInhibitor final
{
public:
    SleepInhibitor();
    ~SleepInhibitor();

    bool isActive() const noexcept;
    juce::String getError() const;

private:
    class Pimpl;
    std::unique_ptr<Pimpl> pimpl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SleepInhibitor)
};
}
