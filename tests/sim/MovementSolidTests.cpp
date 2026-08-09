/*
Created: 09:08:2026 - 22:18:17
Last updated: 09:08:2026 - 22:18:17
Module: tests
File: tests/sim/MovementSolidTests.cpp

Responsibility:
- The half of jump/crouch/swim that only real collision can answer: how high a
  jump actually goes, whether a ceiling refuses a stand-up, and whether jumping
  repeals the slope limit that makes cliffs impassable by geometry.

Key items:
- Rig: player + Jolt backend + a flat floor, ticking the real movement path.
- ramp_box(): a static box whose TOP FACE is a slope of a chosen angle passing
  through the world origin, so a player can be started standing on it.
- The cliff invariant (the lead's standing question) with its control.

Dependencies:
- Uses: doctest, dfn_gameplay, dfn_physics, dfn_platform_physics (jolt),
  generated constants.
- Used by: ctest (sim_movement_solid).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every case here ships a CONTROL: the thing it exists to reject must fail it.
  A slope test that no slope can fail measures nothing.
- Expectations derive from dfn::config, never from literal duplicates of it.
*/
/*
UPD:
- 09:08:2026 - 22:18:17: Created with the v1 movement work — jump apex, the
                         ceiling-refused stand-up, and the cliff invariant.
*/

#include <doctest/doctest.h>

#include <cmath>
#include <memory>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include "engine/core/config/sources/Constants.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"

namespace {

namespace config = dfn::config;
namespace gameplay = dfn::gameplay;
namespace platform = dfn::platform;
namespace physics_layer = dfn::physics;
using dfn::components::CameraPose;
using dfn::components::PreviousCameraPose;
using dfn::components::PreviousTransform;
using dfn::components::Transform;

constexpr float DT = static_cast<float>(config::SIM_DT);

// Yaw that faces +Z (forward = (sin yaw, 0, -cos yaw), so yaw = pi).
const float YAW_TOWARD_PLUS_Z = 3.14159265f;

struct Rig {
    std::unique_ptr<platform::IPhysics> physics = platform::create_jolt_physics();
    gameplay::PlayerState state;
    Transform transform;
    PreviousTransform prev_transform;
    CameraPose camera;
    PreviousCameraPose prev_camera;

    Rig() { REQUIRE(physics->init()); }

    void add_floor(float top_y) {
        platform::StaticBoxDesc box;
        box.center = {0.0f, top_y - 5.0f, 0.0f};
        box.half_extents = {80.0f, 5.0f, 80.0f};
        box.layer = physics_layer::LAYER_STATIC;
        REQUIRE(physics->create_static_box(box).valid());
    }

    // A static box whose top face is a plane of `slope` radians passing through
    // the world origin and rising toward +Z. Derived rather than hand-placed so
    // the same helper serves both the cliff and its control.
    void add_ramp(float slope) {
        constexpr float HY = 20.0f;
        platform::StaticBoxDesc box;
        // Top-face centre must land on the origin: C = P0 - R * (0, HY, 0).
        box.center = {0.0f, -HY * std::cos(slope), HY * std::sin(slope)};
        box.half_extents = {40.0f, HY, 40.0f};
        box.rotation = glm::angleAxis(-slope, glm::vec3{1.0f, 0.0f, 0.0f});
        box.layer = physics_layer::LAYER_STATIC;
        REQUIRE(physics->create_static_box(box).valid());
    }

    void add_ceiling(float bottom_y, float center_z, float half_z) {
        platform::StaticBoxDesc box;
        box.center = {0.0f, bottom_y + 1.0f, center_z};
        box.half_extents = {8.0f, 1.0f, half_z};
        box.layer = physics_layer::LAYER_STATIC;
        REQUIRE(physics->create_static_box(box).valid());
    }

    void spawn(const glm::vec3& position) {
        platform::CharacterDesc desc;
        desc.position = position;
        desc.radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
        desc.height = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
        desc.max_slope_radians = static_cast<float>(config::PLAYER_MAX_SLOPE);
        desc.step_height = static_cast<float>(config::PLAYER_STEP_HEIGHT);
        desc.layer = physics_layer::LAYER_CHARACTER;
        desc.collides_with = physics_layer::LAYER_STATIC;
        state.character = physics->create_character(desc);
        REQUIRE(state.character.valid());
        transform.position = position;
    }

    void tick(float water_depth = 0.0f) {
        gameplay::player_pre_step(state, *physics, water_depth, transform, prev_transform,
                                  camera, prev_camera);
        physics->step(DT);
        gameplay::player_post_step(state, *physics, transform, camera);
    }

