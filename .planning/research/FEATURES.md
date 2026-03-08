# Feature Research

**Domain:** DAW-native online jamming (Ninjam protocol client as JUCE VST3 plugin)
**Researched:** 2026-03-08
**Confidence:** MEDIUM

## Feature Landscape

### Table Stakes (Users Expect These)

Features users assume exist. Missing these = product feels incomplete.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Ninjam server connection and auth | A Ninjam client must connect reliably to real servers or it is unusable | MEDIUM | Dependency: robust socket lifecycle, reconnect policy, server capability handling |
| Interval/metronome sync with correct timing | Ninjam workflow depends on fixed intervals; timing errors break collaborative play | HIGH | Dependency: stable transport clock, drift handling, host tempo independence when needed |
| Audio send/receive encode-decode path | Core value is real rehearsal in-session; missing codec path means no jam | HIGH | Dependency: low-latency audio thread safety, jitter buffering, packet sequencing |
| Per-channel monitor/mix controls (gain, mute, pan basic) | Musicians expect immediate control of what they hear during rehearsal | MEDIUM | Dependency: predictable internal routing, state persistence in plugin session |
| Session stability in Ableton Live (plugin lifecycle safety) | Plugin crashes/dropouts are unacceptable in rehearsal context | HIGH | Dependency: JUCE-safe threading, graceful device/transport changes, defensive error paths |
| Basic visual status UI (connected, interval position, peers/channels) | Users need quick confidence on sync/state without debugging tools | LOW | Dependency: real-time-safe status handoff from engine to UI thread |

### Differentiators (Competitive Advantage)

Features that set the product apart. Not required, but valuable.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| DAW-native workflow tuned for Ableton (single plugin, no app switching) | Removes friction vs standalone clients; easier rehearsal setup for producers | MEDIUM | Dependency: host-state awareness, sane defaults for track monitoring and latency |
| Rehearsal reliability mode (network resilience + clear recovery UX) | Makes real sessions dependable under imperfect home network conditions | HIGH | Dependency: reconnection state machine, buffered recovery, explicit user feedback |
| Low-cognitive-load v1 UI focused on "join and play" | Faster onboarding for non-power users while retaining Ninjam behavior | MEDIUM | Dependency: strict feature scope, sensible presets, limited but strong controls |
| Plugin session recall for jam settings | Reopen project and resume quickly without manual reconfiguration | MEDIUM | Dependency: stable parameter/state schema, backward-compatible serialization |
| Audio-engine observability panel (latency, drift, packet health) | Helps troubleshoot practical rehearsal problems without external tools | MEDIUM | Dependency: telemetry capture with zero audio-thread contention |

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem good but create problems.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Full JamTaba parity in v1 | Existing users ask for everything they already know | Explodes scope and delays proof of reliable core jamming | Ship jam-core first; add high-impact parity items post-validation |
| Built-in video chat | Teams want all-in-one communication | Adds major CPU/network/UI complexity unrelated to core rehearsal value | Recommend Discord/Zoom alongside plugin during v1 |
| Advanced routing matrix and studio-grade patchbay | Power users want maximal flexibility | Increases UI complexity and support burden; high regression risk in v1 | Provide essential monitor/mix controls and defer advanced routing |
| Cross-platform release (Windows/macOS/Linux) at initial launch | Users expect broad availability | Triples build/test surface before core reliability is proven | Windows-first validation, then staged platform expansion |
| AI-assisted mix/auto-level features early | Looks modern and marketable | Distracts from deterministic real-time stability and protocol correctness | Add simple per-channel normalization presets later if needed |

## Feature Dependencies

```text
[Ninjam server connection/auth]
    -> requires -> [Network session manager + reconnect state machine]
                       -> requires -> [Thread-safe plugin lifecycle integration]

[Interval/metronome sync]
    -> requires -> [Stable timing engine]
                       -> requires -> [Audio callback-safe clock + drift correction]

[Audio send/receive encode-decode]
    -> requires -> [Ninjam protocol parser + packet pipeline]
                       -> requires -> [Jitter buffer + sequencing]

[Per-channel monitor/mix controls]
    -> requires -> [Internal audio routing graph]

[Basic status UI]
    -> requires -> [Engine-to-UI state bridge]

[Session recall]
    -> requires -> [Versioned state serialization]

[Rehearsal reliability mode] -> enhances -> [Connection/auth + encode-decode + sync]
[Advanced routing matrix] -> conflicts -> [Low-cognitive-load v1 UI]
[Built-in video chat] -> conflicts -> [Windows-first jam-core reliability timeline]
```

### Dependency Notes

