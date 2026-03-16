# Phase 4: Audio Streaming, Mix, and Monitoring Core - Research

**Researched:** 2026-03-15
**Domain:** Ninjam streaming hardening, per-channel monitoring controls, and JUCE plugin mixer UI/state persistence
**Confidence:** MEDIUM

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Treat transmit and receive as an existing working baseline, not a net-new capability to invent from scratch.
- Keep the current interval-based send/receive model aligned to Phase 3 timing.
- Remote controls are keyed by `user + channel`, grouped by user, with stable ordering.
- Phase 4 controls are meter, gain, pan, and mute for both remote strips and a dedicated local monitor strip.
- Metering is simple realtime metering, not diagnostics or peak-hold tooling.
- Local monitor must be visually distinct from delayed remote return.
- Inactive remote strips should remain briefly visible, then hide if still gone.
- Per-channel gain/pan/mute settings should persist in project state and restore for the same source identity.
- Solo is deferred.

### Deferred Ideas (OUT OF SCOPE)
- Solo controls
- Advanced routing or patchbay behavior
- Dedicated diagnostics panels
- Host lifecycle hardening beyond streaming compatibility
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| AUD-01 | User can send local input audio to the session using a working Ninjam-compatible encode path. | Preserve the current interval-based upload path and harden it under supported runtime changes. |
| AUD-02 | User can receive and decode remote participant audio in-session. | Promote remote channels into a stable per-source model with identity, continuity, and UI visibility. |
| AUD-03 | Audio pipeline handles supported host sample rates and buffer sizes without pitch/tempo artifacts. | Reinitialize streaming state coherently on host changes and add dedicated compatibility coverage. |
| MIX-01 | User can control essential per-channel monitor/mix parameters (gain, mute, basic pan). | Add processor-owned per-channel mix state and editor bindings for local and remote strips. |
| MIX-02 | UI clearly distinguishes local monitor audio from interval-delayed remote return. | Build a dedicated local strip and grouped remote strips on the existing editor surface. |
</phase_requirements>

## Summary

The current codebase already has a functioning streaming core: `FamaLamaJamAudioProcessor` buffers local audio by authoritative interval, `CodecStreamBridge` performs queued encode/decode work, and `FramedSocketTransport` handles Ninjam auth plus interval upload/download messages. Phase 4 does not need a transport rewrite. The real gaps are a productization layer on top of that core: stable per-channel source identity, per-strip mix state, strip metering, UI rendering for local and remote strips, and persistence of dynamic mix settings.

The safest design is processor-owned truth with editor snapshot consumption. Remote identity should remain protocol-aligned as `user + channel`, while labels can use the current username plus channel metadata when available. Per-channel persistence belongs in the wrapped plugin state tree, not in `SessionSettingsSerializer`, which only covers static endpoint/default settings today.

**Primary recommendation:** Split Phase 4 into three plans: first build processor-owned source/mix/meter state and identity plumbing; second build the dedicated local strip plus grouped remote mixer UI and persistence-driven restoration behavior; third harden sample-rate/block-size behavior and prove artifact-free compatibility under active streaming.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 8.0.12 | Audio processor/editor lifecycle, plugin state, GUI widgets | Already owns the plugin host boundary and current editor implementation. |
| C++20 | repo standard | Source identity maps, state snapshots, and RT-safe control plumbing | Already enforced by top-level CMake. |
| Existing `FramedSocketTransport` | local | Ninjam auth, user subscriptions, interval upload/download payloads | Reuse rather than adding another protocol layer. |
| Existing `CodecStreamBridge` | local | Queued encode/decode bridge and sample-rate preservation | Already integrated and tested. |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Catch2 | 3.8.1 | Unit/integration tests | Use for mix-state, UI snapshot, and runtime compatibility coverage. |
| CTest | CMake-integrated | Test discovery and phase gates | Use the existing `famalamajam_tests` target and `build-vs` path. |
| Existing mini Ninjam server harness | local | End-to-end protocol validation | Extend the current support harness rather than inventing a fake transport. |

## Architecture Patterns

### Pattern 1: Processor-Owned Source Snapshot Model
Keep one processor-owned snapshot describing the local strip and grouped remote strips, and expose a read-only editor getter. Use this for UI rendering, tests, reconnect behavior, and project restore.

### Pattern 2: Wrapped Plugin-State Persistence for Dynamic Mix Settings
Store per-remote-channel gain/pan/mute in the plugin's wrapped state tree rather than in `SessionSettingsSerializer`. Persist by stable source identity and reapply when the same source reappears.

### Pattern 3: RT-Safe Meter Snapshotting
Update simple strip meters inside the processor, then publish atomics/snapshots for the editor. Do not make the editor inspect audio buffers directly.

