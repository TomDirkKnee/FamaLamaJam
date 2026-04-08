# UI-014: Strip Status Text Audit And Shortening

- Status: in_progress
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `074c914`

## Requested Change

Audit the colored informational strip text, work out what fits in the current narrow strip status row, and shorten only the phrases that overflow.

## Why

Several status phrases are longer than the current narrow strip header comfortably supports, which makes them truncate or feel crowded compared with the rest of the mixer UI.

## Non-Goals

- status color changes
- strip layout changes
- button label changes
- footer, chat, or server settings changes

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessor.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`
- `tests/integration/plugin_transmit_controls_tests.cpp`
- `tests/integration/plugin_voice_mode_toggle_tests.cpp`

## Audit Target

- Current narrow strip width is `112-122 px`, leaving roughly `102-112 px` of usable status-label width after the strip side padding.
- Keep phrases that already fit comfortably.
- Replace only the phrases that overflow or crowd that width budget.

## Proposed Terminology

- Keep:
  - `Transmitting`
  - `Healthy`
  - `Warning`
- Shorten:
  - `Not transmitting` -> `TX off`
  - `Getting ready to transmit` -> `Warming up`
  - `Voice mode ready` -> `Voice ready`
  - `Switching to voice mode...` -> `Switching...`
  - `Voice chat: low quality, near realtime` -> `Voice live`
  - `Voice chat: near realtime` -> `Voice live`
  - `Receiving interval audio` -> `Receiving`
  - `Queued for next interval` -> `Queued`
  - `In room, waiting for interval audio` -> `In room`

## Verification Plan

- Build: targeted UI and status tests
- Tests:
  - `plugin_mixer_ui`
  - `plugin_transmit_controls`
  - `plugin_voice_mode_toggle`
- Manual:
  - confirm the shortened phrases fit cleanly in Ableton
  - confirm the meaning still feels clear in local and remote strips

## Outcome

- Actual files changed:
  - `src/plugin/FamaLamaJamAudioProcessor.cpp`
  - `tests/integration/plugin_mixer_ui_tests.cpp`
  - `tests/integration/plugin_transmit_controls_tests.cpp`
  - `tests/integration/plugin_voice_mode_toggle_tests.cpp`
- Notes:
  - the strip status audit used the current narrow strip width budget of roughly `102-112 px`
  - long phrases were shortened only where they crowded or overflowed that width
  - unchanged short phrases such as `Transmitting`, `Healthy`, and `Warning` still fit cleanly

## Verification Run

- Build:
  - `cmake --build build-vs-2026 --target famalamajam_tests --config Debug`
- Tests:
  - `famalamajam_tests.exe "[plugin_mixer_ui],[plugin_transmit_controls],[plugin_voice_mode_toggle]"`
- Plugin rebuild:
  - `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`

## Final Commit

- Commit: pending
- Rollback: `git revert <final-commit>`
