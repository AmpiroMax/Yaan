/*
Module: tests/sim
File: tests/sim/FootPhysicsTests.cpp

Responsibility:
- ПРИБОРЫ ФИЗИЧЕСКИХ СТОП (заказ владельца 04.09: «ноги как физический объект
  на физическую землю с трением: держат на малых склонах, скользят на
  больших»; LOCOMOTION_GROUNDED.md §12). На НАСТОЯЩЕМ Jolt: стопа на
  плоскости стоит; 5° по граниту держит; 30° по стеклу ползёт с ускорением
  g·(sin θ − μ·cos θ) ± FOOT_SLIP_ACCEL_TOL; переход «держит → ползёт» на
  arctan(μ) ± FOOT_SLIP_ANGLE_TOL_DEG; контакт с ящиком — касание, нормаль,
  вещество; стопа не трогает капсулу и хитбоксы; цена двух стоп за тик
  разностью против руки без стоп. Плюс null-бэкенд: та же семантика без Jolt.

Key items:
- Slope: наклонная плита из вещества под углом θ; foot_on(): стопа на ней.
- Каждая приёмка идёт со случаем, который обязана отвергнуть (правило 30):
  45° на том же граните ползёт; кинематическая стопа на том же стекле стоит;
  та же коробка на LAYER_STATIC держит стопу, на LAYER_HITBOX — нет.

Dependencies:
- Uses: platform Jolt + null backends, engine/physics (слои), core
  PhysicsSubstance, generated constants (SIM_DT, GRAVITY, FOOT_*), doctest.
- Used by: ctest (sim_foot_physics).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Волна «контракт физических стоп».
- ФИЗИКА ШАГАЕТ РОВНО РАЗ ЗА ТИК С SIM_DT (правило 12).
- Цена — мера, не ворота (правило 38): печать и WARN, не красный по часам.
*/

#include <doctest/doctest.h>

#include "engine/core/config/sources/Constants.h"
#include "engine/core/materials/sources/PhysicsSubstance.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"
#include "engine/platform/physics/sources/null/CreateNullPhysics.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace dfn;

namespace {

constexpr float DT = static_cast<float>(config::SIM_DT);
constexpr float G = static_cast<float>(config::GRAVITY);
constexpr float PI = 3.14159265358979f;

/// Полуразмеры стопы — порядок хитбокса стопы взрослого: 10 см поперёк,
/// 6 см толщины подошвы, 26 см вдоль.
constexpr glm::vec3 FOOT_HALF{0.05f, 0.03f, 0.13f};

/// ЗАЗОР ПОСАДКИ: стопа кладётся на миллиметр над поверхностью, чтобы не
/// родиться в проникновении (решатель выталкивал бы её сам).
constexpr float SEAT_GAP_M = 0.001f;

[[nodiscard]] float deg(float rad) { return rad * 180.0f / PI; }
[[nodiscard]] float rad(float degrees) { return degrees * PI / 180.0f; }

/// Закон: ускорение ползущего по склону тела при паре трения mu.
[[nodiscard]] float slide_law(float theta, float mu) {
    return G * (std::sin(theta) - mu * std::cos(theta));
}

std::vector<glm::vec3> box_points(glm::vec3 half) {
    return {{-half.x, -half.y, -half.z}, {half.x, -half.y, -half.z},
            {half.x, -half.y, half.z},   {-half.x, -half.y, half.z},
            {-half.x, half.y, -half.z},  {half.x, half.y, -half.z},
            {half.x, half.y, half.z},    {-half.x, half.y, half.z}};
}

struct Rig {
    std::unique_ptr<platform::IPhysics> physics;
    /// Плита под углом theta (вокруг Z): нормаль n, спуск d вдоль плиты.
    glm::quat slope_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec3 downhill{-1.0f, 0.0f, 0.0f};

    explicit Rig(bool jolt = true) {
        physics = jolt ? platform::create_jolt_physics() : platform::create_null_physics();
        REQUIRE(physics->init());
    }
    ~Rig() { physics->shutdown(); }

    /// Наклонная плита из вещества `substance`, верхняя грань через начало.
    platform::PhysicsBodyHandle slope(float theta, const char* substance,
                                      platform::CollisionMask layer = physics::LAYER_STATIC) {
        slope_rotation = glm::angleAxis(theta, glm::vec3{0.0f, 0.0f, 1.0f});
        normal = slope_rotation * glm::vec3{0.0f, 1.0f, 0.0f};
        downhill = -glm::normalize(slope_rotation * glm::vec3{1.0f, 0.0f, 0.0f})
                   * (theta >= 0.0f ? 1.0f : -1.0f);
        // Спуск — проекция тяжести на плиту; для theta > 0 это −(cos, sin, 0).
        downhill = glm::vec3{-std::cos(theta), -std::sin(theta), 0.0f};
        platform::StaticBoxDesc desc;
        desc.half_extents = {20.0f, 0.5f, 20.0f};
        desc.center = slope_rotation * glm::vec3{0.0f, -0.5f, 0.0f};
        desc.rotation = slope_rotation;
        desc.layer = layer;
        desc.substance = core::find_substance(substance);
        REQUIRE(desc.substance != core::SUBSTANCE_NONE);
        desc.user_data = 7;
        const auto h = physics->create_static_box(desc);
        REQUIRE(h.valid());
        return h;
    }

    /// Стопа, лежащая плашмя на плите в её начале, из вещества подошвы.
    platform::PhysicsBodyHandle foot_on(const char* sole = "leather",
                                        float mass = static_cast<float>(config::FOOT_BODY_MASS_KG),
                                        glm::vec3 at = glm::vec3{0.0f}) {
        platform::FootBodyDesc desc;
        desc.half_extents = FOOT_HALF;
        desc.position = at + normal * (FOOT_HALF.y + SEAT_GAP_M);
        desc.rotation = slope_rotation;
        desc.mass_kg = mass;
        desc.substance = core::find_substance(sole);
        REQUIRE(desc.substance != core::SUBSTANCE_NONE);
        desc.layer = physics::LAYER_FOOT;
        desc.collides_with = physics::LAYER_STATIC | physics::LAYER_LOOSE
                             | physics::LAYER_INTERACTABLE;
        desc.user_data = 11;
        const auto h = physics->create_foot_body(desc);
        REQUIRE(h.valid());
        return h;
    }

    void run(float seconds) {
        const int steps = static_cast<int>(std::lround(seconds / DT));
        for (int i = 0; i < steps; ++i) {
            physics->step(DT);
        }
    }

    /// Путь стопы вдоль спуска от точки посадки.
    [[nodiscard]] float slid(platform::PhysicsBodyHandle foot, glm::vec3 start) const {
        return glm::dot(physics->body_pose(foot).position - start, downhill);
    }
};

/// Пара трения по правилу PhysicsSubstance.h: sqrt(f1·f2).
[[nodiscard]] float pair_friction(const char* a, const char* b) {
    return std::sqrt(core::substance(core::find_substance(a)).friction
                     * core::substance(core::find_substance(b)).friction);
}

} // namespace

TEST_CASE("стопа на плоскости стоит: касание, нормаль вверх, держит, скольжение ноль") {
    Rig rig;
    rig.slope(0.0f, "granite");
    const auto foot = rig.foot_on();
    CHECK(rig.physics->foot_mode(foot) == platform::FootMode::Swing);
    rig.physics->set_foot_mode(foot, platform::FootMode::Plant);
    CHECK(rig.physics->foot_mode(foot) == platform::FootMode::Plant);
    const glm::vec3 start = rig.physics->body_pose(foot).position;
    rig.run(1.0f);
    const platform::FootContact c = rig.physics->foot_contact(foot);
    const float moved = glm::length(rig.physics->body_pose(foot).position - start);
    std::printf("[плоскость] касание=%d n=(%.3f %.3f %.3f) глубина=%.4f мм tan=%.3f "
                "пара=%.3f держит=%d скольжение=%.5f м/с сдвиг=%.3f мм\n",
                static_cast<int>(c.touching), static_cast<double>(c.normal.x),
                static_cast<double>(c.normal.y), static_cast<double>(c.normal.z),
                static_cast<double>(c.depth * 1000.0f), static_cast<double>(c.slope_tan),
                static_cast<double>(c.friction_pair), static_cast<int>(c.holds),
                static_cast<double>(c.slip_speed_mps), static_cast<double>(moved * 1000.0f));
    CHECK(c.touching);
    CHECK(c.normal.y > 0.999f);
    CHECK(c.ground == core::find_substance("granite"));
    CHECK(c.ground_user_data == 7);
    CHECK(c.friction_pair == doctest::Approx(pair_friction("leather", "granite")));
    CHECK(c.holds);
    CHECK(c.slip_speed_mps < 1e-3f);
    // Сдвиг по вертикали — посадка на миллиметр; горизонтально — ничего.
    CHECK(glm::length(glm::vec3{rig.physics->body_pose(foot).position.x - start.x, 0.0f,
                                rig.physics->body_pose(foot).position.z - start.z})
          < static_cast<float>(config::FOOT_SLIDE_MAX_M));

    // КОНТРОЛЬ: стопа, поднятая в мах на полметра, ничего не касается.
    rig.physics->set_foot_mode(foot, platform::FootMode::Swing);
    rig.physics->set_foot_kinematic_pose(foot, platform::BodyPose{start + glm::vec3{0.0f, 0.5f, 0.0f}});
    rig.run(0.1f);
    const platform::FootContact air = rig.physics->foot_contact(foot);
    CHECK_FALSE(air.touching);
    CHECK(rig.physics->body_pose(foot).position.y == doctest::Approx(start.y + 0.5f).epsilon(0.01));
}

