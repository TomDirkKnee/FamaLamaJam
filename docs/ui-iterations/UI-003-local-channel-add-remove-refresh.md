# UI-003: Local Channel Add/Remove Refresh

- Status: done
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `85ef11e`

## Requested Change

Fix the bug where local channels disappear completely after add/remove actions and only reappear after toggling the server settings panel.

## Why

The mixer should update immediately when local channels are added or removed. Having the strips vanish until an unrelated resize makes the feature feel broken.

## Non-Goals

- new strip styling changes
- server settings layout changes
- transport or connection behavior changes

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`

## References

- Screenshot: user report from Ableton after `UI-002`
- Notes:
  - local add/remove must relayout immediately
  - no extra hide/show workaround should be needed

## Implementation Notes

- Fix the relayout timing around mixer strip rebuilds.
- Add a focused regression that checks real strip bounds after add/remove, not just strip labels.

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_mixer_ui`
- Manual:
  - add a local channel
  - remove it
  - add it again
  - confirm strips remain visible without toggling server settings

## Outcome

- Actual files changed:
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`
- Notes:
  - Fixed the relayout timing bug where strip widgets were rebuilt and resized before `currentVisibleMixerStrips_` had been repopulated.
  - Added a regression that checks the re-added local strip has real, non-empty layout bounds immediately after the add/remove sequence.
  - Focused verification passed:
    - `famalamajam_tests.exe "[plugin_mixer_ui]"`
    - `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`

## Final Commit

- Commit: pending
- Rollback: `git revert <final-commit>`
