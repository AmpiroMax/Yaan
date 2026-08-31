/*
Module: engine/anim
File: engine/anim/sources/Hitbox.cpp

Responsibility:
- Implements the hitbox table, its per-pose placement, and the ray and point
  tests that name a body part.

Dependencies:
- Uses: Hitbox.h, SkinnedBody.h, core skeleton, glm.
- Used by: dfn_anim, engine/app (the Jolt bodies), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- No ECS, no IO, no physics: this file is handed a pose and answers questions.
*/

#include "engine/anim/sources/Hitbox.h"

#include <limits>

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

namespace dfn::anim {
namespace {

void model_matrices(const skel::Skeleton& skeleton, std::span<const JointLocal> sample,
                    std::vector<glm::mat4>& local, std::vector<glm::mat4>& out) {
    const std::size_t n = skeleton.size();
    local.assign(n, glm::mat4{1.0f});
    out.assign(n, glm::mat4{1.0f});
    for (std::size_t j = 0; j < n && j < sample.size(); ++j) {
        local[j] = glm::translate(glm::mat4{1.0f}, sample[j].translation)
                   * glm::mat4_cast(glm::normalize(sample[j].rotation))
                   * glm::scale(glm::mat4{1.0f}, sample[j].scale);
    }
    skel::skeleton_model_matrices(skeleton, local, out);
}

} // namespace

std::string_view body_part_name(BodyPart p) {
    switch (p) {
    case BodyPart::None: return "none";
    case BodyPart::Head: return "head";
    case BodyPart::Chest: return "chest";
    case BodyPart::Abdomen: return "abdomen";
    case BodyPart::Hips: return "hips";
    case BodyPart::UpperArmL: return "upper_arm_l";
    case BodyPart::ForearmL: return "forearm_l";
    case BodyPart::HandL: return "hand_l";
    case BodyPart::UpperArmR: return "upper_arm_r";
    case BodyPart::ForearmR: return "forearm_r";
    case BodyPart::HandR: return "hand_r";
    case BodyPart::ThighL: return "thigh_l";
    case BodyPart::ShinL: return "shin_l";
    case BodyPart::FootL: return "foot_l";
    case BodyPart::ThighR: return "thigh_r";
    case BodyPart::ShinR: return "shin_r";
    case BodyPart::FootR: return "foot_r";
    }
    return "none";
}

uint32_t HitboxPose::count() const {
    uint32_t n = 0;
    for (const uint8_t v : valid) {
        n += v != 0 ? 1u : 0u;
    }
    return n;
}

HitboxSet build_hitboxes(const RigProportions& p) {
    HitboxSet set;
    const float torso = std::max(0.01f, p.torso_length());
    const float thigh = std::max(0.01f, p.thigh_length());
    const float shin = std::max(0.01f, p.shin_length());
    const float upper = std::max(0.01f, p.upper_arm_length);
    const float fore = std::max(0.01f, p.forearm_length);
    uint32_t i = 0;
    const auto put = [&](BodyPart part, Bone from, Bone to, HitShape shape, float t0,
                         float t1, float hx, float hz, float radius) {
        HitboxSlot& s = set.slot[i++];
        s = HitboxSlot{part, from, to, shape, t0, t1, hx, hz, radius};
    };
    // THE SKULL, A SPHERE, PAST THE NECK. The head JOINT is the neck (the rig's
    // rest offset puts it a torso above the hips), so the ball of the head sits
    // half a head height further along the same line — expressed as a fraction
    // of the torso because that is the segment the line is measured in.
    put(BodyPart::Head, Bone::Torso, Bone::Head, HitShape::Sphere,
        1.0f + 0.5f * p.head_height / torso, 1.0f + 0.5f * p.head_height / torso, 0.0f,
        0.0f, 0.5f * p.head_width);
    // THE TRUNK IN THREE, and three rather than one because a chest and a
    // stomach are different answers to "where did it land" — the whole reason
    // the channel exists. The three spans are contiguous by construction
    // (0.55..1.00, 0.25..0.55, -0.05..0.25): a gap between them would be a
    // stripe of a body no shot can hit.
    put(BodyPart::Chest, Bone::Torso, Bone::Head, HitShape::Box, 0.55f, 1.0f,
        0.5f * p.shoulder_width, 0.5f * p.torso_depth, 0.0f);
    put(BodyPart::Abdomen, Bone::Torso, Bone::Head, HitShape::Box, 0.25f, 0.55f,
        0.46f * p.shoulder_width, 0.46f * p.torso_depth, 0.0f);
    put(BodyPart::Hips, Bone::Torso, Bone::Head, HitShape::Box, -0.08f, 0.25f,
        0.5f * p.hip_width, 0.47f * p.torso_depth, 0.0f);
    // THE ARMS. A hand has no joint past the wrist in our fifteen, so its box
    // is the forearm's line continued by a hand length — the same trick the
    // skull uses, and the same reason: a fraction of a measured segment is
    // checkable, a typed length is not.
    put(BodyPart::UpperArmL, Bone::UpperArmL, Bone::ForearmL, HitShape::Box, 0.0f, 1.0f,
        0.5f * p.arm_thickness, 0.5f * p.arm_thickness, 0.0f);
    put(BodyPart::ForearmL, Bone::ForearmL, Bone::HandL, HitShape::Box, 0.0f, 1.0f,
        0.45f * p.arm_thickness, 0.45f * p.arm_thickness, 0.0f);
    put(BodyPart::HandL, Bone::ForearmL, Bone::HandL, HitShape::Box, 1.0f,
        1.0f + p.hand_length / fore, 0.45f * p.arm_thickness, 0.35f * p.arm_thickness,
        0.0f);
    put(BodyPart::UpperArmR, Bone::UpperArmR, Bone::ForearmR, HitShape::Box, 0.0f, 1.0f,
        0.5f * p.arm_thickness, 0.5f * p.arm_thickness, 0.0f);
    put(BodyPart::ForearmR, Bone::ForearmR, Bone::HandR, HitShape::Box, 0.0f, 1.0f,
        0.45f * p.arm_thickness, 0.45f * p.arm_thickness, 0.0f);
    put(BodyPart::HandR, Bone::ForearmR, Bone::HandR, HitShape::Box, 1.0f,
        1.0f + p.hand_length / fore, 0.45f * p.arm_thickness, 0.35f * p.arm_thickness,
        0.0f);
    // THE LEGS, and the foot is the shin's line continued by an ankle height
    // with the FOOT'S OWN LENGTH across it. It is the one box whose thickness
    // is not a limb thickness, because a foot is long the way nothing else on
    // the body is.
    put(BodyPart::ThighL, Bone::ThighL, Bone::ShinL, HitShape::Box, 0.0f, 1.0f,
        0.5f * p.leg_thickness, 0.5f * p.leg_thickness, 0.0f);
    put(BodyPart::ShinL, Bone::ShinL, Bone::FootL, HitShape::Box, 0.0f, 1.0f,
        0.45f * p.leg_thickness, 0.45f * p.leg_thickness, 0.0f);
    put(BodyPart::FootL, Bone::ShinL, Bone::FootL, HitShape::Box, 1.0f,
        1.0f + p.ankle_height / shin, 0.45f * p.leg_thickness, 0.5f * p.foot_length,
        0.0f);
    put(BodyPart::ThighR, Bone::ThighR, Bone::ShinR, HitShape::Box, 0.0f, 1.0f,
        0.5f * p.leg_thickness, 0.5f * p.leg_thickness, 0.0f);
    put(BodyPart::ShinR, Bone::ShinR, Bone::FootR, HitShape::Box, 0.0f, 1.0f,
        0.45f * p.leg_thickness, 0.45f * p.leg_thickness, 0.0f);
    put(BodyPart::FootR, Bone::ShinR, Bone::FootR, HitShape::Box, 1.0f,
        1.0f + p.ankle_height / shin, 0.45f * p.leg_thickness, 0.5f * p.foot_length,
        0.0f);
    (void)thigh;
    (void)upper;
    return set;
}

HitboxPose hitbox_pose(const HitboxSet& set, const skel::Skeleton& skeleton,
                       const SkinnedRigBinding& binding,
                       std::span<const JointLocal> sample) {
    HitboxPose pose;
    if (skeleton.empty() || sample.size() < skeleton.size()) {
        return pose;
    }
    std::vector<glm::mat4> local;
    std::vector<glm::mat4> model;
    model_matrices(skeleton, sample, local, model);
    for (uint32_t i = 0; i < HITBOX_COUNT; ++i) {
        const HitboxSlot& s = set.slot[i];
        if (s.part == BodyPart::None) {
            continue;
        }
        const int32_t a = binding.names.joint[bone_index(s.from)];
        const int32_t b = binding.names.joint[bone_index(s.to)];
        if (a < 0 || b < 0) {
            continue;
        }
        const glm::vec3 A{model[static_cast<std::size_t>(a)][3]};
        const glm::vec3 B{model[static_cast<std::size_t>(b)][3]};
        const glm::vec3 axis = B - A;
        const float len = glm::length(axis);
        if (len < 1.0e-4f) {
            continue;
        }
        const glm::vec3 y = axis / len;
        // THE OTHER TWO AXES COME FROM THE JOINT'S OWN FRAME, projected onto
        // the plane across the segment. Taking a world axis instead would make
        // a raised arm's box stop following the arm's twist, and a forearm's
        // box is not square: it is the difference between hitting a wrist and
        // hitting the air beside it.
        glm::vec3 ref{model[static_cast<std::size_t>(a)][0]};
        if (glm::length(ref) < 1.0e-5f
            || std::abs(glm::dot(glm::normalize(ref), y)) > 0.98f) {
            ref = glm::vec3{model[static_cast<std::size_t>(a)][2]};
        }
        glm::vec3 x = ref - y * glm::dot(ref, y);
        if (glm::length(x) < 1.0e-5f) {
            x = std::abs(y.x) < 0.9f ? glm::cross(y, glm::vec3{1.0f, 0.0f, 0.0f})
                                     : glm::cross(y, glm::vec3{0.0f, 0.0f, 1.0f});
        }
        x = glm::normalize(x);
        const glm::vec3 z = glm::normalize(glm::cross(x, y));
        const float mid = 0.5f * (s.t0 + s.t1);
        const glm::vec3 centre = A + axis * mid;
        glm::mat4 frame{1.0f};
        frame[0] = glm::vec4{x, 0.0f};
        frame[1] = glm::vec4{y, 0.0f};
        frame[2] = glm::vec4{z, 0.0f};
        frame[3] = glm::vec4{centre, 1.0f};
        pose.frame[i] = frame;
        pose.half[i] = s.shape == HitShape::Sphere
                           ? glm::vec3{s.radius}
                           : glm::vec3{s.half_x, 0.5f * std::abs(s.t1 - s.t0) * len,
                                       s.half_z};
        pose.valid[i] = 1;
    }
    return pose;
}

namespace {

/// Ray against one oriented box, slab method, in the box's own frame.
[[nodiscard]] bool ray_box(const glm::vec3& o, const glm::vec3& d, const glm::vec3& half,
                           float max_distance, float& out_t) {
    float tmin = 0.0f;
    float tmax = max_distance;
    for (int k = 0; k < 3; ++k) {
        if (std::abs(d[k]) < 1.0e-8f) {
            if (std::abs(o[k]) > half[k]) {
                return false;
            }
            continue;
        }
        const float inv = 1.0f / d[k];
        float t1 = (-half[k] - o[k]) * inv;
        float t2 = (half[k] - o[k]) * inv;
        if (t1 > t2) {
            std::swap(t1, t2);
        }
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) {
            return false;
        }
    }
    out_t = tmin;
    return true;
}

[[nodiscard]] bool ray_sphere(const glm::vec3& o, const glm::vec3& d, float r,
                              float max_distance, float& out_t) {
    const float b = glm::dot(o, d);
    const float c = glm::dot(o, o) - r * r;
    const float disc = b * b - c;
    if (disc < 0.0f) {
        return false;
    }
    const float root = std::sqrt(disc);
    float t = -b - root;
    if (t < 0.0f) {
        t = -b + root;
    }
    if (t < 0.0f || t > max_distance) {
        return false;
    }
    out_t = t;
    return true;
}

} // namespace

