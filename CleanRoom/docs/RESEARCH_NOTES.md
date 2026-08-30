# Research notes

## Publicly observable target behavior

Alpha Labs publicly describes De-Feedback V1 as a zero-added-latency vocal cleaner/isolator that improves gain before feedback while reducing room reverb and background noise. It is optimized for individual speech/singing channels, exposes strength and mute controls, operates from 44.1 to 96 kHz, and uses non-static nonlinear phase behavior. Alpha Sound says the model was developed from venue impulse-response-derived training data. These statements define a broad product category; they do not reveal the proprietary architecture.

Sources:

- Alpha Labs product FAQ: https://www.alphalabsaudio.com/defeedback/
- Alpha Sound technology overview: https://www.alphasound.tech/technology/

A clean-room implementation therefore needs more than conventional feedback notches. It needs a speech-aware causal estimator that separates direct vocal content from recirculated/late acoustic energy while preserving level and timbre. The current notch processor exists as a safety/reference baseline and an objective comparator.

## Low-latency model families reviewed

### Minimum-phase predicted FIR

Google's Deep FIR research predicts a computationally efficient minimum-phase FIR and performs sample-by-sample synthesis. The published online supplement reports mean algorithmic latency from 0.32 to 1.25 ms, including a 0.38 ms configuration, with a lightweight recurrent model. This is the strongest architectural starting point for our latency target because the synthesis filter can use the current sample rather than waiting for a long spectral frame.

- https://google-research.github.io/sound-separation/papers/deepfir/

### Causal waveform enhancement

Meta's real-time waveform-domain speech enhancement research demonstrates that a causal raw-waveform model can reduce nonstationary noise and room reverb on a laptop CPU. It is useful as a quality reference, but the first RingGuard model should be materially smaller and explicitly optimized for tiny live-audio deadlines.

- https://ai.meta.com/research/publications/real-time-speech-enhancement-in-the-waveform-domain/

### Longer-frame spectral systems

Many strong speech-enhancement systems use 10–20 ms frames and additional lookahead. They are useful training and quality references but are unsuitable for a stage-monitor path unless redesigned. RingGuard will not hide a long lookahead behind a zero host-latency declaration.

## Apple deployment finding

Apple's BNNS Graph guidance is unusually relevant: it documents whole-graph CPU optimization, single-thread execution, direct pointer arguments, preallocated workspace, and no runtime allocation for Audio Unit use. Apple explicitly presents vocal isolation/removal as an Audio Unit ML use case.

- https://developer.apple.com/videos/play/wwdc2024/10211/

This makes BNNS Graph CPU the first production backend. Core ML remains useful for model conversion, performance reports, and experimental CPU/GPU/Neural Engine dispatch, but any non-CPU backend must earn its place through p99.99 callback measurements and fault tests.

## Open technical questions

- Can a blind one-microphone model distinguish a pure sustained singer harmonic from early feedback without momentary vocal loss?
- Is a minimum-phase FIR alone expressive enough for dereverberation plus feedback suppression, or is a small residual waveform branch required?
- Can one compact model cover speech and singing across handheld dynamics, headset condensers, lavaliers, wedges, line arrays, and IEM spill?
- What model/state size allows ten channels at a 32- or 64-sample Core Audio buffer on M1 Max and M4-class hardware?
- Does a separate reference-sidechain mode provide enough extra gain-before-feedback to justify routing complexity?

These questions are measurement tasks, not assumptions to encode in the product description.
