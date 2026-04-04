# Phase 08.3.3 Stem Capture Matrix

- `P8.3.3-REC-01`: `pass` - Record stems cannot be armed until a folder is chosen, and the inline error is clear.
- `P8.3.3-REC-02`: `pass` - Local interval stem starts on the next interval boundary and drops into the DAW in time without manual nudging.
- `P8.3.3-REC-03`: `pass` - Local and remote voice mode create no stem files at all, and interval export resumes only after returning to interval mode.
- `P8.3.3-REC-04`: `pass` - Interval-only naming and native source channel format look correct with voice files excluded.
- `P8.3.3-REC-05`: `pass` - `New stem folder` starts a fresh session folder without reconnecting and preserves the earlier run's files.
- `P8.3.3-REC-06`: `pass` - Turning Record stems off closes files on the next interval boundary rather than truncating mid-interval.
- `P8.3.3-REC-07`: `pass` - Late-joining remote peers create files only from their first actual audio, with no silence backfill before that point.
