---
phase: 5
slug: ableton-reliability-v1-rehearsal-ux-validation
status: draft
nyquist_compliant: true
wave_0_complete: true
created: 2026-03-15
---

# Phase 5 - Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 integration/unit suite via CTest |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `.\build-vs\tests\famalamajam_tests.exe "[plugin_state_roundtrip],[plugin_streaming_runtime_compat],[plugin_transport_ui_sync]"` |
| **Full suite command** | `ctest --test-dir build-vs --output-on-failure` |
| **Estimated runtime** | ~60 seconds |

---

## Sampling Rate

- **After every task commit:** Run `.\build-vs\tests\famalamajam_tests.exe "[plugin_state_roundtrip],[plugin_streaming_runtime_compat],[plugin_transport_ui_sync]"`
- **After every plan wave:** Run `ctest --test-dir build-vs --output-on-failure`
- **Before `$gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 60 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 05-01-01 | 01 | 1 | HOST-01 | integration | `.\build-vs\tests\famalamajam_tests.exe "[plugin_host_lifecycle],[plugin_state_roundtrip],[plugin_streaming_runtime_compat]"` | ❌ W0 | ⬜ pending |
| 05-02-01 | 02 | 1 | UI-01, UI-02 | integration | `.\build-vs\tests\famalamajam_tests.exe "[plugin_rehearsal_ui_flow],[plugin_transport_ui_sync],[plugin_connection_recovery],[plugin_connection_error_recovery]"` | ❌ W0 | ⬜ pending |
| 05-03-01 | 03 | 2 | HOST-01, UI-01, UI-02 | manual + full regression | `ctest --test-dir build-vs --output-on-failure` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/integration/plugin_host_lifecycle_tests.cpp` - restore-idle, duplicate-safe, unload/remove, and engine stop/start lifecycle coverage
- [ ] `tests/integration/plugin_rehearsal_ui_flow_tests.cpp` - setup-first single-page workflow and persistent status-banner behavior
- [ ] Stream-specific message assertions added to existing transport/status suites

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Reopen saved Ableton set restores idle plugin state with persisted settings and mix | HOST-01, UI-01 | True host reopen behavior cannot be fully simulated from JUCE integration tests | Save a set with configured plugin, close Ableton, reopen the set, confirm settings/mix restore, status is honest, and explicit Connect is required. |
| Duplicated Ableton track keeps copied plugin idle instead of silently joining | HOST-01 | Track duplication semantics are host-driven | In Ableton, duplicate a track containing a connected plugin, confirm the duplicate does not auto-connect or hijack session behavior. |
| Audio engine stop/start or device reconfigure recovers without zombie session state | HOST-01, UI-02 | Host engine lifecycle is not fully represented in the existing test harness | While connected, stop/restart the engine or change device settings, confirm status guidance is actionable and reconnect/recovery behavior remains clean. |
| Small-group rehearsal can be completed from the built-in JUCE UI | UI-01, UI-02 | Final usability confidence requires a real musician workflow | Join a real small room, confirm setup/connect/status/mixer flow is understandable, start playing, and record any confusion or recovery gaps. |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or manual-validation coverage
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all missing references
- [x] No watch-mode flags
- [x] Feedback latency < 60s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
