/*
Created: 09:08:2026 - 00:45:08
Last updated: 09:08:2026 - 17:08:40
Module: tests
File: tests/sim/PlayerMovementTests.cpp

Responsibility:
- Player movement tests on the null physics backend: snapshot discipline
  (prev == old curr after the tick), pitch clamping, walk/run speed selection,
  diagonal normalization, input accumulation, movement conventions.

Key items:
- FakeInput: scriptable IInput for input-accumulation tests.
- Doctest cases over the ref-based movement core (no ECS World needed).

Dependencies:
- Uses: doctest, dfn_gameplay, dfn_platform_physics (null factory), constants.
- Used by: ctest (sim_player_movement).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Expectations derive from dfn::config constants, never literal duplicates.
*/
/*
UPD:
- 09:08:2026 - 00:45:08: Stage 2 — initial movement test suite.
- 09:08:2026 - 15:08:24: Rig sets explicit collision layers (zero masks are
                         now rejected by the IPhysics contract).
- 09:08:2026 - 17:08:40: Run input now sprints (DEBUG_SPRINT_MULTIPLIER).
*/

#include <doctest/doctest.h>

#include <cmath>

#include <glm/geometric.hpp>

#include "engine/core/config/sources/Constants.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/platform/physics/sources/null/CreateNullPhysics.h"

namespace {

namespace config = dfn::config;
namespace gameplay = dfn::gameplay;
namespace platform = dfn::platform;
// Aliased: Rig holds a member named `physics`, which would otherwise shadow
// the dfn::physics namespace inside its methods.
namespace physics_layer = dfn::physics;
using dfn::components::CameraPose;
using dfn::components::PreviousCameraPose;
using dfn::components::PreviousTransform;
using dfn::components::Transform;

constexpr float DT = static_cast<float>(config::SIM_DT);
constexpr float EPS = 1e-4f;

// Scriptable IInput: set the fields, the interface reports them.
class FakeInput final : public platform::IInput {
public:
    bool w = false, a = false, s = false, d = false, shift = false;
    glm::vec2 delta{0.0f};

    void update() override {}
    bool is_down(platform::Key key) const override {
        switch (key) {
        case platform::Key::W: return w;
        case platform::Key::A: return a;
        case platform::Key::S: return s;
        case platform::Key::D: return d;
        case platform::Key::LEFT_SHIFT: return shift;
        default: return false;
        }
    }
    bool was_pressed(platform::Key) const override { return false; }
    bool was_released(platform::Key) const override { return false; }
    bool is_down(platform::MouseButton) const override { return false; }
    bool was_pressed(platform::MouseButton) const override { return false; }
    bool was_released(platform::MouseButton) const override { return false; }
    glm::vec2 mouse_position() const override { return {0.0f, 0.0f}; }
    glm::vec2 mouse_delta() const override { return delta; }
    glm::vec2 scroll_delta() const override { return {0.0f, 0.0f}; }
    void set_cursor_captured(bool) override {}
    bool is_cursor_captured() const override { return true; }
};

// One player on null physics with all four pose components.
struct Rig {
    std::unique_ptr<platform::IPhysics> physics = platform::create_null_physics();
    gameplay::PlayerState state;
    Transform transform;
    PreviousTransform prev_transform;
    CameraPose camera;
    PreviousCameraPose prev_camera;

    explicit Rig(const glm::vec3& spawn = {0.0f, 0.0f, 0.0f}) {
        REQUIRE(physics->init());
        platform::CharacterDesc desc;
        desc.position = spawn;
        desc.radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
        desc.height = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
        // Explicit layers: zero masks are rejected by contract (IPhysics.h).
        desc.layer = physics_layer::LAYER_CHARACTER;
        desc.collides_with = physics_layer::LAYER_STATIC;
        state.character = physics->create_character(desc);
        REQUIRE(state.character.valid());
        transform.position = spawn;
    }

    void tick() {
        gameplay::player_pre_step(state, *physics, transform, prev_transform, camera,
                                  prev_camera);
        physics->step(DT);
        gameplay::player_post_step(state, *physics, transform, camera);
    }
};

TEST_CASE("snapshot discipline: prev equals old curr after the tick") {
    Rig rig({3.0f, 1.0f, -2.0f});
    rig.transform.position = {3.0f, 1.0f, -2.0f};
    rig.camera.position = {3.0f, 2.7f, -2.0f};
    rig.camera.yaw = 0.5f;
    rig.camera.pitch = -0.25f;
    const Transform old_transform = rig.transform;
    const CameraPose old_camera = rig.camera;

    rig.state.move_axes = {0.0f, 1.0f}; // ensure curr changes this tick
    rig.tick();

    CHECK(rig.prev_transform.position == old_transform.position);
    CHECK(rig.prev_transform.rotation == old_transform.rotation);
    CHECK(rig.prev_transform.scale == old_transform.scale);
    CHECK(rig.prev_camera.position == old_camera.position);
    CHECK(rig.prev_camera.yaw == old_camera.yaw);
    CHECK(rig.prev_camera.pitch == old_camera.pitch);
    // And the tick did move curr away from prev (null physics applies horizontal).
    CHECK(rig.transform.position != rig.prev_transform.position);
}

TEST_CASE("pitch clamps at CAMERA_PITCH_LIMIT in both directions") {
    Rig rig;
    rig.state.pending_look = {0.0f, -100000.0f}; // huge mouse-up = look up
    rig.tick();
    CHECK(rig.state.pitch == doctest::Approx(config::CAMERA_PITCH_LIMIT));

    rig.state.pending_look = {0.0f, 100000.0f}; // huge mouse-down = look down
    rig.tick();
    CHECK(rig.state.pitch == doctest::Approx(-config::CAMERA_PITCH_LIMIT));
}

TEST_CASE("walk and sprint speeds move exactly one tick's distance") {
    Rig rig;
    rig.state.move_axes = {0.0f, 1.0f}; // forward; yaw 0 faces -Z
    rig.tick();
    CHECK(rig.transform.position.z ==
          doctest::Approx(-static_cast<float>(config::WALK_SPEED) * DT).epsilon(EPS));
    CHECK(rig.transform.position.x == doctest::Approx(0.0f));

    // DEBUG CONVENIENCE (user request): the run input sprints at
    // RUN_SPEED * DEBUG_SPRINT_MULTIPLIER, not RUN_SPEED. Revisit at the
    // movement/combat grill — RUN_SPEED itself stays the design value.
    const float after_walk = rig.transform.position.z;
    const float sprint_speed =
        static_cast<float>(config::RUN_SPEED * config::DEBUG_SPRINT_MULTIPLIER);
    rig.state.move_axes = {0.0f, 1.0f};
    rig.state.run = true;
    rig.tick();
    CHECK(rig.transform.position.z - after_walk ==
          doctest::Approx(-sprint_speed * DT).epsilon(EPS));
    CHECK(sprint_speed > static_cast<float>(config::RUN_SPEED)); // guard the multiplier
}

TEST_CASE("diagonal movement is not faster") {
    Rig rig;
    rig.state.move_axes = {1.0f, 1.0f};
    rig.tick();
    const float distance = glm::length(
        glm::vec2(rig.transform.position.x, rig.transform.position.z));
    CHECK(distance == doctest::Approx(static_cast<float>(config::WALK_SPEED) * DT)
                          .epsilon(EPS));
}

TEST_CASE("yaw conventions: positive mouse x turns clockwise, forward follows") {
    Rig rig;
    // Turn to yaw = +pi/2 (east, +X) via accumulated look.
    const float half_pi = std::acos(0.0f);
    rig.state.pending_look = {half_pi / static_cast<float>(config::MOUSE_SENSITIVITY),
                              0.0f};
    rig.state.move_axes = {0.0f, 1.0f};
    rig.tick();
    CHECK(rig.state.yaw == doctest::Approx(half_pi));
    CHECK(rig.transform.position.x ==
          doctest::Approx(static_cast<float>(config::WALK_SPEED) * DT).epsilon(EPS));
    CHECK(rig.transform.position.z == doctest::Approx(0.0f).epsilon(EPS));
}

TEST_CASE("camera eye rides PLAYER_EYE_HEIGHT above the capsule bottom") {
    Rig rig({5.0f, 2.0f, 5.0f});
    rig.tick();
    CHECK(rig.camera.position.y ==
          doctest::Approx(2.0f + static_cast<float>(config::PLAYER_EYE_HEIGHT)));
    CHECK(rig.camera.position.x == doctest::Approx(5.0f));
}

TEST_CASE("input accumulation: look sums across frames, axes take the latest") {
    FakeInput input;
    gameplay::PlayerState state;

    input.delta = {2.0f, -3.0f};
    input.w = true;
    gameplay::accumulate_input(input, state);
    input.delta = {1.5f, 1.0f};
    input.w = false;
    input.d = true;
    input.shift = true;
    gameplay::accumulate_input(input, state);

    CHECK(state.pending_look.x == doctest::Approx(3.5f));
    CHECK(state.pending_look.y == doctest::Approx(-2.0f));
    CHECK(state.move_axes.x == doctest::Approx(1.0f)); // latest: D only
    CHECK(state.move_axes.y == doctest::Approx(0.0f));
    CHECK(state.run);
}

TEST_CASE("null physics contract: vertical intent ignored, always grounded") {
    Rig rig({0.0f, 4.0f, 0.0f});
    for (int i = 0; i < 30; ++i) {
        rig.tick(); // gravity accumulates into displacement.y — null ignores it
    }
    CHECK(rig.transform.position.y == doctest::Approx(4.0f)); // glides on its plane
    CHECK(rig.physics->character_grounded(rig.state.character));
}

} // namespace
