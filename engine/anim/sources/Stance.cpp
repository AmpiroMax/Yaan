/*
Module: engine/anim
File: engine/anim/sources/Stance.cpp

Responsibility:
- Implements the stance measurement and the corrective stance layer.

Dependencies:
- Uses: Stance.h, Clips.h (HEAD_STABILIZE), SkinnedBody.h, core skeleton, glm,
  generated constants (STANCE_*).
- Used by: dfn_anim (ClipPlayer), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Nothing here reads a clock, a file or the ECS.
*/

#include "engine/anim/sources/Stance.h"

#include "engine/anim/sources/Clips.h"
#include "engine/core/config/sources/Constants.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dfn::anim {
namespace {

constexpr glm::vec3 AXIS_X{1.0f, 0.0f, 0.0f};
constexpr glm::vec3 AXIS_Y{0.0f, 1.0f, 0.0f};
constexpr glm::vec3 AXIS_Z{0.0f, 0.0f, 1.0f};

/// THE PITCH CONVENTION, in one place because five corrections share it: the
/// character faces -Z and +Y is up, so leaning FORWARD by `a` is a turn of
/// `-a` about +X. Getting this sign wrong tips the body backwards, which looks
/// like a different bug entirely.
[[nodiscard]] glm::quat forward_pitch(float a) { return glm::angleAxis(-a, AXIS_X); }

void model_matrices(const skel::Skeleton& skeleton, std::span<const JointLocal> sample,
                    std::vector<glm::mat4>& out) {
    const std::size_t n = skeleton.size();
    std::vector<glm::mat4> local(n, glm::mat4{1.0f});
    out.assign(n, glm::mat4{1.0f});
    for (std::size_t j = 0; j < n && j < sample.size(); ++j) {
        local[j] = glm::translate(glm::mat4{1.0f}, sample[j].translation)
                   * glm::mat4_cast(glm::normalize(sample[j].rotation))
                   * glm::scale(glm::mat4{1.0f}, sample[j].scale);
    }
    skel::skeleton_model_matrices(skeleton, local, out);
}

[[nodiscard]] glm::quat model_rotation(const glm::mat4& m) {
    glm::mat3 r{m};
    for (int c = 0; c < 3; ++c) {
        const float len = glm::length(r[c]);
        r[c] = len > 1.0e-8f ? r[c] / len : glm::vec3{c == 0, c == 1, c == 2};
    }
    return glm::normalize(glm::quat_cast(r));
}

[[nodiscard]] glm::vec3 origin_of(const glm::mat4& m) { return glm::vec3{m[3]}; }

/// TURNS A JOINT BY A MODEL-SPACE ROTATION, pre-multiplied in the parent's
/// frame — the same mechanism apply_arm_relax uses and for the same reason:
/// whatever the clip was doing with this joint keeps happening underneath.
void turn_joint(const skel::Skeleton& skeleton, const std::vector<glm::mat4>& model,
                int32_t j, const glm::quat& model_turn, std::span<JointLocal> sample) {
    if (j < 0 || static_cast<std::size_t>(j) >= sample.size()) {
        return;
    }
    const int32_t p = skeleton.joints[static_cast<std::size_t>(j)].parent;
    const glm::quat parent_rot =
        p >= 0 && static_cast<std::size_t>(p) < model.size()
            ? model_rotation(model[static_cast<std::size_t>(p)])
            : glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
    const glm::quat in_parent = glm::conjugate(parent_rot) * model_turn * parent_rot;
    JointLocal& jl = sample[static_cast<std::size_t>(j)];
    jl.rotation = glm::normalize(in_parent * glm::normalize(jl.rotation));
}

/// The flexion angle of a two-segment chain, 0 = straight, plus the axis the
/// flexion happens about. THE AXIS FALLS BACK TO +X when the chain is straight
/// (the cross product is then zero and carries no direction): a hanging arm
/// bends forward, and +X is the axis that takes the hand toward -Z.
struct Flexion {
    float angle_rad = 0.0f;
    glm::vec3 axis{1.0f, 0.0f, 0.0f};
};
[[nodiscard]] Flexion flexion_of(const glm::vec3& root, const glm::vec3& mid,
                                 const glm::vec3& tip) {
    Flexion f;
    const glm::vec3 a = mid - root;
    const glm::vec3 b = tip - mid;
    const float la = glm::length(a);
    const float lb = glm::length(b);
    if (la < 1.0e-5f || lb < 1.0e-5f) {
        return f;
    }
    const glm::vec3 ua = a / la;
    const glm::vec3 ub = b / lb;
    f.angle_rad = std::acos(std::clamp(glm::dot(ua, ub), -1.0f, 1.0f));
    const glm::vec3 c = glm::cross(ua, ub);
    const float lc = glm::length(c);
    f.axis = lc > 1.0e-4f ? c / lc : AXIS_X;
    return f;
}

/// THE ROTATION ABOUT `axis` THAT MOVES `r`'s component along `want_dir` TO
/// `target`, choosing the branch nearest zero. Used twice: to place a hand
/// sideways and to place it in height, both of which are "swing this limb
/// about the joint until the end of it reads the number the reference does".
/// Returns 0 when the target is out of the circle the point can reach, which
/// is honest: the limb cannot get there and forcing it would dislocate it.
[[nodiscard]] float turn_to_reach(const glm::vec3& r, const glm::vec3& axis,
                                  const glm::vec3& want_dir, float target) {
    // The point travels a circle in the plane normal to `axis`. Express it in
    // that plane's basis (want_dir projected, and axis x that).
    const glm::vec3 n = glm::normalize(axis);
    glm::vec3 e0 = want_dir - n * glm::dot(want_dir, n);
    if (glm::length(e0) < 1.0e-5f) {
        return 0.0f;
    }
    e0 = glm::normalize(e0);
    const glm::vec3 e1 = glm::cross(n, e0);
    const float x = glm::dot(r, e0);
    const float y = glm::dot(r, e1);
    const float radius = std::sqrt(x * x + y * y);
    if (radius < 1.0e-4f || std::abs(target) > radius) {
        return 0.0f;
    }
    const float phi = std::atan2(y, x);
    const float want = std::acos(std::clamp(target / radius, -1.0f, 1.0f));
    // Two branches (+-want); the one nearest the current phi is the small turn.
    const float a = want - phi;
    const float b = -want - phi;
    const auto wrap = [](float v) {
        while (v > glm::pi<float>()) { v -= 2.0f * glm::pi<float>(); }
        while (v < -glm::pi<float>()) { v += 2.0f * glm::pi<float>(); }
        return v;
    };
    const float wa = wrap(a);
    const float wb = wrap(b);
    return std::abs(wa) <= std::abs(wb) ? wa : wb;
}

} // namespace

StanceLayer build_stance_layer(const skel::Skeleton& skeleton,
                               const SkinnedRigBinding& binding) {
    StanceLayer s;
    if (skeleton.empty()) {
        return s;
    }
    const auto at = [&](Bone b) { return binding.names.joint[bone_index(b)]; };
    s.pelvis = at(Bone::Pelvis);
    s.torso = at(Bone::Torso);
    s.head = at(Bone::Head);
    s.upper_arm = {at(Bone::UpperArmL), at(Bone::UpperArmR)};
    s.forearm = {at(Bone::ForearmL), at(Bone::ForearmR)};
    s.hand = {at(Bone::HandL), at(Bone::HandR)};
    s.thigh = {at(Bone::ThighL), at(Bone::ThighR)};
    s.shin = {at(Bone::ShinL), at(Bone::ShinR)};
    s.foot = {at(Bone::FootL), at(Bone::FootR)};
    return s;
}

StanceMetrics measure_stance(const skel::Skeleton& skeleton,
                             const SkinnedRigBinding& binding,
                             std::span<const JointLocal> sample) {
    StanceMetrics m;
    const StanceLayer s = build_stance_layer(skeleton, binding);
    if (!s.valid() || sample.size() < skeleton.size()) {
        return m;
    }
    std::vector<glm::mat4> model;
    model_matrices(skeleton, sample, model);
    const auto pos = [&](int32_t j) {
        return j >= 0 && static_cast<std::size_t>(j) < model.size()
                   ? origin_of(model[static_cast<std::size_t>(j)])
                   : glm::vec3{0.0f};
    };
    const glm::vec3 pelvis = pos(s.pelvis);
    const glm::vec3 head = pos(s.head);

    // THE SILHOUETTE. `head - pelvis` is the line a profile photograph shows,
    // and it is deliberately not the chest joint's own rotation: an asset that
    // pitches the pelvis and an asset that pitches the chest draw the same
    // leaning man, and only the line tells us so.
    const glm::vec3 trunk = head - pelvis;
    if (glm::length(glm::vec2{trunk.y, trunk.z}) > 1.0e-4f) {
        m.trunk_pitch_rad = std::atan2(-trunk.z, trunk.y);
    }

    // THE GAZE, AGAINST THE BIND. The bind pose stands level by construction
    // (the importer grounds the rest pose), so "where would the bind's forward
    // be after this rotation" is a gaze direction on any asset — no head-axis
    // convention has to be guessed per model.
    if (s.head >= 0) {
        const glm::mat4 bind = glm::inverse(
            skeleton.joints[static_cast<std::size_t>(s.head)].inverse_bind);
        const glm::quat bind_rot = model_rotation(bind);
        const glm::vec3 local_fwd = glm::conjugate(bind_rot) * glm::vec3{0.0f, 0.0f, -1.0f};
        const glm::vec3 gaze =
            model_rotation(model[static_cast<std::size_t>(s.head)]) * local_fwd;
        m.gaze_pitch_rad = std::atan2(-gaze.y, glm::length(glm::vec2{gaze.x, gaze.z}));
    }

    for (int i = 0; i < 2; ++i) {
        const auto k = static_cast<std::size_t>(i);
        m.elbow_rad[k] =
            flexion_of(pos(s.upper_arm[k]), pos(s.forearm[k]), pos(s.hand[k])).angle_rad;
        m.knee_rad[k] =
            flexion_of(pos(s.thigh[k]), pos(s.shin[k]), pos(s.foot[k])).angle_rad;
        const glm::vec3 hand = pos(s.hand[k]);
        m.hand_drop_m[k] = pelvis.y - hand.y;
        m.hand_spread_m[k] = std::abs(hand.x - pelvis.x);
        const glm::vec3 thigh = pos(s.shin[k]) - pos(s.thigh[k]);
        m.thigh_pitch_rad[k] = std::atan2(-thigh.z, -thigh.y);
        const glm::vec3 arm = pos(s.forearm[k]) - pos(s.upper_arm[k]);
        m.arm_pitch_rad[k] = std::atan2(-arm.z, -arm.y);
    }
    m.stance_width_m = std::abs(pos(s.foot[0]).x - pos(s.foot[1]).x);
    m.shoulder_width_m = std::abs(pos(s.upper_arm[0]).x - pos(s.upper_arm[1]).x);
    const glm::vec3 shoulders = pos(s.upper_arm[1]) - pos(s.upper_arm[0]);
    const glm::vec3 hips = pos(s.thigh[1]) - pos(s.thigh[0]);
    m.shoulder_twist_rad =
        std::atan2(shoulders.z, shoulders.x) - std::atan2(hips.z, hips.x);
    while (m.shoulder_twist_rad > glm::pi<float>()) {
        m.shoulder_twist_rad -= 2.0f * glm::pi<float>();
    }
    while (m.shoulder_twist_rad < -glm::pi<float>()) {
        m.shoulder_twist_rad += 2.0f * glm::pi<float>();
    }
    m.valid = true;
    return m;
}

