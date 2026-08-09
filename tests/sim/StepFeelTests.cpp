/*
Created: 10:08:2026 - 01:53:17
Last updated: 10:08:2026 - 01:53:17
Module: tests (sim zone)
File: tests/sim/StepFeelTests.cpp

Responsibility:
- The step-as-an-event acceptance suite (в3): footfalls equally spaced at
  constant speed, count matching stride arithmetic over a known distance,
  ZERO bob when stationary (the user's rejected «парение» is the control),
  surface class carried on the event, FOV coupling clamped, stop settle,
  and the landing dip + Landed event on real (jolt) collision.

Key items:
- NullRig: constant-speed walking on null physics (grounded by contract).
- JoltRig: real gravity for the landing edge.
- Every case names its Rule 30 control inline.

Dependencies:
- Uses: doctest, gameplay movement + StepFeel + StepEvents, core events,
  platform physics (null + jolt), generated constants.
- Used by: ctest (sim_step_feel).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Controls are part of the suite (Rule 30): a case that nothing can fail is
  a description, not a test.
*/
/*
UPD:
- 10:08:2026 - 01:53:17: Created for the landscape stage (шаг как событие).
*/

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

#include "engine/core/config/sources/Constants.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/gameplay/sources/StepEvents.h"
#include "engine/gameplay/sources/StepFeel.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"
#include "engine/platform/physics/sources/null/CreateNullPhysics.h"

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
constexpr float WALK = static_cast<float>(config::WALK_SPEED);
constexpr float EYE = static_cast<float>(config::PLAYER_EYE_HEIGHT);

// Collects every step event for assertions.
struct EventLog {
    std::vector<gameplay::FootfallEvent> footfalls;
    std::vector<int> footfall_ticks; // tick index of each footfall
    std::vector<gameplay::Landed> landings;
    std::vector<gameplay::Jumped> jumps;
    int tick = 0;

    void attach(dfn::events::EventBus& bus) {
        bus.subscribe<gameplay::FootfallEvent>([this](const gameplay::FootfallEvent& e) {
            footfalls.push_back(e);
            footfall_ticks.push_back(tick);
        });
        bus.subscribe<gameplay::Landed>(
            [this](const gameplay::Landed& e) { landings.push_back(e); });
        bus.subscribe<gameplay::Jumped>(
            [this](const gameplay::Jumped& e) { jumps.push_back(e); });
    }
};

// A walking rig on a physics backend, using the FULL step-feel signature.
template <typename Factory>
struct StepRig {
    std::unique_ptr<platform::IPhysics> physics;
    dfn::events::EventBus bus;
    EventLog log;
    gameplay::PlayerState state;
    Transform transform;
    PreviousTransform prev_transform;
    CameraPose camera;
    PreviousCameraPose prev_camera;
    gameplay::StepContext step;

    explicit StepRig(Factory factory, const glm::vec3& spawn = {0.0f, 0.0f, 0.0f}) {
        physics = factory();
        REQUIRE(physics->init());
        platform::CharacterDesc desc;
        desc.position = spawn;
        desc.radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
        desc.height = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
        desc.max_slope_radians = static_cast<float>(config::PLAYER_MAX_SLOPE);
        desc.step_height = static_cast<float>(config::PLAYER_STEP_HEIGHT);
        desc.layer = physics_layer::LAYER_CHARACTER;
        desc.collides_with = physics_layer::LAYER_STATIC;
        state.character = physics->create_character(desc);
        REQUIRE(state.character.valid());
        transform.position = spawn;
        log.attach(bus);
        step.events = &bus;
    }

    void tick(float water_depth = 0.0f) {
        gameplay::player_pre_step(state, *physics, water_depth, transform, prev_transform,
                                  camera, prev_camera, step);
        physics->step(DT);
        gameplay::player_post_step(state, *physics, prev_transform, transform, camera,
                                   step);
        bus.pump(); // the app pumps within the tick: same-tick delivery is real
        ++log.tick;
    }
};

using NullRig = StepRig<decltype(&platform::create_null_physics)>;
NullRig make_null_rig() { return NullRig(&platform::create_null_physics); }

} // namespace

TEST_CASE("footfalls at constant speed are equally spaced and match stride arithmetic") {
    auto rig = make_null_rig();
    rig.state.move_axes = {0.0f, 1.0f}; // forward at WALK_SPEED
    const int TICKS = 600; // 10 s of walking
    for (int i = 0; i < TICKS; ++i) {
        rig.tick();
    }

    // Count matches distance / step_length (stride arithmetic). Null physics
    // grants the commanded displacement fully, so distance is exact.
    const float distance = WALK * TICKS * DT;
    const float expected = distance / gameplay::step_length(WALK);
    REQUIRE(rig.log.footfalls.size() > 10);
    CHECK(std::abs(static_cast<float>(rig.log.footfalls.size()) - expected) <= 1.0f);

    // CONTROL (Rule 30): the same count against a half-length stride must NOT
    // fit — proves the arithmetic discriminates rather than always passing.
    const float wrong = distance / (0.5f * gameplay::step_length(WALK));
    CHECK(std::abs(static_cast<float>(rig.log.footfalls.size()) - wrong) > 1.0f);

    // Equal spacing: every interval sits within one tick of the EXACT stride
    // period (a half cycle at WALK_SPEED, in ticks — 28.0 here). The crossing
    // tick quantizes +-1 around the fractional period, so the bound is 1.5:
    // gaps of 27..29 are the quantization of a perfectly even stride, 26 or
    // 30 would be a real irregularity (the discriminating end of the range).
    const double expected_gap = gameplay::step_length(WALK) / WALK / DT;
    std::vector<int> intervals;
    for (size_t i = 1; i < rig.log.footfall_ticks.size(); ++i) {
        intervals.push_back(rig.log.footfall_ticks[i] - rig.log.footfall_ticks[i - 1]);
    }
    double mean = 0.0;
    for (const int gap : intervals) {
        CHECK_MESSAGE(std::abs(static_cast<double>(gap) - expected_gap) <= 1.5,
                      "gap " << gap << " vs period " << expected_gap);
        mean += gap;
    }
    mean /= static_cast<double>(intervals.size());
    // ...and the MEAN matches the period tightly (the +-1 jitter averages
    // out; a systematic drift would move the mean and fail here).
    CHECK(std::abs(mean - expected_gap) < 0.5);

    // Feet alternate: left, right, left, right...
    for (size_t i = 1; i < rig.log.footfalls.size(); ++i) {
        CHECK(rig.log.footfalls[i].left_foot != rig.log.footfalls[i - 1].left_foot);
    }
}

TEST_CASE("stationary means flat: zero bob, zero events — the rejected floating") {
    auto rig = make_null_rig();
    for (int i = 0; i < 240; ++i) {
        rig.tick();
    }
    // THE control of this whole feature: the user's rejected instance is a
    // camera that floats. Standing still must be EXACTLY eye height...
    CHECK(rig.camera.position.y == doctest::Approx(EYE).epsilon(1e-6));
    CHECK(rig.camera.position.x == doctest::Approx(0.0f).epsilon(1e-6));
    CHECK(rig.log.footfalls.empty());

    // ...while walking must NOT be (Rule 30a: the case that CAN differ, does).
    rig.state.move_axes = {0.0f, 1.0f};
    float min_y = 1e9f, max_y = -1e9f;
    for (int i = 0; i < 240; ++i) {
        rig.tick();
        min_y = std::min(min_y, rig.camera.position.y);
        max_y = std::max(max_y, rig.camera.position.y);
    }
    CHECK(max_y - min_y > 0.01f); // the bob visibly moves the eye
    // ...and the bob dips BELOW the rest height, never above (curve contract).
    CHECK(min_y < EYE - 0.01f);
    CHECK(max_y < EYE + 1e-4f);
}

TEST_CASE("footfall carries the surface class underfoot and the wading flag") {
    auto rig = make_null_rig();
    rig.step.surface_class_at = [](glm::vec2) {
        return std::optional<dfn::math::SurfaceClass>{dfn::math::SurfaceClass::Rock};
    };
    rig.state.move_axes = {0.0f, 1.0f};
    for (int i = 0; i < 120; ++i) {
        rig.tick();
    }
    REQUIRE(!rig.log.footfalls.empty());
    for (const auto& e : rig.log.footfalls) {
        CHECK(e.surface == dfn::math::SurfaceClass::Rock);
        CHECK(!e.wading);
    }

    // Ankle-deep water: same surface, wading flag up (the step must splash).
    rig.log.footfalls.clear();
    for (int i = 0; i < 120; ++i) {
        rig.tick(/*water_depth=*/0.3f);
    }
    REQUIRE(!rig.log.footfalls.empty());
    for (const auto& e : rig.log.footfalls) {
        CHECK(e.wading);
    }
}

