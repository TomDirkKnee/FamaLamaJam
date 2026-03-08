# Requirements: FamaLamaJam

**Defined:** 2026-03-08
**Core Value:** Musicians can reliably join and complete real Ninjam rehearsals directly inside Ableton using a stable VST3 plugin workflow.

## v1 Requirements

### Connection & Session

- [ ] **SESS-01**: User can configure Ninjam server endpoint and credentials from the plugin UI.
- [ ] **SESS-02**: User can connect and disconnect from a Ninjam server without restarting Ableton.
- [ ] **SESS-03**: User can recover from transient network loss via automatic reconnect with clear status feedback.

### Timing & Sync

- [ ] **SYNC-01**: Plugin follows server-authoritative Ninjam interval timing (BPM/BPI/interval boundaries).
- [ ] **SYNC-02**: User can see current interval/metronome state in the UI.
- [ ] **SYNC-03**: Interval timing remains stable across at least a full rehearsal session without cumulative drift.

### Audio Streaming

- [ ] **AUD-01**: User can send local input audio to the session using a working Ninjam-compatible encode path.
- [ ] **AUD-02**: User can receive and decode remote participant audio in-session.
- [ ] **AUD-03**: Audio pipeline handles supported host sample rates and buffer sizes without pitch/tempo artifacts.

### Mixing & Monitoring

- [ ] **MIX-01**: User can control essential per-channel monitor/mix parameters (gain, mute, basic pan).
- [ ] **MIX-02**: UI clearly distinguishes local monitor audio from interval-delayed remote return.

### Host Reliability

- [ ] **HOST-01**: Plugin handles Ableton lifecycle events (load/unload/reopen/duplicate) without crashes or zombie sessions.
- [ ] **HOST-02**: Plugin state can be saved/restored in Ableton projects for core session settings.

### UI & Usability

- [ ] **UI-01**: User can perform end-to-end jam setup and start playing from a simple built-in JUCE UI.
- [ ] **UI-02**: User receives actionable status/error messages for connection, sync, and stream failures.

## v2 Requirements

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
| SESS-01 | Phase 1 - Plugin Foundation & Session Configuration | Pending |
| SESS-02 | Phase 2 - Connection Lifecycle & Recovery | Pending |
| SESS-03 | Phase 2 - Connection Lifecycle & Recovery | Pending |
| SYNC-01 | Phase 3 - Server-Authoritative Timing & Sync | Pending |
| SYNC-02 | Phase 3 - Server-Authoritative Timing & Sync | Pending |
| SYNC-03 | Phase 3 - Server-Authoritative Timing & Sync | Pending |
| AUD-01 | Phase 4 - Audio Streaming, Mix, and Monitoring Core | Pending |
| AUD-02 | Phase 4 - Audio Streaming, Mix, and Monitoring Core | Pending |
| AUD-03 | Phase 4 - Audio Streaming, Mix, and Monitoring Core | Pending |
| MIX-01 | Phase 4 - Audio Streaming, Mix, and Monitoring Core | Pending |
| MIX-02 | Phase 4 - Audio Streaming, Mix, and Monitoring Core | Pending |
| HOST-01 | Phase 5 - Ableton Reliability & v1 Rehearsal UX Validation | Pending |
| HOST-02 | Phase 1 - Plugin Foundation & Session Configuration | Pending |
| UI-01 | Phase 5 - Ableton Reliability & v1 Rehearsal UX Validation | Pending |
| UI-02 | Phase 5 - Ableton Reliability & v1 Rehearsal UX Validation | Pending |

**Coverage:**
- v1 requirements: 15 total
- Mapped to phases: 15
- Unmapped: 0
- Coverage: 100%

---
*Requirements defined: 2026-03-08*
*Last updated: 2026-03-08 after roadmap creation and traceability mapping*
