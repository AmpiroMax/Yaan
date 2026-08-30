/*
Module: engine/anim
File: engine/anim/sources/PoseLayers.cpp

Responsibility:
- Implements the branch mask, the masked blend, and the calibrated arm layer.

Dependencies:
- Uses: PoseLayers.h, SkinnedBody.h, core skeleton, glm.
- Used by: dfn_anim (ClipPlayer), engine/app, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Nothing here reads a clock, a file or the ECS: every function takes what it
  needs as a parameter, and the tests depend on that being true.
*/

#include "engine/anim/sources/PoseLayers.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dfn::anim {
namespace {

/// FK over a sampled pose. The same composition track_contacts uses, kept
/// short here because three functions below need it and none of them needs
/// the inverse binds sample_palette folds in.
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

/// The rotation part of a model matrix, with the scale divided out. A joint
/// whose bind carries a scale is normal in a bought asset, and quat_cast on a
/// scaled basis returns a quaternion that is not a rotation at all.
[[nodiscard]] glm::quat model_rotation(const glm::mat4& m) {
    glm::mat3 r{m};
    for (int c = 0; c < 3; ++c) {
        const float len = glm::length(r[c]);
        r[c] = len > 1.0e-8f ? r[c] / len : glm::vec3{c == 0, c == 1, c == 2};
    }
    return glm::normalize(glm::quat_cast(r));
}

/// Marks `j` and everything descending from it with `b`. The skeleton's
/// parents precede their children (the format's contract), so one forward
/// pass is enough and no recursion is needed.
void mark_subtree(const skel::Skeleton& skeleton, int32_t root, Branch b,
                  std::vector<uint8_t>& out) {
    if (root < 0 || static_cast<std::size_t>(root) >= out.size()) {
        return;
    }
    out[static_cast<std::size_t>(root)] = static_cast<uint8_t>(b);
    for (std::size_t j = 0; j < skeleton.size(); ++j) {
        const int32_t p = skeleton.joints[j].parent;
        if (p >= 0 && out[static_cast<std::size_t>(p)] == static_cast<uint8_t>(b)
            && static_cast<std::size_t>(p) < j) {
            out[j] = static_cast<uint8_t>(b);
        }
    }
}

/// Every joint descending from `root`, `root` excluded.
void collect_subtree(const skel::Skeleton& skeleton, int32_t root,
                     std::vector<int32_t>& out) {
    if (root < 0) {
        return;
    }
    std::vector<uint8_t> in(skeleton.size(), 0);
    in[static_cast<std::size_t>(root)] = 1;
    for (std::size_t j = 0; j < skeleton.size(); ++j) {
        const int32_t p = skeleton.joints[j].parent;
        if (p >= 0 && in[static_cast<std::size_t>(p)] != 0) {
            in[j] = 1;
            out.push_back(static_cast<int32_t>(j));
        }
    }
}

} // namespace

uint32_t BranchMask::count(Branch b) const {
    uint32_t n = 0;
    for (const uint8_t v : branch) {
        n += v == static_cast<uint8_t>(b) ? 1u : 0u;
    }
    return n;
}

BranchMask build_branch_mask(const skel::Skeleton& skeleton,
                             const SkinnedRigBinding& binding) {
    BranchMask mask;
    if (skeleton.empty()) {
        return mask;
    }
    mask.branch.assign(skeleton.size(), static_cast<uint8_t>(Branch::Root));
    // THE LEGS FIRST AND THE TORSO SECOND, and the order is load-bearing on a
    // hierarchy where a thigh hangs off the hips and the hips are also the
    // spine's parent: marking Lower first and Upper second lets the spine's
    // subtree win the joints it actually owns, while the legs keep theirs.
    mark_subtree(skeleton, binding.names.joint[bone_index(Bone::ThighL)], Branch::Lower,
                 mask.branch);
    mark_subtree(skeleton, binding.names.joint[bone_index(Bone::ThighR)], Branch::Lower,
                 mask.branch);
    mark_subtree(skeleton, binding.names.joint[bone_index(Bone::Torso)], Branch::Upper,
                 mask.branch);
    return mask;
}

