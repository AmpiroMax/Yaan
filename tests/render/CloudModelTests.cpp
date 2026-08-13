/*
Created: 10:08:2026 - 03:13:00
Last updated: 13:08:2026 - 18:59:13
Module: tests
File: tests/render/CloudModelTests.cpp

Responsibility:
- Unit tests for the cloud coverage field and its drift model (W4): the drift is a pure function of
  time, moves the coverage PATTERN downwind, obeys the state's wind
  multiplier, and apply_clouds touches only its own fields.

Key items:
- doctest cases over cloud_drift_offset / apply_clouds.

Dependencies:
- Uses: doctest, engine/render CloudModel.
- Used by: ctest (render_cloud_model).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. GPU-free.
*/
/*
UPD:
- 10:08:2026 - 03:13:00: Initial tests with the cloud pass.
- 10:08:2026 - 10:45:06: The COVERAGE FIELD's distribution tests (Rule 31),
  each shipping the pre-remap field as the control it must reject.
- 11:08:2026 - 14:43:44: THE 3-D FIELD (R3.1): uniformity, cover-means-coverage, and the
  control that the 2-D mean/SD FAIL in 3-D — which is why the second pair of
  constants exists at all. Plus the assertion that the field actually varies
  with HEIGHT, since not varying with height is what made the band's
  silhouette single-valued and produced the mushroom caps.
- 12:08:2026 - 22:45:00: R3.3 — the LOD's own distribution (Rule 31 one level down: the
  first pass asserted the field at FULL resolution and left the LOD
  unasserted, and the LOD moves the distribution). Three cases, each with
  the shipped-until-now form as the control it rejects: the surviving
  spread is predicted (SD of the recovered z is 1.000 at every rate, the
  control collapses BY the residual), cover means coverage at every rate,
  and the outer convergence fires on the residual instead of on
  cells-per-pixel (alpha SD at 0.60 cells/px 0.000 -> 0.466).
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
*/

#include "engine/render/sources/CloudModel.h"

#include <doctest/doctest.h>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using dfn::platform::RenderEnvironment;
using dfn::render::apply_clouds;
using dfn::render::cloud_drift_offset;
using dfn::render::cloud_alpha;
using dfn::render::cloud_field;
using dfn::render::cloud_field_fixed_sd;
using dfn::render::cloud_field_raw;
using dfn::render::WIND_FIELD_DRIFT_SPEED_MPS;

TEST_CASE("the coverage pattern travels DOWNWIND at the NUMBERS speed") {
    const glm::vec2 wind{1.0f, 0.0f};
    const glm::vec2 o30 = cloud_drift_offset(wind, 1.0f, 30.0f);
    // Samplers read field(p + offset): a NEGATIVE offset along the wind is
    // what moves the pattern toward +wind across the world (W2.3: weather
    // arrives from upwind). The sign is the whole feature — a positive offset
    // would ship weather marching INTO the wind, which no frame would catch
    // without this assertion.
    CHECK(o30.x == doctest::Approx(-WIND_FIELD_DRIFT_SPEED_MPS * 30.0f));
    CHECK(o30.y == doctest::Approx(0.0f));
    // The acceptance pair: 30 s of game time moves the field 300 m — half a
    // WIND_FIELD_WAVELENGTH, the "unmistakable at 640x360" derivation.
    CHECK(glm::length(o30) == doctest::Approx(300.0f));
    // Pure function of time: same inputs, same offset (the W2.5 form — any
    // reported frame reproduces from its timestamp alone).
    CHECK(cloud_drift_offset(wind, 1.0f, 30.0f) == o30);
}

TEST_CASE("drift direction follows the wind, never a second wind") {
    const glm::vec2 wind = glm::normalize(glm::vec2{0.87f, 0.50f});
    const glm::vec2 o = cloud_drift_offset(wind, 1.0f, 10.0f);
    // Anti-parallel to the wind vector, exactly.
    const glm::vec2 dir = glm::normalize(o);
    CHECK(dir.x == doctest::Approx(-wind.x));
    CHECK(dir.y == doctest::Approx(-wind.y));
    // An unnormalized direction must not scale the speed (a stale env value
    // is a direction, not a magnitude).
    const glm::vec2 o_scaled = cloud_drift_offset(wind * 7.0f, 1.0f, 10.0f);
    CHECK(glm::length(o_scaled) == doctest::Approx(glm::length(o)));
}

TEST_CASE("CONTROL: a becalmed state pins the field still") {
    // Rule 30's rejected case for the multiplier: weather_wind_mult 0 (a
    // dead-calm state) must freeze the drift entirely; if this returned any
    // motion the state tuple would not actually govern the wind.
    const glm::vec2 wind{1.0f, 0.0f};
    CHECK(cloud_drift_offset(wind, 0.0f, 1000.0f) == glm::vec2{0.0f, 0.0f});
    // Degenerate direction: stand still rather than drift along garbage.
    CHECK(cloud_drift_offset({0.0f, 0.0f}, 1.0f, 1000.0f)
          == glm::vec2{0.0f, 0.0f});
}

TEST_CASE("apply_clouds writes drift + wavelength and NOTHING else") {
    RenderEnvironment env;
    env.wind_direction = {0.0f, 1.0f};
    env.weather_wind_mult = 2.0f;
    const float cover = env.cloud_cover;
    const float cumulus = env.cloud_cumulus;
    const float shadow = env.cloud_shadow;
    // eye_xz joined the signature when the decks got a real altitude:
    // the field is sampled where the view ray MEETS the deck, so the
    // eye is now an input. Origin here keeps this case about drift.
    apply_clouds(env, 5.0f, glm::vec2(0.0f, 0.0f));
    // Offset: -dir * speed * mult * t.
    CHECK(env.cloud_offset_m.x == doctest::Approx(0.0f));
    CHECK(env.cloud_offset_m.y
          == doctest::Approx(-WIND_FIELD_DRIFT_SPEED_MPS * 2.0f * 5.0f));
    CHECK(env.cloud_wavelength_m == doctest::Approx(600.0f));
    // The STATE tuple is the app's/schedule's to write; the drift model must
    // never touch it (two writers of one tuple is the Rule 35 state defect).
    CHECK(env.cloud_cover == cover);
    CHECK(env.cloud_cumulus == cumulus);
    CHECK(env.cloud_shadow == shadow);
}

// ===========================================================================
// THE COVERAGE FIELD's distribution (Rule 31). These are the tests the first
// cloud shoot did not have: the sheet materialised only as a speckle band at
// the horizon with empty mid-sky, and the reason was measurable before any
// pixel was looked at. Every case below ships with cloud_field_raw — the
// SHIPPED, pre-remap field — as its control, and every one of them rejects it.
// ===========================================================================

namespace {

// A sample set spread over 40 km so the field is judged over many cells and
// not over one lucky neighbourhood.
std::vector<glm::vec2> field_samples(int n) {
    std::vector<glm::vec2> pts;
    pts.reserve(static_cast<size_t>(n));
    // Deterministic low-discrepancy walk: no RNG dependency, same set every
    // run, so a failure is reproducible from the test name alone.
    float a = 0.0f, b = 0.0f;
    for (int i = 0; i < n; ++i) {
        a = std::fmod(a + 0.7548776662f, 1.0f); // R2 sequence
        b = std::fmod(b + 0.5698402910f, 1.0f);
        pts.push_back({(a - 0.5f) * 40000.0f, (b - 0.5f) * 40000.0f});
    }
    return pts;
}

float worst_decile_error(const std::vector<float>& values) {
    std::vector<float> s = values;
    std::sort(s.begin(), s.end());
    float worst = 0.0f;
    for (int d = 0; d <= 10; ++d) {
        const float got = s[(s.size() - 1) * static_cast<size_t>(d) / 10];
        worst = std::max(worst, std::fabs(got - static_cast<float>(d) / 10.0f));
    }
    return worst;
}

} // namespace

