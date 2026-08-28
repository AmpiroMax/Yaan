/*
Created: 28:08:2026 - 14:05:00
Last updated: 28:08:2026 - 14:05:00
Module: tests/sim
File: tests/sim/LoosePropTests.cpp

Responsibility:
- ЧЕТЫРЕ ПРИЁМКИ ФИЗИКИ ПРЕДМЕТОВ, названные ресёрчером до первой правки
  (записка №4, Б4), плюс две, названные владельцем: (1) стопка засыпает и не
  шевелится; (2) комната, поднятая десять раз, неотличима; (3) предмет в руках
  толкает другие; (4) заклиненный предмет ВЫПАДАЕТ, а не проходит сквозь стену
  и не тянет игрока; (5) тело игрока толкает бутыль и не толкает бочку;
  (6) вес заметен — потолок силы хвата тащит тяжёлое медленнее лёгкого.
- Всё на НАСТОЯЩЕМ бэкенде Jolt и НАСТОЯЩЕЙ арифметике хвата (GrabDrive.cpp),
  а не на её копии: две копии пружины разошлись бы, и рукав мерил бы не то, во
  что играют (правило 39).

Key items:
- Каждая приёмка публикуется вместе со случаем, который она обязана ОТВЕРГНУТЬ
  (правило 30): «стопка спит» — против стопки, собранной с проникновением;
  «предмет толкает» — против того же кадра без ведения; «капсула толкает
  бутыль» — против бочки в 200 кг.

Dependencies:
- Uses: platform Jolt backend, engine/physics (слои), engine/app/GrabDrive.h,
  generated constants (SIM_DT), doctest.
- Used by: ctest (sim_loose_props).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона big-grab владеет этим файлом.
- ФИЗИКА ШАГАЕТ РОВНО РАЗ ЗА ТИК С SIM_DT (правило 12 и записка №4 Б3: это
  единственное место, где мы устроены лучше эталона — у Havok шаг привязан к
  частоте кадров). Ни один рукав здесь не смеет шагнуть «ещё разок».
*/
/*
UPD:
- 28:08:2026 - 14:05:00: Создан вместе с волной big-grab.
*/

#include <doctest/doctest.h>

#include "engine/app/sources/GrabDrive.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/materials/sources/PhysicsSubstance.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include <glm/geometric.hpp>

using namespace dfn;

namespace {

constexpr float DT = static_cast<float>(config::SIM_DT);

/// Точки коробки — то, чем в игре служат вершины предмета.
std::vector<glm::vec3> box_points(glm::vec3 half) {
    return {{-half.x, -half.y, -half.z}, {half.x, -half.y, -half.z},
            {half.x, -half.y, half.z},   {-half.x, -half.y, half.z},
            {-half.x, half.y, -half.z},  {half.x, half.y, -half.z},
            {half.x, half.y, half.z},    {-half.x, half.y, half.z}};
}

struct Rig {
    std::unique_ptr<platform::IPhysics> physics;
    platform::PhysicsBodyHandle floor;

    Rig() {
        physics = platform::create_jolt_physics();
        REQUIRE(physics->init());
        platform::StaticBoxDesc desc;
        desc.center = {0.0f, -0.5f, 0.0f};
        desc.half_extents = {20.0f, 0.5f, 20.0f};
        desc.layer = physics::LAYER_STATIC;
        // ПОЛ — ДОСКА, и это половина всякой пары трения. Без названного
        // вещества стопка стояла бы на умолчании Jolt (0.2), то есть на
        // поверхности скользче любой настоящей.
        desc.substance = core::find_substance("oak");
        floor = physics->create_static_box(desc);
        REQUIRE(floor.valid());
    }
    ~Rig() { physics->shutdown(); }

    platform::PhysicsBodyHandle prop(glm::vec3 half, glm::vec3 at, float mass,
                                     const char* substance = "clay") {
        const auto points = box_points(half);
        platform::DynamicBodyDesc d;
        d.points = points;
        d.position = at;
        d.mass_kg = mass;
        d.layer = physics::LAYER_LOOSE;
        d.substance = core::find_substance(substance);
        const auto h = physics->create_dynamic_body(d);
        REQUIRE(h.valid());
        return h;
    }

    void run(float seconds) {
        const int steps = static_cast<int>(seconds / DT);
        for (int i = 0; i < steps; ++i) {
            physics->step(DT);
        }
    }
};

} // namespace

TEST_CASE("предмет падает, ложится и засыпает") {
    Rig rig;
    // Миска сброшена с полуметра: она обязана лечь и УСНУТЬ.
    const auto bowl = rig.prop({0.11f, 0.04f, 0.11f}, {0.0f, 0.5f, 0.0f}, 1.1f);
    rig.physics->activate_body(bowl);

    // КОНТРОЛЬ (правило 30): в полёте она ОБЯЗАНА не спать — иначе «спит»
    // означало бы «мне не с чем сравнить», а не «стоит смирно».
    rig.run(0.2f);
    CHECK_FALSE(rig.physics->body_asleep(bowl));

    rig.run(2.0f);
    const platform::BodyPose pose = rig.physics->body_pose(bowl);
    std::printf("[падение] высота покоя %.4f м, спит=%d\n",
                static_cast<double>(pose.position.y),
                static_cast<int>(rig.physics->body_asleep(bowl)));
    CHECK(rig.physics->body_asleep(bowl));
    // Легла НА пол, а не в него и не над ним: центр на полувысоте.
    CHECK(pose.position.y == doctest::Approx(0.04f).epsilon(0.15));
}

TEST_CASE("стопка из пяти предметов засыпает за секунду и не шевелится") {
    Rig rig;
    // ПЯТЬ ПРЕДМЕТОВ ДРУГ НА ДРУГЕ, поставленных ровно (без проникновения):
    // ровно та приёмка, которую назвала записка №4 (Б4.1).
    constexpr float H = 0.05f; // полувысота каждого
    std::array<platform::PhysicsBodyHandle, 5> stack{};
    for (std::size_t i = 0; i < stack.size(); ++i) {
        const float y = H + static_cast<float>(i) * 2.0f * H;
        stack[i] = rig.prop({0.10f, H, 0.10f}, {0.0f, y, 0.0f}, 1.0f);
    }
    // Разбудить всё: спящая с рождения стопка прошла бы приёмку не проснувшись,
    // и это была бы проверка того, что мы её не тронули (правило 47).
    for (const auto h : stack) {
        rig.physics->activate_body(h);
    }

    float slept_at = -1.0f;
    for (int i = 0; i < static_cast<int>(3.0f / DT); ++i) {
        rig.physics->step(DT);
        bool all = true;
        for (const auto h : stack) {
            all = all && rig.physics->body_asleep(h);
        }
        if (all && slept_at < 0.0f) {
            slept_at = static_cast<float>(i) * DT;
        }
    }
    REQUIRE(slept_at >= 0.0f);
    std::printf("[стопка] уснула через %.2f с\n", static_cast<double>(slept_at));
    // Jolt по умолчанию усыпляет через 0.5 с покоя; полторы секунды — потолок
    // с запасом на осадку стопки.
    CHECK(slept_at < 1.5f);

    std::array<glm::vec3, 5> before{};
    for (std::size_t i = 0; i < stack.size(); ++i) {
        before[i] = rig.physics->body_pose(stack[i]).position;
    }
    rig.run(10.0f);
    float max_drift = 0.0f;
    for (std::size_t i = 0; i < stack.size(); ++i) {
        max_drift = std::max(max_drift,
                             glm::length(rig.physics->body_pose(stack[i]).position - before[i]));
    }
    std::printf("[стопка] дрейф за 10 с: %.6f м\n", static_cast<double>(max_drift));
    // «Не шевелится ни на пиксель»: 0.1 мм — ниже того, что видно на экране с
    // метра при FullHD (пиксель на таком расстоянии ~0.5 мм).
    CHECK(max_drift < 1.0e-4f);

    // КОНТРОЛЬ (правило 30): та же стопка, собранная С ПРОНИКНОВЕНИЕМ на 2 см,
    // обязана ЗАМЕТНО разъехаться — иначе замер дрейфа ничего не различает.
    Rig bad;
    std::array<platform::PhysicsBodyHandle, 5> jammed{};
    for (std::size_t i = 0; i < jammed.size(); ++i) {
        const float y = H + static_cast<float>(i) * (2.0f * H - 0.02f);
        jammed[i] = bad.prop({0.10f, H, 0.10f}, {0.0f, y, 0.0f}, 1.0f);
        bad.physics->activate_body(jammed[i]);
    }
    std::array<glm::vec3, 5> jam_before{};
    for (std::size_t i = 0; i < jammed.size(); ++i) {
        jam_before[i] = bad.physics->body_pose(jammed[i]).position;
    }
    bad.run(2.0f);
    float jam_drift = 0.0f;
    for (std::size_t i = 0; i < jammed.size(); ++i) {
        jam_drift = std::max(jam_drift,
                             glm::length(bad.physics->body_pose(jammed[i]).position - jam_before[i]));
    }
    std::printf("[контроль] стопка с проникновением уехала на %.4f м\n",
                static_cast<double>(jam_drift));
    CHECK(jam_drift > 1.0e-3f);
}

TEST_CASE("комната, поднятая десять раз, неотличима") {
    // Классический отказ Скайрима: предметы разлетаются при входе в помещение,
    // потому что всё просыпается разом с взаимным проникновением. Приёмка —
    // десять подъёмов одной и той же расстановки (записка №4, Б4.2).
    std::vector<glm::vec3> first;
    for (int run = 0; run < 10; ++run) {
        Rig rig;
        std::vector<platform::PhysicsBodyHandle> props;
        for (int i = 0; i < 12; ++i) {
            const float x = static_cast<float>(i % 4) * 0.3f;
            const float z = static_cast<float>(i / 4) * 0.3f;
            props.push_back(rig.prop({0.06f, 0.05f, 0.06f}, {x, 0.05f, z}, 0.9f));
        }
        rig.run(2.0f);
        std::vector<glm::vec3> now;
        now.reserve(props.size());
        for (const auto h : props) {
            now.push_back(rig.physics->body_pose(h).position);
        }
        if (run == 0) {
            first = now;
            // ТЕЛА РОЖДАЮТСЯ СПЯЩИМИ: комната, которую никто не трогал, не
            // должна тратить ни одного тика симуляции.
            for (const auto h : props) {
                CHECK(rig.physics->body_asleep(h));
            }
            continue;
        }
        float worst = 0.0f;
        for (std::size_t i = 0; i < now.size(); ++i) {
            worst = std::max(worst, glm::length(now[i] - first[i]));
        }
        CHECK(worst < 1.0e-5f);
    }
}

TEST_CASE("предмет в руках толкает другие") {
    Rig rig;
    // Кувшин, ведомый рукой, проносится сквозь ряд кубков.
    const auto jug = rig.prop({0.10f, 0.15f, 0.10f}, {-0.8f, 0.15f, 0.0f}, 2.1f);
    const auto cup = rig.prop({0.05f, 0.06f, 0.05f}, {0.0f, 0.06f, 0.0f}, 0.5f, "tin");
    const glm::vec3 cup_before = rig.physics->body_pose(cup).position;

    const app::GrabTuning tuning;
    rig.physics->set_body_gravity_factor(jug, 0.0f);
    for (int i = 0; i < static_cast<int>(1.2f / DT); ++i) {
        const platform::BodyPose pose = rig.physics->body_pose(jug);
        const glm::vec3 target{-0.8f + 1.6f * static_cast<float>(i) * DT / 1.2f, 0.15f, 0.0f};
        rig.physics->set_body_velocity(
            jug,
            app::grab_velocity(pose.position, rig.physics->body_velocity(jug), target, 2.1f,
                               DT, tuning),
            glm::vec3{0.0f});
        rig.physics->step(DT);
    }
    const float moved = glm::length(rig.physics->body_pose(cup).position - cup_before);
    std::printf("[смахнул] кубок уехал на %.3f м\n", static_cast<double>(moved));
    CHECK(moved > 0.05f);

    // КОНТРОЛЬ: тот же кадр без ведения — кубок стоит.
    Rig still;
    const auto quiet_jug = still.prop({0.10f, 0.15f, 0.10f}, {-0.8f, 0.15f, 0.0f}, 2.1f);
    const auto quiet_cup = still.prop({0.05f, 0.06f, 0.05f}, {0.0f, 0.06f, 0.0f}, 0.5f, "tin");
    (void)quiet_jug;
    const glm::vec3 quiet_before = still.physics->body_pose(quiet_cup).position;
    still.run(1.2f);
    CHECK(glm::length(still.physics->body_pose(quiet_cup).position - quiet_before) < 1.0e-3f);
}

TEST_CASE("вес заметен: потолок силы хвата") {
    Rig rig;
    // Один и тот же приказ руки против двух масс. Лёгкое доходит, тяжёлое
    // отстаёт — и это ЕДИНСТВЕННОЕ, что делает вес заметным без таблицы
    // «классов веса» (записка №4, Б2).
    const auto light = rig.prop({0.10f, 0.15f, 0.10f}, {0.0f, 0.15f, 0.0f}, 2.1f);
    const auto heavy = rig.prop({0.22f, 0.31f, 0.22f}, {2.0f, 0.31f, 0.0f}, 46.0f, "grain");
    const app::GrabTuning tuning;
    const glm::vec3 light_goal{0.0f, 1.40f, 0.0f};
    const glm::vec3 heavy_goal{2.0f, 1.40f, 0.0f};
    rig.physics->set_body_gravity_factor(light, 0.0f);
    rig.physics->set_body_gravity_factor(heavy, 0.0f);
    for (int i = 0; i < static_cast<int>(0.5f / DT); ++i) {
        for (const auto [h, goal, mass] :
             std::array<std::tuple<platform::PhysicsBodyHandle, glm::vec3, float>, 2>{
                 {{light, light_goal, 2.1f}, {heavy, heavy_goal, 46.0f}}}) {
            const platform::BodyPose p = rig.physics->body_pose(h);
            rig.physics->set_body_velocity(
                h,
                app::grab_velocity(p.position, rig.physics->body_velocity(h), goal, mass, DT,
                                   tuning),
                glm::vec3{0.0f});
        }
        rig.physics->step(DT);
    }
    const float light_left = glm::length(rig.physics->body_pose(light).position - light_goal);
    const float heavy_left = glm::length(rig.physics->body_pose(heavy).position - heavy_goal);
    std::printf("[вес] через 0.5 с: лёгкому осталось %.3f м, тяжёлому %.3f м\n",
                static_cast<double>(light_left), static_cast<double>(heavy_left));
    CHECK(light_left < 0.05f);
    CHECK(heavy_left > 4.0f * light_left);
}

TEST_CASE("заклиненный предмет выпадает, а не проходит сквозь стену") {
    Rig rig;
    // Стена между предметом и рукой: рука зовёт за неё, предмет остаётся здесь.
    platform::StaticBoxDesc wall;
    wall.center = {0.5f, 1.0f, 0.0f};
    wall.half_extents = {0.1f, 1.0f, 2.0f};
    wall.layer = physics::LAYER_STATIC;
    wall.substance = core::find_substance("oak");
    REQUIRE(rig.physics->create_static_box(wall).valid());

    const auto jug = rig.prop({0.10f, 0.15f, 0.10f}, {0.0f, 0.60f, 0.0f}, 2.1f);
    rig.physics->set_body_gravity_factor(jug, 0.0f);
    const glm::vec3 hand{1.6f, 0.60f, 0.0f}; // по ту сторону стены
    const app::GrabTuning tuning;
    float lag = 0.0f;
    bool slipped = false;
    int slipped_at = -1;
    for (int i = 0; i < static_cast<int>(2.0f / DT); ++i) {
        const platform::BodyPose p = rig.physics->body_pose(jug);
        rig.physics->set_body_velocity(
            jug,
            app::grab_velocity(p.position, rig.physics->body_velocity(jug), hand, 2.1f, DT,
                               tuning),
            glm::vec3{0.0f});
        rig.physics->step(DT);
        if (!slipped
            && app::grab_slipped(glm::length(hand - rig.physics->body_pose(jug).position), DT,
                                 lag, tuning)) {
            slipped = true;
            slipped_at = i;
        }
    }
    const glm::vec3 end = rig.physics->body_pose(jug).position;
    std::printf("[заклинило] выпал через %.2f с, предмет остался на x=%.3f (стена x=0.4..0.6)\n",
                static_cast<double>(static_cast<float>(slipped_at) * DT),
                static_cast<double>(end.x));
    CHECK(slipped);
    CHECK(end.x < 0.42f); // не прошёл сквозь стену

    // КОНТРОЛЬ: та же рука без стены — предмет доходит и НЕ выпадает.
    Rig clear_way;
    const auto free_jug = clear_way.prop({0.10f, 0.15f, 0.10f}, {0.0f, 0.60f, 0.0f}, 2.1f);
    clear_way.physics->set_body_gravity_factor(free_jug, 0.0f);
    float free_lag = 0.0f;
    bool free_slipped = false;
    for (int i = 0; i < static_cast<int>(2.0f / DT); ++i) {
        const platform::BodyPose p = clear_way.physics->body_pose(free_jug);
        clear_way.physics->set_body_velocity(
            free_jug,
            app::grab_velocity(p.position, clear_way.physics->body_velocity(free_jug), hand,
                               2.1f, DT, tuning),
            glm::vec3{0.0f});
        clear_way.physics->step(DT);
        free_slipped =
            free_slipped
            || app::grab_slipped(
                glm::length(hand - clear_way.physics->body_pose(free_jug).position), DT,
                free_lag, tuning);
    }
    CHECK_FALSE(free_slipped);
    CHECK(glm::length(clear_way.physics->body_pose(free_jug).position - hand) < 0.05f);
}

TEST_CASE("тело игрока толкает бутыль и не толкает бочку") {
    Rig rig;
    // Владелец 28.08: «моё тело тоже имеет физические свойства — хочу банки,
    // бутылки, еду толкать». Потолок силы капсулы (push_force_n) обязан
    // РАЗЛИЧАТЬ массы: это второй из двух потолков.
    platform::CharacterDesc cd;
    cd.position = {0.0f, 0.0f, -1.2f};
    cd.radius = 0.35f;
    cd.height = 1.8f;
    cd.max_slope_radians = 0.9f;
    cd.step_height = 0.3f;
    cd.layer = physics::LAYER_CHARACTER;
    cd.collides_with = physics::LAYER_STATIC | physics::LAYER_LOOSE;
    // ТА ЖЕ СИЛА, ЧТО У ЖИВОГО ИГРОКА, и это не формальность: рука, мерившая
    // умолчание Jolt в 100 Н, отвечала бы про мир, в который никто не играет.
    cd.push_force_n = gameplay::PLAYER_PUSH_FORCE_N;
    const auto man = rig.physics->create_character(cd);
    REQUIRE(man.valid());

    const auto bottle = rig.prop({0.05f, 0.16f, 0.05f}, {0.0f, 0.16f, 0.0f}, 0.5f, "glass");
    const glm::vec3 bottle_before = rig.physics->body_pose(bottle).position;
    for (int i = 0; i < static_cast<int>(2.0f / DT); ++i) {
        rig.physics->move_character(man, glm::vec3{0.0f, -0.02f, 1.5f} * DT);
        rig.physics->step(DT);
    }
    const float bottle_moved = glm::length(rig.physics->body_pose(bottle).position - bottle_before);

    // КОНТРОЛЬ ТОЙ ЖЕ ФОРМЫ: бочка в 200 кг на том же месте — тот же проход.
    Rig heavy_rig;
    const auto man2 = heavy_rig.physics->create_character(cd);
    REQUIRE(man2.valid());
    const auto barrel =
        heavy_rig.prop({0.30f, 0.45f, 0.30f}, {0.0f, 0.45f, 0.0f}, 200.0f, "oak");
    const glm::vec3 barrel_before = heavy_rig.physics->body_pose(barrel).position;
    for (int i = 0; i < static_cast<int>(2.0f / DT); ++i) {
        heavy_rig.physics->move_character(man2, glm::vec3{0.0f, -0.02f, 1.5f} * DT);
        heavy_rig.physics->step(DT);
    }
    const float barrel_moved =
        glm::length(heavy_rig.physics->body_pose(barrel).position - barrel_before);

    std::printf("[толчок телом] бутыль (0.5 кг) уехала на %.3f м, бочка (200 кг) на %.3f м\n",
                static_cast<double>(bottle_moved), static_cast<double>(barrel_moved));
    CHECK(bottle_moved > 0.10f);
    CHECK(barrel_moved < 0.5f * bottle_moved);
}

