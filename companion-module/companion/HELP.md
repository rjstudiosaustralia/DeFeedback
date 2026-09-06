# DeFeedback Live

Controls a DeFeedback Live Mac over the app's optional full-control LAN remote.

## Setup

1. In DeFeedback Live, enable **Enable full-control LAN remote**.
2. Click **COPY DETAILS** to get the Mac's address and access information.
3. Add this module in Companion, then enter the Mac's IP address, port, and access code.
4. If access-code protection is disabled in the Mac app, leave **Access code** blank.

Use a fixed IP address or DHCP reservation for the Mac. This connection uses unencrypted HTTP and is intended only for a trusted private production network. Do not expose the remote port to the internet.

## Controls

The module provides actions for:

- setting or stepping a lane's De-Feedback Strength (also called Sensitivity);
- toggling or explicitly setting lane bypass, plugin mute, and plugin on/off;
- toggling or explicitly setting the master output mute and audio engine;
- resetting XRuns and refreshing Core Audio devices.

Feedbacks and variables report the live state received from the Mac, including each lane's name, Strength, mute, bypass, enabled state, status, and meters.

Turning a lane off suspends that plugin and silences that lane's output while preserving its route and settings. Bypass keeps the lane running and passes dry audio. Master mute leaves processing active but zeros every physical output.

## Safety and independence

This is independent third-party software. It is not affiliated with, sponsored by, approved by, or endorsed by Alpha Labs LLC. De-Feedback is separately installed and licensed. Use this engineering preview at your own risk, verify every route, and retain an independent hardware or console mute.
