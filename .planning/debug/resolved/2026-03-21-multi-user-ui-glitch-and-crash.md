---
status: resolved
trigger: "In Ableton, one FLJ instance on its own is fine. In rooms with multiple users / roughly three or more incoming streams, the Ableton UI becomes glitchy (not just the FLJ editor) and sometimes crashes completely. Audio continues without glitches, which suggests graphics/editor/state churn rather than audio-thread overload."
created: 2026-03-21T21:40:00Z
updated: 2026-03-21T22:46:00Z
---

## Current Focus

hypothesis: Confirmed. The remaining host UI instability was dominated by editor/diagnostics message-thread churn rather than audio-thread work.
test: Closed after real-world Ableton verification in busy public-room usage.
expecting: No further action in this session.
next_action: Archive the debug note and keep any further work scoped to new symptoms only.

## Symptoms

expected: A single FLJ instance should remain UI-stable in Ableton even when several remote users/streams are present.
actual: With several incoming streams/users, Ableton UI becomes glitchy and sometimes fully crashes, while audio keeps running cleanly.
errors: User has seen UI freezes/crashes in earlier testing; current symptom emphasis is Ableton-wide UI glitching and occasional complete crash, with audio unaffected.
reproduction: Use one FLJ instance in Ableton and join a room with multiple users / 3+ incoming streams. Issue does not show with a single instance alone and is less apparent with fewer incoming streams.
started: Ongoing current issue after recent feature work; user now wants focused troubleshooting on performance/UI/crash behavior.

## Eliminated

## Evidence

- timestamp: 2026-03-21
  checked: prior debug notes plus current symptom pattern
  found: audio remains stable while Ableton UI glitches or crashes under several incoming streams
  implication: the dominant problem is likely message-thread/editor churn rather than audio-thread overload

- timestamp: 2026-03-21
  checked: `FamaLamaJamAudioProcessorEditor::timerCallback`
  found: the editor polls six refresh paths at `20 Hz` unconditionally: lifecycle, transport, server discovery, room UI, diagnostics, and mixer
  implication: any expensive work inside those refreshers scales directly with remote-user count and can starve the host UI thread

- timestamp: 2026-03-21
  checked: `refreshDiagnosticsUi`
  found: it always rebuilds the diagnostics string and calls `diagnosticsEditor_.setText(...)` every timer tick, even when diagnostics are collapsed by default
  implication: per-source diagnostics formatting and `TextEditor` document churn can grow with incoming streams while remaining invisible to the user

- timestamp: 2026-03-21
  checked: `refreshMixerStrips` + `mixerStripStateMatches`
  found: `mixerStripStateMatches` includes `meterLeft` and `meterRight`, so normal meter motion marks the whole strip as changed and triggers label/status/toggle/slider updates for every visible strip on every timer tick
  implication: remote-stream count directly increases editor mutation work even when only meters need repainting

- timestamp: 2026-03-21
  checked: focused regression tests after editor changes
  found: `[plugin_pre_layout_cpu_diagnostics]` passed with new coverage proving hidden diagnostics no longer poll and meter-only changes no longer count as full strip updates
  implication: the two highest-confidence editor churn paths are now reduced in code, not just in theory

- timestamp: 2026-03-21
  checked: broader UI regression slice and plugin rebuild
  found: `[plugin_pre_layout_cpu_diagnostics],[plugin_mixer_ui],[plugin_room_controls_ui],[plugin_server_discovery_ui],[plugin_transmit_controls_ui]` passed (`197 assertions / 24 cases`) and the Debug VST3 rebuilt successfully on `build-vs-2026`
  implication: the editor optimization did not regress the currently covered UI surfaces and is ready for Ableton verification

- timestamp: 2026-03-21
  checked: follow-up code review after the first fix and the user's "improved but still glitchy" result
  found: the editor timer still ran all refresh domains at 20 Hz, meaning full room/discovery snapshot copies and mixer snapshot polling were still happening more often than needed
  implication: there was a second, independent source of message-thread pressure even after reducing diagnostics and meter-only strip churn

- timestamp: 2026-03-21
  checked: timer-throttling regression tests
  found: new coverage proves the editor now refreshes mixer at 10 Hz, room at 5 Hz, discovery at 2 Hz, and diagnostics only when expanded, while the UI-heavy regression slice still passes (`203 assertions / 25 cases`)
  implication: the polling model is materially lighter in code and ready for another Ableton validation run

