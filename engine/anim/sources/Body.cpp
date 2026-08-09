/*
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 01:56:45
Module: engine/anim
File: engine/anim/sources/Body.cpp

Responsibility:
- Body/puppet system implementation: spawn, pose evaluation, FK, segment
  Transform writes, mirroring, showcase cycling.

Dependencies:
- Uses: Body.h, BodyMesh.h (bounds), core ecs World, core components,
  generated Constants.h.
- Used by: engine/app, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Segment writes keep the snapshot discipline (prev = curr BEFORE new curr) —
  render interpolates the pair; breaking it reads as body stutter.
*/
/*
UPD:
- 10:08:2026 - 01:56:45: Initial implementation.
*/

#include "engine/anim/sources/Body.h"

#include "engine/anim/sources/BodyMesh.h"
#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>

namespace dfn::anim {

namespace {

// Locomotion blending (procedural asset data, see Clips.cpp header ruling).
constexpr float GAIT_FULL_AT_FRAC = 0.3f;  // full gait by this fraction of WALK_SPEED
constexpr float LAND_DIP_DECAY_PER_S = 4.0f;   // ~0.25 s dip
constexpr float LAND_DIP_FULL_AT_MPS = 6.0f;   // impact speed for a full dip
// Showcase: jump mini-cycle timing (seconds) and clip dwell.
constexpr float SHOWCASE_CLIP_SECONDS = 4.0f;
constexpr float JUMP_CROUCH_S = 0.35f;
constexpr float JUMP_AIR_S = 0.8f;
constexpr float JUMP_LAND_S = 0.45f;

[[nodiscard]] float fract(float v) { return v - std::floor(v); }

// The showcase double walks "in the air": its phase comes from the CLIP TIME
// axis, not from a second gameplay stride clock — there is no displacement to
// integrate on a floating body (seam note in Clips.h holds: the LIVE body
// only ever uses sim's phase).
[[nodiscard]] LocalPose showcase_pose(const Rig& rig, uint8_t clip, float t) {
    const auto step_at = [](float v) {
        return static_cast<float>(config::STEP_LENGTH_BASE)
             + static_cast<float>(config::STEP_LENGTH_PER_MPS) * v;
    };
    switch (static_cast<ShowcaseClip>(clip)) {
    case ShowcaseClip::Idle:
        return idle_pose(t);
    case ShowcaseClip::Walk: {
        const auto v = static_cast<float>(config::WALK_SPEED);
        const float step = step_at(v);
        return gait_pose(rig, fract(t * v / (2.0f * step)), step, 0.0f);
    }
    case ShowcaseClip::Run: {
        const auto v = static_cast<float>(config::RUN_SPEED);
        const float step = step_at(v);
        return gait_pose(rig, fract(t * v / (2.0f * step)), step, 1.0f);
    }
    case ShowcaseClip::Jump: {
        const float cycle = JUMP_CROUCH_S + JUMP_AIR_S + JUMP_LAND_S;
        const float ct = fract(t / cycle) * cycle;
        if (ct < JUMP_CROUCH_S) { // anticipation: ease into a crouch
            LocalPose p = idle_pose(t);
            apply_crouch(rig, 0.8f * ct / JUMP_CROUCH_S, p);
            return p;
        }
        if (ct < JUMP_CROUCH_S + JUMP_AIR_S) { // airborne: rising then falling
            const float at = (ct - JUMP_CROUCH_S) / JUMP_AIR_S;
            return air_pose(4.0f * (1.0f - 2.0f * at));
        }
        LocalPose p = idle_pose(t); // touchdown dip, decaying
        const float lt = (ct - JUMP_CROUCH_S - JUMP_AIR_S) / JUMP_LAND_S;
        apply_land_dip(rig, 1.0f - lt, p);
        return p;
    }
    case ShowcaseClip::Wave:
        return wave_pose(t);
    case ShowcaseClip::Flex:
        return flex_pose(t);
    }
    return idle_pose(t);
}

void write_segments(ecs::World& world, const BodyRig& body, const Rig& rig,
                    const LocalPose& pose, const BodyRoot& root) {
    std::array<glm::mat4, BONE_COUNT> bones;
    forward_kinematics(rig, pose, root, bones);
    for (uint32_t b = 0; b < BONE_COUNT; ++b) {
        auto* tr = world.get<components::Transform>(body.segments[b]);
        auto* prev = world.get<components::PreviousTransform>(body.segments[b]);
        if (tr == nullptr || prev == nullptr) {
            continue;
        }
        prev->position = tr->position;
        prev->rotation = tr->rotation;
        prev->scale = tr->scale;
        tr->position = glm::vec3{bones[b][3]};
        tr->rotation = glm::quat_cast(glm::mat3{bones[b]});
        tr->scale = glm::vec3{1.0f};
    }
}

} // namespace

LocalPose evaluate_body_pose(const Rig& rig, const BodyDrive& drive) {
    if (drive.showcase_clip != SHOWCASE_NONE) {
        return showcase_pose(rig, drive.showcase_clip, drive.showcase_time_s);
    }
    if (!drive.grounded) {
        return air_pose(drive.vertical_velocity);
    }
    const auto walk_speed = static_cast<float>(config::WALK_SPEED);
    const auto run_speed = static_cast<float>(config::RUN_SPEED);
    const float gait_w =
        std::clamp(drive.speed_mps / (GAIT_FULL_AT_FRAC * walk_speed), 0.0f, 1.0f);
    const float run_w = std::clamp(
        (drive.speed_mps - walk_speed) / std::max(0.01f, run_speed - walk_speed), 0.0f,
        1.0f);
    LocalPose pose = idle_pose(drive.anim_time_s);
    if (gait_w > 0.0f) {
        pose = blend(pose,
                     gait_pose(rig, drive.stride_phase, drive.step_length_m, run_w),
                     gait_w);
    }
    apply_crouch(rig, drive.crouch_blend, pose);
    apply_land_dip(rig, drive.land_dip, pose);
    return pose;
}

void spawn_body(ecs::World& world, ecs::EntityId owner, const Rig& rig, bool hide_head) {
    if (!world.alive(owner) || world.has<BodyRig>(owner)) {
        return;
    }
    const auto* owner_tr = world.get<components::Transform>(owner);
    const glm::vec3 at = owner_tr != nullptr ? owner_tr->position : glm::vec3{0.0f};

    BodyRig body;
    body.hide_head = hide_head;
    for (uint32_t b = 0; b < BONE_COUNT; ++b) {
        const auto bone = static_cast<Bone>(b);
        const ecs::EntityId seg = world.spawn();
        world.add(seg, components::Transform{.position = at});
        world.add(seg, components::PreviousTransform{.position = at});
        const bool hidden = hide_head && bone == Bone::Head;
        world.add(seg, components::RenderMesh{
                           .mesh_asset = hidden ? 0u : body_segment_mesh_id(bone),
                           .texture_asset = 0});
        const BodySegmentMesh mesh = build_body_segment_mesh(bone, rig.proportions);
        world.add(seg, components::LocalBounds{.min = mesh.bounds_min,
                                               .max = mesh.bounds_max});
        body.segments[b] = seg;
    }
    world.add(owner, std::move(body));
    world.add(owner, BodyDrive{});
}

void destroy_body(ecs::World& world, ecs::EntityId owner) {
    const auto* body = world.get<BodyRig>(owner);
    if (body == nullptr) {
        return;
    }
    for (const ecs::EntityId seg : body->segments) {
        if (world.alive(seg)) {
            world.destroy(seg);
        }
    }
    world.remove<BodyRig>(owner);
    world.remove<BodyDrive>(owner);
    world.remove<MirrorPuppet>(owner);
}

ecs::EntityId spawn_mirror_puppet(ecs::World& world, const Rig& rig,
                                  ecs::EntityId source, const glm::vec3& plane_point,
                                  const glm::vec2& plane_normal_xz) {
    const ecs::EntityId puppet = world.spawn();
    world.add(puppet, components::Transform{.position = plane_point});
    world.add(puppet, components::PreviousTransform{.position = plane_point});
    spawn_body(world, puppet, rig, /*hide_head=*/false);
    MirrorPuppet mp;
    mp.source = source;
    mp.plane_point = plane_point;
    const float len = std::max(1e-6f, glm::length(plane_normal_xz));
    mp.plane_normal_xz = plane_normal_xz / len;
    mp.clip_seconds = SHOWCASE_CLIP_SECONDS;
    world.add(puppet, std::move(mp));
    return puppet;
}

void note_landed(ecs::World& world, ecs::EntityId owner, float impact_speed_mps) {
    if (auto* drive = world.get<BodyDrive>(owner)) {
        drive->land_dip = std::clamp(impact_speed_mps / LAND_DIP_FULL_AT_MPS, 0.3f, 1.0f);
    }
}

void update_bodies(ecs::World& world, const Rig& rig) {
    const auto dt = static_cast<float>(config::SIM_DT);

    // Pass 1: live bodies (no MirrorPuppet) — advance clocks, pose, write.
    for (auto [id, body, drive] : world.view<BodyRig, BodyDrive>()) {
        if (world.has<MirrorPuppet>(id)) {
            continue;
        }
        drive.anim_time_s += dt;
        drive.land_dip = std::max(0.0f, drive.land_dip - LAND_DIP_DECAY_PER_S * dt);
        if (drive.showcase_clip != SHOWCASE_NONE) {
            drive.showcase_time_s += dt;
        }
        const auto* tr = world.get<components::Transform>(id);
        if (tr == nullptr) {
            continue;
        }
        write_segments(world, body, rig, evaluate_body_pose(rig, drive),
                       BodyRoot{tr->position, drive.facing_yaw});
    }

    // Pass 2: mirror puppets — mirror THIS tick's source pose, or showcase.
    for (auto [id, body, drive, mp] : world.view<BodyRig, BodyDrive, MirrorPuppet>()) {
        auto* tr = world.get<components::Transform>(id);
        auto* prev = world.get<components::PreviousTransform>(id);
        if (tr == nullptr || prev == nullptr) {
            continue;
        }
        prev->position = tr->position;

        if (mp.showcase) {
            // The techno-demo double: floats at hover height, cycles clips.
            drive.showcase_time_s += dt;
            const auto cycle =
                static_cast<uint32_t>(drive.showcase_time_s / mp.clip_seconds);
            drive.showcase_clip =
                static_cast<uint8_t>(cycle % SHOWCASE_CLIP_COUNT);
            tr->position = mp.plane_point + glm::vec3{0.0f, mp.hover_height_m, 0.0f};
            const float clip_t = drive.showcase_time_s
                               - static_cast<float>(cycle) * mp.clip_seconds;
            BodyDrive local = drive;
            local.showcase_time_s = clip_t;
            write_segments(world, body, rig, evaluate_body_pose(rig, local),
                           BodyRoot{tr->position, drive.facing_yaw});
            continue;
        }

        const auto* src_tr = world.get<components::Transform>(mp.source);
        const auto* src_drive = world.get<BodyDrive>(mp.source);
        if (src_tr == nullptr || src_drive == nullptr) {
            continue;
        }
        drive.showcase_clip = SHOWCASE_NONE;
        tr->position = mirror_point(src_tr->position, mp.plane_point,
                                    mp.plane_normal_xz);
        drive.facing_yaw = mirror_yaw(src_drive->facing_yaw, mp.plane_normal_xz);
        const LocalPose mirrored =
            mirror_pose(evaluate_body_pose(rig, *src_drive));
        write_segments(world, body, rig, mirrored,
                       BodyRoot{tr->position, drive.facing_yaw});
    }
}

} // namespace dfn::anim
