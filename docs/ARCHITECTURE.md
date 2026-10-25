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

The application has one `juce::AudioDeviceManager`, one `AudioProcessorPlayer`, and one `ParallelRackProcessor`. Each lane owns a mono `AudioProcessorGraph`, an instance of its selected plugin, and raw-input/post-gate output meter nodes. Input and output channel assignments are exclusive across lanes; duplicate routing is rejected by both the UI and engine. A rack-level delay aligns lanes to the longest graph latency, followed by a final master mute gate.

## Plugin mode extension (0.8)

Each lane selects its own plugin; the rack can mix all three supported Audio Units. Inactive per-lane plugin states exist only as saved settings. De-Feedback keeps its original AU identity and processing path. Clear Voice Live 1.1 uses `aufx/Cv11/FabS`, its own AU registration fallback, normalized control bindings and rate validation. Settings owns patching, and mode/patch changes stop and mute audio. See [Plugin modes](PLUGIN_MODES.md) for state-bank isolation, parameter snapshots and integration tests.

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

- The global mute is an atomic flag read by each post-plugin meter node and again after rack latency alignment.
- Audio callbacks perform no filesystem access, UI work, logging, or allocation in application code.
- Peak meters publish through atomics and are consumed by the 10 Hz UI timer.
- Graph construction, state capture, and plugin editor creation happen on the message thread while callbacks are detached.
- Device changes detach the callback, rebuild the graph, and reattach it.

## LAN remote boundary

The optional remote uses a small HTTP server on its own normal-priority JUCE thread:

```text
browser/Companion ─ authenticated HTTP thread ─ command queue ─ JUCE message thread ─ AudioEngine
                                           ▲
10 Hz UI timer ─ JSON state snapshot ─ mutex-protected string
```

The network thread never calls `AudioEngine`, an Audio Unit instance, JUCE UI objects, or the Core Audio callback. It serves the most recent immutable JSON snapshot and queues validated commands with `MessageManager::callAsync`. A slow or deliberately stalled network client can delay other browser requests but cannot wait on or block the audio callback.

The server exists only while the saved remote-enabled setting is on. It binds TCP `8765` on all active interfaces, serves a page embedded in the executable, and loads no remote assets. By default, API access requires the operator's 4–64-character custom/random code. A successful login receives a one-year HttpOnly, SameSite cookie; Companion retains the same cookie in process and re-authenticates from its configured code as required. Changing the code or protection mode rotates the saved random browser secret and invalidates remembered clients. Explicit no-code mode bypasses authentication and is warned as full LAN access. Failed logins are delayed, API responses are not cacheable, and the page uses a restrictive content security policy. The preview is HTTP-only and is intended for an isolated trusted LAN, not internet exposure.

In multicore mode, persistent real-time threads and the hardware callback claim independent lanes from an atomic queue and finish within the same block. The callback waits for every dispatched worker before copying outputs. Worker count is bounded by lane count, performance-core count, and the device workgroup recommendation. Workers retain and join the device workgroup on their own thread. Serial mode runs the same lane graphs on the callback alone. Single-lane racks create no worker threads. See [Multicore preview](MULTICORE.md) for synchronization, measurements, and limits.

## Persistence

Settings are stored at:

```text
~/Library/Application Support/RJ Studios Australia/DeFeedback Live/settings.xml
```

The file contains device names, sample rate, buffer size, lane mappings, operator bypass state, main/plugin window layouts, remote-enabled state, access settings, the random remembered-browser secret, and each AU instance's opaque state block. Open plugin editors are recreated at their saved positions. Writes use a temporary file followed by atomic replacement, and the resulting file is restricted to the current macOS user. Version 0.2 migrates reads from the original `~/Library/RJ Studios Australia/...` path and writes future changes to the Application Support path above.

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
| Plugin reports changed latency | Rack output is silenced, master mute is armed, and alignment is rebuilt on the message thread |
| Worker cannot start | Use the workers that started successfully; fall back to serial if none started |
| LAN client disconnect or request stall | Audio continues; only browser responses are affected |
| Invalid/duplicate remote route | Command is rejected; active graph and route remain unchanged |
| Remote disabled | Listener closes; audio state and remembered-browser secret are unchanged |
| Access code or protection mode changed | Listener restarts and remembered clients are revoked; audio state is unchanged |
| AUv2 process crash | Host can terminate; this release cannot recover in-process execution |

## Distribution

The application is not sandboxed because it must host a third-party AUv2 and access professional Core Audio devices. A notarized release will need hardened runtime plus the library-validation entitlement appropriate for an audio plugin host. Signing credentials remain outside the repository.

VoiceISO uses `aufx/Viso/Maze`. Settings format 8 stores the mixed rack with independent per-lane vendor states; only each lane's selected plugin has a live instance. OFF selects the output meter's second, dry input while suspending the AU; JUCE compensates its delay against the processed input. Suspension and selector changes share a short graph callback lock, with plugin reset, serialization and editor work outside it. Master mute remains the final gate. Parameter snapshots bind VoiceISO Amount and Bypass; private model state remains opaque.
