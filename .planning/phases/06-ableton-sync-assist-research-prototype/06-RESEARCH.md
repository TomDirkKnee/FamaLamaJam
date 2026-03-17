# Phase 6: Ableton Sync Assist Research & Prototype - Research

**Researched:** 2026-03-16
**Domain:** Ableton host-play-start sync assist, host transport observation, and safe UX boundaries for a server-authoritative NINJAM plugin
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

#### Host-sync assist behavior
- Phase 6 centers on an explicit host-play-start sync assist, not loop-brace control as the main interaction.
- The assist is an armed one-shot action: wait for Ableton playback to start, then align from the host's current musical position.
- If Ableton starts mid-bar, use offset alignment from the host musical position rather than forcing a bar-start-only workflow.
- While armed and waiting, hold Ninjam playback and metronome rather than letting interval playback continue.
- After a successful synced start, the plugin stays free-running instead of automatically returning to waiting state on host stop.

#### Arming preconditions and failure behavior
- Arming is only valid when real room timing exists.
- Arming must be blocked unless Ableton tempo already matches the room BPM.
- If room timing is lost while armed, cancel the armed state.
- If aligned start cannot complete when Ableton begins playback, fail visibly and stay stopped.
- Retry after failure requires explicit manual re-arming.

#### Trigger flow
- Use a dedicated one-shot control such as `Arm Sync to Ableton Play`.
- The same control also cancels the waiting state.
- Arming resets automatically after a successful synced start.
- This is not a persistent always-on sync mode.

#### UI framing
- Put the host-sync assist near the existing transport/status area.
- Keep it secondary but clear, not louder than connect/status workflow.
- Show room BPM/BPI near the control.
- If Ableton tempo does not match room BPM, disable the arm control with a short inline explanation.
- Armed state should use explicit status text.

#### Secondary helper scope
- Phase 6 may show room BPM/BPI clearly as supporting information.
- Do not promise host tempo push or loop-region manipulation as the primary Phase 6 outcome.
- Tempo/loop helpers remain secondary research items unless they prove safe and feasible.

### Claude's Discretion
- Exact armed/blocked/canceled/failed-start copy.
- Exact control label/styling in the current JUCE UI.
- Whether a minor secondary tempo-helper affordance appears in prototype form, as long as host-play-start sync remains primary.

### Deferred Ideas (OUT OF SCOPE)
- Direct host tempo push or loop-region manipulation as the primary deliverable.
- Persistent always-on host sync mode.
- Broader DAW transport integration beyond one-shot armed start.
- Deeper parity work that depends on host capabilities the plugin cannot safely assume.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| HSYNC-01 | User can align Ableton tempo/loop helpers to room BPM/BPI without violating server-authoritative Ninjam timing. | The safest transferable behavior is read-only host transport observation plus explicit one-shot armed start alignment. Generic plugin-side host tempo/loop control is not the right baseline assumption. |
| HSYNC-02 | Any host-sync assist behavior is explicit, optional, and understandable from the plugin UI. | JamTaba's pattern is a deliberate wait-for-host-sync mode, which matches the desired explicit armed UX and supports clear blocked/armed/failure states. |
</phase_requirements>

## Summary

Phase 6 should not begin by trying to "sync Ableton" in a generic DAW-control sense. The local evidence strongly points to a narrower and safer interpretation: observe host playhead state from the plugin, then offer an explicit armed start helper that aligns the Ninjam engine when the host transport actually starts.

JUCE already exposes the read-only host timing data needed for this. `juce::AudioPlayHead::PositionInfo` provides BPM, PPQ position, last bar start, time signature, and play-state through `getPosition()` in the audio callback, and `AudioProcessor` supports `getPlayHead()`/`setPlayHead()` for runtime use and test injection. That means the codebase can prototype host-start detection and musical-position alignment without inventing new host APIs or depending on Ableton-specific SDK hooks.

