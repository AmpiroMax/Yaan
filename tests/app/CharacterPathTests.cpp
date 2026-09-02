/*
Module: tests
File: tests/app/CharacterPathTests.cpp

Responsibility:
- ПРИБОР НА ПУТИ ИГРОКА (заказ владельца 02.09). Владелец открыл экран
  создания и увидел слипшиеся ноги и руки у ягодиц — в тот день, когда стенд
  персонажа отчитывался нулём пересечений на 36 позах. Стенд мерил клип покоя
  со слоем стойки и обходом рук; экран показывал ГОЛУЮ позу привязки рига
  через ретаргет. Два пути позы — два ответа. Этот набор меряет ту позу,
  которую строят ЭКРАН и СМОТРОВАЯ, теми же классами и тем же вызовом, что и
  кнопка меню, и судит её порогами владельца (строки REST_GAP_*).

Key items:
- the_chargen_screen_pose_meets_the_gaps: CharGenBody (то, что грузит
  chargen_enter) -> screen_gaps() -> нога↔нога ≥ 2 см, кисть↔бедро ≥ 1.5 см,
  предплечье↔корпус ≥ 2 см, ПО МЕШУ, со знаком.
- the_viewer_pose_meets_the_gaps: та же мера на позе, которую рисует
  смотровая для персонажей.
- the_instrument_sees_an_intersection: контрольная рука (правило 30) — ноги,
  сведённые в одну ось, обязаны дать ОТРИЦАТЕЛЬНЫЙ зазор; минимум по парам
  вершин при этом остаётся положительным, и это ровно тот прибор, который
  «ноги слиплись» пропускал.

Dependencies:
- Uses: doctest, engine/app CharGenBody, engine/anim (BodyGaps, Rig,
  SkinnedBody, Hitbox), нулевой бэкенд рендера, HumanBase.dfo.
- Used by: ctest (app_character_path).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Пороги — строки реестра через BodyGapTargets::from_config(), не литералы:
  решатель рест-позы целится в те же строки (правило 35).
*/

#include "engine/app/sources/CharGenBody.h"

#include "engine/anim/sources/BodyGaps.h"
#include "engine/anim/sources/Hitbox.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/platform/render/sources/null/NullRenderer.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>
#include <glm/gtc/quaternion.hpp>

using namespace dfn;

namespace {

namespace fs = std::filesystem;

[[nodiscard]] bool body_present() {
    std::error_code ec;
    return fs::exists(app::CHARGEN_SOURCE_BODY, ec);
}

void judge(const char* where, const anim::BodyGaps& g) {
    const anim::BodyGapTargets t = anim::BodyGapTargets::from_config();
    MESSAGE(where << ": " << anim::describe_gaps(g));
    REQUIRE(g.valid);
    CAPTURE(g.legs.judged_m());
    CAPTURE(g.hand_thigh_worst_m());
    CAPTURE(g.forearm_trunk_worst_m());
    // ПО МЕШУ И СО ЗНАКОМ: отрицательное число — взаимопроникновение.
    CHECK(g.legs.judged_m() >= t.legs_m);
    CHECK(g.hand_thigh_worst_m() >= t.hand_thigh_m);
    CHECK(g.forearm_trunk_worst_m() >= t.forearm_trunk_m);
    // И КОРОБКИ, КОТОРЫМИ ПОЛЬЗУЕТСЯ ИГРА, НЕ ПЕРЕСЕКАЮТСЯ у тех же пар.
    CHECK(g.legs_box_m > 0.0f);
    CHECK(g.hand_thigh_box_m[0] > 0.0f);
    CHECK(g.hand_thigh_box_m[1] > 0.0f);
    CHECK(anim::gaps_meet(g, t));
}

} // namespace

TEST_CASE("the_chargen_screen_pose_meets_the_gaps") {
    if (!body_present()) {
        MESSAGE("HumanBase.dfo нет в дереве — набор пропущен");
        return;
    }
    // РОВНО ТОТ ЖЕ ПУТЬ, ЧТО У КНОПКИ «СОЗДАТЬ ПЕРСОНАЖА»: App::chargen_enter
    // строит риг ensure_body_rig() и зовёт CharGenBody::load с ним. Второго
    // способа собрать позу экрана в дереве нет, и этот набор его не заводит.
    platform::NullRenderer renderer;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(renderer, rig, fs::path(app::CHARGEN_SOURCE_BODY)));
    judge("экран создания", body.screen_gaps());
    body.release(renderer);
}

