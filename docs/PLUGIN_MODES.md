# Plugin modes and patching

## Operator workflow

Open **SETTINGS** to choose **De-Feedback**, **Clear Voice Live 1.1**, or **VoiceISO** for each lane. Mix all three in one rack. Each strip shows its plugin name and the appropriate controls and Strength scale. Input and output patches are edited beside the plugin selector.

To use one plugin everywhere, choose it under **PLUGIN FOR ALL LANES** and click **APPLY TO ALL LANES**, then **APPLY SETTINGS**. This keeps all lane names and routes. New lanes use the last all-lanes selection (De-Feedback by default).

**APPLY SETTINGS** validates exclusive routing, stops the engine, and mutes every output before changing a plugin or patch. Start the engine afterward to check processing and load while outputs remain muted. Unmuting is a separate operator action. Cancel leaves the running rack untouched; unchanged settings also leave it running. Browser Settings provides the same per-lane selectors and an explicit Apply to all lanes button; browser changes apply immediately and also stop/mute the engine.

Each lane remembers a separate AU state, exposed parameter snapshot, dry bypass, enabled state and editor layout for each plugin. Switching a lane back restores its plugin settings while retaining its current name and route. First use starts from fresh vendor settings. Other lanes retain their latest controls when the graph rebuilds.

De-Feedback's AU identity, Strength scale and plugin Mute are preserved. In every mode, plugin OFF now suspends DSP and passes input audio dry; BYPASS also passes dry. Master output mute still overrides both. Clear Voice uses a separate, fixed AU identity (`aufx/Cv11/FabS`); the host never scans unrelated plugins or loads Clear Voice v1.0 in place of v1.1.

## Clear Voice controls

| Control | Host behavior |
| --- | --- |
| Strength | Native/browser scale 0–10. The API remains normalized 0–1 and existing Companion Strength actions remain percentages. At zero, the host explicitly reports dry processing. |
| Dereverb | Scale 0–10; only effective when Strength is above zero. |
| CPU | LOW / HIGH. Host changes require a stopped engine. |
| Latency | ZERO / LOW. Host changes require a stopped engine, with graph reconstruction on restart. |
| Plugin on/off | Suspends that instance and passes input audio dry, retaining settings. |
| Bypass | Host dry pass, as in De-Feedback mode. |
| UI | Opens the installed plugin's native editor. |

The first-use preset is the vendor's **Good starting point**: Strength 3, Dereverb 6, LOW CPU, ZERO latency. Saved settings always take precedence. Clear Voice exposes no AU Mute parameter, so the De-Feedback-specific plugin Mute control is hidden in this mode. Master output mute and plugin off remain available.

The host checks the four actual published AU control names (`Strength`, `Dereverb`, `Engine`, `Latency`). Legacy parameters such as `Dereverb Boost` and `Voice Mode` are not mistaken for the visible controls. It reads authoritative values at the existing 10 Hz UI cadence, including changes made in the native editor.

## VoiceISO controls

VoiceISO **1.0.6** is bound to the installed AU identity `aufx/Viso/Maze` and `VoiceISOAUFactory`. No broad plugin scan or binary/model download is performed.

- **Strength 0–120%** controls the published AU parameter **Amount**. The AU value is normalized 0–1; its factory value is 0.833333 (100%). New lanes retain the vendor's factory settings. Zero is reported as dry.
- **Full / Lite** is selected in the native plugin editor using the lane's **UI** button on the Mac. Stop the engine before changing model. The AU exposes no model parameter; the host preserves the opaque state returned by the plugin and does not rewrite private model bytes.
- The published **Bypass** parameter is observed separately from host BYPASS. A native bypass is reported as dry. There is no VoiceISO Mute, Dereverb, CPU or latency selector in the host.
- **OFF** suspends plugin DSP and passes input audio dry. Host **BYPASS** passes dry audio. Both continue through the master output gate and output meter.
- There is no application-imposed VoiceISO channel limit. Add lanes up to the connected device's I/O capacity and choose the number your Mac can process at the selected buffer.
- The installed AU accepted mono processing at 44.1, 48, 88.2, 96, 176.4 and 192 kHz in offline tests. Its reported delay was zero at the tested factory setting; any reported graph delay is included in the host latency estimate.

