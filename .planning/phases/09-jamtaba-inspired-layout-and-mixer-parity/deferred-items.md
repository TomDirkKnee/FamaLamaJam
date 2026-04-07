# Deferred Items

## 2026-04-07

- `ctest --test-dir build-vs-2026 --output-on-failure -C Debug` still reports an unrelated failure in `ogg vorbis codec decodes concatenated payloads as full audio` (`tests/unit/ogg_vorbis_codec_tests.cpp`). This plan did not touch codec code.
- `ctest --test-dir build-vs-2026 --output-on-failure -C Debug` still reports an unrelated failure in `plugin stem capture excludes local voice mode and resumes interval export with a fresh file` (`tests/integration/plugin_stem_capture_tests.cpp`). This plan did not touch stem-capture code.
