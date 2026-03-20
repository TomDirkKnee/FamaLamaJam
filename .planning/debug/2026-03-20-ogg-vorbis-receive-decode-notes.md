---
status: active_reference
created: 2026-03-20T01:45:00Z
updated: 2026-03-20T01:45:00Z
---

## Purpose

This note isolates the Ogg/Vorbis receive-path changes from the broader interval-alignment investigation so the codec work is easy to find and revert independently if needed.

## Problem

During early real-world receive debugging, `ReaNINJAM -> FamaLamaJam` produced cases where the plugin appeared to play only part of a remote interval and then pad the rest with silence.

The key diagnostic capture showed:

- room interval length at `100 BPM / 16 BPI / 44.1 kHz`: `423360` samples
- received source decode length: `88320` samples
- copied samples: `88320`
- active interval buffer length: `423360`

That established that the receive path was sometimes accepting a decoded buffer much shorter than the room interval.

## Original Implementation

File:
- [OggVorbisCodec.cpp](/C:/Users/Dirk/Documents/source/FamaLamaJam/src/audio/OggVorbisCodec.cpp)

Previous decode behavior:
- used `juce::OggVorbisAudioFormat`
- created a one-shot `AudioFormatReader` over the whole in-memory payload
- read `reader->lengthInSamples` once into a single output buffer

Concern:
- this was convenient, but it did not mirror JamTaba's incremental `vorbisfile` decoder model
- it was a plausible failure point for chained/concatenated or otherwise non-trivial received Ogg/Vorbis payloads

## Reference Behavior

JamTaba references:
- [VorbisDecoder.cpp](/C:/Users/Dirk/Documents/source/reference/JamTaba/src/Common/audio/vorbis/VorbisDecoder.cpp)
- [NinjamTrackNode.cpp](/C:/Users/Dirk/Documents/source/reference/JamTaba/src/Common/audio/NinjamTrackNode.cpp)

JamTaba uses:
- a callback-driven `vorbisfile` decoder
- incremental consumption of the encoded interval bytes
- repeated decode calls until the payload is exhausted

## Change Made

File changed:
- [OggVorbisCodec.cpp](/C:/Users/Dirk/Documents/source/FamaLamaJam/src/audio/OggVorbisCodec.cpp)

New receive decode behavior:
- uses `vorbisfile` callbacks directly over the in-memory payload
- reads decoded floats in chunks via `ov_read_float`
- appends decoded samples until EOF
- validates channel-count and sample-rate consistency across sections

This keeps encode unchanged and only replaces the receive decode implementation.

## Related Test Work

Regression added:
- [ogg_vorbis_codec_tests.cpp](/C:/Users/Dirk/Documents/source/FamaLamaJam/tests/unit/ogg_vorbis_codec_tests.cpp)

Added case:
- concatenated payload decode should produce the combined sample count rather than only the first segment

Note:
- full `famalamajam_tests` rebuild is still hindered on this machine by the existing Windows `NMake` dependency-path issue, so this test addition is currently documented even where local full-suite execution is awkward

## Outcome

The decoder change is worth keeping:
- it makes the receive path closer to JamTaba
- it removes a suspicious one-shot decode assumption
- it gives us a cleaner codec baseline going forward

But it did **not** fully explain the remaining slight FLJ-to-FLJ interval timing offset.

Important distinction:
- the later ReaNINJAM silence repro was strongly tied to ReaNINJAM being left in voice-chat mode
- the remaining intervalic offset bug appears separate and is now being tracked in:
  - [2026-03-20-remote-interval-misalignment.md](/C:/Users/Dirk/Documents/source/FamaLamaJam/.planning/debug/2026-03-20-remote-interval-misalignment.md)

## Rollback Scope

If needed, this codec change can be reasoned about separately from:
- receive-boundary scheduling experiments
- UI diagnostics work
- voice-mode compatibility work

Primary rollback target:
- [OggVorbisCodec.cpp](/C:/Users/Dirk/Documents/source/FamaLamaJam/src/audio/OggVorbisCodec.cpp)
