/*
Created: 14:08:2026 - 20:17:48
Last updated: 14:08:2026 - 20:30:18
Module: tests/core
File: tests/core/MountainHorizonTests.cpp

Responsibility:
- THE HORIZON PROFILE: how much of what you can see from this world is
  mountain. The user's request is "чуть больше всяких больших гор", which is a
  claim about the VIEW, so the instrument reads the view (Rule 41): from a
  fixed lattice of standpoints, the maximum elevation angle of the skyline on
  each of 72 bearings.

Key items:
- horizon_profile(): per-bearing skyline elevation angle, degrees.
- The GENERATOR-side count beside it (Rule 47's corollary): how many landforms
  in this world carry relief >= MASSIF_RULE_MIN_RELIEF. A count is established
  in the generator and confirmed on the view, never counted off the view.

Dependencies:
- Uses: dfn_world (Worldgen, LayoutLoad), config.
- Used by: ctest (test_mountain_horizon). Runs from the repo ROOT — it opens
  the shipped layout asset by relative path, exactly as test_ground_relief does.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE STANDPOINT LATTICE IS FIXED AND UNFILTERED ON PURPOSE (Rule 47). Nothing
  is skipped for being in water, on rock or "unrepresentative": a sample set
  chosen by any property correlated with the thing under test is how an
  instrument comes to report "no effect" exactly where the effect is largest.
*/
/*
UPD:
- 14:08:2026 - 20:17:48: Created — the baseline read before any mountain lands.
- 14:08:2026 - 20:30:18: The r_min SWEEP replaced a single near-field cutoff, because the first
  read was measuring the ground under the eye: a 4 m rise 8 m away subtends 26
  deg, and two standpoints of the lattice reported p50 27.5 and 5.5 deg on a
  world whose highest point outside the crag is 28 m. The sweep is printed in
  full rather than resolved to one number (Rule 36: an exclusion chosen by
  magnitude makes the filter the result). Plus the LR stamp read-out with its
  BUILT relief measured over the tor footprint rather than at the centre
  sample, since §2.8.4's slabs are laterally offset.
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/LayoutLoad.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/world/sources/WorldgenMacro.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <vector>

using namespace dfn;
using world::WorldGenParams;

namespace {

constexpr float WORLD_M =
    static_cast<float>(config::WORLD_EXTENT_CHUNKS) * static_cast<float>(config::CHUNK_SIZE);
constexpr float EYE = static_cast<float>(config::PLAYER_EYE_HEIGHT);
constexpr int BEARINGS = 72;      ///< every 5 degrees
constexpr float RAY_STEP = 8.0f;  ///< m along the ray
/// "This bearing has a mountain in it." Derived, not chosen: SILHOUETTE_MIN_PX
/// is 8 px at INTERNAL_RES, i.e. an apparent size of distance/30 -- 1/30 rad =
/// 1.91 degrees is the angle at which a mass stops being a line on the horizon.
constexpr float MOUNTAIN_DEG = 1.91f;

const world::WorldGenContext& shipped_world() {
    static const world::WorldGenContext ctx = [] {
        WorldGenParams p;
        p.seed = 1;
        p.min_chunk = {0, 0};
        p.max_chunk = {static_cast<int>(config::WORLD_EXTENT_CHUNKS) - 1,
                       static_cast<int>(config::WORLD_EXTENT_CHUNKS) - 1};
        const auto lr = world::load_layout_file(
            "games/daggerfall_n/assets/world/testbed_layout.json", p.layout);
        REQUIRE_MESSAGE(lr.ok, "layout asset must load (run ctest from the repo root)");
        return world::build_world_context(p);
    }();
    return ctx;
}

/// Skyline elevation angle (degrees) per bearing from `eye`, marching the
/// FINAL height field outward to the world edge, ignoring everything nearer
/// than `r_min`.
///
/// r_min EXISTS BECAUSE A BUMP UNDER YOUR NOSE IS NOT A HORIZON, and it is
/// SWEPT rather than chosen (Rule 36 — an exclusion picked by magnitude makes
/// the filter the result). A 4 m rise 8 m away subtends 26°, so at r_min = 0
/// this quantity reports the ground you are standing on and calls it a
/// mountain. The sweep is printed in full so a reader can see what the
/// exclusion did instead of taking one number on trust. The band that matches
/// the SUBJECT is the acceptance distance of the smallest landform this
/// project calls a massif: relief MASSIF_RULE_MIN_RELIEF over a radius
/// satisfying MASSIF_ASPECT_MIN is 48 m, and d_accept = 3R (§1.6.1) is 143 m.
std::vector<float> horizon_profile(const world::WorldGenContext& ctx, glm::vec2 eye_xz,
                                   float r_min) {
    const float eye_y = world::terrain_height(ctx, eye_xz) + EYE;
    std::vector<float> out(BEARINGS, 0.0f);
    for (int b = 0; b < BEARINGS; ++b) {
        const float th = 2.0f * 3.14159265358979f * static_cast<float>(b)
                       / static_cast<float>(BEARINGS);
        const glm::vec2 dir{std::cos(th), std::sin(th)};
        float best = 0.0f;
        for (float r = RAY_STEP; r < WORLD_M * 1.5f; r += RAY_STEP) {
            const glm::vec2 p = eye_xz + dir * r;
            if (p.x < 0.0f || p.y < 0.0f || p.x > WORLD_M || p.y > WORLD_M) {
                break;
            }
            if (r < r_min) {
                continue;
            }
            const float h = world::terrain_height(ctx, p);
            best = std::max(best, std::atan2(h - eye_y, r));
        }
        out[static_cast<size_t>(b)] = best * 57.2957795f;
    }
    return out;
}

float percentile(std::vector<float> v, float q) {
    std::sort(v.begin(), v.end());
    const size_t i = static_cast<size_t>(q * static_cast<float>(v.size() - 1) + 0.5f);
    return v[std::min(i, v.size() - 1)];
}

} // namespace

TEST_CASE("horizon profile of the shipped world -- the baseline read") {
    const world::WorldGenContext& ctx = shipped_world();
    std::vector<float> all;
    float mountain_bearings = 0.0f;
    float total_bearings = 0.0f;
    std::printf("\n--- horizon profile, seed 1, shipped world (%.0f m square) ---\n", WORLD_M);
    std::printf("  standpoint      p50    p90    max   share of bearings >= %.2f deg\n",
                static_cast<double>(MOUNTAIN_DEG));
    for (int gz = 0; gz < 5; ++gz) {
        for (int gx = 0; gx < 5; ++gx) {
            const glm::vec2 eye{WORLD_M * (0.1f + 0.2f * static_cast<float>(gx)),
                                WORLD_M * (0.1f + 0.2f * static_cast<float>(gz))};
            const std::vector<float> prof = horizon_profile(ctx, eye, 0.0f);
            float share = 0.0f;
            for (float a : prof) {
                if (a >= MOUNTAIN_DEG) share += 1.0f;
            }
            share /= static_cast<float>(prof.size());
            mountain_bearings += share;
            total_bearings += 1.0f;
            std::printf("  (%6.0f,%6.0f) %6.2f %6.2f %6.2f   %5.1f%%\n",
                        static_cast<double>(eye.x), static_cast<double>(eye.y),
                        static_cast<double>(percentile(prof, 0.50f)),
                        static_cast<double>(percentile(prof, 0.90f)),
                        static_cast<double>(percentile(prof, 1.0f)),
                        static_cast<double>(share * 100.0f));
            all.insert(all.end(), prof.begin(), prof.end());
        }
    }
    std::printf("  WORLD: p50 %.2f  p90 %.2f  max %.2f  mountain share %.1f%%\n",
                static_cast<double>(percentile(all, 0.50f)),
                static_cast<double>(percentile(all, 0.90f)),
                static_cast<double>(percentile(all, 1.0f)),
                static_cast<double>(100.0f * mountain_bearings / total_bearings));
    CHECK(all.size() == 25u * BEARINGS);
}

TEST_CASE("the world's elevation, coarsely -- where is there room for a mountain") {
    const world::WorldGenContext& ctx = shipped_world();
    constexpr int N = 17;
    float hmax = 0.0f;
    glm::vec2 arg{0.0f, 0.0f};
    std::printf("\n--- elevation (m), %d x %d over %.0f m ---\n", N, N, WORLD_M);
    for (int gz = 0; gz < N; ++gz) {
        std::printf("  ");
        for (int gx = 0; gx < N; ++gx) {
            const glm::vec2 p{WORLD_M * static_cast<float>(gx) / static_cast<float>(N - 1),
                              WORLD_M * static_cast<float>(gz) / static_cast<float>(N - 1)};
            const float h = world::terrain_height(ctx, p);
            if (h > hmax) { hmax = h; arg = p; }
            std::printf("%4.0f", static_cast<double>(h));
        }
        std::printf("\n");
    }
    std::printf("  highest sampled: %.1f m at (%.0f, %.0f)\n",
                static_cast<double>(hmax), static_cast<double>(arg.x),
                static_cast<double>(arg.y));
    CHECK(hmax > 0.0f);
}

TEST_CASE("the regional massif -- the stamp, its relief, and what it does to the horizon") {
    const world::WorldGenContext& ctx = shipped_world();
    const world::CragStamp lr = world::regional_massif(ctx.params.layout);
    const float datum_off = world::terrain_height(
        ctx, lr.center + glm::vec2{lr.radius * 2.0f, 0.0f});
    // The summit is the max over the tor's own footprint, not the centre
    // sample: §2.8.4's slabs are laterally offset, so the highest point is not
    // at (0,0) of the stamp and reading the centre would under-report relief.
    float summit = 0.0f;
    for (int gz = -12; gz <= 12; ++gz) {
        for (int gx = -12; gx <= 12; ++gx) {
            summit = std::max(summit,
                              world::terrain_height(ctx, lr.center
                                                             + glm::vec2{static_cast<float>(gx),
                                                                         static_cast<float>(gz)}
                                                                   * 2.0f));
        }
    }
    std::printf("\n--- the regional massif (LR) ---\n");
    std::printf("  centre        (%.0f, %.0f)\n", static_cast<double>(lr.center.x),
                static_cast<double>(lr.center.y));
    std::printf("  radius        %.0f m   aretes %d   ruled relief %.0f m\n",
                static_cast<double>(lr.radius), lr.arete_count,
                static_cast<double>(lr.peak_height));
    std::printf("  distance from Ravenscar  %.0f m  (LANDMARK_HAZE_ONSET %d m)\n",
                static_cast<double>(glm::length(lr.center - ctx.params.layout.crag.center)),
                static_cast<int>(config::LANDMARK_HAZE_ONSET));
    std::printf("  summit %.1f m   ground beyond the foot %.1f m   BUILT RELIEF %.1f m\n",
                static_cast<double>(summit), static_cast<double>(datum_off),
                static_cast<double>(summit - datum_off));
}

TEST_CASE("horizon profile against near-field range -- what the r_min sweep does") {
    const world::WorldGenContext& ctx = shipped_world();
    std::printf("\n--- skyline angle vs how much near ground is excluded ---\n");
    std::printf("  r_min      p50    p90    max   share >= %.2f deg\n",
                static_cast<double>(MOUNTAIN_DEG));
    for (float r_min : {0.0f, 50.0f, 143.0f, 300.0f}) {
        std::vector<float> all;
        float share = 0.0f;
        for (int gz = 0; gz < 5; ++gz) {
            for (int gx = 0; gx < 5; ++gx) {
                const glm::vec2 eye{WORLD_M * (0.1f + 0.2f * static_cast<float>(gx)),
                                    WORLD_M * (0.1f + 0.2f * static_cast<float>(gz))};
                const std::vector<float> prof = horizon_profile(ctx, eye, r_min);
                for (float a : prof) {
                    if (a >= MOUNTAIN_DEG) share += 1.0f;
                }
                all.insert(all.end(), prof.begin(), prof.end());
            }
        }
        std::printf("  %5.0f m %6.2f %6.2f %6.2f   %5.1f%%\n", static_cast<double>(r_min),
                    static_cast<double>(percentile(all, 0.50f)),
                    static_cast<double>(percentile(all, 0.90f)),
                    static_cast<double>(percentile(all, 1.0f)),
                    static_cast<double>(100.0f * share / static_cast<float>(all.size())));
    }
}
