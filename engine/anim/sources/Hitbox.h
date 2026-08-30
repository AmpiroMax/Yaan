/*
Module: engine/anim
File: engine/anim/sources/Hitbox.h

Responsibility:
- WHERE A BODY CAN BE HIT, and what the answer is CALLED. One table of
  "rig bone -> shape + local offset", sized from the same proportion rows the
  rig itself is built from, plus the two pure operations everything else needs:
  put the shapes where this frame's pose puts them, and name the part a ray
  went through.

Key items:
- BodyPart: the closed set of answers a hit can have. A CHANNEL, not a
  cosmetic label: damage tables, aim assist and the crosshair all ask this.
- HitShape / HitboxSlot / HitboxSet / build_hitboxes(): the table.
- HitboxPose / hitbox_pose(): slot -> model-space frame + half extents for a
  posed skeleton.
- hitbox_raycast(): a ray against the posed set; returns the part and where.
- hitbox_contains(): the same question about a point.

Dependencies:
- Uses: Rig, SkinnedBody (JointLocal, the binding), core skeleton, glm.
- Used by: engine/app (the Jolt bodies that carry these shapes into the world),
  gameplay later, tests.

Notes:
- WHY THIS IS NOT "THE PLAYER'S HITBOXES". Nothing here knows who is being
  hit: the table is a function of a RIG's proportions and a BINDING, so the
  first NPC on the same skeleton gets its hitboxes from this same call with no
  edit. A table written against the player would have to be found and rewritten
  the day an NPC needs one, and that is the day nobody remembers it exists.
- WHY THE SHAPES ARE DERIVED AND NOT TYPED IN. Every extent below is a
  fraction of a segment the rig already knows (thigh length, shoulder width,
  leg thickness). Typed-in numbers would be a second body description beside
  RigProportions, and it would go stale the first time the canon's proportions
  move — which they have, twice.
- THE CAPSULE STAYS. These shapes are on their own collision layer and answer
  RAYS; the character controller keeps its capsule and keeps doing locomotion.
  Merging the two would make every doorway a question about elbows.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure functions and plain data (Rule 8, Rule 30): no ECS, no IO, no physics.
- BodyPart values are a CONTRACT the moment anything serialises them: extend
  at the END, never reorder (docs/RIG.md's rule for the bone enum, same reason).
*/

#pragma once

#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/skeleton/sources/Skeleton.h"

#include <array>
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <span>
#include <string_view>

namespace dfn::anim {

/// WHAT A HIT IS CALLED. Extend at the end (see the notice above).
enum class BodyPart : uint8_t {
    None = 0,
    Head,
    Chest,
    Abdomen,
    Hips,
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
inline constexpr uint32_t BODY_PART_COUNT = 17; ///< including None

[[nodiscard]] std::string_view body_part_name(BodyPart p);

/// A sphere or a box. Two shapes and not five: a limb is a box, a skull is a
/// sphere, and everything else a bought rig has is one of those two seen from
/// a different angle.
enum class HitShape : uint8_t {
    Sphere = 0,
    Box,
};

/// ONE HITBOX: the two rig joints whose line is its axis, the span of that
/// line it covers, and how thick it is across.
///
/// TWO JOINTS AND NOT A BONE PLUS AN OFFSET, and the difference is the whole
/// reason this file works on a bought skeleton. An offset "half a thigh down
/// the bone's -Y" is a statement about OUR rig's axes; the imported skeleton
/// has its own, and on a Rigify export they are not even close. A LINE BETWEEN
/// TWO JOINTS is the same line in every skeleton that has both joints, so the
/// table needs no per-asset column and the first NPC on a different rig gets
/// its boxes from the same rows.
struct HitboxSlot {
    BodyPart part = BodyPart::None;
    Bone from = Bone::Pelvis; ///< the joint the segment starts at
    Bone to = Bone::Torso;    ///< the joint that gives it its direction
    HitShape shape = HitShape::Box;
    /// The span of the from->to line this shape covers, as a fraction. Values
    /// outside [0,1] are legal and used: the skull sits PAST the head joint,
    /// the hand past the wrist, and both are expressed as a fraction of the
    /// segment above them rather than as a length nobody can check.
    float t0 = 0.0f;
    float t1 = 1.0f;
    /// Half extents ACROSS the segment, metres: x sideways, z fore-and-aft.
    /// The half extent ALONG it is derived from the span and the segment's
    /// measured length, so it follows a body of different proportions.
    /// A sphere ignores both and uses `radius`.
    float half_x = 0.0f;
    float half_z = 0.0f;
    float radius = 0.0f; ///< spheres only
};

inline constexpr uint32_t HITBOX_COUNT = 16;

struct HitboxSet {
    std::array<HitboxSlot, HITBOX_COUNT> slot{};
};

/// The table for a body of these proportions. Sized entirely from the rig; see
/// the header note on why nothing here is typed in.
[[nodiscard]] HitboxSet build_hitboxes(const RigProportions& p);

/// WHERE THE SHAPES ARE FOR ONE POSE, in the IMPORTED SKELETON'S MODEL SPACE
/// (the space `sample_palette` works in). Multiply a frame by the body's world
/// transform for a world one.
struct HitboxPose {
    std::array<glm::mat4, HITBOX_COUNT> frame{};
    /// Half extents in the frame's own axes, metres. A sphere carries its
    /// radius in all three.
    std::array<glm::vec3, HITBOX_COUNT> half{};
    /// False for a slot whose joints this model does not carry — which is how
    /// a rig with no hands ends up with no hand box rather than one at the
    /// origin, silently absorbing every ray that misses.
    std::array<uint8_t, HITBOX_COUNT> valid{};
    [[nodiscard]] uint32_t count() const;
};

[[nodiscard]] HitboxPose hitbox_pose(const HitboxSet& set, const skel::Skeleton& skeleton,
                                     const SkinnedRigBinding& binding,
                                     std::span<const JointLocal> sample);

/// WHAT A RAY WENT THROUGH. `origin`/`direction` in the pose's space;
/// direction need not be unit. None past `max_distance` or on a miss.
struct HitboxHit {
    BodyPart part = BodyPart::None;
    float distance = 0.0f;
    uint32_t slot = HITBOX_COUNT;
    [[nodiscard]] bool hit() const { return part != BodyPart::None; }
};

[[nodiscard]] HitboxHit hitbox_raycast(const HitboxSet& set, const HitboxPose& pose,
                                       const glm::vec3& origin,
                                       const glm::vec3& direction, float max_distance);

/// Which part contains `point`, or None. The same test the ray uses, exposed
/// because coverage is asked about points and hits are asked about rays.
[[nodiscard]] BodyPart hitbox_contains(const HitboxSet& set, const HitboxPose& pose,
                                       const glm::vec3& point);

} // namespace dfn::anim
