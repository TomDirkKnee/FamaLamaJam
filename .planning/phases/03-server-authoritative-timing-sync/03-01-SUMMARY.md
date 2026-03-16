---
phase: 03-server-authoritative-timing-sync
plan: "01"
subsystem: timing-core
tags: [juce, c++, ninjam, timing, integration-tests]
requires:
  - phase: 02-connection-lifecycle-recovery
    provides: Stable lifecycle transitions and reconnect reset semantics
provides:
  - Connection-scoped authoritative timing state in the processor
  - Boundary-deferred BPM/BPI application
  - Honest timing reset on disconnect/reconnect/timing loss
  - Shared MiniNinjamServer timing harness
requirements-completed: [SYNC-01, SYNC-03]
completed: 2026-03-14
---

# Phase 3 Plan 01: Timing Core Summary

Implemented the authoritative timing engine in the processor, deferred BPM/BPI changes to shared boundaries, and added dedicated integration coverage with a reusable mini-server harness.

## Verification

- `cmake --build build-vs --target famalamajam_tests`
- `.\build-vs\tests\famalamajam_tests.exe "[plugin_server_timing_sync]"`

Result: passed.

## Notes

- Windows verification used `build-vs` because the local Ninja path deadlocked in CMake/MSVC try-compile execution on this machine.
