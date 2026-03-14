# Phase 3: Server-Authoritative Timing & Sync - Research

**Researched:** 2026-03-14
**Domain:** NINJAM interval timing, transport state, and boundary-quantized remote playback in a JUCE VST3 plugin
**Confidence:** MEDIUM

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
### Remote interval switching
- All remote decoded streams must follow the same shared server interval boundary.
- A remote user's interval is eligible for playback only if a full interval is buffered by its intended server boundary.
- Remote playback switches only on server-defined boundaries, never on packet arrival time.
- If an interval misses its boundary, do not play it late or mid-interval; skip it and wait for a later valid interval.
- Preserve alignment over continuity: on-time users can advance at the boundary while late users remain silent until they again have a full interval ready on a valid boundary.
- If BPM/BPI changes mid-session, apply the new interval length on the next server boundary rather than cutting the current interval.

### Sync recovery behavior
- If the connection is active but server timing has not arrived yet, show an explicit waiting-for-timing state rather than estimating locally.
- On timing loss or reconnect, stop the metronome and show an explicit error/reconnecting message.
- Do not keep a local clock running during timing loss or reconnect.
- Wait for real server timing to return before resuming transport display and sync behavior.
- Recovery should remain honest about missing timing; no fake beat or interval countdown should be shown while authoritative timing is unavailable.

### Timing display
- Healthy sync UI should prioritize current beat plus interval progress.
- Keep the existing progress bar, but present it as beat-divided interval progress rather than one undifferentiated long sweep.
- Interval rollover should be subtle rather than flashy; the boundary should be understandable without an aggressive visual effect.
- Beat-1/downbeat should still remain legible within the beat-and-progress presentation.

### Metronome role
- The metronome is an important sync aid in Phase 3 and should remain user-controllable.
- Phase 3 only requires simple on/off control for the metronome, aligned to server timing.
- Expanded metronome routing or mix controls are not required in this phase.

### Claude's Discretion
- Exact wording of waiting/reconnecting/timing-lost messages, as long as they stay explicit and concise.
- Exact visual treatment for beat-divided progress presentation.
- Exact handling of "ready exactly on the boundary" in implementation, as long as anything arriving after the boundary is treated as late.

### Deferred Ideas (OUT OF SCOPE)
- Separate debug/diagnostics window for deeper timing details.
- Expanded metronome level, pan, solo, or routing controls.
- Broader monitor/mix controls outside the core sync aid role.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| SYNC-01 | Plugin follows server-authoritative Ninjam interval timing (BPM/BPI/interval boundaries). | Use one shared audio-thread timing engine, reset timing on disconnect/reconnect, and commit BPM/BPI changes only at interval boundaries. |
| SYNC-02 | User can see current interval/metronome state in the UI. | Extend `TransportUiState` to include explicit sync health plus beat-divided interval progress from the authoritative timing engine. |
| SYNC-03 | Interval timing remains stable across at least a full rehearsal session without cumulative drift. | Advance timing only in samples on the audio thread, avoid wall-clock/UI timers, and add long-run drift tests plus reconnect realignment coverage. |
</phase_requirements>

## Summary

The current codebase already has the right raw pieces for Phase 3: `FramedSocketTransport` parses server config changes, the processor already computes interval and beat lengths from server BPM/BPI, the editor already exposes transport text plus a progress bar, and the integration test harness already includes a mini NINJAM server. The main gap is semantic, not structural: timing is still effectively a locally free-running sample counter, config changes are rescaled in place, and remote interval promotion is based on decoded availability rather than a shared boundary schedule.

Upstream NINJAM source confirms the core protocol messages relevant here are still config-change (`BPM/BPI`) plus interval upload/download begin/write frames. I did not find an explicit boundary timestamp or interval-phase packet in the inspected upstream message definitions. That means the safest plan is to treat "server-authoritative" in this phase as: server supplies timing parameters, the plugin owns one sample-domain connection-scoped epoch, timing is never guessed while authoritative timing is missing, and all remote playback changes are quantized to that shared boundary state. This satisfies the user constraints without inventing late playback or fake recovery behavior.

