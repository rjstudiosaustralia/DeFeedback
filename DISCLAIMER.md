# Disclaimer

DeFeedback Live is an independent third-party Audio Unit host created by Ryan Somerfield / RJ Studios Australia.

It is not affiliated with, sponsored by, approved by, or endorsed by Alpha Labs LLC. “De-Feedback” and any related product or company names belong to their respective owners. The Alpha Labs De-Feedback Audio Unit is not included in this repository or its release packages and must be obtained and licensed separately.

This software is an engineering preview supplied without warranty. Use it entirely at your own risk. The authors and contributors are not responsible for show interruption, lost audio, feedback, hearing injury, equipment damage, data loss, lost revenue, or any other direct or indirect loss arising from its use.

Live audio systems can produce sudden and dangerous sound levels. Before passing audio to amplifiers or loudspeakers:

- engage the application's master output mute;
- verify every input and output route end to end;
- remove unintended dry or parallel paths;
- use appropriate system limiting and hearing protection;
- test the intended device, sample rate, buffer, lane count, and plugin version under realistic load; and
- keep a qualified operator in control of an independent hardware or console mute.

No software mute should be treated as the only emergency safety control.

The optional LAN remote provides authenticated full control, including engine start and master-output unmute. Its current HTTP transport is not encrypted. Use it only on a trusted private network or isolated control VLAN; never expose its port to the internet or an untrusted/shared network. Anyone who obtains the access code or an active session can change live-audio state. Network authentication and browser confirmation dialogs do not replace a qualified operator or independent hardware safety control.
