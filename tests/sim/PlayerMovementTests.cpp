/*
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

#include <doctest/doctest.h>

#include <cmath>
#include <memory>

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
    bool w = false, a = false, s = false, d = false, shift = false, ctrl = false;
    bool space_pressed = false;
    glm::vec2 delta{0.0f};

    void update() override {}
    bool is_down(platform::Key key) const override {
        switch (key) {
        case platform::Key::W: return w;
        case platform::Key::A: return a;
        case platform::Key::S: return s;
        case platform::Key::D: return d;
        case platform::Key::LEFT_SHIFT: return shift;
        case platform::Key::LEFT_CONTROL: return ctrl;
        default: return false;
        }
    }
    bool was_pressed(platform::Key key) const override {
        return key == platform::Key::SPACE && space_pressed;
    }
    bool was_released(platform::Key) const override { return false; }
    bool is_down(platform::MouseButton) const override { return false; }
    bool was_pressed(platform::MouseButton) const override { return false; }
    bool was_released(platform::MouseButton) const override { return false; }
    glm::vec2 mouse_position() const override { return {0.0f, 0.0f}; }
    glm::vec2 mouse_delta() const override { return delta; }
    glm::vec2 scroll_delta() const override { return {0.0f, 0.0f}; }
    void set_cursor_captured(bool) override {}
    bool is_cursor_captured() const override { return true; }
    // Пункт контракта, заведённый ради стенда захвата курсора; здесь двигать
    // нечего — рукав задаёт смещение полем delta напрямую.
    void place_cursor(const glm::vec2&) override {}
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

    // water_depth defaults to 0: dry land unless a case says otherwise.
    void tick(float water_depth = 0.0f) {
        gameplay::player_pre_step(state, *physics, water_depth, transform, prev_transform,
                                  camera, prev_camera);
        physics->step(DT);
        gameplay::player_post_step(state, *physics, transform, camera);
    }

    // Same tick, but with the app's ferry present — the crouch case needs it,
    // because the crouched eye is no longer a constant this side owns.
    void tick(const gameplay::StepContext& step, float water_depth = 0.0f) {
        gameplay::player_pre_step(state, *physics, water_depth, transform, prev_transform,
                                  camera, prev_camera, step);
        physics->step(DT);
        gameplay::player_post_step(state, *physics, prev_transform, transform, camera, step);
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

TEST_CASE("gear speeds move exactly one tick's distance") {
    // UPDATED for the user's three-speed ruling: `run` now means RUN_SPEED,
    // and the debug sprint moved to its own flag. Before the ruling this case
    // asserted that `run` produced the DEBUG speed — it went red on the
    // change, which is the test doing its job rather than a break.
    Rig rig;
    rig.state.move_axes = {0.0f, 1.0f}; // forward; yaw 0 faces -Z
    rig.tick();
    CHECK(rig.transform.position.z ==
          doctest::Approx(-static_cast<float>(config::WALK_SPEED) * DT).epsilon(EPS));
    CHECK(rig.transform.position.x == doctest::Approx(0.0f));

    const float after_walk = rig.transform.position.z;
    rig.state.move_axes = {0.0f, 1.0f};
    rig.state.run = true;
    rig.tick();
    CHECK(rig.transform.position.z - after_walk ==
          doctest::Approx(-static_cast<float>(config::RUN_SPEED) * DT).epsilon(EPS));

    // DEBUG CONVENIENCE (user request, still live): its own flag now, so it
    // cannot be confused with the real run gear. Revisit at the movement grill.
    const float after_run = rig.transform.position.z;
    const float sprint_speed =
        static_cast<float>(config::RUN_SPEED * config::DEBUG_SPRINT_MULTIPLIER);
    rig.state.move_axes = {0.0f, 1.0f};
    rig.state.run = false;
    rig.state.debug_sprint = true;
    rig.tick();
    CHECK(rig.transform.position.z - after_run ==
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

TEST_CASE("the eye sits on the face, PLAYER_EYE_FORWARD ahead of the capsule axis") {
    // The camera used to sit ON the axis, which put it inside the body's chest
    // box (centred on that same axis) — looking down filled the frame with
    // torso and the feet were unreachable by construction (character's
    // measured frame). The offset is along FACING, not the view direction.
    constexpr float FWD = static_cast<float>(config::PLAYER_EYE_FORWARD);
    Rig rig({5.0f, 2.0f, 5.0f});
    rig.tick();
    // yaw 0 faces -Z: the eye leads the feet along -Z, and x is untouched.
    CHECK(rig.camera.position.z == doctest::Approx(5.0f - FWD).epsilon(EPS));
    CHECK(rig.camera.position.x == doctest::Approx(5.0f).epsilon(EPS));

    // Turned east (+X), the same offset must follow the FACING, not the world.
    const float half_pi = std::acos(0.0f);
    rig.state.yaw = half_pi;
    rig.tick();
    CHECK(rig.camera.position.x - rig.transform.position.x ==
          doctest::Approx(FWD).epsilon(EPS));
    CHECK(rig.camera.position.z - rig.transform.position.z ==
          doctest::Approx(0.0f).epsilon(EPS));

    // PITCH MUST NOT MOVE THE EYE (the reason the offset is yaw-only): looking
    // down would otherwise walk the camera forward into whatever it faces.
    const glm::vec3 before = rig.camera.position;
    rig.state.pitch = -1.0f;
    rig.tick();
    CHECK(rig.camera.position.x == doctest::Approx(before.x).epsilon(EPS));
    CHECK(rig.camera.position.z == doctest::Approx(before.z).epsilon(EPS));

    // BOUND, the one that keeps the camera out of walls (Rule 30, both ends):
    // the near plane must stay inside the collision capsule, or the view pokes
    // through geometry that stops the body.
    CHECK(FWD + static_cast<float>(config::CAMERA_NEAR) <
          static_cast<float>(config::PLAYER_CAPSULE_RADIUS));
    // ...and the offset must clear zero, which is the rejected instance.
    CHECK(FWD > 0.0f);
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

// --- Jump / crouch / swim (v1 movement, user-approved) -----------------------
//
// The recording backend exists because the null contract deliberately DISCARDS
// vertical displacement, so "did the player intend to dive" cannot be read from
// the resulting position. Intent is submitted through move_character, so that
// is what these cases inspect.
class RecordingPhysics final : public platform::IPhysics {
public:
    std::unique_ptr<platform::IPhysics> inner = platform::create_null_physics();
    glm::vec3 last_displacement{0.0f};

    bool init() override { return inner->init(); }
    void shutdown() override { inner->shutdown(); }
    void step(float dt) override { inner->step(dt); }
    platform::PhysicsBodyHandle create_terrain_mesh(
        const platform::TerrainMeshDesc& d) override {
        return inner->create_terrain_mesh(d);
    }
    platform::PhysicsBodyHandle create_terrain(const platform::TerrainDesc& d) override {
        return inner->create_terrain(d);
    }
    platform::PhysicsBodyHandle create_static_box(
        const platform::StaticBoxDesc& d) override {
        return inner->create_static_box(d);
    }
    void set_body_transform(platform::PhysicsBodyHandle b, const glm::vec3& p,
                            const glm::quat& r) override {
        inner->set_body_transform(b, p, r);
    }
    void destroy_body(platform::PhysicsBodyHandle b) override { inner->destroy_body(b); }
    // ДИНАМИЧЕСКИЕ ТЕЛА (28.08) — сквозной проброс, как и всё остальное здесь:
    // этот двойник существует ради ОДНОГО перехваченного вызова
    // (move_character), и всякий новый обязан вести себя ровно как настоящий,
    // иначе двойник начинает быть отдельной физикой.
    platform::PhysicsBodyHandle create_dynamic_body(
        const platform::DynamicBodyDesc& d) override {
        return inner->create_dynamic_body(d);
    }
    platform::BodyPose body_pose(platform::PhysicsBodyHandle b) const override {
        return inner->body_pose(b);
    }
    glm::vec3 body_velocity(platform::PhysicsBodyHandle b) const override {
        return inner->body_velocity(b);
    }
    void set_body_velocity(platform::PhysicsBodyHandle b, const glm::vec3& l,
                           const glm::vec3& a) override {
        inner->set_body_velocity(b, l, a);
    }
    void set_body_gravity_factor(platform::PhysicsBodyHandle b, float f) override {
        inner->set_body_gravity_factor(b, f);
    }
    bool body_asleep(platform::PhysicsBodyHandle b) const override {
        return inner->body_asleep(b);
    }
    void activate_body(platform::PhysicsBodyHandle b) override { inner->activate_body(b); }
    platform::CharacterHandle create_character(const platform::CharacterDesc& d) override {
        return inner->create_character(d);
    }
    void destroy_character(platform::CharacterHandle c) override {
        inner->destroy_character(c);
    }
    void move_character(platform::CharacterHandle c, const glm::vec3& d) override {
        last_displacement = d;
        inner->move_character(c, d);
    }
    glm::vec3 character_position(platform::CharacterHandle c) const override {
        return inner->character_position(c);
    }
    bool character_grounded(platform::CharacterHandle c) const override {
        return inner->character_grounded(c);
    }
    void set_character_height(platform::CharacterHandle c, float h) override {
        inner->set_character_height(c, h);
    }
    float character_height(platform::CharacterHandle c) const override {
        return inner->character_height(c);
    }
    void teleport_character(platform::CharacterHandle c, const glm::vec3& p) override {
        inner->teleport_character(c, p);
    }
    platform::RayHit raycast(const glm::vec3& o, const glm::vec3& d, float m,
                             platform::CollisionMask k) const override {
        return inner->raycast(o, d, m, k);
    }
    platform::RayHit sphere_cast(const glm::vec3& o, const glm::vec3& d, float r, float m,
                                 platform::CollisionMask k) const override {
        return inner->sphere_cast(o, d, r, m, k);
    }
};

TEST_CASE("jump: takeoff speed is derived from JUMP_HEIGHT, not stored twice") {
    Rig rig;
    const float expected = std::sqrt(2.0f * static_cast<float>(config::GRAVITY) *
                                     static_cast<float>(config::JUMP_HEIGHT));
    rig.state.jump_pressed = true;
    rig.tick();
    // One tick of gravity is already subtracted by the time we observe it.
    CHECK(rig.state.vertical_velocity ==
          doctest::Approx(expected - static_cast<float>(config::GRAVITY) * DT)
              .epsilon(1e-3));

    // CONTROL: the same tick without the press must not launch anybody. The
    // comparison is against zero rather than a negative number because the null
    // backend reports "grounded" always, so post_step clears the falling
    // velocity every tick; a launch is the only way this becomes positive.
    Rig control;
    control.tick();
    CHECK(control.state.vertical_velocity <= 0.0f);
}

TEST_CASE("jump: the press is a latch, and it is spent even when refused") {
    Rig rig;
    rig.state.jump_pressed = true;
    rig.tick();
    CHECK(rig.state.vertical_velocity > 0.0f);
    CHECK_FALSE(rig.state.jump_pressed); // consumed

    // A press while crouched is refused — a crouch-jump is the classic way to
    // climb geometry built to stop the player — and is NOT banked for later.
    Rig crouched;
    crouched.state.crouch_held = true;
    crouched.tick();
    REQUIRE(crouched.state.crouched);
    crouched.state.jump_pressed = true;
    crouched.tick();
    CHECK(crouched.state.vertical_velocity <= 0.0f); // refused, never launched
    CHECK_FALSE(crouched.state.jump_pressed);        // spent, not banked
}

TEST_CASE("crouch: the capsule shrinks, and the eye goes where the BODY says") {
    // REWRITTEN 10:08:2026 (character's carve). This case used to assert that
    // the crouched camera arrives at `CROUCH_EYE_HEIGHT` 0.85 — and it did,
    // faithfully, while character folded the drawn body by half its LEG and put
    // the same character's eye at 1.2211. The camera sat 0.37 m below the drawn
    // skull and 0.25 m below its NECK, i.e. inside the chest, which the user
    // reported twice. The eye height is no longer this zone's number to hold:
    // it arrives as StepContext::crouch_eye from anim::crouch_eye_offset().
    Rig rig;
    REQUIRE(rig.physics->character_height(rig.state.character) ==
            doctest::Approx(static_cast<float>(config::PLAYER_CAPSULE_HEIGHT)));
    const auto eye_height = static_cast<float>(config::PLAYER_EYE_HEIGHT);
    // What character reports at full crouch for the shipped rig, ferried by the
    // app each tick. bob_scale 0 so the only thing moving the eye is the crouch.
    constexpr float DEEP_DROP = 0.4716f;
    gameplay::StepContext ferry;
    ferry.bob_scale = 0.0f;
    ferry.crouch_eye = {0.0f, DEEP_DROP};

    rig.state.crouch_held = true;
    rig.tick(ferry);
    // The CAPSULE is the point: a camera-only crouch leaves this untouched, and
    // that is the implementation this check exists to reject.
    CHECK(rig.physics->character_height(rig.state.character) ==
          doctest::Approx(static_cast<float>(config::CROUCH_CAPSULE_HEIGHT)));
    CHECK(rig.state.crouched);
    // ...while the BLEND is still on its way down after a single tick (it is
    // the blend that eases, and character scales the offset by it).
    CHECK(rig.state.crouch_blend < 1.0f);

    for (int i = 0; i < 60; ++i) {
        rig.tick(ferry);
    }
    CHECK(rig.state.crouch_blend == doctest::Approx(1.0f));
    // THE EYE IS WHERE THE BODY PUT IT: 1.2284, a hair above the drawn skull's
    // 1.2211 (the 7.3 mm the converged legs cost the STANDING pose too) and
    // 0.13 m above the crouched neck. Never 0.85 again.
    CHECK(rig.camera.position.y == doctest::Approx(eye_height - DEEP_DROP).epsilon(1e-3));
    CHECK(rig.camera.position.y > 1.09f); // above the crouched neck joint

    // CONTROL (Rule 30), and it is the implementation this rewrite rejects: a
    // camera that lowers itself to a constant of its own would land at the same
    // height for a DIFFERENT body. Halve the ferried drop and the eye must
    // follow it exactly.
    Rig shallow;
    gameplay::StepContext half = ferry;
    half.crouch_eye = {0.0f, 0.5f * DEEP_DROP};
    shallow.state.crouch_held = true;
    for (int i = 0; i < 61; ++i) {
        shallow.tick(half);
    }
    CHECK(shallow.camera.position.y ==
          doctest::Approx(eye_height - 0.5f * DEEP_DROP).epsilon(1e-3));
    CHECK(shallow.camera.position.y > rig.camera.position.y + 0.2f);

    // Released with nothing overhead (null raycasts always miss = open sky).
    rig.state.crouch_held = false;
    rig.tick();
    CHECK_FALSE(rig.state.crouched);
    CHECK(rig.physics->character_height(rig.state.character) ==
          doctest::Approx(static_cast<float>(config::PLAYER_CAPSULE_HEIGHT)));
}

TEST_CASE("crouch: crouched movement uses CROUCH_SPEED") {
    Rig rig;
    rig.state.crouch_held = true;
    rig.state.move_axes = {0.0f, 1.0f};
    rig.tick();
    const glm::vec3 start = rig.transform.position;
    rig.tick();
    CHECK(glm::length(rig.transform.position - start) ==
          doctest::Approx(static_cast<float>(config::CROUCH_SPEED) * DT).epsilon(1e-3));

    // CONTROL: standing, the same input moves at walking speed. An
    // implementation that ignored the crouch speed would make these equal.
    Rig control;
    control.state.move_axes = {0.0f, 1.0f};
    control.tick();
    const glm::vec3 cstart = control.transform.position;
    control.tick();
    CHECK(glm::length(control.transform.position - cstart) ==
          doctest::Approx(static_cast<float>(config::WALK_SPEED) * DT).epsilon(1e-3));
}

TEST_CASE("swim: the two thresholds are hysteresis, and one threshold fails this") {
    const float enter = static_cast<float>(config::SWIM_ENTER_DEPTH);
    const float exit_depth = static_cast<float>(config::SWIM_EXIT_DEPTH);
    REQUIRE(exit_depth < enter); // the pair is the mechanism, not slack
    const float between = 0.5f * (enter + exit_depth);

    // Walking in: at a depth between the thresholds you are still WADING.
    Rig rig;
    rig.tick(between);
    CHECK(rig.state.locomotion == gameplay::Locomotion::Wade);

    // Deep enough, you swim.
    rig.tick(enter + 0.01f);
    CHECK(rig.state.locomotion == gameplay::Locomotion::Swim);

    // Coming out: at the SAME between-depth you are still SWIMMING. This pair
    // of checks is the control for the whole design — a single-threshold
    // implementation returns the same answer for the same depth and therefore
    // cannot pass both halves, whatever threshold it picks.
    rig.tick(between);
    CHECK(rig.state.locomotion == gameplay::Locomotion::Swim);

    // Shallower than the exit threshold, you are back on your feet.
    rig.tick(exit_depth - 0.01f);
    CHECK(rig.state.locomotion == gameplay::Locomotion::Wade);

    // CONTROL: dry land is neither of the two water modes.
    rig.tick(0.0f);
    CHECK(rig.state.locomotion == gameplay::Locomotion::Ground);
}

TEST_CASE("swim: movement follows the look direction in three dimensions") {
    RecordingPhysics physics;
    REQUIRE(physics.init());
    platform::CharacterDesc desc;
    desc.radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
    desc.height = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
    desc.layer = physics_layer::LAYER_CHARACTER;
    desc.collides_with = physics_layer::LAYER_STATIC;

    gameplay::PlayerState state;
    state.character = physics.create_character(desc);
    REQUIRE(state.character.valid());
    Transform transform;
    PreviousTransform prev_transform;
    CameraPose camera;
    PreviousCameraPose prev_camera;

    const float deep = static_cast<float>(config::SWIM_ENTER_DEPTH) + 2.0f;
    state.move_axes = {0.0f, 1.0f};
    state.pitch = -0.7f; // look down: forward now means DIVE

    gameplay::player_pre_step(state, physics, deep, transform, prev_transform, camera,
                              prev_camera);
    REQUIRE(state.locomotion == gameplay::Locomotion::Swim);
    CHECK(physics.last_displacement.y < 0.0f); // pressing forward went DOWN

    // CONTROL: the same look and the same key on dry land must NOT drive the
    // player into the floor — on the ground, forward is horizontal whatever the
    // head is doing. Without this, "y < 0" above would also pass for gravity.
    gameplay::PlayerState ground_state;
    ground_state.character = state.character;
    ground_state.move_axes = {0.0f, 1.0f};
    ground_state.pitch = -0.7f;
    gameplay::player_pre_step(ground_state, physics, 0.0f, transform, prev_transform,
                              camera, prev_camera);
    REQUIRE(ground_state.locomotion == gameplay::Locomotion::Ground);
    const float horizontal = glm::length(
        glm::vec2{physics.last_displacement.x, physics.last_displacement.z});
    CHECK(horizontal ==
          doctest::Approx(static_cast<float>(config::WALK_SPEED) * DT).epsilon(1e-3));
    // Only gravity for one tick, not a dive.
    CHECK(physics.last_displacement.y ==
          doctest::Approx(-static_cast<float>(config::GRAVITY) * DT * DT).epsilon(1e-3));
}

TEST_CASE("wade: shallow water drags") {
    Rig rig;
    rig.state.move_axes = {0.0f, 1.0f};
    const float shallow = 0.5f * static_cast<float>(config::SWIM_EXIT_DEPTH);
    rig.tick(shallow);
    const glm::vec3 start = rig.transform.position;
    rig.tick(shallow);
    REQUIRE(rig.state.locomotion == gameplay::Locomotion::Wade);
    CHECK(glm::length(rig.transform.position - start) ==
          doctest::Approx(static_cast<float>(config::WALK_SPEED) *
                          static_cast<float>(config::WADE_SPEED_FACTOR) * DT)
              .epsilon(1e-3));

    // CONTROL: dry, the same input is undragged.
    Rig control;
    control.state.move_axes = {0.0f, 1.0f};
    control.tick();
    const glm::vec3 cstart = control.transform.position;
    control.tick();
    CHECK(glm::length(control.transform.position - cstart) ==
          doctest::Approx(static_cast<float>(config::WALK_SPEED) * DT).epsilon(1e-3));
}

TEST_CASE("input: jump latches across frames, crouch is sampled") {
    gameplay::PlayerState state;
    FakeInput input;

    input.space_pressed = true;
    gameplay::accumulate_input(input, state);
    input.space_pressed = false; // released before the fixed tick arrives
    gameplay::accumulate_input(input, state);
    // CONTROL: an implementation that SAMPLED the key instead of latching it
    // reads false here, which is exactly the dropped-jump bug at high frame
    // rates that the latch exists to prevent.
    CHECK(state.jump_pressed);

    input.ctrl = true;
    gameplay::accumulate_input(input, state);
    CHECK(state.crouch_held);
    input.ctrl = false;
    gameplay::accumulate_input(input, state);
    CHECK_FALSE(state.crouch_held);
}

} // namespace
