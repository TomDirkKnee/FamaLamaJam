# Phase 6: Ableton Sync Assist Research & Prototype - Context

**Gathered:** 2026-03-16
**Status:** Ready for planning

<domain>
## Phase Boundary

Determine what host-sync behavior is actually feasible in Ableton and prototype a safe helper workflow that respects server-authoritative Ninjam timing. This phase is centered on an explicit Ableton play-start sync assist, not on replacing the server timing model and not on promising full DAW transport control.

</domain>

<decisions>
## Implementation Decisions

### Host-sync assist behavior
- Phase 6 should center on an explicit host-play-start sync assist, not on loop-brace control as the primary interaction.
- The assist should behave like an armed one-shot action: the plugin waits for Ableton playback to start, then aligns from the host's current musical position.
- When Ableton starts from the middle of a bar, use offset alignment from the host musical position rather than forcing playback to bar 1 or the next bar.
- While armed and waiting for Ableton playback, hold Ninjam playback and metronome rather than letting interval playback continue in the background.
- After a successful synced start, the plugin should stay free-running rather than automatically returning to a waiting-for-host state on transport stop.

### Arming preconditions and failure behavior
- Arming is only valid when real server timing is present.
- Arming must be blocked unless Ableton's current tempo already matches the room BPM.
- If server timing is lost while armed, cancel the armed state rather than silently waiting.
- If the plugin cannot complete the aligned start when Ableton playback begins, fail visibly and stay stopped; do not fall back to a fake or best-effort start.
- After a failed start, retry should require explicit manual re-arming.

### Trigger flow
- Use a dedicated one-shot control such as "Arm Sync to Ableton Play".
- The same control should also disarm/cancel the waiting state if the user changes their mind before pressing play.
- Arming resets automatically after a successful synced start.
- This should not be framed as a persistent always-on sync mode.

### UI framing
- Place the host-sync assist near the existing transport/status area rather than burying it in general settings or creating a large separate panel.
- The control should be secondary but clear: visible and understandable without overpowering connect/status workflow.
- Room BPM/BPI should be shown near the control so the user can immediately understand the sync target.
- If Ableton tempo does not match the room BPM, the arm control should be disabled with a short inline explanation describing the required tempo.
- Armed state should use explicit banner/state text such as "Armed: press Play in Ableton to start aligned."

### Secondary helper scope
- Phase 6 may show room BPM/BPI clearly as supporting information.
- Do not promise host tempo push or loop-region manipulation as the primary outcome of this phase.
- Tempo/loop helper behaviors remain secondary research topics unless they emerge as safe and feasible during implementation research.

### Claude's Discretion
- Exact copy for armed, blocked, canceled, and failed-start status text, as long as it stays explicit and plain-language.
- Exact control label text and visual styling inside the current JUCE UI.
- Whether any secondary tempo-helper affordance is shown in prototype form, provided the host-play-start sync assist remains the clear primary behavior.

</decisions>

<specifics>
## Specific Ideas

- "Ableton's playhead usually snaps to the first beat of a bar, so it shouldn't really matter."
- "If the plugin can sense Ableton has started playing, and to immediately play the Ninjam loop. I think this is how JamTaba does it."
- The desired model is an explicit assist, not hidden background behavior.
- The user believes this armed host-start approach is the only realistic way Phase 6 can work inside Ableton.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/plugin/FamaLamaJamAudioProcessor.cpp`: already owns authoritative server timing, transport UI state, lifecycle state, and editor callbacks; this is the natural place to add host-playhead observation and armed sync state.
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`: already has a transport/status region and current room timing display patterns that can host the new assist control and banner messaging.
- `src/app/session/ConnectionLifecycleController.cpp`: already provides concise, user-facing state messaging patterns that Phase 6 should match.

### Established Patterns
- Phase 3 established that server timing remains authoritative and the plugin must not invent fake local timing during reconnect or timing loss.
- Phase 5 established that the main JUCE UI should stay single-page, with important status visible near the top of the workflow.
- The current plugin does not yet read host playhead/transport state through JUCE host-playhead APIs, so this is a new integration point for research and prototype work.

### Reference Implementations
- `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Plugins/JamTabaPlugin.h`: JamTaba detects host transport start by comparing current host-play state with the previous audio callback.
- `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Plugins/VST/JamTabaVSTPlugin.cpp`: JamTaba computes a start offset from `ppqPos` and `barStartPos` and triggers synchronized start when the host begins playback.
- `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Plugins/NinjamControllerPlugin.cpp`: JamTaba's plugin controller enters a waiting-for-host-sync state, suppresses Ninjam playback while waiting, and activates playback only when the host-start sync is completed.
- `ninjam-upstream/jmde/fx/reaninjam/winclient.cpp`: upstream ReaNINJAM exposes separate sync-helper actions like set tempo, set loop, and start playback on next loop, which are useful comparison points but not the chosen primary Phase 6 behavior.

### Integration Points
- Host transport detection will likely connect at the processor level via JUCE host-playhead APIs and feed into the existing transport/sync UI model.
- Armed sync state should integrate with the current authoritative timing state so the feature can block on real room timing and cancel cleanly on timing loss.
- UI work should extend the current transport/status surface rather than introducing a second independent sync workflow.

</code_context>

<deferred>
## Deferred Ideas

- Direct host tempo push or loop-region manipulation as a primary Phase 6 deliverable.
- Persistent always-on host sync mode.
- Broader DAW transport integration beyond the explicit one-shot armed start workflow.
- Any deeper JamTaba/JUCE parity work that turns out to require capabilities Ableton plugins cannot safely rely on.

</deferred>

---

*Phase: 06-ableton-sync-assist-research-prototype*
*Context gathered: 2026-03-16*