- timestamp: 2026-03-21
  checked: user insight plus follow-up code review
  found: the user explicitly prefers beat-sized UI movement, and the current transport/room UI naturally has a beat/interval structure that can be used as a refresh pulse
  implication: a beat-quantized model is a better fit than a uniform timer sweep for non-meter UI

- timestamp: 2026-03-21
  checked: beat-quantized timer implementation and regression coverage
  found: healthy timing now drives transport/room/status updates from beat/interval changes, while disconnected/waiting states keep a slower fallback cadence; the focused UI slice still passes (`208 assertions / 26 cases`) and the Debug VST3 rebuilt successfully
  implication: the editor now avoids continuous broad sweeps in the healthy busy-room case and is ready for another Ableton validation pass

- timestamp: 2026-03-21
  checked: new user symptom
  found: simply opening the diagnostics pane makes the UI much more likely to crash
  implication: diagnostics generation/rendering is a high-signal hotspot and should be treated as a separate risk from the rest of the editor cadence model

- timestamp: 2026-03-21
  checked: diagnostics pane implementation
  found: expanded diagnostics still rebuilt the full diagnostics string and called `TextEditor::setText(...)` on refresh, while the processor getter rebuilt/sorted a multiline snapshot each time
  implication: diagnostics had both generation churn and document/layout churn, making it a credible crash multiplier when visible

- timestamp: 2026-03-21
  checked: diagnostics-specific reduction and regression coverage
  found: diagnostics are now refreshed only at a coarse fixed cadence when expanded, and the editor only rewrites the `TextEditor` when the diagnostics text actually changes; focused UI tests still pass (`211 assertions / 27 cases`) and the Debug VST3 rebuilt successfully
  implication: the obvious diagnostics-specific churn path is reduced in code and is ready for another Ableton validation pass

- timestamp: 2026-03-21
  checked: real-world Ableton verification from the user
  found: "working much better", 4 instances ran with no problems, and tests in a few public rooms worked well
  implication: this issue is resolved enough to close the session; the implemented editor/diagnostics churn reductions materially improved the real host behavior

## Resolution

root_cause: The editor still did unnecessary heavy UI work on every 20 Hz timer tick. Specifically, hidden diagnostics rebuilt and rewrote a `TextEditor` document even when collapsed, and ordinary meter movement forced full strip widget updates because meter values were part of the strip equality check. With several incoming streams, that caused message-thread churn that matched the user's Ableton-wide UI glitch/crash symptoms while leaving audio unaffected.
fix:
- changed `refreshDiagnosticsUi()` to skip diagnostics text generation when the diagnostics section is collapsed, and refresh once on expand instead
- removed meter values from the strip equality check so meter motion no longer forces label/button/slider/status updates
- updated mixer refresh to track meter changes separately, repainting only the meter component when levels move
- throttled timer-driven refresh domains so the editor no longer polls mixer, room, server discovery, and diagnostics all at 20 Hz
- changed the refresh model so healthy transport/room/non-meter UI advances in beat-sized chunks instead of a continuous broad sweep, with slower fallback polling only when timing is not healthy
- reduced expanded diagnostics to low-cadence snapshot updates and skipped `TextEditor::setText(...)` when the diagnostics text has not changed
- added focused regression coverage for both reduced-churn paths
verification:
- `cmake --build build-vs-2026 --target famalamajam_tests --config Debug -- /m:1 /v:minimal`
- `build-vs-2026\\tests\\Debug\\famalamajam_tests.exe "[plugin_pre_layout_cpu_diagnostics]"`
- `build-vs-2026\\tests\\Debug\\famalamajam_tests.exe "[plugin_pre_layout_cpu_diagnostics],[plugin_mixer_ui],[plugin_room_controls_ui],[plugin_server_discovery_ui],[plugin_transmit_controls_ui]"`
- `cmake --build build-vs-2026 --target famalamajam_plugin_VST3 --config Debug -- /m:1 /v:minimal`
- Real-world Ableton verification: user reported the build was working much better, handled 4 instances with no problems, and worked well in several public rooms
files_changed:
- src/plugin/FamaLamaJamAudioProcessorEditor.h
- src/plugin/FamaLamaJamAudioProcessorEditor.cpp
- tests/integration/plugin_pre_layout_cpu_diagnostics_tests.cpp
