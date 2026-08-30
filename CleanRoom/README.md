# RingGuard Prototype

RingGuard Prototype is a clean-room, Apple-silicon-first research plug-in for live vocal feedback reduction. It is intentionally developed separately from Alpha Labs De-Feedback and contains no Alpha Labs code, model weights, assets, extracted behavior tables, licence material, or reverse-engineered implementation details.

The current milestone is a **working deterministic baseline**, not an AI-equivalent claim.

## What builds now

- Native arm64 AU, VST3, and standalone targets through JUCE.
- Framework-independent C++20 `RingGuardCore` library.
- Mono or linked-stereo processing.
- A 48-band streaming resonator detector and up to eight smoothly controlled notch filters.
- Zero samples of reported plug-in latency and no lookahead in the baseline signal path.
- No allocation, locking, logging, model compilation, file access, or UI work in `process()`.
- Dry bypass, hard mute, strength, sensitivity, and maximum-notch parameters.
- Unit tests and a non-gating benchmark executable.

## What is not implemented yet

- The causal learned vocal-isolation/dereverberation/noise-removal stage.
- A reference-assisted acoustic echo/feedback canceller sidechain.
- A production user interface, presets, signing, notarization, or installer.
- A claim of commercial-grade feedback protection or parity with any other product.

The deterministic baseline may notch a sustained vocal harmonic and should be treated as an engineering prototype. Do not rely on it as the only protection for loudspeakers, hearing, performers, or audiences.

## Build

From the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Useful explicit targets:

```bash
cmake --build build --target RingGuardPrototype_AU
cmake --build build --target RingGuardPrototype_VST3
cmake --build build --target RingGuardPrototype_Standalone
cmake --build build --target RingGuardCoreTests
cmake --build build --target RingGuardBenchmark
```

The host-reported plug-in delay is zero samples. End-to-end latency still includes the console, network, interface, Core Audio driver, and host buffers.

## Research direction

The proposed learned stage predicts a short causal minimum-phase FIR rather than delaying the waveform for a conventional long STFT mask. The first production candidate will use Apple's BNNS Graph CPU backend because Apple documents controls for single-threaded execution and preallocated workspace in real-time Audio Units. Core ML/Neural Engine and GPU paths remain benchmark experiments until they prove bounded callback behavior on the target Macs.

See:

- [Architecture](docs/ARCHITECTURE.md)
- [Research notes](docs/RESEARCH_NOTES.md)
- [Measurement plan](docs/MEASUREMENT_PLAN.md)
- [Dataset and training policy](docs/DATASET_AND_TRAINING.md)
- [Apple silicon backends](docs/APPLE_SILICON_BACKENDS.md)
