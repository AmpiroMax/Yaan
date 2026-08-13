/*
Created: 09:08:2026 - 19:22:41
Last updated: 13:08:2026 - 19:55:00
Module: engine/render
File: engine/render/sources/FloraSpecies.h

Responsibility:
- The flora species vocabulary: a species is a SET OF NUMBERS plus a silhouette
  intent, never bespoke code (user request в38). Declares SpeciesParams, the
  species/envelope enums, and the table accessor.

Key items:
- FloraSpecies, CrownEnvelope, FoliageShape, SpeciesParams, species_params().

Dependencies:
- Uses: glm.
- Used by: ProcFlora, ScatterBatcher (via ProcFlora), ProcFloraTests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; the zone contract is docs/specs/flora.md.
- Size bands, spacing, maturity shares and clearances come from the generated
  Constants.h (Rule 14). The PROPORTIONS here (taper, decay, angles, crown
  fractions) are asset geometry, not gameplay constants — same standing as the
  §5/§6 dimensions in ProcMesh (Rule 5: real content moves to data files later).
- FLORA ZONE (flora agent). Boundary agreed with render 09:08:2026: these files
  are flora's; ProcMesh/ScatterBatcher stay render's.
*/
/*
UPD:
- 09:08:2026 - 19:22:41: Created — catalog per LANDSCAPE §5.7-§5.10.
- 09:08:2026 - 20:21:13: Leaf-card vocabulary: FoliageShape::Card, the
  per-species atlas tone/shape bands and card proportions, has_leaf_cards().
- 10:08:2026 - 01:59:06: Dead-wood vocabulary for the §5.10 forest floor:
  SnagPale (the snag's second MATERIAL, not a second shape), stub/root-plate/
  moss fields on SpeciesParams.
- 10:08:2026 - 02:49:15: Design's acceptance asks: MOSS_TONE_B and the
  GRASS_BAND_REFERENCE proxy — every moss tone asserted a readable step below
  the grass band.
- 12:08:2026 - 00:20:00: GreatOak, CrownEnvelope::GreatCrown, the crown
  allometry / width-jitter fields, the fractal-grower parameter block, the
  climbing-furniture counts, and flora_control_arm().
- 12:08:2026 - 00:45:00: card_scrap_floor() — the card legibility floor becomes
  a FUNCTION so that there cannot be two of it. There were three (emitter,
  emitter's re-check, suite), they diverged, and the great oak emitted zero
  cards with the suite green.
- 13:08:2026 - 16:20:00: flora_envelope_arm() -- the zero-dose door for the
  crown's CONSTRUCTION alone (DFN_FLORA_CROWN=1), separate from
  flora_control_arm() because a control binds only the measurements in which it
  itself moves (Rule 48).
- 13:08:2026 - 19:55:00: flora_shyness_arm() -- a THIRD zero-dose door
  (DFN_FLORA_SHY=0), for crown shyness alone. DFN_FLORA_CROWN reverts how the
  crown is BUILT, so a pair taken across it would answer two questions at once
  and the canopy-overlap number is about one of them (Rule 48).
*/

#pragma once

#include "engine/render/sources/FloraCards.h"

#include <glm/vec3.hpp>

#include <cstdint>

