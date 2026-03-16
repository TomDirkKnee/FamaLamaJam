# Phase 5: Ableton Reliability & v1 Rehearsal UX Validation - Research

**Researched:** 2026-03-15
**Domain:** Ableton host-lifecycle hardening, single-page rehearsal UX polish, and final Windows v1 validation
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

#### Ableton lifecycle hardening
- Save/close/reopen of an Ableton set is the highest-priority Phase 5 lifecycle case.
- After reopening a set, the plugin should restore settings and mix state but remain disconnected until the user explicitly presses Connect.
- When a track containing the plugin is duplicated, the duplicated instance should stay idle by default rather than auto-joining the session.
- Audio engine stop/start behavior in Ableton is part of the formal Phase 5 reliability scope and should recover cleanly without requiring plugin reload.

#### v1 rehearsal flow
- Phase 5 should optimize for a single simple page in the built-in JUCE UI, not a multi-view workflow.
- The top of the UI should prioritize settings, Connect, and clear status so a user can join a room without hunting through the interface.
- The mixer should remain on the same page but be visually secondary to the connection/setup flow.
- Phase 5 should validate and polish the current built-in workflow rather than introduce external tooling or a fundamentally different interaction model.

#### Status and error messaging
- Connection, sync, and streaming problems should use short plain-language messages with one clear recovery action.
- Important failure states should remain visible in a persistent status/banner treatment until the state changes.
- Stream-specific issues should explain both what happened and what the user should expect next, especially for missed or skipped interval audio.
- Main UI messaging should avoid technical protocol detail unless it is necessary for user recovery.

#### Validation and sign-off
- Phase 5 verification should produce a practical checklist with pass/fail notes and recorded follow-up issues.
- The key v1 rehearsal validation target is a real small-group room in Ableton rather than only solo loopback or synthetic stress testing.
- Phase 5 is good enough to close once the required lifecycle and rehearsal scenarios pass and any residual rough edges are clearly documented.

### Claude's Discretion
- Exact wording and visual styling of the persistent status/banner treatment within the existing JUCE UI style.
- Exact ordering and spacing of same-page sections, as long as setup/status remains visually first.
- Exact manual validation checklist structure, provided it stays concise and practical.

### Deferred Ideas (OUT OF SCOPE)
- Advanced diagnostics or observability panels for deeper stream/network troubleshooting.
- New routing, patchbay, or other post-v1 mixer expansion.
- Automatic reconnect-on-reopen preference memory.
- Larger-scale stress-room validation beyond the practical small-group v1 rehearsal target.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| HOST-01 | Plugin handles Ableton lifecycle events (load/unload/reopen/duplicate) without crashes or zombie sessions. | Keep disconnected-on-restore as a hard invariant, add explicit host stop/start cleanup semantics, and add dedicated lifecycle integration plus Ableton host matrix coverage. |
| UI-01 | User can perform end-to-end jam setup and start playing from a simple built-in JUCE UI. | Preserve the current single-page editor and processor-owned snapshot model, then move setup/connect/status into a clearly top-priority flow with mixer secondary. |
| UI-02 | User receives actionable status/error messages for connection, sync, and stream failures. | Build on the existing lifecycle/status strings and transport health model, but add a persistent priority banner and explicit stream-recovery copy. |
</phase_requirements>

## Summary

Phase 5 does not need a transport rewrite or a new UI architecture. The current code already has the right high-level shape: `FamaLamaJamAudioProcessor` owns connection state, stream state, timing state, mix persistence, and host entry points; `FamaLamaJamAudioProcessorEditor` is a single-page JUCE surface fed by processor getters; and restore semantics already force a reopened instance back to disconnected `Idle` with persisted settings and mix state intact.

The real planning gap is host-lifecycle proof. `setStateInformation()` and the destructor already close sockets and clear reconnect state, but `releaseResources()` only tears down codec/timing/meter state and leaves lifecycle/socket state alive. That is a reasonable starting point for Ableton audio-engine stop/start recovery, but it is not yet proven safe under real host suspend/resume behavior, plugin removal while connected, or project close/reopen with active transport. Phase 5 should therefore focus on making cleanup invariants explicit and testable rather than scattering more one-off resets.

On the UX side, the current editor already matches the locked product direction: one page, settings at the top, transport detail in the middle, mixer below. What is missing is prioritization and persistence of user guidance. Connection status is currently rendered as a bottom `statusLabel_`, while transport health is a separate `transportLabel_`; that split is good and should stay, but the main recovery banner needs to move visually higher and gain clear precedence rules for connection, sync, and streaming problems.