The JamTaba reference confirms the product direction. Its plugin layer detects host transport start by comparing current host play-state with the previous audio callback, computes a start offset from `ppqPos` and `barStartPos`, and keeps the Ninjam controller in a waiting-for-host-sync state until the host actually starts playback. That is materially different from upstream ReaNINJAM's REAPER-specific helper menu items such as `Set project tempo`, `Set loop`, and `Start playback on next loop`, which rely on host-specific functions not available through a generic plugin path.

That difference is the key planning constraint: Phase 6 should treat generic VST3/JUCE host transport observation as feasible, while treating direct host tempo and loop manipulation as speculative and secondary. The plan should therefore split into three concerns: 1) add a processor-owned host transport snapshot + armed sync state model, 2) expose a clear armed/blocked/canceled UI around the current transport area, and 3) validate the exact behavior with injected playhead tests plus a small manual Ableton check, so we learn whether Ableton actually supplies the expected playhead data under the real VST3 host path.

**Primary recommendation:** Plan Phase 6 as a focused prototype in three streams: 1) host-playhead observation and armed sync state machine in the processor, 2) a transport-adjacent one-shot `Arm Sync to Ableton Play` UI with strict eligibility gating, and 3) automated playhead-driven tests plus a concise manual Ableton validation note to confirm the behavior under the real host.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 8.0.12 | Host playhead access via `AudioPlayHead::PositionInfo`, plugin processor/editor framework | Already pinned in `cmake/dependencies.cmake` and directly supports the required read-only host transport observation. |
| C++ | C++20 | Processor/editor/session implementation | Already enforced by the top-level project configuration. |
| Existing `FamaLamaJamAudioProcessor` + `FamaLamaJamAudioProcessorEditor` | local | Own authoritative timing, lifecycle state, transport UI, and editor callbacks | Reuse the processor-owned truth rather than adding a second sync controller layer. |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Catch2 | 3.8.1 | Unit/integration tests | Use for injected playhead tests and UI-state verification. |
| CTest | CMake-integrated | Full Windows suite gate | Keep as the full phase verification path. |
| Existing `build-vs` path | local repo standard | Stable MSVC build/test path on this machine | Continue using it as the canonical Windows validation path. |
| JamTaba reference source | local reference repo | Product-pattern reference for host-start sync behavior | Use as a behavioral reference, not as an architectural dependency. |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Generic JUCE playhead observation | Host-specific tempo/loop control APIs | Host-specific control is less portable, less testable, and does not fit the chosen "explicit assist" framing for this phase. |
| Processor-owned armed sync state | Editor-owned transient sync mode | Harder to keep timing loss, reconnect, and audio-thread-triggered host start behavior coherent. |
| One-shot armed start | Persistent always-on sync mode | Conflicts with the locked product direction and makes failure/eligibility states harder to explain. |

**Build baseline:**
```bash
cmake --build build-vs --target famalamajam_tests --config Debug
cmake --build build-vs --target famalamajam_plugin_VST3 --config Debug
```

## Architecture Patterns

### Pattern 1: Read Host State, Do Not Assume Host Control
**What:** Use `getPlayHead()->getPosition()` inside `processBlock()` to read host BPM, PPQ position, last bar start, and `isPlaying`, but treat the host as observational data rather than something the plugin can safely command.
**When to use:** Host-start detection, tempo-match gating, and offset computation for the armed sync assist.
**Why:** JUCE exposes this read-only data generically; it does not make generic host tempo/loop mutation the baseline pattern.
**Example:**
```cpp
if (auto* playHead = getPlayHead())
    if (auto position = playHead->getPosition())
        isPlaying = position->getIsPlaying();
```
Source: JUCE `juce_AudioPlayHead.h`, `AudioPluginDemo.h`

### Pattern 2: Processor-Owned Armed Sync State
**What:** Keep the armed/waiting/blocked/failure state in `FamaLamaJamAudioProcessor`, next to lifecycle state and authoritative timing, and surface it through editor polling.
**When to use:** Arm/disarm actions, timing-loss cancellation, tempo-match gating, and host-start transition.
**Why:** The processor already owns lifecycle and transport truth; Phase 6 should extend that model rather than creating a UI-side sync mode.

