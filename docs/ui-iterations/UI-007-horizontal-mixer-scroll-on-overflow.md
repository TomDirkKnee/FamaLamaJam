# UI-007: Horizontal Mixer Scroll On Overflow

- Status: done
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `dfe9734`

## Requested Change

When there are too many channel strips to fit in the mixer window, add a left/right scroll function so the hidden strips can still be reached.

## Why

The current mixer can overflow the available width with no usable horizontal navigation, which cuts off strips on the right side.

## Non-Goals

- strip redesign
- chat layout changes
- footer changes
- transport changes

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `src/plugin/FamaLamaJamAudioProcessorEditor.h`
- `tests/integration/plugin_mixer_ui_tests.cpp`

## References

- Notes:
  - the mixer should scroll left/right when strip content exceeds the viewport width
  - the rest of the page layout should stay the same

## Implementation Notes

- Prefer using the existing mixer viewport rather than introducing a second scroll container.
- Add focused regression coverage that proves overflow enables horizontal scrolling and that the view position can move.

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_mixer_ui`
- Manual:
  - confirm a horizontal scrollbar appears when enough strips are visible
  - confirm the strip area can be moved left/right to reach hidden channels

## Outcome

- Actual files changed:
  - `src/plugin/FamaLamaJamAudioProcessorEditor.h`
  - `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
  - `tests/integration/plugin_mixer_ui_tests.cpp`
  - `docs/ui-iterations/INDEX.md`
  - `docs/ui-iterations/UI-006-boxed-strip-button-states.md`
- Notes:
  - the mixer viewport now exposes a horizontal scrollbar when strip content exceeds the available width
  - the scroll step is tuned for left/right strip movement rather than vertical page scrolling
  - focused mixer UI coverage now proves overflow width, scrollbar visibility, and non-zero horizontal view movement

## Verification Run

- Build:
  - `cmake --build build-vs-2026 --target famalamajam_tests --config Debug`
- Tests:
  - `famalamajam_tests.exe "[plugin_mixer_ui]"`
- Plugin rebuild:
  - `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`

## Final Commit

- Commit: `cb6811c`
- Rollback: `git revert cb6811c`
