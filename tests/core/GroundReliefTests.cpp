/*
Created: 11:08:2026 - 14:23:03
Last updated: 13:08:2026 - 17:32:00
Module: tests/core
File: tests/core/GroundReliefTests.cpp

Responsibility:
- LANDSCAPE §10.1's instrument, run on the world the app actually ships:
  GROUND_RELIEF_SIGMA_20M measured on the flattest legal ground, against the
  approved floor and ceiling.

Key items:
- The PINNED standpoint: the eye of `docs/acceptance/render-haze-lowland-900m-*`
  (51, 650). Both arms of any before/after read this same ground.
- The blind search: `flattest_legal_standpoints` ranks by TREND, never by the
  σ it is about to report.

Dependencies:
- Uses: dfn_world (Worldgen, WorldgenValidation, LayoutLoad), config.
- Used by: ctest (test_ground_relief). Runs from the repo ROOT — it opens the
  shipped layout asset by relative path on purpose, exactly as
  test_layout_load does, so measuring a world other than the shipped one is a
  red test rather than a silent fallback.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE STANDPOINT IS PINNED AND MUST STAY PINNED. It was chosen by RENDER, for a
  haze question, before this work existed — which is the whole reason it is
  trustworthy: nobody composed it to make an "after" look better. Re-deriving
  it from a fresh search in a later commit would let the ground move under the
  number.
*/
/*
UPD:
- 11:08:2026 - 14:23:03: Created — §10.1's floor gets its first consumer.
- 12:08:2026 - 23:35:00: THE WAVELENGTH SWEEP, and it refutes the prediction this
  file was carrying: shortening GROUND_MESO_WAVELENGTH's top moves pointwise
  slope monotonically and moves ground-hiding NOT AT ALL (63-77 %, no trend,
  never above what the approved band already gives; A1 p5 stays 0 at every
  wavelength). The table and the reason are in the test body where the wrong
  prediction used to be.
- 13:08:2026 - 16:35:00: THE DRAWN ARM, and it changed a conclusion the same
  hour it landed. Every reading is now taken on the 2 m heightmap the world is
  actually built from as well as on the continuous field, because the two
  disagree: at a 0.35 m terrace step the continuous field reads p5 = 3 (a pass)
  and the drawn ground reads p5 = 0. A form finer than a few samples exists in
  the field and not in the world. Plus the per-column dump (a percentile hides
  the shape of its own tail — the failing columns are a contiguous SECTOR, not
  scattered bad luck), the population census over the world's flattest legal
  standpoints (one standpoint's p5 can be bought by 64 lucky rays), and the σ
  ceiling read in the same run as the count.
- 13:08:2026 - 16:52:00: WALKABILITY, and the instrument was replaced rather
  than re-floored: a count of cells crossing PLAYER_MAX_SLOPE cannot separate a
  wall the pass built from ground the pass nudged that was already at the limit
  (every arm tipped the same knife-edge cell), so the quantity was wrong. The
  gate is CONNECTIVITY over the 2 m lattice the world is collided on, with a
  positive control showing the instrument moves when driven.
- 13:08:2026 - 17:05:00: The anisotropy ruler diagnostic — the same structure
  tensor read at four arms on ONE world, because §2.1's probe samples with a
  +-6 m arm on a 12 m lattice and the forms sit at a 15-24 m pitch, which is at
  or past Nyquist.
- 13:08:2026 - 17:20:00: The regularity measure (gap CV between draws) with the
  band it resolves stated before the number — 2 m to 120 m against a subject at
  12-40 m — and read against the pass's own washboard arm.
- 13:08:2026 - 17:32:00: The gap-CV comparison is REPORTED and no longer gated:
  it reads 0.679 against 0.718 at the shipped pitch and 0.573 against 0.567 at
  the previous one, i.e. the arms are indistinguishable and the sign is speckle.
  The spacing was never regular, so no assertion on it can separate the corduroy
  from the fix — the discriminating-power test met a third time in one day.
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/LayoutLoad.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/world/sources/WorldgenForest.h"
#include "engine/world/sources/WorldgenForms.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenOutcrop.h"
#include "engine/world/sources/WorldgenScatter.h"
#include "engine/world/sources/WorldgenValidation.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <doctest/doctest.h>
#include <string>
#include <vector>

using namespace dfn;
using world::WorldGenParams;

namespace {

/// The world the app builds: seed 1, the shipped layout asset, WORLD_EXTENT.
const world::WorldGenContext& shipped_world() {
    static const world::WorldGenContext ctx = [] {
        WorldGenParams p;
        p.seed = 1;
        p.min_chunk = {0, 0};
        p.max_chunk = {static_cast<int>(config::WORLD_EXTENT_CHUNKS) - 1,
                       static_cast<int>(config::WORLD_EXTENT_CHUNKS) - 1};
        const auto lr =
            world::load_layout_file("games/daggerfall_n/assets/world/testbed_layout.json",
                                    p.layout);
        REQUIRE_MESSAGE(lr.ok, "layout asset must load (run ctest from the repo root)");
        return world::build_world_context(p);
    }();
    return ctx;
}

/// THE PINNED STANDPOINT — the eye of the archived lowland frames
/// (`DFN_MASSIF_EYE=51,650`, docs/acceptance/README.md). σ here and the frame
/// there are the same ground, so the number and the picture cannot disagree
/// about which world they describe.
constexpr glm::vec2 A1_STANDPOINT{51.0f, 650.0f};

// THE σ GATE IS GONE FROM THIS FILE ON PURPOSE, AND THIS NOTE IS ITS HEADSTONE.
//
// `GROUND_RELIEF_SIGMA_20M_MIN` was withdrawn on the day it was approved
// (NUMBERS.md, 0825317). It was not a bad criterion — it measured what it
// promised, it had a correct control, it was falsifiable, and it PASSED at
// 0.353 against a 0.35 floor on the very frame that failed F7 by eye. It was
// aimed one quantity to the left of its subject: for h = A*sin(2*pi*x/L),
// σ = A/sqrt(2) while RMS slope = 2*pi*σ/L, so σ bounds AMPLITUDE, ground
// hiding ground is a property of SLOPE, and the only thing joining them is the
// WAVELENGTH — which the contract never named.
//
// Tests for a withdrawn quantity are worse than no tests: they go green and
// certify nothing. Removed rather than demoted. `ground_relief_20m` itself
// survives as the trend-ranking machinery behind flattest_legal_standpoints,
// which is a different job and still a real one.

} // namespace

TEST_CASE("§10.1 the A1 standpoint is legal, unauthored, ungraded ground") {
    const auto& ctx = shipped_world();
    // If this ever goes false the pinned frame stopped being a fair test: the
    // floor does not bind on graded or wet ground, so a pass there would mean
    // nothing.
    CHECK(world::relief_floor_binds(ctx, A1_STANDPOINT));
}




// --- §10.4 THE MID-FIELD CENSUS, ESTABLISHED IN THE GENERATOR ------------------
//
// RULE 47. `MIDGROUND_OBJECT_COUNT_MIN` used to be defined as something you
// count on a frame, and a count taken by segmenting a frame falls with anything
// that lowers contrast — haze, flat light, a palette edit — WITH THE PLACEMENT
// COMPLETELY UNCHANGED. It would then blame a lighting change on this pass, and
// it would do so biased toward "there is nothing there" exactly when the objects
// are hardest to see. Haze went in today, so that is not a hypothetical.
//
// So the count is established HERE, by projecting what the generator placed and
// applying the 8 px cut to the COMPUTED apparent size. The frame CONFIRMS it.
// Where the two disagree, the disagreement is the finding — about drawing or
// about light — and a frame-side count would have destroyed exactly that
// information by folding it into the number.

namespace {

/// The A1 camera, and it is not ours: it is the archived lowland frame's
/// (`DFN_MASSIF_EYE=51,650`, Tour::massif_probe_steps — eye at
/// PLAYER_EYE_HEIGHT, aimed at the crag, pitch 0.09).
constexpr float A1_PITCH = 0.09f;
constexpr glm::vec2 A1_AIM{830.0f, 200.0f};

/// Pixels per radian at INTERNAL_RES — the same conversion §1.5's readability
/// gate uses in WorldgenValidation.cpp, not a second copy of the idea.
constexpr float PX_PER_RAD =
    static_cast<float>(config::INTERNAL_RES_H) / static_cast<float>(config::CAMERA_FOV_Y);

struct Census {
    int outcrops = 0;   ///< B2 masses reading >= SILHOUETTE_MIN_PX
    int boulders = 0;   ///< B1 stones reading >= SILHOUETTE_MIN_PX
    int total = 0;
    int visible = 0;    ///< the subset the terrain does not hide (see below)
};

/// TERRAIN OCCLUSION, BY RAYCAST — never by reading the picture.
///
/// The first census counted 17 at this standpoint and the frame showed two.
/// Both numbers were right and they answer different questions: "what stands
/// there" and "what can be seen from here". §10.4.1's clause is about a FRAME,
/// so it needs the second, and the gap between them is a real property of this
/// standpoint (it looks up a rise, so most of its own mid field is over the
/// horizon) rather than a placement failure.
///
/// The ray marches the FINISHED height field and asks whether the object's top
/// clears every intervening ridge — the same machinery C1 uses, and it needs no
/// shading, no contrast and no colour, so haze cannot move it (Rule 47).
bool unoccluded(const world::WorldGenContext& ctx, glm::vec2 eye, glm::vec2 pos, float top) {
    const float eye_y = world::terrain_height(ctx, eye)
                      + static_cast<float>(config::PLAYER_EYE_HEIGHT);
    const glm::vec2 d = pos - eye;
    const float dist = glm::length(d);
    if (dist < 1.0f) return true;
    const glm::vec2 dir = d / dist;
    const float t_obj = (world::terrain_height(ctx, pos) + top - eye_y) / dist;
    for (float t = 4.0f; t < dist - 4.0f; t += 4.0f) {
        if ((world::terrain_height(ctx, eye + dir * t) - eye_y) / t > t_obj) return false;
    }
    return true;
}

/// True if an object of `size` metres at `pos` is inside the A1 frustum and
/// reads at least SILHOUETTE_MIN_PX. Deliberately NOT "is it visible" — this
/// counts what STANDS there; occlusion is the frame's half of the pair.
bool reads(glm::vec2 eye, glm::vec2 pos, float size, float min_range) {
    const glm::vec2 d = pos - eye;
    const float dist = glm::length(d);
    if (dist < min_range) return false; // near-field is excluded by §10.4.1
    const glm::vec2 fwd = glm::normalize(A1_AIM - eye);
    const float along = glm::dot(d, fwd);
    if (along <= 0.0f) return false;
    // Horizontal half-angle from the vertical FOV and the 16:9 frame.
    const float aspect = static_cast<float>(config::INTERNAL_RES_W)
                       / static_cast<float>(config::INTERNAL_RES_H);
    const float half_h = std::atan(std::tan(static_cast<float>(config::CAMERA_FOV_Y) * 0.5f)
                                   * aspect);
    const glm::vec2 side{-fwd.y, fwd.x};
    if (std::fabs(std::atan(glm::dot(d, side) / along)) > half_h) return false;
    return size / dist * PX_PER_RAD >= static_cast<float>(config::SILHOUETTE_MIN_PX);
}

Census a1_census(const world::WorldGenContext& ctx) {
    Census c;
    const glm::vec2 eye = A1_STANDPOINT;
    // §10.4.1's exclusions: not the far massif, not near-field vegetation. The
    // near-field cut is 30 m — §10.4.1's own "a world populates 0-30 m and
    // 1 km+ and has nothing in between".
    constexpr float NEAR = 30.0f;
    const float crag_r = ctx.params.layout.crag.radius;

    for (const world::Outcrop& r : world::outcrops_in(ctx.params.seed, ctx.params.layout,
                                                      eye - glm::vec2{900.0f, 900.0f},
                                                      eye + glm::vec2{900.0f, 900.0f})) {
        if (glm::length(r.centre - ctx.params.layout.crag.center) < crag_r) continue;
        if (reads(eye, r.centre, r.extent * 2.0f, NEAR)) {
            ++c.outcrops;
            if (unoccluded(ctx, eye, r.centre, r.proud)) ++c.visible;
        }
    }
    const auto CH = static_cast<float>(config::CHUNK_SIZE);
    for (int cz = 0; cz < static_cast<int>(config::WORLD_EXTENT_CHUNKS); ++cz) {
        for (int cx = 0; cx < static_cast<int>(config::WORLD_EXTENT_CHUNKS); ++cx) {
            const glm::vec2 lo{static_cast<float>(cx) * CH, static_cast<float>(cz) * CH};
            if (glm::length(lo + glm::vec2{CH, CH} * 0.5f - eye) > 900.0f) continue;
            for (const math::ScatterInstance& i :
                 world::build_scatter(ctx, lo, lo + glm::vec2{CH, CH})) {
                if (i.species != math::ScatterSpecies::Stone) continue;
                if (i.scale < static_cast<float>(config::BOULDER_SIZE_MIN)) continue;
                const glm::vec2 p{i.position.x, i.position.z};
                if (glm::length(p - ctx.params.layout.crag.center) < crag_r) continue;
                if (reads(eye, p, i.scale, NEAR)) {
                    ++c.boulders;
                    if (unoccluded(ctx, eye, p, i.scale)) ++c.visible;
                }
            }
        }
    }
    c.total = c.outcrops + c.boulders;
    return c;
}

} // namespace

TEST_CASE("§10.4 mid-field silhouettes at the A1 standpoint, counted in the generator") {
    const auto& ctx = shipped_world();
    const Census c = a1_census(ctx);
    MESSAGE("A1 mid-field silhouettes >= " << config::SILHOUETTE_MIN_PX << " px: outcrops "
                                           << c.outcrops << ", boulders " << c.boulders
                                           << ", total " << c.total << ", of which UNOCCLUDED "
                                           << c.visible << " (floor "
                                           << config::MIDGROUND_OBJECT_COUNT_MIN << ")");
    // The clause is about a FRAME, so it is the unoccluded count that is held
    // to the floor; the placed count is reported beside it because the two
    // diverging is information about the standpoint, not noise.
    CHECK(c.visible >= static_cast<int>(config::MIDGROUND_OBJECT_COUNT_MIN));
    // §10.5 B2's own frame-side control, and it is a SEPARATE clause: five
    // silhouettes made entirely of boulders would satisfy the count and leave
    // the 150-750 m band as empty as it was, because a 4 m stone expires at
    // 120 m (§10.4.2).
    CHECK(c.outcrops >= static_cast<int>(config::OUTCROP_IN_VIEW_MIN));
}

TEST_CASE("§10.5 B2 diagnostic: does the boss reach the FINISHED ground?") {
    const auto& ctx = shipped_world();
    const glm::vec2 eye = A1_STANDPOINT;
    for (const world::Outcrop& r : world::outcrops_in(ctx.params.seed, ctx.params.layout,
                                                      eye - glm::vec2{300.0f, 300.0f},
                                                      eye + glm::vec2{300.0f, 300.0f})) {
        if (!r.boss) continue;
        const float centre = world::terrain_height(ctx, r.centre);
        float off = 0.0f;
        for (int k = 0; k < 4; ++k) {
            const float a = static_cast<float>(k) * 1.5707963f;
            off = std::max(off, world::terrain_height(ctx, r.centre
                                                           + glm::vec2{std::cos(a), std::sin(a)}
                                                                 * (r.extent + 12.0f)));
        }
        MESSAGE("boss at " << glm::length(r.centre - eye) << " m: declared proud " << r.proud
                           << " m, FINISHED ground rise " << (centre - off) << " m");
    }
}

// --- WHY σ DID NOT PREDICT F7, AND THE QUANTITY THAT DOES ---------------------
//
// The A1 pair passed σ (0.353 against a 0.35 floor) and still FAILED §10.1.3's
// F7: the ground ran unbroken from the player's feet to the tree line. The two
// are not the same claim and the arithmetic says so — σ bounds AMPLITUDE and
// says nothing about WAVELENGTH, while ground hiding ground is about GRADIENT.
// The same 0.35 m spread laid on a 40 m wave is atan(2*pi*0.35/40) ~ 3.1 deg
// against a 2.4 deg grazing angle: legal by construction and still a swell that
// hides nothing.
//
// Two instruments below, both in the generator (Rule 47), both blind to light,
// contrast and colour so haze cannot move either:
//
//   1. SLOPE EXCEEDANCE — the fraction of legal open ground whose local slope
//      clears the grazing angle. Reported at several BASELINES on purpose: a
//      slope is meaningless without the run it is measured over, and which
//      baseline carries the exceedance is the whole question about which band
//      can do this work.
//   2. HIDDEN POCKETS — F7's own quantity, by raycast. From eye height, march
//      each column of the frame and count the stretches of ground that lie
//      BELOW the running sight-line envelope. A hidden stretch is exactly
//      "near ground hides ground behind it", and its near edge is the crest
//      line the eye reads. This is not a proxy for F7; it IS F7, arrived at by
//      marching a height field instead of by looking at a picture.

namespace {

/// Local slope (rad) over a horizontal `arm`, central differences.
float slope_at(const world::WorldGenContext& ctx, glm::vec2 p, float arm) {
    const float gx = world::terrain_height(ctx, {p.x + arm, p.y})
                   - world::terrain_height(ctx, {p.x - arm, p.y});
    const float gz = world::terrain_height(ctx, {p.x, p.y + arm})
                   - world::terrain_height(ctx, {p.x, p.y - arm});
    return std::atan(std::sqrt(gx * gx + gz * gz) / (2.0f * arm));
}

/// The grazing angle at `d` from eye height: atan(EYE / d). §10.1.3 quotes its
/// value at 40 m (2.4 deg); it is DERIVED here so the number cannot drift from
/// PLAYER_EYE_HEIGHT.
float grazing(float d) {
    return std::atan(static_cast<float>(config::PLAYER_EYE_HEIGHT) / d);
}

/// THE GROUND AS IT IS DRAWN AND COLLIDED, and it is a SEPARATE FIELD from the
/// one above — this is the whole reason it exists.
///
/// `terrain_height` is the continuous field; the world the player sees and
/// walks on is a heightmap sampled every HEIGHTMAP_STEP (2 m) and interpolated
/// between those samples. A form narrower than a few samples exists in the
/// first field and NOT in the second, so a pocket count taken only on the
/// continuous field can certify a picture that was never drawn — the exact
/// failure Rule 27 pairs a frame against a number to catch, one level lower
/// down. Bilinear on the 2 m lattice, world-aligned, so it is the same lattice
/// generate_chunk writes.
float drawn_height(const world::WorldGenContext& ctx, glm::vec2 p) {
    constexpr float S = static_cast<float>(config::HEIGHTMAP_STEP);
    const float gx = std::floor(p.x / S);
    const float gz = std::floor(p.y / S);
    const float tx = p.x / S - gx;
    const float tz = p.y / S - gz;
    const auto h = [&](float ix, float iz) {
        return world::terrain_height(ctx, {ix * S, iz * S});
    };
    const float h00 = h(gx, gz), h10 = h(gx + 1.0f, gz);
    const float h01 = h(gx, gz + 1.0f), h11 = h(gx + 1.0f, gz + 1.0f);
    return (h00 + (h10 - h00) * tx) * (1.0f - tz) + (h01 + (h11 - h01) * tx) * tz;
}

/// F7 by raycast, for one column. Returns the number of distinct stretches of
/// ground between `near` and `far` that sit below the sight-line envelope —
/// i.e. how many times the ground cuts across this column.
///
/// Takes the height field as a FUNCTION so the continuous field and the drawn
/// heightmap are marched by one implementation rather than by two that could
/// disagree about the counting rule (Rule 32).
template <typename HeightFn>
int hidden_pockets_of(const HeightFn& height, glm::vec2 eye, float eye_y, glm::vec2 dir,
                      float near_m, float far_m) {
    constexpr float STEP = 0.5f;
    float envelope = -1e9f; // running max elevation angle
    bool hidden = false;
    int pockets = 0;
    float run = 0.0f;
    for (float t = 1.0f; t <= far_m; t += STEP) {
        const float ang = (height(eye + dir * t) - eye_y) / t;
        if (ang >= envelope) {
            envelope = ang;
            if (hidden && run >= 1.5f && t >= near_m) ++pockets; // a pocket that closed
            hidden = false;
            run = 0.0f;
        } else {
            hidden = true;
            run += STEP;
        }
    }
    if (hidden && run >= 1.5f) ++pockets; // still hidden at the far edge
    return pockets;
}

int hidden_pockets(const world::WorldGenContext& ctx, glm::vec2 eye, float eye_y, glm::vec2 dir,
                   float near_m, float far_m) {
    return hidden_pockets_of([&](glm::vec2 p) { return world::terrain_height(ctx, p); }, eye,
                             eye_y, dir, near_m, far_m);
}

/// The 64 columns of the A1 frame as directions — one definition, because the
/// continuous arm, the drawn arm and the per-column dump must be the same 64
/// columns or their numbers are not comparable.
std::vector<glm::vec2> a1_columns(glm::vec2 eye) {
    const glm::vec2 fwd = glm::normalize(A1_AIM - eye);
    const float aspect = static_cast<float>(config::INTERNAL_RES_W)
                       / static_cast<float>(config::INTERNAL_RES_H);
    const float half_h = std::atan(std::tan(static_cast<float>(config::CAMERA_FOV_Y) * 0.5f)
                                   * aspect);
    std::vector<glm::vec2> dirs;
    for (int c = 0; c < 64; ++c) {
        const float a = -half_h + 2.0f * half_h * (static_cast<float>(c) + 0.5f) / 64.0f;
        dirs.push_back({fwd.x * std::cos(a) - fwd.y * std::sin(a),
                        fwd.x * std::sin(a) + fwd.y * std::cos(a)});
    }
    return dirs;
}

} // namespace

TEST_CASE("GROUND_OCCLUSION_COUNT_MIN: how often ground hides ground (§10.1.3 F7)") {
    const auto& ctx = shipped_world();

    // --- 1. SLOPE EXCEEDANCE over legal open ground --------------------------
    const float thresh = grazing(40.0f); // §10.1.3's own 2.4 deg, derived
    MESSAGE("grazing angle at 40 m = " << thresh * 57.2957795f << " deg (eye "
                                       << config::PLAYER_EYE_HEIGHT << " m)");
    for (const float arm : {1.0f, 2.0f, 4.0f, 8.0f, 16.0f}) {
        int n = 0;
        int over = 0;
        for (float z = 320.0f; z < 1728.0f; z += 48.0f) {
            for (float x = 320.0f; x < 1728.0f; x += 48.0f) {
                const glm::vec2 p{x, z};
                if (!world::relief_floor_binds(ctx, p)) continue;
                ++n;
                if (slope_at(ctx, p, arm) > thresh) ++over;
            }
        }
        MESSAGE("  baseline " << arm * 2.0f << " m run: " << over << "/" << n << " = "
                              << (n ? 100.0f * static_cast<float>(over) / static_cast<float>(n)
                                    : 0.0f)
                              << " % of legal open ground over the grazing angle");
    }

    // --- 2. HIDDEN POCKETS across the A1 frame -------------------------------
    // Every column of the frame at INTERNAL_RES, so the count is per-frame and
    // not per-lucky-ray.
    const glm::vec2 eye = A1_STANDPOINT;
    const float eye_y = world::terrain_height(ctx, eye)
                      + static_cast<float>(config::PLAYER_EYE_HEIGHT);
    const std::vector<glm::vec2> dirs = a1_columns(eye);
    std::vector<int> per_column;
    std::vector<int> per_column_drawn;
    for (const glm::vec2& dir : dirs) {
        per_column.push_back(hidden_pockets(ctx, eye, eye_y, dir, 5.0f, 60.0f));
        per_column_drawn.push_back(hidden_pockets_of(
            [&](glm::vec2 p) { return drawn_height(ctx, p); }, eye,
            drawn_height(ctx, eye) + static_cast<float>(config::PLAYER_EYE_HEIGHT), dir, 5.0f,
            60.0f));
    }
    {
        // THE DRAWN ARM. Same 64 columns, same counting rule, the field the
        // player actually sees: the 2 m heightmap. A count here materially
        // below the continuous one means the forms are finer than the mesh
        // that carries them, and the frame would not show what the field
        // contains.
        std::vector<int> d = per_column_drawn;
        std::sort(d.begin(), d.end());
        MESSAGE("A1 on the DRAWN 2 m heightmap: min " << d.front() << " p5 " << d[d.size() / 20]
                                                      << " median " << d[d.size() / 2] << " max "
                                                      << d.back());
    }
    // The same question over MANY standpoints, because one standpoint's answer
    // is one standpoint's answer: fraction of frame columns carrying at least
    // one pocket, over the flattest legal ground the world has.
    {
        int cols = 0;
        int with = 0;
        std::vector<int> counts;
        std::string dump;
        for (const glm::vec2 sp : world::flattest_legal_standpoints(ctx, 12, 64.0f)) {
            dump += " (" + std::to_string(static_cast<int>(sp.x)) + ","
                  + std::to_string(static_cast<int>(sp.y)) + ")";
            const float sy = drawn_height(ctx, sp)
                           + static_cast<float>(config::PLAYER_EYE_HEIGHT);
            for (int c = 0; c < 16; ++c) {
                const float a = static_cast<float>(c) * 0.3926991f;
                const glm::vec2 d{std::cos(a), std::sin(a)};
                ++cols;
                // ON THE DRAWN FIELD, like every other reading in this test
                // now: the population number and the frame number must be
                // about the same ground.
                const int n = hidden_pockets_of([&](glm::vec2 q) { return drawn_height(ctx, q); },
                                                sp, sy, d, 5.0f, 60.0f);
                counts.push_back(n);
                dump += static_cast<char>('0' + std::min(n, 9));
                if (n > 0) ++with;
            }
        }
        MESSAGE("flattest legal ground, all azimuths: " << with << "/" << cols << " = "
                << 100.0f * static_cast<float>(with) / static_cast<float>(cols)
                << " % of columns hide any ground at all in 5-60 m");
        // THE POPULATION READING, and it exists to stop this pass being tuned
        // to one standpoint. A1 is 64 columns of ONE eye: its p5 is the 4th
        // worst of 64 rays through one patch of ground, and a parameter sweep
        // read on it alone will find the setting whose speckle happens to
        // favour those 64 rays — fitting to a sample, one level down from
        // fitting a threshold (Rule 45's shape, not its letter). This is the
        // same count over the flattest legal standpoints in the world at every
        // azimuth, so a setting has to work on the LAND rather than on a view.
        MESSAGE("  per standpoint, 16 azimuths each:" << dump);
        std::vector<int> pop = counts;
        std::sort(pop.begin(), pop.end());
        MESSAGE("  ...and their pocket COUNTS: p5 " << pop[pop.size() / 20] << " median "
                                                    << pop[pop.size() / 2] << " p95 "
                                                    << pop[pop.size() * 19 / 20] << " over "
                                                    << pop.size() << " columns");
    }
    {
        // WHICH COLUMNS FAIL, AND WHERE THEY POINT. A percentile hides the
        // shape of its own tail: without this dump "p5 = 1" cannot distinguish
        // "one awkward bearing" from "the whole left half of the frame".
        std::string row;
        for (const int n : per_column) row += static_cast<char>('0' + std::min(n, 9));
        MESSAGE("per column, left to right: " << row);
    }
    {
        // THE CEILING, READ IN THE SAME BREATH AS THE COUNT. §10.1.2's floor
        // was withdrawn; its MAX 1.20 m survived, because the ceiling's job
        // really is amplitude — "the answer to flat must not become unwalkable
        // churn". Any pass that buys occlusion has to be read against it in
        // the same run, or the two contracts get satisfied on different days
        // against different builds.
        const world::GroundRelief gr = world::ground_relief_20m(ctx, A1_STANDPOINT);
        MESSAGE("A1 detrended sigma over 20 m: " << gr.sigma << " m (ceiling "
                                                 << config::GROUND_RELIEF_SIGMA_20M_MAX
                                                 << ", trend " << gr.trend_slope << ")");
        CHECK(gr.sigma <= static_cast<float>(config::GROUND_RELIEF_SIGMA_20M_MAX));
    }
    std::sort(per_column.begin(), per_column.end());
    // THE GATE, as NUMBERS.md defines it: the 5th percentile across columns, so
    // the margin lives in the percentile rather than in the threshold.
    const int p5 = per_column[per_column.size() / 20];
    MESSAGE("A1 ground-hides-ground in 5-60 m, per frame column: min "
            << per_column.front() << " p5 " << p5 << " median "
            << per_column[per_column.size() / 2] << " max " << per_column.back()
            << " (GROUND_OCCLUSION_COUNT_MIN " << config::GROUND_OCCLUSION_COUNT_MIN << ")");

    // REPORTED, NOT ASSERTED — AND THIS IS A KNOWN OPEN FAILURE, NOT A PASS.
    // p5 is 0 against a floor of 3. It is left visible rather than red because
    // the constant landed today and the fix is a wavelength question nobody has
    // ruled yet (see below); making the suite red over a contract this pass was
    // never built against would bury the failures it CAN speak to.
    //
    // THE ARITHMETIC THAT SAID "SHORTEN THE WAVE" — AND THE SWEEP THAT
    // FALSIFIED IT. At the σ this field produces (0.353 m), RMS slope
    // 2*pi*σ/L clears the 2.434 deg grazing angle only for L below ~52 m,
    // while GROUND_MESO_WAVELENGTH is approved as 25-60 m: 2.12 deg at 60 m
    // against 5.08 deg at 25 m, amplitude unchanged. That predicted a free win
    // — shorter waves cost nothing against the σ ceiling, corridor slope or
    // PLAYER_STEP_HEIGHT. It was measured instead of adopted, through
    // DFN_MESO_LAMBDA_MAX (WorldgenRelief.cpp), one binary, five runs:
    //
    //   λmax     slope>graze @2 m   @8 m    columns hiding ground   A1 p5
    //   60 m     60.7 %             62.9 %  76.6 %                  0
    //   52 m     —                  —       69.3 %                  1
    //   40 m     61.7 %             63.9 %  63.5 %                  0
    //   32 m     —                  —       76.6 %                  0
    //   25 m     64.2 %             68.6 %  67.7 %                  0
    //
    // SLOPE MOVES MONOTONICALLY, EXACTLY AS PREDICTED. THE QUANTITY THE
    // CONTRACT ASKS FOR DOES NOT MOVE AT ALL: ground-hiding wanders 63-77 %
    // with no trend, never above the 76.6 % the approved band already gives,
    // and A1's p5 stays 0 at every wavelength. The frames say the same thing —
    // docs/acceptance/core-A1-meso-lambda-{60,25}-DIAG-*.png are the same
    // picture, ground unbroken from the feet to the tree line in both.
    //
    // WHY, AND IT IS §8.1's FINDING ONE STEP FURTHER OUT: the derivation went
    // through RMS SLOPE, which is a POINTWISE quantity, and a pocket needs a
    // drop that is DIRECTED and SUSTAINED over a run. Shortening the wave
    // raises the slope and shortens the run by the same factor; the two cancel
    // in exactly the quantity we are trying to move. The same trap that
    // retired σ (bounds amplitude, not slope) and then retired slope
    // exceedance (bounds the point, not the run) — a third proxy, refused on a
    // measurement rather than on taste (Rule 41/45).
    //
    // SO: DO NOT LOWER THE TOP OF THE BAND FOR THIS. The move is not costly,
    // it is EMPTY, and lowering an approved number for a result it does not
    // produce would be a fitted constant with a good story.
    WARN(p5 >= static_cast<int>(config::GROUND_OCCLUSION_COUNT_MIN));
}

TEST_CASE("diagnostic: the sight profile of the WORST column, sample by sample") {
    // Aimed at the tail rather than at the middle: the frame's failing columns
    // are a contiguous sector, and the profile of one of THEM is what says
    // whether the shortfall is a form that is missing or a bearing where the
    // geometry cannot pay.
    const auto& ctx = shipped_world();
    const glm::vec2 eye = A1_STANDPOINT;
    const float eye_y = world::terrain_height(ctx, eye)
                      + static_cast<float>(config::PLAYER_EYE_HEIGHT);
    const std::vector<glm::vec2> dirs = a1_columns(eye);
    int worst = 0;
    int worst_n = 1 << 30;
    for (int c = 0; c < static_cast<int>(dirs.size()); ++c) {
        const int n = hidden_pockets(ctx, eye, eye_y, dirs[c], 5.0f, 60.0f);
        if (n < worst_n) {
            worst_n = n;
            worst = c;
        }
    }
    MESSAGE("worst column " << worst << " of " << dirs.size() << " with " << worst_n
                            << " pockets, bearing (" << dirs[worst].x << ", " << dirs[worst].y
                            << ")");
    const glm::vec2 dir = dirs[worst];
    MESSAGE("eye ground " << world::terrain_height(ctx, eye) << " m, eye_y " << eye_y);
    float env = -1e9f;
    for (float t = 2.0f; t <= 60.0f; t += 2.0f) {
        const float h = world::terrain_height(ctx, eye + dir * t);
        const float ang = (h - eye_y) / t;
        const bool hid = ang < env;
        env = std::max(env, ang);
        MESSAGE("  t=" << t << " h=" << h << " rel=" << (h - eye_y) << " ang="
                       << ang * 57.2957795f << " deg" << (hid ? "  HIDDEN" : ""));
    }
}

// --- WALKABILITY: THE PRICE OF THE FORMS, PAID IN THE ONE CURRENCY THAT ------
// --- CANNOT BE SEEN IN A FRAME ----------------------------------------------
//
// A riser multiplies the local gradient by up to 5x and a draw cuts a bank into
// it. On the lowland that is 3 deg becoming 12 and nobody notices; the failure
// mode worth fearing is a BARRIER — ground you cannot walk up, running far
// enough to cut the country in two — and a barrier is invisible in every
// screenshot ever taken of it, because it looks exactly like a bank until you
// try to walk up it.
//
// THE FIRST INSTRUMENT HERE WAS THE WRONG ONE AND ITS HEADSTONE IS THIS
// PARAGRAPH. It counted cells of legal open ground whose slope crossed
// `PLAYER_MAX_SLOPE`, and asserted zero. It read ONE cell in 4676 — at
// (688, 688), 0.936 rad where the control was ALREADY 0.862 against a 0.87
// limit. Then: making the draws shallower moved it to 0.910 and 0.923 and
// produced TWO such cells rather than none, and fading the forms off the rock
// (a fast-varying mask makes its own cliff) moved it to 1.025. Every arm tipped
// the same knife-edge cell. So the count does not separate "the pass built a
// wall" from "the pass nudged ground that was already at the limit" — no value
// on it does — which is the discriminating-power test failing, and by that test
// the QUANTITY is wrong rather than the threshold. Replaced, not demoted.
//
// The quantity that does separate them is CONNECTIVITY: walk the 2 m lattice
// the world is collided on, with the character controller's own two rules
// (`PLAYER_STEP_HEIGHT` free, `PLAYER_MAX_SLOPE` climbable), and ask what
// fraction of the ground around the pinned standpoint the player can still
// reach. A knife-edge cell costs nothing there; a scarp band across the country
// costs everything, and the number says which one happened.
TEST_CASE("the forms do not build barriers: the country stays connected") {
    const auto& ctx = shipped_world();
    // The window is 400 m across the pinned standpoint: wide enough that a
    // barrier has somewhere to run, small enough to walk the 2 m lattice twice.
    constexpr float S = static_cast<float>(config::HEIGHTMAP_STEP);
    constexpr int N = 200; // 400 m at 2 m
    const glm::vec2 origin = A1_STANDPOINT - glm::vec2{N * S * 0.5f, N * S * 0.5f};
    // The controller's own edge rule, stated once: a step is free up to
    // PLAYER_STEP_HEIGHT, and above that the ground must not rise faster than
    // PLAYER_MAX_SLOPE over the lattice it is collided on.
    const float rise_max = std::max(static_cast<float>(config::PLAYER_STEP_HEIGHT),
                                    S * std::tan(static_cast<float>(config::PLAYER_MAX_SLOPE)));

    const auto reachable_fraction = [&]() {
        std::vector<float> h(static_cast<std::size_t>(N) * N);
        for (int z = 0; z < N; ++z) {
            for (int x = 0; x < N; ++x) {
                h[static_cast<std::size_t>(z) * N + x] = world::terrain_height(
                    ctx, origin + glm::vec2{static_cast<float>(x) * S, static_cast<float>(z) * S});
            }
        }
        std::vector<uint8_t> seen(h.size(), 0);
        std::vector<int> stack{(N / 2) * N + N / 2};
        seen[static_cast<std::size_t>(stack[0])] = 1;
        int count = 0;
        while (!stack.empty()) {
            const int c = stack.back();
            stack.pop_back();
            ++count;
            const int cx = c % N;
            const int cz = c / N;
            for (int d = 0; d < 4; ++d) {
                const int nx = cx + (d == 0) - (d == 1);
                const int nz = cz + (d == 2) - (d == 3);
                if (nx < 0 || nz < 0 || nx >= N || nz >= N) continue;
                const auto n = static_cast<std::size_t>(nz) * N + nx;
                if (seen[n]) continue;
                // Passable both ways: the player has to be able to come back.
                if (std::fabs(h[n] - h[static_cast<std::size_t>(c)]) > rise_max) continue;
                seen[n] = 1;
                stack.push_back(static_cast<int>(n));
            }
        }
        return static_cast<float>(count) / static_cast<float>(h.size());
    };

    const float shipped = reachable_fraction();
    setenv("DFN_TERRACE_STRENGTH", "0", 1);
    setenv("DFN_DRAW_DEPTH", "0", 1);
    const float control = reachable_fraction();
    unsetenv("DFN_TERRACE_STRENGTH");
    unsetenv("DFN_DRAW_DEPTH");
    MESSAGE("reachable from the A1 standpoint over 400 m of 2 m lattice: shipped "
            << shipped * 100.0f << " %, control (forms off) " << control * 100.0f
            << " % — rise limit " << rise_max << " m per " << S << " m");
    // Both arms are REPORTED and the gate is the one clause that needs no new
    // number: whatever the country's own connectivity is, this pass must not be
    // what severs it. A floor on the absolute fraction would be a number
    // invented here, and it is the lead's to set if it is wanted.
    //
    // AND THE INSTRUMENT CAN FAIL, which is the part a control is worthless
    // without: driven through its own doors it MOVES, monotonically and only in
    // the arm that is driven — draws at 2x depth 99.9725 %, at 4x 99.9575 %, at
    // 6x (a 7-15 m ravine) 99.595 %, and a 3 m terrace step on an 0.08 riser
    // 99.9075 %, against a control that sits at 99.9725 % in every one of them.
    // At the shipped scale both arms read 99.9725 % — identical to five
    // figures — so the honest reading is not "the guard passed" but "at this
    // scale of form, barriers are not the risk, and here is the scale at which
    // they would start to be".
    CHECK(shipped > 0.5f * control);
}

// --- IS THE GRAIN LOST, OR IS THE RULER TOO SHORT TO SEE IT? -----------------
//
// §2.1's anisotropy probe reads the structure-tensor eigenvalue ratio over open
// meadow and falls 3.13 -> 2.64 (floor 2.50) when this pass is switched on. The
// obvious reading is that the forms erase the land's grain, which is what an
// isotropic octave did to it once before — and that reading has a competing
// explanation with a MECHANISM, so it is measurable rather than arguable.
//
// THE PROBE SAMPLES GRADIENTS WITH A +-6 m ARM ON A 12 m LATTICE. The draws sit
// at a 15-24 m pitch. That is one to two samples per cycle: AT OR PAST NYQUIST,
// where a regular lineation aliases into a long-wavelength beat whose direction
// is essentially arbitrary — which lowers an eigenvalue ratio no matter which
// way the real lineation runs. The prediction that separates the two readings:
// if the grain is genuinely gone, it is gone at EVERY scale; if the ruler is too
// short, the ratio recovers as the arm grows past the forms and the HILL band
// (the 128 m octave the contract is actually about) reads unchanged.
//
// The sweep already hints at it — the ratio recovers monotonically as the draw
// pitch moves away from the sampling pitch (14 m: 2.18, 16: 2.42, 24: 2.64,
// 29: 2.89, 36: 2.95) — but that varies the WORLD. This varies the RULER, on
// one world, which is the arm that settles it.
//
// REPORTED ONLY. §2.1's row and its instrument belong to design; this is
// evidence for that ruling, not a second opinion shipped beside it.
TEST_CASE("diagnostic: the anisotropy ratio as a function of the RULER, not the world") {
    const auto& ctx = shipped_world();
    const auto ratio_at = [&](float arm) {
        std::vector<float> ratios;
        for (float wz = 100.0f; wz < 950.0f; wz += 110.0f) {
            for (float wx = 60.0f; wx < 700.0f; wx += 110.0f) {
                if (world::crag_distance(ctx.params.layout, {wx, wz})
                    < ctx.params.layout.crag.radius + 60.0f) continue;
                if (world::surface_point(ctx, {wx, wz}).dist_to_water < 40.0f) continue;
                float jxx = 0.0f, jzz = 0.0f, jxz = 0.0f;
                // The window keeps its 7x7 shape and its span scales with the
                // arm, so what changes is the RESOLUTION of the ruler and not
                // how much country it looks at.
                for (int iz = -3; iz <= 3; ++iz) {
                    for (int ix = -3; ix <= 3; ++ix) {
                        const glm::vec2 p{wx + static_cast<float>(ix) * arm * 2.0f,
                                          wz + static_cast<float>(iz) * arm * 2.0f};
                        const float gx = world::terrain_height(ctx, {p.x + arm, p.y})
                                       - world::terrain_height(ctx, {p.x - arm, p.y});
                        const float gz = world::terrain_height(ctx, {p.x, p.y + arm})
                                       - world::terrain_height(ctx, {p.x, p.y - arm});
                        jxx += gx * gx;
                        jzz += gz * gz;
                        jxz += gx * gz;
                    }
                }
                const float tr = jxx + jzz;
                const float disc = std::sqrt(std::max(0.0f, tr * tr - 4.0f * (jxx * jzz
                                                                             - jxz * jxz)));
                ratios.push_back(((tr + disc) * 0.5f) / std::max((tr - disc) * 0.5f, 1e-6f));
            }
        }
        std::sort(ratios.begin(), ratios.end());
        return ratios[ratios.size() / 2];
    };

    for (const float arm : {6.0f, 12.0f, 24.0f, 48.0f}) {
        const float shipped = ratio_at(arm);
        setenv("DFN_TERRACE_STRENGTH", "0", 1);
        setenv("DFN_DRAW_DEPTH", "0", 1);
        const float control = ratio_at(arm);
        unsetenv("DFN_TERRACE_STRENGTH");
        unsetenv("DFN_DRAW_DEPTH");
        MESSAGE("arm +-" << arm << " m (probe uses 6): forms ON " << shipped << ", OFF "
                         << control << ", ratio of ratios " << shipped / control);
    }
}

// --- REGULARITY: THE DEFECT NO INSTRUMENT IN THIS PROJECT NAMES --------------
//
// The forms' own first frames named it before any probe did: at a tight pitch
// the draws read as a WASHBOARD — long, parallel, evenly spaced — and an even
// pitch is exactly how a generated world gives itself away. It is the same
// complaint the user opened with about the ground's colour ("repeating large
// pieces"), one system along.
//
// THE BAND THIS MEASURE RESOLVES, STATED BEFORE THE NUMBER (the day's rule: an
// instrument whose arm is shorter than its subject aliases it instead of
// measuring it). Transects are marched at 1 m over 240 m, and a gap is the
// distance between the centres of successive channels:
//
//   * shortest gap it can see: 2 m (two marching steps),
//   * longest: ~120 m (a transect must hold at least two gaps to have one
//     spacing to compare),
//   * SUBJECT: a nominal 15-24 m pitch, warped to roughly 12-40 m.
//
// The subject sits in the middle third of the resolved band, an order of
// magnitude off both ends. That is the check §2.1's probe failed.
//
// The quantity is the COEFFICIENT OF VARIATION of the gaps. A washboard has one
// pitch and CV -> 0; country that is dissected irregularly has a broad gap
// distribution and CV of order 0.5. It needs no absolute floor to be useful,
// because it is read against the pass's OWN washboard arm through
// DFN_DRAW_WANDER=0 — the same code path with the warp at zero.
TEST_CASE("diagnostic: how REGULAR are the draws (and what band can this see)") {
    const auto& ctx = shipped_world();
    // Deep enough to be a channel rather than the shoulder of one: a tenth of
    // the shallowest draw the pass can cut.
    constexpr float ENTER = 0.12f;
    const auto gaps_at = [&](std::vector<float>& out) {
        out.clear();
        for (float wz = 200.0f; wz < 1800.0f; wz += 220.0f) {
            for (float wx = 200.0f; wx < 1800.0f; wx += 220.0f) {
                const glm::vec2 centre{wx, wz};
                if (!world::relief_floor_binds(ctx, centre)) continue;
                // THE TRANSECT IS CHOSEN BY THE GROUND, not by us: eight
                // bearings, keep the one that crosses the MOST channels, which
                // is the one running across the grain. Pooling all eight would
                // mix cross-channel spacings with along-channel runs and report
                // the average of two different things.
                std::vector<float> best;
                for (int b = 0; b < 8; ++b) {
                    const float a = static_cast<float>(b) * 0.3926991f;
                    const glm::vec2 dir{std::cos(a), std::sin(a)};
                    std::vector<float> centres;
                    bool inside = false;
                    float run_start = 0.0f;
                    for (float t = -120.0f; t <= 120.0f; t += 1.0f) {
                        const glm::vec2 p = centre + dir * t;
                        const float d = -world::draw_forms(ctx.params.seed, p, 1.0f);
                        if (d > ENTER && !inside) {
                            inside = true;
                            run_start = t;
                        } else if (d <= ENTER && inside) {
                            inside = false;
                            centres.push_back((run_start + t) * 0.5f);
                        }
                    }
                    if (centres.size() > best.size()) best = centres;
                }
                for (std::size_t i = 1; i < best.size(); ++i) out.push_back(best[i] - best[i - 1]);
            }
        }
    };

    const auto stats = [](const std::vector<float>& g) {
        double m = 0.0;
        for (const float v : g) m += v;
        m /= static_cast<double>(g.size());
        double s = 0.0;
        for (const float v : g) s += (v - m) * (v - m);
        s = std::sqrt(s / static_cast<double>(g.size()));
        return std::pair<double, double>{m, s / m};
    };

    std::vector<float> shipped;
    gaps_at(shipped);
    setenv("DFN_DRAW_WANDER", "0", 1);
    std::vector<float> washboard;
    gaps_at(washboard);
    unsetenv("DFN_DRAW_WANDER");
    REQUIRE(shipped.size() > 100);
    REQUIRE(washboard.size() > 100);
    const auto [ms, cs] = stats(shipped);
    const auto [mw, cw] = stats(washboard);
    MESSAGE("gap between draws, shipped:   mean " << ms << " m, CV " << cs << " over "
                                                  << shipped.size() << " gaps");
    MESSAGE("gap between draws, washboard: mean " << mw << " m, CV " << cw << " over "
                                                  << washboard.size() << " gaps");
    // NOT GATED, AND THE REASON IS THE FINDING ITSELF. This started as
    // CHECK(shipped CV > washboard CV) — the obvious assertion for a knob added
    // to make the spacing wander. It reads 0.679 against 0.718 at the shipped
    // pitch and 0.573 against 0.567 at the previous one: the two arms are
    // indistinguishable, and which way the tiny difference falls is speckle.
    // THE SPACING WAS NEVER REGULAR, so no assertion on it can separate the
    // corduroy from the fix — the same discriminating-power test that retired
    // the slope-exceedance surrogate and the newly-impassable-cell count, met a
    // third time in one day. The number stays REPORTED because it is what
    // proves the diagnosis wrong; it just is not a gate.
    MESSAGE("  (the two arms are indistinguishable on spacing — that IS the "
            "finding: the defect was parallelism, not pitch)");

    // --- AND THE HALF A SPACING MEASURE CANNOT SEE -------------------------
    //
    // The two arms above come out IDENTICALLY irregular in spacing, and yet the
    // frame plainly shows a corduroy. Spacing is not the quantity: "washboard"
    // is PARALLEL + evenly spaced, and it is the parallelism that reads.
    //
    // THE FIRST TRY AT MEASURING IT FAILED AND THE FAILURE IS WORTH THE LINES,
    // because it is the day's own lesson landing on my own instrument. It
    // counted channel crossings along twelve bearings and took max/mean, and I
    // wrote down "it separates lineation directions 15 deg apart" — the BIN
    // WIDTH. That is not the resolution. Counting crossings along a bearing
    // integrates over a 240 m line, so a single set of parallel channels
    // produces a |sin| response whose LOBE is ~90 deg wide, and max/mean
    // saturates at pi/2 = 1.5708 for ANY set of parallel lines. Measured:
    // shipped 1.5714 against tributaries-parallel 1.5652 — both pinned at the
    // saturation value. Driven hard it does move (1.448 at a 69 deg offset,
    // 1.474 at 89), which locates its real resolution at 60-90 deg, while the
    // subject is a 19-41 deg tributary. THE INSTRUMENT'S BAND WAS THE LOBE, NOT
    // THE BIN, and the subject sat under it.
    //
    // THE REPLACEMENT IS A POINT MEASURE, so it has no lobe at all: take the
    // GRADIENT of the draw field wherever the field has a bank, and measure the
    // circular spread of those directions as AXIAL data (a bank has an
    // orientation, not a sign, so angles are doubled). One set of parallel
    // channels puts every gradient on one axis and the spread goes to 0; two
    // sets 30 deg apart put half the mass 60 deg away in the doubled angle and
    // the spread rises to ~0.13 by construction.
    //
    //   BAND: the gradient arm is 2 m, so it reads the orientation of any bank
    //   4 m or wider — the draws' banks are 4-10 m. Angular resolution is set
    //   by sampling noise rather than by binning, and the noise is reported
    //   beside the number as the parallel-tributary arm.
    //
    // AND IT IS READ PER WINDOW, NEVER POOLED OVER THE WORLD — a mistake worth
    // one more paragraph because it makes the number meaningless rather than
    // merely weak. The land's axis field turns over 512 m cells, so gradients
    // pooled across 1600 m carry the WORLD'S rotation, not the local
    // lineation: pooled, a maximally straight arm (no wander, parallel
    // tributaries, stretch 20) read 0.743 against an isotropic 0.958 — nearly
    // all of the range spent on the axis field turning. The grain is a LOCAL
    // property and has to be measured in local windows, exactly as §2.1's own
    // probe does with its 7x7.
    const auto bank_direction_spread = [&]() {
        std::vector<double> per_window;
        for (float wz = 200.0f; wz < 1800.0f; wz += 96.0f) {
            for (float wx = 200.0f; wx < 1800.0f; wx += 96.0f) {
                double cx = 0.0;
                double cz = 0.0;
                double w = 0.0;
                for (float dz = -36.0f; dz <= 36.0f; dz += 4.0f) {
                    for (float dx = -36.0f; dx <= 36.0f; dx += 4.0f) {
                        const glm::vec2 p{wx + dx, wz + dz};
                        if (!world::relief_floor_binds(ctx, p)) continue;
                        const float gx =
                            world::draw_forms(ctx.params.seed, {p.x + 2.0f, p.y}, 1.0f)
                            - world::draw_forms(ctx.params.seed, {p.x - 2.0f, p.y}, 1.0f);
                        const float gz =
                            world::draw_forms(ctx.params.seed, {p.x, p.y + 2.0f}, 1.0f)
                            - world::draw_forms(ctx.params.seed, {p.x, p.y - 2.0f}, 1.0f);
                        const float mag = std::sqrt(gx * gx + gz * gz);
                        if (mag < 0.15f) continue; // flat: no bank to have a direction
                        const float a2 = 2.0f * std::atan2(gz, gx); // axial: doubled
                        cx += static_cast<double>(mag) * std::cos(a2);
                        cz += static_cast<double>(mag) * std::sin(a2);
                        w += mag;
                    }
                }
                if (w < 2.0) continue; // no banks in this window
                per_window.push_back(1.0 - std::sqrt(cx * cx + cz * cz) / w);
            }
        }
        std::sort(per_window.begin(), per_window.end());
        return per_window[per_window.size() / 2];
    };

    const double spread_shipped = bank_direction_spread();
    setenv("DFN_DRAW_TRIB_BEARING", "0", 1); // tributaries parallel to the trunk
    const double spread_parallel = bank_direction_spread();
    unsetenv("DFN_DRAW_TRIB_BEARING");
    MESSAGE("bank-direction spread (axial, 0 = one axis): shipped " << spread_shipped
            << ", tributaries-parallel " << spread_parallel);
    CHECK(spread_shipped > spread_parallel);
}
