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
- move_to_follows_the_path_around_a_wall (NPC_NAVIGATION.md §4; контроль —
  без сетки прямая сквозь стену)
- a_blocked_capsule_closes_the_cell_ahead_and_replans (blocked из
  WalkerLocomotion прошлого тика; контроль — без blocked перепланов нет)
- the_higher_id_walker_yields_and_the_player_never_does

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
#include "engine/gameplay/sources/NavGrid.h"
#include "engine/gameplay/sources/NpcAction.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/platform/physics/sources/null/CreateNullPhysics.h"

#include <doctest/doctest.h>

#include <cmath>
#include <functional>
#include <memory>
#include <string>
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

    // --- СЕТКА ПРОХОДИМОСТИ (§4): плоское поле с ящиками, nullptr — прямая ----
    std::vector<gameplay::NavBox> boxes;
    gameplay::NavGrid grid;
    gameplay::NavContext nav;
    bool use_nav = false;
    static float flat(void*, glm::vec2) { return 0.0f; }
    void build_nav(float span = 16.0f) {
        gameplay::NavInput in;
        in.ground = &flat;
        in.min_xz = {-0.5f * span, -0.5f * span};
        in.max_xz = {0.5f * span, 0.5f * span};
        in.agent = gameplay::nav_agent_from_registry();
        in.boxes = boxes;
        std::string err;
        REQUIRE_MESSAGE(gameplay::nav_build(in, grid, &err), err);
        nav.grid = &grid;
        use_nav = true;
    }
    /// тик с записью следа НПС (для приборов пути)
    std::vector<glm::vec3> trail;
    std::function<void(int)> before_tick;

    void run(int ticks) {
        gameplay::StepContext step;
        for (int i = 0; i < ticks; ++i) {
            if (before_tick) {
                before_tick(i);
            }
            gameplay::execute_npc_actions(world, *physics, events, tick++, use_nav ? &nav : nullptr);
            gameplay::player_pre_step(world, *physics, [](glm::vec2) { return 0.0f; }, step);
            physics->step(DT);
            gameplay::player_post_step(world, *physics, step);
            events.pump();
            trail.push_back(pos());
        }
    }
};

/// Сколько точек следа лежит в прямоугольнике ящика, раздутом на margin.
int trail_hits(const std::vector<glm::vec3>& trail, const gameplay::NavBox& b, float margin) {
    int n = 0;
    for (const glm::vec3& p : trail) {
        if (std::abs(p.x - b.center.x) <= b.half_extents.x + margin && std::abs(p.z - b.center.z) <= b.half_extents.z + margin) {
            ++n;
        }
    }
    return n;
}

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
    // рыск смотрит в заднюю полуплоскость (+Z): пришёл по дуге, курс в конце —
    // куда была цель с последней точки, не строго ±π
    CHECK(std::abs(std::atan2(std::sin(s.state().yaw), std::cos(s.state().yaw))) > 1.5708f);
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

TEST_CASE("move_to_follows_the_path_around_a_wall") {
    // стена 4 м поперёк прямой от (0,0,0) к (0,0,−6); нулевая физика сквозь
    // стену пустила бы — путь по сетке обязан её обойти
    const gameplay::NavBox wall{{0.0f, 1.0f, -3.0f}, {2.0f, 1.0f, 0.1f}, 0.0f};
    const glm::vec3 target{0.0f, 0.0f, -6.0f};
    for (const bool with_nav : {true, false}) {
        Stage s;
        s.boxes.push_back(wall);
        if (with_nav) {
            s.build_nav();
        }
        const uint64_t seq = gameplay::enqueue(s.queue(), gameplay::MoveTo{target});
        s.run(60 * 12);
        const float dist = glm::length(glm::vec2{s.pos().x - target.x, s.pos().z - target.z});
        const int hits = trail_hits(s.trail, wall, 0.0f);
        float walked = 0.0f;
        for (std::size_t i = 1; i < s.trail.size(); ++i) {
            walked += glm::length(glm::vec2{s.trail[i].x - s.trail[i - 1].x, s.trail[i].z - s.trail[i - 1].z});
        }
        const gameplay::NpcNavReport r = gameplay::npc_nav_report(s.world, s.npc);
        MESSAGE((with_nav ? "по сетке" : "контроль, прямая") << ": дошёл до " << dist << " м, прошёл " << walked
                << " м, тиков в стене " << hits << ", путь " << r.path_m << " м из " << r.waypoints << " точек, перепланов "
                << r.replans);
        REQUIRE(s.done.size() == 1);
        CHECK(s.done[0].sequence == seq);
        CHECK(dist <= static_cast<float>(config::NPC_ARRIVE_RADIUS) + 0.05f);
        if (with_nav) {
            CHECK(hits == 0);
            CHECK(walked > 6.5f);
            CHECK(r.waypoints >= 3);
            CHECK(r.replans == 0);
        } else {
            CHECK(hits > 0); // прямая режет стену
            CHECK(walked < 6.5f);
        }
    }
}

