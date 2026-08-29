/*
Module: tests/app
File: tests/app/EditorBrushOutlineTests.cpp

Responsibility:
- Holds the drawn outline of the brush's zone to the ONE property that makes it
  worth drawing: that it is the boundary the brush actually honours. A ring that
  promises ground the brush does not bite — or hides ground it does — is worse
  than no ring, because the builder stops looking at it.

Dependencies:
- Uses: engine/editor (EditorBrush: brush_outline and the falloff it is bisected
  out of), engine/world (ReliefLayer), doctest.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NO RENDERER IS LINKED HERE, and that is the point (Rule 3): the outline is
  GEOMETRY, so it is measured in metres instead of judged from a screenshot. A
  ring drawn beautifully in the wrong place looks correct in every frame.
- THE ARMS DIFFER BY ONE THING (Rule 30). The "nothing outside the ring moved"
  claim is worthless alone — a ring a kilometre wide passes it — so it is always
  paired with "and something within one lattice step of the ring did".
- SEPARATE FROM EditorBrushTests.cpp ONLY BECAUSE OF RULE 21: the two suites
  are one subject and share the target app_editor_brush; the split is the 800
  line limit and nothing else.
*/

#include <doctest/doctest.h>

#include "engine/editor/sources/EditorBrush.h"
#include "engine/world/sources/ReliefLayer.h"

#include <cmath>
#include <cstdint>
#include <vector>

using namespace dfn;
using namespace dfn::app;

namespace {

/// A FLAT WORLD AT A KNOWN HEIGHT — the outline is being measured, not the
/// generator. Its own constant rather than a shared one: this suite links no
/// world generator at all, and a fixture reaching across files for a number
/// would tie two suites together for the sake of the digit 20.
constexpr float FLAT_GROUND_M = 20.0f;

float flat_ground(void* /*ctx*/, glm::vec2 /*p*/) { return FLAT_GROUND_M; }

/// A SLOPE, for the one question a plane cannot answer: whether the ring reads
/// the ground under it at all.
float ramp_ground(void* /*ctx*/, glm::vec2 p) { return FLAT_GROUND_M + 0.25f * p.x; }

BrushGround flat_world() { return BrushGround{&flat_ground, nullptr}; }

} // namespace

// ========================= THE ZONE, MADE VISIBLE ===========================

namespace {

/// A GROUND WITH CURVATURE. A plane and a ramp both hide the smoothing brush:
/// on either, the average of a sample's four neighbours IS the sample, so a
/// dab moves nothing and a test on it would prove nothing. A bowl has the same
/// curvature everywhere, so every sample inside the brush moves.
float bowl_ground(void* /*ctx*/, glm::vec2 p) {
    return FLAT_GROUND_M + 0.02f * (p.x * p.x + p.y * p.y);
}

/// What a dab moved, as the report's box with the one lattice step of padding
/// TAKEN BACK OFF. The padding exists so the chunk rebuild covers the bilinear
/// reach; leaving it in would let a ring one step too wide pass as honest.
glm::vec2 touched_min(const BrushDabReport& r) {
    return r.min_xz + glm::vec2{world::RELIEF_STEP_M, world::RELIEF_STEP_M};
}
glm::vec2 touched_max(const BrushDabReport& r) {
    return r.max_xz - glm::vec2{world::RELIEF_STEP_M, world::RELIEF_STEP_M};
}

} // namespace

TEST_CASE("the ring is the boundary the brush actually honours") {
    // THE CLAIM THE WHOLE FEATURE STANDS ON, and it has two sides because
    // either alone is worthless: a ring of a kilometre passes "nothing outside
    // moved", and a ring drawn at half the brush passes it too while lying
    // about the other half. So: nothing outside the ring moves, AND something
    // within one lattice step of the ring does.
    constexpr glm::vec2 CENTRE{0.0f, 0.0f}; // ON a lattice point — see below
    struct Arm {
        BrushMode mode;
        float radius_m;
        float hardness;
    };
    const Arm ARMS[] = {
        {BrushMode::Raise, 6.0f, 0.0f},   {BrushMode::Raise, 21.5f, 0.9f},
        {BrushMode::Lower, 2.5f, 0.5f},   {BrushMode::Smooth, 12.0f, 0.3f},
        {BrushMode::Paint, 9.0f, 0.5f},
        // A SLIDER UNDER THE LATTICE. The brush is widened to 2 m, so a ring at
        // 0.4 m would promise a hand's-width edit while two metres of ground
        // move — the exact lie this section exists to make impossible.
        {BrushMode::Raise, 0.4f, 0.0f},
    };
    const BrushGround bowl{&bowl_ground, nullptr};
    for (const Arm& arm : ARMS) {
        CAPTURE(static_cast<int>(arm.mode));
        CAPTURE(arm.radius_m);
        TerrainBrush brush;
        brush.mode = arm.mode;
        brush.radius_m = arm.radius_m;
        brush.hardness = arm.hardness;
        brush.strength_m_s = 2.0f;
        world::ReliefLayer layer;
        const BrushDabReport r = apply_brush(layer, brush, CENTRE, 0.5f, bowl);
        REQUIRE(r.any);
        const float rim = brush_rim_m(brush);

        // NOTHING OUTSIDE THE RING MOVED.
        CHECK(touched_min(r).x >= CENTRE.x - rim);
        CHECK(touched_min(r).y >= CENTRE.y - rim);
        CHECK(touched_max(r).x <= CENTRE.x + rim);
        CHECK(touched_max(r).y <= CENTRE.y + rim);
        // AND THE RING IS NOT OVERSIZED. The centre sits on a lattice point, so
        // the row through it carries a moved sample within one step of the rim;
        // any slack beyond that would be ground the ring claims and never bites.
        CHECK(touched_max(r).x >= CENTRE.x + rim - world::RELIEF_STEP_M - 1.0e-3f);
        CHECK(touched_max(r).y >= CENTRE.y + rim - world::RELIEF_STEP_M - 1.0e-3f);
    }
}

