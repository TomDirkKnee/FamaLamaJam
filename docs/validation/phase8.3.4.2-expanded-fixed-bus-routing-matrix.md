# Phase 08.3.4.2 Expanded Fixed-Bus Routing Matrix

This matrix captures the widened fixed-bus implementation outcome for the `8x8` phase and separates automated proof from the still-pending Ableton host verdict.

## Scope

- `ROUTE-04`: widened fixed-bus contract toward `8` stereo local inputs and `8` stereo host-facing outputs
- `ROUTE-05`: fixed extra-local lifecycle with reveal-next-hidden-slot, hide-and-remember behavior, and live-remove confirmation
- `ROUTE-06`: exact fixed-slot local restore on reopen, with remote output routing kept session-only and reset to `FLJ Main Output` on project restore

## Manual Matrix

| ID | Requirement | Scenario | Ableton / FLJ Setup | Expected Result | Result | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| `P8.3.4.2-CEILING-01` | `ROUTE-04` | Practical widened input ceiling | Load the widened VST3 in Ableton and inspect plugin input routing / sidechain choices up through `Local Send 8`. | Ableton exposes the practical stereo-input ceiling and each exposed local send is selectable and usable. | not run | Automated coverage proves the plugin now declares and processes the widened fixed local-slot table, but the actual Ableton ceiling is still unrecorded. |
| `P8.3.4.2-CEILING-02` | `ROUTE-04` | Practical widened output ceiling | Load the widened VST3 in Ableton and inspect plugin outputs up through `Remote Out 7 - FamaLamaJam`. | Ableton exposes the practical stereo-output ceiling and routed remotes can be assigned cleanly to every exposed pair. | not run | Automated routing tests prove the widened host-facing output contract and per-route isolation in-process; host proof is still pending. |
| `P8.3.4.2-LIFE-01` | `ROUTE-05` | Add channel reveals the next hidden fixed slot | Start with only `Main` visible, click `Add channel` repeatedly, and watch the extra strips appear. | Each click reveals the next hidden fixed slot in order rather than inventing or reordering slots. | pass (automated) | Covered by `plugin_mixer_ui` against the widened fixed-slot model. Manual Ableton rerun not yet performed. |
| `P8.3.4.2-LIFE-02` | `ROUTE-05` | Live remove requires confirmation and preserves prior state | Add an extra local slot, rename it, change gain/pan, turn transmit on, then remove and re-add it. | Live removal requires a short confirmation, hides the slot cleanly, and re-adding restores the prior name plus mix state. | pass (automated) | Covered by `plugin_mixer_ui`; DAW-side feel still needs a manual pass. |
| `P8.3.4.2-STATE-01` | `ROUTE-05`, `ROUTE-06` | Reopen preserves exact local slot mapping | Hide an extra local slot after renaming and changing its mix state, save, and reopen. | The same fixed slot restores hidden, keeps its prior name and mix state, and reappears intact when re-added. | pass (automated) | Covered by `plugin_state_roundtrip` using exact fixed-slot restore assertions. Manual Ableton reopen is still pending. |
| `P8.3.4.2-STATE-02` | `ROUTE-06` | Project reopen resets remote routes to main | Save with a remote strip routed away from main, then reopen the project. | Remote output routing resets to `FLJ Main Output` on reopen even though same-session reconnect preserves it. | pass (automated) | Covered by `plugin_state_roundtrip`; manual host reopen still pending. |
| `P8.3.4.2-SESSION-01` | `ROUTE-06` | Same-session reconnect keeps remote output assignment | Route a remote away from main, disconnect, reconnect within the same plugin session, and inspect the strip. | The remote keeps its output assignment for the live session only. | pass (automated) | Covered by widened runtime routing behavior and session-scoped assignment storage. |

## Automated Outcome Summary

- The plugin now declares and processes an `8`-slot local input model and an `8`-route output model.
- Extra local slots are fixed, revealable in order, hideable with confirmation while live, and restored by exact slot identity.
- Remote output routing now remains in memory for the live session but is intentionally not project-persistent across restore.

## Host Verdict

- `P8.3.4.2-VERDICT-01`: pending manual Ableton validation.
- Current automated verdict: implementation complete for the widened fixed-bus contract and lifecycle semantics.
- Remaining host-proof gap: the practical Ableton-exposed ceiling for widened inputs and outputs is still unverified in this phase artifact.