namespace dfn::render {

/// Approved catalog (LANDSCAPE §5.7-§5.10). NOTE: there is deliberately no
/// "ElderOak" — a giant is DaleOak with maturity > 1 (design's ruling: one
/// system, not two).
enum class FloraSpecies : uint8_t {
    DaleOak = 0,
    HighlandPine = 1,
    RiverBirch = 2,
    ValeWillow = 3,
    Snag = 4,
    Bush = 5,
    BigBush = 6,
    FallenLog = 7,
    Deadfall = 8,
    /// THE SNAG SPLIT IS TWO MATERIALS ON ONE ASSET (design §5.10, recorded in
    /// flora.md §3.4): a pale snag alone in a meadow is a LANDMARK, a grey snag
    /// in a wood is weather. Snag = in-forest, grey-brown weathered, dense
    /// (SNAG_DENSITY_FOREST_*); SnagPale = open ground, bone-white, rare
    /// (SNAG_DENSITY_OPEN_*). The GEOMETRY of a given variant is byte-identical
    /// across the two — asserted in the suite — because that is what "the same
    /// asset got two materials" means; only the values differ.
    SnagPale = 9,
    /// §5.12 talus apron: a wind-formed dwarf conifer for the scree band under
    /// the cliffline. Same whorl generator as HighlandPine, krummholz numbers.
    StuntedPine = 10,
    // --- The RICH EDGE SET (в8/в19в): what lives on path margins, water
    // --- margins and among the talus. Flower palette roles are design's
    // --- ruling (10.08.2026): carpet / warm accent / rare jewel / pale umbel.
    MossPatch = 11,     ///< ground moss; also the shade-side dress on stones
    FlowerCarpet = 12,  ///< (a) common carpet — cool blue-violet, hue vs grass
    FlowerAccent = 13,  ///< (b) common warm accent — white/yellow, VALUE carrier
    FlowerJewel = 14,   ///< (c) RARE JEWEL — deep red; NEVER in common scatter
    FlowerUmbel = 15,   ///< (d) pale umbel — water margins, birch's ground echo
    Mushroom = 16,      ///< caps; placement rings/clumps via mushroom_ring_offsets
    PebbleCluster = 17, ///< small stones; path borders and scree texture
    /// THE GREAT OAK (user request 11.08.2026). A separate class, not DaleOak
    /// with a large maturity, and the difference is structural rather than one
    /// of degree: its branches are grown by RECURSIVE RAMIFICATION so the tree
    /// reads as a structure; its lower crown's radius equals its own height;
    /// people live in it, so it carries stair treads and platforms. Design's
    /// §5.8 ruling that "the Elder Oak IS the giant tier — one system, not two"
    /// was made about a tree that differed only in SIZE and it was right about
    /// that tree. This one differs in what it is made of.
    GreatOak = 18,
};
inline constexpr uint8_t FLORA_SPECIES_COUNT = 19;

/// The silhouette intent. Branch target lengths are clipped to this envelope so
/// the species read at SILHOUETTE_MIN_PX is GUARANTEED rather than emergent —
/// at 640x360 an accidental silhouette is a different silhouette every seed
/// (docs/specs/flora.md §3.1 stage D).
enum class CrownEnvelope : uint8_t {
    Sphere,  ///< oak: wider than tall, "ball on a stump"
    Cone,    ///< pine: narrow triangle, stacked tiers
    Vase,    ///< birch: narrow below, opening above
    Weeping, ///< willow: wide shoulder, falling skirt
    /// great oak: widest at the BOTTOM of the crown and doming over. The user's
    /// rule is «нижняя часть кроны в радиусе равна высоте» — a crown whose
    /// widest ring is its lowest one is the shape that statement describes, and
    /// it is not reachable by widening Sphere (which closes at both ends).
    GreatCrown,
    None,    ///< snag / log / deadfall: no foliage at all
};

enum class FoliageShape : uint8_t {
    Card,      ///< crossed alpha-cutout leaf cards (the canopy default, §3.8)
    Blob,      ///< faceted ellipsoid cluster (solid: bushes, and Silhouette LOD)
    ConeShell, ///< tier skirt (conifer)
    None,
};

/// Ground-cover build recipe. A patch species is a FORM plus numbers — the
/// same doctrine as trees (a species is a table row, never bespoke code), one
/// level down. All solid geometry: design ruled only TREE foliage is cards,
/// and at 0.1-0.8 m a card would be one shimmering pixel anyway (Rule 33).
enum class GroundForm : uint8_t {
    None = 0,
    MossDome,  ///< overlapping flattened domes hugging the ground
    HeadsTuft, ///< a green tuft carrying flower heads ON it (low flowers)
    HeadsStem, ///< visible stems with heads on top (tall flowers)
    Caps,      ///< mushroom: stem + flattened cap per element
    Stones,    ///< part-buried pebbles
};

/// One species. Everything the generator needs; nothing it does not.
struct SpeciesParams {
    // --- identity -----------------------------------------------------------
    const char* name = "";
    CrownEnvelope envelope = CrownEnvelope::Sphere;
    FoliageShape foliage = FoliageShape::Blob;

    // --- trunk --------------------------------------------------------------
    float height_min = 24.0f;        ///< m (from Constants.h at table build)
    float height_max = 32.0f;        ///< m
    float trunk_radius_frac = 0.022f;///< base radius / height
    float taper_exp = 1.0f;          ///< r(t) = r_base * (1-t)^taper_exp
    float trunk_sweep = 0.10f;       ///< rad of total bend along the trunk
    uint8_t trunk_count_min = 1;     ///< > 1 = clump (the user's "сколько стволов")
    uint8_t trunk_count_max = 1;
    float trunk_spread = 0.0f;       ///< m, base offset of clump stems
    uint8_t trunk_sides = 5;
    uint8_t trunk_segments = 6;

    // --- crown envelope -----------------------------------------------------
    float crown_base_frac = 0.40f;   ///< foliage starts here (CROWN_BASE_FRACTION_*)
    float crown_width_frac = 0.45f;  ///< crown DIAMETER / height
    /// CROWN ALLOMETRY (user request 11.08.2026: «большую часть деревьев
    /// сделать шире, но мелкие также добавить»). Crown diameter does not scale
    /// with height: D = D_nominal * maturity^exp while H = H_nominal * maturity,
    /// so the WIDTH-TO-HEIGHT ratio moves as maturity^(exp-1).
    ///
    /// Above 1 by measurement, not by taste. A broadleaf's height growth stops
    /// decades before its crown stops spreading — that is why a veteran oak is
    /// famously broader than tall while a pole-stage oak of the same species is
    /// not. Setting exp > 1 is the single lever that makes the user's two
    /// clauses one rule instead of two: the giants get much wider and the
    /// saplings stay narrow, so the SPREAD widens rather than the mean sliding.
    /// 1.0 = width proportional to height (conifers: a spruce really does hold
    /// its ratio, and a wide spruce is not a spruce).
    float crown_allometry_exp = 1.0f;
    /// Half-width of the per-instance crown-width draw, as a fraction. Width
    /// gets its OWN random axis: two trees of the same height that are also the
    /// same width read as one asset used twice, and that is the complaint.
    float crown_width_jitter = 0.0f;

    /// False for bushes: they have no branch skeleton worth growing, they ARE
    /// their foliage. Everything else grows a crown.
    bool has_skeleton = true;

    // --- branching: SPACE COLONIZATION (broadleaves) -------------------------
    // Runions, Lane & Prusinkiewicz 2007. See docs/specs/flora_algorithms.md §1.
    // The paper expresses di and dk as multiples of D, and so do we, because
    // that is the form in which its published values transfer between species of
    // different size.
    uint16_t attractors = 300;      ///< N. SMALL N gives IRREGULAR branches — wanted
    float colonize_step_frac = 0.16f; ///< D as a fraction of crown radius
    float influence_d = 9.0f;       ///< di/D. Paper: 8 for trees, 17 for shrubs
    float kill_d = 2.2f;            ///< dk/D. Paper: 2 (fine) .. 20 (smooth, sparse)
    /// 0 = attractors uniform through the crown, 1 = on its SHELL only. The
    /// paper's fig. 7 gives an open branch system with twigs limited to the
    /// crown surface, which is independently what flora.md §3.10 measured in the
    /// user's photographs: porosity is a RIM effect over a near-opaque core.
    float surface_bias = 0.55f;
    float pipe_exponent = 2.5f;     ///< n in r^n = sum(r_child^n); literature 2..3
    float fork_softening = 0.35f;   ///< the paper's post-process (e)
    /// Where the lowest BRANCH may leave the trunk, as a fraction of height.
    /// Deliberately BELOW crown_base_frac: a real bole sheds ascending limbs
    /// well under the foliage line, and a crown that begins exactly where the
    /// branches begin is the palm silhouette. Foliage still obeys crown_base.
    float branch_base_frac = 0.34f;

