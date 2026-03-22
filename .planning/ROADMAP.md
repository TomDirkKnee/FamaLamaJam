# Roadmap: FamaLamaJam

## Overview

The first milestone delivered a validated Windows-first rehearsal baseline. The next milestone extends that stable core into a more complete collaboration client by tackling Ableton sync assist research, room interaction features, server discovery/history, a stronger JamTaba-inspired layout, and targeted NINJAM parity investigation.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

- [x] **Phase 1: Plugin Foundation & Session Configuration** - Establish JUCE/VST3 baseline, core session settings model, and persistence scaffolding. (completed 2026-03-08)
- [x] **Phase 2: Connection Lifecycle & Recovery** - Implement robust connect/disconnect and automatic reconnect behavior. (completed 2026-03-09)
- [x] **Phase 3: Server-Authoritative Timing & Sync** - Deliver accurate Ninjam interval timing and metronome state behavior. (completed 2026-03-14)
- [x] **Phase 4: Audio Streaming, Mix, and Monitoring Core** - Build functional send/receive audio pipeline with essential channel controls. (completed 2026-03-15)
- [x] **Phase 5: Ableton Reliability & v1 Rehearsal UX Validation** - Harden host lifecycle behavior and validate complete user jam workflow. (completed 2026-03-16)
- [x] **Phase 6: Ableton Sync Assist Research & Prototype** - Research JamTaba/ReaNINJAM host-sync behavior and prototype a safe one-shot Ableton host-start assist. (completed 2026-03-16)
- [x] **Phase 7: Chat & Room Control Commands** - Add room chat plus BPM/BPI voting controls and feedback. (completed 2026-03-17)
- [x] **Phase 8: Server Discovery & History** - Add public server discovery plus remembered private server history. (completed 2026-03-19)
- [x] **Phase 08.1: Server Discovery Polish & JamTaba Parity Check** - Correct discovery-count fidelity, ordering, and picker stability before the larger layout work. (completed 2026-03-19)
- [ ] **Phase 08.2: Pre-Layout CPU, Mixer, UI, and Auth Hardening (INSERTED)** - Sidebar, mixer, and private-room auth reruns now pass in Ableton, but the original same-machine every-four-beat CPU repro is still unexercised, so Phase 9 remains blocked on that narrow host-evidence gap.
- [x] **Phase 08.3: Functional Release Controls, Session UX, and Voice-Mode Compatibility Guard (INSERTED)** - Completed 2026-03-21 after the brand-new-instance private-room recall rerun passed in Ableton, with the local `build-vs` NMake caveat still documented separately.
- [ ] **Phase 9: JamTaba-Inspired Layout & Mixer Parity** - Refresh the plugin layout with horizontal strips, integrated chat, and mixer parity features like solo.
- [ ] **Phase 10: Advanced NINJAM Parity Research** - Investigate room listen/live-feed behavior, voice chat mode, and other high-value parity features.

## Phase Details

### Phase 1: Plugin Foundation & Session Configuration
**Goal**: Create a stable plugin baseline with configurable Ninjam endpoint/credentials and project state persistence for core settings.
**Depends on**: Nothing (first phase)
**Requirements**: SESS-01, HOST-02
**Success Criteria** (what must be TRUE):
1. User can enter/edit Ninjam server endpoint and credentials from the plugin UI and values are validated before use.
2. Core session settings (at minimum endpoint, username, and basic channel defaults) are saved and restored with Ableton project state.
3. Plugin initializes and reloads with persisted settings without requiring manual re-entry after project reopen.
4. Configuration changes are applied through a non-blocking control path that does not disrupt host audio processing.
**Plans**: TBD

Plans:
- [x] 01-01: Establish JUCE/VST3 project scaffold and core session settings model.
- [x] 01-02: Implement settings UI and parameter/state serialization.

### Phase 2: Connection Lifecycle & Recovery
**Goal**: Provide reliable session connect/disconnect behavior and resilient reconnect handling under transient failures.
**Depends on**: Phase 1
**Requirements**: SESS-02, SESS-03
**Success Criteria** (what must be TRUE):
1. User can connect to and disconnect from a Ninjam server repeatedly in a single Ableton session without restart.
2. On transient network interruption, reconnect attempts start automatically within configured retry policy.
3. Session state transitions are explicit (idle/connecting/active/reconnecting/error) and reflected in UI status.
4. Reconnection recovers active session flow without requiring plugin reload when credentials and endpoint remain valid.
**Plans**: TBD

