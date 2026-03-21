# Phase 08.3 Functional Release Controls Matrix

**Phase:** 08.3-functional-release-controls-session-ux-and-voice-mode-compatibility-guard  
**Host:** Ableton Live on Windows  
**Plugin artifact:** `build-vs-2026/famalamajam_plugin_artefacts/Debug/VST3/FamaLamaJam.vst3`

Use this matrix for the focused manual smoke pass. Record exactly what happened, including partial success or anything not exercised.

| ID | Scenario | Steps | Expected plugin outcome | Result | Notes |
|----|----------|-------|-------------------------|--------|-------|
| P8.3-RECALL-01 | Remembered private-room recall reuses masked credentials | Pick a remembered private-room entry from the combined server picker. Confirm host, username, and masked password autofill. Connect once without retyping the password. | The connection draft is restored from remembered state, the password stays masked, and the room connects successfully without manual password re-entry. | pass | 2026-03-21 Ableton rerun passed: after seeding one successful private-room connection, removing the plugin, and inserting a brand-new instance, the remembered private entry appeared in the picker before public rooms and reconnected without retyping the password. |
| P8.3-TX-02 | Local strip transmit lifecycle is understandable | Connect to a room and watch the local strip after connection and timing acquisition. | The local strip clearly shows `Not transmitting`, then `Getting ready`, then `Transmitting` as appropriate. | pass | User reported the transmit lifecycle is clear in Ableton. |
| P8.3-TX-03 | Transmit cancel only stops upload | While the local strip is warming or active, click the transmit control once, then keep the session otherwise running. | Transmit returns to red/disabled, while connection, timing, receive, monitoring, chat, and metronome continue to work. | pass | User confirmed transmit-off only stops upload. |
| P8.3-UI-04 | Collapsible single-page polish preserves practical screen space | Collapse `Server settings` and diagnostics, then inspect the plugin window at a practical Ableton size. | The interval bar remains a full-width footer, room feed text wraps cleanly, and the plugin remains usable without opening a larger Phase 9 layout. | pass | User confirmed the collapsed single-page layout and footer bar work well enough in practice. |
| P8.3-VOICE-05 | Unsupported voice-mode peers are explicit | Join a room with a remote client flagged for voice mode. | The remote strip or status text makes it clear the voice-mode channel is unsupported, and the plugin does not pretend it is a normal interval return. | pass | User confirmed `mode=voice` produced no interval audio and switching the peer back to interval mode resumed audio after buffering. |

## Manual Outcome Summary

- All five focused manual checks passed.
- `P8.3-RECALL-01` now passes in the real host workflow the user asked for: the remembered private-room entry survives removing the plugin and creating a brand-new instance.