    // --- branching: FRACTAL (the great oak) ---------------------------------
    // The third growth model (FloraSkeleton.h, fractal_skeleton). Zero here for
    // every species that does not use it.
    uint8_t fractal_depth = 0;       ///< 0 = species does not grow fractally
    uint8_t fractal_majors_min = 2;  ///< first-order limbs; 2 = two-lobed
    uint8_t fractal_majors_max = 5;  ///< 5 = broad dome. The silhouette variety
    uint8_t fractal_children_min = 2;
    uint8_t fractal_children_max = 3;
    float fractal_major_pitch = 0.95f;  ///< rad from vertical, first order
    float fractal_pitch_spread = 0.45f; ///< rad, divergence per fork
    float fractal_length_decay = 0.72f;
    /// THE USER'S GREAT-OAK RULE, as one number: lower-crown RADIUS divided by
    /// tree height. 1.0 means a 40 m oak carries an 80 m wide crown — «редкие и
    /// очень большие как горы». It replaces crown_width_frac for fractal
    /// species precisely because it is stated as a radius and against HEIGHT,
    /// and re-expressing it as a diameter fraction is where a factor of two
    /// goes missing.
    float crown_radius_per_height = 0.0f;
    /// Climbable furniture: stair treads up the bole and platforms at the major
    /// forks. GEOMETRY ONLY — this zone hands over shapes at usable heights and
    /// spacings; collision, habitation and ladders belong to sim/core/design.
    uint8_t climb_steps = 0;      ///< treads spiralling up the bole (0 = none)
    uint8_t climb_platforms = 0;  ///< decks at the first-order forks

    // --- branching: WHORLS (conifers) ---------------------------------------
    // A WHORL IS A YEAR (flora_algorithms.md §3.2). Spacing is the year's height
    // increment, branch count tracks the same vigour, and the lower whorls are
    // self-pruned. None of that is expressible as a stack of cone shells.
    uint8_t whorl_count = 8;
    uint8_t whorl_branches_min = 3;   ///< a "complete" whorl is >= 3
    uint8_t whorl_branches_max = 6;   ///< an average whorl carries 2-7
    float whorl_miss_bottom = 0.42f;  ///< self-pruning: the old whorls lose most
    float whorl_miss_top = 0.06f;
    float whorl_angle_top = 1.00f;    ///< rad above horizontal, near the leader
    float whorl_angle_bottom = -0.24f;///< the oldest branches sag past horizontal
    uint8_t whorl_shoots = 2;         ///< pendulous second-order shoots per branch
    /// Where the dead-stub band starts, as a fraction of the bare bole. Below it
    /// the stem is clean; between it and the crown base a conifer carries dead
    /// stubs. It is the difference between a trunk and a pole.
    float stub_band_frac = 0.55f;
    uint8_t whorl_stubs = 5;

    float phototropism = 0.35f;      ///< +Y component of eq. (3)'s tropism vector
    float droop = 0.10f;             ///< -Y component; high = willow, conifer sag
    float min_branch_diameter = 0.35f; ///< SHADOW FLOOR — see docs/specs/flora.md §3.5

    // --- ground cover (the §5.10/в19в patch classes) ------------------------
    // A patch is a FORM plus numbers. `height_min/max` doubles as the patch's
    // vertical size; the fields below carry the rest.
    GroundForm ground_form = GroundForm::None;
    uint8_t ground_elements = 0;   ///< heads / caps / stones per patch
    float element_radius = 0.06f;  ///< m, one head/cap/stone
    float element_aspect = 1.1f;   ///< head height / radius (0.35 = flat plate)
    float patch_radius = 0.5f;     ///< m, footprint the elements scatter over
    glm::vec3 accent_color{0.5f};   ///< head/cap/stone colour
    glm::vec3 accent_color_b{0.5f}; ///< its variation tone

    // --- dead wood (snags and fallen logs) ----------------------------------
    // A snag is NOT a tree with zero leaves and a log is NOT a floating
    // cylinder — both need the marks of a DEAD tree: truncated limb stubs, a
    // broken (blunt, splintered) top, an upturned root plate, moss on the side
    // the rain hits. Fields are zero for everything alive.
    uint8_t stub_count = 0;      ///< truncated dead limbs on the bole/log
    float stub_len_frac = 0.0f;  ///< stub length as a fraction of height/length
    bool root_plate = false;     ///< fallen log: upturned root disc at the butt
    /// Fraction of UP-facing surface carrying moss (logs; 0 = none). Moss lives
    /// on the upper side because that is where light and rain land; the
    /// underside gate is asserted in the suite.
    float moss_cover = 0.0f;
    glm::vec3 moss_color{0.20f, 0.32f, 0.12f};

