/*
Module: engine/app
File: engine/app/sources/CharacterFactory.h

Responsibility:
- ОДИН ПУТЬ ПОСТРОЕНИЯ ПЕРСОНАЖА (решение владельца 02.09, вариант В):
  скиннованное тело с рест-позой, решённой по коже, привязкой, библиотекой
  клипов и подогнанными хитбоксами — ПЛЮС его тела в Jolt (коробки частей
  и, где просят, капсула). Мир, экран создания и смотровая зовут ЭТО, и
  никакой отдельной «болванки» для экрана в дереве нет.

Key items:
- CharacterSpec: что строить — риг пропорций, прежняя ли рест-поза, номера
  мешей, куда ставить в мир, чей это entity, нужна ли своя капсула.
- build_character() / build_character_object(): из файла / из объекта в
  памяти (экран собирает тело из бленда и масштаба и не пишет файл, чтобы
  показать).
- release_character(): меши сняты, тела Jolt уничтожены.
- debug_draw_hitboxes(): коробки частей линиями (доза DFN_HITBOX_DRAW).

Dependencies:
- Uses: SkinnedCharacter, BodyHitboxes, engine/render RenderSystem,
  engine/platform IRenderer / IPhysics, engine/anim Rig, core ecs EntityId.
- Used by: AppWorld.cpp (игрок), AppCharGen.cpp / CharGenBody (экран),
  AppViewer.cpp (экспонат), tests.

Notes:
- ПОЧЕМУ КАПСУЛА — ПО ЗАПРОСУ. У игрока капсула уже есть (PlayerState,
  gameplay::spawn_player), и вторая под тем же телом сталкивалась бы с
  первой. У экрана и смотровой хозяина-капсулы нет, и фабрика заводит свою:
  тело в редакторе — та же сущность, что в мире, с теми же телами физики.
- ХИТБОКСЫ СТАВЯТСЯ СРАЗУ, В РЕСТ-ПОЗЕ. До этой волны игрок получал их
  лениво, на первом нарисованном кадре; тело, у которого коробки появляются
  «когда-нибудь», нельзя проверить на входе.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона app (lead) владеет этим файлом.
- Ни одного второго описания тела: всё, что о нём знает фабрика, приходит из
  anim (RestFit, Hitbox) и SkinnedCharacter.
*/

#pragma once

#include "engine/app/sources/BodyHitboxes.h"
#include "engine/app/sources/SkinnedCharacter.h"
#include "engine/anim/sources/BodyGaps.h"
#include "engine/anim/sources/Rig.h"
#include "engine/core/ecs/sources/EntityId.h"
#include "engine/platform/physics/interfaces/IPhysics.h"
#include "engine/platform/render/interfaces/IRenderer.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/RenderSystem.h"

#include <glm/mat4x4.hpp>

#include <cstdint>
#include <filesystem>

namespace dfn::app {

/// НОМЕРА МЕШЕЙ ТРЁХ ХОЗЯЕВ в полосе 128..159 (ProcMesh.h): игрок 128/129,
/// экран создания 130/131, смотровая 132/133. Разные, потому что игрок жив и
/// в меню (пауза → корень), и экран, взявший 128, получил бы отказ «занят».
inline constexpr uint32_t CHARGEN_BODY_MESH_ID = 130;
inline constexpr uint32_t CHARGEN_BLADE_MESH_ID = 131;
inline constexpr uint32_t VIEWER_BODY_MESH_ID = 132;
inline constexpr uint32_t VIEWER_BLADE_MESH_ID = 133;

struct CharacterSpec {
    /// Риг ПРОПОРЦИЙ; стойка реста решается по коже внутри.
    const anim::Rig* proportions = nullptr;
    /// Прежняя коробочная рест-поза (DFN_REST_POSE=legacy) — рука «до».
    bool legacy_rest = false;
    uint32_t mesh_asset = SKINNED_CHARACTER_MESH_ID;
    uint32_t blade_asset = anim::HELD_BLADE_MESH_ID;
    /// Куда тело ставится в мир на входе (та же матрица, что у draw.transform).
    glm::mat4 to_world{1.0f};
    /// Чьё это тело в Jolt (user_data коробок); пустой — ничьё (экран).
    ecs::EntityId owner{};
    /// Заводить ли свою капсулу; у игрока она есть, у экрана и смотровой нет.
    bool make_capsule = false;
    /// Ноги капсулы, мир (низ капсулы).
    glm::vec3 capsule_feet{0.0f};
};

/// ЧТО ПОСТРОИЛА ФАБРИКА, кроме самого тела: коробки Jolt и своя капсула.
struct CharacterBodies {
    BodyHitboxes hitboxes;
    platform::CharacterHandle capsule{};
};

/// Из файла. False (и причина в потоке ошибок) — тело не построено и пусто.
[[nodiscard]] bool build_character(SkinnedCharacter& body, CharacterBodies& bodies,
                                   render::RenderSystem& render_system,
                                   platform::IRenderer& renderer,
                                   platform::IPhysics* physics,
                                   const std::filesystem::path& path,
                                   const CharacterSpec& spec);

/// Из объекта в памяти; `label` — имя для журнала и для выпечки.
[[nodiscard]] bool build_character_object(SkinnedCharacter& body, CharacterBodies& bodies,
                                          render::RenderSystem& render_system,
                                          platform::IRenderer& renderer,
                                          platform::IPhysics* physics,
                                          render::RegistryObject object,
                                          const std::filesystem::path& label,
                                          const CharacterSpec& spec);

void release_character(SkinnedCharacter& body, CharacterBodies& bodies,
                       render::RenderSystem& render_system, platform::IRenderer& renderer,
                       platform::IPhysics* physics);

/// ПРИБОР НА ПУТИ ИГРОКА для ЛЮБОГО тела фабрики: зазоры нога↔нога,
/// кисть↔бедро, предплечье↔корпус в рест-позе этого тела — те же вершины, та
/// же привязка, та же нулевая поза, из которых собран его портрет. Экран,
/// смотровая и мир меряются ОДНИМ вызовом (правило 47).
[[nodiscard]] anim::BodyGaps character_rest_gaps(const SkinnedCharacter& body);

/// КОРОБКИ ЧАСТЕЙ ЛИНИЯМИ, в мире (доза DFN_HITBOX_DRAW=1): двенадцать рёбер
/// на коробку, три оси на сферу. Линии одолжены у IRenderer::debug_line и
/// живут один кадр.
void debug_draw_hitboxes(platform::IRenderer& renderer, const anim::HitboxSet& set,
                         const anim::HitboxPose& pose, const glm::mat4& to_world,
                         uint32_t color_rgba);

} // namespace dfn::app
