/*
Created: 09:08:2026 - 19:24:10
Last updated: 14:08:2026 - 00:14:00
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
- 12:08:2026 - 00:20:00: Crown widths re-derived (oak 0.48 -> 0.70 from three
  independent arms; birch only as far as its bank-line spacing contract allows;
  the conifer's ratio deliberately untouched), crown allometry and per-instance
  width jitter, oak crown base at the band floor, and the GreatOak row.
  flora_control_arm() -- the zero-dose control that lets BEFORE and AFTER come
  out of ONE binary.
- 12:08:2026 - 00:36:00: Great oak fractal depth 4 -> 5 (foliage count is
  bounded by branch TIPS, not by cluster_count).
- 12:08:2026 - 00:45:00: card_scrap_floor() defined here; its three call sites
  now share one definition.
- 13:08:2026 - 16:20:00: EVERY BROADLEAF RAMIFIES (user, 13.08.2026). Fractal
  rows for the oak, the birch and the willow -- the great oak's grower was never
  great-oak-specific. And the cluster sizes are RE-DERIVED from the containment
  arithmetic rather than nudged: a mass is contained by its card CORNER reach,
  the oak's was 0.56 of the whole crown radius, so a crown of 26 such masses
  could not exist and 7 of them were being dropped in silence (birch 16 of 20,
  willow 15 of 20). Corner reach is now a third of the crown radius and the
  count rises to match, paid for out of the wood by max_crown_segments().
- 13:08:2026 - 19:55:00: flora_shyness_arm() defined.
- 13:08:2026 - 21:00:00: THE PUBLISHED WEBER & PENN ROWS (CA Black Oak,
  Quaking Aspen, Weeping Willow), transcribed from docs/WEBER_PENN_PARAMS.md.
  What is ours and says so: the mapping onto our species, the height, and the
  great oak's two deviated numbers. Plus flora_weber_arm(), the generator door.
- 13:08:2026 - 19:56:00: flora_far_lod_arm(), the zero-dose door for the far
  LOD's wood/foliage balance.
- 13:08:2026 - 20:55:00: STAMP CORRECTION ONLY, no code and no content change:
  this session's own UPD entries above were written AHEAD of the wall clock (one
  said 22:00 for work committed at 20:24) and are corrected against the commit
  times. Recorded rather than done silently -- a record whose stamps are
  invented cannot be put in order afterwards, and the entries it would mislead
  are this zone's own.
- 13:08:2026 - 21:50:00: crown_plasticity per species: broadleaf 0.45 (oak,
  birch, willow), conifer 0.05 (pine, krummholz), and 0 for the great oak, which
  is the open-grown case by definition -- nothing crowds it, so a plasticity it
  could never express would be a number with no consumer.
- 13:08:2026 - 23:27:00: flora_united_bole_arm() defined (DFN_FLORA_ONEBOLE,
  default on; =0 is the zero-dose arm).
- 13:08:2026 - 23:45:00: LEAF PACKS: дуб/берёза/ива читают
  LEAF_CLUSTERS_PER_CROWN (12) и LEAF_CLUSTER_RADIUS_FRAC (0.45) из реестра за
  дверью flora_pack_arm(); гигант, сосна и стланик не тронуты, у каждого своя
  записанная причина. Вывод обоих чисел — docs/TREE_MODELS_RESEARCH.md.
- 14:08:2026 - 00:14:00: flora_trunk_arc_arm() defined.
*/

#include "engine/render/sources/FloraSpecies.h"

#include "engine/render/sources/FloraWeber.h"

#include "engine/core/config/sources/Constants.h"

#include <array>
#include <algorithm>
#include <cstdlib>

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
    // The band's FLOOR (CROWN_BASE_FRACTION_MIN), not its middle: the user's
    // «листва должна быть пониже» is a request to move down inside a band that
    // design already approved, and the lowest approved value needs no new
    // number. It buys 9.8 m of clear bole on a 28 m oak against a 2.2 m
    // walkability requirement — the fraction has four times the margin the
    // clearance rule asks for, which is why the rest of the answer is the
    // bottom-heavy envelope profile (FloraSkeleton.cpp) and not this number.
    oak.crown_base_frac = f(config::CROWN_BASE_FRACTION_MIN);
    // CALIBRATED AGAINST THE BUILT TREE, not against the envelope. Foliage
    // never reaches the envelope's widest point (containment keeps a card's
    // CORNER inside, and the widest ring sits at a height where a card would
    // overshoot the crown top), so the achieved diameter is ~0.89 of the
    // nominal. 0.45 measured 9.6-13.6 m against design's 10-16 m band; 0.48
    // lands inside it. Width is load-bearing: design derived
    // TREE_SPACING_FOREST FROM the crown width.
    // RE-DERIVED 11.08.2026, user: «в целом большую часть деревьев сделать
    // шире». 0.48 is not withdrawn as a measurement — it is the value that put
    // the built crown inside design's 10-16 m brief — the BRIEF is what the
    // user has revised, and the derivation below is the one that produces the
    // new number rather than a nudged old one.
    //
    // THREE INDEPENDENT ARMS, and they land within 10 % of each other:
    //  (1) THE FRAME. Reference frame 16, the one the user's sentence is about:
    //      the canopy oaks' crown width measures 0.85-0.95 of the visible tree
    //      height. Read as a DIAMETER over height, that is 0.85-0.95.
    //  (2) BOTANY. Open-grown Quercus crown diameter runs 0.8-1.0 x height;
    //      closed forest-grown runs 0.4-0.5. Our stand is thinned on purpose
    //      (TREE_SPACING_FOREST 12-18 m against a 20 m crown), so it sits
    //      between the two and nearer the open figure: ~0.7.
    //  (3) THE LATTICE. At 12-18 m spacing a crown of 0.70 x 28 m = 19.6 m
    //      overlaps its neighbour by ~25 %, which is what a closed-but-thinned
    //      canopy IS. Crown shyness (0.28) and the crowding lean already exist
    //      to resolve that overlap, and they have had nothing to do until now.
    // The conservative arm wins: 0.70, i.e. built ~0.62 after containment.
    //
    // NOTE WHAT THIS IS NOT DERIVED FROM. CROWN_ASPECT_MAX is not an input
    // here and must not become one — its own NUMBERS row records that it once
    // became the LEADING input and spoiled the birch. Widening moves the crown
    // AWAY from that ceiling (container aspect falls 1.25 -> 0.93), so the
    // guard is left with more slack, not less, which is the only relationship
    // a guard is allowed to have with the thing it guards.
    oak.crown_width_frac = 0.70f;
    oak.crown_plasticity = f(config::CROWN_PLASTICITY_BROADLEAF);
    // A veteran oak spreads long after it stops climbing: exp 1.35 makes a
    // giant (x1.5) 15 % wider FOR ITS HEIGHT and a sapling (x0.4) 27 % narrower
    // for its own, so the same rule delivers both halves of the user's
    // sentence — most trees wider, small ones still small.
    oak.crown_allometry_exp = 1.35f;
    oak.crown_width_jitter = 0.16f;
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
    // RAMIFICATION, and it replaces the colonizer above as what BUILDS this
    // crown (the colonizer's rows are left in place because DFN_FLORA_CROWN=1
    // is the zero-dose arm and has to be able to rebuild the old tree from the
    // same binary).
    //
    // THE USER'S DIAGNOSIS, 13.08.2026: «крона от ствола отходит… листья
    // должны из веток расти», and the reason he could see it is structural.
    // The Quercus text above is still true — a decurrent crown with a few heavy
    // sinuous limbs — but space colonization DERIVES those limbs from a cloud,
    // so what the finished tree shows is the cloud's shape, and the foliage was
    // placed at cloud CENTROIDS that are on no branch at all. Recursive
    // ramification states the limbs directly, and the same rule at every scale
    // is what makes an oak's silhouette read as an oak at 8 px and at 8 m.
    //
    // NO NEW TRIANGLES. The node budget handed to the grower is
    // max_crown_segments(), the identical ceiling the colonizer was given, so
    // this is a change of WHAT the wood is, not of how much (measured: oak wood
    // 287 -> 297 triangles a tree, inside the same TREE_TRI_BUDGET_MAX 700).
    //
    // 2-4 MAJORS, not the great oak's 2-5: at a 43-node budget five first-order
    // limbs leaves eight nodes each and every one of them is a stub. The count
    // is what the budget affords, not a style — and the two-lobed / elliptical /
    // domed variety the great oak gets from this same number survives, one
    // notch narrower.
    oak.fractal_depth = 4;
    oak.fractal_majors_min = 2;
    oak.fractal_majors_max = 4;
    oak.fractal_children_min = 2;
    oak.fractal_children_max = 3;
    oak.fractal_major_pitch = 0.88f;  // ~50 deg off vertical: an oak goes OUT
    oak.fractal_pitch_spread = 0.46f;
    oak.fractal_length_decay = 0.72f;
    // Card foliage: FEW and LARGE. The user asked for «большими плоскими
    // наборами листочков», and the arithmetic agrees — a crown reads as one
    // mass only when its elements are a sizeable fraction of it (the lesson
    // that finally cured the birch, §3.7.5), and every extra card is pure
    // overdraw, which is the currency alpha-cutout foliage actually spends.
    // RE-BALANCED WITH THE WIDTH, because these two are one decision. Cluster
    // radius is a fraction of the CROWN radius, so widening the crown by 46 %
    // and leaving 0.46 alone would have grown each card by the same 46 % — the
    // crown would have stayed a picture of 22 blobs, only bigger. Holding the
    // absolute cluster size roughly where the user approved it (3.1 -> 3.5 m)
    // and spending the widening on COUNT is what makes the extra width read as
    // more crown rather than as a zoomed-in crown. 28 x 3 x 2 = 168 card
    // triangles, still a fifth of TREE_TRI_BUDGET_MAX.
    // RE-DERIVED 13.08.2026, AND IT IS A DERIVATION RATHER THAN A NUDGE.
    // A leaf mass is contained by its CARD CORNER reach,
    // `cluster_radius_frac * card_width_frac * hypot(1, card_aspect)`, and a
    // mass hung on a branch tip is allowed only what is left between that tip
    // and the envelope. At 0.40 the oak's corner reach was 0.56 of the whole
    // crown radius: a crown "made of 26 such masses" cannot exist, there is
    // room for about four, and the rest were being SILENTLY DROPPED by the
    // scrap floor — measured 19 of 26 kept on the oak, 4 of 20 on the birch,
    // 5 of 20 on the willow. That is what «их просто малюют» looks like from
    // inside the generator: the table describes a crown of twenty masses and
    // the geometry can only hold four, so the tree really is a handful of
    // painted billboards.
    //
    // THE RULE: the wood gets two thirds of the crown radius to ramify in and
    // the foliage shell gets the outer third, so corner reach = 0.33 and
    // cluster_radius_frac = 0.33 / (card_width_frac * hypot(1, card_aspect)).
    // For the oak that is 0.33 / 1.409 = 0.234.
    //
    // COUNT RISES SO THE CROWN IS NOT EMPTIER, and the triangle budget pays for
    // it out of the WOOD by construction: max_crown_segments() subtracts the
    // card commitment from TREE_TRI_BUDGET_MAX, so 40 clusters buy themselves
    // 240 card triangles and leave 35 branch segments instead of 43. Nothing
    // here can overrun the budget; the two halves trade inside it.
    //
    // WHAT IS NOT CLAIMED: this is not the user's «большими плоскими наборами
    // листочков» being withdrawn. The masses that SURVIVED were that big; the
    // ones that did not exist were not any size at all. Total surviving card
    // area is held, and the crown stops being four blobs.
    oak.cluster_count = 40;
    oak.cluster_radius_frac = 0.24f;
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
    pine.crown_plasticity = f(config::CROWN_PLASTICITY_CONIFER);
    // NO ALLOMETRY FOR THE CONIFER, and that is a positive statement rather
    // than an omission: a spruce really does hold its width-to-height ratio
    // through its life, and «шире» applied to the anti-oak would delete the one
    // silhouette contrast the catalog is built on (§1.5). It gets the
    // per-instance jitter only, so no two pines are the same pine.
    pine.crown_allometry_exp = 1.0f;
    pine.crown_width_jitter = 0.08f;
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
    // WIDENED ONLY AS FAR AS ITS OWN COUPLING ALLOWS (11.08.2026). The user's
    // «шире» is general, but the birch's width has a second consumer: core
    // derives BIRCH_BANKLINE_SPACING (7-9 m) from the crown width, so a birch
    // that outgrows its 6-8 m band silently breaks the bank line it exists to
    // draw. 0.40 builds ~6.7 m at the 19 m nominal — the top of the band, wider
    // than today, and inside the number somebody else is standing on. The oak
    // takes the user's request in full; the accent tree takes it up to its
    // contract and no further, and the difference is reported rather than
    // smoothed over.
    birch.crown_width_frac = 0.35f;
    birch.crown_plasticity = f(config::CROWN_PLASTICITY_BROADLEAF);
    birch.crown_allometry_exp = 1.22f;
    birch.crown_width_jitter = 0.08f;
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
    // RAMIFICATION. The birch's difference from the oak is entirely in these
    // four numbers and that is the point of a table: a STEEP first order (0.52
    // rad, ~30 deg off vertical) with a narrow divergence gives the high open
    // crown of the Vase envelope, where the oak's 0.88 rad gives a spreading
    // one. Two majors at most, because a birch really does carry one dominant
    // axis with a second competing — three would be an oak with white bark.
    birch.fractal_depth = 4;
    birch.fractal_majors_min = 2;
    birch.fractal_majors_max = 3;
    birch.fractal_children_min = 2;
    birch.fractal_children_max = 3;
    birch.fractal_major_pitch = 0.52f;
    birch.fractal_pitch_spread = 0.38f;
    birch.fractal_length_decay = 0.76f; // slender: the taper is slow
    // The birch is the species that twice read as STACKED PLATES (§3.7.4/5).
    // Both cures were the same one: elements about as wide as the crown, and
    // never allowed to slide onto the axis. Cards inherit that discipline —
    // seven cluster centres, each carrying cards nearly as wide as the whole
    // crown, so no arrangement of them can look like a pile of discs.
    // Re-derived on the same rule as the oak (see its block): corner reach a
    // third of the crown radius, so cluster_radius_frac = 0.33 / (1.05 * 1.257)
    // = 0.25. The birch was the worst case of the defect that rule fixes — 4
    // clusters of 20 survived the scrap floor, i.e. a crown of four leaves —
    // and it is the species whose treeline the user called «частокол
    // одинаковых деревьев».
    birch.cluster_count = 36;
    birch.cluster_radius_frac = 0.25f;
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
    willow.crown_plasticity = f(config::CROWN_PLASTICITY_BROADLEAF);
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
    // RAMIFICATION. A willow's signature is that its limbs LEAVE horizontally
    // and its shoots then fall, so the first order is the flattest in the
    // catalog (1.12 rad, ~64 deg off vertical) and `droop` 0.40 above already
    // outweighs a phototropism of 0.06 — the grower adds -droop*(depth+1)
    // against +phototropism per step, so each generation hangs lower than its
    // parent without a single weeping-specific line of code.
    willow.fractal_depth = 4;
    willow.fractal_majors_min = 2;
    willow.fractal_majors_max = 4;
    willow.fractal_children_min = 2;
    willow.fractal_children_max = 3;
    willow.fractal_major_pitch = 1.12f;
    willow.fractal_pitch_spread = 0.50f;
    willow.fractal_length_decay = 0.70f;
    // Same derivation: 0.33 / (1.15 * 1.281) = 0.224. Five clusters of 20 were
    // surviving before it.
    willow.cluster_count = 38;
    willow.cluster_radius_frac = 0.22f;
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
    kp.crown_plasticity = f(config::CROWN_PLASTICITY_CONIFER);
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

    // --- THE GREAT OAK: a landmark that happens to be a tree ---------------
    // User, 11.08.2026: «дубы, которые будут высокие и чья нижняя часть кроны
    // будет в радиусе равна высоте, ветки будут расти как фракталы, редкие и
    // очень большие как горы, на некоторые можно будет забраться».
    //
    // WHY IT IS A ROW AND NOT A MATURITY. Design ruled (§5.8) that a giant is
    // DaleOak with maturity > 1 — one system, not two — and that ruling was
    // correct for a tree that differed only in size. This one differs in what
    // it is made of: a different GROWER (recursive ramification, not space
    // colonization), a crown rule stated as radius-equals-height rather than
    // as a fraction of height, and furniture that no other plant carries.
    // Reached through the maturity scalar it would need every one of those as
    // an if-branch on the scalar, which is the same two systems with the seam
    // hidden inside the first one.
    SpeciesParams& great = t[static_cast<size_t>(FloraSpecies::GreatOak)];
    great = oak; // the value language, bark, leaf tones and cards are the oak's
    great.name = "GreatOak";
    great.envelope = CrownEnvelope::GreatCrown;
    // HEIGHT. Not the giant tier's 1.5 x 32 = 48 m, and the ceiling is somebody
    // else's rule rather than taste: design §5.7 binds the forest to stay under
    // the landmark it frames, and Ravenscar's approved relief is 110-120 m. A
    // 46 m great oak standing on a 25 m foothill tops out at 71 m against a
    // 110 m summit — clear, with the margin that ruling asked for. Its own
    // maturity band is narrow (see ProcFlora): this species IS the giant, so
    // drawing another 1.5 x on top of it would be the same multiplier twice.
    great.height_min = 34.0f;
    great.height_max = 46.0f;
    // A trunk you can put a staircase on. 0.045 of height = 1.8 m diameter at
    // 40 m, and the flare takes it to 2.9 m — wide enough for a 0.9 m tread to
    // land on solid wood, which is the geometric precondition for anything sim
    // does with it later.
    great.trunk_radius_frac = 0.045f;
    great.taper_exp = 0.65f; // a veteran's bole barely tapers below the fork
    great.trunk_sweep = 0.16f;
    great.trunk_sides = 7;   // it is read from 20 m away, not from 200
    great.trunk_segments = 7;
    // The crown starts LOW, and on this species that is not a stylistic choice
    // either: an 80 m wide crown carried at 0.40 of height would be a canopy
    // roof with nothing under it, and the user's picture is a tree you climb
    // INTO. 0.30 puts the lowest limbs at 12 m on a 40 m tree.
    great.crown_base_frac = 0.30f;
    // THE HEADLINE RULE, verbatim: lower crown radius = tree height.
    great.crown_radius_per_height = 1.0f;
    great.crown_width_frac = 2.0f; // = 2 x radius/height; kept in sync, see above
    // THE GIANT IS THE OPEN-GROWN CASE BY DEFINITION: its own row says nothing
    // crowds it (shyness 0) and design sites it in a clearing. A plasticity it
    // can never express would be a number with no consumer.
    great.crown_plasticity = 0.0f;
    great.crown_allometry_exp = 1.0f; // the rule is absolute, not maturity-scaled
    great.crown_width_jitter = 0.10f;
    // FRACTAL RAMIFICATION. Depth 5 with 2-5 majors and 2-3 children gives
    // 64-405 tips before the node budget bites; the variety of the FIRST-order
    // count is what makes one great oak two-lobed («как сиськи» — two majors,
    // wide pitch, each carrying its own sub-crown), another an upright ellipse
    // (two majors at a steep pitch), another a broad dome (four or five).
    // Nothing enumerates those outcomes; they are what the parameter does.
    great.fractal_depth = 5;
    great.fractal_majors_min = 2;
    great.fractal_majors_max = 5;
    great.fractal_children_min = 2;
    great.fractal_children_max = 3;
    great.fractal_major_pitch = 1.05f;  // ~60 deg off vertical: limbs go OUT
    great.fractal_pitch_spread = 0.42f;
    great.fractal_length_decay = 0.70f;
    great.droop = 0.16f;
    great.phototropism = 0.20f;
    great.pipe_exponent = 2.6f;
    // Climbing furniture. Treads every ~0.42 m of rise up a 12 m bole is 28
    // steps; two platforms sit at the first-order forks. Numbers requested from
    // lead as GREAT_OAK_STEP_RISE / _TREAD / _PLATFORM_R (see the file header).
    great.climb_steps = 28;
    great.climb_platforms = 2;
    // Foliage: many more, larger clusters — the crown is an order of magnitude
    // bigger in plan than a forest oak's, and 28 clusters spread over it would
    // read as a bare frame with confetti.
    // MEASURED OFF THE FIRST FRAME, not chosen: at 64 clusters the great oak
    // photographed as a WINTER tree — the ramification read beautifully and the
    // crown did not exist. The arithmetic says why, and it is the sparseness
    // trap in a new place: a card cluster is three crossed cutouts, so its
    // effective covering power is a fraction of its disc area, and 64 discs
    // that tile an 80 m crown ON PAPER leave it transparent in fact.
    // AND THE SECOND MEASUREMENT SAYS THIS NUMBER IS NOT THE BINDING ONE.
    // Raising it from 110 to 190 changed the frame by nothing, because the
    // foliage path takes ONE mass per branch TIP and subsamples down to this
    // count — so when the fractal grower yields ~156 tips, anything above ~156
    // is dead weight and the real lever is the grower's depth and node budget.
    // Recorded rather than quietly tuned: the next agent who wants a fuller
    // great oak should raise `fractal_depth` / GREAT_OAK_MAX_NODES, not this.
    // THE GIANT'S MASSES ARE SMALLER AND THERE ARE MORE OF THEM, and the
    // reason is the measurement GIANT_OAKS.md §4 predicted in words. At 190
    // masses of 7.6 m radius the crown's own optical depth measured 81 LAYERS
    // OF LEAF and its silhouette ambiguity 0.078 — i.e. the giant was a solid
    // green hill with no sky between its limbs, which is exactly the failure
    // the fractal grower was introduced to prevent, arriving through the
    // FOLIAGE after the WOOD had been fixed. Smaller masses on the same
    // ramification let the structure show through it.
    // 240 AND NOT MORE: `cluster_count` is a uint8_t, so 300 silently becomes
    // 44. Caught by the warning gauntlet on the first build of this row.
    great.cluster_count = 240;
    great.cluster_radius_frac = 0.06f;
    great.attractors = 0;    // it does not colonize; the fractal grower supplies
    great.shyness = 0.0f;    // nothing crowds a great oak
    great.lean_response = 0.04f;

    // --- LEAF PACKS (13.08.2026, the user's «пачки текстур»). ---------------
    // The broadleaf card species carried 36-40 clusters of 0.22-0.25 crown
    // radii — confetti, the mechanism of his «листва как наждачка». Every
    // studied model does the opposite (docs/TREE_MODELS_RESEARCH.md §1.7:
    // 5-15 elements of 0.4-0.9 crown radii; SpeedTree §1.1: the SMALLER the
    // budget the BIGGER the cluster). The two registry rows carry the
    // derivation; the species keep their own card shapes, tones and aspect.
    // THE GIANT IS EXEMPT by its own contract (its 240 small masses at 0.06
    // are the measured answer to a crown that must show structure through
    // foliage — see the block above), the krummholz already sits at 0.52, and
    // the PINE is a different doctrine (needle sprays on shoots, one per
    // whorl branch) and is not touched by this change.
    // DFN_FLORA_PACKS=0 is the zero-dose arm (flora_pack_arm).
    if (flora_pack_arm()) {
        // VERIFICATION HOOKS, never a shipping path (the standing of
        // DFN_FLORA_NODES): sweep the pack count and radius from one binary.
        // They exist because the first landing of the rows was uniform across
        // three envelopes and the VASE (birch) lost half its card area to
        // containment while the SPHERE (oak) gained a third — one number, three
        // containers, and the sweep is how the per-envelope factors below were
        // measured rather than argued.
        static const auto pack_n = [] {
            const char* e = std::getenv("DFN_FLORA_PACKN");
            const int v = e != nullptr ? std::atoi(e) : 0;
            return (v >= 3 && v <= 64)
                ? static_cast<uint8_t>(v)
                : static_cast<uint8_t>(config::LEAF_CLUSTERS_PER_CROWN);
        }();
        static const auto pack_r = [] {
            const char* e = std::getenv("DFN_FLORA_PACKR");
            const float v =
                e != nullptr ? static_cast<float>(std::atof(e)) : 0.0f;
            return (v > 0.05f && v < 1.0f)
                ? v
                : static_cast<float>(config::LEAF_CLUSTER_RADIUS_FRAC);
        }();
        // PER-ENVELOPE COUNT FACTORS, measured rather than argued (the sweep
        // hooks above, R=0.45 throughout, fleet presented area in m^2):
        //   SPHERE (oak) counts the registry row VERBATIM: at 12 the fleet
        //     presents 654 and the half-density control still fails the
        //     retired 229 floor (0.3x654=196); at 14 the control PASSES it
        //     (229.4) and stops being a control (Rule 30). 12 is the ceiling
        //     the control leaves the sphere, not a taste.
        //   VASE (birch) x3/2 = 18: the narrow container CLAMPS a big pack to
        //     ~0.30 of its envelope at the rim (emit_cluster's shrink-to-fit),
        //     so the vase holds its area by COUNT where the sphere holds it by
        //     SIZE: 12 -> 28.5 m^2 (half the confetti build), 16 -> 44.5,
        //     18 -> 56.6 (above the confetti build's own 53 tripwire).
        //   WEEPING (willow) x7/6 = 14: the skirt spreads its packs along the
        //     fall; 12 leaves the Reduced fleet at 242.9 and Full at 195 —
        //     14 clears both old tripwires (195->229 Full) without touching
        //     the sphere's control arithmetic.
        struct PackRow {
            FloraSpecies s;
            uint32_t num, den;
        };
        for (const PackRow& pr : {PackRow{FloraSpecies::DaleOak, 1u, 1u},
                                  PackRow{FloraSpecies::RiverBirch, 3u, 2u},
                                  PackRow{FloraSpecies::ValeWillow, 7u, 6u}}) {
            SpeciesParams& sp = t[static_cast<size_t>(pr.s)];
            sp.cluster_count = static_cast<uint8_t>(
                std::min(64u, static_cast<uint32_t>(pack_n) * pr.num / pr.den));
            sp.cluster_radius_frac = pack_r;
        }
    }

    return t;
}

} // namespace

float card_scrap_floor(const SpeciesParams& sp, float crown_radius) {
    const float nominal_half_width =
        crown_radius * sp.cluster_radius_frac * sp.card_width_frac;
    return std::min(0.22f * crown_radius, 0.55f * nominal_half_width);
}

bool flora_control_arm() {
    static const bool on = [] {
        const char* e = std::getenv("DFN_FLORA_CONTROL");
        return e != nullptr && e[0] == '1';
    }();
    return on;
}

bool flora_shyness_arm() {
    static const bool on = [] {
        const char* e = std::getenv("DFN_FLORA_SHY");
        return e == nullptr || e[0] != '0';
    }();
    return on;
}

bool flora_weber_arm() {
    static const bool on = [] {
        const char* e = std::getenv("DFN_FLORA_GEN");
        return e == nullptr || e[0] != '0';
    }();
    return on;
}

bool flora_envelope_arm() {
    static const bool on = [] {
        const char* e = std::getenv("DFN_FLORA_CROWN");
        return e != nullptr && e[0] == '1';
    }();
    return on;
}

bool flora_far_lod_arm() {
    static const bool on = [] {
        const char* e = std::getenv("DFN_FLORA_FARLOD");
        return e != nullptr && e[0] == '1';
    }();
    return on;
}

bool flora_united_bole_arm() {
    static const bool on = [] {
        const char* e = std::getenv("DFN_FLORA_ONEBOLE");
        return e == nullptr || e[0] != '0';
    }();
    return on;
}

bool flora_pack_arm() {
    static const bool on = [] {
        const char* e = std::getenv("DFN_FLORA_PACKS");
        return e == nullptr || e[0] != '0';
    }();
    return on;
}

bool flora_trunk_arc_arm() {
    static const bool on = [] {
        const char* e = std::getenv("DFN_FLORA_TRUNKARC");
        return e != nullptr && e[0] == '1';
    }();
    return on;
}

// --- WEBER & PENN ROWS ------------------------------------------------------
// THE MODEL IS THE PAPER'S AND SO ARE THESE NUMBERS. They are transcribed from
// docs/WEBER_PENN_PARAMS.md, which the lead extracted from the paper itself
// (SIGGRAPH 1995, Appendix, p.126) after this zone refused to reproduce them
// from memory. That refusal was the right call and it is worth keeping the
// reason visible: a number with the wrong provenance is worse than a missing
// one, because "Weber-Penn parameters" would have lent someone else's authority
// to our guesses.
//
// WHAT IS OURS AND SAYS SO: the MAPPING of their four species onto our four,
// the height (our band is a cross-zone contract, their `Scale` is not), and
// `base_size` where our crown-base band and CANOPY_CLEARANCE_MIN bind. Nothing
// else has been adjusted — where a value looks wrong for us it is left alone
// and reported, because the first thing anyone does with a published table is
// start "improving" it, and then it is no longer a published table.
//
// THREE THINGS THE REAL TABLE SETTLED THAT WE HAD ARGUED ABOUT:
//  1. `CA Black Oak` has `0BaseSplits` 2 and `0SegSplits` 0.4 — the bole splits
//     at the ground AND keeps splitting. That is where an oak's several main
//     limbs come from, and it means the two-lobed veteran silhouette is a
//     property of STRUCTURE, not of an envelope. docs/GIANT_OAKS.md §4 argued
//     exactly that from first principles; this is the published confirmation.
//  2. `PruneRatio` is 1 for the WILLOW ALONE. Envelope pruning is an OPTIONAL
//     part of the model that three of four species do not use — and our old
//     crown envelope was that mechanism applied to every species as the primary
//     one. We took the exception and made it the rule.
//  3. `Levels` 3-4 with `nBranches` up to 300: density comes from the NUMBER of
//     branches per level, not from finer steps. At our 700-triangle budget that
//     is where our own seam has to be, and the authors have no equivalent of it.
WeberParams species_weber(FloraSpecies species, float height) {
    WeberParams w;
    w.height = height;

    switch (species) {
    case FloraSpecies::GreatOak: {
        // CA BLACK OAK, WITH TWO NUMBERS CHANGED, AND THEY ARE OURS.
        //
        // The great oak is not a published species — it is the user's, and it
        // comes with an explicit geometric rule that no real tree obeys:
        // «нижняя часть кроны в радиусе равна высоте». Verbatim CA Black Oak
        // builds it 34.8 m across against the 82 m that rule demands, because
        // its limbs leave at 30 deg from the trunk and run 0.8 of its length —
        // proportions for a tree whose crown is about its own height WIDE, not
        // twice it.
        //
        // So two numbers move and both are marked: the first order leaves at
        // 72 deg instead of 30 (limbs go OUT, which is the whole silhouette),
        // and runs 1.0 of the trunk instead of 0.8. Everything else is the
        // published row. Deviating knowingly from a published table on a
        // species that does not exist is legitimate; doing it silently, or on a
        // species that DOES exist, would not be.
        w.levels = 3;
        w.shape = WeberShape::Hemispherical;
        w.base_size = 0.05f;
        w.ratio = 0.018f;
        w.ratio_power = 1.3f;
        w.flare = 1.2f;
        w.attraction_up = 0.8f;
        w.base_splits = 2;
        w.level[0] = {0.0f,  0.0f,  0.0f,  0.0f, 1.00f, 0.00f, 0.95f,  0.0f,   0.0f,  90.0f, 8, 0.40f, 10.0f,  0.0f,   0};
        w.level[1] = {72.0f,-30.0f, 80.0f,  0.0f, 1.00f, 0.10f, 1.00f, 40.0f, -70.0f, 150.0f,10, 0.20f, 10.0f, 10.0f,  40};
        w.level[2] = {45.0f, 10.0f,140.0f,  0.0f, 0.20f, 0.05f, 1.00f,  0.0f,   0.0f, -30.0f, 3, 0.10f, 10.0f, 10.0f, 120};
        w.level[3] = {45.0f, 10.0f,140.0f,  0.0f, 0.40f, 0.00f, 1.00f,  0.0f,   0.0f,   0.0f, 1, 0.00f,  0.0f,  0.0f,   0};
        break;
    }
    case FloraSpecies::DaleOak: {
        // CA BLACK OAK, verbatim. Ours is an oak and so is theirs.
        w.levels = 3;
        w.shape = WeberShape::Hemispherical; // Shape 2
        w.base_size = 0.05f;
        w.ratio = 0.018f;
        w.ratio_power = 1.3f;
        w.flare = 1.2f;
        w.attraction_up = 0.8f;
        w.base_splits = 2;
        //          down  downV  rot  rotV   len  lenV  taper  curve  cBack  curveV res split splitA splitAV branches
        w.level[0] = {0.0f,  0.0f,  0.0f,  0.0f, 1.00f, 0.00f, 0.95f,  0.0f,   0.0f,  90.0f, 8, 0.40f, 10.0f,  0.0f,   0};
        w.level[1] = {30.0f,-30.0f, 80.0f,  0.0f, 0.80f, 0.10f, 1.00f, 40.0f, -70.0f, 150.0f,10, 0.20f, 10.0f, 10.0f,  40};
        w.level[2] = {45.0f, 10.0f,140.0f,  0.0f, 0.20f, 0.05f, 1.00f,  0.0f,   0.0f, -30.0f, 3, 0.10f, 10.0f, 10.0f, 120};
        w.level[3] = {45.0f, 10.0f,140.0f,  0.0f, 0.40f, 0.00f, 1.00f,  0.0f,   0.0f,   0.0f, 1, 0.00f,  0.0f,  0.0f,   0};
        break;
    }
    case FloraSpecies::RiverBirch: {
        // QUAKING ASPEN, verbatim — AND THE MAPPING IS OURS AND IMPERFECT, so
        // it is stated rather than hidden: Populus tremuloides is not Betula.
        // It is the closest of the four published species to what our birch has
        // to be (a slender pale-boled broadleaf with a high open crown), and
        // the alternative was inventing numbers, which is what the whole
        // exercise exists to stop. If a real Betula table is ever obtained it
        // replaces this one.
        w.levels = 3;
        w.shape = WeberShape::TendFlame; // Shape 7
        w.base_size = 0.40f;
        w.ratio = 0.015f;
        w.ratio_power = 1.2f;
        w.flare = 0.6f;
        w.attraction_up = 0.5f;
        w.base_splits = 0;
        w.level[0] = {0.0f,  0.0f,  0.0f, 0.0f, 1.00f, 0.00f, 1.00f,   0.0f, 0.0f, 20.0f, 3, 0.0f, 0.0f, 0.0f,  0};
        w.level[1] = {60.0f,-50.0f,140.0f, 0.0f, 0.30f, 0.00f, 1.00f, -40.0f, 0.0f, 50.0f, 5, 0.0f, 0.0f, 0.0f, 50};
        w.level[2] = {45.0f, 10.0f,140.0f, 0.0f, 0.60f, 0.00f, 1.00f, -40.0f, 0.0f, 75.0f, 3, 0.0f, 0.0f, 0.0f, 30};
        w.level[3] = {45.0f, 10.0f, 77.0f, 0.0f, 0.00f, 0.00f, 1.00f,   0.0f, 0.0f,  0.0f, 1, 0.0f, 0.0f, 0.0f, 10};
        break;
    }
    case FloraSpecies::ValeWillow: {
        // WEEPING WILLOW, verbatim. Note `AttractionUp` -3: the model's upward
        // tug run BACKWARDS is the whole of what makes a willow weep, and it is
        // the only negative value in the published table. Note also that this
        // is the one species with `PruneRatio` 1 — see the header note.
        w.levels = 4;
        w.shape = WeberShape::Cylindrical; // Shape 3
        w.base_size = 0.05f;
        w.ratio = 0.03f;
        w.ratio_power = 2.0f;
        w.flare = 0.75f;
        w.attraction_up = -3.0f;
        w.base_splits = 2;
        w.level[0] = { 0.0f,  0.0f,   0.0f,  0.0f, 0.80f, 0.00f, 1.00f,  0.0f, 20.0f, 120.0f, 8, 0.10f,  3.0f,  0.0f,   0};
        w.level[1] = {20.0f, 10.0f,-120.0f, 30.0f, 0.50f, 0.10f, 1.00f, 40.0f, 80.0f,  90.0f,16, 0.20f, 30.0f, 10.0f,  25};
        w.level[2] = {30.0f, 10.0f,-120.0f, 30.0f, 1.50f, 0.00f, 1.00f,  0.0f,  0.0f,   0.0f,12, 0.20f, 45.0f, 20.0f,  10};
        w.level[3] = {20.0f, 10.0f, 140.0f,  0.0f, 0.10f, 0.00f, 1.00f,  0.0f,  0.0f,   0.0f, 1, 0.00f,  0.0f,  0.0f, 300};
        break;
    }
    default:
        // Conifers are NOT on this path and that is a positive statement: a
        // spruce is monopodial and rhythmic, its whorls are a developmental
        // pattern rather than a branching law, and whorl_skeleton already
        // models that. The published set has no conifer either.
        //
        // BLACK TUPELO is transcribed in docs/WEBER_PENN_PARAMS.md and unused:
        // Levels 4, Shape 4, a tall straight forest broadleaf. It is the row to
        // reach for when the catalog gains the dense-stand species the user
        // asked for, and it is recorded here so the next agent does not go
        // looking for a fifth table that already exists.
        w.levels = 0;
        break;
    }
    // OURS: the clear bole. Their `BaseSize` is a species trait; our crown base
    // is a design band with a walkability floor under it, and the two disagree
    // (CA Black Oak clears 5 % of its height, our band starts at 35 %). The
    // contract wins, because CANOPY_CLEARANCE_MIN is a rule about the player.
    // Recorded rather than silently overwritten: it means our oaks carry their
    // lowest limbs higher than the published oak does, and that is a knowing
    // deviation, not a transcription error.
    return w;
}

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
    case FloraSpecies::GreatOak:
        return true;
    default:
        return false; // bushes and logs are obstacles you walk AROUND (§3.5)
    }
}

bool has_leaf_cards(FloraSpecies species) {
    return species_params(species).foliage == FoliageShape::Card;
}

} // namespace dfn::render
