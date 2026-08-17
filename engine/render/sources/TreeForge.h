/*
Created: 14:08:2026 - 23:36:19
Last updated: 17:08:2026 - 07:04:26
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
- 15:08:2026 - 00:45:20: v3 по вердикту: core_frac по умолчанию 0 («всё ещё шарик в центре»),
  режим conifer (ёлки: мутовки, провис, конус) — другая ГРАММАТИКА, не
  перекрученный лиственный; spray_frac 0.26.
- 15:08:2026 - 16:17:07: frond_width: ширина ленты фронда долей от еловой — лиственница носит ту же грамматику тоньше (0.55).
- 16:08:2026 - 22:48:45: Кузница мелкой флоры: forge_bush (стволики от земли, крона до земли — куст без болы), forge_fallen_log (лежащий ствол с рваным сломом и корневой ПЛИТОЙ — правило 52), forge_ground_prop (пучок травы из гнутых лезвий / цветы с лепестками вершинного цвета / грибы объёмом ножка+шляпка). Этап «полянка» пользователя, 16.08.
- 17:08:2026 - 02:31:45: Кусты-ВИДЫ: berry_count/berry_r/berry (ягоды — крупные закрытые объёмы, «должно быть видно»), creeping (стелющийся можжевельник); GroundPropKind::Fern (веер гнутых перьев).
- 17:08:2026 - 02:51:54: Ягодное РАЗНООБРАЗИЕ (лист 4, ответ №5): BerryStyle (Balls/Cluster/Drops), berry_pattern крапинка/полоски + berry_spot, berry_sepal (попка), berry_stalk (веточка). Все оси — закрытая геометрия: рисунок краской умирает на плоде 4-7 см.
- 17:08:2026 - 03:51:22: Кузница света троп (этап 2): PathLightKind TorchStake/LanternPost, forge_path_light — геометрия и яркие вершинные цвета пламени/стекла; сама точка света — секция [light] лида.
- 17:08:2026 - 07:04:26: GroundPropParams.bloom — сказочный размер венчика (утро 17.08: «цветов не вижу... пусть крупные будут, зато играбельно»).
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
    float spray_frac = 0.26f;
    /// Inner shadow core as a fraction of the crown radius. 0 (the default)
    /// disables — the user's v2 verdict: «всё ещё какой-то шарик в центре».
    /// Depth now comes from spray density, not from a ball behind them.
    float core_frac = 0.0f;
    /// CONIFER MODE (ёлки): the crown is whorls of drooping branches down a
    /// cone, needle sprays along them — a different GRAMMAR, not a re-tuned
    /// broadleaf, exactly as Weber's own pine differs from his oak.
    bool conifer = false;
    int whorl_count = 9;          ///< branch rings down the cone
    int whorl_branches = 6;       ///< branches per ring
    float droop = 0.28f;          ///< rad, how far below horizontal a branch sags
    /// Frond ribbon width as a fraction of the default. 1.0 is the spruce's
    /// full lapa; a LARCH (passports queue: light feathery needles, a crown
    /// you can see the sky through) runs ~0.55 — same grammar, thinner dress.
    float frond_width = 1.0f;
};

/// Forges one tree. The returned object carries its content hash and is ready
/// for write_object().
[[nodiscard]] RegistryObject forge_tree(const TreeForgeParams& params);

/// How a bush wears its fruit. Balls — single near-spheres (currant-large);
/// Cluster — a rowan-style hanging bunch of smaller fruits on one stalk;
/// Drops — elongated fruit with a nub tip.
enum class BerryStyle : uint8_t { Balls = 0, Cluster = 1, Drops = 2 };

/// A BUSH has no bole (rule 52 + the zone brief): several stems leave the
/// GROUND, arc outward, and carry curved leaf sheets down to the grass.
struct BushForgeParams {
    uint64_t seed = 1;
    std::string name = "bush";
    float height = 1.4f;         ///< tallest stem tip
    float radius = 1.1f;         ///< footprint half-extent
    int stems = 5;
    glm::vec3 bark{0.24f, 0.18f, 0.12f};
    LeafTone tone = LeafTone::OakMid;
    LeafShape card_shape = LeafShape::RoundLobed;
    /// BERRIES (user, 17.08: «ягоды должны быть! крупными и их должно быть
    /// видно»): closed bipyramid volumes hung at the sheet rims. berry_count
    /// 0 disables; berry_r is the fruit's half-height in metres — 0.05-0.07
    /// reads at a glance, which is the point.
    int berry_count = 0;
    float berry_r = 0.06f;
    glm::vec3 berry{0.75f, 0.12f, 0.10f};
    /// VARIETY AXES (user's answer №5 on sheet 4: «где-то шарики, где-то
    /// грозди... с разными рисунками... с разными попками и веточками»).
    /// Every axis is honest closed geometry, because a 4-7 cm fruit must
    /// read from the path — vertex paint alone would melt at that size.
    BerryStyle berry_style = BerryStyle::Balls;
    /// 0 none; 1 крапинка (three raised specks of berry_spot on the flank);
    /// 2 полоски (two vertical ribs of berry_spot pole to pole).
    int berry_pattern = 0;
    glm::vec3 berry_spot{0.9f, 0.88f, 0.8f};
    bool berry_sepal = false;   ///< попка: a tiny calyx cone under the fruit
    float berry_stalk = 0.5f;   ///< веточка: stalk length as a multiple of berry_r
    /// Creeping habit (можжевельник): stems hug the ground instead of rising.
    bool creeping = false;
};
[[nodiscard]] RegistryObject forge_bush(const BushForgeParams& params);

/// A FALLEN LOG: the trunk chain lying along +X with a ragged broken end and
/// a root plate (flare + radial spurs) standing at the butt — the pine_forest
/// photoscans' dead_tree_trunk, as a registry object.
struct LogForgeParams {
    uint64_t seed = 1;
    std::string name = "log";
    float length = 7.0f;
    float radius = 0.45f;
    bool mossy = true;
};
[[nodiscard]] RegistryObject forge_fallen_log(const LogForgeParams& params);

/// Small ground flora, one object per clump: a tall-grass tuft (folded
/// blades), a flower cluster (stems + petal crowns, petals vertex-coloured),
/// or a mushroom family (stem + cap volumes, rule 52).
enum class GroundPropKind : uint8_t { GrassTuft = 0, Flowers = 1, Mushrooms = 2,
                                      Fern = 3 };
struct GroundPropParams {
    uint64_t seed = 1;
    std::string name = "prop";
    GroundPropKind kind = GroundPropKind::GrassTuft;
    float height = 0.8f;              ///< tuft/stem height, cap height for mushrooms
    glm::vec3 accent{0.85f, 0.82f, 0.9f}; ///< petal / cap colour
    /// Petal length in metres (Flowers only). The user's ruling, 17.08: «пусть
    /// крупные будут, не важно что не как в реале, зато играбельно» — a bloom
    /// must read from the path, so the default is already storybook-sized.
    float bloom = 0.13f;
};
[[nodiscard]] RegistryObject forge_ground_prop(const GroundPropParams& params);

/// PATH LIGHTS (stage-2 of the glade, user: «фонари и факела поставить
/// аккуратные, не те, что сейчас есть, надо новых сделать»). Geometry only —
/// the actual light point is the lead's [light] scene section; the flame and
/// the glass are bright vertex colour so an UNLIT lamp still reads at dusk.
enum class PathLightKind : uint8_t { TorchStake = 0, LanternPost = 1 };
struct PathLightParams {
    uint64_t seed = 1;
    std::string name = "torch";
    PathLightKind kind = PathLightKind::TorchStake;
    float height = 2.2f;              ///< top of the flame / lantern hook
    glm::vec3 wood{0.15f, 0.11f, 0.07f};
    glm::vec3 iron{0.10f, 0.10f, 0.11f};
    glm::vec3 flame{1.0f, 0.62f, 0.18f};
};
[[nodiscard]] RegistryObject forge_path_light(const PathLightParams& params);

} // namespace dfn::render
