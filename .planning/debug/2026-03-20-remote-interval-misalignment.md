---
status: partial_fix_verified
trigger: "Remote receive timing/audio loss: local side-by-side offset, ReaNINJAM half-interval then silence"
created: 2026-03-20T00:00:00Z
updated: 2026-03-20T03:15:00Z
---

## Current Focus

hypothesis: the core interval-sync issue was caused by outbound interval capture drifting away from the authoritative interval position; once `upl` is derived directly from `samplesIntoInterval`, receive alignment should stabilize
test: keep the safer transmit warmup, remove the aggressive download-begin phase snap, and drive outbound buffer placement from the current interval position each block
expecting: `upl` should track `pos`, full intervals should continue decoding cleanly, and the receiver metronome should stay aligned in normal single-instance use
next_action: treat the core timing path as provisionally fixed for single-instance validation; track the remaining two-instance UI freeze as a separate non-blocking limitation unless it begins affecting normal single-instance use

## Symptoms

expected: received remote intervals should stay aligned to the server interval/metronome across reconnects and across other NINJAM clients, including ReaNINJAM
actual:
- two FamaLamaJam instances side by side can be about half a beat out of sync at 100 BPM
- ReaNINJAM -> FamaLamaJam can play for about four beats (half an interval) and then go silent permanently
- some public NINJAM rooms appear to stay in sync
errors: none reported in-plugin for this symptom
reproduction:
- run two FamaLamaJam instances side by side and monitor one from the other
- run ReaNINJAM/Reaper into the same room and monitor it from FamaLamaJam
- compare received audio against the plugin metronome/interval
started: persists after the previous receive-alignment fix; previous real-world validation contradicted the code-only conclusion

## Eliminated

- hypothesis: the previous per-source target-boundary fix fully resolved remote misalignment
  evidence: new real-world tests show side-by-side half-beat offset and ReaNINJAM half-interval playback then permanent silence
  timestamp: 2026-03-20

## Evidence

- timestamp: 2026-03-20
  checked: prior real-world validation after commit 94916a5
  found: the previous automated tests passed, but real-world behavior still fails in at least two distinct ways
  implication: the current tests are missing a lower-level cross-client or decode-completeness failure mode

- timestamp: 2026-03-20
  checked: current processor receive model
  found: decoded inbound audio is treated as a complete interval PCM buffer, then resized to the active interval length before queuing
  implication: if decode output is shorter than a full interval for some senders, the processor will play early audio then silence for the padded remainder

- timestamp: 2026-03-20
  checked: JamTaba reference implementation
  found: JamTaba queues completed encoded intervals and decodes on demand during playback instead of assuming one eager decode call yields the final playable interval PCM
  implication: FamaLamaJam may still be architecturally mismatched to real NINJAM client behavior

- timestamp: 2026-03-20
  checked: local server-timing initialization in FamaLamaJam and startup behavior in JamTaba
  found: FamaLamaJam sets `samplesIntoInterval = 0` on first config receipt, while JamTaba explicitly delays transmit startup and treats interval preparation more conservatively
  implication: FamaLamaJam may still be too eager both in phase start and in interval readiness assumptions

- timestamp: 2026-03-20
  checked: MiniNinjamServer download forwarding versus JamTaba protocol shape
  found: the test harness marks every forwarded `DownloadIntervalWrite` as final instead of preserving the incoming last-part flag
  implication: existing automated transport/alignment tests are not exercising real multi-chunk interval delivery semantics

- timestamp: 2026-03-20
  checked: focused receive-path tests after correcting MiniNinjamServer chunk-final forwarding
  found: existing `[plugin_experimental_transport]` and `[plugin_remote_interval_alignment]` tests still passed
  implication: the automated coverage is still too weak to reproduce the real-world timing/audio-loss bug even with a more accurate test harness

- timestamp: 2026-03-20
  checked: processor/editor diagnostics plumbing
  found: the editor already had a diagnostics pane and the processor already had receive-diagnostic text; the rebuilt plugin now includes richer per-source fields for encoded bytes, copied samples, and queued boundary
  implication: the next high-signal step is a live repro with those counters visible instead of another blind receive-path change

