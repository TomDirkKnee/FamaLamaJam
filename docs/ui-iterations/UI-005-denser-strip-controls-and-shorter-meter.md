# UI-005: Denser Strip Controls And Wider Meter

- Status: done
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `f177eb2`

## Requested Change

Reduce dead space in the channel strip by:

- making the meter/fader lane much wider so it uses about half of the strip width
- keeping the meter/fader lane full height
- increasing the size of the pan control
- increasing the size of the side buttons

## Why

The current strip still wastes too much horizontal space on the right-hand side. A wider meter lane and larger right-side controls should make the strip feel denser and easier to use.

## Non-Goals

- server settings changes
- footer changes
- routing behavior changes
- new strip control types

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`

## References

- Screenshot: user-provided strip screenshot showing the remaining dead space on the right-hand side
- Notes:
  - meter should take up about half the strip width
  - meter should stay full height
  - pan and side buttons should be larger

## Implementation Notes

- Keep the current strip ordering and integrated meter/gain behavior.
- Adjust geometry only for this pass.
- Add focused assertions for the wider meter lane and larger controls.

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_mixer_ui`
- Manual:
  - confirm the strip uses space more efficiently
  - confirm the meter is visibly wider and still full height
  - confirm the pan pot and buttons are larger and easier to hit

## Outcome

- Actual files changed:
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`
- Notes:
  - The meter/fader lane is now much wider and stays full height, using about half of the strip width instead of leaving dead space on the right.
  - The pan pot and side buttons are larger, using the recovered space to reduce dead air in the strip.
  - Focused verification passed:
    - `famalamajam_tests.exe "[plugin_mixer_ui]"`
    - `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`

## Final Commit

- Commit: pending
- Rollback: `git revert <final-commit>`
