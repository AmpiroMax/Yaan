/*
Module: tests
File: tests/sim/NpcBehaviourTests.cpp

Responsibility:
- ПРИБОР ПОВЕДЕНИЙ НПС (engine/gameplay/sources/NpcBehaviour.cpp,
  NPC_NAVIGATION.md §5): система только ставит действия в очередь (правило
  15) — Transform и PlayerState после её тика не тронуты; брожение
  детерминировано (то же зерно — тот же след, другое — другой), держится в
  радиусе и на проходимом этаже сетки (контроль без сетки — точки в стене);
  патруль обходит точки по порядку и по кругу; следование держит
  дистанцию и перезаказывает, когда цель ушла; взгляд ставит Face один раз,
  а не каждый тик (контроль — без порога NPC_LOOK_REFACE_DEG шторм).

Key items:
- the_system_only_enqueues
- wander_is_deterministic_and_stays_in_the_circle
- wander_on_the_grid_avoids_the_wall
- patrol_visits_the_points_in_order_and_loops
- follow_keeps_the_distance_and_requeues_when_the_target_moves
- look_at_nearby_faces_the_player_once

Dependencies:
- Uses: doctest, dfn_gameplay (NpcBehaviour, NpcAction, NavGrid,
  PlayerMovement), null physics, World, EventBus, реестр.
- Used by: ctest (sim_npc_behaviours).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Порядок тика — как в App: run_npc_behaviours → execute_npc_actions →
  player_pre_step → step → player_post_step → events.pump.
*/
#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/gameplay/sources/NavGrid.h"
#include "engine/gameplay/sources/NpcAction.h"
#include "engine/gameplay/sources/NpcBehaviour.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/platform/physics/sources/null/CreateNullPhysics.h"

#include <doctest/doctest.h>

#include <cmath>
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
    std::vector<gameplay::NavBox> boxes;
    gameplay::NavGrid grid;
    gameplay::NavContext nav;
    bool use_nav = false;
    std::vector<glm::vec3> trail;
    gameplay::NpcBehaviourReport total;
    uint32_t total_moves = 0;
    uint32_t total_waits = 0;

    explicit Stage(const glm::vec3& at = {0.0f, 0.0f, 0.0f}) {
        REQUIRE(physics->init());
        npc = gameplay::spawn_npc(world, *physics, at);
        events.subscribe<gameplay::NpcActionCompleted>([this](const gameplay::NpcActionCompleted& e) { done.push_back(e); });
        events.subscribe<gameplay::NpcActionFailed>([this](const gameplay::NpcActionFailed& e) { failed.push_back(e); });
    }
    ~Stage() { physics->shutdown(); }

    static float flat(void*, glm::vec2) { return 0.0f; }
    void build_nav(float span = 24.0f) {
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
    gameplay::NpcBehaviour& beh(ecs::EntityId id) { return *world.get<gameplay::NpcBehaviour>(id); }
    gameplay::NpcActionQueue& queue(ecs::EntityId id) { return *world.get<gameplay::NpcActionQueue>(id); }
    const glm::vec3& pos(ecs::EntityId id) { return world.get<components::Transform>(id)->position; }

    void run(int ticks) {
        gameplay::StepContext step;
        for (int i = 0; i < ticks; ++i) {
            const gameplay::NpcBehaviourReport r = gameplay::run_npc_behaviours(world, use_nav ? &grid : nullptr, tick);
            total.enqueued += r.enqueued;
            total_moves += r.moves;
            total_waits += r.waits;
            total.interrupted += r.interrupted;
            total.wander_misses += r.wander_misses;
            gameplay::execute_npc_actions(world, *physics, events, tick++, use_nav ? &nav : nullptr);
            gameplay::player_pre_step(world, *physics, [](glm::vec2) { return 0.0f; }, step);
            physics->step(DT);
            gameplay::player_post_step(world, *physics, step);
            events.pump();
            trail.push_back(pos(npc));
        }
    }
};

} // namespace

TEST_CASE("the_system_only_enqueues") {
    Stage s;
    gameplay::NpcBehaviour b;
    b.mode = gameplay::Wander{{0.0f, 0.0f, 0.0f}, 3.0f, 0.5f};
    b.seed = 7;
    s.world.add(s.npc, b);
    const glm::vec3 before = s.pos(s.npc);
    const float yaw_before = s.world.get<gameplay::PlayerState>(s.npc)->yaw;
    const gameplay::NpcBehaviourReport r = gameplay::run_npc_behaviours(s.world, nullptr, 0);
    CHECK(r.enqueued == 2); // MoveTo + Wait
    CHECK(s.queue(s.npc).pending.size() == 2);
    CHECK(s.pos(s.npc) == before);
    CHECK(s.world.get<gameplay::PlayerState>(s.npc)->yaw == yaw_before);
    // очередь занята — система молчит
    const gameplay::NpcBehaviourReport r2 = gameplay::run_npc_behaviours(s.world, nullptr, 1);
    CHECK(r2.enqueued == 0);
}

