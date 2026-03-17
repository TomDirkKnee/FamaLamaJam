---
phase: 08-server-discovery-history
plan: "02"
subsystem: discovery-ui
tags: [server-discovery, ui, public-list, editor, testing]
requires:
  - phase: 08-01
    provides: remembered-server persistence and processor-owned discovery snapshot contracts
provides:
  - dedicated public-list discovery client seam with fake-client test support
  - combined remembered/public picker in the existing connection area
  - stale/failure-aware discovery status and manual host/port autofill behavior
affects: [08-03, rehearsal-ui, connection-entry]
tech-stack:
  added:
    - src/infra/net/PublicServerDiscoveryClient.h
    - src/infra/net/PublicServerDiscoveryClient.cpp
    - tests/integration/support/FakeServerDiscoveryClient.h
    - tests/integration/plugin_server_discovery_ui_tests.cpp
  patterns:
    - processor-polled background discovery results
    - picker selection only prefills manual draft fields
key-files:
  created:
    - src/infra/net/PublicServerDiscoveryClient.h
    - src/infra/net/PublicServerDiscoveryClient.cpp
    - tests/integration/support/FakeServerDiscoveryClient.h
    - tests/integration/plugin_server_discovery_ui_tests.cpp
  modified:
    - src/plugin/FamaLamaJamAudioProcessor.h
    - src/plugin/FamaLamaJamAudioProcessor.cpp
    - src/plugin/FamaLamaJamAudioProcessorEditor.h
    - src/plugin/FamaLamaJamAudioProcessorEditor.cpp
    - tests/integration/plugin_server_discovery_tests.cpp
    - tests/integration/plugin_rehearsal_ui_flow_tests.cpp
    - tests/integration/plugin_mixer_ui_tests.cpp
    - tests/integration/plugin_transport_ui_sync_tests.cpp
    - tests/integration/plugin_room_controls_ui_tests.cpp
    - tests/CMakeLists.txt
    - CMakeLists.txt
key-decisions:
  - Public discovery refresh stays off the audio thread and off the editor message thread through a dedicated client seam plus processor polling.
  - Selecting a remembered/public entry only fills Host and Port; manual typing remains the real source of truth for connections.
  - Public entry labels are richer than remembered history labels and surface room info plus connected-user text when available.
patterns-established:
  - "Fake discovery client scripts one refresh result per request so stale-cache behavior is deterministic in integration tests."
  - "Existing editor harnesses explicitly opt into discovery callbacks to avoid hidden constructor-path regressions."
requirements-completed: [DISC-01]
duration: 1 session
completed: 2026-03-17
---

# Phase 08 Plan 02: Combined Picker Summary

**Public server discovery, remembered-history merge behavior, and the compact combined picker are now wired into the current connection area without replacing manual host/port entry.**

## Accomplishments

- Added a dedicated `PublicServerDiscoveryClient` seam and a deterministic fake client for discovery tests.
- Extended the processor to request, poll, parse, cache, and merge public discovery results while exposing stale/failure state honestly.
- Added a combined picker plus refresh/status controls to the existing editor and kept entry selection as editable host/port prefill only.
- Added UI and integration coverage for combined ordering, autofill-without-connect, stale cache behavior, and manual refresh.
- Fixed a regression in the older rehearsal/mixer/transport editor harnesses by moving them onto the explicit discovery-aware constructor path.

## Files Created/Modified

- `src/infra/net/PublicServerDiscoveryClient.h` - public discovery interface.
- `src/infra/net/PublicServerDiscoveryClient.cpp` - background worker-based implementation for the public list source.
- `src/plugin/FamaLamaJamAudioProcessor.cpp` - refresh orchestration, merged discovery snapshot, and fake-client injection hooks.
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp` - combined picker row, refresh button, status line, and test helpers.
- `tests/integration/plugin_server_discovery_tests.cpp` - public merge and stale-cache coverage.
- `tests/integration/plugin_server_discovery_ui_tests.cpp` - editor-level picker workflow coverage.
- `tests/integration/support/FakeServerDiscoveryClient.h` - scripted refresh test seam.

## Verification

- `cmake --build build-vs --target famalamajam_tests --config Debug`
- `famalamajam_tests.exe "[plugin_server_discovery],[plugin_server_discovery_ui]"`
- Result: pass (`7` test cases / `120` assertions)

## Notes

- Changes remain uncommitted in this session.
- The public list source is currently `http://ninjam.com/serverlist.php`; residual risk about upstream source behavior will be carried into final Phase 8 verification rather than hidden.
