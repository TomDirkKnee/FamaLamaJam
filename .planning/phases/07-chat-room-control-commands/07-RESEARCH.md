# Phase 7: Chat & Room Control Commands - Research

**Researched:** 2026-03-17
**Domain:** JUCE plugin room chat, NINJAM chat/control protocol, and rehearsal-safe room UI state
**Confidence:** MEDIUM

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
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
- Vote controls should be usable when connected to a live room; they do not need to wait for a separate "timing ready" gate beyond normal connection state.
- Vote submission should show small inline pending/failure feedback near the controls while also allowing room/system confirmation to appear in the chat feed.

### Command and feedback framing
- Under the hood, Phase 7 should still respect NINJAM's command/chat model: dedicated controls may send room commands, but the user should experience them as normal plugin controls.
- Chat and vote feedback should be understandable during rehearsal without requiring users to know slash-command syntax.
- The room feed should remain the canonical place for shared room-visible outcomes such as topic changes and vote progress/results.

### Claude's Discretion
- Exact copy for chat/system labels, pending-vote wording, and subtle join/part styling.
- Exact compact layout inside the current JUCE page, provided it stays single-page and avoids the larger Phase 9 redesign.
- Exact validation ranges and input affordances for BPM/BPI fields, as long as the controls remain direct and rehearsal-friendly.

### Deferred Ideas (OUT OF SCOPE)
## Deferred Ideas

- Private messages and tabbed/private chat surfaces.
- Moderation/admin commands beyond BPM/BPI voting, such as kick/topic authoring workflows more complex than passive display.
- Public server browser and remembered private-server history, which belong to Phase 8.
- Larger JamTaba-inspired horizontal layout, right-hand chat pane, and mixer/UI reshaping, which belong to Phase 9.
- Voice-chat parity and other advanced NINJAM features, which belong to later research/parity phases.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| ROOM-01 | User can send and receive Ninjam room chat messages from inside the plugin. | Extend `FramedSocketTransport` for `MESSAGE_CHAT_MESSAGE (0xC0)`, add processor-owned bounded room-feed/topic state, and add editor composer/feed bindings plus integration coverage with `MiniNinjamServer`. |
| ROOM-02 | User can issue BPM/BPI vote commands from the plugin UI and see room feedback/results. | Add direct BPM/BPI controls near transport, route commands through a dedicated formatter, track inline pending/error state, parse vote-related system messages/config changes, and cover both transport and UI flows in tests. |
</phase_requirements>

## Summary

Phase 7 should stay inside the current single-page JUCE editor and use the same processor-to-editor getter/callback pattern already established for transport status, sync assist, and mixer strips. The main new work is not just UI: the current transport only handles timing, user-info, and audio frames, and its outbound queue is hard-wired to upload intervals. Room chat and room control commands need a separate room-message path.

The safest implementation is: parse and queue room events in `FramedSocketTransport`, store a bounded session-only room model in `FamaLamaJamAudioProcessor`, and let `FamaLamaJamAudioProcessorEditor` poll that model on its existing 20 Hz timer. Do not parse chat or allocate feed entries in `processBlock()`. That keeps room interaction visible in the plugin without contaminating the audio thread.

For BPM/BPI voting, the best match to the requirement language and the JamTaba reference is a dedicated command formatter that emits JamTaba-style vote messages (`MSG "!vote bpm N"` / `MSG "!vote bpi N"`) behind direct controls. Upstream NINJAM also supports admin-style `/bpm` and `/bpi` commands, but those have different permission and range semantics. Hide that distinction behind one formatter so the planner can localize protocol changes if the real rehearsal server behaves differently.

