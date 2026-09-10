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

#include "engine/anim/sources/FootIk.h"
#include "engine/anim/sources/Locomotion.h"

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
constexpr float TEMPO_BAND = static_cast<float>(config::LOCOMOTION_TEMPO_BAND);
/// СКОРОСТЬ, ПО КОТОРОЙ ВЫБИРАЕТСЯ РОЛЬ: намерение, когда перемещение ведёт
/// стопа (иначе роль выбирала бы себя сама через капсулу), факт — в прежнем шве.
[[nodiscard]] float drive_speed(const ClipLibrary& lib, const BodyDrive& drive) {
    (void)lib;
    return drive.want_speed_mps;
}

/// THE CEILING ON A STANCE GAIN. A gain is a ratio against a MEASURED peak,
/// and a clip whose peak is almost nothing would ask for almost anything: the
/// Idle's twist measures 0.004 rad, and reaching the reference's running 0.23
/// from there is a multiplier of 57, i.e. a body wringing itself out while
/// standing still. Not a NUMBERS row: it guards a division in this file and
/// has no second reader. The 0.02 rad floor beside it is the same guard from
/// the other side — below it there is no waveform to scale, only noise.
constexpr float STANCE_GAIN_MAX = 3.0f;

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
/// КОРОЧЕ ЭТОГО КАСАНИЕ — НЕ ПОСТАНОВКА (расписание контактов, §16): стопа в
/// махе задевает пол на 1…3 выборки при приземлении и отрыве, и такое
/// «касание» дробило опору на куски (замер 10.09: по 2…4 куска на цикл).
constexpr float CONTACT_MIN_PLANT_S = 0.08f;

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
    // ХОДЬБА И ТРУСЦА — MIXAMO (04.09, §11.1): часы от пути требуют клип,
    // чья стопа в опоре идёт со скоростью заказа в полосе темпа. UAL Walk —
    // 1,0 м/с при заказе 1,8 (в полосу не входит: снос 25 см/шаг), MX_Walking
    // — 1,72. Трусца 3,0: MX_Jog_Forward — 2,02 (мимо), UAL Jog_Fwd_Loop 2,76,
    // MX_Running 3,01, MX_Standard_Run 3,16 — ближе всех к авторскому темпу.
    // Спринт остаётся UAL Sprint_Loop (стопа 4,79 при заказе 6,0 — 1,25×, край
    // полосы; Mixamo-спринта в паке нет — MX_Standard_Run всего 3,1).
    // Роль по умолчанию — за словом владельца; DFN_CLIP_ROLES примеряет.
    // РОЛИ ПО УМОЛЧАНИЮ — СЛОВО ВЛАДЕЛЬЦА 10.09 (переделка на дорожку корня,
    // LOCOMOTION_GROUNDED.md §16): Walk=MX_Walking (1,71 м/с), Jog=MX_Standard_Run
    // (4,2 м/с) — у них авторский ход таза в дорожке; UAL Walk_Loop/Jog_Fwd_Loop
    // остаются запасными (дорожка у них синтезирована по стопам на выпечке).
    {ClipRole::Walk, "Walk",
     {"MX_Walking", "Walk_Loop", "Walk_Fwd_Loop", "Walk"}},
    {ClipRole::Jog, "Jog",
     {"MX_Standard_Run", "Jog_Fwd_Loop", "Jog_Loop", "Jog"}},
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
    {ClipRole::Backward, "Backward",
     {"MX_Walking_Backwards", "KK_Walking_Backwards", "Walk_Back_Loop", "Walking_Backwards"}},
    {ClipRole::StrafeL, "StrafeL",
     {"MX_Left_Strafe_Walking", "Strafe_Left_Loop", "Walk_Left_Loop", "Left_Strafe_Walking"}},
    {ClipRole::StrafeR, "StrafeR",
     {"MX_Right_Strafe_Walking", "Strafe_Right_Loop", "Walk_Right_Loop", "Right_Strafe_Walking"}},
    {ClipRole::StrafeRunL, "StrafeRunL",
     {"MX_Left_Strafe", "KK_Running_Strafe_Left", "Run_Strafe_Left_Loop", "Jog_Left_Loop"}},
    {ClipRole::StrafeRunR, "StrafeRunR",
     {"MX_Right_Strafe", "KK_Running_Strafe_Right", "Run_Strafe_Right_Loop", "Jog_Right_Loop"}},
    // ПЕРЕХОДЫ (04.09). Клипы Mixamo, скачанные владельцем: Start_Walking —
    // шаг с места в ходьбу; Idle_To_Sprint — рывок с места; Run_To_Stop —
    // торможение в стойку; Left/Right_Turn_90 — поворот на месте с переступом.
    // Остановки шага в паках нет — роль остаётся нерешённой, и это видно в
    // паспорте библиотеки (тикет владельцу: скачать Walk To Stop).
    {ClipRole::StartWalk, "StartWalk",
     {"MX_Start_Walking", "Walk_Start", "Start_Walking", ""}},
    {ClipRole::StartRun, "StartRun",
     {"MX_Idle_To_Sprint", "Run_Start", "Idle_To_Sprint", ""}},
    {ClipRole::StopWalk, "StopWalk",
     {"MX_Stop_Walking", "MX_Walk_To_Stop", "Walk_Stop", "Walk_To_Stop"}},
    {ClipRole::StopRun, "StopRun",
     {"MX_Run_To_Stop", "MX_Run_to_stop", "Run_Stop", "Run_To_Stop"}},
    {ClipRole::TurnL, "TurnL",
     {"MX_Left_Turn_90", "MX_Left_turn_90", "Turn_Left_90", "Left_Turn_90"}},
    {ClipRole::TurnR, "TurnR",
     {"~MX_Left_Turn_90", "MX_Right_Turn_90", "Turn_Right_90", "Right_Turn_90"}},
    {ClipRole::Stagger, "Stagger",
     {"MX_Sword_and_shield_impact", "MX_Sword_and_shield_impact_2", "Hit_Chest", "Hit_Reaction"}},
    {ClipRole::Turn180L, "Turn180L",
     {"MX_Walking_Turn_180", "MX_Sword_and_shield_180_turn", "Turn_Left_180", "Left_Turn_180"}},
    {ClipRole::Turn180R, "Turn180R",
     {"~MX_Walking_Turn_180", "~MX_Sword_and_shield_180_turn", "Turn_Right_180", "Right_Turn_180"}},
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

/// Разница рысков, сведённая в (−π, π]: «камера правее корпуса на 10°», а не
/// «на 350° левее».
[[nodiscard]] float wrap_pi(float a) {
    const float two_pi = 2.0f * glm::pi<float>();
    a = std::fmod(a + glm::pi<float>(), two_pi);
    if (a < 0.0f) {
        a += two_pi;
    }
    return a - glm::pi<float>();
}

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

const ClipEntry& entry_for(const ClipLibrary& lib, ClipRole role, int32_t variant) {
    if (role == ClipRole::Idle && variant >= 0
        && static_cast<std::size_t>(variant) < lib.idle_variants.size()) {
        return lib.idle_variants[static_cast<std::size_t>(variant)];
    }
    return lib[role];
}

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

MoveDir move_dir_class(const glm::vec3& move_dir_model, MoveDir previous) {
    // Угол между ходом и лицом (−Z): 0 — вперёд, ±90 — бок, 180 — назад.
    const float x = move_dir_model.x;
    const float f = -move_dir_model.z;
    if (x * x + f * f < 1.0e-6f) {
        return previous;
    }
    const float deg = glm::degrees(std::atan2(std::abs(x), f)); // 0..180
    const float strafe = static_cast<float>(config::DIR_STRAFE_DEG);
    const float back = static_cast<float>(config::DIR_BACK_DEG);
    const float h = static_cast<float>(config::DIR_HYSTERESIS_DEG);
    // ГИСТЕРЕЗИС: границу класса, в котором стоим, отодвигаем на h наружу.
    const bool was_fwd = previous == MoveDir::Forward;
    const bool was_back = previous == MoveDir::Backward;
    const float fwd_edge = strafe + (was_fwd ? h : -h);
    const float back_edge = back + (was_back ? -h : h);
    if (deg < fwd_edge) {
        return MoveDir::Forward;
    }
    if (deg >= back_edge) {
        return MoveDir::Backward;
    }
    const MoveDir side = x >= 0.0f ? MoveDir::StrafeR : MoveDir::StrafeL;
    // внутри бокового сектора сторона тоже с гистерезисом у ±90 не нужна:
    // смена стороны — это смена знака x, границы нет посередине.
    return side;
}

glm::vec3 travel_axis(const glm::vec3& move_dir_model) {
    const glm::vec3 flat{move_dir_model.x, 0.0f, move_dir_model.z};
    const float len = glm::length(flat);
    return len > 1.0e-6f ? -flat / len : glm::vec3{0.0f, 0.0f, 1.0f};
}

