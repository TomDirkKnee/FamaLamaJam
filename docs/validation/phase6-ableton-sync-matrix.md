# Phase 6 Ableton Sync Assist Validation Matrix

Use this matrix against the freshly built Debug VST3 artifact:

- `build-vs/famalamajam_plugin_artefacts/Debug/VST3/FamaLamaJam.vst3`

Record pass or fail plus concise notes for each case. Keep notes specific about what Ableton exposed to the plugin, especially if host musical-position data is missing or inconsistent.

## Test Setup

- Host: Ableton Live on Windows
- Plugin: `FamaLamaJam.vst3` from the Debug build above
- Preconditions:
  - Connect to a room with real timing data.
  - Confirm the transport area shows room BPM and BPI.
  - Use the explicit `Arm Sync to Ableton Play` workflow only; do not treat this as an always-on sync mode.

## Cases

| ID | Scenario | Steps | Expected Result | Outcome | Notes |
|----|----------|-------|-----------------|---------|-------|
| P6-HSYNC-01 | Armed start from bar start with matching tempo | Match Ableton tempo to room BPM. Arm sync while Ableton is stopped at a bar boundary, then press Play. | Plugin begins aligned playback from the host musical position, clears the armed state after success, and does not remain in a waiting state. | pending | |
| P6-HSYNC-02 | Armed start from mid-bar with matching tempo | Match Ableton tempo to room BPM. Move Ableton playhead to the middle of a bar, arm sync, then press Play. | Plugin uses host PPQ and bar-start data for a mid-bar aligned start rather than forcing a bar-1 restart. If required host fields are missing, it fails visibly instead of guessing. | pending | |
| P6-HSYNC-03 | Tempo mismatch blocks arming | Change Ableton tempo away from room BPM and inspect the host-sync assist control. | Arm control stays disabled with a short inline explanation that the host tempo must match the room BPM before arming. | pending | |
| P6-HSYNC-04 | Timing loss cancels armed waiting | With matching tempo, arm sync and then force timing loss or reconnect before pressing Play. | Armed state cancels immediately, the UI explains that sync assist is no longer available, and no hidden waiting state remains. | pending | |
| P6-HSYNC-05 | Explicit cancel before Play | With matching tempo, arm sync and press the same control again before pressing Play in Ableton. | Waiting state is canceled cleanly, the control returns to its ready state, and later Play presses do nothing until the user arms again. | pending | |
| P6-HSYNC-06 | Failed start requires manual re-arm | Force a failed start condition after arming, such as starting playback without the required host musical-position fields if reproducible. Then press Play again without re-arming. | Failed start is shown explicitly, the one-shot arm is consumed, and later host Play presses remain inert until the user manually re-arms the assist. | pending | |
| P6-HSYNC-07 | Successful start resets one-shot arm | Complete a successful aligned start, stop Ableton, then press Play again without re-arming. | Plugin does not attempt another sync-assisted start until the user explicitly arms again. | pending | |

## Completion Notes

- Copy each final `Outcome` and `Notes` row into `.planning/phases/06-ableton-sync-assist-research-prototype/06-VERIFICATION.md`.
- If any case fails because Ableton does not expose stable host timing data, note exactly which fields or transitions appeared missing.
