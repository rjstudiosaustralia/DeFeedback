# DeFeedback Live

DeFeedback Live is a focused, Apple Silicon-native macOS host for running multiple independent mono instances of Alpha Labs De-Feedback in live sound systems.

It is deliberately not a general-purpose DAW. The operator selects one Core Audio device, chooses the sample rate and buffer, maps up to ten mono input/output lanes, and opens each De-Feedback editor in its own window.

> [!IMPORTANT]
> This project is independent and is not affiliated with or endorsed by Alpha Labs LLC. De-Feedback must be downloaded, installed, activated, and licensed separately. No Alpha Labs binary or licence data belongs in this repository.

## Current status

Version `0.3.0` is a working engineering preview:

- arm64-only macOS application;
- AUv2 hosting for De-Feedback 1.1.4 (`aufx/FbTI/jDSP`);
- one Core Audio input/output pair at a time;
- up to ten independent mono lanes;
- arbitrary input-to-output routing;
- exclusive routing that prevents an input or output channel being used by two lanes;
- separate plugin editor windows;
- restored main-window and open plugin-editor layouts;
- inline De-Feedback Strength and plugin Mute controls;
- separate raw-input and post-gate output meters;
- aligned signal-flow columns with a wider Strength fader;
- lane state colors: green processing, yellow bypass, red mute/error, gray stopped;
- 48 kHz default and device-supported buffer selection;
- manual Core Audio device refresh for interfaces connected after launch;
- CPU, device latency, peak level, and XRun display;
- resettable session XRun count;
- per-lane operator-selected dry pass;
- automatic dry pass when a plugin instance cannot load;
- a real-time global output mute after processing;
- atomic setup and plugin-state persistence;
- optional auto-start and native macOS launch-at-login.

The app has been compiled and exercised on an M1 Max with the installed De-Feedback 1.1.4 demo. Ten-lane reliability still needs to be qualified on the target DVS, RME Digiface, and Focusrite RedNet TNX systems. See [Hardware validation](docs/HARDWARE_VALIDATION.md).

## Safety behavior

The requested failure policy is dry pass. Therefore, a lane routes input directly to output when:

- the operator enables `BYPASS`;
- De-Feedback is missing or cannot instantiate; or
- the selected route cannot host the plugin.

The rack displays an amber full-width warning whenever dry pass is active because feedback protection is then off.

`STOP ENGINE` removes the Core Audio callback: every plugin and meter stops processing, CPU load drops, and outputs are silent. `MUTE ALL OUTPUTS` leaves Core Audio and every plugin running but zeros samples at the final lane gates. Input meters and processing continue while the post-gate output meters fall to zero, allowing a safe CPU/XRun load test before a show.

The per-lane `PLUGIN MUTE` checkbox is De-Feedback's own parameter and is saved inside that AU instance. `BYPASS` skips the plugin and passes dry audio. The master output mute intentionally does not alter those plugin checkboxes; instead every lane turns red and reports `OUTPUT MUTED`, preserving each plugin's state for immediate unmute.

A hard in-process AUv2 crash can still terminate the host before it can choose a fallback. True crash isolation requires a different plugin format or a multi-process bridge and is not claimed in this release.

## Requirements

- Apple Silicon Mac;
- macOS 13 or newer;
- Xcode command-line tools;
- CMake 3.25 or newer;
- De-Feedback AU installed at `/Library/Audio/Plug-Ins/Components/Defeedback.component`;
- an Alpha Labs demo or paid activation;
- a JUCE licence appropriate for the product owner's revenue/funding tier.

JUCE 9 is a pinned Git submodule and is licensed separately by Raw Material Software. The current JUCE Starter tier is free up to its published revenue/funding threshold; review the current JUCE terms before distributing or selling the app.

## Build

```bash
git clone --recurse-submodules <private-repository-url>
cd DeFeedback
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The debug app is written to:

```text
build/DeFeedbackLive_artefacts/Debug/DeFeedback Live.app
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
`dist/`. It is suitable for private hardware qualification on this Mac. Public
distribution still requires an Apple Developer ID signature and notarization.

## First live setup

1. Install and activate the current De-Feedback AU.
2. Connect and power the interface before launching the app.
3. Select the same Core Audio device for input and output.
4. Start at 48 kHz and 128 samples.
5. Add lanes and confirm each route says `PROCESSED`.
6. Press `MUTE EVERYTHING`.
7. Start audio and watch CPU/XRuns for several minutes.
8. Reduce the buffer one step at a time only after the configuration passes.
9. Unmute outputs only after the console/interface routing has been checked for a parallel dry path.

The latency number is the device-reported Core Audio input-plus-output latency. DVS and some interface drivers may include additional network, firmware, or console latency that the host cannot observe.

## Repository policy

- Keep the repository private unless the owner deliberately chooses another licence.
- Never add `.component`, `.vst3`, activation, notarization, or signing files.
- Pin JUCE updates and qualify them before changing the live build.
- Hardware test results should be committed to `docs/HARDWARE_VALIDATION.md`.

## Roadmap

The host and the proposed clean-room feedback processor are intentionally separate efforts. See [Roadmap](docs/ROADMAP.md) and [Architecture](docs/ARCHITECTURE.md).
