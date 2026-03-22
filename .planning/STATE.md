---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: Planned
stopped_at: Completed 08.3.1-01-PLAN.md
last_updated: "2026-03-22T10:00:32.647Z"
last_activity: 2026-03-22 - Planned urgent follow-up Phase 08.3.1 to preserve rich public discovery rows while compacting the current session/chat UI before the broader Phase 9 redesign.
progress:
  total_phases: 14
  completed_phases: 10
  total_plans: 42
  completed_plans: 39
  percent: 93
---

# Project State

## Project Reference

See: `.planning/PROJECT.md`

**Core value:** Musicians can reliably join and complete real Ninjam rehearsals directly inside Ableton using a stable VST3 plugin workflow.  
**Current focus:** Phase 08.3.1 is planned and ready to execute for public-row credential overlay plus compact session/chat polish. Phase 08.2 still carries the separate same-machine CPU repro concern.

## Current Position

Phase: 08.3.1 - Private Server Recall + Compact Session UI Polish  
Plan: Ready for execution  
Status: Planned  
Last activity: 2026-03-22 - Planned urgent follow-up Phase 08.3.1 to preserve rich public discovery rows while compacting the current session/chat UI before the broader Phase 9 redesign.

Progress: 93%

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
- [Phase 08.3]: Keep remembered private-room persistence as a narrow processor-owned app-data store that merges wrapped project history instead of introducing a separate credential-management UX.
- [Phase 08.3]: Use the existing build-vs-2026 Visual Studio generator for verification when the local build-vs NMake tree blocks on U1076 name-too-long failures.
- [Phase 08.3]: Preserve the four previously passing manual rows as carried-forward evidence during the gap-close rerun unless the focused checkpoint reports a new regression.
- [Phase 08.3.1]: Keep credential overlay synthesis inside getServerDiscoveryUiState so cached public metadata stays untouched. — The editor already handles remembered password masking and draft hydration, so changing the merge layer preserves public-row metadata without adding a second credential path.
- [Phase 08.3.1]: Use temp remembered-server stores in discovery tests so AppData history cannot leak into deterministic regressions. — The rebuilt discovery suite was reading shared remembered history from the real store, so the RED expectations needed isolated temp stores to represent this plan only.

### Roadmap Evolution

- Phase 08.1 inserted after Phase 8: Server Discovery Polish & JamTaba Parity Check (URGENT)
- Phase 08.2 inserted after Phase 8: Pre-Layout CPU, Mixer, UI, and Auth Hardening (URGENT)
- Phase 08.3 inserted after Phase 08.2: Functional Release Controls, Session UX, and Voice-Mode Compatibility Guard (URGENT)
- Phase 08.3.1 inserted after Phase 08.3: Private Server Recall + Compact Session UI Polish (URGENT)

### Pending Todos

- Keep using the verified `build-vs-2026` validation path until the local `build-vs` NMake tree stops failing with `U1076 name too long`.
- Run the every-four-beat Ableton repro on the same machine to judge how much CPU risk remains after the editor-churn mitigation.

### Blockers/Concerns

- Local Windows Ninja/CMake remains unreliable on this machine, and the `build-vs` NMake tree now also fails with `U1076 name too long`; use the verified `build-vs-2026` path for now.
- Phase 6 validated the healthy-path Ableton host-start workflow, but the timing-loss cancellation and failed-start re-arm paths were not manually exercised in Ableton.
- Phase 7 verified BPM/BPI voting end-to-end, but non-initiator vote-against / vote-no semantics still need explicit real-server validation.
- Phase 08.2 still carries forward unresolved same-machine CPU evidence, but it is now treated as a documented carry-forward concern rather than the active execution step.
- Phase 08.3 is closed, but the local `build-vs` NMake tree still fails with `U1076 name too long`; continue using `build-vs-2026` until that environment issue is resolved.

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
- 2026-03-20: Completed Phase 08.2 plan 06 across 2 sessions (~10 min active) with 2 task commits and 5 modified files; the Ableton auth rerun passed, but the phase remains blocked by the unexercised same-machine CPU repro.
- 2026-03-21: Completed Phase 08.3 plans 01-04 plus automated plan 05 work, including the VS18 test gate (`52` test cases / `595` assertions) and a rebuilt Debug VST3 for manual smoke validation.
- 2026-03-21: Recorded the Phase 08.3 manual smoke pass; four checks passed, but `P8.3-RECALL-01` failed because remembered private-room recall is not yet global across new plugin instances.
- 2026-03-21: Completed Phase 08.3 plan 06 in 13 min with 1 clean task commit plus verified working-tree integration changes; app-data remembered-server persistence, cross-instance recall, and wrapped/global merge automation now pass in the `build-vs-2026` validation tree.
- 2026-03-21: Completed Phase 08.3 plan 07 by rerunning the focused 08.3 gate on `build-vs-2026` (`54` test cases / `626` assertions), rebuilding the Debug VST3, and recording the approved brand-new-instance private-room recall rerun in Ableton.

## Session Continuity

Last session: 2026-03-22T10:00:32.638Z
Stopped at: Completed 08.3.1-01-PLAN.md
Resume file: None
