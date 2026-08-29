/*
Module: engine/render
File: engine/render/sources/TreeBark.h

Responsibility:
- The two primitives every forged tree's WOOD is made of, shared by the first
  iteration (TreeForge.cpp) and the second (TreeForgeV2.cpp): the wind vertex
  colour and the bark-textured tapered tube.

Key items:
- pack_wind(), bark_tube().

Dependencies:
- Uses: ProcMesh.h (MeshData), FloraBuild.h (safe_normalize, perp_of, TAU).
- Used by: TreeForge.cpp, TreeForgeV2.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- MOVED, NOT REWRITTEN. This code was lifted verbatim out of TreeForge.cpp's
  anonymous namespace on 28.08 so the second iteration could grow its own
  builder without a second copy of a tube mapper this subtle. Every .dfo on the
  shelf was re-baked across the move and every content_hash came out
  unchanged — that comparison is the licence for the move and must be repeated
  by anyone who edits below this line.
*/

#pragma once

#include <cstdint>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace dfn::render {

struct MeshData;

/// Vertex colour for textured wood on the foliage program: r = sway weight
/// (honest distance-from-support — the lead's 09f75eb shader derives all
/// three wind bands from this one weight), g = per-tree phase, b (value
/// jitter) = 0.5 neutral, a (sky vis) = 0.55.
[[nodiscard]] uint32_t pack_wind(float sway, float phase);

/// One tapered tube segment with BARK UVs. Same geometry as tube_segment, but
/// each face maps into the given atlas tile rect: u runs around the
/// circumference, v along the segment's own length. The mapping repeats
/// plainly inside the tile (the bark tile is torus-periodic) and every face is
/// SPLIT at each tile boundary, so no face ever folds across one.
/// wind_c0 colours the p0 ring, wind_c1 the p1 ring (and the tip vertex):
/// the sway weight must be CONTINUOUS along a limb, or adjacent segments
/// translate by different amounts under wind and the joint cracks open.
/// u_hint PARALLEL-TRANSPORTS the texture frame along a limb: pass the
/// previous segment's frame and the furrows run straight down the limb instead
/// of twisting at every joint (perp_of() alone picks an arbitrary frame per
/// segment).
void bark_tube(MeshData& m, glm::vec3 p0, glm::vec3 p1, glm::vec3 axis, float r0,
               float r1, int sides, glm::vec4 uv_rect, float v0_m, float circum_m,
               uint32_t wind_c0, uint32_t wind_c1, glm::vec3* u_hint = nullptr);

} // namespace dfn::render
