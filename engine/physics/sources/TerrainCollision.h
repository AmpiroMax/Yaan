/*
Created: 09:08:2026 - 00:45:08
Last updated: 09:08:2026 - 00:45:08
Module: engine/physics
File: engine/physics/sources/TerrainCollision.h

Responsibility:
- Bridges world heightmap data to physics: converts a math::HeightFieldView
  (raw uint16 + scale/offset, the agreed core<->sim contract) into an IPhysics
  terrain body. The uint16 -> float meters conversion lives HERE, per the
  stage-1 boundary agreement with core.

Key items:
- create_terrain_body(): decode + create_terrain in one call, one body per chunk.

Dependencies:
- Uses: core math (HeightFieldView), platform physics interface, CollisionLayers.
- Used by: chunk-load handlers (app/world side), jolt-backed tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Height formula is the frozen HeightFieldView contract:
  height_m = height_offset + raw * height_scale. Never reimplement elsewhere.
*/
/*
UPD:
- 09:08:2026 - 00:45:08: Stage 2 — heightfield -> terrain body conversion.
*/

#pragma once

#include <vector>

#include "engine/core/math/sources/HeightField.h"
#include "engine/platform/physics/interfaces/IPhysics.h"

namespace dfn::physics {

/// Decodes `view` into float meters (scratch is reused between calls to avoid
/// per-chunk allocations on the streaming path) and creates one static terrain
/// body on LAYER_STATIC. `user_data` carries the chunk terrain entity's id bits.
/// Returns an invalid handle if the view is malformed.
[[nodiscard]] platform::PhysicsBodyHandle create_terrain_body(
    platform::IPhysics& physics, const math::HeightFieldView& view,
    uint64_t user_data, std::vector<float>& scratch);

} // namespace dfn::physics
