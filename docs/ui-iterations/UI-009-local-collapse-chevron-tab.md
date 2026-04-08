# UI-009: Local Collapse Chevron Tab

- Status: done
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `380f09e`

## Requested Change

Replace the local channel collapse button with a chevron tab attached to the side of the rounded local group border.

## Why

The current collapse button still reads like a regular header control. A side-mounted chevron tab should feel more like part of the local group container and free up header space.

## Non-Goals

- remote group changes
- strip redesign
- chat or footer changes
- channel behavior changes

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessorEditor.h`
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`

## References

- Notes:
  - remove the text-style collapse control from the local header row
  - keep `+` and `-` in the header
  - attach a small chevron tab to the right side of the local group border
  - flipped chevron should still show expand vs collapse state

## Implementation Notes

- Reuse the current local group layout and move only the collapse affordance.
- Keep the tab non-disruptive to the new group border and horizontal scrolling.
- Add focused regression coverage for the tab being positioned on the group edge instead of inside the header cluster.

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_mixer_ui`
- Manual:
  - confirm local `+/-` remain in the header
  - confirm the collapse control is a side tab on the local group border
  - confirm collapse/expand still works correctly

## Outcome

- Actual files changed:
  - `src/plugin/FamaLamaJamAudioProcessorEditor.h`
  - `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
  - `tests/integration/plugin_mixer_ui_tests.cpp`
  - `docs/ui-iterations/INDEX.md`
  - `docs/ui-iterations/UI-008-group-borders-around-local-and-remote-users.md`
- Notes:
  - the local collapse control is now a narrow chevron tab attached to the right edge of the local group border
  - `+` and `-` remain in the header row while the tab is positioned from the local group bounds instead of the header bounds
  - collapsed local width no longer reserves header space for the old text collapse button

## Verification Run

- Build:
  - `cmake --build build-vs-2026 --target famalamajam_tests --config Debug`
- Tests:
  - `famalamajam_tests.exe "[plugin_mixer_ui]"`
- Plugin rebuild:
  - `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`

## Final Commit

- Commit: `0f86dbc`
- Rollback: `git revert 0f86dbc`
