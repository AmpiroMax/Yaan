/*
Created: 09:08:2026 - 00:42:03
Last updated: 09:08:2026 - 14:41:26
Module: tests
File: tests/core/ChunkManagerTests.cpp

Responsibility:
- Chunk streaming suite: 3x3 residency ring, load/unload hysteresis, event
  protocol (unload published while data is still valid), height queries,
  extent clipping, unload_all.

Dependencies:
- Uses: doctest, dfn_world (ChunkManager), dfn_core (ecs, events).
- Used by: ctest (test_chunk_streaming).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 00:42:03: Stage 2 — initial suite.
- 09:08:2026 - 11:05:22: Stage 3b — worldgen v2 spawns site entities: baseline
  captured before streaming; new cases for site component attachment,
  surfacefield/scatter/water_bodies accessors.
- 09:08:2026 - 14:41:26: Frame-05 bed fix: water_bodies().lakes is now lake + pond planes (was exactly 1); all planes checked for positive extent.
*/

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/world/sources/ChunkManager.h"
#include "engine/world/sources/SiteComponents.h"

#include <doctest/doctest.h>
#include <vector>

using dfn::world::ChunkCoord;
using dfn::world::ChunkLoaded;
using dfn::world::ChunkManager;
using dfn::world::ChunkStreamingParams;
using dfn::world::ChunkUnloaded;
using dfn::world::WorldGenParams;

namespace {
constexpr float CHUNK_SIZE_M = static_cast<float>(dfn::config::CHUNK_SIZE);

struct Fixture {
    dfn::ecs::World ecs;
    dfn::events::EventBus bus;
    ChunkManager chunks;
    std::vector<ChunkCoord> loaded_events;
    std::vector<ChunkCoord> unloaded_events;

    // Radius 1 / 2 -> a 3x3 resident ring with hysteresis (stage-2 skeleton).
    explicit Fixture(WorldGenParams gen = WorldGenParams{123, {-10, -10}, {10, 10}}) {
        bus.subscribe<ChunkLoaded>([this](const ChunkLoaded& e) {
            loaded_events.push_back(e.coord);
            // Contract: data must be valid when the event fires.
            CHECK(chunks.heightfield(e.coord).has_value());
        });
        bus.subscribe<ChunkUnloaded>([this](const ChunkUnloaded& e) {
            unloaded_events.push_back(e.coord);
            // Contract: unload fires BEFORE the chunk memory is freed.
            CHECK(chunks.heightfield(e.coord).has_value());
        });
        chunks.open_generated(gen, ChunkStreamingParams{1, 2});
    }
};
} // namespace

TEST_CASE("update at origin loads a 3x3 ring with valid heightfields") {
    Fixture f;
    f.chunks.update({0.0f, 0.0f, 0.0f}, f.ecs, f.bus);

    CHECK(f.loaded_events.size() == 9);
    CHECK(f.chunks.loaded_chunks().size() == 9);
    for (int32_t z = -1; z <= 1; ++z) {
        for (int32_t x = -1; x <= 1; ++x) {
            CHECK(f.chunks.is_loaded(ChunkCoord{x, z}));
        }
    }
    CHECK_FALSE(f.chunks.is_loaded(ChunkCoord{2, 0}));

    const auto view = f.chunks.heightfield(ChunkCoord{0, 0});
    REQUIRE(view.has_value());
    CHECK(view->resolution == static_cast<uint32_t>(dfn::config::HEIGHTMAP_RESOLUTION));
    CHECK(view->heights.size() == static_cast<std::size_t>(view->resolution) * view->resolution);
    CHECK(view->height_scale > 0.0f);

    // Idempotent: a second update at the same focus changes nothing.
    f.chunks.update({1.0f, 0.0f, 1.0f}, f.ecs, f.bus);
    CHECK(f.loaded_events.size() == 9);
    CHECK(f.unloaded_events.empty());
}

TEST_CASE("moving the focus streams with hysteresis; far teleport swaps the ring") {
    Fixture f;
    f.chunks.update({0.0f, 0.0f, 0.0f}, f.ecs, f.bus);
    REQUIRE(f.chunks.loaded_chunks().size() == 9);

    // One chunk east: 3 new columns load; the old west column stays within
    // unload_radius 2 (hysteresis), so nothing unloads.
    f.chunks.update({1.5f * CHUNK_SIZE_M, 0.0f, 0.0f}, f.ecs, f.bus);
    CHECK(f.chunks.loaded_chunks().size() == 12);
    CHECK(f.unloaded_events.empty());
    CHECK(f.chunks.is_loaded(ChunkCoord{-1, 0}));

    // Far teleport: everything old is beyond radius 2 and unloads; a fresh
    // 3x3 ring appears around the new focus.
    f.loaded_events.clear();
    f.chunks.update({8.5f * CHUNK_SIZE_M, 0.0f, 8.5f * CHUNK_SIZE_M}, f.ecs, f.bus);
    CHECK(f.unloaded_events.size() == 12);
    CHECK(f.loaded_events.size() == 9);
    CHECK(f.chunks.loaded_chunks().size() == 9);
    CHECK(f.chunks.is_loaded(ChunkCoord{8, 8}));
    CHECK_FALSE(f.chunks.is_loaded(ChunkCoord{0, 0}));
    CHECK_FALSE(f.chunks.heightfield(ChunkCoord{0, 0}).has_value());
}