Plans:
- [x] 02-01: Implement session state machine and command handling for connect/disconnect.
- [x] 02-02: Add reconnect policy, retry/backoff behavior, and failure-state signaling.
- [x] 02-03: Finalize error-correction and restore semantics.

### Phase 3: Server-Authoritative Timing & Sync
**Goal**: Implement correct Ninjam interval timing semantics and visible sync state for stable rehearsal timing.
**Depends on**: Phase 2
**Requirements**: SYNC-01, SYNC-02, SYNC-03
**Success Criteria** (what must be TRUE):
1. Plugin follows server-authoritative BPM/BPI/interval-boundary updates without local timing divergence.
2. Current interval/metronome state is visible in the UI and updates consistently at boundary transitions.
3. Timing remains stable over a full rehearsal-length validation run with no cumulative interval drift.
4. Sync recovery after reconnect re-aligns interval state to server timing without manual user intervention.
5. Remote participant interval audio switches at shared server boundary points (no arrival-time staggering between users).
**Plans**: 3 plans

Plans:
- [x] 03-01-PLAN.md - Implement the authoritative timing engine, timing reset semantics, and shared Phase 3 mini-server harness.
- [x] 03-02-PLAN.md - Add explicit sync-health UI state, beat-divided transport progress, and metronome alignment coverage.
- [x] 03-03-PLAN.md - Enforce boundary-quantized remote playback switching and verify late-interval skip behavior.

### Phase 4: Audio Streaming, Mix, and Monitoring Core
**Goal**: Deliver working Ninjam-compatible audio send/receive pipeline with essential monitor and mix controls.
**Depends on**: Phase 3
**Requirements**: AUD-01, AUD-02, AUD-03, MIX-01, MIX-02
**Success Criteria** (what must be TRUE):
1. Local input is encoded and sent to the session using a Ninjam-compatible encode path.
2. Remote participant streams are decoded and rendered in-session with audible continuity.
3. Supported host sample rates and buffer sizes (including runtime changes) play without pitch or tempo artifacts.
4. User can control per-channel gain, mute, and basic pan for essential monitoring/mix tasks.
5. UI clearly distinguishes local monitor path from interval-delayed remote return so users can make correct mix decisions.
**Plans**: 3 plans

Plans:
- [x] 04-01: Implement RT-safe send/receive audio buffering and codec integration.
- [x] 04-02: Build essential per-channel monitor/mix controls and routing distinctions.
- [x] 04-03: Validate sample-rate/buffer compatibility and artifact-free playback.

### Phase 5: Ableton Reliability & v1 Rehearsal UX Validation
**Goal**: Ensure host-lifecycle robustness and confirm end-to-end rehearsal usability in Ableton with actionable status/error feedback.
**Depends on**: Phase 4
**Requirements**: HOST-01, UI-01, UI-02
**Success Criteria** (what must be TRUE):
1. Plugin survives Ableton lifecycle events (load/unload/reopen/duplicate) without crashes, leaks, or zombie sessions.
2. User can complete end-to-end jam setup and begin playing from the built-in JUCE UI without external tooling.
3. Status and error messages for connection, sync, and streaming failures are actionable and specific enough for user recovery.
4. v1 rehearsal validation passes on Windows in Ableton across representative rehearsal scenarios.
**Plans**: 3 plans

Plans:
- [x] 05-01-PLAN.md - Add Ableton lifecycle hardening and cleanup guarantees.
- [x] 05-02-PLAN.md - Finalize simple end-to-end UI flow and error/status message quality.
- [x] 05-03-PLAN.md - Execute v1 rehearsal validation matrix and document outcomes.

### Phase 6: Ableton Sync Assist Research & Prototype
**Goal**: Determine what host-sync behavior is actually feasible in Ableton and prototype a safe one-shot host-start helper workflow that respects server-authoritative Ninjam timing.
**Depends on**: Phase 5
**Requirements**: HSYNC-01, HSYNC-02
**Success Criteria** (what must be TRUE):
1. Project documents how JamTaba/ReaNINJAM-style host-sync behavior works and what parts are transferable to Ableton.
2. Plugin can prototype at least one optional host-start sync assist action that aligns from Ableton musical position when explicitly armed.
3. The feature is framed as an assistive workflow, not as a replacement for server-authoritative interval timing.
4. UX and technical constraints are clear enough to decide whether fuller host-sync work should continue.
**Plans**: 3 plans

Plans:
- [x] 06-01-PLAN.md - Add processor-side host playhead observation, armed sync state, and injected playhead regression coverage.
- [x] 06-02-PLAN.md - Add transport-adjacent arm or cancel UI, blocked explanations, and explicit sync-assist status messaging.
- [x] 06-03-PLAN.md - Run automated plus Ableton validation and capture the Phase 6 feasibility decision.

