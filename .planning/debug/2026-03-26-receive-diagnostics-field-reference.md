---
status: reference
created: 2026-03-26T00:00:00Z
updated: 2026-03-26T00:00:00Z
---

## Receive Diagnostics Field Reference

This note explains the fields shown in the plugin's `Recv Debug` panel.

## Global Fields

- `interval=N`
  - current authoritative room interval index inside FLJ

- `pos=A/B`
  - current sample position inside the interval
  - `A` = current sample offset
  - `B` = total samples in the interval

- `bpm=N`
  - current room BPM from server timing

- `bpi=N`
  - current beats per interval from server timing

- `sr=N`
  - current plugin sample rate

- `anchor=cfg|dl`
  - how the interval phase was anchored
  - `cfg` = config/timing based
  - `dl` = download-begin based
  - current preferred normal state is `cfg`

- `txWarm=N`
  - how many transmit warmup intervals remain before local upload becomes active

- `upl=A/B`
  - local outbound upload fill position
  - should normally track `pos` closely

- `encLast=N`
  - last locally encoded payload size in bytes

- `rx=N`
  - number of received framed transport messages

- `tx=N`
  - number of sent framed transport messages

## Per-Source Fields

Each user/channel block refers to one remote source, usually `user@host - channel`.

- `mode=interval|voice|flagsN`
  - remote channel mode from channel flags
  - `interval` = normal NINJAM interval mode
  - `voice` = voice-chat mode
  - `flagsN` = some other raw flag combination

- `known=yes|no`
  - whether FLJ currently believes this source is still present in the room

- `sub=yes|no`
  - whether FLJ believes it is subscribed to this exact channel bit

- `mask=N`
  - current subscription bitmask for that remote user
  - useful for multi-channel users

- `strip=interval|idle|voice|hidden`
  - current processor-side strip state
  - `interval` = source currently has playable interval audio
  - `idle` = source is known in the room but no interval audio is currently active/queued
  - `voice` = unsupported voice-mode source
  - `hidden` = strip not currently visible

- `enc=N`
  - size in bytes of the last completed inbound encoded payload for this source

- `dec=N@RATE`
  - decoded sample count and decoded sample rate
  - example: `dec=542769@48000`

- `copy=N`
  - number of decoded samples copied into the normalized interval buffer

- `active=N`
  - current active interval buffer size in samples for this source
  - `0` means nothing is actively playing from this source right now

- `queued=N`
  - number of queued future intervals waiting for activation

- `queuedAt=N`
  - boundary index assigned to the most recently queued interval

- `in=N`
  - last interval index where a completed inbound frame arrived for this source

- `seenUser=N`
  - last interval index where presence/user-info for this source was refreshed

- `seenAudio=N`
  - last interval index where this source had active or queued interval audio

- `next=N`
  - next target boundary FLJ expects to use for this source

- `last=N`
  - last boundary where an interval from this source was successfully activated

- `drop=A->B`
  - last late-drop transition
  - `A` = target boundary FLJ was still trying to use
  - `B` = earliest playable boundary by the time the drop was detected
  - example: `drop=19->20` means the source was one interval late

- `drops=N`
  - total count of late-drop events recorded for this source

## Quick Reading Guide

- `known=yes`, `strip=idle`, `sub=yes`, old `in`, old `seenAudio`
  - source is still present, but inbound audio may have stopped arriving

- `known=yes`, `sub=yes`, `in` advancing, `drop` advancing, `drops` increasing
  - audio is arriving, but FLJ is dropping it as late

- `known=yes`, `sub=no`
  - likely channel subscription problem

- one channel from a multi-channel user is green and another is red
  - likely not a whole-user disconnect
  - more likely per-channel subscription or per-channel late-drop behavior
