/*
Module: tests/sim
File: tests/sim/CharacterImpulseTests.cpp

Responsibility:
- ПРИБОРЫ ТОЛЧКОВ КАПСУЛЫ И РЕГДОЛЛА (docs/design/HIT_REACTIONS_PHYSICS.md
  §3, контракт платформы): импульс даёт Δv = J/m ± 1 % и гаснет на земле за
  CHARACTER_PUSH_DECAY_S; масса — единственный иммунитет; лёгкое тело
  капсулу не толкает, а само толкается, тяжёлое — толкает капсулу; контакты
  капсулы читаются наружу; регдолл из позы падает, ложится и засыпает;
  моторы держат позу против малого толчка там, где пассивный (контроль)
  складывается.

Key items:
- Rig: пол из дуба, капсула 80 кг по умолчанию (CharacterDesc.mass_kg = 0).
- chain(): регдолл из трёх частей (таз, бедро, голень) с суставами.

Dependencies:
- Uses: platform Jolt + null backends, engine/physics (слои), core
  PhysicsSubstance, generated constants, doctest.
- Used by: ctest (sim_character_impulse).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Волна «контракт физических стоп».
- ФИЗИКА ШАГАЕТ РОВНО РАЗ ЗА ТИК С SIM_DT (правило 12).
*/

#include <doctest/doctest.h>

#include "engine/core/config/sources/Constants.h"
#include "engine/core/materials/sources/PhysicsSubstance.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"
#include "engine/platform/physics/sources/null/CreateNullPhysics.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace dfn;

namespace {

constexpr float DT = static_cast<float>(config::SIM_DT);
constexpr float PI = 3.14159265358979f;

std::vector<glm::vec3> box_points(glm::vec3 half) {
    return {{-half.x, -half.y, -half.z}, {half.x, -half.y, -half.z},
            {half.x, -half.y, half.z},   {-half.x, -half.y, half.z},
            {-half.x, half.y, -half.z},  {half.x, half.y, -half.z},
            {half.x, half.y, half.z},    {-half.x, half.y, half.z}};
}

struct Rig {
    std::unique_ptr<platform::IPhysics> physics;

    explicit Rig(bool jolt = true) {
        physics = jolt ? platform::create_jolt_physics() : platform::create_null_physics();
        REQUIRE(physics->init());
        platform::StaticBoxDesc floor;
        floor.center = {0.0f, -0.5f, 0.0f};
        floor.half_extents = {30.0f, 0.5f, 30.0f};
        floor.layer = physics::LAYER_STATIC;
        floor.substance = core::find_substance("oak");
        floor.user_data = 7;
        REQUIRE(physics->create_static_box(floor).valid());
    }
    ~Rig() { physics->shutdown(); }

    platform::CharacterHandle character(glm::vec3 at, float mass = 0.0f) {
        platform::CharacterDesc desc;
        desc.position = at;
        desc.radius = 0.3f;
        desc.height = 1.8f;
        desc.max_slope_radians = 50.0f * PI / 180.0f;
        desc.step_height = 0.35f;
        desc.layer = physics::LAYER_CHARACTER;
        desc.collides_with = physics::LAYER_STATIC | physics::LAYER_LOOSE | physics::LAYER_RAGDOLL;
        desc.mass_kg = mass;
        desc.user_data = 1;
        const auto h = physics->create_character(desc);
        REQUIRE(h.valid());
        return h;
    }

    platform::PhysicsBodyHandle prop(glm::vec3 half, glm::vec3 at, float mass, uint64_t tag,
                                     const char* substance = "pine") {
        const auto points = box_points(half);
        platform::DynamicBodyDesc d;
        d.points = points;
        d.position = at;
        d.mass_kg = mass;
        d.layer = physics::LAYER_LOOSE;
        d.substance = core::find_substance(substance);
        d.user_data = tag;
        d.start_asleep = false;
        const auto h = physics->create_dynamic_body(d);
        REQUIRE(h.valid());
        return h;
    }

    void run(float seconds) {
        const int steps = static_cast<int>(std::lround(seconds / DT));
        for (int i = 0; i < steps; ++i) {
            physics->step(DT);
        }
    }
};

/// ЦЕПОЧКА ИЗ ТРЁХ ЧАСТЕЙ — таз, бедро, голень, стоя над полом: суставы у
/// бедра и колена, конус 60°, скрутка ±20°. Возвращает описание и позы.
struct Chain {
    std::array<platform::RagdollPartDesc, 3> parts{};
    std::array<platform::BodyPose, 3> poses{};

    explicit Chain(float pelvis_y = 1.2f) {
        // Таз: коробка 0.30×0.20×0.20, 20 кг.
        parts[0].parent = -1;
        parts[0].half_extents = {0.15f, 0.10f, 0.10f};
        parts[0].pose.position = {0.0f, pelvis_y, 0.0f};
        parts[0].mass_kg = 20.0f;
        parts[0].substance = core::find_substance("meat");
        parts[0].user_data = 100;
        // Бедро: 0.14×0.40×0.14, 8 кг, сустав на нижней грани таза.
        const float hip_y = pelvis_y - 0.10f;
        parts[1].parent = 0;
        parts[1].half_extents = {0.07f, 0.20f, 0.07f};
        parts[1].pose.position = {0.0f, hip_y - 0.20f, 0.0f};
        parts[1].mass_kg = 8.0f;
        parts[1].substance = parts[0].substance;
        parts[1].user_data = 101;
        parts[1].joint_position = {0.0f, hip_y, 0.0f};
        parts[1].twist_axis = {0.0f, -1.0f, 0.0f};
        parts[1].plane_axis = {1.0f, 0.0f, 0.0f};
        parts[1].swing_limit_rad = 60.0f * PI / 180.0f;
        parts[1].twist_min_rad = -20.0f * PI / 180.0f;
        parts[1].twist_max_rad = 20.0f * PI / 180.0f;
        // Голень: 0.10×0.40×0.10, 4 кг, колено под бедром.
        const float knee_y = hip_y - 0.40f;
        parts[2] = parts[1];
        parts[2].parent = 1;
        parts[2].half_extents = {0.05f, 0.20f, 0.05f};
        parts[2].pose.position = {0.0f, knee_y - 0.20f, 0.0f};
        parts[2].mass_kg = 4.0f;
        parts[2].user_data = 102;
        parts[2].joint_position = {0.0f, knee_y, 0.0f};
        for (size_t i = 0; i < 3; ++i) {
            poses[i] = parts[i].pose;
        }
    }

    [[nodiscard]] platform::RagdollDesc desc() const {
        platform::RagdollDesc d;
        d.parts = parts;
        d.layer = physics::LAYER_RAGDOLL;
        d.collides_with = physics::LAYER_STATIC | physics::LAYER_LOOSE | physics::LAYER_RAGDOLL;
        d.user_data = 100;
        return d;
    }
};

/// Угол между осями Y двух частей — насколько сустав ушёл от прямой.
[[nodiscard]] float bend_deg(const platform::BodyPose& a, const platform::BodyPose& b) {
    const glm::vec3 ya = a.rotation * glm::vec3{0.0f, 1.0f, 0.0f};
    const glm::vec3 yb = b.rotation * glm::vec3{0.0f, 1.0f, 0.0f};
    return std::acos(std::clamp(glm::dot(ya, yb), -1.0f, 1.0f)) * 180.0f / PI;
}

} // namespace

TEST_CASE("импульс даёт Δv = J/m ± 1 % и гаснет на земле за CHARACTER_PUSH_DECAY_S") {
    for (const bool jolt : {true, false}) {
        Rig rig(jolt);
        const auto who = rig.character({0.0f, 0.0f, 0.0f});
        CHECK(rig.physics->character_mass(who) == doctest::Approx(static_cast<float>(config::CHARACTER_MASS_KG)));
        rig.run(0.2f); // посадка
        const glm::vec3 start = rig.physics->character_position(who);
        // КОНТРОЛЬ: без импульса капсула стоит.
        rig.run(0.2f);
        CHECK(glm::length(rig.physics->character_position(who) - start) < 1e-3f);

        const glm::vec3 J{static_cast<float>(config::CHARACTER_MASS_KG) * 1.5f, 0.0f, 0.0f};
        rig.physics->character_add_impulse(who, J);
        rig.physics->step(DT);
        const glm::vec3 v = rig.physics->character_velocity(who);
        const glm::vec3 moved = rig.physics->character_position(who) - start;
        std::printf("[импульс %s] J=%.0f Н·с, m=%.0f кг: v=(%.4f %.4f %.4f) м/с, путь за тик %.4f м\n",
                    jolt ? "jolt" : "null", static_cast<double>(J.x),
                    static_cast<double>(rig.physics->character_mass(who)), static_cast<double>(v.x),
                    static_cast<double>(v.y), static_cast<double>(v.z), static_cast<double>(moved.x));
        CHECK(v.x == doctest::Approx(1.5f).epsilon(0.01));
        CHECK(moved.x == doctest::Approx(1.5f * DT).epsilon(0.01));
        // Спад: через одну постоянную времени — e⁻¹ от начального (±5 %).
        rig.run(static_cast<float>(config::CHARACTER_PUSH_DECAY_S));
        const float after = rig.physics->character_velocity(who).x;
        std::printf("[импульс %s] через %.2f с: v=%.4f (ожидание e⁻¹ = %.4f)\n",
                    jolt ? "jolt" : "null", static_cast<double>(config::CHARACTER_PUSH_DECAY_S),
                    static_cast<double>(after), static_cast<double>(1.5f * std::exp(-1.0f)));
        CHECK(after == doctest::Approx(1.5f * std::exp(-1.0f)).epsilon(0.06));
    }
}

TEST_CASE("иммунитет — масса и ничего больше: тот же J на 800 кг даёт десятую долю Δv") {
    Rig rig;
    const auto light = rig.character({0.0f, 0.0f, 0.0f});
    const auto heavy = rig.character({3.0f, 0.0f, 0.0f}, 800.0f);
    rig.run(0.2f);
    const glm::vec3 J{80.0f, 0.0f, 0.0f};
    rig.physics->character_add_impulse(light, J);
    rig.physics->character_add_impulse(heavy, J);
    rig.physics->step(DT);
    const float v_light = rig.physics->character_velocity(light).x;
    const float v_heavy = rig.physics->character_velocity(heavy).x;
    std::printf("[иммунитет] 80 кг: %.4f м/с; 800 кг: %.4f м/с\n", static_cast<double>(v_light),
                static_cast<double>(v_heavy));
    CHECK(v_light == doctest::Approx(1.0f).epsilon(0.01));
    CHECK(v_heavy == doctest::Approx(0.1f).epsilon(0.01));
}

TEST_CASE("лёгкое тело капсулу не толкает (и само отлетает), тяжёлое — толкает") {
    // Ящик летит в стоящую капсулу с 3 м/с с полуметра.
    const auto shove = [](float mass, uint64_t tag) {
        Rig rig;
        const auto who = rig.character({0.0f, 0.0f, 0.0f});
        rig.run(0.2f);
        const glm::vec3 start = rig.physics->character_position(who);
        const auto crate = rig.prop({0.2f, 0.2f, 0.2f}, {-0.9f, 0.2f, 0.0f}, mass, tag);
        rig.run(0.1f);
        rig.physics->set_body_velocity(crate, {3.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        bool seen = false;
        platform::CharacterContact first{};
        for (int i = 0; i < 60; ++i) {
            rig.physics->step(DT);
            for (const platform::CharacterContact& c : rig.physics->character_contacts(who)) {
                if (c.user_data == tag && !seen) {
                    seen = true;
                    first = c;
                }
            }
        }
        const float moved = rig.physics->character_position(who).x - start.x;
        const float crate_x = rig.physics->body_pose(crate).position.x;
        std::printf("[толчок] ящик %.0f кг: контакт=%d масса=%.1f n=(%.2f %.2f %.2f) v_rel=%.2f "
                    "толкнул капсулу=%d толкается=%d; капсула ушла на %.3f м, ящик x=%.3f\n",
                    static_cast<double>(mass), static_cast<int>(seen),
                    static_cast<double>(first.mass_kg), static_cast<double>(first.normal.x),
                    static_cast<double>(first.normal.y), static_cast<double>(first.normal.z),
                    static_cast<double>(first.relative_velocity.x),
                    static_cast<int>(first.pushed_character), static_cast<int>(first.pushed_body),
                    static_cast<double>(moved), static_cast<double>(crate_x));
        REQUIRE(seen);
        CHECK(first.mass_kg == doctest::Approx(mass));
        CHECK(first.normal.x > 0.7f); // нормаль смотрит В капсулу: ящик слева
        CHECK(first.relative_velocity.x > 0.5f);
        return std::tuple{moved, first.pushed_character, first.pushed_body, crate_x};
    };
    const auto [light_moved, light_pushes, light_pushed, light_x] = shove(1.0f, 21);
    CHECK_FALSE(light_pushes);
    CHECK(light_pushed);
    CHECK(std::abs(light_moved) < 0.01f);
    CHECK(light_x < 0.0f); // остался с той стороны: капсула — стена для него

    const auto [heavy_moved, heavy_pushes, heavy_pushed, heavy_x] = shove(200.0f, 22);
    CHECK(heavy_pushes);
    CHECK_FALSE(heavy_pushed);
    CHECK(heavy_moved > 0.05f);
    (void)heavy_x;
}

TEST_CASE("регдолл из позы: поза читается назад бит в бит, падает, ложится и засыпает") {
    for (const bool jolt : {true, false}) {
        Rig rig(jolt);
        const Chain chain;
        const auto ragdoll = rig.physics->create_ragdoll(chain.desc());
        REQUIRE(ragdoll.valid());
        std::array<platform::BodyPose, 3> read{};
        rig.physics->ragdoll_pose(ragdoll, read);
        for (size_t i = 0; i < 3; ++i) {
            CHECK(glm::length(read[i].position - chain.poses[i].position) < 1e-5f);
        }
        if (!jolt) {
            rig.run(1.0f);
            rig.physics->ragdoll_pose(ragdoll, read);
            CHECK(glm::length(read[0].position - chain.poses[0].position) < 1e-5f);
            CHECK(rig.physics->ragdoll_asleep(ragdoll));
            continue;
        }
        // Пассивный (моторы выключены): падает.
        rig.physics->ragdoll_drive_to_pose(ragdoll, chain.poses, 0.0f);
        rig.run(0.3f);
        CHECK_FALSE(rig.physics->ragdoll_asleep(ragdoll)); // контроль: в полёте не спит
        float asleep_at = -1.0f;
        for (float t = 0.3f; t < 8.0f; t += 0.1f) {
            rig.run(0.1f);
            if (rig.physics->ragdoll_asleep(ragdoll)) {
                asleep_at = t;
                break;
            }
        }
        rig.physics->ragdoll_pose(ragdoll, read);
        std::printf("[регдолл] пассивный: таз y=%.3f бедро y=%.3f голень y=%.3f, уснул на %.1f с; "
                    "сгиб бедра %.1f°, колена %.1f°\n",
                    static_cast<double>(read[0].position.y), static_cast<double>(read[1].position.y),
                    static_cast<double>(read[2].position.y), static_cast<double>(asleep_at),
                    static_cast<double>(bend_deg(read[0], read[1])),
                    static_cast<double>(bend_deg(read[1], read[2])));
        CHECK(asleep_at > 0.0f);
        CHECK(read[0].position.y < 0.6f); // таз лёг, а не висит в воздухе
        for (const auto& pose : read) {
            CHECK(pose.position.y > 0.0f); // и никто не провалился под пол
        }
        // Поза назад: SetPose ставит цепочку обратно в стойку, скорости ноль.
        rig.physics->set_ragdoll_pose(ragdoll, chain.poses);
        rig.physics->ragdoll_pose(ragdoll, read);
        CHECK(glm::length(read[0].position - chain.poses[0].position) < 1e-5f);
        rig.physics->destroy_ragdoll(ragdoll);
        CHECK(rig.physics->ragdoll_asleep(ragdoll)); // нет тела — нечему двигаться
    }
}

TEST_CASE("моторы возвращают позу после падения и держат её против малого толчка; "
          "пассивная цепочка (контроль) остаётся сложенной") {
    // Цепочка падает с 1.2 м на пол. В УДАР моторов не хватает ни у кого
    // (20 кг таза на 2 м/с — это больше RAGDOLL_MOTOR_TORQUE_NM за тик,
    // и это правильно: живое тело от удара тоже складывается), поэтому мерка
    // — ЛЁЖА: вернулись ли суставы к позе и держат ли её против 5 Н·с.
    const auto drop = [](float strength, const char* label) {
        Rig rig;
        const Chain chain;
        const auto ragdoll = rig.physics->create_ragdoll(chain.desc());
        REQUIRE(ragdoll.valid());
        rig.physics->ragdoll_drive_to_pose(ragdoll, chain.poses, strength);
        float worst_fall_hip = 0.0f;
        float worst_fall_knee = 0.0f;
        std::array<platform::BodyPose, 3> read{};
        for (int i = 0; i < 120; ++i) {
            rig.physics->step(DT);
            rig.physics->ragdoll_pose(ragdoll, read);
            worst_fall_hip = std::max(worst_fall_hip, bend_deg(read[0], read[1]));
            worst_fall_knee = std::max(worst_fall_knee, bend_deg(read[1], read[2]));
        }
        const float settled_hip = bend_deg(read[0], read[1]);
        const float settled_knee = bend_deg(read[1], read[2]);
        // Малый толчок в голень: 5 Н·с вбок (Δv голени 1.25 м/с).
        rig.physics->ragdoll_add_impulse(ragdoll, 2, {5.0f, 0.0f, 0.0f}, read[2].position);
        float worst_hip = 0.0f;
        float worst_knee = 0.0f;
        for (int i = 0; i < 60; ++i) {
            rig.physics->step(DT);
            rig.physics->ragdoll_pose(ragdoll, read);
            worst_hip = std::max(worst_hip, bend_deg(read[0], read[1]));
            worst_knee = std::max(worst_knee, bend_deg(read[1], read[2]));
        }
        std::printf("[моторы %s] в падении худший сгиб бедро %.1f° колено %.1f°; лёжа %.1f° / %.1f°; "
                    "после толчка 5 Н·с худший %.1f° / %.1f°, в конце %.1f° / %.1f°; таз y=%.3f\n",
                    label, static_cast<double>(worst_fall_hip), static_cast<double>(worst_fall_knee),
                    static_cast<double>(settled_hip), static_cast<double>(settled_knee),
                    static_cast<double>(worst_hip), static_cast<double>(worst_knee),
                    static_cast<double>(bend_deg(read[0], read[1])),
                    static_cast<double>(bend_deg(read[1], read[2])),
                    static_cast<double>(read[0].position.y));
        return std::array{settled_hip, settled_knee, worst_hip, worst_knee,
                          bend_deg(read[0], read[1]), bend_deg(read[1], read[2])};
    };
    const auto on = drop(1.0f, "сила 1");
    const auto off = drop(0.0f, "выкл, контроль");
    // Моторы: лёжа поза восстановлена, толчок отклоняет меньше 15°, потом снова поза.
    CHECK(std::max(on[0], on[1]) < 10.0f);
    CHECK(std::max(on[2], on[3]) < 15.0f);
    CHECK(std::max(on[4], on[5]) < 10.0f);
    // Контроль: без моторов цепочка так и лежит сложенной.
    CHECK(std::max(off[0], off[1]) > 30.0f);
    CHECK(std::max(off[4], off[5]) > 30.0f);
}

TEST_CASE("отказы регдолла: нулевой слой, ребёнок раньше родителя, часть без массы — на обоих бэкендах") {
    for (const bool jolt : {true, false}) {
        Rig rig(jolt);
        Chain chain;
        CHECK(rig.physics->create_ragdoll(chain.desc()).valid());
        platform::RagdollDesc no_layer = chain.desc();
        no_layer.layer = 0;
        CHECK_FALSE(rig.physics->create_ragdoll(no_layer).valid());
        Chain orphan;
        orphan.parts[1].parent = 2; // родитель после ребёнка
        CHECK_FALSE(rig.physics->create_ragdoll(orphan.desc()).valid());
        Chain weightless;
        weightless.parts[2].mass_kg = 0.0f;
        CHECK_FALSE(rig.physics->create_ragdoll(weightless.desc()).valid());
        platform::RagdollDesc empty;
        empty.layer = physics::LAYER_RAGDOLL;
        CHECK_FALSE(rig.physics->create_ragdoll(empty).valid());
    }
}
