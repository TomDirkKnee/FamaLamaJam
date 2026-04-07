# Requirements: FamaLamaJam

**Defined:** 2026-03-08
**Core Value:** Musicians can reliably join and complete real Ninjam rehearsals directly inside Ableton using a stable VST3 plugin workflow.

## v1 Requirements

### Connection & Session

- [x] **SESS-01**: User can configure Ninjam server endpoint and credentials from the plugin UI.
- [x] **SESS-02**: User can connect and disconnect from a Ninjam server without restarting Ableton.
- [x] **SESS-03**: User can recover from transient network loss via automatic reconnect with clear status feedback.

### Timing & Sync

- [x] **SYNC-01**: Plugin follows server-authoritative Ninjam interval timing (BPM/BPI/interval boundaries).
- [x] **SYNC-02**: User can see current interval/metronome state in the UI.
- [x] **SYNC-03**: Interval timing remains stable across at least a full rehearsal session without cumulative drift.

### Audio Streaming

- [x] **AUD-01**: User can send local input audio to the session using a working Ninjam-compatible encode path.
- [x] **AUD-02**: User can receive and decode remote participant audio in-session.
- [x] **AUD-03**: Audio pipeline handles supported host sample rates and buffer sizes without pitch/tempo artifacts.

### Mixing & Monitoring

- [x] **MIX-01**: User can control essential per-channel monitor/mix parameters (gain, mute, basic pan).
- [x] **MIX-02**: UI clearly distinguishes local monitor audio from interval-delayed remote return.

### Host Reliability

- [x] **HOST-01**: Plugin handles Ableton lifecycle events (load/unload/reopen/duplicate) without crashes or zombie sessions.
- [x] **HOST-02**: Plugin state can be saved/restored in Ableton projects for core session settings.

### UI & Usability

- [x] **UI-01**: User can perform end-to-end jam setup and start playing from a simple built-in JUCE UI.
- [x] **UI-02**: User receives actionable status/error messages for connection, sync, and stream failures.

## Next Milestone Requirements

### Host Sync Assist

- [x] **HSYNC-01**: User can align Ableton tempo/loop helpers to room BPM/BPI without violating server-authoritative Ninjam timing.
- [x] **HSYNC-02**: Any host-sync assist behavior is explicit, optional, and understandable from the plugin UI.

### Room Interaction

- [x] **ROOM-01**: User can send and receive Ninjam room chat messages from inside the plugin.
- [x] **ROOM-02**: User can issue BPM/BPI vote commands from the plugin UI and see room feedback/results.

### Server Discovery

- **DISC-01**: User can choose from a public Ninjam server list inside the plugin.
- **DISC-02**: Previously used private servers are remembered and shown alongside public entries.

### Mixer & Layout

- [x] **LAYOUT-01**: Mixer layout supports horizontal track presentation with local monitor clearly distinguished from delayed remote strips.
- [x] **LAYOUT-02**: User can solo an individual mixer strip in addition to gain/pan/mute.
- [x] **LAYOUT-03**: Chat and room controls are integrated into the main plugin layout without obscuring transport or mixer workflows.

### NINJAM Parity Research

- **PAR-01**: Project documents the practical feasibility and protocol implications of room-listen/live-feed features.
- **PAR-02**: Project documents the feasibility and desired UX model for NINJAM voice chat mode before implementation.

### Voice Mode

- **VOICE-01**: User can toggle the local channel between interval mode and voice mode from the plugin UI.
- **VOICE-02**: Plugin can send and receive voice-mode channels while other room channels remain in normal interval mode.

### Stem Capture

- **REC-01**: User can save transmitted and received stems to a chosen folder for later DAW import.

### Host Routing Research

- [x] **ROUTE-01**: Project documents the practical JUCE/VST3/Ableton path for multiple local send inputs and per-stream output routing before implementation.
- **ROUTE-02**: User can transmit at least two separate local NINJAM channels from one plugin instance using host-routed fixed input buses (`Main` plus `Local Send 2`).
- **ROUTE-03**: User can route selected remote returns to fixed extra FLJ output pairs while leaving unrouted returns on the main FLJ output.
- **ROUTE-04**: Project documents and, where practical, implements an expanded fixed-bus target up to eight FLJ inputs and eight FLJ outputs instead of stopping at only `Main` plus `Local Send 2`.
- **ROUTE-05**: User can add and remove extra local transmit channels from the FLJ UI.
- **ROUTE-06**: Input-side local channel state persists across project restore, including local channel names, enabled extra slots, and bus-to-channel mapping.

## Later Requirements

### Platform Expansion

- **PLAT-01**: v1 feature set works on macOS VST3 hosts with compatibility validation.
- **PLAT-02**: Standalone application target reaches feature parity with plugin core.

### Parity & Advanced Controls

- **PAR-01**: Selected high-impact JamTaba parity features are added based on user demand.
- **PAR-02**: Advanced routing matrix/patchbay controls are available for power users.

### Workflow Enhancements

