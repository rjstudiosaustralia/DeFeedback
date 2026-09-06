# DeFeedback Live

DeFeedback Live is a focused, Apple Silicon-native macOS host for running multiple independent mono instances of Alpha Labs De-Feedback in live sound systems.

It is deliberately not a general-purpose DAW. The operator selects one Core Audio device, chooses the sample rate and buffer, maps independent mono input/output lanes up to the device's available I/O capacity, and opens each De-Feedback editor in its own window.

> [!IMPORTANT]
> This project is independent and is not affiliated with or endorsed by Alpha Labs LLC. De-Feedback must be downloaded, installed, activated, and licensed separately. No Alpha Labs binary or licence data belongs in this repository.

> [!CAUTION]
> This is an engineering preview, not a show-qualified safety system. Use it entirely at your own risk. Live feedback and routing mistakes can produce sudden high sound levels, hearing damage, or equipment damage. Begin every test with the master outputs muted and validate the complete interface, console, network, and loudspeaker signal path before passing live audio. See the [full disclaimer](DISCLAIMER.md).

## Download

### [Download DeFeedback Live 0.7.0 for Apple Silicon →](https://github.com/rjstudiosaustralia/DeFeedback/releases/download/v0.7.0/DeFeedback-Live-0.7.0-adhoc-arm64.zip)

### [Download the Bitfocus Companion module →](https://github.com/rjstudiosaustralia/DeFeedback/releases/download/v0.7.0/rjstudiosaustralia-defeedback-live-0.1.0.tgz)