TEST_CASE("a_blocked_capsule_closes_the_cell_ahead_and_replans") {
    // стена, которой сетка НЕ знает (утварь, LAYER_LOOSE): прямая свободна,
    // но тело сообщает blocked — исполнитель закрывает столбец впереди и
    // планирует заново; контроль — без сигнала перепланов нет
    const glm::vec3 target{0.0f, 0.0f, -6.0f};
    for (const bool signal : {true, false}) {
        Stage s;
        s.build_nav();
        s.world.add(s.npc, gameplay::WalkerLocomotion{});
        int blocked_ticks = 0;
        s.before_tick = [&](int i) {
            auto* w = s.world.get<gameplay::WalkerLocomotion>(s.npc);
            // после старта (тик 30) три тика подряд «заперто», заявка есть, хода нет
            const bool now = signal && i >= 30 && i < 33;
            w->request = {};
            w->request.valid = now;
            w->request.blocked = now;
            w->request.verbatim = now;
            blocked_ticks += now ? 1 : 0;
        };
        gameplay::enqueue(s.queue(), gameplay::MoveTo{target});
        s.run(60 * 10);
        const gameplay::NpcNavReport r = gameplay::npc_nav_report(s.world, s.npc);
        const float dist = glm::length(glm::vec2{s.pos().x - target.x, s.pos().z - target.z});
        MESSAGE((signal ? "с сигналом" : "контроль") << ": заперто тиков " << blocked_ticks << ", перепланов " << r.replans
                << ", закрытых столбцов " << r.blocks << ", дошёл до " << dist << " м, точек пути " << r.waypoints);
        REQUIRE(s.done.size() == 1);
        CHECK(dist <= static_cast<float>(config::NPC_ARRIVE_RADIUS) + 0.05f);
        if (signal) {
            CHECK(r.replans == 1); // три запертых тика подряд — один переплан (NAV_REPLAN_GRACE_S)
            CHECK(s.failed.empty());
        } else {
            CHECK(r.replans == 0);
        }
    }
    // сверх NAV_REPLAN_MAX — тупик, честный PathBlocked
    Stage s;
    s.build_nav();
    s.world.add(s.npc, gameplay::WalkerLocomotion{});
    s.before_tick = [&](int i) {
        auto* w = s.world.get<gameplay::WalkerLocomotion>(s.npc);
        w->request = {};
        w->request.valid = i >= 30;
        w->request.blocked = i >= 30;
        w->request.verbatim = i >= 30;
    };
    gameplay::enqueue(s.queue(), gameplay::MoveTo{target});
    s.run(60 * 8);
    MESSAGE("заперто навсегда: провалов " << s.failed.size() << ", перепланов " << gameplay::npc_nav_report(s.world, s.npc).replans);
    REQUIRE(s.failed.size() == 1);
    CHECK(s.failed[0].reason == gameplay::NpcActionFailure::PathBlocked);
}

TEST_CASE("the_higher_id_walker_yields_and_the_player_never_does") {
    // два НПС навстречу по одной прямой: уступает тот, чей EntityId больше
    Stage s;
    s.build_nav();
    const ecs::EntityId second = gameplay::spawn_npc(s.world, *s.physics, {0.0f, 0.0f, -6.0f});
    gameplay::enqueue(s.queue(), gameplay::MoveTo{{0.0f, 0.0f, -6.0f}});
    gameplay::enqueue(*s.world.get<gameplay::NpcActionQueue>(second), gameplay::MoveTo{{0.0f, 0.0f, 0.0f}});
    s.run(60 * 12);
    const gameplay::NpcNavReport a = gameplay::npc_nav_report(s.world, s.npc);
    const gameplay::NpcNavReport b = gameplay::npc_nav_report(s.world, second);
    MESSAGE("первый (id " << s.npc.index << "): уступал " << a.yields << " тиков, перепланов " << a.replans << "; второй (id "
            << second.index << "): уступал " << b.yields << ", перепланов " << b.replans << "; событий " << s.done.size());
    CHECK(s.done.size() == 2);
    CHECK(a.yields == 0);       // меньший id идёт
    CHECK(b.yields > 0);        // больший уступает, пока встречный проходит
    CHECK(b.yields <= static_cast<uint32_t>(static_cast<float>(config::NAV_YIELD_S) / DT) + 2); // прошёл раньше срока
    // игрок (ходок без очереди) на пути — НПС уступает ему всегда и обходит
    Stage p;
    p.build_nav();
    const ecs::EntityId player = gameplay::spawn_player(p.world, *p.physics, {0.0f, 0.0f, -3.0f});
    (void)player;
    gameplay::enqueue(p.queue(), gameplay::MoveTo{{0.0f, 0.0f, -6.0f}});
    p.run(60 * 12);
    const gameplay::NpcNavReport c = gameplay::npc_nav_report(p.world, p.npc);
    const float dist = glm::length(glm::vec2{p.pos().x, p.pos().z + 6.0f});
    MESSAGE("игрок на пути: уступал " << c.yields << " тиков, перепланов " << c.replans << ", дошёл до " << dist << " м");
    CHECK(p.done.size() == 1);
    CHECK(c.yields > 0);
    CHECK(c.replans >= 1);
    CHECK(dist <= static_cast<float>(config::NPC_ARRIVE_RADIUS) + 0.05f);
}