    // --- foliage ------------------------------------------------------------
    uint8_t cluster_count = 22;
    float cluster_radius_frac = 0.30f; ///< cluster radius / crown radius
    uint8_t cluster_slices = 5;
    uint8_t cluster_bands = 2;

    // --- foliage cards (docs/specs/flora.md §3.8) ---------------------------
    // A card's atlas TILE is its (shape, colour) pair, so a species declares a
    // band of each and the generator varies them per card. Colour therefore
    // costs no vertex bytes and is NOT welded to shape: the same leaf outline
    // appears light and dark in one crown, which is what a crown that is
    // 79-86 % leaf in its core needs in order to read as volume (§3.10).
    LeafTone tone_first = LeafTone::OakMid;
    uint8_t tone_count = 3;
    LeafShape card_shape_a = LeafShape::RoundLobed;
    LeafShape card_shape_b = LeafShape::RaggedTip;
    uint8_t cards_per_cluster = 3;   ///< crossed cards sharing one centre
    float card_width_frac = 1.15f;   ///< card half-width / cluster radius
    float card_aspect = 0.80f;       ///< half-height / half-width

    // --- value (LANDSCAPE §5 palette roles) ---------------------------------
    glm::vec3 trunk_color{0.14f, 0.11f, 0.08f};
    /// Colour of the THIN wood — anything under about a third of the trunk
    /// radius. Not a detail: §3.10 measured that the tracery in the user's
    /// reference photographs reads by VALUE CONTRAST (branch 50, leaf 135, a
    /// 2.54x ratio), not by transparency. A species whose limbs share its bole's
    /// value has no tracery at all — the birch's near-white branches read as a
    /// white wire frame with leaves stuck on it, which is the one thing left of
    /// the palm after the shape was fixed. Real birches do this too: the bole is
    /// white and the twigs are dark brown.
    glm::vec3 twig_color{0.10f, 0.08f, 0.06f};
    /// Wood thinner than this fraction of the trunk radius takes `twig_color`.
    /// Per species because the boundary is a real botanical one and it sits in a
    /// different place on each: an oak's bark runs out onto its limbs, a birch's
    /// white bark stops at the bole and everything above it is dark brown. At
    /// 640x360 a birch whose limbs keep the bole's value reads as a white wire
    /// frame with leaves stuck on it.
    float twig_radius_frac = 0.34f;
    glm::vec3 foliage_color{0.30f, 0.42f, 0.18f};

