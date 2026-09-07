/*
Module: tests
File: tests/character/InertializerTests.cpp

Responsibility:
- ПРИБОР ИНЕРЦИАЛИЗАЦИИ (engine/anim/sources/Inertializer.h): кривая держит
  краевые условия (значение и скорость на входе, ноль по значению/скорости на
  выходе, без перелёта), а на суставе стык непрерывен по скорости: поза на
  первом тике после смены — экстраполяция старой, к концу — ровно новая.

Key items:
- the_curve_meets_its_boundary_conditions
- a_joint_switch_is_velocity_continuous
- the_control_arm_is_a_hard_cut (blend 0 — тождество новой позе)

Dependencies:
- Uses: doctest, engine/anim/sources/Inertializer.h, glm.
- Used by: ctest (character_inertializer).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Синтетика, без модели: слой суставов не знает скелета.
*/
#include "engine/anim/sources/Inertializer.h"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

#include <glm/gtc/quaternion.hpp>

using namespace dfn;

TEST_CASE("the_curve_meets_its_boundary_conditions") {
    for (const float v0 : {0.0f, 2.0f, -0.5f, -20.0f}) {
        const anim::Quintic q = anim::Quintic::fit(0.4f, v0, 0.25f);
        CAPTURE(v0);
        CHECK(q.at(0.0f) == doctest::Approx(0.4f));
        const float h = 1.0e-3f;
        CHECK((q.at(h) - q.at(0.0f)) / h == doctest::Approx(v0).epsilon(0.02).scale(1.0f));
        CHECK(q.at(q.t1) == doctest::Approx(0.0f).scale(1.0f));
        CHECK((q.at(q.t1) - q.at(q.t1 - h)) / h == doctest::Approx(0.0f).scale(1.0f).epsilon(0.02));
        // без перелёта через ноль и без разгона разницы (a0 ≥ 0 у Болло)
        float lo = 1.0e9f, hi = -1.0e9f;
        for (int i = 0; i <= 100; ++i) {
            const float x = q.at(q.t1 * static_cast<float>(i) / 100.0f);
            lo = std::min(lo, x);
            hi = std::max(hi, x);
        }
        CHECK(lo >= -1.0e-4f);
        if (v0 <= 0.0f) {
            CHECK(hi <= 0.4f + 1.0e-4f);
        }
        if (v0 < 0.0f) {
            CHECK(q.t1 <= 0.25f + 1.0e-6f); // закрывающаяся разница укорачивает
        }
    }
}

TEST_CASE("a_joint_switch_is_velocity_continuous") {
    const float dt = 1.0f / 60.0f;
    const glm::vec3 axis = glm::normalize(glm::vec3{0.3f, 1.0f, 0.2f});
    // старый клип: сустав вращается 3 рад/с вокруг оси; новый — стоит на 40° в другой ориентации
    auto old_pose = [&](float t) {
        anim::JointLocal j;
        j.rotation = glm::angleAxis(3.0f * t, axis);
        j.translation = glm::vec3{0.0f, 1.0f + 0.5f * t, 0.0f};
        return j;
    };
    anim::JointLocal fresh;
    fresh.rotation = glm::angleAxis(glm::radians(40.0f), glm::vec3{1.0f, 0.0f, 0.0f});
    fresh.translation = glm::vec3{0.1f, 0.9f, 0.0f};
    const std::vector<anim::JointLocal> older{old_pose(-dt)};
    const std::vector<anim::JointLocal> last{old_pose(0.0f)};
    const std::vector<anim::JointLocal> now{fresh};
    anim::Inertializer in;
    in.capture(older, last, now, dt, 0.25f);
    // на самом стыке — ровно старая поза
    std::vector<anim::JointLocal> p = now;
    in.apply(0.0f, p);
    CHECK(std::abs(glm::dot(p[0].rotation, last[0].rotation)) == doctest::Approx(1.0f).epsilon(1.0e-5));
    CHECK(glm::length(p[0].translation - last[0].translation) < 1.0e-5f);
    // через тик — экстраполяция старой скорости (угол ≈ 3·dt дальше, y ≈ +0.5·dt)
    p = now;
    in.apply(dt, p);
    const glm::quat d = glm::inverse(last[0].rotation) * p[0].rotation;
    const float step = 2.0f * std::acos(std::min(1.0f, std::abs(d.w)));
    MESSAGE("шаг ориентации за первый тик " << step << " рад (старая скорость даёт " << 3.0f * dt << ")");
    CHECK(step == doctest::Approx(3.0f * dt).epsilon(0.15));
    CHECK(p[0].translation.y - last[0].translation.y == doctest::Approx(0.5f * dt).epsilon(0.15));
    // к концу — ровно новая
    p = now;
    in.apply(0.25f, p);
    CHECK(std::abs(glm::dot(p[0].rotation, fresh.rotation)) == doctest::Approx(1.0f).epsilon(1.0e-6));
    CHECK(glm::length(p[0].translation - fresh.translation) < 1.0e-6f);
    // ускорение по кривой ограничено: второй разности угла за тик не выше 60 рад/с²
    float worst = 0.0f;
    float prev_ang = 0.0f, prev_om = 0.0f;
    for (int i = 0; i <= 15; ++i) {
        p = now;
        in.apply(dt * static_cast<float>(i), p);
        const glm::quat r = glm::inverse(fresh.rotation) * p[0].rotation;
        const float ang = 2.0f * std::atan2(glm::length(glm::vec3{r.x, r.y, r.z}), std::abs(r.w));
        if (i >= 1) {
            const float om = (ang - prev_ang) / dt;
            if (i >= 2) {
                worst = std::max(worst, std::abs(om - prev_om) / dt);
            }
            prev_om = om;
        }
        prev_ang = ang;
    }
    MESSAGE("худшее угловое ускорение остатка " << worst << " рад/с²");
    CHECK(worst < 60.0f);
    CHECK(in.weight() == doctest::Approx(1.0f));
    in.advance(0.125f);
    CHECK(in.weight() == doctest::Approx(0.5f).epsilon(0.01));
}

TEST_CASE("the_control_arm_is_a_hard_cut") {
    anim::JointLocal a, b;
    a.rotation = glm::angleAxis(1.0f, glm::vec3{0.0f, 1.0f, 0.0f});
    const std::vector<anim::JointLocal> older{a}, last{a}, now{b};
    anim::Inertializer in;
    in.capture(older, last, now, 1.0f / 60.0f, 0.0f);
    CHECK_FALSE(in.active());
    std::vector<anim::JointLocal> p = now;
    in.apply(0.0f, p);
    CHECK(p[0].rotation.w == doctest::Approx(1.0f));
    CHECK(in.weight() == 0.0f);
}
