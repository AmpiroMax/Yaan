/*
Created: 09:08:2026 - 16:51:22
Last updated: 09:08:2026 - 16:51:22
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
*/

#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <memory>
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

    size_t total_triangles = 0;
    size_t chunk_bodies = 0;
    double shape_build_ms = 0.0;

    explicit TunnelRig(const glm::vec3& focus) {
        REQUIRE(physics->init());
        chunks.open_generated(world::WorldGenParams{1, {0, 0}, {3, 3}},
                              world::ChunkStreamingParams{2, 3});
        chunks.update(focus, ecs, bus);

        // One static mesh body per resident chunk — the ferry the app performs.
        const auto start = std::chrono::steady_clock::now();
        for (const world::ChunkCoord coord : chunks.loaded_chunks()) {
            const auto mesh = chunks.voxel_mesh(coord);
            if (!mesh.has_value()) {
                continue;
            }
            total_triangles += mesh->triangle_count();
            const auto body = dfn::physics::create_terrain_mesh_body(
                *physics, *mesh, static_cast<uint64_t>(chunks.is_loaded(coord)));
            if (body.valid()) {
                terrain_bodies.push_back(body);
                ++chunk_bodies;
            }
        }
        shape_build_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                      start)
                .count();
        REQUIRE(chunk_bodies > 0);
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

    MESSAGE("collision chunks: " << rig.chunk_bodies
                                 << ", triangles: " << rig.total_triangles
                                 << ", shape build: " << rig.shape_build_ms << " ms");
    CHECK(rig.total_triangles > 0);
    // Budget sanity: building every resident chunk's collision must stay in the
    // "chunk load hitch" class, not the "the game stalls" class.
    CHECK(rig.shape_build_ms < 2000.0);
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

    // 40 simulated seconds is ample for a 158 m corridor at walking pace.
    for (int tick = 0; tick < 2400 && waypoint < tunnel.point_count; ++tick) {
        const glm::vec3 position = rig.physics->character_position(character);
        const glm::vec3 target = tunnel.points[waypoint];
        const glm::vec2 to_target{target.x - position.x, target.z - position.z};
        if (glm::length(to_target) < 1.5f) {
            ++waypoint; // reached this leg's end, steer to the next
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

} // namespace
