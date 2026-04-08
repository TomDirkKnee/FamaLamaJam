# UI-008: Group Borders Around Local And Remote Users

- Status: in_progress
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `cb6811c`

## Requested Change

Add clear visual borders or boundaries around each mixer user grouping so local channels are framed together and each remote user gets a separate framed group.

## Why

Once the mixer gets busy, the strip lane is harder to scan by user. Group containers should make local channels and each remote user stand out more clearly.

## Non-Goals

- strip control redesign
- chat or footer changes
- server settings changes
- routing or transport logic changes

## Expected Files

- `src/plugin/FamaLamaJamAudioProcessorEditor.h`
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`

## References

- Notes:
  - local channels should share one visible border
  - each remote user should have a separate visible border
  - the borders should be subtle, not heavy

## Implementation Notes

- Use the existing group layout blocks in the mixer rather than adding another nested layout system.
- Keep the current strip positions inside each user group.
- Add focused regression coverage that proves each group gets a container rectangle and that those containers wrap the correct strips.

## Verification Plan

- Build: targeted UI test binary run
- Tests:
  - `plugin_mixer_ui`
- Manual:
  - confirm local channels sit inside one bordered group
  - confirm each remote user group has its own bordered container
  - confirm scrolling still works with the new grouping

## Outcome

- Actual files changed:
  - `src/plugin/FamaLamaJamAudioProcessorEditor.h`
  - `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
  - `tests/integration/plugin_mixer_ui_tests.cpp`
  - `docs/ui-iterations/INDEX.md`
  - `docs/ui-iterations/UI-007-horizontal-mixer-scroll-on-overflow.md`
- Notes:
  - mixer group containers are now painted behind the existing strip layout for the local group and each remote user group
  - each group container carries a subtle header count label so multi-channel users read more clearly at a glance
  - the containers are non-interactive and stay compatible with the horizontal overflow scrolling pass

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
