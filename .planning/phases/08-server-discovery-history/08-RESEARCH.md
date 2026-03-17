# Phase 8: Server Discovery & History - Research

**Researched:** 2026-03-17
**Domain:** JUCE plugin server discovery, persisted recent-server history, and compact single-page room-entry UX
**Confidence:** MEDIUM

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
## Implementation Decisions

### Connection picker shape
- Phase 8 should add a server picker that assists the existing connection fields instead of replacing them.
- Manual `Host` and `Port` fields should remain visible in the current single-page UI so power users can still type raw destinations directly.
- Public servers and remembered private servers should appear in one combined picker rather than two separate workflows.
- Remembered/private entries should appear before public entries in that combined list because they are the most likely repeat destinations.
- Selecting an entry should populate editable host/port fields rather than locking the selection or connecting immediately.

### Entry presentation
- Picker rows should be richer than host-only labels.
- Each entry should show server-identifying information plus host/port, and should include connected-user count when that data is available from the public list source.
- The Phase 8 picker should stay compact enough for the current UI; it should not become a full room-browser panel.

### Remembered-server history policy
- Only successful connections should be added to remembered history.
- Remembered history should stay short, around 10-12 entries, following a lightweight recent-history model similar to classic NINJAM clients.
- History management should stay passive in Phase 8: no per-entry editing/removal UI and no separate management panel yet.
- Remembered entries should restore host and port only; username and other session defaults remain controlled by the normal session fields.

### Public-list refresh and failure behavior
- The public server list should auto-refresh when the connection area is opened, with an explicit manual refresh action available as well.
- If public-list fetching fails, the UI should show a clear recoverable error while leaving manual host/port entry and remembered history fully usable.
- If a cached public list exists and refresh fails, the cached list should remain visible but be marked as stale.
- Public-list status should be visible through a small lightweight status line rather than a heavy diagnostics panel.

### Claude's Discretion
- Exact wording for refresh, stale, and fetch-failure copy.
- Exact visual treatment for source badges or section labels inside the combined picker.
- Exact compact-row design, as long as the picker remains usable in the current single-page layout and does not preempt the larger Phase 9 redesign.

### Deferred Ideas (OUT OF SCOPE)
## Deferred Ideas

- Larger JamTaba-inspired room-browser layout and broader horizontal UI rework belong to Phase 9.
- Per-server profile behavior such as remembering usernames or full session presets is out of scope for Phase 8.
- Explicit history management UI such as removing individual remembered servers or a clear-history action is deferred unless planning later proves a tiny control is essential.
- Advanced NINJAM discovery/listen parity work remains part of later research phases.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DISC-01 | User can choose from a public Ninjam server list inside the plugin. | Add a background discovery client using JUCE HTTP APIs, parse the upstream `SERVER ... END` text format into a processor-owned snapshot, expose a compact combined picker plus refresh/status UI, and keep stale/error states explicit without blocking manual entry. |
| DISC-02 | Previously used private servers are remembered and shown alongside public entries. | Persist a small deduped LRU history keyed by canonical host+port, add entries only after successful authentication/connect, merge remembered entries ahead of public entries in one picker model, and extend state roundtrip plus UI tests for ordering and restore behavior. |
</phase_requirements>

## Summary

Phase 8 does not need new third-party dependencies. The codebase already has the right building blocks: JUCE 8.0.12 for UI and HTTP-capable URL streams, processor-owned UI snapshots, editor polling, and background thread patterns in `FramedSocketTransport` and `CodecStreamBridge`. The safest plan is to follow those same patterns rather than invent a second UI-local state machine.

The critical planning decision is to keep public discovery runtime-only and separate from the active `SessionSettings` draft. The picker is an assistive input: it fills the editable `Host` and `Port` fields, but those fields remain the canonical connection target. Remembered history should be persisted as a small optional plugin-state child keyed by normalized host+port, not mixed into the active settings validation path. Public-list cache data should stay runtime-only unless later validation proves cross-session caching is necessary.

As of 2026-03-17, upstream ReaNINJAM source still fetches `http://ninjam.com/serverlist.php`, while a recent indexed public list is visible at `https://autosong.ninjam.com/serverlist.php`. Treat the discovery URL as configuration, not protocol truth. The current indexed list still uses the classic `SERVER "host:port" "info" "users"` lines ending in `END`, but not every row is pure `BPM/BPI`; some rows are lobby-style descriptive text. Parser and UI planning must account for that variability.

