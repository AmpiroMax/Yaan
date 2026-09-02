/*
Module: engine/app
File: engine/app/sources/SkinnedCharacter.h

Responsibility:
- The app's half of the skinned character: load the baked .dfo, hand the mesh
  to render, and turn each tick's LocalPose into the frame's bone palette.

Key items:
- SkinnedCharacter::load(): .dfo -> registered skinned mesh + rig binding +
  the clip library resolved against the model's own clip names.
- SkinnedCharacter::advance(): ONE FIXED TICK -- picks the clip, moves it, and
  snapshots the tick's pose beside the previous one.
- SkinnedCharacter::build_draw(): alpha -> RenderSystem::SkinnedDraw, the pose
  interpolated between the two ticks the way render interpolates a Transform.
- morphs() / set_morph_weight() / apply_morphs() / bake_morphs(): ползунки тела
  (секция MORF). Состояние живёт ЗДЕСЬ: у него три потребителя — доза
  DFN_MORPH, панель редактора и выпечка, — и ни один из них не окно.
- SKINNED_CHARACTER_MESH_ID: the RenderMesh id this occupies (50).

Dependencies:
- Uses: engine/render (ObjectRegistry, RenderSystem), engine/anim (Rig, Pose,
  SkinnedBody), engine/platform/render (IRenderer).
- Used by: engine/app (AppWorld load, App per-frame ferry).

Notes:
- THE FERRY IS THE POINT. anim and render are siblings in the DAG and cannot
  include each other; the app is the one place that sees both, so this is
  where an imported skeleton becomes a draw. Exactly the shape BodyMesh's
  registration ferry already has, one wave later and with bones in it.
- TWO PATHS, ONE DOOR. By default the body plays the IMPORTED CLIPS; with
  DFN_PROC_GAIT=1 it plays the procedural gait the skinning wave left it with
  (Clips.h), through the retarget. The door exists so the two can be shot from
  one binary, one camera and one moment of the world (Rule 47) -- it is the
  before/after arm of this wave, exactly as DFN_BODY_BOXES was of the last.
- THE POSE IS INTERPOLATED BETWEEN TICKS, which is what the skinning wave
  named as its own tail: the segments got Rule 12's prev/curr pair and the
  skinned body did not, so at a 30 Hz sim the model stepped while the world
  around it slid. Both paths interpolate -- the clip by its clip time, the
  procedural by slerping the two ticks' poses -- so the door stays a fair
  comparison at any alpha.
- IT FAILS SOFT AND LOUD. No .dfo, a .dfo without a SKIN section, a skeleton
  nothing binds to: the character does not load, stderr says which, and the
  fifteen boxes keep drawing. A character that half-loads is worse than none.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The palette buffer lives here and is handed to render as a SPAN for the
  duration of one render() call. Do not let it outlive the frame or reallocate
  while a draw list points into it.
*/

#pragma once

#include "engine/anim/sources/Body.h"
#include "engine/anim/sources/ClipPlayer.h"
#include "engine/anim/sources/FootIk.h"
#include "engine/anim/sources/HeldBlade.h"
#include "engine/anim/sources/Hitbox.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/RootMotion.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/skeleton/sources/Skeleton.h"
#include "engine/render/sources/MorphBlend.h"
#include "engine/render/sources/RenderSystem.h"

#include "engine/platform/render/interfaces/IRenderer.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace dfn::app {

/// RenderMesh id for the skinned character: 128, the first id past every
/// range the map in render's ProcMesh.h hands out (items end at 127). NOT 49
/// -- that is the SPARE BONE of the body block, kept for a sixteenth bone --
/// and not 50, which is sim's door placeholder and refused the registration
/// out loud the first time this was tried.
inline constexpr uint32_t SKINNED_CHARACTER_MESH_ID = 128;

