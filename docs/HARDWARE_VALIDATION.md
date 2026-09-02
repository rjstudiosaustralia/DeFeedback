# Hardware validation

## Acceptance criteria

A configuration is considered show-ready only when all of the following pass:

- 48 kHz at the chosen buffer;
- all requested lanes report `PROCESSED`;
- no new XRuns after startup during an eight-hour muted soak;
- callback CPU normally below 70%, with editor windows both open and closed;
- plugin state and routing restore after a clean reboot;
- device power-cycle causes a clear stopped/error state and recovers without unexpected output;
- no unprocessed parallel path exists in the interface, Dante, or console routing;
- trial mode is replaced by a valid live licence.
- the disabled LAN remote leaves TCP `8765` closed;
- remote reconnect, device refresh, lane editing, engine stop/start, and master mute/unmute behave correctly; and
- losing the browser or control network does not interrupt audio or change the last commanded state.

## Development baseline

Date: 2026-08-30

| Host | Device | Rate / buffer | Lanes | Output | CPU | XRuns | Result |
|---|---|---:|---:|---|---:|---:|---|
| M1 Max, macOS 26.3.1 | Built-in mic/speakers | 48 kHz / 64 | 1 | Globally muted after AU | 9.5–11.2% | 1 at startup, no increase over short run | Functional smoke test |

The built-in device reported 72.7 ms round trip and is not representative of a professional interface. The result verifies AU discovery, instantiation, real-time processing, editor hosting, output gating, CPU reporting, and stable XRun count over a short run only.

## Target matrix

Record one row for each test. Do not infer untested lane counts.

| Host | Device/driver | Transport | Buffer | Lanes | Closed UI CPU | Open UI CPU | New XRuns / 8h | Measured RTT | Pass |
|---|---|---|---:|---:|---:|---:|---:|---:|---|
| M1 Max | Dante Virtual Soundcard | Dante | 128 | 1/2/4/8/10/then device capacity | TBD | TBD | TBD | TBD | TBD |
| M1 Max | RME Digiface | USB/Thunderbolt | 32/64/128 | 1/2/4/8/10/then device capacity | TBD | TBD | TBD | TBD | TBD |
| M1 Max | Focusrite RedNet TNX | Thunderbolt/Dante | 32/64/128 | 1/2/4/8/10/then device capacity | TBD | TBD | TBD | TBD | TBD |
| M4 Mac mini | Target interface | Target transport | 32/64/128 | 1/2/4/8/10/then device capacity | TBD | TBD | TBD | TBD | TBD |
| M4 Pro or newer | Target interface | Target transport | 32/64/128 | 1/2/4/8/10/then device capacity | TBD | TBD | TBD | TBD | TBD |

## Test sequence

1. Reboot with the interface already powered and connected.
2. Disable unnecessary background applications and automatic OS updates for the test window.
3. Launch with all outputs muted.
4. Verify the selected device, 48 kHz, and desired buffer.
5. Add lanes incrementally: 1, 2, 4, 8, 10, then continue toward the intended device capacity while watching CPU and XRuns.
6. Feed representative isolated vocal material to every active input.
7. Record CPU and XRun counts with all plugin windows closed.
8. Open every editor, move windows, and adjust strength while observing XRuns.
9. Run the final lane count for eight hours while muted.
10. Repeat with console/interface returns connected at show gain only after the muted tests pass.
11. Reboot and log in with the display disconnected, then verify launch-at-login and remote authentication from the reserved control address.
12. Disconnect/reconnect the control client and network while audio runs; confirm CPU/XRuns and output state remain stable.
13. Attempt duplicate browser routes and incorrect access codes; confirm routes remain exclusive and authentication is rate-limited.
