# LAN remote control

DeFeedback Live 0.5 adds an optional full-control browser interface for operating a monitor-free Apple Silicon Mac on a trusted local network.

## Enable and connect

1. Connect the Mac and control device to the same trusted private network.
2. In DeFeedback Live, enable `Enable full-control LAN remote`.
3. Use `COPY DETAILS` or note the displayed `http://IP-address:8765` address and eight-digit access code.
4. Open the address in a current browser and enter the code.
5. Enable `Launch at login` if the app should return after the macOS user logs in.
6. Enable `Auto-start audio` only after the restored routes and output state have been qualified on the target system.

The remote-enabled state and access code are saved. `NEW CODE` immediately restarts the listener and invalidates every existing browser session. Disabling the remote closes the listening socket and invalidates all sessions.

## Available controls

The browser can:

- refresh and select the Core Audio device;
- select a supported sample rate and buffer size;
- start or stop the complete audio engine;
- mute or unmute all physical outputs;
- view latency, callback CPU, meters, and XRuns;
- reset the session XRun counter;
- add, remove, and rename lanes;
- change exclusive mono input/output routes;
- adjust De-Feedback Strength and plugin Mute;
- enable or disable dry bypass; and
- change auto-start and launch-at-login preferences.

The browser confirms engine start and master-output unmute in its normal UI. These dialogs reduce accidental clicks but are not an authorization boundary; an authenticated client has full control. The native Audio Unit editor cannot be rendered remotely, but all De-Feedback parameters currently exposed by this host are available in the lane strip.

## Network and credential safety

The preview server uses HTTP without transport encryption. The code and session traffic can be observed by another party with access to the same untrusted network. Therefore:

- use a dedicated wired control network or isolated VLAN where practical;
- do not port-forward or otherwise publish TCP `8765` to the internet;
- do not use the remote on public, venue-guest, or shared Wi-Fi;
- use a DHCP reservation or static control-network address for a headless Mac;
- regenerate the code after giving temporary access to another operator;
- disable the remote when it is not needed; and
- always retain an independent console or hardware mute.

The eight-digit code is stored in the app's settings file, which is restricted to the current macOS user. Login failures are rate-limited, authenticated sessions are memory-only, and the browser stores its session token only for that browser tab session. The page loads no third-party scripts, fonts, analytics, or cloud resources.

## Headless limitations and recovery

DeFeedback Live remains a macOS GUI login item rather than a system service. A macOS user must log in after reboot, and FileVault may require physical or approved remote login before that session exists. Closing the app quits both audio and remote control.

Before relying on monitor-free operation:

1. verify launch-at-login after a full reboot;
2. verify the Mac's control IP address remains stable;
3. disable automatic sleep and qualify any OS update policy;
4. confirm remote access still works when the preferred interface is disconnected at launch;
5. confirm device refresh and recovery after powering the interface later;
6. test a complete browser disconnect/reconnect without interrupting audio; and
7. retain Screen Sharing or physical keyboard/display access for recovery.

If the listener cannot bind to `8765`, the native app reports the error and leaves the remote off. Another local service may already be using that port.

## Audio-thread boundary

The network thread never calls the real-time audio callback or plug-in graph directly. The 10 Hz app timer publishes read-only state snapshots for browser polling. Authenticated commands are queued onto the JUCE message thread, where the same validated device, routing, and plug-in-control operations used by the native interface are performed. A stalled or malicious browser connection can delay network responses, but it cannot block the Core Audio callback.
