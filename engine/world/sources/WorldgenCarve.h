/*
Created: 09:08:2026 - 16:45:00
Last updated: 09:08:2026 - 17:36:42
Module: engine/world
File: engine/world/sources/WorldgenCarve.h

Responsibility:
- Pass P7 (3D terrain): the carved volumes that a heightfield cannot express —
  the switchback tunnel up Ravenscar Crag (LANDSCAPE §7 / user request в23,
  modelled on Skyrim's Seven Thousand Steps) and the Backbarrow interior.
  Expressed as signed distance fields subtracted from the terrain SDF.

Key items:
- CarveCorridor: a walkable corridor along a polyline (flat floor, flat
  ceiling, real headroom) — the shape both carves are built from.
- carve_distance(): the union SDF of every carved volume (negative inside).
- carve_column_range(): the y span a column's carves occupy, so the voxel
  builder can widen that column's active band.

Dependencies:
- Uses: TestbedLayout.h, glm.
- Used by: VoxelVolume (subtraction), validation, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- A corridor polyline may START AND END IN OPEN AIR on purpose: carving air out
  of air is a no-op, so the portal forms exactly where the path meets rock.
  That is how the mouths are made — do not "optimise" the outside segments away.
- FLAT FLOOR, not a tube. A capsule tunnel gives a rounded floor that reads as
  a burrow and walks badly; the cross-section here is a box (flat floor, flat
  ceiling) so the corridor reads as cut rock and the player stands upright.
- Deterministic: pure geometry, no rng.
*/
/*
UPD:
- 09:08:2026 - 16:45:00: Created — P7 carve pass for the 3D terrain stage.
- 09:08:2026 - 16:47:51: Created — P7 carve SDF: box cross-section corridors (flat floor, real headroom) and chambers, plus the per-column range the voxel builder needs to widen its band.
- 09:08:2026 - 17:36:42: §6.2: carve_mouth / site_carve_mouth (entrance markers derived from the mouth, never scored) and carve overloads taking derived corridors.
*/

#pragma once

#include "engine/world/sources/TestbedLayout.h"

#include <functional>
#include <glm/vec3.hpp>
#include <optional>
#include <span>
#include <utility>

namespace dfn::world {

/// Signed distance to the union of all carved volumes at `world`
/// (negative = inside carved air). Returns a large positive value far away.
[[nodiscard]] float carve_distance(const TestbedLayout& layout, glm::vec3 world);

/// Same, including DERIVED corridors (the §6.2 entrance adits).
[[nodiscard]] float carve_distance(const TestbedLayout& layout,
                                   std::span<const CarveCorridor> extra, glm::vec3 world);

/// The vertical span carved volumes occupy in the column at `world_xz`, as
/// (lo, hi) in meters. Returns (1, -1) — an empty range — when the column
/// touches no carve. Used to widen the voxel band; a carve outside the band
/// would simply not exist.
[[nodiscard]] std::pair<float, float> carve_column_range(const TestbedLayout& layout,
                                                         glm::vec2 world_xz);
[[nodiscard]] std::pair<float, float> carve_column_range(const TestbedLayout& layout,
                                                         std::span<const CarveCorridor> extra,
                                                         glm::vec2 world_xz);

/// True if any carve exists in this layout (lets the builder skip the work).
[[nodiscard]] bool has_carves(const TestbedLayout& layout);

/// Terrain height sampler (macro + carve, WITHOUT pads — pads are what P4 is
/// still deciding when this is called).
using GroundSampler = std::function<float(glm::vec2)>;

/// Where a corridor stops being open to the sky and rock closes overhead: the
/// real entrance. `outward` points back out of the hill, i.e. the direction an
/// arriving player faces the opening from.
struct CarveMouth {
    glm::vec3 position{0.0f};
    glm::vec2 outward{0.0f, 1.0f};
};

/// Finds the mouth of `corridor`, or nullopt when the corridor never goes
/// under rock at all (a carve entirely in the open is not an entrance).
[[nodiscard]] std::optional<CarveMouth> carve_mouth(const CarveCorridor& corridor,
                                                    const GroundSampler& ground);

/// The mouth belonging to site `site_index`, or nullopt when that site has no
/// carve. THIS is what P4 uses: a carved entrance is derived, never scored.
[[nodiscard]] std::optional<CarveMouth> site_carve_mouth(const TestbedLayout& layout,
                                                         int site_index,
                                                         const GroundSampler& ground);

} // namespace dfn::world
