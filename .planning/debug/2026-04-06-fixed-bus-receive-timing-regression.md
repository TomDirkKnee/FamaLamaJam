# 2026-04-06 Fixed-Bus Receive Timing Regression

## Context

Phase `08.3.4.1` introduced the first real fixed-bus multi-input implementation:
- `Main` plus `Local Send 2` local transmit slots
- fixed remote output assignments
- hidden-channel publish/unpublish support

During live Ableton validation, the second local channel was initially silent on the receiving client. While troubleshooting that path, a later regression appeared:
- remote audio on the extra channel became audible
- but playback dropped out roughly once per interval
- in some runs the main remote channel started showing the same queued/refresh behavior

## What Was Confirmed Working

These parts were confirmed good and were kept:
- real packed `ClientSetChannelInfo` publishing for multiple local NINJAM channels
- hidden extra local channel unpublish
- local `Local Send 2` strip metering
- local `Local Send 2` foldback into FLJ main output
- snapshot-based local upload capture from copied host input buffers

The copied-input snapshot change was important because diagnostics later proved:
- sender `txRms1` became non-zero
- receiver `jim#1 decRms` became non-zero

So the sidechain/local-send silence bug was genuinely fixed before the later dropout issue.

## Regression Cause

The most likely regression source was the newer live receive scheduling path added during troubleshooting:
- `boundaryGeneration` propagated through transport and codec decode
- processor-side `preferredTargetBoundary` assignment inferred playback timing from the first observed download-begin generation
- live network intervals were then forced onto inferred target boundaries instead of following the older sequential remote queueing model

Files involved:
- `src/net/FramedSocketTransport.h`
- `src/net/FramedSocketTransport.cpp`
- `src/audio/CodecStreamBridge.h`
- `src/audio/CodecStreamBridge.cpp`
- `src/plugin/FamaLamaJamAudioProcessor.cpp`

Why this looked like the culprit:
- before the timing inference work, the problem was silent upload/capture
- after the local upload snapshot fix, diagnostics showed healthy transmit RMS and healthy decode RMS
- despite that, the remote audio still refreshed or dropped once per interval
- that points to activation scheduling, not transport silence

## Rollback Applied

The rollback was intentionally narrow:
- removed the processor-side live `preferredTargetBoundary` mapping
- removed the `firstObservedDownloadBoundaryGeneration_` and related local anchor state
- returned live remote interval scheduling to the older sequential `nextRemoteTargetBoundaryBySource_` model
- kept the transport/codec boundary metadata plumbing in place for now, but stopped consuming it in the processor’s live receive path

Files changed for the rollback:
- `src/plugin/FamaLamaJamAudioProcessor.h`
- `src/plugin/FamaLamaJamAudioProcessor.cpp`

## Validation After Rollback

Focused automation rerun on `build-vs-2026`:
- `famalamajam_tests.exe "[plugin_host_multi_io_routing],[plugin_remote_interval_alignment],[plugin_voice_mode_transport]"`
- result: pass, `179` assertions across `18` test cases

Debug VST3 rebuilt:
- `build-vs-2026/famalamajam_plugin_artefacts/Debug/VST3/FamaLamaJam.vst3`

## Current Position

As of this note:
- the fixed-bus local send capture path is preserved
- the newer live receive-boundary inference has been backed out
- focused automation is green again
- the next required check is a real Ableton retest of the Jim/Ben sidechain case to confirm whether the once-per-interval dropout is gone

## Practical Next Check

Retest in Ableton with:
- `Jim` transmitting `Main` plus `Local Send 2`
- `Ben` receiving both channels

Expected post-rollback behavior:
- `jim - bass` remains audible on the receiver
- no once-per-interval refresh/drop on the extra channel
- no new flicker/regression on the main channel
