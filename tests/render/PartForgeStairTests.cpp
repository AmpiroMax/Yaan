/*
Module: tests
File: tests/render/PartForgeStairTests.cpp

Responsibility:
- The stair family's contract: the steep flight (variant 1, 45°) lands on the
  next floor WITHIN ITS OWN PLAN RUN, the gentle flight (26.5°) keeps twice
  the run — same meter on both, so the suite proves the property and not the
  tolerance. Plus the riser-against-controller check: every step the kit
  sells is one the player can climb.

Key items:
- run/rise ratio meter over the forged mesh (bbox of real vertices, not of
  the params — the geometry is what the player walks on).
- riser vs PLAYER_STEP_HEIGHT, with the 2u counter-arm spelled out.

Dependencies:
- Uses: engine/render (PartForge), engine/core config (PLAYER_STEP_HEIGHT),
  MeshMeters.h, doctest.
- Used by: ctest (render_part_forge_stairs).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 30: the gentle flight IS the control of the run meter — the same
  instrument that reads ~1.0 on the steep flight must read ~2.0 there, or it
  is not measuring pitch at all.
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/render/sources/PartForge.h"

#include "MeshMeters.h"

#include <algorithm>
#include <doctest/doctest.h>

using namespace dfn::render;

namespace {

PartParams stair_params(int variant, int width_u, int steps, PartMaterial mat,
                        float wear = 0.3f) {
    PartParams p;
    p.kind = PartKind::Stair;
    p.material = mat;
    p.variant = variant;
    p.length_u = 1;
    p.width_u = width_u;
    p.height_u = steps;
    p.wear = wear;
    p.name = part_name(p);
    p.seed = 2026;
    return p;
}

struct FlightSpan {
    float run;  ///< plan extent along +X, metres
    float rise; ///< vertical extent, metres
};

/// The forged flight's real footprint: bbox over vertices. What the player
/// walks on is the mesh, so the meter reads the mesh and not the params.
FlightSpan span_of(const RegistryObject& obj) {
    float max_x = 0.0f;
    float max_y = 0.0f;
    for (const auto& v : meshtest::solid_of(obj).vertices) {
        max_x = std::max(max_x, v.position.x);
        max_y = std::max(max_y, v.position.y);
    }
    return {max_x, max_y};
}

} // namespace

TEST_CASE("the steep flight reaches its floor within its own run; the gentle one needs double") {
    // 13 steps = the primary dwelling wall height (13u = 3.25 m, HOUSES.md §6).
    const auto steep = forge_part(stair_params(1, 4, 13, PartMaterial::Timber));
    const auto gentle = forge_part(stair_params(0, 4, 13, PartMaterial::Timber));
    const FlightSpan s = span_of(steep);
    const FlightSpan g = span_of(gentle);
    // Both flights land at the same height: 13 steps x 0.25 m = 3.25. The
    // bbox reads a little HIGH, never low: the stringer (half-section 0.125)
    // rides past the top tread along the diagonal — measured 3.34/3.36 —
    // so the window is [nominal, nominal + stringer + wobble].
    CHECK(s.rise >= 3.25f);
    CHECK(s.rise <= 3.25f + 0.15f);
    CHECK(g.rise >= 3.25f);
    CHECK(g.rise <= 3.25f + 0.15f);
    // THE PROPERTY UNDER TEST: run/rise ~ 1 for the steep flight (45°) —
    // «на второй этаж за длину этих доводили» — and ~2 for the gentle one
    // (26.5°). The control arm is the same meter reading DOUBLE, so a meter
    // that always says "about one" cannot pass this pair.
    CHECK(s.run / s.rise == doctest::Approx(1.0f).epsilon(0.05));
    CHECK(g.run / g.rise == doctest::Approx(2.0f).epsilon(0.05));
}

TEST_CASE("every riser the kit sells is one the player climbs") {
    const auto steep = forge_part(stair_params(1, 4, 13, PartMaterial::Timber));
    const FlightSpan s = span_of(steep);
    const float riser = s.rise / 13.0f;
    // 0.25 m against PLAYER_STEP_HEIGHT 0.35: passable, with margin.
    CHECK(riser < static_cast<float>(dfn::config::PLAYER_STEP_HEIGHT));
    // The counter-arm, spelled as arithmetic: a flight made steep by DOUBLING
    // the rise (2u = 0.50 m) would exceed the controller's step and be
    // scenery. This is why STAIR_STEEP_GOING_U shortens the going instead —
    // the constraint the catalogue comment cites, held red here.
    CHECK(2.0f * riser > static_cast<float>(dfn::config::PLAYER_STEP_HEIGHT));
}

TEST_CASE("steep stairs are closed volumes and deterministic") {
    const auto p = stair_params(1, 6, 12, PartMaterial::Stone, 0.8f);
    const auto obj = forge_part(p);
    CHECK(meshtest::half_edge_defects(meshtest::solid_of(obj)) == 0);
    CHECK(object_content_hash(obj) == object_content_hash(forge_part(p)));
}

TEST_CASE("stair names: -steep spells the pitch, legacy names stay byte-stable") {
    CHECK(stair_params(1, 4, 13, PartMaterial::Timber).name
          == "stair-steep-timber-1x4x13-w03");
    CHECK(stair_params(1, 6, 8, PartMaterial::Stone, 0.8f).name
          == "stair-steep-stone-1x6x8-w08");
    // The shipped gentle flights keep their exact old names (houses stand on
    // them): variant 0 still routes through the generic LxWxH grammar.
    CHECK(stair_params(0, 4, 7, PartMaterial::Timber, 0.3f).name
          == "stair-timber-1x4x7-w03");
}

TEST_CASE("the catalogue carries both stair families") {
    const auto cat = kit_catalogue();
    int gentle = 0;
    int steep = 0;
    for (const auto& p : cat) {
        if (p.kind != PartKind::Stair) continue;
        if (p.variant == 1) {
            ++steep;
        } else {
            ++gentle;
        }
    }
    // Gentle: 9 sections x 2 materials x 2 wears — UNCHANGED, streets and
    // terraces stand on them. Steep: widths 4/6 x steps 8/11/12/13/14 x
    // 2 materials x 2 wears.
    CHECK(gentle == 36);
    CHECK(steep == 40);
}
