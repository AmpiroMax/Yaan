/*
Module: engine/anim
File: engine/anim/sources/ClipPlayer.cpp

Responsibility:
- Implements role resolution, the load-time measurements (a clip's travel, its
  plant phase, the lift that grounds it, and which clip a gear ends up with),
  the tick and the frame of imported-clip playback, and the foot-slide prober.

Dependencies:
- Uses: ClipPlayer.h, Pose.h, SkinnedBody.h, core skeleton, generated constants.
- Used by: dfn_anim, engine/app, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Nothing here reads a wall clock, the ECS or a file: every function takes the
  time it needs as a parameter, and the tests depend on that being true.
*/

#include "engine/anim/sources/ClipPlayer.h"

#include "engine/core/config/sources/Constants.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>

namespace dfn::anim {
namespace {

constexpr float PHASE_LEFT = static_cast<float>(config::FOOTFALL_PHASE_LEFT);

/// SPEED AT WHICH THE FEET ARE CONSIDERED TO BE MOVING AT ALL. Not a gear and
/// not a NUMBERS row: it is the point where the idle clip stops being a better
/// answer than a locomotion clip whose stride has shrunk to nothing, and its
/// only consumer is the role choice below. Body.cpp's `gait_fade` fades the
/// procedural gait in over the same neighbourhood for the same reason.
constexpr float MOVING_SPEED_MPS = 0.15f;

/// WHICH SAMPLES COUNT AS THE FOOT BEING DOWN: the foot's CONTACT POINT (the
/// lowest of the ankle and whatever the asset hangs off it — here a toe)
/// within this many metres of ITS OWN lowest point in the cycle.
///
/// AN ABSOLUTE TOLERANCE, and the four rules that failed before it are worth
/// keeping written down. (1) "the lower of the two ankles" includes heel
/// strike and toe off, and during those the ankle pivots over heel or toe
/// instead of travelling with the body. (2) A fixed fraction of the ankle's
/// vertical RANGE fixed the walk and broke the run. (3) A PERCENTILE of the
/// pooled heights fails for the opposite reason: a walk has its foot down
/// 60 % of the cycle and a sprint barely a third of that. (4) Two centimetres
/// around the ANKLE'S own minimum — the rule this wave inherited — is a true
/// statement about a foot applied to the wrong point: a running foot lands on
/// the BALL, so on this asset the jog's ankle sat 0.026 m above where it does
/// when standing while its toe was on the ground, and the band caught only
/// the fast samples either side of the pass. That read the jog as 5.35 m of
/// ground per cycle and the SPRINT as 0.698 m, which is not a small error but
/// an inverted one: the sprint clip covers more ground than the jog, and the
/// number said it covered an eighth.
///
/// Three centimetres about the CONTACT POINT is the same statement about a
/// foot, made about the part of the foot that is actually on the ground, and
/// it is deliberately a shade looser than the two the ankle rule used: the
/// contact point is a JOINT and not the sole, so it rides a centimetre or so
/// above the grass while the ball of the foot rolls over it.
constexpr float GRIP_TOLERANCE_M = 0.03f;

/// The fraction of the candidate stance samples the travel fit trusts. The
/// fit is a LEAST TRIMMED one (see fit_travel) and this is its trimming: a
/// plant begins and ends with the foot rolling, and those samples are exactly
/// the ones an ordinary mean lets ruin the estimate.
constexpr float TRAVEL_FIT_KEEP = 0.60f;

/// How many phase samples the load-time measurements use. A sprint has its
/// foot down for barely a fifth of its loop, so 96 is what puts a dozen
/// samples inside the shortest plant this asset has — which is the interval
/// every number below is actually about.
constexpr uint32_t MEASURE_SAMPLES = 96;

struct RoleNames {
    ClipRole role;
    std::string_view name;      ///< the role's own short name (for logs)
    std::array<std::string_view, 4> clips; ///< asset names, best first
};

/// THE NAME TABLE, and it is EXACT MATCH ONLY on purpose. The obvious
/// alternative — "does the clip name contain 'Idle'" — binds Idle to
/// `Idle_Talking_Loop` and Walk to `Walk_Formal_Loop` on this very asset,
/// which is a body that talks with its hands while you walk it down a street.
/// Comparison is case- and separator-insensitive, so `Jog_Fwd_Loop`,
/// `jogfwdloop` and `Jog Fwd Loop` are the same name; anything else is a new
/// row somebody writes down.
constexpr RoleNames ROLE_NAMES[] = {
    {ClipRole::Idle, "Idle",
     {"Idle_Loop", "Idle", "Idle_Stand", "Stand_Idle"}},
    {ClipRole::Walk, "Walk",
     {"Walk_Loop", "Walk_Fwd_Loop", "Walk", "Walking"}},
    {ClipRole::Jog, "Jog",
     {"Jog_Fwd_Loop", "Jog_Loop", "Jog", "Run_Loop"}},
    {ClipRole::Sprint, "Sprint",
     {"Sprint_Loop", "Sprint", "Run_Fwd_Loop", "Running"}},
    {ClipRole::JumpStart, "JumpStart",
     {"Jump_Start", "JumpStart", "Jump_Takeoff", "Jump"}},
    {ClipRole::JumpLoop, "JumpLoop",
     {"Jump_Loop", "JumpLoop", "Fall_Loop", "Falling"}},
    {ClipRole::JumpLand, "JumpLand",
     {"Jump_Land", "JumpLand", "Land", "Landing"}},
    {ClipRole::CrouchIdle, "CrouchIdle",
     {"Crouch_Idle_Loop", "CrouchIdle", "Crouch_Idle", "Crouching"}},
    {ClipRole::CrouchWalk, "CrouchWalk",
     {"Crouch_Fwd_Loop", "CrouchWalk", "Crouch_Walk", "Crouch_Walk_Loop"}},
    {ClipRole::Sit, "Sit",
     {"Sitting_Idle_Loop", "Sit_Idle_Loop", "Sitting", "Sit"}},
    // THE WEAPON GUARD. Not "any clip with Sword in the name": Sword_Attack
    // and Sword_Attack_RM are on this asset too, and a guard that swings is a
    // body that attacks whenever it stands still.
    {ClipRole::WeaponIdle, "WeaponIdle",
     {"Sword_Idle", "Sword_Idle_Loop", "Combat_Idle_Loop", "Weapon_Idle"}},
};
static_assert(std::size(ROLE_NAMES) == CLIP_ROLE_COUNT,
              "every role needs a row in the name table");

[[nodiscard]] bool same_name(std::string_view a, std::string_view b) {
    std::size_t i = 0;
    std::size_t j = 0;
    const auto skip = [](std::string_view s, std::size_t k) {
        while (k < s.size() && (s[k] == '_' || s[k] == ' ' || s[k] == '-'
                                || s[k] == '.')) {
            ++k;
        }
        return k;
    };
    for (;;) {
        i = skip(a, i);
        j = skip(b, j);
        if (i >= a.size() || j >= b.size()) {
            return i >= a.size() && j >= b.size();
        }
        const char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
        const char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[j])));
        if (ca != cb) {
            return false;
        }
        ++i;
        ++j;
    }
}

[[nodiscard]] float wrap01(float v) { return v - std::floor(v); }

/// The forward-in-time difference between two clip times, so an interpolation
/// across the loop point runs FORWARD instead of sweeping the whole clip
/// backwards. Without it every wrap is a visible rewind at 60 fps.
[[nodiscard]] float forward_delta(float from, float to, float duration) {
    if (duration <= 0.0f) {
        return 0.0f;
    }
    float d = to - from;
    if (d < -0.5f * duration) {
        d += duration;
    } else if (d > 0.5f * duration) {
        d -= duration;
    }
    return d;
}

[[nodiscard]] glm::quat scaled_rotation(const glm::quat& q, float scale) {
    const glm::quat identity{1.0f, 0.0f, 0.0f, 0.0f};
    if (std::abs(scale - 1.0f) < 1e-4f) {
        return q;
    }
    // slerp EXTRAPOLATES correctly past 1 as long as the pair is on the same
    // hemisphere, which is exactly what a stride wider than the clip's needs.
    glm::quat b = q;
    if (glm::dot(identity, b) < 0.0f) {
        b = -b;
    }
    return glm::normalize(glm::slerp(identity, b, scale));
}

