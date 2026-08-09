/*
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 01:56:45
Module: tests
File: tests/character/BodyTests.cpp

Responsibility:
- ECS body system tests: segment spawning (head hidden for first person),
  snapshot discipline on segment transforms, locomotion evaluation wiring,
  the mirror puppet (I turn left -> it turns right, positions reflected),
  and showcase mode (floats, cycles clips).

Dependencies:
- Uses: doctest, dfn_anim, dfn_core, constants.
- Used by: ctest (character_body).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Expectations derive from dfn::config constants, never literal duplicates.
*/
/*
UPD:
- 10:08:2026 - 01:56:45: Initial suite.
*/

#include <doctest/doctest.h>

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "engine/anim/sources/Body.h"
#include "engine/anim/sources/BodyMesh.h"
#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"

using namespace dfn;
using namespace dfn::anim;

namespace {

[[nodiscard]] ecs::EntityId make_owner(ecs::World& world, const glm::vec3& at) {
    const ecs::EntityId id = world.spawn();
    world.add(id, components::Transform{.position = at});
    world.add(id, components::PreviousTransform{.position = at});
    return id;
}

} // namespace

TEST_CASE("spawn_body creates all segments; first person hides only the head") {
    ecs::World world;
    const Rig rig = Rig::build(RigProportions::from_config());
    const auto owner = make_owner(world, {5.0f, 1.0f, -3.0f});
    spawn_body(world, owner, rig, /*hide_head=*/true);

    const auto* body = world.get<BodyRig>(owner);
    REQUIRE(body != nullptr);
    // Value, not pointer: spawning ANOTHER body below grows the component
    // pool and may relocate `body` (sparse-set semantics) — comparing through
    // the stale pointer was this test's own first bug.
    const auto first_segment = body->segments[0];
    for (uint32_t b = 0; b < BONE_COUNT; ++b) {
        const auto bone = static_cast<Bone>(b);
        CAPTURE(bone_name(bone));
        REQUIRE(world.alive(body->segments[b]));
        const auto* rm = world.get<components::RenderMesh>(body->segments[b]);
        REQUIRE(rm != nullptr);
        if (bone == Bone::Head) {
            CHECK(rm->mesh_asset == 0); // camera sits inside the skull
        } else {
            CHECK(rm->mesh_asset == body_segment_mesh_id(bone));
        }
    }
    // Control: a THIRD-person body (the puppet's case) keeps its head.
    const auto other = make_owner(world, {0.0f, 0.0f, 0.0f});
    spawn_body(world, other, rig, /*hide_head=*/false);
    const auto* other_body = world.get<BodyRig>(other);
    REQUIRE(other_body != nullptr);
    const auto* head_rm = world.get<components::RenderMesh>(
        other_body->segments[bone_index(Bone::Head)]);
    REQUIRE(head_rm != nullptr);
    CHECK(head_rm->mesh_asset == body_segment_mesh_id(Bone::Head));

    // Idempotence: spawning twice must not double the segments.
    spawn_body(world, owner, rig, true);
    CHECK(world.get<BodyRig>(owner)->segments[0] == first_segment);
}

TEST_CASE("update writes segment transforms with snapshot discipline") {
    ecs::World world;
    const Rig rig = Rig::build(RigProportions::from_config());
    const auto owner = make_owner(world, {2.0f, 0.5f, 2.0f});
    spawn_body(world, owner, rig, false);
    auto* drive = world.get<BodyDrive>(owner);
    REQUIRE(drive != nullptr);
    drive->stride_phase = 0.1f;
    drive->step_length_m = 0.8f;
    drive->speed_mps = static_cast<float>(config::WALK_SPEED);
    drive->facing_yaw = 0.7f;

    update_bodies(world, rig);
    const auto* body = world.get<BodyRig>(owner);
    const auto seg = body->segments[bone_index(Bone::Head)];
    const glm::vec3 first = world.get<components::Transform>(seg)->position;
    // The head sits above the owner's ground point, world-positioned.
    CHECK(first.y > 2.0f * 0.5f); // well above the ground point's y

    drive->stride_phase = 0.35f; // quarter cycle on
    update_bodies(world, rig);
    const auto* tr = world.get<components::Transform>(seg);
    const auto* prev = world.get<components::PreviousTransform>(seg);
    // prev must hold the FIRST tick's value (render interpolates the pair).
    CHECK(prev->position.x == doctest::Approx(first.x));
    CHECK(prev->position.y == doctest::Approx(first.y));
    CHECK(prev->position.z == doctest::Approx(first.z));
    // ...and the pose actually moved (control: the test can fail).
    const float moved = glm::length(tr->position - first);
    CHECK(moved > 1e-4f);
}

