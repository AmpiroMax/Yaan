/*
Created: 09:08:2026 - 16:51:22
Last updated: 10:08:2026 - 02:44:55
Module: tests
File: tests/sim/TunnelWalkTests.cpp

Responsibility:
- THE acceptance test for voxel terrain collision: a character walks INTO the
  crag tunnel mouth and is still inside the mountain 100 m later, and stands on
  the tunnel floor with rock overhead. A heightfield-derived body passes
  neither — it has no overhangs, so the capsule would ride the surface over the
  crag instead of through it.

Key items:
- TunnelRig: real ChunkManager (seed 1, testbed extent) + Jolt + one static
  mesh body per resident chunk via physics::create_terrain_mesh_body.
- Collision-resolution measurement (triangle counts + shape build time) that
  justifies the chosen extraction resolution.

Dependencies:
- Uses: doctest, dfn_physics, dfn_platform_physics, dfn_world, dfn_core.
- Used by: ctest (sim_tunnel_walk).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- This test is the stage's acceptance line: it must drive the capsule through
  the real generated world, never a synthetic stand-in.
*/
/*
UPD:
- 09:08:2026 - 16:51:22: Created with the voxel terrain collision swap.
- 09:08:2026 - 17:08:40: DEBUG sprint (30 m/s) risk coverage: per-tick
                         tunnelling detector against tunnel walls and the
                         castle curtain wall, plus a streaming fall-through
                         run across chunk boundaries.
- 09:08:2026 - 18:21:53: Follow core's CHUNK_LOAD_BUDGET (one chunk per
                         update): drive streaming to SETTLED before walking
                         instead of assuming a ring loads in one update.
                         Assertions unchanged.
- 09:08:2026 - 19:17:00: Steer along the corridor CENTERLINE with a look-ahead
                         instead of beelining at each waypoint. Diagnosed after
                         a worldgen change stalled the walk at 91.6 m: the
                         corridor was open (nudged to the centerline the capsule
                         walked freely), but a beeline cuts the corner into the
                         outer wall at a switchback and wedges on voxel-wall
                         bumps. Assertions unchanged; the walk now completes the
                         full 8/8 waypoints, ~149 m.
- 09:08:2026 - 20:56:45: Split the timing: `shape_ms` (my Jolt MeshShape
                         build) is now measured separately from `stream_ms`
                         (core's generate + extract inside ChunkManager::update).
                         The old single timer wrapped both and reported
                         worldgen's cost as physics' cost. The per-chunk ceiling
                         now covers only this zone's cost and is an
                         order-of-magnitude guard, not a budget — the measured
                         figure moves 2x with machine load alone.
- 10:08:2026 - 02:44:55: Walk budget 3600 -> 5400 (core's verified fix, landed by the lead with sim resting; the walk is longer since the massif reshape, completing at 4251).
*/

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <vector>

#include <glm/geometric.hpp>

#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/physics/sources/TerrainCollision.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"
#include "engine/world/sources/ChunkManager.h"

namespace {

namespace config = dfn::config;
namespace platform = dfn::platform;
namespace physics_layer = dfn::physics;
namespace world = dfn::world;

constexpr float DT = static_cast<float>(config::SIM_DT);

// The generated world the whole stage is verified against (core's testbed:
// seed 1, 4x4 chunks). The crag tunnel lives in the layout defaults.
struct TunnelRig {
    dfn::ecs::World ecs;
    dfn::events::EventBus bus;
    world::ChunkManager chunks;
    std::unique_ptr<platform::IPhysics> physics = platform::create_jolt_physics();
    std::vector<platform::PhysicsBodyHandle> terrain_bodies;
    std::unordered_map<uint64_t, platform::PhysicsBodyHandle> bodies_by_chunk;

    size_t total_triangles = 0;
    size_t chunk_bodies = 0;
    // Two SEPARATE costs, deliberately not merged: `stream_ms` is core's chunk
    // generation + surface extraction inside ChunkManager::update, `shape_ms`
    // is MY Jolt MeshShape build. Timing them together (as this rig first did)
    // reports worldgen's cost as physics' cost and misdirects optimisation.
    double stream_ms = 0.0;
    double shape_ms = 0.0;
    [[nodiscard]] double settle_ms() const { return stream_ms + shape_ms; }

