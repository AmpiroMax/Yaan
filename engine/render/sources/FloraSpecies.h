/*
Created: 09:08:2026 - 19:22:41
Last updated: 09:08:2026 - 20:21:13
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
};
inline constexpr uint8_t FLORA_SPECIES_COUNT = 9;

/// The silhouette intent. Branch target lengths are clipped to this envelope so
/// the species read at SILHOUETTE_MIN_PX is GUARANTEED rather than emergent —
/// at 640x360 an accidental silhouette is a different silhouette every seed
/// (docs/specs/flora.md §3.1 stage D).
enum class CrownEnvelope : uint8_t {
    Sphere,  ///< oak: wider than tall, "ball on a stump"
    Cone,    ///< pine: narrow triangle, stacked tiers
    Vase,    ///< birch: narrow below, opening above
    Weeping, ///< willow: wide shoulder, falling skirt
    None,    ///< snag / log / deadfall: no foliage at all
};

enum class FoliageShape : uint8_t {
    Card,      ///< crossed alpha-cutout leaf cards (the canopy default, §3.8)
    Blob,      ///< faceted ellipsoid cluster (solid: bushes, and Silhouette LOD)
    ConeShell, ///< tier skirt (conifer)
    None,
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

/// The catalog. Stable reference for the process lifetime.
[[nodiscard]] const SpeciesParams& species_params(FloraSpecies species);

/// True for the species that carry a canopy and obey CANOPY_CLEARANCE_MIN.
[[nodiscard]] bool is_canopy_tree(FloraSpecies species);

/// True when this species' foliage is alpha-cutout cards (i.e. it contributes
/// to the FOLIAGE material stream, not the opaque one).
[[nodiscard]] bool has_leaf_cards(FloraSpecies species);

} // namespace dfn::render
