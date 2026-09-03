/*
Module: engine/anim
File: engine/anim/sources/FootIk.h

Responsibility:
- PUTTING THE FEET ON THE GROUND THAT IS ACTUALLY THERE. A clip knows one
  ground: the flat one its author drew it on. A stair tread and a hillside are
  the world's answer, they arrive as two numbers per foot (where the ground is
  under the ankle and under the toe), and this file turns those numbers into a
  vertical shift of the ROOT plus a two-bone knee solve for the foot the root
  did not reach.

Key items:
- FootIkSetup / build_foot_ik(): which joints the solve moves, and how high
  each contact point stands in OUR REST POSE.
- FootIkProbe: what the world answered, in the body's own frame (y = 0 is the
  ground the root stands on).
- FootIkPlan / plan_foot_ik(): the measurement — how far each ankle and toe
  must rise, how planted each foot is, and the root shift that follows.
- apply_foot_ik(): the edit — root translation, two-bone knee, ankle pitch.
- foot_penetration(): the acceptance instrument, metres below the ground.

Dependencies:
- Uses: Rig, SkinnedBody (JointLocal, the retarget), ClipPlayer (ContactSet),
  core skeleton.
- Used by: engine/app (SkinnedCharacter, which owns the raycast), tests.

Notes:
- WHY THE ROOT MOVES AT ALL, and why to the LOWER foot. With one foot on a
  tread and the other on the tread below, no pose of the legs alone can put
  both on their step: the pelvis has to come down to the lower one, and the
  upper leg then folds. Shifting to the HIGHER foot instead would push the
  lower one through the stair by the tread's whole rise.
- WHY THIS IS NOT Posture::solve_legs, which the order pointed at. That solver
  answers a different question in a different space: it takes a PELVIS HEIGHT
  and returns the thigh angle that lands the ankle on the floor, with the shin
  held at a fixed tilt, on OUR fifteen bones. Here the target is a POINT per
  foot in the imported skeleton's own frame, the shin is whatever the clip
  says, and the bend plane has to be preserved rather than chosen. What the
  two share is the law of cosines on two segments, and that is three lines;
  what they do not share is everything around it. Merging them would give one
  function with two modes, which is the shape both would be worse for.
- THE STANCE WEIGHT IS READ OFF THE POSE, not off the stride phase. The phase
  says when the clip's author planted the foot; the pose says whether THIS
  frame's foot is near the ground, which is the same question for a walk and
  the right question for a run's flight phase, a jump and a crossfade between
  two clips whose plants do not coincide. A foot in the air is weight 0 and
  the solve is a no-op on it, which is what keeps a jump from being glued.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure functions and plain data (Rule 8, Rule 30): no ECS, no IO, no clock and
  no physics. The raycast belongs to whoever owns a world; this file is handed
  its answers.
*/

#pragma once

#include "engine/anim/sources/ClipPlayer.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/skeleton/sources/Skeleton.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace dfn::anim {

/// HOW CLOSE A CONTACT POINT HAS TO BE TO ITS REST HEIGHT to count as fully
/// planted, metres. The same three centimetres ClipPlayer's stance band uses,
/// and for the same reason: the contact point is a JOINT, not a sole, so it
/// rides a shade above the grass while the ball of the foot rolls over it.
inline constexpr float FOOT_IK_GRIP_M = 0.03f;

/// AND HOW FAR ABOVE THAT THE WEIGHT REACHES ZERO. A hard edge at the grip
/// band would switch the solve on and off inside one stride and read as the
/// ankle ticking; twelve centimetres is about the height a walking foot
/// clears, so the fade covers the whole lift rather than its first millimetre.
inline constexpr float FOOT_IK_RELEASE_M = 0.12f;

/// THE MOST THE ROOT MAY BE MOVED, metres. Not a taste: a raycast that misses
/// its step and finds the floor of the stairwell would otherwise drop the
/// whole body a storey, and a body that sinks through the world on one bad
/// ray is worse than a foot that hangs for one frame. Half a metre is three
/// canonical stair rises (0.18), so a legitimate stride can never reach it.
inline constexpr float FOOT_IK_ROOT_LIMIT_M = 0.5f;

/// Which joints the solve reads and writes, and the heights it judges against.
struct FootIkSetup {
    std::array<int32_t, 2> hip{-1, -1};   ///< thigh joint, [0] left [1] right
    std::array<int32_t, 2> knee{-1, -1};  ///< shin joint
    std::array<int32_t, 2> ankle{-1, -1}; ///< foot joint
    std::array<int32_t, 2> toe{-1, -1};   ///< the foot's child, -1 when absent
    /// Where the ankle and the toe sit in OUR REST POSE — the pose whose soles
    /// the importer put on y = 0. A "ground height" from the world is added to
    /// these, never used raw.
    std::array<float, 2> ankle_rest_y{};
    std::array<float, 2> toe_rest_y{};
    /// Every parentless joint, i.e. everything the root shift is written on.
    std::vector<int32_t> roots;
    [[nodiscard]] bool valid() const {
        return hip[0] >= 0 && hip[1] >= 0 && knee[0] >= 0 && knee[1] >= 0
               && ankle[0] >= 0 && ankle[1] >= 0 && !roots.empty();
    }
};

