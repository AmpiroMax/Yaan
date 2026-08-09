/*
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 01:56:45
Module: engine/anim
File: engine/anim/sources/BodyMesh.h

Responsibility:
- CPU geometry for the rigid body segments: one flat-shaded box mesh per bone,
  authored in bone space (origin at the proximal joint, extending along the
  rest direction), sized from RigProportions. The app ferries these into
  render's registry (render owns mesh ids and the GPU side).

Key items:
- BodySegmentMesh: vertices/indices (platform::Vertex — the frozen layout) +
  model-space bounds for LocalBounds.
- build_body_segment_mesh(bone, proportions): pure, deterministic.
- BODY_SEGMENT_MESH_ID_FIRST + body_segment_mesh_id(): 34 + bone index, per
  the id map in render's ProcMesh.h (range 34..49 agreed with render).

Dependencies:
- Uses: Rig.h, engine/platform/render interface (Vertex only), glm.
- Used by: the app (registration ferry), Body.cpp (bounds), tests.

Notes:
- Colors are placeholder ASSET data (same standing as render's ProcMesh
  placeholder palette): skin/tunic/trousers/boots, flat vertex colors.
- Deliberately does NOT include engine/render (DAG: anim and render are
  siblings); the tiny tri/quad helpers are re-stated here rather than
  imported. If a third zone ever needs them, they move to core/math (Rule 32
  watchpoint, recorded in docs/README.md).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Head segment must stay a separate mesh: first person hides it (camera sits
  inside the skull).
*/
/*
UPD:
- 10:08:2026 - 01:56:45: Initial segment boxes.
*/

#pragma once

#include "engine/anim/sources/Rig.h"
#include "engine/platform/render/interfaces/IRenderer.h"

#include <vector>

namespace dfn::anim {

// RenderMesh ids for body segments: 34 + bone index (34..48, spare 49).
// The id MAP lives in render's ProcMesh.h (their zone); this mirrors the
// agreed range and a test pins the arithmetic.
inline constexpr uint32_t BODY_SEGMENT_MESH_ID_FIRST = 34;
[[nodiscard]] constexpr uint32_t body_segment_mesh_id(Bone b) {
    return BODY_SEGMENT_MESH_ID_FIRST + bone_index(b);
}

struct BodySegmentMesh {
    std::vector<platform::Vertex> vertices;
    std::vector<uint32_t> indices;
    glm::vec3 bounds_min{0.0f};
    glm::vec3 bounds_max{0.0f};
};

// Pure and deterministic: the box for `bone` at the proportions' sizes.
// Every bone yields non-empty geometry (a gap would be an invisible limb —
// same defect class as render's missing castle; tests enumerate all bones).
[[nodiscard]] BodySegmentMesh build_body_segment_mesh(Bone bone,
                                                      const RigProportions& p);

} // namespace dfn::anim