glm::vec3 role_move_dir(ClipRole r) {
    switch (r) {
    case ClipRole::Backward: return glm::vec3{0.0f, 0.0f, 1.0f};
    case ClipRole::StrafeL: case ClipRole::StrafeRunL: return glm::vec3{-1.0f, 0.0f, 0.0f};
    case ClipRole::StrafeR: case ClipRole::StrafeRunR: return glm::vec3{1.0f, 0.0f, 0.0f};
    default: return glm::vec3{0.0f, 0.0f, -1.0f};
    }
}

bool locomotion_role(ClipRole r) {
    return r == ClipRole::Walk || r == ClipRole::Jog || r == ClipRole::Sprint
           || r == ClipRole::CrouchWalk || r == ClipRole::Backward
           || r == ClipRole::StrafeL || r == ClipRole::StrafeR
           || r == ClipRole::StrafeRunL || r == ClipRole::StrafeRunR
           // ПЕРЕХОДЫ ТОЖЕ ВЕЗУТ ТЕЛО: старт разгоняет своими ногами,
           // остановка тормозит своими, поворот своими поворачивает —
           // заявка корня от стопы выдаётся и на них.
           || one_shot_role(r);
}

bool one_shot_role(ClipRole r) {
    return r == ClipRole::JumpStart || r == ClipRole::JumpLand
           || r == ClipRole::StartWalk || r == ClipRole::StartRun
           || r == ClipRole::StopWalk || r == ClipRole::StopRun
           || r == ClipRole::TurnL || r == ClipRole::TurnR || r == ClipRole::Stagger
           || r == ClipRole::Turn180L || r == ClipRole::Turn180R;
}

/// Роль перехода — та, что ведёт ноги на месте (старт, остановка, поворот);
/// прыжок сюда не входит: он не про переступ, а про полёт.
bool transit_role(ClipRole r) {
    return r == ClipRole::StartWalk || r == ClipRole::StartRun
           || r == ClipRole::StopWalk || r == ClipRole::StopRun
           || r == ClipRole::TurnL || r == ClipRole::TurnR || r == ClipRole::Stagger
           || r == ClipRole::Turn180L || r == ClipRole::Turn180R;
}

namespace {
MoveDir role_dir_class(ClipRole r) {
    switch (r) {
    case ClipRole::Backward: return MoveDir::Backward;
    case ClipRole::StrafeL: case ClipRole::StrafeRunL: return MoveDir::StrafeL;
    case ClipRole::StrafeR: case ClipRole::StrafeRunR: return MoveDir::StrafeR;
    default: return MoveDir::Forward;
    }
}
} // namespace

namespace {

/// Значение канала в момент t — линейно между ключами (поворот — nlerp);
/// вне диапазона — крайний ключ. Для дорожки корня и позы кадра 0.
[[nodiscard]] glm::vec4 channel_at(const skel::AnimChannel& ch, float t) {
    if (ch.times.empty() || ch.values.size() != ch.times.size()) {
        return glm::vec4{0.0f};
    }
    if (t <= ch.times.front()) {
        return ch.values.front();
    }
    if (t >= ch.times.back()) {
        return ch.values.back();
    }
    const auto hi = std::upper_bound(ch.times.begin(), ch.times.end(), t);
    const std::size_t i1 = static_cast<std::size_t>(hi - ch.times.begin());
    const std::size_t i0 = i1 - 1;
    const float span = ch.times[i1] - ch.times[i0];
    const float f = span > 1.0e-6f ? (t - ch.times[i0]) / span : 0.0f;
    glm::vec4 v = ch.values[i0] + (ch.values[i1] - ch.values[i0]) * f;
    if (ch.path == skel::AnimPath::Rotation) {
        // nlerp: короткая дуга
        glm::vec4 b = ch.values[i1];
        if (glm::dot(ch.values[i0], b) < 0.0f) {
            b = -b;
        }
        v = ch.values[i0] + (b - ch.values[i0]) * f;
        const float len = glm::length(v);
        if (len > 1.0e-6f) {
            v /= len;
        }
    }
    return v;
}

/// Поза сустава `joint` в момент t по каналам клипа (бинд там, где канала нет).
void joint_pose_at(const skel::Skeleton& skeleton, const skel::AnimClip& clip, uint32_t joint,
                   float t, glm::vec3& translation, glm::quat& rotation) {
    translation = skeleton.joints[joint].bind_translation;
    rotation = skeleton.joints[joint].bind_rotation;
    for (const skel::AnimChannel& ch : clip.channels) {
        if (ch.joint != joint) {
            continue;
        }
        const glm::vec4 v = channel_at(ch, t);
        if (ch.path == skel::AnimPath::Translation) {
            translation = glm::vec3{v};
        } else if (ch.path == skel::AnimPath::Rotation) {
            rotation = glm::normalize(glm::quat{v.w, v.x, v.y, v.z});
        }
    }
}

} // namespace

void neutralize_root(const skel::Skeleton& skeleton, const skel::AnimClip& clip,
                     std::span<JointLocal> out) {
    const std::size_t n = std::min(skeleton.size(), out.size());
    for (std::size_t j = 0; j < n; ++j) {
        if (skeleton.joints[j].parent >= 0) {
            continue;
        }
        joint_pose_at(skeleton, clip, static_cast<uint32_t>(j), 0.0f, out[j].translation,
                      out[j].rotation);
    }
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
    // КОРЕНЬ — НЕ ПОЗА (§16): дорожка сустава root ведёт КАПСУЛУ (RootTrack),
    // а поза рисуется на месте — корень сбрасывается в позу кадра 0 клипа.
    // У клипов без дорожки корень постоянен, и это бит-в-бит их прежний
    // корень; у клипов с дорожкой — тело стоит над капсулой на любом кадре.
    neutralize_root(skeleton, clip, out.first(n));
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
                                          const MixSource& src,
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
    //
    // AND NOTHING ENDS UP ABOVE IT EITHER: the lift is SIGNED. A clip whose
    // feet never come down to the rest pose's ground is dropped by the same
    // rule that lifts one whose feet go through it. Measured on the MPFB
    // body: Crouch_Fwd_Loop retargeted onto its longer legs kept both toes
    // 6 cm above the ground for the whole cycle, a one-sided lift left it
    // there, no sample ever counted as "down", and the crouch walk read as
    // a clip that covers no ground (cycle 0, duty 0). The jump triple is not
    // grounded at all (see the standing-roles note in build_clip_library),
    // so a signed lift cannot drag an arc down.
    float deepest = std::numeric_limits<float>::infinity();
    for (std::size_t c = 0; c < track.joint.size(); ++c) {
        for (uint32_t k = 0; k < samples; ++k) {
            deepest = std::min(deepest, track.joint[c][k].y - track.rest_y[c]);
        }
    }
    track.lift = std::isfinite(deepest) ? -deepest : 0.0f;
    return track;
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
    return src;
}

} // namespace

// --- ДОРОЖКА КОРНЯ И РАСПИСАНИЕ КОНТАКТОВ (§16) ------------------------------

namespace {

/// Рыск сим'а (+ по часовой сверху) из закрутки ориентации вокруг вертикали.
[[nodiscard]] float twist_yaw_sim(const glm::quat& q) {
    // swing-twist вокруг +Y: twist = normalize(w, 0, y, 0); угол = 2·atan2(y, w)
    // (glm против часовой) → знак сим'а обратный, как в pelvis_yaw.
    return -2.0f * std::atan2(q.y, q.w);
}

[[nodiscard]] float wrap_pi_f(float a) {
    return std::atan2(std::sin(a), std::cos(a));
}

} // namespace

