# Deferred Items

## 2026-04-08

- `ctest --test-dir build-vs-2026 --output-on-failure -C Debug` still fails outside Phase 09.1 plan scope:
  - `phase::ogg vorbis codec decodes concatenated payloads as full audio`
  - `phase::plugin stem capture excludes local voice mode and resumes interval export with a fresh file`
- These failures do not involve the strip-layout files touched by `09.1-03` and were not auto-fixed here.