**Primary recommendation:** Plan Phase 5 as three focused streams: 1) codify host cleanup/restore invariants and add lifecycle tests around stop/start, unload, reopen, and duplicate; 2) keep the current single-page editor but promote a persistent top-priority banner and tighten setup-first layout ordering; 3) add a final Windows Ableton validation contract that combines targeted `build-vs` automation, plugin validation tooling, and a concise real-room manual matrix.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 8.0.12 | Plugin lifecycle hooks, editor widgets, timers, `ValueTree` state | Already pinned in `cmake/dependencies.cmake` and owns the host boundary. |
| C++ | C++20 | Processor/editor/session implementation | Already enforced by the top-level CMake configuration. |
| Existing `FamaLamaJamAudioProcessor` + `ConnectionLifecycleController` | local | Single owner for connection, timing, streaming, restore, and reconnect semantics | Reuse this processor-owned truth instead of introducing another lifecycle layer. |
| Existing `FamaLamaJamAudioProcessorEditor` | local | Built-in single-page JUCE UI | Already matches the locked Phase 5 interaction model. |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Catch2 | 3.8.1 | Unit and integration tests | Use for new lifecycle and banner-priority coverage. |
| CTest | CMake-integrated | Phase quick/full test gates | Keep using it for full-suite Windows verification. |
| Existing `MiniNinjamServer` harness | local | Realistic auth/timing/stream integration coverage | Extend it for host-lifecycle and rehearsal validation rather than inventing a new fake transport. |
| `build-vs` MSVC path | local repo standard | Stable Windows build/test path on this machine | Already called out in Phase 4 and current state as the verified path. |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Existing single-page JUCE editor | New multi-view workflow or external helper app | Violates locked scope and creates avoidable Phase 5 churn. |
| Processor-owned status and mixer snapshots | Editor-owned transient UI state | Harder to keep restore, reconnect, and manual validation deterministic. |
| Explicit disconnected-on-restore invariant | Auto-reconnect on reopen or duplicate | Conflicts with the locked Ableton behavior and raises surprise/zombie-session risk. |

**Installation / Build Baseline:**
```bash
cmake --build build-vs --target famalamajam_tests --config Debug
cmake --build build-vs --target famalamajam_plugin_VST3 --config Debug
```

## Architecture Patterns

### Pattern 1: Disconnected-On-Restore Is The Invariant
**What:** Any host state restore path must restore settings and mix state, clear live transport/runtime state, and leave the instance idle until the user presses Connect.
**When to use:** `setStateInformation()`, reopen, duplicate, and any restore/migration code touched in Phase 5.
**Why:** This is already the codebase's safest behavior and directly matches the locked Phase 5 decision.
**Example:**
```cpp
closeLiveSocket();
applyLifecycleTransition(lifecycleController_.resetToIdle());
mixerStripsBySourceId_.clear();
ensureLocalMonitorMixerStrip();
```
Source: `src/plugin/FamaLamaJamAudioProcessor.cpp`

### Pattern 2: Processor-Owned Snapshots, Editor Polling
**What:** The processor remains the source of truth for lifecycle state, transport state, metronome state, and mixer strip snapshots; the editor polls that state on a timer and renders it.
**When to use:** UI ordering changes, banner work, mixer polish, and any new status messaging.
**Why:** The current processor/editor contract is simple, testable, and already covered by harness-based UI tests.
**Example:**
```cpp
auto lifecycleGetter = [this]() { return getLifecycleSnapshot(); };
auto transportUiGetter = [this]() {
    const auto state = getTransportUiState();
    return FamaLamaJamAudioProcessorEditor::TransportUiState {
        .connected = state.connected,
        .hasServerTiming = state.hasServerTiming,
        .syncHealth = static_cast<FamaLamaJamAudioProcessorEditor::SyncHealth>(state.syncHealth),
        .metronomeAvailable = state.metronomeAvailable,
    };
};
```
Source: `src/plugin/FamaLamaJamAudioProcessor.cpp`

### Pattern 3: One Cleanup Policy Across Host Entry Points
**What:** Phase 5 should introduce an explicit, shared runtime teardown policy for `releaseResources()`, lifecycle transitions, restore, and destruction instead of relying on partially overlapping reset blocks.
**When to use:** Audio engine stop/start, disconnect, reconnect, plugin unload, and transport failure handling.
**Why:** The current code clears different subsets of state in different places; that is workable now, but it is the main risk area for host-lifecycle regressions and zombie sessions.

