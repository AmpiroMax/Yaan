/*
Created: 09:08:2026 - 00:18:26
Last updated: 09:08:2026 - 14:12:02
Module: engine/gameplay
File: engine/gameplay/sources/Ids.h

Responsibility:
- Typed stable content identifiers shared across gameplay headers. Each wraps a
  64-bit hash of an authored string id from content files (Rule 5).

Key items:
- DialogueLineId, VoiceId, ItemId, ScheduleId, LootTableId: distinct POD types
  so ids of different kinds cannot be mixed up (0 = invalid).

Dependencies:
- Uses: C++ stdlib only.
- Used by: every engine/gameplay header; game systems; the editor.

Notes:
- Values are produced from authored string ids via the frozen stable hash
  (dfn::serialization fnv1a64, core zone) at content-load time; C++ never
  contains the authored strings themselves (Rule 5). Distinct types are plain
  structs, not a shared template, to keep component fields greppable and the
  save format explicit (Rule 7).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Add a new id type here only when a content file kind actually exists for it.
*/
/*
UPD:
- 09:08:2026 - 00:18:26: Initial stage-1 id set (dialogue, voice, item,
                         schedule, loot table).
- 09:08:2026 - 14:12:02: Quest-grill sync: appended QuestId, FlagId, TopicId,
                         NpcCardId (QUEST_FORMAT.md contract, story-ACKed).
*/

#pragma once

#include <cstdint>

namespace dfn::gameplay {

// Identifies a dialogue line (Q71). Lines do not know who speaks them.
struct DialogueLineId {
    uint64_t value = 0;
    [[nodiscard]] bool valid() const { return value != 0; }
};

// Identifies a voice timbre (Q80). An NPC component carries it; it feeds the
// per-segment audio content hash (Q79).
struct VoiceId {
    uint64_t value = 0;
    [[nodiscard]] bool valid() const { return value != 0; }
};

// Identifies an item definition in content files.
struct ItemId {
    uint64_t value = 0;
    [[nodiscard]] bool valid() const { return value != 0; }
};

// Identifies a daily schedule definition in content files (Q44).
struct ScheduleId {
    uint64_t value = 0;
    [[nodiscard]] bool valid() const { return value != 0; }
};

// Identifies a loot table definition in content files (Q11 lootables).
struct LootTableId {
    uint64_t value = 0;
    [[nodiscard]] bool valid() const { return value != 0; }
};

// Identifies a quest definition (*.quest.json, QUEST_FORMAT.md §2).
struct QuestId {
    uint64_t value = 0;
    [[nodiscard]] bool valid() const { return value != 0; }
};

// Identifies a declared world flag (world_flags.json, QUEST_FORMAT.md §3).
struct FlagId {
    uint64_t value = 0;
    [[nodiscard]] bool valid() const { return value != 0; }
};

// Identifies a dialogue topic graph (*.dlg.json, QUEST_FORMAT.md §4).
struct TopicId {
    uint64_t value = 0;
    [[nodiscard]] bool valid() const { return value != 0; }
};

// Identifies an LLM character card (*.card.json, QUEST_FORMAT.md §6).
struct NpcCardId {
    uint64_t value = 0;
    [[nodiscard]] bool valid() const { return value != 0; }
};

} // namespace dfn::gameplay