### Pattern 3: Offset Alignment From Musical Position
**What:** Compute the aligned start offset from host musical position (`ppqPos` and bar start reference) rather than assuming host playback only starts exactly at the beginning of a measure.
**When to use:** Host starts mid-bar or when host bar math is slightly non-integer due to floating-point rounding.
**Why:** JamTaba already uses this approach and it matches the user's desired behavior.

### Pattern 4: Honest Eligibility And Failure Messaging
**What:** Disable arming until real room timing exists and host tempo matches room BPM; cancel armed state on timing loss; fail visibly rather than falling back to fake sync.
**When to use:** UI-state rendering, status copy, and state-machine transitions.
**Why:** This directly continues the Phase 3/5 product principle of explicit, non-deceptive sync behavior.

### Pattern 5: Injected PlayHead Testing
**What:** Use `AudioProcessor::setPlayHead()` with a test playhead to simulate host play state and musical position in integration tests.
**When to use:** Verifying armed-state transitions, offset computation, tempo-match gating, and one-shot reset behavior without needing Ableton for every case.
**Why:** JUCE supports injected playheads, and this is the lowest-risk way to automate most of Phase 6.

### Anti-Patterns to Avoid
- **Assuming the plugin can set Ableton tempo generically:** this is not supported by the current JUCE/plugin path evidence.
- **Background auto-sync behavior:** conflicts with the locked explicit one-shot armed workflow.
- **Arming without room timing:** violates the chosen "no fake sync" rule.
- **Silent fallback on failed aligned start:** undermines the point of explicit assist behavior.
- **Editor-owned sync logic:** risks drift from processor timing and lifecycle truth.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Host transport access | Custom host-specific hooks first | JUCE `AudioPlayHead::PositionInfo` path | It is already available, portable, and testable in the current stack. |
| Sync UX | Hidden "smart" background mode | Explicit one-shot arm/disarm control | Matches user decisions and JamTaba's wait-for-host pattern. |
| Eligibility checks | Loose best-effort warnings only | Hard block when tempo mismatches or timing is absent | Prevents fake or misleading sync starts. |
| Phase validation | Ableton-only manual experimentation | Injected playhead tests plus one small manual Ableton check | Gives fast iteration and still validates the real host path. |

**Key insight:** The transferable value from JamTaba is not "control the DAW." It is "wait for the DAW transport start and align the Ninjam engine from the host's musical position." That narrower behavior is both technically plausible in the current stack and consistent with the user's chosen product framing.

## Common Pitfalls

### Pitfall 1: Using Host BPM As If It Were Authoritative Room Timing
**What goes wrong:** The processor starts trusting host BPM or bar position in place of room BPM/BPI, causing the plugin to drift from server timing.
**Why it happens:** Host-sync work can easily blur observational host data with authoritative room timing.
**How to avoid:** Keep room timing as the source of truth; use host data only to decide when and where to start the one-shot aligned assist.

### Pitfall 2: Reading Playhead Outside The Audio Callback
**What goes wrong:** Host playhead data becomes undefined or inconsistent because JUCE only guarantees meaningful playhead context during processing.
**Why it happens:** It's tempting to read host state from UI timers or button handlers.
**How to avoid:** Capture host transport snapshots during `processBlock()` and publish the relevant state to the UI through the existing snapshot model.

### Pitfall 3: Armed State Survives Timing Loss Or Tempo Mismatch
**What goes wrong:** The plugin stays "armed" even after conditions become invalid, leading to surprising or misleading starts.
**Why it happens:** State-machine edges around reconnect, timing loss, or host tempo changes are easy to miss.
**How to avoid:** Make timing availability and tempo-match validity first-class guards in the armed sync state model.

### Pitfall 4: Tests Prove Logic But Not Real Host Behavior
**What goes wrong:** Injected playhead tests all pass, but Ableton VST3 does not supply the exact playhead fields the prototype expects.
**Why it happens:** Host implementations vary in what they populate.
**How to avoid:** Keep the prototype narrow, guard against missing fields, and include a minimal manual Ableton validation note in the phase gate.

