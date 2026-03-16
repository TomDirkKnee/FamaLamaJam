---
phase: 04
slug: audio-streaming-mix-monitoring-core
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-03-15
---

# Phase 04 - Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 3.8.1 + CTest |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && .\build-vs\tests\famalamajam_tests.exe "[plugin_experimental_transport],[plugin_remote_interval_alignment],[plugin_codec_runtime],[plugin_mixer_controls],[plugin_mixer_ui],[plugin_streaming_runtime_compat]"'` |
| **Full suite command** | `cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && ctest --test-dir build-vs --output-on-failure'` |
| **Estimated runtime** | ~45 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && .\build-vs\tests\famalamajam_tests.exe "[plugin_experimental_transport],[plugin_remote_interval_alignment],[plugin_codec_runtime],[plugin_mixer_controls],[plugin_mixer_ui],[plugin_streaming_runtime_compat]"'`
- **After every plan wave:** Run `cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && ctest --test-dir build-vs --output-on-failure'`
- **Before `$gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 45 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 04-01-01 | 01 | 1 | AUD-01 | integration | `famalamajam_tests.exe "[plugin_experimental_transport]"` | yes | pending |
| 04-01-02 | 01 | 1 | AUD-02 | integration | `famalamajam_tests.exe "[plugin_remote_interval_alignment]"` | yes | pending |
| 04-01-03 | 01 | 1 | MIX-01 | integration | `famalamajam_tests.exe "[plugin_mixer_controls]"` | no - W0 | pending |
| 04-02-01 | 02 | 2 | MIX-02 | integration | `famalamajam_tests.exe "[plugin_mixer_ui]"` | no - W0 | pending |
| 04-02-02 | 02 | 2 | MIX-01 | integration | `famalamajam_tests.exe "[plugin_mixer_controls]"` | no - W0 | pending |
| 04-03-01 | 03 | 2 | AUD-03 | integration | `famalamajam_tests.exe "[plugin_streaming_runtime_compat]"` | no - W0 | pending |
| 04-03-02 | 03 | 2 | AUD-01, AUD-02, AUD-03 | integration | `ctest --test-dir build-vs --output-on-failure` | yes | pending |

*Status: pending / green / red / flaky*

---

## Wave 0 Requirements

- [ ] `tests/integration/plugin_mixer_controls_tests.cpp` - per-channel gain, pan, mute, and persisted restore coverage
- [ ] `tests/integration/plugin_mixer_ui_tests.cpp` - dedicated local strip, grouped remote strips, labels, and inactive-retention behavior
- [ ] `tests/integration/plugin_streaming_runtime_compat_tests.cpp` - runtime sample-rate and buffer-size compatibility while streaming remains active

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Local strip vs remote strip clarity in Ableton | MIX-02 | Final legibility and musician comprehension are host-UI judgments | Load the plugin in Ableton, join a session with at least one remote user, and confirm the local strip is clearly labeled as live monitoring while remote strips read as delayed returns. |
| Practical per-channel balancing during a real room test | MIX-01 | Real rehearsal behavior across multiple users/channels is easier to judge live than through automation only | Join a room with multiple remote sources, adjust gain/pan/mute on separate strips, and confirm the audible result matches the visible mixer state. |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all missing references
- [x] No watch-mode flags
- [x] Feedback latency < 45s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