Or visit the [Latest release page](https://github.com/rjstudiosaustralia/DeFeedback/releases/latest) for release notes and the SHA-256 checksum.

1. Download and unzip `DeFeedback-Live-0.7.0-adhoc-arm64.zip`.
2. Move `DeFeedback Live.app` to the Applications folder.
3. Install and activate the Alpha Labs De-Feedback Audio Unit separately.
4. Open DeFeedback Live and begin with `MUTE ALL OUTPUTS` engaged.

The current preview is ad-hoc signed but not Apple-notarized. If macOS blocks the first launch, try opening the app once, then go to **System Settings → Privacy & Security** and use **Open Anyway** only if you downloaded it from this official release. See [Apple's safety guidance](https://support.apple.com/en-au/102445).

Binary compatibility: Apple Silicon and macOS 13 Ventura or newer. Intel Macs are not supported. macOS 13–15 remain build-compatible but should be hardware-qualified with the exact De-Feedback version and audio-interface driver before live use.

## Current status

Version `0.7.0` is a working engineering preview:

- arm64-only macOS application;
- AUv2 hosting for De-Feedback 1.1.4 (`aufx/FbTI/jDSP`);
- one Core Audio input/output device at a time;
- independent mono lanes with no fixed application cap;
- lane capacity determined by the selected device's unassigned input/output pairs;
- arbitrary input-to-output routing;
- exclusive routing that prevents an input or output channel being used by two lanes;
- separate plugin editor windows;
- restored main-window and open plugin-editor layouts;
- inline De-Feedback Strength and plugin Mute controls;
- a per-lane plugin on/off control that suspends unused De-Feedback DSP, silences that lane, and preserves its route and settings;
- separate raw-input and post-gate output meters;
- aligned signal-flow columns with a wider Strength fader;
- lane state colors: green processing, yellow bypass, red mute/error, gray stopped/inactive;
- responsive wide and compact layouts for landscape or portrait displays;
- 48 kHz default and device-supported buffer selection;
- manual Core Audio device refresh for interfaces connected after launch;
- CPU, device latency, peak level, and XRun display;
- resettable session XRun count;
- per-lane operator-selected dry pass;
- automatic dry pass when a plugin instance cannot load;
- a real-time global output mute after processing;
- atomic setup and plugin-state persistence;
- optional auto-start and native macOS launch-at-login;
- automatic macOS idle-system-sleep and App Nap prevention while the app is open;
- an optional responsive LAN browser remote, disabled by default;
- authenticated full control of devices, rate, buffer, engine, output mute, lanes, routing, per-lane plugin state, Strength, plugin Mute, bypass, meters, and XRuns;
- immediate browser feedback and authoritative live reconciliation for plugin Mute and bypass;
- operator-selected access-code protection, with custom and random codes or an explicitly warned no-code mode;
- remembered browsers using an HttpOnly one-year cookie that is revoked by logout or any access-setting change;
- a downloadable Bitfocus Companion module with ready-made master/lane buttons, rotary Strength control, feedbacks, and variables; and
- duplicate-route prevention in both native and browser controls, with server-side validation.

The app has been compiled and exercised on an M1 Max with the installed De-Feedback 1.1.4 demo. Multi-lane reliability and the practical CPU ceiling still need to be qualified on the target DVS, RME Digiface, and Focusrite RedNet TNX systems. Removing the software cap does not imply that every lane count is real-time safe. See [Hardware validation](docs/HARDWARE_VALIDATION.md).

## Safety behavior

The requested failure policy is dry pass. Therefore, a lane routes input directly to output when:

- the operator enables `BYPASS`;
- De-Feedback is missing or cannot instantiate; or
- the selected route cannot host the plugin.

The rack displays an amber full-width warning whenever dry pass is active because feedback protection is then off.

`STOP ENGINE` removes the Core Audio callback: every plugin and meter stops processing, CPU load drops, and outputs are silent. `MUTE ALL OUTPUTS` leaves Core Audio and every plugin running but zeros samples at the final lane gates. Input meters and processing continue while the post-gate output meters fall to zero, allowing a safe CPU/XRun load test before a show.

The per-lane `PLUGIN` switch is an activity and CPU control. Turning it `OFF` suspends that De-Feedback instance so its processing callback is not run, hard-gates that lane's output to silence, and retains the route, Strength, plugin settings, and editor layout for later re-enabling. It is intentionally not a dry bypass.

The per-lane `PLUGIN MUTE` checkbox is De-Feedback's own parameter and is saved inside that AU instance. `BYPASS` skips the plugin and passes dry audio. The master output mute intentionally does not alter those plugin checkboxes; instead every active lane turns red and reports `OUTPUT MUTED`, preserving each plugin's state for immediate unmute.

A hard in-process AUv2 crash can still terminate the host before it can choose a fallback. True crash isolation requires a different plugin format or a multi-process bridge and is not claimed in this release.

## LAN remote and headless use

Use `ACCESS` to choose a custom code, generate a random code, or explicitly disable code protection. Then enable `Enable full-control LAN remote` and use `COPY DETAILS` to copy the displayed LAN address and access information. The setting and code persist across restarts so a Mac configured with `Launch at login` can be controlled without a connected monitor after its user session has logged in. A successfully authenticated browser is remembered for one year; `LOG OUT` or any access-code/protection change revokes it. The server starts even when the saved audio device is unavailable, allowing the browser to refresh and select a newly connected interface.

The browser mirrors the operational host controls: Core Audio device refresh/selection, sample rate, buffer, engine start/stop, master mute/unmute, auto-start, launch-at-login, lane add/remove/name/routing, per-lane plugin on/off, Strength, plugin Mute, bypass, meters, CPU/latency/XRuns, and XRun reset. Native Audio Unit editor windows cannot be embedded in a browser; the exposed lane-strip controls remain available remotely.

The remote is deliberately local and self-contained: the app serves its own page and does not require internet or a cloud account. It listens on TCP port `8765` on the Mac's active network interfaces only while enabled. Code protection is on by default, and the settings file containing the code and remembered-browser secret is restricted to the current macOS user. No-code mode is available for isolated control networks but gives every device that can reach the port full control.

> [!WARNING]
> The current preview uses ordinary HTTP, not encrypted HTTPS. Use it only on a trusted private production network or isolated control VLAN. Do not expose port `8765` to the internet, forward it through a router, or use the remote across public/shared Wi-Fi. Anyone who obtains the access code or a remembered browser has full live-audio control, including engine start and output unmute. With code protection off, every reachable LAN client has that control. See [Remote control](docs/REMOTE_CONTROL.md) for setup, Companion, recovery, and validation details.

## Bitfocus Companion

The release includes an importable Companion `.tgz`. In Companion 3.4 or newer, use **Modules → Import module package**, add the **DeFeedback Live** connection, and enter the Mac address, port, and access code. Leave the code blank only if protection is disabled in the Mac app.

The module includes ready-made master mute, engine, XRun, and per-lane plugin/mute/bypass/Strength presets. Its Strength preset responds to rotary left/right, and equivalent set/step actions can be assigned to any supported dial, fader, or button. Live feedbacks and variables report authoritative engine, master, lane, Strength, meter, CPU, latency, and XRun state. See [Companion setup](docs/COMPANION.md).

`Launch at login` is a normal macOS login item, not a system daemon. After a reboot, a user must complete macOS/FileVault login before the app and remote can start. While open, DeFeedback Live asks macOS to block idle system sleep and App Nap; the display may still sleep, and closing the lid or explicitly choosing Sleep can still interrupt audio. For a monitor-free machine, reserve its IP address in DHCP or assign a stable control-network address and retain Screen Sharing or physical access as a recovery path.

## Runtime requirements

- Apple Silicon Mac;
- macOS 13 or newer;
- De-Feedback AU installed at `/Library/Audio/Plug-Ins/Components/Defeedback.component`;
- an Alpha Labs demo or paid activation.

## Source-build requirements

- Xcode command-line tools;
- CMake 3.25 or newer;
- a JUCE licence appropriate for the builder's use and revenue/funding tier.

Building the optional Companion module additionally requires Node.js 22 and pnpm 10 or newer.

JUCE 9 is a pinned Git submodule and is licensed separately by Raw Material Software. The current JUCE Starter tier is free up to its published revenue/funding threshold; review the current JUCE terms before distributing or selling the app.

## Build

```bash
git clone --recurse-submodules https://github.com/rjstudiosaustralia/DeFeedback.git
cd DeFeedback
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The debug app is written to:

```text
build/DeFeedbackLive_artefacts/Debug/DeFeedback Live.app
```

Build and validate the Companion module with:

```bash
cd companion-module
pnpm install --frozen-lockfile
pnpm run lint
pnpm run package
```

Debug builds always use safe launch: they restore the setup but do not auto-start audio or register themselves as login items. Release builds follow the saved auto-start settings. Any build can be launched safely with `--safe`.

Convenience scripts are provided:

```bash
./scripts/build.sh Debug
./scripts/run-safe.sh Debug
./scripts/build.sh Release
./scripts/package-adhoc.sh Release
```

The package script creates an ad-hoc-signed Apple Silicon engineering build in
`dist/`. Public preview downloads are not notarized, so macOS may require the
tester to approve the app in Privacy & Security. A normal one-click public
distribution still requires an Apple Developer ID signature and notarization.

## First live setup

1. Install and activate the current De-Feedback AU.
2. Connect and power the interface before launching the app.
3. Select the same Core Audio device for input and output.
4. Start at 48 kHz and 128 samples.
5. Add lanes and confirm each route says `PROCESSED`.
6. Press `MUTE ALL OUTPUTS`.
7. Start audio and watch CPU/XRuns for several minutes.
8. Reduce the buffer one step at a time only after the configuration passes.
9. Unmute outputs only after the console/interface routing has been checked for a parallel dry path.

The latency number is the device-reported Core Audio input-plus-output latency. DVS and some interface drivers may include additional network, firmware, or console latency that the host cannot observe.

## Repository policy

- The repository is public and the Mac app is open source under GNU AGPLv3. The Companion adapter is MIT licensed as required for portable Companion modules.
- Never add `.component`, `.vst3`, activation, notarization, or signing files.
- Pin JUCE updates and qualify them before changing the live build.
- Hardware test results should be committed to `docs/HARDWARE_VALIDATION.md`.

## Licence

Copyright (c) 2026 Ryan Somerfield / RJ Studios Australia.

DeFeedback Live is free and open-source software licensed under the [GNU Affero General Public License version 3](LICENSE). This matches JUCE's AGPLv3 open-source licensing path. Alternative commercial licensing may be offered separately by RJ Studios Australia for code it owns.

JUCE and its bundled dependencies retain their own licences. Alpha Labs De-Feedback is separately installed and licensed and is not included here.

## Roadmap

The host and the proposed clean-room feedback processor are intentionally separate efforts. See [Roadmap](docs/ROADMAP.md) and [Architecture](docs/ARCHITECTURE.md).
