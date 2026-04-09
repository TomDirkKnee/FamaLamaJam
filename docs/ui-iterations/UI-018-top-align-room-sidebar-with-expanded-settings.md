# UI-018: Top Align Room Sidebar With Expanded Settings

- Status: done
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `19ae33a`

## Requested Change

Keep the room chat sidebar anchored to the top of the shell when server settings are expanded, instead of dropping it down and leaving empty space above the chat column.

## Why

The expanded server settings shell currently pushes the room chat sidebar downward, which wastes space and makes the right column feel disconnected from the rest of the interface.

## Non-Goals

- chat feature changes
- server settings field changes
- mixer strip changes
- footer changes

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_room_controls_ui_tests.cpp`

## References

- Notes:
  - the room chat column should stay top-aligned even when server settings are shown
  - the recovered vertical space should remain part of the room sidebar
  - diagnostics takeover behavior should remain unchanged

## Implementation Notes

- Prefer removing the artificial expanded-settings inset instead of compensating elsewhere.
- Keep the sidebar width and right-hand anchoring unchanged.

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_room_controls_ui`
- Manual:
  - confirm the room chat column no longer drops when server settings are expanded
  - confirm the chat area uses the recovered height

## Outcome

- Actual files changed:
  - `docs/ui-iterations/INDEX.md`
  - `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
  - `tests/integration/plugin_room_controls_ui_tests.cpp`
- Notes:
  - the artificial expanded-settings top inset on the room sidebar is removed, so the right column stays anchored to the top of the shell
  - the room chat label now sits above the expanded server field rows instead of dropping down beside them
  - diagnostics takeover still works, but the test now allows the diagnostics button and room label to share the same top line

## Verification Run

- Build:
  - `cmake --build build-vs-2026 --target famalamajam_tests --config Debug`
- Tests:
  - `famalamajam_tests.exe "[plugin_room_controls_ui]"`
- Plugin rebuild:
  - `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`

## Final Commit

- Commit: `dd0fafe`
- Rollback: `git revert dd0fafe`