### Phase 7: Chat & Room Control Commands
**Goal**: Let users participate in room communication and room-level BPM/BPI voting directly from the plugin.
**Depends on**: Phase 6
**Requirements**: ROOM-01, ROOM-02
**Success Criteria** (what must be TRUE):
1. User can send and receive room chat from inside the plugin.
2. User can issue BPM and BPI vote commands from the UI without manual slash-command typing.
3. Chat and vote feedback remain understandable and visible during normal rehearsal use.
4. The implementation preserves realtime safety and does not destabilize the audio/session core.
**Plans**: 3 plans

Plans:
- [x] 07-01-PLAN.md - Add room-message transport support, vote command plumbing, and processor-owned session-only room state.
- [x] 07-02-PLAN.md - Add the compact room feed, always-visible composer, and direct BPM/BPI vote controls to the current editor.
- [x] 07-03-PLAN.md - Run automated plus Ableton validation and record the Phase 7 verification outcome.

### Phase 8: Server Discovery & History
**Goal**: Improve room entry workflow with public server discovery and remembered private server history.
**Depends on**: Phase 7
**Requirements**: DISC-01, DISC-02
**Success Criteria** (what must be TRUE):
1. User can choose from a public Ninjam server list without typing raw host/port every time.
2. Previously used private servers are remembered and available in the same workflow.
3. Discovery/history UI does not make the existing direct host/port workflow worse for power users.
4. Failure states for fetching/parsing the public server list are explicit and recoverable.
**Plans**: 3 plans

Plans:
- [x] 08-01-PLAN.md - Add remembered-server persistence, parser/history helpers, and processor-owned discovery state.
- [x] 08-02-PLAN.md - Implement public discovery refresh, combined picker UI, and stale/failure behavior.
- [x] 08-03-PLAN.md - Run automated plus Ableton validation and record the Phase 8 verification outcome.

### Phase 08.1: Server Discovery Polish & JamTaba Parity Check (INSERTED)

**Goal**: Correct the public-room discovery details that still undermine trust after Phase 8: user-count fidelity, busiest-room ordering, and picker selection stability.
**Depends on**: Phase 8
**Requirements**: DISC-01, DISC-02
**Success Criteria** (what must be TRUE):
1. Public room rows display current-user information from a richer discovery model than the previous raw `1/8` text scrape whenever better source data is available.
2. Remembered servers still appear first, but public rooms are ordered by active users descending rather than raw feed order.
3. The discovery picker preserves selection by endpoint across refresh and connect flows instead of jumping to another server row.
4. The phase fixes discovery correctness issues without pretending to solve the broader cramped-layout problem that remains Phase 9 work.
**Plans**: 3 plans

Plans:
- [x] 08.1-01-PLAN.md - Upgrade public-room count semantics and busiest-room ordering with JamTaba-informed discovery modeling.
- [x] 08.1-02-PLAN.md - Fix endpoint-stable picker selection and surface clearer public-room labels without harming manual connect.
- [x] 08.1-03-PLAN.md - Run the discovery-polish gate, manual Ableton matrix, and close the inserted follow-up phase.

### Phase 08.2: Pre-Layout CPU, Mixer, UI, and Auth Hardening (INSERTED)

**Goal**: Resolve the newly surfaced functional issues that make the current build hard to rehearse with before the larger JamTaba-inspired layout phase begins.
**Depends on**: Phase 08.1
**Requirements**: P082-SC1, P082-SC2, P082-SC3, P082-SC4
**Success Criteria** (what must be TRUE):
1. CPU spikes around interval or beat boundaries are investigated, reproduced, and materially reduced versus the current build.
2. The chat area no longer obscures the mixer or stream-monitoring workflow in practical Ableton window sizes.
3. Non-working `Default Gain`, `Default Pan`, and `Default Muted` controls are removed, and a clear master output control exists inside the mixer section.
4. Password/authentication workflow is visible in the UI and validated against a local or controlled test server path.
**Plans**: 6 plans

**Current status (2026-03-20):** Plan 08.2-04 recorded the blocked real-host outcome, plan 08.2-05 closed the unexplained auth mismatch in automation, and plan 08.2-06 confirmed the corrected private-room auth rerun plus wrong-password inline behavior in Ableton. Phase 08.2 remains blocked only because the original every-four-beat CPU repro is still unexercised on the same machine, so Phase 9 is not yet unblocked.

