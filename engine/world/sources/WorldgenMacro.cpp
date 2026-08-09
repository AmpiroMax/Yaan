/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 14:03:23
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
- 09:08:2026 - 13:28:27: P1 anisotropy retune (§2.1, gated on HILL_ANISOTROPY): mid octave input-stretched along a drifting per-valley axis field via bilinear blending of fixed-frame samples (position-varying rotation rejected — |world|*grad(theta) distortion; cross-axis rhythm pinned at the 128 m contract by construction).
- 09:08:2026 - 14:03:23: Micro-relief batch: path groove applied in macro_height before the river carve (channel clamp overrides in-water; constant along-path depth keeps CORRIDOR_SLOPE_MAX untouched; ~6 deg edge slopes stay under the blend threshold).
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

/// Landform anisotropy (LANDSCAPE §2.1): the MID octave (the round-bump
/// layer) input-stretched by HILL_ANISOTROPY along a drifting per-valley
/// axis field. Implemented as bilinear blending of four FIXED-FRAME samples:
/// each axis-lattice cell (WORLDGEN_OCTAVE1_CELL — "per-valley" is the
/// octave-1 wavelength) carries one axis angle in [0, pi); within a frame the
/// rotation is constant, so the along-axis input is compressed exactly
/// 1/HILL_ANISOTROPY and the cross-axis input stays 1:1 — the ~128 m
/// cross-axis rhythm the corridors and the C1 grid feel is pinned by
/// construction (design contract; domain-warp rejected in §2.1, and a
/// POSITION-VARYING rotation is rejected here too: its |world|*grad(theta)
/// distortion term shreds the octave far from the origin). Frames with equal
/// angles produce identical fields (seamless); drifting angles crossfade.
float aniso_mid_octave(uint64_t seed, glm::vec2 world) {
    const float axis_cell = static_cast<float>(config::WORLDGEN_OCTAVE1_CELL);
    const float cx = world.x / axis_cell;
    const float cz = world.y / axis_cell;
    const int64_t gx = static_cast<int64_t>(std::floor(cx));
    const int64_t gz = static_cast<int64_t>(std::floor(cz));
    const float tx = smoothstep01(cx - static_cast<float>(gx));
    const float tz = smoothstep01(cz - static_cast<float>(gz));

    float vals[2][2];
    for (int dz = 0; dz <= 1; ++dz) {
        for (int dx = 0; dx <= 1; ++dx) {
            const float theta =
                noise::lattice_value(seed, STREAM_HILL_AXIS, gx + dx, gz + dz)
                * 3.14159265358979f;
            const glm::vec2 axis{std::cos(theta), std::sin(theta)};
            const glm::vec2 stretched{
                glm::dot(world, axis) / static_cast<float>(config::HILL_ANISOTROPY),
                world.y * axis.x - world.x * axis.y}; // dot(world, across)
            vals[dz][dx] =
                value_noise(seed, STREAM_OCTAVE_BASE + 1,
                            static_cast<float>(config::WORLDGEN_OCTAVE2_CELL), stretched);
        }
    }
    const float v0 = vals[0][0] + (vals[0][1] - vals[0][0]) * tx;
    const float v1 = vals[1][0] + (vals[1][1] - vals[1][0]) * tx;
    return v0 + (v1 - v0) * tz;
}

/// Base gentle-hills fBm in meters, then valley redistribution. Octaves 1
/// (macro rolls) and 3 (fine texture) are isotropic; octave 2 is the
/// anisotropic ridgelet layer above.
float base_height(uint64_t seed, glm::vec2 world) {
    float h = 0.0f;
    h += value_noise(seed, STREAM_OCTAVE_BASE + 0,
                     static_cast<float>(config::WORLDGEN_OCTAVE1_CELL), world)
         * static_cast<float>(config::WORLDGEN_OCTAVE1_AMP);
    h += aniso_mid_octave(seed, world) * static_cast<float>(config::WORLDGEN_OCTAVE2_AMP);
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

/// Drainage valley (layout data): clamps terrain DOWN to a monotone floor
/// profile inside the valley and UP to a shoulder crest just outside — the
/// deterministic watershed the §3.1 descent trace follows. Applied to the
/// base BEFORE the crag max() so the crag flank stays intact and the valley
/// takes over at its foot.
float trough_shape(const ValleyTrough& trough, float h, glm::vec2 world) {
    if (trough.point_count < 2) {
        return h;
    }
    const float shoulder_band = trough.half_width * trough.shoulder_frac;
    const float reach = trough.half_width + shoulder_band;
    // Distance to the polyline and the normalized along-path position of the
    // closest point (for the floor lerp).
    float best_d = 1e9f;
    float best_t = 0.0f;
    float cum = 0.0f;
    float total = 0.0f;
    for (int i = 0; i + 1 < trough.point_count; ++i) {
        total += glm::length(trough.points[i + 1] - trough.points[i]);
    }
    if (total <= 0.0f) {
        return h;
    }
    for (int i = 0; i + 1 < trough.point_count; ++i) {
        const glm::vec2 a = trough.points[i];
        const glm::vec2 b = trough.points[i + 1];
        const glm::vec2 ab = b - a;
        const float len = glm::length(ab);
        const float t = len > 0.0f
                          ? std::clamp(glm::dot(world - a, ab) / (len * len), 0.0f, 1.0f)
                          : 0.0f;
        const float d = glm::length(world - (a + ab * t));
        if (d < best_d) {
            best_d = d;
            best_t = (cum + t * len) / total;
        }
        cum += len;
    }
    if (best_d >= reach) {
        return h;
    }
    const float floor_h =
        trough.floor_source + (trough.floor_mouth - trough.floor_source) * best_t;
    if (best_d < trough.half_width) {
        // Valley interior: terrain is clamped to the designed cross-section
        // (floor + parabolic rise, 1.5 m noise allowance below) so the floor
        // descends monotonically all the way to the lake rim — no stray dips
        // that would pond against the levee, no rises that would divert the
        // §3.1 descent out of the valley.
        const float cross = best_d / trough.half_width;
        const float surface = floor_h + trough.wall_height * cross * cross;
        return std::clamp(h, surface - 1.5f, surface);
    }
    // Shoulder: raise terrain to the crest, fading back to the base outward —
    // this is the watershed that keeps the drainage in the valley.
    const float crest = floor_h + trough.wall_height;
    const float s = smoothstep01((best_d - trough.half_width) / shoulder_band);
    return std::max(h, (1.0f - s) * crest + s * h);
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
    for (const ValleyTrough& trough : layout.troughs) {
        h = trough_shape(trough, h, world);
    }
    h = std::max(h, crag_height(seed, layout.crag, world));
    h += bump_height(layout.knoll, world);
    h += bump_height(layout.bluff, world);
    h = lake_stamp(layout.lake, h, world);
    h -= path_groove_depth(layout, world);
    return std::clamp(h, 0.0f, MAX_HEIGHT_M);
}

float crag_distance(const TestbedLayout& layout, glm::vec2 world) {
    return glm::length(world - layout.crag.center);
}

float path_groove_depth(const TestbedLayout& layout, glm::vec2 world) {
    constexpr float HALF_W = static_cast<float>(config::PATH_GROOVE_HALF_WIDTH);
    const float d = corridor_distance(layout, world);
    if (d >= HALF_W) {
        return 0.0f;
    }
    return static_cast<float>(config::PATH_GROOVE_DEPTH)
         * (1.0f - smoothstep01(d / HALF_W));
}

} // namespace dfn::world