- timestamp: 2026-03-20
  checked: live Receive Diagnostics during `ReaNINJAM -> FamaLamaJam` silence repro
  found: at `100 BPM / 16 BPI`, the room interval is `423360` samples at `44.1 kHz`, but the remote source showed `enc=21043`, `dec=88320@44100`, `copy=88320`, `active=423360`, `queued=4`, `queuedAt=5`, `next=6`, `last=1`, `drops=0`
  implication: the source is not being dropped as late; FamaLamaJam is accepting a decoded buffer that is only about 21% of the required interval length, then padding the rest with silence

- timestamp: 2026-03-20
  checked: current Vorbis decode path versus JamTaba decoder architecture
  found: FamaLamaJam uses a one-shot `juce::OggVorbisAudioFormat` reader over the full payload, while JamTaba uses a `vorbisfile`-based incremental decoder for received interval bytes
  implication: FamaLamaJam may be truncating chained or concatenated Ogg/Vorbis interval payloads that JamTaba/ReaNINJAM decode correctly

- timestamp: 2026-03-20
  checked: rebuilt plugin after replacing the receive decode path with a `vorbisfile` callback decoder
  found: `ReaNINJAM -> FamaLamaJam` still went silent when the remote ReaNINJAM channel was accidentally left in voice-chat mode, but interval-mode receive succeeded afterward
  implication: the earlier silence report was a mix of two issues: unsupported voice-mode semantics plus the now-fixed uncertainty around full-payload Vorbis decode

- timestamp: 2026-03-20
  checked: live Receive Diagnostics during the remaining `FamaLamaJam -> FamaLamaJam` slight timing offset
  found: the remote interval now decodes and copies at the full room length (`dec=423360`, `copy=423360`, `active=423360`) with `queued=0` and `drops=0`
  implication: the remaining bug is no longer a receive truncation or late-drop problem; it is more likely tied to local phase anchoring or transmit-side interval segmentation

- timestamp: 2026-03-20
  checked: JamTaba startup/timing code path
  found: JamTaba also initializes its local interval position to zero on start and does not reveal an obvious explicit “absolute server phase” packet in the code paths reviewed
  implication: we should avoid inventing a blind startup-offset fix and instead gather one more round of sender-side upload/phase evidence before changing the clock model

- timestamp: 2026-03-20
  checked: startup timing acquisition in FamaLamaJam versus JamTaba/ReaNINJAM
  found: FamaLamaJam sets `samplesIntoInterval = 0` immediately on first config receipt, while JamTaba separates initial BPM/BPI acquisition from conservative startup/transmit readiness and ReaNINJAM applies config in the audio thread before interval advancement, with remote playback driven by buffered decode-state queues
  implication: after the decoder fix, the remaining side-by-side offset is likely a separate phase-anchoring issue caused by treating first config receipt as proof of interval boundary zero

- timestamp: 2026-03-20
  checked: JamTaba transmit startup gate versus FamaLamaJam
  found: JamTaba does not enable transmit immediately on connect; `preparedForTransmit` stays false until `waitingIntervals >= TOTAL_PREPARED_INTERVALS`, where `TOTAL_PREPARED_INTERVALS = 2`
  implication: FamaLamaJam was more eager than the known-good client and could begin filling its first outbound interval against an untrusted local boundary too early

- timestamp: 2026-03-20
  checked: experimental FLJ patch to mirror JamTaba's transmit warmup
  found: FamaLamaJam now holds outbound interval capture for two interval rollovers after timing acquisition and exposes the remaining warmup count as `txWarm=` in Receive Diagnostics
  implication: the next manual retest can tell us whether the residual offset is mainly caused by premature transmit startup or whether a deeper boundary-acquisition problem remains

- timestamp: 2026-03-20
  checked: first retest after the download-begin phase-anchor experiment
  found: the loop could start in time, then jump out of time, and the first or second received interval could reset about halfway through; the diagnostic screenshot showed `anchor=dl`, `pos=2176`, and `upl=106254`
  implication: treating `DownloadIntervalBegin` arrival as a trustworthy boundary pulse was too aggressive and could yank the local phase mid-interval, while the divergent `upl` value exposed a separate transmit write-head drift

- timestamp: 2026-03-20
  checked: current upload-buffer placement logic
  found: FLJ had been maintaining a free-running `localUploadIntervalWritePosition_`, which could diverge from `samplesIntoInterval` after anchoring experiments or other corrections
  implication: the safer model is to derive outbound write position directly from the authoritative interval position at block start, so local transmit segmentation cannot drift away from the metronome clock