void blend_masked(std::span<const JointLocal> lower, std::span<const JointLocal> upper,
                  const BranchMask& mask, float upper_weight,
                  std::span<JointLocal> out) {
    const float w = std::clamp(upper_weight, 0.0f, 1.0f);
    const std::size_t n =
        std::min({lower.size(), upper.size(), out.size(), mask.branch.size()});
    for (std::size_t j = 0; j < n; ++j) {
        if (!mask.is_upper(j) || w <= 0.0f) {
            out[j] = lower[j];
            continue;
        }
        JointLocal r;
        r.translation = glm::mix(lower[j].translation, upper[j].translation, w);
        r.scale = glm::mix(lower[j].scale, upper[j].scale, w);
        const glm::quat a = glm::normalize(lower[j].rotation);
        glm::quat b = glm::normalize(upper[j].rotation);
        if (glm::dot(a, b) < 0.0f) {
            b = -b;
        }
        r.rotation = glm::normalize(glm::slerp(a, b, w));
        out[j] = r;
    }
}

HandSpread measure_hand_spread(const skel::Skeleton& skeleton,
                               const SkinnedRigBinding& binding,
                               std::span<const JointLocal> sample) {
    HandSpread out;
    const int32_t pelvis = binding.names.joint[bone_index(Bone::Pelvis)];
    const int32_t hl = binding.names.joint[bone_index(Bone::HandL)];
    const int32_t hr = binding.names.joint[bone_index(Bone::HandR)];
    if (pelvis < 0 || hl < 0 || hr < 0) {
        return out;
    }
    std::vector<glm::mat4> local;
    std::vector<glm::mat4> model;
    model_matrices(skeleton, sample, local, model);
    const float cx = model[static_cast<std::size_t>(pelvis)][3][0];
    out.left = std::abs(model[static_cast<std::size_t>(hl)][3][0] - cx);
    out.right = std::abs(model[static_cast<std::size_t>(hr)][3][0] - cx);
    return out;
}

float measure_hand_openness(const skel::Skeleton& skeleton, const ArmRelax& relax,
                            std::span<const JointLocal> sample) {
    if (relax.finger.empty() || relax.hand[0] < 0) {
        return 0.0f;
    }
    std::vector<glm::mat4> local;
    std::vector<glm::mat4> model;
    model_matrices(skeleton, sample, local, model);
    // THE TIPS ONLY. A finger's middle joint barely moves between an open hand
    // and a fist; the tip moves the whole way, and averaging the two would
    // halve the number the layer is judged by for no reason.
    float sum = 0.0f;
    uint32_t n = 0;
    for (const int32_t j : relax.finger) {
        bool leaf = true;
        for (const skel::SkeletonJoint& other : skeleton.joints) {
            if (other.parent == j) {
                leaf = false;
                break;
            }
        }
        if (!leaf) {
            continue;
        }
        int32_t hand = relax.hand[0];
        // Walk up to whichever hand this tip hangs off, rather than guessing
        // by index: the two hands are not contiguous ranges on every asset.
        for (int32_t k = j; k >= 0; k = skeleton.joints[static_cast<std::size_t>(k)].parent) {
            if (k == relax.hand[0] || k == relax.hand[1]) {
                hand = k;
                break;
            }
        }
        sum += glm::length(glm::vec3{model[static_cast<std::size_t>(j)][3]}
                           - glm::vec3{model[static_cast<std::size_t>(hand)][3]});
        ++n;
    }
    return n > 0 ? sum / float(n) : 0.0f;
}

void apply_arm_relax(const skel::Skeleton& skeleton, const ArmRelax& relax, float weight,
                     std::span<JointLocal> sample) {
    const float w = std::clamp(weight, 0.0f, 1.0f);
    if (!relax.valid() || w <= 0.0f) {
        return; // bit-for-bit no-op: "weapon drawn" depends on it
    }
    std::vector<glm::mat4> local;
    std::vector<glm::mat4> model;
    model_matrices(skeleton, sample, local, model);
    // THE ADDUCTION, EXPRESSED IN THE PARENT'S FRAME. The layer's statement is
    // about MODEL space ("bring the arm toward the body's midline"), and the
    // thing that has to be written is a LOCAL rotation, so the model-space
    // turn is carried into the parent's frame once per side per frame. It is
    // PRE-multiplied, so the clip's own arm swing survives underneath.
    for (int s = 0; s < 2; ++s) {
        const int32_t j = relax.upper_arm[static_cast<std::size_t>(s)];
        const int32_t p = relax.parent[static_cast<std::size_t>(s)];
        if (j < 0 || static_cast<std::size_t>(j) >= sample.size()) {
            continue;
        }
        const glm::quat parent_rot =
            p >= 0 ? model_rotation(model[static_cast<std::size_t>(p)])
                   : glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
        // +Z is the character's BACK (it faces -Z), so a turn about +Z moves a
        // hanging arm toward +X = its right. The left arm therefore comes in
        // on +angle and the right on -angle.
        const float sign = s == 0 ? 1.0f : -1.0f;
        const glm::quat turn =
            glm::angleAxis(sign * relax.angle_rad * w, glm::vec3{0.0f, 0.0f, 1.0f});
        const glm::quat in_parent = glm::conjugate(parent_rot) * turn * parent_rot;
        JointLocal& jl = sample[static_cast<std::size_t>(j)];
        jl.rotation = glm::normalize(in_parent * glm::normalize(jl.rotation));
    }
    // THE FINGERS GO BACK TOWARD THEIR BIND, which on this asset is an open
    // hand: the fist is KEYED, in every clip, including the idle. Relaxing
    // toward the bind rather than toward an authored open pose is the same
    // choice the branch mask makes — the bind is the one pose every asset has.
    for (const int32_t j : relax.finger) {
        if (j < 0 || static_cast<std::size_t>(j) >= sample.size()) {
            continue;
        }
        const glm::quat bind =
            glm::normalize(skeleton.joints[static_cast<std::size_t>(j)].bind_rotation);
        glm::quat cur = glm::normalize(sample[static_cast<std::size_t>(j)].rotation);
        if (glm::dot(cur, bind) < 0.0f) {
            cur = -cur;
        }
        sample[static_cast<std::size_t>(j)].rotation = glm::normalize(glm::slerp(cur, bind, w));
    }
}

