/*
Created: 10:08:2026 - 02:57:10
Last updated: 10:08:2026 - 10:45:06
Module: engine/render
File: engine/render/sources/CloudModel.cpp

Responsibility:
- CloudModel implementation: the pure drift-offset math and the per-frame
  RenderEnvironment write.

Key items:
- cloud_drift_offset, apply_clouds.
- cloud_field_raw / cloud_field / cloud_alpha: the coverage field, mirrored
  from dfn_env.sh. cloud_field_raw is the REJECTED (Gaussian, un-remapped)
  form, kept as the distribution tests' control.

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
- 10:08:2026 - 10:45:06: THE COVERAGE FIELD's reference implementation
  (cloud_field_raw / cloud_field / cloud_alpha) — the CPU mirror of
  dfn_env.sh, added so the field's DISTRIBUTION can be asserted (Rule 31).
*/

#include "engine/render/sources/CloudModel.h"

#include <glm/geometric.hpp>

#include <cmath>

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

namespace {

float fract1(float v) { return v - std::floor(v); }

float smooth_step(float e0, float e1, float x) {
    float t = (x - e0) / (e1 - e0);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

float mix1(float a, float b, float t) { return a + (b - a) * t; }

// Value-noise hash and lattice. Mirrored in dfn_env.sh; the sin/fract form is
// the GPU's, kept here verbatim so the two agree to float precision.
float cloud_hash(glm::vec2 c) {
    return fract1(std::sin(c.x * 127.1f + c.y * 311.7f) * 43758.5453f);
}

float cloud_vnoise(glm::vec2 p) {
    const glm::vec2 c{std::floor(p.x), std::floor(p.y)};
    glm::vec2 f{p.x - c.x, p.y - c.y};
    f.x = f.x * f.x * (3.0f - 2.0f * f.x);
    f.y = f.y * f.y * (3.0f - 2.0f * f.y);
    const float a = cloud_hash(c);
    const float b = cloud_hash({c.x + 1.0f, c.y});
    const float d = cloud_hash({c.x, c.y + 1.0f});
    const float e = cloud_hash({c.x + 1.0f, c.y + 1.0f});
    return mix1(mix1(a, b, f.x), mix1(d, e, f.x), f.y);
}

// An octave whose cells have shrunk under about two pixels contributes nothing
// but aliasing. It is replaced by its MEAN (0.5), not scaled toward zero:
// scaling toward zero would shrink the field's spread and quietly move every
// coverage threshold with it, which is the Rule 31 defect re-introduced by the
// fix for it.
float octave_lod(float cells_per_pixel, float frequency) {
    return 1.0f - smooth_step(0.22f, 0.75f, cells_per_pixel * frequency);
}

} // namespace

float cloud_field_raw(glm::vec2 p_m, float wavelength_m) {
    const float w = wavelength_m > 1.0f ? wavelength_m : 1.0f;
    const glm::vec2 q{p_m.x / w, p_m.y / w};
    return cloud_vnoise(q) * CLOUD_OCTAVE_W0
           + cloud_vnoise({q.x * CLOUD_OCTAVE_F1 + 17.0f,
                           q.y * CLOUD_OCTAVE_F1 + 31.0f})
                 * CLOUD_OCTAVE_W1
           + cloud_vnoise({q.x * CLOUD_OCTAVE_F2 + 47.0f,
                           q.y * CLOUD_OCTAVE_F2 + 89.0f})
                 * CLOUD_OCTAVE_W2;
}

float cloud_field(glm::vec2 p_m, float wavelength_m, float cells_per_pixel) {
    const float w = wavelength_m > 1.0f ? wavelength_m : 1.0f;
    const glm::vec2 q{p_m.x / w, p_m.y / w};
    const float l0 = octave_lod(cells_per_pixel, 1.0f);
    const float l1 = octave_lod(cells_per_pixel, CLOUD_OCTAVE_F1);
    const float l2 = octave_lod(cells_per_pixel, CLOUD_OCTAVE_F2);
    const float raw =
        0.5f + (cloud_vnoise(q) - 0.5f) * CLOUD_OCTAVE_W0 * l0
        + (cloud_vnoise({q.x * CLOUD_OCTAVE_F1 + 17.0f,
                         q.y * CLOUD_OCTAVE_F1 + 31.0f})
           - 0.5f)
              * CLOUD_OCTAVE_W1 * l1
        + (cloud_vnoise({q.x * CLOUD_OCTAVE_F2 + 47.0f,
                         q.y * CLOUD_OCTAVE_F2 + 89.0f})
           - 0.5f)
              * CLOUD_OCTAVE_W2 * l2;
    // The octave sum is Gaussian; push it through that Gaussian's own CDF and
    // what comes out is uniform on [0,1]. The logistic form approximates the
    // normal CDF to under 0.01 absolute and costs one exp — measured deciles
    // land within 0.024 of the ideal across the whole range.
    const float z = (raw - CLOUD_FIELD_MEAN) / CLOUD_FIELD_SD;
    return 1.0f / (1.0f + std::exp(-1.702f * z));
}

float cloud_alpha(glm::vec2 p_m, float wavelength_m, float cover,
                  float cells_per_pixel) {
    if (cover <= 0.0f) {
        return 0.0f;
    }
    const float u = cloud_field(p_m, wavelength_m, cells_per_pixel);
    // The soft edge is clamped by the distance to whichever END of the range
    // is nearer, so the threshold window never hangs off it. Without this,
    // cover 1.0 left the field's bottom decile UNCOVERED — a "total overcast"
    // state with holes in it, and the second of the two assertions a range is
    // (Rule 30). The midpoint stays exactly at 1-cover, so coverage still
    // equals cover everywhere in between.
    float edge = CLOUD_EDGE_U;
    edge = edge < cover ? edge : cover;
    edge = edge < 1.0f - cover ? edge : 1.0f - cover;
    const float a = smooth_step(1.0f - cover - edge, 1.0f - cover + edge, u);
    // Below the resolution limit the only defensible value is the area
    // average, which for a uniform field thresholded at 1-cover is `cover`.
    return mix1(a, cover, smooth_step(0.20f, 0.60f, cells_per_pixel));
}

} // namespace dfn::render
