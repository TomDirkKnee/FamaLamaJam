# UI-000: Move UI Development To Direct Iteration

- Status: done
- Requested: 2026-04-08
- Requested by: user
- Baseline commit: `cc5f986`

## Requested Change

Move UI development away from the GSD phase workflow and switch to direct implementation driven by one user-requested UI change at a time.

## Why

The current UI work needs tighter control, smaller steps, and clearer rollback points than the GSD phase flow is giving.

## Non-Goals

- Rewriting existing GSD planning history
- Deleting old planning artifacts
- Changing non-UI systems unless a future UI request explicitly requires it

## Expected Files

- `docs/ui-iterations/README.md`
- `docs/ui-iterations/INDEX.md`
- `docs/ui-iterations/TEMPLATE.md`
- `docs/ui-iterations/UI-000-strategy-pivot.md`

## References

- Mockup: [phase9-layout-reboot-mockup.html](/C:/Users/Dirk/Documents/source/FamaLamaJam/docs/mockups/phase9-layout-reboot-mockup.html)
- Notes: Future UI changes should reference a specific screenshot, mockup, or written request where possible.

## Implementation Notes

- Each future UI request should become its own `UI-###` entry.
- Each finished UI request should be committed atomically with its documentation.
- Preferred rollback path is `git revert <commit>` so the history stays readable.

## Verification Plan

- Build: not required for this documentation-only pivot
- Tests: none
- Manual: confirm the workflow files exist and are clear enough to use for the next UI request

## Outcome

- Actual files changed:
  - `docs/ui-iterations/README.md`
  - `docs/ui-iterations/INDEX.md`
  - `docs/ui-iterations/TEMPLATE.md`
  - `docs/ui-iterations/UI-000-strategy-pivot.md`
- Notes:
  - This entry establishes the process only.
  - The first actual direct UI implementation should start at `UI-001`.

## Final Commit

- Commit: pending
- Rollback: `git revert <final-commit>`
