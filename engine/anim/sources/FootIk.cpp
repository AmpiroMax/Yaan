/*
Module: engine/anim
File: engine/anim/sources/FootIk.cpp

Responsibility:
- Implements the foot-grounding measurement and the two-bone knee solve.

Dependencies:
- Uses: FootIk.h, SkinnedBody.h, core skeleton, glm.
- Used by: dfn_anim, engine/app, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- No clock, no physics, no IO. The ground heights arrive as arguments.
*/

#include "engine/anim/sources/FootIk.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dfn::anim {
namespace {

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

[[nodiscard]] glm::quat model_rotation(const glm::mat4& m) {
    glm::mat3 r{m};
    for (int c = 0; c < 3; ++c) {
        const float len = glm::length(r[c]);
        r[c] = len > 1.0e-8f ? r[c] / len : glm::vec3{c == 0, c == 1, c == 2};
    }
    return glm::normalize(glm::quat_cast(r));
}

[[nodiscard]] glm::vec3 origin_of(const glm::mat4& m) { return glm::vec3{m[3]}; }

/// The shortest rotation carrying unit `from` onto unit `to`. Written out
/// rather than taken from glm's experimental GTX, which the project does not
/// enable; the antiparallel case is the one worth spelling, because that is
/// where a naive cross product is the zero vector and the quaternion is NaN.
[[nodiscard]] glm::quat shortest_arc(const glm::vec3& from, const glm::vec3& to) {
    const float d = glm::dot(from, to);
    if (d > 1.0f - 1.0e-6f) {
        return glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
    }
    if (d < -1.0f + 1.0e-6f) {
        glm::vec3 axis = glm::cross(glm::vec3{1.0f, 0.0f, 0.0f}, from);
        if (glm::length(axis) < 1.0e-4f) {
            axis = glm::cross(glm::vec3{0.0f, 1.0f, 0.0f}, from);
        }
        return glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
    }
    const glm::vec3 axis = glm::cross(from, to);
    glm::quat q{1.0f + d, axis.x, axis.y, axis.z};
    return glm::normalize(q);
}

/// Turns a MODEL-space rotation about a joint's own origin into the local
/// rotation that produces it. One line, three callers, and getting it wrong
/// looks like a limb that twists instead of bending.
void turn_in_model(const glm::quat& parent_rot, const glm::quat& turn, JointLocal& jl) {
    const glm::quat in_parent = glm::conjugate(parent_rot) * turn * parent_rot;
    jl.rotation = glm::normalize(in_parent * glm::normalize(jl.rotation));
}

/// HOW PLANTED A FOOT IS, from the pose alone: 1 while its lowest contact sits
/// inside the grip band of its rest height, fading to 0 by the release height.
[[nodiscard]] float stance_weight(float height_above_rest) {
    if (height_above_rest <= FOOT_IK_GRIP_M) {
        return 1.0f;
    }
    if (height_above_rest >= FOOT_IK_RELEASE_M) {
        return 0.0f;
    }
    const float u = (height_above_rest - FOOT_IK_GRIP_M)
                    / (FOOT_IK_RELEASE_M - FOOT_IK_GRIP_M);
    return 1.0f - u * u * (3.0f - 2.0f * u); // smoothstep, zero slope at both ends
}

} // namespace

FootIkSetup build_foot_ik(const skel::Skeleton& skeleton, const SkinnedRigBinding& binding,
                          const ContactSet& contacts) {
    FootIkSetup s;
    const Bone hips[2] = {Bone::ThighL, Bone::ThighR};
    const Bone knees[2] = {Bone::ShinL, Bone::ShinR};
    const Bone feet[2] = {Bone::FootL, Bone::FootR};
    for (int i = 0; i < 2; ++i) {
        const auto side = static_cast<std::size_t>(i);
        s.hip[side] = binding.names.joint[bone_index(hips[side])];
        s.knee[side] = binding.names.joint[bone_index(knees[side])];
        s.ankle[side] = binding.names.joint[bone_index(feet[side])];
        const FootContacts& fc = contacts.side[side];
        if (fc.count > 0) {
            s.ankle_rest_y[side] = fc.rest_y[0];
        }
        if (fc.count > 1) {
            s.toe[side] = fc.joint[1];
            s.toe_rest_y[side] = fc.rest_y[1];
        }
    }
    for (std::size_t j = 0; j < skeleton.size(); ++j) {
        if (skeleton.joints[j].parent < 0) {
            s.roots.push_back(static_cast<int32_t>(j));
        }
    }
    return s;
}

FootIkPlan plan_foot_ik(const skel::Skeleton& skeleton, const FootIkSetup& setup,
                        const FootIkProbe& probe, std::span<const JointLocal> sample) {
    FootIkPlan plan;
    if (!setup.valid() || !probe.valid || sample.size() < skeleton.size()) {
        return plan;
    }
    std::vector<glm::mat4> local;
    std::vector<glm::mat4> model;
    model_matrices(skeleton, sample, local, model);
    for (int i = 0; i < 2; ++i) {
        const auto side = static_cast<std::size_t>(i);
        const float ankle_y = origin_of(model[static_cast<std::size_t>(setup.ankle[side])]).y;
        const float ankle_need =
            probe.ankle_ground[side] + setup.ankle_rest_y[side] - ankle_y;
        float lowest = ankle_y - setup.ankle_rest_y[side];
        if (setup.toe[side] >= 0) {
            const float toe_y = origin_of(model[static_cast<std::size_t>(setup.toe[side])]).y;
            plan.toe_need[side] = probe.toe_ground[side] + setup.toe_rest_y[side] - toe_y;
            lowest = std::min(lowest, toe_y - setup.toe_rest_y[side]);
        } else {
            plan.toe_need[side] = ankle_need;
        }
        // THE FOOT RISES BY THE WORSE OF ITS TWO CONTACTS, and that choice is
        // the whole answer to a NOSING. When the heel is on one tread and the
        // ball on the next, the two contacts ask for heights 0.18 m apart —
        // more than a 0.11 m foot can span by pitching, so no pose puts both
        // on their own ground. Taking the LOWER of the two demands would bury
        // the other one by the rise; taking the higher stands the foot on the
        // nosing with its heel in the air, which is what a person does on a
        // stair and what leaves nothing under the ground.
        plan.need[side] = std::max(ankle_need, plan.toe_need[side]);
        plan.weight[side] = stance_weight(lowest);
    }
    // THE ROOT GOES TO THE LOWER PLANTED FOOT, and the rule has to stay
    // CONTINUOUS in the weights or the body ticks every time a foot leaves the
    // ground. Written out: the lower foot votes with its own weight, and the
    // higher one only with whatever weight the lower one is missing. Both
    // planted -> the lower one decides; only one planted -> that one decides;
    // neither -> nothing moves, which is what a jump needs.
    const std::size_t lo = plan.need[0] <= plan.need[1] ? 0 : 1;
    const std::size_t hi = 1 - lo;
    const float w_lo = plan.weight[lo];
    const float w_hi = std::max(0.0f, plan.weight[hi] - w_lo);
    const float total = w_lo + w_hi;
    plan.root_dy = total > 1.0e-4f
                       ? (w_lo * plan.need[lo] + w_hi * plan.need[hi]) / total
                       : 0.0f;
    plan.root_dy = std::clamp(plan.root_dy, -FOOT_IK_ROOT_LIMIT_M, FOOT_IK_ROOT_LIMIT_M);
    return plan;
}

void apply_foot_ik(const skel::Skeleton& skeleton, const FootIkSetup& setup,
                   const FootIkProbe& probe, const FootIkPlan& plan, float strength,
                   std::span<JointLocal> sample) {
    const float k = std::clamp(strength, 0.0f, 1.0f);
    if (!setup.valid() || k <= 0.0f || sample.size() < skeleton.size()) {
        return;
    }
    const float root_dy = k * plan.root_dy;
    for (const int32_t r : setup.roots) {
        sample[static_cast<std::size_t>(r)].translation.y += root_dy;
    }
    std::vector<glm::mat4> local;
    std::vector<glm::mat4> model;
    model_matrices(skeleton, sample, local, model);

    for (int i = 0; i < 2; ++i) {
        const auto side = static_cast<std::size_t>(i);
        const float w = k * plan.weight[side];
        if (w <= 0.0f) {
            continue;
        }
        // WHAT THE ROOT DID NOT REACH. The lower foot's residual is zero by
        // construction; the higher one's is the step's rise, and it is folded
        // out of the knee. Only upward: a foot the clip holds ABOVE its ground
        // is a foot in flight, and pulling it down is how a walk becomes a
        // shuffle.
        const float residual = std::max(0.0f, w * plan.need[side] - root_dy);
        const int32_t hip = setup.hip[side];
        const int32_t knee = setup.knee[side];
        const int32_t ankle = setup.ankle[side];
        const glm::vec3 A = origin_of(model[static_cast<std::size_t>(hip)]);
        const glm::vec3 B = origin_of(model[static_cast<std::size_t>(knee)]);
        const glm::vec3 C = origin_of(model[static_cast<std::size_t>(ankle)]);
        if (residual > 1.0e-5f) {
            const glm::vec3 T = C + glm::vec3{0.0f, residual, 0.0f};
            const float l1 = glm::length(B - A);
            const float l2 = glm::length(C - B);
            if (l1 > 1.0e-4f && l2 > 1.0e-4f) {
                // THE LAW OF COSINES, on the two segments the leg is. The
                // reach is clamped INSIDE the reachable annulus rather than
                // at its edges: a leg asked for exactly l1 + l2 is a leg with
                // no bend plane left, and the next frame's normal flips.
                const float lo = std::abs(l1 - l2) + 1.0e-3f;
                const float hi = l1 + l2 - 1.0e-3f;
                const float r_now = std::clamp(glm::length(C - A), lo, hi);
                const float r_want = std::clamp(glm::length(T - A), lo, hi);
                const auto interior = [&](float r) {
                    return std::acos(std::clamp((l1 * l1 + l2 * l2 - r * r)
                                                    / (2.0f * l1 * l2),
                                                -1.0f, 1.0f));
                };
                const glm::vec3 u = A - B;
                const glm::vec3 v = C - B;
                glm::vec3 n = glm::cross(u, v);
                if (glm::length(n) < 1.0e-6f) {
                    // A STRAIGHT LEG HAS NO PLANE OF ITS OWN, so the knee is
                    // bent about the character's own lateral axis, which is
                    // what a knee does. Guessing a normal from the near-zero
                    // cross product would pick a direction from rounding.
                    n = glm::vec3{1.0f, 0.0f, 0.0f};
                }
                n = glm::normalize(n);
                const float delta = interior(r_want) - interior(r_now);
                if (std::abs(delta) > 1.0e-5f) {
                    turn_in_model(model_rotation(model[static_cast<std::size_t>(hip)]),
                                  glm::angleAxis(delta, n),
                                  sample[static_cast<std::size_t>(knee)]);
                }
                // The knee bend moved the ankle; the hip now swings the whole
                // leg so the ankle lands ON the target rather than near it.
                model_matrices(skeleton, sample, local, model);
                const glm::vec3 C2 = origin_of(model[static_cast<std::size_t>(ankle)]);
                const glm::vec3 from = C2 - A;
                const glm::vec3 to = T - A;
                if (glm::length(from) > 1.0e-5f && glm::length(to) > 1.0e-5f) {
                    const glm::quat swing =
                        shortest_arc(glm::normalize(from), glm::normalize(to));
                    const int32_t hp = skeleton.joints[static_cast<std::size_t>(hip)].parent;
                    const glm::quat parent_rot =
                        hp >= 0 ? model_rotation(model[static_cast<std::size_t>(hp)])
                                : glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
                    turn_in_model(parent_rot, swing, sample[static_cast<std::size_t>(hip)]);
                    model_matrices(skeleton, sample, local, model);
                }
            }
        }
        // THE ANKLE PITCHES SO THE TOE MEETS ITS OWN GROUND. Without it a flat
        // foot on a 15-degree slope buries a toe or a heel by half the foot's
        // length times the sine of the slope — three centimetres on this
        // model, i.e. three times the centimetre the acceptance allows.
        const int32_t toe = setup.toe[side];
        if (toe < 0) {
            continue;
        }
        const glm::vec3 ankle_p = origin_of(model[static_cast<std::size_t>(ankle)]);
        const glm::vec3 toe_p = origin_of(model[static_cast<std::size_t>(toe)]);
        const glm::vec3 span = toe_p - ankle_p;
        const glm::vec3 flat{span.x, 0.0f, span.z};
        const float d = glm::length(flat);
        if (d < 1.0e-3f) {
            continue;
        }
        // THE NEED IS RE-READ HERE, not carried from the plan: the root shift
        // and the knee solve have both already moved this toe, and pitching by
        // a number measured before them would count those metres twice.
        const float toe_need_now =
            probe.toe_ground[side] + setup.toe_rest_y[side] - toe_p.y;
        const float rise = w * toe_need_now;
        // THE PITCH IS AN arcsine ON THE FOOT'S OWN LENGTH, not an arctangent
        // on its horizontal span. The toe travels on a CIRCLE about the ankle,
        // so the height it can gain is bounded by that circle's radius; an
        // atan2(rise, span) reads as an angle for any rise at all and quietly
        // returns 61 degrees for a lift the foot cannot make, which leaves the
        // toe short and the frame wrong in a way no number complains about.
        // Past the circle the target is CLAMPED and the shortfall is real —
        // at a nosing it is the heel that stays up, and that is a person on a
        // stair rather than a defect.
        const glm::vec3 span_now = toe_p - ankle_p;
        const float reach = glm::length(span_now);
        if (reach < 1.0e-4f) {
            continue;
        }
        const float phi0 = std::asin(std::clamp(span_now.y / reach, -1.0f, 1.0f));
        const float phi1 = std::asin(std::clamp((span_now.y + rise) / reach, -1.0f, 1.0f));
        const float angle = phi1 - phi0;
        if (std::abs(angle) < 1.0e-4f) {
            continue;
        }
        // The axis that lifts the TOE: perpendicular to the foot's own
        // heading and to up, so the ankle rolls the foot about its heel
        // rather than twisting it.
        const glm::vec3 axis =
            glm::normalize(glm::cross(glm::normalize(flat), glm::vec3{0.0f, 1.0f, 0.0f}));
        const int32_t ap = skeleton.joints[static_cast<std::size_t>(ankle)].parent;
        const glm::quat parent_rot =
            ap >= 0 ? model_rotation(model[static_cast<std::size_t>(ap)])
                    : glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
        turn_in_model(parent_rot, glm::angleAxis(angle, axis),
                      sample[static_cast<std::size_t>(ankle)]);
        model_matrices(skeleton, sample, local, model);
    }
}

FootGap foot_gap(const skel::Skeleton& skeleton, const FootIkSetup& setup,
                 const FootIkProbe& probe, const FootIkPlan& plan,
                 std::span<const JointLocal> sample) {
    FootGap out;
    if (!setup.valid() || sample.size() < skeleton.size()) {
        return out;
    }
    std::vector<glm::mat4> local;
    std::vector<glm::mat4> model;
    model_matrices(skeleton, sample, local, model);
    for (int i = 0; i < 2; ++i) {
        const auto side = static_cast<std::size_t>(i);
        out.judged[side] = plan.weight[side] >= FOOT_JUDGED_WEIGHT ? 1u : 0u;
        // THE DEEPEST OF THE FOOT'S CONTACTS decides, and the sign is kept:
        // `deficit` is how far the ground is ABOVE the contact, so the gap the
        // foot leaves is its negative.
        const float ankle_y = origin_of(model[static_cast<std::size_t>(setup.ankle[side])]).y;
        float deficit = probe.ankle_ground[side] + setup.ankle_rest_y[side] - ankle_y;
        if (setup.toe[side] >= 0) {
            const float toe_y = origin_of(model[static_cast<std::size_t>(setup.toe[side])]).y;
            deficit = std::max(deficit,
                               probe.toe_ground[side] + setup.toe_rest_y[side] - toe_y);
        }
        out.gap[side] = -deficit;
    }
    return out;
}

float foot_penetration(const skel::Skeleton& skeleton, const FootIkSetup& setup,
                       const FootIkProbe& probe, const FootIkPlan& plan,
                       std::span<const JointLocal> sample) {
    // ONE ARITHMETIC, TWO READINGS (Rule 39): this is the buried half of
    // foot_gap, and writing the model matrices out a second time here is
    // exactly the shadow copy that drifts on the next edit.
    const FootGap g = foot_gap(skeleton, setup, probe, plan, sample);
    float worst = 0.0f;
    for (int i = 0; i < 2; ++i) {
        const auto side = static_cast<std::size_t>(i);
        if (g.judged[side] == 0) {
            continue; // in its swing: see the header note
        }
        worst = std::max(worst, -g.gap[side]);
    }
    return worst;
}

} // namespace dfn::anim
