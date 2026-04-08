# UI-010: Local Add Remove Button Clarity

- Status: done
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `0f86dbc`

## Requested Change

Make the local `+` and `-` header buttons display their symbols properly and sit slightly lower so they no longer float on the top border line.

## Why

The current add/remove buttons are too cramped, which makes the plus/minus marks read poorly and leaves the buttons looking slightly misaligned against the local group boundary.

## Non-Goals

- collapse tab changes
- remote group changes
- strip layout changes
- footer, chat, or server settings changes

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`

## References

- Notes:
  - plus and minus should render clearly
  - the buttons should sit inside the header, not on the boundary line

## Implementation Notes

- Prefer a small size/placement adjustment over a broad style rewrite.
- Keep the existing local header control order and behavior.

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_mixer_ui`
- Manual:
  - confirm `+` and `-` are fully readable
  - confirm the buttons sit slightly lower and cleaner inside the header

## Outcome

- Actual files changed:
  - `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
  - `tests/integration/plugin_mixer_ui_tests.cpp`
  - `docs/ui-iterations/INDEX.md`
  - `docs/ui-iterations/UI-009-local-collapse-chevron-tab.md`
- Notes:
  - local `+ / -` header buttons are slightly larger and sit lower inside the header band
  - a compact glyph button look keeps the symbols centered and readable without changing the header control order

## Verification Run

- Build:
  - `cmake --build build-vs-2026 --target famalamajam_tests --config Debug`
- Tests:
  - `famalamajam_tests.exe "[plugin_mixer_ui]"`
- Plugin rebuild:
  - `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`

## Final Commit

- Commit: `3463247`
- Rollback: `git revert 3463247`
