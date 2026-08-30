/*
Module: engine/anim
File: engine/anim/sources/SkinnedBody.cpp

Responsibility:
- Implements the retarget: the per-bone change of basis, the palette build and
  the CPU skinning reference.

Dependencies:
- Uses: SkinnedBody.h, Pose.h (forward_kinematics), glm.
- Used by: dfn_anim.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Nothing here may read the clock, the ECS or a config row: the palette is a
  pure function of (rig, skeleton, binding, pose), and every test depends on
  that being true.
*/

#include "engine/anim/sources/SkinnedBody.h"

#include <algorithm>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dfn::anim {
namespace {

/// The rotation part of a matrix as a quaternion, with scale divided out.
/// Bind matrices legitimately carry scale (a model authored in centimetres),
/// and quat_cast on a scaled matrix returns nonsense.
/// The shortest rotation taking unit `from` to unit `to`. Written out rather
/// than pulled from glm's GTX (an experimental extension the build refuses on
/// principle): it is six lines, and the antiparallel case -- which is exactly
/// the case that arises when two rigs disagree about which way a bone points --
/// needs a deliberate axis choice rather than whatever a library picks.
[[nodiscard]] glm::quat rotation_between(const glm::vec3& from, const glm::vec3& to) {
    const glm::vec3 a = glm::normalize(from);
    const glm::vec3 b = glm::normalize(to);
    const float d = glm::dot(a, b);
    if (d > 0.999999f) {
        return glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
    }
    if (d < -0.999999f) {
        // Antiparallel: any perpendicular axis is a valid half turn. Pick the
        // one furthest from `a` so the cross product is well conditioned.
        glm::vec3 axis = glm::cross(glm::vec3{1.0f, 0.0f, 0.0f}, a);
        if (glm::length(axis) < 1e-4f) {
            axis = glm::cross(glm::vec3{0.0f, 1.0f, 0.0f}, a);
        }
        return glm::normalize(glm::angleAxis(glm::pi<float>(), glm::normalize(axis)));
    }
    const glm::vec3 axis = glm::cross(a, b);
    return glm::normalize(glm::quat{1.0f + d, axis.x, axis.y, axis.z});
}

[[nodiscard]] glm::quat rotation_of(const glm::mat4& m) {
    glm::mat3 r{m};
    for (int c = 0; c < 3; ++c) {
        const float len = glm::length(r[c]);
        r[c] = len > 1e-8f ? r[c] / len : glm::vec3{c == 0, c == 1, c == 2};
    }
    return glm::normalize(glm::quat_cast(r));
}

} // namespace

namespace {

/// Which bone's joint gives each bone its DIRECTION. A bone points at its
/// child; where our hierarchy branches (the pelvis carries the torso and both
/// thighs, the torso carries the head and both arms) the SPINE child is the
/// one that defines the parent's direction, because that is the bone whose
/// axis a human reads as "which way is this part pointing".
struct DirectionChild {
    Bone bone;
    Bone child;
};
constexpr DirectionChild DIRECTION_CHILD[] = {
    {Bone::Pelvis, Bone::Torso},        {Bone::Torso, Bone::Head},
    {Bone::UpperArmL, Bone::ForearmL},  {Bone::ForearmL, Bone::HandL},
    {Bone::UpperArmR, Bone::ForearmR},  {Bone::ForearmR, Bone::HandR},
    {Bone::ThighL, Bone::ShinL},        {Bone::ShinL, Bone::FootL},
    {Bone::ThighR, Bone::ShinR},        {Bone::ShinR, Bone::FootR},
};

} // namespace

