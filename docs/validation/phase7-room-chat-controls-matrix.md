# Phase 7 Room Chat and Controls Validation Matrix

**Phase:** 07-chat-room-control-commands  
**Plan:** 07-03  
**Host:** Ableton Live on Windows  
**Plugin artifact:** `build-vs/famalamajam_plugin_artefacts/Debug/VST3/FamaLamaJam.vst3`

Use this matrix during the real-room Ableton pass. Record what happened in the plugin, not what the code is expected to do.

| ID | Scenario | Steps | Expected plugin outcome | Result | Notes |
|----|----------|-------|-------------------------|--------|-------|
| P7-ROOM-01 | Public room chat send | Connect to a live NINJAM room and send one normal public chat message from the plugin composer. | The composer stays visible, the send action works from the plugin UI, and the message appears in the mixed room feed without requiring slash commands. | pending | |
| P7-ROOM-02 | Topic and presence visibility | While connected, observe the room after at least one topic line and one join or part event. | The current topic is visible near room controls, topic changes also appear in the feed, and join or part activity remains visible but subtler than normal chat. | pending | |
| P7-ROOM-03 | BPM vote submission | Enter one BPM value from the direct room control and submit it. | Inline status clearly shows pending or failure state, and any shared room-visible outcome appears in the mixed feed. | pending | |
| P7-ROOM-04 | BPI vote submission | Enter one BPI value from the direct room control and submit it. | Inline status clearly shows pending or failure state, and any shared room-visible outcome appears in the mixed feed. | pending | |
| P7-ROOM-05 | Reconnect or fresh-session reset honesty | Disconnect and reconnect to the same or another room, or start a fresh room session after prior activity. | Old room feed history, old topic text, and stale vote status do not survive into the new live session. | pending | |
| P7-ROOM-06 | Readability at intended plugin size | Use the plugin at the intended rehearsal window size during the chat and vote pass. | The room section stays readable and does not crowd out transport or mixer workflows. | pending | |

## Session Notes

- Room/server:
- Ableton version:
- Plugin build timestamp:
- Anything surprising about vote semantics, feedback wording, or room density:
