/*
Created: 14:08:2026 - 23:36:19
Last updated: 15:08:2026 - 00:24:00
Module: engine/render
File: engine/render/sources/TreeForge.h

Responsibility:
- THE FORGE: builds one tree the way the studied industry models are built
  (docs/TREE_MODELS_RESEARCH.md), for the OBJECT REGISTRY — not for the live
  frame. A forged tree is written to a .dfo once, offline, and the game only
  places it (в1). This is deliberately a NEW pipeline beside ProcFlora, not a
  patch on it: the user's ruling after the one-tree stand's first inspection
  was «это фиксы ломаного объекта, надо дерево принципиально по-новому».

Key items:
- TreeForgeParams / forge_tree(): params -> RegistryObject.

The architecture, each clause traceable to the research:
- CROWN = SOLID MASSES + BIG CARDS. An inner faceted core plus 6-9 satellite
  masses carry 60-90 % of the triangles (§1.7: the measured open models spend
  87-91 % on leaf, 9-13 % on wood — ours spent 17-24 % on leaf, which IS the
  «наждачка»), and 10-14 LARGE cluster cards (each ~half the crown radius,
  §1.1 SpeedTree clusters, §1.6 "fewer cards better") soften the rim.
- ONE LIGHT DOME. Card normals point FROM the crown centre (§1.3 Airborn's
  projected normals), and the masses carry a baked top-lit value gradient —
  the crown shades as one volume, not as confetti.
- BRANCHES GROW FROM THE DRAWN BOLE. Scaffolds start embedded in the trunk
  surface with a thickened base (§1.5's inflate-at-the-joint), curve upward,
  and END INSIDE the crown masses — no bare hooks above the foliage (the
  user's «витые палки» die by construction, not by pruning).
- BOLE NEAR-VERTICAL. Zero-mean Weber wander only (§1.8: Curve = 0 for every
  species in the paper; measured boles deviate 5-13°) — no arc.

Dependencies:
- Uses: ObjectRegistry.h (RegistryObject), FloraBuild.h (tube primitives),
  FloraCards.h (card emitter, tones).
- Used by: tools/forge_trees.cpp, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- PURE AND DETERMINISTIC: same params, same bytes, same content hash. The
  registry depends on it.
- This builder answers to the RESEARCH TABLE (§2), not to ProcFlora's
  conventions. When the two disagree, the measured models win.
*/
/*
UPD:
- 14:08:2026 - 23:36:19: Created — the forge, first cut: bole + embedded
  scaffolds + mass-and-card crown with projected normals.
- 15:08:2026 - 00:24:00: v2 ПО РЕФЕРЕНСАМ ПОЛЬЗОВАТЕЛЯ (Skyrim): вердикт по v1 — «мультяшный стиль,
  шарик сплошной». Сплошные массы заменены НАРИСОВАННЫМ ветвлением двух порядков
  с листовыми лапами НА ветвях: spray_frac/spray_per_branch/secondary_per_scaffold
  вместо mass_count/card_count; ядро ужато до тени (core_frac 0.24).
*/

#pragma once

#include "engine/render/sources/FloraCards.h"    // LeafTone
#include "engine/render/sources/ObjectRegistry.h"

#include <cstdint>
#include <glm/vec3.hpp>
#include <string>

namespace dfn::render {

/// Everything a forged tree is made FROM. All metres; the seed is the whole
/// source of variation — two calls with equal params are byte-identical.
struct TreeForgeParams {
    uint64_t seed = 1;
    std::string name = "tree";     ///< registry handle; goes to the .dfo
    float height = 16.0f;          ///< ground to crown top
    float crown_radius = 5.5f;     ///< horizontal crown half-extent
    float crown_base_frac = 0.35f; ///< of height: where the crown begins
    float trunk_radius = 0.42f;    ///< bole at breast height
    glm::vec3 bark{0.16f, 0.12f, 0.09f};
    LeafTone tone = LeafTone::OakMid;
    LeafShape card_shape = LeafShape::RoundLobed;
    int scaffold_count = 5;        ///< order-1 branches off the bole
    int secondary_per_scaffold = 4;///< order-2 branches per scaffold
    int spray_per_branch = 2;      ///< leafy spray cards per outer branch
    /// Spray card half-size as a fraction of the crown radius. The Skyrim
    /// reference the user ruled by: dozens of MEDIUM ragged sprays hanging on
    /// visible branches with sky between them — not a solid ball (v1's camp),
    /// not confetti (the old generator's camp).
    float spray_frac = 0.20f;
    /// Inner shadow core as a fraction of the crown radius. Small and DARK:
    /// it is the depth behind the sprays (Airborn's blob demoted to a shadow),
    /// not the crown itself. 0 disables.
    float core_frac = 0.24f;
};

/// Forges one tree. The returned object carries its content hash and is ready
/// for write_object().
[[nodiscard]] RegistryObject forge_tree(const TreeForgeParams& params);

} // namespace dfn::render
