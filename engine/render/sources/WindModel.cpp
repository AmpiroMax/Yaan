/*
Module: engine/render
File: engine/render/sources/WindModel.cpp

Responsibility:
- WindModel implementation: the CPU-side gust envelope and the RenderEnvironment
  wind fields written from it.

Key items:
- wind_gust_envelope, apply_wind.

Dependencies:
- Uses: WindModel.h, glm, <cmath>.
- Used by: engine/app (per frame), tests, later the audio rustle.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure: time arrives as a parameter; no clock reads, no GPU, no ECS.
*/

#include "engine/render/sources/WindModel.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace dfn::render {

float wind_gust_envelope(float seconds) {
    constexpr float TAU = 6.28318530718f;
    // Two incommensurate periods so the envelope never repeats audibly or
    // visibly on a short loop — a single sine reads as a machine breathing.
    const float slow = std::sin(seconds * TAU / WIND_GUST_PERIOD_S);
    const float fast = std::sin(seconds * TAU / WIND_GUST_PERIOD2_S);
    // Weighted toward the slow term: gusts should build and fade, not chatter.
    const float mixed = 0.72f * slow + 0.28f * fast;
    return std::clamp(0.5f * (mixed + 1.0f), 0.0f, 1.0f);
}

void apply_wind(platform::RenderEnvironment& env, float seconds) {
    const float gust = wind_gust_envelope(seconds);
    env.wind_direction = glm::normalize(WIND_DEFAULT_DIRECTION);
    // The CURRENT strength, gusts included, as ONE scalar — this is the number
    // audio reads for the rustle and gameplay reads for "has the wind dropped".
    env.wind_strength =
        std::clamp(WIND_BASE_STRENGTH + WIND_GUST_AMPLITUDE * (gust - 0.5f) * 2.0f,
                   0.0f, 1.0f);
    // Flutter rises with the gust: a still day barely trembles, a gust makes
    // the leaf edges chatter. Same envelope, so picture and sound agree.
    env.wind_flutter = 0.6f + 0.8f * gust;
}

} // namespace dfn::render
