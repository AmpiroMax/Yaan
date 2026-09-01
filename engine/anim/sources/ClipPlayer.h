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
- ClipLibrary / build_clip_library(): role -> clip index plus the things only
  measurement can answer — the clip's duration, the metres its planted foot
  carries the body per loop, the phase at which its LEFT foot plants, the lift
  that puts its lowest contact on the ground, and WHICH clip a gear ends up
  playing once those are known.
- FootContacts / ContactSet / build_contacts(): where a foot touches the
  ground on THIS model, and how high those points stand in our rest pose.
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
  the footfall seam: the asset's Walk_Loop is authored at 1.14 m/s, so at
  WALK_SPEED it would have to run 1.6x fast — 175 steps a minute against sim's
  110, i.e. three visible plants per two audible ones.
- AND THE STRIDE SCALE MOVES THE BODY UP AND DOWN, which is the half this file
  was missing until 31.08. Scaling a leg's swing about its bind also changes
  how far the leg REACHES, so a shrunk stride straightens the knee: the pelvis
  ought to ride higher and instead the feet went 0.157 m through the grass.
  ClipEntry::ground_curve is that height, measured per scale at load and added
  to the root joint in the frame.
- THE SCALE IS INVERTED FROM A MEASUREMENT, not from a formula. Foot travel is
  not linear in thigh angle and it saturates: build_clip_library samples the
  actual retargeted planted-foot travel over a grid of scales and stores the
  curve, and stride_scale_for() reads it backwards. Past the end of the curve
  the scale CLAMPS and the residual slide is real — reported, not hidden.
- AND THE MEASUREMENT IS ABOUT THE PART OF THE FOOT THE GROUND IS UNDER. The
  ankle stood in for it while the rig had no toe bone, and the ankle is a fair
  proxy for a walk and a wrong one for a run — see FootContacts. That one
  substitution read this asset's Sprint_Loop as covering 0.698 m of ground per
  cycle where it covers 6.08, and asked for a stride scale of 1.14 where 0.80
  was right: 0.191 m of slide per step, on the gear the owner complained
  about.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure functions and plain data (Rule 8, Rule 30): no ECS, no IO, no clock.
  Everything here takes its time as a parameter.
*/

#pragma once

#include "engine/anim/sources/Body.h"
#include "engine/anim/sources/Clips.h"
#include "engine/anim/sources/Mirror.h"
#include "engine/anim/sources/Pose.h"
#include "engine/anim/sources/PoseLayers.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/anim/sources/Stance.h"
#include "engine/core/config/sources/Constants.h"
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
    /// A LAYER, NOT A STATE. WeaponIdle is never chosen by role_for_drive:
    /// it is the pose the UPPER HALF wears while the legs keep walking, and
    /// giving it its own row here rather than a special-cased clip name is
    /// what lets it be resolved, measured and printed like every other role.
    WeaponIdle,
};
inline constexpr uint32_t CLIP_ROLE_COUNT = 11;

[[nodiscard]] constexpr uint32_t role_index(ClipRole r) {
    return static_cast<uint32_t>(r);
}
[[nodiscard]] std::string_view role_name(ClipRole r);

/// The role a gear asks for. A TABLE, not an interpolation, for the reason
/// gait_run_weight is one (Rule 37): a gear added between two rows must be a
/// decision somebody wrote down, not a point a line happened to pass through.
[[nodiscard]] ClipRole role_for_gait(Gait gait);

/// How many scale samples the stride curve holds, and where they sit.
///
/// GEOMETRIC SPACING, NOT LINEAR, and the difference is a gear. A stride
/// scale is a RATIO, so the interesting distances between two of them are
/// ratios too: linear spacing over [0.25, 3.0] puts eleven of its twelve
/// points above 0.5 and leaves the whole shrinking half of the range to a
/// single 0.25-wide cell. Sim's jog asks this asset for scale 0.35 — inside
/// that cell — and a straight line across it read 2.80 m where the clip
/// covers 2.44. Twelve geometric points step by 12^(1/11) = 1.253 each, so
/// every cell is the same 25 % of stride wherever it sits.
inline constexpr uint32_t STRIDE_CURVE_POINTS = 12;
inline constexpr float STRIDE_SCALE_MIN = 0.25f;
inline constexpr float STRIDE_SCALE_MAX = 3.0f;