**Primary recommendation:** Add a processor-owned `ServerDiscoveryUiState` fed by a dedicated JUCE worker thread, persist only successful remembered host+port history as a small LRU child in plugin state, and expose one compact remembered-first picker that populates the existing editable `Host` and `Port` fields without replacing them.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 8.0.12 | Plugin UI, background threads, timers, URL/Web input streams, persistence glue | Already pinned in `cmake/dependencies.cmake` and used across the plugin/editor/runtime stack |
| Catch2 | v3.8.1 | Unit and integration tests | Already pinned and wired through `catch_discover_tests()` |
| Existing processor/editor getter pattern | Workspace current | Processor-owned UI snapshot with editor polling and command callbacks | This is already how transport, sync assist, room UI, and mixer state are exposed |
| NINJAM public list text format | Current upstream format verified from source + recent indexed list | Public discovery payload format (`SERVER ... END`) | This is the authoritative interoperability surface for DISC-01 |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `juce::URL` + `juce::URL::InputStreamOptions` | JUCE 8.0.12 | HTTP GET with timeout, redirect, status-code, and header capture | Use in the real discovery worker implementation |
| `juce::Thread` | JUCE 8.0.12 | Background fetch loop or one-shot worker execution | Use because JUCE `Timer` callbacks run on the message thread and are not suitable for blocking I/O |
| `juce::ComboBox` | JUCE 8.0.12 | Compact combined picker for remembered + public entries | Use if one-line formatted rows remain readable in the current single-page layout |
| `juce::TextButton` + `juce::Label` | JUCE 8.0.12 | Manual refresh action and lightweight stale/error status line | Use near the existing connection controls |
| Local parser/history helpers | New local code only | Parse `SERVER` lines, extract optional user counts, normalize endpoint keys, and maintain LRU history | Use to keep processor/editor code smaller and more testable |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `juce::ComboBox` with rich one-line labels | Custom popup `ListBox` / `Viewport` picker | More flexible row rendering, but more UI code and more layout/testing surface in an already dense editor |
| Runtime-only public list cache | Persist last public list in plugin state | Would support stale list after reopen, but adds schema/state complexity without being required by DISC-01 |
| Separate discovery config file | Plugin wrapped `ValueTree` child for remembered history | Extra file management is unnecessary when project state persistence already exists |
| Keying combined entries by label text | Keying by normalized `host + port` | Label-keying creates duplicates and merge bugs when the same endpoint appears in both history and public results |

**Installation:**
```bash
# No new third-party dependencies should be added for Phase 8.
cmake --build build-vs --target famalamajam_tests --config Debug
```

## Architecture Patterns

### Recommended Project Structure
```text
src/
|- app/
|  `- session/
|     |- ServerDiscoveryModel.h/.cpp         # parser, endpoint normalization, combined-entry model
|     `- RememberedServerHistory.h/.cpp      # capped deduped LRU helpers
|- plugin/
|  |- FamaLamaJamAudioProcessor.h/.cpp       # discovery snapshot, persisted history, refresh commands
|  `- FamaLamaJamAudioProcessorEditor.h/.cpp # picker, refresh button, status line, host/port population
`- infra/
   `- net/
      `- PublicServerDiscoveryClient.h/.cpp  # JUCE URL worker wrapper
tests/
|- unit/
|  `- server_discovery_parser_tests.cpp
`- integration/
   |- plugin_server_discovery_tests.cpp
   |- plugin_server_discovery_ui_tests.cpp
   `- support/FakeServerDiscoveryClient.h
```

### Pattern 1: Use a Dedicated Discovery Worker, Not the Existing Timer
**What:** Fetch the public server list from a dedicated JUCE thread or worker object, parse the payload off the message thread, and publish an immutable discovery snapshot into processor-owned state under a small lock.
**When to use:** Any auto-refresh or manual refresh of the public list.
**Example:**
```cpp
// Source: JUCE URL/InputStreamOptions docs + current Thread usage in FramedSocketTransport/CodecStreamBridge
int statusCode = 0;
juce::StringPairArray responseHeaders;