TEST_CASE("склон 5° по граниту держит; контроль — 45° на том же граните ползёт") {
    const float mu = pair_friction("leather", "granite");
    std::printf("[гранит] пара трения %.3f, порог %.1f°\n", static_cast<double>(mu),
                static_cast<double>(deg(std::atan(mu))));
    {
        Rig rig;
        rig.slope(rad(5.0f), "granite");
        const auto foot = rig.foot_on();
        rig.physics->set_foot_mode(foot, platform::FootMode::Plant);
        rig.run(0.05f); // посадка: миллиметр зазора съедается тяжестью
        const glm::vec3 start = rig.physics->body_pose(foot).position;
        rig.run(2.0f);
        const float s = rig.slid(foot, start);
        const platform::FootContact c = rig.physics->foot_contact(foot);
        std::printf("[5° гранит] путь за 2 с %.3f мм, tan=%.3f держит=%d скольжение=%.5f м/с\n",
                    static_cast<double>(s * 1000.0f), static_cast<double>(c.slope_tan),
                    static_cast<int>(c.holds), static_cast<double>(c.slip_speed_mps));
        CHECK(c.touching);
        CHECK(c.slope_tan == doctest::Approx(std::tan(rad(5.0f))).epsilon(0.02));
        CHECK(c.holds);
        CHECK(std::abs(s) < static_cast<float>(config::FOOT_SLIDE_MAX_M));
        CHECK(c.slip_speed_mps < 1e-3f);
    }
    {
        Rig rig;
        rig.slope(rad(45.0f), "granite");
        const auto foot = rig.foot_on();
        rig.physics->set_foot_mode(foot, platform::FootMode::Plant);
        const glm::vec3 start = rig.physics->body_pose(foot).position;
        rig.run(1.0f);
        const float s = rig.slid(foot, start);
        const platform::FootContact c = rig.physics->foot_contact(foot);
        std::printf("[45° гранит, контроль] путь за 1 с %.3f м, держит=%d скольжение=%.3f м/с\n",
                    static_cast<double>(s), static_cast<int>(c.holds),
                    static_cast<double>(c.slip_speed_mps));
        CHECK_FALSE(c.holds);
        CHECK(s > 0.1f);
        CHECK(c.slip_speed_mps > 0.5f);
    }
}

