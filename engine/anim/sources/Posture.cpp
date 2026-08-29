/*
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

#include "engine/anim/sources/Posture.h"

#include "engine/anim/sources/Clips.h" // HEAD_STABILIZE: одна доля на все наклоны
#include "engine/core/config/sources/Constants.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
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

/// СГЛАЖЕННАЯ ДОЛЯ (ease-in-out). Кубика Эрмита, а не косинуса и не прямой:
/// прямая даёт РЫВОК на обоих концах (скорость с нуля в полную за кадр), а
/// именно его заказ и называет своим отрицательным критерием.
[[nodiscard]] float ease(float x) {
    const float t = std::clamp(x, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/// ДОЛЯ, НАЧАТАЯ ПОЗЖЕ И/ИЛИ ЗАКОНЧЕННАЯ РАНЬШЕ. `from`..`to` — окно внутри
/// перехода; вне окна доля точно 0 и точно 1, и это существенно: конец
/// перехода обязан быть позой БИТ-В-БИТ, а не «почти позой».
[[nodiscard]] float ease_window(float t, float from, float to) {
    return ease((t - from) / std::max(1.0e-3f, to - from));
}

/// РЕШЁННЫЙ ДВУЗВЕННИК НОГИ ПРИ ДАННОЙ ВЫСОТЕ ТАЗА. Вынесен из sit_pose
/// целиком и без изменений — затем, что переход обязан спрашивать ЕГО ЖЕ на
/// каждой промежуточной высоте (иначе стопа отрывается от пола посреди
/// спуска), а второе описание того же треугольника разошлось бы молча.
struct LegSolve {
    float thigh = 0.0f; ///< 0 = отвес вниз, pi/2 = горизонт
    float shin = 0.0f;  ///< МЕСТНЫЙ угол колена (сгиб < 0)
    float foot = 0.0f;  ///< подошва плашмя: суммарный тангаж 0
};

[[nodiscard]] LegSolve solve_legs(const Rig& rig, float hip_y) {
    const RigProportions& p = rig.proportions;
    // И НОГА КОСАЯ, ЭТО НАДО УЧЕСТЬ. Бедро в покое отвёрнуто внутрь на
    // leg_convergence (Rig::build), голень наследует поворот, — значит по
    // ВЕРТИКАЛИ звено проходит не свою длину, а её долю cos(схождение). Ровно
    // ту же поправку берёт standing_hip_height у стоящего; без неё лодыжка
    // сидящего вставала на 3.8 мм выше пола (замерено тестом до правки).
    const float conv = std::cos(p.leg_convergence());
    const float drop = (hip_y - p.ankle_height) / std::max(0.01f, conv);
    const float shin_drop = p.shin_length() * std::cos(SIT_SHIN_TILT_RAD);
    const float c = std::clamp((drop - shin_drop) / std::max(0.01f, p.thigh_length()),
                               -1.0f, 1.0f);
    LegSolve s;
    s.thigh = std::acos(c);
    // КОЛЕНО ЗАЖИМАЕТСЯ СВОИМ ЖЕ ПРЕДЕЛОМ, И ПРЕДЕЛ БЕРЁТСЯ У РИГА. У почти
    // прямой ноги (таз на стоячей высоте — начало перехода) наклон голени в
    // 0.12 рад пришёлся бы целиком на РАЗГИБ колена, а разгиб у него всего
    // 0.09 (BODY_KNEE_HYPEREXT_MAX): поза уходила за предел на 1.7°, и её
    // молча правил apply_joint_limits уже на выходе. Второй копии предела
    // здесь нет — спрашивается тот самый, которым потом и правят.
    const glm::vec2 knee = rig.hinge_range[bone_index(Bone::ShinL)];
    s.shin = std::clamp(SIT_SHIN_TILT_RAD - s.thigh, knee.x, knee.y);
    // ПОДОШВА ПЛАШМЯ: суммарный тангаж бедра, голени и стопы равен нулю. В
    // незажатом случае это ровно прежнее -SIT_SHIN_TILT_RAD, в зажатом —
    // по-прежнему плашмя, а не «почти».
    s.foot = -(s.thigh + s.shin);
    return s;
}

} // namespace

float posture_transit_s(Posture p) {
    return p == Posture::Lie ? LIE_TRANSIT_S : SIT_TRANSIT_S;
}

PostureTransit posture_transit(Posture p, float t) {
    const float x = std::clamp(t, 0.0f, 1.0f);
    PostureTransit w;
    // ЖИВОЙ СЛОЙ ОТДАЁТ ТЕЛО В ПЕРВОЙ ТРЕТИ. Дальше поза ведётся траекторией
    // таза, а не весом кроссфейда, — в этом и состоит разница между
    // «движением» и «фейдом»: после 0.35 никакого смешивания уже нет.
    w.take = ease_window(x, 0.0f, 0.35f);
    if (p == Posture::Lie) {
        // ЛЁЖА ДВА ХОДА ПОДРЯД, А НЕ ОДИН: сначала таз опускается на настил
        // (первые 70% времени), потом тело откидывается на спину. Слить их в
        // один значило бы валиться назад с высоты стоящего роста.
        w.drop = ease_window(x, 0.0f, 0.75f);
        w.plan = w.drop;
        // ОКНО ОТКИДЫВАНИЯ ШИРОКОЕ НАМЕРЕННО. Оно несёт почти весь путь
        // ГЛАЗА (0.95 м), а ускорение сглаженной дуги идёт как 6·ход/окно²:
        // узкое окно даёт двойное g на голове, и это читается падением, а не
        // укладыванием. Перекрытие со спуском законно — человек и правда
        // начинает откидываться, ещё не досев.
        w.recline = ease_window(x, 0.35f, 1.0f);
    } else {
        w.drop = ease(x);
        // КОРЕНЬ ОПЕРЕЖАЕТ СПУСК — ЭТО И ЕСТЬ ДУГА. Сначала человек оказывается
        // НАД лавкой (перенос и разворот), и только потом опускается на неё;
        // одна общая доля дала бы отрезок наискось, то есть проход тазом сквозь
        // кромку настила.
        w.plan = ease_window(x, 0.0f, 0.55f);
        w.recline = 0.0f;
    }
    w.settle = ease_window(x, 0.20f, 1.0f);
    return w;
}

LocalPose posture_pose(const Rig& rig, Posture p, float height_m,
                       const PostureTransit& w) {
    LocalPose s;
    if (p == Posture::None) {
        return s;
    }
    const RigProportions& pr = rig.proportions;
    const bool lie = p == Posture::Lie;
    const float r = lie ? std::clamp(w.recline, 0.0f, 1.0f) : 0.0f;
    const float st = std::clamp(w.settle, 0.0f, 1.0f);

    // 1. ТАЗ — ЕДИНСТВЕННАЯ ВЕДУЩАЯ КООРДИНАТА ПЕРЕХОДА. Конец пути: у сиденья
    // ось бедра выше настила на ПОЛТОЛЩИНЫ НОГИ (бедро лежит на настиле), у
    // лежака — на ПОЛГЛУБИНЫ КОРПУСА (на матрас ложится спина). Начало —
    // СТОЯЧАЯ высота бедра: именно её FK прибавляет к земле, и вычесть надо
    // ровно её (не hip_height: ноги сходятся, и стоячий таз сидит ниже).
    const float hip_end = height_m + (lie ? hip_above_deck(pr) : hip_above_seat(pr));
    const float hip_y = std::lerp(pr.standing_hip_height(), hip_end,
                                  std::clamp(w.drop, 0.0f, 1.0f));
    s.pelvis_offset = {0.0f, hip_y - pr.standing_hip_height(), 0.0f};

    // 2. КОЛЕНИ И БЁДРА — ВЕДОМЫЕ. Двузвенник решается по ЛОДЫЖКЕ на ТЕКУЩЕЙ
    // высоте таза: голень задана (почти отвес), бедро выводится так, чтобы
    // стопа стояла на полу. На стоячей высоте таза acos зажимается в 1 и ноги
    // выходят отвесными — оттого переход начинается ровно со стоячих ног, а
    // не с прыжка в согнутые. У высокого сиденья (пола не достать) те же
    // ноги честно висят: на барном стуле они и висят.
    const LegSolve leg = solve_legs(rig, hip_y);
    // ЛЁЖА НОГИ РАСПРЯМЛЯЮТСЯ ВМЕСТЕ С ОТКИДЫВАНИЕМ: таз уходит назад, а ноги
    // поднимаются с пола на настил — то же движение одним числом.
    const float thigh = std::lerp(leg.thigh, 0.0f, r);
    const float shin = std::lerp(leg.shin, LIE_KNEE, r);
    const float foot = std::lerp(leg.foot, 0.0f, r);
    s.rotation[bone_index(Bone::ThighL)] = pitch(thigh);
    s.rotation[bone_index(Bone::ThighR)] = pitch(thigh);
    s.rotation[bone_index(Bone::ShinL)] = pitch(shin);
    s.rotation[bone_index(Bone::ShinR)] = pitch(shin);
    s.rotation[bone_index(Bone::FootL)] = pitch(foot);
    s.rotation[bone_index(Bone::FootR)] = pitch(foot);

    // 3. КОРПУС И ГОЛОВА. Сидя — лёгкий наклон вперёд, и голова его частью
    // гасит (та же доля HEAD_STABILIZE, что у бегового наклона и у присяда: на
    // пол не смотрят оттого, что сели). Лёжа наклон уходит в ноль, а голова —
    // на подушку.
    const float trunk = std::lerp(-SIT_TRUNK_LEAN_RAD * st, 0.0f, r);
    const float head = std::lerp(SIT_TRUNK_LEAN_RAD * HEAD_STABILIZE * st, LIE_HEAD, r);
    s.rotation[bone_index(Bone::Torso)] = pitch(trunk);
    s.rotation[bone_index(Bone::Head)] = pitch(head);

    // 4. РУКИ: сидя — на бёдрах, лёжа — вдоль тела. Отвод плеча лёжа это
    // РОЛЛ, а не тангаж: в стороны свободная кость и умеет, а вот локтю
    // боковое выдавать нельзя (оно бы стёрлось, шапка файла).
    const glm::quat sit_shoulder = pitch(SIT_SHOULDER * st);
    s.rotation[bone_index(Bone::UpperArmL)] =
        glm::slerp(sit_shoulder,
                   glm::angleAxis(-LIE_ARM_OUT, glm::vec3{0.0f, 0.0f, 1.0f}), r);
    s.rotation[bone_index(Bone::UpperArmR)] =
        glm::slerp(sit_shoulder,
                   glm::angleAxis(LIE_ARM_OUT, glm::vec3{0.0f, 0.0f, 1.0f}), r);
    const float elbow = std::lerp(SIT_ELBOW * st, LIE_ELBOW, r);
    s.rotation[bone_index(Bone::ForearmL)] = pitch(elbow);
    s.rotation[bone_index(Bone::ForearmR)] = pitch(elbow);
    const float wrist = std::lerp(SIT_WRIST * st, 0.0f, r);
    s.rotation[bone_index(Bone::HandL)] = pitch(wrist);
    s.rotation[bone_index(Bone::HandR)] = pitch(wrist);

    // 5. ОТКИДЫВАНИЕ. ВСЁ ТЕЛО КЛАДЁТ ОДИН ПОВОРОТ ТАЗА: +90° вокруг X, и
    // местное «вверх» (голова) уходит в местное +Z, а «взгляд» (-Z) — в
    // мировой верх, то есть человек лежит НА СПИНЕ. Руки, висящие по -Y,
    // ложатся вдоль тела сами; стопы, глядящие в -Z, сами встают носками
    // вверх. На полпути тот же поворот и есть «привстал на локте» — поза
    // между, которой никто не рисовал.
    if (lie) {
        s.rotation[bone_index(Bone::Pelvis)] = pitch(glm::half_pi<float>() * r);
    }
    return s;
}

LocalPose sit_pose(const Rig& rig, float seat_above_ground_m) {
    return posture_pose(rig, Posture::Sit, seat_above_ground_m, PostureTransit{});
}

LocalPose lie_pose(const Rig& rig, float deck_above_ground_m) {
    PostureTransit done;
    done.recline = 1.0f;
    return posture_pose(rig, Posture::Lie, deck_above_ground_m, done);
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
