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
- trial mode is replaced by a valid live licence;
- the disabled LAN remote leaves TCP `8765` closed;
- remote reconnect, device refresh, lane editing, engine stop/start, and master mute/unmute behave correctly; and
- losing the browser or control network does not interrupt audio or change the last commanded state;
- disabling a lane passes its input dry, lowers plugin CPU load, and restores the same route/settings without a graph interruption when re-enabled; and
- `pmset -g assertions` reports the app's idle-system-sleep prevention assertion while it is open.

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
13. Attempt duplicate browser routes and incorrect access codes; confirm routes remain exclusive and failed authentication is delayed.
14. Reload and restart an authenticated browser; confirm it reconnects without another code, then use `LOG OUT` and confirm authentication is required.
15. Change the access code and confirm both browser and Companion must re-authenticate.
16. If no-code mode will be used, confirm the control network is isolated and that an untrusted LAN client cannot reach the port.
17. Record CPU with one or more lanes disabled; confirm their outputs pass dry audio and re-enable them while watching XRuns and retained Strength/plugin state.
18. Run `pmset -g assertions`; confirm DeFeedback Live owns a `PreventUserIdleSystemSleep` assertion while it is open, then confirm it is released after quitting the app.

## Clear Voice Live 1.1.5 integration — 2026-09-15

The opt-in `DeFeedbackPluginTests` ran against the installed De-Feedback and Clear Voice AUs through the production engine using a 16-channel memory-only device. No Core Audio interface or speaker output was opened. All functional assertions passed: eight mode round-trips, De-Feedback Strength/Mute/activity retention and unchanged delivery of the saved opaque bank, Clear Voice controls/state, plugin latency reporting, supported sample rates (48/96/192 kHz) at 32/64/128/512 samples, rejection of unsupported rates and live mode/CPU/latency changes, hard mute, suspended lanes, and sample-exact dry pass. Generated-noise attenuation after warm-up confirmed Clear Voice was actually processing, not merely passing dry audio.

One run's callback wall times at 48 kHz, LOW CPU / ZERO latency:

| Active lanes | Buffer | Mean | p99 | Maximum | Block deadline |
| --- | --- | --- | --- | --- | --- |
| 4 | 64 | 0.128 ms | 0.343 ms | 0.845 ms | 1.333 ms |
| 8 | 64 | 0.264 ms | 0.895 ms | 1.446 ms | 1.333 ms |
| 16 | 64 | 0.533 ms | 1.788 ms | 2.633 ms | 1.333 ms |
| 16 | 128 | 1.068 ms | 2.649 ms | 6.278 ms | 2.667 ms |

These are normal-thread offline timings, not a real-time Core Audio/XRun qualification. The 16-lane / 64-sample case exceeded its deadline at p99; 8 lanes / 64 and 16 lanes / 128 also had individual overruns in this run. Do not qualify those combinations from average CPU alone. The real interface, driver, Core Audio scheduling, buffer, editor load and intended channel count must pass a muted soundcheck/soak. No parallel processing architecture or fixed lane cap was introduced into the existing stable host.


## VoiceISO and OFF dry pass — 2026-09-15, host 0.9.0

Tested with installed VoiceISO 1.0.6, Clear Voice 1.1.5 and De-Feedback 1.1.4 through a memory-only device. No Core Audio interface or loudspeaker was used.

- Configuration tests passed for format 6 migration, all directed three-mode transitions, restart persistence, route/state separation, and the processed/dry selector with master mute in float and double precision.
- VoiceISO tests passed for Amount 0–120%, 100% factory value, native bypass, zero Amount dry pass, mono processing at 44.1/48/88.2/96/176.4/192 kHz with 32/64/128/512 samples, route/control restoration, and independent lanes. Noise energy after warm-up was approximately 0.182 of input; the tested factory setting reported zero plugin delay.
- All-mode pass-through tests passed: OFF suspends the same AU node while passing sample-exact dry audio after the graph's reported delay; OFF/ON repeats without rebuilding nodes; saved OFF restores through graph rebuilds; output meters track dry audio; master mute still zeros every output. The Clear Voice test included LOW latency and retained plugin Mute was also exercised in De-Feedback.
- Existing De-Feedback/Clear Voice functional integration checks passed after the output-path change.
- Companion TypeScript/lint and four API/control tests passed (Node 24 build/test runtime; packaged module continues to target Companion Node 22).
- Native settings/rack and browser remote were inspected in the isolated QA app. Automated HTTP tests cover all three banks, stale commands, routing, OFF dry state, output meters, and master mute precedence.
- The real VoiceISO editor opened with factory **Full**, Strength **100%**, and **License not active: Activation required**. Complete licensing in the lane UI before licensed operation. Changing Full/Lite could not be confirmed through UI automation; verify that selection and its restoration manually after activation. The host preserves the AU-provided opaque state without modifying private model bytes.

These are functional checks, not a maximum-channel benchmark or live feedback test. No VoiceISO channel cap was added; choose the lane count using actual CPU/XRuns on the intended interface. The new two-input output meter adds a dry feed to the graph; re-check live timing on the complete rig.

## Mixed plugins and display wake — 2026-09-20, host 0.10.0

- Configuration tests passed for mixed-rack serialization, per-lane vendor state isolation, apply-all patch preservation, format 6/7 migration, legacy downgrade projection and combined sample-rate restrictions.
- Installed-AU `--mixed`, `--functional`, `--voiceiso`, and `--pass-through` suites passed using only the memory audio device. Mixed tests cover all three plugins together, independent controls, repeated reassignment, lane reordering, fresh-engine XML restoration, dry OFF output, master mute and sample-rate recovery after removing Clear Voice.
- Companion build, lint and all five API/control tests passed; the module was packaged as 0.4.0.
- Isolated QA HTTP tests passed for mixed selectors, stale-command rejection, patch validation, unchanged settings, apply-all, OFF meters and master mute. Native Settings and mixed rack controls, plus browser mixed selectors and controls, were visually inspected.
- `pmset -g assertions` showed the QA app holding both `PreventUserIdleSystemSleep` and `PreventUserIdleDisplaySleep`. This verifies macOS assertion registration; a full unattended idle-duration test and hardware audio qualification remain operator checks. Explicit Sleep and lid closure can still interrupt the session.
