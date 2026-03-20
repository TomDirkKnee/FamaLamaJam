## Remote Interval Misalignment

Date: 2026-03-20
Status: fixed in code, awaiting real-world two-instance validation

### Symptom

- Remote decoded loops could start in sync with the metronome, then a later glitch would push them out of interval alignment.
- Once offset, the stream could stay musically wrong instead of naturally re-aligning on the next interval.

### Root Cause

- Receive-side remote intervals were still vulnerable to arrival-time-driven misassignment.
- Completed decoded remote intervals were not tracked with a stable per-source expected boundary.
- After a late arrival, the next completed interval could effectively inherit the wrong musical slot and keep the source one interval out of phase.

### JamTaba Comparison

- JamTaba treats completed remote intervals as queued interval objects and swaps them at interval boundaries.
- This fix moves FamaLamaJam closer to that model by using explicit per-source boundary sequencing and dropping intervals that miss their intended boundary.

Reference files checked:
- `C:\Users\Dirk\Documents\source\reference\JamTaba\src\Common\audio\NinjamTrackNode.cpp`
- `C:\Users\Dirk\Documents\source\reference\JamTaba\src\Common\NinjamController.cpp`
- `C:\Users\Dirk\Documents\source\reference\JamTaba\src\Common\ninjam\client\Service.cpp`

### Code Changes

- `src/plugin/FamaLamaJamAudioProcessor.cpp`
- `src/plugin/FamaLamaJamAudioProcessor.h`
- `tests/integration/plugin_remote_interval_alignment_tests.cpp`

### Fix Summary

- Added per-source `nextRemoteTargetBoundaryBySource_`.
- Added per-source `lastRemoteActivationBoundaryBySource_` for targeted regression inspection.
- Removed the old pending-interval stitching model from the receive path.
- Queued completed decoded intervals with explicit target boundaries.
- Dropped late completed intervals instead of allowing them to shift future alignment.

### Verification

Targeted tests:
- `famalamajam_tests.exe "[plugin_remote_interval_alignment]"`
- Result: 5 cases, 51 assertions, all passed

Broader neighboring coverage:
- `famalamajam_tests.exe "[plugin_experimental_transport],[plugin_streaming_runtime_compat],[plugin_remote_interval_alignment]"`
- Result: 11 cases, 112 assertions, all passed

Plugin build:
- `build-vs/famalamajam_plugin_artefacts/Debug/VST3/FamaLamaJam.vst3`

### Next Manual Check

- Run two local instances side by side.
- Let one loop for long enough to reproduce the prior glitch window.
- Confirm a delayed/glitchy receive event no longer causes permanent offset from the interval/metronome.
