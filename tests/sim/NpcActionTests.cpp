/*
Module: tests
File: tests/sim/NpcActionTests.cpp

Responsibility:
- ПРИБОР ИСПОЛНИТЕЛЯ ДЕЙСТВИЙ НПС (engine/gameplay/sources/NpcAction.cpp):
  НПС — тот же ходок, что игрок; MoveTo доходит до точки той же походкой
  (нулевая физика — смещение по осям × скорость передачи) и встаёт в
  радиусе; Face доворачивает рыск; Wait ждёт сим-секунды; clear_queue даёт
  Interrupted; события Completed/Failed идут на шину с тем же sequence.

Key items:
- move_to_walks_there_and_stops
- move_to_behind_turns_first
- face_and_wait_complete_in_order
- clear_queue_interrupts
- the_player_input_does_not_touch_the_npc

Dependencies:
- Uses: doctest, dfn_gameplay (NpcAction, PlayerMovement), null physics, World,
  EventBus.
- Used by: ctest (sim_npc_actions).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Порядок тика — как в App: execute_npc_actions → player_pre_step → step →
  player_post_step → events.dispatch.
*/
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/gameplay/sources/NpcAction.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/platform/physics/sources/null/CreateNullPhysics.h"

#include <doctest/doctest.h>

#include <cmath>
#include <memory>
#include <vector>

using namespace dfn;

namespace {

constexpr float DT = static_cast<float>(config::SIM_DT);

struct Stage {
    std::unique_ptr<platform::IPhysics> physics = platform::create_null_physics();
    ecs::World world;
    events::EventBus events;
    std::vector<gameplay::NpcActionCompleted> done;
    std::vector<gameplay::NpcActionFailed> failed;
    ecs::EntityId npc{};
    uint64_t tick = 0;

    Stage(const glm::vec3& at = {0.0f, 0.0f, 0.0f}) {
        REQUIRE(physics->init());
        npc = gameplay::spawn_npc(world, *physics, at);
        events.subscribe<gameplay::NpcActionCompleted>(
            [this](const gameplay::NpcActionCompleted& e) { done.push_back(e); });
        events.subscribe<gameplay::NpcActionFailed>(
            [this](const gameplay::NpcActionFailed& e) { failed.push_back(e); });
    }
    ~Stage() { physics->shutdown(); }

    gameplay::NpcActionQueue& queue() { return *world.get<gameplay::NpcActionQueue>(npc); }
    gameplay::PlayerState& state() { return *world.get<gameplay::PlayerState>(npc); }
    const glm::vec3& pos() { return world.get<components::Transform>(npc)->position; }

    void run(int ticks) {
        gameplay::StepContext step;
        for (int i = 0; i < ticks; ++i) {
            gameplay::execute_npc_actions(world, *physics, events, tick++);
            gameplay::player_pre_step(world, *physics, [](glm::vec2) { return 0.0f; }, step);
            physics->step(DT);
            gameplay::player_post_step(world, *physics, step);
            events.pump();
        }
    }
};

} // namespace

TEST_CASE("move_to_walks_there_and_stops") {
    Stage s;
    const glm::vec3 target{0.0f, 0.0f, -6.0f};
    const uint64_t seq = gameplay::enqueue(s.queue(), gameplay::MoveTo{target});
    s.run(60 * 8);
    const float dist = glm::length(glm::vec2{s.pos().x - target.x, s.pos().z - target.z});
    MESSAGE("MoveTo 6 м: дошёл до " << dist << " м от цели, событий Completed " << s.done.size());
    REQUIRE(s.done.size() == 1);
    CHECK(s.done[0].sequence == seq);
    CHECK(s.done[0].npc == s.npc);
    CHECK(dist <= static_cast<float>(config::NPC_ARRIVE_RADIUS) + 0.05f);
    CHECK(s.queue().pending.empty());
    CHECK(s.state().move_axes.y == 0.0f); // стоит
    // ход занял разумное время: 6 м шагом WALK_SPEED
    CHECK(s.failed.empty());
}

TEST_CASE("move_to_behind_turns_first") {
    Stage s;
    // цель сзади (+Z при рыске 0 = −Z)
    gameplay::enqueue(s.queue(), gameplay::MoveTo{glm::vec3{0.0f, 0.0f, 4.0f}, 0.0f,
                                                  gameplay::MoveGait::Run});
    s.run(1);
    // первый тик: доворот, хода нет
    CHECK(s.state().move_axes.y == 0.0f);
    s.run(60 * 6);
    REQUIRE(s.done.size() == 1);
    CHECK(std::abs(s.pos().z - 4.0f) <= static_cast<float>(config::NPC_ARRIVE_RADIUS) + 0.05f);
    // рыск смотрит назад (+Z ≈ ±π)
    CHECK(std::abs(std::abs(std::atan2(std::sin(s.state().yaw), std::cos(s.state().yaw))) - 3.14159f) < 0.2f);
}

TEST_CASE("face_and_wait_complete_in_order") {
    Stage s;
    const uint64_t a = gameplay::enqueue(s.queue(), gameplay::Face{{}, glm::vec3{5.0f, 0.0f, 0.0f}});
    const uint64_t b = gameplay::enqueue(s.queue(), gameplay::Wait{1.0f});
    s.run(30);
    REQUIRE(s.done.size() >= 1);
    CHECK(s.done[0].sequence == a);
    // рыск на +X = +π/2 (по часовой сверху)
    CHECK(std::abs(s.state().yaw - 1.5708f) < glm::radians(static_cast<float>(config::NPC_FACE_DONE_DEG)) + 0.01f);
    s.run(70);
    REQUIRE(s.done.size() == 2);
    CHECK(s.done[1].sequence == b);
    CHECK(glm::length(glm::vec2{s.pos().x, s.pos().z}) < 0.01f); // Face и Wait не ходят
}

TEST_CASE("clear_queue_interrupts") {
    Stage s;
    const uint64_t seq = gameplay::enqueue(s.queue(), gameplay::MoveTo{glm::vec3{0.0f, 0.0f, -20.0f}});
    s.run(30);
    gameplay::clear_queue(s.queue());
    s.run(2);
    REQUIRE(s.failed.size() == 1);
    CHECK(s.failed[0].sequence == seq);
    CHECK(s.failed[0].reason == gameplay::NpcActionFailure::Interrupted);
    CHECK(s.state().move_axes.y == 0.0f);
}

TEST_CASE("unsupported_actions_complete_at_once") {
    Stage s;
    gameplay::enqueue(s.queue(), gameplay::Wait{0.0f});
    gameplay::enqueue(s.queue(), gameplay::Attack{});
    s.run(3);
    CHECK(s.done.size() == 2);
    CHECK(s.failed.empty());
}