    // --- neighbour response (docs/specs/flora.md §3.3) ----------------------
    float shyness = 0.25f;       ///< 0..1 crown pullback toward a crowding neighbour
    float lean_response = 0.10f; ///< rad per unit crowding
};

/// Moss's brighter variation tone, as a multiplier on the base moss colour.
/// ONE number for the log moss pass, the MossPatch species and the suite —
/// design's acceptance rule is that EVERY moss tone stays a readable step
/// DARKER than the grass band (luminance <= grass - 0.05, asserted), so the
/// variation multiplier is part of that contract, not a local flourish.
inline constexpr float MOSS_TONE_B = 1.18f;

/// The grass-band reference the moss rule (and the flower value roles) are
/// measured against. A PROXY until the shipped ground grass value is a named
/// registry row — when render's grass colour lands in NUMBERS.md, this reads
/// it and the literal dies (Rule 14; flagged in the Task 4 grass report).
inline constexpr glm::vec3 GRASS_BAND_REFERENCE{0.30f, 0.42f, 0.18f};

/// THE ZERO-DOSE CONTROL ARM (Rule 30/48), and it lives here because three
/// files have to agree about it. `DFN_FLORA_CONTROL=1` puts the flora zone back
/// to its 11.08.2026 behaviour — flat maturity from core's scatter scale, no
/// crown allometry, no per-instance width draw, no wind lean, the old
/// mid-crown-widest envelope profile — WITHOUT rebuilding.
///
/// A rebuilt binary is not the same control: it also changes the terrain seed
/// stamp, the shader set, and everything else that moved in between, so a
/// BEFORE/AFTER pair taken across a rebuild answers "did anything change" and
/// not "did THIS change". One binary, one pose, one variable.
[[nodiscard]] bool flora_control_arm();

/// THE ZERO-DOSE ARM FOR THE CROWN'S CONSTRUCTION, and it is a SECOND door on
/// purpose (Rule 48: a control binds only the measurements in which it itself
/// moves). `flora_control_arm()` above puts back the width, the maturity draw,
/// the lean and the envelope profile all at once — it answers "did the
/// 11.08.2026 stage change this", which is not the question here.
///
/// `DFN_FLORA_CROWN=1` changes exactly ONE thing: the card species go back to
/// growing their crowns by space colonization into the envelope and hanging
/// their foliage on MERGED CLOUD CENTRES (FloraSkeleton's
/// gather_foliage_anchors), instead of ramifying and hanging their foliage on
/// the SHOOTS (gather_shoot_anchors). Everything else — species widths, heights,
/// leans, card sizes, tone bands, budgets — is byte-identical between the arms,
/// so a BEFORE/AFTER pair off one binary answers "did leaves-from-branches do
/// this" and nothing else.
[[nodiscard]] bool flora_envelope_arm();

/// THE ZERO-DOSE ARM FOR CROWN SHYNESS ALONE. `DFN_FLORA_SHY=0` turns off the
/// neighbour boundaries and nothing else.
///
/// A THIRD DOOR, AND THE REASON IS THE SAME EACH TIME (Rule 48): a control
/// binds only the measurements in which it itself moves. `DFN_FLORA_CROWN`
/// reverts how the crown is BUILT, so a before/after taken across it answers
/// "did ramification plus shyness do this" — which is two questions, and the
/// canopy-overlap number is about one of them. Shyness has its own door so its
/// own quantity has its own arm.
[[nodiscard]] bool flora_shyness_arm();

/// THE CARD SCRAP FLOOR — the smallest half-width a leaf card may be emitted at
/// before it stops reading as part of a crown and starts reading as a detached
/// scrap hanging under one.
///
/// IT IS A FUNCTION SO THAT THERE CANNOT BE TWO OF IT. There were three: the
/// emitter checked it, then re-checked it after its own clamps, and the suite
/// restated it a third time. On 12.08.2026 the first was re-derived for very
/// large crowns and the other two were not, so the great oak passed one gate
/// and was rejected by the next — ZERO cards on every variant, a green suite,
/// and two acceptance frames misread as "sparse foliage" when the crown did
/// not exist at all. A shared rule that is spelled out per site is a rule with
/// a countdown on it (Rule 32).
///
/// THE MINIMUM OF TWO FORMS, and both terms are load-bearing. `0.22 * crown_r`
/// is the value measured on the 10 m crowns this floor was written against;
/// scaled to a 40 m crown it demands a bigger card than the species produces.
/// `0.55 * nominal cluster half-width` says the same thing about the quantity
/// that generalises — has containment shrunk this card to a scrap OF WHAT IT
/// WAS MEANT TO BE — and agrees with the first to two figures where the first
/// was calibrated. Taking the MINIMUM means the correction can only relax the
/// floor where the old form was absurd, never tighten it anywhere: swapping
/// them outright cost the birch 65 % of its foliage in one measured run.
[[nodiscard]] float card_scrap_floor(const SpeciesParams& sp, float crown_radius);

/// The catalog. Stable reference for the process lifetime.
[[nodiscard]] const SpeciesParams& species_params(FloraSpecies species);

/// True for the species that carry a canopy and obey CANOPY_CLEARANCE_MIN.
[[nodiscard]] bool is_canopy_tree(FloraSpecies species);

/// True when this species' foliage is alpha-cutout cards (i.e. it contributes
/// to the FOLIAGE material stream, not the opaque one).
[[nodiscard]] bool has_leaf_cards(FloraSpecies species);

} // namespace dfn::render
