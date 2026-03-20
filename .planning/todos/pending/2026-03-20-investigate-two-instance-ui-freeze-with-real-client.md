---
status: pending
priority: medium
created: 2026-03-20T03:15:00Z
owner: codex
---

# Investigate Two-Instance UI Freeze With Real External Client

## Summary

When two FamaLamaJam plugin editors are open in parallel and a real external NINJAM client joins from another machine, the UI can freeze or assert while:

- audio continues
- networking continues
- Receive Diagnostics continue updating

This does **not** currently appear to affect the normal intended workflow of one FLJ instance in the DAW, so it is being tracked as a known non-blocking limitation rather than a release blocker.

## Current Understanding

- Two FLJ instances side by side can work for local testing by themselves.
- The freeze/assert is triggered specifically when a real external client joins while both FLJ editors are open.
- The issue looks editor-only rather than transport/audio-thread related.
- Recent mitigations already added:
  - explicit editor timer/callback cleanup on destruction
  - server picker refresh no longer rebuilds every timer tick
  - server picker refresh made non-reentrant
  - mixer/room rebuild paths made less destructive by removing `removeAllChildren()` churn and clearing callbacks before widget destruction

## Why Deferred

- Running two FLJ plugin instances in the same DAW session is mainly a test harness, not the expected end-user workflow.
- Core single-instance timing/audio validation is more important to product progress right now.

## Resume Clues

If this becomes important later, start by inspecting:

- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- dynamic mixer strip rebuilds when remote users/channels appear
- room feed widget rebuilds
- any host-specific JUCE editor/listener churn when multiple editor windows are open

Best repro:

1. Open two FLJ instances in the same DAW.
2. Leave both editor windows visible.
3. Have a real external NINJAM client join from another machine.
4. Observe whether the UI freezes/asserts while audio continues.
