# Phase 09 JamTaba-Inspired Layout and Mixer Parity Validation Matrix

This matrix captures the final Phase 09 layout checks that still need practical Ableton judgment after the focused automation gate.

## Scope

- `LAYOUT-01`: strip-first layout remains usable at standard and narrower Ableton plugin sizes
- `LAYOUT-02`: grouped strip presentation stays clear after the final compact polish
- `LAYOUT-03`: chat/sidebar/footer coexist without obscuring the primary mixer workflow

## Rebuilt Artifact

- Build target: `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`
- VST3 path: `build-vs-2026/famalamajam_plugin_artefacts/Debug/VST3/FamaLamaJam.vst3`
- Focused automation already green for:
  - narrow-width local collapse and reopen behavior
  - slimmer sidebar persistence
  - compact footer timing and sync-assist placement
  - denser local and remote strip sizing

## Manual Matrix

| ID | Requirement | Scenario | Ableton / FLJ Setup | Expected Result | Result | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| `P9-LAYOUT-01` | `LAYOUT-01` | Standard-width strip-first readability | Load the rebuilt Debug VST3 in Ableton at a typical plugin size with one local strip and multiple remote strips visible. | The top bar reads as a compact support region, the grouped remote mixer remains the dominant workspace, the right sidebar stays readable, and the footer remains pinned. | fail | Disconnected state has no visible `Connect` button, so the core session action is missing and the standard-width layout already feels less usable than the previous version. |
| `P9-LAYOUT-02` | `LAYOUT-01`, `LAYOUT-03` | Narrow-width collapse-first fallback | Narrow the Ableton plugin window until the local lane auto-collapses. Keep the room sidebar visible while monitoring at least one remote strip. | Local collapse is the first fallback, the sidebar remains visible instead of disappearing, and the remote mixer still holds most of the width. | fail | Collapse-first behavior does not rescue usability in practice; the window still reads as sparse and awkward instead of preserving a clear primary workflow. |
| `P9-LAYOUT-03` | `LAYOUT-01` | Manual local reopen after auto-collapse | At the same narrower width, click `Expand Locals` after the lane auto-collapses. | Local strip bodies reopen on demand without hiding the sidebar or footer, so users can quickly inspect local controls even in a tighter window. | fail | Manual local reopening does not recover a workable flow because the missing `Connect` action remains a blocker. |
| `P9-LAYOUT-04` | `LAYOUT-02`, `LAYOUT-03` | Grouped remotes plus room/chat coexistence | Join a room with grouped remote strips visible and room activity in the sidebar. | Group headers and strip cards remain readable, chat/topic stay available on the right, and sidebar content does not crowd the grouped strip area. | fail | Grouped room/chat coexistence is not reading well in practice; the previous version feels cleaner and easier to parse in Ableton. |
| `P9-LAYOUT-05` | `LAYOUT-01`, `LAYOUT-03` | Footer timing, sync, and master readability | Stay connected with room timing active, then inspect the footer while resizing between normal and tighter widths. | Interval timing, `Arm Sync to Ableton Play`, metronome, and master output remain visible in the footer without displacing the strip area. | fail | The footer remains present, but the overall host layout still regresses usability because the primary action is missing and the shell leaves excessive empty space. |

## Manual Outcome Summary

- Real Ableton validation against `build-vs-2026/famalamajam_plugin_artefacts/Debug/VST3/FamaLamaJam.vst3` failed for `P9-LAYOUT-01` through `P9-LAYOUT-05`.
- Phase 09 is not host-proven. The named residual Ableton gap is a practical usability regression: the disconnected layout hides the primary `Connect` action, the collapse/reopen behaviors do not recover a clear workflow, and the grouped room/chat composition feels weaker than the previous version.
- Treat the current Phase 09 shell as automation-backed but host-rejected until a follow-up layout fix restores the missing primary action and removes the sparse, awkward host presentation.
