/*
Created: 13:08:2026 - 16:05:00
Last updated: 13:08:2026 - 16:05:00
Module: engine/gameplay
File: engine/gameplay/sources/FloraCollision.h

Responsibility:
- Turns a scattered plant into the two things physics can use: the SOLID part
  of it (the bole, a fallen log) as triangles, and the part that only DRAGS
  (a bush, brushwood) as a disc. Both are MEASURED from the mesh flora draws,
  never recomputed from flora's tables.

Key items:
- FloraSolidKind: what a species is, physically — Solid / Drag / Nothing.
- FloraSolid: one cached, measured collider in the plant's own local space.
- FloraCollisionCache: the memo, owned by a World resource (Rule 9).
- flora_solid(): the accessor that measures on a miss and returns the memo.
- flora_collision_maturity(): the ONE maturity a collider is built at.

Dependencies:
- Uses: core math (ScatterSpecies), engine/render ProcFlora/ProcMesh (pure
  geometry builders, no GPU — the same permission PropCollision already uses).
- Used by: PropCollision (chunk bodies + the drag field), tests.

Notes:
- WHY MEASURED AND NOT COMPUTED. `species_trunk_radius()` is the flared base
  radius at the species' NOMINAL height, and every instance is drawn at its own
  height, so a cylinder built from it stands up to ~0.35 m proud of an oak's
  real bark: an invisible wall, which is worse than walking through the tree
  because nothing on screen explains the stop. The fix is not a better formula
  — a second formula is a shadow copy and drifts the day flora re-authors the
  first (Rule 39). We take the wood mesh flora actually builds and keep the
  triangles below the cut. What you bump into is then what you see, by
  construction, exactly as PropCollision does it for buildings and boulders.
- THE CUT is what keeps the crown non-physical (design: "crowns never get
  collision, ever") and what keeps this affordable. Everything below
  min(TRUNK_COLLISION_HEIGHT, the instance's crown base) is bole; above it,
  branches. TrunkTests asserts no kept triangle is wider than the bole, so
  "the cut leaves only bole" is refutable rather than asserted.
- A BUSH IS NOT A BODY. It has no rigid part to speak of and forty of them per
  hectare would cost forty bodies for the sensation of "slightly harder to walk
  here". It is a DRAG DISC read by the movement code through the same speed
  factor wading already uses — one mechanism, two mediums.
- MATURITY IS BUCKETED, DOWNWARD (FLORA_COLLISION_MATURITY_STEP). The memo is
  O(species x variants x buckets) and never O(instances) — flora's own cost
  rule. Rounding DOWN means a collider is at worst a couple of centimetres
  THINNER than the tree drawn over it, which is the harmless direction: you may
  brush the bark, never a wall short of it.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Nothing here may restate a flora table or formula. If a number is needed,
  measure the mesh or call flora's own accessor.
- Deterministic: an ordered map, no hashing, no wall clock (Rule 13.2).
*/
/*
UPD:
- 13:08:2026 - 16:05:00: Created — trunks become solid and brush becomes drag
                         (user: "деревья — не объекты физики… кусты пропускают
                         героя").
*/

#pragma once

#include <cstdint>
#include <map>

#include "engine/core/math/sources/SurfaceField.h"
#include "engine/render/sources/ProcMesh.h"

namespace dfn::gameplay {

// What a scattered plant IS to physics. Three answers, and the third is a real
// answer: a mushroom stops nothing and slows nothing.
enum class FloraSolidKind : uint8_t {
    None = 0,  // ground cover: no collision, no drag
    Solid = 1, // a bole or a downed trunk: triangles you cannot walk through
    Drag = 2,  // a bush or brushwood: a disc that slows you down
};

// One measured collider, in the plant's LOCAL space (origin at the stem base,
// +Y up, unrotated). The instance's yaw/position are applied at append time.
struct FloraSolid {
    FloraSolidKind kind = FloraSolidKind::None;
    // Solid: the kept wood triangles. Empty for every other kind.
    render::MeshData mesh;
    // Solid: the highest point the kept triangles reach, meters above the
    // stem base. Used to decide whether a downed log is a step or an obstacle.
    float top = 0.0f;
    // Solid: the widest horizontal reach of the kept triangles from the stem
    // axis. This is the number that would be an invisible wall if it were
    // wrong, so it is measured and asserted rather than trusted.
    float max_radius = 0.0f;
    // Drag: the disc that slows a walker, meters from the stem axis, and how
    // high the foliage stands. A drag volume whose top is under the player's
    // knee is not brush, it is litter, and litter does not slow anybody.
    float drag_radius = 0.0f;
    float drag_top = 0.0f;
};

// The memo. Lives in a World resource so the systems that read it stay
// stateless (Rule 9) and a test can hand in an empty one. Ordered, never
// hashed: the build order of chunk bodies must be identical on every run.
struct FloraCollisionCache {
    std::map<uint64_t, FloraSolid> solids;
    // Diagnostics, because a cache whose miss rate is unknown is a budget you
    // cannot defend. Read by the trunk-budget probe and by tests.
    uint32_t misses = 0;
    uint32_t hits = 0;
};

// Maturity bucket step. Coarse enough that the memo stays small, fine enough
// that the error is under two centimetres of bark on the biggest oak.
inline constexpr float FLORA_COLLISION_MATURITY_STEP = 0.05f;

// How far up a standing tree is solid, meters above its base.
//
// PROPOSED NUMBERS ROW (`TRUNK_COLLISION_HEIGHT`), not yet approved — reported
// to the lead with the rest of today's numbers. It is a physics reach, not a
// look: the player capsule is PLAYER_CAPSULE_HEIGHT 1.8 m and stands on ground
// that tilts under a tree, so the solid part has to clear the tallest head the
// terrain can put beside the trunk. 4 m does, with margin, and it is also
// where the measurement stops being pure bole on the smallest sapling — so the
// cut and the crown base meet rather than fight.
inline constexpr float TRUNK_COLLISION_HEIGHT = 4.0f;

// What this species is, physically, before anything is measured. Exposed so a
// caller can drop the classes that cost nothing BEFORE paying for a variant
// lookup and a maturity draw: on a real chunk 14 425 of 19 363 scatter
// instances are boulders and ground cover, and asking flora about each of them
// was three quarters of this pass.
[[nodiscard]] FloraSolidKind flora_solid_kind(math::ScatterSpecies species);

// The maturity a collider for this instance is built at: flora's own draw,
// rounded DOWN to a bucket. `instance_scale` is ScatterInstance::scale, which
// is what flora uses for everything that is not a canopy tree.
[[nodiscard]] float flora_collision_maturity(math::ScatterSpecies species,
                                             glm::vec2 world_xz, float instance_scale);

// The measured collider for one (species, variant, bucketed maturity).
// Measures on a miss, returns the memo on a hit. Never null: an unhandled
// species answers FloraSolidKind::None, which is a decision, not a failure.
[[nodiscard]] const FloraSolid& flora_solid(FloraCollisionCache& cache,
                                            math::ScatterSpecies species, uint32_t variant,
                                            float maturity);

} // namespace dfn::gameplay
