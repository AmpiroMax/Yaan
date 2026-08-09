/*
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 01:56:45
Module: engine/anim
File: engine/anim/sources/Rig.cpp

Responsibility:
- Builds the rest pose from proportions and maps the BODY_*_FRAC NUMBERS rows
  into meters (the only file in this zone reading dfn::config for body shape).

Key items:
- RigProportions::from_config(), Rig::build(), bone_name().

Dependencies:
- Uses: Rig.h, generated Constants.h (via engine/core/config).
- Used by: everything in engine/anim, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- H is PLAYER_CAPSULE_HEIGHT on purpose (Rule 35: the body fits sim's capsule;
  there is deliberately no BODY_HEIGHT row). Do not introduce a second height.
*/
/*
UPD:
- 10:08:2026 - 01:56:45: Initial rest-pose build + config mapping.
*/

#include "engine/anim/sources/Rig.h"

#include "engine/core/config/sources/Constants.h"

namespace dfn::anim {

std::string_view bone_name(Bone b) {
    switch (b) {
    case Bone::Pelvis: return "pelvis";
    case Bone::Torso: return "torso";
    case Bone::Head: return "head";
    case Bone::UpperArmL: return "upper_arm_l";
    case Bone::ForearmL: return "forearm_l";
    case Bone::HandL: return "hand_l";
    case Bone::UpperArmR: return "upper_arm_r";
    case Bone::ForearmR: return "forearm_r";
    case Bone::HandR: return "hand_r";
    case Bone::ThighL: return "thigh_l";
    case Bone::ShinL: return "shin_l";
    case Bone::FootL: return "foot_l";
    case Bone::ThighR: return "thigh_r";
    case Bone::ShinR: return "shin_r";
    case Bone::FootR: return "foot_r";
    }
    return "?";
}

RigProportions RigProportions::from_config() {
    const auto h = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
    RigProportions p;
    p.ankle_height = h * static_cast<float>(config::BODY_ANKLE_HEIGHT_FRAC);
    p.knee_height = h * static_cast<float>(config::BODY_KNEE_HEIGHT_FRAC);
    p.hip_height = h * static_cast<float>(config::BODY_HIP_HEIGHT_FRAC);
    p.shoulder_height = h * static_cast<float>(config::BODY_SHOULDER_HEIGHT_FRAC);
    p.neck_height = h * static_cast<float>(config::BODY_NECK_HEIGHT_FRAC);
    p.head_height = h * static_cast<float>(config::BODY_HEAD_HEIGHT_FRAC);
    p.upper_arm_length = h * static_cast<float>(config::BODY_UPPER_ARM_FRAC);
    p.forearm_length = h * static_cast<float>(config::BODY_FOREARM_FRAC);
    p.hand_length = h * static_cast<float>(config::BODY_HAND_FRAC);
    p.foot_length = h * static_cast<float>(config::BODY_FOOT_LENGTH_FRAC);
    p.shoulder_width = h * static_cast<float>(config::BODY_SHOULDER_WIDTH_FRAC);
    p.hip_width = h * static_cast<float>(config::BODY_HIP_WIDTH_FRAC);
    p.torso_depth = h * static_cast<float>(config::BODY_TORSO_DEPTH_FRAC);
    p.head_width = h * static_cast<float>(config::BODY_HEAD_WIDTH_FRAC);
    p.arm_thickness = h * static_cast<float>(config::BODY_ARM_THICKNESS_FRAC);
    p.leg_thickness = h * static_cast<float>(config::BODY_LEG_THICKNESS_FRAC);
    return p;
}

Rig Rig::build(const RigProportions& p) {
    Rig rig;
    rig.proportions = p;
    auto& o = rig.rest_offset;

    // Root: pelvis joint sits at hip height above the ground point; the FK
    // root transform supplies that lift (Pose.cpp), so the pelvis offset here
    // is zero — segment meshes and children all hang off the hip center.
    o[bone_index(Bone::Pelvis)] = {0.0f, 0.0f, 0.0f};
    // Torso shares the hip center and extends up (its mesh does the rising).
    o[bone_index(Bone::Torso)] = {0.0f, 0.0f, 0.0f};
    o[bone_index(Bone::Head)] = {0.0f, p.torso_length(), 0.0f};

    const float shoulder_up = p.shoulder_height - p.hip_height;
    const float sx = p.shoulder_width * 0.5f;
    o[bone_index(Bone::UpperArmL)] = {-sx, shoulder_up, 0.0f};
    o[bone_index(Bone::ForearmL)] = {0.0f, -p.upper_arm_length, 0.0f};
    o[bone_index(Bone::HandL)] = {0.0f, -p.forearm_length, 0.0f};
    o[bone_index(Bone::UpperArmR)] = {sx, shoulder_up, 0.0f};
    o[bone_index(Bone::ForearmR)] = {0.0f, -p.upper_arm_length, 0.0f};
    o[bone_index(Bone::HandR)] = {0.0f, -p.forearm_length, 0.0f};

    const float hx = p.hip_width * 0.5f;
    o[bone_index(Bone::ThighL)] = {-hx, 0.0f, 0.0f};
    o[bone_index(Bone::ShinL)] = {0.0f, -p.thigh_length(), 0.0f};
    o[bone_index(Bone::FootL)] = {0.0f, -p.shin_length(), 0.0f};
    o[bone_index(Bone::ThighR)] = {hx, 0.0f, 0.0f};
    o[bone_index(Bone::ShinR)] = {0.0f, -p.thigh_length(), 0.0f};
    o[bone_index(Bone::FootR)] = {0.0f, -p.shin_length(), 0.0f};

    return rig;
}

} // namespace dfn::anim
