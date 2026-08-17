/*
Created: 17:08:2026 - 13:02:08
Last updated: 17:08:2026 - 13:02:08
Module: engine/render
File: engine/render/sources/PartForgeDetail.h

Responsibility:
- The kit's INTERNAL kitchen, shared by the part-forge translation units
  (PartForge.cpp, PartForgeJoints.cpp, PartForgeWalls.cpp): the material
  table, the grid helper, the tone/block shorthands and the cross-TU make_*
  declarations. Split out the day PartForge.cpp hit 999 lines against the
  800 hard limit (Rule 21) — cut by FAMILY, because a family is what gets
  read and grown together.

Key items:
- part_detail::material_of() / m_of() / tone() / block().
- make_joint / make_sleeper / make_log_corner (PartForgeJoints.cpp).
- make_wall_styled (PartForgeWalls.cpp).

Dependencies:
- Uses: PartForge.h, HewnBar.h, ProcMesh.h.
- Used by: PartForge.cpp, PartForgeJoints.cpp, PartForgeWalls.cpp. NOBODY
  else: this header is the forge's kitchen door, not a public contract.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ONE material table. A second copy of material_of in any TU is Rule 39's
  shadow-chain defect waiting for its branch.
*/
/*
UPD:
- 17:08:2026 - 13:02:08: Создан при разрезе PartForge.cpp по семьям (999 строк
  против предела 800).
*/

#pragma once

#include "engine/render/sources/HewnBar.h"
#include "engine/render/sources/PartForge.h"
#include "engine/render/sources/ProcMesh.h"

namespace dfn::render::part_detail {

using Material = HewnMaterial;
using Rng = HewnRng;

[[nodiscard]] inline float m_of(int units) {
    return static_cast<float>(units) * BUILD_GRID_M;
}

/// THE material table — one definition for every family TU (Rule 39).
/// Implemented in PartForge.cpp.
[[nodiscard]] Material material_of(PartMaterial m);

[[nodiscard]] inline uint32_t tone(const Material& mat, float wear, Rng& rng) {
    return hewn_tone(mat, wear, rng);
}

/// An axis-aligned block from its min corner (HewnBar's shorthand under the
/// name every make_* was written against).
inline void block(MeshData& m, glm::vec3 min, glm::vec3 size, const Material& mat,
                  float wear, Rng& rng, int segments = 1) {
    hewn_block(m, min, size, mat, wear, rng, segments);
}

// Kit dimensions shared across family TUs. Proportions of a part, set against
// the reference frames in images_examples/houses_outdoors.
inline constexpr float BOARD_W_M = 0.30f;      ///< one cladding board's width
inline constexpr float BOARD_GAP_M = 0.02f;    ///< the shadow line between two
inline constexpr float INFILL_THICK_M = 0.08f; ///< a board skin's depth
inline constexpr float WALL_CORE_M = 0.08f;    ///< sealed slab behind a boarded bay
inline constexpr float PANE_THICK_M = 0.05f;   ///< a window's blind insert

// The connector family (PartForgeJoints.cpp).
void make_joint(MeshData& m, const PartParams& p, const Material& mat, Rng& rng);
void make_sleeper(MeshData& m, const PartParams& p, const Material& mat, Rng& rng);
void make_log_corner(MeshData& m, const PartParams& p, const Material& mat, Rng& rng);

// The styled walls (PartForgeWalls.cpp): every WallPanel with variant != 0.
void make_wall_styled(MeshData& m, const PartParams& p, const Material& mat, Rng& rng);

} // namespace dfn::render::part_detail
