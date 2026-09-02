/*
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
- RestStance: how the limbs hang at rest (leg splay, arm abduction, elbow
  flexion) — the box body's converged rest and the «по швам» rest of a bound
  model are two values of it.
- Rig: rest offsets per bone (parent-joint -> this-joint) plus rest rotations
  from the stance, built once from proportions.
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

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <glm/gtc/quaternion.hpp>
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
    float hip_width = 0.0f;      // hip breadth: the PELVIS box and the leg PIVOTS
    float stance_width = 0.0f;   // ankle separation standing; legs converge to it
    float torso_depth = 0.0f;
    float head_width = 0.0f;
    float arm_thickness = 0.0f;
    float leg_thickness = 0.0f;

    // Derived (consistency asserted in tests: hip == thigh + shin + ankle).
    [[nodiscard]] float thigh_length() const { return hip_height - knee_height; }
    [[nodiscard]] float shin_length() const { return knee_height - ankle_height; }
    [[nodiscard]] float torso_length() const { return neck_height - hip_height; }
    [[nodiscard]] float leg_length() const { return hip_height - ankle_height; }

    // LEG CONVERGENCE, derived — never a row of its own (Rule 35: the stance is
    // the measured thing, the angle follows it). Real legs are oblique: the hip
    // JOINTS are a hip-breadth apart while a standing adult's ankles are ~0.12 m
    // apart, so hanging both legs straight down from +-hip_width/2 planted the
    // feet 0.344 m apart — a wide, comic stance, and the user said so. This is
    // the inward tilt of the hip->ankle line that closes that gap.
    [[nodiscard]] float leg_convergence() const {
        const float reach = (hip_width - stance_width) * 0.5f;
        return std::asin(std::clamp(reach / std::max(0.01f, leg_length()), -1.0f, 1.0f));
    }
    // Hip height once the legs are oblique: a converged leg spans less VERTICAL
    // distance than a straight one, so the pelvis rides lower by
    // leg_length*(1-cos) — about 7 mm here. Compensating it in the root lift is
    // what keeps the soles exactly on the ground, which is a thing the frames
    // measured (sole at y=242, grass at 243) and must not silently lose.
    [[nodiscard]] float standing_hip_height() const {
        return ankle_height + leg_length() * std::cos(leg_convergence());
    }

    // The one place that reads dfn::config for body shape (Rule 14).
    [[nodiscard]] static RigProportions from_config();
};

// HOW THE LIMBS HANG IN THE REST POSE, beyond where the joints sit. The joint
// offsets above are the skeleton; this is the STANCE the skeleton rests in,
// and it is what a bound model shows the moment nothing else moves it — the
// character screen, the viewer, the neutral every layer is a delta from.
//
// TWO RESTS, ONE STRUCT (owner's order 02.09, «поза покоя — строевая по
// швам»). The box body's rest converges the legs to the stance row because
// its hip joints sit ON THE SKIN (BODY_HIP_WIDTH_FRAC is an outer breadth)
// and hangs the arms dead vertical. A bound model has REAL hip joints, so its
// legs stand vertical under them, and its arms have flesh beside them, so
// they hang along the sides at an abduction that is SOLVED against the skin
// (RestFit.h) until the hand clears the thigh and the forearm clears the
// trunk. Both are expressed here, as four angles.
struct RestStance {
    /// The hip->ankle line off the plumb, radians, positive = OUTWARD (ankles
    /// wider than hips). The converged box rest is negative.
    float leg_splay_rad = 0.0f;
    /// The upper arm off the plumb, radians, positive = outward (abduction).
    float arm_abduction_rad = 0.0f;
    /// The elbow's rest flexion, radians, positive = hand carried forward —
    /// the same sense as the elbow hinge in `hinge_range`.
    float elbow_flex_rad = 0.0f;
    /// The upper arm carried forward of the plumb, radians, positive = hand
    /// toward -Z. Zero in both rests today; a field so a solver can use it.
    float arm_forward_rad = 0.0f;

    /// THE BOX BODY'S REST: legs converged to the stance row, arms straight.
    [[nodiscard]] static RestStance converged(const RigProportions& p) {
        RestStance s;
        s.leg_splay_rad = -p.leg_convergence();
        return s;
    }
    /// «ПО ШВАМ»: legs vertical, elbow at REST_ELBOW_FLEX, arms along the
    /// sides with the abduction still to be solved (RestFit.h).
    [[nodiscard]] static RestStance attention();
};

// Rest offsets: translation from the parent's joint to this bone's joint, in
// the parent's frame, identity rotations = standing rest pose (docs/RIG.md:
// at yaw 0 the character faces -Z and +X is its RIGHT, so left = -X).
struct Rig {
    RigProportions proportions;
    /// The stance the rest rotations below were built from — kept so a reader
    /// (a report, a test, the retarget's log) can print what the rest IS
    /// instead of inferring it back out of quaternions.
    RestStance stance;
    std::array<glm::vec3, BONE_COUNT> rest_offset{};
    // Rest ORIENTATION per bone, applied between the rest offset and the pose
    // quaternion: model = parent * T(rest_offset) * R(rest_rotation) * R(q).
    // Identity everywhere would be the old straight-down rest; the legs use it
    // to converge (see leg_convergence). Clips are unaffected — they are still
    // deltas from rest, which is exactly why the rest is the place to put it.
    std::array<glm::quat, BONE_COUNT> rest_rotation{};

    // HINGES ARE HINGES (user note 10:08:2026: knees and elbows «не должны
    // выгибаться обратно» — it read as creepy, and it was). A finite range here
    // marks the bone as a one-axis hinge: the pose's rotation for it is reduced
    // to a pitch about the bone's own X axis and clamped into [x, y] radians.
    // Everything else is left free. Applied when a pose is EVALUATED rather
    // than when a clip is authored, so a hyperextended limb is unrepresentable
    // — including from crouch, the landing dip, the showcase reel and any clip
    // written after this comment. Infinite range = free bone.
    std::array<glm::vec2, BONE_COUNT> hinge_range{};

    /// The box body's rest: `build(p, RestStance::converged(p))`.
    [[nodiscard]] static Rig build(const RigProportions& p);
    /// A rest in an explicit stance. The leg splay rolls each thigh about the
    /// body's forward axis and counter-rolls the foot so the sole stays flat;
    /// the arm abduction rolls each upper arm outward about the same axis;
    /// the elbow flexion pitches each forearm about its own hinge axis.
    [[nodiscard]] static Rig build(const RigProportions& p, const RestStance& stance);

    /// WHERE THE PELVIS RESTS ABOVE THE GROUND for THIS stance: a splayed (or
    /// converged) leg spans less vertical distance than a straight one, so
    /// the pelvis rides at ankle + leg·cos(splay). The FK root lifts by this,
    /// which is what keeps the soles exactly on the ground in either rest.
    [[nodiscard]] float rest_hip_height() const {
        return proportions.ankle_height
               + proportions.leg_length() * std::cos(stance.leg_splay_rad);
    }
};

} // namespace dfn::anim