void apply_stance(const skel::Skeleton& skeleton, const StanceLayer& layer,
                  const StanceDrive& drive, std::span<JointLocal> sample) {
    const float w = std::clamp(drive.weight, 0.0f, 1.0f);
    if (!layer.valid() || w <= 0.0f || sample.size() < skeleton.size()) {
        return; // bit-for-bit no-op, so a "layer off" arm is a real control
    }
    const float run = std::clamp(drive.run_weight, 0.0f, 1.0f);
    const float stand = std::clamp(drive.stand_weight, 0.0f, 1.0f) * w;

    std::vector<glm::mat4> model;
    const auto refresh = [&] { model_matrices(skeleton, sample, model); };
    const auto pos = [&](int32_t j) {
        return j >= 0 && static_cast<std::size_t>(j) < model.size()
                   ? origin_of(model[static_cast<std::size_t>(j)])
                   : glm::vec3{0.0f};
    };

    // 1. THE TRUNK. The lean the reference has at this gear, minus the lean
    //    the clip brought, applied at the chest — so a run still leans and an
    //    idle stops crouching, and neither number is the clip author's.
    {
        const float want =
            glm::mix(static_cast<float>(config::STANCE_TRUNK_PITCH_STAND),
                     static_cast<float>(config::STANCE_TRUNK_PITCH_RUN), run);
        // TWICE, AND THE SECOND PASS IS NOT BELT AND BRACES. The angle is
        // measured on the pelvis->head LINE and corrected at the CHEST joint,
        // and those are two different centres of rotation: turning the chest
        // by d moves the head about the chest, not about the pelvis, so one
        // pass lands short in proportion to how big d was. Measured on the
        // sprint, one pass left 15.7 degrees where 13.7 was asked for; the
        // second pass closes it to under a tenth of a degree. Iterating is the
        // honest answer because the relation is a real geometric one and not a
        // fudge factor somebody would have to keep true.
        for (int pass = 0; pass < 2; ++pass) {
            refresh();
            const glm::vec3 trunk = pos(layer.head) - pos(layer.pelvis);
            const float have = std::atan2(-trunk.z, trunk.y);
            turn_joint(skeleton, model, layer.torso, forward_pitch((want - have) * w),
                       sample);
        }
    }

    // 2. THE LEGS, only while standing (see the header for why). The knee
    //    first, because straightening it moves the foot the stance width is
    //    then measured to.
    if (stand > 0.001f) {
        refresh();
        const auto knee_target = static_cast<float>(config::STANCE_KNEE_STAND);
        for (int i = 0; i < 2; ++i) {
            const auto k = static_cast<std::size_t>(i);
            const Flexion f =
                flexion_of(pos(layer.thigh[k]), pos(layer.shin[k]), pos(layer.foot[k]));
            const float d = (knee_target - f.angle_rad) * stand;
            turn_joint(skeleton, model, layer.shin[k], glm::angleAxis(d, f.axis), sample);
        }
        refresh();
        const glm::vec3 pelvis = pos(layer.pelvis);
        const float shoulders =
            std::abs(pos(layer.upper_arm[0]).x - pos(layer.upper_arm[1]).x);
        const float half =
            0.5f * shoulders * static_cast<float>(config::STANCE_WIDTH_SHOULDERS);
        for (int i = 0; i < 2; ++i) {
            const auto k = static_cast<std::size_t>(i);
            const glm::vec3 hip = pos(layer.thigh[k]);
            const glm::vec3 foot = pos(layer.foot[k]);
            const float side = foot.x >= pelvis.x ? 1.0f : -1.0f;
            const float want_x = pelvis.x + side * half - hip.x;
            const float a = turn_to_reach(foot - hip, AXIS_Z, AXIS_X, want_x);
            turn_joint(skeleton, model, layer.thigh[k],
                       glm::angleAxis(a * stand, AXIS_Z), sample);
        }
    }

    // 3. THE GAZE, LAST OF THE TRUNK CHAIN and after the legs, because both
    //    moved the head. HEAD_STABILIZE is the same reflex the crouch and the
    //    seat already use (Clips.h): you do not look at the floor because you
    //    leaned.
    refresh();
    if (layer.head >= 0) {
        const glm::vec3 trunk = pos(layer.head) - pos(layer.pelvis);
        const float lean = std::atan2(-trunk.z, trunk.y);
        const glm::mat4 bind = glm::inverse(
            skeleton.joints[static_cast<std::size_t>(layer.head)].inverse_bind);
        const glm::vec3 local_fwd =
            glm::conjugate(model_rotation(bind)) * glm::vec3{0.0f, 0.0f, -1.0f};
        const glm::vec3 gaze =
            model_rotation(model[static_cast<std::size_t>(layer.head)]) * local_fwd;
        const float have =
            std::atan2(-gaze.y, glm::length(glm::vec2{gaze.x, gaze.z}));
        const float want = (1.0f - HEAD_STABILIZE) * lean;
        turn_joint(skeleton, model, layer.head, forward_pitch((want - have) * w), sample);
    }

    // 4. THE RUN'S TWIST, as a GAIN and not an angle: the shoulders alternate
    //    over the stride, so an added constant would list the body to one side
    //    for the whole run. The gain is 1 at a walk (the clip's own twist is
    //    what a walk wants) and rises with the gear.
    if (drive.twist_gain > 1.0f && run > 0.001f) {
        refresh();
        const glm::vec3 shoulders = pos(layer.upper_arm[1]) - pos(layer.upper_arm[0]);
        const glm::vec3 hips = pos(layer.thigh[1]) - pos(layer.thigh[0]);
        float twist = std::atan2(shoulders.z, shoulders.x)
                      - std::atan2(hips.z, hips.x);
        while (twist > glm::pi<float>()) { twist -= 2.0f * glm::pi<float>(); }
        while (twist < -glm::pi<float>()) { twist += 2.0f * glm::pi<float>(); }
        const float gain = 1.0f + (drive.twist_gain - 1.0f) * run * w;
        turn_joint(skeleton, model, layer.torso,
                   glm::angleAxis((gain - 1.0f) * twist, AXIS_Y), sample);
    }

    // 5. THE ARM SWING, the same gain shape and the same reason. It is not
    //    gated on the run: the complaint the reference names is about the
    //    WALK, whose arms barely move at all on this asset.
    if (drive.arm_swing_gain > 1.0f) {
        refresh();
        for (int i = 0; i < 2; ++i) {
            const auto k = static_cast<std::size_t>(i);
            const glm::vec3 arm = pos(layer.forearm[k]) - pos(layer.upper_arm[k]);
            const float pitch = std::atan2(-arm.z, -arm.y);
            const float gain = 1.0f + (drive.arm_swing_gain - 1.0f) * w;
            // AND THIS ONE IS +X WHERE THE TRUNK'S IS -X, which is not a typo
            // and was a real bug for one measurement: the trunk's pitch is
            // read off a vector pointing UP and the arm's off one pointing
            // DOWN, so the same turn carries them opposite ways. Written with
            // the trunk's sign the gain SUBTRACTED swing — the walk's arms
            // went from 16.5 degrees to 5.6, i.e. the fix made the complaint
            // worse while every other number improved.
            turn_joint(skeleton, model, layer.upper_arm[k],
                       glm::angleAxis((gain - 1.0f) * pitch, AXIS_X), sample);
        }
    }
}

} // namespace dfn::anim