TEST_CASE("streaming is clipped to the world extent") {
    Fixture f{WorldGenParams{5, {0, 0}, {0, 0}}}; // single-chunk world
    f.chunks.update({0.0f, 0.0f, 0.0f}, f.ecs, f.bus);
    CHECK(f.chunks.loaded_chunks().size() == 1);
    CHECK(f.chunks.is_loaded(ChunkCoord{0, 0}));
    CHECK_FALSE(f.chunks.is_loaded(ChunkCoord{1, 0}));
}

TEST_CASE("height_at answers inside loaded chunks, declines outside") {
    Fixture f;
    f.chunks.update({0.0f, 0.0f, 0.0f}, f.ecs, f.bus);

    const auto h = f.chunks.height_at({10.0f, 20.0f});
    REQUIRE(h.has_value());
    CHECK(*h >= 0.0f);
    CHECK(*h <= 40.0f); // gentle hills stay well under 40 m

    // Bilinear sample matches the decoded corner at an exact grid point.
    const auto view = f.chunks.heightfield(ChunkCoord{0, 0});
    REQUIRE(view.has_value());
    const auto exact = f.chunks.height_at({0.0f, 0.0f});
    REQUIRE(exact.has_value());
    CHECK(*exact == doctest::Approx(view->height_at(0, 0)));

    CHECK_FALSE(f.chunks.height_at({100.0f * CHUNK_SIZE_M, 0.0f}).has_value());
}

TEST_CASE("determinism across managers: same seed serves identical heights") {
    Fixture a{WorldGenParams{777, {-2, -2}, {2, 2}}};
    Fixture b{WorldGenParams{777, {-2, -2}, {2, 2}}};
    a.chunks.update({0.0f, 0.0f, 0.0f}, a.ecs, a.bus);
    b.chunks.update({0.0f, 0.0f, 0.0f}, b.ecs, b.bus);
    const auto va = a.chunks.heightfield(ChunkCoord{1, -1});
    const auto vb = b.chunks.heightfield(ChunkCoord{1, -1});
    REQUIRE(va.has_value());
    REQUIRE(vb.has_value());
    for (std::size_t i = 0; i < va->heights.size(); ++i) {
        REQUIRE(va->heights[i] == vb->heights[i]);
    }
}

TEST_CASE("unload_all releases everything with the event protocol") {
    Fixture f;
    const std::size_t baseline = f.ecs.entity_count(); // before any streaming
    f.chunks.update({0.0f, 0.0f, 0.0f}, f.ecs, f.bus);
    REQUIRE(f.chunks.loaded_chunks().size() == 9);

    f.chunks.unload_all(f.ecs, f.bus);
    CHECK(f.unloaded_events.size() == 9);
    CHECK(f.chunks.loaded_chunks().empty());
    CHECK_FALSE(f.chunks.is_loaded(ChunkCoord{0, 0}));
    // No entity leaks: site entities spawned by loaded chunks are destroyed
    // with their groups; the world returns to its pre-streaming population.
    CHECK(f.ecs.entity_count() == baseline);
}

TEST_CASE("site entities spawn with components; surface/scatter views serve") {
    // Testbed layout: the hamlet (360, 500) lives in chunk (1, 1) — stream it.
    Fixture f{WorldGenParams{1, {0, 0}, {3, 3}}};
    f.chunks.update({384.0f, 0.0f, 384.0f}, f.ecs, f.bus); // focus chunk (1, 1)
    REQUIRE(f.chunks.is_loaded(ChunkCoord{1, 1}));

    // The hamlet's entities carry the full component set (Rule 11 batches).
    std::size_t sites = 0;
    for (const auto& [id, marker, transform, mesh] :
         f.ecs.view<dfn::world::SiteMarker, dfn::components::Transform,
                    dfn::components::RenderMesh>()) {
        ++sites;
        CHECK(mesh.mesh_asset != 0);
        CHECK(transform.position.y > 0.0f);
    }
    CHECK(sites >= static_cast<std::size_t>(dfn::config::HAMLET_SIZE_MIN));

    // Stage-3b views mirror heightfield residency and lifetimes.
    const auto surface = f.chunks.surfacefield(ChunkCoord{1, 1});
    REQUIRE(surface.has_value());
    CHECK(surface->surface_class.size() == surface->dist_to_water.size());
    CHECK(surface->water_surface.size() == surface->dist_to_water.size());
    CHECK_FALSE(f.chunks.surfacefield(ChunkCoord{3, 3}).has_value());

    // Water bodies are exposed for render's plane/ribbon materials: the lake
    // first, then one plane per surviving pond (so no water-covered sample
    // lacks a drawable body — see the WaterBed coverage invariant).
    const auto water = f.chunks.water_bodies();
    REQUIRE(water.lakes.size() >= 1);
    CHECK(water.lakes[0].surface_height
          == doctest::Approx(static_cast<float>(dfn::config::LAKE_LEVEL_TESTBED)));
    for (const auto& plane : water.lakes) {
        CHECK(plane.half_extent.x > 0.0f);
        CHECK(plane.half_extent.y > 0.0f);
    }
    CHECK_FALSE(water.river_stations.empty());
    REQUIRE(water.river_segment_offsets.size() >= 2);
    CHECK(water.river_segment_offsets.front() == 0);
    CHECK(water.river_segment_offsets.back() == water.river_stations.size());
}