TEST_CASE("bob amplitude is capped at sprint and fov scale is clamped") {
    auto rig = make_null_rig();
    rig.state.move_axes = {0.0f, 1.0f};
    rig.state.run = true; // debug sprint: 30 m/s, far beyond RUN_SPEED
    float min_y = 1e9f;
    for (int i = 0; i < 360; ++i) {
        rig.tick();
        min_y = std::min(min_y, rig.camera.position.y);
        // The clamp is a range assertion, both ends (Rule 30): never wider
        // than the row's ceiling, and never below neutral.
        CHECK(rig.camera.fov_scale <= static_cast<float>(config::FOV_SPEED_SCALE_MAX) + 1e-4f);
        CHECK(rig.camera.fov_scale >= 1.0f - 1e-4f);
    }
    // Amplitude cap: the deepest bob dip is bounded by HEADBOB_AMPLITUDE_MAX.
    CHECK(EYE - min_y <= static_cast<float>(config::HEADBOB_AMPLITUDE_MAX) + 1e-3f);
    // The ease reached the ceiling (30a: the target is attainable).
    CHECK(rig.camera.fov_scale ==
          doctest::Approx(static_cast<float>(config::FOV_SPEED_SCALE_MAX)).epsilon(0.01));

    // Control: after stopping, the scale returns to neutral.
    rig.state.move_axes = {0.0f, 0.0f};
    rig.state.run = false;
    for (int i = 0; i < 360; ++i) {
        rig.tick();
    }
    CHECK(rig.camera.fov_scale == doctest::Approx(1.0f).epsilon(0.01));
}

TEST_CASE("the stop is a settle, not a freeze") {
    auto rig = make_null_rig();
    rig.state.move_axes = {0.0f, 1.0f};
    for (int i = 0; i < 240; ++i) {
        rig.tick();
    }
    rig.state.move_axes = {0.0f, 0.0f};

    // During the settle window the camera dips below rest, then returns.
    float min_y = 1e9f;
    const int settle_ticks =
        static_cast<int>(static_cast<float>(config::STOP_SETTLE_TIME) / DT) + 2;
    for (int i = 0; i < settle_ticks; ++i) {
        rig.tick();
        min_y = std::min(min_y, rig.camera.position.y);
    }
    CHECK(min_y < EYE - 0.005f); // the overshoot exists
    for (int i = 0; i < 60; ++i) {
        rig.tick();
    }
    CHECK(rig.camera.position.y == doctest::Approx(EYE).epsilon(1e-5));

    // CONTROL: a rig that never walked settles nothing — same window, flat.
    auto still = make_null_rig();
    float still_min = 1e9f;
    for (int i = 0; i < settle_ticks; ++i) {
        still.tick();
        still_min = std::min(still_min, still.camera.position.y);
    }
    CHECK(still_min == doctest::Approx(EYE).epsilon(1e-6));
}

TEST_CASE("landing after a jump dips the camera and fires Landed with the measured impact") {
    StepRig rig(&platform::create_jolt_physics, glm::vec3{0.0f, 0.1f, 0.0f});
    // A real floor for a real landing.
    platform::StaticBoxDesc box;
    box.center = {0.0f, -5.0f, 0.0f};
    box.half_extents = {80.0f, 5.0f, 80.0f};
    box.layer = physics_layer::LAYER_STATIC;
    REQUIRE(rig.physics->create_static_box(box).valid());
    for (int i = 0; i < 120; ++i) {
        rig.tick(); // settle onto the floor
    }
    // The initial 10 cm drop onto the floor may legitimately fire one small
    // landing; the jump assertions count from here rather than from zero.
    const size_t base_landings = rig.log.landings.size();

    rig.state.jump_pressed = true;
    float min_y = 1e9f;
    bool landed_seen = false;
    for (int i = 0; i < 240; ++i) {
        rig.tick();
        if (rig.log.landings.size() > base_landings) {
            landed_seen = true;
            min_y = std::min(min_y, rig.camera.position.y);
        }
    }
    REQUIRE(landed_seen);
    REQUIRE(rig.log.jumps.size() == 1);
    const auto& landing = rig.log.landings.back();

    // The measured impact approximates the takeoff speed (symmetric flight);
    // jolt's collide-and-slide eats a little, hence the loose band — but it
    // must be a REAL fall, not a micro-step (the band's low end matters too).
    const float takeoff = std::sqrt(2.0f * static_cast<float>(config::GRAVITY)
                                    * static_cast<float>(config::JUMP_HEIGHT));
    CHECK(landing.impact_speed > 0.5f * takeoff);
    CHECK(landing.impact_speed < 1.5f * takeoff);

    // The dip: below the standing eye by a depth scaled from that impact.
    const float expected_dip =
        static_cast<float>(config::LANDING_DIP_PER_MPS) * landing.impact_speed;
    const float rest = rig.transform.position.y + EYE;
    CHECK(rest - min_y > 0.5f * expected_dip);

    // ...and it recovers: after LANDING_DIP_TIME the camera is back at rest.
    for (int i = 0; i < 120; ++i) {
        rig.tick();
    }
    CHECK(rig.camera.position.y == doctest::Approx(rig.transform.position.y + EYE)
                                       .epsilon(1e-4));

    // CONTROL: standing still afterwards fires no further landings.
    const size_t settled = rig.log.landings.size();
    for (int i = 0; i < 120; ++i) {
        rig.tick();
    }
    CHECK(rig.log.landings.size() == settled);
}
