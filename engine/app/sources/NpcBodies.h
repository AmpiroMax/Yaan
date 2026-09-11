/*
Module: engine/app
File: engine/app/sources/NpcBodies.h

Responsibility:
- ТЕЛА НПС (07.09, приёмка лида: болванчик-ходок на стенде): у каждой сущности
  с очередью действий (NpcActionQueue) — своё скиннованное тело
  (SkinnedCharacter, строится ТОЛЬКО через CharacterFactory, как игрок,
  экран и смотровая), свои физические стопы (CharacterFeet) и своя заявка
  локомоции ходоку (gameplay::WalkerLocomotion): НПС ходит теми же ролями,
  переходами и стопами, что игрок.
- Полосы номеров мешей — свои, явным правилом с потолком: NPC k занимает
  NPC_MESH_ID_FIRST + k·NPC_MESH_ID_STRIDE (тело, клинок, 16 частей); НПС
  сверх NPC_BODIES_MAX отказывается вслух, а не берёт чужой номер.

Key items:
- NpcBodies::spawn(): ходок + тело + телеметрия; патруль по точкам (стенд).
- before_step(): паром → advance → заявка ходоку (+ рыск корпуса от клипа).
- after_step(): commit_root по факту капсулы, стопы.
- draws(): тело, клинок, части, хитбоксы — в общий список скиннованных дро.

Dependencies:
- Uses: SkinnedCharacter, CharacterFactory, CharacterFeet, BodyFerry, gameplay
  (NpcAction, PlayerMovement), anim (Body), render, physics.
- Used by: App (тик и кадр), tests/app/NpcBodiesTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Порядок тика: execute_npc_actions → before_step → player_pre_step → step →
  player_post_step → after_step.
- НПС не получают view_valid (контракт третьего лица; лид 07.09).
*/
#pragma once

#include "engine/anim/sources/Rig.h"
#include "engine/app/sources/CharacterFactory.h"
#include "engine/app/sources/CharacterFeet.h"
#include "engine/app/sources/SkinnedCharacter.h"
#include "engine/core/ecs/sources/EntityId.h"
#include "engine/gameplay/sources/NpcAction.h"
#include "engine/render/sources/RenderSystem.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace dfn::ecs {
class World;
}

namespace dfn::app {

/// ПОЛОСЫ НОМЕРОВ МЕШЕЙ НПС (ProcMesh.h: 128..255 — скиннованные персонажи):
/// NPC k — тело NPC_MESH_ID_FIRST + k·NPC_MESH_ID_STRIDE, клинок +1, части
/// +2..+17 (CHARACTER_PARTS_MAX). Три НПС: 192..245.
inline constexpr uint32_t NPC_MESH_ID_FIRST = 192;
inline constexpr uint32_t NPC_MESH_ID_STRIDE = 2 + CHARACTER_PARTS_MAX;
inline constexpr uint32_t NPC_BODIES_MAX = 3;

struct NpcBody {
    ecs::EntityId id{};
    SkinnedCharacter body;
    CharacterBodies bodies;
    CharacterFeet feet;
};

class NpcBodies {
public:
    /// Ходок + тело в точке `at`. `csv` — путь телеметрии (пусто — только отчёт).
    NpcBody* spawn(ecs::World& world, platform::IPhysics& physics,
                   render::RenderSystem& render_system, platform::IRenderer& renderer,
                   const anim::Rig& rig, const std::filesystem::path& body_path,
                   const glm::vec3& at, bool telemetry, const std::string& csv);
    void before_step(ecs::World& world, const platform::IPhysics* physics, float dt);
    void after_step(ecs::World& world, platform::IPhysics* physics, float dt);
    void draws(ecs::World& world, platform::IPhysics* physics, float alpha,
               std::vector<render::RenderSystem::SkinnedDraw>& out);
    void shutdown(platform::IPhysics* physics);
    [[nodiscard]] std::size_t size() const { return bodies_.size(); }
    [[nodiscard]] NpcBody* at(std::size_t i) { return i < bodies_.size() ? bodies_[i].get() : nullptr; }
    [[nodiscard]] bool empty() const { return bodies_.empty(); }

private:
    std::vector<std::unique_ptr<NpcBody>> bodies_;
};

} // namespace dfn::app