/// The scale the i-th curve cell was measured at.
[[nodiscard]] float stride_scale_at(uint32_t i);
/// Where `scale` sits on the curve's grid, as a real index (clamped).
[[nodiscard]] float stride_curve_index(float scale);

/// WHERE A FOOT CAN TOUCH THE GROUND: the rig's foot joint plus whatever the
/// IMPORTED skeleton hangs off it — a toe, when the asset has one, and this
/// one does (`DEF-toe.L/R`). It is not a name table: "the children of the foot
/// joint" is the same sentence on a Rigify export and on Skyrim's
/// `NPC L Foot -> NPC L Toe0`.
///
/// THE PREVIOUS WAVE MEASURED THE ANKLE AND SAID SO, and named it a tail: our
/// fifteen bones have no toe, so the ankle stood in for the contact point.
/// The ankle is a fair proxy for a WALK and a wrong one for a RUN, because a
/// running foot lands on the ball: measured on this asset, the jog's ankle
/// never came within 3 cm of the ground its toe was standing on, so the
/// "stance" band caught the ankle mid-flight and read the clip as covering
/// 5.35 m of ground per cycle where the fit below reads 5.66 m — and, far
/// worse, the SPRINT read 0.698 m, which asked for a stride scale of 1.14
/// where 0.79 was right. The toe is in the file. We were not looking at it.
struct FootContacts {
    std::array<int32_t, 4> joint{};
    /// Where each of those joints sits in OUR REST POSE — the pose whose soles
    /// the importer put on y = 0. THIS, AND NOT THE JOINT'S OWN MINIMUM OVER
    /// THE CLIP, is what "the foot is down" means: an ankle has a lowest point
    /// in every clip, including one where it never comes near the ground, and
    /// a band around that minimum calls a running ankle planted in mid-air.
    std::array<float, 4> rest_y{};
    uint32_t count = 0;
};

/// The contact geometry of one bound model: both feet, and the height the
/// LOWEST of those joints sits at in OUR REST POSE — the pose whose soles are
/// on the ground by construction (the importer grounds them). Everything that
/// says "this clip floats" or "this clip sinks" says it against this number.
struct ContactSet {
    std::array<FootContacts, 2> side{}; ///< [0] = left, [1] = right
    float rest_y = 0.0f;
    [[nodiscard]] bool valid() const { return side[0].count > 0 && side[1].count > 0; }
};

[[nodiscard]] ContactSet build_contacts(const Rig& rig, const skel::Skeleton& skeleton,
                                        const SkinnedRigBinding& binding);

