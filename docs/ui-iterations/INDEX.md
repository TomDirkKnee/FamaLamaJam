# UI Iteration Index

## Baseline

- Current baseline commit before the next direct UI iteration: `dd0fafe`
- Strategy: one UI feature at a time, documented and reversible

## Entries

| ID | Status | Summary | Baseline | Final Commit | Rollback |
|----|--------|---------|----------|--------------|----------|
| `UI-000` | done | Strategy pivot away from GSD for UI development | `cc5f986` | `9471468` | `git revert 9471468` |
| `UI-001` | done | Expanded server settings should fill to the chat column and keep connect/disconnect visible | `cc5f986` | `e6c7913` | `git revert e6c7913` |
| `UI-002` | done | Move the pan pot into the strip control column and stretch strips to fill the mixer height | `e6c7913` | `85ef11e` | `git revert 85ef11e` |
| `UI-003` | done | Refresh local strip layout immediately after add or remove so channels do not disappear until another resize | `85ef11e` | `fdc21f2` | `git revert fdc21f2` |
| `UI-004` | done | Superimpose the gain fader on the meter and reset gain to 0 dB on double-click | `fdc21f2` | `f177eb2` | `git revert f177eb2` |
| `UI-005` | done | Widen the full-height meter lane and enlarge the pan pot plus strip buttons to reduce dead space | `f177eb2` | `b7aa93c` | `git revert b7aa93c` |
| `UI-006` | done | Make mute, solo, and voice buttons use the boxed transmit style with state colors | `b7aa93c` | `dfe9734` | `git revert dfe9734` |
| `UI-007` | done | Add left/right mixer scrolling when strip groups overflow the available viewport width | `dfe9734` | `cb6811c` | `git revert cb6811c` |
| `UI-008` | done | Add subtle bordered group containers around local strips and each remote user group | `cb6811c` | `380f09e` | `git revert 380f09e` |
| `UI-009` | done | Replace the local collapse button with a side-mounted chevron tab attached to the group border | `380f09e` | `0f86dbc` | `git revert 0f86dbc` |
| `UI-010` | done | Enlarge and lower the local add/remove header buttons so the plus/minus render cleanly inside the header | `0f86dbc` | `3463247` | `git revert 3463247` |
| `UI-011` | done | Remove the extra subtitle line from mixer strips and keep only the title plus status line | `3463247` | `3778e48` | `git revert 3778e48` |
| `UI-012` | done | Hide the redundant `1 channel` remote count so single-channel user names keep the full group header width | `3778e48` | `79398c0` | `git revert 79398c0` |
| `UI-013` | done | Shorten the expanded local group title from `Local Sends` to `Local` so it fits the narrow header more cleanly | `79398c0` | `074c914` | `git revert 074c914` |
| `UI-014` | done | Audit strip status phrases against the narrow header width and shorten only the ones that overflow | `074c914` | `d707574` | `git revert d707574` |
| `UI-015` | done | Keep compact strip status text on-screen but show the original full wording in a tooltip on hover | `d707574` | `b916b95` | `git revert b916b95` |
| `UI-016` | done | Make the remote output selector span the strip width and show compact output names in the selector text | `b916b95` | `0853e6b` | `git revert 0853e6b` |
| `UI-017` | done | Stretch the footer across the full shell, remove the sync-assist note, and restore a pan-style metronome volume knob | `0853e6b` | `19ae33a` | `git revert 19ae33a` |
| `UI-018` | done | Keep the room chat sidebar top-aligned when server settings are expanded so the revealed space is not wasted | `19ae33a` | `dd0fafe` | `git revert dd0fafe` |
| `UI-019` | done | Add a Normal/Compact in-plugin size selector that shrinks the mixer/server/footer area while leaving the chat width unchanged | `dd0fafe` | `8744c99` | `git revert 8744c99` |
| `UI-020` | done | Replace the metronome knob with a horizontal slider and make the footer side controls read as matched metronome/master sections | `8744c99` | pending | `git revert <final-commit>` |

## Next ID

- Next feature entry: `UI-021`