void measure_root_track(const skel::Skeleton& skeleton, const skel::AnimClip& clip,
                        bool mirrored, ClipEntry& entry) {
    RootTrack& tr = entry.root;
    tr = RootTrack{};
    int32_t root = -1;
    for (std::size_t j = 0; j < skeleton.size(); ++j) {
        if (skeleton.joints[j].parent < 0) {
            root = static_cast<int32_t>(j);
            break;
        }
    }
    if (root < 0 || clip.duration_s <= 0.0f) {
        return;
    }
    bool keyed = false;
    for (const skel::AnimChannel& ch : clip.channels) {
        if (ch.joint == static_cast<uint32_t>(root) && ch.times.size() >= 2) {
            keyed = true;
        }
    }
    glm::vec3 t0;
    glm::quat q0;
    joint_pose_at(skeleton, clip, static_cast<uint32_t>(root), 0.0f, t0, q0);
    tr.pose0_t = t0;
    tr.pose0_r = q0;
    if (!keyed) {
        return;
    }
    const glm::quat inv0 = glm::inverse(q0);
    float prev_yaw = 0.0f;
    float length = 0.0f;
    for (uint32_t i = 0; i < ROOT_TRACK_POINTS; ++i) {
        const float t = clip.duration_s * static_cast<float>(i)
                        / static_cast<float>(ROOT_TRACK_POINTS - 1);
        glm::vec3 ti;
        glm::quat qi;
        joint_pose_at(skeleton, clip, static_cast<uint32_t>(root), t, ti, qi);
        const glm::vec3 d = ti - t0;
        glm::vec2 xz{d.x, d.z};
        float yaw = twist_yaw_sim(glm::normalize(qi * inv0));
        if (mirrored) {
            xz.x = -xz.x;
            yaw = -yaw;
        }
        // разворот: накопление через разницу с прошлой точкой
        const float unwrapped = i == 0 ? 0.0f : prev_yaw + wrap_pi_f(yaw - wrap_pi_f(prev_yaw));
        prev_yaw = unwrapped;
        tr.xz[i] = xz;
        tr.yaw[i] = unwrapped;
        if (i > 0) {
            length += glm::length(xz - tr.xz[i - 1]);
        }
    }
    tr.total_m = length;
    tr.total_yaw = tr.yaw[ROOT_TRACK_POINTS - 1];
    tr.mps = length / clip.duration_s;
    tr.valid = length > 0.05f || std::abs(tr.total_yaw) > glm::radians(5.0f);
    entry.settle_phase = 0.0f;
    if (tr.valid) {
        const float dphase = 1.0f / static_cast<float>(ROOT_TRACK_POINTS - 1);
        const float dts = dphase * clip.duration_s;
        const float v_still = static_cast<float>(config::CONTACT_STILL_MPS) / 3.0f;
        const float w_still = glm::radians(10.0f);
        for (uint32_t i = ROOT_TRACK_POINTS - 1; i >= 1; --i) {
            const float v = glm::length(tr.xz[i] - tr.xz[i - 1]) / dts;
            const float w = std::abs(tr.yaw[i] - tr.yaw[i - 1]) / dts;
            if (v >= v_still || w >= w_still) {
                entry.settle_phase = std::min(1.0f, static_cast<float>(i) * dphase + 2.0f * dphase);
                break;
            }
        }
    }
    if (!tr.valid) {
        // клип без дорожки — ноль ровно, не шум квантования: покой не едет
        tr.xz.fill(glm::vec2{0.0f});
        tr.yaw.fill(0.0f);
        tr.total_m = 0.0f;
        tr.total_yaw = 0.0f;
        tr.mps = 0.0f;
    }
}

glm::vec2 root_track_xz_at(const RootTrack& track, float phase) {
    const float f = std::clamp(phase, 0.0f, 1.0f) * static_cast<float>(ROOT_TRACK_POINTS - 1);
    const auto i0 = static_cast<uint32_t>(std::floor(f));
    const uint32_t i1 = std::min(i0 + 1, ROOT_TRACK_POINTS - 1);
    const float a = f - static_cast<float>(i0);
    return track.xz[i0] + (track.xz[i1] - track.xz[i0]) * a;
}

float root_track_yaw_at(const RootTrack& track, float phase) {
    const float f = std::clamp(phase, 0.0f, 1.0f) * static_cast<float>(ROOT_TRACK_POINTS - 1);
    const auto i0 = static_cast<uint32_t>(std::floor(f));
    const uint32_t i1 = std::min(i0 + 1, ROOT_TRACK_POINTS - 1);
    const float a = f - static_cast<float>(i0);
    return track.yaw[i0] + (track.yaw[i1] - track.yaw[i0]) * a;
}

RootDelta root_track_delta(const RootTrack& track, float from_phase, float to_phase,
                           bool cyclic, float yaw_warp) {
    RootDelta out;
    if (!track.valid) {
        return out;
    }
    // ПЕТЛЯ: фаза to < from — прошли стык; ход = (до конца) + (от начала).
    // Хвостовая точка дорожки — конец клипа, стык считается непрерывным.
    if (cyclic && to_phase < from_phase) {
        const RootDelta a = root_track_delta(track, from_phase, 1.0f, false, yaw_warp);
        const RootDelta b = root_track_delta(track, 0.0f, to_phase, false, yaw_warp);
        out.xz = a.xz + b.xz;
        out.yaw = a.yaw + b.yaw;
        return out;
    }
    out.xz = root_track_xz_at(track, to_phase) - root_track_xz_at(track, from_phase);
    out.yaw = (root_track_yaw_at(track, to_phase) - root_track_yaw_at(track, from_phase)) * yaw_warp;
    return out;
}

void measure_contact_schedule(const skel::Skeleton& skeleton, const SkinnedRigBinding& binding,
                              const ContactSet& contacts, std::span<const skel::AnimClip> clips,
                              bool cyclic, ClipEntry& entry) {
    entry.plant_count = {0, 0};
    entry.plant_phase = {};
    entry.lift_phase = {};
    if (!entry.present() || entry.duration_s <= 0.0f || !contacts.valid()) {
        return;
    }
    const ContactTrack track =
        track_contacts(skeleton, binding, contacts, mix_of(entry, clips), MEASURE_SAMPLES);
    if (track.samples() == 0) {
        return;
    }
    const std::size_t n = track.samples();
    const float dt = entry.duration_s / static_cast<float>(n);
    const float still = static_cast<float>(config::CONTACT_STILL_MPS);
    for (std::size_t side = 0; side < 2; ++side) {
        std::vector<char> down(n, 0);
        for (std::size_t k = 0; k < n; ++k) {
            const float ph = static_cast<float>(k) / static_cast<float>(n);
            const float ph1 = static_cast<float>(k + 1) / static_cast<float>(n);
            const glm::vec2 r0 = root_track_xz_at(entry.root, ph);
            const glm::vec2 r1 = root_track_xz_at(entry.root, cyclic && k + 1 == n ? 1.0f : ph1);
            const float y0 = root_track_yaw_at(entry.root, ph);
            const float y1 = root_track_yaw_at(entry.root, cyclic && k + 1 == n ? 1.0f : ph1);
            for (std::size_t c = 0; c < track.tracks(); ++c) {
                if (track.side[c] != side || !track.down(c, k, GRIP_TOLERANCE_M)) {
                    continue;
                }
                // мировая точка = дорожка + повёрнутая локальная (рыск сим'а по
                // часовой = поворот на −yaw в системе x/z)
                const auto world = [](const glm::vec3& p, const glm::vec2& r, float yaw) {
                    const float c0 = std::cos(-yaw);
                    const float s0 = std::sin(-yaw);
                    return r + glm::vec2{c0 * p.x - s0 * p.z, s0 * p.x + c0 * p.z};
                };
                const glm::vec2 w0 = world(track.joint[c][k], r0, y0);
                const glm::vec2 w1 = world(track.joint[c][k + 1], r1, y1);
                if (glm::length(w1 - w0) / dt < still) {
                    down[k] = 1;
                    break;
                }
            }
        }
        // отрезки «стоит»
        struct Seg {
            std::size_t from;
            std::size_t to; // exclusive
        };
        std::vector<Seg> segs;
        for (std::size_t k = 0; k < n; ++k) {
            if (down[k] && (k == 0 || !down[k - 1])) {
                segs.push_back({k, k});
            }
            if (down[k]) {
                segs.back().to = k + 1;
            }
        }
        {
            const auto min_len = static_cast<std::size_t>(std::ceil(CONTACT_MIN_PLANT_S / dt));
            std::vector<Seg> kept;
            for (const Seg& sg : segs) {
                if (sg.to - sg.from >= min_len) {
                    kept.push_back(sg);
                }
            }
            // БЕГ СТОИТ КОРОТКО: у MX_Standard_Run неподвижность стопы длится
            // ~0,07 с, у спринта ~0,03 с — короче CONTACT_MIN_PLANT_S, и фильтр
            // «касание — не постановка» выбрасывал ВСЕ опоры бега (замер
            // 11.09: постановок 0/0, шаги без событий). Порог режет только
            // дребезг рядом с настоящей опорой: если не осталось ничего —
            // самая длинная неподвижность и есть постановка.
            if (kept.empty() && !segs.empty()) {
                const Seg longest = *std::max_element(
                    segs.begin(), segs.end(),
                    [](const Seg& a, const Seg& b) { return (a.to - a.from) < (b.to - b.from); });
                kept.push_back(longest);
            }
            // …но касание в самом начале/конце петли — половина одной постановки
            if (cyclic && !segs.empty() && segs.front().from == 0 && segs.back().to == n
                && (segs.front().to - segs.front().from) + (segs.back().to - segs.back().from)
                       >= min_len) {
                if (kept.empty() || kept.front().from != 0) {
                    kept.insert(kept.begin(), segs.front());
                }
                if (kept.back().to != n) {
                    kept.push_back(segs.back());
                }
            }
            segs = kept;
        }
        if (cyclic && segs.size() >= 2 && segs.front().from == 0 && segs.back().to == n) {
            // постановка через стык петли — одна: начинается в хвосте
            segs.front().from = segs.back().from;
            segs.front().to += n;
            segs.pop_back();
        }
        std::sort(segs.begin(), segs.end(),
                  [](const Seg& a, const Seg& b) { return (a.to - a.from) > (b.to - b.from); });
        if (segs.size() > MAX_PLANTS_PER_SIDE) {
            std::fprintf(stderr,
                         "[anim] clip %d: %zu contact segments on side %zu, keeping the %u "
                         "longest\n",
                         entry.clip, segs.size(), side, MAX_PLANTS_PER_SIDE);
            segs.resize(MAX_PLANTS_PER_SIDE);
        }
        std::sort(segs.begin(), segs.end(), [](const Seg& a, const Seg& b) { return a.from < b.from; });
        for (std::size_t i = 0; i < segs.size(); ++i) {
            entry.plant_phase[side][i] = static_cast<float>(segs[i].from % n) / static_cast<float>(n);
            entry.lift_phase[side][i] = static_cast<float>(segs[i].to % n) / static_cast<float>(n);
        }
        entry.plant_count[side] = static_cast<uint8_t>(segs.size());
    }
}