    explicit TunnelRig(const glm::vec3& focus) {
        REQUIRE(physics->init());
        chunks.open_generated(world::WorldGenParams{1, {0, 0}, {3, 3}},
                              world::ChunkStreamingParams{2, 3});
        settle(focus);
        REQUIRE(chunk_bodies > 0);
    }

    /// Streaming is rate-limited to CHUNK_LOAD_BUDGET chunks per update (core's
    /// fix for multi-second load freezes), so a ring fills over many updates.
    /// Tests that walk on the terrain need the SETTLED world: drive updates —
    /// each building collision for whatever became resident — until residency
    /// stops changing. Same pattern as core's Fixture::settle.
    void settle(const glm::vec3& focus) {
        for (int i = 0; i < 256; ++i) {
            const std::size_t before = chunks.loaded_chunks().size();
            restream(focus);
            if (chunks.loaded_chunks().size() == before) {
                return;
            }
        }
        FAIL("streaming never settled");
    }

    // Re-streams around `focus` exactly as the app ferry does: update the
    // resident set, then rebuild collision so newly resident chunks have bodies
    // and departed ones do not. Keyed by chunk coord so repeats are cheap.
    void restream(const glm::vec3& focus) {
        const auto stream_start = std::chrono::steady_clock::now();
        chunks.update(focus, ecs, bus);
        stream_ms += std::chrono::duration<double, std::milli>(
                         std::chrono::steady_clock::now() - stream_start)
                         .count();
        for (const world::ChunkCoord coord : chunks.loaded_chunks()) {
            const uint64_t key = chunk_key(coord);
            if (bodies_by_chunk.contains(key)) {
                continue; // already collidable
            }
            const auto mesh = chunks.voxel_mesh(coord);
            if (!mesh.has_value()) {
                continue;
            }
            total_triangles += mesh->triangle_count();
            const auto shape_start = std::chrono::steady_clock::now();
            const auto body =
                dfn::physics::create_terrain_mesh_body(*physics, *mesh, key);
            shape_ms += std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - shape_start)
                            .count();
            if (body.valid()) {
                bodies_by_chunk.emplace(key, body);
                ++chunk_bodies;
            }
        }
    }

    static uint64_t chunk_key(world::ChunkCoord coord) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(coord.x)) << 32) |
               static_cast<uint32_t>(coord.z);
    }

    // Ground height from the heightfield — the SURFACE of the mountain. Being
    // below it with rock overhead is what "inside the mountain" means.
    [[nodiscard]] float surface_height(glm::vec2 world_xz) const {
        const world::ChunkCoord coord = world::chunk_at_position(world_xz);
        const auto view = chunks.heightfield(coord);
        if (!view.has_value()) {
            return 0.0f;
        }
        const float fx = (world_xz.x - view->origin.x) / view->step;
        const float fz = (world_xz.y - view->origin.y) / view->step;
        const auto x = static_cast<uint32_t>(
            std::clamp(fx, 0.0f, static_cast<float>(view->resolution - 1)));
        const auto z = static_cast<uint32_t>(
            std::clamp(fz, 0.0f, static_cast<float>(view->resolution - 1)));
        return view->height_at(x, z);
    }
};

platform::CharacterDesc player_desc(const glm::vec3& position) {
    platform::CharacterDesc desc;
    desc.position = position;
    desc.radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
    desc.height = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
    desc.max_slope_radians = static_cast<float>(config::PLAYER_MAX_SLOPE);
    desc.step_height = static_cast<float>(config::PLAYER_STEP_HEIGHT);
    desc.layer = physics_layer::LAYER_CHARACTER;
    desc.collides_with = physics_layer::LAYER_STATIC;
    desc.user_data = 0x9A1Full; // stand-in EntityId bits for the walker
    return desc;
}