**Primary recommendation:** Implement a single audio-thread timing state machine in the processor, stage timing changes for the next boundary, and promote remote interval audio only at shared boundary transitions when a full interval is ready on time.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 8.0.12 | Plugin runtime, audio callback, editor UI, atomics, buffers | Already pinned in repo and already owns processor/editor lifecycle, which is where timing and UI sync must live. |
| C++20 | repo standard | Timing state, atomics, queue bookkeeping, test helpers | Already enforced in top-level CMake and sufficient for deterministic sample-domain state handling. |
| Existing `FramedSocketTransport` | local | Source of authenticated connection state, server BPM/BPI config, remote interval frames | Reuse instead of adding another transport abstraction. |
| Existing `CodecStreamBridge` | local | Encode/decode interval audio payloads | Already integrated into processor and transport tests. |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Catch2 | 3.8.1 | Unit/integration tests | Use for deterministic timing math, reconnect reset, and boundary-alignment host tests. |
| CTest | CMake-integrated | Test discovery and phase gates | Use existing `famalamajam_tests` target for quick and full validation. |
| Upstream NINJAM source snapshot | local checkout | Protocol semantics and client/server reference behavior | Use as the primary behavior reference for interval promotion and config-change semantics. |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Audio-thread sample counter as the only transport clock | UI timer or wall-clock timing | Do not use wall-clock/UI timing; it will drift and breaks host-buffer determinism. |
| One shared processor timing engine | Per-remote-source playback clocks | Per-source clocks make staggered playback more likely and violate the locked alignment behavior. |
| Current NINJAM framing/messages | Custom protocol extension for boundary timestamps | Only makes sense as a later protocol phase; current phase should stay compatible with the existing transport and upstream semantics. |

**Installation:**
```bash
cmake -S . -B build-vs-ninja
cmake --build build-vs-ninja --config Debug --target famalamajam_tests
```

## Architecture Patterns

### Recommended Project Structure
```text
src/
|-- net/                          # protocol framing and server timing snapshots
|-- plugin/                       # authoritative timing engine, remote interval gating, UI state export
`-- audio/                        # codec bridge and decoded interval handling
tests/
|-- integration/                  # mini-server timing/reconnect/alignment coverage
`-- unit/                         # pure timing math and state-machine edge cases
```

### Pattern 1: One Shared Timing Engine in the Processor
**What:** Keep one connection-scoped timing state machine in `FamaLamaJamAudioProcessor`, advanced only by audio callback sample counts.
**When to use:** For current beat, interval progress, boundary rollovers, metronome triggering, reconnect resets, and remote promotion eligibility.
**Example:**
```cpp
struct TimingState
{
    bool hasAuthoritativeTiming { false };
    int bpm { 0 };
    int bpi { 0 };
    int activeIntervalSamples { 0 };
    int pendingIntervalSamples { 0 };
    int activeBeatSamples { 0 };
    int samplesIntoInterval { 0 };
    std::uint64_t intervalIndex { 0 };
};
```
// Source: local project pattern derived from `src/plugin/FamaLamaJamAudioProcessor.cpp`

### Pattern 2: Boundary-Deferred Config Application
**What:** When server BPM/BPI changes, compute the next interval/beat sizes immediately, but only swap them in when `samplesIntoInterval` reaches the current interval boundary.
**When to use:** Any config-change packet received mid-interval.
**Example:**
```cpp
if (timingConfigChanged)
{
    timing.pendingIntervalSamples = computeIntervalSamplesFromServerTiming(sampleRate, bpm, bpi);
    timing.pendingBeatSamples = computeBeatSamplesFromServerTiming(sampleRate, bpm);
}

if (timing.samplesIntoInterval >= timing.activeIntervalSamples)
{
    advanceIntervalBoundary(timing);
}
```
// Source: recommended correction to the existing rescale-in-place logic in `src/plugin/FamaLamaJamAudioProcessor.cpp`

