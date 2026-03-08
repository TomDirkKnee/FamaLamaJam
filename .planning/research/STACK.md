# Stack Research

**Domain:** Real-time networked music collaboration (NINJAM) as a DAW plug-in
**Researched:** 2026-03-08
**Confidence:** MEDIUM-HIGH

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| JUCE | 8.0.12 | Cross-platform audio plug-in/application framework | Best practical fit for a VST3-first product: mature host integration layer, strong audio/MIDI abstractions, and direct support for Windows-first shipping with later macOS/Linux expansion. Latest stable tag is 8.0.12. |
| Steinberg VST3 SDK | v3.8.0_build_66 | VST3 API and validation/tooling baseline | Required protocol/API layer for production VST3 behavior and compatibility validation. Current top tag is 3.8.0 build 66 (2025-10-20), and SDK includes validator/test-host workflows. |
| C++ Toolchain (Windows-first) | MSVC 2022 + C++17 (minimum), C++20 (project default) | Compile/runtime baseline for plugin + networking + DSP | JUCE 8 requires C++17 minimum; VST3 SDK system requirements list MSVC 2022 for Windows 11. Using C++20 project-wide improves maintainability while staying ABI-safe with C++17-capable dependencies. |
| NINJAM Core Source | `justinfrankel/ninjam` main pinned by commit hash | Protocol behavior reference + interoperability target | There is no modern release cadence for a standalone SDK; pinning a known-good commit gives reproducible behavior. Treat Cockos server/client behavior as interoperability truth and lock protocol tests against it. |
| Build System | CMake 3.22+ (recommend 3.29.x on dev machines) | Single build orchestration for JUCE + VST3 + tests | JUCE 8 requires CMake >= 3.22. CMake keeps Windows-first builds clean and reduces friction for later macOS/Linux CI bring-up. |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| libogg | 1.3.5 | Ogg container support used in classic NINJAM ecosystem | Use when implementing or validating compatibility with legacy NINJAM packet/session handling paths that expect Ogg framing. |
| libvorbis | 1.3.7 | Vorbis encode/decode used by NINJAM-era clients | Use for protocol-compatible streaming/recording paths where exact legacy behavior matters more than codec modernization. |
| ASIO SDK integration (through JUCE audio device layer where applicable) | 2.3.x family | Low-latency Windows audio I/O in rehearsal scenarios | Use in standalone/debug tools and host-interoperability testing where you must characterize low-latency monitoring behavior beyond DAW-managed playback paths. |
| Catch2 | 3.8.x | Unit/integration test framework for protocol, timing, and state machines | Use for deterministic NINJAM timing math, interval transitions, ring-buffer behavior, and reconnect logic testing. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| Visual Studio 2022 (17.x) | Primary Windows IDE/compiler/debugger | Align with VST3 SDK Windows requirements; keep one canonical compiler in v1 to reduce host-specific drift. |
| Steinberg `validator` (from VST3 SDK package) | Verify VST3 compliance and metadata | Run in CI and pre-release gates; fail builds on class/category/factory or state-restore regressions. |
| CTest + GitHub Actions (Windows runner first) | Automated build/test pipeline | Start with Windows-only CI for fast iteration; add macOS then Linux after protocol/audio-core stabilizes. |
| Wireshark + custom protocol logging | NINJAM transport inspection and troubleshooting | Essential for diagnosing interval-sync and reconnect behavior under real network jitter. |

## Installation

```bash
# Core (pin versions in your repo tooling)
git submodule add https://github.com/juce-framework/JUCE external/JUCE
# checkout juce tag: 8.0.12

git submodule add https://github.com/steinbergmedia/vst3sdk external/vst3sdk
# checkout vst3 tag: v3.8.0_build_66

# Optional protocol-compat refs
git submodule add https://github.com/justinfrankel/ninjam external/ninjam-ref
# pin to a tested commit hash

# Configure (Windows-first)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config RelWithDebInfo
ctest --test-dir build --output-on-failure
```

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| JUCE 8 + CMake | iPlug2 | Choose iPlug2 only if you need a smaller framework surface and are willing to build more host/platform glue yourself. |
| VST3-first deliverable | CLAP-first | Choose CLAP-first only if your immediate commercial target is CLAP-native host ecosystems and VST3 is secondary. |
| Legacy-compatible Vorbis path | Opus-based custom transport | Choose Opus only for a non-legacy protocol fork where lower bitrate/latency tradeoffs outweigh compatibility with existing NINJAM behavior. |
| Windows-first CI/release | Simultaneous tri-platform v1 | Choose simultaneous tri-platform only if team capacity can absorb 2-3x host/driver QA expansion without delaying rehearsal reliability. |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| VST2 target for new work | Deprecated ecosystem and compatibility/legal overhead; also called out in legacy ReaNINJAM notes as needing old SDK paths | VST3 (`v3.8.0_build_66`) |
| Floating dependency heads in core audio/protocol code | Reproducibility risk in timing-sensitive plugin/network behavior | Pin JUCE/VST3/ninjam-ref to explicit tags/commits |
| Cross-platform UI/framework divergence in v1 | Splits effort away from protocol reliability and Ableton workflow quality | Single JUCE UI codepath; phase platform expansion after Windows rehearsal validation |

## Stack Patterns by Variant

**If shipping v1 (Windows + Ableton-first):**
- Use JUCE 8.0.12 + VST3 SDK v3.8.0_build_66 + MSVC 2022
- Because it minimizes integration risk and aligns directly with the project's validated constraints

**If expanding to macOS after v1 stabilization:**
- Keep JUCE/VST3 versions pinned, add macOS CI and host matrix (Live + Reaper)
- Because protocol/audio parity should be proven before introducing platform/host variability

**If adding standalone client mode later:**
- Reuse JUCE audio engine and Ninjam core modules; isolate plugin wrapper layer
- Because plugin and standalone should share timing/protocol logic with only I/O/shell differences

## Version Compatibility

| Package A | Compatible With | Notes |
|-----------|-----------------|-------|
| `JUCE@8.0.12` | `CMake>=3.22`, `C++17+` | JUCE README explicitly states CMake 3.22+ and C++17 minimum. |
| `vst3sdk@v3.8.0_build_66` | `MSVC 2022` (Win11 baseline) | SDK README system requirements call out Windows/MSVC combinations. |
| `ninjam-ref@pinned-commit` | `libogg 1.3.5`, `libvorbis 1.3.7` | Use compatibility-focused codec path; validate with real server sessions. |
| `Catch2@3.8.x` | `CMake 3.22+` | Fits the same CMake-first build graph for plugin + protocol tests. |

## Sources

- https://github.com/juce-framework/JUCE/releases - verified latest JUCE stable tag (`8.0.12`)
- https://raw.githubusercontent.com/juce-framework/JUCE/master/README.md - verified JUCE CMake and C++ minimum requirements
- https://github.com/steinbergmedia/vst3sdk/tags - verified current VST3 SDK tag (`v3.8.0_build_66`)
- https://raw.githubusercontent.com/steinbergmedia/vst3sdk/master/README.md - verified platform/compiler requirements and validator availability
- https://www.cockos.com/ninjam/ - verified official NINJAM source links and legacy ecosystem context
- https://github.com/justinfrankel/ninjam - verified active reference repository status and legacy client notes

---
*Stack research for: JUCE 8 + VST3 + NINJAM client (Windows-first, cross-platform next)*
*Researched: 2026-03-08*
