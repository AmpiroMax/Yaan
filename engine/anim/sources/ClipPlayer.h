/*
Module: engine/anim
File: engine/anim/sources/ClipPlayer.h

Responsibility:
- PLAYING THE IMPORTED CLIPS. The skinning wave read 46 clips out of the model
  and then bent it with our procedural gait anyway; this file is the missing
  half: which clip a player state asks for, where in that clip we are, how two
  clips cross-fade, and how a clip's stride is made to cover the ground sim
  says the character covered.

Key items:
- ClipRole: the states the body has clips for (Idle, Walk, Jog, Sprint, jump
  triple, crouch pair, sit). Roles are OURS; clip NAMES are the asset's.
- ClipLibrary / build_clip_library(): role -> clip index plus the three things
  only measurement can answer — the clip's duration, the metres its stance foot
  covers per loop, and the phase at which its LEFT foot plants.
- sample_clip_local(): one clip at one time, in the imported skeleton's own
  local TRS (SkinnedBody's JointLocal). Full fidelity: spine, neck, shoulders, toes and
  fingers are keyed by the clip and no rig bone speaks for them.
- clip_local_pose(): the same sample expressed as OUR LocalPose, so every
  layer written for the fifteen bones (crouch, landing dip, joint limits,
  the mirror) still applies on top of a bought clip.
- ClipPlayback: the play state, plain data, advanced once per fixed tick.
- advance_playback() / playback_pose(): the tick and the frame.

Dependencies:
- Uses: Rig, Pose, Clips (Gait), Body (BodyDrive, the ferried sim state),
  SkinnedBody (the binding + rest delta),
  core skeleton, generated constants (FOOTFALL_PHASE_LEFT).
- Used by: engine/app (SkinnedCharacter), tests.

Notes:
- THE PHASE IS STILL SIM'S CLOCK, and that is why this file may exist at all
  (Clips.h: "never advance a phase here"). A locomotion clip's time is a pure
  function of `stride_phase` — the same [0,1) sim advances by displacement —
  shifted so the clip's own left footfall lands exactly on FOOTFALL_PHASE_LEFT.
  The footfall event, the camera bob and the drawn plant therefore stay the
  same instant; nothing in this zone integrates a clock for locomotion.
  Non-locomotion clips (the jump triple, the interactions) DO run off their own
  seconds, because a jump is an event with a duration and not a cycle.
- HOW SPEED REACHES THE FEET, and it is NOT playback rate. The rate is sim's:
  one clip loop per stride cycle. Speed enters as STRIDE SCALE — the leg
  chain's rotations are scaled about our rest pose until the stance foot
  covers 2 x `step_length_m` per loop. This is the same decision `gait_pose`
  already makes for the procedural gait (its amplitudes derive from
  step_length_m), and it is why the two paths can be compared frame for frame.
  Rate-scaling instead would have been the other classic answer and it breaks
  the footfall seam: the asset's Walk_Loop is authored at 0.77 m/s, so at
  WALK_SPEED it would have to run 2.3x fast — 210 steps a minute against sim's
  110, i.e. two visible plants per audible one.
- THE SCALE IS INVERTED FROM A MEASUREMENT, not from a formula. Foot travel is
  not linear in thigh angle and it saturates: build_clip_library samples the
  actual retargeted stance-foot travel over a grid of scales and stores the
  curve, and stride_scale_for() reads it backwards. Past the end of the curve
  the scale CLAMPS and the residual slide is real — reported, not hidden.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure functions and plain data (Rule 8, Rule 30): no ECS, no IO, no clock.
  Everything here takes its time as a parameter.
*/

#pragma once

#include "engine/anim/sources/Body.h"
#include "engine/anim/sources/Clips.h"
#include "engine/anim/sources/Pose.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/skeleton/sources/Skeleton.h"

#include <array>
#include <cstdint>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <span>
#include <string_view>
#include <vector>

namespace dfn::anim {

/// The states the body has a clip for. A role is a question the game asks;
/// the clip that answers it is a name in the asset, resolved once at load.
enum class ClipRole : uint8_t {
    Idle = 0,
    Walk,
    Jog,
    Sprint,
    JumpStart,
    JumpLoop,
    JumpLand,
    CrouchIdle,
    CrouchWalk,
    Sit,
};
inline constexpr uint32_t CLIP_ROLE_COUNT = 10;

[[nodiscard]] constexpr uint32_t role_index(ClipRole r) {
    return static_cast<uint32_t>(r);
}
[[nodiscard]] std::string_view role_name(ClipRole r);

/// The role a gear asks for. A TABLE, not an interpolation, for the reason
/// gait_run_weight is one (Rule 37): a gear added between two rows must be a
/// decision somebody wrote down, not a point a line happened to pass through.
[[nodiscard]] ClipRole role_for_gait(Gait gait);

/// How many scale samples the stride curve holds. Twelve points over
/// [0.25, 3.0] resolve the curve to better than a percent where it is
/// straight and still show the knee of the saturation.
inline constexpr uint32_t STRIDE_CURVE_POINTS = 12;
inline constexpr float STRIDE_SCALE_MIN = 0.25f;
inline constexpr float STRIDE_SCALE_MAX = 3.0f;

/// What load-time measurement found out about one clip.
struct ClipEntry {
    int32_t clip = -1;         ///< index into the model's clip list, -1 = absent
    float duration_s = 0.0f;
    /// Metres the STANCE foot carries the body per loop at stride scale 1,
    /// measured through the retarget. Zero for a clip that does not travel.
    float cycle_m = 0.0f;
    /// Phase in [0,1) at which this clip's LEFT foot is planted (its ankle at
    /// its lowest). Zero for a clip with no plant.
    float footfall_phase = 0.0f;
    /// cycle_m as a function of stride scale, over the grid
    /// [STRIDE_SCALE_MIN, STRIDE_SCALE_MAX]. Monotone where the leg has room.
    std::array<float, STRIDE_CURVE_POINTS> stride_curve{};

    [[nodiscard]] bool present() const { return clip >= 0; }
};

struct ClipLibrary {
    std::array<ClipEntry, CLIP_ROLE_COUNT> role{};
    /// How many roles the asset actually answered. Zero means the model has
    /// clips we do not recognise, which is a naming problem and is said out
    /// loud rather than drawn as a T-pose.
    uint32_t resolved = 0;

    [[nodiscard]] const ClipEntry& operator[](ClipRole r) const {
        return role[role_index(r)];
    }
    [[nodiscard]] bool has(ClipRole r) const { return role[role_index(r)].present(); }
};

/// Resolves every role against the model's clip names and MEASURES what a name
/// cannot give: the clip's duration, the metres its stance foot covers, the
/// phase its left foot plants at, the pelvis rest, and the stride curve. The
/// travel is measured on the body AS DRAWN — the same sampling path the frame
/// uses — so a change to playback moves the measurement with it.
[[nodiscard]] ClipLibrary build_clip_library(const skel::Skeleton& skeleton,
                                             const SkinnedRigBinding& binding,
                                             std::span<const skel::AnimClip> clips);

/// One clip at one time as the imported skeleton's local TRS. Joints the clip
/// does not key keep their BIND values (skel::sample_clip's contract).
/// out.size() must be >= skeleton.size().
void sample_clip_pose(const skel::Skeleton& skeleton, const skel::AnimClip& clip,
                      float time_s, std::span<JointLocal> out);

/// out = a blended with b by `weight` (slerp on rotation, lerp on the rest).
void blend_local(std::span<const JointLocal> a, std::span<const JointLocal> b,
                 float weight, std::span<JointLocal> out);

/// Scales the six leg joints' deviation FROM THEIR BIND by `scale`, in place.
/// This is what makes a 1.03 m clip stride cover 1.96 m of sim's ground, and
/// it is deliberately expressed against the bind rather than against our rest:
/// the bind is the only frame both the clip and the skeleton already agree on.
void scale_sample_stride(const SkinnedRigBinding& binding,
                         const skel::Skeleton& skeleton, float scale,
                         std::span<JointLocal> sample);

/// The scale whose measured travel is `target_m`, read backwards off the
/// entry's curve. CLAMPED to [STRIDE_SCALE_MIN, STRIDE_SCALE_MAX]; when the
/// target is past the end of the curve the clamp is what the caller gets and
/// the residual slide is real.
[[nodiscard]] float stride_scale_for(const ClipEntry& entry, float target_m);

/// How long a cross-fade between two roles lasts, seconds. One number for
/// every transition on purpose: a per-pair table is a thing nobody keeps true,
/// and 0.18 s sits inside the 0.15..0.25 s band the order names.
inline constexpr float CLIP_CROSSFADE_S = 0.18f;

/// THE PLAY STATE, plain data (Rule 8), one per body. Advanced once per fixed
/// tick by advance_playback and read at any alpha by playback_pose.
struct ClipPlayback {
    ClipRole role = ClipRole::Idle;
    ClipRole previous = ClipRole::Idle;
    /// 1 -> 0 while the previous role fades out. Zero means "no cross-fade".
    float fade = 0.0f;
    /// Clip time in seconds for the CURRENT role and for the one fading out.
    float time_s = 0.0f;
    float previous_time_s = 0.0f;
    /// Stride scale in force this tick, and the one the previous role had.
    float stride = 1.0f;
    float previous_stride = 1.0f;
    /// The tick before this one, so the frame can interpolate (Rule 12's
    /// shape, applied to a pose instead of a Transform).
    float prev_time_s = 0.0f;
    float prev_previous_time_s = 0.0f;
    float prev_fade = 0.0f;
    float prev_stride = 1.0f;
    float prev_previous_stride = 1.0f;
    /// True once a tick has run: the first frame must not interpolate from an
    /// uninitialised past, which reads as the body snapping out of its bind.
    bool primed = false;
};

/// Chooses the role for a drive state. Pure, and the whole state machine:
/// airborne -> the jump triple, crouched -> the crouch pair, seated -> Sit,
/// otherwise idle or the gear's locomotion role.
[[nodiscard]] ClipRole role_for_drive(const ClipLibrary& lib, const BodyDrive& drive);

/// ONE FIXED TICK. Snapshots the previous tick, picks the role, starts a
/// cross-fade if it changed, and moves both clip times: locomotion roles are
/// placed by sim's stride phase, everything else advances by `dt`.
void advance_playback(const ClipLibrary& lib, const BodyDrive& drive, float dt,
                      ClipPlayback& play);

/// THE FRAME. `alpha` in [0,1] interpolates between the previous tick and this
/// one exactly as render interpolates a Transform (Rule 12's shape). Writes the
/// imported skeleton's local TRS into `out_sample` (size >= skeleton.size()).
/// False means the role has no clip and the caller must draw something else.
[[nodiscard]] bool playback_sample(const skel::Skeleton& skeleton,
                                   const SkinnedRigBinding& binding,
                                   std::span<const skel::AnimClip> clips,
                                   const ClipLibrary& lib, const ClipPlayback& play,
                                   float alpha, std::span<JointLocal> out_sample);

/// MEASUREMENT, and the prober item 4 of the wave is built on it: how far the
/// stance foot slides in WORLD space while the body walks with `step_length_m`.
/// The world track of each foot while it is PLANTED is what is measured, and
/// one plant is one step, so the two feet are judged separately and the worse
/// of them is the answer — the unit the acceptance threshold is written in
/// ("<= 2 cm per step"). Summing the pair would let a foot that plants cleanly
/// pay for one that does not.
struct FootSlide {
    /// THE NUMBER THE THRESHOLD IS ABOUT: the widest the planted foot's world
    /// position spreads while it is planted, worse of the two feet.
    float worst_per_step_m = 0.0f;
    /// The same plant measured as summed path instead of spread — the strict
    /// reading. It can only be larger; the gap between the two is jitter.
    float path_per_step_m = 0.0f;
    float cycle_travel_m = 0.0f;   ///< what the clip actually covered
    float demanded_m = 0.0f;       ///< what sim said the body covered
};
[[nodiscard]] FootSlide measure_foot_slide(const skel::Skeleton& skeleton,
                                           const SkinnedRigBinding& binding,
                                           std::span<const skel::AnimClip> clips,
                                           const ClipLibrary& lib, ClipRole role,
                                           float step_length_m, bool stride_match,
                                           uint32_t samples);

} // namespace dfn::anim