TEST_CASE("the field is UNIFORM over its whole declared range") {
    const auto pts = field_samples(120000);
    std::vector<float> fixed, raw;
    fixed.reserve(pts.size());
    raw.reserve(pts.size());
    for (const glm::vec2& p : pts) {
        fixed.push_back(cloud_field(p, dfn::render::CLOUD_WAVELENGTH_M, 0.0f));
        raw.push_back(cloud_field_raw(p, dfn::render::CLOUD_WAVELENGTH_M));
    }
    // A threshold at 1-cover only means "cover" if the deciles land where a
    // uniform field's deciles land. Measured worst deviation of the remapped
    // field is 0.024; 0.05 sits above that and far below the control's 0.218.
    const float err_fixed = worst_decile_error(fixed);
    INFO("remapped worst decile error = ", err_fixed);
    CHECK(err_fixed < 0.05f);

    // CONTROL — the shipped octave sum. Three value-noise octaves added
    // together are Gaussian, and this is what that costs: the field never
    // leaves the middle of the range it declares.
    const float err_raw = worst_decile_error(raw);
    INFO("raw (shipped) worst decile error = ", err_raw);
    CHECK(err_raw > 0.15f); // it measures ~0.218 — the case that must FAIL
    CHECK(err_raw > err_fixed * 3.0f);
}

TEST_CASE("cover MEANS coverage, at both ends of the declared range") {
    const auto pts = field_samples(60000);
    // Rule 30's "a range is two assertions": 0.0 and 1.0 are asserted
    // explicitly, and so is the light end, which is where the shipped field
    // failed hardest and most invisibly — it drew a CLEAR SKY and nothing in
    // the pipeline said the request had been dropped.
    for (int ci = 0; ci <= 10; ++ci) {
        const float cover = static_cast<float>(ci) / 10.0f;
        int covered = 0;
        for (const glm::vec2& p : pts) {
            if (cloud_alpha(p, dfn::render::CLOUD_WAVELENGTH_M, cover, 0.0f)
                > 0.5f) {
                ++covered;
            }
        }
        const float frac = static_cast<float>(covered) / pts.size();
        INFO("cover ", cover, " -> covered fraction ", frac);
        CHECK(std::fabs(frac - cover) < 0.05f);
    }

    // CONTROL — the shipped threshold on the shipped field. cover 0.20 drew
    // 0.05% of the sky: the state tuple could not express light overcast at
    // all, which is exactly the "empty mid-sky" of the first shoot.
    int covered_raw = 0;
    for (const glm::vec2& p : pts) {
        const float f = cloud_field_raw(p, dfn::render::CLOUD_WAVELENGTH_M);
        if (f > 1.0f - 0.20f + 0.08f) { // old smoothstep(thr, thr+0.16) midpoint
            ++covered_raw;
        }
    }
    const float frac_raw = static_cast<float>(covered_raw) / pts.size();
    INFO("shipped field at cover 0.20 covered ", frac_raw);
    CHECK(frac_raw < 0.05f); // asked 0.20, drew ~0.000 — off by the whole ask
}

TEST_CASE("CONTROL: cover 0 erases the field, cover 1 fills it") {
    const auto pts = field_samples(4000);
    for (const glm::vec2& p : pts) {
        // The pass's control (DFN_CLOUD=0): sheet, cumulus and ground shadow
        // all read this one function, so zero here is zero everywhere.
        CHECK(cloud_alpha(p, dfn::render::CLOUD_WAVELENGTH_M, 0.0f, 0.0f)
              == 0.0f);
        CHECK(cloud_alpha(p, dfn::render::CLOUD_WAVELENGTH_M, 1.0f, 0.0f)
              > 0.99f);
    }
}

TEST_CASE("below the resolution limit the field converges to its AREA MEAN") {
    // The horizon problem: one pixel spans many cells, so a point sample there
    // is noise. The honest value is the area average — `cover` — and reaching
    // it is what replaced the blanket distance fade that had been carving a
    // hard shelf across the sky.
    const auto pts = field_samples(4000);
    const float cover = 0.45f;
    float worst = 0.0f;
    for (const glm::vec2& p : pts) {
        const float a =
            cloud_alpha(p, dfn::render::CLOUD_WAVELENGTH_M, cover, 4.0f);
        worst = std::max(worst, std::fabs(a - cover));
    }
    INFO("worst deviation from the area mean at 4 cells/pixel = ", worst);
    CHECK(worst < 0.01f);

    // CONTROL — a fully resolved sample must NOT be flattened, or the sheet
    // would be a uniform grey everywhere and the LOD would have eaten the
    // feature it exists to protect.
    float spread = 0.0f;
    for (const glm::vec2& p : pts) {
        const float a =
            cloud_alpha(p, dfn::render::CLOUD_WAVELENGTH_M, cover, 0.0f);
        spread = std::max(spread, std::fabs(a - cover));
    }
    CHECK(spread > 0.5f);
}

// ===========================================================================
// THE FIELD AT REDUCED SAMPLING RATES (R3.3). Rule 31 again, one level down:
// the first pass asserted the distribution at FULL resolution and left the LOD
// unasserted, and the LOD moves the distribution — replacing an octave by its
// mean shrinks the sum's spread, so a threshold calibrated on the full-
// resolution SD walks off the end of it. That is what drew the hard bright
// band at the horizon: a strip of cloud tone with the structure taken out.
//
// Every case below ships cloud_field_fixed_sd — the form that shipped until
// R3.3 — as the control, and every one of them rejects it.
// ===========================================================================

namespace {

// The sampling rates the sheet actually occupies between the zenith and the
// horizon. 0.60 is where the OLD outer convergence had finished throwing the
// field away; 0.80 is past every octave's own LOD, i.e. genuinely dead.
constexpr float kRates[] = {0.0f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f};

// z-score recovered from the field by inverting its own logistic remap. If the
// renormalisation is right this has mean 0 and SD 1 AT EVERY RATE — which is
// the uncorrelated-equal-variance premise stated as something a test can read,
// with no new API and no second copy of the octave sum to drift out of step.
float field_z(float f) {
    const float c = std::min(std::max(f, 1e-6f), 1.0f - 1e-6f);
    return std::log(c / (1.0f - c)) / 1.702f;
}

float sd_of(const std::vector<float>& v) {
    double s = 0.0;
    double s2 = 0.0;
    for (const float x : v) {
        s += x;
        s2 += static_cast<double>(x) * x;
    }
    const double m = s / static_cast<double>(v.size());
    // Clamped at zero: for a CONSTANT array — which is precisely what the
    // rejected control produces — the two accumulators cancel to a value that
    // rounds negative, and a NaN would report as "not less than" and let the
    // control pass by failing to be a number.
    const double var = std::max(0.0, s2 / static_cast<double>(v.size()) - m * m);
    return static_cast<float>(std::sqrt(var));
}

} // namespace

TEST_CASE("Rule 31: the spread that SURVIVES the LOD is predicted, not assumed") {
    // The renormalisation rests on the three octaves being uncorrelated with
    // equal marginal variance, so that the surviving spread is
    // CLOUD_FIELD_SD * sqrt(sum w_i^2) / 0.640156. That is a premise about a
    // noise field, which is exactly the kind of thing this file has been
    // wrong about before, so it is MEASURED here rather than reasoned about.
    const auto pts = field_samples(120000);
    for (const float rate : kRates) {
        std::vector<float> z;
        z.reserve(pts.size());
        for (const glm::vec2& p : pts) {
            z.push_back(field_z(
                cloud_field(p, dfn::render::CLOUD_WAVELENGTH_M, rate)));
        }
        const float sd = sd_of(z);
        INFO("rate ", rate, " -> SD of z = ", sd);
        // Measured over 200k samples the prediction tracks the truth to within
        // 0.03 % at every rate; 2 % is two orders of margin and still far
        // below the control's collapse.
        CHECK(std::fabs(sd - 1.0f) < 0.02f);
    }

    // CONTROL — the shipped form. Its z is (raw - full-res mean)/full-res SD,
    // whose spread IS the residual, so it collapses exactly as far as the LOD
    // has gone: 0.91 at rate 0.20, 0.39 at 0.50, 0.17 at 0.60. A threshold
    // held still against a distribution shrinking like that is the defect.
    for (const float rate : {0.40f, 0.50f, 0.60f}) {
        std::vector<float> z;
        z.reserve(pts.size());
        for (const glm::vec2& p : pts) {
            z.push_back(field_z(cloud_field_fixed_sd(
                p, dfn::render::CLOUD_WAVELENGTH_M, rate)));
        }
        const float sd = sd_of(z);
        INFO("CONTROL rate ", rate, " -> SD of z = ", sd);
        CHECK(sd < 0.70f);
        // And it collapses BY the residual, which is the diagnosis itself
        // rather than just a failure: the two agree to a few percent.
        CHECK(sd == doctest::Approx(dfn::render::cloud_lod_residual(rate))
                        .epsilon(0.03));
    }
}

TEST_CASE("cover MEANS coverage at EVERY sampling rate, not only at full res") {
    const auto pts = field_samples(120000);
    for (const float rate : kRates) {
        std::vector<float> v;
        v.reserve(pts.size());
        for (const glm::vec2& p : pts) {
            v.push_back(cloud_field(p, dfn::render::CLOUD_WAVELENGTH_M, rate));
        }
        const float err = worst_decile_error(v);
        INFO("rate ", rate, " -> worst decile error ", err);
        CHECK(err < 0.05f); // measures 0.023..0.035 across the whole set
    }

    // CONTROL — the shipped form, at the rates the horizon band occupies.
    // MEASURED, and it is worse than the diagnosis claimed: at rate 0.50 a
    // requested cover of 0.15 drew 0.0000 of the plane and 0.60 drew 1.0000,
    // i.e. both ends of the range collapsed into the two constants a field can
    // be. That is the bright flat strip, in numbers, before any pixel.
    for (const float rate : {0.50f, 0.60f}) {
        std::vector<float> v;
        v.reserve(pts.size());
        for (const glm::vec2& p : pts) {
            v.push_back(
                cloud_field_fixed_sd(p, dfn::render::CLOUD_WAVELENGTH_M, rate));
        }
        const float err = worst_decile_error(v);
        INFO("CONTROL rate ", rate, " -> worst decile error ", err);
        CHECK(err > 0.15f); // 0.19 at 0.50, 0.34 at 0.60
    }
}

TEST_CASE("the outer convergence fires on the RESIDUAL, not on cells/pixel") {
    const auto pts = field_samples(40000);
    const float cover = 0.45f;

    // The residual is the quantity, and its two ends are both asserted
    // (Rule 30's "a range is two assertions"): full field at full resolution,
    // nothing left once every octave is past its own LOD.
    CHECK(dfn::render::cloud_lod_residual(0.0f) == doctest::Approx(1.0f));
    CHECK(dfn::render::cloud_lod_residual(0.80f) == doctest::Approx(0.0f));
    // And the number the whole diagnosis turns on: at cells/px 0.60, where the
    // old convergence had finished, a sixth of the field is still alive.
    CHECK(dfn::render::cloud_lod_residual(0.60f)
          == doctest::Approx(0.1675f).epsilon(0.01));

    std::vector<float> live;
    live.reserve(pts.size());
    for (const glm::vec2& p : pts) {
        live.push_back(
            cloud_alpha(p, dfn::render::CLOUD_WAVELENGTH_M, cover, 0.60f));
    }
    const float sd_live = sd_of(live);
    INFO("alpha SD at cells/px 0.60 = ", sd_live);
    // Full resolution measures 0.4747. Structure must SURVIVE here, because
    // this rate is the middle of the band the user is looking at.
    CHECK(sd_live > 0.40f);

    // CONTROL — the shipped convergence at the same rate, computed here so the
    // rejected case is in the test rather than in a comment:
    // mix(a, cover, smoothstep(0.20, 0.60, cells_px)) is a full replacement by
    // the area mean at 0.60, i.e. SD exactly 0. A flat strip.
    float t = (0.60f - 0.20f) / (0.60f - 0.20f);
    t = t * t * (3.0f - 2.0f * t);
    std::vector<float> shipped;
    shipped.reserve(pts.size());
    for (const glm::vec2& p : pts) {
        const float a = cloud_alpha(p, dfn::render::CLOUD_WAVELENGTH_M, cover,
                                    0.60f);
        shipped.push_back(a + (cover - a) * t);
    }
    const float sd_shipped = sd_of(shipped);
    INFO("CONTROL (shipped window) alpha SD at 0.60 = ", sd_shipped);
    CHECK(sd_shipped < 0.001f);
    CHECK(sd_live > sd_shipped * 100.0f);

    // Past the residual window the answer is still exactly the area mean —
    // the convergence was moved, not deleted.
    for (const glm::vec2& p : pts) {
        CHECK(cloud_alpha(p, dfn::render::CLOUD_WAVELENGTH_M, cover, 0.90f)
              == doctest::Approx(cover));
    }
}

