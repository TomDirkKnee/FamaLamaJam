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
| `P8.3.4.1-ROUTE-03` | `ROUTE-03` | Routed remote isolation from main | In a room with at least one remote source, assign that strip to `Remote Out 1`, monitor `FLJ Main Output` and a separate Ableton track fed from `Remote Out 1 - FamaLamaJam`. | The assigned remote leaves `FLJ Main Output` completely and is audible only on the routed return track. | not run | No routed-remote host evidence was recorded before the checkpoint stopped on the `ROUTE-02` failure. |
| `P8.3.4.1-ROUTE-04` | `ROUTE-03` | Fixed extra output pair coverage | Assign one remote strip to `Remote Out 1` and another to `Remote Out 2`, leaving a third remote strip on `FLJ Main Output`. | The two assigned remotes land only on their respective routed outputs while the unassigned remote remains only on the main output. | not run | No host evidence was recorded for multi-route output coverage in this failed checkpoint. |
| `P8.3.4.1-ROUTE-05` | `ROUTE-03` | Aux silence when no route is active | Clear every remote output assignment and monitor the routed Ableton return tracks. | `Remote Out 1` and `Remote Out 2` stay silent when no remote strip is assigned to them. | not run | Aux silence after clearing assignments was not exercised before the checkpoint stopped. |
| `P8.3.4.1-ROUTE-06` | `ROUTE-02`, `ROUTE-03` | Restore routed output assignments after reopen | Save a set with a hidden extra local strip and at least one remote strip assigned away from main, then reopen the project. | Hidden local-strip state and remote output selections restore together, and invalid restored assignments fall back to `FLJ Main Output`. | not run | Restore coverage for hidden local strips and routed remote outputs remains unverified after the early manual failure. |

## Manual Outcome Summary

- `ROUTE-02` is blocked by real Ableton evidence: the sender exposed two transmitting local sends, but the receiving client still surfaced only one remote source (`jim#0`).
- `ROUTE-03` remains unverified in Ableton because the checkpoint stopped at the failed second-local-channel proof.
