# UI-013: Local Header Title Shortening

- Status: done
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `79398c0`

## Requested Change

Change the expanded local mixer group title from `Local Sends` to `Local` so it fits the narrow group header better.

## Why

The longer title wastes scarce header width in the narrow local group and looks cramped once the add/remove buttons and chevron tab are present.

## Non-Goals

- collapsed local title behavior
- local strip controls
- remote group headers
- footer, chat, or server settings changes

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessorEditor.h`
- `tests/integration/plugin_mixer_ui_tests.cpp`

## References

- Notes:
  - only the expanded local group title should change
  - the goal is a cleaner fit in the current header width

## Implementation Notes

- Prefer updating the shared local header title constant so tests and UI stay aligned.
- Keep the change scoped to the visible header text rather than broader layout changes.

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_mixer_ui`
- Manual:
  - confirm the expanded local group title reads `Local`
  - confirm the narrower title sits more comfortably beside the header controls

## Outcome

- Actual files changed:
  - `src/plugin/FamaLamaJamAudioProcessorEditor.h`
- Notes:
  - the shared expanded local header title now reads `Local`
  - the collapsed shorthand remains unchanged

## Verification Run

- Build:
  - `cmake --build build-vs-2026 --target famalamajam_tests --config Debug`
- Tests:
  - `famalamajam_tests.exe "[plugin_mixer_ui]"`
- Plugin rebuild:
  - `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`

## Final Commit

- Commit: `074c914`
- Rollback: `git revert 074c914`
