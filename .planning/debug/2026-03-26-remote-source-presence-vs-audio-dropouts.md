---
status: provisional_fix_observed
trigger: "Remote users remain known in diagnostics but lose interval audio or go idle unexpectedly"
created: 2026-03-26T00:00:00Z
updated: 2026-03-26T00:00:00Z
---

## Current Focus

hypothesis: the disappearing-strip symptom was masking a per-source receive problem; some sources stay present in the room, but their interval payloads stop activating because they are arriving late, not being subscribed correctly, or getting dropped per channel
test: keep remote strips visible while the user is still known in the room, add per-source diagnostics for presence, subscription, inbound timing, and drop boundaries, then capture a real-world repro from a failing room
expecting: the next screenshot should distinguish between `sub=no`, stale/no `in=`, or advancing `dropAt=` for the affected channel
next_action: treat the late-requeue change as a promising mitigation and move forward unless the dropout pattern resurfaces in real rooms

## Symptoms

expected: if a remote user is still in the room, their strip should remain visible and either show active interval audio or an honest idle state while waiting for audio
actual:
- some remote channel strips appear briefly and then disappear
- diagnostics can still show the user/source even when the strip is gone or idle
- some users appear in diagnostics but have no sound and no queued/active interval audio
- split-channel failures can happen where one channel from the same remote user keeps working and another goes idle
errors: none reported in-plugin for the failing sources
reproduction:
- join a live public room with several users and let it run
- watch for a peer that was previously audible to become idle
- watch for a multi-channel peer where one channel remains green and another turns idle/red

## Evidence

- timestamp: 2026-03-26
  checked: processor strip visibility logic in `src/plugin/FamaLamaJamAudioProcessor.cpp`
  found: remote strips were previously considered "active" only while they had an active or queued interval buffer; otherwise they were hidden after `kInactiveStripRetentionIntervals = 2`
  implication: a remote user could still be present in the room while losing their strip due to lack of recent audio activity

- timestamp: 2026-03-26
  checked: live screenshot with `NJoy` idle
  found: diagnostics showed `known=yes`, `strip=idle`, `queued=0`, `active=0`, `seenUser=20`, `seenAudio=20`, `last=16`, `drops=3`
  implication: the real failure is not strip rendering; the source stayed known but had no playable interval queued/active and had already accumulated late drops

- timestamp: 2026-03-26
  checked: live screenshot with split-channel `Kosmischer` failure
  found: one channel remained active while another channel from the same remote user went idle
  implication: the problem is unlikely to be just user-level presence or whole-user network loss; it may be channel-specific subscription or per-channel late-drop behavior

- timestamp: 2026-03-26
  checked: live screenshot with `worpheausmata` idle while `Javito` stayed stable
  found: the failing source showed `known=yes`, `sub=yes`, full decode/copy, `drop=30->31`, and `drops=2`
  implication: the strongest remaining failure mode was a one-interval late arrival causing FLJ to drop the source rather than recover it

- timestamp: 2026-03-26
  checked: live testing after changing late-drop handling to re-queue onto the next playable boundary
  found: no drops were observed in the current session
  implication: the recovery change appears to mitigate the bug in practice, but it should still be treated as provisional until more long-room testing accumulates

## Changes Made

- kept remote strips visible while the source is still known to be present in the room, even when no interval audio is currently queued
- added a clearer idle status for known-but-not-currently-receiving sources: `In room, waiting for interval audio`
- added a blank line between source blocks in the diagnostics view
- added simple strip coloring to make remote state easier to scan:
  - green: active interval audio
  - orange: unsupported voice-mode source
  - red: known in room but currently idle / no interval audio
- changed late-drop recovery so that when a decoded interval arrives after its original target boundary, FLJ still counts the late event but re-queues the current decoded payload onto the next playable interval boundary instead of discarding it outright

## Current Conclusion

- the disappearing-strip behavior was mostly a symptom of source audio going idle after a late-drop
- the underlying issue looked more like brittle late-arrival recovery than UI instability or missing subscription
- the latest recovery change appears to help, and the current recommendation is to move on unless the pattern reappears

## New Diagnostics Fields

- `known=yes/no`
  - whether FLJ currently believes the source is still present in the room

- `strip=interval|idle|voice|hidden`
  - processor-side strip state, not just UI state

- `seenUser=N`
  - last interval index where presence for this source was refreshed

- `seenAudio=N`
  - last interval index where this source had active or queued interval audio

- `sub=yes/no`
  - whether FLJ believes it has subscribed to the specific remote channel bit for this source

- `mask=N`
  - current subscription bitmask held for that remote user

- `in=N`
  - last interval index where a completed inbound frame arrived for that source

- `dropAt=N`
  - boundary where the most recent late-drop decision happened for that source

## What To Look For Next

- `sub=no`
  - points toward a channel subscription issue

- `sub=yes` but `in` stays old
  - points toward inbound frames for that channel stopping before decode/queue

- `sub=yes` and `in` keeps advancing, but `dropAt` also advances and `drops` climbs
  - points toward per-channel late-arrival / boundary-assignment failure

- `known=yes` with one channel green and another red for the same user
  - especially important, because it argues against simple whole-user disconnect/network loss

- if the bug returns after the late-requeue change, capture another screenshot and compare:
  - whether `drop=A->B` is still usually one boundary late
  - whether `sub=yes` still holds
  - whether the source now recovers on the next interval or remains stranded

## Verification

- rebuilt plugin successfully:
  - `build-vs-2026/famalamajam_plugin_artefacts/Debug/VST3/FamaLamaJam.vst3`
- no automated test pass was run for this diagnostics/investigation update

## Files Changed

- `src/plugin/FamaLamaJamAudioProcessor.h`
- `src/plugin/FamaLamaJamAudioProcessor.cpp`
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `src/net/FramedSocketTransport.h`
- `src/net/FramedSocketTransport.cpp`
