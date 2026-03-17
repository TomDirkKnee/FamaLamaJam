# Phase 7 Room Chat and Controls Validation Matrix

**Phase:** 07-chat-room-control-commands  
**Plan:** 07-03  
**Host:** Ableton Live on Windows  
**Plugin artifact:** `build-vs/famalamajam_plugin_artefacts/Debug/VST3/FamaLamaJam.vst3`

Use this matrix during the real-room Ableton pass. Record what happened in the plugin, not what the code is expected to do.

| ID | Scenario | Steps | Expected plugin outcome | Result | Notes |
|----|----------|-------|-------------------------|--------|-------|
| P7-ROOM-01 | Public room chat send and receive | Connect to a live NINJAM room and send one normal public chat message from the plugin composer. Observe at least one room message round-trip. | The composer stays visible, the send action works from the plugin UI, and public room chat appears in the mixed room feed without requiring slash commands. | pass | Public chat send/receive worked in Ableton. |
| P7-ROOM-02 | Topic visibility | While connected, observe the room after a topic line is visible. | The current topic is visible near room controls, and topic changes also appear in the feed. | pass | Topic visibility worked in Ableton. |
| P7-ROOM-03 | Join/part visibility | While connected, observe at least one join or part event in the room feed. | Join or part activity remains visible in the mixed room feed and reads as room activity rather than chat authored by the local user. | pass | Join/part visibility worked in Ableton. |
| P7-ROOM-04 | BPM/BPI vote controls and feedback | Submit one BPM vote and one BPI vote from the direct controls. | Inline status clearly shows pending or failure state, shared room-visible outcomes appear in the feed, and successful room votes can affect server room settings. | pass | Voting worked and changed server settings. Residual uncertainty remains around how a non-initiator would vote against or vote no. |
| P7-ROOM-05 | Reconnect or fresh-session reset honesty | Disconnect and reconnect to the same or another room, or start a fresh room session after prior activity. | Old room feed history, old topic text, and stale vote status do not survive into the new live session. | pass | Old feed history cleared correctly on reconnect/new room. |
| P7-ROOM-06 | Readability at intended plugin size | Use the plugin at the intended rehearsal window size during the chat and vote pass. | The room section stays readable and does not crowd out transport or mixer workflows. | pass | Working pass, but the current room section squeezes the mixer too small and needs a broader layout overhaul later. |

## Session Notes

- Room/server:
- Ableton version:
- Plugin build timestamp:
- Anything surprising about vote semantics, feedback wording, or room density: Voting worked and changed server settings, but non-initiator vote-against / vote-no behavior remains unverified. The room section is readable, though the current single-page layout squeezes the mixer more than desired.
