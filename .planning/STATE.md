---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: in_progress
stopped_at: Completed 09.1-01-PLAN.md
last_updated: "2026-04-07T20:46:48.1238039Z"
last_activity: 2026-04-07 - Completed 09.1-01 by locking the strip-only recovery shell: the stable footer/sidebar shell is preserved, locals and remotes now share one horizontal mixer plane, and collapsed locals remain visible as meter-only mini strips.
progress:
  total_phases: 20
  completed_phases: 15
  total_plans: 67
  completed_plans: 61
  percent: 91
---

# Project State

## Project Reference

See: `.planning/PROJECT.md`

**Core value:** Musicians can reliably join and complete real Ninjam rehearsals directly inside Ableton using a stable VST3 plugin workflow.  
**Current focus:** Phase 09.1 is underway. Plan 01 locked the strip-only recovery shell around the stable reverted page layout, so the next work is the narrow strip-widget conversion without reopening the broader page redesign.

## Current Position

Phase: 09.1 - Strip-Only Mixer Rebuild On Stable Layout  
Plan: 01 - Completed  
Status: In Progress  
Last activity: 2026-04-07 - Completed 09.1-01 by locking the strip-only recovery shell: stable footer/sidebar shell preserved, horizontal local-first strip plane landed, and collapsed locals now stay visible as meter-only mini strips.

Progress: 91%

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

- [Phase 08.3.3]: Export stems on interval boundaries only, with mid-session arming deferred to the next boundary and no leading-silence backfill. - The user wants DAW-importable stems without manual timing nudging, but also does not want prefilled silence before late start points.
- [Phase 08.3.3]: Keep the first export pass narrow: WAV only, dry/pre-mix, and one global record workflow. - This phase is about trustworthy alignment and usable files, not about format proliferation or per-source record UX.
- [Phase 08.3.3]: Keep one recording run folder across disconnect/reconnect and reopen new files there on the next boundary. - This preserves a single jam-session export folder without pretending the reconnect audio belongs in the same already-written file.
- [Phase 08.3.3]: Do not export voice chat stems. - Manual validation clarified that only musical interval contributions should be captured for later mixing; voice chat should be excluded even if the transport path supports it.
- [Phase 08.3.4]: Keep plan 01 at RED only: expose a proof seam and failing fixed-bus coverage without starting processor routing implementation early.
- [Phase 08.3.4]: Lock the Ableton verdict to explicit fallback outcomes: per-user output routing or no follow-on phase if host ergonomics are weak.
- [Phase 08.3.4]: Keep the proof buses exposed by default while still accepting disabled aux layouts so the later Ableton pass sees a real stereo bus declaration.
- [Phase 08.3.4]: Keep routed-output proof scope at one selected sourceId on output bus 1 and leave all other decoded sources on the main output until the Ableton verdict is recorded.
- [Phase 08.3.4]: Approve a follow-on fixed-bus implementation phase because Ableton exposes both proof buses in normal host routing UI.
- [Phase 08.3.4]: Use host-owned track-to-bus routing in Live rather than plugin-owned Ableton track enumeration.
- [Phase 08.3.4]: Start implementation with Main plus Local Send 2 and fixed extra remote output pairs; defer broader bus counts.
- [Phase 08.3.4.1]: First implementation target is exactly two local transmit paths (`Main` and `Local Send 2`) and fixed extra output pairs for remote returns, not dynamic bus counts or plugin-owned DAW track browsing.
- [Phase 08.3.4.1]: Extra local channels should be user-created full strips with editable names and an inline `Add channel` control.
- [Phase 08.3.4.1]: Local `Transmit` and `Voice` controls should be global across all active local channels to preserve strip space.
- [Phase 08.3.4.1]: Remote output assignment should live directly on each remote strip as a dropdown using explicit host-facing labels like `FLJ Main Output` and `Remote Out 1`.
- [Phase 08.3.4.1]: Lock Wave 0 to two local send slots and three fixed host-facing output labels before runtime work begins.
- [Phase 08.3.4.1]: Use header-level compatibility shims and captured transport metadata so RED coverage can compile against the new contracts without implementing the runtime early.
- [Phase 08.3.4.1]: The second remote source for `Local Send 2` is now proven in real Ableton reruns; the remaining questions are persistence and local-channel lifecycle UX, not whether the extra bus can transmit.
- [Phase 08.3.4.1]: Keep routed-output validation tied to host evidence rather than automation-only inference; `ROUTE-03` is now manually passed.
- [Phase 08.3.4.2]: Treat eight inputs/eight outputs as the expansion target, but explicitly verify what Ableton/VST3 actually expose before promising the full count.
- [Phase 08.3.4.2]: Persist input-side local channel identity and bus mapping, but do not assume remote output assignments should persist across arbitrary room membership changes.
- [Phase 08.3.4.2]: Extra local channels must become removable in the UI rather than add-only.
- [Phase 09]: Land the five-region shell before the strip-control rewrite so later Phase 09 plans can change strip anatomy inside a stable editor frame.
- [Phase 09]: Narrow widths collapse the local lane first at `860 px` or below while keeping the sidebar visible and the footer pinned.
- [Phase 09]: Keep processor-owned local transmit and voice semantics mirrored across local strips while moving control ownership and discovery onto each strip widget.
- [Phase 09]: Render remote channels as grouped card clusters under user headers instead of continuing the transitional full-width stacked rows.
- [Phase 09]: Use explicit auto/forced local-lane collapse modes so narrow-width auto-collapse can still be manually reopened.
- [Phase 09]: Slim the sidebar and footer before sacrificing sidebar visibility, keeping grouped remote strips as the dominant workspace.
- [Phase 09]: Treat Phase 09 as host-rejected rather than host-proven — Manual Ableton validation failed all five layout scenarios, so automation-backed geometry is insufficient evidence for ergonomic closure.
- [Phase 09]: Use the missing disconnected-state Connect action and sparse host presentation as the named residual Ableton gap — The follow-up should target the practical usability regression surfaced by the host verdict instead of re-running documentation-only validation.
- [Phase 09.1]: Keep the pre-Phase-9 top bar, right sidebar, and footer shell intact and limit the recovery to the mixer viewport.
- [Phase 09.1]: Make the local header own add/remove/collapse controls while collapsed locals retain visible meter-only mini strips.

