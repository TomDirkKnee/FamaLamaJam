# UI-006: Boxed Strip Button States

- Status: in_progress
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `b7aa93c`

## Requested Change

Make the strip `Mute`, `Solo`, and `Voice/Interval` buttons use the same boxed visual language as `Transmit`, with text inside the box and these color rules:

- `Voice` = orange
- `Interval` = green
- `Mute on` = red
- `Mute off` = transparent
- `Solo on` = red
- `Solo off` = transparent

## Why

The current toggle styling does not match the strip button language and makes those controls look inconsistent next to `Transmit`.

## Non-Goals

- strip geometry changes
- meter/fader changes
- transport or routing changes
- server settings changes

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `src/plugin/FamaLamaJamAudioProcessorEditor.h`
- `tests/integration/plugin_mixer_ui_tests.cpp`

## References

- Notes:
  - buttons should look like transmit
  - text should sit inside the box
  - state colors should follow the user-defined mapping exactly

## Implementation Notes

- Prefer using the same button family for these strip controls so their visuals match naturally.
- Add focused regression coverage for button text, toggle state, and state colour rules.

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_mixer_ui`
- Manual:
  - confirm mute/solo/voice read like boxed strip buttons
  - confirm the requested active/inactive colors show correctly

## Outcome

- Actual files changed:
  - `src/plugin/FamaLamaJamAudioProcessorEditor.h`
  - `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
  - `tests/integration/plugin_mixer_ui_tests.cpp`
- Notes:
  - strip `Mute`, `Solo`, and `Voice/Interval` now use the same boxed button treatment as `Transmit`
  - off-state `Mute` and `Solo` stay transparent-filled with a visible outline instead of disappearing
  - `Interval` is green and `Voice` is orange using the same boxed control shape
  - mixer UI verification now includes an explicit appearance check for these strip buttons

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
