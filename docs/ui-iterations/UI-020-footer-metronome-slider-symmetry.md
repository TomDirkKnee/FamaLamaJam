# UI-020: Footer Metronome Slider Symmetry

- Status: done
- Requested: 2026-04-09
- Requested by: user
- Baseline commit: `8744c99`

## Requested Change

Replace the metronome volume knob with a horizontal slider like the master output slider, and make the footer side sections feel symmetrical in size.

## Why

The footer still wastes width on the metronome side. A horizontal slider uses the space better and gives the metronome and master output a cleaner matched look.

## Non-Goals

- transport timing logic
- host sync button behavior
- chat or mixer changes

## Expected Files

- `docs/ui-iterations/INDEX.md`
- `docs/ui-iterations/UI-019-normal-compact-size-selector.md`
- `docs/ui-iterations/UI-020-footer-metronome-slider-symmetry.md`
- `src/plugin/FamaLamaJamAudioProcessorEditor.h`
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`

## References

- Notes:
  - metronome volume should become a horizontal slider
  - it should visually match the master output slider language
  - the left and right footer sections should be close to symmetrical
  - it is acceptable to give the sync button a bit less width if needed

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_mixer_ui`
- Manual:
  - confirm the metronome control is now a horizontal slider
  - confirm the metronome and master sections look balanced
  - confirm the host sync button still remains readable in normal and compact sizes

## Outcome

- Actual files changed:
  - `docs/ui-iterations/INDEX.md`
  - `docs/ui-iterations/UI-019-normal-compact-size-selector.md`
  - `docs/ui-iterations/UI-020-footer-metronome-slider-symmetry.md`
  - `src/plugin/FamaLamaJamAudioProcessorEditor.h`
  - `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
  - `tests/integration/plugin_mixer_ui_tests.cpp`
- Notes:
  - the metronome volume control now uses a horizontal slider with the same text-box style as the master output slider
  - the footer side sections now reserve matched widths, so the metronome and master slider runs read as a balanced pair
  - the sync button gives up some width to make room for the more symmetrical slider sections

## Verification Run

- Build:
  - `cmake --build build-vs-2026 --target famalamajam_tests --config Debug`
- Tests:
  - `famalamajam_tests.exe "[plugin_mixer_ui]"`
- Plugin rebuild:
  - `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`

## Final Commit

- Commit: `985ac00`
- Rollback: `git revert 985ac00`
