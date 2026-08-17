/*
Created: 17:08:2026 - 13:02:08
Last updated: 17:08:2026 - 15:46:07
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
- Used by: PartForge.cpp, PartForgeJoints.cpp, PartForgeWalls.cpp,
  PartForgeRoofs.cpp, SignForge.cpp — THE KIT'S OWN FAMILY and nobody else:
  this header is the forge's kitchen door, not a public contract. A sign is a
  kit part (same materials, same atlas rows, same bars), so it eats in the
  same kitchen rather than keeping a second copy of the material table.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ONE material table. A second copy of material_of in any TU is Rule 39's
  shadow-chain defect waiting for its branch.
*/
/*
UPD:
- 17:08:2026 - 13:02:08: Создан при разрезе PartForge.cpp по семьям (999 строк
  против предела 800).
- 17:08:2026 - 13:24:19: объявления семьи крыш (make_roof/make_roof_hip/make_smoke_vent
  в PartForgeRoofs.cpp).
- 17:08:2026 - 14:29:43: material_of получил wear: ряд атласа — это тон И износ вместе
  (PartsAtlas.h), выветренный брус не «тот же, но темнее», а другая
  поверхность. skin_as_board — доска против бруса решается ДЕТАЛЬЮ, а не
  материалом: доска и стойка — один и тот же дуб.
- 17:08:2026 - 14:46:25: кухня приняла ещё одного едока — SignForge.cpp: табличка это
  деталь набора (те же материалы, те же ряды атласа, те же бруски), и вторая
  копия таблицы материалов ради неё была бы правилом 39 в чистом виде.
- 17:08:2026 - 15:46:07: текстурность стала свойством ДЕТАЛИ (kit_textured_default() — одно
  определение умолчания). Была процессная дверь, читаемая внутри кузницы, и
  тест текстурного потока не мог её попросить: он проверял умолчание и
  покраснел в день, когда умолчание сменилось. Полка байт в байт прежняя.
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
///
/// `wear` is here because the ATLAS ROW is the tone AND the wear together
/// (PartsAtlas.h): a weathered oak beam is not a darker fresh one, it is a
/// greyed, checked, lichened surface, and that is a different tile. The
/// default keeps the untextured call sites honest rather than silently fresh.
[[nodiscard]] Material material_of(PartMaterial m, float wear, bool textured);

/// The same material as SAWN BOARD rather than hewn timber: planks, door
/// leaves, cladding skins, shingles. The kit tells hewn from sawn by the PART,
/// not by the material — a board and a beam are the same oak.
inline void skin_as_board(Material& mat) {
    mat.skin.side = PartSurface::SawnBoard;
}

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

// The roof family (PartForgeRoofs.cpp): the pitched slope in five coverings,
// the hip slope (variant 1 = полувальма) and the ridge smoke vent.
void make_roof(MeshData& m, const PartParams& p, const Material& mat, Rng& rng);
void make_roof_hip(MeshData& m, const PartParams& p, const Material& mat, Rng& rng);
void make_smoke_vent(MeshData& m, const PartParams& p, const Material& mat, Rng& rng);

} // namespace dfn::render::part_detail