class SkinnedCharacter {
public:
    /// Reads the .dfo, uploads its SKIN stream and binds its SKEL to our rig.
    /// False (with a reason on stderr) leaves the object unloaded and inert.
    /// `rig` supplies the PROPORTIONS; the rest stance is solved on this
    /// body's skin (anim::fit_rest_pose) and the fitted rig lives here —
    /// `legacy_rest` keeps the box body's converged rest instead, the
    /// "before" arm of the owner's comparison (Rule 47).
    [[nodiscard]] bool load(render::RenderSystem& render_system,
                            platform::IRenderer& renderer, const anim::Rig& rig,
                            const std::filesystem::path& path, bool legacy_rest = false,
                            uint32_t mesh_asset = SKINNED_CHARACTER_MESH_ID,
                            uint32_t blade_asset = anim::HELD_BLADE_MESH_ID);
    /// THE SAME, FROM AN OBJECT ALREADY IN MEMORY — the half load() is made
    /// of. It exists so the character screen can build the SAME character the
    /// world will load from its bake (morphs applied, height scaled) without a
    /// file in between; `label` is what the log calls it.
    [[nodiscard]] bool load_object(render::RenderSystem& render_system,
                                   platform::IRenderer& renderer, const anim::Rig& rig,
                                   render::RegistryObject object,
                                   const std::filesystem::path& label,
                                   bool legacy_rest = false,
                                   uint32_t mesh_asset = SKINNED_CHARACTER_MESH_ID,
                                   uint32_t blade_asset = anim::HELD_BLADE_MESH_ID);
    /// Drops the meshes and forgets the body. A body that lives shorter than
    /// the world (the screen's, the viewer's) has to give its ids back.
    void release(render::RenderSystem& render_system, platform::IRenderer& renderer);
    /// The rig this body is bound with: config proportions, skin-fitted rest.
    [[nodiscard]] const anim::Rig& rig() const { return rig_; }
    [[nodiscard]] const anim::SkinnedRigBinding& binding() const { return binding_; }
    [[nodiscard]] uint32_t mesh_asset() const { return mesh_asset_; }
    /// THE VERTICES AS DRAWN: the morph blend when a slider moved, the bind
    /// otherwise. Bind-pose space; the palette poses them.
    [[nodiscard]] const std::vector<platform::SkinnedVertex>& current_vertices() const {
        return morph_dirty_ || morphed_.size() != bind_vertices_.size() ? bind_vertices_
                                                                        : morphed_;
    }
    [[nodiscard]] const std::vector<uint32_t>& skin_indices() const { return skin_indices_; }
    /// THE BODY IN OUR REST POSE, model space, CPU-skinned — what the screen
    /// frames by and what the "screen = world" hash is taken over. Not the
    /// idle's pose: the clip breathes, the rest does not.
    void rest_positions(std::vector<glm::vec3>& out) const;
    /// PURE REST, NO CLIP: build_draw poses the body in the rig's rest instead
    /// of the idle with its layers — the dose (DFN_CHARGEN_POSE=rest) that
    /// shows the neutral itself.
    void set_rest_only(bool on) { rest_only_ = on; }
    [[nodiscard]] bool rest_only() const { return rest_only_; }
    /// REPLACES THE SKIN ONLY (mesh, bind vertices, skin-fitted hitboxes) and
    /// keeps the rig, the binding and the clip library — the fast half of a
    /// slider drag. The rest is NOT re-solved here; load_object() does that.
    bool replace_vertices(render::RenderSystem& render_system,
                          platform::IRenderer& renderer,
                          std::span<const platform::SkinnedVertex> vertices);

