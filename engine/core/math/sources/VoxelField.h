/*
Created: 09:08:2026 - 16:00:00
Last updated: 10:08:2026 - 21:05:10
Module: engine/core/math
File: engine/core/math/sources/VoxelField.h

Responsibility:
- Stage-4 ADDITIVE cross-zone contract for true 3D terrain: the extracted
  voxel surface mesh handed from engine/world to engine/render (drawing) and
  engine/physics (static collision). Lives in core/math for the same reason
  HeightFieldView does — world, render and physics are DAG siblings.

Key items:
- VoxelMaterial: per-voxel material carried through to the mesh.
- voxel_material_of(SurfaceClass): THE projection of the design-truth surface
  class onto the voxel material. One definition, called by both zones.
- VoxelMeshView: non-owning view over one chunk's extracted surface.

Dependencies:
- Uses: glm, <span>, <cstdint>, SurfaceField.h (SurfaceClass, same module).
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
- THE SURFACE-CLASS PROJECTION LIVES HERE AND NOWHERE ELSE. It used to be a
  private switch inside world's VoxelVolume.cpp, which meant the heightfield
  mesher and the voxel mesher each held their own idea of what a surface class
  looks like — and they drifted the moment SurfaceClass gained a member this
  enum lacked (see UPD). Both zones call this function now. Adding a
  SurfaceClass without a home here is a compile error, which is the point.
*/
/*
UPD:
- 09:08:2026 - 16:00:00: Created — voxel surface handoff for the 3D terrain
  stage (representation swap).
- 09:08:2026 - 16:30:44: Representation swap: VoxelMeshView + VoxelMaterial — the additive cross-zone geometry handoff (HeightFieldView untouched, still the ground-height query).
- 09:08:2026 - 21:37:57: ADDITIVE: VoxelMeshView::sky_visibility (per-vertex, 255=open sky, 0=sealed) at render's request; an empty span means unknown and render falls back to 255, so the field lands before it is filled. No existing field moved.
- 10:08:2026 - 21:05:10: RULE 39 DEFECT, FIXED AT THE ROOT. VoxelMaterial had no
  member for SurfaceClass::GrassRockBlend, so world's private projection folded
  the blend into plain Grass and the class was destroyed at the hop. The two
  meshers then disagreed on the same shader input: the heightfield path drew
  the blend with rock weight 0.5 and the voxel path with 0.0, on the same
  chunk-load branch (App.cpp:532 vs :535). MEASURED on the seed-1 testbed (4x4
  chunks): 3536 of 266256 surface samples (1.33%) are GrassRockBlend, and
  26136 of 1183258 voxel vertices (2.21%) stand on one — they were all drawn as
  grass. ADDITIVE: GrassRockBlend = 5 appended (no ordinal moved; the uint8_t
  values are a silent cross-DAG contract), and the projection is exported as
  voxel_material_of() so render can key ONE splat table off it. Agreed with
  render before landing; render takes the mesher-side consolidation.
*/

#pragma once

#include "engine/core/math/sources/SurfaceField.h"

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
    /// The SurfaceClass of the same name, carried intact across the hop. It
    /// exists because its absence WAS the defect: without a member here the
    /// projection below had nowhere to put the blend and silently answered
    /// Grass, so 2.25% of the voxel terrain drew with the wrong splat weight.
    GrassRockBlend = 5,
};

/// THE projection of a design-truth surface class onto a voxel material.
///
/// World stamps this onto the voxel skin; render keys its splat weights off
/// the result. It is deliberately total and deliberately has NO `default:` —
/// a SurfaceClass added without a case here is a -Wswitch warning at every
/// call site, which is the mechanism this project prefers over remembering.
///
/// One projection is LOSSY and it is named rather than hidden: WaterBed and a
/// carved cave wall both land on Dirt. That is free only while both want the
/// same splat weight (bed 1.0), which is true today. The day a river bed must
/// look unlike a tunnel wall, VoxelMaterial needs its own WaterBed member and
/// this is the function that will need the row (render's finding, 10.08.2026).
[[nodiscard]] constexpr VoxelMaterial voxel_material_of(SurfaceClass cls) {
    switch (cls) {
    case SurfaceClass::Rock:
        return VoxelMaterial::Rock;
    case SurfaceClass::Sand:
        return VoxelMaterial::Sand;
    case SurfaceClass::WaterBed:
        return VoxelMaterial::Dirt; // river and lake beds; see the note above
    case SurfaceClass::GrassRockBlend:
        return VoxelMaterial::GrassRockBlend;
    case SurfaceClass::Grass:
        return VoxelMaterial::Grass;
    }
    return VoxelMaterial::Grass; // unreachable for a valid enumerator
}

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
