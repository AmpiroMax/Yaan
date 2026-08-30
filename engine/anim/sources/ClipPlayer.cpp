/*
Module: engine/anim
File: engine/anim/sources/ClipPlayer.cpp

Responsibility:
- Implements role resolution, the load-time measurements, the tick and the
  frame of imported-clip playback, and the foot-slide prober.

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

/// WHICH SAMPLES COUNT AS THE FOOT BEING DOWN: the ankle within this many
/// metres of ITS OWN lowest point in the cycle.
///
/// AN ABSOLUTE TOLERANCE, and three earlier rules all failed on this one asset
/// before it. (1) "the lower of the two ankles" includes heel strike and toe
/// off, and during those the ankle pivots over heel or toe instead of
/// travelling with the body — measured on the walk, 0.045 m per sample flat
/// against 0.030 m rolling — which put the stride 26 % short and reported
/// 0.48 m of slide on a body that does not slide. (2) A fixed fraction of the
/// ankle's vertical RANGE fixed the walk and broke the run: a jog lifts its
/// foot four times as high, so 8 % of that range caught only touchdown. (3) A
/// PERCENTILE of the pooled heights fails for the opposite reason — a walk has
/// its foot down 60 % of the cycle and a sprint barely a third of that, so any
/// one percentile is wrong for one of them, and on the jog it swept in the
/// pre-contact retraction and read the stride as 5.14 m instead of 2.4.
///
/// Two centimetres is a statement about a FOOT and not about a clip: below it
/// the sole is on the ground whatever the gait. It is the only rule of the
/// four that gives both the walk and the sprint a plausible stride.
constexpr float STANCE_TOLERANCE_M = 0.02f;

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

float stride_scale_for(const ClipEntry& entry, float target_m) {
    const auto& c = entry.stride_curve;
    if (!entry.present() || target_m <= 0.0f || c[STRIDE_CURVE_POINTS - 1] <= 0.0f) {
        return 1.0f;
    }
    const float step =
        (STRIDE_SCALE_MAX - STRIDE_SCALE_MIN) / float(STRIDE_CURVE_POINTS - 1);
    // THE NEAREST POINT ON THE CURVE, not the first crossing of the target.
    // The curve is monotone while the leg has room and folds over once it runs
    // out (a thigh scaled past its swing starts carrying the foot BACK), and a
    // first-crossing search on a folded curve answers with the far side of the
    // fold: measured on the sprint, it asked for 1.14 where 2.4 was nearer.
    uint32_t best = 0;
    float best_err = std::abs(c[0] - target_m);
    for (uint32_t i = 1; i < STRIDE_CURVE_POINTS; ++i) {
        const float err = std::abs(c[i] - target_m);
        if (err < best_err) {
            best_err = err;
            best = i;
        }
    }
    // Refine INSIDE the winning cell, toward whichever neighbour brackets the
    // target: a twelve-point grid is 0.25 wide and that is visible on a stride.
    const auto refine = [&](uint32_t a, uint32_t b) {
        const float span = c[b] - c[a];
        if (std::abs(span) < 1e-6f) {
            return float(a);
        }
        const float u = std::clamp((target_m - c[a]) / span, 0.0f, 1.0f);
        return float(a) + u;
    };
    float at = float(best);
    if (best > 0 && (target_m - c[best]) * (c[best - 1] - c[best]) > 0.0f) {
        at = refine(best, best - 1);
        at = float(best) - (at - float(best));
    } else if (best + 1 < STRIDE_CURVE_POINTS
               && (target_m - c[best]) * (c[best + 1] - c[best]) > 0.0f) {
        at = refine(best, best + 1);
    }
    return std::clamp(STRIDE_SCALE_MIN + step * at, STRIDE_SCALE_MIN,
                      STRIDE_SCALE_MAX);
}

namespace {

struct FootTrack {
    std::vector<glm::vec3> left;
    std::vector<glm::vec3> right;
};

/// Walks one clip over a whole loop at a given stride scale and records where
/// both ankles were, in model space. THE SAME SAMPLING PATH THE FRAME USES, so
/// the measurement cannot describe a body the renderer does not draw.
[[nodiscard]] FootTrack track_feet(const skel::Skeleton& skeleton,
                                   const SkinnedRigBinding& binding,
                                   const skel::AnimClip& clip, const ClipEntry& entry,
                                   float scale, uint32_t samples) {
    FootTrack track;
    const int32_t jl = binding.names.joint[bone_index(Bone::FootL)];
    const int32_t jr = binding.names.joint[bone_index(Bone::FootR)];
    if (jl < 0 || jr < 0 || entry.duration_s <= 0.0f) {
        return track;
    }
    std::vector<JointLocal> sample(skeleton.size());
    std::vector<glm::mat4> local(skeleton.size());
    std::vector<glm::mat4> model(skeleton.size());
    track.left.reserve(samples + 1);
    track.right.reserve(samples + 1);
    for (uint32_t k = 0; k <= samples; ++k) {
        const float t = entry.duration_s * float(k) / float(samples);
        sample_clip_pose(skeleton, clip, t, sample);
        scale_sample_stride(binding, skeleton, scale, sample);
        for (std::size_t j = 0; j < skeleton.size(); ++j) {
            local[j] = glm::translate(glm::mat4{1.0f}, sample[j].translation)
                       * glm::mat4_cast(glm::normalize(sample[j].rotation))
                       * glm::scale(glm::mat4{1.0f}, sample[j].scale);
        }
        skel::skeleton_model_matrices(skeleton, local, model);
        track.left.push_back(glm::vec3{model[static_cast<std::size_t>(jl)][3]});
        track.right.push_back(glm::vec3{model[static_cast<std::size_t>(jr)][3]});
    }
    return track;
}

/// The height below which THIS foot counts as down: its own lowest sample plus
/// the tolerance. Per foot, because an asset whose two feet do not land at the
/// same height is exactly the asset a shared threshold would mis-read.
[[nodiscard]] float stance_height(const std::vector<glm::vec3>& foot) {
    if (foot.empty()) {
        return 0.0f;
    }
    float lo = foot[0].y;
    for (const glm::vec3& p : foot) {
        lo = std::min(lo, p.y);
    }
    return lo + STANCE_TOLERANCE_M;
}

/// The body's displacement PER SAMPLE that would hold a flat foot still,
/// averaged over every flat-foot sample of both feet. Not the whole-cycle sum
/// of the lower ankle: see STANCE_BAND for what that measures instead.
[[nodiscard]] glm::vec3 stance_rate(const FootTrack& track) {
    glm::vec3 acc{0.0f};
    uint32_t count = 0;
    for (int side = 0; side < 2; ++side) {
        const std::vector<glm::vec3>& t = side == 0 ? track.left : track.right;
        const float band = stance_height(t);
        for (std::size_t k = 0; k + 1 < t.size(); ++k) {
            if (t[k].y > band || t[k + 1].y > band) {
                continue;
            }
            acc += glm::vec3{t[k + 1].x - t[k].x, 0.0f, t[k + 1].z - t[k].z};
            ++count;
        }
    }
    return count > 0 ? -acc / float(count) : glm::vec3{0.0f};
}

/// The vector the body must travel over one whole loop for a flat foot to stay
/// put.
[[nodiscard]] glm::vec3 cycle_travel_vector(const FootTrack& track) {
    if (track.left.size() < 2) {
        return glm::vec3{0.0f};
    }
    return stance_rate(track) * float(track.left.size() - 1);
}

/// The phase at the MIDDLE OF THE LEFT FOOT'S PLANT, which is what the gait
/// contract calls "the left foot's lowest point": a flat foot has no single
/// lowest sample, it has a plateau, and an argmin over a plateau picks
/// whichever end quantisation noise favours. The run is found with a wrap, so
/// a clip whose plant straddles the loop point is not cut in two.
[[nodiscard]] float left_plant_phase(const FootTrack& track) {
    const std::size_t n = track.left.size() > 0 ? track.left.size() - 1 : 0;
    if (n < 2) {
        return 0.0f;
    }
    const float band = stance_height(track.left);
    std::size_t best_start = 0;
    std::size_t best_len = 0;
    for (std::size_t start = 0; start < n; ++start) {
        if (track.left[start].y > band
            || track.left[(start + n - 1) % n].y <= band) {
            continue; // not the first sample of a run
        }
        std::size_t len = 0;
        while (len < n && track.left[(start + len) % n].y <= band) {
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

ClipLibrary build_clip_library(const skel::Skeleton& skeleton,
                               const SkinnedRigBinding& binding,
                               std::span<const skel::AnimClip> clips) {
    ClipLibrary lib;
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
        const FootTrack base =
            track_feet(skeleton, binding, clip, entry, 1.0f, MEASURE_SAMPLES);
        entry.cycle_m = glm::length(cycle_travel_vector(base));
        entry.footfall_phase = left_plant_phase(base);
        const float step =
            (STRIDE_SCALE_MAX - STRIDE_SCALE_MIN) / float(STRIDE_CURVE_POINTS - 1);
        for (uint32_t i = 0; i < STRIDE_CURVE_POINTS; ++i) {
            const float sc = STRIDE_SCALE_MIN + step * float(i);
            entry.stride_curve[i] = glm::length(cycle_travel_vector(
                track_feet(skeleton, binding, clip, entry, sc, MEASURE_SAMPLES)));
        }
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
        play.primed = true;
    }
}

namespace {

/// One role's contribution to the frame: the clip sampled at the interpolated
/// time, with its stride scaled.
void role_frame(const skel::Skeleton& skeleton, const SkinnedRigBinding& binding,
                const skel::AnimClip& clip, const ClipEntry& entry, float prev_t,
                float t, float alpha, float stride, std::span<JointLocal> out) {
    const float d = forward_delta(prev_t, t, entry.duration_s);
    float when = prev_t + alpha * d;
    if (entry.duration_s > 0.0f) {
        when = wrap01(when / entry.duration_s) * entry.duration_s;
    }
    sample_clip_pose(skeleton, clip, when, out);
    scale_sample_stride(binding, skeleton, stride, out);
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
    role_frame(skeleton, binding, clips[static_cast<std::size_t>(cur.clip)], cur,
               play.prev_time_s, play.time_s, a,
               glm::mix(play.prev_stride, play.stride, a), out_sample);
    const float fade = glm::mix(play.prev_fade, play.fade, a);
    const ClipEntry& prev = lib[play.previous];
    if (fade <= 0.0f || !prev.present()) {
        return true;
    }
    // THE CROSS-FADE RUNS ON TWO FINISHED SAMPLES, not on one sample built
    // from a blended time: the two roles carry DIFFERENT STRIDE SCALES and
    // different durations, and a single interpolated clip time between them
    // means nothing at all.
    std::vector<JointLocal> other(n);
    role_frame(skeleton, binding, clips[static_cast<std::size_t>(prev.clip)], prev,
               play.prev_previous_time_s, play.previous_time_s, a,
               glm::mix(play.prev_previous_stride, play.previous_stride, a), other);
    blend_local(out_sample.first(n), other, fade, out_sample.first(n));
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
    if (!entry.present() || entry.duration_s <= 0.0f || samples < 4) {
        return out;
    }
    out.demanded_m = 2.0f * std::max(0.0f, step_length_m);
    const float scale = stride_match ? stride_scale_for(entry, out.demanded_m) : 1.0f;
    const FootTrack track =
        track_feet(skeleton, binding, clips[static_cast<std::size_t>(entry.clip)],
                   entry, scale, samples);
    if (track.left.size() < 2) {
        return out;
    }
    const glm::vec3 travel = cycle_travel_vector(track);
    out.cycle_travel_m = glm::length(travel);
    // THE ROOT MOVES THE DEMANDED DISTANCE OVER THE LOOP, evenly, along the
    // direction the clip itself walks in -- so the prober does not assume our
    // -Z convention a second time. Whatever the stance foot has left over
    // AFTER that motion is subtracted IS the slide.
    const glm::vec3 dir = out.cycle_travel_m > 1e-5f
                              ? travel / out.cycle_travel_m
                              : glm::vec3{0.0f, 0.0f, -1.0f};
    const float per_sample = out.demanded_m / float(samples);
    // ONE FOOT'S PLANT IS ONE STEP, so each foot is summed on its own and the
    // worse of the two is the answer. Summing the pair together would let a
    // foot that plants cleanly pay for one that does not.
    float drift[2] = {0.0f, 0.0f};
    float path[2] = {0.0f, 0.0f};
    const std::size_t n = track.left.size() > 0 ? track.left.size() - 1 : 0;
    for (int side = 0; side < 2; ++side) {
        const std::vector<glm::vec3>& t = side == 0 ? track.left : track.right;
        const float band = stance_height(t);
        // ONE PLANT AT A TIME, AND A PLANT MAY STRADDLE THE LOOP POINT. The
        // first version indexed the world track by the raw sample number, so a
        // plant that began at sample 94 and ended at sample 3 was read as a
        // foot that had travelled the whole cycle: it reported exactly
        // `demanded_m` of slide on a body that was walking correctly, which is
        // the kind of wrong that looks like a real measurement.
        for (std::size_t start = 0; start < n; ++start) {
            if (t[start].y > band || t[(start + n - 1) % n].y <= band) {
                continue; // not the first sample of a plant
            }
            std::vector<glm::vec3> world;
            for (std::size_t len = 0; len < n; ++len) {
                const std::size_t k = (start + len) % n;
                if (t[k].y > band) {
                    break;
                }
                const glm::vec3 p = glm::vec3{t[k].x, 0.0f, t[k].z}
                                    + dir * (per_sample * float(start + len));
                if (!world.empty()) {
                    path[side] = std::max(path[side],
                                          path[side] + glm::length(p - world.back())
                                              - path[side]);
                }
                world.push_back(p);
            }
            float run_path = 0.0f;
            for (std::size_t i = 1; i < world.size(); ++i) {
                run_path += glm::length(world[i] - world[i - 1]);
            }
            path[side] = std::max(path[side], run_path);
            // DRIFT IS THE SPREAD OF THE PLANTED POINT, and it is the headline
            // because it is what an eye reads as a sliding foot: a foot that
            // trembles a millimetre and comes back has not slid anywhere. The
            // summed PATH is kept beside it as the strict reading -- it can
            // only be larger, and a gap between the two is jitter, not drift.
            for (std::size_t i = 0; i < world.size(); ++i) {
                for (std::size_t j = i + 1; j < world.size(); ++j) {
                    drift[side] =
                        std::max(drift[side], glm::length(world[j] - world[i]));
                }
            }
        }
    }
    out.worst_per_step_m = std::max(drift[0], drift[1]);
    out.path_per_step_m = std::max(path[0], path[1]);
    return out;
}

} // namespace dfn::anim
