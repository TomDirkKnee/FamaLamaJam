# Phase 2 Research: Connection Lifecycle & Recovery

**Phase:** `02-connection-lifecycle-recovery`  
**Date:** 2026-03-09  
**Requirements in scope:** `SESS-02`, `SESS-03`  
**Inputs used:**
- `.planning/phases/02-connection-lifecycle-recovery/02-CONTEXT.md`
- `.planning/REQUIREMENTS.md`
- `.planning/STATE.md`
- `.planning/ROADMAP.md`
- Existing code/tests under `src/` and `tests/`

**Constraint checks:**
- `./CLAUDE.md`: not present
- `./.claude/skills/`: not present
- `./.agents/skills/`: not present

## 1. Phase 2 Scope for Planning

Phase 2 should only deliver lifecycle and recovery behavior for session connectivity:
- `SESS-02`: connect/disconnect repeatedly in one Ableton session without restart.
- `SESS-03`: automatic reconnect for transient failure with clear status feedback.

Out of scope for this phase:
- No interval timing/metronome semantics (`SYNC-*`, Phase 3).
- No encode/decode streaming path (`AUD-*`, Phase 4).
- No advanced mix/monitor controls (`MIX-*`, Phase 4).
- No full Ableton lifecycle hardening matrix (`HOST-01`, Phase 5), beyond preventing obvious zombie connection state on restore.

## 2. Reusable Assets in Current Codebase

### Core assets to reuse directly

1. `SessionSettingsStore` + `SessionSettingsController` already provide validated, atomic apply semantics for endpoint/username input.
2. `FamaLamaJamAudioProcessor` is already the host-lifecycle boundary and owns persisted state restore behavior.
3. `FamaLamaJamAudioProcessorEditor` already has a status line and Apply interaction pattern that can be extended for lifecycle state projection.
4. Existing integration tests establish baseline patterns for processor construction, state roundtrip, and apply-flow checks.

### File-level integration anchors

- `src/plugin/FamaLamaJamAudioProcessor.h/.cpp`
  - Existing `isConnected_` and `lastStatusMessage_` should evolve into a richer lifecycle snapshot surface.
- `src/plugin/FamaLamaJamAudioProcessorEditor.h/.cpp`
  - Existing `statusLabel_` and button pattern should host connection status and action gating.
- `src/app/session/SessionSettings*.h/.cpp`
  - Existing validation path remains source-of-truth for retryability classification of configuration errors.
- `tests/integration/plugin_state_roundtrip_tests.cpp`
  - Existing "disconnected on restore" expectation should remain true in Phase 2.

## 3. Standard Stack (Phase 2)

No new external dependency is required for Phase 2. Use existing stack:
- C++20 + JUCE 8 existing codebase patterns.
- Domain logic in `src/app/session` with minimal JUCE coupling.
- Host/UI glue in `src/plugin`.
- Catch2 unit/integration test style already used in `tests/`.

If transport implementation is still partial, Phase 2 should use an interface boundary plus a fake transport for deterministic lifecycle testing.

## 4. Architecture Decisions (Phase 2 Only)

### D1. Introduce an explicit connection lifecycle state machine in app layer

Use explicit states aligned to context decisions:
- `Idle`
- `Connecting`
- `Active`
- `Reconnecting`
- `Error`

Transitions must be command/event-driven and centrally validated. This is required to satisfy roadmap success criterion for explicit state transitions and to prevent UI and processor drift.

### D2. Enforce single-writer lifecycle ownership

All lifecycle transitions should occur in one controller/reducer path (app/session layer). UI and processor read snapshots; they do not mutate state directly.

This avoids race conditions from rapid connect/disconnect spam and guarantees duplicate commands can be safely ignored during active transitions.

### D3. Classify failures as retryable vs non-retryable at boundary

Introduce failure category mapping:
- Retryable: transient transport/network drop, timeout, remote close without local config issue.
- Non-retryable: invalid host/port/user config, auth/protocol-invalid, malformed handshake.

Only retry retryable failures. Non-retryable failures transition directly to `Error` and require user correction/manual Connect.

### D4. Retry policy: capped exponential backoff with reset-on-manual-connect

Use policy object in app/session:
- `initialDelayMs` (for example 1000)
- `maxDelayMs` (for example 15000)
- `maxAttempts` (for example 6)
- `multiplier` (for example 2.0)

