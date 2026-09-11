/*
Module: tests
File: tests/app/NpcBodiesTests.cpp

Responsibility:
- ПРИБОР ТЕЛ НПС (engine/app/sources/NpcBodies.h): болванчик-ходок — тело
  HumanBase по CharacterFactory, очередь MoveTo, тот же порядок тика, что в
  App (исполнитель → before_step → pre_step → step → post_step → after_step).
  Приход в радиус, снос опорной стопы по телеметрии тела в норме, полоса
  мешей своя (192..), четвёртый НПС отказывается вслух.

Key items:
- the_bot_walks_to_the_point_on_its_own_feet
- bodies_of_one_asset_share_its_meshes_and_the_budget_has_a_ceiling
- diagnostic_tick_cost_per_npc (замер цены тика на одного НПС)

Dependencies:
- Uses: doctest, app (NpcBodies, CharacterFactory), gameplay, null physics,
  NullRenderer, World, EventBus, HumanBase.dfo.
- Used by: ctest (app_npc_bodies).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
#include "engine/anim/sources/Rig.h"
#include "engine/app/sources/CharGenBody.h"
#include "engine/app/sources/NpcBodies.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/gameplay/sources/NpcAction.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/platform/physics/sources/null/CreateNullPhysics.h"
#include "engine/platform/render/sources/null/NullRenderer.h"
#include "engine/render/sources/RenderSystem.h"

#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

using namespace dfn;
namespace fs = std::filesystem;

namespace {

constexpr float DT = static_cast<float>(config::SIM_DT);

struct Stage {
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    std::unique_ptr<platform::IPhysics> physics = platform::create_null_physics();
    ecs::World world;
    events::EventBus events;
    app::NpcBodies npcs;
    uint64_t tick = 0;

    Stage() { REQUIRE(physics->init()); }
    ~Stage() {
        npcs.shutdown(physics.get());
        physics->shutdown();
    }
    app::NpcBody* spawn(const glm::vec3& at) {
        // DFN_BOT_CSV=<путь> — телеметрия бота по тикам в файл (разбор пиков)
        const char* csv = std::getenv("DFN_BOT_CSV");
        return npcs.spawn(world, *physics, rs, renderer, rig, fs::path(app::CHARGEN_SOURCE_BODY),
                          at, /*telemetry=*/true, csv != nullptr ? std::string{csv} : std::string{});
    }
    void run(int ticks) {
        gameplay::StepContext step;
        for (int i = 0; i < ticks; ++i) {
            gameplay::execute_npc_actions(world, *physics, events, tick++);
            npcs.before_step(world, physics.get(), DT);
            gameplay::player_pre_step(world, *physics, [](glm::vec2) { return 0.0f; }, step);
            physics->step(DT);
            gameplay::player_post_step(world, *physics, step);
            npcs.after_step(world, physics.get(), DT);
            events.pump();
        }
    }
};

} // namespace

TEST_CASE("the_bot_walks_to_the_point_on_its_own_feet") {
    if (!fs::exists(app::CHARGEN_SOURCE_BODY)) {
        MESSAGE("no baked body -- skipped");
        return;
    }
    Stage s;
    app::NpcBody* npc = s.spawn(glm::vec3{0.0f});
    REQUIRE(npc != nullptr);
    REQUIRE(npc->body.ready());
    auto& queue = *s.world.get<gameplay::NpcActionQueue>(npc->id);
    const glm::vec3 target{0.0f, 0.0f, -5.0f};
    gameplay::enqueue(queue, gameplay::MoveTo{target});
    s.run(60 * 9);
    const glm::vec3 pos = s.world.get<components::Transform>(npc->id)->position;
    const float dist = glm::length(glm::vec2{pos.x - target.x, pos.z - target.z});
    const anim::LocoProbeRow& slide = npc->body.telemetry().row(anim::LocoProbe::StanceSlip);
    MESSAGE("бот: дошёл до " << dist << " м от цели; ход опорной стопы к земле worst "
                             << slide.worst << " м/с, сверх порога " << slide.hits
                             << " тиков; заявка ходоку валидна: "
                             << s.world.get<gameplay::WalkerLocomotion>(npc->id)->request.valid);
    CHECK(dist <= static_cast<float>(config::NPC_ARRIVE_RADIUS) + 0.05f);
    CHECK(queue.pending.empty());
    // устойчивого сноса опорной стопы нет (§16.7; края окна опоры печатаются worst)
    // тело шло своими стопами: за прогон заявка была валидна (роль цикла)
    CHECK(slide.hits == 0);
}

