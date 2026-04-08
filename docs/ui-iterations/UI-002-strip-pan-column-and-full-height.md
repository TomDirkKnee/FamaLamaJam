# UI-002: Strip Pan Column And Full-Height Mixer Strips

- Status: done
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `e6c7913`

## Requested Change

In the mixer strip area:

- move the pan control into line with the other strip controls instead of leaving it offset below the fader
- elongate the whole mixer strip so it fills the available vertical mixer space more fully

## Why

The current strip layout wastes vertical room and makes the pan pot feel detached from the rest of the strip controls.

## Non-Goals

- server settings layout changes
- footer redesign
- remote/local control behavior changes
- integrated meter/fader redesign beyond the geometry needed for this pass

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`

## References

- Screenshot: user-provided strip screenshot in chat showing the pan pot hanging below the control column
- Notes:
  - pan should sit in the same vertical control lane as the other strip buttons
  - strips should use the available height instead of stopping halfway down the mixer area

## Implementation Notes

- Keep the current stable shell and focus only on strip geometry.
- Prefer layout changes in the mixer strip `resized` path over broader structural refactors.
- Tighten or extend focused strip-geometry tests rather than broad UI suites.

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_mixer_ui`
- Manual:
  - open the plugin and confirm the pan pot sits inline with the other strip controls
  - confirm strips fill more of the vertical mixer area

## Outcome

- Actual files changed:
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`
- Notes:
  - The pan pot now sits in the same vertical control column as the strip buttons instead of hanging below the fader spine.
  - Expanded and collapsed strips now use more of the available mixer viewport height.
  - Focused verification passed:
    - `famalamajam_tests.exe "[plugin_mixer_ui]"`
    - `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`

## Final Commit

- Commit: `85ef11e`
- Rollback: `git revert 85ef11e`
