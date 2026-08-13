/*
Created: 11:08:2026 - 14:31:10
Last updated: 13:08:2026 - 16:35:00
Module: engine/world
File: engine/world/sources/WorldgenRelief.h

Responsibility:
- LANDSCAPE §2.7's GENERAL ground relief, the pass that section has been
  waiting for: the meso octave (25-60 m / 1.5-4 m) and the micro octave
  (8-16 m / 0.3-0.6 m) applied to ALL ground, with the two masks the ruling
  requires — amplitude tapering to zero across the shore band and across
  corridor masks. This is what §10.1's `GROUND_RELIEF_SIGMA_20M` measures.

Key items:
- ground_meso_relief(): the missing middle octave. Until now GROUND_MESO_* had
  ZERO consumers in the engine.
- ground_relief(): meso + micro + the masks. THE one general relief term.

Dependencies:
- Uses: WorldgenMacro.h (ground_micro_relief, streams), TestbedLayout.h, config.
- Used by: Worldgen.cpp (compose_passes — the single application site).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- IT IS APPLIED IN EXACTLY ONE PLACE (compose_passes). The previous attempt at
  this pass was backed out because it went in inside the massif step, where it
  moved the shoreline; the fix is not a smaller amplitude, it is applying it
  after the water carve and letting the shore mask do its job (§2.7's own
  ruling — ground beside water is flat BECAUSE water flattens it).
- THE MASSIF IS NOT GENERAL GROUND. §2.8 owns that surface completely — the
  contour bands, the benches, the summit tor and the bench micro-relief are one
  designed language with its own invariants (I2/I4/I5/I7/I8), and the crag
  tunnel's portals are cut from it. This pass fades to zero across the stamp
  and does not touch it. Measured when it did: the tunnel's uphill portal
  closed (sim_tunnel_walk lost its mouth) and the massif's own micro octave
  summed with this one.
- THE MASKS ARE THE CONTRACT, not a workaround. Everything they exempt is
  ground that an approved rule flattens on purpose, and §10.1.2 exempts exactly
  the same list from the σ floor. If the two lists ever disagree, the floor is
  being measured on ground the generator was told to keep flat.
*/
/*
UPD:
- 11:08:2026 - 14:31:10: Created — §2.7's general pass, §10.1's subject.
- 13:08:2026 - 16:35:00: relief_mask() and ground_relief_tiers() published —
  the mask gained a second consumer (§10.1.3's bench/riser operator inherits the
  same exemption list) and the tiers had to be split because that operator sits
  BETWEEN them.
*/

#pragma once

#include "engine/world/sources/TestbedLayout.h"

#include <cstdint>
#include <glm/vec2.hpp>

namespace dfn::world {

/// §2.7 THE MESO OCTAVE (meters, signed): dips, rises and hollows you walk into
/// and out of — the band between the 128 m hill octave and the 8-16 m micro
/// octave. Two octaves at the ruled GROUND_MESO_WAVELENGTH bounds, amplitude
/// drifting between the ruled GROUND_MESO_AMPLITUDE bounds on a slow field so
/// the roughness itself is not constant.
///
/// Same construction and the same amplitude convention as ground_micro_relief:
/// a tier of one rule must not read its own bounds differently from the tier
/// beside it, or the two stop being comparable and no single σ can describe
/// them.
[[nodiscard]] float ground_meso_relief(uint64_t seed, glm::vec2 world);

/// THE GENERAL GROUND RELIEF (meters, signed) at `world`: meso + micro, scaled
/// by the §2.7 masks.
///
/// `dist_to_water` is the hydrology field's own distance to the nearest water
/// EDGE (0 inside water), which is why this is applied after the carve and not
/// inside the macro step. No new constant: the taper reuses the shore mask that
/// §4 and §5 already consume.
///
/// The corridor mask is the §2.4 clause, not a new rule: a corridor is graded
/// ground, its slope budget is validated, and dropping 1-4 m of hollows into it
/// would spend a traversability invariant on a cosmetic win.
/// `meso_scale` is the caller's extra taper on the MESO tier only (micro is
/// retained — §2.7's "flat, not sterile"). The forest stand passes в9's glade
/// factor through it, so the ONE authored calm plain stays calm.
[[nodiscard]] float ground_relief(uint64_t seed, const TestbedLayout& layout, glm::vec2 world,
                                  float dist_to_water, float meso_scale = 1.0f);

/// THE §2.7 MASK, published because it has a SECOND consumer and a second copy
/// of it would be a second opinion about which ground an approved rule keeps
/// flat (Rule 32). shore x corridor x massif, in [0, 1]; ground_relief() is
/// this multiplied onto its own two octaves, and §10.1.3's bench/riser
/// operator (WorldgenForms.h) inherits exactly the same exemption list —
/// terraces ARE ground relief, so a place the corridor rule grades flat must
/// not grow a scarp across it.
[[nodiscard]] float relief_mask(const TestbedLayout& layout, glm::vec2 world,
                                float dist_to_water);

/// The general relief SPLIT INTO ITS TIERS, because the bench/riser operator
/// sits BETWEEN them: it reshapes the meso tier (and the macro under it) into
/// flats and steps, and the micro tier is added on top of the result. Folding
/// micro in first would iron §2.7's "flat, not sterile" clause into the
/// benches — the operator multiplies a bench's gradient by (1 - strength), and
/// that would apply to the 8-16 m octave as much as to the 40 m one.
struct ReliefTiers {
    float mask = 0.0f;  ///< relief_mask() at this position
    float meso = 0.0f;  ///< §2.7 meso tier, already scaled by meso_scale, PRE-mask
    float micro = 0.0f; ///< §2.7 micro tier, PRE-mask
};
[[nodiscard]] ReliefTiers ground_relief_tiers(uint64_t seed, const TestbedLayout& layout,
                                              glm::vec2 world, float dist_to_water,
                                              float meso_scale = 1.0f);

} // namespace dfn::world
