/*
File: tests/character/PoseLibraryTests.cpp

Responsibility:
- ПРИЁМКА РЕЕСТРА ПОЗ И ГРАФА ПЕРЕХОДОВ. Три утверждения заказа, каждое
  числом: (1) всякая поза законна по пределам суставов САМА, а не после
  зажима на выходе; (2) поза, объявившая опору, действительно на ней стоит —
  ни одна кость не уходит в землю; (3) переход из любой позы в любую идёт
  через посредников и не рвёт суставов быстрее уже принятой походки.

Dependencies:
- Uses: engine/anim (PoseLibrary, Pose, Rig, Clips), doctest. Ни файла, ни
  окна: реестр обязан считаться без единого ассета, и это часть контракта.

AI Agents Notice:
- Follow docs/ARCHITECTURE.md strictly.
- КОНТРОЛЬНАЯ РУКА ЖИВЁТ ВНУТРИ НАБОРА (правило 47). Потолок угловой
  скорости судится не абсолютным числом, а против ПИКА ПОХОДКИ, замеренного
  тут же тем же кодом: судья, у которого нечем провалить проверку, ничего не
  меряет.
*/

#include "engine/anim/sources/Clips.h"
#include "engine/anim/sources/PoseLibrary.h"
#include "engine/anim/sources/Rig.h"
#include "engine/core/config/sources/Constants.h"

#include <doctest/doctest.h>

#include <cmath>
#include <string>
#include <vector>

using namespace dfn::anim;

namespace {

[[nodiscard]] Rig test_rig() { return Rig::build(RigProportions::from_config()); }

/// Угол между двумя поворотами, рад. Один и тот же на всю приёмку.
[[nodiscard]] float quat_angle(const glm::quat& a, const glm::quat& b) {
    const float d = std::abs(glm::dot(a, b));
    return 2.0f * std::acos(std::min(1.0f, d));
}

/// САМАЯ НИЗКАЯ КОСТЬ ПОЗЫ НАД ЗЕМЛЁЙ, м. Корень стоит в начале координат,
/// поэтому «земля» это ровно y = 0.
[[nodiscard]] float lowest_joint_y(const Rig& rig, const LocalPose& pose) {
    std::array<glm::mat4, BONE_COUNT> m{};
    forward_kinematics(rig, pose, BodyRoot{}, m);
    float lo = 1.0e9f;
    for (uint32_t b = 0; b < BONE_COUNT; ++b) {
        lo = std::min(lo, m[b][3].y);
    }
    return lo;
}

/// ПИК УГЛОВОЙ СКОРОСТИ ПРИНЯТОЙ ПОХОДКИ — КОНТРОЛЬНАЯ РУКА. Считается тем
/// же выражением, что и пик перехода, на той же частоте.
[[nodiscard]] float gait_peak_rate(const Rig& rig, float speed_mps, Gait gear, float dt) {
    const auto step = static_cast<float>(dfn::config::STEP_LENGTH_BASE)
                    + static_cast<float>(dfn::config::STEP_LENGTH_PER_MPS) * speed_mps;
    const float cycle = 2.0f * step / speed_mps;
    float worst = 0.0f;
    LocalPose prev = gait_pose(rig, 0.0f, step, gait_run_weight(gear));
    apply_joint_limits(rig, prev);
    for (float t = dt; t <= cycle * 2.0f; t += dt) {
        LocalPose cur =
            gait_pose(rig, std::fmod(t / cycle, 1.0f), step, gait_run_weight(gear));
        apply_joint_limits(rig, cur);
        for (uint32_t j = 0; j < BONE_COUNT; ++j) {
            worst = std::max(worst, quat_angle(prev.rotation[j], cur.rotation[j]) / dt);
        }
        prev = cur;
    }
    return worst;
}

} // namespace

TEST_CASE("реестр поз: имена уникальны, а первые четыре записи — сами опоры") {
    std::vector<std::string> seen;
    for (uint32_t i = 0; i < POSE_COUNT; ++i) {
        const auto id = static_cast<PoseId>(i);
        const PoseRecord& r = pose_record(id);
        CHECK_FALSE(r.name.empty());
        CHECK_FALSE(r.label.empty());
        const std::string n{r.name};
        for (const std::string& other : seen) {
            CHECK(other != n);
        }
        seen.push_back(n);
        bool found = false;
        CHECK(pose_by_name(r.name, &found) == id);
        CHECK(found);
    }
    // ЦЕПЬ ОПОР — КОНТРАКТ: маршрут читает опору по индексу записи, и сдвиг
    // любой из четырёх молча увёл бы каждый переход не туда.
    CHECK(pose_support(PoseId::Stand) == Support::Stand);
    CHECK(pose_support(PoseId::Crouch) == Support::Crouch);
    CHECK(pose_support(PoseId::SitFloor) == Support::SitFloor);
    CHECK(pose_support(PoseId::LieProne) == Support::LieProne);

    bool found = true;
    CHECK(pose_by_name("нет такой позы", &found) == PoseId::Stand);
    CHECK_FALSE(found);
}

TEST_CASE("всякая поза законна САМА: наложение пределов её не меняет") {
    const Rig rig = test_rig();
    // Порог — не ноль, потому что кватернион строится через тригонометрию и
    // сравнивается через acos; 0.2 градуса это шум представления, а настоящее
    // нарушение предела стоит десятками градусов (замерено на черновике
    // двуручной стойки: локоть просился на 165° при пределе 150 и давал 15.9).
    constexpr float NOISE_RAD = 0.0035f;
    for (uint32_t i = 0; i < POSE_COUNT; ++i) {
        const auto id = static_cast<PoseId>(i);
        const LocalPose raw = pose_of(rig, id);
        LocalPose clamped = raw;
        apply_joint_limits(rig, clamped);
        for (uint32_t b = 0; b < BONE_COUNT; ++b) {
            const float moved = quat_angle(raw.rotation[b], clamped.rotation[b]);
            INFO("поза ", pose_name(id), ", кость ", bone_name(static_cast<Bone>(b)),
                 ": зажим повернул на ", moved * 57.2958f, " градусов");
            CHECK(moved < NOISE_RAD);
        }
    }
}

TEST_CASE("поза стоит на своей опоре: ни одна кость не уходит в землю") {
    const Rig rig = test_rig();
    // Допуск вниз — толщина численного шума, а не «немножко можно»: поза,
    // у которой стопа на сантиметр в земле, на кадре видна.
    constexpr float SINK_M = 0.02f;
    // И сверху: тело, объявившее опору, обязано ЛЕЖАТЬ на ней, а не парить.
    // Полметра — это высота, на которой у самой высокой из низких поз (упор
    // лёжа) стоит таз; выше начинается стоячая фигура.
    constexpr float FLOAT_M = 0.30f;
    for (uint32_t i = 0; i < POSE_COUNT; ++i) {
        const auto id = static_cast<PoseId>(i);
        const PoseRecord& r = pose_record(id);
        if (r.upper_only) {
            continue; // оружейные стойки ног не трогают: их ведёт локомоция
        }
        const float lo = lowest_joint_y(rig, pose_of(rig, id));
        INFO("поза ", pose_name(id), ": нижняя кость на ", lo, " м");
        CHECK(lo > -SINK_M);
        if (r.support != Support::Stand) {
            CHECK(lo < FLOAT_M);
        }
    }
}

TEST_CASE("руки приходят туда, куда их послали") {
    const Rig rig = test_rig();
    const float unit = rig.proportions.standing_hip_height();
    // Цель, недосягаемая для руки, честно даёт прямую руку — таких в реестре
    // две (упор в пол сидя), и промах у них не больше двух сантиметров.
    constexpr float MISS_M = 0.025f;
    std::array<glm::mat4, BONE_COUNT> m{};
    for (uint32_t i = 0; i < POSE_COUNT; ++i) {
        const auto id = static_cast<PoseId>(i);
        const PoseRecord& r = pose_record(id);
        const LocalPose pose = pose_of(rig, id);
        forward_kinematics(rig, pose, BodyRoot{}, m);
        // Точка цели дана В СИСТЕМЕ ТАЗА, поэтому и мерить надо там же:
        // кисть переводится в систему таза его же матрицей.
        const glm::mat4 to_pelvis = glm::inverse(m[bone_index(Bone::Pelvis)]);
        for (int side = 0; side < 2; ++side) {
            ArmTarget t = r.arm[static_cast<std::size_t>(side)];
            if (side == 1 && r.symmetric) {
                t = r.arm[0];
                t.at.x = -t.at.x;
            }
            if (!t.used) {
                continue;
            }
            const Bone hand = side == 0 ? Bone::HandL : Bone::HandR;
            const glm::vec3 got{to_pelvis * m[bone_index(hand)][3]};
            const float miss = glm::length(got - t.at * unit);
            INFO("поза ", pose_name(id), ", сторона ", side, ": промах ", miss, " м");
            CHECK(miss < MISS_M);
        }
    }
}

TEST_CASE("маршрут из любой позы в любую идёт через посредников") {
    for (uint32_t a = 0; a < POSE_COUNT; ++a) {
        for (uint32_t b = 0; b < POSE_COUNT; ++b) {
            const auto from = static_cast<PoseId>(a);
            const auto to = static_cast<PoseId>(b);
            const PoseRoute r = pose_route(from, to);
            INFO("маршрут ", pose_name(from), " -> ", pose_name(to));
            REQUIRE(r.count >= 1);
            REQUIRE(r.count <= POSE_ROUTE_MAX);
            CHECK(r.step[0] == from);
            CHECK(r.step[r.count - 1] == to);
            for (uint32_t k = 0; k + 1 < r.count; ++k) {
                CHECK(r.step[k] != r.step[k + 1]); // колен нулевой длины нет
                CHECK(r.leg_s[k] > 0.0f);
                // СОСЕДНИЕ УЗЛЫ ЛИБО ОДНОЙ ОПОРЫ, ЛИБО СОСЕДНИХ ОПОР: это и
                // есть «через посредников», выраженное числом. Прямое ребро
                // между полом и стойкой сюда не пролезет.
                const int sa = static_cast<int>(pose_support(r.step[k]));
                const int sb = static_cast<int>(pose_support(r.step[k + 1]));
                CHECK(std::abs(sa - sb) <= 1);
            }
            if (from == to) {
                CHECK(r.count == 1);
                CHECK(r.total_s() == doctest::Approx(0.0f));
            } else {
                CHECK(r.total_s() > 0.0f);
            }
        }
    }
    // САМЫЙ ДЛИННЫЙ ПУТЬ ЕСТЬ ТОТ, КОТОРОГО ТРЕБУЕТ ЗАКАЗ: из лежачей позы в
    // стоячую — через сидя на полу и через присед, шестью узлами.
    const PoseRoute worst = pose_route(PoseId::PushUp, PoseId::HandsOnHips);
    CHECK(worst.count == 6);
    CHECK(worst.step[1] == PoseId::LieProne);
    CHECK(worst.step[2] == PoseId::SitFloor);
    CHECK(worst.step[3] == PoseId::Crouch);
    CHECK(worst.step[4] == PoseId::Stand);
}

TEST_CASE("переход не рвёт суставов быстрее принятой походки") {
    const Rig rig = test_rig();
    constexpr float DT = 1.0f / 60.0f;

    // КОНТРОЛЬНАЯ РУКА: то же выражение на движении, которое УЖЕ принято.
    const float walk_peak = gait_peak_rate(rig, static_cast<float>(dfn::config::WALK_SPEED),
                                           Gait::Walk, DT);
    const float run_peak = gait_peak_rate(rig, static_cast<float>(dfn::config::RUN_SPEED),
                                          Gait::Run, DT);
    INFO("пик походки: шаг ", walk_peak, " рад/с, бег ", run_peak, " рад/с");
    CHECK(walk_peak > POSE_MAX_JOINT_RATE); // рука обязана быть ВЫШЕ потолка,
    CHECK(run_peak > POSE_MAX_JOINT_RATE);  // иначе судить нечем

    float worst = 0.0f;
    std::string worst_where;
    for (uint32_t a = 0; a < POSE_COUNT; ++a) {
        for (uint32_t b = 0; b < POSE_COUNT; ++b) {
            PoseTransit tr = pose_transit_at(static_cast<PoseId>(a));
            pose_transit_begin(tr, static_cast<PoseId>(b));
            LocalPose prev = pose_transit_pose(rig, tr);
            apply_joint_limits(rig, prev);
            int guard = 0;
            while (tr.moving && guard++ < 4000) {
                pose_transit_advance(tr, DT);
                LocalPose cur = pose_transit_pose(rig, tr);
                apply_joint_limits(rig, cur);
                for (uint32_t j = 0; j < BONE_COUNT; ++j) {
                    const float w = quat_angle(prev.rotation[j], cur.rotation[j]) / DT;
                    if (w > worst) {
                        worst = w;
                        worst_where = std::string{pose_name(static_cast<PoseId>(a))} + " -> "
                                    + std::string{pose_name(static_cast<PoseId>(b))} + " ("
                                    + std::string{bone_name(static_cast<Bone>(j))} + ")";
                    }
                }
                prev = cur;
            }
            CHECK(guard < 4000); // маршрут обязан кончаться
        }
    }
    INFO("худший переход: ", worst, " рад/с на ", worst_where);
    CHECK(worst < POSE_MAX_JOINT_RATE);
    CHECK(worst < walk_peak);
}

