# Phase 1 Research: Plugin Foundation & Session Configuration

**Phase:** 01-plugin-foundation-session-configuration  
**Date:** 2026-03-08  
**Inputs used:** `01-CONTEXT.md`, `REQUIREMENTS.md`, `STATE.md`, `ROADMAP.md`, `research/SUMMARY.md`

## 1. Phase 1 Scope for Planning

Phase 1 should only deliver foundations for:
- `SESS-01`: Editable Ninjam endpoint/credentials (username-only in v1 Phase 1 scope)
- `HOST-02`: Save/restore core session settings in Ableton project state

Non-goals for this phase:
- No live connection lifecycle (`SESS-02/03`)
- No timing/sync implementation (`SYNC-*`)
- No send/receive stream behavior (`AUD-*`)

## 2. JUCE 8 + VST3 Scaffold Choices (Windows-First)

### Recommended stack
- JUCE `8.0.12` pinned (exact version/commit, no floating branch)
- VST3 target only for Phase 1
- MSVC 2022, x64 builds only
- C++20 mode
- Plugin format: `VST3` only, no standalone target yet

### Scaffold architecture
Use a layered source layout from day one so Phase 2+ can add behavior without refactors:
- `src/plugin/`: `AudioProcessor`, `AudioProcessorEditor`, host glue
- `src/app/`: settings use-cases and command handlers (non-RT)
- `src/audio/`: RT-safe access points (currently minimal placeholders)
- `src/net/`: reserved for Phase 2 networking core
- `src/infra/`: serialization, defaults, validation, logging abstraction

### Why this scaffold
- Matches roadmap progression (Phase 1 -> 5) with minimal structural churn
- Keeps host boundary narrow and testable
- Prevents settings/persistence logic from leaking into audio callback path

## 3. CMake-First Project Setup Recommendations

### Build system baseline
- Top-level CMake as canonical build entry point
- Pin dependencies explicitly (JUCE, any third-party helpers)
- Generate Visual Studio solution from CMake only (avoid hand-edited `.sln` as source of truth)

### Target structure
- `famalamajam_core` (static lib): app/infra domain logic
- `famalamajam_plugin` (JUCE plugin target): host wrapper + editor
- Optional `famalamajam_tests` target for logic and serialization tests

### CMake policy for this phase
- Turn on strict warnings for project code (`/W4` + warnings-as-errors for local targets)
- Keep external dependency warnings suppressed
- Export `compile_commands.json` for tooling
- Keep runtime output paths deterministic so smoke scripts can locate artifacts

### Minimal CI posture for Phase 1
- Configure + build Debug/Release on Windows
- Run unit tests (settings + serialization)
- Run plugin validation tool step (pluginval and/or VST3 validator)

## 4. Secure/Simple Settings Model for SESS-01

### Data model (Phase 1)
Keep this small and explicit:
- `schemaVersion` (int)
- `serverHost` (string)
- `serverPort` (uint16/int)
- `username` (string)
- `defaultChannelGainDb` (float)
- `defaultChannelPan` (float)
- `defaultChannelMuted` (bool)

Do not store credentials beyond username in Phase 1.

### Validation rules
- `serverHost`: non-empty, trimmed, length bounded (for example <= 253)
- `serverPort`: numeric range `1..65535`
- `username`: non-empty after trim, sane max length (for example <= 64)
- Channel defaults clamped to defined ranges

### Apply model
- UI edits live in a draft view-model
- `Apply` performs validation and commits atomically to active settings snapshot
- No auto-apply per keystroke
- Commit path runs on message thread, never in audio callback

### Safety defaults
- On first load: populate sensible defaults
- On invalid input: keep previous valid active settings, surface warning
- On corrupt restored state: reset to defaults and warn

## 5. Ableton Plugin State Serialization Patterns for HOST-02

Use JUCE host-state hooks with versioned payloads:
- `getStateInformation(juce::MemoryBlock&)`
- `setStateInformation(const void*, int)`

### Recommended serialization pattern
1. Represent persisted state as `ValueTree` root with explicit type and `schemaVersion`.
2. Store only core session settings fields (no runtime connection state, no transient UI state).
3. Serialize tree to binary for compact host state payload.
4. On restore, parse defensively:
   - if parse fails: apply defaults + warning
   - if schema is older/newer: run migration or fallback defaults
