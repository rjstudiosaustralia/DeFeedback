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

Turning a lane off suspends that plugin and passes dry audio while preserving its route and settings. Bypass keeps the lane running and passes dry audio. Master mute leaves processing active but zeros every physical output.

## Safety and independence

This is independent third-party software. It is not affiliated with, sponsored by, approved by, or endorsed by Alpha Labs LLC. De-Feedback is separately installed and licensed. Use this engineering preview at your own risk, verify every route, and retain an independent hardware or console mute.

## VoiceISO (host 0.9 / module 0.3)

Select VoiceISO through the mode action. The dedicated **VoiceISO: Set Strength (0–120%)** action uses the plugin's visible scale; `lane_N_voiceiso_strength` reports it. Existing generic Strength set/step actions and `lane_N_strength` keep their normalized percentage semantics: 100% of range equals VoiceISO Strength 120%. Full/Lite selection uses the lane's native UI on the Mac. OFF and BYPASS both pass dry audio in every mode; master mute still silences outputs.

## Mixed racks (host 0.10 / module 0.4)

Use **Settings: Select lane plugin** to choose the plugin for one lane, or **Settings: Apply plugin to all lanes** for the whole rack. Both stop audio and mute outputs. Names and patches stay in place. Commands and VoiceISO/Clear Voice controls follow the target lane's plugin, including mixed racks. `lane_N_plugin_mode` reports each lane's selection; the global `plugin_mode` variable reports `mixed` when selections differ.