constexpr Bone LEG_BONES[] = {Bone::ThighL, Bone::ShinL, Bone::FootL,
                              Bone::ThighR, Bone::ShinR, Bone::FootR};

} // namespace

std::string_view role_name(ClipRole r) {
    for (const RoleNames& row : ROLE_NAMES) {
        if (row.role == r) {
            return row.name;
        }
    }
    return "?";
}

ClipRole role_for_gait(Gait gait) {
    switch (gait) {
    case Gait::Walk: return ClipRole::Walk;
    case Gait::Jog: return ClipRole::Jog;
    case Gait::Run: return ClipRole::Sprint;
    }
    return ClipRole::Walk;
}

void sample_clip_pose(const skel::Skeleton& skeleton, const skel::AnimClip& clip,
                      float time_s, std::span<JointLocal> out) {
    const std::size_t n = std::min(skeleton.size(), out.size());
    if (n == 0) {
        return;
    }
    std::vector<glm::vec3> t(skeleton.size());
    std::vector<glm::quat> r(skeleton.size());
    std::vector<glm::vec3> sc(skeleton.size());
    skel::sample_clip(skeleton, clip, time_s, t, r, sc);
    for (std::size_t i = 0; i < n; ++i) {
        out[i].translation = t[i];
        out[i].rotation = r[i];
        out[i].scale = sc[i];
    }
}

