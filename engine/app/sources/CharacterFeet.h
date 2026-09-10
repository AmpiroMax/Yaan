/*
Module: engine/app
File: engine/app/sources/CharacterFeet.h

Responsibility:
- ФИЗИЧЕСКИЕ СТОПЫ ПЕРСОНАЖА — половина контракта §12 LOCOMOTION_GROUNDED.md
  на стороне anim/app (платформенная половина — IPhysics: create_foot_body,
  set_foot_kinematic_pose, set_foot_mode, foot_contact; ef64025e). Две
  коробки по хитбоксам стоп. В МАХЕ стопа кинематическая и едет за клипом; на
  ПОСТАНОВКЕ (замок защёлкнулся) становится динамической, и держит её трение
  пары «подошва × земля», а не якорь: на пологом склоне стоит, на крутом или
  на льду ползёт по закону Кулона. Якорь замка каждый тик — от тела физики;
  его ход (скольжение) уходит в корень: тело съезжает вместе со стопой.
- Приказ владельца 04.09: «убрать трюк с замком стопы — физические стопы с
  трением». Замок остаётся тем, чем и был — IK ноги к точке, — но точку
  теперь называет физика.

Key items:
- CharacterFeet::bind(): физика и биты хозяина; тела создаются на первом тике.
- CharacterFeet::tick(): после commit_root и после step(): мах → кинематическая
  поза следующего шага; защёлкнулся → Plant; стоит → якорь и скольжение от
  тела; отпущен → Swing.
- FootPhysicsReport: что стопа сообщила (касание, держит, скольжение, вещество).

Dependencies:
- Uses: IPhysics.h, SkinnedCharacter.h (хитбоксы стоп, расписание опоры, скольжение),
  CollisionLayers.h, PhysicsSubstance.h, Constants.h.
- Used by: App (игрок), tests/app/PhysicalFeetTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Порядок в тике обязателен: advance → шаг физики → commit_root → tick(): тело
  стопы читается ПОСЛЕ шага, кинематическая поза задаётся ДО следующего.
- Дверь DFN_PHYSICAL_FEET=0 — контрольная рука: замок как раньше, тел нет.
- ДОРОЖКА КОРНЯ (LOCOMOTION_GROUNDED.md §16.6): стопы — ДАТЧИКИ. Постановка
  и отрыв — из расписания контактов клипа (LocomotionOut::planted), якорей
  нет; ход поставленного тела за тик — скольжение — уходит в корень. Без
  заявки (DFN_ROOT_TRACK=0, воздух, поза) тела только следуют за клипом.
*/
#pragma once

#include "engine/core/materials/sources/PhysicsSubstance.h"
#include "engine/platform/physics/interfaces/IPhysics.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

namespace dfn::app {

class SkinnedCharacter;

/// Что стопа сообщила на этом тике (читается после tick()).
struct FootPhysicsReport {
    bool has_body = false;
    bool planted = false;  ///< тело динамическое (Plant)
    bool touching = false;
    bool holds = false;    ///< трение пары держит (по Кулону)
    float slip_mps = 0.0f; ///< измеренное скольжение тела
    glm::vec3 slip_delta{0.0f}; ///< ход якоря за тик, мир (только в Plant)
    core::SubstanceId ground = core::SUBSTANCE_DEFAULT;
    float friction_pair = 0.0f;
    float slope_tan = 0.0f;
};

class CharacterFeet {
public:
    /// Вещество подошвы (пара с землёй: sqrt(μ_подошвы · μ_земли)).
    static constexpr const char* SOLE_SUBSTANCE = "leather";

    void bind(platform::IPhysics* physics, uint64_t owner_bits);
    void set_enabled(bool on);
    [[nodiscard]] bool enabled() const { return enabled_; }
    [[nodiscard]] bool bound() const { return physics_ != nullptr; }

    /// Один тик: после commit_root тела и после step() физики.
    void tick(SkinnedCharacter& body, float dt);
    [[nodiscard]] const FootPhysicsReport& report(std::size_t side) const {
        return report_[side];
    }
    /// Снять тела (до shutdown() физики).
    void shutdown();

private:
    platform::IPhysics* physics_ = nullptr;
    uint64_t owner_ = 0;
    bool enabled_ = true;
    std::array<platform::PhysicsBodyHandle, 2> foot_{};
    std::array<bool, 2> planted_{};
    /// ДОРОЖКА КОРНЯ (§16.6): где тело стопы было тик назад, пока стоит —
    /// его ход и есть скольжение (лёд двигает тело, больше ничто).
    std::array<glm::vec3, 2> body_seen_{};
    std::array<FootPhysicsReport, 2> report_{};
};

} // namespace dfn::app
