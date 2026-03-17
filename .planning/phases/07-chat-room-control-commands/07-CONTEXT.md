# Phase 7: Chat & Room Control Commands - Context

**Gathered:** 2026-03-17
**Status:** Ready for planning

<domain>
## Phase Boundary

Let users participate in room communication and room-level BPM/BPI voting directly from the plugin. This phase adds public room chat, room-topic/system visibility, and dedicated BPM/BPI vote controls inside the existing single-page plugin UI. It does not include private chat, moderation tooling, server discovery, or the broader JamTaba-inspired layout refresh.

</domain>

<decisions>
## Implementation Decisions

### Chat surface in the current UI
- Phase 7 should add a compact embedded room chat surface inside the current single-page plugin UI.
- The chat surface should not trigger a larger layout rewrite; the bigger JamTaba-inspired rearrangement stays in a later phase.
- The room-message composer should stay always visible while connected rather than being hidden behind an extra expander or mode switch.
- The visible room chat log should be session-only: clear it on disconnect or when joining a different room so the feed always reflects the current live session.

### Room feed composition
- Use one mixed room feed for public chat plus room/system activity rather than splitting chat and system activity into separate tabs or panes in this phase.
- Support the public room message set: normal room chat, topic changes, join/part events, and vote-related system feedback.
- Private messages and moderation/admin actions beyond BPM/BPI voting are deferred out of Phase 7.
- Join/leave activity should remain visible but visually subtler than normal chat so the room still feels live without becoming noisy.
- The room topic should be shown both as a pinned current-topic element near room controls and as feed entries when the topic changes.

### BPM/BPI voting workflow
- BPM/BPI voting should use direct UI controls rather than requiring typed slash commands.
- Vote controls should sit near transport and room-timing information, not be buried inside the chat composer.
- BPM and BPI should use editable numeric fields rather than preset-only dropdowns or steppers.
- Vote controls should be usable when connected to a live room; they do not need to wait for a separate “timing ready” gate beyond normal connection state.
- Vote submission should show small inline pending/failure feedback near the controls while also allowing room/system confirmation to appear in the chat feed.

### Command and feedback framing
- Under the hood, Phase 7 should still respect NINJAM’s command/chat model: dedicated controls may send room commands, but the user should experience them as normal plugin controls.
- Chat and vote feedback should be understandable during rehearsal without requiring users to know slash-command syntax.
- The room feed should remain the canonical place for shared room-visible outcomes such as topic changes and vote progress/results.

### Claude's Discretion
- Exact copy for chat/system labels, pending-vote wording, and subtle join/part styling.
- Exact compact layout inside the current JUCE page, provided it stays single-page and avoids the larger Phase 9 redesign.
- Exact validation ranges and input affordances for BPM/BPI fields, as long as the controls remain direct and rehearsal-friendly.

</decisions>

<specifics>
## Specific Ideas

- The user wants “Ninjam Chat” as a proper plugin feature rather than a hidden command workflow.
- The user wants the ability to set BPM/BPI for voting through GUI controls and asked how JamTaba does it.
- Upstream NINJAM/ReaNINJAM treats voting as chat/admin-style commands, while JamTaba wraps BPM/BPI voting in explicit room controls.
- The user previously described a future right-hand chat panel and broader layout changes, but agreed those larger UI shifts belong to a later phase rather than Phase 7.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/net/FramedSocketTransport.h`: already owns socket I/O, framed message parsing, authentication, config-change handling, and user-activity updates; this is the natural place to extend inbound/outbound room-message handling.
- `src/plugin/FamaLamaJamAudioProcessor.h`: already aggregates connection state, transport UI state, host-sync assist state, and mixer snapshots for the editor; this gives Phase 7 a processor-owned place to store chat log state, topic state, and vote-control state.
- `src/plugin/FamaLamaJamAudioProcessorEditor.h`: already renders the single-page session/transport/mixer UI and exposes testing helpers; this is the natural place to add a compact room feed, composer, and vote controls without inventing another UI shell.
- `tests/integration/support/MiniNinjamServer.h`: existing fake-server harness already supports timing/config/user events and can be extended for room chat/topic/vote message tests.

### Established Patterns
- Phase 5 locked the app to a single-page JUCE UI with setup/status information visible near the top.
- Phase 6 kept room timing and host-sync affordances near transport/status, which supports placing BPM/BPI room controls near that same area.
- Recent bug fixes established that stale room state should be cleared on disconnect/reconnect; session-only chat history should follow the same honesty principle.
- The current plugin has no chat parser or chat-facing transport API yet, so Phase 7 introduces a new state surface rather than extending an existing partial feature.

### Reference Implementations
- `ninjam-upstream/jmde/fx/reaninjam/chat.cpp`: upstream room chat handles `MSG`, `PRIVMSG`, `TOPIC`, `JOIN`, and `PART`, which defines the core message vocabulary.
- `ninjam-upstream/jmde/fx/reaninjam/winclient.cpp`: upstream sends room chat with `ChatMessage_Send("MSG", ...)`, private messages with `PRIVMSG`, and admin/vote actions with `ADMIN`.
- `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Common/gui/NinjamRoomWindow.cpp`: JamTaba exposes BPM/BPI changes through explicit UI controls and routes them to `voteBpm()` / `voteBpi()`.
- `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Common/gui/chat/NinjamChatMessageParser.cpp`: JamTaba parses voting-related system messages and treats admin commands as a separate command class under the hood, which is useful for room-feedback parsing even if our Phase 7 UI stays simpler.

### Integration Points
- Chat/topic/vote receive paths will likely enter through `FramedSocketTransport`, then flow into processor-owned room state consumed by the editor.
- Room controls should integrate with the existing connection lifecycle and top-of-page transport/status area instead of creating a second disconnected collaboration workflow.
- Automated tests should extend the existing integration harness rather than relying on manual server testing for basic room-message correctness.

</code_context>

<deferred>
## Deferred Ideas

- Private messages and tabbed/private chat surfaces.
- Moderation/admin commands beyond BPM/BPI voting, such as kick/topic authoring workflows more complex than passive display.
- Public server browser and remembered private-server history, which belong to Phase 8.
- Larger JamTaba-inspired horizontal layout, right-hand chat pane, and mixer/UI reshaping, which belong to Phase 9.
- Voice-chat parity and other advanced NINJAM features, which belong to later research/parity phases.

</deferred>

---

*Phase: 07-chat-room-control-commands*
*Context gathered: 2026-03-17*
