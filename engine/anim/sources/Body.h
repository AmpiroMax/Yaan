/*
Module: engine/anim
File: engine/anim/sources/Body.h

Responsibility:
- The rigid-segmented humanoid body in the ECS: spawning a body's segment
  entities, evaluating its pose each fixed tick (locomotion from sim's stride
  clock, showcase clips), the mirror puppet (grill в11), and writing segment
  Transforms for render's ordinary interpolated pass.

Key items:
- BodyRig / BodyDrive / MirrorPuppet: plain-data components (Rule 8), this
  zone's own (only the app composes them; no other zone includes this).
- spawn_body / destroy_body: create/remove the 15 segment entities.
- spawn_mirror_puppet: a second body that mirrors a source body across a
  vertical plane, or floats and cycles showcase clips.
- update_bodies(world): fixed-tick system — pose, FK, segment Transform pairs.
- note_landed(): landing-dip trigger, keyed off sim's Landed event (app ferry).

Dependencies:
- Uses: Rig/Pose/Clips/BodyMesh, core ecs + components, generated constants.
- Used by: engine/app (spawn + per-tick update + drive ferry), tests.

Notes:
- THE DRIVE IS A FERRY, NOT A CLOCK (agreed with sim, 10:08:2026): the app
  copies sim's PlayerState stride phase/speed/etc. into BodyDrive each fixed
  tick. This zone never advances the phase. The DAG is why it is a copy at
  the composition root: anim sits below gameplay and cannot read PlayerState.
- Call update_bodies AFTER player_post_step (same-tick pose, the ViewModel
  precedent: a stale pose reads as the body lagging the camera) and BEFORE
  render. Segments snapshot curr->prev themselves inside update_bodies.
- The player's body hides the HEAD segment (camera sits inside the skull);
  everything else — chest, arms, legs, feet — is deliberately visible
  (user decision в11: full first-person body).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Components are plain data (Rule 8). Segment entities are spawned at init
  paths, never on streaming paths (Rule 11 does not bite here).
*/

#pragma once

#include "engine/anim/sources/Clips.h"
#include "engine/anim/sources/Pose.h"
#include "engine/anim/sources/PoseLibrary.h"
#include "engine/anim/sources/Posture.h"
#include "engine/anim/sources/Rig.h"
#include "engine/core/ecs/sources/EntityId.h"

#include <array>
#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace dfn::ecs {
class World;
}

