#include "PluginRegistration.h"

#import <AudioToolbox/AudioToolbox.h>
#import <CoreFoundation/CoreFoundation.h>

namespace defeedback
{
namespace
{
constexpr OSType fourCC (char a, char b, char c, char d)
{
    return (static_cast<OSType> (static_cast<unsigned char> (a)) << 24)
         | (static_cast<OSType> (static_cast<unsigned char> (b)) << 16)
         | (static_cast<OSType> (static_cast<unsigned char> (c)) << 8)
         |  static_cast<OSType> (static_cast<unsigned char> (d));
}

AudioComponentDescription description()
{
    AudioComponentDescription value {};
    value.componentType = kAudioUnitType_Effect;
    value.componentSubType = fourCC ('F', 'b', 'T', 'I');
    value.componentManufacturer = fourCC ('j', 'D', 'S', 'P');
    return value;
}
}

bool ensureDeFeedbackAudioUnitRegistered (juce::String& diagnostic)
{
    auto desc = description();
    if (AudioComponentFindNext (nullptr, &desc) != nullptr)
    {
        diagnostic = "De-Feedback AU is registered with Core Audio.";
        return true;
    }

    static CFBundleRef retainedBundle = nullptr;
    if (retainedBundle == nullptr)
    {
        const auto bundlePath = juce::File ("/Library/Audio/Plug-Ins/Components/Defeedback.component");
        if (! bundlePath.isDirectory())
        {
            diagnostic = "De-Feedback.component is not installed in /Library/Audio/Plug-Ins/Components.";
            return false;
        }

        auto url = CFURLCreateFromFileSystemRepresentation (kCFAllocatorDefault,
                                                            reinterpret_cast<const UInt8*> (bundlePath.getFullPathName().toRawUTF8()),
                                                            static_cast<CFIndex> (bundlePath.getFullPathName().getNumBytesAsUTF8()),
                                                            true);
        if (url == nullptr)
        {
            diagnostic = "Could not create a URL for De-Feedback.component.";
            return false;
        }

        retainedBundle = CFBundleCreate (kCFAllocatorDefault, url);
        CFRelease (url);
    }

    if (retainedBundle == nullptr || ! CFBundleLoadExecutable (retainedBundle))
    {
        diagnostic = "Core Foundation could not load the De-Feedback AU bundle.";
        return false;
    }

    const auto factory = reinterpret_cast<AudioComponentFactoryFunction> (
        CFBundleGetFunctionPointerForName (retainedBundle, CFSTR ("DefeedbackAUFactory")));

    if (factory == nullptr)
    {
        diagnostic = "The installed AU does not export DefeedbackAUFactory.";
        return false;
    }

    auto component = AudioComponentRegister (&desc,
                                             CFSTR ("Alpha Labs LLC: De-Feedback"),
                                             0x00010104,
                                             factory);
    if (component == nullptr)
    {
        diagnostic = "Core Audio rejected in-process registration of De-Feedback.";
        return false;
    }

    diagnostic = "De-Feedback AU was loaded directly because the system component cache was stale.";
    return true;
}
}
