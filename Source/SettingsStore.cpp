#include "SettingsStore.h"

#if JUCE_MAC
 #include <sys/stat.h>
#endif

namespace defeedback
{
SettingsStore::SettingsStore()
{
    const auto library = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if DEFEEDBACK_MULTICORE_PREVIEW
    const auto applicationFolder = "DeFeedback Live Multicore";
   #else
    const auto applicationFolder = "DeFeedback Live";
   #endif
    settingsFile = library.getChildFile ("Application Support")
                          .getChildFile ("RJ Studios Australia")
                          .getChildFile (applicationFolder)
                          .getChildFile ("settings.xml");
    legacySettingsFile = library.getChildFile ("RJ Studios Australia")
                                .getChildFile (applicationFolder)
                                .getChildFile ("settings.xml");
}

AppConfig SettingsStore::load() const
{
    if (auto xml = juce::XmlDocument::parse (settingsFile))
        return AppConfig::fromXml (*xml);

   #if ! DEFEEDBACK_MULTICORE_PREVIEW
    if (auto xml = juce::XmlDocument::parse (legacySettingsFile))
        return AppConfig::fromXml (*xml);

   #endif
    AppConfig defaults;
   #if DEFEEDBACK_MULTICORE_PREVIEW
    defaults.autoStart = defaults.launchAtLogin = false;
    defaults.remoteControlPort = 8767;
   #endif
    return defaults;
}

bool SettingsStore::save (const AppConfig& config, juce::String& error) const
{
    const auto parent = settingsFile.getParentDirectory();
    if (! parent.createDirectory())
    {
        error = "Could not create settings directory: " + parent.getFullPathName();
        return false;
    }

    juce::TemporaryFile temporary (settingsFile);
    if (! config.toXml()->writeTo (temporary.getFile()))
    {
        error = "Could not write temporary settings file.";
        return false;
    }

    if (! temporary.overwriteTargetFileWithTemporary())
    {
        error = "Could not atomically replace settings.xml.";
        return false;
    }

   #if JUCE_MAC
    if (::chmod (settingsFile.getFullPathName().toRawUTF8(), S_IRUSR | S_IWUSR) != 0)
    {
        error = "Settings were saved, but their file permissions could not be restricted to this user.";
        return false;
    }
   #endif

    return true;
}
}