Plans:
- [x] 08.2-01-PLAN.md - Add password/auth and master-output state seams plus controlled Wave 0 coverage.
- [x] 08.2-02-PLAN.md - Move the room workflow into a fixed right sidebar and finish mixer/auth UI cleanup.
- [x] 08.2-03-PLAN.md - Diagnose and reduce periodic CPU spikes with measurement-first hardening.
- [x] 08.2-04-PLAN.md - Captured the manual Ableton/private-room validation; the phase remains blocked by the failing correct-password auth path and deferred CPU follow-up.
- [x] 08.2-05-PLAN.md - Investigate and fix the live private-room auth handoff so the real plugin window matches the controlled auth harness.
- [x] 08.2-06-PLAN.md - Rebuilt the host-validation artifact, recorded the Ableton auth rerun as passing, and kept Phase 08.2 blocked only on the still-unexercised CPU repro.

### Phase 08.3: Functional Release Controls, Session UX, and Voice-Mode Compatibility Guard (INSERTED)

**Goal**: Finish the remaining release-critical controls and single-page session polish before the broader Phase 9 layout overhaul, including remembered private-room recall, explicit local transmit lifecycle, mixer usability refinements, and safe unsupported voice-mode handling.
**Depends on:** Phase 08.2
**Requirements**: P083-01, P083-02, P083-03, P083-04, P083-05, P083-06, P083-07, P083-08
**Success Criteria** (what must be TRUE):
1. User can recall a previously successful private-room connection from the combined picker with host, username, and reusable masked password restored without manual re-entry.
2. The local strip exposes an explicit transmit control with red, amber, and green lifecycle feedback, and turning transmit off stops only local upload while the rest of the session keeps running.
3. Mixer and session polish improve practical use in the current single-page editor: multi-solo works, diagnostics and server settings can collapse, host sync assist stays inline and explicit, the room feed wraps cleanly, and the interval bar stays fixed as a full-width footer.
4. Remote channels flagged for unsupported voice mode are not mixed as interval audio and are surfaced clearly in normal mixer or session UI instead of failing silently.
5. The phase closes with targeted automation plus a focused Ableton smoke pass, without starting the broader Phase 9 redesign or implementing full voice chat.
**Plans:** 7/7 plans executed

Plans:
- [x] 08.3-01-PLAN.md - Added Wave 0 transmit/voice-guard test scaffolds and shared strip-state contracts.
- [x] 08.3-02-PLAN.md - Implemented remembered private-server credential persistence, picker recall, and masked password reuse.
- [x] 08.3-03-PLAN.md - Added the local-strip transmit lifecycle and upload-only gating.
- [x] 08.3-04-PLAN.md - Added multi-solo, unsupported voice-mode guard, and the remaining single-page session UX polish.
- [x] 08.3-05-PLAN.md - Manual Ableton smoke pass recorded four passes and one remaining gap: private-room recall is not yet global across new plugin instances.
- [x] 08.3-06-PLAN.md - Add a small app-level remembered-server store and merge it with the existing project-state recall path.
- [x] 08.3-07-PLAN.md - Re-ran the focused 08.3 gate, recorded the approved brand-new-instance recall rerun, and closed the phase without reopening unrelated scope.

### Phase 08.3.1: Private Server Recall + Compact Session UI Polish (INSERTED)

**Goal:** Preserve rich Ninbot public discovery rows while remembered credentials remain convenient, and compact the current session/chat UI without starting the deferred mixer redesign.
**Requirements**: DISC-02, LAYOUT-03
**Depends on:** Phase 08.3
**Success Criteria** (what must be TRUE):
1. Public Ninbot rows remain the visible primary discovery rows even when remembered credentials exist for the same `host:port`.
2. Selecting a public row for a previously successful endpoint restores remembered username/password behind the scenes, while failed attempts do not overwrite the last known good credentials.
3. The compact session area reads cleanly in one page: concise connection summary, concise BPM/BPI timing line, and sync-assist guidance carried by the button itself.
4. The room sidebar wastes less space: a shared vote action, hidden-by-default diagnostics that can take over the sidebar, and chat follow behavior that only auto-scrolls when already near the bottom.
5. The phase explicitly avoids the deferred mixer redesign and hands broader strip/layout work forward to the later phase.
**Plans:** 1/3 plans executed

Plans:
- [ ] 08.3.1-01-PLAN.md - Keep Ninbot public rows primary while overlaying remembered credentials by endpoint.
- [ ] 08.3.1-02-PLAN.md - Compact the top/session area and room sidebar inside the existing single-page editor.
- [ ] 08.3.1-03-PLAN.md - Run the focused gate, rebuild the VST3, and close the inserted polish phase.

