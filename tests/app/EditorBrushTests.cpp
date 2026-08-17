/*
Created: 17:08:2026 - 19:05:00
Last updated: 17:08:2026 - 20:09:15
Module: tests/app
File: tests/app/EditorBrushTests.cpp

Responsibility:
- Holds the terrain brush and the planting hand to the properties a screenshot
  cannot show: that the ground moved BY A NUMBER, that an empty edit layer
  changes the world by NOTHING AT ALL, and that a plant's green comes from the
  scene judge rather than from a second copy of its rules.

Dependencies:
- Uses: engine/editor (EditorBrush: the mechanics), engine/app (EditorPlant:
  the judge-facing half), engine/world (ReliefLayer, Scene, Worldgen), doctest.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE ARMS DIFFER BY ONE THING (Rule 30). Every claim here is paired with its
  counterfactual: the no-op proof is worthless without a case that DOES move
  the ground, because a test that only ever sees zero cannot tell "nothing
  changed" from "nothing is wired up".
*/
/*
UPD:
- 17:08:2026 - 19:05:00: Создан — кисть рельефа и посадка растительности (заказ 17.08).
- 17:08:2026 - 20:09:15: Мазок НЕ СРАБАТЫВАЕТ, пока мышь на панели (требование лида руками):
  решение принимается ОДИН раз, на нажатии, поэтому протяжка ползунка размера,
  ушедшая за край панели с зажатой кнопкой, не копает — и обратная рука, без
  которой первая ничего не значит: мазок, начатый по земле, рисует и не
  прерывается, когда указатель наезжает на панель.
*/

#include <doctest/doctest.h>

#include "engine/app/sources/EditorPlant.h"
#include "engine/editor/sources/EditorBrush.h"
#include "engine/world/sources/ReliefLayer.h"
#include "engine/world/sources/Worldgen.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace dfn;
using namespace dfn::app;

namespace {

/// A FLAT WORLD AT A KNOWN HEIGHT. The brush is being measured, not the
/// generator: on a plane every millimetre the ground moves belongs to the
/// stroke and to nothing else.
constexpr float FLAT_GROUND_M = 20.0f;

float flat_ground(void* /*ctx*/, glm::vec2 /*p*/) { return FLAT_GROUND_M; }

/// A SLOPE, for the one question a plane cannot answer: whether smoothing
/// actually reads the ground under it.
float ramp_ground(void* /*ctx*/, glm::vec2 p) { return FLAT_GROUND_M + 0.25f * p.x; }

BrushGround flat_world() { return BrushGround{&flat_ground, nullptr}; }

// --- The judge's world, for the planting half -------------------------------
//
// One object, one size, a flat ground. Everything the rules need and nothing
// they do not, so a finding in these tests can only be about the rule under
// test (Rule 30).

float judge_ground(void* /*ctx*/, glm::vec2 /*p*/) { return FLAT_GROUND_M; }

bool judge_extent(void* /*ctx*/, const std::string& name, float& radius, float& bottom) {
    if (name != "oak" && name != "fern") {
        return false; // unknown to the registry: the judge says KnownObject
    }
    radius = (name == "oak") ? 2.0f : 0.3f;
    bottom = 0.0f;
    return true;
}

/// Where the trodden ground runs in these tests. IN THE MIDDLE OF THE MAP, not
/// along z = 0: the map's own edge margin also refuses placements, and a path
/// laid on the boundary would let InsideBounds answer for OffPath — two rules
/// firing on one candidate, so the test would prove neither (Rule 30). Caught
/// exactly that way: the first version read `build.no.bounds`.
constexpr float PATH_CENTRE_Z = 128.0f;

/// The path clearance hook: a corridor of trodden ground along PATH_CENTRE_Z,
/// so a candidate's z says how far off the path it stands. This is what makes
/// the OffPath rule fire in a test with no path network in it.
bool judge_path_clearance(void* /*ctx*/, glm::vec2 p, float& metres) {
    metres = std::fabs(p.y - PATH_CENTRE_Z) - 2.0f; // worn half-width 2 m, outward
    return true;
}

world::SceneWorld judge_world(bool with_path) {
    world::SceneWorld w;
    w.ground_at = &judge_ground;
    w.object_extent = &judge_extent;
    w.path_clearance = with_path ? &judge_path_clearance : nullptr;
    return w;
}

bool radius_of(void* /*ctx*/, const std::string& name, float& radius) {
    float bottom = 0.0f;
    return judge_extent(nullptr, name, radius, bottom);
}

} // namespace

