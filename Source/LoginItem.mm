#include "LoginItem.h"

#import <ServiceManagement/ServiceManagement.h>

namespace defeedback
{
LoginItemStatus getLoginItemStatus()
{
    if (@available (macOS 13.0, *))
    {
        switch (SMAppService.mainAppService.status)
        {
            case SMAppServiceStatusEnabled:          return LoginItemStatus::enabled;
            case SMAppServiceStatusRequiresApproval: return LoginItemStatus::requiresApproval;
            case SMAppServiceStatusNotRegistered:    return LoginItemStatus::disabled;
            case SMAppServiceStatusNotFound:         return LoginItemStatus::unavailable;
        }
    }

    return LoginItemStatus::unavailable;
}

bool setLaunchAtLogin (bool enabled, juce::String& error)
{
    if (@available (macOS 13.0, *))
    {
        NSError* nativeError = nil;
        const auto success = enabled
                           ? [SMAppService.mainAppService registerAndReturnError:&nativeError]
                           : [SMAppService.mainAppService unregisterAndReturnError:&nativeError];

        if (! success && nativeError != nil)
            error = juce::String::fromUTF8 (nativeError.localizedDescription.UTF8String);

        return success;
    }

    error = "Launch at login requires macOS 13 or later.";
    return false;
}
}
