/*
Created: 09:08:2026 - 19:26:55
Last updated: 09:08:2026 - 19:26:55
Module: engine/render
File: engine/render/sources/ProcFlora.h

Responsibility:
- Public API of the parametric flora generator: build a species mesh from
  (species, variant, per-instance shape, LOD), and derive per-instance shapes
  from a neighbourhood of scatter instances.

Key items:
- FloraLod, FloraShape, build_flora_mesh(), analyse_neighbourhood(),
  species_nominal_height/crown_radius/crown_base/trunk_radius(),
  FLORA_VARIANTS.

Dependencies:
- Uses: FloraSpecies.h, ProcMesh.h (MeshData + tri/quad/pack, render's, public
  by agreement 09:08:2026), core math ScatterInstance, glm.
- Used by: ScatterBatcher (render, via a diff flora hands over), ProcFloraTests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md.
- PURE AND DETERMINISTIC: no GPU, no ECS, no globals, no wall-clock, no IO.
  Same inputs must give byte-identical buffers (Rule 13.1 discipline).
- Does NOT decide placement (core), the catalog (design), LOD SELECTION,
  instancing or materials (render), or collision (sim).
*/
/*
UPD:
- 09:08:2026 - 19:26:55: Created — stage-4 parametric branching system.
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
};

/// The canonical builder. Deterministic in every argument.
[[nodiscard]] MeshData build_flora_mesh(FloraSpecies species, uint32_t variant,
                                        const FloraShape& shape, FloraLod lod);

/// Variant index for a world position (stable across runs and chunk borders).
[[nodiscard]] uint32_t flora_variant_for(glm::vec2 world_xz);

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
