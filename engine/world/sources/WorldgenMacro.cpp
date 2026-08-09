/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 11:05:22
Module: engine/world
File: engine/world/sources/WorldgenMacro.cpp

Responsibility:
- P1 macro heightfield implementation: base fBm (constants from dfn::config —
  the stage-2 local octave table is deleted per the stage-3 sync), valley
  redistribution, crag/knoll/bluff/lake stamps.

Key items:
- macro_height, crag_distance.

Dependencies:
- Uses: WorldgenMacro.h, WorldgenNoise.h, generated constants.
- Used by: dfn_world.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM (Rule 13.1): pure position-based function; the valley curve uses
  sqrt (IEEE correctly-rounded) for the 1.25 exponent — keep the std::pow
  fallback only for non-quarter exponents.
- The quantization range is WORLDGEN_MAX_HEIGHT shared by ALL chunks (offset
  0) — edge stitching depends on this function being position-based.
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — P1 macro v2; octave constants now consumed
  from dfn::config (WORLDGEN_OCTAVE*), local constexprs removed.
*/

#include "engine/world/sources/WorldgenMacro.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenNoise.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace dfn::world {

namespace {

using noise::ridged_noise;
using noise::smoothstep01;
using noise::value_noise;

constexpr float MAX_HEIGHT_M = static_cast<float>(config::WORLDGEN_MAX_HEIGHT);
constexpr float BASE_AMPLITUDE_M = static_cast<float>(
    config::WORLDGEN_OCTAVE1_AMP + config::WORLDGEN_OCTAVE2_AMP + config::WORLDGEN_OCTAVE3_AMP);
constexpr float LAKE_LEVEL_M = static_cast<float>(config::LAKE_LEVEL_TESTBED);
constexpr float LAKE_DEPTH_M = static_cast<float>(config::LAKE_DEPTH_MAX);

/// Valley redistribution (LANDSCAPE §2.1): n^WORLDGEN_VALLEY_EXPONENT on the
/// normalized base height. For the 1.25 exponent this is n * n^(1/4) via two
/// square roots — IEEE-exact, platform-deterministic (std::pow is not).
float valley_curve(float n) {
    constexpr double EXP = config::WORLDGEN_VALLEY_EXPONENT;
    if constexpr (EXP == 1.25) {
        return n * std::sqrt(std::sqrt(n));
    } else {
        return std::pow(n, static_cast<float>(EXP));
    }
}

/// Base gentle-hills fBm in meters (stage-2 field, unchanged hashes: octave
/// streams 0..2), then valley redistribution.
float base_height(uint64_t seed, glm::vec2 world) {
    float h = 0.0f;
    h += value_noise(seed, STREAM_OCTAVE_BASE + 0,
                     static_cast<float>(config::WORLDGEN_OCTAVE1_CELL), world)
         * static_cast<float>(config::WORLDGEN_OCTAVE1_AMP);
    h += value_noise(seed, STREAM_OCTAVE_BASE + 1,
                     static_cast<float>(config::WORLDGEN_OCTAVE2_CELL), world)
         * static_cast<float>(config::WORLDGEN_OCTAVE2_AMP);
    h += value_noise(seed, STREAM_OCTAVE_BASE + 2,
                     static_cast<float>(config::WORLDGEN_OCTAVE3_CELL), world)
         * static_cast<float>(config::WORLDGEN_OCTAVE3_AMP);
    const float n = std::clamp(h / BASE_AMPLITUDE_M, 0.0f, 1.0f);
    return valley_curve(n) * BASE_AMPLITUDE_M;
}

/// L0 crag stamp (§7.1): radial profile toward peak_height, ridged-noise
/// modulation fading out at the summit so the layout peak height is exact.
float crag_height(uint64_t seed, const CragStamp& crag, glm::vec2 world) {
    const float d = glm::length(world - crag.center);
    if (d >= crag.radius) {
        return 0.0f;
    }
    const float prof = smoothstep01(1.0f - d / crag.radius);
    const float ridged = ridged_noise(seed, STREAM_CRAG_RIDGED, crag.ridge_cell, world);
    const float modulation = crag.ridge_amp_frac * (1.0f - prof); // 0 at the peak
    return prof * crag.peak_height * (1.0f - modulation * (1.0f - ridged));
}

/// Additive radial bump (knoll, bluff).
float bump_height(const BumpStamp& bump, glm::vec2 world) {
    if (bump.radius <= 0.0f) {
        return 0.0f;
    }
    const float d = glm::length(world - bump.center);
    if (d >= bump.radius) {
        return 0.0f;
    }
    const float t = smoothstep01(1.0f - d / bump.radius);
    return bump.height * t * t;
}

/// Lake basin stamp (§3.2): carve the bed below LAKE_LEVEL inside the ellipse;
/// raise a soft rim ring so the water plane is contained by construction. The
/// rim crest is lowered toward outlet_dir so the spill point (river exit) is
/// where the design wants it.
float lake_stamp(const LakeStamp& lake, float h, glm::vec2 world) {
    const float q = lake_norm_radius(lake, world);
    if (q < 1.0f) {
        const float bed = LAKE_LEVEL_M - LAKE_DEPTH_M * smoothstep01(1.0f - q);
        return std::min(h, bed);
    }
    const float band = lake.rim_band_frac;
    const float fade = lake.rim_fade_frac;
    if (q >= 1.0f + band + fade) {
        return h;
    }
    const glm::vec2 out = world - lake.center;
    const float out_len = glm::length(out);
    const float alignment =
        out_len > 0.0f ? std::max(0.0f, glm::dot(out / out_len, glm::normalize(lake.outlet_dir)))
                       : 0.0f;
    const float crest_top = LAKE_LEVEL_M + lake.rim_rise - lake.outlet_bias * alignment;
    if (q < 1.0f + band) {
        const float crest =
            LAKE_LEVEL_M + (crest_top - LAKE_LEVEL_M) * smoothstep01((q - 1.0f) / band);
        return std::max(h, crest);
    }
    const float s = smoothstep01((q - 1.0f - band) / fade);
    const float floor_h = (1.0f - s) * crest_top + s * h;
    return std::max(h, floor_h);
}

} // namespace

float macro_height(uint64_t seed, const TestbedLayout& layout, glm::vec2 world) {
    float h = base_height(seed, world);
    h = std::max(h, crag_height(seed, layout.crag, world));
    h += bump_height(layout.knoll, world);
    h += bump_height(layout.bluff, world);
    h = lake_stamp(layout.lake, h, world);
    return std::clamp(h, 0.0f, MAX_HEIGHT_M);
}

float crag_distance(const TestbedLayout& layout, glm::vec2 world) {
    return glm::length(world - layout.crag.center);
}

} // namespace dfn::world