**Primary recommendation:** Add a bounded processor-owned room model fed by transport-thread chat parsing, and ship direct BPM/BPI controls that default to JamTaba-style `!vote` messages while keeping the mixed room feed as the canonical shared feedback surface.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 8.0.12 | Plugin UI, timers, text inputs, viewport, sockets | Already pinned in `cmake/dependencies.cmake` and used throughout the plugin/editor stack |
| Catch2 | v3.8.1 | Unit and integration tests | Already pinned and wired through `catch_discover_tests()` |
| NINJAM `mpb_chat_message` protocol | Current upstream protocol in repo | Room chat/control frame format (`MESSAGE_CHAT_MESSAGE`, null-terminated params) | This is the authoritative protocol surface the transport must speak |
| Existing `FramedSocketTransport` + processor/editor getter pattern | Workspace current | Transport I/O, processor state ownership, editor polling | Matches the codebase's existing architecture and minimizes new moving parts |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `juce::TextEditor` | JUCE 8.0.12 | Room composer and numeric BPM/BPI fields | Use for the always-visible composer and editable vote inputs |
| `juce::Viewport` | JUCE 8.0.12 | Scrollable mixed room feed | Use for a compact embedded feed, matching the existing mixer UI approach |
| `tests/integration/support/MiniNinjamServer` | Workspace current | Fake server for chat/topic/vote/config message coverage | Extend instead of building a second socket stub |
| Small room-feed parser/helper | New local helper if needed | Classify system/vote text and keep processor code smaller | Use if vote/system parsing grows beyond trivial prefix checks |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `MSG "!vote ..."` vote formatter | `ADMIN "bpm ..."` / `ADMIN "bpi ..."` | Admin commands exist upstream, but they change semantics from "vote" to immediate privileged room change and use different ranges |
| `juce::Viewport` + custom feed content | `juce::ListBox` | `ListBox` is viable, but `Viewport` matches the current editor's manual layout/testing pattern |
| Processor-owned session-only room feed | Persisting feed/topic in plugin state | Persisted room chat violates the locked requirement that the feed reflect only the current live session |

**Installation:**
```bash
# No new dependencies should be added for Phase 7.
cmake --build build-vs --target famalamajam_tests --config Debug
```

## Architecture Patterns

### Recommended Project Structure
```text
src/
|- net/
|  |- FramedSocketTransport.h/.cpp   # add chat frame parsing and typed outbound room commands
|- plugin/
|  |- FamaLamaJamAudioProcessor.h/.cpp      # room feed/topic/vote state + editor command handlers
|  |- FamaLamaJamAudioProcessorEditor.h/.cpp # compact room controls, feed, composer
|  `- RoomFeedParser.h/.cpp           # optional helper if system/vote text parsing grows
tests/
|- integration/
|  |- plugin_room_chat_tests.cpp      # send/receive/reset behavior
|  |- plugin_room_controls_ui_tests.cpp # vote controls, pending state, layout
|  `- support/MiniNinjamServer.h      # chat/topic/vote helpers
```

### Pattern 1: Generalize the Transport Outbound Queue
**What:** Replace the current audio-only outbound queue with a typed outbound message queue so audio uploads still use `writeUploadInterval()`, while room chat/control commands use `writeMessage(MESSAGE_CHAT_MESSAGE, ...)`.
**When to use:** Any time the plugin sends non-audio NINJAM traffic after authentication.
**Example:**
```cpp
// Source: src/net/FramedSocketTransport.cpp, ninjam-upstream/ninjam/mpb.h
struct OutboundMessage
{
    enum class Kind { UploadInterval, RoomChat };
    Kind kind {};
    juce::MemoryBlock payload;
};

if (message.kind == OutboundMessage::Kind::UploadInterval)
    writeUploadInterval(message.payload);
else
    writeMessage(0xC0, message.payload.getData(), message.payload.getSize());
```

### Pattern 2: Keep Room State in the Processor, Not the Editor
**What:** Add a bounded room snapshot owned by `FamaLamaJamAudioProcessor`, for example: `currentTopic`, `std::deque<RoomFeedEntry>`, `pendingBpmVote`, `pendingBpiVote`, and command methods like `sendRoomChatMessage()` / `submitRoomVote()`.
**When to use:** For all room UI data, including feed history, pinned topic, and inline vote status.
**Example:**
```cpp
// Source: src/plugin/FamaLamaJamAudioProcessor.cpp createEditor() getter/callback pattern
struct RoomFeedEntry
{
    enum class Kind { Chat, Topic, JoinPart, VoteSystem, GenericSystem };
    Kind kind {};
    std::string author;
    std::string text;
    bool subdued { false };
};

struct RoomUiState
{
    std::string topic;
    std::vector<RoomFeedEntry> visibleFeed;
    bool bpmVotePending { false };
    bool bpiVotePending { false };
};
```

### Pattern 3: Poll Room State From the Existing Editor Timer
**What:** Mirror the current transport/mixer pattern: the editor gets `RoomUiGetter` and command handlers, then refreshes labels, feed rows, and control states in `timerCallback()`.
**When to use:** For all visible room UI updates while the editor is open.
**Example:**
```cpp
// Source: src/plugin/FamaLamaJamAudioProcessorEditor.cpp
void timerCallback() override
{
    refreshLifecycleStatus();
    refreshTransportStatus();
    refreshRoomUi();
    refreshMixerStrips();
}
```

### Pattern 4: Treat Vote Feedback as Server-Authored Feed Entries
**What:** Keep inline pending/error state near the BPM/BPI controls, but only treat server/system messages and config changes as canonical shared outcomes in the room feed.
**When to use:** For `!vote` progress, successful timing changes, permission failures, and generic server notices.
**Example:**
```cpp
// Source: JamTaba vote flow and NINJAM server message patterns
if (sentLocalVote)
    pendingBpmVote = true;

if (incomingSystemText.starts_with("[voting system]") || configChangedToVotedValue)
    pendingBpmVote = false;
```

### Anti-Patterns to Avoid
- **Editor-direct socket access:** The editor should not know about sockets or protocol frame types.
- **Room-feed mutation in `processBlock()`:** Chat parsing and feed entry allocation do not belong on the audio thread.
- **Persisting chat/topic in plugin state:** The feed must clear with session changes.
- **Inferring vote success locally:** Only server/system text or config-change notifications should clear pending state as "successful."

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Outbound room command transport | A second socket path or editor-owned sender | Extend `FramedSocketTransport` with typed outbound messages | Keeps auth, framing, keepalive, and failure handling in one place |
| Room feed storage | An unbounded append-only log | A bounded deque/ring buffer in processor state | Prevents memory creep and matches session-only requirements |
| Vote workflow semantics | Client-side "successful vote" assumptions | Server/system message parsing plus config-change confirmation | Real NINJAM servers vary in permission and vote behavior |
| Test server | A new ad hoc socket harness | `MiniNinjamServer` extensions for `MESSAGE_CHAT_MESSAGE` | Existing tests already rely on this harness and its timing/config behavior |

**Key insight:** The deceptively hard part here is not drawing a chat box. It is preserving the current transport/audio safety model while adding a second, text-heavy event stream that has very different allocation and threading behavior from audio frames.

## Common Pitfalls

### Pitfall 1: Reusing the Current Outbound Queue for Chat
**What goes wrong:** Chat or vote commands get wrapped as upload intervals because `outboundQueue_` currently assumes every payload goes through `writeUploadInterval()`.
**Why it happens:** `FramedSocketTransport::run()` only knows about one outbound payload type today.
**How to avoid:** Introduce a typed outbound message abstraction before adding any room command UI.
**Warning signs:** Commands appear to send, but the server never emits chat/vote feedback and transport stats only look like audio uploads.

### Pitfall 2: Parsing Room Events in `processBlock()`
**What goes wrong:** Audio stability regresses because string parsing, deque growth, and lock contention happen on the realtime path.
**Why it happens:** `processBlock()` already drains audio-related transport queues, so it is tempting to drain room events there too.
**How to avoid:** Parse room chat on the transport thread, then merge it into processor state from a non-audio path.
**Warning signs:** New locks or heap allocations appear in room-feed update code reachable from `processBlock()`.

### Pitfall 3: Picking the Wrong Vote Command Semantics
**What goes wrong:** The UI says "vote," but the transport sends admin-style immediate timing commands, or vice versa.
**Why it happens:** Upstream NINJAM clients and JamTaba expose both admin and vote-shaped command flows.
**How to avoid:** Hide command formatting behind one helper and validate against the target rehearsal server early.
**Warning signs:** Range checks disagree (`40-400 / 2-64` for `!vote` vs broader admin ranges), or server feedback text does not match expected vote behavior.

### Pitfall 4: Over-Specializing System Message Parsing
**What goes wrong:** Vote/system feedback disappears because the parser only recognizes one exact string.
**Why it happens:** NINJAM system feedback often arrives as plain `MSG` text with an empty username.
**How to avoid:** Classify known patterns like `[voting system] ...`, but preserve and display unrecognized system text verbatim.
**Warning signs:** Empty-author server messages are dropped instead of shown.

