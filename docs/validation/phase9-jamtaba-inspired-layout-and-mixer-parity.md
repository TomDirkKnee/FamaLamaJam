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
| `P9-LAYOUT-01` | `LAYOUT-01` | Standard-width strip-first readability | Load the rebuilt Debug VST3 in Ableton at a typical plugin size with one local strip and multiple remote strips visible. | The top bar reads as a compact support region, the grouped remote mixer remains the dominant workspace, the right sidebar stays readable, and the footer remains pinned. | pending manual pass | Automation proves geometry and ordering, but practical host readability still needs a real Ableton verdict. |
| `P9-LAYOUT-02` | `LAYOUT-01`, `LAYOUT-03` | Narrow-width collapse-first fallback | Narrow the Ableton plugin window until the local lane auto-collapses. Keep the room sidebar visible while monitoring at least one remote strip. | Local collapse is the first fallback, the sidebar remains visible instead of disappearing, and the remote mixer still holds most of the width. | pending manual pass | The automated editor gate now locks the collapse, sidebar, and width-balance rules that back this Ableton check. |
| `P9-LAYOUT-03` | `LAYOUT-01` | Manual local reopen after auto-collapse | At the same narrower width, click `Expand Locals` after the lane auto-collapses. | Local strip bodies reopen on demand without hiding the sidebar or footer, so users can quickly inspect local controls even in a tighter window. | pending manual pass | This was the last narrow-width usability gap fixed in code for 09-03. |
| `P9-LAYOUT-04` | `LAYOUT-02`, `LAYOUT-03` | Grouped remotes plus room/chat coexistence | Join a room with grouped remote strips visible and room activity in the sidebar. | Group headers and strip cards remain readable, chat/topic stay available on the right, and sidebar content does not crowd the grouped strip area. | pending manual pass | Automation covers grouped strip order and sidebar persistence, not human readability under live room density. |
| `P9-LAYOUT-05` | `LAYOUT-01`, `LAYOUT-03` | Footer timing, sync, and master readability | Stay connected with room timing active, then inspect the footer while resizing between normal and tighter widths. | Interval timing, `Arm Sync to Ableton Play`, metronome, and master output remain visible in the footer without displacing the strip area. | pending manual pass | The focused transport/layout tests now enforce the slimmer footer widths and pinned placement used here. |

## Manual Outcome Summary

- No direct Ableton host verdict was recorded during this automated execution.
- The rebuilt artifact and this matrix are ready for the follow-up host pass.
- Treat Phase 09 layout ergonomics as automation-backed but still awaiting explicit Ableton confirmation until the rows above are filled with pass or fail results.
