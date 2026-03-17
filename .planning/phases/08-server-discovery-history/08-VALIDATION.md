---
phase: 08
slug: server-discovery-history
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-03-17
---

# Phase 08 - Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 v3.8.1 |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `.\build-vs\tests\famalamajam_tests.exe "[plugin_server_discovery]"` or `.\build-vs\tests\famalamajam_tests.exe "[plugin_server_discovery_ui]"` |
| **Full suite command** | `ctest --test-dir build-vs --output-on-failure` |
| **Estimated runtime** | ~60-90 seconds for a targeted discovery tag; ~120 seconds for the combined gate |

---

## Sampling Rate

- **After every task commit:** Run the narrowest relevant tag for the task just completed: `"[plugin_server_discovery]"` for backend/history/fetch work and `"[plugin_server_discovery_ui]"` for editor workflow work.
- **After every plan completion:** Run `.\build-vs\tests\famalamajam_tests.exe "[plugin_server_discovery],[plugin_server_discovery_ui]"` before advancing to the next wave.
- **After every plan wave:** Run `ctest --test-dir build-vs --output-on-failure`
- **Before `$gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 90 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 08-01-01 | 01 | 1 | DISC-02 | integration | `.\build-vs\tests\famalamajam_tests.exe "[plugin_server_discovery]"` | MISSING - W0 | pending |
| 08-02-01 | 02 | 2 | DISC-01, DISC-02 | integration | `.\build-vs\tests\famalamajam_tests.exe "[plugin_server_discovery_ui]"` | MISSING - W0 | pending |
| 08-03-01 | 03 | 2 | DISC-01, DISC-02 | integration | `ctest --test-dir build-vs --output-on-failure` | present | pending |

*Status: pending / green / red / flaky*

---

## Wave 0 Requirements

- [ ] `tests/integration/plugin_server_discovery_tests.cpp` - covers successful-history updates, public-list parsing, and stale/failure state
- [ ] `tests/integration/plugin_server_discovery_ui_tests.cpp` - covers combined picker ordering, editable field population, and direct-manual-flow non-regression
- [ ] `tests/CMakeLists.txt` - register new Phase 8 integration files

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Combined picker readability and compactness in a typical Ableton plugin window | DISC-01, DISC-02 | Compact discovery UI density and row readability are host-window dependent and partially subjective | Load the VST3 in Ableton, open the connection area, confirm the combined picker remains readable, manual host/port stays usable, and stale/error status is understandable without obscuring the rest of the page. |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Task-level feedback path tightened to targeted tags where possible
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
