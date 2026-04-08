# UI Iteration Index

## Baseline

- Current baseline commit before the next direct UI iteration: `cc5f986`
- Strategy: one UI feature at a time, documented and reversible

## Entries

| ID | Status | Summary | Baseline | Final Commit | Rollback |
|----|--------|---------|----------|--------------|----------|
| `UI-000` | done | Strategy pivot away from GSD for UI development | `cc5f986` | `9471468` | `git revert 9471468` |
| `UI-001` | done | Expanded server settings should fill to the chat column and keep connect/disconnect visible | `cc5f986` | `e6c7913` | `git revert e6c7913` |
| `UI-002` | in_progress | Move the pan pot into the strip control column and stretch strips to fill the mixer height | `e6c7913` | pending | `git revert <final-commit>` |

## Next ID

- Next feature entry: `UI-003`
