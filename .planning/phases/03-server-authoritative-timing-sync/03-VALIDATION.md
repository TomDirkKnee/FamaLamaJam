---
phase: 03
slug: server-authoritative-timing-sync
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-03-14
---

# Phase 03 - Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 3.8.1 + CTest |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `ctest --test-dir build-vs-ninja --output-on-failure --timeout 30 -R "(plugin_server_timing_sync|plugin_transport_ui_sync|plugin_remote_interval_alignment)"` |
| **Full suite command** | `ctest --test-dir build-vs-ninja --output-on-failure` |
| **Estimated runtime** | ~30 seconds |

---

## Sampling Rate

- **After every task commit:** Run `ctest --test-dir build-vs-ninja --output-on-failure --timeout 30 -R "(plugin_server_timing_sync|plugin_transport_ui_sync|plugin_remote_interval_alignment)"`
- **After every plan wave:** Run `ctest --test-dir build-vs-ninja --output-on-failure`
- **Before `$gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 30 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 03-01-01 | 01 | 1 | SYNC-01 | integration | `ctest --test-dir build-vs-ninja --output-on-failure --timeout 30 -R plugin_server_timing_sync` | ❌ W0 | ⬜ pending |
| 03-01-02 | 01 | 1 | SYNC-01 | integration | `ctest --test-dir build-vs-ninja --output-on-failure --timeout 30 -R plugin_server_timing_sync` | ❌ W0 | ⬜ pending |
| 03-02-01 | 02 | 1 | SYNC-02 | integration | `ctest --test-dir build-vs-ninja --output-on-failure --timeout 30 -R plugin_transport_ui_sync` | ❌ W0 | ⬜ pending |
| 03-02-02 | 02 | 1 | SYNC-03 | integration | `ctest --test-dir build-vs-ninja --output-on-failure --timeout 30 -R plugin_transport_ui_sync` | ❌ W0 | ⬜ pending |
| 03-03-01 | 03 | 2 | SYNC-01 | integration | `ctest --test-dir build-vs-ninja --output-on-failure --timeout 30 -R plugin_remote_interval_alignment` | ❌ W0 | ⬜ pending |
| 03-03-02 | 03 | 2 | SYNC-03 | integration | `ctest --test-dir build-vs-ninja --output-on-failure --timeout 30 -R plugin_remote_interval_alignment` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/integration/plugin_server_timing_sync_tests.cpp` - authoritative boundary math, deferred BPM/BPI application, reconnect timing reset
- [ ] `tests/integration/plugin_transport_ui_sync_tests.cpp` - explicit waiting/reconnecting/timing-lost UI state and beat-divided progress snapshots
- [ ] `tests/integration/plugin_remote_interval_alignment_tests.cpp` - multi-source boundary-quantized remote switching and late-interval skip behavior
- [ ] `tests/integration/support/MiniNinjamServer.h` - shared server harness extracted from `plugin_experimental_transport_tests.cpp`

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Beat-divided progress readability in Ableton plugin UI | SYNC-02 | Final visual legibility and subtle rollover feel are subjective host-UI checks | Load the plugin in Ableton, connect to a timing-capable session, verify beat segmentation is readable and interval rollover is subtle but understandable. |
| Reconnect-status wording clarity during real host use | SYNC-02 | Message tone and clarity are best judged in host context during an actual reconnect interruption | Simulate disconnect/reconnect in Ableton and confirm the transport/metronome stop, waiting/reconnecting text is explicit, and timing resumes only after authoritative timing returns. |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 30s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
