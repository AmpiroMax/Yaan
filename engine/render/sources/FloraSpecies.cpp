/*
Created: 09:08:2026 - 19:24:10
Last updated: 09:08:2026 - 20:21:13
Module: engine/render
File: engine/render/sources/FloraSpecies.cpp

Responsibility:
- The species parameter tables. Every species in the catalog is one entry here;
  adding a species is a table row, not a code path (user request в38).

Key items:
- species_params(), is_canopy_tree().

Dependencies:
- Uses: FloraSpecies.h, generated Constants.h.
- Used by: ProcFlora, ProcFloraTests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md.
- Size bands / clearances come from Constants.h (Rule 14). Proportions are
  asset geometry (same standing as ProcMesh's §5 dimensions).
- SILHOUETTE IDENTITY OUTRANKS REALISM: at 640x360 these species must be
  separable by outline and value alone (LANDSCAPE §1.5).
*/
/*
UPD:
- 09:08:2026 - 19:24:10: Created — the three catalog trees, willow, snag, bush,
  big bush, fallen log, deadfall (LANDSCAPE §5.7-§5.10).
- 09:08:2026 - 20:21:13: Broadleaf foliage switched to alpha-cutout CARDS (oak,
  birch, willow) with their atlas tone/shape bands and card proportions; the
  conifer deliberately stays on cone tiers this stage so one frame compares the
  two treatments. Bushes stay solid (design §5: only tree foliage is cards).
*/

#include "engine/render/sources/FloraSpecies.h"

#include "engine/core/config/sources/Constants.h"

#include <array>