- timestamp: 2026-03-20
  checked: `DownloadIntervalBegin` handling in FamaLamaJam
  found: the transport was already seeing interval-begin messages for completed remote downloads, but the processor was not using them as any kind of timing pulse
  implication: config receipt was still the only thing steering local phase, which leaves cross-client startup offset uncorrected

- timestamp: 2026-03-20
  checked: boundary-pulse re-anchor patch
  found: FamaLamaJam now coalesces audio `DownloadIntervalBegin` messages into a transport boundary event and lets the processor do a one-time phase correction from that event when the local phase is clearly off
  implication: this gives the metronome and outbound interval segmentation a concrete server-adjacent anchor without touching the decoder path again

- timestamp: 2026-03-20
  checked: retest after the boundary-pulse re-anchor patch
  found: the patch made things worse: loops could start in time, then jump out, and early intervals could reset partway through; the diagnostic screenshot showed `anchor=dl`, `pos=2176`, and `upl=106254`
  implication: `DownloadIntervalBegin` arrival is not trustworthy enough to use as an active phase snap, and the real bug signal was the divergence between `upl` and the authoritative interval position

- timestamp: 2026-03-20
  checked: upload-position rewrite and re-test
  found: after removing the active re-anchor and deriving outbound writes directly from `samplesIntoInterval`, screenshots showed `upl` tracking `pos` closely on both instances while full intervals still decoded (`dec=423360`, `copy=423360`, `active=423360`, `queued=0`, `drops=0`)
  implication: the core send/receive interval path is likely corrected for normal usage, and the timing issue is provisionally fixed pending more single-instance soak testing

- timestamp: 2026-03-20
  checked: two-instance editor behavior with a real external NINJAM client joining
  found: audio continues and diagnostics keep updating, but the plugin UI can still freeze or assert when two FLJ editors are open in parallel and a real external client joins
  implication: there is a separate editor-only multi-instance instability that should be tracked independently from the core timing/audio work

## Resolution

root_cause:
fix:
- replaced the one-shot JUCE `OggVorbisAudioFormat` receive decode path with a `vorbisfile` callback decoder that reads the full in-memory payload through EOF, matching JamTaba's decoder model more closely
- kept the diagnostics panel in place so the next repro can confirm whether decoded sample counts now match the room interval length
- mirrored JamTaba's conservative startup by holding transmit for a short warmup window before local outbound interval capture begins
- removed the experimental download-begin phase snap after it proved unstable in practice
- changed outbound interval capture so the write position is derived from `samplesIntoInterval` on each block rather than from a free-running write head
verification:
- rebuilt VST3 successfully after the decoder change
- added a concatenated-payload unit regression in `tests/unit/ogg_vorbis_codec_tests.cpp`, but the local `famalamajam_tests` target is still blocked by the existing Windows `NMake` dependency-path issue during full rebuild
- rebuilt the VST3 successfully after the transmit warmup and upload-position changes
- manual screenshots after the final upload-position fix showed healthy full-interval decode/copy/active values and `upl` tracking `pos` closely again
- `famalamajam_tests` rebuild is still blocked by the existing Windows `NMake` `compiler_depend.make` path-length failure, so the current confidence comes primarily from real-world plugin validation rather than a fresh full suite run
files_changed:
- tests/integration/support/MiniNinjamServer.h
- tests/unit/ogg_vorbis_codec_tests.cpp
- src/audio/OggVorbisCodec.cpp
- src/net/FramedSocketTransport.h
- src/net/FramedSocketTransport.cpp
- src/plugin/FamaLamaJamAudioProcessor.h
- src/plugin/FamaLamaJamAudioProcessor.cpp
- src/plugin/FamaLamaJamAudioProcessorEditor.h
- src/plugin/FamaLamaJamAudioProcessorEditor.cpp

## Notes

- ReaNINJAM voice-chat mode is not yet supported as an intervalic remote source. The recent silence repro matched that unsupported mode and should not be confused with the separate intervalic timing bug.
- The receive diagnostics now also expose sender-side upload fill progress (`upl=...`) and the last locally encoded payload size (`encLast=...`) to help isolate whether the residual FLJ-to-FLJ offset is being introduced on the transmit side.
- The `anchor=dl` experiment should be treated as a discarded debugging branch, not the preferred solution.
- A separate editor-only issue remains: with two FLJ instances open in parallel, a real external NINJAM client joining can freeze/assert the UI even while audio and diagnostics continue. That issue is currently considered non-blocking because the normal intended workflow is a single FLJ instance per DAW session.