namespace dfn::anim {

inline constexpr uint8_t SHOWCASE_NONE = 0xFF;

// The body's segment entities, one per bone (Rule 8: ids, not pointers).
struct BodyRig {
    std::array<ecs::EntityId, BONE_COUNT> segments{};
    bool hide_head = false; // first person: camera sits inside the skull
};

// Per-tick drive values, FERRIED by the app from sim's PlayerState (see
// header note). This zone only reads them; the app only writes them.
struct BodyDrive {
    float stride_phase = 0.0f;      // sim's clock, [0,1)
    float step_length_m = 0.0f;     // sim's length(v) model output
    float speed_mps = 0.0f;         // horizontal speed
    // ЖЕЛАЕМАЯ СКОРОСТЬ ПЕРЕДАЧИ, м/с: 0 — ввода нет. Перемещение ведёт клип
    // (docs/design/LOCOMOTION_GROUNDED.md), поэтому роль клипа и его темп
    // выбираются по НАМЕРЕНИЮ, а не по фактической скорости капсулы — та
    // сама следствие клипа, и выбирать по ней клип значило бы замкнуть круг.
    float want_speed_mps = 0.0f;
    // THE GEAR sim CHOSE, not the speed it was derived from. This zone used to
    // re-derive it by comparing speed_mps against WALK_SPEED and RUN_SPEED,
    // which is the two-copies defect (Rule 35) and became a visible one when a
    // third gear landed between the two rows (Rule 37). speed_mps survives for
    // the idle<->moving fade only, which is a question about whether the feet
    // are moving at all and has no gears in it.
    Gait gait = Gait::Walk;
    float facing_yaw = 0.0f;        // radians, sim's yaw convention
    /// НАПРАВЛЕНИЕ ЖЕЛАЕМОГО ХОДА В СИСТЕМЕ ТЕЛА (LOCOMOTION_GROUNDED.md §9):
    /// единичный вектор, лицом в −Z; (0,0,−1) — вперёд, (+1,0,0) — вправо,
    /// (0,0,+1) — назад. Единственный источник направления и для выбора
    /// роли (вперёд / стрейф / назад по углу к лицу), и для корня от опорной
    /// стопы (ось хода). Пишет приложение из ввода игрока или движитель НПС.
    glm::vec3 move_dir_model{0.0f, 0.0f, -1.0f};
    /// ФАКТИЧЕСКИЙ ГОРИЗОНТАЛЬНЫЙ ХОД КОРНЯ ЗА ПРОШЛЫЙ ТИК, м (§11.1): по нему
    /// идут часы клипа на ходу. Отрицательное — неизвестно (часы по времени).
    float travelled_m = -1.0f;
    bool grounded = true;
    float vertical_velocity = 0.0f; // m/s, + up
    float crouch_blend = 0.0f;      // sim's eased 0..1
    // Internal animation state (this zone's, decayed/advanced in update).
    float land_dip = 0.0f;          // 1 at touchdown -> 0
    float anim_time_s = 0.0f;       // idle breathing clock (fixed-tick sum)
    // THE EASED GEAR WEIGHT, and it has TWO READERS ON PURPOSE (Rule 35's
    // state form): this zone leans the trunk by it, and the app ferries THIS
    // FLOAT — not `gait_run_weight(gait)` — into `anim::eye_lean_offset` so
    // sim's camera leans by the same number. They cannot drift, because there
    // is only one of them.
    //
    // WHY IT IS EASED HERE RATHER THAN IN EITHER CAMERA: `gait_run_weight` is
    // a step function, so a gear change moved trunk and eye 0.132 m in ONE
    // tick. Easing either side alone is worse than the pop, and asymmetric —
    // accelerating, the eye leads a body still straightening up, which is
    // SAFER than steady state; decelerating, the body is still leaning while
    // the eye is already back on the axis, and the chest returns to frame for
    // the length of every run->walk. An intermittent defect in one transition
    // direction costs more than a lurch you can see every time.
    // AND IT IS THE LEAN THE TRUNK IS ACTUALLY DRAWN WITH, not the gear alone.
    // The gear weight is eased into `gear_weight` below; `run_weight` is that
    // value AFTER the same "are the feet moving at all" fade the trunk lean
    // gets from the idle->gait blend (`gait_fade`). The two were different
    // numbers until 11:08:2026 and the difference was a defect the user
    // reported: holding LEFT_ALT while STANDING STILL leaned nothing on the
    // body — gait_fade is 0, so gait_pose is not blended in at all — while the
    // camera rode the full gear weight and lunged 66.4 mm forward and 7.2 mm
    // down with no locomotion whatsoever («словно я шеей вперед двигаю»).
    // docs/findings/FINDING_CROUCH_AND_ALT_LEAN.md.
    float run_weight = 0.0f;        // PUBLISHED: gait_fade(speed) * gear_weight
    // The eased gear itself (this zone's integrator). Kept separate from the
    // published value above because an ease must integrate its own state: fold
    // the fade into it and the fade multiplies every tick instead of once.
    float gear_weight = 0.0f;       // eased toward gait_run_weight(gait)
    /// ОРУЖИЕ В РУКАХ (заказ владельца 31.08, пункты 5-6). ЗАЯВКА, а не
    /// картинка: сама картинка едет в ClipPlayback::weapon за
    /// WEAPON_CROSSFADE_S. Ферма как у всего остального привода — пишет
    /// приложение (клавиша T), читает эта зона.
    ///
    /// ПОЧЕМУ ФЛАГ, А НЕ «КАКОЕ ОРУЖИЕ». Модели оружия у нас ещё нет, и
    /// заводить ради неё перечисление значило бы заморозить контракт вокруг
    /// вымысла. Состояний ровно два, и оба видны на кадре: руки свободны или
    /// руки заняты.
    bool weapon_drawn = false;
    /// ПЬЯН (владелец 03.09: «на какую-нибудь кнопку я буду пьянеть», клавиша
    /// H): покой играет пьяный клип (ClipLibrary::drunk_variant); походка
    /// пьяной станет, когда будут клипы Drunk Walk/Run. Заявка приложения.
    bool drunk = false;
    // Showcase override (mirror map techno-demo): SHOWCASE_NONE = live body.
    uint8_t showcase_clip = SHOWCASE_NONE;
    float showcase_time_s = 0.0f;