void blend_local(std::span<const JointLocal> a, std::span<const JointLocal> b,
                 float weight, std::span<JointLocal> out) {
    const float w = std::clamp(weight, 0.0f, 1.0f);
    const std::size_t n = std::min({a.size(), b.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        out[i].translation = glm::mix(a[i].translation, b[i].translation, w);
        out[i].scale = glm::mix(a[i].scale, b[i].scale, w);
        glm::quat qb = b[i].rotation;
        if (glm::dot(a[i].rotation, qb) < 0.0f) {
            qb = -qb;
        }
        out[i].rotation = glm::normalize(glm::slerp(a[i].rotation, qb, w));
    }
}

void scale_sample_stride(const SkinnedRigBinding& binding,
                         const skel::Skeleton& skeleton, float scale,
                         std::span<JointLocal> sample) {
    if (std::abs(scale - 1.0f) < 1e-4f) {
        return;
    }
    for (const Bone b : LEG_BONES) {
        const int32_t ji = binding.names.joint[bone_index(b)];
        if (ji < 0 || static_cast<std::size_t>(ji) >= sample.size()) {
            continue;
        }
        const auto j = static_cast<std::size_t>(ji);
        const glm::quat bind = glm::normalize(skeleton.joints[j].bind_rotation);
        const glm::quat deviation =
            glm::inverse(bind) * glm::normalize(sample[j].rotation);
        sample[j].rotation = glm::normalize(bind * scaled_rotation(deviation, scale));
    }
}

float stride_scale_at(uint32_t i) {
    const float u = float(std::min(i, STRIDE_CURVE_POINTS - 1u))
                    / float(STRIDE_CURVE_POINTS - 1);
    return STRIDE_SCALE_MIN * std::pow(STRIDE_SCALE_MAX / STRIDE_SCALE_MIN, u);
}

float stride_curve_index(float scale) {
    const float s = std::clamp(scale, STRIDE_SCALE_MIN, STRIDE_SCALE_MAX);
    return std::log(s / STRIDE_SCALE_MIN)
           / std::log(STRIDE_SCALE_MAX / STRIDE_SCALE_MIN)
           * float(STRIDE_CURVE_POINTS - 1);
}

float stride_scale_for(const ClipEntry& entry, float target_m) {
    const auto& c = entry.stride_curve;
    if (!entry.present() || target_m <= 0.0f || entry.stride_valid == 0) {
        return 1.0f;
    }
    // THE NEAREST POINT ON THE CURVE, not the first crossing of the target.
    // The curve is monotone while the leg has room and folds over once it runs
    // out (a thigh scaled past its swing starts carrying the foot BACK), and a
    // first-crossing search on a folded curve answers with the far side of the
    // fold: measured on the sprint, it asked for 1.14 where 2.4 was nearer.
    uint32_t best = STRIDE_CURVE_POINTS;
    float best_err = std::numeric_limits<float>::max();
    for (uint32_t i = 0; i < STRIDE_CURVE_POINTS; ++i) {
        if (!entry.curve_has(i)) {
            continue;
        }
        const float err = std::abs(c[i] - target_m);
        if (err < best_err) {
            best_err = err;
            best = i;
        }
    }
    if (best >= STRIDE_CURVE_POINTS) {
        return 1.0f;
    }
    // Refine INSIDE the winning cell, toward whichever neighbour brackets the
    // target: a twelve-point grid is 0.25 wide and that is visible on a stride.
    // Returns a + u, u in [0,1]: how far from cell a toward cell b the target
    // sits. The CALLER turns that fraction into a grid position, because the
    // two cells need not be adjacent once holes are stepped over.
    const auto refine = [&](uint32_t a, uint32_t b) {
        const float span = c[b] - c[a];
        if (std::abs(span) < 1e-6f) {
            return float(a);
        }
        const float u = std::clamp((target_m - c[a]) / span, 0.0f, 1.0f);
        return float(a) + u;
    };
    // The neighbour a refinement uses is the nearest MEASURED cell on that
    // side, so a hole is stepped over instead of being interpolated through.
    const auto neighbour = [&](int32_t from, int32_t dir) {
        for (int32_t i = from + dir; i >= 0 && i < int32_t(STRIDE_CURVE_POINTS);
             i += dir) {
            if (entry.curve_has(uint32_t(i))) {
                return i;
            }
        }
        return -1;
    };
    float at = float(best);
    const int32_t below = neighbour(int32_t(best), -1);
    const int32_t above = neighbour(int32_t(best), +1);
    if (below >= 0
        && (target_m - c[best]) * (c[uint32_t(below)] - c[best]) > 0.0f) {
        const float u = refine(best, uint32_t(below)) - float(best);
        at = float(best) + u * (float(below) - float(best));
    } else if (above >= 0
               && (target_m - c[best]) * (c[uint32_t(above)] - c[best]) > 0.0f) {
        const float u = refine(best, uint32_t(above)) - float(best);
        at = float(best) + u * (float(above) - float(best));
    }
    // INTERPOLATED IN THE GRID'S OWN COORDINATE, which is the logarithm of
    // the scale: the cells are geometric, so a half-cell is a square root and
    // not a half-sum.
    const float u = std::clamp(at, 0.0f, float(STRIDE_CURVE_POINTS - 1))
                    / float(STRIDE_CURVE_POINTS - 1);
    return std::clamp(STRIDE_SCALE_MIN
                          * std::pow(STRIDE_SCALE_MAX / STRIDE_SCALE_MIN, u),
                      STRIDE_SCALE_MIN, STRIDE_SCALE_MAX);
}

namespace {

/// One cycle of a clip as the world track of every CONTACT JOINT the model
/// has — the two ankles and, on an asset that has them, the two toes. THE
/// SAME SAMPLING PATH THE FRAME USES, so the measurement cannot describe a
/// body the renderer does not draw.
///
/// ONE TRACK PER JOINT, AND THAT IS NOT AN IMPLEMENTATION DETAIL. The first
/// version of this wave tracked "the lowest contact point of the foot", which
/// reads beautifully and is a moving target: as the foot rolls onto the ball
/// the lowest point SWITCHES from the ankle to the toe, and those two are a
/// tenth of a metre apart. The switch is a teleport, and a prober whose own
/// reference teleports reported 3.9 cm of foot slide on the walk where the
/// honest figure is 1.6. A contact point has to be a POINT.
struct ContactTrack {
    std::vector<std::vector<glm::vec3>> joint; ///< [track][sample], model space
    std::vector<uint8_t> side;                 ///< which foot each track belongs to
    std::vector<char> ankle;                   ///< 1 for the foot joint itself
    std::vector<float> rest_y;                 ///< where each track stands at rest
    std::size_t sample_count = 0;
    /// The lift that puts the DEEPEST contact of the cycle exactly on the
    /// ground it belongs on. Applied by every reader below, so a clip is
    /// judged in the place it will be drawn and not in the place it shipped.
    float lift = 0.0f;

    [[nodiscard]] std::size_t samples() const { return sample_count; }
    [[nodiscard]] std::size_t tracks() const { return joint.size(); }
    /// True while track `c` is on the ground at sample `k`.
    [[nodiscard]] bool down(std::size_t c, std::size_t k, float grip) const {
        return joint[c][k].y + lift - rest_y[c] <= grip;
    }
};

/// A GEAR'S POSE SOURCE: one clip, or two blended at a fixed weight with their
/// PLANTS ALIGNED. Aligning the plants is the whole of the second half — two
/// locomotion clips blended at equal clip time put a left footfall on top of a
/// right one and produce a body that shuffles in place.
struct MixSource {
    const skel::AnimClip* a = nullptr;
    float dur_a = 0.0f;
    float plant_a = 0.0f;
    const skel::AnimClip* b = nullptr;
    float dur_b = 0.0f;
    float plant_b = 0.0f;
    float weight = 0.0f;
    [[nodiscard]] bool blended() const { return b != nullptr && weight > 0.0f; }
};

/// The source at cycle phase `p`, where p is a fraction of clip A's own loop.
/// Clip B is sampled at the instant its OWN plant is the same distance away.
void sample_mix(const skel::Skeleton& skeleton, const MixSource& src, float p,
                std::span<JointLocal> out, std::vector<JointLocal>& scratch) {
    if (src.a == nullptr) {
        return;
    }
    sample_clip_pose(skeleton, *src.a, wrap01(p) * src.dur_a, out);
    if (!src.blended()) {
        return;
    }
    scratch.assign(skeleton.size(), JointLocal{});
    sample_clip_pose(skeleton, *src.b, wrap01(src.plant_b + p - src.plant_a) * src.dur_b,
                     scratch);
    blend_local(out.first(skeleton.size()), scratch, src.weight, out.first(skeleton.size()));
}

[[nodiscard]] ContactTrack track_contacts(const skel::Skeleton& skeleton,
                                          const SkinnedRigBinding& binding,
                                          const ContactSet& contacts,
                                          const MixSource& src, float scale,
                                          uint32_t samples) {
    ContactTrack track;
    if (!contacts.valid() || src.a == nullptr || src.dur_a <= 0.0f || samples < 4) {
        return track;
    }
    std::vector<int32_t> joints;
    for (int s = 0; s < 2; ++s) {
        const FootContacts& fc = contacts.side[static_cast<std::size_t>(s)];
        for (uint32_t c = 0; c < fc.count; ++c) {
            joints.push_back(fc.joint[c]);
            track.side.push_back(static_cast<uint8_t>(s));
            track.ankle.push_back(c == 0 ? 1 : 0);
            track.rest_y.push_back(fc.rest_y[c]);
        }
    }
    track.joint.assign(joints.size(), {});
    track.sample_count = samples;
    std::vector<JointLocal> sample(skeleton.size());
    std::vector<glm::mat4> local(skeleton.size());
    std::vector<glm::mat4> model(skeleton.size());
    for (auto& t : track.joint) {
        t.reserve(samples + 1);
    }
    // The last entry REPEATS the first: every consumer below walks pairs, and
    // a plant that straddles the loop point is the normal case, not the edge.
    std::vector<JointLocal> scratch;
    for (uint32_t k = 0; k <= samples; ++k) {
        const float p = float(k % samples) / float(samples);
        sample_mix(skeleton, src, p, sample, scratch);
        scale_sample_stride(binding, skeleton, scale, sample);
        for (std::size_t j = 0; j < skeleton.size(); ++j) {
            local[j] = glm::translate(glm::mat4{1.0f}, sample[j].translation)
                       * glm::mat4_cast(glm::normalize(sample[j].rotation))
                       * glm::scale(glm::mat4{1.0f}, sample[j].scale);
        }
        skel::skeleton_model_matrices(skeleton, local, model);
        for (std::size_t c = 0; c < joints.size(); ++c) {
            track.joint[c].push_back(
                glm::vec3{model[static_cast<std::size_t>(joints[c])][3]});
        }
    }
    // THE LIFT IS PART OF THE TRACK, not a decoration on top of it: every
    // question below ("is this foot down", "how far did it slide") is about
    // the body as it will be DRAWN, and the body as it will be drawn is
    // lifted. The deepest contact of the whole cycle is put exactly on the
    // ground its own rest pose stands on; nothing ends up below it.
    for (std::size_t c = 0; c < track.joint.size(); ++c) {
        for (uint32_t k = 0; k < samples; ++k) {
            track.lift = std::max(track.lift, track.rest_y[c] - track.joint[c][k].y);
        }
    }
    return track;
}

/// Every sample where a contact JOINT is down and STAYS down into the next
/// sample. Per joint, because a plant is a statement about one point.
struct Candidate {
    uint32_t track;
    uint32_t k;
};
[[nodiscard]] std::vector<Candidate> stance_candidates(const ContactTrack& track) {
    std::vector<Candidate> out;
    const std::size_t n = track.samples();
    for (std::size_t c = 0; c < track.tracks(); ++c) {
        for (std::size_t k = 0; k < n; ++k) {
            if (track.down(c, k, GRIP_TOLERANCE_M)
                && track.down(c, k + 1, GRIP_TOLERANCE_M)) {
                out.push_back({static_cast<uint32_t>(c), static_cast<uint32_t>(k)});
            }
        }
    }
    return out;
}

/// WHAT THE CLIP SAYS THE BODY COVERED, and it is a FIT rather than an
/// average.
///
/// The statement being fitted is the only thing anybody actually wants from a
/// locomotion clip: WHILE A FOOT IS PLANTED IT DOES NOT MOVE IN THE WORLD. In
/// model space the planted contact point slides backwards; add the body's own
/// travel and the sum must be zero. So for a candidate travel D the residual
/// of one sample is |dx| where dx is that sum, and D is the value that makes
/// the residuals smallest.
///
/// SMALLEST IN THE LEAST-TRIMMED SENSE, not the least-squares one, and that
/// is the whole difference between this and the mean the file used to take.
/// A plant does not begin and end cleanly: the foot rolls onto the ball and
/// off it, and those samples move several centimetres each while the middle
/// of the plant moves a millimetre. A mean lets the roll set the answer — on
/// the jog it set it to 5.35 m against the 5.66 m the plant itself says, and
/// on the sprint to 0.698 m against 5.94 m. Keeping the best 60 % of the
/// candidates and ignoring the rest is the standard cure and it is honest
/// about what it is doing: the residual it reports is the residual of the
/// samples it kept, so a clip with no real plant reports a large one instead
/// of a plausible stride.
///
/// THE DIRECTION IS MEASURED TOO. It is the mean backward motion of the
/// planted feet, normalised — not our -Z convention restated, which would
/// make a mis-oriented import read as a body that covers no ground.
struct TravelFit {
    float travel_m = 0.0f;
    float residual_m = 0.0f; ///< per kept sample, metres
    float duty = 0.0f;       ///< fraction of the cycle a foot is down
    glm::vec2 direction{0.0f}; ///< the way the BODY goes, xz, unit or zero
};

[[nodiscard]] TravelFit fit_travel(const ContactTrack& track) {
    TravelFit out;
    const std::size_t n = track.samples();
    if (n < 4) {
        return out;
    }
    const std::vector<Candidate> cand = stance_candidates(track);
    out.duty = track.tracks() > 0
                   ? float(cand.size()) / float(track.tracks() * n)
                   : 0.0f;
    if (cand.size() < 3) {
        return out;
    }
    glm::vec2 back{0.0f};
    for (const Candidate& c : cand) {
        const std::vector<glm::vec3>& t = track.joint[c.track];
        back -= glm::vec2{t[c.k + 1].x - t[c.k].x, t[c.k + 1].z - t[c.k].z};
    }
    if (glm::length(back) < 1e-6f) {
        return out;
    }
    const glm::vec2 dir = glm::normalize(back);
    out.direction = dir;
    const std::size_t keep =
        std::max<std::size_t>(3, std::size_t(TRAVEL_FIT_KEEP * float(cand.size())));
    // A LINEAR SCAN over the whole plausible range rather than a descent: the
    // trimmed cost is piecewise linear with kinks, and a descent on it stops
    // at the first kink it meets.
    constexpr float TRAVEL_MAX_M = 12.0f;
    constexpr uint32_t TRAVEL_STEPS = 600;
    std::vector<float> err;
    err.reserve(cand.size());
    float best = std::numeric_limits<float>::max();
    for (uint32_t i = 0; i <= TRAVEL_STEPS; ++i) {
        const float d = TRAVEL_MAX_M * float(i) / float(TRAVEL_STEPS);
        const float per = d / float(n);
        err.clear();
        for (const Candidate& c : cand) {
            const std::vector<glm::vec3>& t = track.joint[c.track];
            const glm::vec2 world{t[c.k + 1].x - t[c.k].x + dir.x * per,
                                  t[c.k + 1].z - t[c.k].z + dir.y * per};
            err.push_back(glm::length(world));
        }
        std::nth_element(err.begin(), err.begin() + std::ptrdiff_t(keep - 1), err.end());
        float sum = 0.0f;
        for (std::size_t q = 0; q < keep; ++q) {
            sum += err[q];
        }
        if (sum < best) {
            best = sum;
            out.travel_m = d;
            out.residual_m = sum / float(keep);
        }
    }
    return out;
}

/// The phase at the MIDDLE OF THE LEFT FOOT'S PLANT, which is what the gait
/// contract calls "the left foot's lowest point": a planted foot has no single
/// lowest sample, it has a plateau, and an argmin over a plateau picks
/// whichever end quantisation noise favours. The run is found with a wrap, so
/// a clip whose plant straddles the loop point is not cut in two. THE FOOT IS
/// DOWN WHEN ANY OF ITS CONTACT JOINTS IS: a heel strike and a toe-off are
/// the same plant seen through two different joints.
[[nodiscard]] float left_plant_phase(const ContactTrack& track) {
    const std::size_t n = track.samples();
    if (n < 2) {
        return 0.0f;
    }
    std::vector<char> down(n, 0);
    for (std::size_t c = 0; c < track.tracks(); ++c) {
        if (track.side[c] != 0) {
            continue;
        }
        for (std::size_t k = 0; k < n; ++k) {
            down[k] = down[k] || track.down(c, k, GRIP_TOLERANCE_M) ? 1 : 0;
        }
    }
    std::size_t best_start = 0;
    std::size_t best_len = 0;
    for (std::size_t start = 0; start < n; ++start) {
        if (down[start] == 0 || down[(start + n - 1) % n] != 0) {
            continue; // not the first sample of a run
        }
        std::size_t len = 0;
        while (len < n && down[(start + len) % n] != 0) {
            ++len;
        }
        if (len > best_len) {
            best_len = len;
            best_start = start;
        }
    }
    if (best_len == 0) {
        return 0.0f;
    }
    return wrap01((float(best_start) + 0.5f * float(best_len - 1)) / float(n));
}

} // namespace

ContactSet build_contacts(const Rig& rig, const skel::Skeleton& skeleton,
                          const SkinnedRigBinding& binding) {
    ContactSet out;
    const Bone feet[2] = {Bone::FootL, Bone::FootR};
    for (int s = 0; s < 2; ++s) {
        FootContacts& fc = out.side[static_cast<std::size_t>(s)];
        const int32_t foot = binding.names.joint[bone_index(feet[static_cast<std::size_t>(s)])];
        if (foot < 0) {
            continue;
        }
        fc.joint[fc.count++] = foot;
        for (std::size_t j = 0; j < skeleton.size() && fc.count < fc.joint.size(); ++j) {
            if (skeleton.joints[j].parent == foot) {
                fc.joint[fc.count++] = static_cast<int32_t>(j);
            }
        }
    }
    if (!out.valid()) {
        return out;
    }
    // THE REST POSE IS THE GROUND TRUTH, and it is read through the same FK
    // the clips are: the importer put this model's soles on y = 0 in ITS rest
    // pose, so the height its contact joints sit at there is the height a
    // planted foot is supposed to sit at, whatever a clip's author assumed.
    std::vector<JointLocal> sample(skeleton.size());
    std::vector<glm::mat4> local(skeleton.size());
    std::vector<glm::mat4> model(skeleton.size());
    pose_local_transforms(rig, skeleton, binding, LocalPose{}, sample);
    for (std::size_t j = 0; j < skeleton.size(); ++j) {
        local[j] = glm::translate(glm::mat4{1.0f}, sample[j].translation)
                   * glm::mat4_cast(glm::normalize(sample[j].rotation))
                   * glm::scale(glm::mat4{1.0f}, sample[j].scale);
    }
    skel::skeleton_model_matrices(skeleton, local, model);
    out.rest_y = std::numeric_limits<float>::max();
    for (FootContacts& fc : out.side) {
        for (uint32_t c = 0; c < fc.count; ++c) {
            fc.rest_y[c] = model[static_cast<std::size_t>(fc.joint[c])][3][1];
            out.rest_y = std::min(out.rest_y, fc.rest_y[c]);
        }
    }
    return out;
}

float ground_lift_for(const ClipEntry& entry, float scale) {
    if (!entry.present()) {
        return 0.0f;
    }
    const float at = stride_curve_index(scale);
    const auto lo = static_cast<uint32_t>(at);
    const uint32_t hi = std::min(lo + 1u, STRIDE_CURVE_POINTS - 1u);
    const float u = at - float(lo);
    return glm::mix(entry.ground_curve[lo], entry.ground_curve[hi], u);
}

namespace {

/// The pose source an entry describes: its clip, plus its blend partner when
/// it has one. One place builds it, so the frame and every measurement below
/// cannot disagree about what a gear plays (Rule 35).
[[nodiscard]] MixSource mix_of(const ClipEntry& entry,
                               std::span<const skel::AnimClip> clips) {
    MixSource src;
    if (!entry.present()) {
        return src;
    }
    src.a = &clips[static_cast<std::size_t>(entry.clip)];
    src.dur_a = entry.duration_s;
    src.plant_a = entry.footfall_phase;
    if (entry.mixed()) {
        src.b = &clips[static_cast<std::size_t>(entry.mix_clip)];
        src.dur_b = entry.mix_duration_s;
        src.plant_b = entry.mix_footfall;
        src.weight = entry.mix_weight;
    }
    return src;
}

/// EVERYTHING MEASUREMENT KNOWS ABOUT ONE POSE SOURCE: its native travel, the
/// phase its left foot plants at, how still that plant is, and the two curves
/// over the stride grid. Written into `entry` so a blended gear is measured by
/// exactly the code a solo one is.
void measure_travel(const skel::Skeleton& skeleton, const SkinnedRigBinding& binding,
                    const ContactSet& contacts, const MixSource& src, ClipEntry& entry) {
    const ContactTrack base =
        track_contacts(skeleton, binding, contacts, src, 1.0f, MEASURE_SAMPLES);
    const TravelFit native = fit_travel(base);
    entry.cycle_m = native.travel_m;
    entry.plant_residual_m = native.residual_m;
    entry.duty = native.duty;
    entry.footfall_phase = left_plant_phase(base);
    entry.stride_valid = 0;
    for (uint32_t i = 0; i < STRIDE_CURVE_POINTS; ++i) {
        const float sc = stride_scale_at(i);
        const ContactTrack t =
            track_contacts(skeleton, binding, contacts, src, sc, MEASURE_SAMPLES);
        const TravelFit f = fit_travel(t);
        entry.stride_curve[i] = f.travel_m;
        entry.ground_curve[i] = t.lift;
        if (f.travel_m > 0.0f) {
            entry.stride_valid |= 1u << i;
        }
    }
}

} // namespace

ClipLibrary build_clip_library(const Rig& rig, const skel::Skeleton& skeleton,
                               const SkinnedRigBinding& binding,
                               std::span<const skel::AnimClip> clips) {
    ClipLibrary lib;
    lib.contacts = build_contacts(rig, skeleton, binding);
    for (const RoleNames& row : ROLE_NAMES) {
        ClipEntry& entry = lib.role[role_index(row.role)];
        for (const std::string_view want : row.clips) {
            if (want.empty()) {
                continue;
            }
            for (std::size_t c = 0; c < clips.size(); ++c) {
                if (same_name(clips[c].name, want)) {
                    entry.clip = static_cast<int32_t>(c);
                    entry.duration_s = clips[c].duration_s;
                    break;
                }
            }
            if (entry.present()) {
                break;
            }
        }
        if (!entry.present()) {
            continue;
        }
        ++lib.resolved;
    }
    // THE STRIDE MEASUREMENTS. Only the roles that travel need them: asking
    // for a stride curve on Idle produces a row of zeros that every reader
    // would then have to special-case.
    const ClipRole travelling[] = {ClipRole::Walk, ClipRole::Jog, ClipRole::Sprint,
                                   ClipRole::CrouchWalk};
    for (const ClipRole r : travelling) {
        ClipEntry& entry = lib.role[role_index(r)];
        if (!entry.present() || entry.duration_s <= 0.0f) {
            continue;
        }
        const skel::AnimClip& clip = clips[static_cast<std::size_t>(entry.clip)];
        measure_travel(skeleton, binding, lib.contacts, mix_of(entry, clips), entry);
        // A CURVE THAT STOPS IS STILL A CURVE, but a curve that never started
        // is a clip whose feet this model cannot read, and that is said out
        // loud rather than drawn as a body sliding down a street.
        if (entry.stride_valid == 0) {
            std::fprintf(stderr,
                         "[anim] clip \"%s\": no foot plant found at any stride "
                         "scale — its stride CANNOT be matched to sim's, and the "
                         "body will slide. Check that the model's feet reach the "
                         "ground in this clip.\n",
                         clip.name.c_str());
        }
    }
    // THE GEAR PLAYS THE CLIP THAT PLANTS BEST AT ITS OWN STRIDE, and not the
    // clip whose NAME matches the gear's. Names are the asset author's guess
    // at what a clip is for; the stride it was authored at is a fact, and the
    // two need not agree. Measured on this asset (metres of planted-foot slide
    // per step, at the stride each gear demands):
    //
    //     gear            Walk_Loop   Jog_Fwd_Loop   Sprint_Loop
    //     WALK  1.8 m/s    [0.035]       0.237          0.046
    //     JOG   3.0 m/s     0.033       [0.274]         0.068
    //     RUN   6.0 m/s     0.068       [0.027]         0.133
    //                                    ^ chosen
    //
    // The walk and the jog keep the clip their name points at. The RUN does
    // not: Quaternius put `Sprint_Loop` at about 9 m/s and `Jog_Fwd_Loop` at
    // about 6 — which is our RUN_SPEED almost exactly — so the gear named
    // "sprint" is best served by the clip named "jog", at a stride scale of
    // 0.81 instead of the 0.80 the sprint needs and five times less slide.
    //
    // AND THE JOG KEEPS ITS OWN CLIP THOUGH THE WALK'S NUMBER IS BETTER,
    // which is the scale window below doing its job: 3 m/s is the one gear
    // this asset has no clip near, and reaching it from Walk_Loop means
    // stretching a walk to 1.97 of its stride. That is not a jog with a good
    // number, it is a walk with a good number.
    //
    // A CLIP MUST BE CLEARLY BETTER TO TAKE A ROLE, not merely better: the
    // factor of two below is what keeps a measurement's noise from
    // reshuffling the gears between two runs of the same build. On a tie the
    // name wins, because the name is a decision somebody made.
    struct GearRow {
        ClipRole role;
        float speed;
    };
    const GearRow gears[] = {
        {ClipRole::Walk, static_cast<float>(config::WALK_SPEED)},
        {ClipRole::Jog, static_cast<float>(config::JOG_SPEED)},
        {ClipRole::Sprint, static_cast<float>(config::RUN_SPEED)},
    };
    constexpr float SWAP_MARGIN = 2.0f;
    // AND IT MUST STILL BE THE MOTION ITS AUTHOR DREW. A stride scale is how
    // far this file bends a clip away from what was animated, and past some
    // bend the clip stops being evidence about anything: at 0.41 the jog's
    // legs are nearly straight and the feet skim, at 1.97 the walk is a
    // stride-length nobody walks. Either can still win on slide alone —
    // measured, the stretched walk slides 0.033 m against the shrunk jog's
    // 0.274 — and letting it win would be choosing a body by a number that
    // does not describe the thing being chosen. So a candidate may only take
    // a role while its own scale stays inside this window; outside it, the
    // gear keeps the clip its name points at and the slide is reported as the
    // tail it is.
    constexpr float SWAP_SCALE_MIN = 0.6f;
    constexpr float SWAP_SCALE_MAX = 1.6f;
    std::array<int32_t, CLIP_ROLE_COUNT> adopt{};
    adopt.fill(-1);
    std::array<int32_t, CLIP_ROLE_COUNT> named{};
    named.fill(-1);
    std::array<float, CLIP_ROLE_COUNT> named_slide{};
    for (const GearRow& g : gears) {
        const ClipEntry& own = lib[g.role];
        if (!own.present()) {
            continue;
        }
        // sim's own length(v) model, one reader of two registry rows — the
        // same pair the test restates and for the same DAG reason.
        const float step = static_cast<float>(config::STEP_LENGTH_BASE)
                           + static_cast<float>(config::STEP_LENGTH_PER_MPS) * g.speed;
        const FootSlide mine = measure_foot_slide(skeleton, binding, clips, lib,
                                                 g.role, step, true, MEASURE_SAMPLES);
        float best = mine.worst_per_step_m;
        ClipRole winner = g.role;
        for (const ClipRole r : travelling) {
            if (r == g.role || r == ClipRole::CrouchWalk || !lib.has(r)) {
                continue;
            }
            const float sc = stride_scale_for(lib[r], 2.0f * step);
            if (sc < SWAP_SCALE_MIN || sc > SWAP_SCALE_MAX) {
                continue;
            }
            const FootSlide other = measure_foot_slide(skeleton, binding, clips, lib, r,
                                                       step, true, MEASURE_SAMPLES);
            if (other.worst_per_step_m * SWAP_MARGIN < best) {
                best = other.worst_per_step_m;
                winner = r;
            }
        }
        if (winner != g.role) {
            adopt[role_index(g.role)] = static_cast<int32_t>(role_index(winner));
            named[role_index(g.role)] = own.clip;
            named_slide[role_index(g.role)] = mine.worst_per_step_m;
            std::fprintf(stderr,
                         "[anim] gear %s: \"%s\" slides %.3f m/step at this "
                         "stride, \"%s\" only %.3f — the %s role takes the %s "
                         "clip\n",
                         role_name(g.role).data(),
                         clips[static_cast<std::size_t>(own.clip)].name.c_str(),
                         static_cast<double>(mine.worst_per_step_m),
                         clips[static_cast<std::size_t>(lib[winner].clip)].name.c_str(),
                         static_cast<double>(best), role_name(g.role).data(),
                         role_name(winner).data());
        }
    }
    // FROM A SNAPSHOT, so two gears adopting in the same pass cannot chain:
    // assigning in place once made the sprint adopt the jog's SLOT after the
    // jog had already been overwritten, and every gear ended up playing the
    // walk.
    const std::array<ClipEntry, CLIP_ROLE_COUNT> before_adoption = lib.role;
    for (uint32_t r = 0; r < CLIP_ROLE_COUNT; ++r) {
        if (adopt[r] >= 0) {
            lib.role[r] = before_adoption[static_cast<std::size_t>(adopt[r])];
            lib.role[r].named_clip = named[r];
            lib.role[r].named_slide_m = named_slide[r];
        }
    }
    // A GEAR WITH NO CLIP NEAR IT GETS A BLEND OF THE TWO IT HAS.
    //
    // The swap above answers "which single clip is this gear best served by".
    // For 3 m/s on this asset the honest answer is NEITHER: the jog clip has
    // to be shrunk to 0.42 (legs straighten, feet skim, and grounding it lifts
    // the body 0.17 m — the "figure grows 18 cm on the gear change" the owner
    // reported) and the walk clip has to be stretched to 1.84 (a stride nobody
    // walks). Blending them at the weight whose MEASURED cycle equals the one
    // sim asks for leaves the stride scale near 1: neither clip is bent, and
    // the ground lift falls out with the bend.
    //
    // THE WEIGHT IS A ROOT, NOT A TASTE. It is solved by scanning, because the
    // blend of two poses is not linear in the ground either pose covers, and
    // the scan reports what it found rather than assuming monotonicity.
    //
    // AND IT ONLY HAPPENS WHEN THE SOLO CLIP IS ALREADY OUT OF ITS WINDOW.
    // A gear whose own clip sits inside [SWAP_SCALE_MIN, SWAP_SCALE_MAX] is a
    // gear being adjusted, and blending it would trade a clip somebody drew
    // for an average of two.
    constexpr uint32_t MIX_STEPS = 20;
    for (const GearRow& g : gears) {
        ClipEntry& entry = lib.role[role_index(g.role)];
        if (!entry.present() || entry.duration_s <= 0.0f) {
            continue;
        }
        const float step = static_cast<float>(config::STEP_LENGTH_BASE)
                           + static_cast<float>(config::STEP_LENGTH_PER_MPS) * g.speed;
        const float demanded = 2.0f * step;
        const float solo_scale = stride_scale_for(entry, demanded);
        if (solo_scale >= SWAP_SCALE_MIN && solo_scale <= SWAP_SCALE_MAX) {
            continue;
        }
        const FootSlide solo = measure_foot_slide(skeleton, binding, clips, lib, g.role,
                                                  step, true, MEASURE_SAMPLES);
        const ClipEntry solo_entry = entry;
        ClipEntry best = entry;
        float best_err = std::numeric_limits<float>::max();
        for (const ClipRole r : travelling) {
            if (r == g.role || r == ClipRole::CrouchWalk || !lib.has(r)) {
                continue;
            }
            const ClipEntry& partner = lib[r];
            if (partner.clip == entry.clip || partner.duration_s <= 0.0f) {
                continue;
            }
            for (uint32_t i = 1; i < MIX_STEPS; ++i) {
                ClipEntry trial = solo_entry;
                trial.mix_clip = partner.clip;
                trial.mix_duration_s = partner.duration_s;
                trial.mix_footfall = partner.footfall_phase;
                trial.mix_weight = float(i) / float(MIX_STEPS);
                measure_travel(skeleton, binding, lib.contacts, mix_of(trial, clips),
                               trial);
                if (trial.stride_valid == 0 || trial.cycle_m <= 0.0f) {
                    continue;
                }
                const float err = std::abs(trial.cycle_m - demanded);
                if (err < best_err) {
                    best_err = err;
                    best = trial;
                }
            }
        }
        if (!best.mixed()) {
            continue;
        }
        entry = best;
        const FootSlide mixed = measure_foot_slide(skeleton, binding, clips, lib, g.role,
                                                   step, true, MEASURE_SAMPLES);
        const float mixed_scale = stride_scale_for(entry, demanded);
        // THE BLEND HAS TO EARN THE ROLE. If it does not plant better than the
        // clip it replaced, the clip stays: a body that skates on an average
        // of two animations is worse than one that skates on one.
        if (mixed.worst_per_step_m >= solo.worst_per_step_m) {
            entry = solo_entry;
            std::fprintf(stderr,
                         "[anim] gear %s: no blend beat the solo clip "
                         "(%.3f m/step) — keeping it\n",
                         role_name(g.role).data(),
                         static_cast<double>(solo.worst_per_step_m));
            continue;
        }
        entry.mix_solo_slide_m = solo.worst_per_step_m;
        entry.mix_solo_lift_m = ground_lift_for(solo_entry, solo_scale);
        std::fprintf(stderr,
                     "[anim] gear %s has no clip near it: \"%s\" alone needs stride "
                     "%.2f and slides %.3f m/step. Blended %.0f%% into \"%s\": stride "
                     "%.2f, slide %.3f m/step, ground lift %.3f -> %.3f m\n",
                     role_name(g.role).data(),
                     clips[static_cast<std::size_t>(solo_entry.clip)].name.c_str(),
                     static_cast<double>(solo_scale),
                     static_cast<double>(solo.worst_per_step_m),
                     static_cast<double>(100.0f * entry.mix_weight),
                     clips[static_cast<std::size_t>(entry.mix_clip)].name.c_str(),
                     static_cast<double>(mixed_scale),
                     static_cast<double>(mixed.worst_per_step_m),
                     static_cast<double>(entry.mix_solo_lift_m),
                     static_cast<double>(ground_lift_for(entry, mixed_scale)));
    }
    // THE STANDING ROLES STILL HAVE TO STAND ON THE GROUND, and they get ONE
    // lift, measured at scale 1 and written into every cell of the curve so
    // ground_lift_for needs no special case for them.
    //
    // THE JUMP TRIPLE AND THE SEAT ARE NOT ON THIS LIST, deliberately. A jump
    // is a body that is SUPPOSED to leave the ground: grounding its lowest
    // sample would drag the whole arc down by the height of the arc, which is
    // the one thing a jump may not lose. A seated body's feet hang where the
    // bench puts them and the seat, not the ground, is what they answer to.
    // Absence here is a decision, and the list says so rather than a filter
    // that happens to exclude them.
    const ClipRole standing[] = {ClipRole::Idle, ClipRole::CrouchIdle};
    for (const ClipRole r : standing) {
        ClipEntry& entry = lib.role[role_index(r)];
        if (!entry.present() || entry.duration_s <= 0.0f) {
            continue;
        }
        entry.ground_curve.fill(track_contacts(skeleton, binding, lib.contacts,
                                               mix_of(entry, clips), 1.0f,
                                               MEASURE_SAMPLES)
                                    .lift);
    }
    // THE TWO LAYERS. The mask is pure structure and needs nothing measured;
    // the arm layer is solved against the asset's OWN IDLE, because that is
    // the pose the owner was looking at when he called it a combat stance.
    lib.mask = build_branch_mask(skeleton, binding);
    const ClipEntry& idle = lib[ClipRole::Idle];
    if (idle.present()) {
        std::vector<JointLocal> reference(skeleton.size());
        sample_clip_pose(skeleton, clips[static_cast<std::size_t>(idle.clip)], 0.0f,
                         reference);
        lib.relax = calibrate_arm_relax(rig, skeleton, binding, reference);
    }
    return lib;
}

ClipRole role_for_drive(const ClipLibrary& lib, const BodyDrive& drive) {
    const bool moving = drive.speed_mps > MOVING_SPEED_MPS;
    if (!drive.grounded && lib.has(ClipRole::JumpLoop)) {
        return ClipRole::JumpLoop;
    }
    if (drive.posture_blend > 0.5f && lib.has(ClipRole::Sit)) {
        // SIT ONLY, and lying deliberately falls through to the procedural
        // posture: this asset has no lying clip, and the nearest one it does
        // have (Death01) reads as a corpse in a bed. Named as a tail rather
        // than approximated.
        return drawn_posture(drive) == Posture::Sit ? ClipRole::Sit : ClipRole::Idle;
    }
    if (drive.crouch_blend > 0.5f) {
        const ClipRole want = moving ? ClipRole::CrouchWalk : ClipRole::CrouchIdle;
        if (lib.has(want)) {
            return want;
        }
    }
    if (moving) {
        const ClipRole want = role_for_gait(drive.gait);
        if (lib.has(want)) {
            return want;
        }
    }
    return ClipRole::Idle;
}

namespace {

[[nodiscard]] bool locomotion(ClipRole r) {
    return r == ClipRole::Walk || r == ClipRole::Jog || r == ClipRole::Sprint
           || r == ClipRole::CrouchWalk;
}
[[nodiscard]] bool one_shot(ClipRole r) {
    return r == ClipRole::JumpStart || r == ClipRole::JumpLand;
}

/// Where in a locomotion clip sim's stride phase puts us. The whole footfall
/// seam is this one line: the clip is shifted so its own plant coincides with
/// the phase sim fires its event at.
[[nodiscard]] float locomotion_time(const ClipEntry& entry, float stride_phase) {
    if (entry.duration_s <= 0.0f) {
        return 0.0f;
    }
    return wrap01(stride_phase - PHASE_LEFT + entry.footfall_phase) * entry.duration_s;
}

} // namespace

void advance_playback(const ClipLibrary& lib, const BodyDrive& drive, float dt,
                      ClipPlayback& play) {
    // THE PREVIOUS TICK, snapshotted before anything moves. Render reads it
    // with an alpha exactly as it reads PreviousTransform (Rule 12); a pose
    // that only exists for the current tick is the reason a 30 Hz sim looked
    // like a 30 Hz body while everything around it was smooth.
    play.prev_time_s = play.time_s;
    play.prev_previous_time_s = play.previous_time_s;
    play.prev_fade = play.fade;
    play.prev_stride = play.stride;
    play.prev_previous_stride = play.previous_stride;
    play.prev_mix_time_s = play.mix_time_s;
    play.prev_previous_mix_time_s = play.previous_mix_time_s;
    play.prev_weapon = play.weapon;
    play.prev_weapon_time_s = play.weapon_time_s;

    // THE HANDS, EASED. The state is a flag on the drive and the picture is
    // this float; the 0.2 s is WEAPON_CROSSFADE_S, which the order names.
    const float want_weapon = drive.weapon_drawn ? 1.0f : 0.0f;
    if (dt > 0.0f && WEAPON_CROSSFADE_S > 0.0f) {
        const float move = dt / WEAPON_CROSSFADE_S;
        play.weapon = play.weapon < want_weapon
                          ? std::min(want_weapon, play.weapon + move)
                          : std::max(want_weapon, play.weapon - move);
    } else {
        play.weapon = want_weapon;
    }
    const ClipEntry& guard_entry = lib[ClipRole::WeaponIdle];
    if (guard_entry.present() && guard_entry.duration_s > 0.0f) {
        play.weapon_time_s += dt;
        play.weapon_time_s =
            wrap01(play.weapon_time_s / guard_entry.duration_s) * guard_entry.duration_s;
    }

    ClipRole want = role_for_drive(lib, drive);
    // THE JUMP TRIPLE is the one sequence a state cannot answer on its own:
    // "airborne" is true for the take-off, the arc and the fall alike, and the
    // difference between them is HOW LONG WE HAVE BEEN airborne.
    if (!drive.grounded && lib.has(ClipRole::JumpStart)) {
        const bool starting = play.role == ClipRole::JumpStart
                              && play.time_s < lib[ClipRole::JumpStart].duration_s;
        const bool just_left = play.role != ClipRole::JumpStart
                               && play.role != ClipRole::JumpLoop;
        if (starting || just_left) {
            want = ClipRole::JumpStart;
        }
    }
    if (drive.grounded && lib.has(ClipRole::JumpLand)) {
        const bool landing = play.role == ClipRole::JumpLand
                             && play.time_s < lib[ClipRole::JumpLand].duration_s;
        const bool just_landed = play.role == ClipRole::JumpLoop
                                 || play.role == ClipRole::JumpStart;
        if ((landing || just_landed) && drive.speed_mps <= MOVING_SPEED_MPS) {
            want = ClipRole::JumpLand;
        }
    }

    if (want != play.role) {
        play.previous = play.role;
        play.previous_time_s = play.time_s;
        play.previous_stride = play.stride;
        play.fade = 1.0f;
        play.role = want;
        play.time_s = locomotion(want) ? 0.0f : 0.0f;
    }
    if (play.fade > 0.0f && dt > 0.0f) {
        play.fade = std::max(0.0f, play.fade - dt / CLIP_CROSSFADE_S);
    }

    const ClipEntry& cur = lib[play.role];
    // HOW WIDE THE STRIDE HAS TO BE. sim's step_length_m is the model that
    // already sets the procedural gait's amplitudes and the camera's bob
    // frequency; a cycle is two steps (StepFeel's own convention).
    const float demanded = 2.0f * std::max(0.0f, drive.step_length_m);
    play.stride = locomotion(play.role) ? stride_scale_for(cur, demanded) : 1.0f;
    if (locomotion(play.role)) {
        play.time_s = locomotion_time(cur, drive.stride_phase);
        play.mix_time_s = cur.mixed() ? wrap01(drive.stride_phase - PHASE_LEFT
                                               + cur.mix_footfall)
                                            * cur.mix_duration_s
                                      : 0.0f;
    } else if (cur.duration_s > 0.0f) {
        play.time_s += dt;
        play.time_s = one_shot(play.role) ? std::min(play.time_s, cur.duration_s)
                                          : wrap01(play.time_s / cur.duration_s)
                                                * cur.duration_s;
    }
    if (play.fade > 0.0f) {
        const ClipEntry& prev = lib[play.previous];
        if (locomotion(play.previous)) {
            play.previous_stride = stride_scale_for(prev, demanded);
            play.previous_time_s = locomotion_time(prev, drive.stride_phase);
            play.previous_mix_time_s =
                prev.mixed() ? wrap01(drive.stride_phase - PHASE_LEFT + prev.mix_footfall)
                                   * prev.mix_duration_s
                             : 0.0f;
        } else if (prev.duration_s > 0.0f) {
            play.previous_time_s += dt;
            play.previous_time_s =
                one_shot(play.previous)
                    ? std::min(play.previous_time_s, prev.duration_s)
                    : wrap01(play.previous_time_s / prev.duration_s) * prev.duration_s;
        }
    }
    if (!play.primed) {
        // The first tick has no past. Copying the present into it is what
        // stops frame one from interpolating out of the bind pose.
        play.prev_time_s = play.time_s;
        play.prev_previous_time_s = play.previous_time_s;
        play.prev_fade = play.fade;
        play.prev_stride = play.stride;
        play.prev_previous_stride = play.previous_stride;
        play.prev_mix_time_s = play.mix_time_s;
        play.prev_previous_mix_time_s = play.previous_mix_time_s;
        play.prev_weapon = play.weapon;
        play.prev_weapon_time_s = play.weapon_time_s;
        play.primed = true;
    }
}

namespace {

/// One role's contribution to the frame: the clip sampled at the interpolated
/// time, with its stride scaled and the clip's own ground lift applied.
///
/// THE LIFT GOES ON THE ROOT JOINT'S TRANSLATION, which is model space: a
/// root has no parent, so its local translation IS where the body stands.
/// Every root gets it, because an asset may bind more than one and a body
/// half-lifted is worse than one not lifted at all.
void role_frame(const skel::Skeleton& skeleton, const SkinnedRigBinding& binding,
                std::span<const skel::AnimClip> clips, const ClipEntry& entry,
                float prev_t, float t, float prev_mix_t, float mix_t, float alpha,
                float stride, std::span<JointLocal> out) {
    const float d = forward_delta(prev_t, t, entry.duration_s);
    float when = prev_t + alpha * d;
    if (entry.duration_s > 0.0f) {
        when = wrap01(when / entry.duration_s) * entry.duration_s;
    }
    sample_clip_pose(skeleton, clips[static_cast<std::size_t>(entry.clip)], when, out);
    if (entry.mixed()) {
        // THE PARTNER IS INTERPOLATED IN ITS OWN CLOCK. Deriving its instant
        // from `when` would need the phase, and the frame does not have one:
        // it sits between two ticks, and each tick is what knew where in the
        // cycle it was.
        const float dm = forward_delta(prev_mix_t, mix_t, entry.mix_duration_s);
        float when_mix = prev_mix_t + alpha * dm;
        if (entry.mix_duration_s > 0.0f) {
            when_mix = wrap01(when_mix / entry.mix_duration_s) * entry.mix_duration_s;
        }
        std::vector<JointLocal> partner(skeleton.size());
        sample_clip_pose(skeleton, clips[static_cast<std::size_t>(entry.mix_clip)],
                         when_mix, partner);
        blend_local(out.first(skeleton.size()), partner, entry.mix_weight,
                    out.first(skeleton.size()));
    }
    scale_sample_stride(binding, skeleton, stride, out);
    // THE GROUND LIFT IS NO LONGER APPLIED HERE, and its absence is the point.
    // A constant per stride scale was the best a clip could do about ground it
    // could not see: it grounded the body on the FLAT floor the clip was drawn
    // on, and on a stair or a slope it was simply the wrong number. FootIk.h
    // now supplies the shift per frame from a raycast under each foot, and two
    // mechanisms both moving the root would double-count every centimetre.
    // `ground_curve` stays a MEASUREMENT (how deep a clip sits at a scale) and
    // is read by the library's log and by the tests; nothing draws with it.
}

} // namespace

bool playback_sample(const skel::Skeleton& skeleton, const SkinnedRigBinding& binding,
                     std::span<const skel::AnimClip> clips, const ClipLibrary& lib,
                     const ClipPlayback& play, float alpha,
                     std::span<JointLocal> out_sample) {
    const float a = std::clamp(alpha, 0.0f, 1.0f);
    const std::size_t n = skeleton.size();
    const ClipEntry& cur = lib[play.role];
    if (!cur.present() || out_sample.size() < n) {
        return false;
    }
    role_frame(skeleton, binding, clips, cur, play.prev_time_s, play.time_s,
               play.prev_mix_time_s, play.mix_time_s, a,
               glm::mix(play.prev_stride, play.stride, a), out_sample);
    const float fade = glm::mix(play.prev_fade, play.fade, a);
    const ClipEntry& prev = lib[play.previous];
    if (fade > 0.0f && prev.present()) {
    // THE CROSS-FADE RUNS ON TWO FINISHED SAMPLES, not on one sample built
    // from a blended time: the two roles carry DIFFERENT STRIDE SCALES and
    // different durations, and a single interpolated clip time between them
    // means nothing at all.
        std::vector<JointLocal> other(n);
        role_frame(skeleton, binding, clips, prev, play.prev_previous_time_s,
                   play.previous_time_s, play.prev_previous_mix_time_s,
                   play.previous_mix_time_s, a,
                   glm::mix(play.prev_previous_stride, play.previous_stride, a), other);
        blend_local(out_sample.first(n), other, fade, out_sample.first(n));
    }
    // THE TWO LAYERS, in the order a body wears them: the weapon guard is put
    // ON the upper half, and the arm relax is what comes OFF when it is.
    const float weapon = glm::mix(play.prev_weapon, play.weapon, a);
    const ClipEntry& guard = lib[ClipRole::WeaponIdle];
    if (weapon > 0.0f && guard.present() && lib.mask.valid()) {
        std::vector<JointLocal> upper(n);
        role_frame(skeleton, binding, clips, guard, play.prev_weapon_time_s,
                   play.weapon_time_s, 0.0f, 0.0f, a, 1.0f, upper);
        blend_masked(out_sample.first(n), upper, lib.mask, weapon, out_sample.first(n));
    }
    apply_arm_relax(skeleton, lib.relax, 1.0f - weapon, out_sample);
    return true;
}

FootSlide measure_foot_slide(const skel::Skeleton& skeleton,
                             const SkinnedRigBinding& binding,
                             std::span<const skel::AnimClip> clips,
                             const ClipLibrary& lib, ClipRole role,
                             float step_length_m, bool stride_match,
                             uint32_t samples) {
    FootSlide out;
    const ClipEntry& entry = lib[role];
    if (!entry.present() || entry.duration_s <= 0.0f || samples < 4
        || !lib.contacts.valid()) {
        return out;
    }
    out.demanded_m = 2.0f * std::max(0.0f, step_length_m);
    const float scale = stride_match ? stride_scale_for(entry, out.demanded_m) : 1.0f;
    const ContactTrack track =
        track_contacts(skeleton, binding, lib.contacts, mix_of(entry, clips), scale,
                       samples);
    const std::size_t n = track.samples();
    if (n < 2) {
        return out;
    }
    const TravelFit fit = fit_travel(track);
    out.cycle_travel_m = fit.travel_m;
    out.plant_residual_m = fit.residual_m;
    // THE BODY MOVES THE DEMANDED DISTANCE OVER THE LOOP, evenly, along the
    // direction the clip itself walks in -- MEASURED (fit_travel), so the
    // prober does not assume our -Z convention a second time. Whatever the
    // planted contact point has left over AFTER that motion is subtracted IS
    // the slide.
    const glm::vec2 dir =
        glm::length(fit.direction) > 1e-5f ? fit.direction : glm::vec2{0.0f, -1.0f};
    const float per_sample = out.demanded_m / float(samples);
    // ONE FOOT'S PLANT IS ONE STEP, so each foot is judged on its own and the
    // worse of the two is the answer. Summing the pair together would let a
    // foot that plants cleanly pay for one that does not.
    // PER CONTACT JOINT first; the two reductions below turn them into the
    // two numbers the header describes.
    std::vector<float> joint_drift(track.tracks(),
                                   std::numeric_limits<float>::max());
    std::vector<float> joint_path(track.tracks(), 0.0f);
    std::vector<uint32_t> joint_down(track.tracks(), 0);
    for (std::size_t c = 0; c < track.tracks(); ++c) {
        const std::vector<glm::vec3>& t = track.joint[c];
        // ONE PLANT AT A TIME, AND A PLANT MAY STRADDLE THE LOOP POINT. The
        // first version indexed the world track by the raw sample number, so a
        // plant that began at sample 94 and ended at sample 3 was read as a
        // foot that had travelled the whole cycle: it reported exactly
        // `demanded_m` of slide on a body that was walking correctly, which is
        // the kind of wrong that looks like a real measurement.
        for (std::size_t start = 0; start < n; ++start) {
            if (!track.down(c, start, GRIP_TOLERANCE_M)
                || track.down(c, (start + n - 1) % n, GRIP_TOLERANCE_M)) {
                continue; // not the first sample of a plant
            }
            std::vector<glm::vec2> world;
            ++joint_down[c];
            for (std::size_t len = 0; len < n; ++len) {
                const std::size_t k = (start + len) % n;
                if (!track.down(c, k, GRIP_TOLERANCE_M)) {
                    break;
                }
                world.push_back(glm::vec2{t[k].x, t[k].z}
                                + dir * (per_sample * float(start + len)));
                ++joint_down[c];
            }
            float run_path = 0.0f;
            for (std::size_t i = 1; i < world.size(); ++i) {
                run_path += glm::length(world[i] - world[i - 1]);
            }
            joint_path[c] = std::max(joint_path[c], run_path);
            // DRIFT IS THE SPREAD OF THE PLANTED POINT, and it is the headline
            // because it is what an eye reads as a sliding foot: a foot that
            // trembles a millimetre and comes back has not slid anywhere. The
            // summed PATH is kept beside it as the strict reading -- it can
            // only be larger, and a gap between the two is jitter, not drift.
            float spread = 0.0f;
            for (std::size_t i = 0; i < world.size(); ++i) {
                for (std::size_t j = i + 1; j < world.size(); ++j) {
                    spread = std::max(spread, glm::length(world[j] - world[i]));
                }
            }
            joint_drift[c] = joint_drift[c] == std::numeric_limits<float>::max()
                                 ? spread
                                 : std::max(joint_drift[c], spread);
        }
    }
    // WHICH JOINT SPEAKS FOR A FOOT: THE DEEPEST ONE AT REST — the toe on an
    // asset that has one, the ankle on one that does not. A fixed choice per
    // MODEL, made once, and not a contest run per clip, because both contests
    // I tried could be won by an artefact. "Smallest drift" is won by a joint
    // that grazes the contact band for two samples: its spread is small by
    // arithmetic, and a body sliding a whole stride passed. "Longest time
    // down" is won, on a clip whose stride has been shrunk, by the ankle that
    // never leaves the band at all — and an ankle does honestly travel five
    // centimetres while the ball of the foot it is rotating about does not.
    // The deepest joint at rest is the part of the foot the ground is under.
    float foot_drift[2] = {0.0f, 0.0f};
    float foot_path[2] = {0.0f, 0.0f};
    std::size_t speaker[2] = {track.tracks(), track.tracks()};
    for (std::size_t c = 0; c < track.tracks(); ++c) {
        const std::size_t side = track.side[c];
        if (speaker[side] >= track.tracks()
            || track.rest_y[c] < track.rest_y[speaker[side]]) {
            speaker[side] = c;
        }
        if (track.ankle[c] != 0
            && joint_drift[c] < std::numeric_limits<float>::max()) {
            out.ankle_per_step_m = std::max(out.ankle_per_step_m, joint_drift[c]);
        }
    }
    for (int side = 0; side < 2; ++side) {
        const std::size_t c = speaker[static_cast<std::size_t>(side)];
        if (c >= track.tracks()
            || joint_drift[c] >= std::numeric_limits<float>::max()) {
            continue; // this foot never planted at all
        }
        foot_drift[side] = joint_drift[c];
        foot_path[side] = joint_path[c];
    }
    out.worst_per_step_m = std::max(foot_drift[0], foot_drift[1]);
    out.path_per_step_m = std::max(foot_path[0], foot_path[1]);
    return out;
}

} // namespace dfn::anim
