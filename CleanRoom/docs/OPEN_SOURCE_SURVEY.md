# Open-source model survey

The immediate goal is not simply “find a denoiser.” A useful live-stage substitute must improve direct-vocal-to-room/PA energy, preserve singing, and stay inside a monitor-path latency budget. That eliminates several otherwise good speech-enhancement projects.

## DeepFilterNet

DeepFilterNet is the strongest permissively licensed off-the-shelf quality baseline found. Its code is dual MIT/Apache-2.0 and it runs at 48 kHz, but the project's own LADSPA documentation reports a **20 ms minimum latency** even for its no-lookahead model because of STFT processing. Independent plug-in implementations commonly report 30–40 ms once host buffering is included. That is appropriate for calls, streaming, or post work, not the primary live wedge/IEM path targeted here.

- https://github.com/Rikorose/DeepFilterNet
- https://github.com/Rikorose/DeepFilterNet/blob/main/ladspa/README.md

Use it as an offline perceptual comparator and possible teacher for research only where licences allow; do not put its delayed signal in the zero-delay production path.

## Meta/Facebook causal Demucs denoiser

The published waveform model removes noise and room reverb, but its streaming implementation uses 40 ms input frames with a 16 ms stride and reports about 41 ms total lag. The repository is archived and licensed CC-BY-NC 4.0, which is unsuitable as the code/model foundation for a commercial plug-in without separate permission.

- https://github.com/facebookresearch/denoiser
- https://facebookresearch.github.io/denoiser/

Its augmentation and loss design remain useful research references.

## RNNoise and RNNoise plug-ins

RNNoise is compact and established for speech-noise suppression, but standard integrations process 480-sample frames at 48 kHz. It is not designed to isolate direct vocal energy from late venue reflections or loudspeaker recirculation. Existing full plug-in projects may also impose GPL obligations that are incompatible with a closed product. RNNoise remains useful as a CPU and artifact baseline, not as the target architecture.

- https://github.com/xiph/rnnoise
- https://github.com/werman/noise-suppression-for-voice

## Deep FIR minimum-phase synthesis

Google's Deep FIR research is the closest published match to the latency requirement: a small stateful model predicts a short minimum-phase FIR and the waveform is synthesized sample by sample. Reported mean algorithmic latency spans 0.32–1.25 ms, including a 0.38 ms configuration, with a 644k-parameter LSTM.

- https://google-research.github.io/sound-separation/papers/deepfir/

The publication is an architectural reference, not a drop-in model release. RingGuard therefore needs independently trained weights and venue/PA-specific data.

## Deployment conclusion

1. Keep the deterministic zero-delay RingGuard core as the always-valid safety/fallback path.
2. Train an original Deep-FIR-style causal model at 48 kHz, with direct vocal as target and independently generated room/feedback/noise mixtures as input.
3. Deploy the first model through single-threaded BNNS Graph CPU with preallocated workspace.
4. Benchmark an equivalent Core ML stateful model on CPU/Neural Engine, but promote it only if p99.99 callback completion is better—not merely average throughput.
5. Retain DeepFilterNet and causal Demucs as offline quality references, not runtime dependencies.