5. After restore, force connection/session status to disconnected.

### Ableton-specific behaviors to design for
- State restore can occur before editor exists: do not depend on UI lifetime
- Track duplicate should clone settings deterministically
- Reopen/reload should not trigger network activity in Phase 1
- `setStateInformation` must be idempotent and non-throwing

### Migration stance (start now)
- Reserve `schemaVersion = 1` in Phase 1
- Centralize migration code path even if only v1 exists initially
- Keep unknown keys ignored (forward-compatible read)

## 6. Validation Architecture (Concrete Checks for Phase 1)

### A. Unit checks (fast, deterministic)
- Settings validation accepts valid host/port/username and rejects invalid values
- Draft -> Apply commit updates active snapshot only on valid input
- Serialization round-trip preserves all persisted fields
- Corrupt blob restore falls back to defaults
- Unknown fields do not crash restore path

### B. Integration checks (plugin-level)
- `AudioProcessor` starts with defaults when no state exists
- `setStateInformation` updates active settings used by processor/editor model
- `getStateInformation` output can be restored into a fresh processor instance
- Repeated save/restore cycles remain stable (no drift of values)

### C. Host/manual checks (Ableton on Windows)
- Insert plugin, edit settings, save project, close Ableton, reopen: settings restored
- Duplicate track instance: duplicated plugin has cloned settings
- Corrupt test state (via harness/injected blob): plugin stays loadable and resets safely
- Verify plugin remains disconnected on reopen

### D. Tooling checks
- Run plugin validator as Phase 1 gate
- Run leak/sanitizer equivalents available in Windows debug toolchain
- Add a lightweight regression script that automates build + tests + validator

### Suggested acceptance gate for Phase 1 planning
Phase 1 is complete only when all four are true:
1. `SESS-01` edit/apply flow works with validation and defaults
2. `HOST-02` save/restore works across Ableton reopen and duplicate workflows
3. Corrupt/invalid state handling is safe and user-visible
4. Validation suite (unit + integration + host manual matrix) passes

## 7. Risks and Tradeoffs

### Primary risks
- Mixing settings and runtime connection state in one payload causes reload side effects
- Schema/version negligence creates brittle future migrations
- Putting apply/serialization work on RT path can cause host glitching
- Over-coupling UI controls directly to processor internals increases Phase 2 refactor cost

### Tradeoffs to choose deliberately
- `ValueTree` binary vs custom JSON/XML:
  - Prefer `ValueTree` binary for JUCE-native host integration and lower plumbing cost
- APVTS-everything vs dedicated settings store:
  - Prefer dedicated settings domain object plus explicit serialization for non-automatable session config
- Monolith target vs layered libs:
  - Prefer layered libs now to reduce later rewrite cost

## 8. Recommended Sequence (Planner-Ready)

### Plan 01-01 (scaffold + settings domain)
1. Create JUCE/CMake VST3 project skeleton and directory layering
2. Add settings domain model + validation + defaults
3. Add serialization service with schema version and fallback behavior
4. Add unit tests for validation and serialization

### Plan 01-02 (UI + host state integration)
1. Implement minimal settings editor UI (host/port/username/channel defaults)
2. Implement draft/edit + explicit `Apply` flow
3. Wire JUCE state hooks (`getStateInformation`/`setStateInformation`)
4. Add plugin-level integration tests and Ableton manual validation checklist

### Expected handoff to Phase 2
Phase 2 should receive a stable, validated configuration subsystem with:
- atomic read access to active settings
- deterministic host restore behavior
- no automatic reconnect behavior coupled to restore/load

## 9. What Not to Hand-Roll in Phase 1

- Custom build system wrappers that bypass JUCE+CMake conventions
- Custom serialization format without migration/version support
- Custom UI state sync framework beyond simple draft/apply model
- Any network/session lifecycle behavior (defer to Phase 2)

---

**Planning note:** Keep Phase 1 narrow. The highest-value outcome is a stable foundation that makes Phase 2 connection lifecycle work straightforward and low-risk.
