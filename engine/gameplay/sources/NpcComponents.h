/*
Created: 09:08:2026 - 00:18:26
Last updated: 09:08:2026 - 00:18:26
Module: engine/gameplay
File: engine/gameplay/sources/NpcComponents.h

Responsibility:
- NPC-specific plain-data components: voice timbre (Q80) and daily schedule
  state (Q44). Cross-zone components (Transform, CameraPose) live in lead-owned
  engine/core/components — never here.

Key items:
- VoiceTimbre: which voice speaks this NPC's lines; feeds the segment hash.
- ScheduleState: which schedule the NPC follows and where in it it is.

Dependencies:
- Uses: engine/gameplay Ids.h, C++ stdlib.
- Used by: dialogue system (voice at playback/hash time), schedule system
  (derives NpcActions), save delta (ScheduleState persists; see NpcAction.h
  notes for what does NOT persist).

Notes:
- Q80: the line stores "what" and "how"; the NPC brings "with whose voice".
  VoiceTimbre is that component; segment_content_hash(segment, voice) resolves
  the audio file per speaker.
- Q44: NPCs are statically-placed functions at start, but the schedule concept
  is architectural from day one; NPC position is a component, not a chunk
  record. The schedule system reads ScheduleState each tick and drives the NPC
  exclusively by enqueueing NpcActions (Rule 15) — it never moves an NPC
  directly. Schedule definitions are content files (Rule 5).
- ScheduleState is save-delta state (agreed with core, stage-1 sync); phase
  granularity (phase index + elapsed) is enough to resume after load without
  replaying the day.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Plain data only (Rule 8); reference other entities by EntityId, content by id
  types from Ids.h.
- Propose any component needed by render/core to the lead instead of adding a
  cross-zone type here (Rule 25).
*/
/*
UPD:
- 09:08:2026 - 00:18:26: Initial stage-1 NPC components (voice timbre,
                         schedule state).
*/

#pragma once

#include <cstdint>

#include "engine/gameplay/sources/Ids.h"

namespace dfn::gameplay {

// Q80: voice timbre is an NPC component. Lines are speaker-agnostic; this id
// enters segment_content_hash() to select/name the audio per speaker.
struct VoiceTimbre {
    VoiceId voice{};
};

// Q44: which daily schedule this NPC follows and its current position in it.
// The schedule system turns this into NpcActions each tick (Rule 15).
struct ScheduleState {
    ScheduleId schedule{};        // content-defined schedule; invalid = static NPC
    uint32_t phase = 0;           // index into the schedule's phase list
    float phase_elapsed = 0.0f;   // sim seconds spent in the current phase
};

} // namespace dfn::gameplay
