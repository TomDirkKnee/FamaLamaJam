# UI-021: Master And Pan Double-Click Resets

- Status: done
- Requested: 2026-04-09
- Requested by: user
- Baseline commit: `985ac00`

## Requested Change

Add double-click reset to the footer master output slider and the mixer strip pan controls.

## Why

The strip gain controls already reset on double-click, and the same interaction should apply consistently to the master output and pan controls.

## Non-Goals

- meter or strip geometry
- footer layout changes
- gain reset behavior

## Expected Files

- `docs/ui-iterations/INDEX.md`
- `docs/ui-iterations/UI-020-footer-metronome-slider-symmetry.md`
- `docs/ui-iterations/UI-021-master-and-pan-double-click-resets.md`
- `src/plugin/FamaLamaJamAudioProcessorEditor.h`
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`

## References

- Notes:
  - master output should reset to `0 dB`
  - pan controls should reset to center (`0.0`)
  - this should use the existing JUCE double-click reset behavior

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_mixer_ui`
- Manual:
  - confirm double-click on the master output slider resets to `0.0 dB`
  - confirm double-click on strip pan pots resets to centered pan

## Outcome

- Actual files changed:
  - `docs/ui-iterations/INDEX.md`
  - `docs/ui-iterations/UI-020-footer-metronome-slider-symmetry.md`
  - `docs/ui-iterations/UI-021-master-and-pan-double-click-resets.md`
  - `src/plugin/FamaLamaJamAudioProcessorEditor.h`
  - `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
  - `tests/integration/plugin_mixer_ui_tests.cpp`
- Notes:
  - the master output slider now resets to `0 dB` on double-click
  - all strip pan controls now reset to centered pan (`0.0`) on double-click
  - gain reset behavior remains unchanged

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