    [[nodiscard]] bool ready() const { return ready_; }
    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] uint32_t bound_bones() const { return binding_.bound_count(); }
    [[nodiscard]] std::size_t joint_count() const { return skeleton_.size(); }
    [[nodiscard]] std::size_t triangle_count() const { return triangles_; }
    [[nodiscard]] const skel::Skeleton& skeleton() const { return skeleton_; }
    [[nodiscard]] const std::vector<skel::AnimClip>& clips() const { return clips_; }
    [[nodiscard]] const anim::ClipLibrary& clip_library() const { return library_; }
    [[nodiscard]] const anim::ClipPlayback& playback() const { return play_; }
    /// True when the body is being drawn from imported clips rather than from
    /// the procedural gait (DFN_PROC_GAIT=1 turns it off).
    [[nodiscard]] bool playing_clips() const;

    /// WHERE THE GROUND IS UNDER A WORLD POINT, metres, or NaN for "no ground
    /// found". The app installs a raycast into the physics world; anim never
    /// sees it (FootIk.h is handed answers, not a world).
    ///
    /// A CALLBACK AND NOT AN IPhysics HANDLE, so a test can stand this body on
    /// a staircase it wrote in four lines. The stand's real stairs are what
    /// the frames are shot on; the four lines are what makes the number
    /// reproducible without a window.
    using GroundProbe = std::function<float(const glm::vec3&)>;
    void set_ground_probe(GroundProbe probe) { ground_probe_ = std::move(probe); }

    /// THE HITBOX TABLE OF THIS BODY and where its shapes are for the pose
    /// build_draw last produced. The pose is in the MODEL'S own space; the
    /// caller multiplies by `draw.transform` — the same matrix the mesh got,
    /// which is what makes "what you see is what you shoot" true by
    /// construction rather than by a second placement.
    [[nodiscard]] const anim::HitboxSet& hitboxes() const { return hitboxes_; }
    [[nodiscard]] const anim::HitboxPose& hitbox_pose() const { return hitbox_pose_; }

    /// The last tick's grounding, for the debug readout and the report.
    [[nodiscard]] const anim::FootIkPlan& foot_plan() const { return plan_; }
    [[nodiscard]] float foot_root_shift_m() const { return root_dy_; }

    /// ONE FIXED TICK. Advances the clip state machine and snapshots this
    /// tick's procedural pose and root beside the previous tick's, so
    /// build_draw can interpolate either path. `standing_ground` is the
    /// player's Transform position, i.e. the capsule bottom.
    ///
    /// ПЕРЕМЕЩЕНИЕ ВЕДЁТ ОПОРНАЯ СТОПА (docs/design/LOCOMOTION_GROUNDED.md):
    /// зовётся ДО шага сим'а, потому что здесь рождается заявка на смещение
    /// капсулы за этот тик (locomotion()): корень обязан сдвинуться ровно на
    /// столько, на сколько опорная стопа ушла под телом между прошлой и этой
    /// позой. После шага сим'а приложение зовёт commit_root() с фактом.
    void advance(const anim::BodyDrive& drive, const glm::vec3& standing_ground,
                 float dt);
    /// ЗАЯВКА ЭТОГО ТИКА: смещение корня в системе тела, фаза, постановки.
    [[nodiscard]] const anim::LocomotionOut& locomotion() const { return loco_; }
    /// КОРЕНЬ ПОСЛЕ ШАГА СИМ'А. Положение капсулы стало фактом: сюда встаёт
    /// корень этого тика (advance() снял его до шага), и на нём защёлкиваются
    /// и отпускаются замки стоп — якорь замка это мировая точка касания на
    /// ФАКТИЧЕСКОМ корне, иначе замок держал бы стопу там, где капсула не была.
    void commit_root(const anim::BodyDrive& drive, const glm::vec3& standing_ground,
                     float dt);
    [[nodiscard]] const anim::ContactState& contacts() const { return contact_curr_; }
    [[nodiscard]] const anim::FootLockState& foot_locks() const { return locks_; }
    /// ЗАЗОР СТОП ПОСЛЕДНЕГО КАДРА (знаковый, по FootIk::foot_gap) — после
    /// подъёма на грунт и замка; прибор ступеней/склона читает его с кадра.
    [[nodiscard]] const anim::FootGap& foot_gap_last() const { return last_gap_; }
    /// Двери: DFN_ROOT_FROM_FEET=0 — прежний шов (сим двигает, стрид-скейл),
    /// DFN_FOOT_LOCK=0 — без замка. Тесты ставят их напрямую (правило 47:
    /// обе руки из одного бинарника).
    void set_feet_drive(bool on);
    void set_foot_lock(bool on) { foot_lock_ = on; }
    [[nodiscard]] bool feet_drive() const { return feet_drive_; }
    [[nodiscard]] bool foot_lock() const { return foot_lock_; }

    /// Builds the palette for one pose and returns the draw that shows it.
    /// `hide_head` collapses the head bone so a first-person camera inside the
    /// skull sees the world instead of the inside of a face -- the skinned
    /// equivalent of the box body's hidden head segment.
    [[nodiscard]] render::RenderSystem::SkinnedDraw build_draw(bool hide_head,
                                                               float alpha);

    // --- МОРФЫ ТЕЛА (редактор персонажа, шаг 1) ---------------------------
    //
    // ЗДЕСЬ, А НЕ В ПАНЕЛИ, потому что состояние ползунков — свойство ТЕЛА, а
    // не окна: доза DFN_MORPH правит его без всякого окна, пресет пишется из
    // него же, и выпечка читает его. Панель — один из трёх потребителей и
    // самый поздний.

    /// Что за ползунки есть у этого тела (секция MORF). Пусто у выпеченного.
    [[nodiscard]] const std::vector<render::MorphTarget>& morphs() const {
        return morphs_;
    }
    /// Веса ползунков. Читается панелью и дозой; писать — только через
    /// set_morph_weight, потому что запись обязана пометить тело грязным.
    [[nodiscard]] const render::MorphState& morph_state() const { return morph_; }
    /// Ставит вес одного ползунка, ЗАЖИМАЯ его в полосу цели. Полоса лежит в
    /// файле (MorphTarget::lo/hi) и измерена приёмкой на крайних положениях:
    /// ползунок, умеющий выйти за неё, — это ползунок, после которого судья
    /// пропорций красный, а виноват интерфейс.
    void set_morph_weight(std::size_t index, float value);
    void reset_morphs();
    /// Есть ли несданная правка ползунков. Отдельный флаг, а не сравнение
    /// векторов: бленд стоит 0.03 мс, но перекладка меша на GPU — нет, и
    /// делать её на каждом кадре неподвижного ползунка незачем.
    [[nodiscard]] bool morphs_dirty() const { return morph_dirty_; }
    /// ПЕРЕСЧЁТ И ПЕРЕКЛАДКА. Зовётся НА ДВИЖЕНИЕ ПОЛЗУНКА (и один раз после
    /// загрузки, если доза что-то накрутила), а не покадрово — см. MorphBlend.h.
    /// Возвращает false и говорит вслух, если перекладка не удалась.
    bool apply_morphs(render::RenderSystem& render_system,
                      platform::IRenderer& renderer);
    /// ВЫПЕЧКА ПО «ГОТОВО»: тело с применёнными ползунками и БЕЗ секции MORF,
    /// как Creation Kit пишет FaceGeom. Мир грузит обычного персонажа и про
    /// ползунки не знает.
    /// `scale` — равномерный масштаб роста (scale_registry_object): в мир
    /// уезжает тело того роста, что на экране.
    [[nodiscard]] bool bake_morphs(const std::filesystem::path& out,
                                   float scale = 1.0f) const;
    /// Пресет — ТОЛЬКО ЧИСЛА ПОЛЗУНКОВ (json). Тем же порядком, что в файле.
    [[nodiscard]] bool save_preset(const std::filesystem::path& out) const;

    /// IS THERE A BLADE IN THE HAND THIS FRAME. The pose crosses over at
    /// WEAPON_CROSSFADE_S; the sword is not faded with it, because a blade
    /// half-drawn is a blade half-INSIDE the hand and there is nothing to see
    /// through. It appears when the guard is more than half on.
    [[nodiscard]] bool blade_drawn() const;
    /// THE SAME PALETTE AND THE SAME TRANSFORM AS THE BODY, with the blade's
    /// mesh id. It rides the character's own bones (HeldBlade.h), so passing
    /// the body's draw through here is the whole placement: there is no second
    /// matrix that could disagree with the fist.
    [[nodiscard]] render::RenderSystem::SkinnedDraw blade_draw(
        const render::RenderSystem::SkinnedDraw& body) const;

