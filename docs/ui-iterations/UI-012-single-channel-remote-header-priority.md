# UI-012: Single-Channel Remote Header Priority

- Status: in_progress
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `3778e48`

## Requested Change

When a remote user has only one channel, stop replacing their visible header with `...` by giving the username priority over the redundant `1 channel` count.

## Why

The fixed count badge is consuming too much of the narrow remote group header, which leaves the username so little room that JUCE ellipsizes it down to three dots.

## Non-Goals

- multi-channel remote group count behavior
- local group header behavior
- strip control changes
- footer, chat, or server settings changes

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`

## References

- Notes:
  - this is not intentional obfuscation
  - hide the `1 channel` label for single-channel remote groups
  - keep counts for `2+ channels`

## Implementation Notes

- Prefer a layout-only fix in the remote group header.
- Preserve the existing local count behavior.
- Add focused coverage that a one-channel remote group no longer reserves count-badge space.

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_mixer_ui`
- Manual:
  - confirm single-channel remote headers show the username instead of `...`
  - confirm multi-channel remote groups still show their channel counts

## Outcome

- Actual files changed:
  - `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
  - `tests/integration/plugin_mixer_ui_tests.cpp`
- Notes:
  - single-channel remote groups no longer reserve the fixed right-side count slot
  - multi-channel remote groups still show their channel count as before

## Verification Run

- Build:
  - `cmake --build build-vs-2026 --target famalamajam_tests --config Debug`
- Tests:
  - `famalamajam_tests.exe "[plugin_mixer_ui]"`
- Plugin rebuild:
  - `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`

## Final Commit

- Commit: pending
- Rollback: `git revert <final-commit>`
