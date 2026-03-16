# Phase 5: Ableton Reliability & v1 Rehearsal UX Validation - Context

**Gathered:** 2026-03-15
**Status:** Ready for planning

<domain>
## Phase Boundary

Ensure the plugin survives real Ableton lifecycle events, supports a simple end-to-end jam workflow from the built-in JUCE UI, and communicates connection/sync/streaming problems with clear recovery guidance. This phase hardens and validates the existing v1 feature set; it does not add new audio capabilities, advanced diagnostics, or post-v1 UX expansion.

</domain>

<decisions>
## Implementation Decisions

### Ableton lifecycle hardening
- Save/close/reopen of an Ableton set is the highest-priority Phase 5 lifecycle case.
- After reopening a set, the plugin should restore settings and mix state but remain disconnected until the user explicitly presses Connect.
- When a track containing the plugin is duplicated, the duplicated instance should stay idle by default rather than auto-joining the session.
- Audio engine stop/start behavior in Ableton is part of the formal Phase 5 reliability scope and should recover cleanly without requiring plugin reload.

### v1 rehearsal flow
- Phase 5 should optimize for a single simple page in the built-in JUCE UI, not a multi-view workflow.
- The top of the UI should prioritize settings, Connect, and clear status so a user can join a room without hunting through the interface.
- The mixer should remain on the same page but be visually secondary to the connection/setup flow.
- Phase 5 should validate and polish the current built-in workflow rather than introduce external tooling or a fundamentally different interaction model.

### Status and error messaging
- Connection, sync, and streaming problems should use short plain-language messages with one clear recovery action.
- Important failure states should remain visible in a persistent status/banner treatment until the state changes.
- Stream-specific issues should explain both what happened and what the user should expect next, especially for missed or skipped interval audio.
- Main UI messaging should avoid technical protocol detail unless it is necessary for user recovery.

### Validation and sign-off
- Phase 5 verification should produce a practical checklist with pass/fail notes and recorded follow-up issues.
- The key v1 rehearsal validation target is a real small-group room in Ableton rather than only solo loopback or synthetic stress testing.
- Phase 5 is good enough to close once the required lifecycle and rehearsal scenarios pass and any residual rough edges are clearly documented.

### Claude's Discretion
- Exact wording and visual styling of the persistent status/banner treatment within the existing JUCE UI style.
- Exact ordering and spacing of same-page sections, as long as setup/status remains visually first.
- Exact manual validation checklist structure, provided it stays concise and practical.

</decisions>

<specifics>
## Specific Ideas

- Reopened or duplicated plugin instances should not surprise the user by silently auto-connecting to a room.
- The user wants to keep working from the built-in JUCE UI inside Ableton rather than relying on extra tools or a separate control surface.
- Status copy should feel honest and helpful: say what failed, whether playback or reconnect is waiting on the next interval, and what the musician should do next.
- Phase 5 should feel like final hardening and usability validation of the existing workflow, not a new feature phase.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/plugin/FamaLamaJamAudioProcessor.cpp`: already owns connection commands, transport/sync state, stream processing, mix state persistence, and host lifecycle entry points such as `prepareToPlay`, `releaseResources`, `getStateInformation`, and `setStateInformation`.
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`: already presents the single-page JUCE UI with settings, connect/disconnect controls, status text, transport state, metronome toggle, and mixer strip surface.
- `src/app/session/ConnectionLifecycleController.cpp`: already centralizes user-facing lifecycle/status strings and recovery guidance for connect/reconnect/error transitions.
- Existing integration tests already cover reconnect flows, state round-trip, sync-state UI, mixer UI behavior, and runtime sample-rate/block-size compatibility.

### Established Patterns
- Phase 3 established honest sync behavior: no fake local timing during reconnect, explicit waiting/reconnecting/timing-lost states, and late-interval skipping.
- Phase 4 established a single built-in mixer surface with local monitor plus remote strips keyed by `user + channel`.
- Current builds use the verified `build-vs` path on Windows; local Ninja reliability remains out of scope for this phase.

### Integration Points
- Lifecycle hardening work will likely connect processor cleanup/restore behavior with transport teardown, reconnect scheduling, and persisted state restoration.
- UX/message improvements should build on the current status label and transport/mixer layout rather than replacing the editor structure.
- Validation work should combine automated integration coverage with explicit manual Ableton rehearsal checks against the real VST3 build artifact.

</code_context>

<deferred>
## Deferred Ideas

- Advanced diagnostics or observability panels for deeper stream/network troubleshooting.
- New routing, patchbay, or other post-v1 mixer expansion.
- Automatic reconnect-on-reopen preference memory.
- Larger-scale stress-room validation beyond the practical small-group v1 rehearsal target.

</deferred>

---

*Phase: 05-ableton-reliability-v1-rehearsal-ux-validation*
*Context gathered: 2026-03-15*