auto stream = juce::URL(discoveryUrl).createInputStream(
    juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
        .withConnectionTimeoutMs(3000)
        .withNumRedirectsToFollow(2)
        .withStatusCode(&statusCode)
        .withResponseHeaders(&responseHeaders));

if (stream != nullptr && statusCode >= 200 && statusCode < 300)
    parsePublicServerList(stream->readEntireStreamAsString().toStdString());
```

### Pattern 2: Treat the Picker as a Host/Port Autofill Helper
**What:** The picker does not own the active endpoint. Selecting an entry copies host and port into the existing editable fields, then the normal draft/apply/connect path remains in charge.
**When to use:** For all remembered/public entry selections.
**Example:**
```cpp
// Source: Phase 8 locked decisions + current Connect/apply draft flow
void handlePickerSelection(const DiscoveryEntry& entry)
{
    hostEditor_.setText(entry.host, juce::dontSendNotification);
    portEditor_.setText(juce::String(entry.port), juce::dontSendNotification);
}

// Connect still uses makeDraftFromUi() -> applyHandler_ -> connectHandler_.
```

### Pattern 3: Persist Remembered History as Optional Plugin State, Not as Live Runtime Cache
**What:** Keep active session fields in `SessionSettings`, but persist remembered successful endpoints in a separate optional child under the wrapped plugin state tree. Keep the public list cache runtime-only.
**When to use:** For DISC-02 persistence and state roundtrip.
**Example:**
```cpp
// Source: current plugin wrapped state in FamaLamaJamAudioProcessor::getStateInformation()
juce::ValueTree historyTree("rememberedServerHistory");

for (const auto& entry : rememberedHistory)
{
    juce::ValueTree child("rememberedServer");
    child.setProperty("host", entry.host, nullptr);
    child.setProperty("port", entry.port, nullptr);
    historyTree.addChild(child, -1, nullptr);
}

stateTree.addChild(historyTree, -1, nullptr);
```

### Pattern 4: Build One Combined Remembered-First Entry List With Endpoint Deduplication
**What:** Normalize entries by canonical endpoint key, insert remembered entries first, then merge in public entries that are not already present. If a remembered endpoint is also public, keep one remembered-priority row and optionally enrich it with public metadata.
**When to use:** Whenever the editor requests picker entries.
**Example:**
```cpp
// Source: Phase 8 combined-picker requirement + upstream LRU history behavior
std::string key = normalizeHost(entry.host) + ":" + std::to_string(entry.port);

if (! combinedByKey.contains(key))
    combinedByKey[key] = entry;
else
    combinedByKey[key] = mergeRememberedPriority(combinedByKey[key], entry);
```

### Pattern 5: Use Upstream-Like Freshness Rules for Auto-Refresh
**What:** Track last successful refresh time and suppress repeated auto-refreshes while the cached list is still fresh. Upstream ReaNINJAM uses a two-minute freshness window before auto-refetching again.
**When to use:** On editor open / connection-area open and on manual refresh.
**Example:**
```cpp
// Source: ninjam-upstream/jmde/fx/reaninjam/winclient.cpp
if (! hasCachedList || lastSuccessfulFetch < now - std::chrono::minutes(2))
    requestPublicListRefresh(RefreshReason::AutoOpen);
