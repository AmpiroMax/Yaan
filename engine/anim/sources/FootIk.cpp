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

#include "engine/core/config/sources/Constants.h"

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

/// ДВУЗВЕННИК КОЛЕНА ДО ТОЧКИ: лодыжка стороны `side` ставится в T (система
/// тела) — угол колена по закону косинусов в плоскости бедро-колено-лодыжка,
/// затем поворот бедра дугой к цели. Один решатель на подъём (apply_foot_ik)
/// и замок стопы (apply_foot_lock); `local`/`model` пересчитываются на месте.
void reach_ankle(const skel::Skeleton& skeleton, const FootIkSetup& setup, std::size_t side,
                 const glm::vec3& T, std::span<JointLocal> sample,
                 std::vector<glm::mat4>& local, std::vector<glm::mat4>& model) {
    const int32_t hip = setup.hip[side];
    const int32_t knee = setup.knee[side];
    const int32_t ankle = setup.ankle[side];
    const glm::vec3 A = origin_of(model[static_cast<std::size_t>(hip)]);
    const glm::vec3 B = origin_of(model[static_cast<std::size_t>(knee)]);
    const glm::vec3 C = origin_of(model[static_cast<std::size_t>(ankle)]);
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
        const int32_t ankle = setup.ankle[side];
        const glm::vec3 C = origin_of(model[static_cast<std::size_t>(ankle)]);
        if (residual > 1.0e-5f) {
            reach_ankle(skeleton, setup, side, C + glm::vec3{0.0f, residual, 0.0f},
                        sample, local, model);
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


ContactState contact_state(const skel::Skeleton& skeleton, const FootIkSetup& setup,
                           const FootIkPlan& plan, std::span<const JointLocal> sample) {
    ContactState out;
    if (!setup.valid() || sample.size() < skeleton.size()) {
        return out;
    }
    std::vector<glm::mat4> local;
    std::vector<glm::mat4> model;
    model_matrices(skeleton, sample, local, model);
    for (std::size_t side = 0; side < 2; ++side) {
        const glm::vec3 ankle = origin_of(model[static_cast<std::size_t>(setup.ankle[side])]);
        out.ankle[side] = ankle;
        out.toe[side] = ankle;
        const float ankle_h = ankle.y - setup.ankle_rest_y[side];
        out.point[side] = ankle;
        out.height[side] = ankle_h;
        if (setup.toe[side] >= 0) {
            const glm::vec3 toe = origin_of(model[static_cast<std::size_t>(setup.toe[side])]);
            out.toe[side] = toe;
            const float toe_h = toe.y - setup.toe_rest_y[side];
            if (toe_h < ankle_h) {
                out.point[side] = toe;
                out.height[side] = toe_h;
                out.toe_point[side] = true;
            }
        }
        out.weight[side] = std::clamp(plan.weight[side], 0.0f, 1.0f);
    }
    // ВЕС ОПОРЫ: самая низкая стопа ведёт, вторая — в полосе над ней; полёт —
    // никто (см. FOOT_SUPPORT_BAND_M).
    const float lowest = std::min(out.height[0], out.height[1]);
    const float band = static_cast<float>(config::FOOT_SUPPORT_BAND_M);
    for (std::size_t side = 0; side < 2; ++side) {
        if (lowest > FOOT_IK_RELEASE_M) {
            out.support[side] = 0.0f;
            continue;
        }
        const float above = out.height[side] - lowest;
        const float u = band > 0.0f ? std::clamp(above / band, 0.0f, 1.0f) : (above > 0.0f ? 1.0f : 0.0f);
        out.support[side] = 1.0f - u * u * (3.0f - 2.0f * u);
    }
    out.valid = true;
    return out;
}

FootLockParams FootLockParams::from_config() {
    FootLockParams p;
    p.on_weight = static_cast<float>(config::FOOT_LOCK_ON_WEIGHT);
    p.off_weight = static_cast<float>(config::FOOT_LOCK_OFF_WEIGHT);
    p.release_s = static_cast<float>(config::FOOT_LOCK_RELEASE_S);
    p.twist_max_rad = static_cast<float>(config::FOOT_LOCK_TWIST_MAX_RAD);
    p.step_s = static_cast<float>(config::FOOT_LOCK_STEP_S);
    return p;
}

void update_foot_locks(FootLockState& state, const std::array<glm::vec3, 2>& contact_world,
                       const std::array<float, 2>& weight,
                       const std::array<bool, 2>& point_is_toe, float body_yaw, float dt,
                       const FootLockParams& params) {
    const auto wrap = [](float a) {
        const float two_pi = 2.0f * glm::pi<float>();
        a = std::fmod(a + glm::pi<float>(), two_pi);
        if (a < 0.0f) {
            a += two_pi;
        }
        return a - glm::pi<float>();
    };
    state.step_cooldown_s = std::max(0.0f, state.step_cooldown_s - dt);
    // ДОСАДКА ПОСЛЕ ПОВОРОТА: корпус встал — стопы, оставшиеся под ним
    // повёрнутыми больше трети порога, переступают по одной, пока не встанут
    // под корпус; иначе после разворота на 180° человек стоял бы с тазом,
    // развёрнутым к стопам на 30°, сколько угодно.
    const float dyaw = std::abs(wrap(body_yaw - state.last_yaw));
    state.last_yaw = body_yaw;
    state.still_s = dyaw > 1.0e-3f ? 0.0f : state.still_s + dt;
    const float twist_limit = state.still_s >= params.step_s ? params.twist_max_rad / 3.0f
                                                             : params.twist_max_rad;
    // ПЕРЕСТУПАЕТ ОДНА СТОПА ЗА РАЗ — та, над которой корпус ушёл дальше;
    // вторая ждёт FOOT_LOCK_STEP_S, иначе обе уходят разом (подскок на месте).
    std::size_t first = 2;
    float worst_twist = 0.0f;
    for (std::size_t side = 0; side < 2; ++side) {
        if (state.locked[side]) {
            const float twist = std::abs(wrap(body_yaw - state.anchor_yaw[side]));
            if (twist > worst_twist) {
                worst_twist = twist;
                first = side;
            }
        }
    }
    for (std::size_t side = 0; side < 2; ++side) {
        state.engaged[side] = false;
        state.hold_s[side] = std::max(0.0f, state.hold_s[side] - dt);
        if (state.locked[side]) {
            const float twist = std::abs(wrap(body_yaw - state.anchor_yaw[side]));
            if (weight[side] < params.off_weight) {
                state.locked[side] = false; // отпущен: сила сходит ниже
            } else if (twist > twist_limit && side == first
                       && state.step_cooldown_s <= 0.0f) {
                // ПЕРЕСТУП: корпус ушёл дальше, чем бедро может довернуть
                // (владелец 02.09-2: «ноги скручиваются крестиком»). Замок
                // сходит за FOOT_LOCK_RELEASE_S, стопа уходит под корпус, и
                // до того замыкать её заново нельзя.
                state.locked[side] = false;
                state.hold_s[side] = params.release_s;
                state.step_cooldown_s = params.step_s;
            } else {
                // ПЕРЕКАТ: нижняя точка сменилась (пятка → носок) — якорь
                // перецепляется на новую точку там, где она сейчас стоит;
                // держать пятку на месте, когда она уже поднимается, значит
                // ломать перекат клипа.
                if (state.anchor_toe[side] != point_is_toe[side]) {
                    state.anchor[side] = contact_world[side];
                    state.anchor_toe[side] = point_is_toe[side];
                }
                state.strength[side] = 1.0f;
                continue;
            }
        } else if (weight[side] >= params.on_weight && state.hold_s[side] <= 0.0f) {
            state.locked[side] = true;
            state.engaged[side] = true;
            state.anchor[side] = contact_world[side];
            state.anchor_toe[side] = point_is_toe[side];
            state.anchor_yaw[side] = body_yaw;
            state.strength[side] = 1.0f;
            continue;
        }
        const float fall = params.release_s > 0.0f ? dt / params.release_s : 1.0f;
        state.strength[side] = std::max(0.0f, state.strength[side] - fall);
    }
}

void apply_foot_lock(const skel::Skeleton& skeleton, const FootIkSetup& setup,
                     const std::array<glm::vec3, 2>& point_target_model,
                     const std::array<bool, 2>& target_is_toe,
                     const std::array<float, 2>& strength, std::span<JointLocal> sample) {
    if (!setup.valid() || sample.size() < skeleton.size()) {
        return;
    }
    std::vector<glm::mat4> local;
    std::vector<glm::mat4> model;
    model_matrices(skeleton, sample, local, model);
    for (std::size_t side = 0; side < 2; ++side) {
        float k = std::clamp(strength[side], 0.0f, 1.0f);
        if (k <= 1.0e-4f) {
            continue;
        }
        // ЗАМОК СДАЁТСЯ, А НЕ РВЁТ НОГУ (владелец 03.09: «при беге ноги тянутся,
        // потом отрываются»): когда тело убегает от якоря быстрее, чем стопа
        // клипа, якорь уходит за вытяжение ноги, и прежде замок держал стопу
        // на пределе (0,998 длины ноги) до самого отпускания по весу —
        // нога вытягивалась струной и щёлкала. Теперь сила замка сходит на
        // нет по мере приближения якоря к полному вытяжению (0,96…1,02 длины
        // ноги): стопа мягко уезжает с якоря, колено не выпрямляется в
        // струну. Считается по позе до проходов.
        {
            const glm::vec3 A = origin_of(model[static_cast<std::size_t>(setup.hip[side])]);
            const glm::vec3 B = origin_of(model[static_cast<std::size_t>(setup.knee[side])]);
            const glm::vec3 C = origin_of(model[static_cast<std::size_t>(setup.ankle[side])]);
            const glm::vec3 P = (target_is_toe[side] && setup.toe[side] >= 0)
                                    ? origin_of(model[static_cast<std::size_t>(setup.toe[side])])
                                    : C;
            const float leg = glm::length(B - A) + glm::length(C - B);
            const glm::vec3 want{point_target_model[side].x, P.y, point_target_model[side].z};
            const float d0 = glm::length(want + (C - P) - A);
            if (leg > 1.0e-4f) {
                const float u = std::clamp((1.02f * leg - d0) / (0.06f * leg), 0.0f, 1.0f);
                k *= u * u * (3.0f - 2.0f * u);
            }
            if (k <= 1.0e-4f) {
                continue;
            }
        }
        // ТРИ ПРОХОДА: двузвенник ведёт ЛОДЫЖКУ, а якорь — у подушечки; поворот
        // бедра и колена поворачивает и стопу, так что смещение «лодыжка минус
        // подушечка» после решения уже не то, что до него. Повтор с
        // пересчитанным смещением сходится за два-три шага (замер: 8 мм → <1).
        // 24 прохода, не 12: инерция на смене опоры (RootMotion.cpp) даёт замку
        // до 2 см расхождения за окно, и 12 проходов оставляли 2,2 мм на беге
        // трусцой при пороге 2.
        for (int pass = 0; pass < 36; ++pass) {
            const glm::vec3 A = origin_of(model[static_cast<std::size_t>(setup.hip[side])]);
            const glm::vec3 B = origin_of(model[static_cast<std::size_t>(setup.knee[side])]);
            const glm::vec3 C = origin_of(model[static_cast<std::size_t>(setup.ankle[side])]);
            const glm::vec3 P = (target_is_toe[side] && setup.toe[side] >= 0)
                                    ? origin_of(model[static_cast<std::size_t>(setup.toe[side])])
                                    : C;
            // Цель для ЛОДЫЖКИ: точка касания в якорь ПО ГОРИЗОНТАЛИ, высоту
            // держит подъём на грунт (apply_foot_ik). Пробовали и по трём
            // осям: высота якоря спорит с тазом клипа (купленные клипы гонят
            // стопу под землю на 5 см, и вертикально пришпиленная стопа
            // упирается в вытяжение ноги — 30 мм сноса по горизонтали вместо
            // 1.2). Стопа едет жёстко: лодыжка = якорь + её смещение от точки.
            const glm::vec3 want_point{point_target_model[side].x, P.y,
                                       point_target_model[side].z};
            glm::vec3 T = want_point + (C - P);
            T = glm::mix(C, T, k);
            // Досягаемость: дальше 99,5 % вытяжения ноги цель режется к бедру,
            // а не растягивает ногу (0.985 оставляло 1 мм на вытянутой ноге трусцы).
            const float reach = 0.998f * (glm::length(B - A) + glm::length(C - B));
            const glm::vec3 from_hip = T - A;
            if (const float d = glm::length(from_hip); d > reach && d > 1.0e-5f) {
                T = A + from_hip * (reach / d);
            }
            if (glm::length(T - C) < 1.0e-4f) {
                break;
            }
            reach_ankle(skeleton, setup, side, T, sample, local, model);
        }
    }
}

} // namespace dfn::anim
