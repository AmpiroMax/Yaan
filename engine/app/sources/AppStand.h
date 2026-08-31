/*
Module: engine/app
File: engine/app/sources/AppStand.h

Responsibility:
- THE CHARACTER STAND: the four fixed camera poses and the fixed clip queue
  that every acceptance frame of the character is shot from.

Key items:
- StandCamera / stand_camera(): five poses (front, profile, three-quarter,
  close, hand) selected by DFN_STAND_CAM=1..5.
- StandStep / stand_sequence_at(): the queue Idle -> Walk -> Jog -> Sprint ->
  Jump -> Crouch -> Sit -> WEAPON DRAWN (idle, walk, run) -> the long walk onto
  the stand's slope, as PLAYER INPUT at a fixed time, selected by
  DFN_STAND_SEQ=1.
- STAND_SEQUENCE_S: how long the whole queue lasts.

Dependencies:
- Uses: glm, std. Nothing else -- pure data and pure functions, so the tests
  can read the queue without a window.
- Used by: engine/app (App: the door wiring), tests.

Notes:
- THE QUEUE COMES BACK TO WHERE IT STARTED. Every gear runs out and back in
  equal halves, so the net displacement over the whole queue is about zero and
  the figure is still beside the bench when the sit phase arrives. The first
  version ran forward for fifteen seconds at up to RUN_SPEED, ended fifty
  metres away with no bench in reach, and photographed a man standing in a
  field where a man sitting was ordered.
- WHY A STAND AND NOT A CITY (owner, 31.08, Rule 17a): a test figure standing
  in Whiterun is a test figure the owner sees in his game. Acceptance frames
  of the character are shot HERE and nowhere else. The stand is also the only
  way two runs can be compared at all -- a bot walking a city meets different
  ground, different light and different props every time, and the difference
  between two arms of a comparison then contains the world.
- THE QUEUE IS INPUT, NOT POSE. It writes move_axes, the gear modifiers, jump,
  crouch and interact -- the same intents a hand on the keyboard writes -- so
  the frames show the real movement code choosing the real clip. A queue that
  set the clip directly would photograph this file instead of the engine.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure functions, no clock: the time is a parameter. Two runs of the same
  door on the same commit must produce the same frames, and that is only true
  while nothing here reads anything but its arguments.
*/

#pragma once

#include <cstdint>
#include <glm/vec2.hpp>

namespace dfn::app {

/// One fixed camera pose for the stand, expressed the way the third-person
/// boom already speaks: an azimuth around the character, a pitch, and the
/// boom's own reach and lift.
struct StandCamera {
    float orbit_yaw_deg = 0.0f;  ///< 0 = behind the character, +90 = its left
    float orbit_pitch_deg = 0.0f;
    float back_m = 3.2f;         ///< CameraBoomDesc::back
    float lift_m = 0.55f;        ///< CameraBoomDesc::lift
    const char* label = "";
};

inline constexpr uint32_t STAND_CAMERA_COUNT = 5;

/// `n` is 1..STAND_CAMERA_COUNT (DFN_STAND_CAM). Out of range returns the
/// profile pose, which is the one a gait is read from.
[[nodiscard]] StandCamera stand_camera(uint32_t n);

/// One instant of the queue, as player input.
struct StandStep {
    glm::vec2 move{0.0f, 0.0f}; ///< x = strafe right, y = forward
    bool jog = false;
    bool run = false;
    bool jump = false;      ///< edge: true on the first tick of the window only
    bool crouch = false;
    bool interact = false;  ///< edge: sit down / stand up
    /// WHERE THE FIGURE FACES, radians in sim's convention. Written straight
    /// into PlayerState::yaw and NOT as mouse pixels, deliberately: a look
    /// intent is measured in PIXELS and turns by the player's SENSITIVITY
    /// SETTING, so a queue that turned with the mouse would photograph a
    /// setting. The stand must photograph the character.
    float face_yaw = 0.0f;
    const char* label = "idle";
    /// ОРУЖИЕ В РУКАХ — УРОВЕНЬ, А НЕ ФРОНТ, в отличие от прыжка и E.
    /// Клавиша T переключает, но очередь не нажимает клавиш: она ОБЪЯВЛЯЕТ
    /// состояние, и объявление идемпотентно. Фронт здесь означал бы, что кадр,
    /// снятый на тик позже границы фазы, показывает другое состояние.
    ///
    /// И ПОСЛЕДНИМ ПОЛЕМ, а не рядом с `interact`, где оно по смыслу: каждая
    /// строка очереди — агрегатный инициализатор по ПОРЯДКУ полей, и поле,
    /// вставленное в середину, молча сдвигает рыск и подпись у всех
    /// четырнадцати прежних фаз.
    bool weapon = false;
    /// ВСТАТЬ, ЕСЛИ СИДИМ — ТОЖЕ УРОВЕНЬ, И ЭТО ПОЧИНКА ЗАМЕРА, А НЕ УДОБСТВО.
    /// Второе нажатие E «чтобы встать» — переключатель: если посадка не
    /// удалась (на стенде она удаётся НЕ КАЖДЫЙ ПРОГОН — прицел сиденья
    /// зависит от тика), то же нажатие УСАЖИВАЕТ. Два прогона одной очереди
    /// давали разные позы на 35-й секунде, то есть приёмочные кадры волны
    /// зависели от жребия. Заявка «стоим» идемпотентна и жребия не имеет.
    bool stand = false;
};

/// THE WHOLE QUEUE, seconds. Each phase is long enough to hold a full cycle of
/// the clip it exercises plus the cross-fade into it.
inline constexpr float STAND_SEQUENCE_S = 64.0f;

/// The queue at `t` seconds. `prev_t` is the previous tick's time and exists
/// only so the EDGE intents (jump, interact) can be true for exactly one tick:
/// a latch held down would jump forever and would sit down and stand up on
/// alternate ticks.
[[nodiscard]] StandStep stand_sequence_at(float prev_t, float t);

/// СКОЛЬКО ЛЕНТА ПОЗ ДЕРЖИТ ОДИН СЛОТ, с.
///
/// ЧЕТЫРЕ, И ЭТО ВЫВЕДЕНО, А НЕ ВЫБРАНО: самый длинный маршрут реестра
/// (упор лёжа -> любая стоячая поза, шесть узлов) идёт 3.15 с, и кадр позы
/// имеет право быть снятым ПОСЛЕ того, как тело доехало. Четыре секунды дают
/// маршруту доехать при любом соседстве и оставляют почти секунду покоя, на
/// которой снимается кадр. Меньше — и лента сняла бы полпути вместо позы.
inline constexpr float POSE_TAPE_DWELL_S = 4.0f;

/// КАКОЙ СЛОТ ПЕРЕБОРА ЛЕНТА ДЕРЖИТ В МОМЕНТ `t`. Слот 0 — живое тело,
/// 1..slot_count-1 — записи реестра по порядку.
///
/// СЛОТ, А НЕ ПОЗА, и это не мелочь: у этого файла нет и не должно быть
/// зависимости от engine/anim — он чистые данные, и его тесты читаются без
/// рига. Перевод слота в запись реестра делает тот, кто реестр и так знает.
[[nodiscard]] uint32_t pose_tape_slot_at(float t, uint32_t slot_count);
/// Сколько длится вся лента, с.
[[nodiscard]] float pose_tape_length_s(uint32_t slot_count);

} // namespace dfn::app
