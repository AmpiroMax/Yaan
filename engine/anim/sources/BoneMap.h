/*
Module: engine/anim
File: engine/anim/sources/BoneMap.h

Responsibility:
- Naming bridge between an IMPORTED skeleton and the frozen humanoid rig:
  normalises a joint name and answers which Bone (if any) it drives.

Key items:
- normalize_bone_name(): lowercase, strip punctuation and authoring prefixes.
- bone_from_joint_name(): the synonym table (Mixamo/Blender/Khronos families).
- SkeletonBinding + bind_skeleton(): one joint per rig bone, root-most wins.

Dependencies:
- Uses: Rig.h, engine/core skeleton (plain data), glm.
- Used by: SkinnedBody.cpp, tools/import_gltf.cpp, tests.

Notes:
- ONE JOINT PER BONE: the anatomically EXACT name wins, and among equals the
  ROOT-MOST one does. Real rigs chain several
  joints where ours has one (a three-segment spine against our single Torso).
  Driving all of them with the SAME delta would bend the spine three times over
  -- the classic retarget defect that reads as "the model is broken". The
  joints past the first keep their bind pose and ride along with their parent,
  which is exactly what a rig with fewer bones can honestly say.
- EXTRA JOINTS ARE NOT RENAMED AND NOT DROPPED: they stay in the skeleton and
  in the palette. docs/RIG.md's freeze is about the BONE ENUM (extend at the
  end, never reorder), and an imported toe or clavicle is not a bone of ours
  until somebody adds it there deliberately.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Adding a synonym is cheap and safe; changing which Bone a synonym maps to is
  a retarget change and needs a frame to back it.
*/

#pragma once

#include "engine/anim/sources/Rig.h"
#include "engine/core/skeleton/sources/Skeleton.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace dfn::anim {

/// "mixamorig:LeftForeArm" -> "leftforearm"; "arm_joint_L_2" -> "armjointl2".
/// Strips every non-alphanumeric character and the authoring prefixes that
/// carry no anatomy ("mixamorig", "armature", "skeleton", "bip01", "def").
[[nodiscard]] std::string normalize_bone_name(std::string_view raw);

/// Which rig bone this joint name means, or nullopt for a joint we have no
/// bone for (a toe, a clavicle, a finger, a prop bone).
[[nodiscard]] std::optional<Bone> bone_from_joint_name(std::string_view raw);

/// How exact this name is for the bone it maps to: 0 = the anatomically exact
/// name, 1 = an acceptable stand-in, 0xFF = maps to nothing. See the note in
/// BoneMap.cpp for the measurement that made this column necessary.
[[nodiscard]] uint8_t joint_name_rank(std::string_view raw);

/// The answer for a whole skeleton: joint index per rig bone, -1 = unbound.
struct SkeletonBinding {
    std::array<int32_t, BONE_COUNT> joint{};
    uint32_t bound_count = 0;

    SkeletonBinding() { joint.fill(-1); }
    [[nodiscard]] bool bound(Bone b) const { return joint[bone_index(b)] >= 0; }
};

[[nodiscard]] SkeletonBinding bind_skeleton(const skel::Skeleton& skeleton);

} // namespace dfn::anim
