# Multicore preview — 0.11.0

Select **Settings → Plugin processing → Multicore (automatic)** to process independent lanes concurrently. **Serial (one audio thread)** provides a comparison and fallback. Changing the setting stops the engine and arms master mute; restart and unmute explicitly. Existing settings without the new preference remain serial. Fresh configurations default to multicore.

The separate `DeFeedback Live Multicore.app` uses bundle ID `au.com.rjstudiosaustralia.defeedback-live.multicore`, its own single-instance identity, and its own settings at `~/Library/Application Support/RJ Studios Australia/DeFeedback Live Multicore/settings.xml`. Its defaults disable auto-start and launch at login. The optional remote defaults to port 8767 and remains disabled until selected. This preview does not import or overwrite the released app's settings.

## Scheduling and audio behavior

- Each lane owns a mono JUCE graph and its plugin instance. No instance is called concurrently with itself.
- Fixed real-time workers are created during preparation. The device callback also processes lanes, claiming work from the same atomic queue. No application-side worker creation or buffer allocation occurs during rendering.
- The automatic thread limit is the lowest of lane count, Apple Silicon performance-core count, and the device audio workgroup recommendation when present. A single lane uses the callback alone. Failed worker startup reduces the count, down to serial if necessary.
- macOS workers use preallocated Mach semaphores for dispatch and completion. Release/acquire atomics publish the input, workgroup and playhead snapshot before processing, then publish every output before the callback reads it. Workers retain their workgroup and leave it on their own thread during teardown or replacement.
- Every worker completes the current block before output is copied. There is no added block of buffering. The tradeoff is that one stalled plugin can still hold up the entire callback; this is not crash or hang isolation.
- Each mono graph aligns its dry and processed paths. The rack then aligns every lane to the longest lane latency, and master mute gates the final result after those delays. All inputs are copied before outputs are written, preserving crossed patches.
- A plugin latency notification latches silence immediately. The UI's regular status update arms master mute and schedules a rebuild while callbacks are detached. A manual unmute is required afterwards.
- Settings changes, worker teardown, graph construction, plugin serialization and editors stay off the audio workers. Existing ON/OFF transitions use the rack callback lock across the whole dispatch/completion cycle.

Apple's [audio workgroup guidance](https://developer.apple.com/documentation/audiotoolbox/understanding-audio-workgroups) describes why auxiliary real-time threads should join the device workgroup. A device exposing a workgroup does not by itself prove successful scheduling under live load.

## Build and run separately

```sh
cmake -S . -B build-multicore -DCMAKE_BUILD_TYPE=Release \
  -DDEFEEDBACK_MULTICORE_PREVIEW=ON -DDEFEEDBACK_BUILD_PLUGIN_TESTS=ON
cmake --build build-multicore --parallel 4
ctest --test-dir build-multicore --output-on-failure
./scripts/package-adhoc.sh Release build-multicore "DeFeedback Live Multicore"
open "build-multicore/DeFeedbackLive_artefacts/Release/DeFeedback Live Multicore.app" --args --safe
```

The package script verifies arm64 and macOS 13 minimum compatibility and signs the completed bundle ad hoc. It does not notarize or publish the app.

## Validation on 2026-10-25

Test machine: Apple M1 Max, eight performance cores, 64 GiB RAM, macOS 26.6.2. Installed AUs: De-Feedback 1.1.4, Clear Voice Live 1.1.6, VoiceISO 1.0.6. No third-party plugin binaries or state are included in the repository.

Passed:

- Configuration round-trip and migration tests.
- Deterministic serial/parallel output comparisons at 32, 64, 128 and 512 samples, including short/oversized callbacks, crossed routes, unused output silence, varied plugin latency, master mute and zero-length blocks. Output was bit-identical and four lanes overlapped in execution.
- Worker creation/teardown, repeated serial/parallel and sample-rate changes, single-lane optimization, no concurrent invocation of an individual plugin, and changed-latency silence/reprepare checks.
- Real-AU mixed racks, parameter/state restoration, OFF/ON, bypass, dry-pass alignment and master mute. VoiceISO's noise-processing check also passed.
- Simulated native settings layout and authenticated remote processing-mode changes, including stop/mute and unchanged-selection behavior.

One existing integration failure remains: Clear Voice passes the test noise through unchanged (output/input energy ratio 1). The exact same check fails in the previously built 0.10.0 serial engine. Clear Voice instantiation, controls and dry routing work, but its active noise-processing performance could not be qualified in this environment. The cause has not been established; no licensing conclusion is inferred from this result.

## Performance method and limits

Run the optional benchmark with:

```sh
build-multicore/DeFeedbackPluginTests_artefacts/Release/DeFeedbackPluginTests --parallel-benchmark
```

It uses eight mono lanes at 48 kHz, through the production device-manager/player/rack path on a memory-only device. The callback and workers use real-time thread priority. For each buffer and rack, it measures serial, multicore, multicore, serial in that order, with 800 paced warm-up blocks and 1,200 measured blocks per run. It checks that master mute and latency invalidation are inactive. The two runs of each mode have equal sample counts.

These are callback elapsed times under synthetic input, not whole-process CPU consumption or actual interface XRuns. The memory device exposes no Core Audio workgroup, and measured blocks run back-to-back. Interface scheduling, workgroup behavior, thermal endurance and the live signal chain still require validation with the target device. Faster average callbacks do not guarantee that every block meets its deadline.

Clear Voice-only measurements and mixed racks containing it must be interpreted as including its observed pass-through state. Do not use them as an active Clear Voice capacity estimate. Multicore overhead may exceed the work saved in inexpensive or mostly bypassed racks; serial remains selectable.

## Recorded results

[Raw measurements](benchmarks/2026-10-25-m1-max-multicore.csv). Means combine both runs; P99 is the worse of the two run P99 values. Deadline counts sum both runs (2,400 blocks per mode). Serial and multicore use the same new lane graphs, with workers disabled/enabled respectively. These ratios compare scheduling modes, not the complete 0.10.0 and 0.11.0 applications.

| Eight-lane rack | Buffer | Serial mean (ms) | Multicore mean (ms) | Speedup | Serial / multicore P99 (ms) | Over deadline, serial / multicore |
|---|---:|---:|---:|---:|---:|---:|
| De-Feedback | 32 | 0.409 | 0.175 | 2.34x | 1.956 / 0.814 | 400 / 400 |
| De-Feedback | 64 | 0.801 | 0.319 | 2.51x | 2.063 / 0.836 | 800 / 0 |
| De-Feedback | 128 | 1.605 | 0.611 | 2.63x | 2.338 / 0.910 | 0 / 0 |
| VoiceISO | 32 | 0.124 | 0.038 | 3.26x | 0.149 / 0.051 | 0 / 0 |
| VoiceISO | 64 | 0.237 | 0.051 | 4.64x | 0.263 / 0.065 | 0 / 0 |
| VoiceISO | 128 | 0.472 | 0.077 | 6.12x | 0.496 / 0.090 | 0 / 0 |

The 32-sample deadline is 0.667 ms; eight De-Feedback lanes still exceeded it on some blocks in multicore mode. The 64-sample deadline is 1.333 ms and the 128-sample deadline is 2.667 ms. No measured multicore blocks exceeded those deadlines for De-Feedback or VoiceISO in this run. This does not establish long-duration or hardware reliability.
