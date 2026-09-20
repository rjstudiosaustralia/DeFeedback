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

bool ensureClearVoiceAudioUnitRegistered (juce::String& diagnostic)
{
    AudioComponentDescription desc {};
    desc.componentType = kAudioUnitType_Effect;
    desc.componentSubType = fourCC ('C', 'v', '1', '1');
    desc.componentManufacturer = fourCC ('F', 'a', 'b', 'S');
    if (AudioComponentFindNext (nullptr, &desc) != nullptr)
    {
        diagnostic = "Clear Voice Live 1.1 AU is registered with Core Audio.";
        return true;
    }

    // Retain the bundle for the lifetime of its process-local registration.
    static CFBundleRef retainedBundle = nullptr;
    if (retainedBundle == nullptr)
    {
        auto path = juce::File ("/Library/Audio/Plug-Ins/Components/Clear Voice Live 1.1.component");
        if (! path.isDirectory())
            path = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                       .getChildFile ("Library/Audio/Plug-Ins/Components/Clear Voice Live 1.1.component");
        if (! path.isDirectory())
        {
            diagnostic = "Clear Voice Live 1.1.component is not installed in an Audio Unit folder.";
            return false;
        }
        auto url = CFURLCreateFromFileSystemRepresentation (kCFAllocatorDefault,
            reinterpret_cast<const UInt8*> (path.getFullPathName().toRawUTF8()),
            static_cast<CFIndex> (path.getFullPathName().getNumBytesAsUTF8()), true);
        if (url != nullptr)
        {
            retainedBundle = CFBundleCreate (kCFAllocatorDefault, url);
            CFRelease (url);
        }
    }
    if (retainedBundle == nullptr || ! CFBundleLoadExecutable (retainedBundle))
    {
        diagnostic = "Could not load the Clear Voice Live 1.1 AU bundle.";
        return false;
    }
    const auto factory = reinterpret_cast<AudioComponentFactoryFunction> (
        CFBundleGetFunctionPointerForName (retainedBundle, CFSTR ("ClearVoiceLiveAUFactory")));
    if (factory == nullptr)
    {
        diagnostic = "The installed AU does not export ClearVoiceLiveAUFactory.";
        return false;
    }
    const auto info = CFBundleGetInfoDictionary (retainedBundle);
    const auto components = static_cast<CFArrayRef> (CFDictionaryGetValue (info, CFSTR ("AudioComponents")));
    int version = 0;
    if (components != nullptr && CFArrayGetCount (components) > 0)
    {
        const auto component = static_cast<CFDictionaryRef> (CFArrayGetValueAtIndex (components, 0));
        if (const auto value = static_cast<CFNumberRef> (CFDictionaryGetValue (component, CFSTR ("version"))))
            CFNumberGetValue (value, kCFNumberIntType, &version);
    }
    if (AudioComponentRegister (&desc, CFSTR ("Clear Voice: Clear Voice Live 1.1"),
                                 static_cast<UInt32> (version), factory) == nullptr)
    {
        diagnostic = "Core Audio rejected in-process registration of Clear Voice Live.";
        return false;
    }
    diagnostic = "Clear Voice Live AU loaded directly (system component cache was stale).";
    return true;
}

bool ensureVoiceIsoAudioUnitRegistered (juce::String& diagnostic)
{
    AudioComponentDescription desc {};
    desc.componentType = kAudioUnitType_Effect;
    desc.componentSubType = fourCC ('V', 'i', 's', 'o');
    desc.componentManufacturer = fourCC ('M', 'a', 'z', 'e');
    if (AudioComponentFindNext (nullptr, &desc) != nullptr)
    {
        diagnostic = "VoiceISO AU is registered with Core Audio.";
        return true;
    }

    // Retain the bundle for the lifetime of its process-local registration.
    static CFBundleRef retainedBundle = nullptr;
    if (retainedBundle == nullptr)
    {
        auto path = juce::File ("/Library/Audio/Plug-Ins/Components/VoiceISO.component");
        if (! path.isDirectory())
            path = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                       .getChildFile ("Library/Audio/Plug-Ins/Components/VoiceISO.component");
        if (! path.isDirectory())
        {
            diagnostic = "VoiceISO.component is not installed in an Audio Unit folder.";
            return false;
        }
        auto url = CFURLCreateFromFileSystemRepresentation (kCFAllocatorDefault,
            reinterpret_cast<const UInt8*> (path.getFullPathName().toRawUTF8()),
            static_cast<CFIndex> (path.getFullPathName().getNumBytesAsUTF8()), true);
        if (url != nullptr)
        {
            retainedBundle = CFBundleCreate (kCFAllocatorDefault, url);
            CFRelease (url);
        }
    }
    if (retainedBundle == nullptr || ! CFBundleLoadExecutable (retainedBundle))
    {
        diagnostic = "Could not load the VoiceISO AU bundle.";
        return false;
    }
    const auto factory = reinterpret_cast<AudioComponentFactoryFunction> (
        CFBundleGetFunctionPointerForName (retainedBundle, CFSTR ("VoiceISOAUFactory")));
    if (factory == nullptr)
    {
        diagnostic = "The installed AU does not export VoiceISOAUFactory.";
        return false;
    }
    const auto info = CFBundleGetInfoDictionary (retainedBundle);
    const auto components = static_cast<CFArrayRef> (CFDictionaryGetValue (info, CFSTR ("AudioComponents")));
    int version = 0;
    if (components != nullptr && CFArrayGetCount (components) > 0)
    {
        const auto component = static_cast<CFDictionaryRef> (CFArrayGetValueAtIndex (components, 0));
        if (const auto value = static_cast<CFNumberRef> (CFDictionaryGetValue (component, CFSTR ("version"))))
            CFNumberGetValue (value, kCFNumberIntType, &version);
    }
    if (AudioComponentRegister (&desc, CFSTR ("Maizesoft: VoiceISO"),
                                 static_cast<UInt32> (version), factory) == nullptr)
    {
        diagnostic = "Core Audio rejected in-process registration of VoiceISO.";
        return false;
    }
    diagnostic = "VoiceISO AU loaded directly (system component cache was stale).";
    return true;
}
}