TEST_CASE("wander_is_deterministic_and_stays_in_the_circle") {
    struct Run {
        std::vector<glm::vec3> trail;
        std::size_t done = 0;
        std::size_t failed = 0;
        uint32_t enqueued = 0;
    };
    auto run = [](uint64_t seed) {
        Stage s;
        gameplay::NpcBehaviour b;
        b.mode = gameplay::Wander{{0.0f, 0.0f, 0.0f}, 4.0f, 0.3f};
        b.seed = seed;
        s.world.add(s.npc, b);
        s.run(60 * 30);
        return Run{s.trail, s.done.size(), s.failed.size(), s.total.enqueued};
    };
    const Run a = run(11);
    const Run b = run(11);
    const Run c = run(12);
    float worst_r = 0.0f;
    float path = 0.0f;
    for (std::size_t i = 0; i < a.trail.size(); ++i) {
        worst_r = std::max(worst_r, glm::length(glm::vec2{a.trail[i].x, a.trail[i].z}));
        if (i > 0) {
            path += glm::length(glm::vec2{a.trail[i].x - a.trail[i - 1].x, a.trail[i].z - a.trail[i - 1].z});
        }
    }
    MESSAGE("брожение 30 с: целей " << a.done << ", путь " << path << " м, дальше всего от центра " << worst_r
                                    << " м, поставлено " << a.enqueued);
    CHECK(a.trail == b.trail); // то же зерно — тот же след
    CHECK(a.trail != c.trail); // другое — другой
    CHECK(a.done >= 6);
    CHECK(worst_r <= 4.0f + static_cast<float>(config::NPC_ARRIVE_RADIUS) + 0.1f);
    CHECK(path > 10.0f);
    CHECK(a.failed == 0);
}

TEST_CASE("wander_on_the_grid_avoids_the_wall") {
    // стена поперёк круга: с сеткой цели лежат на проходимом этаже и след не
    // входит в стену; контроль без сетки — цели и след сквозь стену
    const gameplay::NavBox wall{{0.0f, 1.0f, 0.0f}, {5.0f, 1.0f, 0.1f}, 0.0f};
    for (const bool with_grid : {true, false}) {
        Stage s({0.0f, 0.0f, 3.0f});
        s.boxes.push_back(wall);
        if (with_grid) {
            s.build_nav();
        }
        gameplay::NpcBehaviour b;
        b.mode = gameplay::Wander{{0.0f, 0.0f, 0.0f}, 4.0f, 0.3f};
        b.seed = 3;
        s.world.add(s.npc, b);
        s.run(60 * 40);
        int in_wall = 0;
        for (const glm::vec3& p : s.trail) {
            if (std::abs(p.x) <= wall.half_extents.x && std::abs(p.z) <= wall.half_extents.z + 0.2f) {
                ++in_wall;
            }
        }
        MESSAGE((with_grid ? "по сетке" : "контроль") << ": целей " << s.done.size() << ", тиков в стене " << in_wall
                << ", промахов брожения " << s.total.wander_misses << ", провалов " << s.failed.size());
        CHECK(s.done.size() >= 4);
        if (with_grid) {
            CHECK(in_wall == 0);
        } else {
            CHECK(in_wall > 0);
        }
    }
}

TEST_CASE("patrol_visits_the_points_in_order_and_loops") {
    Stage s;
    gameplay::Patrol p;
    p.points = {{3.0f, 0.0f, 0.0f}, {3.0f, 0.0f, -3.0f}, {0.0f, 0.0f, -3.0f}, {0.0f, 0.0f, 0.0f}};
    p.pause_s = 0.2f;
    p.pause_at_loop_s = 0.0f;
    gameplay::NpcBehaviour b;
    b.mode = p;
    s.world.add(s.npc, b);
    // порядок прихода: по событиям Completed для MoveTo — позиция в момент события
    std::vector<glm::vec3> arrivals;
    s.events.subscribe<gameplay::NpcActionCompleted>([&](const gameplay::NpcActionCompleted&) {
        arrivals.push_back(s.pos(s.npc));
    });
    s.run(60 * 25);
    // каждая вторая запись — Wait (та же позиция); MoveTo — каждая нечётная
    std::vector<glm::vec3> visits;
    for (std::size_t i = 0; i + 1 < arrivals.size(); i += 2) {
        visits.push_back(arrivals[i]);
    }
    MESSAGE("патруль 25 с: точек пройдено " << visits.size() << ", следующая " << s.beh(s.npc).patrol_next);
    REQUIRE(visits.size() >= 5); // круг из четырёх и дальше — второй круг
    for (std::size_t i = 0; i < visits.size(); ++i) {
        const glm::vec3& want = p.points[i % p.points.size()];
        CHECK(glm::length(glm::vec2{visits[i].x - want.x, visits[i].z - want.z})
              <= static_cast<float>(config::NPC_ARRIVE_RADIUS) + 0.05f);
    }
    // без кольца — доходит до конца и стоит
    Stage t;
    p.loop = false;
    gameplay::NpcBehaviour b2;
    b2.mode = p;
    t.world.add(t.npc, b2);
    t.run(60 * 25);
    CHECK(t.beh(t.npc).patrol_done);
    CHECK(t.done.size() == 8); // 4 MoveTo + 4 Wait, и больше ничего
    // пауза на круг (стендовый бот): Wait один на круг, на точках — нет
    Stage u;
    gameplay::Patrol q = p;
    q.loop = true;
    q.pause_s = 0.0f;
    q.pause_at_loop_s = 1.0f;
    gameplay::NpcBehaviour b3;
    b3.mode = q;
    u.world.add(u.npc, b3);
    u.run(60 * 25);
    MESSAGE("пауза на круг: MoveTo " << u.total_moves << ", Wait " << u.total_waits);
    CHECK(u.total_waits >= 1);
    CHECK(u.total_waits <= u.total_moves / 4 + 1);
}