// ============================== THE FALLOFF =================================

TEST_CASE("the falloff is 1 at the centre, 0 at the rim, and never rises") {
    constexpr float R = 8.0f;
    CHECK(brush_weight(0.0f, R, 0.5f) == doctest::Approx(1.0f));
    // EXACTLY zero at and past the rim, not merely small. A brush that leaked a
    // millionth of a metre past its own edge would tilt the whole world by a
    // little on every stroke, and no single stroke would look wrong.
    CHECK(brush_weight(R, R, 0.5f) == 0.0f);
    CHECK(brush_weight(R + 1.0f, R, 0.5f) == 0.0f);
    CHECK(brush_weight(1000.0f, R, 0.5f) == 0.0f);

    float previous = 2.0f;
    for (int i = 0; i <= 100; ++i) {
        const float d = R * static_cast<float>(i) / 100.0f;
        const float w = brush_weight(d, R, 0.5f);
        CHECK(w <= previous + 1.0e-6f); // monotone non-increasing
        CHECK(w >= 0.0f);
        CHECK(w <= 1.0f);
        previous = w;
    }
}

TEST_CASE("a harder brush is never softer than a soft one") {
    // ONE THING CHANGES: the hardness. If this failed, the slider would be
    // pushing the fade the wrong way and every stroke would still look
    // plausible — which is exactly the kind of defect a frame cannot catch.
    constexpr float R = 10.0f;
    for (int i = 0; i <= 40; ++i) {
        const float d = R * static_cast<float>(i) / 40.0f;
        CHECK(brush_weight(d, R, 0.9f) >= brush_weight(d, R, 0.1f) - 1.0e-6f);
    }
    // And the hard one is FLAT where the soft one has already faded.
    CHECK(brush_weight(0.5f * R, R, 0.9f) == doctest::Approx(1.0f));
    CHECK(brush_weight(0.5f * R, R, 0.1f) < 0.9f);
}

TEST_CASE("a brush narrower than the world's lattice is widened, not obeyed") {
    // The world holds shapes on a RELIEF_STEP_M lattice. A radius under that
    // would move one sample or none, so the brush is widened to the smallest
    // thing the terrain can actually show — and the tool says so rather than
    // painting edits nobody can see.
    CHECK(BRUSH_MIN_RADIUS_M == doctest::Approx(world::RELIEF_STEP_M));
    CHECK(brush_weight(0.0f, 0.01f, 0.0f) == doctest::Approx(1.0f));
    CHECK(brush_weight(world::RELIEF_STEP_M, 0.01f, 0.0f) == 0.0f);
}

// ============================ WHAT A DAB DOES ===============================

TEST_CASE("raising moves the ground UP, by a number, and only inside the brush") {
    world::ReliefLayer layer;
    TerrainBrush brush;
    brush.mode = BrushMode::Raise;
    brush.radius_m = 6.0f;
    brush.strength_m_s = 2.0f;
    brush.hardness = 0.0f;

    const BrushDabReport r = apply_brush(layer, brush, {100.0f, 100.0f}, 0.5f, flat_world());

    // NUMBERS, NOT ADJECTIVES. "The ground rose" is a screenshot's claim.
    CHECK(r.samples_touched > 0);
    CHECK(r.any);
    // Centre: strength * dt * weight(0) = 2.0 * 0.5 * 1.0.
    CHECK(layer.height_delta_at({100.0f, 100.0f}) == doctest::Approx(1.0f));
    CHECK(r.max_abs_delta_m == doctest::Approx(1.0f));

    // OUTSIDE THE BRUSH THE WORLD IS UNTOUCHED — the counterfactual that makes
    // the number above mean something. Without it, a brush that raised the
    // entire map would pass the check on the centre.
    CHECK(layer.height_delta_at({100.0f + 40.0f, 100.0f}) == 0.0f);
    CHECK(layer.height_delta_at({-500.0f, -500.0f}) == 0.0f);

    // And the reported box actually contains what moved.
    CHECK(r.min_xz.x <= 100.0f - brush.radius_m);
    CHECK(r.max_xz.x >= 100.0f + brush.radius_m);
}

TEST_CASE("lowering is raising with the sign flipped, and nothing else") {
    // THE TWO ARMS DIFFER BY ONE THING: the mode. Anything else that differed
    // would make the comparison prove nothing.
    world::ReliefLayer up;
    world::ReliefLayer down;
    TerrainBrush brush;
    brush.radius_m = 5.0f;
    brush.strength_m_s = 1.5f;

    brush.mode = BrushMode::Raise;
    const BrushDabReport a = apply_brush(up, brush, {0.0f, 0.0f}, 0.25f, flat_world());
    brush.mode = BrushMode::Lower;
    const BrushDabReport b = apply_brush(down, brush, {0.0f, 0.0f}, 0.25f, flat_world());

    CHECK(a.samples_touched == b.samples_touched);
    CHECK(a.max_abs_delta_m == doctest::Approx(b.max_abs_delta_m));
    CHECK(up.height_delta_at({1.0f, 1.0f})
          == doctest::Approx(-down.height_delta_at({1.0f, 1.0f})));
    CHECK(up.height_delta_at({0.0f, 0.0f}) > 0.0f);
    CHECK(down.height_delta_at({0.0f, 0.0f}) < 0.0f);
}

TEST_CASE("holding the brush accumulates; the same total time gives the same ground") {
    // A brush measured PER DAB would dig twice as fast at twice the frame rate,
    // and the builder would blame his own hand. Two dabs of 0.25 s must equal
    // one of 0.5 s.
    world::ReliefLayer twice;
    world::ReliefLayer once;
    TerrainBrush brush;
    brush.mode = BrushMode::Raise;
    brush.radius_m = 4.0f;
    brush.strength_m_s = 3.0f;

    (void)apply_brush(twice, brush, {10.0f, 10.0f}, 0.25f, flat_world());
    (void)apply_brush(twice, brush, {10.0f, 10.0f}, 0.25f, flat_world());
    (void)apply_brush(once, brush, {10.0f, 10.0f}, 0.5f, flat_world());

    CHECK(twice.height_delta_at({10.0f, 10.0f})
          == doctest::Approx(once.height_delta_at({10.0f, 10.0f})));
    CHECK(twice.height_delta_at({12.0f, 10.0f})
          == doctest::Approx(once.height_delta_at({12.0f, 10.0f})));
}

TEST_CASE("smoothing flattens a ramp instead of ignoring it") {
    // THE DEFECT THIS GUARDS AGAINST: smoothing the DELTAS alone. On untouched
    // ground every delta is zero, their average is zero, and a brush that
    // averaged them would report "smoothed" while the generator's own slope sat
    // there unchanged — the ground would refuse to flatten and the tool would
    // look dead. So it must read the FINISHED ground.
    world::ReliefLayer layer;
    TerrainBrush brush;
    brush.mode = BrushMode::Smooth;
    brush.radius_m = 8.0f;
    brush.strength_m_s = 4.0f;
    const BrushGround ramp{&ramp_ground, nullptr};

    // A LINEAR ramp IS its own neighbour-average, so smoothing it moves nothing
    // — and that is the correct answer, not a dead brush. Asserting otherwise
    // was this test's own first mistake, and the assertion is kept inverted so
    // nobody "fixes" the brush into eroding straight slopes.
    const BrushDabReport flat_case = apply_brush(layer, brush, {0.0f, 0.0f}, 0.5f, ramp);
    CHECK(flat_case.samples_touched == 0);
    CHECK(layer.empty());

    // What proves the pass reads the ground is a CORNER: put a spike in the
    // layer and smooth it.
    world::ReliefLayer spike;
    spike.set_delta(0, 0, 4.0f);
    const float before = spike.delta_at(0, 0);
    TerrainBrush s = brush;
    (void)apply_brush(spike, s, {0.0f, 0.0f}, 0.5f, flat_world());
    const float after = spike.delta_at(0, 0);
    CHECK(after < before);          // the spike came down
    CHECK(after > 0.0f);            // ...towards its neighbours, not past them
    // AND ITS NEIGHBOURS CAME UP: smoothing moves material, it does not delete
    // it. A pass that only ever lowered would erode the world every time the
    // builder tidied an edge.
    CHECK(spike.delta_at(1, 0) > 0.0f);
}

TEST_CASE("painting sets a class and touches no heights") {
    world::ReliefLayer layer;
    TerrainBrush brush;
    brush.mode = BrushMode::Paint;
    brush.radius_m = 5.0f;
    brush.paint = math::SurfaceClass::Sand;

    const BrushDabReport r = apply_brush(layer, brush, {50.0f, 50.0f}, 0.016f, flat_world());
    CHECK(r.samples_touched > 0);
    // THE HEIGHT IS THE CONTROL. A paint brush that moved the ground would be
    // discovered by a builder wondering why his flat square sagged.
    CHECK(r.max_abs_delta_m == 0.0f);
    CHECK(layer.height_delta_at({50.0f, 50.0f}) == 0.0f);

    math::SurfaceClass got{};
    CHECK(layer.surface_at({50.0f, 50.0f}, got));
    CHECK(got == math::SurfaceClass::Sand);
    // Outside the brush nobody painted anything.
    CHECK_FALSE(layer.surface_at({50.0f + 60.0f, 50.0f}, got));
}

TEST_CASE("flatten writes no samples — it authors a pad") {
    // A pad is a STATEMENT the composition can move, re-read and judge. A
    // second way of saying "here the ground is this high" would be the drift
    // Rule 32 forbids, so this mode deliberately produces nothing in the layer.
    world::ReliefLayer layer;
    TerrainBrush brush;
    brush.mode = BrushMode::Flatten;
    brush.radius_m = 12.0f;
    brush.hardness = 0.25f;
    brush.flatten_height_m = 31.0f;

    const BrushDabReport r = apply_brush(layer, brush, {7.0f, 9.0f}, 0.5f, flat_world());
    CHECK(r.samples_touched == 0);
    CHECK(layer.empty());

    const world::ScenePad pad = flatten_pad(brush, {7.0f, 9.0f});
    CHECK(pad.center.x == doctest::Approx(7.0f));
    CHECK(pad.center.y == doctest::Approx(9.0f));
    CHECK(pad.radius == doctest::Approx(12.0f));
    CHECK(pad.height == doctest::Approx(31.0f));
    // The blend is DERIVED from the brush's own soft band, so the flattened
    // patch matches the one the builder saw under his cursor while aiming.
    CHECK(pad.blend == doctest::Approx(12.0f * 0.75f));
}

// ============================== THE LAYER ===================================

TEST_CASE("an undone edit leaves no trace in the layer") {
    // A composer who raised a hill and lowered it back must get his original
    // file back. A cell holding a zero delta is a sample nobody edited, and
    // keeping it would preserve every abandoned experiment as a line in a diff
    // forever.
    world::ReliefLayer layer;
    layer.set_delta(3, 4, 1.5f);
    CHECK(layer.size() == 1);
    layer.set_delta(3, 4, 0.0f);
    CHECK(layer.empty());

    // But a sample that still carries PAINT survives having its height undone:
    // the two channels are independent, and losing the paint would be a second
    // edit nobody asked for.
    layer.set_delta(3, 4, 1.5f);
    layer.set_surface(3, 4, math::SurfaceClass::Rock);
    layer.set_delta(3, 4, 0.0f);
    CHECK(layer.size() == 1);
    math::SurfaceClass got{};
    CHECK(layer.surface_at({world::relief_world_of(3), world::relief_world_of(4)}, got));
    CHECK(got == math::SurfaceClass::Rock);
}

TEST_CASE("the sidecar round-trips, and its order is the writer's doing") {
    world::ReliefLayer layer;
    // Written in a deliberately scrambled order: if the file came out of the
    // hash map as-is, a diff would show a rehash rather than the samples that
    // moved, and the record would be worthless as a record.
    layer.set_delta(5, 1, 2.25f);
    layer.set_delta(-2, -7, -0.5f);
    layer.set_delta(0, 1, 1.0f);
    layer.set_surface(0, 1, math::SurfaceClass::Rock);
    layer.set_surface(9, 9, math::SurfaceClass::Sand);

    const auto path = std::filesystem::temp_directory_path() / "dfn_brush_test.relief";
    REQUIRE(world::write_relief(layer, path));

    world::ReliefLayer back;
    std::string error;
    REQUIRE_MESSAGE(world::read_relief(path, back, error), error);
    CHECK(back.size() == layer.size());
    CHECK(back.delta_at(5, 1) == doctest::Approx(2.25f));
    CHECK(back.delta_at(-2, -7) == doctest::Approx(-0.5f));
    math::SurfaceClass got{};
    CHECK(back.surface_at({world::relief_world_of(9), world::relief_world_of(9)}, got));
    CHECK(got == math::SurfaceClass::Sand);

    // SORTED BY z THEN x, in the file itself.
    std::FILE* f = std::fopen(path.string().c_str(), "rb");
    REQUIRE(f != nullptr);
    std::string text;
    char buf[512];
    while (std::size_t n = std::fread(buf, 1, sizeof(buf), f)) {
        text.append(buf, n);
    }
    std::fclose(f);
    const std::size_t first = text.find("dh -2 -7");
    const std::size_t second = text.find("dh 0 1");
    const std::size_t third = text.find("dh 5 1");
    CHECK(first != std::string::npos);
    CHECK(second != std::string::npos);
    CHECK(third != std::string::npos);
    CHECK(first < second);
    CHECK(second < third);

    // AN EMPTY LAYER REMOVES THE FILE: an edit fully undone leaves the tree as
    // it was found.
    world::ReliefLayer nothing;
    REQUIRE(world::write_relief(nothing, path));
    CHECK_FALSE(std::filesystem::exists(path));
}

TEST_CASE("a named sidecar that is missing is a REFUSAL, not an empty layer") {
    // Silently reporting "no edits" would make a lost terrain edit look like a
    // map that moved by itself, and the composer would go hunting for the
    // defect inside the generator.
    world::ReliefLayer layer;
    std::string error;
    const auto missing =
        std::filesystem::temp_directory_path() / "dfn_no_such_file_at_all.relief";
    std::error_code ec;
    std::filesystem::remove(missing, ec);
    CHECK_FALSE(world::read_relief(missing, layer, error));
    CHECK_FALSE(error.empty());
}

// ================== THE EMPTY LAYER CHANGES NOTHING AT ALL ==================

TEST_CASE("an empty layer is a no-op, and a filled one is not") {
    // LEAD'S CONDITION 1, with both arms. The no-op claim on its own is
    // worthless: a test that only ever sees zero cannot tell "nothing changed"
    // from "nothing is wired up". So the same generator is asked the same
    // question twice, and the ONLY difference between the arms is whether the
    // layer has a sample in it.
    world::WorldGenParams params;
    params.min_chunk = {0, 0};
    params.max_chunk = {0, 0};
    const world::WorldGenContext plain = world::build_world_context(params);

    constexpr glm::vec2 PROBE{128.0f, 128.0f};
    constexpr glm::vec2 FAR{20.0f, 20.0f};
    const float h_plain = world::terrain_height(plain, PROBE);
    const float h_plain_far = world::terrain_height(plain, FAR);

    // ARM A: an empty layer, explicitly present. Bit for bit, not approximately.
    world::WorldGenParams with_empty = params;
    with_empty.composed_relief = world::ReliefLayer{};
    const world::WorldGenContext empty_ctx = world::build_world_context(with_empty);
    CHECK(world::terrain_height(empty_ctx, PROBE) == h_plain);
    CHECK(world::terrain_height(empty_ctx, FAR) == h_plain_far);

    // ARM B: ONE sample of edit, at the probe. Everything else is identical.
    world::WorldGenParams with_edit = params;
    const int32_t ix = world::relief_index_floor(PROBE.x);
    const int32_t iz = world::relief_index_floor(PROBE.y);
    with_edit.composed_relief.set_delta(ix, iz, 3.0f);
    const world::WorldGenContext edit_ctx = world::build_world_context(with_edit);
    const float h_edit = world::terrain_height(edit_ctx, PROBE);
    CHECK(h_edit != h_plain);
    CHECK(h_edit == doctest::Approx(h_plain + 3.0f));
    // ...and only there: a sample thirty metres away is untouched, bit for bit.
    CHECK(world::terrain_height(edit_ctx, FAR) == h_plain_far);
}

TEST_CASE("a painted class wins over the derived one, and only where painted") {
    world::WorldGenParams params;
    params.min_chunk = {0, 0};
    params.max_chunk = {0, 0};

    constexpr glm::vec2 PROBE{128.0f, 128.0f};
    const world::WorldGenContext plain = world::build_world_context(params);
    const math::SurfaceClass derived = world::surface_point(plain, PROBE).surface_class;

    // ONE THING CHANGES: a class painted at the probe. Sand is chosen because
    // it is what a shore rule produces, so if the arms came out equal by luck
    // the next line would still catch it.
    const auto painted = derived == math::SurfaceClass::Rock ? math::SurfaceClass::Sand
                                                             : math::SurfaceClass::Rock;
    world::WorldGenParams with_paint = params;
    with_paint.composed_relief.set_surface(
        static_cast<int32_t>(std::lround(PROBE.x / world::RELIEF_STEP_M)),
        static_cast<int32_t>(std::lround(PROBE.y / world::RELIEF_STEP_M)), painted);
    const world::WorldGenContext paint_ctx = world::build_world_context(with_paint);

    CHECK(world::surface_point(paint_ctx, PROBE).surface_class == painted);
    CHECK(world::surface_point(paint_ctx, PROBE).surface_class != derived);
    // Thirty metres away nobody painted, so the derived answer stands.
    CHECK(world::surface_point(paint_ctx, {20.0f, 20.0f}).surface_class
          == world::surface_point(plain, {20.0f, 20.0f}).surface_class);
}

// ============================ THE VEGETATION ================================

TEST_CASE("a dab is reproducible, and its instances keep their distance") {
    PlantBrush brush;
    brush.species = {"oak", "fern"};
    brush.radius_m = 10.0f;
    brush.count = 8;
    brush.min_spacing_m = 2.0f;

    const auto a = plant_candidates(brush, {0.0f, 0.0f}, 12345, flat_world());
    const auto b = plant_candidates(brush, {0.0f, 0.0f}, 12345, flat_world());
    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].object == b[i].object);
        CHECK(a[i].position.x == doctest::Approx(b[i].position.x));
        CHECK(a[i].yaw == doctest::Approx(b[i].yaw));
        CHECK(a[i].scale == doctest::Approx(b[i].scale));
    }
    // ONE THING CHANGES: the seed. Without this arm "reproducible" could mean
    // "always the same eight trees", which is not a dab, it is a stamp.
    const auto c = plant_candidates(brush, {0.0f, 0.0f}, 999, flat_world());
    bool any_different = false;
    for (std::size_t i = 0; i < std::min(a.size(), c.size()); ++i) {
        if (a[i].position.x != c[i].position.x) {
            any_different = true;
        }
    }
    CHECK(any_different);

    for (std::size_t i = 0; i < a.size(); ++i) {
        // Inside the brush...
        const float dx = a[i].position.x;
        const float dz = a[i].position.z;
        CHECK(std::sqrt(dx * dx + dz * dz) <= brush.radius_m + 1.0e-4f);
        // ...and clear of its neighbours.
        for (std::size_t j = i + 1; j < a.size(); ++j) {
            const float ex = a[i].position.x - a[j].position.x;
            const float ez = a[i].position.z - a[j].position.z;
            CHECK(std::sqrt(ex * ex + ez * ez) >= brush.min_spacing_m - 1.0e-4f);
        }
    }
}

TEST_CASE("NOTHING IS PLANTED ON A PATH, and the judge is what says so") {
    // The rule under test lives in world::check_scene (OffPath), not here. What
    // this holds is that the brush ASKS it — a planting hand with its own copy
    // of the rules is a hand that will one day allow what the judge forbids.
    world::SceneDoc doc;
    doc.world_span_m = 256.0f;

    std::vector<PlantCandidate> on_path(1);
    on_path[0].object = "oak";
    on_path[0].position = {50.0f, FLAT_GROUND_M, PATH_CENTRE_Z}; // dead centre of the tread

    const PlantDabReport refused = plant_dab(doc, on_path, judge_world(true));
    CHECK(refused.planted == 0);
    CHECK(refused.refused == 1);
    CHECK_FALSE(refused.verdicts[0].allowed);
    CHECK(refused.verdicts[0].reason == "build.no.path");
    CHECK(doc.placements.empty()); // a refusal changes nothing

    // ONE THING CHANGES: the same tree, off the tread. Without this arm a hand
    // that refused EVERYTHING would pass the test above.
    std::vector<PlantCandidate> off_path = on_path;
    off_path[0].position.z = PATH_CENTRE_Z + 40.0f;
    const PlantDabReport allowed = plant_dab(doc, off_path, judge_world(true));
    CHECK(allowed.planted == 1);
    CHECK(allowed.verdicts[0].allowed);
    CHECK(doc.placements.size() == 1);
}

TEST_CASE("a dab sees its own trees: the second does not stand inside the first") {
    // Judged ALL AT ONCE, two candidates that overlap each other are both
    // refused and the builder gets an empty click where one tree was perfectly
    // possible. Judged in sequence, the first stands and the second steps
    // aside, which is what this holds.
    world::SceneDoc doc;
    doc.world_span_m = 256.0f;

    std::vector<PlantCandidate> pair(2);
    pair[0].object = "oak";
    pair[0].position = {100.0f, FLAT_GROUND_M, 100.0f};
    pair[1].object = "oak";
    pair[1].position = {100.2f, FLAT_GROUND_M, 100.0f}; // 20 cm away: inside it

    const PlantDabReport r = plant_dab(doc, pair, judge_world(false));
    CHECK(r.planted == 1);
    CHECK(r.refused == 1);
    CHECK(r.verdicts[0].allowed);
    CHECK_FALSE(r.verdicts[1].allowed);
    CHECK(doc.placements.size() == 1);
}

TEST_CASE("editing a placement re-judges it, and a refusal changes nothing") {
    world::SceneDoc doc;
    doc.world_span_m = 256.0f;
    world::Placement p;
    p.object = "oak";
    p.position = {100.0f, FLAT_GROUND_M, PATH_CENTRE_Z + 40.0f};
    p.yaw = 0.0f;
    p.scale = 1.0f;
    doc.placements.push_back(p);

    // An accepted edit: turning it changes nothing any rule cares about.
    PlantParams turn;
    turn.set_yaw = true;
    turn.yaw = 1.25f;
    const BuildVerdict ok = edit_placement(doc, 0, turn, judge_world(true));
    CHECK(ok.allowed);
    CHECK(doc.placements[0].yaw == doctest::Approx(1.25f));

    // ONE THING CHANGES: the object becomes one the registry does not carry.
    // The judge says KnownObject, and the placement must come back untouched —
    // an editor that applies what the judge just refused has taught the builder
    // that red is decoration.
    PlantParams bogus;
    bogus.object = "not-on-any-shelf";
    const BuildVerdict no = edit_placement(doc, 0, bogus, judge_world(true));
    CHECK_FALSE(no.allowed);
    CHECK(doc.placements[0].object == "oak");
    CHECK(doc.placements[0].yaw == doctest::Approx(1.25f));
}

