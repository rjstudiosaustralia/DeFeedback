# Measurement plan

A plug-in that changes a feedback loop must be evaluated in a repeatable closed-loop setup. Listening to isolated files is necessary but insufficient.

## 1. Latency

Measure and report all three values separately:

1. **Host-reported delay:** expected to remain 0 samples for the baseline and causal synthesis path.
2. **Algorithmic waveform latency:** impulse onset, cross-correlation peak, energy centroid, and frequency-dependent group delay. A zero delay field does not make these automatically zero.
3. **End-to-end round trip:** console/network/interface/driver/host/output measured electrically or acoustically. This belongs to each hardware configuration, not the plug-in alone.

Run at 44.1, 48, 88.2, and 96 kHz, with 16/32/64/128/256 sample buffers where the driver permits them.

## 2. Real-time deadline qualification

Test 1, 4, 7, and 10 mono channels on M1 Max, M4, and M4 Pro or better targets. Include RME Digiface Dante, Dante Virtual Soundcard, and RedNet TNX routes where available.

For every configuration collect:

- callback median, p99, p99.9, p99.99, and maximum duration;
- callback deadline and percentage of deadline consumed;
- host CPU and process energy impact;
- XRun count during a 30-minute load test and a two-hour soak;
- behavior while opening/closing editors, moving windows, saving state, changing routes, and disconnecting/reconnecting the device;
- thermal behavior after sustained operation.

A configuration passes only with zero XRuns and p99.99 processing below 70% of its callback deadline. Average CPU is not an acceptance criterion.

## 3. Gain before feedback

Create a closed-loop simulator and a physical test rig.

### Simulator

- direct vocal source plus microphone coloration;
- independently measured loudspeaker-to-microphone room impulse response;
- output gain ramped in 0.25 dB steps or at 0.25 dB/s;
- optional loudspeaker distortion and limiter behavior;
- stage/room noise at multiple SNRs;
- at least 30 distinct rooms/positions and 10 unseen voices.

Detect feedback onset from sustained spectral growth and time-domain loop divergence. Compare:

1. no processing;
2. conventional static ring-out EQ;
3. RingGuard deterministic baseline;
4. learned stage;
5. learned stage plus optional reference cancellation.

Report median, 10th percentile, and worst-case additional gain before feedback. Do not publish only the best microphone position.

### Physical rig

Use an isolated or safely level-limited environment, calibrated SPL measurement, output limiters, and a remote emergency mute. Repeat wedge, flown PA, fill, and close-loudspeaker geometries. Never use people as the first feedback detector.

## 4. Vocal and artifact quality

Evaluate direct-vocal preservation and unwanted-sound reduction separately:

- SI-SDR and scale-dependent waveform error against the dry direct target;
- intelligibility such as STOI where the licence permits;
- late-to-early energy ratio and estimated RT60 reduction;
- noise attenuation by type and SNR;
- spectral magnitude and phase deviation on clean close-miked voice;
- level/dynamic-range change;
- blind MUSHRA-style listening for coloration, chirps, watery speech, ringing, pumping, and consonant loss;
- hard negatives: sustained sung vowels, whistle tones, screams, choirs, laughter, instruments, and vocal effects.

## Milestone gates

### M0 — repository and baseline

- AU/VST3/standalone builds arm64.
- Core unit tests pass.
- Impulse starts at sample zero.
- No render-thread allocation or locks in application code.

### M1 — deterministic feedback baseline

- Repeatable additional gain before feedback versus unprocessed signal.
- No false full-notch-pool behavior on the noise and speech corpus.
- Ten-channel 64-sample qualification on at least one target Mac.

### M2 — causal learned enhancement

- Mean algorithmic latency target below 0.5 ms; hard ceiling below 1.5 ms.
- Better median gain before feedback and dereverberation than M1.
- No regression on clean-vocal blind preference.
- Identity fallback on model failure without discontinuity.

### M3 — field candidate

- Two-hour soak at the intended show configuration with zero XRuns.
- Device-loss, state-recall, bypass, and emergency-mute tests pass.
- Independent blind recordings and physical-loop results reviewed before any safety or parity claim.
