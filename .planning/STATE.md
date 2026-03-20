---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: Blocked
stopped_at: Completed 08.2-06-PLAN.md
last_updated: "2026-03-20T12:05:39.7677132+00:00"
last_activity: 2026-03-20 - Completed Phase 08.2 plan 06 closeout docs; Ableton auth rerun passed, and the phase remains blocked only by the still-unexercised CPU repro.
progress:
  total_phases: 12
  completed_phases: 8
  total_plans: 32
  completed_plans: 30
  percent: 94
---

# Project State

## Project Reference

See: `.planning/PROJECT.md`

**Core value:** Musicians can reliably join and complete real Ninjam rehearsals directly inside Ableton using a stable VST3 plugin workflow.  
**Current focus:** Capture the remaining same-machine Ableton CPU repro evidence for Phase 08.2, then close or carry forward that single host-performance blocker honestly before Phase 9.

## Current Position

Phase: 08.2 - Pre-Layout CPU, Mixer, UI, and Auth Hardening  
Plan: 06 - Final closeout docs recorded the Ableton auth rerun as passing; only the same-machine CPU follow-up still blocks phase closeout.  
Status: Blocked  
Last activity: 2026-03-20 - Completed Phase 08.2 plan 06 closeout docs; Ableton auth rerun passed, and the phase remains blocked only by the still-unexercised CPU repro.

Progress: 94%

## Accumulated Context

### Decisions

- Phase 3 timing is server-authoritative with no fake local fallback during reconnect or timing loss.
- Remote interval playback is boundary-quantized: ready sources switch together, late intervals are skipped, and reconnect clears stale remote playback before realignment.
- Experimental streaming now uploads full authoritative intervals, not opportunistic latest blocks, so remote interval semantics match the sync model.
- Transport UI sync health is explicit: disconnected, waiting for timing, healthy, reconnecting, and timing lost.
- Phase 5 is closed as a validated v1 rehearsal baseline, not as a declaration that the whole product is feature-complete.
- The next milestone order is fixed: host sync assist research first, then chat/voting, then server discovery/history, then layout refresh, then advanced parity research.
- Phase 6 is locked around an explicit one-shot "Arm Sync to Ableton Play" workflow with real room timing required and arming blocked when Ableton tempo does not match room BPM.
- [Phase 06]: Keep host sync assist read-only and one-shot - Phase 6 proves host-start alignment without implying generic Ableton tempo or loop control.
- [Phase 06]: Treat armed waiting and failed starts as explicit transport hold states - This preserves the server-authoritative model and prevents silent fallback after missing host timing data.
- [Phase 06]: Use a conservative 0.05 BPM tolerance for host tempo matching - Host tempo floats can vary slightly, but the arm gate still needs to stay strict and predictable.
- [Phase 06]: Derive armed, blocked, and failed wording from processor-owned snapshot data rather than editor-local sync logic.
- [Phase 06]: Preserve a short canceled message as transient UI feedback after the user disarms the assist.
- [Phase 06]: Keep host sync assist as a secondary transport-area action with explicit room target copy.
- [Phase 06]: Phase 6 closes with a narrow read-only Ableton host-start assist, not generic DAW transport control.
- [Phase 06]: Untested Ableton timing-loss and failed-start paths stay documented as residual risk instead of being inferred as passed.
- [Phase 07]: Keep room commands on a typed `MESSAGE_CHAT_MESSAGE` path separate from upload interval framing.
- [Phase 07]: Drain room events into processor-owned session-only state outside `processBlock()` so chat/topic parsing never lands on the realtime path.
- [Phase 07]: Preserve empty-author room notices in the mixed feed and classify vote-related system feedback instead of dropping unknown server text.
- [Phase 07]: Keep the Phase 7 room workflow inside the single-page editor with a compact feed viewport instead of tabs or a sidebar.
- [Phase 07]: Validate direct BPM/BPI room votes in the editor at 40-400 BPM and 2-64 BPI before dispatching processor commands.
- [Phase 07]: Return vote-success copy to neutral immediately and let failure copy decay after a short refresh window so inline room status stays readable.
- [Phase 07]: Close the phase with explicit residual-risk notes rather than blocking on unverified non-initiator vote-opposition semantics.
- [Phase 07]: Treat room-section mixer crowding as layout-refresh debt, not as a Phase 7 blocker, because the current workflow remained readable and functional in Ableton.
- [Phase 08.1]: Prefer JamTaba-style structured discovery data from `ninbot.com/app/servers.php` when available, with the old `serverlist.php` text feed kept only as fallback.
- [Phase 08.1]: Sort only the public-room section by active non-bot users descending while keeping remembered entries grouped first.
- [Phase 08.1]: Restore discovery selection by endpoint key, not row index, so remembered insertion and reordering cannot scramble the picker label.
- [Phase 08.2]: Keep password persistence inside SessionSettings serialization and default missing password fields to empty on restore.
- [Phase 08.2]: Reuse MiniNinjamServer as the controlled auth harness for right-password and wrong-password outcomes.
- [Phase 08.2]: Keep the interim layout deterministic with a fixed-width right sidebar instead of starting the broader Phase 9 redesign early.
- [Phase 08.2]: Surface authentication failures in a dedicated inline connection label while preserving the broader lifecycle status copy.
- [Phase 08.2]: Treat master output as processor-owned post-mix state and seed new strips from explicit unity defaults after removing the dead top-level controls.
- [Phase 08.2]: Diagnose periodic spike contributors with deterministic editor and processor work counters instead of wall-clock timing guesses.
- [Phase 08.2]: Reduce unchanged room-feed and mixer timer work before touching server-authoritative timing or audio semantics.
- [Phase 08.2]: Treat the remaining every-four-beat spike risk as mixed until final Ableton validation confirms how much residual behavior is machine-sensitive.
- [Phase 08.2]: Treat a real-host correct-password private-room auth failure as a hard blocker even when the controlled automation path is green.
- [Phase 08.2]: Leave the wrong-password inline error and every-four-beat CPU repro explicitly unverified when the manual pass stops early; do not infer them from automation.
- [Phase 08.2]: Keep passworded auth on the applied username instead of forcing anonymous-mode protocol usernames.
- [Phase 08.2]: Expose processor-owned live auth attempt snapshots so integration tests can distinguish rewritten usernames from real credential failures.
- [Phase 08.2]: Treat the 2026-03-20 Ableton rerun as enough to close `P082-SC4`, but do not close the phase until `P8.2-CPU-05` is exercised on the original repro machine.

