/*
Created: 28:08:2026 - 11:16:40
Last updated: 28:08:2026 - 11:16:40
Module: tests
File: tests/character/PostureTests.cpp

Responsibility:
- ПОЗЫ МЕБЕЛИ ЗАМЕРОМ, А НЕ ГЛАЗОМ (обязательство эпохи «сидеть и лежать»):
  углы суставов сидящего числами, стопы на полу, руки не в бёдрах, лежащий —
  на спине и в габарите настила, глаз позы совпадает со стоячей камерой на
  стоячей позе, корень позы смешивается коротким путём по кругу.

Dependencies:
- Uses: doctest, dfn_anim (Posture/Body/Pose/Rig), generated constants.
- Used by: ctest (character_posture).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- У КАЖДОГО КРИТЕРИЯ ЗДЕСЬ ЕСТЬ КОНТРОЛЬ (правило 30): «стопы на полу»
  проверяется вместе с сиденьем, у которого пола НЕ ДОСТАТЬ, и оно обязано
  провалить ту же проверку. Иначе критерий мерил бы не то, что называет.
*/
/*
UPD:
- 28:08:2026 - 11:16:40: Создан вместе с engine/anim/sources/Posture.*.
*/

#include <doctest/doctest.h>

#include <array>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

#include "engine/anim/sources/Body.h"
#include "engine/anim/sources/Pose.h"
#include "engine/anim/sources/Posture.h"
#include "engine/anim/sources/Rig.h"
#include "engine/core/config/sources/Constants.h"

using namespace dfn;
using namespace dfn::anim;

namespace {

/// ВЫСОТА СИДЕНЬЯ ЛАВКИ, м — ЗАМЕР с assets/houses/furn-bench.dfh (настил
/// y=0.4225 плюс полтолщины 0.055/2 = верх 0.45; та же величина стоит в
/// колонке my1 манифеста полки). Литерал здесь ЗАКОННЫЙ: это габарит
/// предмета, который тест и обязан назвать вслух, а не строка мира.
constexpr float BENCH_SEAT_M = 0.45f;
/// ВЫСОТА НАСТИЛА КРОВАТИ furn-bed, м — колонка `floor` манифеста полки
/// (поверхность матраса: e9 на 0.47 плюс полтолщины 0.06/2).
constexpr float BED_DECK_M = 0.50f;

[[nodiscard]] std::array<glm::mat4, BONE_COUNT> fk(const Rig& rig, const LocalPose& p,
                                                   const BodyRoot& root = {}) {
    std::array<glm::mat4, BONE_COUNT> out{};
    forward_kinematics(rig, p, root, out);
    return out;
}

[[nodiscard]] glm::vec3 joint(const std::array<glm::mat4, BONE_COUNT>& m, Bone b) {
    return glm::vec3{m[bone_index(b)][3]};
}

/// Местная -Y кости в мире — куда она «висит» (кость авторизована вдоль -Y).
[[nodiscard]] glm::vec3 bone_dir(const std::array<glm::mat4, BONE_COUNT>& m, Bone b) {
    return -glm::vec3{m[bone_index(b)][1]};
}

/// Угол между костью и ОТВЕСОМ ВНИЗ, градусы: 0 — висит прямо вниз,
/// 90 — горизонт. Именно в этих величинах владелец просил числа.
[[nodiscard]] float from_plumb_deg(const std::array<glm::mat4, BONE_COUNT>& m, Bone b) {
    const glm::vec3 d = glm::normalize(bone_dir(m, b));
    return glm::degrees(std::acos(std::clamp(-d.y, -1.0f, 1.0f)));
}

} // namespace

