# Phase 08.1 Server Discovery Polish Validation Matrix

**Phase:** 08.1-server-discovery-polish-jamtaba-parity-check  
**Plan:** 08.1-03  
**Host:** Ableton Live on Windows  
**Plugin artifact:** `build-vs/famalamajam_plugin_artefacts/Debug/VST3/FamaLamaJam.vst3`

Use this matrix during the manual Ableton pass. Record what actually happened in the plugin window.

| ID | Scenario | Steps | Expected plugin outcome | Result | Notes |
|----|----------|-------|-------------------------|--------|-------|
| P8.1-DISC-01 | Public-room counts look believable | Refresh the public list and compare a few visible rooms against live expectations or JamTaba if available. | Counts should no longer look stuck on a suspicious constant value when richer data is available. | pass | Counts now looked believable in the live plugin workflow. |
| P8.1-DISC-02 | Public rooms are ordered by activity | Refresh the public list and inspect the top of the public section. | Busiest rooms should appear first within the public portion of the list. Remembered entries may still stay above them. | pass | Activity ordering looked correct. |
| P8.1-DISC-03 | Picker does not jump after Connect | Select a public room, press `Connect`, then observe the picker. | The picker should still identify the same endpoint you chose or connected to, not a different row. | pass | No jump after Connect. |
| P8.1-DISC-04 | Picker stays stable after refresh or reordering | Select a room, then use `Refresh` or wait for the list to update. | If the same endpoint still exists, the picker should remain attached to that endpoint even if row order changes. | pass | Stable after refresh/reordering. |
| P8.1-DISC-05 | Remaining UI complaint is only layout density | Use the plugin at a practical rehearsal window size. | Discovery should be workable enough to trust, even if the section still feels cramped and needs the planned Phase 9 redesign. | pass | Remaining complaint is layout density, not discovery correctness. |

## Session Notes

- Room/server tested:
- Ableton version:
- Plugin build timestamp:
- Anything surprising about counts, ordering, picker stability, or remaining layout pressure:
