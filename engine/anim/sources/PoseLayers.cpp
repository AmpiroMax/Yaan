/*
Module: engine/anim
File: engine/anim/sources/PoseLayers.cpp

Responsibility:
- Implements the branch mask, the masked blend, and the calibrated arm layer.

Dependencies:
- Uses: PoseLayers.h, Stance.h (the pose measurement), SkinnedBody.h, core
  skeleton, generated constants (STANCE_*), glm.
- Used by: dfn_anim (ClipPlayer), engine/app, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Nothing here reads a clock, a file or the ECS: every function takes what it
  needs as a parameter, and the tests depend on that being true.
*/

#include "engine/anim/sources/PoseLayers.h"

#include "engine/anim/sources/Stance.h"
#include "engine/core/config/sources/Constants.h"

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

void apply_arm_relax(const skel::Skeleton& skeleton, const ArmRelax& relax,
                     const ArmRelaxDose& dose, std::span<JointLocal> sample) {
    const float w = std::clamp(dose.arm, 0.0f, 1.0f);
    const float offset = dose.elbow_offset_rad;
    const float wf = std::clamp(dose.finger, 0.0f, 1.0f);
    if (!relax.valid()
        || (w <= 0.0f && std::abs(offset) < 1.0e-5f && wf <= 0.0f)) {
        return; // bit-for-bit no-op: a zero-dose control arm depends on it
    }
    std::vector<glm::mat4> local;
    std::vector<glm::mat4> model;
    model_matrices(skeleton, sample, local, model);
    // THE ELBOW FIRST, because unfolding it moves the hand the shoulder is
    // then carried by, and the shoulder's two angles were solved on a pose
    // whose elbow had already been unfolded.
    if (std::abs(offset) > 1.0e-5f) {
        for (int s = 0; s < 2; ++s) {
            const auto k = static_cast<std::size_t>(s);
            const int32_t j = relax.forearm[k];
            const int32_t up = relax.upper_arm[k];
            const int32_t hand = relax.hand[k];
            if (j < 0 || up < 0 || hand < 0
                || static_cast<std::size_t>(j) >= sample.size()) {
                continue;
            }
            // THE AXIS IS THE ONE THE ARM IS ACTUALLY BENT ABOUT this frame,
            // not a stored one: an arm that has swung forward folds in a
            // different plane, and a stored axis would twist it instead.
            const glm::vec3 a{model[static_cast<std::size_t>(j)][3]};
            const glm::vec3 root{model[static_cast<std::size_t>(up)][3]};
            const glm::vec3 tip{model[static_cast<std::size_t>(hand)][3]};
            glm::vec3 axis{1.0f, 0.0f, 0.0f};
            const glm::vec3 v1 = a - root;
            const glm::vec3 v2 = tip - a;
            const glm::vec3 c = glm::cross(v1, v2);
            if (glm::length(c) > 1.0e-5f) {
                axis = glm::normalize(c);
            }
            const glm::quat parent_rot = model_rotation(model[static_cast<std::size_t>(
                skeleton.joints[static_cast<std::size_t>(j)].parent >= 0
                    ? skeleton.joints[static_cast<std::size_t>(j)].parent
                    : j)]);
            const glm::quat turn = glm::angleAxis(offset, axis);
            const glm::quat in_parent = glm::conjugate(parent_rot) * turn * parent_rot;
            JointLocal& jl = sample[static_cast<std::size_t>(j)];
            jl.rotation = glm::normalize(in_parent * glm::normalize(jl.rotation));
        }
        model_matrices(skeleton, sample, local, model);
    }
    // THE ADDUCTION, EXPRESSED IN THE PARENT'S FRAME. The layer's statement is
    // about MODEL space ("bring the arm toward the body's midline"), and the
    // thing that has to be written is a LOCAL rotation, so the model-space
    // turn is carried into the parent's frame once per side per frame. It is
    // PRE-multiplied, so the clip's own arm swing survives underneath.
    for (int s = 0; s < 2 && w > 0.0f; ++s) {
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
        // AND THE LIFT IS ABOUT +X, THE SAME AXIS AND SIGN THE TRUNK'S LEAN
        // USES (Stance.cpp): the character faces -Z, so carrying a limb
        // FORWARD by `a` is a turn of `-a`. The angle is signed by the solve,
        // so a model whose clip holds the arms too LOW gets it back.
        const glm::quat turn =
            glm::angleAxis(sign * relax.angle_rad * w, glm::vec3{0.0f, 0.0f, 1.0f})
            * glm::angleAxis(-relax.lift_rad * w, glm::vec3{1.0f, 0.0f, 0.0f});
        const glm::quat in_parent = glm::conjugate(parent_rot) * turn * parent_rot;
        JointLocal& jl = sample[static_cast<std::size_t>(j)];
        jl.rotation = glm::normalize(in_parent * glm::normalize(jl.rotation));
    }
    // THE FINGERS GO BACK TOWARD THEIR BIND, which on this asset is an open
    // hand: the fist is KEYED, in every clip, including the idle. Relaxing
    // toward the bind rather than toward an authored open pose is the same
    // choice the branch mask makes — the bind is the one pose every asset has.
    for (const int32_t j : relax.finger) {
        if (wf <= 0.0f || j < 0 || static_cast<std::size_t>(j) >= sample.size()) {
            continue;
        }
        const glm::quat bind =
            glm::normalize(skeleton.joints[static_cast<std::size_t>(j)].bind_rotation);
        glm::quat cur = glm::normalize(sample[static_cast<std::size_t>(j)].rotation);
        if (glm::dot(cur, bind) < 0.0f) {
            cur = -cur;
        }
        sample[static_cast<std::size_t>(j)].rotation =
            glm::normalize(glm::slerp(cur, bind, wf));
    }
}

/// СКОЛЬКО РАЗ СЛОЙ ПЕРЕСПРАШИВАЕТ. Отвод плеча увозит и таз (он висит на том
/// же корне), поэтому одна поправка в цель не попадает; ЗАМЕРЕНО на этом
/// ассете: за один заход клиренс ходьбы доходит до 2.5 см при цели 3.5, за три
/// — до цели, четвёртый не меняет ничего.
constexpr int CLEARANCE_PASSES = 3;

ArmClearance build_arm_clearance(const skel::Skeleton& skeleton,
                                 const SkinnedRigBinding& binding) {
    ArmClearance a;
    a.upper_arm[0] = binding.names.joint[bone_index(Bone::UpperArmL)];
    a.upper_arm[1] = binding.names.joint[bone_index(Bone::UpperArmR)];
    a.forearm[0] = binding.names.joint[bone_index(Bone::ForearmL)];
    a.forearm[1] = binding.names.joint[bone_index(Bone::ForearmR)];
    a.hand[0] = binding.names.joint[bone_index(Bone::HandL)];
    a.hand[1] = binding.names.joint[bone_index(Bone::HandR)];
    for (int s = 0; s < 2; ++s) {
        const auto k = static_cast<std::size_t>(s);
        const int32_t j = a.upper_arm[k];
        a.parent[k] = j >= 0 ? skeleton.joints[static_cast<std::size_t>(j)].parent : -1;
        if (j < 0 || a.hand[k] < 0) {
            continue;
        }
        // ПЛЕЧО ЗАМЕРЕНО ПО БИНДУ, а не по позе кадра: это ДЛИНА РЫЧАГА,
        // свойство модели, и считать её каждый кадр значило бы делать её
        // функцией того, насколько рука согнута.
        const glm::vec3 sh{glm::inverse(
            skeleton.joints[static_cast<std::size_t>(j)].inverse_bind)[3]};
        const glm::vec3 hd{glm::inverse(
            skeleton.joints[static_cast<std::size_t>(a.hand[k])].inverse_bind)[3]};
        a.arm_len[k] = glm::length(hd - sh);
    }
    return a;
}

namespace {

/// Ближайшая из трёх форм, которые рука обходит: таз и оба бедра. ИМЕННО ТРИ,
/// и это то, что назвал заказ («ягодицы/бедро»): грудь рука на ходу не задевает
/// (её обходит собственная длина плеча), а голень к ней не поднимается.
[[nodiscard]] float body_gap(const HitboxSet& boxes, const HitboxPose& posed,
                             const glm::vec3& p) {
    return std::min({hitbox_distance(boxes, posed, BodyPart::Hips, p),
                     hitbox_distance(boxes, posed, BodyPart::ThighL, p),
                     hitbox_distance(boxes, posed, BodyPart::ThighR, p)});
}

} // namespace

ArmBodyGap measure_arm_body_gap(const skel::Skeleton& skeleton, const ArmClearance& arms,
                                const HitboxSet& boxes,
                                const SkinnedRigBinding& binding,
                                std::span<const JointLocal> sample) {
    ArmBodyGap out;
    for (int s = 0; s < 2; ++s) {
        const auto k = static_cast<std::size_t>(s);
        out.hand[k] = std::numeric_limits<float>::infinity();
        out.forearm[k] = std::numeric_limits<float>::infinity();
    }
    if (!arms.valid() || sample.size() < skeleton.size()) {
        return out;
    }
    const HitboxPose posed = hitbox_pose(boxes, skeleton, binding, sample);
    std::vector<glm::mat4> local;
    std::vector<glm::mat4> model;
    model_matrices(skeleton, sample, local, model);
    for (int s = 0; s < 2; ++s) {
        const auto k = static_cast<std::size_t>(s);
        out.hand[k] = body_gap(
            boxes, posed,
            glm::vec3{model[static_cast<std::size_t>(arms.hand[k])][3]});
        if (arms.forearm[k] >= 0) {
            out.forearm[k] = body_gap(
                boxes, posed,
                glm::vec3{model[static_cast<std::size_t>(arms.forearm[k])][3]});
        }
    }
    return out;
}