// ===========================================================================
// THE 3-D FIELD (R3.1). Same assertions as the 2-D one, because the same
// defect is possible: a Gaussian sum thresholded as though it were uniform.
// ===========================================================================

namespace {

// A deterministic walk through a volume, same construction as field_samples.
std::vector<glm::vec3> field3_samples(int n) {
    std::vector<glm::vec3> pts;
    pts.reserve(static_cast<size_t>(n));
    float a = 0.0f, b = 0.0f, c = 0.0f;
    for (int i = 0; i < n; ++i) {
        a = std::fmod(a + 0.8191725134f, 1.0f); // R3 sequence
        b = std::fmod(b + 0.6710436067f, 1.0f);
        c = std::fmod(c + 0.5497004779f, 1.0f);
        pts.push_back({(a - 0.5f) * 800.0f, (b - 0.5f) * 800.0f,
                       (c - 0.5f) * 800.0f});
    }
    return pts;
}

std::vector<float> field3_values(const std::vector<glm::vec3>& pts, float mean,
                                 float sd) {
    std::vector<float> v;
    v.reserve(pts.size());
    for (const glm::vec3& q : pts) {
        v.push_back(dfn::render::cloud_field3_with(q, mean, sd));
    }
    return v;
}

} // namespace

TEST_CASE("the 3-D field is UNIFORM over its whole declared range") {
    const auto pts = field3_samples(120000);
    const float err = worst_decile_error(field3_values(
        pts, dfn::render::CLOUD_FIELD3_MEAN, dfn::render::CLOUD_FIELD3_SD));
    INFO("worst decile error = ", err);
    CHECK(err < 0.030f);
}