    // --- ПОЗА МЕБЕЛИ: СИДЕТЬ И ЛЕЖАТЬ (заказ владельца 28.08) --------------
    // ЧТО СЕЙЧАС ЗАКАЗАНО телу, и это ЗАЯВКА, а не состояние картинки:
    // `posture_blend` — единственное, чем поза нарисована, и он доезжает до
    // заявки за POSTURE_BLEND_TIME_S. Одно поле на «хочу» и «нарисовано» дало
    // бы мгновенный скачок скелета на каждом входе и выходе.
    Posture posture = Posture::None;
    /// ЧТО РИСУЕТСЯ, ПОКА БЛЕНДЕР ЕДЕТ ОБРАТНО. Заявка на выходе гаснет
    /// первой (`posture` = None), а тело ещё встаёт — и встаёт ИЗ ТОЙ ПОЗЫ, В
    /// КОТОРОЙ БЫЛО. Без этого поля вставание с кровати шло через СИДЯЧУЮ
    /// позу: рисовальщик спрашивал `posture`, а он уже None, и «не Lie»
    /// означало «Sit». Ведётся здесь же (update_bodies), гаснет вместе с
    /// блендером.
    Posture posture_shown = Posture::None;
    /// 0 — живое тело (локомоция/воздух/присяд), 1 — поза целиком. Ведётся
    /// ЗДЕСЬ (update_bodies), как и `gear_weight`: ease обязан интегрировать
    /// собственное состояние, иначе он множится каждый тик.
    float posture_blend = 0.0f;
    /// ЗЕМЛЯ ПОЗЫ, мировые: точка ПОЛА под сиденьем/лежаком. Отдельно от
    /// Transform владельца НАМЕРЕННО — капсула на время позы паркуется там,
    /// где человек стоял, когда нажал E (она законно стоит на полу и ни во что
    /// не воткнута), а тело рисуется на мебели. Сведение этих двух точек в
    /// одну означало бы телепорт капсулы внутрь лавки.
    glm::vec3 posture_ground{0.0f};
    float posture_yaw = 0.0f;
    /// Высота СИДЕНЬЯ (лежака) над `posture_ground`. Замер с чертежа предмета
    /// (0.45 у лавки, 0.50 у кровати), а не строка мира.
    float posture_height_m = 0.0f;
    /// ВЫХОД, а не вход: где оказался глаз НАРИСОВАННОЙ позы. Пишется в
    /// update_bodies из той же позы и того же корня, которыми только что
    /// поставлены сегменты, — камера и тело не могут разойтись, потому что
    /// число одно (тот же довод, что у `run_weight`).
    glm::vec3 eye_point{0.0f};
    /// Есть ли смысл в `eye_point`. Ложь при posture_blend == 0: у стоящего
    /// глаз ставит sim, и подменять его позой значило бы завести вторую камеру.
    bool eye_valid = false;

