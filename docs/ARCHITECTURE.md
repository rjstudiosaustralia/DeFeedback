# Architecture

## Product boundary

DeFeedback Live is a purpose-built host. It does not contain, modify, inspect, or redistribute the De-Feedback implementation.

## Runtime graph

Each mono lane is built as:

```text
Core Audio input N
        │
     input meter
        │
        ├─ De-Feedback AU ─ atomic output gate ─ output meter ─ Core Audio output M
        │
        └─ dry fallback ─── atomic output gate ─ output meter ─ Core Audio output M
```

The application has one `juce::AudioDeviceManager`, one `AudioProcessorPlayer`, and one `AudioProcessorGraph`. Every lane receives its own De-Feedback instance plus raw-input and post-gate output meter nodes. Input and output channel assignments are exclusive across lanes; duplicate routing is rejected by both the UI and engine.

## Audio Unit discovery

The target AU identity is fixed to:

```text
type:         aufx
subtype:      FbTI
manufacturer: jDSP
name:         Alpha Labs LLC: De-Feedback
```

Normal discovery uses Core Audio registration. If the system component cache is stale but the signed component exists in the standard system path, the app loads the documented AUv2 factory and registers that component within its own process. It does not scan or load unrelated plugins.

## Real-time constraints

- The global mute is an atomic flag read by each post-plugin meter node.
- Audio callbacks perform no filesystem access, UI work, logging, or allocation in application code.
- Peak meters publish through atomics and are consumed by the 10 Hz UI timer.
- Graph construction, state capture, and plugin editor creation happen on the message thread while callbacks are detached.
- Device changes detach the callback, rebuild the graph, and reattach it.

JUCE's `AudioProcessorGraph` currently renders these independent lanes serially in one callback. This is intentionally the simpler and more predictable first implementation. If target hardware cannot meet ten lanes at the required buffer, the next engine revision should use fixed worker threads joined to the Core Audio device workgroup. Parallelization must be benchmark-driven because synchronization overhead can be worse at very small buffers.

## Persistence

Settings are stored at:

```text
~/Library/Application Support/RJ Studios Australia/DeFeedback Live/settings.xml
```

The file contains device names, sample rate, buffer size, lane mappings, operator bypass state, main/plugin window layouts and each AU instance's opaque state block. Open plugin editors are recreated at their saved positions. Writes use a temporary file followed by atomic replacement. Version 0.2 migrates reads from the original `~/Library/RJ Studios Australia/...` path and writes future changes to the Application Support path above.

No licence credentials or plugin binaries are stored by the host.

## Failure modes

| Event | Behavior |
|---|---|
| Plugin missing or instance load failure | Lane passes dry and displays an amber warning |
| Invalid input/output index | Lane is disconnected and marked invalid |
| Duplicate input or output assignment | Later lane is disconnected and marked duplicate |
| Operator dry bypass | Lane passes dry and displays an amber warning |
| Global mute | Plugins keep processing; samples are zeroed immediately before graph output |
| Device change/disconnect | Callback stops; graph rebuild is scheduled on the message thread |
| AUv2 process crash | Host can terminate; this release cannot recover in-process execution |

## Distribution

The application is not sandboxed because it must host a third-party AUv2 and access professional Core Audio devices. A notarized release will need hardened runtime plus the library-validation entitlement appropriate for an audio plugin host. Signing credentials remain outside the repository.
