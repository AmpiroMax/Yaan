/*
Created: 09:08:2026 - 23:49:27
Last updated: 10:08:2026 - 19:55:51
Module: tests
File: tests/core/LodSeamTests.cpp

Responsibility:
- The two things core and render must AGREE on, checked against both zones at
  once: the LOD ladder (until it lives in NUMBERS.md), and the inter-level
  height disagreement that render's skirt has to cover.

Dependencies:
- Uses: doctest, dfn_world (CoarseTerrain, Worldgen), dfn_render (TerrainLod).
- Used by: ctest (test_lod_seam).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- This file exists because a number two zones must agree on belongs to neither
  (Rule 35). It is the INTERIM guard while LOD_LEVEL_COUNT / LOD_NODE_VOXELS /
  LOD_VOXEL_SIZE_L0..L5 are requested into NUMBERS.md; when they land, both
  zones read dfn::config and the first case here becomes trivially true.
- Rule 30: the disagreement measure ships with three controls — a flat field
  and a linear ramp, which MUST measure zero, and a step field, which MUST
  measure the step. A measure that cannot fail is not a measurement.
*/
/*
UPD:
- 09:08:2026 - 23:49:27: Created with the LOD streaming half.
- 10:08:2026 - 19:55:51: The seam contract now covers the HEIGHT as well as the
  quantization, on both stands, with the pre-fix open-coded chain as its
  control. The control is EQUAL to the right answer on the testbed and wrong on
  16158 of 16641 forest-stand samples — that asymmetry is the finding, not a
  weak control: no amount of testing the stand everyone was looking at could
  have caught a copy that only diverges where a stand declares passes the copy
  never learned.
*/

#include "engine/world/sources/Chunk.h"
#include "engine/world/sources/CoarseTerrain.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/world/sources/WorldgenForest.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/render/sources/TerrainLod.h"

#include <algorithm>
#include <cmath>
#include <doctest/doctest.h>
#include <functional>
#include <vector>

namespace {

using HeightFn = std::function<float(glm::vec2)>;

/// The height a node actually hands render: the field, quantized and decoded.
/// Measuring the continuous field instead would report a disagreement the
/// renderer never sees, and hide the one it does.
[[nodiscard]] float sampled(const HeightFn& h, float x, float z) {
    return dfn::world::dequantize_height(dfn::world::quantize_height(h({x, z})));
}

struct Gap {
    float max_gap_m = 0.0f;        ///< worst T-junction opening
    float max_border_step_m = 0.0f;///< worst step between adjacent FINE samples
    glm::vec2 gap_at{0.0f};        ///< where the worst opening is, world xz
    glm::vec2 step_at{0.0f};
    std::size_t samples = 0;
};

/// Worst disagreement between a fine node's border samples and the straight
/// edge of a coarser neighbour, over the world.
///
/// This IS the T-junction crack: along a shared border the fine node has a
/// vertex every `fine` metres while the coarser node has one every `coarse`
/// metres, so the fine vertices in between sit off the coarse node's straight
/// edge by exactly this much. It is swept over lines across the whole world
/// rather than only over the borders a particular eye position produces — the
/// number is a property of the field at those two spacings, and a bound that
/// holds everywhere is the one a skirt constant can be derived from.
[[nodiscard]] Gap measure_gap(const HeightFn& h, float fine, float coarse,
                              glm::vec2 world_min, glm::vec2 world_max,
                              float line_spacing) {
    Gap out;
    const auto sweep = [&](bool along_z) {
        const float fixed_lo = along_z ? world_min.x : world_min.y;
        const float fixed_hi = along_z ? world_max.x : world_max.y;
        const float run_lo = along_z ? world_min.y : world_min.x;
        const float run_hi = along_z ? world_max.y : world_max.x;
        for (float f = fixed_lo; f <= fixed_hi; f += line_spacing) {
            float prev = 0.0f;
            bool have_prev = false;
            for (float r = run_lo; r <= run_hi - coarse; r += fine) {
                // The coarse lattice is rooted at world zero (node origins are
                // multiples of the node size, which is 128 coarse steps), so a
                // coarse sample sits at every multiple of `coarse`.
                const float r0 = std::floor(r / coarse) * coarse;
                const float t = (r - r0) / coarse;
                const auto at = [&](float run) {
                    return along_z ? sampled(h, f, run) : sampled(h, run, f);
                };
                const float fine_h = at(r);
                const float edge = (1.0f - t) * at(r0) + t * at(r0 + coarse);
                const glm::vec2 here = along_z ? glm::vec2{f, r} : glm::vec2{r, f};
                if (const float g = std::abs(fine_h - edge); g > out.max_gap_m) {
                    out.max_gap_m = g;
                    out.gap_at = here;
                }
                if (have_prev) {
                    if (const float s = std::abs(fine_h - prev); s > out.max_border_step_m) {
                        out.max_border_step_m = s;
                        out.step_at = here;
                    }
                }
                prev = fine_h;
                have_prev = true;
                ++out.samples;
            }
        }
    };
    sweep(true);
    sweep(false);
    return out;
}

} // namespace

