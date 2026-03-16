# Roadmap: FamaLamaJam

## Overview

Deliver a reliable Windows-first JUCE 8 VST3 Ninjam client for Ableton by building the realtime/plugin foundation first, then connection and timing correctness, then audio/mix capabilities, and finally host-hardening plus end-to-end rehearsal usability validation.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

- [x] **Phase 1: Plugin Foundation & Session Configuration** - Establish JUCE/VST3 baseline, core session settings model, and persistence scaffolding. (completed 2026-03-08)
- [x] **Phase 2: Connection Lifecycle & Recovery** - Implement robust connect/disconnect and automatic reconnect behavior. (completed 2026-03-09)
- [x] **Phase 3: Server-Authoritative Timing & Sync** - Deliver accurate Ninjam interval timing and metronome state behavior. (completed 2026-03-14)
- [x] **Phase 4: Audio Streaming, Mix, and Monitoring Core** - Build functional send/receive audio pipeline with essential channel controls. (completed 2026-03-15)
- [x] **Phase 5: Ableton Reliability & v1 Rehearsal UX Validation** - Harden host lifecycle behavior and validate complete user jam workflow. (completed 2026-03-16)

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

**Coverage Check:**
- v1 requirements total: 15
- v1 requirements mapped: 15
- Unmapped: 0
- Coverage: 100%

## Progress

**Execution Order:**
1 -> 2 -> 3 -> 4 -> 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Plugin Foundation & Session Configuration | 0/TBD | Complete    | 2026-03-08 |
| 2. Connection Lifecycle & Recovery | 3/3 | Complete    | 2026-03-09 |
| 3. Server-Authoritative Timing & Sync | 3/3 | Complete    | 2026-03-14 |
| 4. Audio Streaming, Mix, and Monitoring Core | 3/3 | Complete    | 2026-03-15 |
| 5. Ableton Reliability & v1 Rehearsal UX Validation | 3/3 | Complete    | 2026-03-16 |

---
*Roadmap created: 2026-03-08*


