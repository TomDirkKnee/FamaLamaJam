# UI-015: Strip Status Tooltips With Full Wording

- Status: done
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `d707574`

## Requested Change

Keep the compact status text visible in the mixer strip, but show the original full wording as a tooltip on hover.

## Why

The shortened status labels fit much better in the narrow strips, but some of the fuller meaning is now hidden unless the user already knows what the compact wording stands for.

## Non-Goals

- changing the compact on-strip wording
- status color changes
- strip layout changes
- footer, chat, or server settings changes

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessor.h`
- `src/plugin/FamaLamaJamAudioProcessor.cpp`
- `src/plugin/FamaLamaJamAudioProcessorEditor.h`
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`
- `tests/integration/plugin_transmit_controls_tests.cpp`
- `tests/integration/plugin_voice_mode_toggle_tests.cpp`

## References

- Notes:
  - keep compact labels such as `TX off`, `Receiving`, and `Voice live`
  - tooltip should show the original wording, for example `Not transmitting`, `Receiving interval audio`, or `Voice chat: near realtime`
  - JUCE tooltip support should be local to the editor so it works inside the plugin window

## Implementation Notes

- Prefer carrying both compact and full status strings through the strip state rather than reconstructing the full phrase in the editor.
- Tooltip should be blank when there is no status text.

## Verification Plan

- Build: targeted UI and status tests
- Tests:
  - `plugin_mixer_ui`
  - `plugin_transmit_controls`
  - `plugin_voice_mode_toggle`
- Manual:
  - confirm compact text still shows on-strip
  - confirm hovering the status text shows the original wording

## Outcome

- Actual files changed:
  - `src/plugin/FamaLamaJamAudioProcessor.h`
  - `src/plugin/FamaLamaJamAudioProcessor.cpp`
  - `src/plugin/FamaLamaJamAudioProcessorEditor.h`
  - `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
  - `tests/integration/plugin_mixer_ui_tests.cpp`
  - `tests/integration/plugin_transmit_controls_tests.cpp`
  - `tests/integration/plugin_voice_mode_toggle_tests.cpp`
- Notes:
  - strips keep the compact on-screen wording, but now carry the original full wording separately for hover tooltips
  - the editor now owns a local JUCE `TooltipWindow` so those hover tooltips can appear inside the plugin

## Verification Run

- Build:
  - `cmake --build build-vs-2026 --target famalamajam_tests --config Debug`
- Tests:
  - `famalamajam_tests.exe "[plugin_mixer_ui],[plugin_transmit_controls],[plugin_voice_mode_toggle]"`
- Plugin rebuild:
  - `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`

## Final Commit

- Commit: `b916b95`
- Rollback: `git revert b916b95`
