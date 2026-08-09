/*
Created: 09:08:2026 - 19:22:41
Last updated: 09:08:2026 - 19:22:41
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
*/

#pragma once

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
    Blob,      ///< faceted ellipsoid cluster (broadleaf)
    ConeShell, ///< tier skirt (conifer)
    None,
};

inline constexpr int FLORA_MAX_GENERATIONS = 2;

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

    // --- branching ----------------------------------------------------------
    uint8_t generations = 2;
    bool whorled = false;            ///< conifers branch in whorls, not spirals
    uint8_t branch_count[FLORA_MAX_GENERATIONS] = {5, 3};
    float branch_angle[FLORA_MAX_GENERATIONS] = {1.05f, 0.85f};      ///< rad from parent
    float branch_start_frac[FLORA_MAX_GENERATIONS] = {0.42f, 0.35f}; ///< along parent
    float length_decay[FLORA_MAX_GENERATIONS] = {0.42f, 0.55f};
    float radius_ratio[FLORA_MAX_GENERATIONS] = {0.38f, 0.45f};
    float phototropism = 0.35f;      ///< per unit length, blend toward +Y / light
    float droop = 0.10f;             ///< negative = conifer upsweep, high = willow
    float min_branch_diameter = 0.35f; ///< SHADOW FLOOR — see docs/specs/flora.md §3.5

    // --- foliage ------------------------------------------------------------
    uint8_t cluster_count = 22;
    float cluster_radius_frac = 0.30f; ///< cluster radius / crown radius
    uint8_t cluster_slices = 5;
    uint8_t cluster_bands = 2;

    // --- value (LANDSCAPE §5 palette roles) ---------------------------------
    glm::vec3 trunk_color{0.14f, 0.11f, 0.08f};
    glm::vec3 foliage_color{0.30f, 0.42f, 0.18f};

    // --- neighbour response (docs/specs/flora.md §3.3) ----------------------
    float shyness = 0.25f;       ///< 0..1 crown pullback toward a crowding neighbour
    float lean_response = 0.10f; ///< rad per unit crowding
};

/// The catalog. Stable reference for the process lifetime.
[[nodiscard]] const SpeciesParams& species_params(FloraSpecies species);

/// True for the species that carry a canopy and obey CANOPY_CLEARANCE_MIN.
[[nodiscard]] bool is_canopy_tree(FloraSpecies species);

} // namespace dfn::render
