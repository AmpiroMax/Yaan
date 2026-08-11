/*
Created: 12:08:2026 - 00:44:12
Last updated: 12:08:2026 - 00:44:12
Module: engine/render
File: engine/render/sources/GroundTufts.h

Responsibility:
- The SPARSE GROUND TUFT layer (LANDSCAPE §2.3/§5.6 micro, render-side by
  design): a few small, varied clumps of blades standing on the DRAWN ground,
  everywhere, in the near metres. Their job is not decoration — it is to break
  the flat read of bare ground and the hard seam where an object meets it.

Key items:
- GroundTuftParams; TuftSpot; harvest_tuft_spots(); build_ground_tufts().

Dependencies:
- Uses: ProcMesh.h (MeshData and the flat-triangle helpers only — this file
  does not modify it), engine/core/math VoxelField/SurfaceField, glm.
- Used by: RenderSystem (upload_terrain_voxel harvests, render builds and
  draws); tests/render/GroundTuftsTests.cpp.

Notes:
- WHY THE SPOTS COME FROM THE DRAWN MESH AND NOT FROM THE HEIGHT FIELD. The
  ground the player sees is the VOXEL surface, and the height field disagrees
  with it — that disagreement is a documented defect with a stopgap of its own
  (PATH_SURFACE_LIFT_M, Materials.h). Its residual after core unified the two
  surfaces is around 0.10-0.15 m, which is a rounding error for a tree and half
  the height of a tuft. Planting on the triangles that are actually drawn makes
  the error exactly zero instead of small, and costs nothing: the mesh is
  already in hand at upload.
- WHY A TUFT IS PLANTED PER TRIANGLE. It makes the placement uniform BY AREA
  for free, it is deterministic from the triangle's own index (so tufts do not
  swim when a chunk is re-meshed or the eye moves), and it needs no acceleration
  structure and no second lattice to keep in step with core's.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure functions of their inputs: deterministic, GPU-free, unit-testable.
- THE VIEW DISTANCE IS DERIVED, NOT CHOSEN, AND IT IS SHORT ON PURPOSE
  (Rule 33). A 0.3 m object stops being an object at 9 m; drawing tufts to
  GRASS_VIEW_DISTANCE = 50 m would put thousands of sub-pixel triangles on the
  screen, buying nothing visible and manufacturing exactly the running shimmer
  this project has already fought twice. Tufts cannot fix the middle distance
  and must not try. Their band is the near metres — which is legitimate,
  because that band is currently empty.
*/
/*
UPD:
- 12:08:2026 - 00:44:12: Created. User request, verbatim: «надо траву сделать не
  просто как поверхность где ходим, а добавить мелкую разную траву, её должно
  быть не много, но она должна быть везде и прикрывать плоскость, особенно в
  зоне равнин». Read carefully, that is not a lawn: SPARSE, VARIED, EVERYWHERE.
*/

#pragma once

#include "engine/core/math/sources/VoxelField.h"
#include "engine/render/sources/ProcMesh.h"

#include <cstdint>
#include <span>
#include <vector>

namespace dfn::render {

/// One planted tuft: a point on the drawn ground plus the seed that decides
/// everything about the clump that grows there.
struct TuftSpot {
    glm::vec3 position{0.0f}; ///< world metres, ON the drawn surface
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    uint32_t seed = 0; ///< stable per spot; picks shape, size, lean and tone
};

struct GroundTuftParams {
    /// Tufts per square metre. The design ceiling is GRASS_DENSITY 0.5-1.5
    /// (§2.3); the user asked for «не много», so this rides the FLOOR of the
    /// approved range rather than introducing a number of its own (Rule 14).
    float density_per_m2 = 0.5f;
    /// Metres. Derived from Rule 33 by the caller, never typed in.
    float view_distance_m = 12.0f;
    /// Tallest blade, metres (GRASS_HEIGHT_MAX).
    float height_max_m = 0.4f;
    /// Steepest ground that still carries grass, radians (SLOPE_GRASS_MAX).
    float slope_max_rad = 0.52f;
    uint32_t seed = 0x9E37u;
};

/// Walks a chunk's drawn voxel mesh and returns the spots that carry a tuft.
/// Runs ONCE per chunk upload; the result is small (a few hundred KB for a
/// 256 m chunk) and is what the per-frame build reads.
///
/// Only up-facing GRASS triangles are considered: the material comes from the
/// mesh's own per-vertex VoxelMaterial, which is core's surface truth, so grass
/// never grows on rock, sand or a cave floor by accident.
[[nodiscard]] std::vector<TuftSpot>
harvest_tuft_spots(const math::VoxelMeshView& mesh, const GroundTuftParams& params);

/// Grows the spots within `view_distance_m` of `eye` into one mesh.
///
/// SEVERAL SHAPES AND SEVERAL TONES, not one clump in copies: the seed picks
/// among blade counts, heights, lean angles and a small set of ground-cover
/// tones, so a screen of tufts does not read as a stamp — which is the same
/// failure R5 just fixed in the ground colour, and it would be absurd to fix it
/// there and re-introduce it here.
[[nodiscard]] MeshData build_ground_tufts(std::span<const TuftSpot> spots,
                                          glm::vec3 eye,
                                          const GroundTuftParams& params);

} // namespace dfn::render
