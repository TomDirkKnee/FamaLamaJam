---
created: 2026-03-17T15:59:34.659Z
title: Polish server discovery counts and selection
area: general
files:
  - src/infra/net/PublicServerDiscoveryClient.cpp
  - src/app/session/ServerDiscoveryModel.cpp
  - src/plugin/FamaLamaJamAudioProcessor.cpp
  - src/plugin/FamaLamaJamAudioProcessorEditor.cpp
  - C:/Users/Dirk/Documents/source/reference/JamTaba
---

## Problem

Phase 8 manual validation surfaced two discovery issues that should not get lost before the layout phase.

1. The public server list currently comes from `http://ninjam.com/serverlist.php`, but the visible user-count information does not feel trustworthy in practice. The count shown as the first number in values like `1/8` never seemed to change during manual testing, so we need to confirm whether our interpretation is correct or whether JamTaba derives live room/user counts differently.
2. After pressing `Connect`, the server picker jumps to a different entry than the one the user selected or connected to, which makes the workflow feel unstable even though manual Host/Port connect still works.

The user specifically wants JamTaba parity checked here because JamTaba and ReaNINJAM appear to present rooms ordered by busiest rooms first and show clearer user-count information.

## Solution

Investigate JamTaba source to confirm how it obtains and presents room/user counts, including whether it relies only on the public list payload or on additional room-state data. Then:

- verify our `connectedUsers` extraction against upstream/JamTaba behavior
- display clearer current-user and capacity information instead of blindly echoing raw `1/8` text
- sort public rooms by current connected-user count descending while keeping remembered servers grouped ahead of public entries if that remains the chosen UX
- fix the picker so pressing `Connect` does not jump the selected row to another server unexpectedly

This may become a small Phase 8.1 follow-up if we want the discovery workflow polished before the larger Phase 9 layout redesign.
