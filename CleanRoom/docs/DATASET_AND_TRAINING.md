# Dataset and training policy

## Clean-room rules

Training and development data must be independently licensed or recorded. Do not:

- inspect or disassemble an Alpha Labs binary;
- extract model weights, resources, activation material, or private metadata;
- use Alpha Labs output as a teacher target for distillation;
- copy its UI, branding, presets, parameter response, or assets;
- include recordings whose performer, venue, or dataset rights do not cover model training and redistribution.

Commercial products may be used only as black-box listening/benchmark comparators after our hypotheses, metrics, and test material are fixed. Comparator output is not training data.

## Signal construction

Every training example should retain separate stems for:

- direct microphone vocal target;
- early room reflections;
- late reverberation;
- loudspeaker-to-microphone recirculation path;
- ambient/stage noise;
- optional PA/monitor reference;
- microphone and loudspeaker nonlinearities.

A simplified closed-loop generator is:

```text
microphone[n] = direct[n]
              + room(direct)[n]
              + noise[n]
              + loopPath(processedOutput)[n]
```

The target is the direct vocal with explicitly chosen microphone/direct-path coloration, not an unrealistically anechoic studio voice unless that is the declared product goal.

## Required diversity

- speech and singing across pitch, register, dynamics, language, accent, age, and vocal style;
- handheld dynamic, headset, lavalier, podium, and boundary microphone responses;
- wedges, sidefills, mains, frontfills, delay speakers, and subwoofer leakage;
- indoor/outdoor impulse responses with multiple microphone and loudspeaker distances;
- stationary and nonstationary stage noise;
- gain trajectories from stable through incipient ring to divergent feedback;
- hard negatives including pure sung tones, whistles, instruments, vocal effects, and multiple simultaneous voices.

Split by speaker, room, microphone, loudspeaker, and impulse-response capture session so near-duplicates cannot leak into validation.

## Model target

The first trainable model should be compact and stateful, predicting a short minimum-phase FIR at a small hop. Candidate limits:

- 16–64 synthesis taps;
- 6–24 new input samples per prediction hop;
- recurrent/temporal state rather than future context;
- float32 baseline, with float16 or quantization introduced only after artifact and overflow tests;
- an identity filter that is always valid when confidence is low.

Losses should combine waveform reconstruction, multi-resolution spectral error, direct-vocal preservation, late-energy suppression, and closed-loop stability. A loss that only improves denoising scores can still make feedback behavior worse.

## Data governance

Every source needs a manifest containing owner, licence, performer/venue release, allowed uses, redistribution status, date obtained, and checksum. Model releases must preserve the manifest and document any source that prevents weight redistribution.