ArmRelax calibrate_arm_relax(const Rig& rig, const skel::Skeleton& skeleton,
                             const SkinnedRigBinding& binding,
                             std::span<const JointLocal> reference) {
    ArmRelax relax;
    relax.pelvis = binding.names.joint[bone_index(Bone::Pelvis)];
    relax.upper_arm[0] = binding.names.joint[bone_index(Bone::UpperArmL)];
    relax.upper_arm[1] = binding.names.joint[bone_index(Bone::UpperArmR)];
    relax.hand[0] = binding.names.joint[bone_index(Bone::HandL)];
    relax.hand[1] = binding.names.joint[bone_index(Bone::HandR)];
    for (int s = 0; s < 2; ++s) {
        const int32_t j = relax.upper_arm[static_cast<std::size_t>(s)];
        relax.parent[static_cast<std::size_t>(s)] =
            j >= 0 ? skeleton.joints[static_cast<std::size_t>(j)].parent : -1;
        collect_subtree(skeleton, relax.hand[static_cast<std::size_t>(s)], relax.finger);
    }
    if (!relax.valid() || reference.size() < skeleton.size()) {
        return relax;
    }
    // THE TARGET IS OUR REST POSE, READ THROUGH THE RETARGET. Not a row and
    // not the order's "10 to 12 degrees": the sentence being enforced is "the
    // hand hangs where a person's hand hangs", and the only place this project
    // has ever written that down is the rest pose the box body stands in.
    std::vector<JointLocal> rest(skeleton.size());
    pose_local_transforms(rig, skeleton, binding, LocalPose{}, rest);
    const HandSpread rest_spread = measure_hand_spread(skeleton, binding, rest);
    relax.target_m = 0.5f * (rest_spread.left + rest_spread.right);
    const HandSpread ref_spread = measure_hand_spread(skeleton, binding, reference);
    relax.reference_m = 0.5f * (ref_spread.left + ref_spread.right);

    // A SCAN AND NOT A FORMULA, and not a bisection either. The angle whose
    // hand lands on the target is the root of a function nobody has in closed
    // form (the arm is three joints and the clip has already bent two of
    // them), and a SIGNED scan also answers the question a bisection would
    // have to be told: which way is inward on this skeleton. Load-time cost is
    // 65 forward-kinematics passes, once per model.
    constexpr int STEPS = 64;
    constexpr float SPAN_RAD = 0.9f;
    std::vector<JointLocal> probe(skeleton.size());
    float best_err = std::numeric_limits<float>::max();
    float best_angle = 0.0f;
    for (int i = -STEPS; i <= STEPS; ++i) {
        const float angle = SPAN_RAD * float(i) / float(STEPS);
        std::copy(reference.begin(), reference.begin() + std::ptrdiff_t(skeleton.size()),
                  probe.begin());
        ArmRelax trial = relax;
        trial.angle_rad = angle;
        trial.finger.clear(); // the fingers do not move the wrist
        apply_arm_relax(skeleton, trial, 1.0f, probe);
        const HandSpread got = measure_hand_spread(skeleton, binding, probe);
        const float err = std::abs(0.5f * (got.left + got.right) - relax.target_m);
        if (err < best_err) {
            best_err = err;
            best_angle = angle;
        }
    }
    relax.angle_rad = best_angle;
    return relax;
}

} // namespace dfn::anim