### Roadmap Evolution

- Phase 08.1 inserted after Phase 8: Server Discovery Polish & JamTaba Parity Check (URGENT)
- Phase 08.2 inserted after Phase 8: Pre-Layout CPU, Mixer, UI, and Auth Hardening (URGENT)
- Phase 08.3 inserted after Phase 08.2: Functional Release Controls, Session UX, and Voice-Mode Compatibility Guard (URGENT)
- Phase 08.3.1 inserted after Phase 08.3: Private Server Recall + Compact Session UI Polish (URGENT)
- Phase 08.3.2 inserted after Phase 08.3.1: Voice Mode Research & Prototype (URGENT)
- Phase 08.3.3 inserted after Phase 08.3.2: Stem Capture & Export (URGENT)
- Phase 08.3.4 inserted after Phase 08.3.3: Host Multi-I/O Routing Research (URGENT)
- Phase 08.3.4.1 inserted after Phase 08.3.4: Fixed-Bus Multi-Input And NINJAM Channel Implementation (URGENT)
- Phase 08.3.4.2 inserted after Phase 08.3.4.1: Expanded Fixed-Bus I/O And Persistent Input Mapping (URGENT)

### Pending Todos

- Keep using the verified `build-vs-2026` validation path until the local `build-vs` NMake tree stops failing with `U1076 name too long`.
- Run the every-four-beat Ableton repro on the same machine to judge how much CPU risk remains after the editor-churn mitigation.
- Execute Phase 09.1 plan 02 to convert the strip shell into the narrower integrated strip widget while preserving the stable page shell from plan 01.

### Blockers/Concerns

