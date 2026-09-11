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
- LocomotionOut и rotate_root_joints переехали сюда из RootMotion.h (снесён фазой 6).
- Числа — реестр (LOCO_STATE_DWELL_S, BODY_TURN_START_DEG, TURN_WARP_*,
  TURN_FIRE_DEG, LOCOMOTION_TEMPO_BAND, STAGGER_*); в коде литералов нет.
*/
#pragma once

#include "engine/anim/sources/Clips.h"
#include "engine/anim/sources/ClipPlayer.h"

#include <array>
#include <cstdint>
#include <span>

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
    /// СКОЛЬКО КАПСУЛА ПРОШЛА ЗА ПРОШЛЫЙ ТИК по горизонтали, м (после физики).
    /// Меньше LOCO_BLOCKED_FRAC от заявки прошлого тика подряд LOCO_BLOCKED_S —
    /// капсула заперта (стена, край мира), ход останавливается: дорожка
    /// корня не хозяин мира, а мир не двигается — и цикл на месте был бы
    /// скольжением опорной стопы со скоростью хода (замер 11.09: 2,2 м/с у
    /// края стенда). Отрицательное — неизвестно, правило молчит.
    float travelled_m = -1.0f;
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
    /// ЗАПЕРТАЯ КАПСУЛА (см. LocoInput::travelled_m): заявка прошлого тика,
    /// сколько подряд мир её не исполняет, и защёлка «в эту сторону не
    /// стартовать» с направлением ввода, в котором заперло. Защёлку снимает
    /// отпущенный ввод или ввод дальше LOCO_BLOCKED_RELEASE_DEG от запертого.
    float request_m = 0.0f;
    float blocked_s = 0.0f;
    bool blocked = false;
    glm::vec3 blocked_dir{0.0f, 0.0f, -1.0f};
    /// Порог запертости; по умолчанию — из реестра. Прибор ставит большое
    /// число в контрольной руке «без правила цикл крутится в стену».
    float blocked_min_s = -1.0f;
    /// КЛАСС НАПРАВЛЕНИЯ — ОТ ВЗГЛЯДА, НЕ ОТ КОРПУСА (§16.10): ввод судится
    /// в системе прицела (view_yaw), когда он есть. Корпус догоняет прицел
    /// BODY_TURN_RATE, и судить ввод против корпуса значило бы гнать класс
    /// через назад → бок → вперёд за один разворот прицела (замер 11.09:
    /// три клипа за 0,3 с на спринте, стопа 9,7 м/с на стыке). Прибор
    /// ставит false в контрольной руке.
    bool dir_by_view = true;
};

/// ОДИН ТИК: решения на входах/концах, часы, темп. `lib` — библиотека тела.
void loco_step(const ClipLibrary& lib, const LocoInput& in, float dt, LocoMachine& m);

/// Ход и рыск корня за прошедший тик (prev_phase → phase) из дорожки текущей
/// роли, с варпом рыска поворота. Ноль у клипов без дорожки.
[[nodiscard]] RootDelta loco_root_delta(const ClipLibrary& lib, const LocoMachine& m);

/// Рыск тела принадлежит клипу (поворот на месте или клип с рыском в
/// дорожке): сим не доворачивает корпус к вводу.
[[nodiscard]] bool loco_yaw_owned_by_clip(const ClipLibrary& lib, const LocoMachine& m);

/// ЗАЯВКА ЛОКОМОЦИИ ЗА ТИК — что зона просит у мира (§16.4).
struct LocomotionOut {
    /// Смещение корня за тик в системе тела (y = 0), метры — дорожка корня
    /// плюс скольжение поставленной физической стопы.
    glm::vec3 root_delta_model{0.0f};
    /// Фаза шага [0,1) — часы этой зоны, для боба камеры и событий.
    float phase = 0.0f;
    /// Рыск корпуса за тик из дорожки корня, рад (+ по часовой, как рыск сим'а).
    float root_yaw_delta = 0.0f;
    /// Постановка стопы на этом тике: [0] левая, [1] правая.
    std::array<bool, 2> footfall{};
    /// Стопа в опоре по расписанию контактов клипа.
    std::array<bool, 2> planted{};
    /// Класс направления хода — приборам и приложению (третье лицо).
    MoveDir dir = MoveDir::Forward;
    /// Рыск корпуса принадлежит клипу (поворот на месте): ни сим, ни
    /// приложение корпус не доворачивают.
    bool yaw_owned_by_clip = false;
    /// Заявка вербатим: смещение капсулы = root_delta целиком.
    bool verbatim = false;
    /// Ложь — заявки нет (нет клипов, воздух, поза, DFN_ROOT_TRACK=0):
    /// сим двигает капсулу от модели скорости ввода.
    bool valid = false;
};

/// ПОВОРОТ КОРНЕВЫХ СУСТАВОВ ПОЗЫ на `yaw` (рад, знак сим'а): остаток варпа
/// поворота на месте — нарисованный корпус смотрит туда же, куда едет
/// капсула. yaw = 0 — no-op бит-в-бит.
void rotate_root_joints(const skel::Skeleton& skeleton, std::span<const int32_t> roots, float yaw,
                        std::span<JointLocal> sample);

struct TurnPick {
    ClipRole role = ClipRole::Idle;
    float warp = 1.0f;
    bool ok = false;
};
/// Ближайший по углу клип поворота и варп рыска для заказа `want` (рад, знак
/// сим'а). Нет клипов — ok = false.
[[nodiscard]] TurnPick loco_pick_turn(const ClipLibrary& lib, float want);

} // namespace dfn::anim