### Pitfall 5: Layout Regression in the Single-Page UI
**What goes wrong:** The chat feed/composer or vote controls push transport/status/mixer affordances too far down or offscreen.
**Why it happens:** The current editor already has setup, status, transport, sync assist, connect/disconnect, and mixer sections.
**How to avoid:** Keep the room section compact, keep vote controls near transport, and assert with UI tests that room controls stay above the mixer.
**Warning signs:** Existing `plugin_rehearsal_ui_flow_tests` style assertions start failing or need large expectation rewrites.

### Pitfall 6: Forgetting Session Reset Rules
**What goes wrong:** Old room messages, topic text, or pending vote badges survive disconnect/reconnect.
**Why it happens:** Current teardown paths clear transport/audio runtime, but there is no room state yet.
**How to avoid:** Clear room feed, topic, and pending vote state alongside other live-session teardown paths.
**Warning signs:** Reconnect shows old room text before any new server messages arrive.

## Code Examples

Verified patterns from checked sources:

### Parse NINJAM Chat Frames as Null-Terminated Params
```cpp
// Source: ninjam-upstream/ninjam/mpb.cpp + mpb.h
// MESSAGE_CHAT_MESSAGE = 0xC0
// Payload is up to 5 null-terminated strings.
std::array<std::string, 5> parms = parseChatPayload(payload);

if (parms[0] == "MSG")
    enqueueRoomEvent(RoomEvent::publicChat(parms[1], parms[2]));
else if (parms[0] == "TOPIC")
    enqueueRoomEvent(RoomEvent::topic(parms[1], parms[2]));
else if (parms[0] == "JOIN" || parms[0] == "PART")
    enqueueRoomEvent(RoomEvent::presence(parms[0], parms[1]));
```

### Format Vote Commands Through One Helper
```cpp
// Source: JamTaba Service.cpp, NinjamController.cpp, and upstream server voting paths
std::string formatRoomVoteCommand(RoomVoteKind kind, int value)
{
    if (kind == RoomVoteKind::Bpm)
        return "!vote bpm " + std::to_string(value);

    return "!vote bpi " + std::to_string(value);
}
```

### Bind Editor Controls the Same Way as Existing Sync/Mixer Controls
```cpp
// Source: src/plugin/FamaLamaJamAudioProcessor.cpp createEditor()
auto roomUiGetter = [this]() { return getRoomUiState(); };
auto sendChat = [this](std::string text) { return sendRoomChatMessage(std::move(text)); };
auto voteBpm = [this](int bpm) { return submitRoomVote(RoomVoteKind::Bpm, bpm); };
auto voteBpi = [this](int bpi) { return submitRoomVote(RoomVoteKind::Bpi, bpi); };
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Typed slash/admin commands in a chat box | Direct room controls that emit underlying commands | Already established in JamTaba reference flow | Better rehearsal UX without hiding protocol reality |
| Editor-local UI state islands | Processor-owned snapshot getters and editor polling | Established in Phases 5-6 of this codebase | Makes Phase 7 consistent with current architecture |
| Persistent/general-purpose chat logs | Session-only bounded live-room feed | Locked by Phase 7 context on 2026-03-17 | Avoids stale collaboration state after reconnect |

**Deprecated/outdated:**
- Hidden slash-command-only BPM/BPI workflows for this phase.
- Treating the mixer or transport section as the only realtime collaboration surface.
- Letting raw protocol details leak directly into the editor widget tree.

## Open Questions

1. **Which vote command shape does the real rehearsal server expect?**
   - What we know: JamTaba sends `MSG "!vote bpm/bpi N"`, while upstream ReaNINJAM UIs also route `/bpm` and `/bpi` through `ADMIN`.
   - What's unclear: Which path the actual target rooms rely on in practice.
   - Recommendation: Implement one command formatter abstraction and validate it against the user's real rehearsal server before the final verification pass.

2. **Should room-event draining live in the processor on a non-audio path or fully inside the transport?**
   - What we know: `processBlock()` is the wrong place, and the editor already has a 20 Hz polling timer.
   - What's unclear: Whether planner prefers transport-owned bounded room state or a processor-owned merge step on the message thread.
   - Recommendation: Keep authoritative room UI state in the processor, but make transport parsing/queuing self-contained so the merge point stays cheap and testable.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 v3.8.1 |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `.\build-vs\tests\famalamajam_tests.exe "[plugin_room_chat],[plugin_room_controls_ui]"` |
| Full suite command | `ctest --test-dir build-vs --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| ROOM-01 | Send public chat, receive chat/topic/join/part/system lines, and clear session-only feed on disconnect/rejoin | integration | `.\build-vs\tests\famalamajam_tests.exe "[plugin_room_chat]"` | ❌ Wave 0 |
| ROOM-02 | Submit BPM/BPI votes from direct controls, show inline pending/error feedback, and surface room-visible feedback/results | integration | `.\build-vs\tests\famalamajam_tests.exe "[plugin_room_controls_ui]"` | ❌ Wave 0 |

