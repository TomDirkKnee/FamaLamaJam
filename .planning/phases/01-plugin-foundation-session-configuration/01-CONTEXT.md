# Phase 1: Plugin Foundation & Session Configuration - Context

**Gathered:** 2026-03-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Establish the JUCE/VST3 project foundation and session configuration baseline for v1, including editable core settings and safe project-state persistence in Ableton. This phase does not implement connection lifecycle, timing sync, or audio streaming behavior.

</domain>

<decisions>
## Implementation Decisions

### Build setup
- Canonical build system is **CMake-first** for reproducible builds and CI alignment.
- Phase 1 target output is **VST3 only** (standalone deferred to later phase).
- Dependencies use **strict pinning** (exact versions/commits for JUCE, VST3 SDK, protocol refs).
- Repository structure should start **layered** (`plugin/`, `app/`, `audio/`, `net/`, `infra/`) to support clean planning and testability.

### Settings model
- Minimum Phase 1 settings include **server endpoint/port, username, and basic channel default values**.
- Auth shape in Phase 1 is **username-only** (password/token concerns deferred).
- First-load behavior uses **sensible defaults** rather than blank forms.
- Settings changes apply via an **explicit Apply action**, not auto-apply per keystroke.

### State persistence
- Persist **core configuration only** (endpoint/port, username, channel defaults), not live connection/runtime state.
- On project reopen: **restore values but remain disconnected**.
- Include **explicit schema versioning from Phase 1** for migration safety.
- If persisted state is invalid/corrupt: **fall back to safe defaults and show warning**.

### Claude's Discretion
- Exact CMake layout and file naming within the layered module structure.
- Exact wording and visual treatment of warnings/errors in the simple JUCE UI.
- Internal serialization details as long as schema versioning and restore behavior match decisions above.

</decisions>

<specifics>
## Specific Ideas

- Keep Phase 1 intentionally narrow: foundation + configuration/persistence only.
- Preserve your v1 principle of implementation/functionality first, with simple built-in JUCE UI.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- No application source code exists yet; this phase defines the initial reusable project skeleton.

### Established Patterns
- Current project patterns come from planning decisions: Windows-first, CMake-first, strict pinning, and minimal v1 scope.

### Integration Points
- Must integrate with Ableton project state save/restore behavior through JUCE plugin state APIs.
- Must align with roadmap sequencing so Phase 2 can layer connection lifecycle on top of Phase 1 config model.

</code_context>

<deferred>
## Deferred Ideas

- Standalone target generation and validation.
- Credential model beyond username-only.
- Any connection lifecycle/reconnect behavior (Phase 2).
- Timing/metronome/interval sync behavior (Phase 3).

</deferred>

---

*Phase: 01-plugin-foundation-session-configuration*
*Context gathered: 2026-03-08*
