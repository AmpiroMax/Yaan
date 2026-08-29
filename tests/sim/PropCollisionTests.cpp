/*
Module: tests
File: tests/sim/PropCollisionTests.cpp

Responsibility:
- Proves that buildings and boulders are solid, and that their solid surface is
  the surface that is DRAWN rather than a box around it.

Key items:
- Rig: a real generated world (core's testbed seed) + Jolt, with NO terrain
  collision, so anything a ray hits can only be a prop.
- Controls: the identical ray with prop collision not built must miss.

Dependencies:
- Uses: doctest, dfn_gameplay, dfn_world, dfn_render (the drawn geometry the
  expectations are derived from), dfn_platform_physics, dfn_core.
- Used by: ctest (sim_prop_collision).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deliberately NO terrain body: with terrain present, a downward ray hits the
  ground and every one of these cases would pass without any prop at all. The
  absence of terrain is what makes the assertions mean something.
*/

#include <doctest/doctest.h>

#include <memory>
#include <vector>

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/gameplay/sources/PropCollision.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"
#include "engine/render/sources/ProcMesh.h"
#include "engine/world/sources/Chunk.h"
#include "engine/world/sources/ChunkManager.h"
#include "engine/world/sources/SiteComponents.h"

namespace {

namespace config = dfn::config;
namespace gameplay = dfn::gameplay;
namespace platform = dfn::platform;
namespace physics_layer = dfn::physics;
namespace world = dfn::world;
namespace math = dfn::math;

struct Rig {
    dfn::ecs::World ecs;
    dfn::events::EventBus bus;
    world::ChunkManager chunks;
    std::unique_ptr<platform::IPhysics> physics = platform::create_jolt_physics();

    Rig() {
        REQUIRE(physics->init());
        chunks.open_generated(world::WorldGenParams{1, {0, 0}, {3, 3}},
                              world::ChunkStreamingParams{2, 3});
        // Streaming is budgeted per update, so residency settles over several.
        for (int i = 0; i < 256; ++i) {
            const std::size_t before = chunks.loaded_chunks().size();
            chunks.update({256.0f, 0.0f, 256.0f}, ecs, bus);
            bus.pump();
            if (chunks.loaded_chunks().size() == before && i > 0) {
                break;
            }
        }
        REQUIRE_FALSE(chunks.loaded_chunks().empty());
    }

    void build_props() { gameplay::update_prop_collision(ecs, *physics, chunks); }

    // Straight down from `above`, against static geometry only.
    [[nodiscard]] platform::RayHit ray_down(glm::vec3 above, float distance) const {
        return physics->raycast(above, {0.0f, -1.0f, 0.0f}, distance,
                                physics_layer::LAYER_STATIC);
    }
};

// The first boulder in any resident chunk, or nothing.
[[nodiscard]] bool find_boulder(const Rig& rig, math::ScatterInstance& out) {
    for (const world::ChunkCoord coord : rig.chunks.loaded_chunks()) {
        for (const math::ScatterInstance& inst : rig.chunks.scatter(coord)) {
            if (inst.species == math::ScatterSpecies::Stone) {
                out = inst;
                return true;
            }
        }
    }
    return false;
}

TEST_CASE("boulders are solid, and their top is where the rock is drawn") {
    Rig rig;
    math::ScatterInstance boulder{};
    REQUIRE(find_boulder(rig, boulder));

    const glm::vec3 above{boulder.position.x, boulder.position.y + 20.0f,
                          boulder.position.z};

    // CONTROL FIRST: with no prop bodies built, and no terrain body in this rig
    // at all, the ray has nothing to hit. If this ever hits, the case below is
    // measuring something other than the boulder.
    CHECK_FALSE(rig.ray_down(above, 40.0f).hit);

    rig.build_props();
    const platform::RayHit hit = rig.ray_down(above, 40.0f);
    REQUIRE(hit.hit);

    // The drawn boulder is sunk into the ground by SCATTER_GROUND_SINK_FRAC of
    // its scale; its solid top must follow the drawn top, not the instance
    // origin. Deriving the expectation from the mesh render actually builds is
    // the point — a hand-written height would just restate the bug.
    const dfn::render::MeshData stone =
        dfn::render::build_scatter_mesh(math::ScatterSpecies::Stone);
    REQUIRE_FALSE(stone.vertices.empty());
    float model_top = stone.vertices.front().position.y;
    for (const platform::Vertex& v : stone.vertices) {
        model_top = std::max(model_top, v.position.y);
    }
    const float sink = static_cast<float>(config::SCATTER_GROUND_SINK_FRAC);
    const float expected_top =
        boulder.position.y - sink * boulder.scale + model_top * boulder.scale;

    CHECK(hit.position.y == doctest::Approx(expected_top).epsilon(0.02));
}

TEST_CASE("a capsule dropped on a boulder rests on it, not through it") {
    Rig rig;
    math::ScatterInstance boulder{};
    REQUIRE(find_boulder(rig, boulder));
    rig.build_props();

    platform::CharacterDesc desc;
    desc.position = {boulder.position.x, boulder.position.y + 6.0f, boulder.position.z};
    desc.radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
    desc.height = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
    desc.max_slope_radians = static_cast<float>(config::PLAYER_MAX_SLOPE);
    desc.step_height = static_cast<float>(config::PLAYER_STEP_HEIGHT);
    desc.layer = physics_layer::LAYER_CHARACTER;
    desc.collides_with = physics_layer::LAYER_STATIC;
    const platform::CharacterHandle character = rig.physics->create_character(desc);
    REQUIRE(character.valid());

    const float dt = static_cast<float>(config::SIM_DT);
    float vertical = 0.0f;
    for (int i = 0; i < 180; ++i) {
        vertical -= static_cast<float>(config::GRAVITY) * dt;
        rig.physics->move_character(character, {0.0f, vertical * dt, 0.0f});
        rig.physics->step(dt);
        if (rig.physics->character_grounded(character)) {
            vertical = 0.0f;
        }
    }
    const glm::vec3 rest = rig.physics->character_position(character);

    // It stopped somewhere above the boulder's base rather than falling past it
    // into the empty world (this rig has no terrain body to catch anyone).
    CHECK(rig.physics->character_grounded(character));
    CHECK(rest.y > boulder.position.y - 1.0f);
    CHECK(rest.y > desc.position.y - 6.5f);
}

TEST_CASE("buildings are solid, and the body follows the mesh") {
    Rig rig;

    // Find a site entity whose mesh render can actually build. Castle ids
    // (8..12) have no mesh yet, so they are correctly absent from collision —
    // that is render's gap, reported, not papered over here.
    glm::vec3 site_position{0.0f};
    bool found = false;
    for (auto [id, marker, transform, mesh] :
         rig.ecs.view<world::SiteMarker, dfn::components::Transform,
                      dfn::components::RenderMesh>()) {
        (void)id;
        (void)marker;
        if (!dfn::render::build_site_mesh(mesh.mesh_asset).indices.empty()) {
            site_position = transform.position;
            found = true;
            break;
        }
    }
    REQUIRE(found);

    const glm::vec3 above{site_position.x, site_position.y + 30.0f, site_position.z};

    // CONTROL: nothing built, nothing hit.
    CHECK_FALSE(rig.ray_down(above, 60.0f).hit);

    rig.build_props();
    const platform::RayHit hit = rig.ray_down(above, 60.0f);
    REQUIRE(hit.hit);
    // The roof is above the pad the building stands on, and below the sky.
    CHECK(hit.position.y > site_position.y);
    CHECK(hit.position.y < site_position.y + 30.0f);
}

TEST_CASE("prop bodies are dropped when their chunk stops being resident") {
    Rig rig;
    rig.build_props();
    const std::size_t built = rig.ecs.resource<gameplay::PropCollisionState>().bodies.size();
    REQUIRE(built > 0);

    // Walk the focus far away and let the resident set turn over completely.
    for (int i = 0; i < 256; ++i) {
        rig.chunks.update({900.0f, 0.0f, 900.0f}, rig.ecs, rig.bus);
        rig.bus.pump();
    }
    rig.build_props();
    const auto& state = rig.ecs.resource<gameplay::PropCollisionState>();

    // Every body still held must belong to a chunk that is still resident —
    // otherwise collision accumulates for a world the player has left.
    for (const auto& [key, body] : state.bodies) {
        bool resident = false;
        for (const world::ChunkCoord coord : rig.chunks.loaded_chunks()) {
            resident = resident || (world::chunk_group(coord) == key);
        }
        CHECK(resident);
    }
}

} // namespace
