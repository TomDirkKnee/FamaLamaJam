# Phase 8 Server Discovery Validation Matrix

**Phase:** 08-server-discovery-history  
**Plan:** 08-03  
**Host:** Ableton Live on Windows  
**Plugin artifact:** `build-vs/famalamajam_plugin_artefacts/Debug/VST3/FamaLamaJam.vst3`

Use this matrix during the manual Ableton pass. Record what actually happened in the plugin window.

| ID | Scenario | Steps | Expected plugin outcome | Result | Notes |
|----|----------|-------|-------------------------|--------|-------|
| P8-DISC-01 | Remembered private server appears after success | Connect successfully to a private or manually typed server, disconnect, then reopen the plugin or reconnect flow. | The successful server appears in the discovery picker as a remembered entry. Failed attempts should not create new history entries. | not tested | Could not test because no private server was available during the manual pass. |
| P8-DISC-02 | Remembered entries appear before public entries | Open the plugin with both remembered history and public discovery available. | Remembered/private entries are listed first, public entries follow after them, and duplicate endpoints are not shown twice. | not tested | Could not test because no remembered private server was available during the manual pass. |
| P8-DISC-03 | Picker selection prefills Host/Port without auto-connect | Select one remembered or public entry from the picker. | Host and Port fields update to match the selected server, but the plugin does not connect until Connect is pressed. | pass | Selection prefills Host/Port correctly without auto-connecting. |
| P8-DISC-04 | Manual typing still works naturally after selection | Select a picker entry, then manually edit Host and/or Port before connecting. | Manual edits remain possible and Connect uses the edited Host/Port values rather than locking the user to the picker selection. | pass | Manual Host/Port edits still work after picker selection. |
| P8-DISC-05 | Stale/failure status is understandable | Use Refresh when the public list is unavailable, stale, or temporarily failing, or observe the status copy after a failure. | Status text clearly communicates the refresh problem and whether cached servers are being shown, without blocking manual connection. | pass | Failure/stale messaging was understandable and manual connect remained usable. |
| P8-DISC-06 | Compact connection area remains readable | Use the plugin at a practical rehearsal window size with discovery controls visible. | The picker, refresh button, Host/Port fields, and status line remain understandable without crowding the page excessively. | fail | Usable enough to test, but the connection area now needs a real layout redesign rather than more small tweaks. |

## Session Notes

- Room/server tested:
- Ableton version:
- Plugin build timestamp:
- Anything surprising about discovery ordering, autofill, stale status wording, or UI density: The visible user count in entries like `1/8` never seemed to change during manual testing, so the current interpretation may be incomplete. Also, after pressing `Connect`, the picker jumps to a different server entry than the one actually selected/connected, which feels buggy.