    // --- РЕЕСТР ПОЗ (PoseLibrary.h) ---------------------------------------
    /// ГДЕ ТЕЛО СЕЙЧАС ПО РЕЕСТРУ и куда оно едет. Своих часов у перехода
    /// нет — их ведёт update_bodies, как ведёт `gear_weight` и
    /// `posture_blend`, и по той же причине (правило 35: интегратор обязан
    /// жить в одном месте).
    ///
    /// ОТДЕЛЬНО ОТ `posture`, И ЭТО НЕ ДУБЛЬ. `posture` — поза МЕБЕЛИ: её
    /// заводит мир (лавка, кровать), у неё есть своя земля, свой рыск и свой
    /// глаз, потому что тело уезжает НА ПРЕДМЕТ. Реестр же рисует позу ТАМ,
    /// ГДЕ ЧЕЛОВЕК СТОИТ, и мира не спрашивает вовсе. Слить их в одно поле
    /// значило бы, что «лечь на кровать» и «лечь на пол» — одна вещь, а у
    /// них разное всё, кроме слова.
    PoseTransit pose_transit{};
    /// Рисуется ли реестр поверх живого тела. Ложь — тождество: локомоция,
    /// воздух и присяд идут как шли.
    bool pose_active = false;
    /// 0 — реестр только въезжает или уезжает, 1 — рисуется целиком. Ведётся
    /// здесь же и по той же причине, что `posture_blend`.
    float pose_weight = 0.0f;
    /// ВЫСОТА СИДЕНЬЯ ДЛЯ ПОЗ НА СТУЛЕ, м. Приложение ставит её замером того
    /// предмета, к которому подошёл человек; ноль означает «спроси реестр»
    /// (образцовая лавка).
    float pose_seat_height_m = 0.0f;
};

/// СКОЛЬКО ДЛИТСЯ ВЪЕЗД РЕЕСТРА ПОВЕРХ ЖИВОГО ТЕЛА И ВЫЕЗД ОБРАТНО, с.
/// Отдельно от длительностей самих переходов: это не переход из позы в позу,
/// а СМЕНА ВЕДУЩЕГО — с локомоции на реестр. Величина взята у перекладки
/// конечностей (anim::POSE_TO_SUPPORT_S), потому что первое, что делает
/// въезжающий реестр, ровно это и есть.
inline constexpr float POSE_LAYER_BLEND_S = 0.45f;

/// ЧТО РИСУЕТСЯ ПРЯМО СЕЙЧАС: заявка, пока она есть, и последняя нарисованная
/// поза, пока блендер возвращается. ОДИН ОТВЕТ НА ВЕСЬ ДВИЖОК — и рисовальщик,
/// и корень, и длительность спрашивают его, а не `posture` напрямую.
[[nodiscard]] inline Posture drawn_posture(const BodyDrive& d) {
    return d.posture != Posture::None ? d.posture : d.posture_shown;
}

/// СКОЛЬКО ДЛИТСЯ ПЕРЕХОД, с — БОЛЬШЕ НЕ ЗДЕСЬ. Длительность стала свойством
/// ПОЗЫ (anim::posture_transit_s: сесть 0.60, лечь 0.90), потому что лечь —
/// путь длиннее на целое откидывание, и одно число на обе позы читалось бы
/// падением. Само доведение осталось ЛИНЕЙНЫМ, и это по-прежнему намеренно:
/// экспонента не доходит до нуля, и признак `eye_valid` остался бы истинным
/// навсегда после первого же вставания. Форма движения живёт не здесь, а в
/// долях перехода (PostureTransit) — там, где её видит поза.

// Mirrors `source`'s pose across the vertical plane each tick (I turn left,
// it turns right); or, in showcase mode, floats at hover_height cycling clips.
struct MirrorPuppet {
    ecs::EntityId source{};
    glm::vec3 plane_point{0.0f};
    glm::vec2 plane_normal_xz{0.0f, 1.0f}; // unit, horizontal
    bool showcase = false;
    float hover_height_m = 0.0f;           // showcase float height
    float clip_seconds = 0.0f;             // per-clip dwell before advancing
};

// Adds BodyRig + BodyDrive to `owner` (which must have Transform) and spawns
// the segment entities (Transform + PreviousTransform + RenderMesh +
// LocalBounds). hide_head omits the head MESH, not the head bone.
void spawn_body(ecs::World& world, ecs::EntityId owner, const Rig& rig, bool hide_head);

// Destroys the segment entities and removes the body components.
void destroy_body(ecs::World& world, ecs::EntityId owner);

// Spawns a standalone puppet body whose root Transform this system owns:
// mirror of `source` across the plane, or the floating showcase double.
[[nodiscard]] ecs::EntityId spawn_mirror_puppet(ecs::World& world, const Rig& rig,
                                                ecs::EntityId source,
                                                const glm::vec3& plane_point,
                                                const glm::vec2& plane_normal_xz);

// Landing-dip trigger: the app calls this when sim's Landed event fires.
void note_landed(ecs::World& world, ecs::EntityId owner, float impact_speed_mps);

// Pure pose evaluation (exposed for tests and the puppet): locomotion blend
// (idle <-> gait by speed, air when not grounded, crouch and landing layers)
// or the showcase clip when drive.showcase_clip != SHOWCASE_NONE.
[[nodiscard]] LocalPose evaluate_body_pose(const Rig& rig, const BodyDrive& drive);

// КОРЕНЬ ТЕЛА С УЧЁТОМ ПОЗЫ. При posture_blend 0 это в точности прежнее
// «низ капсулы + рыск взгляда»; при 1 — земля и рыск мебели; между ними —
// прямая. Отдельной функцией, а не внутри evaluate_body_pose, потому что
// поза — это углы суставов, а корень — место в мире: смешать их в одном
// возврате значило бы, что тест позы не может спросить про место.
[[nodiscard]] BodyRoot body_root_for(const BodyDrive& drive,
                                     const glm::vec3& standing_ground);

// Fixed-tick system: advances internal clocks (SIM_DT), evaluates every body,
// runs FK, writes segment Transform pairs. Mirror puppets are evaluated after
// their sources (two passes) so they mirror THIS tick's pose.
void update_bodies(ecs::World& world, const Rig& rig);

} // namespace dfn::anim
