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
- ПЕРЕХОД ЗАМЕРОМ: концы перехода — сами позы бит-в-бит, таз и глаз идут
  МОНОТОННО и БЕЗ РЫВКА (и обе прежние формы — прямая и скачок — обязаны эту
  же проверку провалить), суставы по дороге не выходят за пределы.

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
- 28:08:2026 - 18:10:00: Рукав перехода (второй хвост сдачи зоны): концы,
  монотонность, рывок с двумя отрицательными плечами, пределы суставов,
  длительности, вставание ИЗ ТОЙ позы, в которой были.
*/

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <tuple>
#include <utility>
#include <vector>

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

/// ОДИН ЗАМЕР ПЕРЕХОДА: где таз и где глаз в долю времени `t`. Считается ТЕМ
/// ЖЕ путём, которым тело рисуется, — evaluate_body_pose + body_root_for + FK;
/// собственной формулы «где таз при переходе» здесь нет и быть не должно,
/// иначе тест мерил бы свою арифметику, а не движок.
struct TransitAt {
    float pelvis_y = 0.0f;
    float eye_y = 0.0f;
};

[[nodiscard]] TransitAt transit_at(const Rig& rig, Posture p, float height_m,
                                   const glm::vec3& standing, float t) {
    BodyDrive d;
    d.grounded = true;
    d.posture = p;
    d.posture_shown = p;
    d.posture_height_m = height_m;
    d.posture_blend = t;
    d.posture_ground = glm::vec3{0.0f};
    d.posture_yaw = 0.0f;
    const LocalPose pose = evaluate_body_pose(rig, d);
    const BodyRoot root = body_root_for(d, standing);
    TransitAt out;
    out.pelvis_y = joint(fk(rig, pose, root), Bone::Pelvis).y;
    out.eye_y = posture_eye(rig, pose, root).y;
    return out;
}

/// ДОЛЯ ВРЕМЕНИ НА ШАГЕ `i` ИЗ `steps` — С ЗАПАСОМ СТОЯНИЯ ДО И ПОСЛЕ. Ряд
/// нарочно шире перехода: РЫВОК ЖИВЁТ НА СТЫКАХ, и лента, начатая ровно в
/// начале движения, его не видит вовсе (проверено — прямая на такой ленте
/// показывает нулевой рывок и проходит проверку, которую обязана провалить).
[[nodiscard]] float lead_in_t(int i, int steps) {
    constexpr float LEAD = 0.25f; // доля перехода на стояние до и после
    const float x = static_cast<float>(i) / static_cast<float>(steps);
    return std::clamp(x * (1.0f + 2.0f * LEAD) - LEAD, 0.0f, 1.0f);
}

/// РЯД ЗАМЕРОВ ПО РАВНОМЕРНОЙ СЕТКЕ ВРЕМЕНИ (со стоянием на концах).
[[nodiscard]] std::vector<TransitAt> transit_series(const Rig& rig, Posture p,
                                                    float height_m,
                                                    const glm::vec3& standing,
                                                    int steps) {
    std::vector<TransitAt> out;
    out.reserve(static_cast<std::size_t>(steps) + 1);
    for (int i = 0; i <= steps; ++i) {
        out.push_back(transit_at(rig, p, height_m, standing, lead_in_t(i, steps)));
    }
    return out;
}

/// ХУДШИЙ ПОДЪЁМ ряда, доля полного хода. Ноль — ряд не поднимался ни разу.
[[nodiscard]] float worst_rise(const std::vector<float>& v) {
    const float span = std::fabs(v.front() - v.back());
    float worst = 0.0f;
    for (std::size_t i = 1; i < v.size(); ++i) {
        worst = std::max(worst, (v[i] - v[i - 1]) / std::max(1.0e-4f, span));
    }
    return worst;
}