TEST_CASE("переход кончается ровно в цели и по своим часам") {
    const Rig rig = test_rig();
    PoseTransit tr = pose_transit_at(PoseId::LieProne);
    CHECK_FALSE(tr.moving);
    CHECK(tr.from() == PoseId::LieProne);

    pose_transit_begin(tr, PoseId::HandsOnHips);
    CHECK(tr.moving);
    const float total = tr.route.total_s();
    CHECK(total > 0.0f);
    // Один шаг ЗА ВСЁ ВРЕМЯ и много мелких обязаны привести в одно и то же
    // место: переход не имеет собственных часов, и время — параметр.
    PoseTransit big = tr;
    pose_transit_advance(big, total + 0.001f);
    CHECK_FALSE(big.moving);
    CHECK(big.from() == PoseId::HandsOnHips);

    PoseTransit small = tr;
    for (int i = 0; i < 400 && small.moving; ++i) {
        pose_transit_advance(small, 1.0f / 60.0f);
    }
    CHECK_FALSE(small.moving);
    CHECK(small.from() == PoseId::HandsOnHips);

    // На середине последнего колена поза обязана отличаться от обоих концов —
    // иначе «переход» был бы телепортом с задержкой.
    PoseTransit mid = pose_transit_at(PoseId::Stand);
    pose_transit_begin(mid, PoseId::HandsOnHips);
    pose_transit_advance(mid, POSE_TO_SUPPORT_S * 0.5f);
    const LocalPose half = pose_transit_pose(rig, mid);
    const LocalPose a = pose_of(rig, PoseId::Stand);
    const LocalPose b = pose_of(rig, PoseId::HandsOnHips);
    float from_a = 0.0f;
    float from_b = 0.0f;
    for (uint32_t j = 0; j < BONE_COUNT; ++j) {
        from_a = std::max(from_a, quat_angle(half.rotation[j], a.rotation[j]));
        from_b = std::max(from_b, quat_angle(half.rotation[j], b.rotation[j]));
    }
    CHECK(from_a > 0.05f);
    CHECK(from_b > 0.05f);
}

TEST_CASE("перехват на полпути не разворачивает тело обратно") {
    // Смена цели посреди вставания обязана вести маршрут ВПЕРЁД от ближайшего
    // узла, а не от того, откуда встали.
    PoseTransit tr = pose_transit_at(PoseId::LieProne);
    pose_transit_begin(tr, PoseId::Stand);
    // Доехали до сидя на полу и ушли за середину следующего колена.
    pose_transit_advance(tr, SUPPORT_SITFLOOR_LIEPRONE_S + SUPPORT_CROUCH_SITFLOOR_S * 0.8f);
    CHECK(tr.from() == PoseId::SitFloor);
    CHECK(tr.to() == PoseId::Crouch);
    pose_transit_begin(tr, PoseId::SitCross);
    // Ближайший узел — присед, поэтому обратно на пол идём ЧЕРЕЗ него, и
    // первый узел маршрута именно он.
    CHECK(tr.route.step[0] == PoseId::Crouch);
    CHECK(tr.target() == PoseId::SitCross);
}

TEST_CASE("сидение на стуле считается той же функцией, что и посадка на лавку") {
    const Rig rig = test_rig();
    // ВЫСОТА СИДЕНЬЯ — ПАРАМЕТР, и это проверяемо: две высоты дают две разные
    // позы, а одна и та же высота — одну и ту же. Будь угол ноги записан
    // числом в реестре, обе вышли бы одинаковыми.
    const LocalPose low = pose_of(rig, PoseId::SitChairHandsKnees, 0.35f);
    const LocalPose high = pose_of(rig, PoseId::SitChairHandsKnees, 0.55f);
    const LocalPose again = pose_of(rig, PoseId::SitChairHandsKnees, 0.35f);
    float diff = 0.0f;
    float same = 0.0f;
    for (uint32_t j = 0; j < BONE_COUNT; ++j) {
        diff = std::max(diff, quat_angle(low.rotation[j], high.rotation[j]));
        same = std::max(same, quat_angle(low.rotation[j], again.rotation[j]));
    }
    CHECK(diff > 0.05f);
    // Порог — шум представления: кватернион строится тригонометрией, а
    // сравнивается через acos, у которого производная у единицы бесконечна.
    CHECK(same < 2.0e-3f);
    CHECK(low.pelvis_offset.y < high.pelvis_offset.y);

    // А у позы, которая не на сиденье, высота не читается вовсе.
    const LocalPose s1 = pose_of(rig, PoseId::SitCross, 0.35f);
    const LocalPose s2 = pose_of(rig, PoseId::SitCross, 0.55f);
    for (uint32_t j = 0; j < BONE_COUNT; ++j) {
        CHECK(quat_angle(s1.rotation[j], s2.rotation[j]) < 2.0e-3f);
    }
}
