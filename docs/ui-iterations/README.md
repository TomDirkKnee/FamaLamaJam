# UI Iteration Workflow

This directory tracks direct UI work outside the GSD phase workflow.

## Goal

Make one UI change at a time, document it clearly, and keep every change easy to review and revert.

## Working Rules

1. The user proposes one UI change.
2. The change is documented before implementation starts.
3. Only that requested UI change is implemented in that pass.
4. Code and documentation for the change land together.
5. Each completed UI change gets a dedicated commit so rollback is straightforward.
6. Rollback should use a non-destructive `git revert <commit>` path whenever possible.

## Per-Change Process

For each UI change:

1. Create a new `UI-###-*.md` file from [TEMPLATE.md](/C:/Users/Dirk/Documents/source/FamaLamaJam/docs/ui-iterations/TEMPLATE.md).
2. Record:
   - the requested change
   - the baseline commit before work starts
   - the intended files to touch
   - any mockup or screenshot references
   - verification steps
3. Implement only that change.
4. Update the entry with:
   - actual files changed
   - test/build results
   - final commit hash
   - rollback command
5. Add or update the row in [INDEX.md](/C:/Users/Dirk/Documents/source/FamaLamaJam/docs/ui-iterations/INDEX.md).

## Scope Guardrails

- This workflow is for UI work first.
- If a request needs behavior or engine changes, document that explicitly in the entry before implementation.
- Avoid bundling unrelated polish into the same iteration.
- If a change needs a visual follow-up, treat that as a new UI entry rather than silently expanding scope.
