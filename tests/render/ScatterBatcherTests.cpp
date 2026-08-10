/*
Created: 09:08:2026 - 11:57:20
Last updated: 10:08:2026 - 01:47:53
Module: tests
File: tests/render/ScatterBatcherTests.cpp

Responsibility:
- Unit tests for scatter batching: tree/micro split, world-space baking, tile
  bounding circles, determinism, degenerate inputs.

Key items:
- doctest cases over build_scatter_batches.

Dependencies:
- Uses: doctest, engine/render ScatterBatcher.
- Used by: ctest (render_scatter_batcher).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 11:57:20: Stage 3b — initial tests.
- 09:08:2026 - 19:48:00: Flora bake cost measurement (per-instance tree
  geometry) as a regression guard against the chunk streaming budget.
- 10:08:2026 - 01:47:53: Measured tile radius cases; the deleted species_radius
  table's values kept as the failing control (Rule 30).
*/

#include "engine/render/sources/ScatterBatcher.h"

#include <doctest/doctest.h>

#include <chrono>

#include <cmath>
#include <vector>

using dfn::math::ScatterInstance;
using dfn::math::ScatterSpecies;
using dfn::render::build_scatter_batches;
using dfn::render::ScatterBatches;

namespace {

constexpr glm::vec2 ORIGIN{256.0f, 0.0f};
constexpr float CHUNK = 256.0f;

ScatterInstance make(float x, float z, ScatterSpecies s, float scale = 1.0f) {
    ScatterInstance inst;
    inst.position = {x, 20.0f, z};
    inst.yaw = 0.5f;
    inst.scale = scale;
    inst.species = s;
    return inst;
}

} // namespace

TEST_CASE("trees and micro scatter split into their batches") {
    const std::vector<ScatterInstance> instances{
        make(300.0f, 40.0f, ScatterSpecies::OakTree),
        make(310.0f, 50.0f, ScatterSpecies::PineTree),
        make(320.0f, 60.0f, ScatterSpecies::BirchTree),
        make(330.0f, 70.0f, ScatterSpecies::Bush),
        make(340.0f, 80.0f, ScatterSpecies::Stone),
    };
    const ScatterBatches batches = build_scatter_batches(instances, ORIGIN, CHUNK);
    CHECK(!batches.trees.vertices.empty());
    CHECK(!batches.micro.empty());

    // Baked tree vertices sit around their world positions (not model space).
    float min_x = 1e9f;
    float max_x = -1e9f;
    for (const auto& v : batches.trees.vertices) {
        min_x = std::min(min_x, v.position.x);
        max_x = std::max(max_x, v.position.x);
    }
    CHECK(min_x > 290.0f);
    // The BUSH at x=330 is now baked into this stream too: routing asks
    // `flora_owns()` rather than a hand-written three-tree predicate, and
    // flora owns the bush. So the stream reaches a bush's half-width past 330
    // instead of stopping at the birch — which is the whole point of the
    // change, and the reason §5.10's floor drew as bare earth before it.
    CHECK(max_x < 334.0f);
}

TEST_CASE("micro tiles cover their instances within center + radius") {
    // STONE ONLY. The micro-tile path is now exactly "what flora does not
    // own" — bushes moved to the flora stream with the rest of §5.10 — so a
    // bush here would test the tiling of an empty set.
    const std::vector<ScatterInstance> instances{
        make(260.0f, 10.0f, ScatterSpecies::Stone),
        make(500.0f, 250.0f, ScatterSpecies::Stone),
    };
    const ScatterBatches batches = build_scatter_batches(instances, ORIGIN, CHUNK, 4);
    REQUIRE(batches.micro.size() == 2); // opposite corners -> different tiles
    for (const auto& tile : batches.micro) {
        for (const auto& v : tile.mesh.vertices) {
            const glm::vec2 d{v.position.x - tile.center_xz.x,
                              v.position.z - tile.center_xz.y};
            CHECK(glm::length(d) <= tile.radius_m + 1e-3f);
        }
    }
}

TEST_CASE("scale is applied and batching is deterministic") {
    const std::vector<ScatterInstance> one{make(300.0f, 100.0f,
                                               ScatterSpecies::OakTree, 2.0f)};
    const ScatterBatches a = build_scatter_batches(one, ORIGIN, CHUNK);
    const ScatterBatches b = build_scatter_batches(one, ORIGIN, CHUNK);
    REQUIRE(!a.trees.vertices.empty());
    REQUIRE(a.trees.vertices.size() == b.trees.vertices.size());
    float max_y = 0.0f;
    for (size_t i = 0; i < a.trees.vertices.size(); ++i) {
        CHECK(a.trees.vertices[i].position == b.trees.vertices[i].position);
        max_y = std::max(max_y, a.trees.vertices[i].position.y);
    }
    // Nominal oak ~10 m at scale 2 above ground height 20 -> well over 35.
    CHECK(max_y > 35.0f);
}