### Pattern 3: Boundary-Quantized Remote Promotion
**What:** Treat decoded remote intervals as candidates for a specific next boundary. Promote them only during boundary advancement, and only if a full interval was buffered before that boundary closed.
**When to use:** Every remote decode completion and every interval rollover.
**Example:**
```cpp
void promoteRemoteAtBoundary(RemoteQueueMap& queued,
                             RemoteActiveMap& active,
                             std::uint64_t boundaryIndex)
{
    active.clear();

    for (auto& [sourceId, sourceQueue] : queued)
    {
        if (!sourceQueue.empty() && sourceQueue.front().targetBoundary == boundaryIndex)
            active.emplace(sourceId, std::move(sourceQueue.front().buffer));
    }
}
```
// Source: recommended replacement for immediate promotion in `mixRemoteIntervalsIntoBuffer()`

### Anti-Patterns to Avoid
- **Rescaling current position mid-interval:** The current code rescales `transportIntervalPosition_` when BPM/BPI changes. That keeps UI moving but violates the locked "apply on next boundary" rule.
- **Remote playback on decode arrival:** Current remote promotion logic can advance as soon as `activeBySource` empties, independent of shared transport boundaries.
- **Running sync on non-audio clocks:** UI timers are acceptable for repainting, not for transport truth.
- **Continuing beat/progress during reconnect:** User constraints explicitly reject fake recovery timing.
- **Adding locks/log formatting in the audio hot path:** This phase is timing-sensitive; keep hot-path work to integer/sample arithmetic and bounded queue operations.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Transport authority | Separate wall-clock transport loop | Processor audio-thread sample counter + server timing config | Host audio callback is the only stable clock already synchronized to rendered output. |
| Remote sync | Per-source ad hoc lateness heuristics | One shared boundary index plus per-source queued candidates | Keeps all remote users on the same switch points. |
| Recovery UI truth | Fake countdown while waiting for timing | Explicit `waiting`, `reconnecting`, `timing lost` states | Honest UI is a locked requirement and avoids hidden drift. |
| Protocol parsing | New timing parser layer | Existing `FramedSocketTransport` timing/config path | The necessary wire messages are already parsed locally. |
| Test server | Mocked timing-only fakes disconnected from protocol | Extend `MiniNinjamServer` harness in `plugin_experimental_transport_tests.cpp` | Keeps validation tied to the real framing and interval messages already in use. |

**Key insight:** The risky part of this phase is not arithmetic, it is semantic ownership. If timing truth is split between UI timers, transport callbacks, and remote decode queues, drift and stagger will reappear even if each piece looks locally correct.

## Common Pitfalls

### Pitfall 1: Treating BPM/BPI as the Whole Sync Problem
**What goes wrong:** The processor uses the right BPM/BPI but still diverges because boundary ownership is local and rescaled mid-interval.
**Why it happens:** Config values are authoritative, but boundary transitions are still driven by mutable local state.
**How to avoid:** Add an explicit interval-boundary state machine and only commit timing changes there.
**Warning signs:** Beat/progress changes immediately after config updates, or a partial interval gets shortened/extended in place.

### Pitfall 2: Remote Audio Starts When Ready, Not When Aligned
**What goes wrong:** Users hear different remote participants enter at different sample offsets depending on decode arrival.
**Why it happens:** Current mixing promotes queued remote intervals whenever the active set empties.
**How to avoid:** Queue remote buffers for a target boundary and activate them only during shared boundary advancement.
**Warning signs:** A late user fades in mid-interval or one user advances while another enters late.

### Pitfall 3: Timing Continues Through Reconnect
**What goes wrong:** UI and metronome keep moving after connection loss, then silently disagree with real server timing.
**Why it happens:** The local clock is left running because it "looks smoother."
**How to avoid:** Clear authoritative timing immediately on reconnect/error/idle transitions and stop metronome clicks until timing returns.
**Warning signs:** Beat display still increments while the status says reconnecting.

### Pitfall 4: Off-by-One Boundary Eligibility
**What goes wrong:** Intervals that complete exactly on the boundary are inconsistently treated as on-time or late.
**Why it happens:** Eligibility logic mixes block-level and sample-level comparisons.
**How to avoid:** Centralize the exact rule in one helper and test three cases: before boundary, exactly on boundary, after boundary.
**Warning signs:** Flaky host tests that fail only at specific buffer sizes.