namespace dfn::render {

namespace {

constexpr float f(double v) { return static_cast<float>(v); }

// Value language (LANDSCAPE §5): oak mid-green over near-black; pine the
// darkest flora value in the scene; birch the brightest trunk; willow a DARK
// still-water mass (the value opposite of birch, so a birch line and a willow
// line read as different water); snags split by material — grey in the wood
// (texture), pale in the open (a real L2 guide).
constexpr glm::vec3 OAK_CROWN{0.30f, 0.42f, 0.18f};
constexpr glm::vec3 OAK_TRUNK{0.14f, 0.11f, 0.08f};
constexpr glm::vec3 PINE_DARK{0.12f, 0.22f, 0.19f};
constexpr glm::vec3 PINE_TRUNK{0.20f, 0.15f, 0.10f};
constexpr glm::vec3 BIRCH_TRUNK{0.88f, 0.87f, 0.82f};
constexpr glm::vec3 BIRCH_CROWN{0.55f, 0.62f, 0.30f};
constexpr glm::vec3 WILLOW_CROWN{0.20f, 0.30f, 0.16f};
constexpr glm::vec3 WILLOW_TRUNK{0.17f, 0.14f, 0.11f};
constexpr glm::vec3 SNAG_WEATHERED{0.42f, 0.39f, 0.34f};
constexpr glm::vec3 BUSH_GREEN{0.35f, 0.47f, 0.22f};
constexpr glm::vec3 DEADWOOD{0.31f, 0.27f, 0.21f};

std::array<SpeciesParams, FLORA_SPECIES_COUNT> build_table() {
    std::array<SpeciesParams, FLORA_SPECIES_COUNT> t{};

    // --- Dale Oak: "ball on a stump", the valley tree -----------------------
    SpeciesParams& oak = t[static_cast<size_t>(FloraSpecies::DaleOak)];
    oak.name = "DaleOak";
    oak.envelope = CrownEnvelope::Sphere;
    oak.foliage = FoliageShape::Card;
    oak.height_min = f(config::OAK_HEIGHT_MIN);
    oak.height_max = f(config::OAK_HEIGHT_MAX);
    oak.trunk_radius_frac = 0.022f; // ~1.2 m diameter at 28 m
    oak.taper_exp = 0.85f;
    oak.trunk_sweep = 0.12f;
    oak.trunk_sides = 5;
    oak.trunk_segments = 6;
    oak.crown_base_frac = 0.40f;
    // CALIBRATED AGAINST THE BUILT TREE, not against the envelope. Foliage
    // never reaches the envelope's widest point (containment keeps a card's
    // CORNER inside, and the widest ring sits at a height where a card would
    // overshoot the crown top), so the achieved diameter is ~0.89 of the
    // nominal. 0.45 measured 9.6-13.6 m against design's 10-16 m band; 0.48
    // lands inside it. Width is load-bearing: design derived
    // TREE_SPACING_FOREST FROM the crown width.
    oak.crown_width_frac = 0.48f;
    oak.generations = 2;
    oak.branch_count[0] = 5;
    oak.branch_count[1] = 2;
    oak.branch_angle[0] = 1.02f;
    oak.branch_angle[1] = 0.80f;
    oak.branch_start_frac[0] = 0.40f;
    oak.branch_start_frac[1] = 0.35f;
    oak.length_decay[0] = 0.46f;
    oak.length_decay[1] = 0.55f;
    oak.radius_ratio[0] = 0.38f;
    oak.radius_ratio[1] = 0.46f;
    oak.phototropism = 0.30f;
    oak.droop = 0.12f;
    // Card foliage: FEW and LARGE. The user asked for «большими плоскими
    // наборами листочков», and the arithmetic agrees — a crown reads as one
    // mass only when its elements are a sizeable fraction of it (the lesson
    // that finally cured the birch, §3.7.5), and every extra card is pure
    // overdraw, which is the currency alpha-cutout foliage actually spends.
    oak.cluster_count = 12;
    oak.cluster_radius_frac = 0.40f;
    oak.tone_first = LeafTone::OakMid;
    oak.tone_count = 3; // mid / deep / sunlit — one crown carries all three
    oak.card_shape_a = LeafShape::RoundLobed;
    oak.card_shape_b = LeafShape::RaggedTip;
    oak.cards_per_cluster = 3;
    oak.card_width_frac = 1.10f;
    oak.card_aspect = 0.80f;
    oak.trunk_color = OAK_TRUNK;
    oak.foliage_color = OAK_CROWN;
    oak.shyness = 0.28f;
    oak.lean_response = 0.10f;

    // --- Highland Pine: the anti-oak, narrow triangle, whorled --------------
    SpeciesParams& pine = t[static_cast<size_t>(FloraSpecies::HighlandPine)];
    pine.name = "HighlandPine";
    pine.envelope = CrownEnvelope::Cone;
    pine.foliage = FoliageShape::ConeShell;
    pine.height_min = f(config::PINE_HEIGHT_MIN);
    pine.height_max = f(config::PINE_HEIGHT_MAX);
    pine.trunk_radius_frac = 0.016f;
    pine.taper_exp = 1.25f;
    pine.trunk_sweep = 0.04f; // conifers stand straight
    pine.trunk_sides = 5;
    pine.trunk_segments = 7;
    pine.crown_base_frac = 0.38f;
    pine.crown_width_frac = 0.22f; // 6-8 m base
    pine.generations = 1;          // whorl branches; the tiers carry the mass
    pine.whorled = true;
    pine.branch_count[0] = 6;
    pine.branch_angle[0] = 1.32f; // near-horizontal whorls
    pine.branch_start_frac[0] = 0.38f;
    pine.length_decay[0] = 0.30f;
    pine.radius_ratio[0] = 0.30f;
    pine.phototropism = 0.10f;
    pine.droop = -0.18f; // upsweep
    pine.cluster_count = 3;
    pine.cluster_radius_frac = 1.0f;
    // The conifer DELIBERATELY stays on solid cone tiers this stage, so the
    // verification frame carries both treatments side by side and answers
    // whether needles need cards — rather than the answer being guessed.
    // Column 3 of the atlas (NeedleFan) and this tone are already generated.
    pine.tone_first = LeafTone::ConiferDark;
    pine.tone_count = 1;
    pine.card_shape_a = LeafShape::NeedleFan;
    pine.card_shape_b = LeafShape::NeedleFan;
    pine.trunk_color = PINE_TRUNK;
    pine.foliage_color = PINE_DARK;
    pine.shyness = 0.18f;
    pine.lean_response = 0.06f;

    // --- River Birch: pale slim CLUMP, moving/clear water -------------------
    SpeciesParams& birch = t[static_cast<size_t>(FloraSpecies::RiverBirch)];
    birch.name = "RiverBirch";
    birch.envelope = CrownEnvelope::Vase;
    birch.foliage = FoliageShape::Card;
    birch.height_min = f(config::BIRCH_HEIGHT_MIN);
    birch.height_max = f(config::BIRCH_HEIGHT_MAX);
    birch.trunk_radius_frac = 0.013f;
    birch.taper_exp = 0.70f;
    birch.trunk_sweep = 0.18f;
    birch.trunk_count_min = 2; // classic multi-stem river birch
    birch.trunk_count_max = 3;
    birch.trunk_spread = 0.55f;
    birch.trunk_sides = 5;
    birch.trunk_segments = 5;
    // THE BIRCH EXCEPTION (NUMBERS.md BIRCH_CROWN_BASE_FRACTION_MIN/MAX, landed
    // after the crown failed to read four times). The old 0.45 with a 0.30 width
    // defines a container 1.8:1 tall-to-wide before a single leaf is placed, and
    // the built tree measured 2.3:1 — a column. No arrangement of contents can
    // fix a container: the mass IS the container. Raising the base also buys
    // ~11 m of clear trunk instead of 8.5, which is design's own goal, and it is
    // what a real river birch looks like.
    // The band's MINIMUM, not its midpoint — design's derivation rule is "the
    // smallest value at or above the floor that satisfies the aspect ceiling",
    // and the smallest one leaves the most crown.
    birch.crown_base_frac = f(config::BIRCH_CROWN_BASE_FRACTION_MIN);
    // Same calibration, and the birch needed it most: 0.30 built a 3.6-4.5 m
    // crown against design's 5-7 m band — the accent tree was a third narrower
    // than its brief, which is the other half of why it read as a column.
    birch.crown_width_frac = 0.52f;
    birch.generations = 1;
    birch.branch_count[0] = 4;
    birch.branch_angle[0] = 0.72f;
    birch.branch_start_frac[0] = 0.55f;
    birch.length_decay[0] = 0.40f;
    birch.radius_ratio[0] = 0.34f;
    birch.phototropism = 0.45f;
    birch.droop = 0.16f;
    // The birch is the species that twice read as STACKED PLATES (§3.7.4/5).
    // Both cures were the same one: elements about as wide as the crown, and
    // never allowed to slide onto the axis. Cards inherit that discipline —
    // seven cluster centres, each carrying cards nearly as wide as the whole
    // crown, so no arrangement of them can look like a pile of discs.
    birch.cluster_count = 7;
    birch.cluster_radius_frac = 0.80f; // clusters must OVERLAP into one mass
    birch.tone_first = LeafTone::BirchLight;
    birch.tone_count = 2;
    birch.card_shape_a = LeafShape::OvalSpray;
    birch.card_shape_b = LeafShape::RaggedTip;
    birch.cards_per_cluster = 2; // a narrow crown does not need a third plane
    birch.card_width_frac = 1.05f;
    birch.card_aspect = 0.95f;
    birch.trunk_color = BIRCH_TRUNK;
    birch.foliage_color = BIRCH_CROWN;
    birch.shyness = 0.20f;
    birch.lean_response = 0.14f;

    // --- Vale Willow: dark falling mass, STILL water ------------------------
    SpeciesParams& willow = t[static_cast<size_t>(FloraSpecies::ValeWillow)];
    willow = oak; // a willow is an oak with different numbers — that is the point
    willow.name = "ValeWillow";
    willow.envelope = CrownEnvelope::Weeping;
    willow.height_min = 14.0f;
    willow.height_max = 20.0f;
    willow.trunk_radius_frac = 0.026f;
    willow.trunk_sweep = 0.22f;
    willow.crown_base_frac = 0.34f;
    willow.crown_width_frac = 0.62f; // wide shoulder
    willow.branch_angle[0] = 1.15f;
    willow.phototropism = 0.10f;
    willow.droop = 0.85f; // the falling skirt
    willow.cluster_count = 14;
    willow.cluster_radius_frac = 0.34f;
    willow.tone_first = LeafTone::WillowDark;
    willow.tone_count = 2;
    willow.card_shape_a = LeafShape::OvalSpray;
    willow.card_shape_b = LeafShape::RoundLobed;
    willow.cards_per_cluster = 3;
    willow.card_width_frac = 1.00f;
    willow.card_aspect = 1.35f; // taller than wide: the cards HANG
    willow.trunk_color = WILLOW_TRUNK;
    willow.foliage_color = WILLOW_CROWN;

    // --- Snag: no crown; the only flora legal at full height in a wedge -----
    SpeciesParams& snag = t[static_cast<size_t>(FloraSpecies::Snag)];
    snag.name = "Snag";
    snag.envelope = CrownEnvelope::None;
    snag.foliage = FoliageShape::None;
    snag.height_min = 10.0f;
    snag.height_max = 20.0f;
    snag.trunk_radius_frac = 0.024f;
    snag.taper_exp = 1.4f; // broken, tapering hard to a snapped top
    snag.trunk_sweep = 0.10f;
    snag.trunk_sides = 5;
    snag.trunk_segments = 5;
    snag.crown_base_frac = 1.0f;
    snag.crown_width_frac = 0.0f;
    snag.generations = 1;
    snag.branch_count[0] = 2; // a couple of broken stubs
    snag.branch_angle[0] = 1.15f;
    snag.branch_start_frac[0] = 0.55f;
    snag.length_decay[0] = 0.18f;
    snag.radius_ratio[0] = 0.40f;
    snag.phototropism = 0.0f;
    snag.droop = 0.0f;
    snag.cluster_count = 0;
    snag.trunk_color = SNAG_WEATHERED; // in-forest value; open-ground is paler
    snag.foliage_color = SNAG_WEATHERED;
    snag.shyness = 0.0f;
    snag.lean_response = 0.0f;

    // --- Bush: ground texture ----------------------------------------------
    SpeciesParams& bush = t[static_cast<size_t>(FloraSpecies::Bush)];
    bush.name = "Bush";
    bush.envelope = CrownEnvelope::Sphere;
    bush.foliage = FoliageShape::Blob;
    bush.height_min = 1.0f;
    bush.height_max = 1.5f;
    bush.trunk_radius_frac = 0.03f;
    bush.trunk_segments = 2;
    bush.crown_base_frac = 0.12f; // NOT a canopy tree: exempt from the 2.2 m rule
    bush.crown_width_frac = 1.30f;
    bush.generations = 0;
    bush.cluster_count = 4;
    bush.cluster_radius_frac = 0.60f;
    bush.cluster_slices = 6;
    bush.cluster_bands = 2;
    bush.trunk_color = DEADWOOD;
    bush.foliage_color = BUSH_GREEN;
    bush.shyness = 0.0f;
    bush.lean_response = 0.0f;

    // --- BigBush: an OBSTACLE, not a scaled bush (design §5.10) -------------
    SpeciesParams& big = t[static_cast<size_t>(FloraSpecies::BigBush)];
    big = bush;
    big.name = "BigBush";
    big.height_min = 2.5f;
    big.height_max = 4.0f;
    big.crown_width_frac = 1.05f;
    big.cluster_count = 7;
    big.cluster_radius_frac = 0.52f;

    // --- Fallen logs: the trunk generator, lying down -----------------------
    SpeciesParams& log = t[static_cast<size_t>(FloraSpecies::FallenLog)];
    log.name = "FallenLog";
    log.envelope = CrownEnvelope::None;
    log.foliage = FoliageShape::None;
    log.height_min = 8.0f;  // LENGTH once laid down
    log.height_max = 14.0f;
    log.trunk_radius_frac = 0.055f; // ~0.9-1.4 m diameter
    log.taper_exp = 0.5f;
    log.trunk_sweep = 0.06f;
    log.trunk_sides = 6; // it is seen from the side, close up
    log.trunk_segments = 4;
    log.crown_base_frac = 1.0f;
    log.crown_width_frac = 0.0f;
    log.generations = 0;
    log.cluster_count = 0;
    log.trunk_color = DEADWOOD;
    log.foliage_color = DEADWOOD;
    log.shyness = 0.0f;
    log.lean_response = 0.0f;

    SpeciesParams& dead = t[static_cast<size_t>(FloraSpecies::Deadfall)];
    dead = log;
    dead.name = "Deadfall";
    dead.height_min = 2.0f;
    dead.height_max = 4.0f;
    // Floored so the shadow map still resolves it (docs/specs/flora.md §3.5):
    // 0.35 m / 4 m length keeps the thin end above ~2 shadow texels.
    dead.trunk_radius_frac = 0.062f;
    dead.trunk_sides = 5;
    dead.trunk_segments = 3;

    return t;
}

} // namespace

const SpeciesParams& species_params(FloraSpecies species) {
    static const std::array<SpeciesParams, FLORA_SPECIES_COUNT> table = build_table();
    const auto i = static_cast<size_t>(species);
    return table[i < FLORA_SPECIES_COUNT ? i : 0];
}

bool is_canopy_tree(FloraSpecies species) {
    switch (species) {
    case FloraSpecies::DaleOak:
    case FloraSpecies::HighlandPine:
    case FloraSpecies::RiverBirch:
    case FloraSpecies::ValeWillow:
        return true;
    default:
        return false; // bushes and logs are obstacles you walk AROUND (§3.5)
    }
}

bool has_leaf_cards(FloraSpecies species) {
    return species_params(species).foliage == FoliageShape::Card;
}

} // namespace dfn::render