### Phase 9: JamTaba-Inspired Layout & Mixer Parity
**Goal**: Evolve the plugin UI toward a more ergonomic collaboration layout while preserving the current validated workflow.
**Depends on**: Phase 08.3.1
**Requirements**: LAYOUT-01, LAYOUT-02, LAYOUT-03
**Success Criteria** (what must be TRUE):
1. Mixer strips are presented horizontally with the local monitor clearly distinguished and visually anchored.
2. A right-side chat/room panel coexists cleanly with transport and mixer workflows.
3. Mixer parity improves with solo support and clearer strip grouping/identity.
4. The refreshed layout remains usable on typical Ableton plugin window sizes.
**Plans**: TBD

### Phase 10: Advanced NINJAM Parity Research
**Goal**: Research and scope higher-risk NINJAM features before implementation commitments are made.
**Depends on**: Phase 9
**Requirements**: PAR-01, PAR-02
**Success Criteria** (what must be TRUE):
1. Project documents whether room-listen/live-feed behavior exists in upstream NINJAM and how it could map to this plugin.
2. Project documents how voice chat mode works upstream and whether it should share transport, mixer, or UI concepts with the current plugin.
3. Risks, protocol implications, and likely UX shape are clear enough to decide on a future implementation phase.
**Plans**: TBD

## Requirement Coverage Validation

| Requirement | Mapped Phase |
|-------------|--------------|
| SESS-01 | Phase 1 |
| SESS-02 | Phase 2 |
| SESS-03 | Phase 2 |
| SYNC-01 | Phase 3 |
| SYNC-02 | Phase 3 |
| SYNC-03 | Phase 3 |
| AUD-01 | Phase 4 |
| AUD-02 | Phase 4 |
| AUD-03 | Phase 4 |
| MIX-01 | Phase 4 |
| MIX-02 | Phase 4 |
| HOST-01 | Phase 5 |
| HOST-02 | Phase 1 |
| UI-01 | Phase 5 |
| UI-02 | Phase 5 |
| HSYNC-01 | Phase 6 |
| HSYNC-02 | Phase 6 |
| ROOM-01 | Phase 7 |
| ROOM-02 | Phase 7 |
| DISC-01 | Phase 8 |
| DISC-02 | Phase 8 / Phase 08.1 / Phase 08.3 / Phase 08.3.1 |
| LAYOUT-01 | Phase 9 |
| LAYOUT-02 | Phase 08.3 / Phase 9 |
| LAYOUT-03 | Phase 08.2 / Phase 08.3 / Phase 08.3.1 / Phase 9 |
| PAR-01 | Phase 10 |
| PAR-02 | Phase 10 |

**Coverage Check:**
- v1 requirements total: 15
- v1 requirements mapped: 15
- Unmapped: 0
- Coverage: 100%

## Progress

**Execution Order:**
1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> 8 -> 8.1 -> 8.2 -> 8.3 -> 8.3.1 -> 9 -> 10

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Plugin Foundation & Session Configuration | 0/TBD | Complete    | 2026-03-08 |
| 2. Connection Lifecycle & Recovery | 3/3 | Complete    | 2026-03-09 |
| 3. Server-Authoritative Timing & Sync | 3/3 | Complete    | 2026-03-14 |
| 4. Audio Streaming, Mix, and Monitoring Core | 3/3 | Complete    | 2026-03-15 |
| 5. Ableton Reliability & v1 Rehearsal UX Validation | 3/3 | Complete    | 2026-03-16 |
| 6. Ableton Sync Assist Research & Prototype | 3/3 | Complete    | 2026-03-16 |
| 7. Chat & Room Control Commands | 3/3 | Complete    | 2026-03-17 |
| 8. Server Discovery & History | 3/3 | Complete    | 2026-03-19 |
| 8.1. Server Discovery Polish & JamTaba Parity Check | 3/3 | Complete    | 2026-03-19 |
| 8.2. Pre-Layout CPU, Mixer, UI, and Auth Hardening | 6/6 | Blocked | - |
| 8.3. Functional Release Controls, Session UX, and Voice-Mode Compatibility Guard | 7/7 | Complete | 2026-03-21 |
| 8.3.1. Private Server Recall + Compact Session UI Polish | 0/3 | Planned | - |
| 9. JamTaba-Inspired Layout & Mixer Parity | 0/TBD | Planned     | - |
| 10. Advanced NINJAM Parity Research | 0/TBD | Planned     | - |

---
*Roadmap created: 2026-03-08*