TEST_CASE("the_viewer_pose_meets_the_gaps") {
    if (!body_present()) {
        MESSAGE("HumanBase.dfo нет в дереве — набор пропущен");
        return;
    }
    // СМОТРОВАЯ рисует скин персонажа с ЕДИНИЧНОЙ палитрой, то есть в позе
    // привязки (AppViewer.cpp, append_skin). Здесь та же поза строится как
    // сэмпл и меряется тем же прибором, что и экран выше.
    const auto obj = render::read_object(fs::path(app::CHARGEN_SOURCE_BODY));
    REQUIRE(obj);
    REQUIRE_FALSE(obj->skeleton.empty());
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    const anim::SkinnedRigBinding binding = anim::bind_skinned_rig(rig, obj->skeleton);
    REQUIRE(binding.bound_count() > 0);
    anim::HitboxSet boxes = anim::build_hitboxes(rig.proportions);
    anim::fit_hitboxes_to_skin(boxes, rig, obj->skeleton, binding, obj->skin.vertices);
    const anim::SkinParts parts =
        anim::label_skin_parts(obj->skeleton, binding, obj->skin.vertices);
    std::vector<anim::JointLocal> sample(obj->skeleton.size());
    anim::bind_pose_sample(obj->skeleton, sample);
    judge("смотровая (поза привязки)",
          anim::measure_body_gaps(obj->skeleton, binding, boxes, obj->skin.vertices,
                                  parts, sample));
}

TEST_CASE("the_instrument_sees_an_intersection") {
    if (!body_present()) {
        MESSAGE("HumanBase.dfo нет в дереве — набор пропущен");
        return;
    }
    // КОНТРОЛЬНАЯ РУКА (правило 30): ноги, повёрнутые внутрь у бедра на 12°
    // каждая, входят друг в друга. Боковой зазор обязан стать ОТРИЦАТЕЛЬНЫМ,
    // а минимум по парам вершин — остаться положительным: это и есть разница
    // между прибором, который видит «слиплись», и прибором, который нет.
    const auto obj = render::read_object(fs::path(app::CHARGEN_SOURCE_BODY));
    REQUIRE(obj);
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    const anim::SkinnedRigBinding binding = anim::bind_skinned_rig(rig, obj->skeleton);
    REQUIRE(binding.bound_count() > 0);
    anim::HitboxSet boxes = anim::build_hitboxes(rig.proportions);
    anim::fit_hitboxes_to_skin(boxes, rig, obj->skeleton, binding, obj->skin.vertices);
    const anim::SkinParts parts =
        anim::label_skin_parts(obj->skeleton, binding, obj->skin.vertices);
    anim::LocalPose crossed;
    // Roll about the body's forward axis (+Z in bone space, docs/RIG.md): the
    // left thigh (-X) inward is a positive turn, the right a negative one.
    const float in = 0.21f; // 12 degrees
    crossed.rotation[anim::bone_index(anim::Bone::ThighL)] =
        glm::angleAxis(in, glm::vec3{0.0f, 0.0f, 1.0f});
    crossed.rotation[anim::bone_index(anim::Bone::ThighR)] =
        glm::angleAxis(-in, glm::vec3{0.0f, 0.0f, 1.0f});
    std::vector<anim::JointLocal> sample(obj->skeleton.size());
    anim::pose_local_transforms(rig, obj->skeleton, binding, crossed, sample);
    const anim::BodyGaps g = anim::measure_body_gaps(obj->skeleton, binding, boxes,
                                                     obj->skin.vertices, parts, sample);
    MESSAGE("контроль (ноги внутрь): " << anim::describe_gaps(g));
    REQUIRE(g.valid);
    REQUIRE(g.legs.bands > 0);
    CHECK(g.legs.lateral_m < -0.01f);
    CHECK(g.legs.pair_m > 0.0f);
    CHECK(g.legs_box_m == 0.0f);
}
