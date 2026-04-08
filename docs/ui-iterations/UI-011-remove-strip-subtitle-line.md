# UI-011: Remove Strip Subtitle Line

- Status: in_progress
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `3463247`

## Requested Change

Remove the second line of descriptive text in each mixer strip so only the strip title/name and the status line remain.

## Why

The extra subtitle line is visually noisy and not adding enough value to justify the vertical space it takes in the strip header.

## Non-Goals

- status line changes
- strip control changes
- group border changes
- footer, chat, or server settings changes

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`

## References

- Notes:
  - remove lines such as `Live monitor`, `Local Send 2`, and similar strip subtitles
  - keep the title/name
  - keep the colored status line

## Implementation Notes

- Prefer removing the subtitle from strip layout rather than changing strip data contracts.
- Add focused coverage that the subtitle text no longer renders while status text still does.

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_mixer_ui`
- Manual:
  - confirm the second line is gone from local and remote strips
  - confirm status text still appears under the title

## Outcome

- Actual files changed:
  - `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
  - `tests/integration/plugin_mixer_ui_tests.cpp`
- Notes:
  - strip subtitles are no longer rendered or kept alive as visible text components
  - local and remote strips now show only the title/name plus the colored status line

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