Behavior:
- Auto-retry only from retryable failures.
- Exhaustion transitions to `Error`.
- Manual Connect from `Error` resets attempt count/backoff.

### D5. Keep Apply decoupled from active connection teardown

Apply during `Active` or `Reconnecting` should update "next connect settings" only, with clear status text indicating deferred effect. Do not force immediate reconnect in Phase 2.

This matches Phase 2 context decisions and avoids transport churn while connected.

### D6. Persist only settings, not live connection state

After project reopen/restore:
- Always start disconnected (`Idle`).
- Optionally surface brief last-error context as informational status text.

Do not auto-connect on restore in Phase 2.

## 5. Architecture Patterns for Planner

## Standard Stack
- `app/session`: lifecycle state machine + retry policy + command handler
- `plugin`: control wiring, status projection, button enablement
- `tests`: deterministic model tests + integration behavior tests

## Architecture Patterns

1. Reducer-style state transitions
- Input: current snapshot + event/command
- Output: next snapshot + side-effect intents (connect, disconnect, schedule retry)
- Benefit: deterministic tests and easy Nyquist generation.

2. Side-effect adapter boundary
- Lifecycle core emits intents; transport adapter executes network actions.
- Core remains testable without real sockets.

3. UI projection model
- Map lifecycle snapshot to user-facing status text and enabled/disabled controls in one function.
- Ensures consistent behavior whether editor is open or recreated.

## Don't Hand-Roll

- Do not implement ad-hoc state transitions in multiple classes.
- Do not implement reconnect timers in editor/UI lifetime objects.
- Do not build retries around blocking sleeps or audio-thread timers.
- Do not invent a custom persistence format for runtime connection state.

## Common Pitfalls

- Status text driven by multiple sources, causing stale or contradictory UI.
- Retry attempts continuing after manual Disconnect.
- Misclassifying config/auth errors as retryable and looping pointlessly.
- Connect/Disconnect race leaving stale "connected" boolean true.

## Code Examples

```cpp
enum class ConnectionState { Idle, Connecting, Active, Reconnecting, Error };

enum class FailureKind { RetryableTransport, NonRetryableConfig, NonRetryableAuth, NonRetryableProtocol };

struct LifecycleSnapshot {
    ConnectionState state {ConnectionState::Idle};
    int attempt {0};
    int maxAttempts {0};
    int nextDelayMs {0};
    std::string status;
    std::string lastError;
};
```

```cpp
// Pure transition entry point for testability.
TransitionResult reduce(const LifecycleSnapshot& current, const Event& event, const RetryPolicy& policy);
```

## 6. Risks and Mitigations (Phase 2)

1. Race conditions during rapid user commands
- Risk: overlapping connect/disconnect side effects and stale state.
- Mitigation: single serialized command queue and idempotent command guards.

2. Retry storms or runaway loops
- Risk: aggressive retries flood logs/network and degrade host responsiveness.
- Mitigation: strict attempt cap + exponential backoff + reset only on explicit manual Connect.

3. UI/engine state divergence
- Risk: editor displays Active while core is reconnecting/error.
- Mitigation: UI binds to immutable lifecycle snapshot, never to local widget assumptions.

4. Hidden non-retryable failures
- Risk: user gets generic errors with no recovery hint.
- Mitigation: structured failure classification with actionable status hints.

5. Scope bleed into timing/protocol internals
- Risk: planner drifts into Phase 3 timing behavior.
- Mitigation: keep Phase 2 acceptance at lifecycle and recovery only, with timing alignment explicitly deferred.

## 7. Concrete Planner Recommendations (SESS-02/SESS-03)

### Recommended plan slice for 02-01 (state machine + commands)

1. Add lifecycle domain model under `src/app/session`:
- `ConnectionLifecycle.h/.cpp` (states, events, snapshot)
- `ConnectionLifecycleController.h/.cpp` (command entry points)

2. Extend processor interface:
- Expose lifecycle snapshot getter for UI projection.
- Replace boolean-only connectivity exposure with state-aware query API.

3. Update editor controls:
- Add explicit Connect/Disconnect control(s).
- Gate controls by state (ignore invalid transitions).
- Keep Apply independent from connection commands.

