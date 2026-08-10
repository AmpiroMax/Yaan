/*
Created: 10:08:2026 - 02:23:05
Last updated: 10:08:2026 - 02:23:05
Module: tests (sim zone)
File: tests/sim/PlaytestTests.cpp

Responsibility:
- The playtest checker's OWN controls (Rule 30, the spec's mandate): a
  deliberately broken run MUST produce incidents — under-the-world teleport
  fires below_world within ONE tick, a teleport injection fires speed_bound,
  a skewed drawn-water sampler fires water_mismatch — and a clean patrol on
  flat ground fires NOTHING while actually covering distance (30a).

Key items:
- PlaytestRig: real jolt physics, real World, real movement code; the bot
  drives the same input intents a human would.

Dependencies:
- Uses: doctest, gameplay (PlaytestBot, PlayerMovement), core ecs/events,
  platform physics (jolt), generated constants.
- Used by: ctest (sim_playtest).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 10:08:2026 - 02:23:05: Created with playtest v1.
*/

#include <doctest/doctest.h>

#include <cmath>
#include <fstream>
#include <iterator>
#include <string>

#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/gameplay/sources/PlaytestBot.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"

namespace {

namespace config = dfn::config;
namespace gameplay = dfn::gameplay;
namespace platform = dfn::platform;
namespace physics_layer = dfn::physics;

constexpr float DT = static_cast<float>(config::SIM_DT);

// A real game loop in miniature: World + jolt + the actual movement code,
// with the bot writing the input intents.
struct PlaytestRig {
    std::unique_ptr<platform::IPhysics> physics = platform::create_jolt_physics();
    dfn::ecs::World world;
    dfn::ecs::EntityId player{};
    gameplay::PlaytestState pt;
    gameplay::PlaytestCheckEnv env;

    explicit PlaytestRig(const gameplay::PlaytestConfig& cfg) {
        REQUIRE(physics->init());
        // A large flat floor with its top face at y = 0.
        platform::StaticBoxDesc box;
        box.center = {0.0f, -5.0f, 0.0f};
        box.half_extents = {200.0f, 5.0f, 200.0f};
        box.layer = physics_layer::LAYER_STATIC;
        REQUIRE(physics->create_static_box(box).valid());

        player = gameplay::spawn_player(world, *physics, {0.0f, 0.1f, 0.0f});
        REQUIRE(world.alive(player));

        pt = gameplay::make_playtest(cfg);
        env.terrain_height = [](glm::vec2) { return std::optional<float>{0.0f}; };
        env.world_floor_y = -100.0f;
        env.deep_margin = 60.0f;
    }

    size_t tick() {
        gameplay::playtest_drive(pt, world);
        gameplay::player_pre_step(world, *physics);
        physics->step(DT);
        gameplay::player_post_step(world, *physics);
        return gameplay::playtest_check(pt, world, env);
    }

    [[nodiscard]] gameplay::PlayerState& state() {
        return *world.get<gameplay::PlayerState>(player);
    }
};

} // namespace

TEST_CASE("clean patrol on flat ground: distance covered, zero incidents (30a)") {
    gameplay::PlaytestConfig cfg;
    cfg.mode = gameplay::BotMode::WaypointPatrol;
    cfg.waypoints = {{15.0f, 0.0f}, {15.0f, 15.0f}, {0.0f, 0.0f}};
    cfg.duration_seconds = 0.0f; // once through the list
    PlaytestRig rig(cfg);

    for (int i = 0; i < 60 * 60 && !rig.pt.finished; ++i) {
        rig.tick();
    }
    CHECK(rig.pt.finished);            // the route is walkable and was walked
    CHECK(rig.pt.distance_walked > 30.0); // ...for real, not vacuously
    CHECK(rig.pt.incidents.empty());   // and a healthy world fires nothing
}

TEST_CASE("control: teleport under the world fires below_world within one tick") {
    gameplay::PlaytestConfig cfg;
    cfg.mode = gameplay::BotMode::Soak;
    cfg.soak_radius = 10.0f;
    PlaytestRig rig(cfg);
    for (int i = 0; i < 30; ++i) {
        rig.tick(); // settle + establish last_position
    }
    REQUIRE(rig.pt.incidents.empty());

    rig.physics->teleport_character(rig.state().character, {0.0f, -200.0f, 0.0f});
    const size_t fired = rig.tick(); // THE requirement: within one tick
    CHECK(fired >= 1);
    bool below_world = false;
    for (const auto& inc : rig.pt.incidents) {
        if (inc.invariant == "below_world") {
            below_world = true;
        }
    }
    CHECK(below_world);
}

TEST_CASE("control: deep-under-surface (but above the floor) fires below_surface") {
    gameplay::PlaytestConfig cfg;
    cfg.mode = gameplay::BotMode::Soak;
    PlaytestRig rig(cfg);
    for (int i = 0; i < 30; ++i) {
        rig.tick();
    }
    rig.physics->teleport_character(rig.state().character, {0.0f, -80.0f, 0.0f});
    (void)rig.tick();
    bool below_surface = false, below_world = false;
    for (const auto& inc : rig.pt.incidents) {
        below_surface |= inc.invariant == "below_surface";
        below_world |= inc.invariant == "below_world";
    }
    CHECK(below_surface);
    CHECK(!below_world); // -80 is above the -100 floor: the classes separate
}

