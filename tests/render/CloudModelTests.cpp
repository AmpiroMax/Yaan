/*
Created: 10:08:2026 - 03:13:00
Last updated: 11:08:2026 - 14:43:44
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
    apply_clouds(env, 5.0f);
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