TEST_CASE("the crosshair picks the small thing in front, not the big thing behind") {
    world::SceneDoc doc;
    world::Placement oak;
    oak.object = "oak";
    oak.position = {0.0f, FLAT_GROUND_M, 0.0f};
    world::Placement fern;
    fern.object = "fern";
    fern.position = {1.0f, FLAT_GROUND_M, 0.0f}; // inside the oak's 2 m reach
    doc.placements.push_back(oak);
    doc.placements.push_back(fern);

    // Aimed at the fern: it is nearer, so it wins even though the oak's reach
    // also covers the point.
    CHECK(pick_placement(doc, {1.05f, 0.0f}, &radius_of, nullptr) == 1);
    // Aimed at the oak's far side, where the fern cannot reach.
    CHECK(pick_placement(doc, {-1.5f, 0.0f}, &radius_of, nullptr) == 0);
    // Aimed at nothing: the answer is "nothing", not the nearest anything.
    CHECK(pick_placement(doc, {90.0f, 90.0f}, &radius_of, nullptr) == doc.placements.size());
}

// ================= THE ACCIDENT THIS TOOL MUST NOT HAVE =====================

TEST_CASE("dragging a slider never digs a hole") {
    // THE LEAD NAMED THIS ONE BY HAND: «настроил кисть и случайно выкопал яму»
    // happens on the first afternoon and once per builder. He drags the SIZE
    // slider, the pointer leaves the panel while the button is still down, and
    // the brush bites the ground he was only looking at.
    BrushStroke stroke;

    // Press ON the panel: nothing, and the stroke is marked blocked.
    CHECK_FALSE(stroke_step(stroke, /*pointer_down=*/true, /*ui_wants_mouse=*/true));
    CHECK(stroke.blocked);
    CHECK_FALSE(stroke.active);

    // THE DRAG CONTINUES AND THE POINTER LEAVES THE PANEL, button still held.
    // This is the exact frame the accident happens on, and it must stay quiet:
    // the decision was made at the press and a blocked stroke stays blocked.
    for (int frame = 0; frame < 30; ++frame) {
        CHECK_FALSE(stroke_step(stroke, true, /*ui_wants_mouse=*/false));
        CHECK_FALSE(stroke.active);
    }

    // Letting go clears it, and a BLOCKED stroke ends with nothing to rebuild:
    // it never bit anything.
    CHECK_FALSE(stroke_step(stroke, false, false));
    CHECK_FALSE(stroke.blocked);
    CHECK_FALSE(stroke.just_ended);
}

TEST_CASE("a stroke that began on the ground does paint, and reports its end once") {
    // THE COUNTERFACTUAL. Without it, a stroke_step that returned false forever
    // would pass the test above perfectly — and the brush would simply not work.
    BrushStroke stroke;

    CHECK(stroke_step(stroke, /*pointer_down=*/true, /*ui_wants_mouse=*/false));
    CHECK(stroke.active);
    for (int frame = 0; frame < 5; ++frame) {
        CHECK(stroke_step(stroke, true, false));
    }

    // AND IT KEEPS PAINTING when the pointer wanders over a panel mid-stroke:
    // cutting it off at the panel's edge would tear a sculpted ridge in half,
    // and the builder would blame the brush.
    CHECK(stroke_step(stroke, true, /*ui_wants_mouse=*/true));
    CHECK(stroke.active);

    // The end is reported EXACTLY ONCE — that is the frame the caller spends
    // its chunk-rebuild budget on, and twice would pay for the stroke twice.
    CHECK_FALSE(stroke_step(stroke, false, false));
    CHECK(stroke.just_ended);
    CHECK_FALSE(stroke_step(stroke, false, false));
    CHECK_FALSE(stroke.just_ended);
}