TEST_CASE("CONTROL: the 2-D mean/SD are MEASURABLY WRONG in 3-D") {
    // THE REASON THE SECOND PAIR OF CONSTANTS EXISTS -- stated at its true size,
    // which is smaller than first claimed. Reusing the 2-D numbers does NOT
    // reproduce Rule 31's original severity (there, cover 0.10 drew literally
    // 0.0000 of the sky). It reproduces its FORM at about a fifth of its size:
    // the 3-D sum is tighter (sd 0.1185 against 0.1368 -- a trilinear blend of
    // eight iid uniforms beats a bilinear blend of four), so the 2-D sd
    // over-widens the remap and compresses the field toward its middle.
    //
    // The damage is therefore all at the ENDS, which is exactly where the
    // cumulus band lives: measured, cover 0.05 draws 0.0219 -- 44 % short --
    // and cover 0.90 draws 0.9317. Worst coverage error 0.0317 against the
    // correct pair's 0.0176, i.e. 1.8x. Small, systematic, and in the one place
    // a few fair-weather cumulus on a clear day are asked for.
    //
    // Asserted on COVERAGE rather than on deciles because coverage is what the
    // constants are FOR: by decile error the two pairs are 0.0282 vs 0.0221 and
    // the difference nearly vanishes, which is how this went unnoticed until
    // the control was written.
    const auto pts = field3_samples(120000);
    const auto worst_cover_err = [&](float mean, float sd) {
        float worst = 0.0f;
        for (const float cover : {0.05f, 0.10f, 0.25f, 0.50f, 0.75f, 0.90f}) {
            int hit = 0;
            for (const glm::vec3& q : pts) {
                hit += dfn::render::cloud_field3_with(q, mean, sd)
                               > (1.0f - cover)
                           ? 1
                           : 0;
            }
            worst = std::max(worst,
                             std::fabs(static_cast<float>(hit)
                                           / static_cast<float>(pts.size())
                                       - cover));
        }
        return worst;
    };
    const float with_2d = worst_cover_err(dfn::render::CLOUD_FIELD_MEAN,
                                          dfn::render::CLOUD_FIELD_SD);
    const float with_3d = worst_cover_err(dfn::render::CLOUD_FIELD3_MEAN,
                                          dfn::render::CLOUD_FIELD3_SD);
    INFO("worst coverage error -- 2-D constants = ", with_2d,
         "   3-D constants = ", with_3d);
    CHECK(with_2d > 0.028f);
    CHECK(with_3d < 0.026f);
    CHECK(with_3d < with_2d * 0.85f);

    // THE SHARP END OF THE SAME CLAIM, and the one worth keeping: the aggregate
    // above only separates the pairs by ~1.4x, but at the SPARSE end -- a few
    // fair-weather cumulus on a clear day, which is precisely what this field
    // draws -- the 2-D constants lose a third of the cloud that was asked for.
    const auto admitted = [&](float mean, float sd, float cover) {
        int hit = 0;
        for (const glm::vec3& q : pts) {
            hit += dfn::render::cloud_field3_with(q, mean, sd) > (1.0f - cover)
                       ? 1
                       : 0;
        }
        return static_cast<float>(hit) / static_cast<float>(pts.size());
    };
    const float sparse_2d = admitted(dfn::render::CLOUD_FIELD_MEAN,
                                     dfn::render::CLOUD_FIELD_SD, 0.05f);
    const float sparse_3d = admitted(dfn::render::CLOUD_FIELD3_MEAN,
                                     dfn::render::CLOUD_FIELD3_SD, 0.05f);
    INFO("cover 0.05 admits -- 2-D constants ", sparse_2d, "   3-D constants ",
         sparse_3d);
    CHECK(sparse_2d < 0.035f);  // asked 0.05, drew ~0.022
    CHECK(sparse_3d > 0.038f);
}

TEST_CASE("in 3-D too, cover MEANS coverage") {
    // A threshold at 1-cover must admit `cover` of SPACE — which is what the
    // cumulus band's base_cover/top_cover pair relies on to mean anything.
    const auto pts = field3_samples(120000);
    for (const float cover : {0.10f, 0.25f, 0.50f, 0.75f, 0.90f}) {
        int hit = 0;
        for (const glm::vec3& q : pts) {
            hit += dfn::render::cloud_field3(q) > (1.0f - cover) ? 1 : 0;
        }
        const float got =
            static_cast<float>(hit) / static_cast<float>(pts.size());
        INFO("cover ", cover, " -> ", got);
        CHECK(std::fabs(got - cover) < 0.030f);
    }
}

TEST_CASE("the 3-D field VARIES WITH HEIGHT -- the domes' actual cause") {
    // The band's silhouette was a single-valued function of azimuth for one
    // reason: the field did not depend on height AT ALL. Moving only the
    // vertical coordinate must move the field, or the fix was cosmetic.
    float worst = 0.0f;
    for (int i = 0; i < 400; ++i) {
        const float x = static_cast<float>(i) * 0.37f;
        worst = std::max(worst,
                         std::fabs(dfn::render::cloud_field3({x, 0.20f, 1.7f})
                                   - dfn::render::cloud_field3({x, 1.40f, 1.7f})));
    }
    INFO("largest change from height alone = ", worst);
    CHECK(worst > 0.30f);
}
