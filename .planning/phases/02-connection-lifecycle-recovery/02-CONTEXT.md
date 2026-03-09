# Phase 2: Connection Lifecycle & Recovery - Context

**Gathered:** 2026-03-09
**Status:** Ready for planning

<domain>
## Phase Boundary

Deliver reliable connect/disconnect lifecycle and automatic reconnect recovery for Ninjam sessions in the plugin UI. This phase defines user-visible state transitions and retry behavior but does not add timing-sync or audio-streaming capabilities.

</domain>

<decisions>
## Implementation Decisions

### Connection control flow
- Connection starts only from an explicit **Connect** action (separate from Apply).
- Disconnect during connecting/reconnecting cancels immediately and returns to Idle.
- Apply while active/reconnecting stores updated settings for the next connect attempt (no forced live reconnect).
- Rapid duplicate Connect/Disconnect commands are ignored during active transitions.

### State and status UX
- UI shows explicit lifecycle states: idle, connecting, active, reconnecting, and error.
- Primary status surface is the existing status line, extended with state + concise reason.
- Reconnecting view includes countdown and attempt indicator (e.g., "Reconnecting in Ns (attempt X/Y)").
- Status uses concise actionable messaging with short recovery hints.
- Controls are state-gated so only valid actions are enabled in each state.
- Error/recovery status text auto-clears when a healthy state is reached.
- UI retains current state plus last error context.

### Reconnect policy behavior
- Automatic reconnect uses exponential backoff with a capped delay.
- Retries stop after a defined attempt cap (no infinite retry loop).
- On retry exhaustion, state moves to Error and requires manual Connect to restart.
- Manual Connect from Error fully resets retry counters/backoff.

### Hard-failure recovery rules
- Auto-retry applies only to transient transport/network failures.
- Configuration/auth/protocol-invalid failures are treated as non-retryable until user correction.
- Applying corrected settings while in Error returns session to Idle/Ready (no auto-connect).
- Hard-failure message remains sticky until state changes or user retries.
- After project reopen, plugin starts disconnected and shows brief last-error context.

### Claude's Discretion
- Exact numeric backoff curve and retry-attempt cap values.
- Exact wording/formatting of state messages and hint text.
- Exact visual treatment (color/badge/icon) for state display.

</decisions>

<specifics>
## Specific Ideas

- Keep lifecycle controls predictable: explicit user intent to connect, explicit user control to retry after hard failure.
- Favor clarity over verbosity in status messaging: short, actionable text in a single primary status region.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `FamaLamaJamAudioProcessorEditor` status label + button interactions can host lifecycle status and control-state gating.
- `SessionSettingsController` and `SessionSettingsStore` already provide validated apply flow that can feed connection lifecycle decisions.
- `FamaLamaJamAudioProcessor` already tracks connection boolean and last status message, providing a starting integration surface.

### Established Patterns
- Explicit Apply flow is already established in UI and should remain separate from Connect semantics.
- State restore currently returns to disconnected behavior on load; Phase 2 behavior should preserve this baseline.
- Current plugin status communication is concise single-line text, aligned with the chosen status UX direction.

### Integration Points
- Session lifecycle state machine integrates in/around `FamaLamaJamAudioProcessor` and editor control handlers.
- UI state/status rendering integrates with existing status label and connect/disconnect control enablement logic.
- Retry/error state transitions must interoperate with current settings application and persisted settings restore path.

</code_context>

<deferred>
## Deferred Ideas

None - discussion stayed within phase scope.

</deferred>

---

*Phase: 02-connection-lifecycle-recovery*
*Context gathered: 2026-03-09*
