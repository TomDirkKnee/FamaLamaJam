# Phase 08.3.1 Manual Validation Matrix

## Public Row Recall

- `P8.3.1-RECALL-01`
  - Choose a Ninbot-listed public server that has remembered credentials for the same `host:port`.
  - Confirm the picker still shows the richer public row rather than a plain remembered row.
  - Confirm selecting it restores `host`, `username`, and masked password reuse behind the scenes.

## Compact Header

- `P8.3.1-HEADER-02`
  - Check the top/session area while connected.
  - Confirm the summary reads in the compact form `Connected as USER | HOST:PORT`.
  - Confirm the second line reads as compact timing like `100 BPM | 16 BPI`.

## Room Chat Density

- `P8.3.1-ROOM-03`
  - Confirm the room heading reads `Room Chat`.
  - Confirm BPM/BPI use one shared `Vote` button and only one field stays active at a time.
  - Confirm the room area feels denser without becoming awkward.

## Diagnostics Takeover

- `P8.3.1-DIAG-04`
  - Leave diagnostics collapsed first and confirm the room chat area is normal.
  - Open diagnostics and confirm the diagnostics view takes over the sidebar/chat area instead of appearing as a cramped extra box.

## Chat Follow

- `P8.3.1-CHAT-05`
  - Stay near the bottom of the room feed and confirm new messages keep following naturally.
  - Scroll upward into older history and confirm new messages do not yank the view back to the bottom.
