/*
Created: 09:08:2026 - 00:45:08
Last updated: 09:08:2026 - 15:08:24
Module: tests
File: tests/sim/JoltPhysicsTests.cpp

Responsibility:
- Jolt backend integration tests: terrain body from a HeightFieldView (the
  frozen uint16 decode), character falls to ground and stands, walks on
  terrain, slides against walls, masked raycasts resolve user_data.

Key items:
- make_flat_view(): a synthetic 129x129 HeightFieldView at a known height.
- Doctest cases through IPhysics only (no Jolt types — Rule 1 holds in tests too).

Dependencies:
- Uses: doctest, dfn_platform_physics (jolt factory), dfn_physics, constants.
- Used by: ctest (sim_jolt_physics).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Never include Jolt headers here; the public contract must be enough.
*/
/*
UPD:
- 09:08:2026 - 00:45:08: Stage 2 — initial Jolt backend suite.
- 09:08:2026 - 01:02:15: Added the real-ChunkManager heightfield smoke test
  (core's suggestion: catches decode/contract drift between zones early).
- 09:08:2026 - 15:08:24: Added the zero-mask rejection case per body kind
  (regression guard for the fall-through-the-world bug, app fix 37f1e1c).
*/

#include <doctest/doctest.h>

#include <memory>
#include <vector>

#include "engine/core/config/sources/Constants.h"
#include "engine/core/math/sources/HeightField.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/physics/sources/TerrainCollision.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"
#include "engine/world/sources/ChunkManager.h"

namespace {

namespace config = dfn::config;
namespace physics_layer = dfn::physics;
namespace platform = dfn::platform;

constexpr float DT = static_cast<float>(config::SIM_DT);
constexpr float GROUND_Y = 10.0f;
constexpr uint64_t TERRAIN_USER_DATA = 0xC0FFEEull;

// Flat 129x129 chunk at GROUND_Y meters: raw = 0 everywhere, offset carries
// the height (the frozen formula: height = offset + raw * scale).
struct FlatChunk {
    std::vector<uint16_t> raw;
    dfn::math::HeightFieldView view;

    FlatChunk() {
        const auto resolution = static_cast<uint32_t>(config::HEIGHTMAP_RESOLUTION);
        raw.assign(static_cast<size_t>(resolution) * resolution, 0);
        view.chunk_coord = {0, 0};
        view.origin = {0.0f, 0.0f};
        view.resolution = resolution;
        view.step = static_cast<float>(config::HEIGHTMAP_STEP);
        view.heights = raw;
        view.height_scale = 0.0f; // flat: every sample decodes to the offset
        view.height_offset = GROUND_Y;
    }
};

struct JoltRig {
    std::unique_ptr<platform::IPhysics> physics = platform::create_jolt_physics();
    FlatChunk chunk;
    std::vector<float> scratch;
    platform::PhysicsBodyHandle terrain;
    platform::CharacterHandle character;
    float vertical_velocity = 0.0f;

    JoltRig() {
        REQUIRE(physics->init());
        terrain = physics_layer::create_terrain_body(*physics, chunk.view,
                                                     TERRAIN_USER_DATA, scratch);
        REQUIRE(terrain.valid());

        platform::CharacterDesc desc;
        desc.position = {128.0f, GROUND_Y + 2.0f, 128.0f}; // chunk center, 2 m up
        desc.radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
        desc.height = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
        desc.max_slope_radians = static_cast<float>(config::PLAYER_MAX_SLOPE);
        desc.step_height = static_cast<float>(config::PLAYER_STEP_HEIGHT);
        desc.layer = physics_layer::LAYER_CHARACTER;
        desc.collides_with = physics_layer::LAYER_STATIC;
        desc.user_data = 0xAB1EEull;
        character = physics->create_character(desc);
        REQUIRE(character.valid());
    }

