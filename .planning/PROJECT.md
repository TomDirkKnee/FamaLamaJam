# FamaLamaJam

## What This Is

FamaLamaJam is a modern cross-platform Ninjam client built primarily as a VST3 audio plugin for DAWs, with Ableton Live as the primary host target. It rebuilds core JamTaba-style online jamming workflows in a JUCE 8 codebase, emphasizing reliability and practical rehearsal use over broad feature scope in v1. The long-term direction is a modernized, maintainable Ninjam client experience that can evolve beyond the legacy desktop-client model.

## Core Value

Musicians can reliably join and complete real Ninjam rehearsals directly inside Ableton using a stable VST3 plugin workflow.

## Milestone Status

### Validated Baseline

- [x] Windows-first v1 rehearsal baseline is working in Ableton.
- [x] Core Ninjam connect/sync/send/receive workflow is validated for small-group rehearsal use.
- [x] Basic monitor/mix controls and host-lifecycle reliability have passed the current milestone.

### Next Milestone

The next milestone expands the validated rehearsal baseline into a more complete collaboration client. The focus is host-sync assist research, room interaction features like chat and voting, server discovery/history, and a stronger JamTaba-inspired workflow UI without abandoning the current stable core.

## Requirements

### Validated

- [x] Build a JUCE 8-based VST3 Ninjam client focused on Ableton Live workflows.
- [x] Implement a working Ninjam encoder/decoder path required for real jam sessions.
- [x] Deliver jam-core functionality: connect, interval/metronome sync, channel send/receive, and essential monitor/mix controls.
- [x] Provide a simple functional UI using built-in JUCE libraries.
- [x] Produce a Windows-first v1 that is usable for real rehearsal sessions.

### Active

- [ ] Add collaboration and room-control features expected from a practical modern Ninjam client: chat, BPM/BPI voting, and server discovery/history.
- [ ] Research and prototype safe host-sync assist behavior for Ableton without breaking server-authoritative Ninjam timing.
- [ ] Evolve the UI toward a JamTaba-inspired layout with horizontal strips, clear local/remote distinction, and integrated room interaction.
- [ ] Investigate advanced Ninjam parity features such as room listen/live-feed behavior and voice chat mode for later implementation.

### Out of Scope

- Video chat - still not required for the next milestone.
- Full JamTaba parity in one step - features will be added selectively in phases.
- Advanced routing matrix/patchbay work - still deferred until collaboration/core parity is stronger.
- Broad packaging/distribution polish - still secondary to feature completion and reliability.

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
| Build as JUCE 8 VST3 plugin first (Ableton-first workflow) | DAW-native jamming is the primary product value | Validated in the completed v1 baseline |
| Prioritize stable jam functionality over full JamTaba parity | Reliability for real rehearsals is v1 success criteria | Kept for the completed v1 baseline |
| Exclude video chat and advanced routing from v1 | Reduces complexity and keeps implementation focused | Maintained; still deferred |
| Use simple built-in JUCE UI for v1 | Speeds delivery and emphasizes functional correctness first | Validated, now ready for layout evolution |
| Ship Windows-first in v1 | Faster path to usable milestone before wider platform expansion | Validated baseline reached |
| Treat Ableton sync assist as research/prototype work, not assumed parity | Host transport cannot be controlled the same way as server-authoritative Ninjam timing | Next milestone Phase 6 |
| Add room interaction before advanced parity extras | Chat, voting, and server discovery are higher-value than deeper parity features like voice mode | Next milestone ordering locked |

---
*Last updated: 2026-03-16 after v1 baseline close-out and next-milestone roadmap setup*
