# Phase 1 Ableton Validation Matrix

## Scope

Phase 1 validation for:
- `SESS-01`: editable endpoint/credentials defaults with explicit Apply behavior
- `HOST-02`: plugin state save/restore for core settings

## Environment

- Host: Ableton Live (Windows)
- Plugin: `FamaLamaJam.vst3`
- Build: Debug and Release where available

## Preconditions

- Plugin builds and loads in host
- Session settings UI visible
- Known baseline values available for comparison

## Matrix

| Case ID | Scenario | Steps | Expected Result |
|---|---|---|---|
| P1-A1 | Fresh insert defaults | Insert plugin on new track | Host/port/username/channel defaults populated with safe baseline values |
| P1-A2 | Apply valid settings | Edit host/port/username/channel defaults and click Apply | Status shows success, values persist in active settings |
| P1-A3 | Reject invalid settings | Enter empty host or username and click Apply | Validation warning shown, previous valid active settings remain unchanged |
| P1-A4 | Save/reopen project restore | Save project, close Live, reopen project | Previously applied valid settings restored without manual re-entry |
| P1-A5 | Duplicate track clone | Duplicate track containing plugin | Duplicated instance has same persisted settings values |
| P1-A6 | Corrupt-state fallback | Load plugin with intentionally invalid state payload (test harness/manual injection) | Plugin remains loadable, defaults restored, warning status shown |
| P1-A7 | Disconnected on restore | Reopen project after save | Plugin does not auto-connect session on restore |

## Recording Template

| Case ID | Pass/Fail | Notes | Tester | Date |
|---|---|---|---|---|
| P1-A1 |  |  |  |  |
| P1-A2 |  |  |  |  |
| P1-A3 |  |  |  |  |
| P1-A4 |  |  |  |  |
| P1-A5 |  |  |  |  |
| P1-A6 |  |  |  |  |
| P1-A7 |  |  |  |  |

## Exit Criteria

- All matrix cases pass in at least one supported Ableton environment
- Any failure has a tracked issue and rerun plan
- `SESS-01` and `HOST-02` remain traceable in requirements + summaries