- Local Windows Ninja/CMake remains unreliable on this machine, and the `build-vs` NMake tree now also fails with `U1076 name too long`; use the verified `build-vs-2026` path for now.
- Phase 6 validated the healthy-path Ableton host-start workflow, but the timing-loss cancellation and failed-start re-arm paths were not manually exercised in Ableton.
- Phase 7 verified BPM/BPI voting end-to-end, but non-initiator vote-against / vote-no semantics still need explicit real-server validation.
- Phase 08.2 still carries forward unresolved same-machine CPU evidence, but it is now treated as a documented carry-forward concern rather than the active execution step.
- Phase 08.3 is closed, but the local `build-vs` NMake tree still fails with `U1076 name too long`; continue using `build-vs-2026` until that environment issue is resolved.
- Phase 08.3.4.2 now carries the remaining routing-expansion concerns: proving what extra buses Ableton/VST3 actually expose beyond `Local Send 2`, adding removable extra local channels, and persisting input-side local channel state across project restore.
- The full `ctest` sweep still reports unrelated failures in the Ogg/Vorbis concatenation unit test and one stem-capture integration test; both were logged to the Phase 09 deferred-items file because this plan did not touch codec or stem-capture code.
- Phase 09 host validation failed in Ableton: the disconnected layout hides the primary Connect action, collapse/reopen behavior does not recover a clear workflow, and the grouped room/chat presentation feels worse than the previous version.

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
- 2026-04-04: Completed Phase 08.3.4 plan 01 in 21 min across 2 task commits and 5 modified files; the phase now has RED fixed-bus routing coverage, a manual Ableton proof matrix, and explicit fallback criteria for per-user routing or no follow-on implementation phase.
- 2026-04-04: Completed Phase 08.3.4 plan 02 in 23 min across 2 task commits and 4 modified files; the processor now exposes the fixed proof buses, routes one selected decoded source to `Remote Out 1`, and keeps focused routing plus voice-mode coverage green.
- 2026-04-05: Completed Phase 08.3.4 plan 03 in 11h 12m elapsed across 2 task commits and 4 modified files; Ableton exposed both proof buses in normal routing UI, so the phase closed with an approved host-owned fixed-bus follow-on recommendation.
- 2026-04-05: Completed Phase 08.3.4.1 plan 01 in 12 min across 2 task commits and 9 modified files; fixed-slot RED coverage, captured outbound metadata, and the Ableton routing matrix now lock the implementation target for plan 02.
- 2026-04-05: Completed Phase 08.3.4.1 plan 03 in 25 min across 2 task commits and 6 modified files; the focused fixed-bus gate stayed green, but the first Ableton checkpoint showed only one receiving source (`jim#0`) after `Main` plus `Local Send 2`.
- 2026-04-06: Follow-up Ableton reruns confirmed `Local Send 2` remote audibility and `ROUTE-03` routed-output isolation after rolling back the receive-timing regression; the remaining gaps are reopen persistence and removable extra-local-channel UX.
- 2026-04-06: Closed Phase 08.3.4.1 as a functional first-pass implementation and inserted Phase 08.3.4.2 to pursue expanded bus counts, removable extra channels, and persistent input-side naming/routing.
- 2026-04-07: Completed Phase 09 plan 01 in 33 min across 2 task commits and 6 modified files; the editor now uses the five-region shell with local collapse-first responsiveness and the focused layout gate passes on `build-vs-2026`.
- 2026-04-07: Completed Phase 09 plan 02 in 19 min across 2 task commits and 7 modified files; local transmit and voice actions now live on strip cards, remote strips render in grouped user clusters, and the focused strip-anatomy gate passes on `build-vs-2026`.
- 2026-04-07: Completed Phase 09 plan 03 in 8 min across 2 task commits and 8 modified files; narrow-width local reopening, slimmer sidebar/footer geometry, and the dedicated Phase 09 Ableton layout matrix are now in place, while manual host verdict rows remain pending.
- 2026-04-07: Completed Phase 09 plan 04 in 5 min with 1 task commit and 4 modified files; the Phase 09 Ableton matrix now records five explicit failures, and the phase remains blocked by a missing disconnected-state `Connect` action plus broader host-layout regression.
- 2026-04-07: Completed Phase 09.1 plan 01 in 12 min across 2 task commits and 6 modified files; the stable page shell is preserved while the mixer now runs as a horizontal local-first strip plane with header-owned local controls and collapsed meter-only mini strips.

## Session Continuity

Last session: 2026-04-07T20:46:19.924Z
Stopped at: Completed 09.1-01-PLAN.md
Resume file: None
