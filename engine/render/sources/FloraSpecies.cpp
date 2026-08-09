/*
Created: 09:08:2026 - 19:24:10
Last updated: 10:08:2026 - 02:49:15
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
- 09:08:2026 - 21:18:02: Birch crown base moved to its landed exception band
  (BIRCH_CROWN_BASE_FRACTION_MIN) after the crown measured 2.30:1 tall-to-wide
  — a column by construction. Crown widths CALIBRATED against the built tree
  rather than against the envelope (oak 0.45 -> 0.48, birch 0.30 -> 0.52): the
  achieved diameter is ~0.7-0.9 of nominal, and the birch had drifted a third
  under design's 5-7 m brief.
- 10:08:2026 - 01:59:06: §5.10 forest floor becomes real objects: the snag
  gains truncated stubs and its SPLIT (Snag weathered grey / SnagPale bone —
  one geometry, two materials, the design §5.10 model); logs gain moss on the
  upper side, broken stubs and (big class) an upturned root plate. Pine sprays
  2 -> 3 planes under the new render-spec floor: card count buys ANGULAR
  COVERAGE against the worst azimuth.
- 10:08:2026 - 02:49:15: Moss darkened under design's moss-below-grass rule;
  the variation multiplier unified as MOSS_TONE_B (one number, three sites).
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
// Bone-white, deliberately UNDER the birch bole (0.88) so the open-field snag
// is the brightest DEAD thing without contesting the brightest LIVE one.
constexpr glm::vec3 SNAG_BONE{0.78f, 0.75f, 0.66f};
constexpr glm::vec3 BUSH_GREEN{0.35f, 0.47f, 0.22f};
constexpr glm::vec3 DEADWOOD{0.31f, 0.27f, 0.21f};
// Moss reads as a HUE step against dead wood, not a value step: close in
// luminance to DEADWOOD, well apart in green. Full-colour basis (user ruling).
// DARKENED 10.08.2026 under design's acceptance rule: every moss tone
// (including the MOSS_TONE_B variation) stays >= 0.05 luminance BELOW the
// grass band, or moss converges with grass the moment the shipped pipeline
// shifts either. Asserted in the suite.
constexpr glm::vec3 LOG_MOSS{0.18f, 0.30f, 0.11f};

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
    // SPACE COLONIZATION. A Quercus crown is the paper's DECURRENT case: no
    // single dominant axis above the fork, heavy sinuous limbs that ramify, and
    // — from the botany — "just a few large branches bearing relatively sparse
    // foliage", grouped densely at the ENDS of the twigs with real air between
    // the masses. All of that is emergent here from a wide envelope plus a
    // surface-weighted attractor cloud; none of it had to be authored.
    // The paper's own sparseness lever, applied for the triangle budget as much
    // as for the look: "Decreasing N and increasing dk yields crowns that are
    // increasingly sparse", and large dk also gives "smoothly curved branches"
    // because no single attractor can swing a tip. Both are what an oak wants.
    oak.attractors = 240;
    oak.colonize_step_frac = 0.21f;
    oak.influence_d = 8.0f;   // the paper's own value for trees
    oak.kill_d = 2.3f;
    oak.surface_bias = 0.58f; // foliage on the crown shell, limbs in the dark
    oak.pipe_exponent = 2.5f;
    oak.fork_softening = 0.38f;
    oak.branch_base_frac = 0.30f; // limbs leave the bole below the foliage line
    oak.phototropism = 0.26f;
    oak.droop = 0.14f;
    // Card foliage: FEW and LARGE. The user asked for «большими плоскими
    // наборами листочков», and the arithmetic agrees — a crown reads as one
    // mass only when its elements are a sizeable fraction of it (the lesson
    // that finally cured the birch, §3.7.5), and every extra card is pure
    // overdraw, which is the currency alpha-cutout foliage actually spends.
    oak.cluster_count = 22;
    oak.cluster_radius_frac = 0.46f;
    oak.tone_first = LeafTone::OakMid;
    oak.tone_count = 3; // mid / deep / sunlit — one crown carries all three
    oak.card_shape_a = LeafShape::RoundLobed;
    oak.card_shape_b = LeafShape::RaggedTip;
    oak.cards_per_cluster = 3;
    oak.card_width_frac = 1.10f;
    oak.card_aspect = 0.80f;
    oak.trunk_color = OAK_TRUNK;
    oak.twig_color = OAK_TRUNK * 0.82f;
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
    // CROWN RATIO IS THE HEADLINE NUMBER AND IT WAS THE BUG. A forest-grown
    // Scots pine carries live foliage on only ~0.30 of its height and a forest
    // Norway spruce on ~0.44-0.49 (Austrian NFI, measured); the open-grown
    // figures are 0.86-0.94, and 0.86-0.94 IS «юбка». The old 0.38 crown base
    // gave a crown over the top 62 % — an open-grown paddock spruce standing in
    // a wood. Our stand is thinned (TREE_SPACING_FOREST 12-18 m), so it sits
    // between forest and open: crown ratio 0.48, i.e. base at 0.52.
    // NOTE this is ABOVE design's CROWN_BASE_FRACTION_MAX 0.45, which their §5
    // ruling explicitly demoted from a binding cap to "documentation of the
    // typical outcome for broad crowns". The floor (0.35, walkability) is what
    // binds, and 0.52 clears it with room.
    // 0.45 is design's CROWN_BASE_FRACTION_MAX and this species sits ON it.
    // The forestry evidence would support MORE (a forest-grown Norway spruce
    // measures crown ratio 0.44-0.49, i.e. a base of 0.51-0.56, and a forest
    // Scots pine 0.30 -> 0.70), and design's §5 ruling explicitly demoted _MAX
    // from a binding cap to "documentation of the typical outcome". But a
    // per-species crown base is a REGISTRY row — that is the precedent
    // BIRCH_CROWN_BASE_FRACTION_MIN set — so the derivation has gone to the lead
    // and this ships at the value that needs no new constant. Do not quietly
    // raise it here; ask.
    pine.crown_base_frac = 0.45f;
    // CALIBRATED AGAINST THE BUILT TREE, like every other width in this table.
    // At 0.24 the ENVELOPE was 6.7-9.0 m and looked right on paper, but the
    // built crown measured 4.6-6.6 m — the smallest variants a quarter under
    // design's 6-9 m brief, with a green suite, because only the ceiling of the
    // band was ever asserted. That is design's own «a range is two assertions»
    // defect, and this is its fifth appearance. Built width is 0.69 of the
    // envelope (foliage never reaches the envelope's widest point), so 6 m of
    // built crown needs 8.7 m of envelope: 0.31 of height.
    // NOTE the coupling before changing either of these: the GROWTH envelope is
    // inset from the silhouette envelope by the cluster reach, so lowering
    // cluster_radius_frac lets the branches grow FURTHER and the tree comes out
    // WIDER. The two move together and tuning one alone is how the pine went to
    // 11 m against a 6-9 m brief.
    pine.crown_width_frac = 0.25f;
    // WHORLS, not tiers. A whorl is a YEAR: the leader puts on one internode and
    // flushes one ring of laterals at the top of it. Spacing is therefore that
    // year's height increment (short at the apex, long through the vigorous
    // middle years, short again at the base), the branch count tracks the same
    // vigour, and the older whorls are self-pruned. A solid cone can express
    // none of that, which is exactly why it reads as a skirt.
    pine.whorl_count = 11;
    pine.whorl_branches_min = 3; // forestry: a "complete" whorl is >= 3
    pine.whorl_branches_max = 6; // an average whorl carries 2-7
    pine.whorl_miss_bottom = 0.30f;
    pine.whorl_miss_top = 0.05f;
    // Measured spruce insertion angles run 40-70 deg from the stem, left-skewed,
    // and the ascent-to-horizontal transition happens FAST in the upper crown.
    pine.whorl_angle_top = 1.02f;    // ~58 deg above horizontal at the leader
    pine.whorl_angle_bottom = -0.26f;// the oldest branches sag below horizontal
    pine.whorl_shoots = 3;
    pine.whorl_stubs = 5;
    pine.stub_band_frac = 0.52f;
    pine.phototropism = 0.10f;
    pine.droop = 0.34f; // the primary sags toward its tip under its own weight
    pine.pipe_exponent = 2.6f;
    pine.cluster_count = 34;
    pine.cluster_radius_frac = 0.34f;
    // NEEDLES ARE CARDS NOW, and the frame answered the question the last stage
    // deliberately left open. The pine was kept on solid cone tiers so that one
    // verification frame would carry both treatments side by side rather than
    // the answer being guessed; the user's verdict on that frame was «елки
    // просто юбки большие». The experiment ran and it returned a result.
    pine.foliage = FoliageShape::Card;
    pine.card_width_frac = 1.30f;
    pine.card_aspect = 0.62f; // a needle spray is wider than it is deep
    // THREE PLANES IS NOW THE FLOOR FOR CARD FOLIAGE (render-spec constraint,
    // 10.08.2026): card count buys ANGULAR COVERAGE and is chosen against the
    // WORST azimuth, not the average one. Two was the birch's defect surviving
    // in the pine at lower odds — a 2-plane spray still has viewing directions
    // where its projected area collapses, and 46 sprays only made the failure
    // statistical instead of certain. The suite now measures worst-azimuth
    // coverage per cluster with a parallel-plane control.
    pine.cards_per_cluster = 3;
    pine.tone_first = LeafTone::ConiferDark;
    pine.tone_count = 1;
    pine.card_shape_a = LeafShape::NeedleFan;
    pine.card_shape_b = LeafShape::NeedleFan;
    pine.trunk_color = PINE_TRUNK;
    pine.twig_color = PINE_TRUNK * 0.62f;
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
    // ONE STEM. A river birch really is multi-stemmed, and this is where the
    // field guide loses to §1.5: two or three bare pale poles from a single
    // root, crowned by a tuft, is the PALM silhouette the user rejected —
    // «выглядит как пальма… как острые пики». The clump was half of what
    // produced it and the other half was the tuft. A single slender white bole
    // with branches along its upper length is what reads as a birch at 640x360.
    birch.trunk_count_min = 1;
    birch.trunk_count_max = 1;
    birch.trunk_spread = 0.0f;
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
    birch.crown_width_frac = 0.34f;
    // Betula pendula's two-part branch rule, and it is the whole silhouette:
    // the MAIN branches ascend while the outer branchlets are thin, drooping and
    // flexible — "a fine hanging haze of twigs". So: strong phototropism to lift
    // the primaries, and a droop that the pipe model lets bite only on the thin
    // outer wood. Branches start at 0.40 of height, WELL below the 0.58 foliage
    // line, so the upper bole carries ascending limbs instead of being a pole.
    birch.attractors = 200;
    birch.colonize_step_frac = 0.24f;
    birch.influence_d = 10.0f;
    birch.kill_d = 2.4f;
    birch.surface_bias = 0.42f; // a birch crown is airy THROUGHOUT, not shelled
    birch.pipe_exponent = 2.2f; // slender: less thickening per tip
    birch.fork_softening = 0.42f;
    birch.branch_base_frac = 0.28f;
    birch.phototropism = 0.46f;
    birch.droop = 0.30f;
    // The birch is the species that twice read as STACKED PLATES (§3.7.4/5).
    // Both cures were the same one: elements about as wide as the crown, and
    // never allowed to slide onto the axis. Cards inherit that discipline —
    // seven cluster centres, each carrying cards nearly as wide as the whole
    // crown, so no arrangement of them can look like a pile of discs.
    birch.cluster_count = 20;
    birch.cluster_radius_frac = 0.42f;
    birch.tone_first = LeafTone::BirchLight;
    birch.tone_count = 2;
    birch.card_shape_a = LeafShape::OvalSpray;
    birch.card_shape_b = LeafShape::RaggedTip;
    // THREE PLANES, AND THE NOTE THAT USED TO BE HERE — "a narrow crown does not
    // need a third plane" — WAS WRONG, caught by a tour frame and invisible to
    // every isolated render. Cards are fixed-orientation by design (a billboard
    // shimmers at 640x360 and casts a rotating shadow), so a cluster of TWO
    // crossed planes has azimuths where both present nearly edge-on and the
    // cluster all but disappears. The oak and willow never showed it because
    // three planes cannot all be edge-on at once. The birch showed it as a line
    // of bare white poles with a few flecks — «острые пики» surviving a rewrite
    // that had genuinely fixed the shape, purely as a viewing-angle artefact.
    birch.cards_per_cluster = 3;
    birch.card_width_frac = 1.05f;
    birch.card_aspect = 0.76f; // wider than tall: three planes stack vertically
    birch.trunk_color = BIRCH_TRUNK;
    birch.twig_color = glm::vec3{0.24f, 0.19f, 0.16f};
    birch.twig_radius_frac = 0.80f; // white bole, dark limbs — the real thing
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
    willow.crown_base_frac = 0.30f;
    willow.crown_width_frac = 0.72f; // wide shoulder
    willow.phototropism = 0.06f;
    // The paper is explicit that it could NOT generate strongly pendulous forms
    // with the growth bias alone, so the droop stays an explicit force rather
    // than something we chase with the tropism vector (flora_algorithms.md
    // §1.3.5). Recorded because a successor will otherwise try.
    willow.droop = 0.40f;
    willow.attractors = 230;
    willow.colonize_step_frac = 0.20f;
    willow.influence_d = 7.0f;
    willow.kill_d = 2.3f;
    willow.surface_bias = 0.50f;
    willow.branch_base_frac = 0.26f;
    willow.cluster_count = 20;
    willow.cluster_radius_frac = 0.42f;
    willow.tone_first = LeafTone::WillowDark;
    willow.tone_count = 2;
    willow.card_shape_a = LeafShape::OvalSpray;
    willow.card_shape_b = LeafShape::RoundLobed;
    willow.cards_per_cluster = 3;
    willow.card_width_frac = 1.00f;
    willow.card_aspect = 1.35f; // taller than wide: the cards HANG
    willow.trunk_color = WILLOW_TRUNK;
    willow.twig_color = WILLOW_TRUNK * 0.80f;
    willow.foliage_color = WILLOW_CROWN;

    // --- Snag: no crown; the only flora legal at full height in a wedge -----
    // A SNAG IS NOT A TREE WITH ZERO LEAVES. A winter oak is a live skeleton:
    // full limb spread, fine ramification. A snag is what is LEFT of one —
    // broken blunt top, a handful of truncated stubs where the limbs snapped
    // off, nothing fine surviving. The suite separates the three objects
    // (snag / winter tree / bare pole) on limb reach, with the bare pole — the
    // asset this entry used to build — as the real rejected control.
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
    snag.has_skeleton = false; // no crown to grow; the trunk IS the asset
    snag.phototropism = 0.0f;
    snag.droop = 0.0f;
    snag.cluster_count = 0;
    snag.stub_count = 5;      // truncated dead limbs; band asserted in the suite
    snag.stub_len_frac = 0.085f; // ~1-1.7 m on a 12-20 m snag: stubs, not limbs
    snag.trunk_color = SNAG_WEATHERED; // in-forest look (SNAG_DENSITY_FOREST_*)
    snag.twig_color = SNAG_WEATHERED * 0.85f;
    snag.foliage_color = SNAG_WEATHERED;
    snag.shyness = 0.0f;
    snag.lean_response = 0.0f;

    // --- SnagPale: the SAME asset, open-ground material (design §5.10) ------
    // «a pale snag alone in a meadow is a landmark; a grey snag in a wood is
    // weather» — one geometry, two values, two densities. Geometry identity per
    // variant is asserted in the suite, so nobody can quietly fork the shape.
    SpeciesParams& snag_pale = t[static_cast<size_t>(FloraSpecies::SnagPale)];
    snag_pale = snag;
    snag_pale.name = "SnagPale";
    snag_pale.trunk_color = SNAG_BONE;
    snag_pale.twig_color = SNAG_BONE * 0.88f;
    snag_pale.foliage_color = SNAG_BONE;

    // --- StuntedPine: the §5.12 talus apron's tree --------------------------
    // Krummholz: the same whorl generator as the HighlandPine — a stunted pine
    // is not a new mechanism, it is the same species written by wind and thin
    // soil — with dwarf numbers: squat, wide for its height, hard-swept,
    // foliage nearly to the ground (an obstacle you walk around, not canopy,
    // so it is exempt from CANOPY_CLEARANCE_MIN by classification).
    SpeciesParams& kp = t[static_cast<size_t>(FloraSpecies::StuntedPine)];
    kp = pine;
    kp.name = "StuntedPine";
    kp.height_min = 3.5f;
    kp.height_max = 7.0f;
    kp.trunk_radius_frac = 0.030f; // squat: thick for its height
    kp.trunk_sweep = 0.34f;        // wind-written lean, THE krummholz signal
    kp.trunk_segments = 4;
    kp.crown_base_frac = 0.10f;    // foliage nearly to the ground
    // 0.55 was the first value and the frame refused it: at 5 m tall and
    // 2.4 m wide the dwarf read as a SAPLING — the one thing a talus tree
    // must never read as (saplings are «очень редкие» by user ruling, and a
    // scree full of them would be a nursery). Squat is the krummholz read.
    kp.crown_width_frac = 0.78f;
    kp.whorl_count = 5;
    kp.whorl_branches_min = 3;
    kp.whorl_branches_max = 5;
    kp.whorl_miss_bottom = 0.10f;  // open ground: little self-pruning
    kp.whorl_stubs = 2;
    kp.stub_band_frac = 0.10f;
    kp.droop = 0.22f;
    kp.cluster_count = 12;
    kp.cluster_radius_frac = 0.52f; // dense mats, not sparse sprays
    kp.cards_per_cluster = 3;

    // --- The RICH EDGE SET (в8/в19в) + moss ---------------------------------
    // Flower palette roles are design's ruling (10.08.2026): (a) carpet reads
    // against grass by HUE at low value cost; (b) accent is the VALUE carrier
    // («margin reads lit»); (c) jewel is a PLACEMENT BUDGET, never common
    // scatter; (d) umbel echoes the birch's bank line at ground level. All
    // solid geometry — только листва деревьев картами (design §5).
    auto patch = [](SpeciesParams& p, const char* name, GroundForm form) {
        p = SpeciesParams{};
        p.name = name;
        p.envelope = CrownEnvelope::None;
        p.foliage = FoliageShape::None;
        p.has_skeleton = false;
        p.ground_form = form;
        p.crown_base_frac = 0.0f;
        p.crown_width_frac = 0.0f;
        p.cluster_count = 0;
        p.shyness = 0.0f;
        p.lean_response = 0.0f;
    };

    SpeciesParams& moss = t[static_cast<size_t>(FloraSpecies::MossPatch)];
    patch(moss, "MossPatch", GroundForm::MossDome);
    moss.height_min = 0.07f;
    moss.height_max = 0.13f;
    moss.patch_radius = 0.8f;
    moss.ground_elements = 3;
    moss.foliage_color = LOG_MOSS;
    moss.accent_color = LOG_MOSS;
    moss.accent_color_b = LOG_MOSS * MOSS_TONE_B;

    SpeciesParams& carpet = t[static_cast<size_t>(FloraSpecies::FlowerCarpet)];
    patch(carpet, "FlowerCarpet", GroundForm::HeadsTuft);
    carpet.height_min = 0.16f;
    carpet.height_max = 0.30f;
    carpet.patch_radius = 0.55f;
    carpet.ground_elements = 7;
    carpet.element_radius = 0.05f;
    carpet.foliage_color = {0.24f, 0.36f, 0.16f};
    // Cool blue-violet, luminance ~0.40: reads against green grass by hue and
    // stays off the sky ramp's distance band (design's constraint).
    carpet.accent_color = {0.36f, 0.33f, 0.74f};
    carpet.accent_color_b = {0.46f, 0.42f, 0.80f};

    SpeciesParams& accentf = t[static_cast<size_t>(FloraSpecies::FlowerAccent)];
    patch(accentf, "FlowerAccent", GroundForm::HeadsTuft);
    accentf.height_min = 0.20f;
    accentf.height_max = 0.36f;
    accentf.patch_radius = 0.50f;
    accentf.ground_elements = 6;
    accentf.element_radius = 0.06f;
    accentf.foliage_color = {0.26f, 0.38f, 0.17f};
    // White with yellow variation: the highest VALUE contrast on the margin —
    // this is what makes a path edge read "lit" at 640x360 (§1.5: value
    // first, hue second).
    accentf.accent_color = {0.93f, 0.92f, 0.83f};
    accentf.accent_color_b = {0.88f, 0.75f, 0.24f};

    SpeciesParams& jewel = t[static_cast<size_t>(FloraSpecies::FlowerJewel)];
    patch(jewel, "FlowerJewel", GroundForm::HeadsStem);
    jewel.height_min = 0.45f;
    jewel.height_max = 0.65f;
    jewel.patch_radius = 0.30f;
    jewel.ground_elements = 2;
    jewel.element_radius = 0.09f;
    jewel.foliage_color = {0.24f, 0.36f, 0.16f};
    jewel.trunk_color = {0.28f, 0.38f, 0.19f}; // stem green
    // Saturated deep red: THE RARE JEWEL. Never in common scatter — rarity is
    // a placement budget (FloraEdgeRules marks it), sited as/near finds.
    jewel.accent_color = {0.60f, 0.09f, 0.11f};
    jewel.accent_color_b = {0.48f, 0.07f, 0.24f};

    SpeciesParams& umbel = t[static_cast<size_t>(FloraSpecies::FlowerUmbel)];
    patch(umbel, "FlowerUmbel", GroundForm::HeadsStem);
    umbel.height_min = 0.55f;
    umbel.height_max = 0.85f;
    umbel.patch_radius = 0.40f;
    umbel.ground_elements = 3;
    umbel.element_radius = 0.11f;
    umbel.element_aspect = 0.35f; // a flat plate is the umbel silhouette
    umbel.foliage_color = {0.26f, 0.38f, 0.17f};
    umbel.trunk_color = {0.30f, 0.40f, 0.20f};
    // Pale GREENISH cream: the birch bank-line's ground echo at water margins.
    // Deliberately pulled toward green and away from the accent daisy's warm
    // white — the two pale species must be separable from EACH OTHER at 10 m
    // (design's acceptance), and the first draft sat 0.14 apart in RGB, which
    // is one species in two habitats, not two species.
    umbel.accent_color = {0.74f, 0.79f, 0.62f};
    umbel.accent_color_b = {0.68f, 0.74f, 0.58f};

    SpeciesParams& shroom = t[static_cast<size_t>(FloraSpecies::Mushroom)];
    patch(shroom, "Mushroom", GroundForm::Caps);
    shroom.height_min = 0.10f;
    shroom.height_max = 0.22f;
    shroom.patch_radius = 0.50f;
    shroom.ground_elements = 5;
    shroom.element_radius = 0.10f;
    shroom.trunk_color = {0.72f, 0.68f, 0.58f}; // stems pale
    shroom.accent_color = {0.55f, 0.28f, 0.12f}; // cap warm brown-red
    shroom.accent_color_b = {0.74f, 0.66f, 0.50f}; // cap cream
    shroom.foliage_color = shroom.accent_color;

    SpeciesParams& pebbles = t[static_cast<size_t>(FloraSpecies::PebbleCluster)];
    patch(pebbles, "PebbleCluster", GroundForm::Stones);
    pebbles.height_min = 0.10f;
    pebbles.height_max = 0.18f;
    pebbles.patch_radius = 0.50f;
    pebbles.ground_elements = 6;
    pebbles.element_radius = 0.12f;
    pebbles.accent_color = {0.47f, 0.46f, 0.44f};
    pebbles.accent_color_b = {0.54f, 0.50f, 0.44f};
    pebbles.foliage_color = pebbles.accent_color;

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
    bush.has_skeleton = false;
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
    // THE REJECTED INSTANCE IS A FLOATING CYLINDER. What separates a fallen
    // TREE from a cylinder: it touches the ground along its whole length (the
    // suite asserts every axial slice is part-buried, with a floated copy as
    // the control), it carries moss on its UPPER side, broken limb stubs, and
    // — for the big class — the upturned root plate where it tore out.
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
    log.has_skeleton = false;
    log.cluster_count = 0;
    log.stub_count = 3;
    log.stub_len_frac = 0.07f; // ~0.6-1 m: snapped in the fall, not a rack
    log.root_plate = true;
    log.moss_cover = 0.45f;
    log.moss_color = LOG_MOSS;
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
    dead.stub_count = 1;     // a branch piece, not a trunk: one snag of a limb
    dead.stub_len_frac = 0.10f;
    dead.root_plate = false; // deadfall is shed wood; it never had roots
    dead.moss_cover = 0.22f; // younger wood on the ground mosses less

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
