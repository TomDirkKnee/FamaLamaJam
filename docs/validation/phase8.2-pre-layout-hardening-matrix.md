# Phase 08.2 Pre-Layout Hardening Validation Matrix

**Phase:** 08.2-pre-layout-cpu-mixer-ui-and-auth-hardening  
**Plan:** 08.2-04  
**Host:** Ableton Live on Windows  
**Plugin artifact:** `build-vs/famalamajam_plugin_artefacts/Debug/VST3/FamaLamaJam.vst3` (rebuild prepared for plan `08.2-06` at `2026-03-19T15:55:06Z`)

Use this matrix for the real plugin verification pass. Record exactly what happened in the host, including failures and anything not exercised.

| ID | Scenario | Steps | Expected plugin outcome | Result | Notes |
|----|----------|-------|-------------------------|--------|-------|
| P8.2-SIDEBAR-01 | Fixed sidebar remains usable | Load the rebuilt Debug VST3 at the practical Ableton window size used during the regression report. Stay disconnected and inspect the right side of the editor. | The room workflow stays in a fixed right-hand sidebar, remains visible while disconnected, and no longer hides the mixer or stream-monitoring controls. | pass | User reported the fixed right-hand sidebar stays visible and no longer hides the mixer. |
| P8.2-MIXER-02 | Mixer footer master-output baseline is correct | Inspect the mixer section and compare it with the old top-level controls that were removed. Adjust the footer output level while monitoring plugin output. | The footer master-output control is present, `Default Gain`/`Default Pan`/`Default Muted` are gone, and output scaling behaves correctly without collapsing the Local Monitor distinction. | pass | User reported the footer master output exists, the old controls are gone, and output scaling behaves correctly. |
| P8.2-AUTH-03 | Correct password succeeds on a controlled private room | Connect to the controlled private-room path with the known-good password. | Connection should succeed with the visible password/auth workflow in the live plugin window. | pass | User reran the controlled private-room path in Ableton and reported the correct-password connect path succeeded. |
| P8.2-AUTH-04 | Wrong password shows an inline auth error | Retry the same controlled private-room path with an incorrect password after the good-path check is working. | The connection area should show an explicit inline auth failure for the wrong password. | pass | User reran the wrong-password path as a separate required check and reported the inline auth failure behavior passed in the real plugin window. |
| P8.2-CPU-05 | Prior every-four-beat CPU spike is materially reduced | Reproduce the original every-four-beat CPU-spike scenario on the same machine that showed the issue before the editor-churn mitigation landed. | CPU spikes should be materially reduced, or the remaining residual should be captured explicitly as machine-sensitive behavior. | not exercised | User reported this repro was still not exercised in the follow-up Ableton rerun, so the host-level CPU outcome remains unproven. |

## Session Notes

- Verification source: user response captured after the blocking `checkpoint:human-verify`
- Manual pass date: 2026-03-20
- Controlled private-room path: same harness used for the host auth check
- Ableton version: not recorded in this pass
- Plugin build timestamp: rebuilt during Task 1 on 2026-03-19
- Rerun prep: plan `08.2-06` rebuilt the same Debug VST3 artifact at `2026-03-19T15:55:06Z` and revalidated `"[plugin_experimental_transport],[plugin_rehearsal_ui_flow],[plugin_pre_layout_cpu_diagnostics]"` (`17` test cases / `145` assertions) before the next host pass.
- Additional observations: auth rerun outcomes were reported directly as pass/pass, while the CPU repro remained unexercised.

## Manual Outcome Summary

- Manual pass evidence is green for the fixed sidebar usability claim.
- Manual pass evidence is green for the mixer footer master-output baseline and removal of the dead default controls.
- Manual pass evidence is green for the correct-password private-room auth path.
- Manual pass evidence is green for the wrong-password inline auth error path.
- Manual evidence is still missing for the every-four-beat CPU repro because the user deferred that check for now.

## Gate Decision

Phase 08.2 remains blocked. The Ableton auth rerun now closes the private-room password gap honestly, but the phase still cannot close or unblock Phase 9 while the original same-machine every-four-beat CPU repro remains unexercised.
