# Phase 2 Ableton Lifecycle Recovery Validation Matrix

## Scope

Validate SESS-02 and SESS-03 behavior in Ableton for connection lifecycle recovery:
- Corrected settings in `Error` recover to disconnected `Idle/Ready` without auto-connect.
- Project reopen restores disconnected startup with brief last-error orientation context.
- Reconnect countdown and retry-attempt signaling remains concise and actionable.
- Hard-failure messaging stays sticky until healthy connection or explicit manual retry.

## Environment

- Host: Ableton Live (Windows)
- Plugin: FamaLamaJam VST3 (Debug build)
- Build target: `famalamajam_plugin`
- Network conditions: normal + simulated transient timeout + simulated auth/config failure

## Test Matrix

| ID | Scenario | Preconditions | Steps | Expected Result | Req |
|----|----------|---------------|-------|-----------------|-----|
| P2-LC-01 | Corrected apply recovers from Error | Plugin in `Error` after non-retryable failure | 1) Trigger auth/config failure 2) Edit to valid settings 3) Click Apply | State moves to `Idle/Ready`; no auto-connect; Connect remains user-driven | SESS-02, SESS-03 |
| P2-LC-02 | Manual retry after corrected apply | P2-LC-01 completed | 1) Click Connect after corrected Apply | State moves `Idle -> Connecting`; retry loop counters reset | SESS-02 |
| P2-LC-03 | Reopen starts disconnected after prior error | Save project while last session had failure context | 1) Save project 2) Close Ableton set 3) Reopen set | Plugin opens disconnected (`Idle`); brief last-error context visible; no auto-connect | SESS-02, SESS-03 |
| P2-LC-04 | Transient reconnect countdown messaging | Active session then transient timeout | 1) Connect successfully 2) Simulate timeout | Status shows countdown and attempt `X/Y` with short reason | SESS-03 |
| P2-LC-05 | Retry exhaustion requires manual recovery | Repeated transient failures beyond cap | 1) Force failures until exhausted | State enters `Error`; status includes actionable hint to press Connect | SESS-03 |
| P2-LC-06 | Healthy-state clears stale hard-failure context | Prior `Error` context exists, then successful connection | 1) Recover with manual Connect 2) Reach Active | Status becomes healthy (`Connected`) without stale hard-failure text | SESS-02, SESS-03 |

## Execution Notes

- Use one project file dedicated to lifecycle validation to make reopen checks deterministic.
- Record screenshot/video evidence for P2-LC-01, P2-LC-03, and P2-LC-05.
- Capture exact failure reasons used during simulation to confirm status text fidelity.

## Sign-Off Checklist

- [ ] P2-LC-01 passed
- [ ] P2-LC-02 passed
- [ ] P2-LC-03 passed
- [ ] P2-LC-04 passed
- [ ] P2-LC-05 passed
- [ ] P2-LC-06 passed
- [ ] SESS-02 acceptance confirmed
- [ ] SESS-03 acceptance confirmed
