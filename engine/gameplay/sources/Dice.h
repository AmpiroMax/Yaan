/*
Created: 09:08:2026 - 00:18:26
Last updated: 09:08:2026 - 00:18:26
Module: engine/gameplay
File: engine/gameplay/sources/Dice.h

Responsibility:
- Seedable deterministic dice RNG and the dice-roll combat resolution API
  (Q10): every hit is a roll against stats. Formulas' constants live in
  NUMBERS.md (combat grill pending), never here (Rule 14).

Key items:
- Rng: plain 64-bit state — seedable, serializable, deterministic (Rule 13.2).
- make_rng / roll_die / roll_dice / percent_check: the dice vocabulary.
- AttackInput / AttackOutcome / resolve_attack: combat resolution.

Dependencies:
- Uses: C++ stdlib only.
- Used by: NpcAction executor (Attack), player combat system, loot rolls,
  quest checks, tests.

Notes:
- Determinism (Rule 13.2): Rng is plain state advanced only by explicit calls;
  the simulation's combat Rng is seeded from the save and serialized in the
  gameplay save section, so a replay rolls the same dice. Never mix in
  wall-clock time or unordered iteration. Algorithm (stage 2): splitmix64
  advance — statistically fine for dice, trivial to port, frozen once saves
  ship. Cosmetic randomness (audio variation) must NOT use the simulation Rng.
- resolve_attack() is a pure function of (input, rng): no ECS access, no
  interface calls — trivially unit-testable with fixed seeds. The executor and
  player combat system gather inputs from components and apply outcomes.
- Exact hit/damage formulas are decided at the combat grill; the shapes below
  are the frozen contract (inputs are stat values, outputs are hit/crit/damage
  plus the raw roll for combat-log UI).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Never hardcode combat constants here (Rule 14); formulas read generated
  NUMBERS constants.
- Never reseed or advance a serialized Rng outside the simulation tick.
*/
/*
UPD:
- 09:08:2026 - 00:18:26: Initial stage-1 contract (seedable RNG, dice
                         vocabulary, attack resolution shapes).
*/

#pragma once

#include <cstdint>

namespace dfn::gameplay {

// Deterministic dice RNG (Rule 13.2). Plain state: copyable, serializable in
// the gameplay save section, comparable in tests.
struct Rng {
    uint64_t state = 0;
};

[[nodiscard]] Rng make_rng(uint64_t seed);

// One die: uniform 1..sides (sides >= 1).
[[nodiscard]] uint32_t roll_die(Rng& rng, uint32_t sides);

// Sum of `count` dice of `sides` (classic XdY).
[[nodiscard]] uint32_t roll_dice(Rng& rng, uint32_t count, uint32_t sides);

// True with probability chance_percent/100 (0 = never, 100 = always).
[[nodiscard]] bool percent_check(Rng& rng, uint32_t chance_percent);

// --- Combat resolution (Q10) -------------------------------------------------

// Stat snapshot for one swing. Callers copy values out of Attributes/Skills
// components; this struct never references ECS state.
struct AttackInput {
    uint16_t weapon_skill = 0;      // attacker's skill with the equipped weapon
    uint16_t attacker_agility = 0;
    uint16_t attacker_strength = 0;
    uint16_t attacker_luck = 0;
    uint16_t defender_dodge = 0;    // defender's evasion-relevant skill value
    uint16_t defender_armor = 0;    // flat armor rating
    uint8_t damage_dice_count = 0;  // weapon damage = count d sides + bonus
    uint8_t damage_dice_sides = 0;
    int16_t damage_bonus = 0;
};

struct AttackOutcome {
    bool hit = false;
    bool critical = false;
    int32_t damage = 0;    // final damage after armor; >= 0
    uint32_t hit_roll = 0; // raw to-hit roll, for combat-log UI
};

// Pure function of (input, rng); formulas per NUMBERS (combat grill).
[[nodiscard]] AttackOutcome resolve_attack(const AttackInput& input, Rng& rng);

} // namespace dfn::gameplay