### Anti-Patterns to Avoid
- Editor-owned strip state as the source of truth
- Serializing dynamic remote mix state through `SessionSettingsSerializer`
- Reordering strips by loudness or current activity
- Blocking locks in the audio path for meter/control updates
- Treating a remote user as a single stream rather than `user + channel`

## Common Pitfalls

### Pitfall 1: Remote mix state disappears on reconnect
Keying state to transient runtime objects rather than stable source identity will lose user balances.

### Pitfall 2: Local and remote paths are visually ambiguous
If the local path is shown like any other strip or hidden entirely, users will monitor the wrong signal.

### Pitfall 3: Runtime sample-rate changes restore pitch but break continuity
Buffers, meters, and decode assumptions need coherent reset/resizing on host reconfiguration.

### Pitfall 4: Friendly labels are unstable
If protocol metadata and persisted mix state are not joined through stable source identity, labels will jump between raw IDs and names.

## Code Examples

### Existing wrapped plugin state entry point
```cpp
juce::ValueTree stateTree(kPluginStateType);
stateTree.setProperty(kPluginStateSchemaVersion, kPluginStateSchema, nullptr);
stateTree.setProperty(kPluginStateMetronomeEnabled, metronomeEnabled_.load(std::memory_order_relaxed), nullptr);
```
// Source: `src/plugin/FamaLamaJamAudioProcessor.cpp`

### Existing remote source identity
```cpp
std::string sourceId(reinterpret_cast<const char*>(usernameBytes), usernameLength);
sourceId.push_back('#');
sourceId += std::to_string(static_cast<unsigned>(channelIndex));
```
// Source: `src/net/FramedSocketTransport.cpp`

### Existing interval-based local upload path
```cpp
if (localUploadIntervalWritePosition_ >= authoritativeTiming_.activeIntervalSamples)
{
    codecStreamBridge_.submitInput(localUploadIntervalBuffer_, currentSampleRate_);
    localUploadIntervalBuffer_.clear();
    localUploadIntervalWritePosition_ = 0;
}
```
// Source: `src/plugin/FamaLamaJamAudioProcessor.cpp`

## Open Questions

1. Should channel labels use friendly channel names when known and fall back to channel index otherwise?
2. What exact inactive timeout is best before hiding a remote strip?
3. How broad should runtime sample-rate/block-size compatibility coverage be beyond 44.1/48 kHz and representative block sizes?

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 3.8.1 + CTest |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && .\build-vs\tests\famalamajam_tests.exe "[plugin_experimental_transport],[plugin_remote_interval_alignment],[plugin_codec_runtime],[plugin_mixer_controls],[plugin_mixer_ui],[plugin_streaming_runtime_compat]"'` |
| Full suite command | `cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && ctest --test-dir build-vs --output-on-failure'` |
| Estimated runtime | ~45 seconds |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| AUD-01 | Local upload remains encoded and sent under supported runtime conditions | integration | `famalamajam_tests.exe "[plugin_experimental_transport]"` | yes |
| AUD-02 | Remote sources decode, remain individually identifiable, and render with continuity | integration | `famalamajam_tests.exe "[plugin_remote_interval_alignment]"` | yes |
| AUD-03 | Runtime sample-rate and block-size changes do not introduce pitch or continuity artifacts | integration | `famalamajam_tests.exe "[plugin_streaming_runtime_compat]"` | no - Wave 0 |
| MIX-01 | Per-channel gain, pan, and mute affect local/remote strips and restore from project state | integration | `famalamajam_tests.exe "[plugin_mixer_controls]"` | no - Wave 0 |
| MIX-02 | UI shows dedicated local monitor strip and grouped remote strips clearly | integration | `famalamajam_tests.exe "[plugin_mixer_ui]"` | no - Wave 0 |

### Wave 0 Gaps
- [ ] `tests/integration/plugin_mixer_controls_tests.cpp`
- [ ] `tests/integration/plugin_mixer_ui_tests.cpp`
- [ ] `tests/integration/plugin_streaming_runtime_compat_tests.cpp`

## Sources

### Primary (HIGH confidence)
- `src/plugin/FamaLamaJamAudioProcessor.cpp` / `.h`
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp` / `.h`
- `src/net/FramedSocketTransport.cpp` / `.h`
- `src/audio/CodecStreamBridge.cpp` / `.h`
- `src/infra/state/SessionSettingsSerializer.cpp`
- `tests/integration/plugin_experimental_transport_tests.cpp`
- `tests/integration/plugin_remote_interval_alignment_tests.cpp`
- `tests/integration/plugin_codec_runtime_tests.cpp`
- `ninjam-upstream/ninjam/njclient.cpp` / `njclient.h`

### Secondary (MEDIUM confidence)
- JUCE `AudioProcessor` and `ValueTree` docs, consistent with the local code patterns already in use

## Metadata

**Research date:** 2026-03-15
**Valid until:** 2026-04-14
