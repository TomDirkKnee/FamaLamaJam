---
phase: 01
slug: plugin-foundation-session-configuration
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-03-08
---

# Phase 01 - Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 3.x + CTest |
| **Config file** | `CMakeLists.txt` |
| **Quick run command** | `ctest --test-dir build --output-on-failure --timeout 30 -L phase1-quick` |
| **Full suite command** | `ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | ~25-30 seconds (quick), ~90 seconds (full) |

---

## Sampling Rate

- **After every task commit:** Run the task-specific smoke command from the map (`--timeout 30` target).
- **After every plan wave:** Run `ctest --test-dir build --output-on-failure -L phase1-quick`, then `ctest --test-dir build --output-on-failure`.
- **Before `$gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 120 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | Post-Task Sample | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|------------------|-------------|--------|
| 01-01-01 | 01 | 1 | SESS-01, HOST-02 (foundation) | build smoke | `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` | `ctest --test-dir build --output-on-failure --timeout 30 -L phase1-quick` | W0 | pending |
| 01-01-02 | 01 | 1 | SESS-01 | unit smoke | `ctest --test-dir build --output-on-failure --timeout 30 -R settings_validation` | `ctest --test-dir build --output-on-failure --timeout 30 -L phase1-quick` | W0 | pending |
| 01-01-03 | 01 | 1 | HOST-02 | unit smoke | `ctest --test-dir build --output-on-failure --timeout 30 -R state_serialization` | `ctest --test-dir build --output-on-failure --timeout 30 -L phase1-quick` | W0 | pending |
| 01-02-01 | 02 | 2 | SESS-01 | integration smoke | `ctest --test-dir build --output-on-failure --timeout 30 -R plugin_apply_flow` | `ctest --test-dir build --output-on-failure --timeout 30 -L phase1-quick` | W0 | pending |
| 01-02-02 | 02 | 2 | HOST-02 | integration smoke | `ctest --test-dir build --output-on-failure --timeout 30 -R plugin_state_roundtrip` | `ctest --test-dir build --output-on-failure --timeout 30 -L phase1-quick` | W0 | pending |
| 01-02-03 | 02 | 2 | SESS-01, HOST-02 (gate) | gate smoke | `ctest --test-dir build --output-on-failure --timeout 30 -L phase1-quick` | `ctest --test-dir build --output-on-failure` (wave/phase gate) | W0 | pending |

*Status: pending, green, red, flaky*

---

## Wave 0 Requirements

- [ ] `tests/unit/settings_validation_tests.cpp` - stubs for `SESS-01`
- [ ] `tests/unit/state_serialization_tests.cpp` - stubs for `HOST-02`
- [ ] `tests/integration/plugin_state_roundtrip_tests.cpp` - processor-level round-trip checks
- [ ] Catch2 + CTest wiring in CMake (if no framework detected)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Ableton reopen restores saved settings and remains disconnected | HOST-02 | DAW host lifecycle not fully reproducible in unit tests | Insert plugin in Ableton, set values, save project, close/reopen Live, verify values restored and no auto-connect |
| Duplicate track clones settings deterministically | HOST-02 | Host-specific duplication semantics | Duplicate track containing plugin, compare settings values between instances |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity is attributable per task in the map (no 3 consecutive tasks without automated verify)
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Smoke feedback latency target remains <= 30s per task
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending


