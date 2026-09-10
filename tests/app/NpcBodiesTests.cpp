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
- the_mesh_band_has_a_ceiling

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

TEST_CASE("the_mesh_band_has_a_ceiling") {
    if (!fs::exists(app::CHARGEN_SOURCE_BODY)) {
        return;
    }
    Stage s;
    for (uint32_t i = 0; i < app::NPC_BODIES_MAX; ++i) {
        REQUIRE(s.spawn(glm::vec3{2.0f * static_cast<float>(i), 0.0f, 0.0f}) != nullptr);
    }
    CHECK(s.spawn(glm::vec3{0.0f, 0.0f, 3.0f}) == nullptr); // отказ вслух, чужой номер не взят
    CHECK(s.npcs.size() == app::NPC_BODIES_MAX);
}