- **Ninjam connection requires a network session manager:** Server drop/rejoin handling must be centralized to prevent partial plugin states.
- **Interval sync requires a stable timing engine:** Interval boundaries must remain predictable even under host transport changes.
- **Encode/decode requires jitter buffering:** Network variance without buffering creates audible instability and timing artifacts.
- **Monitor/mix controls require routing graph correctness:** User controls are only trustworthy if routing is deterministic.
- **Status UI requires an engine-to-UI bridge:** Real-time metrics must be transferred safely without blocking audio threads.
- **Session recall requires versioned serialization:** Plugin updates should not corrupt or discard saved project states.
- **Reliability mode enhances multiple core features:** Reconnect and recovery logic must coordinate connection, sync, and stream restoration.
- **Advanced routing conflicts with low-cognitive-load v1 UI:** Exposing full graph control undermines v1 simplicity and speed to reliability.
- **Video chat conflicts with v1 timeline:** It consumes engineering effort better spent on protocol and host stability.

## MVP Definition

### Launch With (v1)

Minimum viable product - what is needed to validate the concept.

- [ ] Ninjam connect/auth in-plugin - baseline requirement for any real session
- [ ] Correct interval/metronome sync - essential for playable collaborative timing
- [ ] Working send/receive encode-decode path - delivers actual remote audio collaboration
- [ ] Essential monitor/mix controls - enables practical rehearsal balance adjustments
- [ ] Ableton-focused stability hardening - validates core value (reliable DAW-native rehearsals)
- [ ] Basic status UI and errors - enables users to diagnose common join/sync issues quickly

### Add After Validation (v1.x)

Features to add once core is working.

- [ ] Session recall polish and migration guards - add when users begin saving/reopening rehearsal templates frequently
- [ ] Reliability observability panel - add when support/debug demand rises from real sessions
- [ ] Convenience onboarding presets (input/output/checklist) - add when initial user activation friction is observed
- [ ] Selected JamTaba parity items - add based on highest-frequency user requests after stable launch

### Future Consideration (v2+)

Features to defer until product-market fit is established.

- [ ] Cross-platform builds and QA matrix - defer until Windows usage validates retention
- [ ] Advanced routing and patchbay controls - defer until proven demand from power-user workflows
- [ ] Companion standalone app mode - defer until plugin-first path is mature
- [ ] Integrated social/collab features (presence/video/chat) - defer until core rehearsal reliability is consistently strong

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Ninjam connect/auth | HIGH | MEDIUM | P1 |
| Interval/metronome sync | HIGH | HIGH | P1 |
| Send/receive encode-decode | HIGH | HIGH | P1 |
| Essential monitor/mix controls | HIGH | MEDIUM | P1 |
| Ableton lifecycle stability hardening | HIGH | HIGH | P1 |
| Basic status/error UI | MEDIUM | LOW | P1 |
| Session recall | MEDIUM | MEDIUM | P2 |
| Reliability observability | MEDIUM | MEDIUM | P2 |
| Onboarding presets | MEDIUM | LOW | P2 |
| Selected JamTaba parity add-ons | MEDIUM | MEDIUM | P2 |
| Cross-platform release expansion | MEDIUM | HIGH | P3 |
| Advanced routing matrix | LOW (v1 cohort) | HIGH | P3 |
| Built-in video chat | LOW (core workflow) | HIGH | P3 |

**Priority key:**
- P1: Must have for launch
- P2: Should have, add when possible
- P3: Nice to have, future consideration

## Competitor Feature Analysis

| Feature | Competitor A | Competitor B | Our Approach |
|---------|--------------|--------------|--------------|
| Ninjam protocol jamming core | JamTaba: mature Ninjam-focused desktop workflow | ReaNINJAM/REAPER ecosystem: capable but tied to host/workflow constraints | Match practical core behavior while simplifying plugin-first setup |
| DAW integration | JamTaba: typically app + DAW bridging workflow | SonoBus/JackTrip style tools: strong networking but not Ninjam interval-native in same way | Deep Ableton-centric VST3 usage with minimal context switching |
| Routing/control complexity | JamTaba and REAPER paths can expose many advanced controls | Network audio tools often prioritize flexibility over simplicity | Keep v1 controls essential; add advanced routing only post-validation |
| Reliability and recoverability UX | Existing tools vary; recovery handling often user-driven | Some tools provide diagnostics but may not map to plugin rehearsal use | Prioritize explicit recovery states and practical in-session feedback |

## Sources

- `.planning/PROJECT.md` (core value, scope, constraints, out-of-scope decisions)
- JamTaba open-source project context (as referenced in PROJECT.md)
- General Ninjam workflow expectations from existing Ninjam client usage patterns
- DAW-plugin reliability expectations for rehearsal workflows

---
*Feature research for: DAW-native Ninjam plugin client*
*Researched: 2026-03-08*