### Sampling Rate
- **Per task commit:** `.\build-vs\tests\famalamajam_tests.exe "[plugin_room_chat],[plugin_room_controls_ui]"`
- **Per wave merge:** `ctest --test-dir build-vs --output-on-failure`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/integration/plugin_room_chat_tests.cpp` - covers ROOM-01 transport, processor state, and disconnect/rejoin reset behavior
- [ ] `tests/integration/plugin_room_controls_ui_tests.cpp` - covers ROOM-02 vote controls, pending/error copy, and layout placement above the mixer
- [ ] `tests/integration/support/MiniNinjamServer.h` - add `MESSAGE_CHAT_MESSAGE` helpers plus scripted topic/vote/system feedback paths
- [ ] `tests/CMakeLists.txt` - register new Phase 7 integration files

## Sources

### Primary (HIGH confidence)
- `cmake/dependencies.cmake` - pinned JUCE `8.0.12` and Catch2 `v3.8.1`
- `src/net/FramedSocketTransport.h`
- `src/net/FramedSocketTransport.cpp`
- `src/plugin/FamaLamaJamAudioProcessor.h`
- `src/plugin/FamaLamaJamAudioProcessor.cpp`
- `src/plugin/FamaLamaJamAudioProcessorEditor.h`
- `src/plugin/FamaLamaJamAudioProcessorEditor.cpp`
- `tests/integration/support/MiniNinjamServer.h`
- `tests/CMakeLists.txt`
- `ninjam-upstream/ninjam/mpb.h`
- `ninjam-upstream/ninjam/mpb.cpp`
- `ninjam-upstream/ninjam/server/usercon.h`
- `ninjam-upstream/ninjam/server/usercon.cpp`
- `ninjam-upstream/jmde/fx/reaninjam/chat.cpp`
- `ninjam-upstream/jmde/fx/reaninjam/winclient.cpp`
- `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Common/gui/NinjamRoomWindow.cpp`
- `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Common/gui/MainWindow.cpp`
- `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Common/gui/chat/NinjamChatMessageParser.cpp`
- `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Common/NinjamController.cpp`
- `C:/Users/Dirk/Documents/source/reference/JamTaba/src/Common/ninjam/client/Service.cpp`
- `https://docs.juce.com/master/classjuce_1_1TextEditor.html` - composer and editable input capabilities
- `https://docs.juce.com/master/classjuce_1_1Viewport.html` - embedded scrollable feed container behavior
- `https://catch2-temp.readthedocs.io/en/latest/cmake-integration.html` - `catch_discover_tests()` CMake integration

### Secondary (MEDIUM confidence)
- `.planning/phases/07-chat-room-control-commands/07-CONTEXT.md` - locked scope and UX constraints for this phase
- `.planning/REQUIREMENTS.md` - ROOM-01 and ROOM-02 scope
- `.planning/STATE.md` - current milestone decisions and validation constraints

### Tertiary (LOW confidence)
- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - versions and test stack are pinned locally, and the relevant code surfaces already exist
- Architecture: HIGH - the codebase already uses processor-owned snapshots and editor polling for adjacent features
- Pitfalls: MEDIUM - the main uncertainty is vote-command semantics across actual target servers, not the local implementation pattern

**Research date:** 2026-03-17
**Valid until:** 2026-04-16
