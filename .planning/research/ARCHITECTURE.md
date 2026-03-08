# Architecture Research

**Domain:** DAW-hosted real-time networked audio plugin (Ninjam client in JUCE 8 VST3)
**Researched:** 2026-03-08
**Confidence:** MEDIUM

## Standard Architecture

### System Overview

```text
+-------------------------------------------------------------------+
| Host Layer (Ableton Live / VST3 Host)                             |
|  +------------------------+                                        |
|  | VST3 Plugin Instance   |                                        |
|  +-----------+------------+                                        |
+--------------|-----------------------------------------------------+
               |
+--------------v-----------------------------------------------------+
| Plugin Runtime Layer (inside JUCE plugin)                          |
|  +----------------------+   +-----------------------------------+  |
|  | UI/Control Thread    |<->| Session/Application Core          |  |
|  | (Editor + ViewModel) |   | (state + commands + scheduling)   |  |
|  +----------------------+   +-------------------+---------------+  |
|                                               /   \                |
|                                              /     \               |
|                     +-----------------------+       +------------+  |
|                     | Audio Engine (RT)     |       | Transport  |  |
|                     | capture/mix/buffer    |       | TCP/Socket |  |
|                     +-----------+------------+       +-----+------+  |
|                                 |                          |         |
+---------------------------------|--------------------------|---------+
                                  |                          |
+---------------------------------v--------------------------v---------+
| External Layer                                                      |
|  +-----------------------------+    +----------------------------+   |
|  | Ninjam Server               |    | Local Persistence          |   |
|  | interval sync + channels    |    | settings + session profile |   |
|  +-----------------------------+    +----------------------------+   |
+---------------------------------------------------------------------+
```

### Component Responsibilities

| Component | Responsibility | Typical Implementation |
|-----------|----------------|------------------------|
| Plugin Host Boundary | Expose VST3 lifecycle and audio/MIDI callbacks | `juce::AudioProcessor` entrypoints |
| Session/Application Core | Own session state machine, command handling, timing decisions, policy | Plain C++ domain/services with minimal JUCE coupling |
| Audio Engine (RT path) | Real-time safe capture, monitor mix, per-interval buffering, encode/decode handoff | Lock-free queues, fixed-size buffers, no heap in audio callback |
| Protocol/Transport | Connection, auth, protocol framing, packet I/O, reconnect | Dedicated network thread + message codec |
| UI Layer | User controls, channel strip, metronome/interval indicators, status | `juce::AudioProcessorEditor` + presenter/view model |
| Persistence | Save/restore server presets, mix defaults, UI state | JUCE `ValueTree`/properties or simple config files |

## Recommended Project Structure

```text
src/
|-- plugin/                    # VST3/JUCE entrypoints and host integration
|   |-- PluginProcessor.cpp    # processBlock, parameter wiring, RT bridges
|   |-- PluginEditor.cpp       # UI composition + bindings
|   `-- parameters/            # AudioProcessorValueTreeState + param defs
|-- app/                       # Session/application core (non-RT business logic)
|   |-- session/               # connect/disconnect, roles, reconnection state
|   |-- timing/                # interval clock, metronome state, drift handling
|   `-- commands/              # UI/host intents mapped to domain commands
|-- audio/                     # Real-time audio path and DSP-safe utilities
|   |-- capture/               # input capture and pre-send conditioning
|   |-- mix/                   # local monitor and received channel mix
|   |-- buffers/               # lock-free ring buffers and frame pools
|   `-- codec/                 # encode/decode adapters for Ninjam payloads
|-- net/                       # Protocol and transport implementation
|   |-- ninjam/                # protocol messages, parser, serializer
|   |-- transport/             # socket client, retry strategy, heartbeat
|   `-- bridge/                # audio<->net handoff queues
|-- infra/                     # Cross-cutting adapters and persistence
|   |-- config/                # settings store + migrations
|   `-- logging/               # runtime diagnostics and host-safe logging
|-- tests/                     # Unit/integration tests (non-host and host-sim where possible)
`-- third_party/               # pinned external dependencies if needed
```

### Structure Rationale

- **`plugin/`:** Keeps JUCE host-specific glue isolated from domain logic so core behavior is testable without a DAW.
- **`audio/` and `net/` split:** Enforces a hard boundary between real-time constraints and non-real-time networking.
- **`app/` as orchestrator:** Centralizes lifecycle/timing policy to avoid implicit coupling between UI and transport.
- **`infra/`:** Prevents persistence/logging concerns from leaking into RT and protocol code.

## Architectural Patterns

### Pattern 1: Ports and Adapters (Hexagonal Core)

**What:** Define interfaces in `app/` and implement them in `plugin/`, `net/`, and `infra/`.
**When to use:** For session control, persistence, and transport interactions where testability matters.
**Trade-offs:** Adds interface boilerplate; significantly improves isolation and test coverage.

**Example:**
```cpp
// app/session/SessionPort.h
struct SessionPort {
    virtual ~SessionPort() = default;
    virtual void connect(const ServerConfig& cfg) = 0;
    virtual void disconnect() = 0;
    virtual SessionStatus status() const = 0;
};
```

### Pattern 2: Real-Time Safe Command Queue

