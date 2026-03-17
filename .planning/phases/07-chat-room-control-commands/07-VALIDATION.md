---
phase: 07
slug: chat-room-control-commands
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-03-17
---

# Phase 07 - Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 v3.8.1 |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | Plan-targeted tag: `.\build-vs\tests\famalamajam_tests.exe "[plugin_room_chat]"` or `.\build-vs\tests\famalamajam_tests.exe "[plugin_room_controls_ui]"` |
| **Full suite command** | `ctest --test-dir build-vs --output-on-failure` |
| **Estimated runtime** | ~30-60 seconds for a single targeted tag; ~90 seconds for the combined room gate |

---

## Sampling Rate

- **After every task commit:** Run the narrowest relevant tag for the task just completed: `"[plugin_room_chat]"` for 07-01 work, `"[plugin_room_controls_ui]"` for 07-02 work, and the combined room gate only when 07-03 is gathering verification evidence.
- **After every plan completion:** Run `.\build-vs\tests\famalamajam_tests.exe "[plugin_room_chat],[plugin_room_controls_ui]"` before advancing to the next wave.
- **After every plan wave:** Run `ctest --test-dir build-vs --output-on-failure`
- **Before `$gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 60 seconds for task-level feedback, ~90 seconds for the combined plan gate

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 07-01-01 | 01 | 1 | ROOM-01 | integration | `.\build-vs\tests\famalamajam_tests.exe "[plugin_room_chat]"` | MISSING - W0 | pending |
| 07-02-01 | 02 | 2 | ROOM-02 | integration | `.\build-vs\tests\famalamajam_tests.exe "[plugin_room_controls_ui]"` | MISSING - W0 | pending |
| 07-03-01 | 03 | 3 | ROOM-01, ROOM-02 | integration | `ctest --test-dir build-vs --output-on-failure` | present | pending |

*Status: pending / green / red / flaky*

---

## Wave 0 Requirements

- [ ] `tests/integration/plugin_room_chat_tests.cpp` - covers ROOM-01 transport, processor state, and disconnect/rejoin reset behavior
- [ ] `tests/integration/plugin_room_controls_ui_tests.cpp` - covers ROOM-02 vote controls, pending/error copy, and layout placement above the mixer
- [ ] `tests/integration/support/MiniNinjamServer.h` - add `MESSAGE_CHAT_MESSAGE` helpers plus scripted topic/vote/system feedback paths
- [ ] `tests/CMakeLists.txt` - register new Phase 7 integration files

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Room chat and vote readability in an actual rehearsal window size | ROOM-01, ROOM-02 | Compact single-page usability and message density are subjective and host-window dependent | Load the VST3 in Ableton, connect to a room, exchange chat, submit BPM/BPI votes, and confirm the feed stays readable without obscuring transport or mixer workflows. |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Task-level feedback path tightened to targeted tags where possible
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
