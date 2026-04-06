# Phase 08.3.4.1 Fixed-Bus Routing Matrix

This matrix now records the real Ableton validation outcome for the first fixed-bus implementation pass.

## Scope

- `ROUTE-02`: two fixed local transmit slots using `Main` plus `Local Send 2`
- `ROUTE-03`: fixed remote output targets using `FLJ Main Output`, `Remote Out 1`, and `Remote Out 2`
- Host-owned Ableton routing only; no dynamic bus growth and no plugin-owned track enumeration

## Manual Matrix

| ID | Requirement | Scenario | Ableton / FLJ Setup | Expected Result | Result | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| `P8.3.4.1-LOCAL-01` | `ROUTE-02` | Separate local upload identities | Route one Ableton track to `Main`, a second track to `Local Send 2 - FamaLamaJam`, click `Add channel`, rename the extra strip, and connect a second NINJAM client. | The remote client sees two separately named local channels, with `Main` on channel `0` and `Local Send 2` on channel `1`. | fail | 2026-04-05 Ableton checkpoint failed. The sender showed two active local sends (`jim main` on Live monitor and `test main` on `Local Send 2`), both marked `Transmitting`, but the receiving client only showed one remote channel for user `jim` (`jim - Channel 1`). Receiver diagnostics showed only one remote source entry with `sourceId=jim#0`, `sub=yes`, and `mask=1`; no second local channel or `jim#1` source appeared. |
| `P8.3.4.1-LOCAL-02` | `ROUTE-02` | Global local controls with per-strip mix state | Add the extra local strip, set different gain/pan/mute or solo on each local strip, then toggle the global `Transmit` and `Voice` controls in the local header. | `Transmit` and `Voice` apply across active local channels without duplicating those controls on each strip, while per-strip mix state remains independent. | blocked | This row cannot be claimed pass after `P8.3.4.1-LOCAL-01` failed. The receiving client never exposed a second local NINJAM channel, so the required across-two-channel `ROUTE-02` behavior remains unproven. |
| `P8.3.4.1-LOCAL-03` | `ROUTE-02` | Hide-and-remember local slot restore | Remove or hide the extra local strip after renaming it and changing its mix state, save the Ableton set, then reopen it. | The extra local slot stays hidden until re-added and restores its last name plus per-strip mix state when it returns. | not run | The checkpoint stopped once the second local channel failed to appear on the receiving client. Reopen and restore behavior was not exercised in this pass. |
| `P8.3.4.1-ROUTE-03` | `ROUTE-03` | Routed remote isolation from main | In a room with at least one remote source, assign that strip to `Remote Out 1`, monitor `FLJ Main Output` and a separate Ableton track fed from `Remote Out 1 - FamaLamaJam`. | The assigned remote leaves `FLJ Main Output` completely and is audible only on the routed return track. | pass | 2026-04-06 Ableton rerun passed. The user confirmed output routing works and that the assigned remote can be isolated away from `FLJ Main Output` onto the routed return. |
| `P8.3.4.1-ROUTE-04` | `ROUTE-03` | Fixed extra output pair coverage | Assign one remote strip to `Remote Out 1` and another to `Remote Out 2`, leaving a third remote strip on `FLJ Main Output`. | The two assigned remotes land only on their respective routed outputs while the unassigned remote remains only on the main output. | not run | No host evidence was recorded for multi-route output coverage in this failed checkpoint. |
| `P8.3.4.1-ROUTE-05` | `ROUTE-03` | Aux silence when no route is active | Clear every remote output assignment and monitor the routed Ableton return tracks. | `Remote Out 1` and `Remote Out 2` stay silent when no remote strip is assigned to them. | not run | Aux silence after clearing assignments was not exercised before the checkpoint stopped. |
| `P8.3.4.1-ROUTE-06` | `ROUTE-02`, `ROUTE-03` | Restore routed output assignments after reopen | Save a set with a hidden extra local strip and at least one remote strip assigned away from main, then reopen the project. | Hidden local-strip state and remote output selections restore together, and invalid restored assignments fall back to `FLJ Main Output`. | not run | 2026-04-06 clarification: disconnect and reconnect remembers the extra channel and naming within the live plugin instance, but true saved-project reopen was not explicitly rerun. The user also noted that removing and reinstating the plugin loses the extra-channel state today. This row remains unverified for actual Ableton set reopen. |

## Manual Outcome Summary

- `ROUTE-02` was blocked in the first checkpoint, but later debugging fixed the second-channel publish and sidechain audibility path.
- `ROUTE-03` now has real Ableton proof: a routed remote can leave `FLJ Main Output` and land on the selected fixed output return.
- `ROUTE-06` still needs an explicit save-and-reopen Ableton rerun. Current evidence only proves in-instance disconnect/reconnect memory, not persisted project restore.

## Follow-Up Debug Notes

After this first blocked checkpoint, follow-up debugging moved the implementation forward:
- the second local channel began publishing as a real remote source
- local and remote sidechain audio became audible

A later receive-timing regression then appeared during troubleshooting:
- extra-channel playback dropped roughly once per interval
- some runs also showed main-channel refresh churn

That regression has been documented and the live boundary-inference path has been rolled back pending a fresh Ableton retest:
- [2026-04-06-fixed-bus-receive-timing-regression.md](/C:/Users/Dirk/Documents/source/FamaLamaJam/.planning/debug/2026-04-06-fixed-bus-receive-timing-regression.md)

## Follow-On UX Gaps

- The extra local channel currently has no user-facing remove action. Added local channels should be removable.
- Extra local-channel naming and slot state are remembered across disconnect/reconnect inside the current plugin instance, but they are not yet restored after plugin removal/reinstatement. Persisting local channel state alongside remembered connection data would be a good follow-on phase item.
