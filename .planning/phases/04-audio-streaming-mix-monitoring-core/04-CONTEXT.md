# Phase 4: Audio Streaming, Mix, and Monitoring Core - Context

**Gathered:** 2026-03-15
**Status:** Ready for planning

<domain>
## Phase Boundary

Deliver a working Ninjam-compatible audio streaming, mix, and monitoring core on top of the Phase 3 timing/interval foundation. This phase hardens the existing send/receive path and adds essential monitor/mix controls and UI for real rehearsal use. It does not expand into advanced routing, diagnostics panels, or host-lifecycle hardening work.

</domain>

<decisions>
## Implementation Decisions

### Streaming scope
- Treat transmit and receive as an existing working baseline, not a net-new capability to invent from scratch.
- Phase 4 should harden the current send/receive path for real use rather than replace the interval-based streaming model established in Phase 3.
- Local input must continue to transmit using the interval-based upload flow already aligned to server timing.

### Remote strip model
- Remote mixer controls are keyed by `user + channel`, not by user alone.
- The UI should group remote strips under each user so multi-channel users remain visually connected.
- Remote strip labels should prioritize `username + channel name`, with channel index available as secondary detail when needed.
- Remote strips should keep a stable user/channel order rather than reordering by activity or loudness.

### Essential controls
- Phase 4 remote-strip controls include per-channel meter, gain, pan, and mute.
- A dedicated local monitor strip is required and should be clearly labeled as live/local monitoring.
- The local monitor strip should also include meter, gain, pan, and mute so it matches the essential monitoring workflow.
- Solo is deferred beyond Phase 4.

### Metering and visibility
- Phase 4 metering should be simple realtime level metering, not advanced diagnostics or detailed peak-hold tooling.
- The UI must clearly distinguish local monitor audio from the delayed remote return so the user understands what is live versus interval-delayed.
- When a remote user/channel becomes inactive, its strip should remain briefly visible and then hide if it stays gone.

### Persistence and continuity
- Per-remote-channel gain, pan, and mute settings should persist in project state.
- When the same remote user/channel appears again after reconnect or project reload, prior mix settings should be restored.
- The phase should continue to support supported host sample rates and buffer sizes without pitch or tempo artifacts.

### Claude's Discretion
- Exact inactive-strip timeout before hiding.
- Exact meter drawing style, as long as it remains simple and realtime.
- Exact visual grouping treatment for multi-channel users within the existing JUCE UI style.

</decisions>

<specifics>
## Specific Ideas

- Separate stream metering in the UI is considered an important part of Phase 4.
- Essential controls should feel like a practical small mixer: per-strip meter, volume, pan, and mute.
- The user explicitly wants the software to both transmit and receive audio; Phase 4 should make that core reliable and usable rather than leaving it as an experimental-feeling path.
- Grouping by user while controlling by user+channel is important because one musician may expose more than one channel.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/plugin/FamaLamaJamAudioProcessor.cpp`: already buffers local upload audio by interval, encodes outbound audio, receives inbound remote intervals, and renders active remote audio in sync with server boundaries.
- `src/net/FramedSocketTransport.cpp`: already implements Ninjam auth, config, user-mask subscription, and interval upload/download message handling.
- `src/audio/CodecStreamBridge.cpp`: already provides queued encode/decode bridging and now preserves decoded sample-rate metadata.
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`: already contains the main JUCE editor surface, transport status, metronome toggle, and current default gain/pan/mute controls.

### Established Patterns
- Phase 3 established server-authoritative interval timing, honest reconnect/timing-loss behavior, and boundary-quantized remote playback.
- The current UI is a simple built-in JUCE control surface rather than a complex custom mixer framework.
- Existing session settings already persist global/default channel gain, pan, and mute; Phase 4 will need a corresponding model for per-remote-channel settings.

### Integration Points
- Per-channel mix state will need to connect remote source identity from transport/audio processing into editor-visible strip models.
- Remote strip activity and metering should derive from the existing active remote interval/render path rather than a separate discovery-only model.
- Local monitor presentation should integrate with the same editor surface that currently hosts global transport and session controls.

</code_context>

<deferred>
## Deferred Ideas

- Solo controls.
- Advanced routing or patchbay behavior.
- Dedicated diagnostics/observability panels for stream debugging.
- Host lifecycle hardening and broader rehearsal UX validation beyond the core mix/monitoring work.

</deferred>

---

*Phase: 04-audio-streaming-mix-monitoring-core*
*Context gathered: 2026-03-15*