```

### Anti-Patterns to Avoid
- **Blocking HTTP on the message thread:** JUCE `Timer` runs on the message thread and is explicitly not suitable for blocking I/O.
- **Making the picker the canonical endpoint:** The editable `Host` and `Port` fields must stay authoritative for power users.
- **Persisting stale public-list data as if it were current room state:** Only remembered successful endpoints should survive project reload by default in Phase 8.
- **Adding history on selection or failed connect:** DISC-02 is explicit that only successful connections belong in remembered history.
- **Assuming every public row is `BPM/BPI` plus `current/max`:** current indexed public rows include lobby-style descriptive entries.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Public list HTTP client | Raw HTTP over `juce::StreamingSocket` | `juce::URL` / `WebInputStream` on a dedicated worker | Reuses JUCE request handling, redirects, status capture, and cancellation semantics |
| Cross-session history storage | Separate INI/JSON file beside the plugin | Existing wrapped plugin `ValueTree` state with optional history child | Avoids extra file lifecycle and stays aligned with current project-state persistence |
| UI-local discovery cache | Editor-owned vectors and status strings | Processor-owned `ServerDiscoveryUiState` getter + refresh handlers | Matches the established architecture and keeps editor lifetimes simple |
| Duplicate merge behavior | Comparing display labels | Canonical normalized `host + port` keys | The same endpoint can appear both as remembered and public with different labels |

**Key insight:** The deceptively hard part is not showing a dropdown. It is keeping discovery state honest and recoverable without weakening the existing connect/apply/manual-entry workflow or introducing blocking I/O on JUCE's message thread.

## Common Pitfalls

### Pitfall 1: Blocking the Message Thread During Fetch
**What goes wrong:** Opening the editor or hitting refresh freezes UI interaction because the HTTP request blocks the message loop.
**Why it happens:** JUCE `Timer` callbacks are invoked on the message thread, and `WebInputStream::connect()` / `read()` block.
**How to avoid:** Run discovery fetches on a dedicated worker thread and publish results back as a snapshot.
**Warning signs:** Refresh feels sticky, editor repaint stalls, or timer-based callbacks start doing network I/O.

### Pitfall 2: Overfitting the Parser to Classic BPM Rows
**What goes wrong:** Valid public rows are dropped or rendered badly because the parser assumes every `info` field is `120 BPM/32` and every `users` field is a simple numeric occupancy token.
**Why it happens:** Older examples often show only the classic rows.
**How to avoid:** Parse the line format structurally (`SERVER`, endpoint, info, users), then extract user counts opportunistically and keep unknown text verbatim.
**Warning signs:** Rows like lobby servers disappear, or user counts show nonsense because descriptive text was parsed as a strict numeric schema.

### Pitfall 3: Duplicating the Same Endpoint in History and Public Results
**What goes wrong:** The combined picker shows the same server twice, once as remembered and once as public.
**Why it happens:** History and public entries are merged by label order instead of canonical endpoint.
**How to avoid:** Deduplicate by normalized host+port and let remembered entries win ordering.
**Warning signs:** Selecting one duplicate fills the same fields as another item.

### Pitfall 4: Polluting `SessionSettings` Validation With History Metadata
**What goes wrong:** Discovery/history persistence starts breaking apply/restore flows that should only validate the active connect draft.
**Why it happens:** `SessionSettings` currently models one active host/port/username draft and is validated on every apply.
**How to avoid:** Keep remembered history in separate persisted state and keep active settings narrow.
**Warning signs:** History-only changes trigger validation failures or require fake placeholder values.

### Pitfall 5: Refreshing Too Often on Editor Polls
**What goes wrong:** The plugin repeatedly refetches while the editor is open because refresh is tied to the editor's 20 Hz timer or every `refreshForTesting()`-style update.
**Why it happens:** The editor already polls processor state, so it is tempting to piggyback fetch logic there.
**How to avoid:** Use explicit refresh commands plus freshness timestamps; auto-refresh only on open or explicit user action.
**Warning signs:** Multiple HTTP requests appear during idle UI refresh, or stale/fresh status flickers constantly.

### Pitfall 6: Recording Failed Attempts Into Remembered History
**What goes wrong:** Typos, dead hosts, or incompatible servers clutter the remembered list.
**Why it happens:** History insertion happens on selection or connect request rather than on successful connection.
**How to avoid:** Append/move-to-front only after successful live authentication/connect completion.
**Warning signs:** One mistyped host shows up forever in history after a failed attempt.

### Pitfall 7: Breaking Saved-State Compatibility
**What goes wrong:** Older projects restore to defaults because the wrapped plugin-state schema was changed without a migration path.
**Why it happens:** `setStateInformation()` currently checks an exact wrapped schema version before unpacking child state.
**How to avoid:** Either keep additive history data backward-compatible under the current wrapped schema, or explicitly teach the loader to accept older wrapped schema versions.
**Warning signs:** Existing `plugin_state_roundtrip` behavior regresses after adding discovery persistence.

## Code Examples

Verified patterns from checked sources:

### Parse Public List Rows Structurally
```cpp
// Source: ninjam-upstream/jmde/fx/reaninjam/winclient.cpp
for (auto& line : splitLines(serverListText))
{
    auto tokens = parseQuotedTokens(line);

    if (equalsIgnoreCase(tokens[0], "END"))
        break;

    if (equalsIgnoreCase(tokens[0], "SERVER") && tokens.size() >= 4)
    {
        PublicServerEntry entry;
        entry.endpointText = tokens[1]; // host:port
        entry.infoText = tokens[2];     // may be BPM/BPI or free text
        entry.usersText = tokens[3];    // may be occupancy or descriptive text
        entries.push_back(std::move(entry));
    }
}
```

### Fetch With Explicit Timeout and Status Capture
```cpp
// Source: https://docs.juce.com/master/classjuce_1_1URL_1_1InputStreamOptions.html
int statusCode = 0;
juce::StringPairArray responseHeaders;

