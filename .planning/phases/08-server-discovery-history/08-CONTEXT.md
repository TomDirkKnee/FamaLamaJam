# Phase 8: Server Discovery & History - Context

**Gathered:** 2026-03-17
**Status:** Ready for planning

<domain>
## Phase Boundary

Improve room entry workflow with public server discovery and remembered private server history inside the existing plugin UI. This phase adds a public server picker plus lightweight remembered-server history without degrading direct manual host/port entry for power users. It does not include the broader JamTaba-inspired layout overhaul, advanced diagnostics, or per-server preset management.

</domain>

<decisions>
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

</decisions>

<specifics>
## Specific Ideas

- The user wants the equivalent of a dropdown server list of available public NINJAM servers plus remembered private servers.
- The user explicitly wants private servers they have connected to before to show up in the same flow.
- The user wants to keep direct manual connect workable rather than hiding host/port behind an advanced workflow.
- The user called out JamTaba and ReaNINJAM as references for richer server rows and specifically wants connected-user count shown when possible.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/app/session/SessionSettings.h`: currently owns persisted connection/session fields and is the natural place to extend state with remembered-server history or cached discovery metadata.
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`: already renders the single-page connection UI with `Host` and `Port` editors; Phase 8 should layer the picker into this existing area instead of inventing a separate shell.
- `src/plugin/FamaLamaJamAudioProcessor.h`: already owns processor-visible connection snapshot/state used by the editor, which is the likely home for discovery status snapshots and picker-facing state.

### Established Patterns
- The current plugin still relies on a single-page JUCE editor, and Phase 7 already made that page denser; Phase 8 should avoid a broad layout rewrite.
- Earlier phases established an honesty rule for connection and room state: stale session data should be cleared or clearly labeled rather than silently implied as current.
- Phase 7 carried forward explicit layout debt about the mixer being squeezed; Phase 8 should stay compact and avoid turning discovery into a large sidebar/browser.

### Reference Implementations
- `ninjam-upstream/jmde/fx/reaninjam/winclient.cpp`: upstream ReaNINJAM fetches the public list from `http://ninjam.com/serverlist.php`, caches list data, and stores recent hosts in lightweight history entries.
- `ninjam-upstream/ninjam/guiclient/mainwnd.cpp`: upstream GUI client keeps a short previous-server history with LRU-like behavior.

### Integration Points
- Discovery/history state will likely need to flow from processor-owned/persisted settings into the existing editor connection area.
- Public-list fetch and parse logic should integrate with the current connection workflow without landing network work on the realtime path.
- Tests should extend current connection/editor validation rather than relying purely on manual room-entry testing.

</code_context>

<deferred>
## Deferred Ideas

- Larger JamTaba-inspired room-browser layout and broader horizontal UI rework belong to Phase 9.
- Per-server profile behavior such as remembering usernames or full session presets is out of scope for Phase 8.
- Explicit history management UI such as removing individual remembered servers or a clear-history action is deferred unless planning later proves a tiny control is essential.
- Advanced NINJAM discovery/listen parity work remains part of later research phases.

</deferred>

---

*Phase: 08-server-discovery-history*
*Context gathered: 2026-03-17*