TEST_CASE("бюджет: во что обходится комната предметов") {
    // ЗАМЕР, А НЕ ПОРОГ (правило 38 и довод sim_collision_cost): стенная
    // секунда на чужой машине ничего не запрещает, а кривая «сколько стоит
    // тридцать предметов» отвечает на вопрос волны — «локация с тридцатью
    // предметами не должна просесть».
    //
    // ДВА ПЛЕЧА, И ВТОРОЕ ВАЖНЕЕ: спящая комната (так живёт мир) и комната,
    // разбуженная целиком (так она выглядит в худшую секунду). Первое плечо
    // без второго измеряло бы, что мы ничего не трогали.
    for (const int count : {0, 10, 30, 60, 120}) {
        for (const bool awake : {false, true}) {
            Rig rig;
            std::vector<platform::PhysicsBodyHandle> props;
            props.reserve(static_cast<std::size_t>(count));
            for (int i = 0; i < count; ++i) {
                const float x = static_cast<float>(i % 12) * 0.30f;
                const float z = static_cast<float>(i / 12) * 0.30f;
                props.push_back(rig.prop({0.06f, 0.05f, 0.06f}, {x, 0.05f, z}, 0.9f));
                if (awake) {
                    rig.physics->activate_body(props.back());
                }
            }
            const auto t0 = std::chrono::steady_clock::now();
            constexpr int TICKS = 600; // десять секунд игры
            for (int i = 0; i < TICKS; ++i) {
                rig.physics->step(DT);
            }
            const auto t1 = std::chrono::steady_clock::now();
            const double us =
                std::chrono::duration<double, std::micro>(t1 - t0).count() / TICKS;
            std::printf("[бюджет] %3d тел, %-8s — %.1f мкс на тик (%.2f%% от 16.7 мс)\n",
                        count, awake ? "разбужены" : "спят", us, 100.0 * us / 16666.0);
        }
    }
}

TEST_CASE("короткое и долгое нажатие различаются по СЫРОЙ клавише") {
    const app::GrabTuning t;
    app::GrabHold hold;
    // Короткий щелчок: 0.1 с — «взять» не случается ни разу.
    for (int i = 0; i < static_cast<int>(0.10f / DT); ++i) {
        CHECK(app::grab_press(hold, true, DT, t) != app::GrabPress::Long);
    }
    CHECK(app::grab_press(hold, false, DT, t) == app::GrabPress::None);

    // Долгое: порог берётся РОВНО ОДИН РАЗ, сколько бы ни держали дальше.
    int longs = 0;
    for (int i = 0; i < static_cast<int>(1.50f / DT); ++i) {
        if (app::grab_press(hold, true, DT, t) == app::GrabPress::Long) {
            ++longs;
        }
    }
    CHECK(longs == 1);
}

TEST_CASE("бросок: лёгкое летит, тяжёлое падает под ноги") {
    const app::GrabTuning t;
    const float cup = app::throw_speed(0.5f, t);
    const float sack = app::throw_speed(46.0f, t);
    std::printf("[бросок] кубок %.2f м/с, мешок %.2f м/с\n", static_cast<double>(cup),
                static_cast<double>(sack));
    CHECK(cup == doctest::Approx(t.throw_max_speed)); // упирается в потолок
    CHECK(sack < 0.5f);
}