- **REL-01**: Advanced observability panel exposes packet/jitter/latency diagnostics.
- **UX-01**: Onboarding and session presets streamline first-time setup.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Video chat | Not required for core Ninjam rehearsal value in v1 |
| Full JamTaba parity in v1 | Would dilute focus from stability and functional core |
| Advanced routing in v1 | High complexity and UX overhead for initial release |
| Non-Windows release for v1 | Windows-first path reduces QA matrix and time-to-validation |
| Broad packaging/distribution polish in v1 | v1 goal is working functionality validated in real rehearsals |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| SESS-01 | Phase 1 - Plugin Foundation & Session Configuration | Complete |
| SESS-02 | Phase 2 - Connection Lifecycle & Recovery | Complete |
| SESS-03 | Phase 2 - Connection Lifecycle & Recovery | Complete |
| SYNC-01 | Phase 3 - Server-Authoritative Timing & Sync | Complete |
| SYNC-02 | Phase 3 - Server-Authoritative Timing & Sync | Complete |
| SYNC-03 | Phase 3 - Server-Authoritative Timing & Sync | Complete |
| AUD-01 | Phase 4 - Audio Streaming, Mix, and Monitoring Core | Complete |
| AUD-02 | Phase 4 - Audio Streaming, Mix, and Monitoring Core | Complete |
| AUD-03 | Phase 4 - Audio Streaming, Mix, and Monitoring Core | Complete |
| MIX-01 | Phase 4 - Audio Streaming, Mix, and Monitoring Core | Complete |
| MIX-02 | Phase 4 - Audio Streaming, Mix, and Monitoring Core | Complete |
| HOST-01 | Phase 5 - Ableton Reliability & v1 Rehearsal UX Validation | Complete |
| HOST-02 | Phase 1 - Plugin Foundation & Session Configuration | Complete |
| UI-01 | Phase 5 - Ableton Reliability & v1 Rehearsal UX Validation | Complete |
| UI-02 | Phase 5 - Ableton Reliability & v1 Rehearsal UX Validation | Complete |
| HSYNC-01 | Phase 6 - Ableton Sync Assist Research & Prototype | Complete |
| HSYNC-02 | Phase 6 - Ableton Sync Assist Research & Prototype | Complete |
| ROOM-01 | Phase 7 - Chat & Room Control Commands | Complete |
| ROOM-02 | Phase 7 - Chat & Room Control Commands | Complete |
| DISC-01 | Phase 8 / Phase 08.1 - Server Discovery & History / Server Discovery Polish & JamTaba Parity Check | Planned |
| DISC-02 | Phase 8 / Phase 08.1 / Phase 08.3 / Phase 08.3.1 - Server Discovery & History / Server Discovery Polish & JamTaba Parity Check / Functional Release Controls, Session UX, and Voice-Mode Compatibility Guard / Private Server Recall + Compact Session UI Polish | Planned |
| LAYOUT-01 | Phase 9 - JamTaba-Inspired Layout & Mixer Parity | Complete |
| LAYOUT-02 | Phase 08.3 / Phase 9 - Functional Release Controls, Session UX, and Voice-Mode Compatibility Guard / JamTaba-Inspired Layout & Mixer Parity | Complete |
| LAYOUT-03 | Phase 08.2 / Phase 08.3 / Phase 08.3.1 / Phase 9 - Pre-Layout CPU, Mixer, UI, and Auth Hardening / Functional Release Controls, Session UX, and Voice-Mode Compatibility Guard / Private Server Recall + Compact Session UI Polish / JamTaba-Inspired Layout & Mixer Parity | Complete |
| VOICE-01 | Phase 08.3.2 - Voice Mode Research & Prototype | Planned |
| VOICE-02 | Phase 08.3.2 - Voice Mode Research & Prototype | Planned |
| REC-01 | Phase 08.3.3 - Stem Capture & Export | Planned |
| ROUTE-01 | Phase 08.3.4 - Host Multi-I/O Routing Research | Complete |
| ROUTE-02 | Phase 08.3.4.1 - Fixed-Bus Multi-Input And NINJAM Channel Implementation | Complete |
| ROUTE-03 | Phase 08.3.4.1 - Fixed-Bus Multi-Input And NINJAM Channel Implementation | Complete |
| ROUTE-04 | Phase 08.3.4.2 - Expanded Fixed-Bus I/O And Persistent Input Mapping | Planned |
| ROUTE-05 | Phase 08.3.4.2 - Expanded Fixed-Bus I/O And Persistent Input Mapping | Planned |
| ROUTE-06 | Phase 08.3.4.2 - Expanded Fixed-Bus I/O And Persistent Input Mapping | Planned |
| PAR-01 | Phase 10 - Advanced NINJAM Parity Research | Planned |
| PAR-02 | Phase 10 - Advanced NINJAM Parity Research | Planned |

**Coverage:**
- v1 requirements: 15 total
- Mapped to phases: 15
- Unmapped: 0
- Coverage: 100%

---
*Requirements defined: 2026-03-08*
*Last updated: 2026-04-06 after closing the first fixed-bus implementation pass and inserting the routing-expansion follow-on phase*