### Pitfall 5: Hard-Coded Timing Limits Reject Valid Server Config
**What goes wrong:** A real server BPM/BPI config is silently ignored because local min/max clamps are narrower than upstream behavior.
**Why it happens:** Current helper limits are locally chosen, not protocol-derived.
**How to avoid:** Trust protocol payload shape first, then enforce only clearly safe bounds with tests around edge values.
**Warning signs:** Connected session shows "waiting for server timing" despite config-change packets arriving.

## Code Examples

Verified patterns from local primary sources:

### Server Timing Config Parsing
```cpp
const auto* bytes = static_cast<const std::uint8_t*>(payload.getData());
const auto bpm = static_cast<int>(bytes[0]) | (static_cast<int>(bytes[1]) << 8);
const auto bpi = static_cast<int>(bytes[2]) | (static_cast<int>(bytes[3]) << 8);
```
// Source: `src/net/FramedSocketTransport.cpp`

### Upstream Config Change Message Layout
```cpp
beats_minute = *p++;
beats_minute |= ((int)*p++)<<8;
beats_interval = *p++;
beats_interval |= ((int)*p++)<<8;
```
// Source: `ninjam-upstream/ninjam/mpb.cpp`

### Upstream Interval Boundary Hook
```cpp
if (!x || m_interval_pos < 0)
{
  if (m_beatinfo_updated) { /* apply bpm/bpi */ }
  on_new_interval();
  m_interval_pos=0;
}
```
// Source: `ninjam-upstream/ninjam/njclient.cpp`

### Existing FamaLamaJam Reconnect Reset Pattern
```cpp
hasServerTimingForUi_.store(false, std::memory_order_relaxed);
serverBpmForUi_.store(0, std::memory_order_relaxed);
beatsPerIntervalForUi_.store(0, std::memory_order_relaxed);
currentBeatForUi_.store(0, std::memory_order_relaxed);
intervalProgressForUi_.store(0.0f, std::memory_order_relaxed);
```
// Source: `src/plugin/FamaLamaJamAudioProcessor.cpp`

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Local transport position is rescaled immediately when BPM/BPI changes | Stage the new interval/beat sizes and apply at the next shared boundary | Phase 3 recommendation | Prevents mid-interval cuts and hidden divergence. |
| Remote interval becomes active when decode/mix path runs out of current audio | Remote interval becomes active only at the authoritative boundary if fully buffered on time | Phase 3 recommendation | Eliminates arrival-time staggering between remote users. |
| UI infers transport mostly from a free-running local counter | UI reflects explicit sync state plus authoritative beat/progress | Phase 3 recommendation | Makes reconnect/waiting states honest and debuggable. |

**Deprecated/outdated:**
- Immediate `transportIntervalPosition_` rescaling on config change: replace with boundary-deferred application.
- `promoteNextRemoteInterval()` as a generic "whatever is queued next" switch: replace with boundary-targeted promotion.

## Open Questions

1. **Does current NINJAM wire protocol expose an explicit boundary epoch beyond BPM/BPI?**
   - What we know: Inspected upstream message definitions include config-change plus interval begin/write messages, but no explicit boundary timestamp field.
   - What's unclear: Whether some higher-level behavior elsewhere establishes authoritative phase beyond "first valid timing after connect."
   - Recommendation: Plan assuming no explicit boundary packet exists; if later evidence appears, adapt the timing engine behind the same state interface.

2. **What exact rule should define "ready exactly on the boundary"?**
   - What we know: User allows implementation discretion here, but anything after the boundary is late.
   - What's unclear: Whether exact-boundary completion should be accepted with zero tolerance or one-sample tolerance.
   - Recommendation: Choose one helper for eligibility, default to exact sample-domain acceptance, and cover exact/early/late cases in tests.

