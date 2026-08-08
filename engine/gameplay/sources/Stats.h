/*
Created: 09:08:2026 - 00:18:26
Last updated: 09:08:2026 - 00:18:26
Module: engine/gameplay
File: engine/gameplay/sources/Stats.h

Responsibility:
- Attribute and skill types (Q42): the 8 Daggerfall attributes, the skill set,
  and the use-based progression hooks. Numeric ranges/thresholds live in
  NUMBERS.md, never here (Rule 14).

Key items:
- Attribute (8, Daggerfall set) / Skill (~12, placeholder until the combat grill).
- Attributes / Skills: plain-data components (Rule 8), save-delta state.
- value() / record_use(): typed access and the use-based progression hook.
- SkillRaised: EventBus event published on a use-threshold level-up.

Dependencies:
- Uses: engine/core/ecs (EntityId in the event), C++ stdlib.
- Used by: dice combat (Dice.h), NpcAction executor, quests, UI, save delta.

Notes:
- The Skill list is a PLACEHOLDER agreed to hold the contract shape: the final
  ~12 names arrive at the combat grill (NUMBERS.md SKILL_COUNT). Enum COUNT
  sentinels size the arrays; stage 2 adds static_asserts binding COUNT to the
  constants generated from NUMBERS.md so code and document cannot diverge.
- Use-based progression (Q42): systems call record_use() when a skill is
  exercised (hit landed, lock picked, deal closed). Thresholds and growth
  formulas come from generated NUMBERS constants; the progression system
  publishes SkillRaised. Direct writes to values outside progression code are a
  violation (same spirit as Rule 15).
- Components are save-delta state (uses counters included) via the gameplay
  save section (world::SaveDeltaCodec, agreed with core, stage-1 sync).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Never hardcode stat ranges, growth rates, or thresholds here (Rule 14).
- Enum reordering breaks saves (indices are serialized); append only.
*/
/*
UPD:
- 09:08:2026 - 00:18:26: Initial stage-1 contract (attributes, placeholder
                         skills, use-based progression hooks).
*/

#pragma once

#include <array>
#include <cstdint>

#include "engine/core/ecs/sources/EntityId.h"

namespace dfn::gameplay {

// The 8 Daggerfall attributes (Q42, ATTRIBUTE_COUNT in NUMBERS.md).
enum class Attribute : uint8_t {
    Strength,
    Intelligence,
    Willpower,
    Agility,
    Endurance,
    Personality,
    Speed,
    Luck,
    COUNT,
};

// ~12 skills (Q42). PLACEHOLDER SET — final names at the combat grill; COUNT
// must equal SKILL_COUNT in NUMBERS.md (static_assert lands in stage 2).
enum class Skill : uint8_t {
    LongBlade,
    ShortBlade,
    Axe,
    BluntWeapon,
    Archery,
    Block,
    Dodging,
    Stealth,
    Lockpicking,
    Mercantile,
    Speechcraft,
    Restoration,
    COUNT,
};

// --- Components (Rule 8: plain data) -----------------------------------------

struct Attributes {
    std::array<uint16_t, static_cast<size_t>(Attribute::COUNT)> values{};
};

struct Skills {
    std::array<uint16_t, static_cast<size_t>(Skill::COUNT)> values{};
    // Use counters driving use-based progression (Q42); save-delta state.
    std::array<uint32_t, static_cast<size_t>(Skill::COUNT)> uses{};
};

// --- Typed access ------------------------------------------------------------

[[nodiscard]] uint16_t value(const Attributes& attributes, Attribute attribute);
[[nodiscard]] uint16_t value(const Skills& skills, Skill skill);

// --- Use-based progression hook (Q42) ----------------------------------------

// Records `times` uses of a skill. Level-ups happen inside the progression
// system against NUMBERS thresholds; callers only report usage.
void record_use(Skills& skills, Skill skill, uint32_t times = 1);

// Published on the EventBus when accumulated uses raise a skill.
struct SkillRaised {
    ecs::EntityId entity{};
    Skill skill = Skill::COUNT;
    uint16_t new_value = 0;
};

} // namespace dfn::gameplay