### Pattern 4: Split Primary Banner From Transport Detail
**What:** Keep the current distinction between overall session status and interval/timing detail, but promote the primary recovery message into a persistent top-of-page banner with clear priority.
**When to use:** Connection failure, retry countdown, timing lost, skipped interval playback, and successful recovery.
**Why:** The current split is sound, but the important message is visually too low and can be overwritten by less important detail.

### Anti-Patterns to Avoid
- **Auto-connect on restore or duplicate:** This directly violates the locked Phase 5 lifecycle behavior.
- **Editor-owned recovery state:** It will drift from processor reality during reconnects, restore, and host stop/start.
- **Bottom-only failure messaging:** Critical recovery guidance should not live below the mixer.
- **Scattered cleanup branches:** Partial resets across `releaseResources()`, reconnect, and restore are the most likely source of host-only regressions.
- **Multi-view workflow churn:** Phase 5 should polish the current surface, not redesign it.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Restore behavior | New "smart" auto-resume heuristics | Existing disconnected-on-restore invariant in processor state restore | Reopen and duplicate must stay idle by default. |
| UI data flow | A second state machine inside the editor | Existing processor getters + editor timer refresh | The current model already supports deterministic UI tests and replayable status snapshots. |
| Lifecycle verification | Ad hoc manual testing from memory | Dedicated validation doc plus targeted integration tests and `build-vs` commands | Final Windows v1 sign-off needs traceable pass/fail evidence. |
| Transport simulation | A bespoke fake networking layer for Phase 5 | Existing `MiniNinjamServer` integration harness | The repo already has working auth/timing/stream coverage to extend. |
| Recovery messaging | Protocol-heavy diagnostics panels | Short banner copy backed by lifecycle/transport snapshots | Locked scope favors plain-language musician guidance, not deep diagnostics. |

**Key insight:** Phase 5 should leverage the architecture already built in Phases 2-4. The right move is to make lifecycle and status invariants stricter and more visible, not to invent a new control architecture late in v1.

## Common Pitfalls

### Pitfall 1: Audio Engine Stop/Start Leaves A Half-Live Session
**What goes wrong:** `releaseResources()` stops codec/timing/meter activity but does not close the socket or change lifecycle state, so Ableton audio-engine stop/start can leave the plugin logically "Active" while the runtime has been partially torn down.
**Why it happens:** Cleanup responsibilities are currently split between `releaseResources()`, `applyLifecycleTransition()`, `setStateInformation()`, and the destructor.
**How to avoid:** Introduce an explicit suspend/resume policy for host stop/start and cover `releaseResources() -> prepareToPlay()` under active transport in integration and manual host tests.
**Warning signs:** UI still reports connected, but timing stays lost; remote audio never resumes; server still sees a user after the plugin is removed or the set is closed.

### Pitfall 2: Reopen Or Duplicate Starts Joining The Room Again
**What goes wrong:** A "helpful" reconnect-on-restore refactor breaks the reopened/duplicated idle requirement and creates surprise joins or zombie participants.
**Why it happens:** Restore code is one of the easiest places to accidentally trigger lifecycle transitions.
**How to avoid:** Treat `closeLiveSocket() + resetToIdle()` inside state restore as a non-negotiable invariant and test it directly.
**Warning signs:** Duplicated instances connect without user input; reconnect timers survive restore; status does not return to a disconnected-ready state.

### Pitfall 3: The Right Message Exists, But The User Does Not See It
**What goes wrong:** Connection status sits at the bottom while transport detail sits above the controls, so the most important recovery message is not where the user is looking.
**Why it happens:** The current editor grew functionally correct status surfaces before final hierarchy polish.
**How to avoid:** Keep transport detail/progress where it is conceptually useful, but move the primary failure/recovery banner to the setup area above or adjacent to Connect.
**Warning signs:** Users hunt through the page after a failure; status disappears when the mixer grows; transport text looks healthy while a more important session failure is easy to miss.

### Pitfall 4: Status Copy Covers Connection Errors But Not Stream Behavior
**What goes wrong:** Existing lifecycle and transport messages already handle connect/reconnect/timing states, but there is no explicit musician-facing wording yet for missed/skipped interval playback expectations.
**Why it happens:** Current status generation is strongest around connection lifecycle and sync health, not stream-specific edge cases.
**How to avoid:** Add a small priority model for stream-specific banner states that says what happened and what the user should expect next.
**Warning signs:** Users hear silence after a late interval and assume the plugin is broken; status only says "connected" or "timing lost" without the recovery expectation.

