# UI Iteration Index

## Baseline

- Current baseline commit before the next direct UI iteration: `fdc21f2`
- Strategy: one UI feature at a time, documented and reversible

## Entries

| ID | Status | Summary | Baseline | Final Commit | Rollback |
|----|--------|---------|----------|--------------|----------|
| `UI-000` | done | Strategy pivot away from GSD for UI development | `cc5f986` | `9471468` | `git revert 9471468` |
| `UI-001` | done | Expanded server settings should fill to the chat column and keep connect/disconnect visible | `cc5f986` | `e6c7913` | `git revert e6c7913` |
| `UI-002` | done | Move the pan pot into the strip control column and stretch strips to fill the mixer height | `e6c7913` | `85ef11e` | `git revert 85ef11e` |
| `UI-003` | done | Refresh local strip layout immediately after add or remove so channels do not disappear until another resize | `85ef11e` | `fdc21f2` | `git revert fdc21f2` |
| `UI-004` | done | Superimpose the gain fader on the meter and reset gain to 0 dB on double-click | `fdc21f2` | pending | `git revert <final-commit>` |

## Next ID

- Next feature entry: `UI-005`