TEST_CASE("empty and degenerate inputs return empty batches") {
    const ScatterBatches none = build_scatter_batches({}, ORIGIN, CHUNK);
    CHECK(none.trees.vertices.empty());
    CHECK(none.micro.empty());
    const std::vector<ScatterInstance> one{make(300.0f, 100.0f, ScatterSpecies::Stone)};
    const ScatterBatches bad = build_scatter_batches(one, ORIGIN, 0.0f);
    CHECK(bad.micro.empty());
}

TEST_CASE("a full chunk of flora bakes well inside the streaming budget") {
    // The flora agent flagged the cost of per-instance tree geometry and asked
    // to be measured rather than guessed at. This is that measurement, kept as
    // a regression guard: core admits ONE chunk per streaming update because a
    // cold ring cost seconds, and ~83 ms of that update is already collision
    // shape building. If flora baking ever approaches that, it stops being an
    // implementation detail and becomes a visible hitch while walking.
    std::vector<ScatterInstance> forest;
    for (int i = 0; i < 100; ++i) {
        const float fx = 260.0f + static_cast<float>((i * 37) % 250);
        const float fz = 10.0f + static_cast<float>((i * 61) % 250);
        const auto species = i % 3 == 0   ? ScatterSpecies::OakTree
                             : i % 3 == 1 ? ScatterSpecies::PineTree
                                          : ScatterSpecies::BirchTree;
        forest.push_back(make(fx, fz, species, 0.8f + 0.01f * static_cast<float>(i % 40)));
    }
    const auto start = std::chrono::steady_clock::now();
    const ScatterBatches batches = build_scatter_batches(forest, ORIGIN, CHUNK);
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - start).count();
    MESSAGE("100 trees baked in " << ms << " ms, "
            << batches.trees.indices.size() / 3 << " triangles");
    REQUIRE(!batches.trees.vertices.empty());
    // Generous ceiling: this is a "did it explode" guard, not a target.
    CHECK(ms < 100.0);
}

TEST_CASE("tile radius is measured from the baked geometry, not tabled") {
    // Species meshes grew past their table twice (stone 0.5 tabled vs ~0.78
    // built, birch 3.1 vs ~4.94 after flora's crown fix) and every time the
    // table lagged, tiles culled while their geometry was on screen. The
    // radius is now measured over the baked vertices, so coverage holds by
    // construction — and the OLD TABLED VALUES are kept here as the control
    // that FAILS against the same geometry (Rule 30).
    const float scale = 1.4f;
    const std::vector<ScatterInstance> instances{
        make(260.0f, 10.0f, ScatterSpecies::Stone, scale),
        make(262.0f, 12.0f, ScatterSpecies::Bush, scale),
    };
    const ScatterBatches batches = build_scatter_batches(instances, ORIGIN, CHUNK, 4);
    REQUIRE(batches.micro.size() == 1);
    const auto& tile = batches.micro.front();

    // Derived radius covers every baked vertex exactly (it IS their maximum).
    float measured = 0.0f;
    for (const auto& v : tile.mesh.vertices) {
        const glm::vec2 d{v.position.x - tile.center_xz.x,
                          v.position.z - tile.center_xz.y};
        CHECK(glm::length(d) <= tile.radius_m + 1e-3f);
        measured = std::max(measured, glm::length(d));
    }
    CHECK(tile.radius_m == doctest::Approx(measured));

    // THE CONTROL — the deleted table, applied the way the old code did
    // (distance from sample point to tile center + tabled radius * scale).
    // The stone's own built geometry must exceed what its table entry
    // claimed, or this whole change guarded against nothing.
    const auto old_formula = [&](const ScatterInstance& inst, float tabled) {
        const glm::vec2 d{inst.position.x - tile.center_xz.x,
                          inst.position.z - tile.center_xz.y};
        return glm::length(d) + tabled * inst.scale;
    };
    // Worst over both instances, exactly as the old per-instance max was.
    const float old_radius = std::max(old_formula(instances[0], 0.5f),
                                      old_formula(instances[1], 2.0f));
    CHECK(measured > old_radius);
}

TEST_CASE("a species mesh's real reach exceeds its dead table entry") {
    // The direct form of the same control: the stone build's horizontal
    // reach from its own origin, measured, against the number the table
    // shipped for it. If a future species build SHRINKS below the old table
    // this case stops discriminating — then pick a new failing pair, do not
    // delete the control.
    const dfn::render::MeshData stone =
        dfn::render::build_scatter_mesh(ScatterSpecies::Stone);
    REQUIRE(!stone.vertices.empty());
    float reach = 0.0f;
    for (const auto& v : stone.vertices) {
        reach = std::max(reach, glm::length(glm::vec2{v.position.x, v.position.z}));
    }
    CHECK(reach > 0.5f); // the deleted table entry under-covered this mesh
}
