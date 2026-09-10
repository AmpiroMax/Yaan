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
- ЧАСЫ ЛОКОМОЦИИ ТЕПЕРЬ ЗДЕСЬ (решение синка 02.09, docs/design/
  LOCOMOTION_GROUNDED.md): при `ClipLibrary::feet_drive` фаза
  `ClipPlayback::phase` растёт своим темпом (заказ передачи / скорость клипа,
  в полосе LOCOMOTION_TEMPO_BAND), а корень едет от опорной стопы (Locomotion.h),
  так что событие шага, боб камеры и нарисованная постановка — один и тот же
  миг по построению, а не по совпадению двух часов. Прежний шов (фаза сима,
  стрид-скейл) остаётся контрольной рукой (`feet_drive = false`,
  DFN_ROOT_FROM_FEET=0) и описан ниже как был.
- THE PHASE WAS SIM'S CLOCK in that seam (Clips.h: "never advance a phase
  here"): a locomotion clip's time is a pure function of `stride_phase` —
  the [0,1) sim advances by displacement — shifted so the clip's own left
  footfall lands exactly on FOOTFALL_PHASE_LEFT. Non-locomotion clips (the
  jump triple, the interactions) run off their own seconds either way,
  because a jump is an event with a duration and not a cycle.
- HOW SPEED REACHED THE FEET IN THE OLD SEAM, and it was NOT playback rate
  (with `feet_drive` it is the feet that set the speed, and the tempo band
  is all the rate there is). The rate is sim's:
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
#include "engine/anim/sources/LookLayer.h"
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
    /// РОЛИ НАПРАВЛЕНИЯ (LOCOMOTION_GROUNDED.md §9): ход назад и стрейфы,
    /// шагом и бегом. Выбираются по углу между BodyDrive::move_dir_model и
    /// лицом; нет клипа — честный откат (StrafeRun → Strafe, Backward →
    /// вперёд) с паспортом в журнале сборки библиотеки.
    Backward,
    StrafeL,
    StrafeR,
    StrafeRunL,
    StrafeRunR,
    /// ПЕРЕХОДЫ (заказ владельца 04.09: «правильные анимации старта ходьбы и
    /// бега, соответствующие анимации остановки, нормальные повороты»).
    /// Одноразовые: играются целиком и сдают роль дальше — цикл после старта,
    /// покой после остановки, покой после поворота. Их выбирает не угол и не
    /// передача, а СОБЫТИЕ: ввод появился, ввод пропал, камера ушла от корпуса.
    StartWalk,
    StartRun,
    StopWalk,
    StopRun,
    TurnL,
    TurnR,
    /// УДАР/ТОЛЧОК (HIT_REACTIONS §2, ярус 0 → 1): одноразовый клип реакции на
    /// сильный толчок капсулы (STAGGER_PUSH_MPS); играет на месте.
    Stagger,
    /// РАЗВОРОТ НА МЕСТЕ на ~180° (§16): клип и его зеркало; какой из них
    /// левый, решает ЗАМЕРЕННЫЙ знак рыска дорожки (build_clip_library
    /// меняет местами, если имя не совпало со знаком).
    Turn180L,
    Turn180R,
};
inline constexpr uint32_t CLIP_ROLE_COUNT = 25;

/// Класс направления хода по углу к лицу (§9.2), с гистерезисом от прежнего.
enum class MoveDir : uint8_t { Forward = 0, StrafeL, StrafeR, Backward };
[[nodiscard]] MoveDir move_dir_class(const glm::vec3& move_dir_model, MoveDir previous);
/// Ось, по которой стопы уходят под телом при этом ходе (для корня от стопы).
[[nodiscard]] glm::vec3 travel_axis(const glm::vec3& move_dir_model);
/// Направление хода, которое несёт клип роли (стрейф — ±X, назад — +Z,
/// всё остальное — вперёд): корень от стопы меряется по оси КЛИПА, а
/// диагональный ввод внутри класса «вперёд» доворачивает сим, не стопы.
[[nodiscard]] glm::vec3 role_move_dir(ClipRole role);
/// Роль, которая ВЕЗЁТ ТЕЛО: циклы хода плюс переходы (старт, остановка,
/// поворот). Заявку корня от стопы зона app выдаёт ровно на эти роли —
/// клип старта разгоняет тело своими ногами, клип поворота своими ногами
/// поворачивает.
[[nodiscard]] bool locomotion_role(ClipRole role);
/// Одноразовая роль: играется от нуля до конца и не заворачивается (прыжок,
/// старт, остановка, поворот на месте).
[[nodiscard]] bool one_shot_role(ClipRole role);
/// Роль ПЕРЕХОДА: старт, остановка, поворот на месте (прыжок сюда не входит —
/// он про полёт, а не про переступ). У этих клипов рыск таза вынимается и
/// отдаётся телу (§13.3), а слой стойки на них снимается (§13.4).
[[nodiscard]] bool transit_role(ClipRole role);
/// Что сейчас идёт вместо цикла: старт, остановка, поворот (или ничего).

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

struct ClipEntry;
/// Корень (суставы без родителя) — в позу кадра 0 клипа: поза «на месте».
void neutralize_root(const skel::Skeleton& skeleton, const skel::AnimClip& clip,
                     std::span<JointLocal> out);
/// ДОРОЖКА КОРНЯ ИЗ КАНАЛОВ СУСТАВА 0 клипа (см. RootTrack): ресемпл по фазе,
/// ход от кадра 0, рыск — закрутка вокруг вертикали относительно кадра 0.
void measure_root_track(const skel::Skeleton& skeleton, const skel::AnimClip& clip,
                        bool mirrored, ClipEntry& entry);
/// РАСПИСАНИЕ КОНТАКТОВ (ClipEntry::plant_phase/lift_phase) по позе «на месте»
/// плюс дорожка корня; `cyclic` — петля (постановка через стык — одна).
void measure_contact_schedule(const skel::Skeleton& skeleton, const SkinnedRigBinding& binding,
                              const ContactSet& contacts, std::span<const skel::AnimClip> clips,
                              bool cyclic, ClipEntry& entry);

/// ДОРОЖКА КОРНЯ КЛИПА (LOCOMOTION_GROUNDED.md §16, 10.09): что авторский
/// сустав root (сустав без родителя) делает за клип — ход по горизонтали и
/// рыск, — ресемплировано равномерно по фазе, накопительно от фазы 0.
/// Единственный источник перемещения тела: капсула едет на root_delta между
/// двумя фазами, поза рисуется «на месте» (корень в позе кадра 0). Меряется
/// при загрузке из каналов сустава 0 (measure_root_track); зеркальному клипу
/// зеркалится (x → −x, рыск → −рыск).
inline constexpr uint32_t ROOT_TRACK_POINTS = 128;
/// Постановок стопы за клип, сколько помним на сторону (шаг — 1, старт/поворот
/// — до 3); лишние — самые короткие — отбрасываются вслух.
inline constexpr uint32_t MAX_PLANTS_PER_SIDE = 4;

struct RootTrack {
    std::array<glm::vec2, ROOT_TRACK_POINTS> xz{}; ///< модель, м, от фазы 0
    std::array<float, ROOT_TRACK_POINTS> yaw{};    ///< рад, знак сим'а (+ по часовой), развёрнуто
    float total_m = 0.0f;     ///< длина пути по дорожке за клип
    float total_yaw = 0.0f;   ///< рыск за клип, рад, со знаком
    float mps = 0.0f;         ///< total_m / duration — скорость клипа
    bool valid = false;       ///< сустав 0 ведёт клип (ход ≥ 5 см или рыск ≥ 5°)
    /// Поза корня на кадре 0 клипа: в неё сбрасывается корень при выборке
    /// (sample_clip_pose), чтобы поза была «на месте»; у клипов без дорожки
    /// это и есть их постоянный корень — кадр бит-в-бит как до дорожки.
    glm::vec3 pose0_t{0.0f};
    glm::quat pose0_r{1.0f, 0.0f, 0.0f, 0.0f};
};

/// Ход корня между двумя фазами клипа с учётом петли (wrap); рыск умножен на
/// yaw_warp (motion warping поворотов: 90° клип на 45° — warp 0,5).
struct RootDelta {
    glm::vec2 xz{0.0f};
    float yaw = 0.0f;
};
[[nodiscard]] RootDelta root_track_delta(const RootTrack& track, float from_phase,
                                         float to_phase, bool cyclic, float yaw_warp = 1.0f);
/// Значение дорожки в фазе (интерполяция между точками; вне [0,1) — по петле).
[[nodiscard]] glm::vec2 root_track_xz_at(const RootTrack& track, float phase);
[[nodiscard]] float root_track_yaw_at(const RootTrack& track, float phase);

/// What load-time measurement found out about one clip.
struct ClipEntry {
    int32_t clip = -1;         ///< index into the model's clip list, -1 = absent
    RootTrack root;            ///< дорожка корня (§16)
    /// РАСПИСАНИЕ КОНТАКТОВ (§16): фазы постановки и отрыва каждой стопы,
    /// измеренные при загрузке по этому телу: точка контакта в полосе
    /// GRIP_TOLERANCE_M над своим покоем И её мировая скорость (локальная +
    /// дорожка корня) ниже CONTACT_STILL_MPS. Постановка — событие клипа, а не
    /// порог тика.
    std::array<std::array<float, MAX_PLANTS_PER_SIDE>, 2> plant_phase{};
    std::array<std::array<float, MAX_PLANTS_PER_SIDE>, 2> lift_phase{};
    std::array<uint8_t, 2> plant_count{};
    /// У КЛИПА СТАРТА: фаза, с которой его дорожка идёт со скоростью цикла
    /// (≥ START_HANDOFF_FRAC × mps цикла) — цикл забирает ход отсюда. −1 — не
    /// старт или не разгоняется до цикла.
    float handoff_phase = -1.0f;
    /// ГДЕ У ОДНОРАЗОВОГО КЛИПА КОНЧАЕТСЯ ДВИЖЕНИЕ ПО ДОРОЖКЕ (§16): последняя
    /// фаза, где корень ещё едет (≥ CONTACT_STILL_MPS/3) или крутится
    /// (≥ 10°/с). Машина кончает остановку и поворот здесь. 0 — не мерялось.
    float settle_phase = 0.0f;
    /// КЛИП ВЗЯТ ЗЕРКАЛОМ (§13.6): в таблице ролей имя с приставкой «~».
    /// Зеркалится вся поза (mirror_pose) сразу после выборки, до слоёв;
    /// дорожка корня зеркалится при загрузке (x → −x, рыск → −рыск).
    bool mirrored = false;
    float duration_s = 0.0f;
    /// ФАЗА ОПОРЫ ЛЕВОЙ СТОПЫ — середина её окна опоры по расписанию (§16):
    /// шов фазы шага (PHASE_LEFT) и вход в цикл со стыка клипов идут от неё.
    /// Ноль у клипа без постановок.
    float footfall_phase = 0.0f;
    /// ОДНОРАЗОВЫЙ КЛИП: до какой секунды в нём есть движение (последний
    /// кадр, где какая-то точка стопы быстрее ONE_SHOT_STILL_MPS, плюс
    /// кроссфейд); какая стопа внизу на выходе; фаза каждого цикла, ближайшая
    /// по позе ног к выходу (−1 — не мерялось).
    float active_s = 0.0f;
    bool exit_left = true;
    std::array<float, CLIP_ROLE_COUNT> exit_phase{};
    /// Что слой стойки узнал о клипе: пик маха рук, пик скрутки плеч и
    /// средний сгиб локтя — чтобы усиливать их к заказу, а не поверх.
    float arm_swing_peak_rad = 0.0f;
    float twist_peak_rad = 0.0f;
    float elbow_mean_rad = 0.0f;

    [[nodiscard]] bool present() const { return clip >= 0; }
};