    void settle(int ticks = 60) {
        for (int i = 0; i < ticks; ++i) {
            tick();
        }
    }
};

TEST_CASE("jump: the apex matches JUMP_HEIGHT against real collision") {
    Rig rig;
    rig.add_floor(0.0f);
    rig.spawn({0.0f, 0.5f, 0.0f});
    rig.settle();
    const float ground = rig.transform.position.y;

    rig.state.jump_pressed = true;
    float apex = ground;
    for (int i = 0; i < 120; ++i) {
        rig.tick();
        apex = std::max(apex, rig.transform.position.y);
    }
    const float rise = apex - ground;
    const float target = static_cast<float>(config::JUMP_HEIGHT);
    // Discrete integration will not hit the analytic apex exactly; 10% is the
    // band that still distinguishes "reaches JUMP_HEIGHT" from any other value
    // someone might plug in.
    CHECK(rise == doctest::Approx(target).epsilon(0.10));
    // And it comes back down.
    CHECK(rig.transform.position.y == doctest::Approx(ground).epsilon(0.02));

    // CONTROL: without the press, the same 120 ticks never leave the floor.
    Rig control;
    control.add_floor(0.0f);
    control.spawn({0.0f, 0.5f, 0.0f});
    control.settle();
    const float control_ground = control.transform.position.y;
    float control_apex = control_ground;
    for (int i = 0; i < 120; ++i) {
        control.tick();
        control_apex = std::max(control_apex, control.transform.position.y);
    }
    CHECK(control_apex - control_ground < 0.05f);
}

TEST_CASE("crouch: a ceiling refuses the stand-up") {
    // The ceiling sits low enough that a standing capsule cannot fit and high
    // enough that a crouched one can.
    const float stand = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
    const float crouch = static_cast<float>(config::CROUCH_CAPSULE_HEIGHT);
    const float ceiling = 0.5f * (stand + crouch);
    REQUIRE(ceiling > crouch);
    REQUIRE(ceiling < stand);

    Rig rig;
    rig.add_floor(0.0f);
    rig.add_ceiling(ceiling, 0.0f, 4.0f); // covers z in [-4, 4]
    rig.spawn({0.0f, 0.5f, 0.0f});
    rig.state.crouch_held = true;
    rig.settle();
    REQUIRE(rig.state.crouched);

    // Release the key UNDER the ceiling: the request is refused, because
    // standing up here would put the capsule inside solid rock.
    rig.state.crouch_held = false;
    for (int i = 0; i < 30; ++i) {
        rig.tick();
    }
    CHECK(rig.state.crouched);
    CHECK(rig.physics->character_height(rig.state.character) == doctest::Approx(crouch));

    // CONTROL: the identical release with nothing overhead stands up at once.
    // Without this, "still crouched" could just mean stand-up never works.
    Rig control;
    control.add_floor(0.0f);
    control.spawn({0.0f, 0.5f, 0.0f});
    control.state.crouch_held = true;
    control.settle();
    REQUIRE(control.state.crouched);
    control.state.crouch_held = false;
    control.tick();
    CHECK_FALSE(control.state.crouched);
    CHECK(control.physics->character_height(control.state.character) ==
          doctest::Approx(stand));
}

// THE CLIFF INVARIANT (the lead's standing question).
//
// PLAYER_MAX_SLOPE (0.87 rad) is below MASSIF_CLIFF_SLOPE_MIN (0.96 rad) on
// purpose: cliffs and world edges stop the player BY GEOMETRY rather than by an
// invisible wall. If jumping lets a player ratchet up a 55-degree face, that
// design ruling has been quietly repealed by a movement feature.
TEST_CASE("jump does not repeal the slope limit that makes cliffs impassable") {
    const float cliff = static_cast<float>(config::MASSIF_CLIFF_SLOPE_MIN);
    const float walkable = 0.5f * static_cast<float>(config::PLAYER_MAX_SLOPE);
    REQUIRE(cliff > static_cast<float>(config::PLAYER_MAX_SLOPE));

    auto climb = [](float slope) {
        Rig rig;
        rig.add_ramp(slope);
        rig.spawn({0.0f, 0.5f, 0.0f});
        rig.settle();
        const float start = rig.transform.position.y;
        rig.state.yaw = YAW_TOWARD_PLUS_Z; // uphill
        rig.state.move_axes = {0.0f, 1.0f};
        for (int i = 0; i < 600; ++i) { // ten seconds of hammering the keys
            rig.state.jump_pressed = true;
            rig.tick();
        }
        return rig.transform.position.y - start;
    };

    const float cliff_gain = climb(cliff);
    const float walkable_gain = climb(walkable);

    MESSAGE("cliff gain " << cliff_gain << " m, walkable gain " << walkable_gain << " m");

    // CONTROL FIRST: the same script on a walkable slope must climb a long way.
    // If it does not, the test cannot detect climbing at all and the cliff
    // result below would be meaningless.
    CHECK(walkable_gain > 5.0f);

    // The invariant: hammering jump into a cliff face buys at most about one
    // jump of height, and never a sustained ascent.
    CHECK(cliff_gain < 2.0f * static_cast<float>(config::JUMP_HEIGHT));
    CHECK(cliff_gain < 0.2f * walkable_gain);
}

} // namespace
