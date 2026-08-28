/*
Created: 28:08:2026 - 16:50:00
Last updated: 28:08:2026 - 16:50:00
Module: engine/render
File: engine/render/sources/TreeForgeV2.h

Responsibility:
- THE SECOND ITERATION OF THE TREE, as a builder BESIDE the first one. Owner's
  order of 28.08.2026: «снять разницу с Готикой 3 и сделать вторую итерацию, НЕ
  ПОРТЯ текущих деревьев». The six differences are written down in
  docs/reports/trees-g3/index.html; five of them are geometry and atlas work and
  live here (the sixth, undergrowth tiers, is placement and belongs to Flora-2).

Key items:
- TreeHabit, TreeV2Params, forge_tree_v2().

WHY A SECOND BUILDER AND NOT A FLAG ON THE FIRST. The first iteration's
forge_tree() is the source of 77 shipped .dfo whose content hashes Whiterun and
Zhitnov reference by name. A flag inside it is one mis-ordered rng draw away
from moving every one of them. A second function cannot: forge_tree() is not
edited at all, so its output is unchanged BY CONSTRUCTION rather than by care.

THE FIVE DIFFERENCES, AND WHERE EACH ONE LIVES IN THIS FILE:
 1. CROWN MASS — a head of cabbage, not a lollipop. The crown is built from
    `lobes` DISCRETE LOBES seeded over an ellipsoid that is as wide as the tree
    is tall and more than half its height deep, and the low lobes hang on the
    FLANKS at shoulder height, so the mass comes down the sides instead of
    floating in the top third. See `lobe_field()`.
 2. TWO RECIPES PER SPECIES — TreeHabit::Solitary puts the crown low (a quarter
    of the height of clean bole) and TreeHabit::Forest puts it high on a long
    clean bole. The same species is forged twice; the silhouettes are not the
    same shape at two scales.
 3. THE BOLE IS AN INDIVIDUAL — `lean_rad`/`lean_dir` tilt it in ITS OWN
    direction and `curve_rad` bends it in an S with a RIGID BUTT, plus a skirt
    of short dry broken snags down its length; and with TreeHabit::MultiStem
    three or four stems leave the GROUND, which is the acacia/alder law.
    Branches leave the bole at staggered heights over its whole length, never
    as a radial fan out of one node.
 4. THE LEAF CARD is the v2 pack (FloraCards rows 14-15) — a twig with dozens
    of leaves, dark heart, lit ragged rim, sky INSIDE the card. The crown
    interior draws the DEEP row and the rim the MID one, so the volume story is
    told twice, once inside a tile and once across the crown.
 5. THE SILHOUETTE BREAKS COARSELY — the rim is `lobes` big lobes with real sky
    between them, not a hundred small cards around a disc.

Dependencies:
- Uses: ObjectRegistry.h (RegistryObject), FloraCards.h (LeafTone/LeafShape),
  FloraBuild.h (Rng, tube primitives), TreeBark.h (bark_tube).
- Used by: tools/forge_trees.cpp, tests/render/TreeForgeV2Tests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- PURE AND DETERMINISTIC: same params, same bytes, same content hash.
- DO NOT ROUTE THE FIRST ITERATION THROUGH THIS FILE. The two builders coexist
  on purpose until the owner rules which one the shipped maps use.
*/
/*
UPD:
- 28:08:2026 - 16:50:00: Создан — вторая итерация деревьев: доли кроны, два
  габитуса на вид, многоствольность от земли, наклон/изгиб ствола с жёстким
  комлем, сухие обломки сучьев, листва рядами v2 атласа.
*/

#pragma once

#include "engine/render/sources/FloraCards.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <cstdint>
#include <glm/vec3.hpp>
#include <string>

namespace dfn::render {

/// THE STRUCTURAL LAW of a v2 tree. Not a style knob — each value changes what
/// the builder does, not how much of it.
enum class TreeHabit : uint8_t {
    /// Меадow tree: crown starts low (a quarter of the height of clean bole),
    /// as wide as the tree is tall, the low scaffolds REACH sideways and the
    /// skirt of foliage comes down past the shoulder.
    Solitary = 0,
    /// Forest tree: a long clean bole with the crown high and narrow, dry snags
    /// all the way up, the crown closing into a ceiling rather than a ball.
    Forest = 1,
    /// Multi-stemmed: three or four stems leave the GROUND from one butt, each
    /// arcs outward, and the crown is wider than it is tall.
    MultiStem = 2,
};

/// Everything a v2 tree is made FROM. Metres and radians; the seed is the only
/// source of variation.
struct TreeV2Params {
    uint64_t seed = 1;
    std::string name = "tree-v2";
    TreeHabit habit = TreeHabit::Solitary;
    float height = 15.0f;
    /// Crown WIDTH as a fraction of height. The measured target of difference
    /// №1: ~1.0 for a solitary broadleaf, ~1.4 for the multi-stem, ~0.62 in
    /// the forest recipe. 0 = the habit's own default.
    float crown_width_frac = 0.0f;
    /// Crown DEPTH (top minus base) as a fraction of height. > 0.5 by the same
    /// difference; 0 = the habit's own default.
    float crown_depth_frac = 0.0f;
    float trunk_radius = 0.44f;
    glm::vec3 bark{0.17f, 0.13f, 0.10f};
    LeafShape card_shape = LeafShape::RoundLobed;
    /// The two v2 atlas rows. Rim carries the lit outside of the crown, core
    /// the shadowed inside; they are separate fields so a forge can put an
    /// autumn or a blossom row on the rim later without touching this file.
    LeafTone tone_rim = LeafTone::PackV2Mid;
    LeafTone tone_core = LeafTone::PackV2Deep;
    /// BIG CROWN LOBES, 5..9 (difference №5). Fewer than five and the crown is
    /// a lumpy ball; more than nine and the rim goes back to being a circle.
    int lobes = 7;
    /// THE BOLE'S OWN TILT. rad from vertical at the top of the bole, in the
    /// `lean_dir` azimuth. The butt stays rigid — measured boles bend above the
    /// lower quarter and not below it (TREE_MODELS_RESEARCH §1.7).
    float lean_rad = 0.12f;
    float lean_dir = 0.0f;
    /// The S: how far the bole bulges out of the chord between butt and top,
    /// as a fraction of the bole's length. This is the «изгиб» that a pure
    /// lean has not got — a leaning straight pole still reads as a pole.
    float curve_frac = 0.06f;
    /// MultiStem only: stems leaving the ground, 2..5.
    int stems = 3;
    /// Dry broken snags down the bole (difference №3). 0 disables.
    int snags = 6;
    /// Far form: cheaper tube facets, no snags, one sheet per anchor.
    bool far_lod = false;
};

/// Forges one v2 tree. The returned object carries its content hash and is
/// ready for write_object().
[[nodiscard]] RegistryObject forge_tree_v2(const TreeV2Params& params);

/// The habit's defaults, exposed so the tests and the passports can assert the
/// proportions without re-deriving them (`crown_width_frac`,
/// `crown_depth_frac`). Values a caller set explicitly win.
void resolve_v2_proportions(const TreeV2Params& in, float& width_frac,
                            float& depth_frac);

} // namespace dfn::render