### Pitfall 5: Final Phase Ships Without Real Host Proof
**What goes wrong:** Current automation is strong for reconnect, restore, transport UI, remote alignment, mixer UI, and runtime compatibility, but there is still no explicit automated suite for plugin unload/remove, duplicate semantics, or host stop/start while connected.
**Why it happens:** Earlier phases optimized for streaming correctness and processor-level invariants, not final host acceptance.
**How to avoid:** Add a dedicated lifecycle integration file plus a concise Ableton matrix and plugin validation tooling gate.
**Warning signs:** Every unit/integration test passes, but Ableton close/reopen, audio-engine restart, or remove-plugin cases still feel uncertain.

## Code Examples

Verified local patterns that Phase 5 should build on:

### Restore Forces Idle Instead Of Reconnecting
```cpp
closeLiveSocket();
applyLifecycleTransition(lifecycleController_.resetToIdle());
...
lastStatusMessage_ = makeRestoreStatusMessage(usedFallback, restoredLastErrorContext);
```
Source: `src/plugin/FamaLamaJamAudioProcessor.cpp`

### Editor Refresh Is Snapshot-Driven, Not Event-Driven
```cpp
void FamaLamaJamAudioProcessorEditor::timerCallback()
{
    refreshLifecycleStatus();
    refreshTransportStatus();
    refreshMixerStrips();
}
```
Source: `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`

### Current Status Split Already Exists
```cpp
statusLabel_.setText(juce::String(status), juce::dontSendNotification);
...
transportLabel_.setText(formatTransportStatus(transport), juce::dontSendNotification);
```
Source: `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`

## Open Questions

1. **Which Ableton Live version(s) are the formal release gate?**
   - What we know: The repo is Windows-first and Ableton-first.
   - What's unclear: Exact host-version matrix for final sign-off.
   - Recommendation: Pick one primary release gate version and one secondary smoke version before planning execution details.

2. **Should restored last-error context appear on duplicates as well as reopens?**
   - What we know: `setStateInformation()` always restores brief last-error context if present.
   - What's unclear: Whether that orientation is helpful or confusing on duplicated instances.
   - Recommendation: Decide this in planning; if host behavior cannot distinguish duplicate from reopen, prefer the simpler consistent rule and document it.

3. **Which external validator will be the official plugin compliance gate?**
   - What we know: Prior research/docs mention `pluginval` and Steinberg `validator`, but the repo does not currently ship either tool.
   - What's unclear: Which one is installed and repeatable on the Windows release machine.
   - Recommendation: Choose one canonical validator in Phase 5 planning and add it to the final gate checklist.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 3.8.1 + CTest |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && .\build-vs\tests\famalamajam_tests.exe "[plugin_state_roundtrip],[plugin_connection_recovery],[plugin_connection_error_recovery],[plugin_transport_ui_sync],[plugin_remote_interval_alignment],[plugin_streaming_runtime_compat],[plugin_mixer_ui],[plugin_mixer_controls]"'` |
| Full suite command | `cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && ctest --test-dir build-vs --output-on-failure'` |
| Build command | `cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build build-vs --target famalamajam_tests --config Debug && cmake --build build-vs --target famalamajam_plugin_VST3 --config Debug'` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| HOST-01 | Save/close/reopen, duplicate, unload/remove, and audio-engine stop/start do not leave live sockets, pending retries, or surprise auto-joins behind. | integration + manual | `famalamajam_tests.exe "[plugin_host_lifecycle]"` | no - Wave 0 |
| UI-01 | User can complete setup/connect/start from one page with setup/status first and mixer secondary. | integration + manual | `famalamajam_tests.exe "[plugin_rehearsal_ui_flow]"` | no - Wave 0 |
| UI-02 | Connection, sync, and stream failures stay visible with one clear next step and honest recovery expectations. | integration | `famalamajam_tests.exe "[plugin_connection_recovery],[plugin_connection_error_recovery],[plugin_transport_ui_sync]"` | yes - existing, extend for stream-specific cases |

### Sampling Rate
- **Per task commit:** Run the quick command above plus the task-specific lifecycle/banner suite once those Wave 0 files exist.
- **Per wave merge:** Run the full suite command.
- **Phase gate:** Build tests + VST3, run the full suite, run the chosen plugin validator, then complete the manual Ableton matrix with pass/fail notes.

