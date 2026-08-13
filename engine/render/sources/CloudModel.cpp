/*
Created: 10:08:2026 - 02:57:10
Last updated: 13:08:2026 - 18:59:13
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
- 11:08:2026 - 14:43:13: cloud_field3 / cloud_field3_with — the 3-D mirror of dfn_env.sh's
  dfn_cloud_field3 (R3.1), with its own measured mean/SD and an injectable
  pair so the 2-D constants can be shipped as the failing control.
- 12:08:2026 - 22:45:00: R3.3 — lod_sum() computes the raw sum, mean_lod and sd_lod from
  ONE set of octave weights so the three cannot disagree; cloud_field
  remaps through THAT distribution; cloud_alpha converges on the residual.
  cloud_field_fixed_sd added as the rejected form.
- 13:08:2026 - 19:20:00: R3.4 — cloud_ceiling_m / cloud_decks_m: THE CEILING'S HEIGHT
  IS A FIELD. Driven half by the weather state's own cloud_cover (heavy cover =
  a low wet ceiling) and half by PLACE, read from THE coverage field at a
  wavelength of two world widths — the same construction asked a different
  question, never a second weather source (Rule 35). Range [400, 2000] m, both
  ends derived rather than picked (see the header). apply_clouds now takes the
  eye, because place is one of its two arguments.
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
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

void apply_clouds(platform::RenderEnvironment& env, float seconds,
                  glm::vec2 eye_xz) {
    env.cloud_offset_m =
        cloud_drift_offset(env.wind_direction, env.weather_wind_mult, seconds);
    env.cloud_wavelength_m = CLOUD_WAVELENGTH_M;
    env.cloud_deck_m = cloud_decks_m(cloud_ceiling_m(env.cloud_cover, eye_xz));
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

namespace {

// The raw (pre-remap) octave sum at a sampling rate, together with the mean and
// spread that SURVIVE the LOD at that rate. One function because the three are
// computed from the same three l_i and must never be able to disagree.
struct LodSum {
    float raw;
    float mean;
    float sd;
    float residual;
};

LodSum lod_sum(glm::vec2 q, float cells_per_pixel) {
    const float l0 = octave_lod(cells_per_pixel, 1.0f);
    const float l1 = octave_lod(cells_per_pixel, CLOUD_OCTAVE_F1);
    const float l2 = octave_lod(cells_per_pixel, CLOUD_OCTAVE_F2);
    const float w0 = CLOUD_OCTAVE_W0 * l0;
    const float w1 = CLOUD_OCTAVE_W1 * l1;
    const float w2 = CLOUD_OCTAVE_W2 * l2;
    LodSum s{};
    s.raw = 0.5f + (cloud_vnoise(q) - 0.5f) * w0
            + (cloud_vnoise({q.x * CLOUD_OCTAVE_F1 + 17.0f,
                             q.y * CLOUD_OCTAVE_F1 + 31.0f})
               - 0.5f)
                  * w1
            + (cloud_vnoise({q.x * CLOUD_OCTAVE_F2 + 47.0f,
                             q.y * CLOUD_OCTAVE_F2 + 89.0f})
               - 0.5f)
                  * w2;
    // The octaves are the same construction at incommensurate frequencies:
    // uncorrelated, equal marginal variance. Then the sum's spread scales with
    // the quadratic norm of the surviving weights and its mean with their
    // linear sum. Both are IDENTITIES at full resolution (the weights sum to
    // 1.0 and their norm to CLOUD_OCTAVE_W_NORM), which is what makes this a
    // generalisation of the shipped constants rather than a second calibration.
    s.residual =
        std::sqrt(w0 * w0 + w1 * w1 + w2 * w2) / CLOUD_OCTAVE_W_NORM;
    s.sd = CLOUD_FIELD_SD * s.residual;
    s.mean = 0.5f + (CLOUD_FIELD_MEAN - 0.5f) * (w0 + w1 + w2);
    return s;
}

} // namespace

float cloud_lod_residual(float cells_per_pixel) {
    return lod_sum({0.0f, 0.0f}, cells_per_pixel).residual;
}

float cloud_field(glm::vec2 p_m, float wavelength_m, float cells_per_pixel) {
    const float w = wavelength_m > 1.0f ? wavelength_m : 1.0f;
    const LodSum s = lod_sum({p_m.x / w, p_m.y / w}, cells_per_pixel);
    // The octave sum is Gaussian; push it through THAT Gaussian's own CDF and
    // what comes out is uniform on [0,1]. The logistic form approximates the
    // normal CDF to under 0.01 absolute and costs one exp — measured deciles
    // land within 0.024 of the ideal across the whole range.
    //
    // "That Gaussian" is the load-bearing word and it is what R3.3 got wrong:
    // the LOD changes the distribution, so the CDF has to change with it. The
    // floor only guards the division at the dead end, where the outer
    // convergence in cloud_alpha has long since taken the answer over.
    const float sd = s.sd > CLOUD_FIELD_SD * 1e-4f ? s.sd : CLOUD_FIELD_SD * 1e-4f;
    const float z = (s.raw - s.mean) / sd;
    return 1.0f / (1.0f + std::exp(-1.702f * z));
}

float cloud_field_fixed_sd(glm::vec2 p_m, float wavelength_m,
                           float cells_per_pixel) {
    const float w = wavelength_m > 1.0f ? wavelength_m : 1.0f;
    const LodSum s = lod_sum({p_m.x / w, p_m.y / w}, cells_per_pixel);
    // THE CONTROL: the full-resolution mean/SD used at every rate, which is
    // what shipped until R3.3.
    const float z = (s.raw - CLOUD_FIELD_MEAN) / CLOUD_FIELD_SD;
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
    // Once the field is DEAD — no octave left, so nothing to threshold — the
    // only defensible value is the area average, which for a uniform field
    // thresholded at 1-cover is `cover`. Keyed to the RESIDUAL SPREAD, not to
    // cells-per-pixel: that is the R3.3 fix, see CLOUD_LOD_RES_LIVE.
    const float res = cloud_lod_residual(cells_per_pixel);
    const float dead =
        1.0f - smooth_step(CLOUD_LOD_RES_DEAD, CLOUD_LOD_RES_LIVE, res);
    return mix1(a, cover, dead);
}

namespace {

// The 3-D hash and lattice, mirrored from dfn_env.sh verbatim.
float cloud_hash3(glm::vec3 c) {
    return fract1(std::sin(c.x * 127.1f + c.y * 311.7f + c.z * 74.7f)
                  * 43758.5453f);
}

float cloud_vnoise3(glm::vec3 p) {
    const glm::vec3 c{std::floor(p.x), std::floor(p.y), std::floor(p.z)};
    glm::vec3 f{p.x - c.x, p.y - c.y, p.z - c.z};
    f.x = f.x * f.x * (3.0f - 2.0f * f.x);
    f.y = f.y * f.y * (3.0f - 2.0f * f.y);
    f.z = f.z * f.z * (3.0f - 2.0f * f.z);
    const auto h = [&](float i, float j, float k) {
        return cloud_hash3({c.x + i, c.y + j, c.z + k});
    };
    const float x00 = mix1(h(0, 0, 0), h(1, 0, 0), f.x);
    const float x10 = mix1(h(0, 1, 0), h(1, 1, 0), f.x);
    const float x01 = mix1(h(0, 0, 1), h(1, 0, 1), f.x);
    const float x11 = mix1(h(0, 1, 1), h(1, 1, 1), f.x);
    return mix1(mix1(x00, x10, f.y), mix1(x01, x11, f.y), f.z);
}

} // namespace

float cloud_field3_with(glm::vec3 q, float mean, float sd) {
    const float raw =
        0.5f + (cloud_vnoise3(q) - 0.5f) * CLOUD_OCTAVE_W0
        + (cloud_vnoise3({q.x * CLOUD_OCTAVE_F1 + 17.0f,
                          q.y * CLOUD_OCTAVE_F1 + 31.0f,
                          q.z * CLOUD_OCTAVE_F1 + 7.0f})
           - 0.5f)
              * CLOUD_OCTAVE_W1
        + (cloud_vnoise3({q.x * CLOUD_OCTAVE_F2 + 47.0f,
                          q.y * CLOUD_OCTAVE_F2 + 89.0f,
                          q.z * CLOUD_OCTAVE_F2 + 23.0f})
           - 0.5f)
              * CLOUD_OCTAVE_W2;
    // No LOD term here, unlike cloud_field: the cumulus band is drawn on a ring
    // at a fixed 20 km, so its cells-per-pixel does not vary across the band
    // the way the overhead sheet's does from zenith to horizon.
    const float z = (raw - mean) / (sd > 1e-6f ? sd : 1e-6f);
    return 1.0f / (1.0f + std::exp(-1.702f * z));
}

float cloud_field3(glm::vec3 q) {
    return cloud_field3_with(q, CLOUD_FIELD3_MEAN, CLOUD_FIELD3_SD);
}

// --- THE CEILING'S HEIGHT (R3.4) -------------------------------------------

float cloud_ceiling_m(float cover, glm::vec2 eye_xz) {
    const float c = cover < 0.0f ? 0.0f : (cover > 1.0f ? 1.0f : cover);
    // WEATHER. Heavy cover is a low wet ceiling; a clear day is a high one.
    // Linear in cover, and linear on purpose: the mapping has to reach BOTH
    // ends of the range at the ends of cover's own range, or the range is a
    // claim nothing exercises (Rule 30 — a range is two assertions).
    const float weather = 1.0f - c;
    // PLACE. THE coverage field, read at a wavelength two world-widths long, so
    // it trends across the map rather than flickering under a walking player.
    // NO DRIFT OFFSET, and that is the load-bearing omission: the wind moves the
    // cloud PATTERN, and a ceiling that slid with it would mean the sky changed
    // altitude every time the weather blew past, which is not what altitude is.
    // Full resolution (cells_per_pixel 0) because this is one CPU sample per
    // frame, not a pixel.
    const float place =
        cloud_field(eye_xz, CLOUD_CEILING_PLACE_WAVELENGTH_M, 0.0f);
    const float t = CLOUD_CEILING_WEATHER_SHARE * weather
                    + (1.0f - CLOUD_CEILING_WEATHER_SHARE) * place;
    return CLOUD_CEILING_MIN_M + (CLOUD_CEILING_MAX_M - CLOUD_CEILING_MIN_M) * t;
}

glm::vec3 cloud_decks_m(float ceiling_m) {
    // ONE multiplier, so the R3.2 ratio ladder (1 : 1.733 : 2.933) cannot be
    // broken by a caller that moves one deck and forgets the others. The ratios
    // are what produce the parallax between decks and the apparent-cell-size
    // cue that says which deck is nearer; they are derived, the altitudes are
    // not sacred.
    const float m = ceiling_m / CLOUD_DECK_LOW_M;
    return {CLOUD_DECK_LOW_M * m, CLOUD_DECK_MID_M * m, CLOUD_DECK_HIGH_M * m};
}

} // namespace dfn::render
