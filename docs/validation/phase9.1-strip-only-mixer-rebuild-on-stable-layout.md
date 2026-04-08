# Phase 09.1 Strip-Only Mixer Rebuild On Stable Layout Validation Matrix

This matrix captures the final Phase 09.1 strip-usability checks that still need real Ableton judgment after the focused automation gate.

## Scope

- `LAYOUT-01`: narrow strip presentation remains practical at standard and narrower Ableton plugin widths
- `LAYOUT-02`: collapsed locals preserve visible meters and compact header controls without wasting strip width
- `LAYOUT-03`: strip polish does not destabilize the restored page shell around the mixer

## Rebuilt Artifact

- Build target: `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`
- VST3 path: `build-vs-2026/famalamajam_plugin_artefacts/Debug/VST3/FamaLamaJam.vst3`
- Pre-checkpoint handoff: run the focused Wave 6 automation gate first, then rebuild this Debug VST3 as a separate artifact-preparation step immediately before the Ableton rerun.
- Focused automation already green for:
  - widened default shell geometry, visible connect/disconnect actions, and aligned settings rows
  - taller integrated mixer strips with larger pan pots and tighter local TX-style controls
  - tighter collapsed local reclaim width plus restored full-width footer and metronome knob

## Manual Matrix

| ID | Requirement | Scenario | Ableton / FLJ Setup | Expected Result | Result | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| `P91-STRIP-01` | `LAYOUT-01`, `LAYOUT-03` | Compact shell and settings visibility beside the strip field | Load the rebuilt Debug VST3 in Ableton at the widened default plugin size, then inspect both the disconnected shell and the expanded settings view. | The server settings shell no longer feels overly wide, connect or disconnect stays visible in both settings modes, and the settings boxes align cleanly without crowding the room sidebar. | pending rerun | Baseline blocked rerun failed because the connection area was too wide, hid connect/disconnect in settings mode, and left the label or field rows visually misaligned. |
| `P91-STRIP-02` | `LAYOUT-01`, `LAYOUT-02` | Standard-width strip density and integrated meter or gain spine | Keep at least two local strips and one grouped remote user visible at the widened default width. | Strips use most of the available mixer height, the meter and gain read as one dominant vertical control, and the pan pot is materially larger without making the strip feel bloated. | pending rerun | Baseline blocked rerun failed because the strips used only about half of the available height, the gain felt detached from the meter, and the pan pot stayed too small. |
| `P91-STRIP-03` | `LAYOUT-01`, `LAYOUT-02` | Compact local controls near the fader | With active local strips visible, inspect `M`, `S`, `TX`, and `INT/VOX` placement and readability at practical host widths. | Local controls stay compact, button-like, and close to the fader so the strip reads as one coherent control cluster. | pending rerun | Baseline blocked rerun failed because local side controls were obscured or drifted too far right from the fader. |
| `P91-STRIP-04` | `LAYOUT-01` | Collapsed locals reclaim width with explicit affordances | Resize narrower, collapse the local group while meters are active, and inspect the local header actions. | Collapsed locals reclaim meaningful strip width while preserving readable mini strips and explicit `+` and `-` affordances in the header. | pending rerun | Baseline blocked rerun failed because collapsed locals still wasted width and the add/remove affordances were not explicit enough. |
| `P91-STRIP-05` | `LAYOUT-03` | Footer and metronome geometry after the shell-plus-strip recovery | With room timing active, inspect the footer at the widened default size and one narrower size. | The footer spans the full editor width again, and the metronome volume knob is restored beside the transport and master controls. | pending rerun | Baseline blocked rerun failed because the footer did not span the full width and the metronome volume knob was missing. |

## Manual Outcome Summary

- The authoritative host verdict is still pending the fresh `09.1-06` Ableton rerun against the rebuilt Debug VST3.
- The rerun must cover the recovered shell width, visible connect/disconnect actions, aligned settings rows, taller integrated strips, larger pan pot, compact local controls, tighter collapsed locals, full-width footer, and restored metronome knob.
- Baseline guidance from the previously blocked rerun still applies if any rows fail again: keep the Ableton fader template literal, keep the meter genuinely integrated with gain, and keep local side controls button-like and TX-style rather than obscured tick-box-like controls.

## Host Verdict

- Verdict: pending rerun
- Current authority: the blocked pre-revision Ableton pass remains the baseline until `09.1-06` records fresh row outcomes.