TEST_CASE("сидя: таз на сиденье, бёдра горизонт, голени вниз") {
    const Rig rig = Rig::build(RigProportions::from_config());
    const RigProportions& p = rig.proportions;
    LocalPose s = sit_pose(rig, BENCH_SEAT_M);
    apply_joint_limits(rig, s); // как в игре: единственный выход зоны их кладёт
    const auto m = fk(rig, s);

    // 1. ТАЗ НА СИДЕНЬЕ. Ось бедра выше настила ровно на половину толщины ноги
    // — это геометрия («бедро лежит на лавке»), и она проверяется числом.
    const float hip_y = joint(m, Bone::Pelvis).y;
    const float want_hip = BENCH_SEAT_M + p.leg_thickness * 0.5f;
    INFO("таз ", hip_y, " ожидается ", want_hip);
    CHECK(std::fabs(hip_y - want_hip) < 1.0e-4f);

    // 2. УГЛЫ СУСТАВОВ ЧИСЛАМИ. Бедро почти горизонт (90° от отвеса),
    // голень почти отвес, стопа плашмя.
    const float thigh_deg = from_plumb_deg(m, Bone::ThighL);
    const float shin_deg = from_plumb_deg(m, Bone::ShinL);
    INFO("бедро ", thigh_deg, "° от отвеса, голень ", shin_deg, "°");
    CHECK(thigh_deg > 80.0f);
    CHECK(thigh_deg < 95.0f);
    CHECK(shin_deg < 12.0f);
    // Симметрия: правая нога делает то же самое.
    CHECK(std::fabs(thigh_deg - from_plumb_deg(m, Bone::ThighR)) < 1.0e-3f);

    // 3. СГИБ КОЛЕНА — В СВОЁМ ДИАПАЗОНЕ И НАСТОЯЩИЙ. Сидеть с прямым коленом
    // нельзя; сгибаться назад — тоже (шарнир).
    const float knee = 2.0f * std::atan2(s.rotation[bone_index(Bone::ShinL)].x,
                                         s.rotation[bone_index(Bone::ShinL)].w);
    INFO("колено ", glm::degrees(knee), "°");
    CHECK(knee < -1.0f);  // сгиб, а не разгиб
    CHECK(knee > -static_cast<float>(config::BODY_KNEE_FLEX_MAX));

    // 4. СТОПЫ НА ПОЛУ. Лодыжка садится на свою стоячую высоту — то есть
    // подошва стоит на той же земле, что у стоящего.
    const float ankle_y = joint(m, Bone::FootL).y;
    INFO("лодыжка ", ankle_y, " ожидается ", p.ankle_height);
    CHECK(std::fabs(ankle_y - p.ankle_height) < 2.0e-3f);
    // ПОДОШВА ПЛАШМЯ: стопа авторизована вдоль -Z, и её суммарный тангаж
    // обязан быть нулём — иначе сидящий стоит на носках.
    const glm::vec3 foot_fwd = -glm::vec3{m[bone_index(Bone::FootL)][2]};
    INFO("стопа y-компонента ", foot_fwd.y);
    CHECK(std::fabs(foot_fwd.y) < 0.02f);
}

TEST_CASE("сидя: КОНТРОЛЬ — с недосягаемого сиденья ноги честно висят") {
    // Правило 30: критерий «стопы на полу» обязан ПАДАТЬ там, где пола не
    // достать. Барный стул 1.20 м выше, чем длина ноги (0.88), и поза не имеет
    // права соврать, будто стопа всё равно на полу.
    const Rig rig = Rig::build(RigProportions::from_config());
    const RigProportions& p = rig.proportions;
    LocalPose s = sit_pose(rig, 1.20f);
    apply_joint_limits(rig, s);
    const auto m = fk(rig, s);
    const float ankle_y = joint(m, Bone::FootL).y;
    INFO("лодыжка на высоком стуле ", ankle_y);
    CHECK(ankle_y > p.ankle_height + 0.20f); // висит, и заметно
    // И ноги при этом ОТВЕСНЫ, а не растопырены: зажим acos даёт бедру нулевой
    // тангаж, и весь остаток от отвеса — это СХОЖДЕНИЕ НОГ рига (7.37°, косая
    // нога стоящего), а не поза. Порог назван через саму величину, чтобы он не
    // рассыпался в день, когда ширина стойки изменится.
    const float conv_deg = glm::degrees(p.leg_convergence());
    INFO("схождение ног ", conv_deg, "°");
    CHECK(from_plumb_deg(m, Bone::ThighL) < conv_deg + 0.5f);
}

TEST_CASE("сидя: кисти лежат на бёдрах, а не внутри них") {
    // Замер, а не картинка: у сидящего бедро горизонтально и занимает ту самую
    // полосу, куда свободно висящая рука и попадает. Проверяется ВЕРХ бедра.
    const Rig rig = Rig::build(RigProportions::from_config());
    const RigProportions& p = rig.proportions;
    LocalPose s = sit_pose(rig, BENCH_SEAT_M);
    apply_joint_limits(rig, s);
    const auto m = fk(rig, s);
    const float thigh_top = joint(m, Bone::Pelvis).y + p.leg_thickness * 0.5f;
    const glm::vec3 wrist = joint(m, Bone::HandL);
    const glm::vec3 tip = wrist + glm::normalize(bone_dir(m, Bone::HandL)) * p.hand_length;
    INFO("верх бедра ", thigh_top, ", запястье ", wrist.y, ", кончик кисти ", tip.y);
    CHECK(wrist.y > thigh_top);
    CHECK(tip.y > thigh_top - 0.01f);
}

TEST_CASE("лёжа: на спине, в габарите настила, головой в местное +Z") {
    const Rig rig = Rig::build(RigProportions::from_config());
    const RigProportions& p = rig.proportions;
    LocalPose s = lie_pose(rig, BED_DECK_M);
    apply_joint_limits(rig, s);
    const auto m = fk(rig, s);

    // 1. ТАЗ НА НАСТИЛЕ: ось выше матраса на половину глубины корпуса.
    const float hip_y = joint(m, Bone::Pelvis).y;
    INFO("таз ", hip_y, " ожидается ", BED_DECK_M + p.torso_depth * 0.5f);
    CHECK(std::fabs(hip_y - (BED_DECK_M + p.torso_depth * 0.5f)) < 1.0e-4f);

    // 2. НА СПИНЕ: взгляд корпуса (местная -Z кости) смотрит В НЕБО.
    const glm::vec3 chest_face = -glm::vec3{m[bone_index(Bone::Torso)][2]};
    INFO("лицо груди ", chest_face.x, " ", chest_face.y, " ", chest_face.z);
    CHECK(chest_face.y > 0.95f);

    // 3. ГОЛОВА УХОДИТ В МЕСТНОЕ +Z (при рыске 0 это мировое +Z) — на этом
    // построен весь пересчёт рыска лежащего.
    const glm::vec3 head = joint(m, Bone::Head);
    INFO("голова z ", head.z, ", таз z ", joint(m, Bone::Pelvis).z);
    CHECK(head.z > joint(m, Bone::Pelvis).z + 0.4f);

    // 4. ВСЁ ТЕЛО В ГАБАРИТЕ ЛЕЖАКА ПО ВЫСОТЕ: ни одна кость не проваливается
    // под матрас и не висит над ним выше собственной толщины.
    float lo = 1.0e9f;
    float hi = -1.0e9f;
    for (uint32_t b = 0; b < BONE_COUNT; ++b) {
        lo = std::min(lo, glm::vec3{m[b][3]}.y);
        hi = std::max(hi, glm::vec3{m[b][3]}.y);
    }
    INFO("суставы лежащего от ", lo, " до ", hi, " (настил ", BED_DECK_M, ")");
    CHECK(lo > BED_DECK_M - 0.01f);
    CHECK(hi < BED_DECK_M + 0.45f);

    // 5. ДЛИНА ЛЕЖАЩЕГО ВДОЛЬ ЛЕЖАКА — она обязана поместиться на матрас
    // furn-bed (1.90 м чистого настила, замер чертежа).
    float zmin = 1.0e9f;
    float zmax = -1.0e9f;
    for (uint32_t b = 0; b < BONE_COUNT; ++b) {
        zmin = std::min(zmin, glm::vec3{m[b][3]}.z);
        zmax = std::max(zmax, glm::vec3{m[b][3]}.z);
    }
    const float span = (zmax + p.head_height) - (zmin - p.foot_length * 0.25f);
    INFO("длина лежащего по суставам ", span);
    CHECK(span < 1.90f);
}

TEST_CASE("глаз позы: на стоячей позе это ровно камера sim") {
    const Rig rig = Rig::build(RigProportions::from_config());
    const BodyRoot root{glm::vec3{3.0f, 10.0f, -4.0f}, 0.0f};
    const glm::vec3 eye = posture_eye(rig, LocalPose{}, root);
    // Стоя камера sim стоит в низе капсулы + PLAYER_EYE_HEIGHT и вперёд на
    // PLAYER_EYE_FORWARD (рыск 0 смотрит в -Z). Если формула позы разойдётся с
    // этой, у сидящего появится ВТОРАЯ камера — ровно тот дефект, за который
    // зона уже платила на присяде.
    // ...С ОДНОЙ ИМЕНОВАННОЙ РАЗНИЦЕЙ, и она НЕ этой волны: риг с косыми ногами
    // стоит на (hip_height - standing_hip_height) = 7.3 мм ниже, чем строки
    // NUMBERS, из которых sim берёт 1.70. Разница названа ВЫРАЖЕНИЕМ, а не
    // допуском: допуск скрыл бы её, а выражение падает в тот день, когда она
    // станет другой.
    const RigProportions& p = rig.proportions;
    const float rig_sag = p.hip_height - p.standing_hip_height();
    const glm::vec3 want = root.ground
                         + glm::vec3{0.0f,
                                     static_cast<float>(config::PLAYER_EYE_HEIGHT) - rig_sag,
                                     -static_cast<float>(config::PLAYER_EYE_FORWARD)};
    INFO("глаз ", eye.x, " ", eye.y, " ", eye.z, ", просадка рига ", rig_sag);
    CHECK(std::fabs(eye.x - want.x) < 1.0e-3f);
    CHECK(std::fabs(eye.y - want.y) < 1.0e-3f);
    CHECK(std::fabs(eye.z - want.z) < 1.0e-3f);

    // КОНТРОЛЬ: сидя глаз обязан ОПУСТИТЬСЯ — иначе величина не отвечает на
    // вопрос, который ей задают.
    const glm::vec3 sit_eye = posture_eye(rig, sit_pose(rig, BENCH_SEAT_M), root);
    INFO("глаз сидя ", sit_eye.y, " против стоячего ", eye.y);
    CHECK(sit_eye.y < eye.y - 0.30f);
    // И лёжа он опускается ещё ниже, оставаясь НАД настилом.
    const glm::vec3 lie_eye = posture_eye(rig, lie_pose(rig, BED_DECK_M), root);
    INFO("глаз лёжа ", lie_eye.y - root.ground.y);
    CHECK(lie_eye.y - root.ground.y > BED_DECK_M);
    CHECK(lie_eye.y - root.ground.y < BED_DECK_M + 0.45f);
}

TEST_CASE("рыск лежащего: голова туда, куда сказано") {
    const Rig rig = Rig::build(RigProportions::from_config());
    // Изголовье на восток (+X): рыск обязан развернуть тело так, чтобы голова
    // ушла в +X. Обычный «рыск взгляда» уложил бы человека головой в ноги —
    // это и есть контроль ниже.
    const float yaw = lie_yaw_for_head_dir(1.0f, 0.0f);
    const auto m = fk(rig, lie_pose(rig, BED_DECK_M), BodyRoot{glm::vec3{0.0f}, yaw});
    const glm::vec3 head = joint(m, Bone::Head);
    INFO("голова ", head.x, " ", head.z, " при рыске ", yaw);
    CHECK(head.x > 0.4f);
    CHECK(std::fabs(head.z) < 0.05f);

    // КОНТРОЛЬ: если взять рыск как у взгляда (atan2(x, -z)), голова уедет в
    // противоположную сторону. Проверка обязана это увидеть.
    const float wrong = std::atan2(1.0f, -0.0f);
    const auto mw = fk(rig, lie_pose(rig, BED_DECK_M), BodyRoot{glm::vec3{0.0f}, wrong});
    CHECK(joint(mw, Bone::Head).x < -0.4f);
}

TEST_CASE("корень позы: концы точные, середина между, круг коротким путём") {
    BodyDrive d;
    d.facing_yaw = 0.5f;
    d.posture = Posture::Sit;
    d.posture_ground = glm::vec3{10.0f, 2.0f, -3.0f};
    d.posture_yaw = 1.5f;
    const glm::vec3 standing{0.0f, 0.0f, 0.0f};

    d.posture_blend = 0.0f;
    CHECK(body_root_for(d, standing).ground.x == doctest::Approx(0.0f));
    CHECK(body_root_for(d, standing).yaw == doctest::Approx(0.5f));

    d.posture_blend = 1.0f;
    CHECK(body_root_for(d, standing).ground.x == doctest::Approx(10.0f));
    CHECK(body_root_for(d, standing).yaw == doctest::Approx(1.5f));

    d.posture_blend = 0.5f;
    CHECK(body_root_for(d, standing).ground.x == doctest::Approx(5.0f));
    CHECK(body_root_for(d, standing).yaw == doctest::Approx(1.0f));

    // КОРОТКИМ ПУТЁМ: с 3.0 рад на -3.0 рад — это 0.283 рад через ±pi, а не
    // 6.0 рад обратно. Прямая разность развернула бы сидящего кругом.
    d.facing_yaw = 3.0f;
    d.posture_yaw = -3.0f;
    d.posture_blend = 0.5f;
    const float mid = body_root_for(d, standing).yaw;
    INFO("середина поворота ", mid);
    CHECK(std::fabs(mid - 3.1415927f) < 0.05f);
}

TEST_CASE("поза кладётся ПОВЕРХ живого слоя весом posture_blend") {
    const Rig rig = Rig::build(RigProportions::from_config());
    BodyDrive d;
    d.posture_height_m = BENCH_SEAT_M;

    d.posture = Posture::None;
    d.posture_blend = 0.0f;
    const LocalPose stand = evaluate_body_pose(rig, d);
    CHECK(std::fabs(stand.pelvis_offset.y) < 0.02f); // дыхание, и только

    d.posture = Posture::Sit;
    d.posture_blend = 1.0f;
    const LocalPose seated = evaluate_body_pose(rig, d);
    const float drop = rig.proportions.standing_hip_height()
                     - (BENCH_SEAT_M + rig.proportions.leg_thickness * 0.5f);
    INFO("просадка таза ", -seated.pelvis_offset.y, " ожидается ", drop);
    CHECK(std::fabs(-seated.pelvis_offset.y - drop) < 1.0e-3f);

    // На половине пути таз обязан быть МЕЖДУ, а не в одном из концов: ровно
    // это делает переход переходом, а не подменой кадра.
    d.posture_blend = 0.5f;
    const LocalPose half = evaluate_body_pose(rig, d);
    CHECK(half.pelvis_offset.y < stand.pelvis_offset.y - 0.05f);
    CHECK(half.pelvis_offset.y > seated.pelvis_offset.y + 0.05f);
}
