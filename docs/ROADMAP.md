# Roadmap

## Host

### 0.2 — hardware qualification

- Test DVS, RME Digiface, and RedNet TNX.
- Add a structured soak-test recorder and CSV export.
- Add an optional startup XRun baseline indicator.
- Verify ten lanes on M4, M4 Pro, and later Apple Silicon targets.

### 0.3 — live hardening

- Detect route/device identity changes by stable Core Audio UID instead of display name only.
- Add explicit device-loss and recovered states.
- Add a configurable startup delay for Dante/USB interfaces.
- Sign and notarize release builds.
- Add a one-click diagnostic bundle with no licence data.

### 0.4 — performance engine

- Prototype fixed per-lane worker threads.
- Join workers to the Core Audio device workgroup.
- Compare serial and parallel engines at 32/64/128 samples.
- Keep the serial engine when it is measurably more reliable.

## Clean-room feedback processor

This is a separate research project and must not reverse engineer or derive from Alpha Labs software.

1. Define objective feedback-onset, speech-quality, and latency metrics.
2. Build a traditional adaptive feedback detector and narrow-notch reference.
3. Assemble independently recorded microphone/loudspeaker/room data with explicit rights.
4. Explore causal speech-aware suppression models suitable for Core ML or Accelerate.
5. Validate artifacts, gain-before-feedback, CPU, and failure behavior against blind recordings.
6. Ship under a distinct name and brand only if it is independently useful.

No Alpha Labs binary, model, data, parameter behavior, or private implementation detail should be used as development input.
