# UI-017: Footer Full Width And Metronome Knob

- Status: in_progress
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `0853e6b`

## Requested Change

Stretch the footer and transport area across the full shell, remove the `Sync assist needs room timing to arm` note, and restore a metronome volume knob that matches the strip pan controls.

## Why

The footer controls are bunched into the left half of the shell, which wastes space and makes the transport feel cramped. The sync-assist note adds noise, and the metronome volume control is no longer visible enough to be useful.

## Non-Goals

- mixer strip changes
- server settings changes
- chat or room sidebar changes
- sync assist logic changes

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessorEditor.h`
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`

## References

- Notes:
  - the footer transport area should use the full available width
  - remove the sync-assist note from the visible footer UI
  - restore the metronome volume control as a knob/pot
  - the metronome knob should look like the strip pan pots

## Implementation Notes

- Prefer layout changes over introducing new footer controls.
- Keep the underlying host sync assist behavior intact; this is only a UI cleanup pass.

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_mixer_ui`
- Manual:
  - confirm the transport/footer reads full-width
  - confirm the sync-assist note is gone
  - confirm the metronome knob is visible and styled like the strip pan pots

## Outcome

- Actual files changed:
  - `docs/ui-iterations/INDEX.md`
  - `docs/ui-iterations/UI-016-remote-output-selector-width-and-labels.md`
  - `src/plugin/FamaLamaJamAudioProcessorEditor.h`
  - `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
  - `tests/integration/plugin_mixer_ui_tests.cpp`
- Notes:
  - the footer transport label now spans the full shell width instead of being capped to a short left-hand block
  - the control row now uses the full footer width, with metronome controls on the left, host sync assist in the center, and master output on the right
  - the sync-assist note row is hidden from the visible footer UI
  - the metronome volume control is restored as a rotary knob using the same rotary style as the strip pan pots

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