TEST_CASE("voxel terrain collision: the extraction resolution is affordable") {
    // Measured, not assumed: the render-resolution mesh is what we collide
    // against, so its cost has to be stated in numbers.
    const dfn::world::TestbedLayout layout{};
    const auto& tunnel = layout.carves.crag_tunnel;
    TunnelRig rig({tunnel.points[0].x, tunnel.points[0].y, tunnel.points[0].z});

    MESSAGE("collision chunks: "
            << rig.chunk_bodies << ", triangles: " << rig.total_triangles
            << " | MY cost (Jolt MeshShape): " << rig.shape_ms << " ms ("
            << (rig.shape_ms / static_cast<double>(rig.chunk_bodies))
            << " ms/chunk) | core's cost (generate + extract): " << rig.stream_ms
            << " ms | settle total: " << rig.settle_ms() << " ms");
    CHECK(rig.total_triangles > 0);
    // The cost that is MINE to defend: Jolt shape building per chunk. Kept
    // separate from streaming on purpose — a ceiling that also covers worldgen
    // fails for reasons this zone cannot fix, and hides the cost it should
    // expose.
    // This is a CATASTROPHE GUARD, not a budget. Measured ~58 ms/chunk on an
    // idle machine and ~105 ms/chunk while parallel builds compete for cores,
    // so anything near the real figure would flake in CI for reasons unrelated
    // to the code. The MESSAGE above carries the actual number for a human to
    // read; this only catches an order-of-magnitude regression.
    CHECK(rig.shape_ms / static_cast<double>(rig.chunk_bodies) < 250.0);
}

TEST_CASE("voxel terrain collision: rock overhead inside the tunnel (overhang)") {
    // The geometric proof that a heightfield-derived body cannot pass: at a
    // deep waypoint there is collision ABOVE the floor, and the floor itself
    // sits below the mountain's surface height.
    const dfn::world::TestbedLayout layout{};
    const auto& tunnel = layout.carves.crag_tunnel;
    const glm::vec3 deep = tunnel.points[3]; // interior leg, well inside the massif
    TunnelRig rig(deep);

    const float surface = rig.surface_height({deep.x, deep.z});
    CHECK(surface > deep.y + tunnel.height); // mountain closes over the corridor

    // Floor beneath the waypoint.
    const auto down = rig.physics->raycast({deep.x, deep.y + 1.0f, deep.z},
                                           {0.0f, -1.0f, 0.0f}, 6.0f,
                                           physics_layer::LAYER_STATIC);
    REQUIRE(down.hit);
    CHECK(down.position.y == doctest::Approx(deep.y).epsilon(0.35));

    // Ceiling above it — this is the overhang a heightfield cannot represent.
    const auto up = rig.physics->raycast({deep.x, deep.y + 0.5f, deep.z},
                                         {0.0f, 1.0f, 0.0f}, 40.0f,
                                         physics_layer::LAYER_STATIC);
    REQUIRE(up.hit);
    CHECK(up.distance < 20.0f); // rock, not open sky
    // Headroom must clear a standing player.
    CHECK(up.position.y - down.position.y >
          static_cast<float>(config::PLAYER_CAPSULE_HEIGHT));
}