3. **Should local timing helper bounds be widened or derived from server payloads?**
   - What we know: Current code uses local guardrails (`kMinBpm`, `kMaxBpm`, `kMinBpi`, `kMaxBpi`), which may not match all upstream/server behavior.
   - What's unclear: Whether any target rehearsal servers use values outside the current local limits.
   - Recommendation: Keep guardrails but add integration tests at realistic edge values and avoid silently discarding valid server timing without surfacing status.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 3.8.1 + CTest |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `ctest --test-dir build-vs-ninja --output-on-failure --timeout 30 -R plugin_experimental_transport` |
| Full suite command | `ctest --test-dir build-vs-ninja --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| SYNC-01 | Timing engine follows server BPM/BPI and only applies changes at interval boundaries | integration | `ctest --test-dir build-vs-ninja --output-on-failure --timeout 30 -R plugin_server_timing_sync` | no - Wave 0 |
| SYNC-02 | UI state reports waiting/reconnecting/active timing plus beat-divided progress | integration | `ctest --test-dir build-vs-ninja --output-on-failure --timeout 30 -R plugin_transport_ui_sync` | no - Wave 0 |
| SYNC-03 | Long-run rehearsal timing shows no cumulative drift and reconnect re-aligns automatically | integration | `ctest --test-dir build-vs-ninja --output-on-failure --timeout 30 -R plugin_remote_interval_alignment` | no - Wave 0 |

### Sampling Rate
- **Per task commit:** `ctest --test-dir build-vs-ninja --output-on-failure --timeout 30 -R "(plugin_server_timing_sync|plugin_transport_ui_sync|plugin_remote_interval_alignment)"`
- **Per wave merge:** `ctest --test-dir build-vs-ninja --output-on-failure`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/integration/plugin_server_timing_sync_tests.cpp` - authoritative boundary math, deferred BPM/BPI change application, reconnect timing reset
- [ ] `tests/integration/plugin_transport_ui_sync_tests.cpp` - explicit waiting/reconnecting/timing-lost UI state and beat-divided progress snapshots
- [ ] `tests/integration/plugin_remote_interval_alignment_tests.cpp` - multi-source boundary-quantized remote switching and late-interval skip behavior
- [ ] `tests/integration/support/MiniNinjamServer.h` - shared server harness extracted from `plugin_experimental_transport_tests.cpp` so Phase 3 tests do not duplicate protocol scaffolding

## Sources

### Primary (HIGH confidence)
- Local repo `src/plugin/FamaLamaJamAudioProcessor.cpp` / `.h` / `FamaLamaJamAudioProcessorEditor.cpp` - current timing, metronome, reconnect reset, and UI transport behavior
- Local repo `src/net/FramedSocketTransport.cpp` / `.h` - current server timing parsing and remote interval frame handling
- Local repo `tests/integration/plugin_experimental_transport_tests.cpp` - existing mini-server harness and protocol integration coverage
- Local upstream `ninjam-upstream/ninjam/mpb.h` / `mpb.cpp` - authoritative NINJAM message layouts for config change and interval transfer
- Local upstream `ninjam-upstream/ninjam/njclient.cpp` / `njclient.h` - upstream interval progression, metronome triggering, and boundary-based remote queue advancement
- Local upstream `ninjam-upstream/ninjam/server/usercon.cpp` - server config broadcast and interval loop behavior

### Secondary (MEDIUM confidence)
- https://docs.juce.com/master/classjuce_1_1AudioProcessor.html - processor callback ownership and plugin lifecycle API
- https://docs.juce.com/master/classjuce_1_1Timer.html - UI/message-thread timer behavior, useful as a contrast to audio-thread timing
- https://github.com/catchorg/Catch2 - current Catch2 project/docs landing page for v3 usage conventions

### Tertiary (LOW confidence)
- None. I did not rely on unverified web-search-only claims for the phase recommendations.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - pinned local dependency versions and existing repo structure are explicit.
- Architecture: MEDIUM - recommendations are strongly grounded in local code and upstream semantics, but the protocol appears not to expose an explicit boundary epoch.
- Pitfalls: HIGH - directly evidenced by current code paths, user constraints, and upstream boundary-switch behavior.

**Research date:** 2026-03-14
**Valid until:** 2026-04-13