[[nodiscard]] FootIkSetup build_foot_ik(const skel::Skeleton& skeleton,
                                        const SkinnedRigBinding& binding,
                                        const ContactSet& contacts);

/// WHAT THE WORLD ANSWERED, in the BODY'S OWN FRAME: the height of the ground
/// under each contact point, relative to the ground the root stands on
/// (y = 0). Flat ground is all zeros, which is why a test needs no physics.
struct FootIkProbe {
    std::array<float, 2> ankle_ground{};
    std::array<float, 2> toe_ground{};
    bool valid = false;
};

/// THE MEASUREMENT, taken on the pose as it will be drawn.
struct FootIkPlan {
    /// Metres the ankle (toe) must RISE to stand on the ground under it.
    /// Negative means the clip is holding the foot above its ground.
    std::array<float, 2> need{};
    std::array<float, 2> toe_need{};
    /// How planted each foot is in the clip, 0..1 (see the header note).
    std::array<float, 2> weight{};
    /// The shift that puts the LOWER planted foot on its ground. A suggestion:
    /// the caller may filter it over time (the app does) and hand the filtered
    /// value back to apply_foot_ik, which is why it is a field and not a
    /// private step of the solve.
    float root_dy = 0.0f;
    [[nodiscard]] bool any_planted() const { return weight[0] > 0.0f || weight[1] > 0.0f; }
};

[[nodiscard]] FootIkPlan plan_foot_ik(const skel::Skeleton& skeleton,
                                      const FootIkSetup& setup, const FootIkProbe& probe,
                                      std::span<const JointLocal> sample);

/// THE EDIT. Moves every root joint by `plan.root_dy`, then for each foot
/// lifts the ankle the rest of the way with a two-bone solve at the knee and
/// pitches the ankle so the toe meets its own ground. `strength` in [0,1]
/// scales the whole thing; 0 is a bit-for-bit no-op, which is what the
/// control arm of the acceptance test runs.
void apply_foot_ik(const skel::Skeleton& skeleton, const FootIkSetup& setup,
                   const FootIkProbe& probe, const FootIkPlan& plan, float strength,
                   std::span<JointLocal> sample);

/// THE ACCEPTANCE INSTRUMENT: how far the deepest contact point sits BELOW the
/// ground under it, metres, worst of the four. Zero or negative means nothing
/// is buried. Measured on the same pose the frame draws, through the same FK.
///
/// ONLY THE FEET THAT ARE TRYING TO STAND, and the exclusion is by CAUSE and
/// not by size (Rule 36). A foot in its swing passes OVER the nosing of the
/// tread ahead, and the ground under it at that instant is a surface it is not
/// standing on and must not be pushed off: counting it would say a correct
/// walk up a flight buries a foot 11 cm on every step, which is a statement
/// about the swing and not about the grounding. `plan` is what says which
/// feet are planted; pass the plan measured on the pose BEFORE the solve.
///
/// THE SWING IS A NAMED TAIL, not an oversight: a swinging foot that clips the
/// step above it is a real defect and it needs a different mechanism (a swept
/// query along the foot's path), which this wave did not build.
[[nodiscard]] float foot_penetration(const skel::Skeleton& skeleton,
                                     const FootIkSetup& setup, const FootIkProbe& probe,
                                     const FootIkPlan& plan,
                                     std::span<const JointLocal> sample);

/// НАСКОЛЬКО СТОПА НЕ СТОИТ НА СВОЁМ ГРУНТЕ — ЗНАКОВО И ПО КАЖДОЙ ОТДЕЛЬНО.
/// Положительное = стопа ПАРИТ над своим грунтом, отрицательное = утонула.
///
/// ЗАЧЕМ ОТДЕЛЬНО ОТ foot_penetration, у которой та же арифметика. Та берёт
/// max(0, ...), то есть срезает ровно ПОЛОЖИТЕЛЬНУЮ половину — а жалоба
/// владельца 31.08 («стоя на объекте одна стопа парит») живёт целиком в ней.
/// Прибор, устроенный так, что проверяемое им состояние всегда читается нулём,
/// — это правило 47 в чистом виде, и здесь оно сработало на собственном
/// приборе зоны. `foot_penetration` теперь ВЫРАЖЕНА через эту функцию, а не
/// написана рядом: две копии одного замера расходятся на первой же правке
/// (правило 39).
///
/// ПО ХУДШЕЙ ИЗ ДВУХ КОНТАКТНЫХ ТОЧЕК стопы, как и подъём: стопа стоит на
/// грунте, если её НИЖНЯЯ точка на нём, и пятка, висящая на кромке ступени, —
/// не парение, а лестница.
struct FootGap {
    std::array<float, 2> gap{};      ///< метры, [0] левая; + парит, − утонула
    std::array<uint8_t, 2> judged{}; ///< 1, если стопа опорная (вес >= FOOT_JUDGED_WEIGHT)
    /// Худшее по модулю среди СУДИМЫХ стоп; 0, если судить нечего.
    [[nodiscard]] float worst_abs() const {
        float w = 0.0f;
        for (int i = 0; i < 2; ++i) {
            const auto s = static_cast<std::size_t>(i);
            if (judged[s] != 0) {
                w = std::max(w, std::abs(gap[s]));
            }
        }
        return w;
    }
};