TEST_CASE("the rim is the falloff's own zero, clamp and all") {
    TerrainBrush brush;
    brush.radius_m = 8.0f;
    brush.hardness = 0.5f;
    const float rim = brush_rim_m(brush);
    // A THIRD OF A MILLIMETRE INSIDE THE SLIDER'S 8 m, AND THAT IS THE POINT:
    // near the rim the smoothstep's `1 - s` rounds to exactly 0 while the
    // distance is still short of the radius, so the brush really does stop
    // biting there. The ring reports the FUNCTION, not the algebra — which is
    // why the two checks below can be exact rather than approximate.
    CHECK(rim <= 8.0f);
    CHECK(rim > 8.0f - 0.01f);
    CHECK(brush_weight(rim, brush.radius_m, brush.hardness) == 0.0f);
    CHECK(brush_weight(std::nextafter(rim, 0.0f), brush.radius_m, brush.hardness) > 0.0f);

    // THE ARM THAT MATTERS: the ring follows the CLAMP, because it is bisected
    // out of brush_weight rather than read off the slider.
    brush.radius_m = 0.4f;
    CHECK(brush_rim_m(brush) == doctest::Approx(BRUSH_MIN_RADIUS_M).epsilon(0.001));
    CHECK(brush_rim_m(brush) > 0.4f);
}

TEST_CASE("the inner ring is where full strength ends") {
    TerrainBrush brush;
    brush.radius_m = 8.0f;
    brush.hardness = 0.5f;
    const float core = brush_core_m(brush);
    // hardness * radius, to within the rounding the smoothstep does at full
    // strength — measured off the function, exactly as the rim is.
    CHECK(core == doctest::Approx(4.0f).epsilon(0.001));
    CHECK(brush_weight(core, 8.0f, 0.5f) == 1.0f);
    CHECK(brush_weight(std::nextafter(core, 9.0f), 8.0f, 0.5f) < 1.0f);
    const BrushOutline hard = brush_outline(brush, {0.0f, 0.0f}, flat_world());
    CHECK_FALSE(hard.core.empty());

    // A BRUSH SOFT TO ITS OWN CENTRE HAS NO FLAT TOP WORTH A LINE, and then
    // there is no inner ring at all — the counterfactual without which "4 m"
    // means nothing. Its measured flat top is not exactly zero (the smoothstep
    // returns exactly 1 for the first few hundred microns), so the claim is
    // "under a lattice step", which is what brush_outline drops.
    brush.hardness = 0.0f;
    CHECK(brush_core_m(brush) < world::RELIEF_STEP_M);
    CHECK(brush_core_m(brush) < 0.01f);
    const BrushOutline soft = brush_outline(brush, {0.0f, 0.0f}, flat_world());
    CHECK(soft.core.empty());
    CHECK_FALSE(soft.rim.empty());
    // AND THE RIM SURVIVES THE SMALLEST BRUSH THERE IS. The floor that drops
    // the inner ring is one lattice step, and a minimum-radius brush's rim
    // measures a hair UNDER that — so a floor applied to both rings would have
    // silently deleted the whole outline on the brush most likely to be used
    // for fine work.
    brush.radius_m = BRUSH_MIN_RADIUS_M;
    CHECK_FALSE(brush_outline(brush, {0.0f, 0.0f}, flat_world()).rim.empty());
}

