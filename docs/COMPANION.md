# Bitfocus Companion control

DeFeedback Live 0.7 includes a Companion connection module for control surfaces, Stream Decks, button panels, rotary encoders, and Companion's web buttons.

## Install

1. Download `rjstudiosaustralia-defeedback-live-0.1.0.tgz` from the same GitHub release as the Mac app.
2. In Bitfocus Companion 3.4 or newer, open **Modules** and choose **Import module package**.
3. Import the `.tgz`, then add a **DeFeedback Live** connection.
4. Enter the Mac's IP address or hostname, LAN remote port (normally `8765`), and configured access code.
5. Leave the module's code blank only when the Mac app explicitly shows `NO ACCESS CODE`.

Use a DHCP reservation or static control-network address for a headless Mac. The module reconnects and re-authenticates automatically after a temporary network loss. If the access code changes, update the Companion connection configuration.

## Included actions

- set Strength/Sensitivity to an absolute percentage;
- step Strength/Sensitivity up or down for a rotary encoder or repeated button;
- set or toggle lane bypass;
- set or toggle the De-Feedback plugin's Mute parameter;
- set or toggle a lane plugin on/off;
- set or toggle the master all-output mute;
- set or toggle the complete audio engine;
- reset XRuns; and
- refresh Core Audio devices.

Ready-made presets are generated from the Mac's current lane IDs and names. The Strength preset maps rotary-left/right to one-percent steps. Actions also remain available for custom buttons, dials, and fader mappings.

## Live feedback and variables

Feedbacks cover engine running, master muted, and per-lane plugin enabled, muted, bypassed, and actively processing states. Variables cover app version, CPU, latency, XRuns, host message, lane count, and each lane's name, Strength, enabled/muted/bypassed state, input/output meter, and status.

Companion receives state through the same HTTP endpoint as the browser and treats the Mac as authoritative. Button feedback normally updates within the configured 250 ms poll interval; a command triggers an earlier refresh.

## Safety

The API uses unencrypted HTTP and has full live-audio control. Keep it on a trusted, isolated production LAN or control VLAN, never forward its port to the internet, and retain an independent console or hardware mute. Turning a lane plugin off silences that lane; bypass passes dry audio; master mute keeps processing active while zeroing physical outputs; stopping the engine stops all processing and meters.

The Companion adapter is MIT licensed for compatibility with the Companion ecosystem. The Mac host remains GNU AGPLv3. Both are independent third-party software and are not affiliated with or endorsed by Alpha Labs LLC.
