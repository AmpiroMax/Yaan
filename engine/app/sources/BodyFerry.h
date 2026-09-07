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
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/gameplay/sources/StepFeel.h"
#include "engine/platform/physics/interfaces/IPhysics.h"

#include <cstdint>

namespace dfn::app {

struct BodyView {
    bool npc = false;          ///< НПС: взгляда нет, корпус — рыск ходока
    bool third_person = false; ///< игрок от третьего лица: корпус = ps->yaw
    float cam_yaw = 0.0f;      ///< рыск камеры обвода
    uint32_t stand_cam = 0;    ///< камера стенда (0 — нет); 6 — «лицо», взгляд в объектив
};

void ferry_body_drive(anim::BodyDrive& drive, gameplay::PlayerState& ps,
                      const platform::IPhysics* physics, const BodyView& view);

} // namespace dfn::app
