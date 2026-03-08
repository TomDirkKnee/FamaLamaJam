# Project Research Summary

**Project:** FamaLamaJam
**Domain:** DAW-native Ninjam online jamming client (JUCE 8 VST3)
**Researched:** 2026-03-08
**Confidence:** MEDIUM

## Executive Summary

This project is best approached as a reliability-first audio/plugin system, not as a broad feature clone. Research consistently points to a JUCE 8 + VST3 SDK stack with strict real-time boundaries between audio callback work and networking/protocol processing. For v1, the right strategy is to ship a narrow but solid jam core in Ableton on Windows: connect/auth, interval sync, working encode/decode send-receive, essential monitor/mix controls, and clear status/error feedback.

Feature research reinforces that full JamTaba parity, advanced routing, and integrated video chat should be deferred. They add substantial complexity and increase the risk of shipping an unstable rehearsal experience. The most critical risk is violating real-time and timing correctness constraints: audio-thread blocking, interval drift, and lifecycle leaks during plugin suspend/reload are the common failure modes that can invalidate the product's core value.

The recommended roadmap should therefore front-load architecture correctness (threading/state machine/protocol timing), then harden host lifecycle behavior, then layer usability and selected parity improvements after real-session validation.

## Key Findings

### Recommended Stack

Use JUCE 8.0.12 with VST3 SDK v3.8.0_build_66 and CMake (3.22+; recommend 3.29.x on dev machines), targeting MSVC 2022 for the Windows-first release path. Keep Ninjam behavior aligned to a pinned reference implementation and prefer pinned dependency versions across the project to preserve timing-sensitive reproducibility.

**Core technologies:**
- JUCE 8.0.12: plugin/application framework and cross-platform abstraction - best fit for VST3-first workflow.
- VST3 SDK v3.8.0_build_66: plugin API baseline and validator tooling - required for robust host compatibility.
- CMake + MSVC 2022: deterministic build/toolchain baseline - reduces integration drift during early phases.

### Expected Features

Research aligns with your stated direction: jam-core functionality is table stakes and should define v1. Differentiators should be limited to low-friction DAW-native workflow and reliability-focused recovery UX.

**Must have (table stakes):**
- Reliable Ninjam server connect/auth flow.
- Correct interval/metronome sync semantics.
- Working audio encode/decode send-receive path.
- Essential per-channel monitor/mix controls.
- Basic real-time status/error UI.

**Should have (competitive):**
- Ableton-tuned workflow defaults.
- Reconnect/recovery behavior with clear user feedback.
- Session recall that remains stable across reloads.

**Defer (v2+):**
- Full JamTaba parity.
- Advanced routing matrix.
- Built-in video/social features.
- Broad multi-platform release and standalone polish.

### Architecture Approach

Use a layered design with explicit boundaries: `plugin` (host glue), `app/session core` (state machine and commands), `audio` (RT-safe capture/mix/buffers), `net/protocol` (transport and Ninjam framing), and `infra` (config/logging). Data should move via lock-free queues between real-time and non-real-time contexts. Session lifecycle must be modeled as explicit states (idle/connecting/syncing/active/reconnecting/error) to prevent teardown and recovery bugs in host environments.

**Major components:**
1. Plugin host boundary (`juce::AudioProcessor`) - lifecycle, parameters, host integration.
2. Session/timing core - authoritative state machine, interval logic, command handling.
3. Audio + protocol pipeline - RT-safe buffering with off-thread encode/decode/transport.

### Critical Pitfalls

1. **Audio-thread blocking** - keep all network/decode/logging off callback path via lock-free bridges.
2. **Incorrect interval semantics** - implement server-authoritative interval timing and verify with deterministic tests.
3. **Sample-rate/buffer mismatch drift/artifacts** - validate 44.1/48/96 kHz and dynamic block-size changes.
4. **Plugin lifecycle leaks** - enforce idempotent init/teardown and test suspend/reload/duplicate flows.
5. **Monitor UX confusion (local vs interval-return)** - keep controls explicit and defaults safe.

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: Realtime Foundation
**Rationale:** All later features depend on safe audio-thread architecture.
**Delivers:** Plugin skeleton, RT-safe queues/buffers, baseline state model.
**Addresses:** Core stability requirements.
**Avoids:** Audio callback blocking pitfall.

### Phase 2: Protocol + Timing Core
**Rationale:** Correct Ninjam semantics are central product value.
**Delivers:** Connect/auth, interval sync, transport framing, timing tests.
**Uses:** Pinned Ninjam reference behavior.
**Implements:** Session state machine + timing engine.

### Phase 3: Audio Send/Receive Pipeline
**Rationale:** Functional jam path is needed for real rehearsal validation.
**Delivers:** Encode/decode integration, jitter handling, monitor path.
**Uses:** RT/non-RT boundary from Phase 1.
**Implements:** End-to-end streaming path.

### Phase 4: Ableton Lifecycle Hardening
**Rationale:** v1 success depends on DAW reliability under real workflows.
**Delivers:** Robust suspend/reload/reopen behavior, error recovery.
**Uses:** Host compatibility checks and lifecycle tests.

### Phase 5: Essential UI + v1 Rehearsal Validation
**Rationale:** Users need clear control and status to complete rehearsals.
**Delivers:** Simple JUCE UI, channel essentials, status/error feedback, UAT pass.

### Phase Ordering Rationale

- Architecture and threading constraints must be solved before feature layering.
- Protocol/timing correctness must precede UX polish to avoid false confidence.
- Host lifecycle hardening is required before claiming rehearsal reliability.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 2:** exact Ninjam protocol edge behavior and reconnect semantics.
- **Phase 4:** host lifecycle variance across Ableton versions/settings.

Phases with standard patterns (can likely skip extra research-phase):
- **Phase 1:** standard RT-safe audio architecture patterns in JUCE/VST3.
- **Phase 5:** basic plugin UI composition with bounded control scope.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Strong primary-source grounding for JUCE/VST3/toolchain baselines. |
| Features | MEDIUM | Category quality is strong, but competitive landscape depth is moderate. |
| Architecture | MEDIUM-HIGH | Clear established patterns, but host/protocol edge behavior needs validation. |
| Pitfalls | HIGH | Risk patterns are well-known and directly applicable to this domain. |

**Overall confidence:** MEDIUM-HIGH

### Gaps to Address

- Exact codec/protocol compatibility expectations vs reference clients: resolve through targeted integration tests in planning/execution.
- Ableton-specific lifecycle edge cases by version/config: resolve with explicit host test matrix in roadmap/phase plans.

## Sources

### Primary (HIGH confidence)
- JUCE documentation and release metadata.
- Steinberg VST3 SDK documentation/tags and validator tooling docs.
- Ninjam/JamTaba reference repositories and official Ninjam resources.

### Secondary (MEDIUM confidence)
- Community-established plugin architecture and real-time audio engineering patterns.

### Tertiary (LOW confidence)
- Competitive feature assumptions not backed by direct product instrumentation.

---
*Research completed: 2026-03-08*
*Ready for roadmap: yes*
