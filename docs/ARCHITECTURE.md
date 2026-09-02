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

## LAN remote boundary

The optional remote uses a small HTTP server on its own normal-priority JUCE thread:

```text
browser fetch ─ authenticated HTTP thread ─ command queue ─ JUCE message thread ─ AudioEngine
                                           ▲
10 Hz UI timer ─ JSON state snapshot ─ mutex-protected string
```

The network thread never calls `AudioEngine`, an Audio Unit instance, JUCE UI objects, or the Core Audio callback. It serves the most recent immutable JSON snapshot and queues validated commands with `MessageManager::callAsync`. A slow or deliberately stalled network client can delay other browser requests but cannot wait on or block the audio callback.

The server exists only while the saved remote-enabled setting is on. It binds TCP `8765` on all active interfaces, serves a page embedded in the executable, and loads no remote assets. Unauthenticated clients can retrieve only the login shell. API access requires an eight-digit code followed by a memory-only bearer session. Disabling/restarting the server or regenerating the code invalidates every session. Failed logins are rate-limited, API responses are not cacheable, and the page uses a restrictive content security policy. The preview is HTTP-only and is intended for an isolated trusted LAN, not internet exposure.

JUCE's `AudioProcessorGraph` currently renders these independent lanes serially in one callback. The application does not impose a fixed lane count; the selected device's exclusive input/output pairs determine how many lanes can be configured. The practical live ceiling remains CPU-, buffer-, plugin-, and driver-dependent. If target hardware cannot meet the required lane count at the required buffer, a future engine revision can prototype fixed worker threads joined to the Core Audio device workgroup. Parallelization must be benchmark-driven because synchronization overhead can be worse at very small buffers.

## Persistence

Settings are stored at:

```text
~/Library/Application Support/RJ Studios Australia/DeFeedback Live/settings.xml
```

The file contains device names, sample rate, buffer size, lane mappings, operator bypass state, main/plugin window layouts, remote-enabled state and access code, and each AU instance's opaque state block. Open plugin editors are recreated at their saved positions. Writes use a temporary file followed by atomic replacement, and the resulting file is restricted to the current macOS user. Version 0.2 migrates reads from the original `~/Library/RJ Studios Australia/...` path and writes future changes to the Application Support path above.

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
| LAN client disconnect or request stall | Audio continues; only browser responses are affected |
| Invalid/duplicate remote route | Command is rejected; active graph and route remain unchanged |
| Remote disabled or access code regenerated | Listener/sessions close; audio state is unchanged |
| AUv2 process crash | Host can terminate; this release cannot recover in-process execution |

## Distribution

The application is not sandboxed because it must host a third-party AUv2 and access professional Core Audio devices. A notarized release will need hardened runtime plus the library-validation entitlement appropriate for an audio plugin host. Signing credentials remain outside the repository.