### Pitfall 5: UI Suggests More Control Than The Plugin Actually Has
**What goes wrong:** A "sync" label makes users think the plugin will actively control Ableton tempo or looping.
**Why it happens:** Transport-related UI language can easily over-promise.
**How to avoid:** Use explicit wording like `Arm Sync to Ableton Play`, show room BPM/BPI as target info, and explain blocked conditions inline.

## Code Examples

Verified local/reference patterns Phase 6 should build on:

### JUCE PositionInfo Provides The Needed Host Data
```cpp
if (auto result = ph->getPosition())
    return *result;
```
Source: `build-vs/_deps/juce-src/examples/Plugins/AudioPluginDemo.h`

### JUCE PositionInfo Includes BPM, PPQ, Bar Start, And Playing State
```cpp
Optional<double> getBpm() const;
Optional<double> getPpqPosition() const;
Optional<double> getPpqPositionOfLastBarStart() const;
bool getIsPlaying() const;
```
Source: `build-vs/_deps/juce-src/modules/juce_audio_basics/audio_play_head/juce_AudioPlayHead.h`

### JamTaba Detects Transport Start By Comparing Successive Audio Callbacks
```cpp
bool JamTabaPlugin::transportStartDetectedInHost() const
{
    return hostIsPlaying() && !hostWasPlayingInLastAudioCallBack;
}
```
Source: `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Plugins/JamTabaPlugin.h`

### JamTaba Computes A Start Offset From Host Musical Position
```cpp
double cursorPosInMeasure = timeInfo->ppqPos - timeInfo->barStartPos;
double samplesUntilNextMeasure = (timeInfo->timeSigNumerator - cursorPosInMeasure) * samplesPerBeat;
startPosition = -samplesUntilNextMeasure;
```
Source: `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Plugins/VST/JamTabaVSTPlugin.cpp`

### JamTaba Holds NINJAM Playback While Waiting For Host Sync
```cpp
if (!waitingForHostSync)
    NinjamController::process(in, out, sampleRate);
else
    controller->doAudioProcess(in, out, sampleRate);
```
Source: `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Plugins/NinjamControllerPlugin.cpp`

## Open Questions

1. **Does Ableton VST3 reliably provide bar-start PPQ information through JUCE in the exact scenarios we care about?**
   - What we know: JUCE exposes the fields and JamTaba relies on equivalent VST timing fields.
   - What's unclear: Whether Ableton always populates `ppqPositionOfLastBarStart` and related data during the targeted workflow.
   - Recommendation: Build guards for missing data and make this part of the manual phase validation.

2. **Should tempo mismatch be strict equality or allow a tiny tolerance?**
   - What we know: The user wants arming blocked unless Ableton tempo matches room BPM.
   - What's unclear: Whether real host tempo representation needs a small float tolerance.
   - Recommendation: Treat this as planner discretion with a conservative tolerance if required by host numeric representation.

3. **What exact fallback is acceptable when host musical-position data is partially missing but play-state is available?**
   - What we know: Fake sync is not acceptable.
   - What's unclear: Whether "fail visibly" should happen immediately or after attempting a limited bar-start-only path.
   - Recommendation: Prefer immediate visible failure unless the required fields for the chosen offset-alignment behavior are present.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 3.8.1 + CTest |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && .\build-vs\tests\famalamajam_tests.exe "[plugin_host_sync_assist],[plugin_transport_ui_sync]"'` |
| Full suite command | `cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && ctest --test-dir build-vs --output-on-failure'` |
| Build command | `cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build build-vs --target famalamajam_tests --config Debug && cmake --build build-vs --target famalamajam_plugin_VST3 --config Debug'` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| HSYNC-01 | Armed sync assist observes host transport and aligns start without breaking server-authoritative room timing. | integration | `famalamajam_tests.exe "[plugin_host_sync_assist]"` | no - Wave 0 |
| HSYNC-02 | Arming is explicit, optional, blocked on invalid conditions, and visible in the UI/status model. | integration + UI | `famalamajam_tests.exe "[plugin_host_sync_assist],[plugin_transport_ui_sync]"` | partially - extend existing UI suite plus Wave 0 host-sync tests |