### Recommended plan slice for 02-02 (retry policy + signaling)

1. Add retry policy component (`RetryPolicy.h/.cpp`) with capped exponential behavior.
2. Add failure classification mapping and error/status reason codes.
3. Implement reconnect scheduling on non-audio thread boundary.
4. Surface reconnect countdown/attempt in status line.
5. Ensure retry exhaustion enters `Error` and requires manual Connect to restart.

## 8. Test Strategy (Phase 2)

### Unit tests (deterministic, no network)

Add state-machine tests in `tests/unit/connection_lifecycle_tests.cpp`:
- Valid transition table coverage for all states and commands.
- Duplicate command suppression (Connect during Connecting, Disconnect during Idle).
- Retry policy curve and cap behavior.
- Manual Connect reset behavior after exhaustion.
- Failure classification mapping (retryable vs non-retryable).

### Integration tests (processor/plugin-level)

Add `tests/integration/plugin_connection_lifecycle_tests.cpp`:
- Repeated connect/disconnect cycles in one processor instance.
- Transient failure triggers auto-reconnect and increments attempt state.
- Non-retryable failure transitions to Error with no retries.
- Apply while Active/Reconnecting updates settings without forced reconnect.
- State restore keeps disconnected (`Idle`) with optional last-error context.

### Manual host checks (Ableton-focused, Phase 2 narrow)

- Connect/disconnect repeatedly without plugin reload.
- Simulate network drop and observe reconnect countdown + capped attempts.
- Trigger config/auth error and confirm no auto-retry loop.
- Reopen project and verify no auto-connect.

## 9. Validation Architecture

Validation must be generated from lifecycle invariants and transition coverage, not ad-hoc UI scripts.

### A. Validation model for Nyquist generation

Define a canonical transition model artifact in tests:
- States: `Idle`, `Connecting`, `Active`, `Reconnecting`, `Error`
- Commands/events: `Connect`, `Disconnect`, `ConnectOk`, `ConnectFail(kind)`, `RetryTimerFired`, `RetryExhausted`, `ApplySettings`
- Expected side effects: `StartConnect`, `CancelConnect`, `ScheduleRetry(delay)`, `ClearRetry`, `NoOp`

Nyquist should generate tests from this matrix to guarantee complete transition coverage.

### B. Core invariants (must always hold)

1. At most one active connect attempt exists at any time.
2. `Disconnect` from `Connecting` or `Reconnecting` always reaches `Idle` and clears retry schedule.
3. Non-retryable failure never schedules retry.
4. Retry attempt counter is monotonic during one recovery cycle and resets only on manual Connect from `Error`/`Idle`.
5. UI status projection is a pure function of lifecycle snapshot.

### C. Scenario families for generated validation

1. Happy path: `Idle -> Connecting -> Active -> Disconnect -> Idle`
2. Transient drop path: `Active -> Reconnecting -> Active`
3. Retry exhaustion path: `Active -> Reconnecting -> Error`
4. Hard failure path: `Connecting -> Error` (no retries)
5. Command-storm path: repeated Connect/Disconnect during transitions remains deterministic
6. Apply-during-active path: settings update without immediate reconnect

### D. Coverage gates tied to requirements

- `SESS-02` gate:
  - 100% transition coverage for connect/disconnect commands.
  - Repeated cycle integration test passes without restart.
- `SESS-03` gate:
  - Retryable failure path coverage includes backoff progression and successful recovery.
  - Exhaustion and non-retryable branches covered with expected terminal state.
  - Status text includes state + actionable reason in each error/recovery step.

## 10. Phase 2 Acceptance Definition (Planner-Ready)

Phase 2 is done when all are true:

1. User can connect/disconnect repeatedly in one Ableton session without plugin restart (`SESS-02`).
2. Transient failures trigger automatic, capped exponential reconnect attempts with visible countdown/attempt context (`SESS-03`).
3. Non-retryable failures do not auto-retry and transition to explicit `Error` with clear recovery hint.
4. Lifecycle states are explicit and consistently reflected in UI status and control gating.
5. All Validation Architecture gates above pass in automated tests plus the narrow manual host checks.

---

Planning note: keep Phase 2 focused on deterministic lifecycle + recovery policy. Avoid pulling interval-sync or streaming internals into this phase.