### Wave 0 Gaps
- [ ] `tests/integration/plugin_host_lifecycle_tests.cpp` - active-session unload, remove-plugin, reopen/disconnected restore, duplicate-idle restore, and `releaseResources()` / `prepareToPlay()` stop-start recovery
- [ ] `tests/integration/plugin_rehearsal_ui_flow_tests.cpp` - top-of-page setup ordering, banner priority, button enablement, and same-page workflow expectations
- [ ] Extend existing UI/status suites with stream-specific "skipped interval / will resume next interval" banner coverage
- [ ] `docs/validation/phase5-ableton-v1-matrix.md` - concise final manual sign-off checklist with pass/fail notes and follow-up issue links
- [ ] External validator tooling (`pluginval` or Steinberg `validator`) selected and documented as a repeatable Windows command

### Manual Ableton Matrix

Use the `build-vs` VST3 artifact produced by `famalamajam_plugin_VST3`, then capture pass/fail notes and any follow-up issue IDs for each case:

| Case ID | Scenario | Expected Result |
|---------|----------|-----------------|
| P5-HOST-01 | Save set while configured, close Ableton, reopen | Settings and mix state restore; instance is disconnected until Connect |
| P5-HOST-02 | Duplicate a connected plugin track | Duplicate restores config/mix but stays idle and does not auto-join |
| P5-HOST-03 | Toggle Ableton audio engine off/on or force a host stop/start while connected | Plugin recovers cleanly without reload; no zombie session remains |
| P5-HOST-04 | Remove plugin or close the set while connected | Session disconnects cleanly; no lingering user remains on the server |
| P5-UX-01 | Fresh join from built-in UI | User can configure, connect, see clear status, and start playing without leaving the page |
| P5-UX-02 | Auth/config failure | Banner explains what failed and the single recovery action |
| P5-UX-03 | Transient timeout / reconnect | Banner stays visible with countdown or next-step guidance until recovery |
| P5-UX-04 | Missed/skipped interval audio | Message explains what happened and that audio will resume on the next valid interval |
| P5-ROOM-01 | Small-group rehearsal in a real room | Complete a practical rehearsal run in Ableton and record residual issues, if any |

### Tooling Gate
- Run the quick suite after each task and the full suite after each plan wave.
- Run the chosen plugin validator against the Windows VST3 before final sign-off.
- Keep `build-vs` as the canonical Windows validation path for this phase; do not spend Phase 5 effort on local Ninja reliability.

## Sources

### Primary (HIGH confidence)
- `.planning/phases/05-ableton-reliability-v1-rehearsal-ux-validation/05-CONTEXT.md`
- `.planning/ROADMAP.md`
- `.planning/REQUIREMENTS.md`
- `.planning/STATE.md`
- `.planning/config.json`
- `cmake/dependencies.cmake`
- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- `src/plugin/FamaLamaJamAudioProcessor.cpp`
- `src/plugin/FamaLamaJamAudioProcessor.h`
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `src/plugin/FamaLamaJamAudioProcessorEditor.h`
- `src/app/session/ConnectionLifecycle.cpp`
- `src/app/session/ConnectionLifecycle.h`
- `src/app/session/ConnectionLifecycleController.cpp`
- `tests/integration/plugin_state_roundtrip_tests.cpp`
- `tests/integration/plugin_connection_recovery_tests.cpp`
- `tests/integration/plugin_connection_error_recovery_tests.cpp`
- `tests/integration/plugin_transport_ui_sync_tests.cpp`
- `tests/integration/plugin_mixer_ui_tests.cpp`
- `tests/integration/plugin_mixer_controls_tests.cpp`
- `tests/integration/plugin_streaming_runtime_compat_tests.cpp`
- `tests/integration/plugin_remote_interval_alignment_tests.cpp`
- `tests/integration/plugin_experimental_transport_tests.cpp`
- `docs/validation/phase1-ableton-matrix.md`
- `docs/validation/phase2-ableton-lifecycle-matrix.md`
- `.planning/phases/04-audio-streaming-mix-monitoring-core/04-VALIDATION.md`
- `.planning/phases/01-plugin-foundation-session-configuration/01-VALIDATION.md`

### Secondary (MEDIUM confidence)
- `.planning/research/SUMMARY.md`
- `.planning/research/PITFALLS.md`
- `.planning/research/STACK.md`

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - version pins, build path, and test tooling are all explicit in local repo files.
- Architecture: HIGH - lifecycle, restore, status, and editor wiring are directly implemented in the current codebase.
- Pitfalls: HIGH - the largest risks are visible in current cleanup boundaries and test gaps.
- Validation: MEDIUM-HIGH - the repo has strong automation scaffolding, but final Ableton host proof and validator tooling are still missing.

**Research date:** 2026-03-15
**Valid until:** 2026-04-14
