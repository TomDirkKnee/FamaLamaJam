# UI-001: Expanded Server Settings Fill Available Space

- Status: done
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `cc5f986`

## Requested Change

When server settings are visible:

- the server-settings area should expand horizontally to fill the available space up to the Room Chat column
- the large dead gap before the chat column should be removed
- the connection action row should have enough vertical space to keep `Connect` / `Disconnect` visible

## Why

The current expanded layout wastes space, makes the settings area feel artificially cramped, and causes the connection actions to disappear when the settings are shown.

## Non-Goals

- mixer strip layout changes
- footer redesign
- chat workflow changes
- connection behavior or session logic changes

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_rehearsal_ui_flow_tests.cpp`
- `tests/integration/plugin_room_controls_ui_tests.cpp`

## References

- Screenshot: user-provided expanded-settings screenshot in chat
- Mockup: not required for this change
- Notes:
  - server settings should fill the space up to the chat column
  - `Connect` / `Disconnect` must remain visible when settings are shown
  - extra vertical room can be used to make that happen

## Implementation Notes

- Keep the right chat column as the boundary.
- Rework the expanded settings shell only.
- Prefer layout changes in `resized()` over wider structural refactors.
- Add or adjust focused UI regression coverage for expanded settings width and connection-action visibility.

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_rehearsal_ui_flow`
  - `plugin_room_controls_ui`
- Manual:
  - open plugin with server settings expanded
  - confirm the expanded settings area reaches to the chat column
  - confirm `Connect` / `Disconnect` remain visible

## Outcome

- Actual files changed:
  - `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
  - `tests/integration/plugin_rehearsal_ui_flow_tests.cpp`
  - `tests/integration/plugin_room_controls_ui_tests.cpp`
- Notes:
  - Expanded server settings now use the full left workspace width up to the chat/sidebar column instead of stopping at a capped shell width.
  - Expanded settings now reserve more vertical room so the `Connect` and `Disconnect` row remains visible.
  - Focused verification passed:
    - `famalamajam_tests.exe "[plugin_rehearsal_ui_flow],[plugin_room_controls_ui]"`
    - `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`

## Final Commit

- Commit: `e6c7913`
- Rollback: `git revert e6c7913`