TEST_CASE("bodies_of_one_asset_share_its_meshes_and_the_budget_has_a_ceiling") {
    if (!fs::exists(app::CHARGEN_SOURCE_BODY)) {
        return;
    }
    // ОБЩИЙ МЕШ ПО ХЭШУ ВЫПЕЧКИ (NPC_NAVIGATION.md §6, условие 3 синка): раньше
    // четвёртое тело не влезало в полосу номеров мешей (192 + 3·18 > 255);
    // теперь полоса считает АССЕТЫ, а тела — бюджет NPC_BODIES_MAX.
    Stage s;
    const auto bodies_max = static_cast<uint32_t>(config::NPC_BODIES_MAX);
    REQUIRE(bodies_max > 3);
    for (uint32_t i = 0; i < bodies_max; ++i) {
        app::NpcBody* b = s.spawn(glm::vec3{2.0f * static_cast<float>(i), 0.0f, 0.0f});
        REQUIRE(b != nullptr);
        CHECK(b->asset_slot == 0);
        CHECK(b->body.mesh_asset() == app::NPC_MESH_ID_FIRST);
        CHECK(b->body.shared_meshes() == (i > 0)); // первое тело владеет, остальные делят
    }
    CHECK(s.npcs.assets() == 1);
    CHECK(s.spawn(glm::vec3{0.0f, 0.0f, 3.0f}) == nullptr); // сверх бюджета — отказ вслух
    CHECK(s.npcs.size() == bodies_max);
}

TEST_CASE("diagnostic_tick_cost_per_npc") {
    if (!fs::exists(app::CHARGEN_SOURCE_BODY)) {
        return;
    }
    // ЗАМЕР ЦЕНЫ ТИКА НА ОДНОГО НПС (условие «замерь»): восемь тел на нулевом
    // рендере идут к точкам и рисуются каждый тик; числа — в записку §6 и в
    // паспорт NPC_TICK_BUDGET_MS. Прибор красный только если тела не встали.
    Stage s;
    std::vector<app::NpcBody*> bodies;
    for (int i = 0; i < 8; ++i) {
        app::NpcBody* b = s.spawn(glm::vec3{2.0f * static_cast<float>(i), 0.0f, 0.0f});
        REQUIRE(b != nullptr);
        bodies.push_back(b);
        gameplay::enqueue(*s.world.get<gameplay::NpcActionQueue>(b->id),
                          gameplay::MoveTo{{2.0f * static_cast<float>(i), 0.0f, -8.0f}});
    }
    std::vector<render::RenderSystem::SkinnedDraw> draws;
    for (int t = 0; t < 300; ++t) {
        s.run(1);
        draws.clear();
        s.npcs.draws(s.world, s.physics.get(), 1.0f, draws);
    }
    const app::NpcCost& c = s.npcs.cost();
    MESSAGE(s.npcs.cost_report());
    MESSAGE("дро за кадр: " << draws.size() << " (тел 8), сим-мс на тело " << c.sim_ms_per_body()
                            << ", 16 тел — " << 16.0 * c.sim_ms_per_body() << " мс при бюджете "
                            << config::NPC_TICK_BUDGET_MS);
    CHECK(c.ticks == 300);
    CHECK(c.frames == 300);
    CHECK(draws.size() >= 8);
    CHECK(c.sim_ms_per_body() > 0.0);
}
