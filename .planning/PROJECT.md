# FamaLamaJam

## What This Is

FamaLamaJam is a modern cross-platform Ninjam client built primarily as a VST3 audio plugin for DAWs, with Ableton Live as the primary host target. It rebuilds core JamTaba-style online jamming workflows in a JUCE 8 codebase, emphasizing reliability and practical rehearsal use over broad feature scope in v1. The long-term direction is a modernized, maintainable Ninjam client experience that can evolve beyond the legacy desktop-client model.

## Core Value

Musicians can reliably join and complete real Ninjam rehearsals directly inside Ableton using a stable VST3 plugin workflow.

## Requirements

### Validated

(None yet - ship to validate)

### Active

- [ ] Build a JUCE 8-based VST3 plugin Ninjam client focused on Ableton Live workflows.
- [ ] Implement a working Ninjam encoder/decoder path required for real jam sessions.
- [ ] Deliver jam-core functionality (connect, interval/metronome sync, channel send/receive, essential monitor/mix controls).
- [ ] Provide a simple functional UI using built-in JUCE libraries.
- [ ] Produce a Windows-first v1 that is usable for real rehearsal sessions.

### Out of Scope

- Video chat - not required for v1 functional jamming.
- Advanced routing features - deferred to keep v1 scope focused on reliability.
- Full JamTaba feature parity in v1 - functional core and stability come first.
- Public beta packaging/distribution polish - implementation and functionality are prioritized first.

## Context

The project is heavily influenced by the open-source JamTaba client (https://github.com/elieserdejesus/JamTaba) and aims to modernize that experience as a DAW-native plugin workflow. The primary user group includes both existing Ninjam power users and Ableton-focused producers, so v1 needs to preserve practical protocol behavior while keeping the UX approachable. The implementation target is JUCE 8.x with output as VST3 plugin and future standalone support; however, v1 delivery is Windows-first and plugin-first.

## Constraints

- **Framework**: JUCE 8.x - implementation must be built with JUCE (Projucer or CMake) to support plugin targets.
- **Plugin Target**: VST3 in DAW hosts (primarily Ableton Live) - core workflows must function reliably in-host.
- **Scope**: Jam core only for v1 - avoid non-essential features that risk rehearsal reliability.
- **UI**: Basic JUCE UI components - keep interface simple and functional in v1.
- **Platform**: Windows-first for v1 - defer broad cross-platform release work until core functionality is proven.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Build as JUCE 8 VST3 plugin first (Ableton-first workflow) | DAW-native jamming is the primary product value | - Pending |
| Prioritize stable jam functionality over full JamTaba parity | Reliability for real rehearsals is v1 success criteria | - Pending |
| Exclude video chat and advanced routing from v1 | Reduces complexity and keeps implementation focused | - Pending |
| Use simple built-in JUCE UI for v1 | Speeds delivery and emphasizes functional correctness first | - Pending |
| Ship Windows-first in v1 | Faster path to usable milestone before wider platform expansion | - Pending |

---
*Last updated: 2026-03-08 after initialization*