/// Матрицы модели по локальным TRS позы — для замеров сборки библиотеки.
static void model_matrices_for(const skel::Skeleton& skeleton, std::span<const JointLocal> sample,
                               std::vector<glm::mat4>& local, std::vector<glm::mat4>& model) {
    local.resize(skeleton.size());
    model.resize(skeleton.size());
    for (std::size_t j = 0; j < skeleton.size() && j < sample.size(); ++j) {
        local[j] = glm::translate(glm::mat4{1.0f}, sample[j].translation)
                   * glm::mat4_cast(glm::normalize(sample[j].rotation))
                   * glm::scale(glm::mat4{1.0f}, sample[j].scale);
    }
    skel::skeleton_model_matrices(skeleton, local, model);
}

ClipLibrary build_clip_library(const Rig& rig, const skel::Skeleton& skeleton,
                               const SkinnedRigBinding& binding,
                               std::span<const skel::AnimClip> clips,
                               std::span<const platform::SkinnedVertex> skin,
                               std::string_view role_overrides) {
    ClipLibrary lib;
    lib.contacts = build_contacts(rig, skeleton, binding);
    // РОЛЬ ПО ДВЕРИ: "Walk=KK_Walking_A,Jog=KK_Running_A" — чужой клип
    // примеряется на роль без пересборки ассета (владелец 02.09-2: клипы из
    // интернета переиспользовать). Названного клипа нет — роль из таблицы.
    const auto override_for = [&](std::string_view role) -> std::string_view {
        std::string_view rest = role_overrides;
        while (!rest.empty()) {
            const std::size_t comma = rest.find(',');
            const std::string_view pair = rest.substr(0, comma);
            rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
            const std::size_t eq = pair.find('=');
            if (eq != std::string_view::npos && pair.substr(0, eq) == role) {
                return pair.substr(eq + 1);
            }
        }
        return {};
    };
    for (const RoleNames& row : ROLE_NAMES) {
        ClipEntry& entry = lib.role[role_index(row.role)];
        if (const std::string_view want = override_for(row.name); !want.empty()) {
            for (std::size_t c = 0; c < clips.size(); ++c) {
                if (same_name(clips[c].name, want)) {
                    entry.clip = static_cast<int32_t>(c);
                    entry.duration_s = clips[c].duration_s;
                    break;
                }
            }
        }
        for (const std::string_view want_raw : row.clips) {
            if (entry.present()) {
                break;
            }
            if (want_raw.empty()) {
                continue;
            }
            // «~имя» — зеркало клипа (ClipEntry::mirrored)
            const bool mirror = want_raw.front() == '~';
            const std::string_view want = mirror ? want_raw.substr(1) : want_raw;
            for (std::size_t c = 0; c < clips.size(); ++c) {
                if (same_name(clips[c].name, want)) {
                    entry.clip = static_cast<int32_t>(c);
                    entry.duration_s = clips[c].duration_s;
                    entry.mirrored = mirror;
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
    // ДОРОЖКА КОРНЯ И РАСПИСАНИЕ КОНТАКТОВ (§16) — у каждой разрешённой роли.
    for (const RoleNames& row : ROLE_NAMES) {
        ClipEntry& entry = lib.role[role_index(row.role)];
        if (!entry.present()) {
            continue;
        }
        measure_root_track(skeleton, clips[static_cast<std::size_t>(entry.clip)], entry.mirrored,
                           entry);
        measure_contact_schedule(skeleton, binding, lib.contacts, clips,
                                 locomotion_role(row.role), entry);
        // ФАЗА ОПОРЫ ЛЕВОЙ СТОПЫ — середина её первого окна по расписанию.
        entry.footfall_phase = 0.0f;
        if (entry.plant_count[0] > 0) {
            const float p = entry.plant_phase[0][0];
            const float l = entry.lift_phase[0][0];
            const float len = l >= p ? l - p : l - p + 1.0f;
            entry.footfall_phase = wrap01(p + 0.5f * len);
        }
    }
    // ЛЕВЫЙ ИЛИ ПРАВЫЙ — ПО ЗАМЕРЕННОМУ ЗНАКУ: имя клипа поворота не говорит,
    // куда он крутит (MX_Walking_Turn_180 — вправо); рыск сим'а + по часовой,
    // левый поворот отрицательный.
    for (const auto [l, r] : {std::pair{ClipRole::TurnL, ClipRole::TurnR},
                              std::pair{ClipRole::Turn180L, ClipRole::Turn180R}}) {
        ClipEntry& el = lib.role[role_index(l)];
        ClipEntry& er = lib.role[role_index(r)];
        if (el.present() && el.root.valid && el.root.total_yaw > 0.0f) {
            std::fprintf(stderr, "[anim] %.*s крутит вправо (%.0f°) — роли %.*s/%.*s поменяны местами\n",
                         static_cast<int>(role_name(l).size()), role_name(l).data(),
                         static_cast<double>(glm::degrees(el.root.total_yaw)),
                         static_cast<int>(role_name(l).size()), role_name(l).data(),
                         static_cast<int>(role_name(r).size()), role_name(r).data());
            std::swap(el, er);
        }
    }
    // ПЕРЕДАЧА ХОДА ОТ СТАРТА К ЦИКЛУ: фаза старта, с которой его дорожка идёт
    // не медленнее START_HANDOFF_FRAC × скорости цикла.
    for (const auto [start, cycle] : {std::pair{ClipRole::StartWalk, ClipRole::Walk},
                                      std::pair{ClipRole::StartRun, ClipRole::Sprint}}) {
        ClipEntry& s = lib.role[role_index(start)];
        const ClipEntry& c = lib.role[role_index(cycle)];
        s.handoff_phase = -1.0f;
        if (!s.present() || !s.root.valid || !c.present() || !c.root.valid || c.root.mps <= 0.0f) {
            continue;
        }
        const float want = static_cast<float>(config::START_HANDOFF_FRAC) * c.root.mps;
        const float dphase = 1.0f / static_cast<float>(ROOT_TRACK_POINTS - 1);
        for (uint32_t i = 1; i < ROOT_TRACK_POINTS; ++i) {
            const float v = glm::length(s.root.xz[i] - s.root.xz[i - 1]) / (dphase * s.duration_s);
            if (v >= want) {
                s.handoff_phase = static_cast<float>(i) * dphase;
                break;
            }
        }
    }
    // THE STRIDE MEASUREMENTS. Only the roles that travel need them: asking
    // for a stride curve on Idle produces a row of zeros that every reader
    // would then have to special-case.
    // ВАРИАНТЫ ПОКОЯ — по именам, что есть в файле; пьяный — отдельно.
    {
        const std::string_view variants[] = {"MX_Idle_1", "MX_Idle_2", "MX_Idle_3", "MX_Idle_4",
                                             "MX_Idle_5", "MX_Happy_Idle", "MX_Sad_Idle"};
        const auto add = [&](std::string_view want) {
            for (std::size_t c = 0; c < clips.size(); ++c) {
                if (same_name(clips[c].name, want) && clips[c].duration_s > 0.0f) {
                    ClipEntry e;
                    e.clip = static_cast<int32_t>(c);
                    e.duration_s = clips[c].duration_s;
                    lib.idle_variants.push_back(e);
                    return true;
                }
            }
            return false;
        };
        for (const std::string_view v : variants) {
            add(v);
        }
        if (add("MX_Drunk_Idle_Variation")) {
            lib.drunk_variant = static_cast<int32_t>(lib.idle_variants.size()) - 1;
        }
    }
    lib.mask = build_branch_mask(skeleton, binding);
    lib.stance = build_stance_layer(rig, skeleton, binding);
    lib.arms = build_arm_clearance(skeleton, binding);
    lib.mirror = build_mirror_map(skeleton);
    lib.look = build_look_layer(skeleton);
    lib.boxes = build_hitboxes(rig.proportions);
    // ...И ПОДОГНАНЫ ПО КОЖЕ ЭТОГО ТЕЛА, если она пришла. Канонные размеры
    // остаются отправной точкой (у части, за которую не голосует ни одна
    // вершина, они и остаются), но на сыром теле бедро на треть толще канона,
    // а талия на пятую часть уже — и слой клиренса ниже мерит именно этими
    // коробками.
    if (!skin.empty()) {
        fit_hitboxes_to_skin(lib.boxes, rig, skeleton, binding, skin);
    }
    // WHAT EACH CLIP SWINGS ON ITS OWN, so the stance layer's gains have a
    // denominator that is a measurement (ClipEntry::arm_swing_peak_rad). Over
    // the clip's OWN cycle and at stride scale 1: the gains multiply a shape,
    // and the shape is not what the stride scale changes.
    {
        std::vector<JointLocal> probe(skeleton.size());
        for (uint32_t r = 0; r < CLIP_ROLE_COUNT; ++r) {
            ClipEntry& entry = lib.role[r];
            if (!entry.present() || entry.duration_s <= 0.0f) {
                continue;
            }
            float elbow_sum = 0.0f;
            for (uint32_t i = 0; i < MEASURE_SAMPLES; ++i) {
                const float t = entry.duration_s * float(i) / float(MEASURE_SAMPLES);
                sample_clip_pose(skeleton,
                                 clips[static_cast<std::size_t>(entry.clip)], t, probe);
                const StanceMetrics m = measure_stance(skeleton, binding, probe);
                entry.arm_swing_peak_rad =
                    std::max(entry.arm_swing_peak_rad,
                             0.5f * std::abs(m.arm_split_rad()));
                entry.twist_peak_rad =
                    std::max(entry.twist_peak_rad, std::abs(m.shoulder_twist_rad));
                elbow_sum += 0.5f * (m.elbow_rad[0] + m.elbow_rad[1]);
            }
            entry.elbow_mean_rad = elbow_sum / float(MEASURE_SAMPLES);
        }
    }
    const ClipEntry& idle = lib[ClipRole::Idle];
    if (idle.present()) {
        std::vector<JointLocal> reference(skeleton.size());
        sample_clip_pose(skeleton, clips[static_cast<std::size_t>(idle.clip)], 0.0f,
                         reference);
        // THE REFERENCE IS THE POSE THE ARM LAYER WILL ACTUALLY SIT ON, i.e.
        // the idle AFTER the stance layer, because that is the order the frame
        // wears them in. Calibrating on the raw clip instead left the hands
        // 4.5 cm high: straightening the knees lifts the pelvis the hand's
        // height is measured against, and the layer had aimed at the old one.
        StanceDrive calib;
        calib.stand_weight = 1.0f;
        apply_stance(skeleton, lib.stance, calib, reference);
        lib.relax = calibrate_arm_relax(rig, skeleton, binding, reference);
    }
    // СКОРОСТЬ КАЖДОЙ ПЕРЕДАЧИ НА ЭТОМ ТЕЛЕ — ЧЕРЕЗ ПУТЬ КАДРА, со всеми слоями
    // (зеркало полуциклом, стойка, руки): капсула поедет с той скоростью, с
    // какой едет НАРИСОВАННАЯ стопа, а зеркальная смесь на несимметричном
    // спринте меняет ход на ±15 % (замер: 6.29 без слоёв против 7.07 с ними).
    // Темп 1: natural_mps на время замера 0, чтобы advance_playback не гнал
    // клип за заказом.
    // НАПРАВЛЕНИЯ БЕЗ СВОЕГО КЛИПА — сказано вслух (роль играет замену).
    {
        const struct { ClipRole want; ClipRole instead; } fallbacks[] = {
            {ClipRole::Backward, ClipRole::Walk},
            {ClipRole::StrafeL, ClipRole::Walk},
            {ClipRole::StrafeR, ClipRole::Walk},
            {ClipRole::StrafeRunL, ClipRole::StrafeL},
            {ClipRole::StrafeRunR, ClipRole::StrafeR},
        };
        for (const auto& f : fallbacks) {
            if (!lib.has(f.want)) {
                std::fprintf(stderr, "[anim] direction %s: no clip — plays %s instead\n",
                             role_name(f.want).data(), role_name(f.instead).data());
            }
        }
    }
    for (ClipEntry& e : lib.role) {
        e.exit_phase.fill(-1.0f);
    }
    // КОНЕЦ ДВИЖЕНИЯ ОДНОРАЗОВЫХ КЛИПОВ (§13.1): последний момент, когда таз или
    // стопа ещё идут быстрее ONE_SHOT_STILL_MPS, плюс запас в кроссфейд.
    {
        const FootIkSetup setup = build_foot_ik(skeleton, binding, lib.contacts);
        for (const RoleNames& row : ROLE_NAMES) {
            ClipEntry& entry = lib.role[role_index(row.role)];
            if (!entry.present() || !one_shot_role(row.role) || !setup.valid()
                || entry.duration_s <= 0.0f) {
                continue;
            }
            // СМОТРИМ НА СТОПЫ, НЕ НА ТАЗ: в хвосте покоя таз дышит и качается
            // (0,03…0,08 м/с), а стопы стоят — по тазу Stop Walking «двигался»
            // все 6,0 с из 6,0.
            const int32_t watch[] = {setup.ankle[0], setup.ankle[1],
                                     setup.toe[0] >= 0 ? setup.toe[0] : setup.ankle[0],
                                     setup.toe[1] >= 0 ? setup.toe[1] : setup.ankle[1]};
            const float step_s = 1.0f / 30.0f;
            std::vector<JointLocal> a(skeleton.size());
            std::vector<JointLocal> b(skeleton.size());
            std::vector<glm::mat4> la;
            std::vector<glm::mat4> ma;
            std::vector<glm::mat4> lb;
            std::vector<glm::mat4> mb;
            float last_moving_s = 0.0f;
            for (float t = step_s; t <= entry.duration_s; t += step_s) {
                sample_clip_pose(skeleton, clips[static_cast<std::size_t>(entry.clip)], t - step_s, a);
                sample_clip_pose(skeleton, clips[static_cast<std::size_t>(entry.clip)], t, b);
                model_matrices_for(skeleton, a, la, ma);
                model_matrices_for(skeleton, b, lb, mb);
                float fastest = 0.0f;
                for (const int32_t j : watch) {
                    const glm::vec3 pa{ma[static_cast<std::size_t>(j)][3]};
                    const glm::vec3 pb{mb[static_cast<std::size_t>(j)][3]};
                    fastest = std::max(fastest, glm::length(pb - pa) / step_s);
                }
                if (fastest > static_cast<float>(config::ONE_SHOT_STILL_MPS)) {
                    last_moving_s = t;
                }
            }
            entry.active_s = std::min(entry.duration_s, last_moving_s + CLIP_CROSSFADE_S);
            // СТОПА ВЫХОДА: чья лодыжка ниже на конце движения
            sample_clip_pose(skeleton, clips[static_cast<std::size_t>(entry.clip)],
                             std::max(0.0f, entry.active_s - CLIP_CROSSFADE_S), a);
            model_matrices_for(skeleton, a, la, ma);
            const float yl = ma[static_cast<std::size_t>(setup.ankle[0])][3].y;
            const float yr = ma[static_cast<std::size_t>(setup.ankle[1])][3].y;
            entry.exit_left = yl <= yr;
            // БЛИЖАЙШАЯ ПО ПОЗЕ НОГ ФАЗА КАЖДОГО ЦИКЛА
            const int32_t legs[] = {setup.hip[0], setup.hip[1], setup.knee[0], setup.knee[1],
                                    setup.ankle[0], setup.ankle[1]};
            const int32_t pelvis_j =
                skeleton.joints[static_cast<std::size_t>(setup.hip[0])].parent;
            for (const RoleNames& cyc : ROLE_NAMES) {
                const ClipEntry& ce = lib.role[role_index(cyc.role)];
                if (!ce.present() || !locomotion_role(cyc.role) || one_shot_role(cyc.role)
                    || ce.duration_s <= 0.0f) {
                    continue;
                }
                float best = std::numeric_limits<float>::max();
                float best_phase = -1.0f;
                for (int k = 0; k < 32; ++k) {
                    const float phase = float(k) / 32.0f;
                    // время цикла по фазе — та же формула, что locomotion_time()
                    sample_clip_pose(skeleton, clips[static_cast<std::size_t>(ce.clip)],
                                     wrap01(phase - PHASE_LEFT + ce.footfall_phase)
                                         * ce.duration_s,
                                     b);
                    float dist = 0.0f;
                    const auto ang = [&](int32_t j) {
                        const glm::quat qa = glm::normalize(a[static_cast<std::size_t>(j)].rotation);
                        const glm::quat qb = glm::normalize(b[static_cast<std::size_t>(j)].rotation);
                        return 2.0f * std::acos(std::clamp(std::abs(glm::dot(qa, qb)), 0.0f, 1.0f));
                    };
                    for (const int32_t j : legs) {
                        dist += ang(j);
                    }
                    if (pelvis_j >= 0) {
                        dist += ang(pelvis_j);
                    }
                    if (dist < best) {
                        best = dist;
                        best_phase = phase;
                    }
                }
                entry.exit_phase[role_index(cyc.role)] = best_phase;
            }
            std::fprintf(stderr, "[clips] one-shot %.*s: движение до %.2f с из %.2f, выход на %s\n",
                         static_cast<int>(row.name.size()), row.name.data(),
                         static_cast<double>(entry.active_s),
                         static_cast<double>(entry.duration_s),
                         entry.exit_left ? "левой" : "правой");
        }
    }
    // КРИВЫЕ ПУТИ — ПОСЛЕ ВСЕГО (§11.1): зеркало полцикла (mirror_dose) и
    // симметрия ставятся ниже передач, а кривая обязана мерить ТУ позу, что
    // играет; мерянная до зеркала, она расходилась со стопой на левой опоре
    // на 3 мм/тик (урок «первый замер верный, второй нет» — снова).
    return lib;
}

ClipRole role_for_drive(const ClipLibrary& lib, const BodyDrive& drive) {
    MoveDir dir = move_dir_class(drive.move_dir_model, MoveDir::Forward);
    return role_for_drive(lib, drive, dir);
}

ClipRole role_for_drive(const ClipLibrary& lib, const BodyDrive& drive, MoveDir& move_dir) {
    const bool moving = drive_speed(lib, drive) > MOVING_SPEED_MPS;
    if (moving) {
        move_dir = move_dir_class(drive.move_dir_model, move_dir);
    }
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
        // РОЛЬ ПО НАПРАВЛЕНИЮ (§9.2): назад — Backward, бок — Strafe (бегом —
        // StrafeRun), иначе передача вперёд. Нет клипа — честный откат: бегом
        // вбок → шагом вбок в верхней полосе темпа; назад/вбок без клипа →
        // передача вперёд (тело едет за вводом, как прежде).
        const ClipRole fwd = role_for_gait(drive.gait);
        const bool run = drive.gait != Gait::Walk;
        ClipRole want = fwd;
        switch (move_dir) {
        case MoveDir::Backward: want = ClipRole::Backward; break;
        case MoveDir::StrafeL:
            want = run && lib.has(ClipRole::StrafeRunL) ? ClipRole::StrafeRunL : ClipRole::StrafeL;
            break;
        case MoveDir::StrafeR:
            want = run && lib.has(ClipRole::StrafeRunR) ? ClipRole::StrafeRunR : ClipRole::StrafeR;
            break;
        case MoveDir::Forward: break;
        }
        if (lib.has(want)) {
            return want;
        }
        if (lib.has(fwd)) {
            return fwd;
        }
    }
    return ClipRole::Idle;
}

namespace {

[[nodiscard]] bool locomotion(ClipRole r) {
    return r == ClipRole::Walk || r == ClipRole::Jog || r == ClipRole::Sprint
           || r == ClipRole::CrouchWalk || r == ClipRole::Backward
           || r == ClipRole::StrafeL || r == ClipRole::StrafeR
           || r == ClipRole::StrafeRunL || r == ClipRole::StrafeRunR;
}
[[nodiscard]] bool one_shot(ClipRole r) {
    return one_shot_role(r);
}

/// Where in a locomotion clip sim's stride phase puts us. The whole footfall
/// seam is this one line: the clip is shifted so its own plant coincides with
/// the phase sim fires its event at.
} // namespace

namespace {

} // namespace

void advance_playback(const ClipLibrary& lib, const BodyDrive& drive, float dt,
                      ClipPlayback& play, const LocoMachine* machine) {
    // THE PREVIOUS TICK, snapshotted before anything moves. Render reads it
    // with an alpha exactly as it reads PreviousTransform (Rule 12); a pose
    // that only exists for the current tick is the reason a 30 Hz sim looked
    // like a 30 Hz body while everything around it was smooth.
    play.prev_time_s = play.time_s;
    play.prev_previous_time_s = play.previous_time_s;
    play.prev_fade = play.fade;
    play.prev_weapon = play.weapon;
    play.prev_weapon_time_s = play.weapon_time_s;
    play.prev_stance_run = play.stance_run;
    play.prev_stance_stand = play.stance_stand;
    play.prev_airborne = play.airborne;

    // THE STANCE LAYER'S TWO WEIGHTS. The run weight is sim's own eased gear
    // blend, ferried on the drive — re-deriving it from the speed here would
    // be the second description of one thing this zone has already paid for
    // twice. The standing weight is eased over the SAME time a role change
    // takes, so the knees straighten while the walk clip is fading out and not
    // a frame after it.
    play.prev_phase = play.phase;
    play.switched = false;
    play.prev_look_yaw = play.look_yaw;
    {
        // ВЗГЛЯД ЗА КАМЕРОЙ: цель — разница «взгляд − корпус» в пределах
        // LOOK_MAX_DEG (тела без взгляда — ноль), догоняется за LOOK_SMOOTH_S.
        float want = 0.0f;
        if (drive.view_valid) {
            const float limit = glm::radians(static_cast<float>(config::LOOK_MAX_DEG));
            want = std::clamp(wrap_pi(drive.view_yaw - drive.facing_yaw), -limit, limit);
        }
        const float tau = static_cast<float>(config::LOOK_SMOOTH_S);
        const float k = (tau > 0.0f && dt > 0.0f) ? 1.0f - std::exp(-dt / tau) : 1.0f;
        play.look_yaw += (want - play.look_yaw) * k;
    }
    play.prev_lean = play.lean;
    {
        // НАКЛОН ПО ТОЛЧКУ (ярус 0): угол PUSH_LEAN_DEG_PER_MPS на м/с толчка,
        // не больше PUSH_LEAN_MAX_DEG, по направлению толчка; догон и возврат
        // за PUSH_LEAN_SMOOTH_S. Тела без толчка — ноль побитово.
        glm::vec3 want{0.0f};
        const glm::vec2 p{drive.push_mps_model.x, drive.push_mps_model.z};
        const float mag = glm::length(p);
        if (mag > 1.0e-4f) {
            const float deg = std::min(static_cast<float>(config::PUSH_LEAN_MAX_DEG),
                                       mag * static_cast<float>(config::PUSH_LEAN_DEG_PER_MPS));
            const glm::vec2 dir = p / mag * glm::radians(deg);
            want = glm::vec3{dir.x, 0.0f, dir.y};
        }
        const float tau = static_cast<float>(config::PUSH_LEAN_SMOOTH_S);
        const float k = (tau > 0.0f && dt > 0.0f) ? 1.0f - std::exp(-dt / tau) : 1.0f;
        play.lean += (want - play.lean) * k;
        if (glm::dot(want, want) == 0.0f && glm::dot(play.lean, play.lean) < 1.0e-8f) {
            play.lean = glm::vec3{0.0f};
        }
    }
    play.prev_transit_dose = play.transit_dose;
    {
        // ДОЗА ПЕРЕХОДА: 1, пока играет одноразовый клип перехода, и обратно к
        // нулю за то же время, что кроссфейд ролей.
        // …и слой стойки возвращается за ТО ЖЕ время, что кроссфейд из перехода
        // (fade_s): за 0,1 с его поправки колена давали рывок 2 700 рад/с² на
        // стыке старт → цикл, хотя фаза была подобрана по позе (замер 07.09).
        const float want = transit_role(play.role) ? 1.0f : 0.0f;
        const float span = play.fade_s > 0.0f ? play.fade_s : CLIP_CROSSFADE_S;
        const float move = span > 0.0f ? dt / span : 1.0f;
        play.transit_dose = play.transit_dose < want
                                ? std::min(want, play.transit_dose + move)
                                : std::max(want, play.transit_dose - move);
    }
    play.stance_run = std::clamp(drive.run_weight, 0.0f, 1.0f);
    {
        const float want = drive.speed_mps > MOVING_SPEED_MPS ? 0.0f : 1.0f;
        const float move = CLIP_CROSSFADE_S > 0.0f ? dt / CLIP_CROSSFADE_S : 1.0f;
        play.stance_stand = play.stance_stand < want
                                ? std::min(want, play.stance_stand + move)
                                : std::max(want, play.stance_stand - move);
    }

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

    ClipRole want = machine != nullptr ? machine->role : role_for_drive(lib, drive, play.move_dir);
    if (machine != nullptr) {
        play.move_dir = machine->dir;
    }
    // ПЕРЕХОДЫ (старт, остановка, поворот, толчок) ЖИВУТ В МАШИНЕ (§16.3):
    // без неё (DFN_ROOT_TRACK=0, воздух, поза) роль — от ввода, покой ↔ цикл.
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
    {
        const bool in_air = want == ClipRole::JumpStart || want == ClipRole::JumpLoop
                            || want == ClipRole::JumpLand;
        const float target = in_air ? 1.0f : 0.0f;
        const float move = CLIP_CROSSFADE_S > 0.0f && dt > 0.0f ? dt / CLIP_CROSSFADE_S
                                                                : 1.0f;
        play.airborne = play.airborne < target ? std::min(target, play.airborne + move)
                                               : std::max(target, play.airborne - move);
    }
    // ВАРИАНТЫ ПОКОЯ: по очереди раз в IDLE_VARIANT_S, пьяный — по флагу.
    int32_t want_variant = -1;
    if (want == ClipRole::Idle && !lib.idle_variants.empty()) {
        const int32_t n_sober = static_cast<int32_t>(lib.idle_variants.size())
                                - (lib.drunk_variant >= 0 ? 1 : 0);
        if (drive.drunk && lib.drunk_variant >= 0) {
            want_variant = lib.drunk_variant;
            play.idle_s = 0.0f;
        } else if (play.role != ClipRole::Idle || play.variant == lib.drunk_variant) {
            want_variant = -1;
            play.idle_s = 0.0f;
        } else {
            play.idle_s += dt;
            want_variant = play.variant;
            if (play.idle_s >= static_cast<float>(config::IDLE_VARIANT_S) && n_sober > 0) {
                play.idle_s = 0.0f;
                ++play.variant_pick;
                const uint32_t k = (play.variant_pick * 7u + 3u) % static_cast<uint32_t>(n_sober + 1);
                int32_t pick = k == 0u ? -1 : static_cast<int32_t>(k) - 1;
                if (pick == lib.drunk_variant) {
                    pick = -1;
                }
                want_variant = pick;
            }
        }
    } else {
        play.idle_s = 0.0f;
    }
    const bool variant_switch = want == ClipRole::Idle && play.role == ClipRole::Idle
                                && want_variant != play.variant;
    if (want != play.role || variant_switch) {
        play.previous = play.role;
        play.previous_variant = play.variant;
        play.previous_time_s = play.time_s;
        play.fade = lib.inertial ? 0.0f : 1.0f;
        play.switched = true;
        play.fade_s = transit_role(play.role) && locomotion(want)
                          ? static_cast<float>(config::TRANSIT_CROSSFADE_S)
                          : CLIP_CROSSFADE_S;
        // ВХОД В ЦИКЛ СО СТЫКА: фаза цикла, ближайшая по позе ног к выходу
        // одноразового клипа (exit_phase), иначе по стопе на выходе.
        if (transit_role(play.previous) && locomotion(want)) {
            const ClipEntry& from = entry_for(lib, play.previous, play.previous_variant);
            const float matched = from.exit_phase[role_index(want)];
            play.phase = matched >= 0.0f ? matched
                                         : (from.exit_left ? PHASE_LEFT : wrap01(PHASE_LEFT + 0.5f));
        }
        play.role = want;
        play.variant = want_variant;
        play.time_s = 0.0f;
    }
    if (play.fade > 0.0f && dt > 0.0f) {
        const float fade_s = play.fade_s > 0.0f ? play.fade_s : CLIP_CROSSFADE_S;
        play.fade = std::max(0.0f, play.fade - dt / fade_s);
    }
    const ClipEntry& cur = entry_for(lib, play.role, play.variant);
    if (machine != nullptr && cur.duration_s > 0.0f) {
        // ОДИН КЛОК — МАШИНА (§16.3): время клипа = её фаза × длительность;
        // фаза шага сим'а — та же фаза через шов опоры левой стопы.
        play.rate = machine->rate;
        play.time_s = std::clamp(machine->phase, 0.0f, 1.0f) * cur.duration_s;
        if (locomotion(play.role)) {
            play.phase = wrap01(machine->phase + PHASE_LEFT - cur.footfall_phase);
        }
    } else if (cur.duration_s > 0.0f) {
        // БЕЗ МАШИНЫ (DFN_ROOT_TRACK=0 — контрольная рука «капсула от модели
        // скорости ввода»; воздух; поза): часы по времени, темп цикла — заказ
        // к скорости дорожки в полосе, одноразовый клип — до конца.
        if (locomotion(play.role)) {
            const float want_mps = std::max(0.0f, drive.want_speed_mps);
            play.rate = (cur.root.valid && cur.root.mps > 1.0e-3f && want_mps > 0.0f)
                            ? std::clamp(want_mps / cur.root.mps, 1.0f - TEMPO_BAND, 1.0f + TEMPO_BAND)
                            : 1.0f;
            play.phase = wrap01(play.phase + dt * play.rate / cur.duration_s);
            play.time_s = wrap01(play.phase - PHASE_LEFT + cur.footfall_phase) * cur.duration_s;
        } else {
            play.rate = 1.0f;
            play.time_s += dt;
            play.time_s = one_shot(play.role) ? std::min(play.time_s, cur.duration_s)
                                              : wrap01(play.time_s / cur.duration_s)
                                                    * cur.duration_s;
        }
    }
    if (play.fade > 0.0f) {
        const ClipEntry& prev = entry_for(lib, play.previous, play.previous_variant);
        if (locomotion(play.previous) && prev.duration_s > 0.0f) {
            play.previous_time_s =
                wrap01(play.phase - PHASE_LEFT + prev.footfall_phase) * prev.duration_s;
        } else if (prev.duration_s > 0.0f) {
            play.previous_time_s += dt;
            play.previous_time_s =
                one_shot(play.previous)
                    ? std::min(play.previous_time_s, prev.duration_s)
                    : wrap01(play.previous_time_s / prev.duration_s) * prev.duration_s;
        }
    }
    if (!play.primed) {
        play.prev_phase = play.phase;
        play.prev_time_s = play.time_s;
        play.prev_previous_time_s = play.previous_time_s;
        play.prev_fade = play.fade;
        play.prev_weapon = play.weapon;
        play.prev_weapon_time_s = play.weapon_time_s;
        play.prev_stance_run = play.stance_run;
        play.prev_stance_stand = play.stance_stand;
        play.prev_airborne = play.airborne;
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
                float prev_t, float t, float alpha, std::span<JointLocal> out,
                const MirrorMap* mirror = nullptr) {
    (void)binding;
    const float d = forward_delta(prev_t, t, entry.duration_s);
    float when = prev_t + alpha * d;
    if (entry.duration_s > 0.0f) {
        when = wrap01(when / entry.duration_s) * entry.duration_s;
    }
    sample_clip_pose(skeleton, clips[static_cast<std::size_t>(entry.clip)], when, out);
    if (entry.mirrored && mirror != nullptr && mirror->valid()) {
        std::vector<JointLocal> raw(out.begin(), out.begin() + skeleton.size());
        mirror_pose(skeleton, *mirror, raw, out.first(skeleton.size()));
    }
}

} // namespace

bool playback_sample(const skel::Skeleton& skeleton, const SkinnedRigBinding& binding,
                     std::span<const skel::AnimClip> clips, const ClipLibrary& lib,
                     const ClipPlayback& play, float alpha,
                     std::span<JointLocal> out_sample) {
    const float a = std::clamp(alpha, 0.0f, 1.0f);
    const std::size_t n = skeleton.size();
    const ClipEntry& cur = entry_for(lib, play.role, play.variant);
    if (!cur.present() || out_sample.size() < n) {
        return false;
    }
    role_frame(skeleton, binding, clips, cur, play.prev_time_s, play.time_s, a, out_sample,
               &lib.mirror);
    // СИММЕТРИЗАЦИЯ — СРАЗУ ЗА СЭМПЛОМ РОЛИ И ДО КРОССФЕЙДА. Предмет слоя —
    // ЦИКЛ, а кроссфейд смешивает два разных цикла: симметризовать смесь
    // значило бы искать полуцикл у позы, у которой его нет. И до слоёв стойки
    // и рук по той же причине, по которой они идут после клипа вообще: они
    // правят то, что цикл уже сказал.
    // ЗЕРКАЛО ПОЛЦИКЛА — ТОЛЬКО СИММЕТРИЧНЫМ ХОДАМ: стрейф, отражённый L↔R,
    // становится стрейфом в другую сторону, и смесь гасит боковой ход в ноль
    // (StrafeL мерился 0,27 м/с при 1,72 у клипа, стопы ходили только по Z).
    const bool sideways = play.role == ClipRole::StrafeL || play.role == ClipRole::StrafeR
                          || play.role == ClipRole::StrafeRunL || play.role == ClipRole::StrafeRunR;
    if (locomotion(play.role) && !sideways && lib.mirror_dose > 0.0f && lib.mirror.valid()
        && cur.duration_s > 0.0f) {
        const float d = cur.duration_s;
        std::vector<JointLocal> half(n);
        // ПАРТНЁР СМЕСИ ТОЖЕ НА ПОЛЦИКЛА ПОЗЖЕ: прежде его время не сдвигалось, и
        // «поза полуциклом позже» была смесью правильного ведущего кадра с
        // партнёром из ДРУГОЙ фазы — зеркало усредняло позу с чужой и съедало
        // треть хода стопы (замер: ходьба 1.34 → 0.91 м/с от одного слоя).
        role_frame(skeleton, binding, clips, cur,
                   std::fmod(play.prev_time_s + 0.5f * d, d),
                   std::fmod(play.time_s + 0.5f * d, d), a, half, &lib.mirror);
        mirror_blend(skeleton, lib.mirror, half, lib.mirror_dose, out_sample);
    }
    // СИММЕТРИЯ ПОКОЯ (заказ владельца 02.09: «стоя ноги ровно, без выноса
    // левой ноги»): поза Idle смешивается со своим зеркалом на ТОЙ ЖЕ фазе —
    // тот же прибор, что симметризует походку полуциклом, но без сдвига.
    // ТОЛЬКО НОГИ (ветвь Lower): заказ про стойку ног; таз ровняет слой
    // стойки (он держит рыск линии бёдер), а верх покоя — дыхание, поворот
    // головы, руки — живёт своей асимметрией, и его симметризация двигала бы
    // хват меча (замер: наклон клинка 25,2° → 23,0° при симметрии таза).
    const auto symmetrise_idle = [&](ClipRole role, std::span<JointLocal> pose) {
        if (role != ClipRole::Idle || lib.idle_symmetry <= 0.0f || !lib.mirror.valid()
            || !lib.mask.valid()) {
            return;
        }
        std::vector<JointLocal> sym(pose.begin(), pose.begin() + n);
        mirror_blend(skeleton, lib.mirror, sym, lib.idle_symmetry, sym);
        for (std::size_t j = 0; j < n; ++j) {
            if (lib.mask.at(j) == Branch::Lower) {
                pose[j] = sym[j];
            }
        }
    };
    symmetrise_idle(play.role, out_sample.first(n));
    const float fade = glm::mix(play.prev_fade, play.fade, a);
    const ClipEntry& prev = entry_for(lib, play.previous, play.previous_variant);
    if (fade > 0.0f && prev.present()) {
    // THE CROSS-FADE RUNS ON TWO FINISHED SAMPLES, not on one sample built
    // from a blended time: the two roles carry DIFFERENT STRIDE SCALES and
    // different durations, and a single interpolated clip time between them
    // means nothing at all.
        std::vector<JointLocal> other(n);
        role_frame(skeleton, binding, clips, prev, play.prev_previous_time_s,
                   play.previous_time_s, a, other, &lib.mirror);
        symmetrise_idle(play.previous, other);
        blend_local(out_sample.first(n), other, fade, out_sample.first(n));
    }
    // THE THREE LAYERS, in the order a body wears them: the weapon guard is
    // put ON the upper half, then the arm layer brings the shoulders and the
    // elbows to our proportions, then the stance layer answers for everything
    // the clip's author decided about standing.
    const float weapon = glm::mix(play.prev_weapon, play.weapon, a);
    const ClipEntry& guard = lib[ClipRole::WeaponIdle];
    if (weapon > 0.0f && guard.present() && lib.mask.valid()) {
        std::vector<JointLocal> upper(n);
        role_frame(skeleton, binding, clips, guard, play.prev_weapon_time_s,
                   play.weapon_time_s, a, upper);
        blend_masked(out_sample.first(n), upper, lib.mask, weapon, out_sample.first(n));
    }
    const float sheathed = 1.0f - weapon;
    const float run = glm::mix(play.prev_stance_run, play.stance_run, a);
    const float stand = glm::mix(play.prev_stance_stand, play.stance_stand, a);

    // THE STANCE LAYER FIRST AND THE ARM LAYER SECOND, and the order is a
    // measurement rather than a preference: the stance straightens the knees,
    // which lifts the PELVIS the arm layer's hand height is measured against.
    // Run the other way round and the hands aim at a pelvis that is about to
    // move — measured, 4.5 cm of it.
    StanceDrive stance;
    stance.run_weight = run;
    stance.stand_weight = stand;
    // И ВЕС ВСЕГО СЛОЯ — «НАСКОЛЬКО МЫ НА ЗЕМЛЕ». В воздухе поза обязана быть
    // чистым клипом: слой целится в стойку СТОЯЩЕГО человека, а у летящего
    // ни стойки, ни опоры нет.
    stance.weight = (1.0f - glm::mix(play.prev_airborne, play.airborne, a))
                    * (1.0f - glm::mix(play.prev_transit_dose, play.transit_dose, a));
    // THE GAINS, from what THIS clip swings against what the reference does.
    // Clamped at 1 below: the layer is here to add the swing the retarget ate,
    // never to take swing away from a clip that already has enough.
    const auto gain_for = [](float peak, float want, float dose) {
        const float g = peak > 0.02f ? std::clamp(want / peak, 1.0f, STANCE_GAIN_MAX)
                                     : 1.0f;
        return 1.0f + (g - 1.0f) * std::clamp(dose, 0.0f, 1.0f);
    };
    // ...AND THE ARM SWING IS DOSED OFF WHEN THERE IS NO WALK TO SWING WITH.
    // A gain multiplies whatever the clip has, and an IDLE has a small
    // asymmetric arm offset rather than a swing: tripling it turned a standing
    // man's arms 22.6 degrees apart. Same for a drawn blade, where the guard
    // clip is what the arms are supposed to be doing.
    stance.arm_swing_gain =
        gain_for(cur.arm_swing_peak_rad, static_cast<float>(config::STANCE_ARM_SWING),
                 (1.0f - stand) * sheathed);
    stance.twist_gain =
        gain_for(cur.twist_peak_rad, static_cast<float>(config::STANCE_TWIST_RUN), 1.0f);
    apply_stance(skeleton, lib.stance, stance, out_sample);
    // СЛОЙ ВЗГЛЯДА — после стойки (она уже поставила грудь), до рук.
    apply_look(skeleton, lib.look, glm::mix(play.prev_look_yaw, play.look_yaw, a), 1.0f,
               out_sample.first(n));
    // НАКЛОН ПО ТОЛЧКУ — той же цепочкой позвонков, вокруг горизонтали.
    {
        const glm::vec3 lean = glm::mix(play.prev_lean, play.lean, a);
        const float ang = glm::length(lean);
        if (ang > 1.0e-5f) {
            // наклон ВДОЛЬ толчка: ось — горизонталь, перпендикулярная ему
            const glm::vec3 axis = glm::normalize(glm::cross(glm::vec3{0.0f, 1.0f, 0.0f}, lean / ang));
            apply_chain_rotation(skeleton, lib.look, axis, ang, 1.0f, out_sample.first(n));
        }
    }

    // THE ARM LAYER DOES NOT SWITCH OFF WHEN THE SWORD COMES OUT, and that
    // single `1.0f - weapon` was the wave's worst line. Measured: drawn, the
    // hands stood 0.529 m from the pelvis centre against 0.256 m sheathed —
    // drawing a blade threw the arms twice as wide, because the guard clip is
    // drawn for its author's proportions and the calibration that fits it to
    // OURS was exactly what got removed. The guard keeps its shoulder ANGLES;
    // it does not get to keep its shoulder WIDTH.
    //
    // THE FINGERS ARE THE HALF THAT DOES SWITCH OFF, and they are dosed apart
    // for that reason: the hand holding the blade must stay in the clip's
    // keyed fist. Empty, it opens STANCE_FINGER_RELAX of the way back to the
    // bind — a soft half-fist, not the splayed fan a full relax gave.
    ArmRelaxDose dose;
    dose.arm = std::max(sheathed,
                        weapon * static_cast<float>(config::STANCE_WEAPON_ARM_RELAX));
    // THE ELBOW OFFSET IS THIS CLIP'S, and the target is the gear's: standing
    // and walking it is OUR REST POSE'S elbow (the neutral, REST_ELBOW_FLEX
    // through the retarget — owner's order 02.09), at a run the reference's
    // 80-90, and the clip playing may already be anywhere between.
    // Subtracting what the clip holds is what keeps the correction a
    // correction.
    // ЛОКТИ — КАК В КЛИПЕ (владелец 02.09-2: «локти должны всегда сгибаться;
    // не изобретать, переиспользовать анимации»): стоя и на ходьбе рука
    // держит сгиб клипа (Walk_Loop 36°, Idle 31°), а не покой экрана
    // создания (10°) — прежняя тяга к покою и давала «прямые руки на
    // ходьбе». К беговому сгибу STANCE_ELBOW_RUN рука ведётся только весом
    // бега.
    const float want_elbow =
        glm::mix(cur.elbow_mean_rad, static_cast<float>(config::STANCE_ELBOW_RUN), run);
    // ...AND WHEN THE SWORD IS OUT THE GUARD DECIDES THE ELBOW. A blade is
    // held with a bent arm and the guard clip says how bent; the layer's job
    // there is the shoulder, not the fold.
    dose.elbow_offset_rad =
        cur.elbow_mean_rad > 0.0f ? (want_elbow - cur.elbow_mean_rad) * sheathed : 0.0f;
    dose.finger = sheathed * static_cast<float>(config::STANCE_FINGER_RELAX);
    apply_arm_relax(skeleton, lib.relax, dose, out_sample);
    // ОБХОД ТЕЛА — ПОСЛЕДНИМ СЛОЕМ РУКИ, и порядок здесь не вкус: и стойка, и
    // приведение ДВИГАЮТ кисть, а обход отвечает на вопрос о том, где кисть
    // ОКАЗАЛАСЬ. Поставленный раньше, он мерил бы клиренс позы, которую
    // следующий слой сейчас поменяет.
    //
    // И ОН НЕ ДОЗИРУЕТСЯ ОРУЖИЕМ. Рука с клинком проходит сквозь бедро ровно
    // так же, как пустая, а guard-клип уводит её ещё дальше назад: доза здесь
    // — единица всегда, и единственное, что её снимает, это дверь контроля.
    apply_arm_clearance(skeleton, lib.arms, lib.boxes, binding, lib.arm_clearance_m,
                        1.0f, out_sample);
    return true;
}

} // namespace dfn::anim