TEST_CASE("voxel terrain collision: the player walks THROUGH the crag, not over it") {
    // The acceptance walk: enter at the mouth, steer waypoint to waypoint, and
    // stay under the mountain. With heightfield-derived collision the capsule
    // rides the surface and this test cannot pass.
    const dfn::world::TestbedLayout layout{};
    const auto& tunnel = layout.carves.crag_tunnel;
    REQUIRE(tunnel.point_count >= 4);

    const glm::vec3 mouth = tunnel.points[0];
    TunnelRig rig(mouth);
    const auto character =
        rig.physics->create_character(player_desc({mouth.x, mouth.y + 0.5f, mouth.z}));
    REQUIRE(character.valid());

    int waypoint = 1;
    float travelled = 0.0f;
    float deepest_cover = 0.0f; // most rock ever recorded over the player's head
    int ticks_under_rock = 0;
    float vertical_velocity = 0.0f;
    glm::vec3 previous = rig.physics->character_position(character);

    // Steering: follow the corridor CENTERLINE with a look-ahead, the way a
    // player walks a passage. Beelining at the next waypoint instead pushes the
    // capsule into the outer wall at a switchback, where it can wedge on the
    // voxel wall's bumps — measured, and the reason this loop is written this
    // way (see the report: nudged back to the centerline it walks freely).
    // 5400: the walk is genuinely longer since the §2.8 massif reshape (the
    // instrumented probe completes at tick 4251); at 3600 the budget expired
    // mid-climb with a transient airborne frame. Verified at 7200 by core.
    for (int tick = 0; tick < 5400 && waypoint < tunnel.point_count; ++tick) {
        const glm::vec3 position = rig.physics->character_position(character);
        const glm::vec3 leg_start = tunnel.points[waypoint - 1];
        const glm::vec3 leg_end = tunnel.points[waypoint];
        const glm::vec3 leg = leg_end - leg_start;
        const float leg_length_sq = glm::dot(leg, leg);

        // How far along this leg the capsule currently is.
        const float t = leg_length_sq > 0.0f
                            ? glm::clamp(glm::dot(position - leg_start, leg) /
                                             leg_length_sq,
                                         0.0f, 1.0f)
                            : 1.0f;
        if (glm::length(glm::vec2(leg_end.x - position.x, leg_end.z - position.z)) <
            1.5f) {
            ++waypoint; // reached this leg's end, steer down the next
            continue;
        }

        // Aim 2 m further along the centerline, so the capsule stays centred
        // instead of cutting the corner into the wall.
        const float look_ahead = leg_length_sq > 0.0f
                                     ? std::min(1.0f, t + 2.0f / std::sqrt(leg_length_sq))
                                     : 1.0f;
        const glm::vec3 aim = leg_start + leg * look_ahead;
        const glm::vec2 to_target{aim.x - position.x, aim.z - position.z};
        if (glm::length(to_target) < 0.01f) {
            ++waypoint;
            continue;
        }

        const glm::vec2 direction = glm::normalize(to_target);
        const float speed = static_cast<float>(config::WALK_SPEED);
        vertical_velocity -= static_cast<float>(config::GRAVITY) * DT;
        rig.physics->move_character(character,
                                    {direction.x * speed * DT,
                                     vertical_velocity * DT,
                                     direction.y * speed * DT});
        rig.physics->step(DT);
        if (rig.physics->character_grounded(character) && vertical_velocity < 0.0f) {
            vertical_velocity = 0.0f;
        }

        const glm::vec3 moved = rig.physics->character_position(character);
        travelled += glm::length(glm::vec2(moved.x - previous.x, moved.z - previous.z));
        previous = moved;

        // "Inside the mountain": the surface is above the player's head.
        const float head = moved.y + static_cast<float>(config::PLAYER_EYE_HEIGHT);
        const float cover = rig.surface_height({moved.x, moved.z}) - head;
        if (cover > 0.0f) {
            ++ticks_under_rock;
            deepest_cover = std::max(deepest_cover, cover);
        }
    }

    MESSAGE("travelled " << travelled << " m, reached waypoint " << waypoint << "/"
                         << tunnel.point_count << ", ticks under rock "
                         << ticks_under_rock << ", deepest cover " << deepest_cover
                         << " m");

    CHECK(travelled > 100.0f);        // a traverse, not a doorway
    CHECK(ticks_under_rock > 600);    // most of the walk happens inside the massif
    CHECK(deepest_cover > 5.0f);      // genuinely under the crag, not in a cutting
    CHECK(rig.physics->character_grounded(character)); // on the floor, not falling
}

// Sprints a capsule from `start` along `direction` for `ticks`, returning the
// number of ticks in which it passed THROUGH static geometry. Detection is
// exact rather than heuristic: each tick, ray-cast the movement segment; if the
// segment hits a wall yet the capsule ended past that hit (beyond its own
// radius of penetration tolerance), collide-and-slide let it tunnel.
[[nodiscard]] int count_tunnelling_ticks(TunnelRig& rig, const glm::vec3& start,
                                         const glm::vec3& direction, int ticks) {
    const auto character = rig.physics->create_character(player_desc(start));
    REQUIRE(character.valid());
    const float speed =
        static_cast<float>(config::RUN_SPEED * config::DEBUG_SPRINT_MULTIPLIER);
    const float radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);

    int tunnelled = 0;
    for (int tick = 0; tick < ticks; ++tick) {
        const glm::vec3 before = rig.physics->character_position(character);
        rig.physics->move_character(character, direction * speed * DT);
        rig.physics->step(DT);
        const glm::vec3 after = rig.physics->character_position(character);

        const glm::vec3 delta = after - before;
        const float distance = glm::length(delta);
        if (distance < 1e-4f) {
            continue; // blocked outright: the correct outcome
        }
        // Cast from chest height so floor contact is not mistaken for a wall.
        const glm::vec3 chest{0.0f, 0.9f, 0.0f};
        const auto hit = rig.physics->raycast(before + chest, delta / distance, distance,
                                              physics_layer::LAYER_STATIC);
        if (hit.hit && distance > hit.distance + radius) {
            ++tunnelled;
        }
    }
    rig.physics->destroy_character(character);
    return tunnelled;
}

TEST_CASE("DEBUG sprint: 30 m/s does not tunnel through tunnel walls") {
    // 0.5 m of travel per fixed tick against corridor walls only 2 m from the
    // centerline. If collide-and-slide misses, the sprinter leaves the mountain.
    const dfn::world::TestbedLayout layout{};
    const auto& tunnel = layout.carves.crag_tunnel;
    const glm::vec3 deep = tunnel.points[3];
    TunnelRig rig(deep);

    const glm::vec3 start{deep.x, deep.y + 0.5f, deep.z};
    int tunnelled = 0;
    // Sweep every horizontal direction: switchback legs stack, so some headings
    // face a thin wall with open corridor on the far side — the worst case.
    for (int i = 0; i < 16; ++i) {
        const float angle = 6.28318530718f * static_cast<float>(i) / 16.0f;
        tunnelled += count_tunnelling_ticks(
            rig, start, {std::cos(angle), 0.0f, std::sin(angle)}, 120);
    }
    CHECK(tunnelled == 0);
}

TEST_CASE("DEBUG sprint: 30 m/s does not tunnel through the castle curtain wall") {
    // The other thin vertical geometry the user can run at head-on.
    const dfn::world::TestbedLayout layout{};
    const glm::vec2 castle = layout.castle.center;
    const glm::vec3 focus{castle.x, 0.0f, castle.y};
    TunnelRig rig(focus);

    const float ground = rig.surface_height(castle);
    int tunnelled = 0;
    // Charge the pad from eight compass points, starting well outside it.
    for (int i = 0; i < 8; ++i) {
        const float angle = 6.28318530718f * static_cast<float>(i) / 8.0f;
        const glm::vec2 offset{std::cos(angle) * 60.0f, std::sin(angle) * 60.0f};
        const glm::vec2 from = castle + offset;
        const glm::vec3 start{from.x, rig.surface_height(from) + 1.0f, from.y};
        const glm::vec3 toward = glm::normalize(glm::vec3{-offset.x, 0.0f, -offset.y});
        tunnelled += count_tunnelling_ticks(rig, start, toward, 180);
    }
    MESSAGE("castle ground height " << ground << " m");
    CHECK(tunnelled == 0);
}

TEST_CASE("DEBUG sprint: no fall-through while chunks are still streaming") {
    // The failure mode that bit us before: at 30 m/s a chunk boundary arrives
    // every 8.5 s while terrain shape build takes ~68 ms/chunk. The player must
    // never end up below the terrain surface, mid-stream or not.
    // Route stays INSIDE the generated extent (4x4 chunks = 0..1024 m): from
    // x=200 east to x=800 crosses three chunk boundaries in 20 s. Running out
    // of the world is a separate concern (see the world-edge case below), not
    // a streaming failure.
    const glm::vec2 start_xz{200.0f, 430.0f};
    TunnelRig rig({start_xz.x, 0.0f, start_xz.y});

    const glm::vec3 start{start_xz.x, rig.surface_height(start_xz) + 1.0f, start_xz.y};
    const auto character = rig.physics->create_character(player_desc(start));
    REQUIRE(character.valid());

    const float speed =
        static_cast<float>(config::RUN_SPEED * config::DEBUG_SPRINT_MULTIPLIER);
    float vertical_velocity = 0.0f;
    int below_surface_ticks = 0;
    float worst_depth = 0.0f;

    // Sprint due east across chunk boundaries, restreaming as the app would.
    for (int tick = 0; tick < 1200; ++tick) {
        const glm::vec3 position = rig.physics->character_position(character);
        if (tick % 30 == 0) {
            rig.restream(position); // app-side ferry: load/unload + bodies
        }
        vertical_velocity -= static_cast<float>(config::GRAVITY) * DT;
        rig.physics->move_character(
            character, {speed * DT, vertical_velocity * DT, 0.0f});
        rig.physics->step(DT);
        if (rig.physics->character_grounded(character) && vertical_velocity < 0.0f) {
            vertical_velocity = 0.0f;
        }

        const glm::vec3 moved = rig.physics->character_position(character);
        // Open valley: the capsule must stay at or above the ground surface.
        const float depth = rig.surface_height({moved.x, moved.z}) - moved.y;
        if (depth > 1.0f) { // 1 m tolerance for heightfield-vs-mesh sampling
            ++below_surface_ticks;
            worst_depth = std::max(worst_depth, depth);
        }
    }

    const glm::vec3 finish = rig.physics->character_position(character);
    MESSAGE("sprinted to x=" << finish.x << ", ticks below surface: "
                             << below_surface_ticks << ", worst depth " << worst_depth
                             << " m");
    CHECK(finish.x > start.x + 500.0f); // actually crossed three chunk boundaries
    CHECK(below_surface_ticks == 0);
    CHECK(finish.y > -50.0f); // not falling forever
}

TEST_CASE("DEBUG sprint: the world edge is reachable in seconds (known limit)") {
    // Consequence of the debug sprint, recorded rather than hidden: 30 m/s
    // reaches the edge of the 1024 m generated extent in well under a minute,
    // and past the last chunk there is no terrain, so the player falls. This is
    // a world-extent/app concern (a boundary or a bigger world), NOT a physics
    // or streaming defect — the test documents it so the grill can decide.
    const glm::vec2 start_xz{200.0f, 430.0f};
    TunnelRig rig({start_xz.x, 0.0f, start_xz.y});
    const glm::vec3 start{start_xz.x, rig.surface_height(start_xz) + 1.0f, start_xz.y};
    const auto character = rig.physics->create_character(player_desc(start));
    REQUIRE(character.valid());

    const float speed =
        static_cast<float>(config::RUN_SPEED * config::DEBUG_SPRINT_MULTIPLIER);
    float vertical_velocity = 0.0f;
    for (int tick = 0; tick < 1200; ++tick) { // 20 s due WEST, off the extent
        if (tick % 30 == 0) {
            rig.restream(rig.physics->character_position(character));
        }
        vertical_velocity -= static_cast<float>(config::GRAVITY) * DT;
        rig.physics->move_character(character,
                                    {-speed * DT, vertical_velocity * DT, 0.0f});
        rig.physics->step(DT);
        if (rig.physics->character_grounded(character) && vertical_velocity < 0.0f) {
            vertical_velocity = 0.0f;
        }
    }

    const glm::vec3 finish = rig.physics->character_position(character);
    MESSAGE("off-extent sprint ended at x=" << finish.x << ", y=" << finish.y);
    CHECK(finish.x < 0.0f);  // left the generated world
    CHECK(finish.y < -50.0f); // and fell, because there is nothing out there
}

} // namespace
