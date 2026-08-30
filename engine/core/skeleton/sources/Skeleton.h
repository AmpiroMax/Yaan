/*
Module: engine/core
File: engine/core/skeleton/sources/Skeleton.h

Responsibility:
- The IMPORTED skeleton and its clips as plain data: joints (name, parent,
  bind TRS, inverse bind matrix) and keyframed channels. Pure structs plus the
  two pure functions everybody would otherwise re-write — local-TRS forward
  kinematics and clip sampling.

Key items:
- SkeletonJoint / Skeleton: the hierarchy an importer read out of a glTF file.
- AnimPath / AnimChannel / AnimClip: keyed translation/rotation/scale.
- skeleton_local_matrices(), skeleton_model_matrices(): FK.
- sample_clip(): clip at time t -> per-joint local TRS (bind where unkeyed).

Dependencies:
- Uses: glm (Rule 2) and the C++ standard library. Nothing else, by design.
- Used by: engine/render (.dfo SKEL/ANIM sections), engine/anim (retarget onto
  the humanoid Rig), engine/app (loading), tools/import_gltf.cpp, tests.

Notes:
- WHY THIS LIVES IN core AND NOT IN render OR anim. Three zones need the SAME
  skeleton: the file format writes it, the character zone retargets our poses
  onto it, and the app ferries it between them. render and anim are siblings
  in the DAG and cannot include each other, so a definition in either one
  means a SECOND definition in the other -- the two-copies defect (Rule 35)
  with a binary format on one side of it. core is the only place all three
  already look.
- Joints are stored PARENT-BEFORE-CHILD (the importer topologically sorts
  them), so FK is a single forward pass. A file that violates it is a corrupt
  file, not a slower one.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Plain data only (Rule 8): no handles, no ownership, no engine types.
*/

#pragma once

#include <cstdint>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dfn::skel {

/// How many bones one draw's palette may carry. Matches the uniform array the
/// skinned vertex program declares (engine/platform/render); a skeleton with
/// more joints than this is refused at import, loudly, rather than silently
/// dropping the joints past the limit.
inline constexpr uint32_t MAX_PALETTE_BONES = 64;

/// One joint of an imported skeleton.
struct SkeletonJoint {
    std::string name;                         ///< as authored (glTF node name)
    int32_t parent = -1;                      ///< -1 = root; always < own index
    glm::vec3 bind_translation{0.0f};         ///< local TRS of the BIND pose
    glm::quat bind_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 bind_scale{1.0f};
    /// Mesh space -> joint space at bind time (glTF's inverseBindMatrices).
    glm::mat4 inverse_bind{1.0f};
};

struct Skeleton {
    std::vector<SkeletonJoint> joints;

    [[nodiscard]] std::size_t size() const { return joints.size(); }
    [[nodiscard]] bool empty() const { return joints.empty(); }
    /// Index of the joint with this exact name, or -1.
    [[nodiscard]] int32_t find(std::string_view name) const {
        for (std::size_t i = 0; i < joints.size(); ++i) {
            if (joints[i].name == name) {
                return static_cast<int32_t>(i);
            }
        }
        return -1;
    }
};

enum class AnimPath : uint8_t {
    Translation = 0,
    Rotation = 1,
    Scale = 2,
};

/// One animated property of one joint. `values` holds xyz (+w for rotation);
/// `times` is strictly increasing and the same length.
struct AnimChannel {
    uint32_t joint = 0;
    AnimPath path = AnimPath::Rotation;
    std::vector<float> times;
    std::vector<glm::vec4> values;
};

struct AnimClip {
    std::string name;
    float duration_s = 0.0f;
    std::vector<AnimChannel> channels;
};

/// Bind-pose local matrices (T * R * S per joint).
void skeleton_bind_local(const Skeleton& skeleton, std::span<glm::mat4> out);

/// FK: local matrices -> model-space matrices. `local` and `out` must both be
/// at least skeleton.size(); aliasing them is allowed.
void skeleton_model_matrices(const Skeleton& skeleton, std::span<const glm::mat4> local,
                             std::span<glm::mat4> out);

/// Samples `clip` at `time_s` (LINEAR between keys, clamped at both ends,
/// quaternions slerped) into per-joint local TRS. Joints the clip does not
/// key keep their BIND values, which is what makes a partial clip legal.
void sample_clip(const Skeleton& skeleton, const AnimClip& clip, float time_s,
                 std::span<glm::vec3> translation, std::span<glm::quat> rotation,
                 std::span<glm::vec3> scale);

/// Convenience: sample_clip + compose + FK, in one call.
void sample_clip_model_matrices(const Skeleton& skeleton, const AnimClip& clip,
                                float time_s, std::span<glm::mat4> out);

} // namespace dfn::skel