TEST_CASE("airborne and crouch change the evaluated pose") {
    const Rig rig = Rig::build(RigProportions::from_config());
    BodyDrive drive;
    drive.speed_mps = 0.0f;
    const LocalPose standing = evaluate_body_pose(rig, drive);
    BodyDrive airborne = drive;
    airborne.grounded = false;
    const LocalPose air = evaluate_body_pose(rig, airborne);
    BodyDrive crouched = drive;
    crouched.crouch_blend = 1.0f;
    const LocalPose crouch = evaluate_body_pose(rig, crouched);

    const auto thigh_dot = [&](const LocalPose& a, const LocalPose& b) {
        return std::abs(glm::dot(a.rotation[bone_index(Bone::ThighL)],
                                 b.rotation[bone_index(Bone::ThighL)]));
    };
    CHECK(thigh_dot(standing, air) < 0.999f);
    CHECK(thigh_dot(standing, crouch) < 0.999f);
    CHECK(crouch.pelvis_offset.y < standing.pelvis_offset.y - 0.1f);
}

TEST_CASE("mirror puppet: I turn left, it turns right; positions reflect") {
    ecs::World world;
    const Rig rig = Rig::build(RigProportions::from_config());
    // Mirror plane faces +Z, two meters north of the source.
    const glm::vec3 plane_pt{0.0f, 0.0f, -2.0f};
    const glm::vec2 plane_n{0.0f, 1.0f};

    const auto source = make_owner(world, {0.5f, 0.0f, 0.0f});
    spawn_body(world, source, rig, true);
    const auto puppet = spawn_mirror_puppet(world, rig, source, plane_pt, plane_n);

    auto* sdrive = world.get<BodyDrive>(source);
    REQUIRE(sdrive != nullptr);
    sdrive->facing_yaw = 0.3f; // slightly left of -Z... (positive yaw = CW)
    sdrive->stride_phase = 0.2f;
    sdrive->step_length_m = 0.9f;
    sdrive->speed_mps = static_cast<float>(config::WALK_SPEED);

    update_bodies(world, rig);

    // Root reflected across z = -2.
    const auto* ptr = world.get<components::Transform>(puppet);
    REQUIRE(ptr != nullptr);
    CHECK(ptr->position.z == doctest::Approx(-4.0f));
    CHECK(ptr->position.x == doctest::Approx(0.5f));
    // Yaw mirrored: source turns one way, puppet the other (like a mirror).
    const auto* pdrive = world.get<BodyDrive>(puppet);
    REQUIRE(pdrive != nullptr);
    CHECK(pdrive->facing_yaw
          == doctest::Approx(glm::pi<float>() - 0.3f).epsilon(1e-4));

    // A segment position mirrors exactly: the source's LEFT hand and the
    // puppet's RIGHT hand are reflections of each other (mirror swaps sides).
    const auto* sbody = world.get<BodyRig>(source);
    const auto* pbody = world.get<BodyRig>(puppet);
    const glm::vec3 src_hand_l =
        world.get<components::Transform>(sbody->segments[bone_index(Bone::HandL)])
            ->position;
    const glm::vec3 pup_hand_r =
        world.get<components::Transform>(pbody->segments[bone_index(Bone::HandR)])
            ->position;
    CHECK(pup_hand_r.x == doctest::Approx(src_hand_l.x).epsilon(1e-3));
    CHECK(pup_hand_r.y == doctest::Approx(src_hand_l.y).epsilon(1e-3));
    CHECK(pup_hand_r.z
          == doctest::Approx(2.0f * plane_pt.z - src_hand_l.z).epsilon(1e-3));

    // Control (Rule 30): the puppet's LEFT hand is NOT the reflection of the
    // source's left hand mid-stride — without the L/R swap the mirror would
    // be a lag double, which is the rejected instance ("зеркалит" means the
    // handedness flips).
    const glm::vec3 pup_hand_l =
        world.get<components::Transform>(pbody->segments[bone_index(Bone::HandL)])
            ->position;
    const glm::vec3 reflected_l{src_hand_l.x, src_hand_l.y,
                                2.0f * plane_pt.z - src_hand_l.z};
    CHECK(glm::length(pup_hand_l - reflected_l) > 0.01f);
}

TEST_CASE("showcase puppet floats and cycles through the clips") {
    ecs::World world;
    const Rig rig = Rig::build(RigProportions::from_config());
    const auto source = make_owner(world, {0.0f, 0.0f, 0.0f});
    spawn_body(world, source, rig, true);
    const glm::vec3 plane_pt{0.0f, 0.0f, -3.0f};
    const auto puppet =
        spawn_mirror_puppet(world, rig, source, plane_pt, {0.0f, 1.0f});
    auto* mp = world.get<MirrorPuppet>(puppet);
    REQUIRE(mp != nullptr);
    mp->showcase = true;
    mp->hover_height_m = 1.5f;
    mp->clip_seconds = 0.5f; // fast dwell so the test sees a clip change

    update_bodies(world, rig);
    const auto* tr = world.get<components::Transform>(puppet);
    CHECK(tr->position.y == doctest::Approx(plane_pt.y + 1.5f));
    const auto* drive = world.get<BodyDrive>(puppet);
    const uint8_t first_clip = drive->showcase_clip;
    CHECK(first_clip != SHOWCASE_NONE);

    // Run one dwell period of fixed ticks: the clip index must advance.
    const int ticks =
        static_cast<int>(0.6 / static_cast<double>(config::SIM_DT));
    for (int i = 0; i < ticks; ++i) {
        update_bodies(world, rig);
    }
    CHECK(world.get<BodyDrive>(puppet)->showcase_clip != first_clip);
}