/// What load-time measurement found out about one clip.
struct ClipEntry {
    int32_t clip = -1;         ///< index into the model's clip list, -1 = absent
    float duration_s = 0.0f;
    /// Metres the STANCE foot carries the body per loop at stride scale 1,
    /// measured through the retarget. Zero for a clip that does not travel.
    float cycle_m = 0.0f;
    /// Phase in [0,1) at which this clip's LEFT foot is planted (its contact
    /// point at its lowest). Zero for a clip with no plant.
    float footfall_phase = 0.0f;
    /// cycle_m as a function of stride scale, over the grid
    /// [STRIDE_SCALE_MIN, STRIDE_SCALE_MAX]. Monotone where the leg has room.
    std::array<float, STRIDE_CURVE_POINTS> stride_curve{};
    /// WHICH CELLS OF stride_curve ARE A MEASUREMENT and not a hole, one bit
    /// each. Past some scale a scaled leg stops planting at all — the swing
    /// is so wide the foot never settles — and the fit says so by finding no
    /// plant. Writing a zero into the cell and letting the reader treat it as
    /// "covers no ground" is what turned the jog's lookup into a refusal:
    /// stride_scale_for saw a zero in the last cell, decided the whole curve
    /// was empty and returned scale 1, which is the clip's own 5.58 m against
    /// the 2.80 m sim asked for — 32 cm of foot slide per step, from a guard.
    ///
    /// A MASK AND NOT A COUNT, because the holes are not a tail. The jog's
    /// curve measures at 0.25, misses at 0.31 and measures again at 0.39 and
    /// every cell up to 0.97: a prefix rule threw away eight good cells for
    /// one bad one and clamped the gear to the bottom of the range.
    uint32_t stride_valid = 0;
    [[nodiscard]] bool curve_has(uint32_t i) const {
        return (stride_valid & (1u << i)) != 0;
    }
    /// HOW FAR THE MODEL HAS TO BE LIFTED so its lowest contact point stands
    /// where the rest pose's does, per stride scale, over the same grid.
    ///
    /// IT IS NOT A COSMETIC TRIM. Scaling a leg's swing about its BIND —
    /// which is how this file makes a clip cover sim's ground — also changes
    /// how far the leg REACHES: scale it down and the knee straightens, so
    /// the pelvis ought to ride HIGHER and instead the feet went through the
    /// grass. Measured on the jog at the scale sim's 3 m/s asks for: the
    /// lowest skinned vertex sat 0.157 m BELOW the ground the character was
    /// standing on. A person with straighter legs stands taller; this is that
    /// sentence, as a number, measured once per scale instead of guessed per
    /// frame.
    std::array<float, STRIDE_CURVE_POINTS> ground_curve{};
    /// How still the planted contact point actually is once the fitted travel
    /// is subtracted, metres per sample, at scale 1. A clip with no real
    /// plant says so here rather than through a plausible-looking stride.
    float plant_residual_m = 0.0f;
    /// Fraction of the cycle either foot spends in contact, at scale 1. A
    /// walk is over a half, a run well under; a number near zero means the
    /// measurement below found no plant and the stride it reports is a guess.
    float duty = 0.0f;
    /// WHAT THE ROLE'S OWN NAME RESOLVED TO, before build_clip_library's
    /// measured pick had its say, and how far that clip's planted foot slid
    /// at the gear's stride. Kept so the swap is auditable: a decision that
    /// leaves no trace of the option it rejected cannot be checked by a test
    /// or read in a report. -1 / 0 when the role kept its own clip.
    int32_t named_clip = -1;
    float named_slide_m = 0.0f;

    /// THE SECOND CLIP OF A BLENDED GEAR, and the weight it carries.
    ///
    /// A GEAR WITH NO CLIP NEAR IT IS THE ONE CASE A SINGLE CLIP CANNOT
    /// ANSWER. This asset is authored at 1.14, 5.98 and 9.12 m/s; sim's gears
    /// are 1.8, 3.0 and 6.0. The walk and the run each have a clip within a
    /// stride scale of 1.35 and 0.82 — that is a clip being ADJUSTED. The JOG
    /// has neither: reaching 3 m/s means shrinking the jog clip to 0.42 (its
    /// legs straighten, the feet skim, and grounding it lifts the whole body
    /// 0.17 m — the "figure grows 18 cm on the gear change" the owner saw) or
    /// stretching the walk to 1.84, which is a stride nobody walks. Blending
    /// the two gives a cycle that natively covers what sim asks for, so the
    /// stride scale lands near 1 and neither clip is bent far from what its
    /// author drew.
    ///
    /// THE WEIGHT IS SOLVED, NOT CHOSEN: it is the blend whose MEASURED cycle
    /// travel equals the gear's demanded travel. -1 / 0 = no blend.
    int32_t mix_clip = -1;
    float mix_duration_s = 0.0f;
    float mix_footfall = 0.0f;
    float mix_weight = 0.0f;
    /// THE AMPLITUDES THE STANCE LAYER'S GAINS ARE MEASURED AGAINST: the peak
    /// fore-aft arm pitch and the peak shoulder-over-hip twist this clip
    /// reaches over its own cycle, radians.
    ///
    /// MEASURED PER CLIP AND NOT ASSUMED, because a gain is a ratio and a
    /// ratio needs a denominator that is a fact. "Multiply the twist by 2.3"
    /// is a sentence about ONE asset; "reach STANCE_TWIST_RUN" is a sentence
    /// about the reference, and it only becomes a gain once somebody has
    /// measured what this clip does on its own.
    float arm_swing_peak_rad = 0.0f;
    float twist_peak_rad = 0.0f;
    /// THE ELBOW THIS CLIP HOLDS ON AVERAGE over its own cycle, radians. The
    /// arm layer's elbow offset is the reference's elbow minus THIS, per clip:
    /// a number solved on the idle and reused on the sprint overshot the
    /// reference by 49 degrees, because the sprint was already right.
    float elbow_mean_rad = 0.0f;
    /// What the blend cost and bought, kept so the decision is auditable the
    /// way `named_clip` keeps the rejected swap: the solo clip's slide and its
    /// ground lift at this gear's stride.
    float mix_solo_slide_m = 0.0f;
    float mix_solo_lift_m = 0.0f;

