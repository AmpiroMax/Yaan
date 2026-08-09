/*
Created: 09:08:2026 - 00:45:08
Last updated: 09:08:2026 - 00:45:08
Module: engine/physics
File: engine/physics/sources/TerrainCollision.cpp

Responsibility:
- Implements the HeightFieldView -> IPhysics terrain body conversion (the
  agreed core<->sim boundary: raw uint16 decoded here, backend gets meters).

Key items:
- create_terrain_body(): decode via the frozen height formula, fill TerrainDesc.

Dependencies:
- Uses: TerrainCollision.h, CollisionLayers.h.
- Used by: chunk-load handlers, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep the decode identical to HeightFieldView::height_at (frozen contract).
*/
/*
UPD:
- 09:08:2026 - 00:45:08: Stage 2 — initial implementation.
*/

#include "engine/physics/sources/TerrainCollision.h"

#include "engine/physics/sources/CollisionLayers.h"

namespace dfn::physics {

platform::PhysicsBodyHandle create_terrain_body(platform::IPhysics& physics,
                                                const math::HeightFieldView& view,
                                                uint64_t user_data,
                                                std::vector<float>& scratch) {
    const size_t sample_count = static_cast<size_t>(view.resolution) * view.resolution;
    if (view.resolution < 2 || view.step <= 0.0f || view.heights.size() < sample_count) {
        return {};
    }

    scratch.clear();
    scratch.reserve(sample_count);
    for (size_t i = 0; i < sample_count; ++i) {
        // Frozen formula (HeightFieldView contract): offset + raw * scale.
        scratch.push_back(view.height_offset +
                          static_cast<float>(view.heights[i]) * view.height_scale);
    }

    platform::TerrainDesc desc;
    desc.origin = {view.origin.x, 0.0f, view.origin.y}; // heights are absolute meters
    desc.sample_count_x = view.resolution;
    desc.sample_count_z = view.resolution;
    desc.sample_spacing = view.step;
    desc.heights = scratch;
    desc.layer = LAYER_STATIC;
    desc.user_data = user_data;
    return physics.create_terrain(desc);
}

} // namespace dfn::physics