### Roadmap Evolution

- Phase 08.1 inserted after Phase 8: Server Discovery Polish & JamTaba Parity Check (URGENT)
- Phase 08.2 inserted after Phase 8: Pre-Layout CPU, Mixer, UI, and Auth Hardening (URGENT)

### Pending Todos

- Keep using the verified `build-vs` validation path until local Ninja reliability is revisited.
- Run the every-four-beat Ableton repro on the same machine to judge how much CPU risk remains after the editor-churn mitigation.

### Blockers/Concerns

- Local Windows Ninja/CMake remains unreliable on this machine; use the verified `build-vs` path for now.
- Phase 6 validated the healthy-path Ableton host-start workflow, but the timing-loss cancellation and failed-start re-arm paths were not manually exercised in Ableton.
- Phase 7 verified BPM/BPI voting end-to-end, but non-initiator vote-against / vote-no semantics still need explicit real-server validation.
- Phase 08.2 remains blocked only by host CPU evidence: the 2026-03-20 Ableton rerun closed the real-host auth gap, but `P8.2-CPU-05` is still `not exercised`.
- `P082-SC1` still lacks same-machine host evidence for whether the original every-four-beat CPU spike is materially reduced or still disruptive after the editor-churn mitigation.

### Performance Metrics

- 2026-03-16: Completed Phase 06 plan 02 in 10 min across 3 task commits and 6 modified files.
- 2026-03-16: Completed Phase 06 plan 03 in 28 min across 3 task commits and 6 modified files.
- 2026-03-17: Completed Phase 07 plan 01 in 11 min across 2 task commits and 7 modified files.
- 2026-03-17: Completed Phase 07 plan 02 in 18 min across 3 task commits and 3 modified files.
- 2026-03-17: Completed Phase 07 plan 03 in 177 min across 3 task commits and 6 modified files.
- 2026-03-19: Completed Phase 08.2 plan 01 in 4 min across 2 task commits and 9 modified files.
- 2026-03-19: Completed Phase 08.2 plan 02 in 14 min across 2 task commits and 8 modified files.
- 2026-03-19: Completed Phase 08.2 plan 03 in 14 min across 2 task commits and 7 modified files.
- 2026-03-19: Completed Phase 08.2 plan 04 in 22 min across 2 task commits and 5 modified files; the phase remained blocked by manual private-room auth failure and deferred CPU follow-up.
- 2026-03-19: Completed Phase 08.2 plan 05 in 23 min across 2 task commits and 5 modified files; automated coverage now proves passworded auth preserves the applied username, but the final Ableton rerun is still pending.

## Session Continuity

Last session: 2026-03-19T20:24:41.118Z
Stopped at: Completed 08.2-05-PLAN.md
Resume file: None
