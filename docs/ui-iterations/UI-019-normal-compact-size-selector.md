# UI-019: Normal Compact Size Selector

- Status: done
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `dd0fafe`

## Requested Change

Add an in-plugin size selector with `Normal` and `Compact` modes. Compact mode should reduce the mixer/server/footer area by about 40% horizontally and about 10% vertically, while leaving the chat sidebar width unchanged.

## Why

The plugin needs a smaller presentation for rooms with fewer users, but the chat column does not need to shrink yet. A simple preset selector is more predictable than host-driven freeform resizing.

## Non-Goals

- mixer-only mode
- chat collapse
- strip resizing
- host-driven freeform window resizing

## Expected Files

- `docs/ui-iterations/INDEX.md`
- `src/plugin/FamaLamaJamAudioProcessorEditor.h`
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_rehearsal_ui_flow_tests.cpp`

## References

- Notes:
  - selector options are `Normal` and `Compact`
  - `Normal` remains `1350 x 760`
  - `Compact` targets `936 x 684`
  - the chat sidebar should keep the same width in compact mode

## Implementation Notes

- Prefer fixed editor size presets over freeform host resizing.
- Keep strip dimensions unchanged and rely on horizontal scrolling when the mixer area narrows.

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_rehearsal_ui_flow`
- Manual:
  - confirm the selector switches between `Normal` and `Compact`
  - confirm compact mode narrows the main area but leaves the chat width unchanged
  - confirm server settings and footer still reflow cleanly in compact mode

## Outcome

- Actual files changed:
  - `docs/ui-iterations/INDEX.md`
  - `docs/ui-iterations/UI-018-top-align-room-sidebar-with-expanded-settings.md`
  - `src/plugin/FamaLamaJamAudioProcessorEditor.h`
  - `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
  - `tests/integration/plugin_rehearsal_ui_flow_tests.cpp`
- Notes:
  - the top bar now includes an in-plugin `Normal / Compact` size selector
  - `Normal` keeps the existing `1350 x 760` shell
  - `Compact` switches to `936 x 684`, shrinking the left mixer/server/footer area while keeping the chat sidebar width stable
  - the mixer strips themselves keep their current dimensions, so compact mode relies more on horizontal scrolling
  - compact mode now uses a tighter vertical shell budget so the strips clear the footer instead of clipping at the bottom
  - the compact footer also trims the extra bottom dead space that was visible under the transport controls
  - transient setup messages no longer consume dedicated rows under the server settings; lifecycle, discovery, stem, and auth guidance are consolidated into the inline status area beside `Connect / Disconnect`
  - the footer now collapses to the actual transport-content height, so disconnected states no longer reserve a tall empty footer band

## Verification Run

- Build:
  - `cmake --build build-vs-2026 --target famalamajam_tests --config Debug`
- Tests:
  - `famalamajam_tests.exe "[plugin_rehearsal_ui_flow]"`
- Plugin rebuild:
  - `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`

## Final Commit

- Commit: pending
- Rollback: `git revert <final-commit>`
