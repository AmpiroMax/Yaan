/*
Module: tests
File: tests/app/GroundedLocomotionTests.cpp

Responsibility:
- ПРИБОР №1 ЗАПИСКИ «НОГИ НА ЗЕМЛЕ» (docs/design/LOCOMOTION_GROUNDED.md) НА
  ПУТИ ИГРОКА: персонаж строится той же фабрикой, что в мире, тик за тиком
  тикает ДО шага сим'а, его заявка на смещение проводится (здесь — без
  физики, как есть), корень подтверждается commit_root(), и кадр строится
  build_draw() — ровно тот путь, что рисует игру. Мировая точка касания
  опорной стопы, снятая с ПАЛИТРЫ КАДРА, за окно опоры обязана стоять на
  месте не хуже FOOT_SLIDE_MAX_M (2 мм) на ходьбе, трусце и беге.
- Контрольные руки (правило 30, правило 47 — из одного бинарника): без
  замка остаток заметно больше; прежний шов (капсула от сим'а, стрид-скейл)
  сносит стопу на сантиметры.
- Симметрия покоя (заказ владельца 02.09: «стоя ноги ровно»): в покое
  лодыжки на одной линии по ходу; с дозой 0 — вынос ноги как в клипе.

Key items:
- the_planted_foot_stays_put_on_the_player_path: walk/jog/run, замок, кадр.
- stairs_and_slope_keep_the_foot_planted_and_on_the_tread: синтетический марш
  0.18/0.28 и скат 15°, корень за грунтом как капсула, снос и зазор стоп.
- without_the_lock_the_residual_is_larger / the_old_seam_slides: контроли.
- idle_feet_stand_level: симметрия покоя и её контрольная рука.

Dependencies:
- Uses: doctest, engine/app (CharacterFactory, SkinnedCharacter), engine/anim,
  нулевой бэкенд рендера, HumanBase.dfo (цель dfn_characters).
- Used by: ctest (app_grounded_locomotion).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Порог — строка реестра FOOT_SLIDE_MAX_M, не литерал; кадры — только стенды,
  этот прибор — числа.
*/

#include "engine/app/sources/CharGenBody.h"
#include "engine/app/sources/CharacterFactory.h"
#include "engine/app/sources/SkinnedCharacter.h"
#include "engine/anim/sources/Body.h"
#include "engine/anim/sources/Rig.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/platform/render/sources/null/NullRenderer.h"
#include "engine/render/sources/RenderSystem.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace dfn;

namespace {

namespace fs = std::filesystem;
constexpr float DT = static_cast<float>(config::SIM_DT);

[[nodiscard]] bool body_present() {
    std::error_code ec;
    return fs::exists(app::CHARGEN_SOURCE_BODY, ec);
}

[[nodiscard]] float step_length(float speed) {
    return static_cast<float>(config::STEP_LENGTH_BASE)
           + static_cast<float>(config::STEP_LENGTH_PER_MPS) * speed;
}

struct Harness {
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::SkinnedCharacter body;
    app::CharacterBodies bodies;
    bool ok = false;