/// ХУДШИЙ РЫВОК ряда: наибольшая ВТОРАЯ разность, в долях полного хода.
/// Именно вторая: первая (скорость) у всякого перехода не ноль, а рывок —
/// это её скачок. У прямой он живёт на концах (скорость с нуля в полную за
/// шаг), у подмены кадра — в середине, у сглаженной дуги его нет нигде.
[[nodiscard]] float worst_jerk(const std::vector<float>& v) {
    const float span = std::fabs(v.front() - v.back());
    float worst = 0.0f;
    for (std::size_t i = 1; i + 1 < v.size(); ++i) {
        worst = std::max(worst, std::fabs(v[i + 1] - 2.0f * v[i] + v[i - 1])
                                    / std::max(1.0e-4f, span));
    }
    return worst;
}

[[nodiscard]] std::vector<float> pelvis_track(const std::vector<TransitAt>& s) {
    std::vector<float> out;
    for (const TransitAt& a : s) {
        out.push_back(a.pelvis_y);
    }
    return out;
}

[[nodiscard]] std::vector<float> eye_track(const std::vector<TransitAt>& s) {
    std::vector<float> out;
    for (const TransitAt& a : s) {
        out.push_back(a.eye_y);
    }
    return out;
}

/// РАВНЫ ЛИ ДВЕ ПОЗЫ ПОКОСТНО (кватернион и −кватернион — один поворот).
[[nodiscard]] float pose_diff(const LocalPose& a, const LocalPose& b) {
    float worst = std::fabs(a.pelvis_offset.y - b.pelvis_offset.y);
    for (std::size_t i = 0; i < BONE_COUNT; ++i) {
        const float d = std::fabs(std::fabs(glm::dot(a.rotation[i], b.rotation[i])) - 1.0f);
        worst = std::max(worst, d);
    }
    return worst;
}

} // namespace

TEST_CASE("переход: концы — ЭТО САМИ ПОЗЫ, а не «почти позы»") {
    const Rig rig = Rig::build(RigProportions::from_config());
    // Одна функция на оба конца и всю середину. Если бы посередине жил
    // отдельный клип перехода, эта проверка была бы единственным местом, где
    // расхождение видно, — и её бы не было.
    PostureTransit done;
    done.recline = 1.0f;
    CHECK(pose_diff(posture_pose(rig, Posture::Sit, BENCH_SEAT_M, PostureTransit{}),
                    sit_pose(rig, BENCH_SEAT_M)) < 1.0e-6f);
    CHECK(pose_diff(posture_pose(rig, Posture::Lie, BED_DECK_M, done),
                    lie_pose(rig, BED_DECK_M)) < 1.0e-6f);

    // И НАЧАЛО ПЕРЕХОДА — СТОЯЧЕЕ ТЕЛО. Доли в нуле дают позу, у которой таз
    // на СТОЯЧЕЙ высоте: переход обязан начинаться там, где человек стоит, а
    // не прыжком в полусогнутое.
    PostureTransit start;
    start.take = 0.0f;
    start.drop = 0.0f;
    start.plan = 0.0f;
    start.settle = 0.0f;
    start.recline = 0.0f;
    const LocalPose s0 = posture_pose(rig, Posture::Sit, BENCH_SEAT_M, start);
    INFO("смещение таза в начале ", s0.pelvis_offset.y);
    CHECK(std::fabs(s0.pelvis_offset.y) < 1.0e-4f);
}

TEST_CASE("переход: таз и глаз идут МОНОТОННО и БЕЗ РЫВКА") {
    const Rig rig = Rig::build(RigProportions::from_config());
    const glm::vec3 standing{0.0f, 0.0f, 0.75f}; // где человек стоял, нажимая E
    constexpr int STEPS = 60;

    for (const auto& [posture, height, name] :
         {std::tuple{Posture::Sit, BENCH_SEAT_M, "сесть"},
          std::tuple{Posture::Lie, BED_DECK_M, "лечь"}}) {
        const auto series = transit_series(rig, posture, height, standing, STEPS);
        const std::vector<float> pelvis = pelvis_track(series);
        const std::vector<float> eye = eye_track(series);
        INFO(name, ": таз ", pelvis.front(), " -> ", pelvis.back(), ", глаз ",
             eye.front(), " -> ", eye.back());
        // ВНИЗ И ТОЛЬКО ВНИЗ. Допуск в тысячную долю хода — это дыхание
        // живого слоя, а не «почти монотонно»: подъём в сотую уже виден.
        CHECK(worst_rise(pelvis) < 0.01f);
        CHECK(worst_rise(eye) < 0.01f);
        // И БЕЗ РЫВКА — ГРУБЫМ ПЛЕЧОМ. Этот порог ловит ПОДМЕНУ КАДРА (у неё
        // 1.0, вдвое больше всего хода на одном шаге) и не ловит прямую: у
        // прямой вторая разность того же порядка, что у честной дуги, и
        // делать вид, будто порог их различает, было бы неправдой. Прямую
        // ловит СЛЕДУЮЩИЙ рукав — по тому, как рывок убывает с сеткой.
        INFO("рывок таза ", worst_jerk(pelvis), ", рывок глаза ", worst_jerk(eye));
        CHECK(worst_jerk(pelvis) < 0.05f);
        CHECK(worst_jerk(eye) < 0.05f);
    }
}

TEST_CASE("переход: КОНТРОЛЬ — прямая и скачок обязаны провалить ту же проверку") {
    // Оба плеча — ПРЕЖНИЕ формы этого же перехода, а не выдуманные: до этой
    // волны блендер вёл позу ЛИНЕЙНО за 0.18 с, а «подмена кадра» — то, во
    // что линейный фейд вырождается на медленной машине, где на весь переход
    // приходится один-два кадра.
    constexpr int STEPS = 60;
    std::vector<float> line;
    std::vector<float> step;
    for (int i = 0; i <= STEPS; ++i) {
        const float t = lead_in_t(i, STEPS); // ТА ЖЕ лента, что у живого перехода
        line.push_back(1.0f - t);
        step.push_back(t < 0.5f ? 1.0f : 0.0f);
    }
    INFO("рывок прямой ", worst_jerk(line), ", рывок скачка ", worst_jerk(step));
    CHECK(worst_jerk(step) > 0.05f); // тот же грубый порог, что у живого перехода
    // ...А ПРЯМУЮ ЭТОТ ПОРОГ НЕ ЛОВИТ, и это сказано вслух: её вторая разность
    // (0.013) того же порядка, что у честной дуги (0.010). Ловит её рукав
    // ниже — убыванием рывка с сеткой, которое у излома вдвое, а у дуги
    // вчетверо. Порог, про который говорят, что он ловит больше, чем ловит,
    // хуже отсутствующего.
    CHECK(worst_jerk(line) < 0.05f);
    // ...и монотонны ОБА: критерий монотонности сам по себе прямую не ловит,
    // и говорить, что он её ловит, было бы неправдой.
    CHECK(worst_rise(line) < 0.01f);
    CHECK(worst_rise(step) < 0.01f);
}

TEST_CASE("переход: рывок УБЫВАЕТ КАК ПОЛОЖЕНО — вчетверо на вдвое мелкой сетке") {
    // ЭТО И ЕСТЬ ГЛАВНЫЙ КРИТЕРИЙ «БЕЗ РЫВКА» этой волны.
    // ПОРОГ ЗАВИСИТ ОТ СЕТКИ, А ЭТО — НЕТ, и потому здесь второй критерий, а
    // не только число выше. У гладкой кривой вторая разность идёт как h²
    // (вчетверо на вдвое мелкой сетке), у излома — как h (вдвое). Разница
    // между «плавно» и «линейно» это ровно она, и никакой порог её не
    // подменяет.
    const Rig rig = Rig::build(RigProportions::from_config());
    const glm::vec3 standing{0.0f, 0.0f, 0.75f};
    const auto ratio_of = [](const std::vector<float>& a, const std::vector<float>& b) {
        return worst_jerk(a) / std::max(1.0e-9f, worst_jerk(b));
    };

    const auto s60 = transit_series(rig, Posture::Lie, BED_DECK_M, standing, 60);
    const auto s120 = transit_series(rig, Posture::Lie, BED_DECK_M, standing, 120);
    const float smooth = ratio_of(pelvis_track(s60), pelvis_track(s120));
    INFO("живой переход: рывок падает в ", smooth, " раза");
    CHECK(smooth > 3.0f);

    // КОНТРОЛЬ — ПРЯМАЯ: у неё излом на стыке, и он падает лишь ВДВОЕ.
    std::vector<float> l60;
    std::vector<float> l120;
    for (int i = 0; i <= 60; ++i) {
        l60.push_back(1.0f - lead_in_t(i, 60));
    }
    for (int i = 0; i <= 120; ++i) {
        l120.push_back(1.0f - lead_in_t(i, 120));
    }
    const float corner = ratio_of(l60, l120);
    INFO("прямая: рывок падает в ", corner, " раза");
    CHECK(corner < 2.5f);

    // КОНТРОЛЬ ВТОРОЙ — ПОДМЕНА КАДРА: у неё рывок вообще не убывает.
    std::vector<float> step60;
    std::vector<float> step120;
    for (int i = 0; i <= 60; ++i) {
        step60.push_back(lead_in_t(i, 60) < 0.5f ? 1.0f : 0.0f);
    }
    for (int i = 0; i <= 120; ++i) {
        step120.push_back(lead_in_t(i, 120) < 0.5f ? 1.0f : 0.0f);
    }
    INFO("скачок: рывок падает в ", ratio_of(step60, step120), " раза");
    CHECK(ratio_of(step60, step120) < 2.5f);
}

TEST_CASE("переход: ни один сустав не выходит за пределы по дороге") {
    const Rig rig = Rig::build(RigProportions::from_config());
    constexpr int STEPS = 40;
    float worst = 0.0f;
    for (const auto& [posture, height] : {std::pair{Posture::Sit, BENCH_SEAT_M},
                                          std::pair{Posture::Lie, BED_DECK_M}}) {
        for (int i = 0; i <= STEPS; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(STEPS);
            const LocalPose raw = posture_pose(rig, posture, height,
                                               posture_transit(posture, t));
            LocalPose limited = raw;
            apply_joint_limits(rig, limited);
            worst = std::max(worst, pose_diff(raw, limited));
        }
    }
    // ПРЕДЕЛЫ НИЧЕГО НЕ ПОПРАВИЛИ — значит переход их и не нарушал. Проверка
    // именно такая, а не «после пределов поза законна»: после них она законна
    // ВСЕГДА, и такой критерий не мерил бы ничего.
    INFO("худшая правка пределов по всему переходу ", worst);
    CHECK(worst < 1.0e-5f);

    // КОНТРОЛЬ: колено, согнутое НЕ В ТУ СТОРОНУ, пределами правится — значит
    // прибор выше действительно умеет говорить «нет».
    LocalPose bad;
    bad.rotation[bone_index(Bone::ShinL)] =
        glm::angleAxis(0.9f, glm::vec3{1.0f, 0.0f, 0.0f});
    LocalPose fixed = bad;
    apply_joint_limits(rig, fixed);
    CHECK(pose_diff(bad, fixed) > 1.0e-3f);
}

TEST_CASE("переход: доли — дуга, а не отрезок; лечь дольше, чем сесть") {
    // КОРЕНЬ ОПЕРЕЖАЕТ СПУСК у сиденья: человек сначала оказывается НАД
    // лавкой и только потом опускается. Одна общая доля дала бы отрезок
    // наискось, то есть проход тазом сквозь кромку настила.
    const PostureTransit mid = posture_transit(Posture::Sit, 0.35f);
    INFO("сидя на 0.35: перенос ", mid.plan, ", спуск ", mid.drop);
    CHECK(mid.plan > mid.drop + 0.15f);

    // ЛЁЖА ДВА ХОДА ПОДРЯД: сперва вниз, потом на спину. На середине спуска
    // откидывания ещё почти нет.
    const PostureTransit lie_mid = posture_transit(Posture::Lie, 0.40f);
    INFO("лёжа на 0.40: спуск ", lie_mid.drop, ", откидывание ", lie_mid.recline);
    CHECK(lie_mid.drop > 0.5f);
    CHECK(lie_mid.recline < 0.05f);

    // Концы точные у всех долей — иначе поза в конце перехода была бы «почти».
    for (const Posture p : {Posture::Sit, Posture::Lie}) {
        const PostureTransit end = posture_transit(p, 1.0f);
        CHECK(end.take == doctest::Approx(1.0f));
        CHECK(end.drop == doctest::Approx(1.0f));
        CHECK(end.plan == doctest::Approx(1.0f));
        CHECK(end.settle == doctest::Approx(1.0f));
        const PostureTransit zero = posture_transit(p, 0.0f);
        CHECK(zero.take == doctest::Approx(0.0f));
        CHECK(zero.drop == doctest::Approx(0.0f));
        CHECK(zero.settle == doctest::Approx(0.0f));
    }
    CHECK(posture_transit(Posture::Lie, 1.0f).recline == doctest::Approx(1.0f));
    CHECK(posture_transit(Posture::Sit, 0.5f).recline == doctest::Approx(0.0f));

    // ДЛИТЕЛЬНОСТЬ — СВОЙСТВО ПОЗЫ: лечь путь длиннее на целое откидывание.
    CHECK(posture_transit_s(Posture::Lie) > posture_transit_s(Posture::Sit));
    CHECK(posture_transit_s(Posture::Sit) == doctest::Approx(SIT_TRANSIT_S));
}

TEST_CASE("встают ИЗ ТОЙ позы, в которой лежали") {
    const Rig rig = Rig::build(RigProportions::from_config());
    BodyDrive d;
    d.grounded = true;
    d.posture_height_m = BED_DECK_M;
    // Заявка снята (человек нажал E), блендер ещё в пути — и рисовать обязаны
    // ЛЕЖАЩЕГО. До появления posture_shown «не Lie» означало «Sit», и
    // вставание с кровати шло через сидячую позу одним кадром.
    d.posture = Posture::None;
    d.posture_shown = Posture::Lie;
    d.posture_blend = 1.0f;
    CHECK(drawn_posture(d) == Posture::Lie);
    const LocalPose out = evaluate_body_pose(rig, d);
    const auto m = fk(rig, out);
    // Лежащий смотрит грудью В НЕБО — сидящий никогда.
    const glm::vec3 chest_face = -glm::vec3{m[bone_index(Bone::Torso)][2]};
    INFO("грудь ", chest_face.y);
    CHECK(chest_face.y > 0.95f);
}

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

    // СЕРЕДИНА — МЕЖДУ, НО НЕ ПОСЕРЕДИНЕ, и это не небрежность: место и рыск
    // идут долей `plan`, которая у сиденья ОПЕРЕЖАЕТ спуск таза (дуга). Ждать
    // здесь ровно 5.0 значило бы требовать отрезка наискось — того самого, от
    // которого переход и уходит.
    d.posture_blend = 0.5f;
    const BodyRoot mid_root = body_root_for(d, standing);
    const float plan = posture_transit(Posture::Sit, 0.5f).plan;
    INFO("перенос на середине ", mid_root.ground.x, " при доле ", plan);
    CHECK(mid_root.ground.x == doctest::Approx(10.0f * plan));
    CHECK(mid_root.ground.x > 5.0f);
    CHECK(mid_root.ground.x < 10.0f);
    CHECK(mid_root.yaw == doctest::Approx(0.5f + 1.0f * plan));

    // КОРОТКИМ ПУТЁМ: с 3.0 рад на -3.0 рад — это 0.283 рад через ±pi, а не
    // 6.0 рад обратно. Прямая разность развернула бы сидящего кругом.
    d.facing_yaw = 3.0f;
    d.posture_yaw = -3.0f;
    d.posture_blend = 0.5f;
    const float mid = body_root_for(d, standing).yaw;
    INFO("середина поворота ", mid, " при доле ", plan);
    // Короткий путь идёт ВВЕРХ через ±pi: рыск обязан остаться в узкой полосе
    // между 3.0 и 3.0 + 0.2832. Длинный путь увёл бы его вниз, к нулю, —
    // сидящего развернуло бы кругом.
    CHECK(mid > 3.0f);
    CHECK(mid < 3.0f + 0.2832f);
    CHECK(mid == doctest::Approx(3.0f + 0.283185f * plan).epsilon(0.01));
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