TEST_CASE("the LOD ladder is the same table in both zones") {
    // Rule 35's interim guard. Element-wise, not "looks the same": a single
    // wrong entry means core builds a node covering different ground than
    // render asked for, at a step render did not expect, and nothing about that
    // failure looks like a bug in either zone on its own.
    REQUIRE(dfn::render::LOD_LEVEL_COUNT == dfn::world::COARSE_LEVEL_COUNT);
    REQUIRE(dfn::render::LOD_NODE_VOXELS == dfn::world::COARSE_NODE_VOXELS);
    for (uint32_t l = 0; l < dfn::render::LOD_LEVEL_COUNT; ++l) {
        CHECK(dfn::render::LOD_VOXEL_SIZE_M[l]
              == doctest::Approx(dfn::world::COARSE_VOXEL_SIZE_M[static_cast<uint8_t>(l)]));
        CHECK(dfn::render::lod_node_size_m(static_cast<uint8_t>(l))
              == doctest::Approx(dfn::world::coarse_node_size_m(static_cast<uint8_t>(l))));
    }
    // And the sample count: a node is its voxels plus the shared edge row, the
    // same 128/129 relation a chunk has.
    CHECK(dfn::world::COARSE_NODE_RESOLUTION == dfn::render::LOD_NODE_VOXELS + 1);
}

TEST_CASE("the disagreement measure detects disagreement") {
    const glm::vec2 wmin{0.0f};
    const glm::vec2 wmax{1024.0f};

    // CONTROL 1 — a flat field. Nothing to disagree about, and a measure that
    // reports anything here is measuring its own arithmetic.
    const Gap flat = measure_gap([](glm::vec2) { return 17.0f; }, 4.0f, 8.0f, wmin, wmax, 128.0f);
    REQUIRE(flat.samples > 1000);
    CHECK(flat.max_gap_m == doctest::Approx(0.0f));

    // CONTROL 2 — a linear ramp. Linear interpolation of a linear field is
    // exact, so the only residue is one quantization unit (~6 mm). This is the
    // control that separates "the measure is zero" from "the measure is dead":
    // it has non-trivial heights and still must read zero.
    const Gap ramp =
        measure_gap([](glm::vec2 p) { return 0.05f * p.x + 0.03f * p.y; }, 4.0f, 8.0f,
                    wmin, wmax, 128.0f);
    CHECK(ramp.max_border_step_m > 0.1f); // the field really does vary
    CHECK(ramp.max_gap_m < 0.02f);

    // CONTROL 3 — a step the coarse lattice cannot see. A 5 m wall at x = 502
    // falls between two 8 m samples, so the coarse edge crosses it as a ramp
    // while the fine samples stand on top of it. The measure must find it.
    const Gap step = measure_gap(
        [](glm::vec2 p) { return p.x >= 502.0f ? 5.0f : 0.0f; }, 4.0f, 8.0f, wmin, wmax,
        128.0f);
    CHECK(step.max_gap_m > 2.0f);
}

TEST_CASE("inter-level height disagreement table for render's skirt") {
    // The table promised to render. It is measured on the real 2x2 km testbed
    // field, with the same quantization the nodes ship, and compared against
    // the skirt render would hang from a node of the finer level.
    const dfn::world::WorldGenParams params{1, {0, 0}, {7, 7}};
    const dfn::world::WorldGenContext ctx = dfn::world::build_world_context(params);
    const HeightFn field = [&ctx](glm::vec2 p) { return dfn::world::terrain_height(ctx, p); };
    const glm::vec2 wmin{0.0f};
    const glm::vec2 wmax{2048.0f};

    bool all_covered = true;
    for (uint8_t fine = 0; fine + 1 < dfn::world::COARSE_LEVEL_COUNT; ++fine) {
        const auto coarse = static_cast<uint8_t>(fine + 1);
        const float vf = dfn::world::coarse_voxel_size_m(fine);
        const float vc = dfn::world::coarse_voxel_size_m(coarse);
        if (vc >= (wmax.x - wmin.x)) {
            MESSAGE("L" << int(fine) << "/L" << int(coarse)
                        << ": coarse step exceeds the world; no such border exists here");
            continue;
        }
        // Line spacing is coarser for the coarse pairs: the pair's own spacing
        // is what sets how many independent samples a line carries.
        const Gap g = measure_gap(field, vf, vc, wmin, wmax, std::max(64.0f, vc * 4.0f));
        const float skirt = dfn::render::lod_skirt_depth_m(fine, g.max_border_step_m);
        MESSAGE("L" << int(fine) << " (" << vf << " m) into L" << int(coarse) << " ("
                    << vc << " m): max gap " << g.max_gap_m << " m at (" << g.gap_at.x
                    << ", " << g.gap_at.y << "), gap/step "
                    << (g.max_border_step_m > 0.0f ? g.max_gap_m / g.max_border_step_m : 0.0f)
                    << ", worst adjacent step " << g.max_border_step_m << " m at ("
                    << g.step_at.x << ", " << g.step_at.y << "), skirt " << skirt
                    << " m, samples " << g.samples);
        CHECK(g.samples > 1000);
        // The gap is real terrain, not a flat field: if this ever reads zero,
        // the table is measuring nothing and the skirt below it means nothing.
        CHECK(g.max_gap_m > 0.0f);
        if (g.max_gap_m > skirt) {
            all_covered = false;
        }
    }
    // Render's provisional LOD_SKIRT_LADDER_RATIO must cover every pair; if it
    // does not, the number to change is in render's zone and this is the
    // measurement that says so.
    CHECK(all_covered);
}