    Harness() {
        app::CharacterSpec spec;
        spec.proportions = &rig;
        spec.mesh_asset = app::VIEWER_BODY_MESH_ID;
        spec.blade_asset = app::VIEWER_BLADE_MESH_ID;
        ok = app::build_character(body, bodies, rs, renderer, nullptr,
                                  fs::path(app::CHARGEN_SOURCE_BODY), spec);
    }
    ~Harness() {
        if (ok) {
            app::release_character(body, bodies, rs, renderer, nullptr);
        }
    }
    /// Мировое положение сустава из ПАЛИТРЫ кадра: transform · palette[j] ·
    /// bind_origin[j], где bind_origin — обратная к inverse_bind.
    [[nodiscard]] glm::vec3 joint_world(const render::RenderSystem::SkinnedDraw& d,
                                        int32_t joint) const {
        const auto j = static_cast<std::size_t>(joint);
        const glm::vec3 bind = glm::vec3{glm::inverse(body.skeleton().joints[j].inverse_bind)[3]};
        return glm::vec3{d.transform * d.palette[j] * glm::vec4{bind, 1.0f}};
    }
};

struct Run {
    float worst_spread_m = 0.0f;
    float travelled_m = 0.0f;
    uint32_t plants = 0;
    float speed_mps = 0.0f;
    std::array<float, 4> alpha_worst{}; ///< остаток к якорю по alpha 0.25/0.5/0.75/1
};

/// Прогон передачи по пути игрока. `legacy` — прежний шов: корень едет со
/// скоростью сим'а, фаза от смещения (как в PlayerMovement::advance_stride).
Run run_gear(Harness& h, anim::Gait gait, float speed, bool legacy, uint32_t ticks) {
    Run out;
    anim::BodyDrive drive;
    drive.grounded = true;
    drive.gait = gait;
    drive.speed_mps = speed;
    drive.want_speed_mps = speed;
    drive.step_length_m = step_length(speed);
    drive.facing_yaw = 0.0f;
    glm::vec3 root{0.0f};
    const int32_t toe_l = h.body.skeleton().find("DEF-toe.L");
    const int32_t toe_r = h.body.skeleton().find("DEF-toe.R");
    REQUIRE(toe_l >= 0);
    REQUIRE(toe_r >= 0);
    const int32_t ankle_l = h.body.skeleton().find("DEF-foot.L");
    const int32_t ankle_r = h.body.skeleton().find("DEF-foot.R");
    REQUIRE(ankle_l >= 0);
    REQUIRE(ankle_r >= 0);
    const int32_t toes[2] = {toe_l, toe_r};
    const int32_t ankles[2] = {ankle_l, ankle_r};
    std::array<bool, 2> track_toe{};
    const float on = static_cast<float>(config::FOOT_LOCK_ON_WEIGHT);
    const float off = static_cast<float>(config::FOOT_LOCK_OFF_WEIGHT);
    std::array<bool, 2> planted{};
    std::array<std::vector<glm::vec3>, 2> window{};
    const auto close_window = [&](std::size_t side) {
        if (window[side].size() >= 4) {
            float spread = 0.0f;
            for (std::size_t i = 0; i < window[side].size(); ++i) {
                for (std::size_t k = i + 1; k < window[side].size(); ++k) {
                    const glm::vec3 d = window[side][k] - window[side][i];
                    spread = std::max(spread, glm::length(glm::vec2{d.x, d.z}));
                }
            }
            out.worst_spread_m = std::max(out.worst_spread_m, spread);
            ++out.plants;
        }
        window[side].clear();
    };
    for (uint32_t t = 0; t < ticks; ++t) {
        if (legacy) {
            drive.stride_phase += speed * DT / (2.0f * drive.step_length_m);
            drive.stride_phase -= std::floor(drive.stride_phase);
        }
        h.body.advance(drive, root, DT);
        const anim::LocomotionOut& lo = h.body.locomotion();
        if (legacy || !lo.valid) {
            root += glm::vec3{0.0f, 0.0f, -speed * DT};
            out.travelled_m += speed * DT;
        } else {
            root += lo.root_delta_model; // рыск 0: система тела = мир
            out.travelled_m += glm::length(lo.root_delta_model);
        }
        h.body.commit_root(drive, root, DT);
        // КАДРЫ между тиками: как рисует игра — на нескольких alpha
        if (t >= 30) {
            const anim::ContactState& c = h.body.contacts();
            for (std::size_t side = 0; side < 2; ++side) {
                const float w = c.support[side];
                if (!planted[side] && w >= on) {
                    planted[side] = true;
                    track_toe[side] = c.toe_point[side]; // якорь замка: та же точка
                } else if (planted[side] && w < off) {
                    planted[side] = false;
                    close_window(side);
                } else if (planted[side] && track_toe[side] != c.toe_point[side]) {
                    close_window(side); // перекат: замок перецепился — новое окно
                    track_toe[side] = c.toe_point[side];
                }
            }
            for (const float alpha : {0.25f, 0.5f, 0.75f, 1.0f}) {
                const render::RenderSystem::SkinnedDraw d = h.body.build_draw(false, alpha);
                REQUIRE(d.palette.size() == h.body.skeleton().size());
                for (std::size_t side = 0; side < 2; ++side) {
                    if (planted[side]) {
                        const glm::vec3 p =
                            h.joint_world(d, track_toe[side] ? toes[side] : ankles[side]);
                        window[side].push_back(p);
                        // диагностика: остаток к якорю замка по alpha
                        const anim::FootLockState& lk = h.body.foot_locks();
                        if (lk.locked[side]) {
                            const glm::vec3 e = p - lk.anchor[side];
                            const float r = glm::length(glm::vec2{e.x, e.z});
                            const std::size_t ai = static_cast<std::size_t>(alpha * 4.0f) - 1;
                            out.alpha_worst[ai] = std::max(out.alpha_worst[ai], r);
                        }
                    }
                }
            }
        }
    }
    out.speed_mps = out.travelled_m / (float(ticks) * DT);
    return out;
}

} // namespace

