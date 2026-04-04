# Phase 08.3.4 Host Multi-I/O Routing Matrix

**Phase:** 08.3.4-host-multi-io-routing-research  
**Plan:** 08.3.4-01  
**Host:** Ableton Live on Windows  
**Plugin artifact:** `build-vs-2026/famalamajam_plugin_artefacts/Debug/VST3/FamaLamaJam.vst3` once the fixed-bus proof build is ready

Use this matrix for the real Ableton proof. Record what Live actually exposes and how usable it feels, not what JUCE or VST3 docs imply should be possible.

| ID | Scenario | Steps | Expected host/plugin outcome | Result | Notes |
|----|----------|-------|------------------------------|--------|-------|
| P8.3.4-ROUTE-01 | Sidechain-style extra-input exposure | Load the proof build, enable the extra FLJ input path, and try to route a second Ableton track into `Local Send 2` through the normal sidechain or aux-input workflow. | Ableton exposes the extra stereo input in a discoverable way, and the second track can feed `Local Send 2` without replacing the main plugin input. | pending | Record the exact Live path used, or the exact UI blocker if it cannot be found. |
| P8.3.4-ROUTE-02 | Routed extra-output exposure | Route exactly one remote source to `Remote Out 1`, then try to pull that output from another Ableton track using the normal `Audio From` or plug-in routing chooser. | Ableton exposes `Remote Out 1` as a practical routed output pair while FLJ main output remains available. | pending | Record the exact chooser path, or the exact blocker if the output pair is hidden or unavailable. |
| P8.3.4-ROUTE-03 | Signal isolation across main, aux-input, and routed-output paths | Feed distinct audio into the main input and `Local Send 2`, then route one selected remote source to `Remote Out 1` while leaving at least one other remote source on the main FLJ output. | Main monitor audio stays on the main path, aux-input audio stays distinct from the main monitor path, the selected remote source lands only on `Remote Out 1`, and unrouted sources stay on FLJ main output. | pending | Note any bleed, duplicated audio, missing path, or host-specific caveat. |
| P8.3.4-ROUTE-04 | Ergonomics verdict and fallback decision | After attempting rows 01-03, judge whether a normal Ableton user could reasonably discover and repeat the workflow without plugin-specific DAW tricks. | Verdict is one of: `approve fixed-bus follow-on`, `fallback to per-user output routing`, or `no follow-on implementation phase`. | pending | This row must name the exact blocker if the verdict is not approval. |

## Fallback Rule

- If `P8.3.4-ROUTE-01` fails because Ableton does not expose the extra input in a normal sidechain or aux-input workflow, record the exact blocker and recommend `no follow-on implementation phase` rather than expanding the fixed-bus input scope by assumption.
- If `P8.3.4-ROUTE-02` or `P8.3.4-ROUTE-03` fail because routed output pairs are too hidden, too fragile, or not isolated enough for per-channel use, record the exact blocker and recommend `fallback to per-user output routing` instead of broader per-channel routing.
- If `P8.3.4-ROUTE-04` concludes the overall Ableton workflow is too awkward or too hidden for normal use, record that ergonomics blocker and recommend either `fallback to per-user output routing` or `no follow-on implementation phase`. Do not widen the fixed-bus scope or assume a later routing matrix will rescue poor host ergonomics.

## Session Notes

- Ableton version:
- Windows version:
- Plugin build timestamp:
- Room or test server used:
- Reporter:
