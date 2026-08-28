/*
Created: 28:08:2026 - 11:16:40
Last updated: 28:08:2026 - 11:16:40
Module: engine/anim
File: engine/anim/sources/Posture.cpp

Responsibility:
- Реализация поз сидя/лёжа и точки глаза позы (контракт в Posture.h).

Dependencies:
- Uses: Posture.h, Pose.h, Rig.h, generated constants, glm.
- Used by: Body.cpp, tests/character.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- КЛИПЫ ПИШУТ ТОЛЬКО ТО ДВИЖЕНИЕ, КОТОРОЕ СУСТАВ УМЕЕТ (правило зоны, шапка
  Clips.h): колено и локоть — шарниры по своей X, и всё, что здесь им
  выдаётся, — чистый тангаж. Боковое на шарнир не зажимается, а СТИРАЕТСЯ.
*/
/*
UPD:
- 28:08:2026 - 11:16:40: Создан. Позы сидя и лёжа, глаз позы, рыск лежащего.
*/

#include "engine/anim/sources/Posture.h"

#include "engine/anim/sources/Clips.h" // HEAD_STABILIZE: одна доля на все наклоны
#include "engine/core/config/sources/Constants.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

namespace dfn::anim {

namespace {

[[nodiscard]] glm::quat pitch(float a) {
    return glm::angleAxis(a, glm::vec3{1.0f, 0.0f, 0.0f});
}

// --- Форма позы сидя (процедурные данные клипа, не строки NUMBERS: тот же
// разбор, что в шапке Clips.h — величина, с которой обязана согласиться ВТОРАЯ
// зона, идёт в NUMBERS; форма позы не согласуется ни с кем) ------------------

// Плечо чуть вперёд, локоть сложен, кисть выпрямлена вдоль бедра: руки
// сидящего лежат на бёдрах. Проверяется ЗАМЕРОМ (тест «кисть не в бедре»), а
// не глазом: у сидящего бедро горизонтально и занимает ровно ту полосу, куда
// свободно висящая рука и попадает.
constexpr float SIT_SHOULDER = 0.15f;  // рад, плечо вперёд
constexpr float SIT_ELBOW = 1.25f;     // рад, сгиб локтя (положительный — сгиб)
constexpr float SIT_WRIST = 0.25f;     // рад, кисть довыпрямляется вдоль бедра

// --- Форма позы лёжа --------------------------------------------------------
constexpr float LIE_KNEE = -0.08f;     // рад, колени не «в замок» (правило рига:
    // жёсткий упор ровно на прямом читается деревянным)
constexpr float LIE_HEAD = -0.16f;     // рад, голова приподнята подушкой
constexpr float LIE_ARM_OUT = 0.10f;   // рад, руки чуть отведены от корпуса
constexpr float LIE_ELBOW = 0.12f;     // рад, локти не в замок

// ПОЛОВИНА ТОЛЩИНЫ НОГИ НАД СИДЕНЬЕМ — ЭТО ГЕОМЕТРИЯ, А НЕ ПОПРАВКА: ось
// бедра проходит серединой ноги, а на настил ложится её низ.
[[nodiscard]] float hip_above_seat(const RigProportions& p) {
    return p.leg_thickness * 0.5f;
}

// ПОЛОВИНА ГЛУБИНЫ КОРПУСА НАД НАСТИЛОМ — то же рассуждение одной позой ниже:
// на матрас ложится спина, а ось таза идёт серединой тела.
[[nodiscard]] float hip_above_deck(const RigProportions& p) {
    return p.torso_depth * 0.5f;
}

} // namespace

LocalPose sit_pose(const Rig& rig, float seat_above_ground_m) {
    const RigProportions& p = rig.proportions;
    LocalPose s;

    // 1. ТАЗ НА СИДЕНЬЕ. Смещение корня меряется от СТОЯЧЕЙ высоты бедра —
    // именно её FK прибавляет к земле, и вычесть надо ровно её (не
    // hip_height: ноги сходятся, и стоячий таз сидит на 7 мм ниже).
    const float hip_y = seat_above_ground_m + hip_above_seat(p);
    s.pelvis_offset = {0.0f, hip_y - p.standing_hip_height(), 0.0f};

    // 2. БЁДРА И ГОЛЕНИ — ДВУЗВЕННИК, РЕШЁННЫЙ ПО ЛОДЫЖКЕ. Голень задана
    // (почти отвес), бедро ВЫВОДИТСЯ так, чтобы лодыжка села на свою стоячую
    // высоту, то есть стопа встала на пол. У высокого сиденья (когда пола не
    // достать) acos зажимается в 1 и ноги честно висят отвесно — это ответ, а
    // не сбой: на барном стуле ноги и висят.
    //
    // И НОГА КОСАЯ, ЭТО НАДО УЧЕСТЬ. Бедро в покое отвёрнуто внутрь на
    // leg_convergence (Rig::build), голень наследует поворот, — значит по
    // ВЕРТИКАЛИ звено проходит не свою длину, а её долю cos(схождение). Ровно
    // ту же поправку берёт standing_hip_height у стоящего; без неё лодыжка
    // сидящего вставала на 3.8 мм выше пола (замерено этим тестом до правки).
    const float conv = std::cos(p.leg_convergence());
    const float drop = (hip_y - p.ankle_height) / std::max(0.01f, conv);
    const float shin_drop = p.shin_length() * std::cos(SIT_SHIN_TILT_RAD);
    const float c = std::clamp((drop - shin_drop) / std::max(0.01f, p.thigh_length()),
                               -1.0f, 1.0f);
    const float thigh = std::acos(c);          // 0 = отвес вниз, pi/2 = горизонт
    const float shin = SIT_SHIN_TILT_RAD - thigh; // МЕСТНЫЙ угол колена (сгиб < 0)
    const float foot = -SIT_SHIN_TILT_RAD;     // подошва плашмя: суммарный тангаж 0

    s.rotation[bone_index(Bone::ThighL)] = pitch(thigh);
    s.rotation[bone_index(Bone::ThighR)] = pitch(thigh);
    s.rotation[bone_index(Bone::ShinL)] = pitch(shin);
    s.rotation[bone_index(Bone::ShinR)] = pitch(shin);
    s.rotation[bone_index(Bone::FootL)] = pitch(foot);
    s.rotation[bone_index(Bone::FootR)] = pitch(foot);

    // 3. ЛЁГКИЙ НАКЛОН КОРПУСА, и голова его частью гасит — та же доля
    // HEAD_STABILIZE, что у бегового наклона и у присяда: на пол не смотрят
    // оттого, что сели.
    s.rotation[bone_index(Bone::Torso)] = pitch(-SIT_TRUNK_LEAN_RAD);
    s.rotation[bone_index(Bone::Head)] = pitch(SIT_TRUNK_LEAN_RAD * HEAD_STABILIZE);

    // 4. РУКИ НА БЁДРАХ.
    for (const Bone arm : {Bone::UpperArmL, Bone::UpperArmR}) {
        s.rotation[bone_index(arm)] = pitch(SIT_SHOULDER);
    }
    for (const Bone fore : {Bone::ForearmL, Bone::ForearmR}) {
        s.rotation[bone_index(fore)] = pitch(SIT_ELBOW);
    }
    for (const Bone hand : {Bone::HandL, Bone::HandR}) {
        s.rotation[bone_index(hand)] = pitch(SIT_WRIST);
    }
    return s;
}

LocalPose lie_pose(const Rig& rig, float deck_above_ground_m) {
    const RigProportions& p = rig.proportions;
    LocalPose s;

    const float hip_y = deck_above_ground_m + hip_above_deck(p);
    s.pelvis_offset = {0.0f, hip_y - p.standing_hip_height(), 0.0f};

    // ВСЁ ТЕЛО КЛАДЁТ ОДИН ПОВОРОТ ТАЗА. +90° вокруг X: местное «вверх»
    // (голова) уходит в местное +Z, а «взгляд» (-Z) — в мировой верх, то есть
    // человек лежит НА СПИНЕ. Руки, висящие по -Y, ложатся вдоль тела сами;
    // стопы, глядящие в -Z, сами встают носками вверх.
    s.rotation[bone_index(Bone::Pelvis)] = pitch(glm::half_pi<float>());

    s.rotation[bone_index(Bone::ShinL)] = pitch(LIE_KNEE);
    s.rotation[bone_index(Bone::ShinR)] = pitch(LIE_KNEE);
    s.rotation[bone_index(Bone::Head)] = pitch(LIE_HEAD);
    // Отвод рук — РОЛЛ ПЛЕЧА, а не тангаж: в стороны свободная кость и умеет,
    // а вот локтю боковое выдавать нельзя (оно бы стёрлось, шапка файла).
    s.rotation[bone_index(Bone::UpperArmL)] =
        glm::angleAxis(-LIE_ARM_OUT, glm::vec3{0.0f, 0.0f, 1.0f});
    s.rotation[bone_index(Bone::UpperArmR)] =
        glm::angleAxis(LIE_ARM_OUT, glm::vec3{0.0f, 0.0f, 1.0f});
    s.rotation[bone_index(Bone::ForearmL)] = pitch(LIE_ELBOW);
    s.rotation[bone_index(Bone::ForearmR)] = pitch(LIE_ELBOW);
    return s;
}

glm::vec3 posture_eye(const Rig& rig, const LocalPose& pose, const BodyRoot& root) {
    std::array<glm::mat4, BONE_COUNT> bones{};
    forward_kinematics(rig, pose, root, bones);
    const glm::mat4& head = bones[bone_index(Bone::Head)];
    // ГЛАЗ СИДИТ В ЧЕРЕПЕ, а череп — на шее: сустав головы плюс подъём вдоль
    // СОБСТВЕННОЙ оси головы и вынос вдоль её взгляда. На стоячей позе это
    // ровно (низ капсулы + PLAYER_EYE_HEIGHT + вперёд·PLAYER_EYE_FORWARD) —
    // то есть та самая точка, куда камеру ставит sim (проверено тестом).
    const glm::vec3 joint{head[3]};
    const glm::vec3 up{head[1]};       // местная +Y головы
    const glm::vec3 fwd{-glm::vec3{head[2]}}; // местная -Z головы = взгляд
    const float eye_above_neck = static_cast<float>(config::PLAYER_EYE_HEIGHT)
                               - rig.proportions.neck_height;
    return joint + up * eye_above_neck
         + fwd * static_cast<float>(config::PLAYER_EYE_FORWARD);
}

float lie_yaw_for_head_dir(float head_dir_x, float head_dir_z) {
    // Местное +Z в мире равно (-sin yaw, 0, cos yaw) — это «назад» тела, и
    // именно туда уходит голова лежащего (см. lie_pose). Значит рыск берётся
    // из направления в изголовье через эту пару, а не через обычный взгляд.
    return std::atan2(-head_dir_x, head_dir_z);
}

} // namespace dfn::anim