TEST_CASE("the_planted_foot_stays_put_on_the_player_path") {
    if (!body_present()) {
        MESSAGE("HumanBase.dfo нет в дереве — набор пропущен");
        return;
    }
    Harness h;
    REQUIRE(h.ok);
    REQUIRE(h.body.feet_drive());
    REQUIRE(h.body.foot_lock());
    const float limit = static_cast<float>(config::FOOT_SLIDE_MAX_M);
    struct Gear {
        anim::Gait gait;
        float speed;
        const char* name;
    };
    const Gear gears[] = {{anim::Gait::Walk, static_cast<float>(config::WALK_SPEED), "walk"},
                          {anim::Gait::Jog, static_cast<float>(config::JOG_SPEED), "jog"},
                          {anim::Gait::Run, static_cast<float>(config::RUN_SPEED), "run"}};
    for (const Gear& g : gears) {
        const Run r = run_gear(h, g.gait, g.speed, false, 240);
        MESSAGE(g.name << ": worst " << 1000.0f * r.worst_spread_m << " мм за " << r.plants
                       << " опор, скорость " << r.speed_mps << " м/с при заказе " << g.speed
                       << "; остаток к якорю по alpha 0.25/0.5/0.75/1: "
                       << 1000.0f * r.alpha_worst[0] << " / " << 1000.0f * r.alpha_worst[1]
                       << " / " << 1000.0f * r.alpha_worst[2] << " / "
                       << 1000.0f * r.alpha_worst[3] << " мм");
        CHECK(r.plants >= 4);
        CHECK(r.worst_spread_m <= limit);
        CHECK(r.speed_mps > 0.5f * g.speed);
    }
}

TEST_CASE("without_the_lock_the_residual_is_larger") {
    if (!body_present()) {
        return;
    }
    Harness locked;
    REQUIRE(locked.ok);
    Harness loose;
    REQUIRE(loose.ok);
    loose.body.set_foot_lock(false);
    const Run a = run_gear(locked, anim::Gait::Walk, static_cast<float>(config::WALK_SPEED),
                           false, 240);
    const Run b = run_gear(loose, anim::Gait::Walk, static_cast<float>(config::WALK_SPEED),
                           false, 240);
    MESSAGE("ходьба: с замком " << 1000.0f * a.worst_spread_m << " мм, без замка "
                                << 1000.0f * b.worst_spread_m << " мм");
    CHECK(b.worst_spread_m > a.worst_spread_m);
}

TEST_CASE("the_old_seam_slides") {
    if (!body_present()) {
        return;
    }
    Harness h;
    REQUIRE(h.ok);
    h.body.set_feet_drive(false);
    h.body.set_foot_lock(false);
    const Run r = run_gear(h, anim::Gait::Walk, static_cast<float>(config::WALK_SPEED),
                           true, 240);
    MESSAGE("прежний шов, ходьба: worst " << 1000.0f * r.worst_spread_m << " мм");
    CHECK(r.plants >= 4);
    CHECK(r.worst_spread_m > 5.0f * static_cast<float>(config::FOOT_SLIDE_MAX_M));
}

TEST_CASE("idle_feet_stand_level") {
    if (!body_present()) {
        return;
    }
    const auto ankle_fore_aft = [](bool symmetric) {
        Harness h;
        REQUIRE(h.ok);
        anim::BodyDrive drive;
        drive.grounded = true;
        anim::ClipLibrary& lib = const_cast<anim::ClipLibrary&>(h.body.clip_library());
        if (!symmetric) {
            lib.idle_symmetry = 0.0f;
        }
        glm::vec3 root{0.0f};
        for (int i = 0; i < 40; ++i) {
            h.body.advance(drive, root, DT);
            h.body.commit_root(drive, root, DT);
        }
        const render::RenderSystem::SkinnedDraw d = h.body.build_draw(false, 1.0f);
        const int32_t l = h.body.skeleton().find("DEF-foot.L");
        const int32_t r = h.body.skeleton().find("DEF-foot.R");
        REQUIRE(l >= 0);
        REQUIRE(r >= 0);
        return std::abs(h.joint_world(d, l).z - h.joint_world(d, r).z);
    };
    const float level = ankle_fore_aft(true);
    const float raw = ankle_fore_aft(false);
    MESSAGE("покой: разница лодыжек по ходу " << 1000.0f * raw << " мм в клипе -> "
                                              << 1000.0f * level << " мм с симметрией");
    CHECK(level < 0.01f);
    CHECK(raw > level + 0.02f);
}

namespace {

/// Прогон по СИНТЕТИЧЕСКОМУ ГРУНТУ на пути игрока: луч в мир — лямбда, корень
/// по вертикали идёт за грунтом под капсулой, как её водит Jolt; стопы —
/// подъём на грунт (FootIk) и замок. Меряется снос анкерной точки за окно
/// опоры и знаковый зазор судимых стоп до их грунта.
struct TerrainRun {
    float worst_spread_m = 0.0f;
    float worst_penetration_m = 0.0f;
    float worst_float_m = 0.0f;
    uint32_t plants = 0;
    float climbed_m = 0.0f;
};

template <typename Ground>
TerrainRun run_terrain(Harness& h, Ground ground, anim::Gait gait, float speed, uint32_t ticks) {
    TerrainRun out;
    h.body.set_ground_probe([&](const glm::vec3& w) { return ground(w.x, w.z); });
    anim::BodyDrive drive;
    drive.grounded = true;
    drive.gait = gait;
    drive.speed_mps = speed;
    drive.want_speed_mps = speed;
    drive.step_length_m = step_length(speed);
    glm::vec3 root{0.0f, ground(0.0f, 0.0f), 0.0f};
    const int32_t toes[2] = {h.body.skeleton().find("DEF-toe.L"), h.body.skeleton().find("DEF-toe.R")};
    const int32_t ankles[2] = {h.body.skeleton().find("DEF-foot.L"), h.body.skeleton().find("DEF-foot.R")};
    REQUIRE(toes[0] >= 0);
    REQUIRE(ankles[0] >= 0);
    const float on = static_cast<float>(config::FOOT_LOCK_ON_WEIGHT);
    const float off = static_cast<float>(config::FOOT_LOCK_OFF_WEIGHT);
    std::array<bool, 2> planted{};
    std::array<bool, 2> track_toe{};
    std::array<std::vector<glm::vec3>, 2> window{};
    const auto close_window = [&](std::size_t side) {
        if (window[side].size() >= 4) {
            float spread = 0.0f;
            for (std::size_t i = 0; i < window[side].size(); ++i) {
                for (std::size_t k = i + 1; k < window[side].size(); ++k) {
                    const glm::vec3 d = window[side][k] - window[side][i];
                    spread = std::max(spread, glm::length(glm::vec2{d.x, d.z}));
                }
            }
            out.worst_spread_m = std::max(out.worst_spread_m, spread);
            ++out.plants;
        }
        window[side].clear();
    };
    const float y0 = root.y;
    for (uint32_t t = 0; t < ticks; ++t) {
        h.body.advance(drive, root, DT);
        const anim::LocomotionOut& lo = h.body.locomotion();
        if (lo.valid) {
            root += lo.root_delta_model;
        }
        root.y = ground(root.x, root.z); // капсула стоит на грунте
        h.body.commit_root(drive, root, DT);
        if (t >= 60) {
            const anim::ContactState& c = h.body.contacts();
            for (std::size_t side = 0; side < 2; ++side) {
                const float w = c.support[side];
                if (!planted[side] && w >= on) {
                    planted[side] = true;
                    track_toe[side] = c.toe_point[side];
                } else if (planted[side] && w < off) {
                    planted[side] = false;
                    close_window(side);
                } else if (planted[side] && track_toe[side] != c.toe_point[side]) {
                    close_window(side);
                    track_toe[side] = c.toe_point[side];
                }
            }
            for (const float alpha : {0.5f, 1.0f}) {
                const render::RenderSystem::SkinnedDraw d = h.body.build_draw(false, alpha);
                for (std::size_t side = 0; side < 2; ++side) {
                    if (planted[side]) {
                        window[side].push_back(
                            h.joint_world(d, track_toe[side] ? toes[side] : ankles[side]));
                    }
                }
                // ПРОНИКАНИЕ — всегда (стопа ниже проступи — дефект); ПАРЕНИЕ —
                // только у стопы с полным весом опоры: на отрыве вес IK гаснет
                // и стопа честно поднимается, а пятка над кромкой нижней
                // ступени — лестница, а не парение.
                const anim::FootGap& g = h.body.foot_gap_last();
                const anim::FootIkPlan& plan = h.body.foot_plan();
                for (std::size_t side = 0; side < 2; ++side) {
                    if (g.judged[side] == 0) {
                        continue;
                    }
                    out.worst_penetration_m =
                        std::max(out.worst_penetration_m, std::max(0.0f, -g.gap[side]));
                    // ПАРЕНИЕ — ТОЛЬКО У ОПОРНОЙ СТОПЫ: вес плана IK ≥ 0.999 есть и у
                    // маха, что проходит низко над ступенью (зазор 3 см при
                    // весе опоры 0) — это не парение, а мах.
                    if (plan.weight[side] >= 0.999f && c.support[side] >= on) {
                        out.worst_float_m = std::max(out.worst_float_m, std::max(0.0f, g.gap[side]));
                    }
                }
            }
        }
    }
    out.climbed_m = root.y - y0;
    h.body.set_ground_probe(nullptr);
    return out;
}

} // namespace

TEST_CASE("stairs_and_slope_keep_the_foot_planted_and_on_the_tread") {
    if (!body_present()) {
        return;
    }
    const float limit = static_cast<float>(config::FOOT_SLIDE_MAX_M);
    // МАРШ 0.18/0.28 (строки лестниц) навстречу ходу (−Z), начиная в 1.5 м.
    const auto stairs = [](float, float z) {
        if (z > -1.5f) {
            return 0.0f;
        }
        const float into = -1.5f - z;
        const int step = static_cast<int>(std::floor(into / 0.28f)) + 1;
        return 0.18f * static_cast<float>(std::min(step, 12));
    };
    // СКАТ 15° навстречу ходу.
    const auto slope = [](float, float z) { return z < 0.0f ? -z * std::tan(glm::radians(15.0f)) : 0.0f; };
    {
        Harness h;
        REQUIRE(h.ok);
        const TerrainRun r = run_terrain(h, stairs, anim::Gait::Walk,
                                         static_cast<float>(config::WALK_SPEED), 300);
        MESSAGE("марш: снос worst " << 1000.0f * r.worst_spread_m << " мм за " << r.plants
                                    << " опор, проникание worst " << 1000.0f * r.worst_penetration_m
                                    << " мм, парение при полной опоре worst "
                                    << 1000.0f * r.worst_float_m << " мм, набрал высоты "
                                    << r.climbed_m << " м");
        CHECK(r.plants >= 4);
        CHECK(r.climbed_m > 0.5f);
        CHECK(r.worst_spread_m <= limit);
        CHECK(r.worst_penetration_m <= 0.02f);
        CHECK(r.worst_float_m <= 0.03f);
    }
    {
        Harness h;
        REQUIRE(h.ok);
        const TerrainRun r = run_terrain(h, slope, anim::Gait::Walk,
                                         static_cast<float>(config::WALK_SPEED), 300);
        MESSAGE("скат 15°: снос worst " << 1000.0f * r.worst_spread_m << " мм за " << r.plants
                                        << " опор, проникание worst " << 1000.0f * r.worst_penetration_m
                                        << " мм, парение при полной опоре worst "
                                        << 1000.0f * r.worst_float_m << " мм, набрал высоты "
                                        << r.climbed_m << " м");
        CHECK(r.plants >= 4);
        CHECK(r.climbed_m > 0.5f);
        CHECK(r.worst_spread_m <= limit);
        CHECK(r.worst_penetration_m <= 0.02f);
        CHECK(r.worst_float_m <= 0.03f);
    }
}

TEST_CASE("turning_in_place_steps_the_feet_instead_of_twisting") {
    // ПОВОРОТ НА МЕСТЕ (владелец 02.09-2: от первого лица «ноги прикреплены к
    // точкам и скручиваются крестиком»): корпус за 2 с уходит на 180°, стопы
    // обязаны переступить — угол между стопой и корпусом не растёт дальше
    // FOOT_LOCK_TWIST_MAX_RAD, а после поворота стопы стоят под корпусом там
    // же, где стояли до него. Контрольная рука — замок без переступа: крест.
    if (!body_present()) {
        MESSAGE("HumanBase.dfo нет в дереве — набор пропущен");
        return;
    }
    struct Turn {
        float worst_offset_m = 0.0f; ///< худший уход лодыжки от места под корпусом за поворот
        float after_offset_m = 0.0f; ///< то же через секунду после поворота
    };
    const auto turn = [&](bool stepping) {
        Harness h;
        REQUIRE(h.ok);
        if (!stepping) {
            h.body.lock_params().twist_max_rad = 100.0f;
        }
        anim::BodyDrive drive;
        drive.grounded = true;
        drive.gait = anim::Gait::Walk;
        drive.speed_mps = 0.0f;
        drive.want_speed_mps = 0.0f;
        drive.step_length_m = step_length(0.0f);
        drive.facing_yaw = 0.0f;
        const glm::vec3 root{0.0f};
        const int32_t toes[2] = {h.body.skeleton().find("DEF-toe.L"),
                                 h.body.skeleton().find("DEF-toe.R")};
        const int32_t ankles[2] = {h.body.skeleton().find("DEF-foot.L"),
                                   h.body.skeleton().find("DEF-foot.R")};
        REQUIRE(toes[0] >= 0);
        REQUIRE(ankles[1] >= 0);
        // стопа в системе корпуса: положение лодыжки и курс носок−лодыжка
        const auto in_body = [&](float yaw, std::size_t side, glm::vec3& ankle, float& heading) {
            const render::RenderSystem::SkinnedDraw d = h.body.build_draw(false, 1.0f);
            const glm::mat4 to_body =
                glm::rotate(glm::mat4{1.0f}, yaw, glm::vec3{0.0f, 1.0f, 0.0f});
            const glm::vec3 a =
                glm::vec3{to_body * glm::vec4{h.joint_world(d, ankles[side]) - root, 1.0f}};
            const glm::vec3 t =
                glm::vec3{to_body * glm::vec4{h.joint_world(d, toes[side]) - root, 1.0f}};
            ankle = a;
            heading = std::atan2(t.x - a.x, -(t.z - a.z));
        };
        const auto wrap = [](float a) {
            while (a > glm::pi<float>()) a -= 2.0f * glm::pi<float>();
            while (a < -glm::pi<float>()) a += 2.0f * glm::pi<float>();
            return a;
        };
        Turn out;
        std::array<glm::vec3, 2> ankle0{};
        std::array<float, 2> heading0{};
        const uint32_t settle = 60, turning = 120, hold = 60;
        for (uint32_t tick = 0; tick < settle + turning + hold; ++tick) {
            if (tick >= settle && tick < settle + turning) {
                drive.facing_yaw = glm::pi<float>() * float(tick - settle + 1) / float(turning);
            }
            h.body.advance(drive, root, DT);
            h.body.commit_root(drive, root, DT);
            if (tick == settle - 1) {
                for (std::size_t s = 0; s < 2; ++s) {
                    in_body(drive.facing_yaw, s, ankle0[s], heading0[s]);
                }
            } else if (tick >= settle) {
                for (std::size_t s = 0; s < 2; ++s) {
                    glm::vec3 a;
                    float hd = 0.0f;
                    in_body(drive.facing_yaw, s, a, hd);
                    (void)hd;
                    const glm::vec3 e = a - ankle0[s];
                    const float off = glm::length(glm::vec2{e.x, e.z});
                    out.worst_offset_m = std::max(out.worst_offset_m, off);
                    if (tick == settle + turning + hold - 1) {
                        out.after_offset_m = std::max(out.after_offset_m, off);
                    }
                }
            }
        }
        return out;
    };
    const Turn with = turn(true);
    const Turn cross = turn(false);
    MESSAGE("поворот 180° за 2 с: с переступом лодыжка уходила от места под корпусом не дальше "
            << 1000.0f * with.worst_offset_m << " мм, через секунду после поворота "
            << 1000.0f * with.after_offset_m << " мм; без переступа "
            << 1000.0f * cross.worst_offset_m << " и " << 1000.0f * cross.after_offset_m << " мм");
    // Крест: лодыжки на 0,4 м от своих мест (стопы разошлись за спину). С
    // переступом стопа уходит не дальше плеча поворота на пороге — с запасом
    // на сход замка — и после поворота стоит под корпусом.
    CHECK(with.worst_offset_m < 0.25f);
    CHECK(with.after_offset_m < 0.06f);
    // Контрольная рука без переступа: раньше 0,44 м; с уступкой замка у полного
    // вытяжения ноги (FootIk, 03.09) стопа сползает с якоря, не рвя ногу, —
    // крест мельче (0,30), но всё равно на порядок дальше, чем с переступом.
    CHECK(cross.worst_offset_m > 0.25f);
    CHECK(cross.after_offset_m > 0.25f);
}