[[nodiscard]] FootGap foot_gap(const skel::Skeleton& skeleton, const FootIkSetup& setup,
                               const FootIkProbe& probe, const FootIkPlan& plan,
                               std::span<const JointLocal> sample);

/// ТОЧКИ КАСАНИЯ ОБЕИХ СТОП НА ОДНОЙ ПОЗЕ, в системе тела, и вес опоры каждой
/// (тот же, что FootIkPlan::weight). `point` — подушечка стопы (сустав носка,
/// когда он есть, иначе лодыжка): при перекате она стоит, а лодыжка вращается
/// вокруг неё. Это вход корневого движения от стопы (RootMotion.h) и якорь
/// замка стопы (ниже).
struct ContactState {
    /// Нижняя из двух точек стопы (носок или лодыжка, каждая относительно
    /// СВОЕЙ высоты покоя): при ударе пяткой это лодыжка, при перекате —
    /// подушечка. Именно эта точка стоит на месте, пока стопа опорная.
    std::array<glm::vec3, 2> point{};
    std::array<glm::vec3, 2> ankle{};
    std::array<glm::vec3, 2> toe{}; ///< = ankle, когда носка у скелета нет
    /// Высота нижней точки над её высотой покоя, метры.
    std::array<float, 2> height{};
    /// Вес IK стоп (FootIkPlan::weight, щедрая полоса 12 см) — для подъёма
    /// на грунт.
    std::array<float, 2> weight{};
    /// ВЕС ОПОРЫ ДЛЯ КОРНЕВОГО ДВИЖЕНИЯ И ЗАМКА: самая низкая стопа — 1, вторая
    /// — по полосе FOOT_SUPPORT_BAND_M над ней; обе 0 в полёте (нижняя выше
    /// FOOT_IK_RELEASE_M). Отдельно от `weight` намеренно: см. строку реестра.
    std::array<float, 2> support{};
    /// Какая точка сейчас нижняя: true — подушечка (носок), false — лодыжка.
    std::array<bool, 2> toe_point{};
    bool valid = false;
    [[nodiscard]] bool any_support() const { return support[0] > 0.0f || support[1] > 0.0f; }
};
[[nodiscard]] ContactState contact_state(const skel::Skeleton& skeleton,
                                         const FootIkSetup& setup, const FootIkPlan& plan,
                                         std::span<const JointLocal> sample);

/// ЗАМОК СТОПЫ (docs/design/LOCOMOTION_GROUNDED.md, §2 п. 2). Когда вес опоры
/// переваливает `on_weight`, мировая точка касания запоминается; пока вес не
/// упал ниже `off_weight`, стопа ставится в неё двузвенником колена; после
/// отпускания сила замка сходит на нет за `release_s`, чтобы не было щелчка.
/// Замок закрывает то, чего корневое движение от стопы не закрывает: капсулу,
/// которую физика провела иначе (стена, склон, ступень), поворот на ходу,
/// кроссфейд двух клипов с разными постановками, фильтр корня по высоте.
struct FootLockParams {
    float on_weight = 0.6f;
    float off_weight = 0.3f;
    float release_s = 0.1f;
    /// ПЕРЕСТУП ПРИ ПОВОРОТЕ НА МЕСТЕ (владелец 02.09-2: от первого лица
    /// «ноги прикреплены к точкам и скручиваются крестиком»): корпус ушёл
    /// над замкнутой стопой дальше этого угла — замок отпускает её, стопа
    /// уходит под корпус и замыкается заново; вторая стопа ждёт `step_s`.
    float twist_max_rad = 0.6f;
    float step_s = 0.2f;
    /// Строки FOOT_LOCK_* реестра.
    [[nodiscard]] static FootLockParams from_config();
};
struct FootLockState {
    std::array<bool, 2> locked{};
    std::array<glm::vec3, 2> anchor{};   ///< мировая точка касания на защёлкивании
    std::array<bool, 2> anchor_toe{};    ///< якорь — подушечка (true) или лодыжка
    std::array<float, 2> strength{};     ///< 0..1, сколько замка в силе
    /// ЗАЩЁЛКНУЛСЯ НА ЭТОМ ТИКЕ — постановка стопы; читатель событий шага.
    std::array<bool, 2> engaged{};
    std::array<float, 2> anchor_yaw{}; ///< рыск корпуса на защёлкивании
    std::array<float, 2> hold_s{};     ///< после переступа: столько не замыкать
    float step_cooldown_s = 0.0f;      ///< вторая стопа не переступает, пока идёт
    float last_yaw = 0.0f;             ///< рыск прошлого тика — покой корпуса
    float still_s = 0.0f;              ///< сколько корпус уже не вращается
};
/// Один тик состояния замков: `contact_world` — точки касания этой позы в
/// МИРЕ (приложение знает корень), `weight` — вес опоры.
void update_foot_locks(FootLockState& state, const std::array<glm::vec3, 2>& contact_world,
                       const std::array<float, 2>& weight,
                       const std::array<bool, 2>& point_is_toe, float body_yaw, float dt,
                       const FootLockParams& params);
