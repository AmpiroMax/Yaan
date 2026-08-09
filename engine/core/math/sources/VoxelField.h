/*
Created: 09:08:2026 - 16:00:00
Last updated: 09:08:2026 - 21:37:57
Module: engine/core/math
File: engine/core/math/sources/VoxelField.h

Responsibility:
- Stage-4 ADDITIVE cross-zone contract for true 3D terrain: the extracted
  voxel surface mesh handed from engine/world to engine/render (drawing) and
  engine/physics (static collision). Lives in core/math for the same reason
  HeightFieldView does — world, render and physics are DAG siblings.

Key items:
- VoxelMaterial: per-voxel material carried through to the mesh.
- VoxelMeshView: non-owning view over one chunk's extracted surface.

Dependencies:
- Uses: glm, <span>, <cstdint>.
- Used by: world::ChunkManager (producer), engine/render (mesh upload),
  engine/physics (MeshShape) — agreed contract, additive to HeightFieldView.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- HeightFieldView is NOT replaced by this. It remains the ground-height query
  for scatter, pads, corridors, validation and the tour. This view carries
  GEOMETRY, including geometry a heightfield cannot express (ceilings,
  overhangs) — that is the entire point of it existing.
- Same lifetime rules as HeightFieldView: valid from ChunkLoaded until AFTER
  ChunkUnloaded has been dispatched.
- Triangles are already in world space; there is no per-chunk transform to
  apply. Winding is counter-clockwise when seen from the solid side.
*/
/*
UPD:
- 09:08:2026 - 16:00:00: Created — voxel surface handoff for the 3D terrain
  stage (representation swap).
- 09:08:2026 - 16:30:44: Representation swap: VoxelMeshView + VoxelMaterial — the additive cross-zone geometry handoff (HeightFieldView untouched, still the ground-height query).
- 09:08:2026 - 21:37:57: ADDITIVE: VoxelMeshView::sky_visibility (per-vertex, 255=open sky, 0=sealed) at render's request; an empty span means unknown and render falls back to 255, so the field lands before it is filled. No existing field moved.
*/

#pragma once

#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <span>

namespace dfn::math {

/// Material of a voxel, carried onto the extracted vertices so render can
/// texture caves differently from the open surface. Append-only.
enum class VoxelMaterial : uint8_t {
    Air = 0,   ///< never appears on a vertex; the empty half-space
    Grass = 1,
    Rock = 2,
    Sand = 3,
    Dirt = 4,  ///< sub-surface fill and carved cave walls
};

/// Non-owning view over one chunk's extracted surface mesh.
///
/// Conventions:
/// - Right-handed, Y up, +X east, +Z south — same as HeightFieldView.
/// - Positions are WORLD space, meters.
/// - `indices` are triangles (3 per face) into `positions`/`normals`/
///   `materials`, which are parallel arrays of equal length.
/// - An empty mesh (no spans) is legal: a chunk can be entirely air or
///   entirely solid.
struct VoxelMeshView {
    glm::ivec2 chunk_coord{0, 0};
    std::span<const glm::vec3> positions;
    std::span<const glm::vec3> normals;
    std::span<const uint8_t> materials; ///< VoxelMaterial per vertex
    std::span<const uint32_t> indices;

    /// Per-vertex sky visibility: 255 = open sky, 0 = sealed under rock.
    /// Parallel to `positions` when present. Render multiplies ambient AND
    /// moonlight by this, so without it a barrow interior is lit by full open
    /// daylight and no torch can read against it.
    ///
    /// An EMPTY span is legal and means "unknown": render falls back to 255,
    /// which is exactly today's behaviour. That is deliberate, so the field
    /// can land before it is filled.
    std::span<const uint8_t> sky_visibility;

    [[nodiscard]] std::size_t triangle_count() const { return indices.size() / 3; }
};

} // namespace dfn::math