void apply_arm_clearance(const skel::Skeleton& skeleton, const ArmClearance& arms,
                         const HitboxSet& boxes, const SkinnedRigBinding& binding,
                         float want_m, float dose, std::span<JointLocal> sample) {
    const float w = std::clamp(dose, 0.0f, 1.0f);
    if (!arms.valid() || w <= 0.0f || want_m <= 0.0f
        || sample.size() < skeleton.size()) {
        return;
    }
    // ТРИ ЗАХОДА, И ВТОРОЙ С ТРЕТЬИМ — НЕ ПЕРЕСТРАХОВКА. Отвод плеча УВОЗИТ И ТАЗ (он
    // висит на том же корне), поэтому первая поправка попадает в цель не
    // точно; замерено на этом ассете, второй заход снимает остаток до долей
    // миллиметра, третий не меняет ничего.
    for (int pass = 0; pass < CLEARANCE_PASSES; ++pass) {
        const HitboxPose posed = hitbox_pose(boxes, skeleton, binding, sample);
        std::vector<glm::mat4> local;
        std::vector<glm::mat4> model;
        model_matrices(skeleton, sample, local, model);
        bool touched = false;
        for (int s = 0; s < 2; ++s) {
            const auto k = static_cast<std::size_t>(s);
            const int32_t j = arms.upper_arm[k];
            if (j < 0 || arms.arm_len[k] <= 1.0e-4f) {
                continue;
            }
            float gap = body_gap(
                boxes, posed,
                glm::vec3{model[static_cast<std::size_t>(arms.hand[k])][3]});
            if (arms.forearm[k] >= 0) {
                gap = std::min(gap,
                               body_gap(boxes, posed,
                                        glm::vec3{model[static_cast<std::size_t>(
                                            arms.forearm[k])][3]}));
            }
            const float need = want_m - gap;
            if (!(need > 0.0f)) {
                continue; // клиренс есть: рука не трогается ВООБЩЕ
            }
            // НЕДОСТАЮЩАЯ ДЛИНА ЧЕРЕЗ ДЛИНУ РЫЧАГА — И ЭТО УГОЛ. Малый угол:
            // рука не отводится на радианы, ей не хватает сантиметров.
            const float angle = w * std::atan2(need, arms.arm_len[k]);
            const int32_t p = arms.parent[k];
            const glm::quat parent_rot =
                p >= 0 ? model_rotation(model[static_cast<std::size_t>(p)])
                       : glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
            // ТА ЖЕ ОСЬ И ТОТ ЖЕ ЗНАК, ЧТО У ПРИВЕДЕНИЯ, только наоборот:
            // приведение ведёт руку ВНУТРЬ на +angle слева, значит наружу —
            // на -angle. Один и тот же поворот, прочитанный в обе стороны,
            // а не второе описание того, где у тела бок.
            const float sign = s == 0 ? -1.0f : 1.0f;
            const glm::quat turn =
                glm::angleAxis(sign * angle, glm::vec3{0.0f, 0.0f, 1.0f});
            const glm::quat in_parent = glm::conjugate(parent_rot) * turn * parent_rot;
            JointLocal& jl = sample[static_cast<std::size_t>(j)];
            jl.rotation = glm::normalize(in_parent * glm::normalize(jl.rotation));
            touched = true;
        }
        if (!touched) {
            return; // побитовое тождество, на нём стоит контрольная рука
        }
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
    relax.forearm[0] = binding.names.joint[bone_index(Bone::ForearmL)];
    relax.forearm[1] = binding.names.joint[bone_index(Bone::ForearmR)];
    if (!relax.valid() || reference.size() < skeleton.size()) {
        return relax;
    }
    // THE SIDEWAYS TARGET IS OUR REST POSE, READ THROUGH THE RETARGET. Not a
    // row and not the order's "10 to 12 degrees": the sentence being enforced
    // is "the hand hangs where a person's hand hangs", and the only place this
    // project has ever written that down is the rest pose the box body stands
    // in.
    std::vector<JointLocal> rest(skeleton.size());
    pose_local_transforms(rig, skeleton, binding, LocalPose{}, rest);
    const HandSpread rest_spread = measure_hand_spread(skeleton, binding, rest);
    relax.target_m = 0.5f * (rest_spread.left + rest_spread.right);
    // THE HEIGHT TARGET IS A ROW AND NOT THE REST POSE, and the difference is
    // the point. Our rest pose hangs both arms dead vertical from the shoulder
    // — a mannequin, not a stance — so its hand height is an artefact of a
    // pose nobody stands in, while "the hands sit at mid-thigh" is something
    // the reference frames actually show (STANCE_HAND_DROP). The sideways
    // number survives that objection because a vertical arm's SIDEWAYS
    // position is exactly the proportion being copied.
    relax.target_drop_m = static_cast<float>(config::STANCE_HAND_DROP);

    const StanceMetrics ref = measure_stance(skeleton, binding, reference);
    relax.reference_m = 0.5f * (ref.hand_spread_m[0] + ref.hand_spread_m[1]);
    relax.reference_drop_m = 0.5f * (ref.hand_drop_m[0] + ref.hand_drop_m[1]);
    relax.reference_elbow_rad = 0.5f * (ref.elbow_rad[0] + ref.elbow_rad[1]);
    // THE ELBOW IS AN ARITHMETIC SOLVE and needs no scan: flexion is measured
    // directly and the layer adds to it, so the offset is the difference. One
    // number for both sides, for the reason the adduction is one number.
    relax.elbow_stand_rad =
        static_cast<float>(config::STANCE_ELBOW_STAND) - relax.reference_elbow_rad;
    relax.elbow_run_rad =
        static_cast<float>(config::STANCE_ELBOW_RUN) - relax.reference_elbow_rad;

    // A SCAN AND NOT A FORMULA, and not a bisection either. The angle whose
    // hand lands on the target is the root of a function nobody has in closed
    // form (the arm is three joints and the clip has already bent two of
    // them), and a SIGNED scan also answers the question a bisection would
    // have to be told: which way is inward on this skeleton.
    //
    // AND TWO SCANS TAKING TURNS, because there are two targets and they
    // interact: adducting the arm changes its height a little, lifting it
    // changes its spread a little. Three rounds is enough for both residuals
    // to stop moving on this asset; a joint 2-D scan would cost 65x65 passes
    // to answer the same question no better.
    constexpr int STEPS = 64;
    constexpr float SPAN_RAD = 0.9f;
    constexpr float LIFT_SPAN_RAD = 1.2f;
    constexpr int ROUNDS = 3;
    std::vector<JointLocal> probe(skeleton.size());
    const auto trial_metrics = [&](float angle, float lift) {
        std::copy(reference.begin(), reference.begin() + std::ptrdiff_t(skeleton.size()),
                  probe.begin());
        ArmRelax t = relax;
        t.angle_rad = angle;
        t.lift_rad = lift;
        t.finger.clear(); // the fingers do not move the wrist
        apply_arm_relax(skeleton, t,
                        ArmRelaxDose{1.0f, relax.elbow_stand_rad, 0.0f}, probe);
        return measure_stance(skeleton, binding, probe);
    };
    float angle = 0.0f;
    float lift = 0.0f;
    for (int round = 0; round < ROUNDS; ++round) {
        float best = std::numeric_limits<float>::max();
        float best_angle = angle;
        for (int i = -STEPS; i <= STEPS; ++i) {
            const float a = SPAN_RAD * float(i) / float(STEPS);
            const StanceMetrics got = trial_metrics(a, lift);
            const float err = std::abs(0.5f * (got.hand_spread_m[0] + got.hand_spread_m[1])
                                       - relax.target_m);
            if (err < best) {
                best = err;
                best_angle = a;
            }
        }
        angle = best_angle;
        best = std::numeric_limits<float>::max();
        float best_lift = lift;
        for (int i = -STEPS; i <= STEPS; ++i) {
            const float l = LIFT_SPAN_RAD * float(i) / float(STEPS);
            const StanceMetrics got = trial_metrics(angle, l);
            const float err = std::abs(0.5f * (got.hand_drop_m[0] + got.hand_drop_m[1])
                                       - relax.target_drop_m);
            if (err < best) {
                best = err;
                best_lift = l;
            }
        }
        lift = best_lift;
    }
    relax.angle_rad = angle;
    relax.lift_rad = lift;
    return relax;
}

} // namespace dfn::anim