/// ПРАВКА ПОЗЫ ПОД ЗАМОК: точка касания каждой стопы с силой > 0 ставится в
/// `point_target_model` (система тела; берётся только горизонталь — высоту
/// держит apply_foot_ik, а прыжок капсулы на ступень гасит корень, см.
/// SkinnedCharacter::probe_ground), лодыжка едет вместе с ней, колено
/// решается двузвенником, бедро доворачивается. Цель дальше вытяжения ноги
/// режется по досягаемости, а не растягивает ногу.
/// ОТПУСКАНИЕ ЗАМКА — ЗАМОРОЖЕННЫЙ СДВИГ, А НЕ ЯКОРЬ (владелец 04.09: «в момент,
/// когда ступня отрывается, ляжка выпрямляется и дёргается назад, потом резко
/// вперёд»). Прибор LocoTelemetry на стенде: в кадре после замка бедро и колено
/// отрывающейся ноги ускорялись до 550…1150 рад/с² при 20…130 в самом клипе —
/// сила замка сходила за FOOT_LOCK_RELEASE_S, а цель оставалась якорем в мире,
/// от которого стопа клипа уже улетала: поправка = (падающая сила) × (растущий
/// уход) — горб, то есть рывок назад и вперёд. Теперь на последнем кадре полной
/// силы запоминается сдвиг «якорь − стопа клипа» в системе тела, и при
/// отпускании применяется ОН, умноженный на силу: стопа сходит с якоря на
/// траекторию клипа по прямой.
/// ПОВОРОТ НОГ К НАПРАВЛЕНИЮ ХОДА (orientation warping, UE Pose Warping;
/// LOCOMOTION_GROUNDED.md §11.1): клип идёт по своей оси (вперёд, вбок,
/// назад), ввод — под углом к ней; корень ведёт сим по вводу, и без поворота
/// стопа клипа едет поперёк на sin(угла) хода (0,5 м за прогон на 30°/120°,
/// прибор восьми направлений 04.09). Обе ноги поворачиваются вокруг
/// вертикали в тазобедренных суставах на угол между осью роли и вводом;
/// корпус не трогается. Длина пути стопы не меняется — кривая пути та же.
void warp_legs(const skel::Skeleton& skeleton, const FootIkSetup& setup, float warp_rad,
               std::span<JointLocal> sample);

struct FootLockRelease {
    std::array<glm::vec3, 2> offset{}; ///< сдвиг в системе тела на последнем кадре полной силы
    std::array<bool, 2> has{};
};

void apply_foot_lock(const skel::Skeleton& skeleton, const FootIkSetup& setup,
                     const std::array<glm::vec3, 2>& point_target_model,
                     const std::array<bool, 2>& target_is_toe,
                     const std::array<float, 2>& strength, std::span<JointLocal> sample,
                     FootLockRelease* release = nullptr);

/// HOW PLANTED A FOOT HAS TO BE before the instrument above judges it, and the
/// number is DERIVED FROM THE SOLVE rather than picked.
///
/// The solve scales its lift by the same stance weight, so a foot at weight w
/// is corrected by w of what it asked for and is short by (1-w) of it BY
/// CONSTRUCTION — not by a defect. On a canonical 0.18 m rise, 0.95 bounds
/// that shortfall at 0.009 m, which is inside the centimetre the acceptance
/// is written in; a looser gate would put the instrument's own fade into the
/// number it reports. Measured on this asset: the run's releasing foot sits at
/// weight 0.81 and is 0.024 m short, which is 19 % of 0.125 m exactly.
inline constexpr float FOOT_JUDGED_WEIGHT = 0.95f;

} // namespace dfn::anim