TEST_CASE("control: a teleport injection fires speed_bound") {
    gameplay::PlaytestConfig cfg;
    cfg.mode = gameplay::BotMode::Soak;
    PlaytestRig rig(cfg);
    for (int i = 0; i < 30; ++i) {
        rig.tick();
    }
    rig.physics->teleport_character(rig.state().character, {100.0f, 0.1f, 0.0f});
    (void)rig.tick();
    bool speed_bound = false;
    for (const auto& inc : rig.pt.incidents) {
        speed_bound |= inc.invariant == "speed_bound";
    }
    CHECK(speed_bound);
}

TEST_CASE("water mismatch: a skewed drawn surface fires, the honest one does not") {
    gameplay::PlaytestConfig cfg;
    cfg.mode = gameplay::BotMode::Soak;
    PlaytestRig rig(cfg);
    rig.env.water_analytic = [](glm::vec2) { return std::optional<float>{2.0f}; };
    rig.env.water_drawn = [](glm::vec2) { return std::optional<float>{2.0f}; };
    for (int i = 0; i < 60; ++i) {
        rig.tick();
    }
    CHECK(rig.pt.incidents.empty()); // agreement is silent

    rig.env.water_drawn = [](glm::vec2) { return std::optional<float>{3.0f}; };
    (void)rig.tick();
    bool mismatch = false;
    for (const auto& inc : rig.pt.incidents) {
        mismatch |= inc.invariant == "water_mismatch";
    }
    CHECK(mismatch); // a metre of visible-but-unswimmable water is a finding
}

TEST_CASE("foot slip: a dragging planted foot fires, a still one does not") {
    // THE INSTRUMENT THE MOVEMENT RULING ASKED FOR, with its own control.
    // Slip is a MOTION artifact — no still frame can show it — so the check is
    // per-tick, and it is tested here with a synthetic rig rather than waiting
    // for character's clips: the checker must be known-good BEFORE it is
    // pointed at real feet, or a silent check reads exactly like a clean run.
    gameplay::PlaytestConfig cfg;
    cfg.mode = gameplay::BotMode::Soak;

    SUBCASE("a foot held still while planted is silent (30a: a case that CAN pass)") {
        PlaytestRig rig(cfg);
        rig.env.foot_sample = []() {
            return std::optional<gameplay::FootSample>{
                gameplay::FootSample{{1.0f, 0.0f, 2.0f}, {1.2f, 0.0f, 2.0f}, true, false}};
        };
        for (int i = 0; i < 120; ++i) {
            rig.tick();
        }
        for (const auto& inc : rig.pt.incidents) {
            CHECK(inc.invariant != "foot_slip");
        }
        CHECK(rig.pt.worst_slip_m < 1e-6); // and it measured, rather than skipped
    }

    SUBCASE("a foot dragged while planted fires") {
        PlaytestRig rig(cfg);
        // Drags 1 mm per tick: after ~60 ticks it has slid 6 cm, past the 5%
        // bound. This is the shape of the real defect — the clip plants the
        // foot and the body walks out from under it.
        static float drag = 0.0f;
        drag = 0.0f;
        rig.env.foot_sample = []() {
            drag += 0.001f;
            return std::optional<gameplay::FootSample>{
                gameplay::FootSample{{1.0f + drag, 0.0f, 2.0f}, {1.2f, 0.0f, 2.0f},
                                     true, false}};
        };
        bool fired = false;
        for (int i = 0; i < 200 && !fired; ++i) {
            rig.tick();
            for (const auto& inc : rig.pt.incidents) {
                fired |= inc.invariant == "foot_slip";
            }
        }
        CHECK(fired);
    }

    SUBCASE("an unbound foot sampler skips the check rather than passing it") {
        PlaytestRig rig(cfg); // env.foot_sample left unbound
        for (int i = 0; i < 60; ++i) {
            rig.tick();
        }
        CHECK(rig.pt.worst_slip_m == 0.0); // nothing measured, nothing claimed
    }
}

TEST_CASE("artifacts: the writers produce the log and the summary") {
    gameplay::PlaytestConfig cfg;
    cfg.mode = gameplay::BotMode::Soak;
    PlaytestRig rig(cfg);
    for (int i = 0; i < 120; ++i) {
        rig.tick();
        gameplay::playtest_note_frame(rig.pt, 0.016f);
    }
    gameplay::playtest_note_frame(rig.pt, 0.2f); // one huge frame
    bool budget = false;
    for (const auto& inc : rig.pt.incidents) {
        budget |= inc.invariant == "frame_budget";
    }
    CHECK(budget);

    const std::string dir = "playtest_test_artifacts";
    gameplay::playtest_write_artifacts(rig.pt, dir);
    CHECK(std::ifstream(dir + "/incidents.log").good());
    std::ifstream summary(dir + "/summary.txt");
    REQUIRE(summary.good());
    std::string text((std::istreambuf_iterator<char>(summary)),
                     std::istreambuf_iterator<char>());
    CHECK(text.find("incidents") != std::string::npos);
    CHECK(text.find("frame_budget") != std::string::npos);
    CHECK(text.find("mean_fps") != std::string::npos);
}
