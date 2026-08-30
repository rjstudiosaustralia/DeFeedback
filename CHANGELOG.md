# Changelog

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
