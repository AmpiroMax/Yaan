/*
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 01:56:45
Module: engine/anim
File: engine/anim/sources/Rig.h

Responsibility:
- The frozen humanoid skeleton: bone enum, hierarchy, and the rest pose built
  from the BODY_*_FRAC proportion rows. This hierarchy is the future NPC
  contract (docs/RIG.md) — extend at the end, never reorder.

Key items:
- Bone: the 15-bone enum; BONE_COUNT; bone_parent(); bone_name().
- RigProportions: segment lengths/joint heights in METERS, derived from the
  NUMBERS fractions x PLAYER_CAPSULE_HEIGHT (from_config()), or synthetic for
  tests.
- Rig: rest offsets per bone (parent-joint -> this-joint), built once from
  proportions.
- MIRROR_BONE: the left<->right bone swap table for pose mirroring.

Dependencies:
- Uses: glm (Rule 2), generated constants (from_config only).
- Used by: Pose.h (FK), Clips (joint targets), BodyMesh (segment geometry),
  Body (segment entities), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Bone order/parents are contract (docs/RIG.md): adding at the end is
  compatible; renaming/reparenting/reordering needs a group sync (Rule 26).
- No hardcoded proportions outside from_config() reading dfn::config (Rule 14).
*/
/*
UPD:
- 10:08:2026 - 01:56:45: Initial rig: 15 bones, hierarchy, rest pose from
                         BODY_* rows, mirror table.
*/

#pragma once

#include <array>
#include <cstdint>
#include <glm/vec3.hpp>
#include <string_view>

namespace dfn::anim {

// Bone indices are contract (docs/RIG.md): RenderMesh id = 34 + index.
enum class Bone : uint8_t {
    Pelvis = 0,
    Torso,
    Head,
    UpperArmL,
    ForearmL,
    HandL,
    UpperArmR,
    ForearmR,
    HandR,
    ThighL,
    ShinL,
    FootL,
    ThighR,
    ShinR,
    FootR,
};
inline constexpr uint32_t BONE_COUNT = 15;

[[nodiscard]] constexpr uint32_t bone_index(Bone b) { return static_cast<uint32_t>(b); }

// Parent of each bone; -1 = root (Pelvis). Order matches the enum, and every
// parent precedes its children, so FK is a single forward pass.
inline constexpr std::array<int8_t, BONE_COUNT> BONE_PARENT{
    -1, // Pelvis
    0,  // Torso        <- Pelvis
    1,  // Head         <- Torso
    1,  // UpperArmL    <- Torso
    3,  // ForearmL     <- UpperArmL
    4,  // HandL        <- ForearmL
    1,  // UpperArmR    <- Torso
    6,  // ForearmR     <- UpperArmR
    7,  // HandR        <- ForearmR
    0,  // ThighL       <- Pelvis
    9,  // ShinL        <- ThighL
    10, // FootL        <- ShinL
    0,  // ThighR       <- Pelvis
    12, // ShinR        <- ThighR
    13, // FootR        <- ShinR
};

// Left<->right swap for pose mirroring (docs/RIG.md): symmetric bones swap,
// center bones map to themselves. An involution by construction.
inline constexpr std::array<uint8_t, BONE_COUNT> MIRROR_BONE{
    0, 1, 2,      // Pelvis, Torso, Head
    6, 7, 8,      // UpperArmL..HandL -> right side
    3, 4, 5,      // UpperArmR..HandR -> left side
    12, 13, 14,   // ThighL..FootL -> right side
    9, 10, 11,    // ThighR..FootR -> left side
};

[[nodiscard]] std::string_view bone_name(Bone b);

// Everything in METERS (Rule 14). Joint heights are from the GROUND; segment
// lengths are joint-to-joint. from_config() derives all of it as
// BODY_*_FRAC x PLAYER_CAPSULE_HEIGHT; tests may build synthetic instances.
struct RigProportions {
    // Joint heights (rest, standing).
    float ankle_height = 0.0f;
    float knee_height = 0.0f;
    float hip_height = 0.0f;
    float shoulder_height = 0.0f;
    float neck_height = 0.0f;
    float head_height = 0.0f; // neck -> crown
    // Limb segment lengths.
    float upper_arm_length = 0.0f;
    float forearm_length = 0.0f;
    float hand_length = 0.0f;
    float foot_length = 0.0f;
    // Widths / thicknesses (segment boxes, LocalBounds).
    float shoulder_width = 0.0f; // biacromial, arm pivots sit at +-half
    float hip_width = 0.0f;      // leg pivots sit at +-half
    float torso_depth = 0.0f;
    float head_width = 0.0f;
    float arm_thickness = 0.0f;
    float leg_thickness = 0.0f;

    // Derived (consistency asserted in tests: hip == thigh + shin + ankle).
    [[nodiscard]] float thigh_length() const { return hip_height - knee_height; }
    [[nodiscard]] float shin_length() const { return knee_height - ankle_height; }
    [[nodiscard]] float torso_length() const { return neck_height - hip_height; }

    // The one place that reads dfn::config for body shape (Rule 14).
    [[nodiscard]] static RigProportions from_config();
};

// Rest offsets: translation from the parent's joint to this bone's joint, in
// the parent's frame, identity rotations = standing rest pose (docs/RIG.md:
// at yaw 0 the character faces -Z and +X is its RIGHT, so left = -X).
struct Rig {
    RigProportions proportions;
    std::array<glm::vec3, BONE_COUNT> rest_offset{};

    [[nodiscard]] static Rig build(const RigProportions& p);
};

} // namespace dfn::anim
