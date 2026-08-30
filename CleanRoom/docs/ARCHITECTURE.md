# RingGuard architecture

## Product boundary

RingGuard is an original feedback-risk reduction and live-vocal enhancement project. Comparable user outcomes may be measured against public market claims, but Alpha Labs binaries, model outputs, parameter curves, UI assets, hidden data, and confidential implementation details are excluded from development and training.

## Current real-time graph

```text
input sample N
    │
    ├── sanitise NaN/Inf
    │
    ├── analysis tap ─ 48 causal resonators ─ tone/persistence scorer
    │                                      └─ fixed notch assignment control
    │
    └── 0–8 linked TPT state-variable notches ─ output sample N
```

Analysis updates every 4 ms, but the audio is never held for analysis. The detector therefore has a control-response time while the signal path itself has no lookahead or block delay. Notches fade in quickly and release slowly. An active recursive filter is faded out before retuning so coefficient changes cannot create an unstable transition.

The stereo layout derives one control signal and applies identical notch frequencies and depths with separate per-channel filter state. This avoids independent left/right phase movement.

## Render-thread contract

`RealtimeProcessor::process()`:

- uses fixed-size `std::array` state only;
- does not allocate or free memory;
- does not take a lock;
- does not launch work or wait for another thread;
- does not log, touch the file system, or call the UI;
- sanitises non-finite input and state;
- produces the current output sample from the current and past input only.

The JUCE wrapper reports zero samples through `setLatencySamples(0)` and uses an unchanged dry path when bypassed.

## Learned-stage target

The Alpha-like part of the research is not another automatic EQ. It is a causal residual estimator intended to preserve direct vocal energy while reducing late room energy, loudspeaker recirculation, and unrelated background noise.

The preferred first topology is:

```text
causal feature history + recurrent state
              │
       small stateful network
              │
    short minimum-phase FIR taps
              │
current + past waveform samples only ─ sample-by-sample FIR synthesis
```

A short minimum-phase filter can start with a non-zero tap at sample N, so it need not impose a fixed frame delay. Its perceptual/energy latency still must be measured rather than inferred from the host delay field.

### Backend interface

The model integration will expose three operations:

1. `prepare(maximumFrames)`: compile the immutable graph and preallocate page-aligned workspace off the render thread.
2. `resetState()`: clear recurrent state at transport/device resets.
3. `predict(input, output, state)`: execute with direct pointers and no allocation.

Failure or deadline protection is identity processing: the deterministic core continues and the learned residual stage becomes a no-op. No stale output buffer is ever substituted for the current live input.

## Apple-silicon execution

The production candidate is BNNS Graph on CPU, single-threaded per context, with graph compilation and workspace allocation completed before audio starts. The immutable model can be shared, but every simultaneously executing plug-in instance needs independent mutable context and recurrent state.

Core ML with CPU/GPU/Neural Engine remains a measurement branch. It may be faster on average, but average throughput is not sufficient for a live render callback; deadline variance and fallback behavior must be qualified.

## Ten-channel strategy

Ten unrelated plug-in instances cannot safely coordinate a batched inference by blocking inside separate callbacks. The staged plan is:

1. Keep each AU/VST3 instance independent and deterministic.
2. Share read-only model weights where the backend permits it, never mutable contexts.
3. Benchmark 1, 4, 7, and 10 instances at 16/32/64/128 samples.
4. Add a direct multi-lane processor to the existing DeFeedback Live host if separate instances cannot meet the target. That host can own scheduling, use fixed workers joined to the Core Audio workgroup, and fail the entire neural layer to identity without cross-plug-in locks.

## Future reference mode

An optional sidechain containing the PA/monitor send would enable acoustic echo/feedback cancellation. It is likely to provide stronger loop suppression than blind microphone-only processing, but requires delay alignment, nonlinear loudspeaker robustness, and double-talk protection so the direct vocalist is not adapted away. It is a separate milestone, not hidden inside the first model.
