# UI-016: Remote Output Selector Width And Labels

- Status: done
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `b916b95`

## Requested Change

Make the remote output selector fit the width of the channel strip and show the current output name inside the selector, using abbreviations where needed.

## Why

The current selector is just a tiny arrow button, which wastes the available strip width and hides the actual output assignment from the user.

## Non-Goals

- routing logic changes
- local strip changes
- mixer layout changes outside the remote selector row
- footer, chat, or server settings changes

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`

## References

- Notes:
  - selector should span the usable width of the remote strip footer row
  - visible names can be abbreviated, for example `Main` or `Out 2`
  - full output name can still be preserved in the tooltip

## Implementation Notes

- Prefer UI-only abbreviation of the existing output labels rather than changing routing identifiers.
- Keep the selected output assignment behavior unchanged.

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_mixer_ui`
- Manual:
  - confirm remote selectors fill the row width
  - confirm the selected output name is visible inside the selector

## Outcome

- Actual files changed:
  - `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
  - `tests/integration/plugin_mixer_ui_tests.cpp`
- Notes:
  - remote output selectors now span the usable width of the strip footer row instead of rendering as a tiny arrow box
  - selector text now shows compact labels such as `Main` and `Out 2`
  - the full output label is preserved in the selector tooltip

## Verification Run

- Build:
  - `cmake --build build-vs-2026 --target famalamajam_tests --config Debug`
- Tests:
  - `famalamajam_tests.exe "[plugin_mixer_ui]"`
- Plugin rebuild:
  - `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`

## Final Commit

- Commit: `0853e6b`
- Rollback: `git revert 0853e6b`