SkinnedRigBinding bind_skinned_rig(const Rig& rig, const skel::Skeleton& skeleton) {
    SkinnedRigBinding binding;
    binding.names = bind_skeleton(skeleton);

    // OUR rest frames: FK on an identity pose at the origin. The rest pose is
    // the pose every LocalPose is a delta FROM, so this is the only frame the
    // correction may be measured against.
    std::array<glm::mat4, BONE_COUNT> rest{};
    forward_kinematics(rig, LocalPose{}, BodyRoot{}, rest);

    // THEIRS: the imported bind pose, model space.
    const std::size_t n = skeleton.size();
    std::vector<glm::mat4> bind_local(n);
    std::vector<glm::mat4> bind_model(n);
    skel::skeleton_bind_local(skeleton, bind_local);
    skel::skeleton_model_matrices(skeleton, bind_local, bind_model);

    std::vector<int32_t> bone_of(n, -1);
    for (uint32_t b = 0; b < BONE_COUNT; ++b) {
        const int32_t j = binding.names.joint[b];
        if (j >= 0 && static_cast<std::size_t>(j) < n) {
            bone_of[static_cast<std::size_t>(j)] = static_cast<int32_t>(b);
        }
    }
    // Which bone each bone points at, as a lookup.
    std::array<int32_t, BONE_COUNT> dir_child{};
    dir_child.fill(-1);
    for (const DirectionChild& d : DIRECTION_CHILD) {
        dir_child[bone_index(d.bone)] = static_cast<int32_t>(bone_index(d.child));
    }

    // ONE FORWARD PASS over the joints. Joints are parent-before-child, so a
    // joint's parent's CORRECTED model rotation is final when the child is
    // reached -- which is why the corrections compose instead of each undoing
    // the last.
    std::vector<glm::quat> corrected(n, glm::quat{1.0f, 0.0f, 0.0f, 0.0f});
    for (std::size_t j = 0; j < n; ++j) {
        const int32_t parent = skeleton.joints[j].parent;
        const glm::quat parent_rot =
            parent >= 0 ? corrected[static_cast<std::size_t>(parent)]
                        : glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
        const glm::quat bind_rot = glm::normalize(rotation_of(bind_local[j]));
        const glm::quat pre = glm::normalize(parent_rot * bind_rot);
        corrected[j] = pre;
        const int32_t b = bone_of[j];
        if (b < 0) {
            continue;
        }
        const int32_t cb = dir_child[static_cast<std::size_t>(b)];
        const int32_t cj = cb >= 0 ? binding.names.joint[static_cast<std::size_t>(cb)]
                                   : -1;
        if (cb < 0 || cj < 0) {
            continue; // a leaf of our rig: head, hands, feet. No direction to fix.
        }
        // THE DIRECTION, NOT THE FRAME, IS WHAT IS MATCHED -- and the difference
        // is a body standing on its head. Setting the joint's model ROTATION to
        // ours assumes the two rigs agree about which local axis runs along the
        // bone; Rigify's runs +Y, ours does not, and the first version of this
        // function drew the reference base with its legs pointing at the sky
        // (measured: ankle at 0.637 of the figure, hip at 0.039).
        const glm::vec3 bind_dir =
            glm::vec3{bind_model[static_cast<std::size_t>(cj)][3]}
            - glm::vec3{bind_model[j][3]};
        const glm::vec3 ours_dir =
            glm::vec3{rest[static_cast<std::size_t>(cb)][3]}
            - glm::vec3{rest[static_cast<std::size_t>(b)][3]};
        if (glm::length(bind_dir) < 1e-6f || glm::length(ours_dir) < 1e-6f) {
            continue;
        }
        // The child's direction as it stands AFTER the ancestors' corrections:
        // the bind direction carried by this joint's accumulated correction.
        const glm::quat accumulated = glm::normalize(
            pre * glm::inverse(glm::normalize(rotation_of(bind_model[j]))));
        const glm::vec3 current = glm::normalize(accumulated * bind_dir);
        const glm::quat turn = rotation_between(current, ours_dir);
        corrected[j] = glm::normalize(turn * pre);
        binding.rest_delta[static_cast<std::size_t>(b)] =
            glm::normalize(glm::inverse(pre) * turn * pre);
    }

    // HOW TALL THE MODEL STANDS, measured from the bind pose's joint positions
    // rather than typed in: a number written beside a model is the first thing
    // to go stale when the model is re-imported (the argument measure_object
    // already makes for object extents).
    float lo = 0.0f;
    float hi = 0.0f;
    for (std::size_t i = 0; i < bind_model.size(); ++i) {
        const float y = bind_model[i][3][1];
        lo = i == 0 ? y : std::min(lo, y);
        hi = i == 0 ? y : std::max(hi, y);
    }
    binding.model_height_m = hi - lo;
    return binding;
}

