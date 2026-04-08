# Phase 09.1 Strip-Only Mixer Rebuild On Stable Layout Validation Matrix

This matrix captures the final Phase 09.1 strip-usability checks that still need real Ableton judgment after the focused automation gate.

## Scope

- `LAYOUT-01`: narrow strip presentation remains practical at standard and narrower Ableton plugin widths
- `LAYOUT-02`: collapsed locals preserve visible meters and compact header controls without wasting strip width
- `LAYOUT-03`: strip polish does not destabilize the restored page shell around the mixer

## Rebuilt Artifact

- Build target: `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug`
- VST3 path: `build-vs-2026/famalamajam_plugin_artefacts/Debug/VST3/FamaLamaJam.vst3`
- Focused automation already green for:
  - compact local header controls and collapsed local mini-strip meter visibility
  - compact remote routing selector sizing inside grouped remote strips
  - disconnected setup, right sidebar, and footer timing/sync shell stability after the strip polish

## Manual Matrix

| ID | Requirement | Scenario | Ableton / FLJ Setup | Expected Result | Result | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| `P91-STRIP-01` | `LAYOUT-01` | Standard-width strip readability | Load the rebuilt Debug VST3 in Ableton at a practical plugin width with at least two local strips and at least one grouped remote user visible. | The strip field reads as the primary workspace, local strips stay compact, and no strip controls feel oversized relative to the stable shell. | fail | Channels only fill vertical space halfway; the gain fader is not integrated into the meter; local solo/mute/voice controls are obscured or pushed too far right; the pan pot is too small; overall plugin width should increase by about a quarter so more channels display before scrolling. |
| `P91-STRIP-02` | `LAYOUT-02` | Collapsed locals keep useful mini strips | At a narrower plugin width, collapse the local group while meters are active. | Collapsed locals still show full-height visible meter mini strips, the header controls stay compact, and the collapse state recovers strip width instead of wasting it on controls. | fail | Collapse controls still waste width when locals are collapsed; add/remove local buttons need explicit `+` and `-` affordances. |
| `P91-STRIP-03` | `LAYOUT-02` | Remote grouped strips stay readable with routing visible | Keep at least one remote user group visible and inspect the per-strip routing affordance at practical widths. | Remote groups remain readable, routing stays discoverable, and the routing affordance does not make the strips feel too wide or crowded. | fail | Overall strip width and density still leave too few visible channels before scrolling; compact routing and readability are not yet strong enough in practice. |
| `P91-STRIP-04` | `LAYOUT-03` | Stable shell beside the rebuilt strip field | While disconnected and while connected, inspect the top connect/session area and the right room sidebar beside the rebuilt strip field. | The restored shell still reads naturally: the connect workflow stays usable above the mixer and the room sidebar remains available without the strip polish crowding it out. | fail | The server connection area is unnecessarily full width and steals room from chat; connect/disconnect only appears when settings are hidden and should also appear on the settings page; the boxes look visually misaligned because of the label/text layout. |
| `P91-STRIP-05` | `LAYOUT-03` | Footer stability after strip polish | With room timing active, inspect the footer while resizing between standard and narrower widths. | Interval timing, sync assist, metronome, and master output remain readable below the strip field without the final strip polish destabilizing footer layout. | fail | The footer should be full width, and the metronome needs its volume control back as a knob/pot. |

## Manual Outcome Summary

- Automation is green for the focused strip compactness gate and for shell-stability regressions around the sidebar, session flow, and footer.
- Real Ableton validation failed all five `P91-STRIP-*` scenarios, so Phase 09.1 still does not have a usable strip-only recovery verdict.
- Preserve the host-design guidance for the next follow-up: use the Ableton fader template more literally, keep the meter long and genuinely integrated with gain, and make the local side controls button-like and TX-style rather than obscured tick-box-like controls.

## Host Verdict

- Verdict: failed
- Follow-up direction:
  - Use the Ableton fader template more literally.
  - Keep the meter long and actually integrated with gain.
  - Make the local side controls button-like, closer to TX style, not obscured tick-box-like controls.
