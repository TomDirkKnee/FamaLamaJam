# Deferred Items

## Out-of-scope full-suite failures seen during 08.3.4.1-03

These failures came from the required `ctest` sweep on 2026-04-05. They were not part of the focused `ROUTE-02` or `ROUTE-03` fixed-bus gate and were not changed in this plan.

- `phase::ogg vorbis codec decodes concatenated payloads as full audio`
- `phase::plugin experimental transport performs ninjam auth and codec roundtrip`
- `phase::plugin experimental transport keeps large interval uploads connected`
- `phase::plugin mixer controls applies remote gain pan and mute per user-plus-channel strip`
- `phase::plugin mixer controls master output scales both mixed playback and metronome-only output`
- `phase::plugin transmit controls ui keeps transmit off the transport row and on the local strip`
- `phase::plugin remote interval alignment drops a late interval and realigns the next arrival to the next boundary`