HitboxHit hitbox_raycast(const HitboxSet& set, const HitboxPose& pose,
                         const glm::vec3& origin, const glm::vec3& direction,
                         float max_distance) {
    HitboxHit best;
    best.distance = max_distance;
    if (glm::length(direction) < 1.0e-8f) {
        return HitboxHit{};
    }
    const glm::vec3 dir = glm::normalize(direction);
    for (uint32_t i = 0; i < HITBOX_COUNT; ++i) {
        if (pose.valid[i] == 0) {
            continue;
        }
        const glm::mat4 inv = glm::inverse(pose.frame[i]);
        const glm::vec3 o{inv * glm::vec4{origin, 1.0f}};
        const glm::vec3 d{inv * glm::vec4{dir, 0.0f}};
        float t = 0.0f;
        const bool got = set.slot[i].shape == HitShape::Sphere
                             ? ray_sphere(o, d, pose.half[i].x, best.distance, t)
                             : ray_box(o, d, pose.half[i], best.distance, t);
        if (got && t <= best.distance) {
            best.distance = t;
            best.part = set.slot[i].part;
            best.slot = i;
        }
    }
    return best;
}

BodyPart hitbox_contains(const HitboxSet& set, const HitboxPose& pose,
                         const glm::vec3& point) {
    for (uint32_t i = 0; i < HITBOX_COUNT; ++i) {
        if (pose.valid[i] == 0) {
            continue;
        }
        const glm::vec3 p{glm::inverse(pose.frame[i]) * glm::vec4{point, 1.0f}};
        if (set.slot[i].shape == HitShape::Sphere) {
            if (glm::length(p) <= pose.half[i].x) {
                return set.slot[i].part;
            }
            continue;
        }
        if (std::abs(p.x) <= pose.half[i].x && std::abs(p.y) <= pose.half[i].y
            && std::abs(p.z) <= pose.half[i].z) {
            return set.slot[i].part;
        }
    }
    return BodyPart::None;
}

float hitbox_distance(const HitboxSet& set, const HitboxPose& pose, BodyPart part,
                      const glm::vec3& point) {
    float best = std::numeric_limits<float>::infinity();
    for (uint32_t i = 0; i < HITBOX_COUNT; ++i) {
        if (pose.valid[i] == 0 || set.slot[i].part != part) {
            continue;
        }
        const glm::vec3 p{glm::inverse(pose.frame[i]) * glm::vec4{point, 1.0f}};
        if (set.slot[i].shape == HitShape::Sphere) {
            best = std::min(best, std::max(0.0f, glm::length(p) - pose.half[i].x));
            continue;
        }
        // РАССТОЯНИЕ ДО КОРОБКИ — ДЛИНА ВЫХОДА ЗА ЕЁ ГРАНИ, покомпонентно
        // срезанного снизу нулём. Точка внутри даёт ноль по всем трём осям,
        // то есть 0 — и это правильный ответ: «касается» и «внутри на
        // сантиметр» одинаково означают, что клиренса нет.
        const glm::vec3 d = glm::max(glm::abs(p) - pose.half[i], glm::vec3{0.0f});
        best = std::min(best, glm::length(d));
    }
    return best;
}

} // namespace dfn::anim