TEST_CASE("the coarse node and the chunks build the SAME terrain, on every stand") {
    // THE EXACT-SEAM CONTRACT, RE-ARMED. It was proved once, on the testbed,
    // by extracting quantize_height/classify_surface so both builders call one
    // function each. The HEIGHT ITSELF was left as two open-coded copies of
    // "water -> entrance works -> pads -> clamp", with a comment in each
    // asserting they were the same chain. They were, until the forest stand's
    // branch (LF-8 erosion, then the path flatten) landed in terrain_height and
    // neither copy was told — at which point the coarse nodes were building a
    // different terrain from the chunks they have to meet, and nothing was red.
    //
    // A comment claiming two things are the same is not a mechanism that makes
    // them the same. Both now call compose_passes().
    for (const bool forest : {false, true}) {
        dfn::world::WorldGenParams p{1, {0, 0}, {3, 3}};
        if (forest) {
            p.layout = dfn::world::forest_stand_layout();
        }
        const dfn::world::WorldGenContext ctx = dfn::world::build_world_context(p);
        const dfn::world::CoarseNode node{0, 1, 2};
        const dfn::world::CoarseNodeData data = dfn::world::build_coarse_node(ctx, node);
        const glm::vec2 origin = dfn::world::coarse_node_origin_m(node);
        const float step = dfn::world::coarse_voxel_size_m(node.level);

        int mismatches = 0;
        int control_mismatches = 0;
        float worst = 0.0f;
        float control_lo = 0.0f;
        float control_hi = 0.0f;
        for (uint32_t z = 0; z < dfn::world::COARSE_NODE_RESOLUTION; ++z) {
            for (uint32_t x = 0; x < dfn::world::COARSE_NODE_RESOLUTION; ++x) {
                const glm::vec2 w = origin
                                  + glm::vec2{static_cast<float>(x) * step,
                                              static_cast<float>(z) * step};
                const float chunk_sample =
                    dfn::world::dequantize_height(dfn::world::quantize_height(dfn::world::terrain_height(ctx, w)));
                const float coarse_sample = dfn::world::dequantize_height(
                    data.heights[static_cast<std::size_t>(z) * dfn::world::COARSE_NODE_RESOLUTION + x]);
                if (coarse_sample != chunk_sample) {
                    ++mismatches;
                    worst = std::max(worst, std::fabs(coarse_sample - chunk_sample));
                }
                // THE CONTROL IS THE REAL REJECTED INSTANCE (Rule 30) — the
                // chain this builder actually open-coded until today, written
                // out verbatim: water -> entrance works -> pads -> clamp.
                //
                // AND IT IS EQUAL TO THE RIGHT ANSWER ON THE TESTBED. That is
                // not a weak control, it is the finding: the copy really was
                // the whole chain on the stand everyone was looking at, so no
                // amount of testing THERE could have caught it. It goes wrong
                // only where a stand declares passes the copy never learned —
                // and on that stand it is wrong almost everywhere.
                const float macro =
                    dfn::world::macro_height(ctx.params.seed, ctx.params.layout, w);
                const dfn::world::WaterSample cw =
                    dfn::world::water_at(ctx.hydrology, ctx.params.layout, w, macro);
                const float control = dfn::world::dequantize_height(dfn::world::quantize_height(
                    std::clamp(dfn::world::pads_height(
                                   ctx.sites, w,
                                   dfn::world::entrance_works_height(ctx.sites, w, cw.height)),
                               0.0f, static_cast<float>(dfn::config::WORLDGEN_MAX_HEIGHT))));
                if (control != chunk_sample) {
                    ++control_mismatches;
                }
                control_lo = std::min(control_lo, control - chunk_sample);
                control_hi = std::max(control_hi, control - chunk_sample);
            }
        }
        CAPTURE(forest);
        INFO("mismatches ", mismatches, " worst ", worst, " m; control mismatches ",
             control_mismatches, " spanning ", control_lo, " .. ", control_hi, " m");
        CHECK(mismatches == 0);
        // The control must FAIL on the stand whose passes are not identities,
        // and it is legitimately EQUAL on the testbed — where the open-coded
        // chain really was the whole chain. Stating both is the point: the
        // defect existed on exactly one stand and that is why it survived.
        if (forest) {
            CHECK(control_mismatches > 10000);
            CHECK(control_lo < -1.0f);
        } else {
            CHECK(control_mismatches == 0);
        }
    }
}