private:
    /// Одна строка прибора стоп на кадр (DFN_FOOT_TRACE): дверь читается при
    /// загрузке, а печатает кадр.
    void foot_trace_step(const anim::FootIkPlan& plan);

    /// ONE TICK'S RAYCASTS. Separate from advance() because it is the only
    /// part of the tick that touches the world, and because a body with no
    /// probe installed has to skip exactly this and nothing else.
    void probe_ground(const anim::BodyDrive& drive, const glm::vec3& standing_ground,
                      float dt);

    bool ready_ = false;
    std::string name_;
    uint32_t mesh_asset_ = SKINNED_CHARACTER_MESH_ID;
    uint32_t blade_asset_ = anim::HELD_BLADE_MESH_ID;
    bool rest_only_ = false;
    /// The fitted rig (see load()). The app's own rig is the box body's;
    /// this one has the rest stance the skin asked for.
    anim::Rig rig_{};
    std::size_t triangles_ = 0;
    skel::Skeleton skeleton_;
    /// A copy of the bind vertices, kept ONLY so the DFN_CHAR_TRACE door can
    /// measure the posed body in metres. The picture cannot answer "how tall
    /// is he" -- perspective makes a small model near the camera and a large
    /// one far away look the same, which is exactly the confusion that cost
    /// this wave an hour.
    std::vector<platform::SkinnedVertex> bind_vertices_;
    /// ЦЕЛИ, ВЕСА И РЕЗУЛЬТАТ БЛЕНДА. `morphed_` — рабочий буфер, а не вторая
    /// правда: истина — это bind_vertices_ плюс веса, и buffer существует лишь
    /// затем, чтобы не выделять восемь тысяч вершин на каждое движение ручки.
    /// ИНДЕКСЫ СКИНА, оставленные ради НОРМАЛЕЙ бленда. Без них морф двигал бы
    /// вершины и оставлял свет от прежней формы — тот самый «всё кривенько»,
    /// за который импортёр уже пересчитывал нормали после --reshape.
    std::vector<uint32_t> skin_indices_;
    std::vector<render::MorphTarget> morphs_;
    render::MorphState morph_{};
    std::vector<platform::SkinnedVertex> morphed_;
    bool morph_dirty_ = false;
    std::filesystem::path source_path_;
    /// THE OBJECT AS IT ARRIVED — materials, provenance, everything the reader
    /// brought — kept so bake_morphs writes the whole body and not the half
    /// this class works with; and so a body built from memory can bake.
    render::RegistryObject source_object_;
    std::vector<skel::AnimClip> clips_;
    anim::SkinnedRigBinding binding_;
    anim::ClipLibrary library_{};
    anim::ClipPlayback play_{};
    /// This tick's and the previous tick's procedural pose and root. Kept here
    /// rather than in BodyDrive because only the model interpolates a POSE:
    /// the box body's segments already get Rule 12's pair from render.
    anim::LocalPose pose_curr_{};
    anim::LocalPose pose_prev_{};
    anim::BodyRoot root_curr_{};
    anim::BodyRoot root_prev_{};
    bool ticked_ = false;
    /// One frame's imported-skeleton locals, a member for the same reason the
    /// palette is: it must not reallocate while a draw list points into it.
    std::vector<anim::JointLocal> sample_;
    /// ПОЗА РЕЕСТРА, ПЕРЕВЕДЁННАЯ В СУСТАВЫ МОДЕЛИ, и вес, с которым она
    /// подмешивается к сэмплу клипа. Иначе поза реестра была бы видна только
    /// за дверью DFN_PROC_GAIT: в штатном пути тело гнут клипы, и наш
    /// пятнадцатикостный слой до кадра не доходит вовсе.
    std::vector<anim::JointLocal> pose_sample_;
    float pose_weight_ = 0.0f;
    /// One frame's palette. A member so the span handed to render points at
    /// storage that outlives the call and is never reallocated mid-frame.
    std::vector<glm::mat4> palette_;

    // --- FOOT IK ----------------------------------------------------------
    GroundProbe ground_probe_;
    anim::FootIkSetup foot_setup_{};
    /// What the raycasts found this tick, in the body's own frame.
    anim::FootIkProbe foot_probe_{};
    anim::FootIkPlan plan_{};
    anim::FootGap last_gap_{};
    /// THE ROOT SHIFT, FILTERED. The raw shift jumps by a whole stair rise the
    /// instant a ray crosses a nosing, and the body would tick with it; the
    /// filter spends FOOT_IK_ROOT_TAU_S getting there. Filtered here rather
    /// than inside the solve because only the caller has a clock (Rule 12).
    float root_dy_ = 0.0f;
    /// Высота капсулы прошлого тика — чтобы её прыжок на ступень вычесть из
    /// корня сразу (см. probe_ground).
    float last_ground_y_ = 0.0f;
    /// How much of the solve is in force, eased. Zero in the air and in a
    /// seat: a jump that is glued to the ground is not a jump, and a seated
    /// body's feet answer to the bench.
    float ik_strength_ = 0.0f;
    /// One frame's scratch for the tick-time probe pose.
    std::vector<anim::JointLocal> tick_sample_;
    /// Поза этого тика снята (playback_sample при alpha = 1) — probe_ground и
    /// контакты читают её, не снимая второй раз.
    bool tick_sampled_ = false;

    // --- ПЕРЕМЕЩЕНИЕ ОТ СТОПЫ И ЗАМОК (RootMotion.h, FootIk.h) --------------
    anim::LocomotionOut loco_{};
    anim::ContactState contact_prev_{};
    anim::ContactState contact_curr_{};
    anim::FootLockState locks_{};
    /// Память корневого движения: оценка скорости тела и инерция полёта.
    anim::RootMotionState root_state_{};
    anim::FootLockParams lock_params_{};
    bool feet_drive_ = true;
    bool foot_lock_ = true;
    /// ROOT_MOTION_SMOOTH_S в силе (DFN_ROOT_SMOOTH=0 снимает).
    bool root_smooth_ = true;
    glm::vec3 smoothed_delta_{0.0f};
    /// ДВЕРЬ DFN_SLIDE_TRACE: печатать остаток, который замку приходится
    /// закрывать (мировая точка касания до замка минус якорь), раз в 10 тиков.
    bool slide_trace_ = false;
    uint32_t slide_trace_ticks_ = 0;
    /// ДВЕРЬ DFN_FOOT_TRACE: печатать по стопам грунт, ЗНАКОВЫЙ зазор, вес
    /// опоры и сдвиг корня. Заведена под пункт 3 заказа 31.08 («стоя на
    /// объекте одна стопа парит»): кадр не отвечает на «парит на сантиметр»,
    /// а прежний прибор зоны (foot_penetration) срезал положительную половину
    /// и по построению читал парение нулём.
    bool foot_trace_ = false;
    /// ДОЗА DFN_FOOT_IK: false — решатель стоп выключен целиком, контрольная
    /// рука приёмки. Не «сила 0» на кадре, а закрытый ЗАТВОР: сила еле-еле
    /// сходит к нулю по FOOT_IK_GATE_TAU_S, и рука, снятая на переходе, была
    /// бы рукой с половиной решателя.
    bool foot_ik_enabled_ = true;
    uint64_t foot_trace_frames_ = 0;

    // --- THE BLADE --------------------------------------------------------
    /// The sword's geometry, kept after upload so the report can print what
    /// was built and a test can find its point without rebuilding it.
    anim::HeldBlade blade_{};
    bool blade_ready_ = false;

    // --- HITBOXES ---------------------------------------------------------
    anim::HitboxSet hitboxes_{};
    anim::HitboxPose hitbox_pose_{};
};

/// РАВНОМЕРНЫЙ МАСШТАБ ВСЕГО, ЧТО ЗНАЕТ ПРО МЕТРЫ: вершины скина, переносы
/// привязки (и строка переносов обратной привязки), переносы клипов. Модельная
/// матрица сустава — произведение локальных T·R·S; умножив каждый локальный
/// перенос на k, получаем сопряжение однородным масштабом, при котором
/// скиннинг даёт ровно k·v: тело едет целиком, ни одна ПРОПОРЦИЯ не трогается
/// — почему судья и пропускает оба конца полосы роста, в отличие от морфа.
/// Один на выпечку и на экран (правило 32).
void scale_registry_object(render::RegistryObject& obj, float k);

} // namespace dfn::app