TEST_CASE("30° по стеклу (самое скользкое вещество таблицы) ползёт с ускорением закона ±10 %") {
    const float theta = rad(30.0f);
    const float mu = pair_friction("leather", "glass");
    const float law = slide_law(theta, mu);
    std::printf("[30° стекло] пара %.3f, порог %.1f°, закон %.3f м/с²\n", static_cast<double>(mu),
                static_cast<double>(deg(std::atan(mu))), static_cast<double>(law));
    REQUIRE(law > 0.0f);

    const auto measure = [&](float mass, bool plant) {
        Rig rig;
        rig.slope(theta, "glass");
        const auto foot = rig.foot_on("leather", mass);
        if (plant) {
            rig.physics->set_foot_mode(foot, platform::FootMode::Plant);
        }
        const glm::vec3 start = rig.physics->body_pose(foot).position;
        constexpr float T = 1.0f;
        rig.run(T);
        const float s = rig.slid(foot, start);
        const platform::FootContact c = rig.physics->foot_contact(foot);
        const float a_path = 2.0f * s / (T * T);
        const float a_speed = c.slip_speed_mps / T;
        std::printf("[30° стекло] масса %.0f кг план=%d: путь %.4f м → a=%.3f; скорость %.3f → a=%.3f; "
                    "держит=%d\n",
                    static_cast<double>(mass), static_cast<int>(plant), static_cast<double>(s),
                    static_cast<double>(a_path), static_cast<double>(c.slip_speed_mps),
                    static_cast<double>(a_speed), static_cast<int>(c.holds));
        return std::pair{a_path, a_speed};
    };

    const float tol = static_cast<float>(config::FOOT_SLIP_ACCEL_TOL);
    const auto [a_path, a_speed] = measure(static_cast<float>(config::FOOT_BODY_MASS_KG), true);
    CHECK(a_path == doctest::Approx(law).epsilon(tol));
    CHECK(a_speed == doctest::Approx(law).epsilon(tol));

    // МАССА СОКРАЩАЕТСЯ: килограммовая стопа ползёт с тем же ускорением.
    const auto [a_light, a_light_speed] = measure(1.0f, true);
    CHECK(a_light == doctest::Approx(a_path).epsilon(0.05));
    (void)a_light_speed;

    // КОНТРОЛЬ 1: та же стопа, не поставленная (кинематическая), стоит —
    // ползёт физика постановки, а не ведение позы.
    const auto [a_swing, a_swing_speed] = measure(static_cast<float>(config::FOOT_BODY_MASS_KG), false);
    CHECK(std::abs(a_swing) < 1e-3f);
    CHECK(std::abs(a_swing_speed) < 1e-3f);

    // КОНТРОЛЬ 2: тот же угол по граниту держит — разделяет трение, не угол.
    {
        Rig rig;
        rig.slope(theta, "granite");
        const auto foot = rig.foot_on();
        rig.physics->set_foot_mode(foot, platform::FootMode::Plant);
        rig.run(0.05f);
        const glm::vec3 start = rig.physics->body_pose(foot).position;
        rig.run(1.0f);
        const float s = rig.slid(foot, start);
        std::printf("[30° гранит, контроль] путь за 1 с %.3f мм\n", static_cast<double>(s * 1000.0f));
        CHECK(rig.physics->foot_contact(foot).holds);
        CHECK(std::abs(s) < static_cast<float>(config::FOOT_SLIDE_MAX_M));
    }
}

TEST_CASE("переход «держит → ползёт» лежит на arctan(μ) ± FOOT_SLIP_ANGLE_TOL_DEG") {
    const float mu = pair_friction("leather", "granite");
    const float predicted = deg(std::atan(mu));
    constexpr float STEP_DEG = 0.5f;
    constexpr float SLIDE_M = 0.005f; // 5 мм за секунду — это уже ход, не дрожь
    float first_slide = -1.0f;
    float last_hold = -1.0f;
    bool monotone = true;
    for (float angle = predicted - 6.0f; angle <= predicted + 6.0f; angle += STEP_DEG) {
        Rig rig;
        rig.slope(rad(angle), "granite");
        const auto foot = rig.foot_on();
        rig.physics->set_foot_mode(foot, platform::FootMode::Plant);
        rig.run(0.05f);
        const glm::vec3 start = rig.physics->body_pose(foot).position;
        rig.run(1.0f);
        const float s = rig.slid(foot, start);
        const bool slides = s > SLIDE_M;
        const bool criterion_holds = rig.physics->foot_contact(foot).holds;
        std::printf("[развёртка] %.1f°: путь %.2f мм → %s; критерий держит=%d\n",
                    static_cast<double>(angle), static_cast<double>(s * 1000.0f),
                    slides ? "ползёт" : "держит", static_cast<int>(criterion_holds));
        if (slides && first_slide < 0.0f) {
            first_slide = angle;
        }
        if (!slides) {
            if (first_slide >= 0.0f) {
                monotone = false; // держит выше угла, где уже ползло
            }
            last_hold = angle;
        }
    }
    std::printf("[развёртка] предсказано %.1f°, последний держит %.1f°, первый ползёт %.1f°\n",
                static_cast<double>(predicted), static_cast<double>(last_hold),
                static_cast<double>(first_slide));
    const float tol = static_cast<float>(config::FOOT_SLIP_ANGLE_TOL_DEG);
    REQUIRE(first_slide >= 0.0f);
    CHECK(monotone);
    CHECK(std::abs(first_slide - predicted) <= tol);
    CHECK(std::abs(last_hold - predicted) <= tol);
}

TEST_CASE("контакт с утварью: стопа на ящике знает ящик — вещество, нормаль, хозяина") {
    Rig rig;
    rig.slope(0.0f, "oak");
    // Ящик — сосновый, 60 см, 10 кг, стоит на полу.
    const auto points = box_points({0.3f, 0.3f, 0.3f});
    platform::DynamicBodyDesc crate;
    crate.points = points;
    crate.position = {0.0f, 0.3f + SEAT_GAP_M, 0.0f};
    crate.mass_kg = 10.0f;
    crate.layer = physics::LAYER_LOOSE;
    crate.substance = core::find_substance("pine");
    crate.user_data = 42;
    const auto crate_body = rig.physics->create_dynamic_body(crate);
    REQUIRE(crate_body.valid());
    rig.physics->activate_body(crate_body);
    rig.run(0.5f);

    const auto foot = rig.foot_on("leather", static_cast<float>(config::FOOT_BODY_MASS_KG),
                                  glm::vec3{0.0f, 0.6f + SEAT_GAP_M, 0.0f});
    rig.physics->set_foot_mode(foot, platform::FootMode::Plant);
    rig.run(0.5f);
    const platform::FootContact c = rig.physics->foot_contact(foot);
    std::printf("[ящик] касание=%d вещество=%s хозяин=%llu n=(%.3f %.3f %.3f) держит=%d "
                "стопа y=%.3f ящик y=%.3f\n",
                static_cast<int>(c.touching), core::substance(c.ground).name.data(),
                static_cast<unsigned long long>(c.ground_user_data),
                static_cast<double>(c.normal.x), static_cast<double>(c.normal.y),
                static_cast<double>(c.normal.z), static_cast<int>(c.holds),
                static_cast<double>(rig.physics->body_pose(foot).position.y),
                static_cast<double>(rig.physics->body_pose(crate_body).position.y));
    CHECK(c.touching);
    CHECK(c.ground == core::find_substance("pine"));
    CHECK(c.ground_user_data == 42);
    CHECK(c.normal.y > 0.99f);
    CHECK(c.holds);
    CHECK(c.friction_pair == doctest::Approx(pair_friction("leather", "pine")));
    // Стоит НА ящике, а не в нём и не сквозь него.
    CHECK(rig.physics->body_pose(foot).position.y == doctest::Approx(0.6f + FOOT_HALF.y).epsilon(0.03));
    // Ящик остался на полу под весом стопы.
    CHECK(rig.physics->body_pose(crate_body).position.y == doctest::Approx(0.3f).epsilon(0.03));

    // КОНТРОЛЬ: стопа рядом с ящиком стоит на дубовом полу, а не на сосне.
    const auto beside = rig.foot_on("leather", static_cast<float>(config::FOOT_BODY_MASS_KG),
                                    glm::vec3{1.0f, 0.0f, 0.0f});
    rig.physics->set_foot_mode(beside, platform::FootMode::Plant);
    rig.run(0.2f);
    const platform::FootContact floor = rig.physics->foot_contact(beside);
    CHECK(floor.touching);
    CHECK(floor.ground == core::find_substance("oak"));
    CHECK(floor.ground_user_data == 7);
}

TEST_CASE("стопа не трогает хитбоксы и капсулу: сквозь коробку хитбокса падает на пол, "
          "капсулу не двигает") {
    // Коробка под стопой: на LAYER_HITBOX стопа сквозь неё проходит,
    // на LAYER_STATIC (контроль) — стоит на ней.
    const auto rest_height = [](platform::CollisionMask layer) {
        Rig rig;
        rig.slope(0.0f, "oak");
        platform::StaticBoxDesc box;
        box.center = {0.0f, 0.5f, 0.0f};
        box.half_extents = {0.3f, 0.2f, 0.3f};
        box.layer = layer;
        box.substance = core::find_substance("cloth");
        box.user_data = 99;
        REQUIRE(rig.physics->create_static_box(box).valid());
        const auto foot = rig.foot_on("leather", static_cast<float>(config::FOOT_BODY_MASS_KG),
                                      glm::vec3{0.0f, 0.9f, 0.0f});
        rig.physics->set_foot_mode(foot, platform::FootMode::Plant);
        rig.run(1.5f);
        const platform::FootContact c = rig.physics->foot_contact(foot);
        std::printf("[хитбокс] слой %u: стопа легла на y=%.3f, стоит на «%s» (хозяин %llu)\n",
                    static_cast<unsigned>(layer),
                    static_cast<double>(rig.physics->body_pose(foot).position.y),
                    core::substance(c.ground).name.data(),
                    static_cast<unsigned long long>(c.ground_user_data));
        return std::pair{rig.physics->body_pose(foot).position.y, c.ground_user_data};
    };
    const auto [through, floor_owner] = rest_height(physics::LAYER_HITBOX);
    CHECK(through == doctest::Approx(FOOT_HALF.y).epsilon(0.05));
    CHECK(floor_owner == 7);
    const auto [on_top, box_owner] = rest_height(physics::LAYER_STATIC);
    CHECK(on_top == doctest::Approx(0.7f + FOOT_HALF.y).epsilon(0.02));
    CHECK(box_owner == 99);

    // Капсула: стопа, махнувшая сквозь неё, не сдвигает её и не задерживается.
    Rig rig;
    rig.slope(0.0f, "oak");
    platform::CharacterDesc who;
    who.position = {0.0f, 0.0f, 0.0f};
    who.radius = 0.3f;
    who.height = 1.8f;
    who.max_slope_radians = rad(50.0f);
    who.step_height = 0.35f;
    who.layer = physics::LAYER_CHARACTER;
    who.collides_with = physics::LAYER_STATIC | physics::LAYER_LOOSE;
    const auto capsule = rig.physics->create_character(who);
    REQUIRE(capsule.valid());
    const auto foot = rig.foot_on("leather", static_cast<float>(config::FOOT_BODY_MASS_KG),
                                  glm::vec3{-1.0f, 0.5f, 0.0f});
    for (int i = 1; i <= 60; ++i) {
        const float x = -1.0f + 2.0f * static_cast<float>(i) / 60.0f; // −1 → +1 через капсулу
        rig.physics->set_foot_kinematic_pose(
            foot, platform::BodyPose{glm::vec3{x, 0.5f + FOOT_HALF.y, 0.0f}, glm::quat{1, 0, 0, 0}});
        rig.physics->step(DT);
    }
    const glm::vec3 where = rig.physics->character_position(capsule);
    std::printf("[капсула] после маха стопы сквозь неё: капсула (%.4f %.4f %.4f), стопа x=%.3f\n",
                static_cast<double>(where.x), static_cast<double>(where.y),
                static_cast<double>(where.z), static_cast<double>(rig.physics->body_pose(foot).position.x));
    CHECK(glm::length(glm::vec3{where.x, 0.0f, where.z}) < 1e-3f);
    CHECK(rig.physics->body_pose(foot).position.x == doctest::Approx(1.0f).epsilon(0.01));
    // И контакта с капсулой стопа не сообщает: под ней в конце — пол.
    rig.physics->set_foot_kinematic_pose(
        foot, platform::BodyPose{glm::vec3{0.0f, FOOT_HALF.y + SEAT_GAP_M, 0.0f}, glm::quat{1, 0, 0, 0}});
    rig.physics->step(DT);
    CHECK(rig.physics->foot_contact(foot).ground_user_data == 7);
}

TEST_CASE("цена двух стоп за тик — разностью против руки без стоп (мера, не ворота)") {
    // Рельеф — карта высот 65×65, чтобы стопы стояли на сетке треугольников,
    // как в игре, а не на одной коробке.
    constexpr uint32_t N = 65;
    std::vector<float> heights(N * N);
    for (uint32_t z = 0; z < N; ++z) {
        for (uint32_t x = 0; x < N; ++x) {
            heights[z * N + x] = 0.05f * std::sin(0.3f * static_cast<float>(x))
                                 * std::cos(0.3f * static_cast<float>(z));
        }
    }
    const auto make_world = [&](Rig& rig) {
        platform::TerrainDesc terrain;
        terrain.origin = {-32.0f, 0.0f, -32.0f};
        terrain.sample_count_x = N;
        terrain.sample_count_z = N;
        terrain.sample_spacing = 1.0f;
        terrain.heights = heights;
        terrain.layer = physics::LAYER_STATIC;
        terrain.substance = core::find_substance("granite");
        REQUIRE(rig.physics->create_terrain(terrain).valid());
        // Пара предметов рядом, чтобы широкая фаза не была пустой.
        for (int i = 0; i < 4; ++i) {
            const auto points = box_points({0.1f, 0.1f, 0.1f});
            platform::DynamicBodyDesc d;
            d.points = points;
            d.position = {2.0f + static_cast<float>(i) * 0.5f, 0.5f, 2.0f};
            d.mass_kg = 2.0f;
            d.layer = physics::LAYER_LOOSE;
            d.substance = core::find_substance("clay");
            REQUIRE(rig.physics->create_dynamic_body(d).valid());
        }
    };
    const auto time_ticks = [&](bool with_feet) {
        Rig rig;
        make_world(rig);
        // ОБЕ РУКИ ЖИВЫЕ: в игре капсула ходит и решатель не спит, поэтому
        // контрольная рука тоже ведёт капсулу — иначе разность мерила бы
        // «пробуждение мира», а не стопы (правило 47: разность против руки,
        // в которой всё, кроме предмета, то же самое).
        platform::CharacterDesc who;
        who.position = {-3.0f, 0.5f, -3.0f};
        who.radius = 0.3f;
        who.height = 1.8f;
        who.max_slope_radians = rad(50.0f);
        who.step_height = 0.35f;
        who.layer = physics::LAYER_CHARACTER;
        who.collides_with = physics::LAYER_STATIC | physics::LAYER_LOOSE;
        const auto walker = rig.physics->create_character(who);
        REQUIRE(walker.valid());
        std::vector<platform::PhysicsBodyHandle> feet;
        if (with_feet) {
            for (float x : {-0.15f, 0.15f}) {
                const auto foot = rig.foot_on("leather", static_cast<float>(config::FOOT_BODY_MASS_KG),
                                              glm::vec3{x, 0.2f, 0.0f});
                rig.physics->set_foot_mode(foot, platform::FootMode::Plant);
                feet.push_back(foot);
            }
        }
        rig.run(0.5f); // разогрев: посадка стоп, засыпание предметов
        constexpr int TICKS = 600;
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < TICKS; ++i) {
            // Капсула ходит по кругу в обеих руках.
            const float phase = static_cast<float>(i) * 0.02f;
            rig.physics->move_character(walker, glm::vec3{std::cos(phase), -0.1f, std::sin(phase)} * (1.8f * DT));
            // Как в игре: мах одной стопы кинематикой, вторая стоит; обе спрашиваются.
            if (with_feet) {
                if (i % 60 == 0) {
                    rig.physics->set_foot_mode(feet[0], platform::FootMode::Swing);
                } else if (i % 60 == 30) {
                    rig.physics->set_foot_mode(feet[0], platform::FootMode::Plant);
                }
                if (rig.physics->foot_mode(feet[0]) == platform::FootMode::Swing) {
                    const auto p = rig.physics->body_pose(feet[0]);
                    rig.physics->set_foot_kinematic_pose(
                        feet[0], platform::BodyPose{p.position + glm::vec3{0.0f, 0.001f, 0.0f}, p.rotation});
                }
            }
            rig.physics->step(DT);
            if (with_feet) {
                for (const auto foot : feet) {
                    (void)rig.physics->foot_contact(foot);
                }
            }
        }
        const auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count() / TICKS;
    };
    // Три повтора, минимум каждой руки: шум машины не в нашу пользу.
    double without = 1e9, with = 1e9;
    for (int r = 0; r < 3; ++r) {
        without = std::min(without, time_ticks(false));
        with = std::min(with, time_ticks(true));
    }
    const double cost = with - without;
    // Отдельно — цена самих запросов контакта (две стопы), без шага.
    double query_ms = 0.0;
    {
        Rig rig;
        make_world(rig);
        const auto a = rig.foot_on("leather", static_cast<float>(config::FOOT_BODY_MASS_KG), {-0.15f, 0.2f, 0.0f});
        const auto b = rig.foot_on("leather", static_cast<float>(config::FOOT_BODY_MASS_KG), {0.15f, 0.2f, 0.0f});
        rig.physics->set_foot_mode(a, platform::FootMode::Plant);
        rig.physics->set_foot_mode(b, platform::FootMode::Plant);
        rig.run(0.5f);
        constexpr int QUERIES = 2000;
        const auto q0 = std::chrono::steady_clock::now();
        for (int i = 0; i < QUERIES; ++i) {
            (void)rig.physics->foot_contact(a);
            (void)rig.physics->foot_contact(b);
        }
        query_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - q0).count() / QUERIES;
    }
    std::printf("[цена] тик без стоп %.4f мс, со стопами %.4f мс, разность %.4f мс (бюджет %.3f); "
                "два запроса контакта %.4f мс\n",
                without, with, cost, static_cast<double>(config::FOOT_BODY_TICK_BUDGET_MS), query_ms);
    WARN_MESSAGE(cost <= static_cast<double>(config::FOOT_BODY_TICK_BUDGET_MS),
                 "две стопы дороже бюджета FOOT_BODY_TICK_BUDGET_MS: " << cost << " мс");
    CHECK(cost < 1.0); // порядок величины: стопы не могут стоить миллисекунду
}