TEST_CASE("the colour says the direction, and says nothing else") {
    TerrainBrush brush;
    brush.radius_m = 6.0f;
    const BrushGround g = flat_world();

    brush.mode = BrushMode::Raise;
    const BrushOutline up = brush_outline(brush, {0.0f, 0.0f}, g);
    brush.mode = BrushMode::Lower;
    const BrushOutline down = brush_outline(brush, {0.0f, 0.0f}, g);
    CHECK(up.direction == BrushDirection::Up);
    CHECK(up.color_rgba == BRUSH_UP_GREEN);
    CHECK(down.direction == BrushDirection::Down);
    CHECK(down.color_rgba == BRUSH_DOWN_RED);
    // ONE THING DIFFERS — the mode — so the colours must differ and the rings
    // must not: a colour that also moved the boundary would be two claims.
    CHECK(up.color_rgba != down.color_rgba);
    CHECK(up.rim_m == doctest::Approx(down.rim_m));

    // SMOOTHING HAS NO DIRECTION: it pulls a bump down and fills the dip beside
    // it in one dab. Either colour there would teach a direction the tool has
    // not got.
    brush.mode = BrushMode::Smooth;
    const BrushOutline mixed = brush_outline(brush, {0.0f, 0.0f}, g);
    CHECK(mixed.direction == BrushDirection::Mixed);
    CHECK(mixed.color_rgba != BRUSH_UP_GREEN);
    CHECK(mixed.color_rgba != BRUSH_DOWN_RED);

    // FLATTEN IS UP OR DOWN BY WHERE IT LEVELS TO, and both arms are here
    // because either alone would pass with the sign inverted.
    brush.mode = BrushMode::Flatten;
    brush.flatten_height_m = FLAT_GROUND_M + 3.0f;
    CHECK(brush_outline(brush, {0.0f, 0.0f}, g).color_rgba == BRUSH_UP_GREEN);
    brush.flatten_height_m = FLAT_GROUND_M - 3.0f;
    CHECK(brush_outline(brush, {0.0f, 0.0f}, g).color_rgba == BRUSH_DOWN_RED);

    // PAINTING HAS NO UP AND NO DOWN, so the ring wears the surface's swatch —
    // and two surfaces do not share one, or the colour would say nothing.
    brush.mode = BrushMode::Paint;
    brush.paint = math::SurfaceClass::Rock;
    const uint32_t rock = brush_outline(brush, {0.0f, 0.0f}, g).color_rgba;
    brush.paint = math::SurfaceClass::Sand;
    const uint32_t sand = brush_outline(brush, {0.0f, 0.0f}, g).color_rgba;
    CHECK(rock != sand);
    CHECK(rock != BRUSH_UP_GREEN);
    CHECK(sand != BRUSH_DOWN_RED);
}

TEST_CASE("the ring closes and lies on the ground it traces") {
    TerrainBrush brush;
    brush.mode = BrushMode::Raise;
    brush.radius_m = 10.0f;
    brush.hardness = 0.5f;
    constexpr glm::vec2 CENTRE{40.0f, -12.0f};
    const BrushOutline o = brush_outline(brush, CENTRE, BrushGround{&ramp_ground, nullptr});
    REQUIRE(!o.rim.empty());
    CHECK(o.rim.size() == static_cast<std::size_t>(brush_outline_segments(o.rim_m)) + 1);
    // CLOSED BIT FOR BIT, not nearly: a hairline gap in a ring redrawn every
    // frame reads as a flickering seam in the terrain.
    CHECK(o.rim.front().x == o.rim.back().x);
    CHECK(o.rim.front().z == o.rim.back().z);
    for (const glm::vec3& p : o.rim) {
        const float dx = p.x - CENTRE.x;
        const float dz = p.z - CENTRE.y;
        CHECK(std::sqrt(dx * dx + dz * dz) == doctest::Approx(o.rim_m).epsilon(0.001));
        // ON THE GROUND, lifted by a hand's width. Drawn exactly on the surface
        // the line loses the depth test to its own terrain and shows NOTHING,
        // which is indistinguishable from a feature nobody wrote.
        CHECK(p.y
              == doctest::Approx(ramp_ground(nullptr, {p.x, p.z}) + BRUSH_OUTLINE_LIFT_M));
    }
    // THE COUNTERFACTUAL for the line above: on a flat world every ring point
    // sits at one height, so a ring that ignored the ground entirely would have
    // passed the ramp check only if the ramp were level — and it is not.
    const BrushOutline flat = brush_outline(brush, CENTRE, flat_world());
    CHECK(flat.rim.front().y == doctest::Approx(FLAT_GROUND_M + BRUSH_OUTLINE_LIFT_M));
    CHECK(flat.rim.front().y != doctest::Approx(o.rim.front().y));
}
