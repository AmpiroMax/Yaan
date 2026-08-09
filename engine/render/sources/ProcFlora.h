/*
Created: 09:08:2026 - 19:26:55
Last updated: 10:08:2026 - 01:59:06
Module: engine/render
File: engine/render/sources/ProcFlora.h

Responsibility:
- Public API of the parametric flora generator: build a species mesh from
  (species, variant, per-instance shape, LOD), and derive per-instance shapes
  from a neighbourhood of scatter instances.

Key items:
- FloraLod, FloraShape, FloraMesh, build_flora_mesh(), append_flora(),
  analyse_neighbourhood(),
  species_nominal_height/crown_radius/crown_base/trunk_radius(),
  FLORA_VARIANTS.

Dependencies:
- Uses: FloraSpecies.h, FloraCards.h, ProcMesh.h (MeshData + tri/quad/pack,
  render's, public by agreement 09:08:2026), core math ScatterInstance, glm.
- Used by: ScatterBatcher (render), RenderSystem (the leaf atlas),
  ProcFloraTests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md.
- PURE AND DETERMINISTIC: no GPU, no ECS, no globals, no wall-clock, no IO.
  Same inputs must give byte-identical buffers (Rule 13.1 discipline).
- Does NOT decide placement (core), the catalog (design), LOD SELECTION,
  instancing or materials (render), or collision (sim).
- A tree comes out as TWO meshes and they must never be merged: see FloraMesh.
*/
/*
UPD:
- 09:08:2026 - 19:26:55: Created — stage-4 parametric branching system.
- 09:08:2026 - 20:21:13: Alpha-cutout leaf cards: build_flora_mesh now returns
  FloraMesh (the wood stream and the card stream, which carry different
  vertex-colour meanings and so must be different draws); append_flora() for
  the batcher; FloraShape::wind_phase; season argument.
- 10:08:2026 - 01:59:06: flora_maturity_for() — the §5.10 maturity-tier draw
  gets its one home, for core to call when filling ScatterInstance.scale.
*/

#pragma once

#include "engine/core/math/sources/SurfaceField.h"
#include "engine/render/sources/FloraSpecies.h"
#include "engine/render/sources/ProcMesh.h"

#include <glm/vec2.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace dfn::render {

/// Geometry detail level. SELECTION IS RENDER'S DECISION — flora only supplies
/// the geometry for each level (docs/specs/flora.md §3.6).
enum class FloraLod : uint8_t {
    Full = 0,       ///< complete skeleton + clusters, <= TREE_TRI_BUDGET_MAX
    Reduced = 1,    ///< trunk + primaries + fewer clusters
    Silhouette = 2, ///< trunk column + one envelope shell
};

/// Pre-built skeleton variants per species. The whole trick that makes
/// unique-looking forests affordable: cost is O(species x variants x lods),
/// never O(instances), while FloraShape supplies per-instance difference.
inline constexpr uint32_t FLORA_VARIANTS = 12;

/// Per-instance shape modifiers. Derived from the neighbourhood, NOT carried on
/// math::ScatterInstance — that contract is frozen (Rule 26) and gains nothing.
struct FloraShape {
    float maturity = 1.0f;     ///< 0.4 sapling .. 1.5 giant; scales height
    glm::vec2 lean_dir{0.0f};  ///< unit; direction the crown leans toward
    float lean = 0.0f;         ///< rad (crowding lean; cliff lean is separate)
    glm::vec2 shy_dir{0.0f};   ///< unit; direction of strongest crowding
    float shyness = 0.0f;      ///< 0..1 crown pullback along shy_dir
    bool understory = false;   ///< raise crown base, narrow crown, shorten
    /// 0..1, this instance's wind phase (vertex GREEN). Derived from the
    /// instance POSITION in analyse_neighbourhood, because a stand whose trees
    /// share a phase does not ripple, it pulses as one object.
    float wind_phase = 0.0f;
};

/// A tree is TWO meshes, and merging them is a bug that looks like an
/// optimisation. On render's "prop" program vertex colour is ALBEDO; on their
/// "foliage" program the same four bytes are WIND DATA (r sway weight, g
/// instance phase, b per-card value jitter, a sky visibility) and the albedo
/// comes from the leaf atlas instead. Same bytes, different meaning, therefore
/// different draws.
struct FloraMesh {
    MeshData wood;  ///< trunk, branches, cone tiers, silhouette shells ("prop")
    MeshData cards; ///< alpha-cutout leaf cards ("foliage" + the leaf atlas)
};

/// The canonical builder. Deterministic in every argument.
///
/// `season` changes GEOMETRY only for winter, where deciduous species emit no
/// cards at all (LANDSCAPE §5.11's one boolean) and the bare skeleton becomes
/// the tree. Summer and autumn produce identical geometry: the card stores an
/// atlas TILE, the atlas stores the colour, so those two seasons are a texture
/// regeneration and nothing else — no mesh, no chunk, no baked jitter.
[[nodiscard]] FloraMesh build_flora_mesh(FloraSpecies species, uint32_t variant,
                                         const FloraShape& shape, FloraLod lod,
                                         FloraSeason season = FloraSeason::Summer);

/// Bakes one instance into the two world-space streams (the batcher's entry
/// point). Scale is 1.0 by contract: maturity is already inside the mesh, and
/// trees do not sink — they stand on their root flare (§3.5).
void append_flora(MeshData& wood, MeshData& cards, FloraSpecies species,
                  uint32_t variant, const FloraShape& shape, FloraLod lod,
                  glm::vec3 position, float yaw,
                  FloraSeason season = FloraSeason::Summer);

/// Variant index for a world position (stable across runs and chunk borders).
[[nodiscard]] uint32_t flora_variant_for(glm::vec2 world_xz);

/// The MATURITY-TIER DRAW (design §5.10: TREE_MATURITY_GIANT/MATURE/SUBMATURE/
/// YOUNG_PCT = 25/60/12/3, until now sixteen constants with zero consumers).
/// Returns the FloraShape::maturity multiplier for a tree standing at this
/// position: giant 1.15-1.50, mature 0.85-1.15, sub-mature 0.50-0.70 (design's
/// mid-canopy layer — do NOT collapse it into sapling, they are different
/// structural jobs), sapling 0.40-0.60. Deterministic and position-keyed like
/// flora_variant_for, so core can call it when filling ScatterInstance.scale
/// and the rule has exactly one home (Rule 35). Distribution is asserted over
/// its whole declared range in the suite (Rule 31).
[[nodiscard]] float flora_maturity_for(glm::vec2 world_xz);

/// Derives a FloraShape per instance from the instance array itself.
/// `all` may include neighbouring-chunk instances; results are returned for
/// the first `count` entries (pass instances.size() for all of them).
[[nodiscard]] std::vector<FloraShape>
analyse_neighbourhood(std::span<const math::ScatterInstance> all, size_t count);

/// Maps core's placement species onto the flora catalog.
[[nodiscard]] FloraSpecies flora_species_of(math::ScatterSpecies species);

// --- Metadata other zones need without pulling in the tables ---------------
[[nodiscard]] float species_nominal_height(FloraSpecies);
[[nodiscard]] float species_crown_radius(FloraSpecies);
[[nodiscard]] float species_crown_base(FloraSpecies);
/// Base trunk radius INCLUDING the root flare's widest point (sim's collision
/// query — see docs/specs/flora.md §3.5).
[[nodiscard]] float species_trunk_radius(FloraSpecies);

} // namespace dfn::render