struct ClipLibrary {
    std::array<ClipEntry, CLIP_ROLE_COUNT> role{};
    /// ВАРИАНТЫ ПОКОЯ (владелец 03.09: «добавь ещё idle из анимаций и пьяную
    /// версию»): стоя дольше IDLE_VARIANT_S, тело меняет клип покоя на один
    /// из этих (MX_Idle_1…5, Happy/Sad) с обычным кроссфейдом; `drunk_variant`
    /// — индекс пьяного покоя (MX_Drunk_Idle_Variation), его выбирает
    /// BodyDrive::drunk (клавиша H). −1 в `ClipPlayback::variant` — клип роли.
    std::vector<ClipEntry> idle_variants;
    int32_t drunk_variant = -1;
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
    /// СЛОЙ ВЗГЛЯДА (LookLayer.h): шея и грудь за камерой до LOOK_MAX_DEG.
    LookLayer look;
    /// ИНЕРЦИАЛИЗАЦИЯ СТЫКОВ (Inertializer.h, §13.7): смена роли — жёсткий срез
    /// (fade = 0, `ClipPlayback::switched`), разницу поз гасит владелец позы;
    /// false — линейный кроссфейд CLIP_CROSSFADE_S, как до 07.09.
    bool inertial = false;
    /// ПЕРЕХОДЫ ВКЛЮЧЕНЫ (§13): старт, остановка, поворот на месте. Ложь —
    /// прежний шов «состояние → цикл» без одноразовых клипов: контрольная
    /// рука приборов, которые характеризуют САМ ЦИКЛ (его размах, снос,
    /// темп), и дверь DFN_CLIP_TRANSITIONS=0 для сравнения в игре.
    bool transitions = true;
    /// ДОЗА СИММЕТРИИ ПОКОЯ: поза Idle, смешанная со своим зеркалом на той же
    /// фазе (строка IDLE_SYMMETRY_DOSE; 0 — клип как есть, контрольная рука).
    float idle_symmetry = static_cast<float>(config::IDLE_SYMMETRY_DOSE);
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
/// Клип роли с учётом варианта покоя (см. ClipLibrary::idle_variants).
[[nodiscard]] const ClipEntry& entry_for(const ClipLibrary& lib, ClipRole role, int32_t variant);

/// Путь опорной стопы от начала цикла до фазы (м), по кривой записи.
/// Фаза, на которой путь стал s (м) — обратная кривая; s сверх цикла
/// заворачивается. Плоский участок кривой обратной не имеет — тогда фаза
/// плоского участка, ближайшая спереди.
/// Наклон кривой на фазе (м на единицу фазы); ноль — стопа не опирается.
/// Фаза после хода ds (м) от фазы phase — ВПЕРЁД по кривой, внутри текущего
/// участка опоры: дошли до плоского участка (мах/полёт) — встали на его
/// начало, остаток хода свободен. Никогда не назад.

[[nodiscard]] ClipLibrary build_clip_library(
    const Rig& rig, const skel::Skeleton& skeleton, const SkinnedRigBinding& binding,
    std::span<const skel::AnimClip> clips,
    std::span<const platform::SkinnedVertex> skin = {},
    std::string_view role_overrides = {});
/// `role_overrides`: "Walk=KK_Walking_A,Jog=KK_Running_A" — роль по её
/// короткому имени получает НАЗВАННЫЙ клип вместо табличного (дверь
/// DFN_CLIP_ROLES: примерка чужих клипов без пересборки ассета). Нет такого
/// клипа — роль берётся из таблицы, как без двери.

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

/// The scale whose measured travel is `target_m`, read backwards off the
/// entry's curve. CLAMPED to [STRIDE_SCALE_MIN, STRIDE_SCALE_MAX]; when the
/// target is past the end of the curve the clamp is what the caller gets and
/// the residual slide is real.

/// The lift the entry's ground_curve asks for at `scale`, linearly between
/// the two grid points that bracket it. Metres, positive = up.

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
    /// Вариант покоя текущей/прошлой роли (ClipLibrary::idle_variants), −1 — клип роли.
    int32_t variant = -1;
    int32_t previous_variant = -1;
    float idle_s = 0.0f;       ///< сколько стоим на текущем покое
    MoveDir move_dir = MoveDir::Forward; ///< класс направления с гистерезисом (§9.2)
    uint32_t variant_pick = 0; ///< счётчик выборов — детерминированная «случайность»
    /// 1 -> 0 while the previous role fades out. Zero means "no cross-fade".
    /// СКОЛЬКО СЕЙЧАС «КЛИП ПЕРЕХОДА», 0..1, сглажено кроссфейдом. На этот вес
    /// СНИМАЕТСЯ СЛОЙ СТОЙКИ: клипы Mixamo (поворот, старт, остановка) —
    /// готовые авторские позы, а слой правит осанку ЦИКЛОВ UAL, которым
    /// ретаргет съел размах. Замер 04.09: слой перетирал рыск таза клипа
    /// поворота (90° в клипе → 9° на теле) — поворот просто не доезжал.
    float transit_dose = 0.0f;
    float prev_transit_dose = 0.0f;
    /// Сколько ещё нельзя запускать поворот на месте (TURN_MIN_GAP_S): камера,
    /// уехавшая на 180°, доворачивается двумя клипами по 90°, но не подряд
    /// кадр-в-кадр — иначе тело крутится волчком.
    /// Знак поворота на месте, который сейчас играет (+1 вправо, −1 влево):
    /// клип обрывается, когда разница «взгляд − корпус» меньше TURN_DONE_DEG
    /// или сменила знак — довернул.
    /// Пауза после клипа удара (STAGGER_MIN_GAP_S): длящийся толчок — один
    /// клип, а не клип каждый тик.
    float fade = 0.0f;
    float fade_s = 0.0f; ///< длительность текущего кроссфейда (переход → цикл длиннее)
    /// НА ЭТОМ ТИКЕ СМЕНИЛАСЬ РОЛЬ (или вариант покоя) — событие одного тика;
    /// при `ClipLibrary::inertial` владелец позы снимает по нему разницу.
    bool switched = false;
    /// РЫСК ВЗГЛЯДА ОТНОСИТЕЛЬНО КОРПУСА, сглаженный (LOOK_SMOOTH_S), рад;
    /// prev — для кадра между тиками.
    float look_yaw = 0.0f;
    float prev_look_yaw = 0.0f;
    /// НАКЛОН КОРПУСА ПО ТОЛЧКУ (ярус 0): вектор в системе тела, XZ, длина —
    /// угол в радианах (PUSH_LEAN_*), сглажен; prev — для кадра.
    glm::vec3 lean{0.0f};
    glm::vec3 prev_lean{0.0f};
    /// Clip time in seconds for the CURRENT role and for the one fading out.
    float time_s = 0.0f;
    float previous_time_s = 0.0f;
    /// The SAME two instants expressed in the blend partner's own clip time
    /// (ClipEntry::mix_clip). Carried rather than recomputed in the frame
    /// because the frame has no stride phase: it interpolates between two
    /// ticks, and each tick is what knew where in the cycle it was.
    /// The weapon guard runs off its OWN seconds: it is not locomotion, it has
    /// no stride, and its cycle is a man breathing over a raised blade.
    float weapon_time_s = 0.0f;
    /// Stride scale in force this tick, and the one the previous role had.
    /// ЧАСЫ ЛОКОМОЦИИ ЭТОЙ ЗОНЫ, [0,1) на цикл L+R, когда `ClipLibrary::feet_drive`:
    /// растут на dt·rate/длительность клипа роли, в покое стоят (как прежде
    /// держал сим на остановке). Наружу публикуются как фаза для боба камеры и
    /// событий шага — один интегратор, остальные читают.
    float phase = 0.0f;
    float prev_phase = 0.0f;
    /// ТЕМП: заказ передачи / скорость клипа, зажатый LOCOMOTION_TEMPO_BAND.
    float rate = 1.0f;
    /// The tick before this one, so the frame can interpolate (Rule 12's
    /// shape, applied to a pose instead of a Transform).
    float prev_time_s = 0.0f;
    float prev_previous_time_s = 0.0f;
    float prev_fade = 0.0f;
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
/// То же с памятью класса направления (гистерезис ±DIR_HYSTERESIS_DEG).
[[nodiscard]] ClipRole role_for_drive(const ClipLibrary& lib, const BodyDrive& drive,
                                      MoveDir& move_dir);

/// ONE FIXED TICK. Snapshots the previous tick, picks the role, starts a
/// cross-fade if it changed, and moves both clip times: locomotion roles are
/// placed by sim's stride phase, everything else advances by `dt`.
struct LocoMachine;
/// `machine` (Locomotion.h, §16) — если дана, роль и часы клипа берутся из
/// машины состояний (решения на входе), а не из состояния привода и порогов
/// переходов; прыжок/приземление по-прежнему решает признак земли.
void advance_playback(const ClipLibrary& lib, const BodyDrive& drive, float dt,
                      ClipPlayback& play, const LocoMachine* machine = nullptr);

/// THE FRAME. `alpha` in [0,1] interpolates between the previous tick and this
/// one exactly as render interpolates a Transform (Rule 12's shape). Writes the
/// imported skeleton's local TRS into `out_sample` (size >= skeleton.size()).
/// False means the role has no clip and the caller must draw something else.
[[nodiscard]] bool playback_sample(const skel::Skeleton& skeleton,
                                   const SkinnedRigBinding& binding,
                                   std::span<const skel::AnimClip> clips,
                                   const ClipLibrary& lib, const ClipPlayback& play,
                                   float alpha, std::span<JointLocal> out_sample);

} // namespace dfn::anim
