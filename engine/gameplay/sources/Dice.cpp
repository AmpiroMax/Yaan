/*
Module: engine/gameplay
File: engine/gameplay/sources/Dice.cpp

Responsibility:
- Deterministic dice RNG implementation (Rule 13.2): splitmix64 advance,
  bias-free-enough die rolls via 32-bit multiply-shift, percent checks.

Key items:
- make_rng / roll_die / roll_dice / percent_check.
- resolve_attack is deliberately NOT here yet — formulas land after the combat
  grill (stage-1 spec, open question 2).

Dependencies:
- Uses: Dice.h only.
- Used by: combat/loot/quest systems (later), tests.

Notes:
- Algorithm FROZEN once saves ship (Rng state is save-delta data): splitmix64 —
  state += golden gamma; mix via xor-shift-multiply (Steele et al. 2014
  constants). Die mapping: x * sides >> 32 over the high 32 mix bits; for
  gameplay-sized dice the deviation from uniform is < 2^-27 per face —
  documented as acceptable, exact across platforms.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NEVER change the advance/mix/mapping once a save has shipped (determinism
  and replays depend on it, Rule 13.2).
*/

#include "engine/gameplay/sources/Dice.h"

namespace dfn::gameplay {

namespace {

// splitmix64 (public-domain constants). Advances state, returns the mix.
uint64_t next_u64(Rng& rng) {
    rng.state += 0x9E3779B97F4A7C15ull;
    uint64_t z = rng.state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

} // namespace

Rng make_rng(uint64_t seed) {
    return Rng{seed};
}

uint32_t roll_die(Rng& rng, uint32_t sides) {
    if (sides <= 1) {
        return sides; // 0-sided: degenerate 0; 1-sided: always 1
    }
    // Multiply-shift map of the high mix bits onto [0, sides).
    const uint32_t x = static_cast<uint32_t>(next_u64(rng) >> 32);
    return 1u + static_cast<uint32_t>((static_cast<uint64_t>(x) * sides) >> 32);
}

uint32_t roll_dice(Rng& rng, uint32_t count, uint32_t sides) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < count; ++i) {
        sum += roll_die(rng, sides);
    }
    return sum;
}

bool percent_check(Rng& rng, uint32_t chance_percent) {
    if (chance_percent == 0) {
        return false;
    }
    if (chance_percent >= 100) {
        return true;
    }
    return roll_die(rng, 100) <= chance_percent;
}

} // namespace dfn::gameplay
