---
phase: 06
slug: ableton-sync-assist-research-prototype
status: draft
nyquist_compliant: true
wave_0_complete: true
created: 2026-03-16
---

# Phase 06 - Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 3.8.1 + CTest |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && .\build-vs\tests\famalamajam_tests.exe "[plugin_host_sync_assist],[plugin_transport_ui_sync]"'` |
| **Full suite command** | `cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && ctest --test-dir build-vs --output-on-failure'` |
| **Estimated runtime** | ~30 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && .\build-vs\tests\famalamajam_tests.exe "[plugin_host_sync_assist],[plugin_transport_ui_sync]"'`
- **After every plan wave:** Run `cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && .\build-vs\tests\famalamajam_tests.exe "[plugin_host_sync_assist],[plugin_transport_ui_sync],[plugin_rehearsal_ui_flow]"'`
- **Phase gate only:** Run `cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && ctest --test-dir build-vs --output-on-failure && cmake --build build-vs --target famalamajam_plugin_VST3 --config Debug'`
- **Before `$gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 30 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 06-01-01 | 01 | 1 | HSYNC-01 | integration | `famalamajam_tests.exe "[plugin_host_sync_assist]"` | pending W0 | pending |
| 06-01-02 | 01 | 1 | HSYNC-01 | integration | `famalamajam_tests.exe "[plugin_host_sync_assist],[plugin_server_timing_sync]"` | pending W0 | pending |
| 06-02-01 | 02 | 2 | HSYNC-02 | integration/ui | `famalamajam_tests.exe "[plugin_host_sync_assist],[plugin_transport_ui_sync]"` | partial | pending |
| 06-03-01 | 03 | 3 | HSYNC-01, HSYNC-02 | targeted integration | `famalamajam_tests.exe "[plugin_host_sync_assist],[plugin_transport_ui_sync]"` | yes | pending |
| 06-03-gate | 03 | 3 | HSYNC-01, HSYNC-02 | phase gate | `ctest --test-dir build-vs --output-on-failure && cmake --build build-vs --target famalamajam_plugin_VST3 --config Debug` | yes | pending |

*Status: pending, green, red, flaky*

---

## Wave 0 Requirements

- [ ] `tests/integration/plugin_host_sync_assist_tests.cpp` - stubs and real coverage for armed sync behavior
- [ ] extend `tests/integration/plugin_transport_ui_sync_tests.cpp` for arm button/status/blocked-state rendering
- [ ] use `AudioProcessor::setPlayHead()`-driven host snapshot injection; add a helper seam only if direct playhead tests prove insufficient

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Real Ableton VST3 host supplies the expected playhead/bar-start info during armed start | HSYNC-01 | Host behavior cannot be fully proven by injected tests alone | Build VST3, connect with matching room tempo, arm sync, press Play from bar start and mid-bar, confirm aligned one-shot start or explicit visible failure. |
| Disabled-arm explanation and armed-state guidance feel clear in the real plugin UI | HSYNC-02 | UX clarity is best judged in-host | With mismatched Ableton tempo, confirm arm control is disabled with inline explanation; with matching tempo, confirm clear armed banner text appears and clears after success/cancel. |
| Failed-start retry requires explicit manual re-arm | HSYNC-01, HSYNC-02 | The one-shot retry contract must be proven against real host Play behavior | Force a failed start scenario, confirm the armed state clears, press Play again without re-arming and confirm nothing happens, then re-arm and confirm a new start attempt is allowed. |
| Successful aligned start resets the one-shot arm | HSYNC-01, HSYNC-02 | The assist must not silently remain armed after success | Complete a successful aligned start, stop Ableton, press Play again without re-arming, and confirm the plugin does not enter a new start attempt until the user explicitly arms again. |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 60s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
