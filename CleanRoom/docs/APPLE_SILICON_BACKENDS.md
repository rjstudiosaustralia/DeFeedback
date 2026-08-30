# Apple silicon backend plan

## Baseline DSP

The deterministic core is portable scalar C++20 arranged as fixed arrays and simple loops. Clang can vectorize parts of it for arm64. Once correctness is stable, profile before replacing code with vDSP; explicit vectorization is useful only where it improves the callback tail, not merely a microbenchmark average.

## BNNS Graph CPU — production candidate

Apple documents BNNS Graph for real-time CPU inference in Audio Units, including:

- compiling the graph before processing;
- selecting single-thread execution;
- direct input/output pointers;
- precomputing maximum dynamic shapes;
- allocating page-aligned workspace before render;
- executing without runtime allocation.

Reference: https://developer.apple.com/videos/play/wwdc2024/10211/

This backend best matches the hard-deadline requirement. It uses Apple-silicon CPU vector/matrix hardware through Accelerate even though it does not use the Neural Engine.

## Core ML / Neural Engine — experimental

Core ML can dispatch across CPU, GPU, and Neural Engine and supplies useful performance reports and compute plans. It is attractive for throughput and energy efficiency, but an audio callback needs bounded completion, not just high average throughput. The experimental branch must test:

- first-inference warmup;
- dynamic-shape behavior;
- execution-provider changes;
- p99.99 and maximum duration at live buffer sizes;
- behavior under GPU/UI load and system thermal pressure;
- whether stateful model calls allocate or synchronize.

It is not the default until those tests pass.

## GPU / Metal

A GPU path may make sense for a single multi-channel host at larger blocks. It is a poor first choice for independent 16–64 sample plug-in callbacks because command submission and synchronization can dominate useful work. No render callback may wait on a UI/graphics queue.

## Instance sharing

Share immutable compiled model data only when the API explicitly permits concurrent contexts. Each instance keeps independent workspace and recurrent state. Never serialize ten plug-ins through a global mutex; that converts ordinary load into unpredictable deadline misses.

## Deployment target

The research branch keeps macOS 13 and arm64 for compatibility with the existing host. BNNS Graph availability and the final minimum macOS version must be checked when the first model is integrated; the build must provide a deterministic DSP-only fallback on unsupported systems.
