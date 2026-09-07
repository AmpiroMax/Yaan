/*
Module: tests
File: tests/character/LookLayerTests.cpp

Responsibility:
- ПРИБОР СЛОЯ ВЗГЛЯДА (engine/anim/sources/LookLayer.h): на теле HumanBase
  голова поворачивается за камерой на заданный угол в пределах LOOK_MAX_DEG,
  без взгляда (view_valid = false) — побитовое тождество; ноги и таз слой не
  трогает.
Key items:
- the_head_follows_the_camera_within_the_limit
- no_view_no_change (контрольная рука)
Dependencies:
- Uses: doctest, engine/anim, tests/character/ClipTestModel.h, HumanBase.dfo.
- Used by: ctest (character_look_layer).
AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Пороги — строки реестра.
*/
#include "engine/anim/sources/ClipPlayer.h"
#include "engine/anim/sources/LookLayer.h"
#include "engine/core/config/sources/Constants.h"
#include "tests/character/ClipTestModel.h"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

#include <glm/gtc/quaternion.hpp>

using namespace dfn;

namespace {

/// Рыск головы в системе тела: закрутка модельной ориентации вокруг Y.
float head_yaw(const skel::Skeleton& skeleton, std::span<const anim::JointLocal> sample,
               int32_t head) {
    glm::quat rot{1.0f, 0.0f, 0.0f, 0.0f};
    std::vector<int32_t> chain;
    for (int32_t j = head; j >= 0; j = skeleton.joints[static_cast<std::size_t>(j)].parent) {
        chain.push_back(j);
    }
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        rot = rot * glm::normalize(sample[static_cast<std::size_t>(*it)].rotation);
    }
    rot = glm::normalize(rot);
    return -2.0f * std::atan2(rot.y, rot.w);
}

} // namespace

TEST_CASE("the_head_follows_the_camera_within_the_limit") {
    Model m;
    REQUIRE_MESSAGE(load(m), "bake the character first (target dfn_characters)");
    const int32_t head = m.obj.skeleton.find("DEF-head");
    const int32_t thigh = m.obj.skeleton.find("DEF-thigh.L");
    REQUIRE(head >= 0);
    REQUIRE(thigh >= 0);
    anim::BodyDrive drive;
    drive.grounded = true;
    drive.view_valid = true;
    drive.facing_yaw = 0.0f;
    const float limit = glm::radians(static_cast<float>(config::LOOK_MAX_DEG));
    std::vector<anim::JointLocal> base(m.obj.skeleton.size());
    std::vector<anim::JointLocal> turned(m.obj.skeleton.size());
    for (const float view : {glm::radians(30.0f), glm::radians(-30.0f), glm::radians(120.0f)}) {
        anim::ClipPlayback play;
        drive.view_yaw = 0.0f;
        for (int i = 0; i < 60; ++i) {
            anim::advance_playback(m.lib, drive, 1.0f / 60.0f, play);
        }
        REQUIRE(anim::playback_sample(m.obj.skeleton, m.binding, m.obj.clips, m.lib, play, 1.0f,
                                      base));
        const float yaw0 = head_yaw(m.obj.skeleton, base, head);
        drive.view_yaw = view;
        for (int i = 0; i < 60; ++i) {
            anim::advance_playback(m.lib, drive, 1.0f / 60.0f, play);
        }
        REQUIRE(anim::playback_sample(m.obj.skeleton, m.binding, m.obj.clips, m.lib, play, 1.0f,
                                      turned));
        const float dyaw = head_yaw(m.obj.skeleton, turned, head) - yaw0;
        const float want = std::clamp(view, -limit, limit);
        MESSAGE("камера " << glm::degrees(view) << "°: голова повернулась на "
                          << glm::degrees(dyaw) << "° (цель " << glm::degrees(want) << "°)");
        // ГОЛОВА ПОВОРАЧИВАЕТСЯ НА ПОЛНЫЙ УГОЛ (сумма долей 1) в пределах лимита
        CHECK(dyaw == doctest::Approx(want).epsilon(0.05));
        // НОГИ НЕ ТРОНУТЫ
        CHECK(glm::dot(base[static_cast<std::size_t>(thigh)].rotation,
                       turned[static_cast<std::size_t>(thigh)].rotation)
              == doctest::Approx(1.0f).epsilon(1.0e-4));
    }
}

TEST_CASE("no_view_no_change") {
    Model m;
    REQUIRE(load(m));
    const int32_t head = m.obj.skeleton.find("DEF-head");
    anim::BodyDrive drive;
    drive.grounded = true;
    drive.view_valid = false; // у НПС и смотровой взгляда нет
    drive.view_yaw = glm::radians(90.0f);
    anim::ClipPlayback play;
    std::vector<anim::JointLocal> s0(m.obj.skeleton.size());
    std::vector<anim::JointLocal> s1(m.obj.skeleton.size());
    for (int i = 0; i < 30; ++i) {
        anim::advance_playback(m.lib, drive, 1.0f / 60.0f, play);
    }
    REQUIRE(anim::playback_sample(m.obj.skeleton, m.binding, m.obj.clips, m.lib, play, 1.0f, s0));
    CHECK(play.look_yaw == 0.0f);
    const float y0 = head_yaw(m.obj.skeleton, s0, head);
    drive.view_valid = true;
    for (int i = 0; i < 60; ++i) {
        anim::advance_playback(m.lib, drive, 1.0f / 60.0f, play);
    }
    REQUIRE(anim::playback_sample(m.obj.skeleton, m.binding, m.obj.clips, m.lib, play, 1.0f, s1));
    // тот же покой, но со взглядом — голова ушла (контрольная рука: без — нет)
    CHECK(std::abs(head_yaw(m.obj.skeleton, s1, head) - y0) > glm::radians(20.0f));
}