void pose_local_transforms(const Rig& rig, const skel::Skeleton& skeleton,
                           const SkinnedRigBinding& binding, const LocalPose& pose,
                           std::span<JointLocal> out) {
    const std::size_t n = std::min(skeleton.size(), out.size());
    if (n == 0) {
        return;
    }
    // HINGES ARE HINGES ON THE IMPORTED MODEL TOO. The limits live on the rig,
    // not on the mesh, so a bought character cannot bend its knee backwards
    // any more than the boxes could -- the whole point of putting the clamp in
    // pose EVALUATION rather than in clip authoring (Pose.h).
    LocalPose clamped = pose;
    apply_joint_limits(rig, clamped);

    for (std::size_t j = 0; j < n; ++j) {
        out[j].translation = skeleton.joints[j].bind_translation;
        out[j].rotation = skeleton.joints[j].bind_rotation;
        out[j].scale = skeleton.joints[j].bind_scale;
    }

    for (uint32_t b = 0; b < BONE_COUNT; ++b) {
        const int32_t ji = binding.names.joint[b];
        if (ji < 0 || static_cast<std::size_t>(ji) >= n) {
            continue;
        }
        const auto j = static_cast<std::size_t>(ji);
        const skel::SkeletonJoint& joint = skeleton.joints[j];
        // The rest delta derived in the header, then the pose's own rotation
        // in the bone's own frame -- in that order, and the order is the whole
        // statement: first carry the joint into OUR rest, then bend it.
        out[j].rotation = glm::normalize(joint.bind_rotation
                                         * binding.rest_delta[b]
                                         * clamped.rotation[b]);
    }
    // THE PELVIS OFFSET IS A TRANSLATION OF THE WHOLE BODY, not a rotation, so
    // it rides on the bound pelvis joint's own local translation. Without it
    // the bob and sway of the walk cycle -- the part a viewer actually reads
    // as "he is walking" -- would be silently dropped by the retarget.
    const int32_t pelvis = binding.names.joint[bone_index(Bone::Pelvis)];
    if (pelvis >= 0 && static_cast<std::size_t>(pelvis) < n) {
        const auto p = static_cast<std::size_t>(pelvis);
        out[p].translation += clamped.pelvis_offset;
    }
}

void sample_palette(const skel::Skeleton& skeleton, std::span<const JointLocal> local,
                    std::span<glm::mat4> out) {
    const std::size_t n = std::min({skeleton.size(), local.size(), out.size()});
    if (n == 0) {
        return;
    }
    std::vector<glm::mat4> mats(skeleton.size());
    for (std::size_t j = 0; j < n; ++j) {
        mats[j] = glm::translate(glm::mat4{1.0f}, local[j].translation)
                  * glm::mat4_cast(glm::normalize(local[j].rotation))
                  * glm::scale(glm::mat4{1.0f}, local[j].scale);
    }
    skel::skeleton_model_matrices(skeleton, mats, mats);
    for (std::size_t j = 0; j < n; ++j) {
        out[j] = mats[j] * skeleton.joints[j].inverse_bind;
    }
}

void rest_model_matrices(const Rig& rig, const skel::Skeleton& skeleton,
                         const SkinnedRigBinding& binding, const LocalPose& pose,
                         std::span<glm::mat4> out) {
    const std::size_t n = std::min(skeleton.size(), out.size());
    if (n == 0) {
        return;
    }
    std::vector<JointLocal> local(skeleton.size());
    pose_local_transforms(rig, skeleton, binding, pose, local);
    std::vector<glm::mat4> mats(skeleton.size());
    for (std::size_t j = 0; j < skeleton.size(); ++j) {
        mats[j] = glm::translate(glm::mat4{1.0f}, local[j].translation)
                  * glm::mat4_cast(glm::normalize(local[j].rotation))
                  * glm::scale(glm::mat4{1.0f}, local[j].scale);
    }
    skel::skeleton_model_matrices(skeleton, mats, out);
}

void skinning_palette(const Rig& rig, const skel::Skeleton& skeleton,
                      const SkinnedRigBinding& binding, const LocalPose& pose,
                      std::span<glm::mat4> out) {
    const std::size_t n = std::min(skeleton.size(), out.size());
    if (n == 0) {
        return;
    }
    std::vector<glm::mat4> model(skeleton.size());
    rest_model_matrices(rig, skeleton, binding, pose, model);
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = model[i] * skeleton.joints[i].inverse_bind;
    }
}

glm::vec3 cpu_skin_position(const platform::SkinnedVertex& v,
                            std::span<const glm::mat4> palette) {
    glm::vec3 acc{0.0f};
    const glm::vec4 p{v.position, 1.0f};
    for (int k = 0; k < 4; ++k) {
        const std::size_t j = v.joints[k];
        if (v.weights[k] == 0.0f || j >= palette.size()) {
            continue;
        }
        acc += glm::vec3{palette[j] * p} * v.weights[k];
    }
    return acc;
}

} // namespace dfn::anim