TEST_CASE("отказы контракта: нулевой слой, нулевой collides_with, пустая стопа — на обоих бэкендах") {
    for (const bool jolt : {true, false}) {
        Rig rig(jolt);
        platform::FootBodyDesc good;
        good.half_extents = FOOT_HALF;
        good.mass_kg = 1.0f;
        good.layer = physics::LAYER_FOOT;
        good.collides_with = physics::LAYER_STATIC;
        CHECK(rig.physics->create_foot_body(good).valid());
        platform::FootBodyDesc no_layer = good;
        no_layer.layer = 0;
        CHECK_FALSE(rig.physics->create_foot_body(no_layer).valid());
        platform::FootBodyDesc touches_nothing = good;
        touches_nothing.collides_with = 0;
        CHECK_FALSE(rig.physics->create_foot_body(touches_nothing).valid());
        platform::FootBodyDesc flat = good;
        flat.half_extents.y = 0.0f;
        CHECK_FALSE(rig.physics->create_foot_body(flat).valid());
        platform::FootBodyDesc weightless = good;
        weightless.mass_kg = 0.0f;
        CHECK_FALSE(rig.physics->create_foot_body(weightless).valid());
        // Чужой хэндл: не стопа — режим «мах», контакта нет.
        CHECK(rig.physics->foot_mode(platform::PhysicsBodyHandle{12345}) == platform::FootMode::Swing);
        CHECK_FALSE(rig.physics->foot_contact(platform::PhysicsBodyHandle{12345}).touching);
    }
}

TEST_CASE("null-бэкенд: стопа стоит на плоскости капсулы, мах приходит за шаг, планта держит") {
    Rig rig(false);
    const auto foot = rig.foot_on();
    const platform::FootContact c = rig.physics->foot_contact(foot);
    CHECK(c.touching);
    CHECK(c.normal.y == doctest::Approx(1.0f));
    CHECK(c.holds);
    CHECK(c.slip_speed_mps == 0.0f);
    CHECK(c.friction_pair == doctest::Approx(pair_friction("leather", "default")));
    // Мах: поза применяется на следующем шаге, не сразу.
    const platform::BodyPose target{glm::vec3{1.0f, 0.2f, 0.5f}, glm::quat{1, 0, 0, 0}};
    rig.physics->set_foot_kinematic_pose(foot, target);
    CHECK(rig.physics->body_pose(foot).position.x == doctest::Approx(0.0f));
    rig.physics->step(DT);
    CHECK(rig.physics->body_pose(foot).position == target.position);
    // Планта: физика владеет стопой, поза маха игнорируется, стопа стоит.
    rig.physics->set_foot_mode(foot, platform::FootMode::Plant);
    rig.physics->set_foot_kinematic_pose(foot, platform::BodyPose{glm::vec3{5.0f}, glm::quat{1, 0, 0, 0}});
    rig.run(0.5f);
    CHECK(rig.physics->body_pose(foot).position == target.position);
    // Обратно в мах: держит место, пока не сказано иное.
    rig.physics->set_foot_mode(foot, platform::FootMode::Swing);
    rig.run(0.1f);
    CHECK(rig.physics->body_pose(foot).position == target.position);
    rig.physics->destroy_body(foot);
    CHECK_FALSE(rig.physics->foot_contact(foot).touching);
}
