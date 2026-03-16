# Phase 5 Rehearsal Checklist

Use this checklist with the fresh Phase 5 VST3 build in Ableton on Windows.

## Build Under Test

- VST3: `build-vs/famalamajam_plugin_artefacts/Debug/VST3/FamaLamaJam.vst3`
- Date: 2026-03-16

## Scenarios

| ID | Scenario | Expected Result | Outcome | Notes |
|----|----------|-----------------|---------|-------|
| P5-HOST-01 | Save set, close Ableton, reopen | Settings and mix restore; plugin starts disconnected and waits for explicit Connect | pass | Covered by automated restore-idle/state tests and consistent with manual validation. |
| P5-HOST-02 | Duplicate a track containing a connected plugin | Duplicate stays idle and does not silently join the room | pass | Covered by `plugin_host_lifecycle`. |
| P5-HOST-03 | Stop/start audio engine or change device while connected | Plugin recovers without reload and without zombie reconnect behavior | pass | Covered by runtime compatibility/lifecycle tests. |
| P5-HOST-04 | Remove plugin or close set while connected | Session disconnects cleanly; no lingering room presence remains | pass | Validated through cleanup fixes and lifecycle coverage. |
| P5-UX-01 | Fresh join from built-in JUCE UI | Setup, connect, and start-playing flow is clear without external tools | pass | Manual validation succeeded using only the built-in plugin UI. |
| P5-UX-02 | Auth/config failure | Main status message explains the failure and next action clearly | pass | Manual testing confirmed actionable error text. |
| P5-UX-03 | Transient reconnect case | Main status and transport text remain understandable until recovery | pass | Manual reconnect/error states remained understandable during regression testing. |
| P5-UX-04 | Missing/late remote interval behavior | Status remains honest about timing/waiting expectations | pass | Confirmed during timing-loss/reconnect validation. |
| P5-ROOM-01 | Small-group rehearsal | Room can rehearse successfully with no blocking UX or lifecycle issues | pass | Final manual test succeeded after transport chunking fix. |

## Completion Rule

Scenarios recorded here were finalized in `05-VERIFICATION.md` when Phase 5 was signed off.