    [[nodiscard]] bool present() const { return clip >= 0; }
    [[nodiscard]] bool mixed() const { return mix_clip >= 0 && mix_weight > 0.0f; }
};

struct ClipLibrary {
    std::array<ClipEntry, CLIP_ROLE_COUNT> role{};
    /// Where this model's feet touch, and how high they touch at rest.
    ContactSet contacts;
    /// WHICH HALF OF THE SKELETON A JOINT BELONGS TO, so the legs can walk
    /// while the arms hold a sword (PoseLayers.h).
    BranchMask mask;
    /// THE ARM LAYER, solved against this model: how far the shoulders come in
    /// and how far the elbow unfolds when the hands are empty.
    ArmRelax relax;
    /// THE STANCE LAYER'S JOINTS. Nothing solved: every target it aims at is
    /// an angle the reference states outright, so what a model contributes is
    /// only which joint is which.
    StanceLayer stance;
    /// ОБХОД ТЕЛА РУКОЙ (заказ владельца 31.08, пункт 1) и формы, до которых
    /// он мерит. Формы — те же, что несут тела Jolt: второй таблицы габаритов
    /// тела в проекте нет и не заводится (правило 35).
    ArmClearance arms;
    HitboxSet boxes;
    /// СКОЛЬКО МЕСТА ОБХОД ОСТАВЛЯЕТ, метры. ПОЛЕ, А НЕ ЧТЕНИЕ КОНСТАНТЫ В
    /// КАДРЕ, и это ДВЕРЬ ДОЗЫ: ноль выключает слой ПОБИТОВО, поэтому
    /// контрольная рука приёмки («тот же кадр без обхода») выходит из того же
    /// бинарника и отличается ровно слоем (правило 47). Значение по умолчанию
    /// — строка реестра, и второго места, где оно берётся, нет.
    float arm_clearance_m = static_cast<float>(config::ARM_BODY_CLEARANCE);
    /// ЗЕРКАЛЬНАЯ РАЗМЕТКА И ДОЗА СИММЕТРИЗАЦИИ ПОХОДКИ (дополнение владельца
    /// 31.08: «никакой левой/правой стойки и ведущей ноги»). Доза 0.5 — точная
    /// антисимметрия, 0 — слой снят ПОБИТОВО, и на этом стоит контрольная рука
    /// приёмки. Поле, а не константа в кадре, по той же причине, что и
    /// клиренс: обе руки сравнения обязаны выходить из одного бинарника.
    MirrorMap mirror;
    float mirror_dose = 0.5f;
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
///
/// `skin` IS THE BODY'S OWN VERTICES, and it is optional only because a caller
/// that has no mesh (a pose fixture, a rig-only test) still needs a library.
/// Given, the hitbox table is sized off THIS body instead of off the canon
/// (fit_hitboxes_to_skin), which is what the arm-clearance layer inside this
/// library measures against — see the header note there for why a canon-sized
/// box on a raw body lets a hand into a thigh.
[[nodiscard]] ClipLibrary build_clip_library(
    const Rig& rig, const skel::Skeleton& skeleton, const SkinnedRigBinding& binding,
    std::span<const skel::AnimClip> clips,
    std::span<const platform::SkinnedVertex> skin = {});

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

/// The lift the entry's ground_curve asks for at `scale`, linearly between
/// the two grid points that bracket it. Metres, positive = up.
[[nodiscard]] float ground_lift_for(const ClipEntry& entry, float scale);

/// How long a cross-fade between two roles lasts, seconds. One number for
/// every transition on purpose: a per-pair table is a thing nobody keeps true,
/// and 0.18 s sits inside the 0.15..0.25 s band the order names.
inline constexpr float CLIP_CROSSFADE_S = 0.18f;

/// HOW LONG DRAWING OR SHEATHING TAKES, seconds. Its own number and not
/// CLIP_CROSSFADE_S: a gear change is a foot leaving the ground and has to
/// happen inside a stride, while drawing is a whole arm travelling from the
/// hip to a guard, and the order names 0.2 s for it.
inline constexpr float WEAPON_CROSSFADE_S = 0.2f;

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
    /// The SAME two instants expressed in the blend partner's own clip time
    /// (ClipEntry::mix_clip). Carried rather than recomputed in the frame
    /// because the frame has no stride phase: it interpolates between two
    /// ticks, and each tick is what knew where in the cycle it was.
    float mix_time_s = 0.0f;
    float previous_mix_time_s = 0.0f;
    /// The weapon guard runs off its OWN seconds: it is not locomotion, it has
    /// no stride, and its cycle is a man breathing over a raised blade.
    float weapon_time_s = 0.0f;
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
    float prev_mix_time_s = 0.0f;
    float prev_previous_mix_time_s = 0.0f;
    float prev_weapon_time_s = 0.0f;
    /// WHETHER THE HANDS ARE FULL, eased. 0 = sheathed (the arm layer is on
    /// and the whole body plays one clip), 1 = drawn (the arm layer is off and
    /// the upper half plays the weapon idle over the legs' locomotion).
    float weapon = 0.0f;
    float prev_weapon = 0.0f;
    /// THE STANCE LAYER'S TWO WEIGHTS, eased here rather than read from the
    /// drive in the frame, for the reason every other field in this struct is
    /// here: the frame sits between two ticks and may only interpolate.
    /// `run` is sim's own eased gear weight; `stand` is 1 while the body is
    /// not travelling, and it gates the leg half (Stance.h says why).
    float stance_run = 0.0f;
    float prev_stance_run = 0.0f;
    float stance_stand = 1.0f;
    float prev_stance_stand = 1.0f;
    /// В ВОЗДУХЕ, 0..1, сглажено тем же временем, что и смена роли (заказ
    /// владельца 31.08, пункт 2: «в воздухе поза — чистый клип»). Гасит СЛОЙ
    /// СТОЙКИ целиком: он выпрямляет колени к STANCE_KNEE_STAND, а у прыжка
    /// колени поджаты нарочно, и выпрямлять их — это и есть «ноги уходят
    /// вперёд». Замерено: стопа улетала на 0.81 м вперёд от таза при длине
    /// ноги 0.88 м.
    ///
    /// СГЛАЖЕНО, А НЕ ФЛАГОМ, по той же причине, что и всё остальное в этой
    /// структуре: отрыв от земли случается в одном тике, а слой, снятый
    /// мгновенно, — это щелчок ног на этом тике.
    float airborne = 0.0f;
    float prev_airborne = 0.0f;
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
    ///
    /// AND FOR ONE FOOT IT IS THE BEST OF ITS CONTACT JOINTS, not the worst,
    /// which is the opposite convention to the one between the two FEET and
    /// is right for the opposite reason. Two feet are two independent claims
    /// and the worse one is the answer. Two contact joints of the SAME foot
    /// are one claim seen twice: while the ball of the foot is planted the
    /// ankle is rotating ABOUT it and honestly travels five centimetres, and
    /// counting that as slide would say every real heel-to-toe roll is a
    /// defect. The question is "did this foot have a point that stayed still",
    /// and the best-planted joint is the one that answers it.
    float worst_per_step_m = 0.0f;
    /// The same measurement made at the ANKLE alone — the unit the previous
    /// wave reported and the one the order names. Kept beside the headline so
    /// the two waves' numbers can be compared at all.
    float ankle_per_step_m = 0.0f;
    /// The same plant measured as summed path instead of spread — the strict
    /// reading. It can only be larger; the gap between the two is jitter.
    float path_per_step_m = 0.0f;
    float cycle_travel_m = 0.0f;   ///< what the clip actually covered
    float demanded_m = 0.0f;       ///< what sim said the body covered
    /// How still the planted contact point is once the FITTED travel is taken
    /// out, per sample. It answers "is there a plant here at all", which a
    /// slide figure alone cannot: a clip whose foot never stops moving can
    /// still report a small drift if its plant window is two samples long.
    float plant_residual_m = 0.0f;
};
[[nodiscard]] FootSlide measure_foot_slide(const skel::Skeleton& skeleton,
                                           const SkinnedRigBinding& binding,
                                           std::span<const skel::AnimClip> clips,
                                           const ClipLibrary& lib, ClipRole role,
                                           float step_length_m, bool stride_match,
                                           uint32_t samples);

} // namespace dfn::anim