    // One fixed tick with gravity, optionally with a horizontal intent.
    void tick(const glm::vec3& horizontal = {0.0f, 0.0f, 0.0f}) {
        vertical_velocity -= static_cast<float>(config::GRAVITY) * DT;
        physics->move_character(character,
                                horizontal + glm::vec3{0.0f, vertical_velocity * DT, 0.0f});
        physics->step(DT);
        if (physics->character_grounded(character) && vertical_velocity < 0.0f) {
            vertical_velocity = 0.0f;
        }
    }
};

TEST_CASE("character falls onto heightfield terrain and stands on it") {
    JoltRig rig;
    for (int i = 0; i < 180; ++i) { // 3 seconds is plenty for a 2 m drop
        rig.tick();
    }
    CHECK(rig.physics->character_grounded(rig.character));
    CHECK(rig.physics->character_position(rig.character).y ==
          doctest::Approx(GROUND_Y).epsilon(0.02));
}

TEST_CASE("grounded character walks without sinking or launching") {
    JoltRig rig;
    for (int i = 0; i < 180; ++i) {
        rig.tick();
    }
    REQUIRE(rig.physics->character_grounded(rig.character));

    const glm::vec3 start = rig.physics->character_position(rig.character);
    const float step = static_cast<float>(config::WALK_SPEED) * DT;
    for (int i = 0; i < 60; ++i) { // one second east
        rig.tick({step, 0.0f, 0.0f});
    }
    const glm::vec3 end = rig.physics->character_position(rig.character);
    CHECK(end.x - start.x ==
          doctest::Approx(static_cast<float>(config::WALK_SPEED)).epsilon(0.05));
    CHECK(end.y == doctest::Approx(GROUND_Y).epsilon(0.02));
    CHECK(rig.physics->character_grounded(rig.character));
}

TEST_CASE("walls block and slide the character") {
    JoltRig rig;
    for (int i = 0; i < 180; ++i) {
        rig.tick();
    }
    const glm::vec3 start = rig.physics->character_position(rig.character);

    platform::StaticBoxDesc wall;
    wall.center = {start.x + 1.5f, GROUND_Y + 2.0f, start.z};
    wall.half_extents = {0.25f, 2.0f, 10.0f}; // wall across +X, long in Z
    wall.layer = physics_layer::LAYER_STATIC;
    wall.user_data = 0xBA11ull;
    REQUIRE(rig.physics->create_static_box(wall).valid());

    const float step = static_cast<float>(config::WALK_SPEED) * DT;
    for (int i = 0; i < 120; ++i) { // two seconds straight into the wall
        rig.tick({step, 0.0f, 0.0f});
    }
    const glm::vec3 end = rig.physics->character_position(rig.character);
    // Blocked before the wall face (allow the capsule radius + padding).
    CHECK(end.x < wall.center.x);
    CHECK(end.x - start.x < 1.5f);
    // And walking diagonally along the wall still slides in Z.
    for (int i = 0; i < 60; ++i) {
        rig.tick({step, 0.0f, step});
    }
    CHECK(rig.physics->character_position(rig.character).z > end.z + 0.5f);
}

TEST_CASE("raycast hits terrain with user_data, respects the mask") {
    JoltRig rig;
    const glm::vec3 origin{64.0f, GROUND_Y + 20.0f, 64.0f};
    const glm::vec3 down{0.0f, -1.0f, 0.0f};

    const auto hit =
        rig.physics->raycast(origin, down, 100.0f, physics_layer::LAYER_STATIC);
    REQUIRE(hit.hit);
    CHECK(hit.position.y == doctest::Approx(GROUND_Y).epsilon(0.01));
    CHECK(hit.distance == doctest::Approx(20.0f).epsilon(0.01));
    CHECK(hit.normal.y == doctest::Approx(1.0f).epsilon(0.01));
    CHECK(hit.user_data == TERRAIN_USER_DATA);

    // A mask excluding static geometry must miss the terrain.
    const auto miss =
        rig.physics->raycast(origin, down, 100.0f, physics_layer::LAYER_CHARACTER);
    CHECK_FALSE(miss.hit);
}

TEST_CASE("raycast finds the character's body via its mask and user_data") {
    JoltRig rig;
    for (int i = 0; i < 180; ++i) {
        rig.tick();
    }
    const glm::vec3 position = rig.physics->character_position(rig.character);
    const glm::vec3 origin = position + glm::vec3{0.0f, 0.9f, -3.0f};
    const auto hit = rig.physics->raycast(origin, {0.0f, 0.0f, 1.0f}, 10.0f,
                                          physics_layer::LAYER_CHARACTER);
    REQUIRE(hit.hit);
    CHECK(hit.user_data == 0xAB1EEull);
}

TEST_CASE("real generated heightfield: terrain body + raycast agree with the view") {
    // Cross-zone smoke test (core's suggestion): a genuine ChunkManager view,
    // not a synthetic one — catches decode-formula drift between zones.
    dfn::ecs::World ecs;
    dfn::events::EventBus bus;
    dfn::world::ChunkManager chunks;
    chunks.open_generated({.seed = 7, .min_chunk = {-1, -1}, .max_chunk = {1, 1}},
                          {.load_radius = 1, .unload_radius = 2});
    chunks.update({0.0f, 0.0f, 0.0f}, ecs, bus);

    const auto view = chunks.heightfield({0, 0});
    REQUIRE(view.has_value());
    REQUIRE(view->resolution ==
            static_cast<uint32_t>(config::HEIGHTMAP_RESOLUTION));

    auto physics = platform::create_jolt_physics();
    REQUIRE(physics->init());
    std::vector<float> scratch;
    const auto terrain =
        dfn::physics::create_terrain_body(*physics, *view, 0xC1A55ull, scratch);
    REQUIRE(terrain.valid());

    // Ray down over the chunk center must hit exactly the decoded height.
    const uint32_t center = view->resolution / 2;
    const float expected = view->height_at(center, center);
    const glm::vec3 over{view->origin.x + static_cast<float>(center) * view->step,
                         expected + 50.0f,
                         view->origin.y + static_cast<float>(center) * view->step};
    const auto hit = physics->raycast(over, {0.0f, -1.0f, 0.0f}, 100.0f,
                                      physics_layer::LAYER_STATIC);
    REQUIRE(hit.hit);
    CHECK(hit.position.y == doctest::Approx(expected).epsilon(0.01));
    CHECK(hit.user_data == 0xC1A55ull);
    physics->shutdown();
}

TEST_CASE("zero-mask bodies are rejected per body kind") {
    // Regression guard: a hand-filled TerrainDesc left `layer` at 0, the body
    // collided with nothing, and the player fell through the world. A collider
    // no mask can select is never intentional (IPhysics.h contract).
    auto physics = platform::create_jolt_physics();
    REQUIRE(physics->init());

    FlatChunk chunk;
    std::vector<float> heights(chunk.raw.size(), GROUND_Y);
    platform::TerrainDesc terrain;
    terrain.sample_count_x = chunk.view.resolution;
    terrain.sample_count_z = chunk.view.resolution;
    terrain.sample_spacing = chunk.view.step;
    terrain.heights = heights;
    terrain.user_data = 1; // layer left at 0 — the exact bug
    CHECK_FALSE(physics->create_terrain(terrain).valid());
    terrain.layer = physics_layer::LAYER_STATIC; // same desc, now well-formed
    CHECK(physics->create_terrain(terrain).valid());

    platform::StaticBoxDesc box;
    box.half_extents = {1.0f, 1.0f, 1.0f};
    CHECK_FALSE(physics->create_static_box(box).valid()); // layer 0
    box.layer = physics_layer::LAYER_STATIC;
    CHECK(physics->create_static_box(box).valid());

    platform::CharacterDesc character;
    character.radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
    character.height = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
    character.collides_with = physics_layer::LAYER_STATIC;
    CHECK_FALSE(physics->create_character(character).valid()); // layer 0
    character.layer = physics_layer::LAYER_CHARACTER;
    character.collides_with = 0; // would walk through the world
    CHECK_FALSE(physics->create_character(character).valid());
    character.collides_with = physics_layer::LAYER_STATIC;
    CHECK(physics->create_character(character).valid());
    physics->shutdown();
}

TEST_CASE("teleport relocates without residual velocity") {
    JoltRig rig;
    rig.physics->teleport_character(rig.character, {10.0f, GROUND_Y, 10.0f});
    rig.tick();
    const glm::vec3 position = rig.physics->character_position(rig.character);
    CHECK(position.x == doctest::Approx(10.0f).epsilon(0.01));
    CHECK(position.z == doctest::Approx(10.0f).epsilon(0.01));
}

} // namespace