### Sampling Rate
- **Per task commit:** Run the quick command above once the host-sync suite exists.
- **Per wave merge:** Run the full suite command.
- **Phase gate:** Build tests + VST3, run the full suite, then run a concise manual Ableton validation covering armed start, blocked arming on tempo mismatch, cancel-on-timing-loss, and one-shot reset behavior.

### Wave 0 Gaps
- [ ] `tests/integration/plugin_host_sync_assist_tests.cpp` - injected playhead coverage for arming, host-start detection, offset alignment, cancellation, failure, and one-shot reset
- [ ] Extend transport/editor UI coverage for disabled-arm explanation, armed banner text, and room BPM/BPI display near the host-sync control
- [ ] Add a processor seam or helper for host transport snapshot capture if the current direct `getPlayHead()` usage is too hard to test cleanly
- [ ] `docs/validation/phase6-ableton-sync-matrix.md` - concise manual Ableton validation note for the real VST3 host path

### Manual Ableton Matrix

Use the `build-vs` VST3 artifact and record pass/fail notes:

| Case ID | Scenario | Expected Result |
|---------|----------|-----------------|
| P6-HSYNC-01 | Connected with room timing and matching Ableton tempo, arm sync, then press Play | Plugin begins aligned start from host musical position and clears armed state after success |
| P6-HSYNC-02 | Connected with room timing but mismatched Ableton tempo | Arm control is disabled with a short inline explanation |
| P6-HSYNC-03 | Arm sync, then lose room timing/reconnect before pressing Play | Armed state cancels and UI explains that sync assist is no longer available |
| P6-HSYNC-04 | Arm sync, cancel before pressing Play | Same control cleanly disarms with no hidden waiting state |
| P6-HSYNC-05 | Successful aligned start, then stop and press Play again without re-arming | Plugin does not return to waiting-for-host mode automatically |

## Sources

### Primary (HIGH confidence)
- `.planning/phases/06-ableton-sync-assist-research-prototype/06-CONTEXT.md`
- `.planning/ROADMAP.md`
- `.planning/REQUIREMENTS.md`
- `.planning/STATE.md`
- `src/plugin/FamaLamaJamAudioProcessor.cpp`
- `src/plugin/FamaLamaJamAudioProcessor.h`
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `src/plugin/FamaLamaJamAudioProcessorEditor.h`
- `build-vs/_deps/juce-src/modules/juce_audio_basics/audio_play_head/juce_AudioPlayHead.h`
- `build-vs/_deps/juce-src/examples/Plugins/AudioPluginDemo.h`
- `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Plugins/JamTabaPlugin.h`
- `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Plugins/VST/JamTabaVSTPlugin.cpp`
- `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Plugins/VST/JamTabaVSTPlugin.h`
- `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Plugins/NinjamControllerPlugin.cpp`
- `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Plugins/NinjamControllerPlugin.h`
- `ninjam-upstream/jmde/fx/reaninjam/winclient.cpp`
- `ninjam-upstream/jmde/fx/reaninjam/res.rc`
- `tests/CMakeLists.txt`

### Secondary (MEDIUM confidence)
- `.planning/research/SUMMARY.md`
- prior phase context/research files for timing and lifecycle conventions

## Metadata

**Confidence breakdown:**
- Host playhead observation: HIGH - directly supported by JUCE APIs present in the repo.
- JamTaba behavior transfer: HIGH - local source clearly shows the wait-for-host-start model and offset logic.
- Generic host tempo/loop control: LOW-MEDIUM - upstream ReaNINJAM support is host-specific and not evidence of portable VST3/JUCE control.
- Validation approach: HIGH - injected playheads plus manual Ableton confirmation fit the current test/build stack.

**Research date:** 2026-03-16
**Valid until:** 2026-04-15
