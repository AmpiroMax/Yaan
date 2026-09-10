/*
Module: engine/app
File: engine/app/sources/BodyFerry.h

Responsibility:
- ПАРОМ «СИМ → ТЕЛО»: одна функция, которая переписывает состояние ходока
  (gameplay::PlayerState) в привод тела (anim::BodyDrive) — часы шага,
  скорость, передача, рыск корпуса и взгляда, ввод в системе тела, толчок
  капсулы. ОДНА для игрока и НПС (07.09): до этого паром жил в App::tick и
  НПС-тела получили бы второе описание того же.
- Что паром не переносит: обратный паром (наклон глаза в камеру) — только у
  игрока, остаётся в App.

Key items:
- BodyView: кто смотрит (третье лицо / камера стенда) — только для игрока;
  у НПС view_valid = false (контракт третьего лица не трогать; лид 07.09).
- ferry_body_drive(): переписать привод.

Dependencies:
- Uses: anim/Body.h, gameplay/PlayerMovement.h, IPhysics (контакты капсулы).
- Used by: App (игрок), NpcBodies (НПС).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Явный switch передач, никаких static_cast между enum'ами (Rule 35/37).
*/
#pragma once

#include "engine/anim/sources/Body.h"
#include "engine/anim/sources/Locomotion.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/gameplay/sources/StepFeel.h"
#include "engine/platform/physics/interfaces/IPhysics.h"

#include <cstdint>

namespace dfn::app {

struct BodyView {
    bool npc = false;          ///< НПС: взгляд — заказ исполнителя (want_yaw)
    /// ДОРОЖКА КОРНЯ (§16.4) у этого тела: корпус ведут клипы и сим; ложь —
    /// прежний шов: от третьего лица и у НПС корпус = рыск ходока, взгляда
    /// у НПС нет (контрольная рука DFN_ROOT_TRACK=0).
    bool root_track = true;
    bool third_person = false; ///< игрок от третьего лица: ps->yaw — корпус
    float cam_yaw = 0.0f;      ///< рыск камеры обвода
    uint32_t stand_cam = 0;    ///< камера стенда (0 — нет); 6 — «лицо», взгляд в объектив
};

void ferry_body_drive(anim::BodyDrive& drive, gameplay::PlayerState& ps,
                      const platform::IPhysics* physics, const BodyView& view);

/// ЗАЯВКА ЛОКОМОЦИИ — ИЗ ЗОНЫ ПЕРСОНАЖА В СИМ (§16.4), одна функция для игрока
/// и НПС: смещение корня из системы тела в мир (поворот на −рыск корпуса),
/// фаза, постановки, опора, класс направления, владение рыском, вербатим.
/// Невалидный выход — невалидная заявка (капсула едет от ввода).
[[nodiscard]] gameplay::StepContext::LocomotionRequest
ferry_locomotion_request(const anim::LocomotionOut& lo, float body_yaw);

} // namespace dfn::app