TEST_CASE("gear_changes_settle_where_the_gear_settles_from_standing") {
    // СМЕНА ПЕРЕДАЧИ НА ХОДУ: трусца после ходьбы обязана ехать так же, как
    // трусца с места (прибор передач мерил трусцу после ходьбы одним телом и
    // получил 2,3–2,5 м/с против 2,9 с места — передача застревала).
    if (!body_present()) {
        return;
    }
    struct Gear {
        anim::Gait gait;
        float speed;
        const char* name;
    };
    const Gear gears[] = {{anim::Gait::Walk, static_cast<float>(config::WALK_SPEED), "ходьба"},
                          {anim::Gait::Jog, static_cast<float>(config::JOG_SPEED), "трусца"},
                          {anim::Gait::Run, static_cast<float>(config::RUN_SPEED), "бег"},
                          {anim::Gait::Jog, static_cast<float>(config::JOG_SPEED), "трусца после бега"},
                          {anim::Gait::Walk, static_cast<float>(config::WALK_SPEED), "ходьба после трусцы"}};
    const auto steady_speed = [&](Harness& h, const Gear& g, glm::vec3& root) {
        anim::BodyDrive drive;
        drive.grounded = true;
        drive.gait = g.gait;
        drive.speed_mps = g.speed;
        drive.want_speed_mps = g.speed;
        drive.step_length_m = step_length(g.speed);
        drive.facing_yaw = 0.0f;
        float steady = 0.0f;
        for (uint32_t t = 0; t < 300; ++t) {
            h.body.advance(drive, root, DT);
            const anim::LocomotionOut& lo = h.body.locomotion();
            if (lo.valid) {
                root += lo.root_delta_model;
                if (t >= 120) {
                    steady += glm::length(lo.root_delta_model);
                }
            }
            h.body.commit_root(drive, root, DT);
        }
        return steady / (180.0f * DT);
    };
    // эталон: каждая передача с места, свежим телом
    float from_standing[3] = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 3; ++i) {
        Harness h;
        REQUIRE(h.ok);
        glm::vec3 root{0.0f};
        from_standing[i] = steady_speed(h, gears[i], root);
    }
    Harness h;
    REQUIRE(h.ok);
    glm::vec3 root{0.0f};
    for (const Gear& g : gears) {
        const float ref = from_standing[g.gait == anim::Gait::Walk ? 0 : g.gait == anim::Gait::Jog ? 1 : 2];
        const float steady = steady_speed(h, g, root);
        MESSAGE(g.name << ": установившаяся " << steady << " м/с, с места " << ref << ", заказ "
                       << g.speed);
        CHECK(steady > 0.9f * ref);
    }
}
