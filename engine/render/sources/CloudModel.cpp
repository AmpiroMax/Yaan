/*
Created: 10:08:2026 - 02:57:10
Last updated: 10:08:2026 - 02:57:10
Module: engine/render
File: engine/render/sources/CloudModel.cpp

Responsibility:
- CloudModel implementation: the pure drift-offset math and the per-frame
  RenderEnvironment write.

Key items:
- cloud_drift_offset, apply_clouds.

Dependencies:
- Uses: CloudModel.h, glm.
- Used by: engine/render (RenderSystem), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure: time arrives as a parameter; no clock reads, no GPU, no ECS.
*/
/*
UPD:
- 10:08:2026 - 02:57:10: Created with the cloud pass (W4).
*/

#include "engine/render/sources/CloudModel.h"

#include <glm/geometric.hpp>

namespace dfn::render {

glm::vec2 cloud_drift_offset(glm::vec2 direction, float wind_mult,
                             float seconds) {
    const float len = glm::length(direction);
    if (len < 1e-6f || wind_mult <= 0.0f) {
        // No wind direction / becalmed state: the field stands still rather
        // than drifting along a garbage vector.
        return glm::vec2{0.0f, 0.0f};
    }
    const glm::vec2 dir = direction / len;
    // Negative: the samplers read field(p + offset), so a negative offset
    // moves the PATTERN downwind across the world (weather arrives from
    // upwind, W2.3). See the header note.
    return dir * (-WIND_FIELD_DRIFT_SPEED_MPS * wind_mult * seconds);
}

void apply_clouds(platform::RenderEnvironment& env, float seconds) {
    env.cloud_offset_m =
        cloud_drift_offset(env.wind_direction, env.weather_wind_mult, seconds);
    env.cloud_wavelength_m = CLOUD_WAVELENGTH_M;
}

} // namespace dfn::render
