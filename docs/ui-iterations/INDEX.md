# UI Iteration Index

## Baseline

- Current baseline commit before the next direct UI iteration: `dfe9734`
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
| `UI-007` | in_progress | Add left/right mixer scrolling when strip groups overflow the available viewport width | `dfe9734` | pending | `git revert <final-commit>` |

## Next ID

- Next feature entry: `UI-008`