**What:** Use lock-free queues to pass control changes and audio frames between UI/network threads and audio thread.
**When to use:** Any communication crossing the audio callback boundary.
**Trade-offs:** Fixed capacity and backpressure handling are mandatory; avoids XRuns and callback stalls.

**Example:**
```cpp
// audio callback consumes prebuilt commands only
while (controlQueue.try_pop(cmd)) {
    engine.apply(cmd); // no allocations, no locks
}
```

### Pattern 3: Explicit Session State Machine

**What:** Model lifecycle states (Idle, Connecting, SyncingInterval, JamActive, Reconnecting, Error).
**When to use:** Connection/reconnection and interval sync behavior.
**Trade-offs:** More upfront modeling work; prevents edge-case drift and undefined transitions.

## Data Flow

### Request Flow (Connect + Jam Start)

```text
[User presses Connect]
    -> [UI ViewModel]
    -> [Session Core Command Handler]
    -> [Transport Connect + Protocol Handshake]
    -> [Session enters SyncingInterval]
    -> [Audio Engine armed on interval boundary]
    -> [JamActive state reflected back to UI]
```

### State Management

```text
[SessionStore]
    -> publishes immutable snapshots
[UI Presenter] <-> dispatches commands -> [Session Core Reducers/Handlers]
[Audio/Net Bridges] subscribe to derived state needed for runtime behavior
```

### Key Data Flows

1. **Local Send Path:** Audio input -> RT capture buffer -> encode worker -> transport send queue -> Ninjam server.
2. **Remote Receive Path:** Transport receive queue -> decode worker -> interval-aligned buffer -> monitor mix in audio callback.
3. **Control Path:** UI parameter change -> command queue -> session/audio engine update -> state snapshot -> UI refresh.
4. **Timing Path:** Server interval metadata -> timing service -> metronome and bar/interval indicators + buffer rollover scheduling.

## Build Order

1. **Plugin skeleton and host contract:** JUCE 8 VST3 project, stable `AudioProcessor` lifecycle, parameter framework.
2. **Session core + state machine:** Implement connect lifecycle and deterministic transitions without full audio/net coupling.
3. **Transport and protocol baseline:** TCP client, handshake, heartbeat, and minimal message framing for server connectivity.
4. **RT audio buffer pipeline:** Input capture, output mix, lock-free bridges, and strict real-time safety checks.
5. **Ninjam encode/decode integration:** Wire codec path into send/receive flows with interval alignment.
6. **Essential UI:** Connect/status, interval/metronome indicators, per-channel send/monitor controls.
7. **Persistence and recovery:** Save session defaults, reconnect behavior, startup restore.
8. **Hardening for rehearsal reliability:** Failure injection, dropout/reconnect tests, CPU/xrun profiling in Ableton.

## Scaling Considerations

| Scale | Architecture Adjustments |
|-------|--------------------------|
| Single musician / rehearsal use | Single-process plugin architecture is sufficient; prioritize deterministic timing and stability. |
| Multi-room power-user use | Add stronger diagnostics, resilient reconnect logic, and optional background worker tuning for variable networks. |
| Broader cross-platform distribution | Introduce standalone target sharing `app/`, `audio/`, and `net/`; isolate OS-specific transport/audio adapters. |

### Scaling Priorities

1. **First bottleneck:** Audio callback overruns from unsafe cross-thread work; fix with stricter queue boundaries and profiling.
2. **Second bottleneck:** Network jitter/packet timing effects on interval sync; fix with jitter buffers and robust interval resync strategy.

## Anti-Patterns

### Anti-Pattern 1: Mixing Network I/O in Audio Callback

**What people do:** Read/write sockets or allocate protocol objects directly in `processBlock`.
**Why it's wrong:** Causes callback stalls, dropouts, and host instability.
**Do this instead:** Keep callback RT-only and use lock-free bridges to non-RT worker threads.

### Anti-Pattern 2: UI as Source of Truth for Session State

**What people do:** Let editor widgets directly own connection/timing truth.
**Why it's wrong:** Breaks behavior when editor is closed or recreated; creates inconsistent state.
**Do this instead:** Keep session state in core store and make UI a projection/controller only.

## Integration Points

### External Services

| Service | Integration Pattern | Notes |
|---------|---------------------|-------|
| Ninjam server | Persistent TCP session + protocol framing + heartbeat/reconnect | Treat server timing metadata as authoritative for interval alignment. |
| DAW host (Ableton Live first) | VST3 host callbacks via JUCE wrapper | Validate lifecycle behavior under play/stop, sample rate change, and buffer size changes. |

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| `plugin` <-> `app` | Commands + state snapshots | No direct network or persistence calls from UI/editor. |
| `audio` <-> `net` | Lock-free frame/message queues | Explicit ownership and fixed-size memory pools. |
| `app` <-> `infra/config` | Repository-style interface | Keep config serialization out of session logic. |

## Sources

- JUCE plugin architecture documentation (VST3 hosting model): https://docs.juce.com/
- VST3 SDK architecture concepts: https://steinbergmedia.github.io/vst3_doc/
- JamTaba project (domain reference for Ninjam client behavior): https://github.com/elieserdejesus/JamTaba
- Ninjam protocol reference and ecosystem docs: https://www.cockos.com/ninjam/

---
*Architecture research for: DAW-native Ninjam plugin client (JUCE 8 / VST3)*
*Researched: 2026-03-08*