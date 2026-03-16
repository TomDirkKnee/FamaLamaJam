# Phase 3: Server-Authoritative Timing & Sync - Context

**Gathered:** 2026-03-14
**Status:** Ready for planning

<domain>
## Phase Boundary

Implement server-authoritative Ninjam interval timing and visible sync state for stable rehearsal timing inside the plugin. This phase covers interval/metronome state visibility, timing recovery after reconnect, and interval-quantized remote playback switching. It does not expand into broader channel-mix or monitoring controls.

</domain>

<decisions>
## Implementation Decisions

### Remote interval switching
- All remote decoded streams must follow the same shared server interval boundary.
- A remote user's interval is eligible for playback only if a full interval is buffered by its intended server boundary.
- Remote playback switches only on server-defined boundaries, never on packet arrival time.
- If an interval misses its boundary, do not play it late or mid-interval; skip it and wait for a later valid interval.
- Preserve alignment over continuity: on-time users can advance at the boundary while late users remain silent until they again have a full interval ready on a valid boundary.
- If BPM/BPI changes mid-session, apply the new interval length on the next server boundary rather than cutting the current interval.

### Sync recovery behavior
- If the connection is active but server timing has not arrived yet, show an explicit waiting-for-timing state rather than estimating locally.
- On timing loss or reconnect, stop the metronome and show an explicit error/reconnecting message.
- Do not keep a local clock running during timing loss or reconnect.
- Wait for real server timing to return before resuming transport display and sync behavior.
- Recovery should remain honest about missing timing; no fake beat or interval countdown should be shown while authoritative timing is unavailable.

### Timing display
- Healthy sync UI should prioritize current beat plus interval progress.
- Keep the existing progress bar, but present it as beat-divided interval progress rather than one undifferentiated long sweep.
- Interval rollover should be subtle rather than flashy; the boundary should be understandable without an aggressive visual effect.
- Beat-1/downbeat should still remain legible within the beat-and-progress presentation.

### Metronome role
- The metronome is an important sync aid in Phase 3 and should remain user-controllable.
- Phase 3 only requires simple on/off control for the metronome, aligned to server timing.
- Expanded metronome routing or mix controls are not required in this phase.

### Claude's Discretion
- Exact wording of waiting/reconnecting/timing-lost messages, as long as they stay explicit and concise.
- Exact visual treatment for beat-divided progress presentation.
- Exact handling of "ready exactly on the boundary" in implementation, as long as anything arriving after the boundary is treated as late.

</decisions>

<specifics>
## Specific Ideas

- "Always preserve alignment. If Bob's audio needs to wait until the next interval starts, this is okay and how Ninjam should work."
- "If a user's interval is late don't play it. It probably means a network error or the user has disconnected."
- The desired behavior is intentionally strict and Ninjam-like: no mid-interval insertion, no late catch-up playback.
- Sync status should be explicit and concise in the main UI; richer diagnostics can exist separately later if needed.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/plugin/FamaLamaJamAudioProcessor.cpp`: already derives interval length from server BPM/BPI, tracks beat/progress UI state, renders the metronome, and queues remote interval audio by source.
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`: already exposes a transport label, interval progress bar, and metronome toggle in the existing UI surface.
- `src/net/FramedSocketTransport.cpp`: already parses server timing config updates that can drive authoritative transport behavior.
- `tests/integration/plugin_experimental_transport_tests.cpp`: already contains a mini Ninjam-server harness that can be extended for timing, boundary, and reconnection validation.

### Established Patterns
- Phase 2 established that status messaging should be concise, actionable, and centered in the existing single-line status surfaces.
- Connection lifecycle and reconnect handling already treat the server as the source of truth for session state; timing should follow the same principle.
- Current transport UI already falls back to "waiting for server timing" when timing is absent, which aligns with the chosen behavior.

### Integration Points
- Phase 3 timing logic should integrate with the existing lifecycle snapshot and reconnect states so timing loss and reconnect messaging stay coherent.
- Remote interval promotion/mixing in `FamaLamaJamAudioProcessor.cpp` is the key place to enforce boundary-quantized playback and skipping of late intervals.
- Editor transport rendering in `FamaLamaJamAudioProcessorEditor.cpp` is the natural place to show beat-divided progress and timing recovery states.

</code_context>

<deferred>
## Deferred Ideas

- Separate debug/diagnostics window for deeper timing details.
- Expanded metronome level, pan, solo, or routing controls.
- Broader monitor/mix controls outside the core sync aid role.

</deferred>

---

*Phase: 03-server-authoritative-timing-sync*
*Context gathered: 2026-03-14*