auto stream = juce::URL(discoveryUrl).createInputStream(
    juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
        .withConnectionTimeoutMs(3000)
        .withStatusCode(&statusCode)
        .withResponseHeaders(&responseHeaders)
        .withNumRedirectsToFollow(2));
```

### Keep History as a Capped LRU of Successful Endpoints
```cpp
// Source: ninjam-upstream/ninjam/guiclient/mainwnd.cpp
void recordSuccessfulConnection(std::vector<RememberedServer>& history, RememberedServer entry)
{
    eraseIf(history, [&](const auto& existing) { return sameEndpoint(existing, entry); });
    history.insert(history.begin(), std::move(entry));

    if (history.size() > 12)
        history.resize(12);
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Separate previous-server history and public list widgets in upstream clients | One combined remembered-first picker feeding the existing editable fields | Locked for Phase 8 on 2026-03-17 | Preserves power-user manual entry while reducing room-entry friction |
| Hard-coded server-list URL assumption | Configurable discovery endpoint with timeout, status, stale, and redirect handling | Recommended now because current indexed live list differs from historical upstream fetch URL | Reduces brittleness when the public list host changes |
| Host-only recent entries | Rows that always include endpoint text and optionally include server info plus user count | Required by Phase 8 context | Makes the picker useful without becoming a full room browser |
| UI-owned transient picker state | Processor-owned discovery snapshot + editor polling | Established in Phases 5-7 of this codebase | Keeps editor simpler and consistent with existing architecture |

**Deprecated/outdated:**
- Treating the discovery selection as the source of truth instead of the editable `Host` and `Port` fields.
- Assuming the public list is always pure `BPM/BPI` metadata with identical row shape.
- Tying refresh behavior to the editor's existing 20 Hz timer.

## Open Questions

1. **Should the public-list cache survive project reopen, or only survive within the current editor/session lifetime?**
   - What we know: DISC-01 only requires cached-list fallback when refresh fails, and the locked context does not require cross-session public cache persistence.
   - What's unclear: Whether the user expects a stale public list to still appear after reopening an Ableton project offline.
   - Recommendation: Do not persist the public list cache in Phase 8. Keep it runtime-only and persist only remembered successful endpoints.

2. **Is a formatted `ComboBox` enough, or will row readability force a custom popup later?**
   - What we know: Phase 8 needs compact richer-than-host-only rows inside an already dense single-page editor.
   - What's unclear: Whether one-line formatted labels remain readable at the current editor width once host/port, info, and optional count are shown together.
   - Recommendation: Plan against `juce::ComboBox` first. Only escalate to a custom popup/list if UI tests or review show the one-line labels are not usable.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 v3.8.1 |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `.\build-vs\tests\famalamajam_tests.exe "[server_discovery_parser],[plugin_server_discovery],[plugin_server_discovery_ui],[plugin_state_roundtrip]"` |
| Full suite command | `ctest --test-dir build-vs --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| DISC-01 | Fetch and parse the public list, expose a compact picker inside the plugin, auto-refresh on open, support manual refresh, and keep stale/error status explicit without blocking manual host/port entry | unit + integration | `.\build-vs\tests\famalamajam_tests.exe "[server_discovery_parser],[plugin_server_discovery],[plugin_server_discovery_ui]"` | No - Wave 0 |
| DISC-02 | Add successful private endpoints to a deduped remembered history, order remembered entries before public entries, persist them across state roundtrip, and keep manual host/port editing authoritative | integration | `.\build-vs\tests\famalamajam_tests.exe "[plugin_server_discovery],[plugin_server_discovery_ui],[plugin_state_roundtrip]"` | No - Wave 0 |

### Sampling Rate
- **Per task commit:** `.\build-vs\tests\famalamajam_tests.exe "[server_discovery_parser],[plugin_server_discovery],[plugin_server_discovery_ui],[plugin_state_roundtrip]"`
- **Per wave merge:** `ctest --test-dir build-vs --output-on-failure`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/unit/server_discovery_parser_tests.cpp` - covers `SERVER`/`END` parsing, malformed rows, optional user-count extraction, and lobby-style row tolerance
- [ ] `tests/integration/plugin_server_discovery_tests.cpp` - covers processor-owned discovery snapshot, refresh success/failure/stale transitions, and success-only history LRU behavior
- [ ] `tests/integration/plugin_server_discovery_ui_tests.cpp` - covers remembered-first picker ordering, selection populating editable fields, refresh button/status line, and manual edit preservation
- [ ] `tests/integration/support/FakeServerDiscoveryClient.h` - scripted success/failure provider so discovery tests do not rely on the real internet
- [ ] `tests/integration/plugin_state_roundtrip_tests.cpp` - extend with remembered-history persistence coverage
- [ ] `tests/CMakeLists.txt` - register new Phase 8 unit/integration files and tags

## Sources

### Primary (HIGH confidence)
- `cmake/dependencies.cmake` - pinned JUCE `8.0.12` and Catch2 `v3.8.1`
- `src/app/session/SessionSettings.h`
- `src/app/session/SessionSettings.cpp`
- `src/infra/state/SessionSettingsSerializer.cpp`
- `src/net/FramedSocketTransport.h`
- `src/net/FramedSocketTransport.cpp`
- `src/plugin/FamaLamaJamAudioProcessor.h`
- `src/plugin/FamaLamaJamAudioProcessor.cpp`
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/CMakeLists.txt`
- `tests/integration/plugin_room_controls_ui_tests.cpp`
- `tests/integration/plugin_rehearsal_ui_flow_tests.cpp`
- `tests/integration/plugin_state_roundtrip_tests.cpp`
- `ninjam-upstream/jmde/fx/reaninjam/winclient.cpp`
- `ninjam-upstream/ninjam/guiclient/mainwnd.cpp`
- `https://docs.juce.com/master/classTimer.html` - message-thread timer behavior and timing limits
- `https://docs.juce.com/master/classjuce_1_1Thread.html` - JUCE background thread model
- `https://docs.juce.com/master/classjuce_1_1URL_1_1InputStreamOptions.html` - timeout, redirects, status-code, and response-header support
- `https://docs.juce.com/master/classURL.html` - `createInputStream()` URL fetch API
- `https://docs.juce.com/master/classjuce_1_1WebInputStream.html` - blocking connect/read behavior and cancellation semantics
- `https://docs.juce.com/master/classComboBox.html` - compact dropdown behavior and selection callbacks

### Secondary (MEDIUM confidence)
- `.planning/phases/08-server-discovery-history/08-CONTEXT.md` - locked scope, ordering, persistence, and stale/error decisions
- `.planning/REQUIREMENTS.md` - DISC-01 and DISC-02 scope
- `.planning/STATE.md` - milestone decisions and current blockers
- `.planning/ROADMAP.md` - Phase 8 success criteria
- `https://autosong.ninjam.com/serverlist.php` - recent indexed public list content seen via search on 2026-03-17; confirms current `SERVER ... END` payload shape and mixed classic/lobby rows

### Tertiary (LOW confidence)
- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - JUCE/Catch2 versions are pinned locally, and the relevant worker/editor/state patterns already exist in the workspace
- Architecture: MEDIUM - the processor-owned snapshot pattern is clear, but the exact persistence child layout and picker widget choice still need implementation judgment
- Pitfalls: HIGH - the main failure modes are directly visible from current code constraints, JUCE docs, and upstream list/history behavior

**Research date:** 2026-03-17
**Valid until:** 2026-03-31
