/*
Created: 09:08:2026 - 14:12:02
Last updated: 09:08:2026 - 14:12:02
Module: engine/gameplay
File: engine/gameplay/sources/Condition.h

Responsibility:
- The closed condition-atom vocabulary shared by dialogue lines, dialogue-graph
  choices, and quest transitions (QUEST_FORMAT.md §2.1, blessed at the Rule 26
  sync). ONE evaluator serves all three consumers.

Key items:
- ConditionKind: the closed atom set (extension = group sync, like NpcAction).
- ConditionOp: the six comparisons (booleans compare as 0/1).
- ConditionAtom: one plain-data condition; ConditionGroup: AND + optional OR.

Dependencies:
- Uses: C++ stdlib only.
- Used by: Dialogue.h (line conditions), the dialogue graph runner and quest
  state machine (stage 4 / act 1), tools validation (story's checker).

Notes:
- Semantics record (sync, sim<->story, both sides ACKed):
  * Same-tick order: dialogue runner commits -> quest transitions evaluate ->
    entered-state effects run in author order; cross-quest cascades resolve on
    the NEXT tick (bounded, deterministic — Rule 13.2).
  * DialogExit / EnteredLocation are EDGE-TRIGGERED: true only on the tick the
    event fired (latched per-tick fact set, never save-state). A quest not yet
    started when an exit fires misses it — gate on a flag instead (validator
    warns).
  * NpcDead is an auto-flag set forever by the death path (chunk-resident NPCs
    despawn; corpses are never queried); persisted in the save delta.
  * HasItem is shape-frozen now, runtime lands with inventory; Clock is
    validator-rejected until the day cycle lands (в1-в2, app-side).
- subject/detail carry stable 64-bit content-id hashes (core FNV-1a), per-kind:
  Flag: subject = FlagId            | Var: subject = var-name hash
  QuestState: subject = QuestId, detail = state-id hash (or terminal sentinel)
  DialogExit: subject = exit id     | HasItem: subject = ItemId
  EnteredLocation: subject = trigger/POI id
  NpcDead: subject = npc id         | Clock: subject = day-phase id

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The set is CLOSED: new kinds/ops only via a group sync (Rule 26), recorded
  in QUEST_FORMAT.md and here in the same change.
- Keep atoms plain serializable data (Rule 8); no pointers, no strings.
*/
/*
UPD:
- 09:08:2026 - 14:12:02: Created at the quest-grill sync: closed atom set per
  QUEST_FORMAT.md §2.1 with sim's pinned semantics, story ACK on record.
*/

#pragma once

#include <cstdint>
#include <vector>

namespace dfn::gameplay {

// The closed atom set (QUEST_FORMAT.md §2.1). Extension = group sync.
enum class ConditionKind : uint8_t {
    Flag,            // world-flag registry (в11)
    Var,             // quest-local variable
    QuestState,      // another quest's state or terminal status
    DialogExit,      // EDGE: dialogue exit fired this tick
    HasItem,         // inventory count; runtime lands with inventory
    EnteredLocation, // EDGE: trigger volume entered this tick
    NpcDead,         // auto-flag, true forever once the death path fires
    Clock,           // day-phase; validator-rejected until the day cycle lands
};

// The six comparisons; booleans compare as 0/1 (old HasFlag/NotHasFlag are
// Equals/NotEquals with value 1).
enum class ConditionOp : uint8_t {
    Equals,
    NotEquals,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
};

// One condition (plain data, Rule 8). subject/detail semantics per kind are in
// the header notes; all ids are stable 64-bit content hashes.
struct ConditionAtom {
    ConditionKind kind = ConditionKind::Flag;
    ConditionOp op = ConditionOp::Equals;
    uint64_t subject = 0;
    uint64_t detail = 0;
    int64_t value = 0; // bools as 0/1
};

// Quest transition `when` = all_of (AND); the schema's `any` wrapper = any_of
// (OR of its atoms, ANDed with all_of). Lines and dialogue choices use a flat
// std::vector<ConditionAtom> (pure AND) instead of a group.
struct ConditionGroup {
    std::vector<ConditionAtom> all_of;
    std::vector<ConditionAtom> any_of; // empty => ignored
};

} // namespace dfn::gameplay
