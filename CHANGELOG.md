# Changelog

## 0.4.0 — 2026-09-01

- Removed the fixed ten-lane application ceiling.
- Made lane capacity follow the selected Core Audio device's available input/output pairs.
- Enabled every input and output channel reported by the selected Core Audio driver after device open.
- Added clear feedback when every device channel pair is assigned.
- Restored every saved lane instead of truncating settings at ten.
- Added the current lane count and device-pair capacity to the signal-flow summary.
- Rebuilt lane choices when Core Audio devices are refreshed.
- Made package filenames follow the built app version automatically.
- Prepared the repository for public release under GNU AGPLv3.
- Added explicit independence, warranty, and live-audio risk notices.
- Added an in-app `ABOUT / SAFETY` notice for testers.
- Embedded the licence, disclaimer, and corresponding-source location in release packages.
- Updated GitHub Actions to Node 24-based releases with read-only repository permissions.

## 0.3.0 — 2026-08-30

- Renamed the engine control to `START ENGINE` / `STOP ENGINE`.
- Renamed and clarified the independent master output safety mute.
- Added green processing, yellow bypass, red mute/error, and neutral stopped lane treatments.
- Added explicit per-lane state text for engine stop, plugin mute, master mute, and bypass.
- Rebuilt the lane headings and controls on one shared layout geometry.
- Widened the Strength fader and increased the minimum live window width.
- Added an isolated no-audio UI preview build for safe visual QA beside a running Release instance.

## 0.2.0 — 2026-08-30

- Restore the main window and each open De-Feedback editor to its saved layout.
- Added an explicit Core Audio device refresh control.
- Added separate raw-input and post-output meters to every lane.
- Renamed operator dry pass to `BYPASS` in the lane flow.
- Added a resettable session XRun counter.
- Added inline De-Feedback Strength and plugin Mute controls.
- Prevented duplicate input and output assignments in both the UI and engine.

## 0.1.0 — 2026-08-30

- Added an arm64-only native macOS Audio Unit host.
- Added targeted De-Feedback 1.1.4 discovery and stale Core Audio cache recovery.
- Added up to ten mono processing lanes with arbitrary input/output mapping.
- Added separate hosted plugin editor windows.
- Added device, sample-rate, and buffer controls.
- Added atomic post-plugin global mute for silent load testing.
- Added explicit dry-pass warnings and invalid-route detection.
- Added CPU, latency, peak, and XRun telemetry.
- Added atomic setup/plugin-state persistence, auto-start, and launch-at-login.
- Added safe debug launch, tests, CI, and hardware qualification documentation.