The [official VoiceISO site](https://voiceiso.com/) describes Full as the highest-quality model, Lite as the lower-load choice, and Strength up to 120%. It advertises no algorithmic latency. Install and license the plugin separately. Offline host tests do not qualify a live interface or establish a maximum channel count.

## OFF and BYPASS in every mode

When the engine is running, both controls keep audio passing on the lane's existing patch. OFF selects a dry feed while suspending the plugin; ON resumes the retained instance. BYPASS selects dry routing. The master mute still zeros outputs, and STOP ENGINE still stops audio. De-Feedback's own Mute is audible only when its processed path is selected.

The output meter has processed and dry inputs. Its preallocated copy selects the dry input for OFF; JUCE aligns the two inputs to the graph delay, including Clear Voice LOW latency. Switching OFF/ON changes suspension and dry selection together between callbacks; plugin reset, state capture and editor work stay outside that short callback lock. No graph reconstruction or allocation is added to the OFF/ON audio path.

## Vendor requirements and practical limits

Checked against installed Clear Voice Live **1.1.5** and the [official manual](https://clearvoice.live/manual) on 2026-09-15:

- Supported rates are **48, 96 and 192 kHz**. Other rates are excluded from the selector and rejected by engine start. If an external device change forces an unsupported rate during playback, lanes explicitly report dry fallback.
- **LOW CPU / ZERO latency** is the recommended combination for feedback-critical live monitoring. HIGH CPU with ZERO latency costs substantially more. LOW latency reports an additional delay (127 samples at 48 kHz in the installed AU); the host includes graph delay in the host's latency estimate. The graph can compensate parallel paths to the longest plugin delay, so ZERO on one lane does not guarantee zero graph delay when another lane uses LOW.
- The vendor recommends 32–64-sample buffers for live monitoring. This is not a guarantee of a particular lane count on a particular Mac. Test CPU/XRuns with the intended interface and complete rack; increase the buffer or reduce active lanes when needed.
- The AI model loads in the background when audio first flows. Allow a muted warm-up before judging operation or load. Closing unused plugin editors reduces UI overhead.
- Use one instance per close-miked voice, early in the channel chain. Tune Strength first, then Dereverb.
- Clear Voice's native editor has its own Bypass, Delta, link groups and licensing UI. These are not exposed as normal AU parameters by 1.1.5. The host cannot certify the license/trial status or observe its private Delta/bypass state. Verify the native editor is active and use the host's bypass for show operation. Do not use Delta on a live PA/monitor channel; the vendor describes it as a diagnostic-only control.

No new runtime/model downloads or plugin binaries are bundled with this host. Install and license Clear Voice separately. AUv2 remains in-process: a plugin crash can still terminate the application, as with De-Feedback.

## Persistence and compatibility

Settings format 8 stores the active rack in `RACK_LANES`, with `pluginMode` on each lane and inactive `PLUGIN_STATE` entries keyed by plugin. Legacy settings load the previously selected rack, preserving its names, routes and controls. Other saved mode states are matched by lane ID; original legacy racks remain archived in `LEGACY_BANK` entries for recovery. Legacy root `LANE` entries contain only De-Feedback state so an older host cannot load another vendor's state into De-Feedback. Downgrading and re-saving discards format 8 data; keep a backup before downgrading.

Parameter snapshots are checked against plugin identity and published parameter IDs/names. Capture and graph reconstruction run on the message thread. Old nodes are retired before lane identity/order is reassigned, preventing state from crossing lanes or plugins. The shared sample-rate selector uses the intersection supported by every selected plugin; a rack containing any Clear Voice lane requires 48, 96 or 192 kHz. The latency estimate includes the entire graph's reported delay.

No scanning, state serialization or network work runs in the audio callback. All selected lanes have independent plugin instances; inactive saved plugin choices are not instantiated.

## Validation

Run the regular configuration tests with `ctest --test-dir build-Release-13 --output-on-failure`.

For opt-in integration tests with both separately installed AUs:

```sh
cmake -S . -B build-Release-13 -DCMAKE_BUILD_TYPE=Release -DDEFEEDBACK_BUILD_PLUGIN_TESTS=ON
cmake --build build-Release-13 --parallel 4
build-Release-13/DeFeedbackPluginTests_artefacts/Release/DeFeedbackPluginTests
```

The test uses a memory-only audio device through the production device manager/player/graph. It never opens an audio interface or plays through speakers. It covers real parameter binding, independent state restoration, repeated swaps, supported rates/buffers, live-change rejection, reported latency, actual noise processing after warm-up, sample-exact dry pass, master mute and plugin-OFF dry pass. Its wall-clock timing measurements are diagnostic; they do not qualify a Core Audio driver or live rig.

Pass `--functional` to the integration executable to skip the optional multi-lane timing loops while retaining restoration, routing, control, warm-up, rate/buffer and output-gate assertions. AU re-serialization is not required to return identical bytes; tests verify delivery of the original saved bank and the actual restored control values.

VoiceISO-specific checks: run the same integration executable with `--voiceiso`. All-mode OFF/BYPASS checks: `--pass-through`. The latter verifies sample-exact dry audio after reported graph delay, output metering, retained plugin Mute, repeated OFF/ON transitions without replacing nodes, reconstruction while OFF, and master mute precedence. Tests open only the memory device.

Mixed-rack integration checks: `DeFeedbackPluginTests --mixed`. This exercises simultaneous instances of all three plugins, per-lane controls, state restoration, reorder safety, XML restart, rate restrictions, dry OFF output and master mute. `python3 Tests/RemoteModeTests.py` targets only the isolated QA app on port 8766 and tests the mixed remote workflow.
