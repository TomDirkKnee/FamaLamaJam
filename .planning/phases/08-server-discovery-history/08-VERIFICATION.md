---
phase: 08
slug: server-discovery-history
status: pending_manual_validation
created: 2026-03-17
updated: 2026-03-17
---

# Verification Summary

Phase 8 automation is complete and green. The remaining work is the focused Ableton/manual validation pass for the new combined picker workflow.

## Automated Results

- `famalamajam_tests.exe "[server_discovery_parser],[plugin_server_discovery],[plugin_server_discovery_ui],[plugin_state_roundtrip]"` - pass (2026-03-17, `15` test cases / `203` assertions)
- `ctest --test-dir build-vs --output-on-failure` - pass (2026-03-17, `97/97` tests)
- `cmake --build build-vs --target famalamajam_plugin_VST3 --config Debug` - pass (2026-03-17)

## VST3 Under Test

- `build-vs/famalamajam_plugin_artefacts/Debug/VST3/FamaLamaJam.vst3` - rebuilt on 2026-03-17 for manual Ableton validation

## Manual Outcomes

Source matrix: `docs/validation/phase8-server-discovery-matrix.md`

| ID | Outcome | Notes |
|----|---------|-------|
| P8-DISC-01 | not tested | No private server was available during the manual pass, so remembered-history behavior was not exercised in Ableton. |
| P8-DISC-02 | not tested | Without a remembered private server in the picker, the remembered-first ordering case was not exercised manually. |
| P8-DISC-03 | pass | Selecting a discovery entry prefills Host/Port without connecting automatically. |
| P8-DISC-04 | pass | Manual Host/Port editing still works after picker selection. |
| P8-DISC-05 | pass | Failure/stale messaging remained understandable and manual connect still worked. |
| P8-DISC-06 | fail | The discovery area is functional but now needs a broader layout redesign rather than further compact tweaks. |

## Current Verification Position

- DISC-01 automation is satisfied: public discovery fetch, merge ordering, richer labels, and stale/failure behavior are covered by targeted tests.
- DISC-02 automation is satisfied: remembered history persistence, recency order, and separation from transient fetch state are covered by targeted tests and state roundtrip coverage.
- Manual host validation partially passed: autofill, manual-entry non-regression, and stale/failure wording were confirmed in Ableton.
- Manual host validation also surfaced two follow-up concerns: the current connection-area layout is too cramped, and the picker selection can jump to another row after `Connect`.
- Remembered-history behavior remains unverified manually because no private server was available during this pass.

## Residual Constraints

- The public server list source is treated as a practical upstream-compatible implementation detail, not as a permanently guaranteed service contract.
- Phase 8 deliberately keeps the public list cache runtime-only; only remembered successful servers are persisted.
- The connection area is now denser, and the manual pass judged it as needing a broader layout redesign rather than more small tweaks.
- The visible user-count display may not yet match JamTaba/ReaNINJAM expectations; the first number in strings like `1/8` never appeared to change during manual testing, so upstream/JamTaba behavior should be researched before polishing this workflow.
- A manual bug remains open: after pressing `Connect`, the picker can jump to a different server entry than the one the user selected or connected to.
