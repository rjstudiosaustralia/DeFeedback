#pragma once

#include "AppConfig.h"

namespace defeedback
{
class SettingsStore
{
public:
    SettingsStore();

    AppConfig load() const;
    bool save (const AppConfig&, juce::String& error) const;
    juce::File getFile() const { return settingsFile; }

private:
    juce::File settingsFile;
    juce::File legacySettingsFile;
};
}
