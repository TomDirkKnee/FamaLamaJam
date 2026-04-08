# UI-004: Integrated Meter/Gain And 0 dB Reset

- Status: done
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `fdc21f2`

## Requested Change

In the mixer strips:

- visually integrate the gain fader into the meter so the fader rides on top of the meter lane
- on double-click, reset the strip gain to `0 dB`

## Why

Separating the gain fader from the meter wastes space and looks busier than necessary. A superimposed fader should read more cleanly and keep the strip narrow.

## Non-Goals

- pan control changes
- strip header layout changes
- server settings or footer layout changes
- routing or transport behavior changes

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `src/plugin/FamaLamaJamAudioProcessorEditor.h`
- `tests/integration/plugin_mixer_ui_tests.cpp`

## References

- Screenshot: user-provided Ableton-style meter/fader sketch in chat
- Notes:
  - gain control should be superimposed on the meter
  - double-click should restore `0 dB`

## Implementation Notes

- Keep this pass focused on the strip gain control only.
- Prefer slider look-and-feel or paint customization over broader layout changes.
- Add a focused regression for overlay geometry and the double-click reset contract.

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_mixer_ui`
- Manual:
  - confirm the gain thumb visually sits on the meter
  - double-click the gain control and confirm it resets to `0 dB`

## Outcome

- Actual files changed:
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `src/plugin/FamaLamaJamAudioProcessorEditor.h`
- `tests/integration/plugin_mixer_ui_tests.cpp`
- Notes:
  - The strip gain slider now uses a custom overlay look so the fader thumb rides directly on the meter lane instead of reading as a separate control.
  - The gain interaction bounds are now centered tightly on the meter so the integrated control stays narrow.
  - Strip gain sliders now reset to `0 dB` on double-click.
  - Focused verification passed:
    - `famalamajam_tests.exe "[plugin_mixer_ui]"`
    - `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`

## Final Commit

- Commit: pending
- Rollback: `git revert <final-commit>`