TEST_CASE("follow_keeps_the_distance_and_requeues_when_the_target_moves") {
    Stage s;
    const ecs::EntityId leader = gameplay::spawn_npc(s.world, *s.physics, {0.0f, 0.0f, -4.0f});
    gameplay::NpcBehaviour b;
    b.mode = gameplay::Follow{leader, 2.0f, 0.0f};
    s.world.add(s.npc, b);
    s.run(60 * 6);
    float dist = glm::length(glm::vec2{s.pos(s.npc).x - s.pos(leader).x, s.pos(s.npc).z - s.pos(leader).z});
    MESSAGE("подошёл: дистанция " << dist << " м, поставлено " << s.total.enqueued << ", сбросов " << s.total.interrupted);
    CHECK(dist <= 2.0f + static_cast<float>(config::NPC_ARRIVE_RADIUS) + 0.1f);
    CHECK(dist >= 2.0f - static_cast<float>(config::NPC_ARRIVE_RADIUS) - 0.1f);
    CHECK(s.total.interrupted == 0);
    // ведущий уходит шагом на 8 м — ведомый перезаказывает по приходу и
    // держит дистанцию (цель за время его MoveTo не уходит дальше slack)
    gameplay::enqueue(s.queue(leader), gameplay::MoveTo{{0.0f, 0.0f, -12.0f}});
    s.run(60 * 12);
    dist = glm::length(glm::vec2{s.pos(s.npc).x - s.pos(leader).x, s.pos(s.npc).z - s.pos(leader).z});
    MESSAGE("после ухода шагом: дистанция " << dist << " м, сбросов " << s.total.interrupted << ", поставлено "
                                            << s.total.enqueued);
    CHECK(dist <= 2.0f + static_cast<float>(config::NPC_ARRIVE_RADIUS) + 0.1f);
    CHECK(s.total.interrupted == 0);
    // ведущий убегает бегом (RUN_SPEED) — за время MoveTo ведомого цель уходит
    // дальше NAV_FOLLOW_SLACK_M: очередь сбрасывается и ставится заново, но не
    // каждый тик
    gameplay::enqueue(s.queue(leader), gameplay::MoveTo{{0.0f, 0.0f, -40.0f}, 0.0f, gameplay::MoveGait::Run});
    const float z_before = s.pos(s.npc).z;
    s.run(60 * 4);
    MESSAGE("после бега ведущего: сбросов " << s.total.interrupted << ", поставлено " << s.total.enqueued << ", ведомый z "
                                            << s.pos(s.npc).z << " (был " << z_before << ")");
    CHECK(s.total.interrupted >= 1);
    CHECK(s.total.interrupted <= 60 * 4 / 10); // реже раза в 10 тиков
    CHECK(s.pos(s.npc).z < z_before - 3.0f);   // идёт следом
}

TEST_CASE("look_at_nearby_faces_the_player_once") {
    Stage s;
    const ecs::EntityId player = gameplay::spawn_player(s.world, *s.physics, {3.0f, 0.0f, 0.0f});
    gameplay::NpcBehaviour b;
    b.mode = gameplay::LookAtNearby{4.0f};
    s.world.add(s.npc, b);
    s.run(60 * 3);
    const float yaw = s.world.get<gameplay::PlayerState>(s.npc)->yaw;
    MESSAGE("взгляд: рыск " << glm::degrees(yaw) << "° (к игроку 90°), поставлено " << s.total.enqueued << " за 3 с");
    CHECK(std::abs(yaw - glm::radians(90.0f)) < glm::radians(static_cast<float>(config::NPC_FACE_DONE_DEG) + 1.0f));
    CHECK(s.total.enqueued == 1); // один Face, не шторм
    // игрок обошёл на 90° — второй Face; ушёл дальше радиуса — молчание.
    // Двигается КАПСУЛА: post_step пишет Transform из неё, прямая запись в
    // Transform откатилась бы следующим тиком.
    const platform::CharacterHandle pc = s.world.get<gameplay::PlayerState>(player)->character;
    s.physics->teleport_character(pc, {0.0f, 0.0f, 3.0f});
    s.run(60 * 3);
    CHECK(s.total.enqueued == 2);
    s.physics->teleport_character(pc, {0.0f, 0.0f, 9.0f});
    s.run(60 * 3);
    CHECK(s.total.enqueued == 2);
    CHECK(s.failed.empty());
}
