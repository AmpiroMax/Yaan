/*
Module: engine/anim
File: engine/anim/sources/Locomotion.h

Responsibility:
- МАШИНА СОСТОЯНИЙ ЛОКОМОЦИИ (LOCOMOTION_GROUNDED.md §16, 10.09): покой →
  поворот на месте → старт → цикл → остановка. Состояние и клип решаются
  ОДИН РАЗ — на входе в состояние или по концу клипа (active_s /
  handoff_phase), не порогами каждый тик; единственный анти-дребезг —
  минимальная жизнь состояния LOCO_STATE_DWELL_S. Машина владеет часами
  клипа (фаза × темп) и не трогает позу.
- Движение тела — только дорожка корня клипа (RootTrack): рыск и ход между
  двумя фазами. Поворот на месте — ближайший по углу клип (TurnL/R 90,
  Turn180L/R), доигранный до конца, остаток — варпом рыска в полосе
  TURN_WARP_MIN…MAX; один поворот за раз, следующий — после dwell.
- Взгляд (view_valid) стреляет поворот только стоя без ввода; в третьем лице
  вызывающий даёт view_valid = есть ввод (стоящее тело камеру не догоняет —
  решение владельца 10.09). Ввод дальше BODY_TURN_START_DEG от корпуса —
  сначала разворот, потом старт.

Key items:
- LocoState, LocoInput, LocoMachine — plain data (Rule 8).
- loco_step(): один тик машины (решения, часы, темп, варп).
- loco_root_delta(): ход/рыск корня за этот тик из дорожки текущего клипа.
- loco_pick_turn(): выбор клипа поворота и варпа по углу.

Dependencies:
- Uses: ClipPlayer.h (ClipLibrary, ClipEntry, RootTrack, роли), Constants.h.
- Used by: SkinnedCharacter (фаза 3), tests/character/LocomotionTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Числа — реестр (LOCO_STATE_DWELL_S, BODY_TURN_START_DEG, TURN_WARP_*,
  TURN_FIRE_DEG, LOCOMOTION_TEMPO_BAND, STAGGER_*); в коде литералов нет.
*/
#pragma once

#include "engine/anim/sources/Clips.h"
#include "engine/anim/sources/ClipPlayer.h"

#include <cstdint>

#include <glm/glm.hpp>

namespace dfn::anim {

enum class LocoState : uint8_t { Idle = 0, TurnInPlace, Start, Cycle, Stop, Stagger, Air };
[[nodiscard]] const char* loco_state_name(LocoState s);

/// Что машина читает за тик. Всё в системе тела; рыски — сим'а (+ по часовой).
struct LocoInput {
    glm::vec3 want_dir_model{0.0f, 0.0f, -1.0f}; ///< направление ввода в системе тела
    float want_speed_mps = 0.0f;                 ///< заказ передачи (0 — ввода нет)
    Gait gait = Gait::Walk;
    float view_yaw = 0.0f;   ///< куда смотрит камера/прицел (мир)
    float body_yaw = 0.0f;   ///< куда стоит корпус (мир)
    bool view_valid = false; ///< взгляд может заказать поворот на месте
    bool grounded = true;
    float push_mps = 0.0f;   ///< толчок капсулы (Stagger)
};

struct LocoMachine {
    LocoState state = LocoState::Idle;
    ClipRole role = ClipRole::Idle;
    MoveDir dir = MoveDir::Forward;
    /// Часы клипа текущей роли: фаза [0,1] (у одноразовых не заворачивается),
    /// прошлая фаза — для дорожки и событий за тик.
    float phase = 0.0f;
    float prev_phase = 0.0f;
    float rate = 1.0f;          ///< темп клипа (заказ / скорость дорожки в полосе)
    float dwell_s = 0.0f;       ///< сколько живёт текущее состояние
    float since_stagger_s = 1.0e9f;
    /// ПОВОРОТ НА МЕСТЕ: сколько заказано, каким варпом рыска, знак.
    float turn_want_rad = 0.0f;
    float turn_warp = 1.0f;
    int8_t turn_sign = 0;
    bool entered = false;       ///< этот тик — вход в состояние (стык для инерциализации)
    bool cyclic = false;        ///< текущая роль — цикл (петля)
    uint32_t transitions = 0;   ///< счётчик смен клипа (прибор)
    /// Порог жизни состояния; по умолчанию — из реестра. Прибор ставит 0 в
    /// контрольной руке «без dwell дребезг превышает бюджет».
    float dwell_min_s = -1.0f;
};

/// ОДИН ТИК: решения на входах/концах, часы, темп. `lib` — библиотека тела.
void loco_step(const ClipLibrary& lib, const LocoInput& in, float dt, LocoMachine& m);

/// Ход и рыск корня за прошедший тик (prev_phase → phase) из дорожки текущей
/// роли, с варпом рыска поворота. Ноль у клипов без дорожки.
[[nodiscard]] RootDelta loco_root_delta(const ClipLibrary& lib, const LocoMachine& m);

/// Рыск тела принадлежит клипу (поворот на месте или клип с рыском в
/// дорожке): сим не доворачивает корпус к вводу.
[[nodiscard]] bool loco_yaw_owned_by_clip(const ClipLibrary& lib, const LocoMachine& m);

struct TurnPick {
    ClipRole role = ClipRole::Idle;
    float warp = 1.0f;
    bool ok = false;
};
/// Ближайший по углу клип поворота и варп рыска для заказа `want` (рад, знак
/// сим'а). Нет клипов — ok = false.
[[nodiscard]] TurnPick loco_pick_turn(const ClipLibrary& lib, float want);

} // namespace dfn::anim
