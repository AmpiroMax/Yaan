/*
Module: engine/app
File: engine/app/sources/SkinnedCharacter.cpp

Responsibility:
- Implements the load + per-frame palette of the skinned character.

Dependencies:
- Uses: SkinnedCharacter.h, engine/render ObjectRegistry.
- Used by: dfn_app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every refusal is LOUD. Absence presenting as a neutral state is this
  project's most expensive recurring bug.
*/

#include "engine/app/sources/SkinnedCharacter.h"

#include "engine/app/sources/AppDoors.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace dfn::app {
namespace {

/// The shortest signed turn from `from` to `to`, radians. Interpolating a yaw
/// by plain subtraction spins the body the long way round exactly once per
/// revolution, and once per revolution is often enough to be seen.
[[nodiscard]] float shortest_turn(float from, float to) {
    float d = to - from;
    while (d > glm::pi<float>()) {
        d -= 2.0f * glm::pi<float>();
    }
    while (d < -glm::pi<float>()) {
        d += 2.0f * glm::pi<float>();
    }
    return d;
}

/// HOW FAST THE ROOT FOLLOWS A NEW GROUND, seconds to 63 % of the way.
///
/// NOT ZERO, and the stand is where that gets decided: a ray crossing a
/// stair's nosing changes its answer by a whole 0.18 m rise in one tick, and a
/// root that took it instantly would tick down the flight one step per frame
/// instead of walking down it. NOT LONG EITHER: past about a tenth of a second
/// the body visibly lags the step it is standing on. 0.08 s is a shade under
/// three ticks at 30 Hz, so a single bad ray is smoothed and a real step is
/// followed inside one stride.
constexpr float FOOT_IK_ROOT_TAU_S = 0.08f;

/// How fast the whole solve fades in and out (a jump, sitting down). Its own
/// number: it gates a MECHANISM rather than tracks a surface, and it wants to
/// be off before the take-off frame rather than a tick after it.
constexpr float FOOT_IK_GATE_TAU_S = 0.06f;

} // namespace

bool SkinnedCharacter::load(render::RenderSystem& render_system,
                            platform::IRenderer& renderer, const anim::Rig& rig,
                            const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        std::fprintf(stderr,
                     "[character] \"%s\" is not there — the body stays as the "
                     "fifteen boxes. Bake it with dfn_import_gltf (the CMake "
                     "target dfn_characters does it).\n",
                     path.string().c_str());
        return false;
    }
    const auto obj = render::read_object(path);
    if (!obj.has_value()) {
        std::fprintf(stderr, "[character] \"%s\" refused by the registry\n",
                     path.string().c_str());
        return false;
    }
    if (obj->skin.empty() || obj->skeleton.empty()) {
        std::fprintf(stderr,
                     "[character] \"%s\" carries no SKIN/SKEL section — it is "
                     "not a character\n", path.string().c_str());
        return false;
    }
    if (obj->skeleton.size() > platform::MAX_BONE_PALETTE) {
        std::fprintf(stderr,
                     "[character] \"%s\": %zu joints, palette holds %u — REFUSED "
                     "(truncating would fold the tail of the skeleton into the "
                     "navel)\n",
                     path.string().c_str(), obj->skeleton.size(),
                     platform::MAX_BONE_PALETTE);
        return false;
    }
    skeleton_ = obj->skeleton;
    clips_ = obj->clips;
    binding_ = anim::bind_skinned_rig(rig, skeleton_);
    if (binding_.bound_count() == 0) {
        std::fprintf(stderr,
                     "[character] \"%s\": not one joint name maps to a rig bone "
                     "— it would draw its bind pose forever. REFUSED; see the "
                     "synonym table in engine/anim/sources/BoneMap.cpp\n",
                     path.string().c_str());
        return false;
    }
    if (!render_system.register_skinned_mesh(renderer, SKINNED_CHARACTER_MESH_ID,
                                             obj->skin.vertices,
                                             obj->skin.indices)) {
        return false;
    }
    bind_vertices_ = obj->skin.vertices;
    skin_indices_ = obj->skin.indices;
    morphs_ = obj->morphs;
    morph_.resize_for(morphs_);
    source_path_ = path;
    name_ = obj->name;
    triangles_ = obj->skin.indices.size() / 3;
    palette_.assign(skeleton_.size(), glm::mat4{1.0f});
    sample_.assign(skeleton_.size(), anim::JointLocal{});
    library_ = anim::build_clip_library(rig, skeleton_, binding_, clips_);
    foot_setup_ = anim::build_foot_ik(skeleton_, binding_, library_.contacts);
    {
        // ЧЕРЕЗ door_value, А НЕ getenv (AppDoors.h): дверь, прочитанная мимо
        // таблицы, — рычаг, о котором знает только написавший.
        const char* e = door_value("DFN_FOOT_TRACE");
        foot_trace_ = e != nullptr && e[0] == '1';
        // ДОЗА DFN_FOOT_IK=0 — КОНТРОЛЬНАЯ РУКА, А НЕ УДОБСТВО. Приёмка
        // «стопы стоят на предмете» без руки, которая обязана провалиться,
        // ничего не меряет (правило 30), а «до» вчерашней сборкой меряет
        // неделю, а не правку (правило 47).
        const char* ik = door_value("DFN_FOOT_IK");
        foot_ik_enabled_ = !(ik != nullptr && ik[0] == '0');
        if (!foot_ik_enabled_) {
            std::fprintf(stderr, "[character] DFN_FOOT_IK=0: решатель стоп "
                                 "выключен (контрольная рука)\n");
        }
    }
    hitboxes_ = anim::build_hitboxes(rig.proportions);
    tick_sample_.assign(skeleton_.size(), anim::JointLocal{});
    // THE BLADE, uploaded beside the body. LOUD when it cannot be built: a
    // drawn weapon that draws as nothing is exactly the state the comparison
    // report called its blocker, and it looked from the frame like a POSE
    // problem.
    // THE GUARD POSE IS PART OF THE ARGUMENT: the sword's tilt is fixed to the
    // hand, so the only pose that can decide it is the one it is carried in.
    std::vector<anim::JointLocal> guard(skeleton_.size());
    if (library_.has(anim::ClipRole::WeaponIdle)) {
        anim::sample_clip_pose(
            skeleton_,
            clips_[static_cast<std::size_t>(library_[anim::ClipRole::WeaponIdle].clip)],
            0.0f, guard);
    } else {
        guard.clear();
    }
    blade_ = anim::build_held_blade(skeleton_, binding_, guard);
    if (!blade_.valid()) {
        std::fprintf(stderr,
                     "[character] no blade: the right hand or forearm of \"%s\" did "
                     "not bind, so there is no line to lay a sword along. The guard "
                     "pose will play over an EMPTY hand.\n",
                     path.string().c_str());
    } else if (!render_system.register_skinned_mesh(renderer, anim::HELD_BLADE_MESH_ID,
                                                    blade_.vertices, blade_.indices)) {
        std::fprintf(stderr,
                     "[character] the blade mesh (id %u) was refused by the registry "
                     "— the guard pose will play over an EMPTY hand\n",
                     anim::HELD_BLADE_MESH_ID);
    } else {
        blade_ready_ = true;
    }
    // ДОЗА DFN_MORPH=имя=вес,... — приёмка БЕЗ ОКНА. Кадр правила 47
    // снимается счётным прогоном, у которого нет ни мыши, ни панели; ползунок,
    // до которого нельзя дотянуться из рецепта, — это ползунок, чей результат
    // нельзя сравнить с вчерашним.
    {
        const char* recipe = door_value("DFN_MORPH");
        if (recipe != nullptr && recipe[0] != '\0') {
            if (morphs_.empty()) {
                std::fprintf(stderr,
                             "[character] DFN_MORPH задана, а у \"%s\" нет секции "
                             "MORF — крутить нечего\n", path.string().c_str());
            } else {
                std::string text = recipe;
                std::size_t at = 0;
                while (at < text.size()) {
                    const std::size_t comma = std::min(text.find(',', at), text.size());
                    const std::size_t eq = text.find('=', at);
                    if (eq == std::string::npos || eq > comma) {
                        std::fprintf(stderr,
                                     "[character] DFN_MORPH: «%s» не имя=число\n",
                                     text.substr(at, comma - at).c_str());
                        at = comma + 1;
                        continue;
                    }
                    const std::string key = text.substr(at, eq - at);
                    const float value =
                        std::strtof(text.c_str() + eq + 1, nullptr);
                    const int slot = render::morph_index(morphs_, key);
                    if (slot < 0) {
                        // ГРОМКО И БЕЗ ПОДМЕНЫ. Опечатка в рецепте обязана
                        // остаться видимой: кадр, снятый «почти по рецепту», —
                        // это кадр, который нельзя сравнить ни с чем.
                        std::fprintf(stderr,
                                     "[character] DFN_MORPH: ползунка \"%s\" у тела "
                                     "нет; есть:", key.c_str());
                        for (const render::MorphTarget& t : morphs_) {
                            std::fprintf(stderr, " %s", t.name.c_str());
                        }
                        std::fprintf(stderr, "\n");
                    } else {
                        set_morph_weight(static_cast<std::size_t>(slot), value);
                        std::fprintf(stderr, "[character] DFN_MORPH %s=%.3f\n",
                                     key.c_str(),
                                     static_cast<double>(
                                         morph_.weights[static_cast<std::size_t>(slot)]));
                    }
                    at = comma + 1;
                }
                if (morph_dirty_) {
                    (void)apply_morphs(render_system, renderer);
                }
            }
        }
    }
    ready_ = true;
    std::fprintf(stderr,
                 "[character] \"%s\": %zu joints (%u of %u rig bones bound), "
                 "%zu triangles, %zu clips, model %.2f m\n",
                 name_.c_str(), skeleton_.size(), binding_.bound_count(),
                 anim::BONE_COUNT, triangles_, clips_.size(),
                 static_cast<double>(binding_.model_height_m));
    // WHICH ROLES THE ASSET ANSWERED, printed for the same reason the importer
    // prints its bone map: a model that resolves four roles of ten still draws
    // and still walks, and the only moment anybody would notice is this line.
    std::fprintf(stderr, "[character] clips: %u of %u roles —", library_.resolved,
                 anim::CLIP_ROLE_COUNT);
    for (uint32_t r = 0; r < anim::CLIP_ROLE_COUNT; ++r) {
        const auto role = static_cast<anim::ClipRole>(r);
        const anim::ClipEntry& e = library_[role];
        if (!e.present()) {
            std::fprintf(stderr, " %s=NONE", anim::role_name(role).data());
            continue;
        }
        std::fprintf(stderr, " %s=\"%s\"(%.2fs",
                     anim::role_name(role).data(),
                     clips_[static_cast<std::size_t>(e.clip)].name.c_str(),
                     static_cast<double>(e.duration_s));
        if (e.cycle_m > 0.0f) {
            std::fprintf(stderr, ", %.2fm/cycle, plant %.2f", 
                         static_cast<double>(e.cycle_m),
                         static_cast<double>(e.footfall_phase));
        }
        std::fprintf(stderr, ")");
    }
    std::fprintf(stderr, "\n");
    // THE TWO LAYERS, printed for the same reason the roles are: an arm layer
    // that solved 0 degrees and a mask with no upper half both draw a body
    // that looks exactly like a body with no layers at all.
    std::fprintf(stderr,
                 "[character] layers: mask upper %u / lower %u of %zu joints; arm "
                 "relax %.1f deg (hand %.3f m -> target %.3f m), %zu finger joints\n",
                 library_.mask.count(anim::Branch::Upper),
                 library_.mask.count(anim::Branch::Lower), skeleton_.size(),
                 static_cast<double>(library_.relax.angle_rad * 57.29578f),
                 static_cast<double>(library_.relax.reference_m),
                 static_cast<double>(library_.relax.target_m),
                 library_.relax.finger.size());
    return true;
}

// ------------------------------------------------------ МОРФЫ ТЕЛА ---------

void SkinnedCharacter::set_morph_weight(std::size_t index, float value) {
    if (index >= morphs_.size() || index >= morph_.weights.size()) {
        return;
    }
    // ЗАЖИМ В ПОЛОСУ ЦЕЛИ, И ОН ЗДЕСЬ, А НЕ В ПАНЕЛИ. Полоса — свойство ЦЕЛИ
    // (замерена приёмкой tools/check_morph_bands.py), и потребителей у неё
    // трое: ползунок, доза и пресет. Зажим в одном из трёх — это два пути,
    // которыми канон обходится.
    const render::MorphTarget& t = morphs_[index];
    const float clamped = std::clamp(value, t.lo, t.hi);
    if (clamped == morph_.weights[index]) {
        return;
    }
    morph_.weights[index] = clamped;
    morph_dirty_ = true;
}

void SkinnedCharacter::reset_morphs() {
    for (std::size_t i = 0; i < morph_.weights.size(); ++i) {
        if (morph_.weights[i] != 0.0f) {
            morph_.weights[i] = 0.0f;
            morph_dirty_ = true;
        }
    }
}

bool SkinnedCharacter::apply_morphs(render::RenderSystem& render_system,
                                    platform::IRenderer& renderer) {
    if (morphs_.empty() || bind_vertices_.empty()) {
        morph_dirty_ = false;
        return false;
    }
    render::blend_morphs(bind_vertices_, morphs_, morph_.weights, skin_indices_,
                         morphed_);
    if (!render_system.replace_skinned_mesh(renderer, SKINNED_CHARACTER_MESH_ID,
                                            morphed_, skin_indices_)) {
        // ГРЯЗНЫМ ОСТАВЛЯЕМ НАРОЧНО: неудачная перекладка — это тело, которое
        // на экране не то, что в состоянии, и следующий вызов обязан
        // попробовать ещё раз, а не считать себя сделанным.
        return false;
    }
    morph_dirty_ = false;
    return true;
}

bool SkinnedCharacter::bake_morphs(const std::filesystem::path& out) const {
    // ЧИТАЕМ ИСХОДНЫЙ ФАЙЛ ЗАНОВО, а не пишем то, что держим в памяти. В памяти
    // у нас скин, скелет и клипы — но НЕ материалы кусков, не происхождение и
    // не всё, что читатель мог принести; выпечка, потерявшая половину объекта,
    // была бы «почти тем же телом», а такого не бывает.
    const auto obj = render::read_object(source_path_);
    if (!obj.has_value()) {
        std::fprintf(stderr, "[character] выпечка: не читается \"%s\"\n",
                     source_path_.string().c_str());
        return false;
    }
    render::RegistryObject baked = *obj;
    std::vector<platform::SkinnedVertex> blended;
    render::blend_morphs(baked.skin.vertices, baked.morphs, morph_.weights,
                         baked.skin.indices, blended);
    baked.skin.vertices = std::move(blended);
    // СЕКЦИЯ СНИМАЕТСЯ — схема Skyrim: ползунки живут только в редакторе, в мир
    // уезжает выпеченное (docs/research/CHARACTER_EDITOR_TOOLS.md §1.3).
    baked.morphs.clear();
    baked.source += " morph:baked";
    if (!render::write_object(baked, out)) {
        std::fprintf(stderr, "[character] выпечка: не пишется \"%s\"\n",
                     out.string().c_str());
        return false;
    }
    std::fprintf(stderr, "[character] выпечено \"%s\" (hash %016llx)\n",
                 out.string().c_str(),
                 static_cast<unsigned long long>(render::object_content_hash(baked)));
    return true;
}

bool SkinnedCharacter::save_preset(const std::filesystem::path& out) const {
    std::error_code ec;
    std::filesystem::create_directories(out.parent_path(), ec);
    std::FILE* f = std::fopen(out.string().c_str(), "wb");
    if (f == nullptr) {
        std::fprintf(stderr, "[character] пресет: не пишется \"%s\"\n",
                     out.string().c_str());
        return false;
    }
    // ПРЕСЕТ — ТОЛЬКО ЧИСЛА. Ни вершин, ни хэша тела: тело выпекается ИЗ чисел,
    // и ровно поэтому один пресет даёт один файл на любой машине.
    std::fprintf(f, "{\n  \"version\": 1,\n  \"sliders\": {\n");
    for (std::size_t i = 0; i < morphs_.size(); ++i) {
        std::fprintf(f, "    \"%s\": %.6f%s\n", morphs_[i].name.c_str(),
                     static_cast<double>(i < morph_.weights.size()
                                             ? morph_.weights[i] : 0.0f),
                     i + 1 == morphs_.size() ? "" : ",");
    }
    std::fprintf(f, "  }\n}\n");
    std::fclose(f);
    std::fprintf(stderr, "[character] пресет -> \"%s\"\n", out.string().c_str());
    return true;
}

void SkinnedCharacter::probe_ground(const anim::BodyDrive& drive,
                                    const glm::vec3& standing_ground, float dt) {
    // THE GATE FIRST: airborne or seated, the solve is off. A jump whose feet
    // are pinned to the ground under them is not a jump, and a seated body's
    // feet answer to the bench and not to the floor.
    const float want_gate = (foot_ik_enabled_ && drive.grounded
                             && drive.posture_blend < 0.5f && ground_probe_)
                                ? 1.0f
                                : 0.0f;
    const float gate_k =
        dt > 0.0f ? 1.0f - std::exp(-dt / FOOT_IK_GATE_TAU_S) : 1.0f;
    ik_strength_ += (want_gate - ik_strength_) * gate_k;
    foot_probe_.valid = false;
    if (!ground_probe_ || !foot_setup_.valid() || !playing_clips()) {
        root_dy_ += (0.0f - root_dy_) * gate_k;
        return;
    }
    // THE POSE THIS TICK ENDED ON, which is where the rays go from. The frame
    // will re-measure the needs against its own interpolated pose; only the
    // GROUND is sampled here, because a raycast is the expensive half and the
    // ground under a foot does not change inside one tick.
    if (!anim::playback_sample(skeleton_, binding_, clips_, library_, play_, 1.0f,
                               tick_sample_)) {
        return;
    }
    std::vector<glm::mat4> local(skeleton_.size());
    std::vector<glm::mat4> model(skeleton_.size());
    for (std::size_t j = 0; j < skeleton_.size(); ++j) {
        local[j] = glm::translate(glm::mat4{1.0f}, tick_sample_[j].translation)
                   * glm::mat4_cast(glm::normalize(tick_sample_[j].rotation))
                   * glm::scale(glm::mat4{1.0f}, tick_sample_[j].scale);
    }
    skel::skeleton_model_matrices(skeleton_, local, model);
    const anim::BodyRoot root = anim::body_root_for(drive, standing_ground);
    const glm::mat4 to_world =
        glm::translate(glm::mat4{1.0f}, root.ground)
        * glm::rotate(glm::mat4{1.0f}, -root.yaw, glm::vec3{0.0f, 1.0f, 0.0f});
    const auto ground_under = [&](int32_t joint, float fallback) {
        if (joint < 0) {
            return fallback;
        }
        const glm::vec3 world =
            glm::vec3{to_world * glm::vec4{glm::vec3{model[static_cast<std::size_t>(joint)][3]},
                                           1.0f}};
        const float y = ground_probe_(world);
        return std::isfinite(y) ? y - root.ground.y : fallback;
    };
    for (int i = 0; i < 2; ++i) {
        const auto side = static_cast<std::size_t>(i);
        foot_probe_.ankle_ground[side] = ground_under(foot_setup_.ankle[side], 0.0f);
        foot_probe_.toe_ground[side] =
            ground_under(foot_setup_.toe[side], foot_probe_.ankle_ground[side]);
    }
    foot_probe_.valid = true;
    const anim::FootIkPlan tick_plan =
        anim::plan_foot_ik(skeleton_, foot_setup_, foot_probe_, tick_sample_);
    plan_ = tick_plan;
    const float k = dt > 0.0f ? 1.0f - std::exp(-dt / FOOT_IK_ROOT_TAU_S) : 1.0f;
    root_dy_ += (tick_plan.root_dy - root_dy_) * k;
}

bool SkinnedCharacter::playing_clips() const {
    // THE DOOR IS READ ONCE. A door that can change mid-run is a door that can
    // make the before and the after arms of a comparison differ by more than
    // the door (Rule 47).
    static const bool procedural = [] {
        const char* e = door_value("DFN_PROC_GAIT");
        return e != nullptr && *e == '1';
    }();
    return ready_ && !procedural && library_.has(anim::ClipRole::Idle);
}

void SkinnedCharacter::advance(const anim::Rig& rig, const anim::BodyDrive& drive,
                               const glm::vec3& standing_ground, float dt) {
    if (!ready_) {
        return;
    }
    anim::advance_playback(library_, drive, dt, play_);
    probe_ground(drive, standing_ground, dt);
    // THE PROCEDURAL PAIR, kept whether or not the door is open: it costs one
    // pose evaluation a tick and it is what makes DFN_PROC_GAIT switchable
    // without a second code path through this file.
    pose_prev_ = ticked_ ? pose_curr_ : anim::evaluate_body_pose(rig, drive);
    root_prev_ = ticked_ ? root_curr_ : anim::body_root_for(drive, standing_ground);
    pose_curr_ = anim::evaluate_body_pose(rig, drive);
    root_curr_ = anim::body_root_for(drive, standing_ground);
    // СКОЛЬКО РЕЕСТРА ПОВЕРХ КЛИПА. Ферма, а не второе решение: вес ведёт
    // update_bodies, здесь он только запоминается до кадра.
    pose_weight_ = std::clamp(drive.pose_weight, 0.0f, 1.0f);
    ticked_ = true;
}

render::RenderSystem::SkinnedDraw SkinnedCharacter::build_draw(const anim::Rig& rig,
                                                              bool hide_head,
                                                              float alpha) {
    render::RenderSystem::SkinnedDraw draw;
    if (!ready_) {
        return draw;
    }
    const float a = std::clamp(alpha, 0.0f, 1.0f);
    // THE POSE OF THIS FRAME, not of this tick. Both arms interpolate: the
    // clip arm by its own clip time (playback_sample), the procedural arm by
    // slerping the two ticks it was evaluated at -- which is exactly what
    // render does to a Transform, one level up from a matrix.
    if (!playing_clips()
        || !anim::playback_sample(skeleton_, binding_, clips_, library_, play_, a,
                                  sample_)) {
        const anim::LocalPose pose = anim::blend(pose_prev_, pose_curr_, a);
        anim::skinning_palette(rig, skeleton_, binding_, pose, palette_);
    } else {
        // THE FEET MEET THE GROUND THAT IS ACTUALLY THERE, on the frame's own
        // pose: the needs are re-measured here (the pose between two ticks is
        // not either tick's), while the ROOT shift comes from the tick, where
        // it was filtered. Applying an unfiltered shift per frame would put
        // the stair's whole rise into one frame at the nosing.
        if (foot_probe_.valid) {
            anim::FootIkPlan plan =
                anim::plan_foot_ik(skeleton_, foot_setup_, foot_probe_, sample_);
            plan.root_dy = root_dy_;
            if (ik_strength_ > 0.001f) {
                anim::apply_foot_ik(skeleton_, foot_setup_, foot_probe_, plan,
                                    ik_strength_, sample_);
            }
            // ПРИБОР МЕРИТ И КОГДА РЕШАТЕЛЬ ВЫКЛЮЧЕН, и это половина приёмки:
            // контрольная рука (DFN_FOOT_IK=0) обязана НАПЕЧАТАТЬ свои
            // сантиметры, иначе она молчит и её нечем предъявить.
            foot_trace_step(plan);
        }
        // ПОЗА РЕЕСТРА ПОВЕРХ СЭМПЛА КЛИПА, и это единственное место, где она
        // может встретить кадр: штатный путь гнёт тело клипами, а реестр
        // говорит на нашем пятнадцатикостном языке. Перевод делает тот же
        // ретаргет, которым живёт процедурная рука (pose_local_transforms), —
        // второго описания «где чей сустав» не заводится.
        //
        // ВЕС БЕРЁТСЯ ОДИН РАЗ, а не дважды: evaluate_body_pose уже смешала
        // позу с локомоцией по нему же, поэтому здесь смешивается СЭМПЛ КЛИПА
        // с уже смешанной позой. На единице это в точности поза, на нуле —
        // побитово прежний кадр, между — движение, приходящее чуть позже
        // середины. Возводить вес в квадрат ради формальной точности значило
        // бы сделать въезд вдвое вялее ради разницы, которой не видно.
        if (pose_weight_ > 0.0f) {
            pose_sample_.resize(skeleton_.size());
            const anim::LocalPose live = anim::blend(pose_prev_, pose_curr_, a);
            anim::pose_local_transforms(rig, skeleton_, binding_, live, pose_sample_);
            for (std::size_t j = 0; j < sample_.size() && j < pose_sample_.size(); ++j) {
                sample_[j].rotation =
                    glm::slerp(sample_[j].rotation, pose_sample_[j].rotation, pose_weight_);
                sample_[j].translation = glm::mix(sample_[j].translation,
                                                  pose_sample_[j].translation, pose_weight_);
            }
        }
        anim::sample_palette(skeleton_, sample_, palette_);
    }
    const anim::BodyRoot root{glm::mix(root_prev_.ground, root_curr_.ground, a),
                              root_prev_.yaw
                                  + a * shortest_turn(root_prev_.yaw, root_curr_.yaw)};
    if (hide_head) {
        // THE HEAD IS COLLAPSED, NOT CULLED, and that is the only option a
        // skinned mesh leaves: the head's triangles live in the same buffer as
        // the chest's, so there is no "head mesh" to omit the way the box body
        // omitted one. Scaling the joint to zero folds every vertex weighted to
        // it onto the joint's own point; the neck vertices that share weight
        // with the chest stretch a little, which is invisible from inside the
        // skull and is the whole audience for this branch.
        const int32_t head =
            binding_.names.joint[anim::bone_index(anim::Bone::Head)];
        if (head >= 0 && static_cast<std::size_t>(head) < palette_.size()) {
            palette_[static_cast<std::size_t>(head)] =
                glm::scale(glm::mat4{1.0f}, glm::vec3{0.0f});
        }
    }
    // The root places the model exactly as forward_kinematics places the boxes:
    // ground point + sim's yaw — and the yaw is NEGATED for the same reason it
    // is there (Pose.cpp): sim's convention is "yaw 0 faces -Z", which in a
    // right-handed frame is a rotation of -yaw about +Y. Getting this sign
    // wrong mirrors the walk instead of breaking it, so it would have been
    // believed. THE SAME TWO NUMBERS, so a skinned body and a
    // box body cannot stand in different places (which is what makes the
    // DFN_BODY_BOXES dose door an honest comparison, Rule 47).
    draw.transform = glm::translate(glm::mat4{1.0f}, root.ground)
                     * glm::rotate(glm::mat4{1.0f}, -root.yaw,
                                   glm::vec3{0.0f, 1.0f, 0.0f});
    draw.mesh_asset = SKINNED_CHARACTER_MESH_ID;
    draw.palette = palette_;
    // THE HITBOXES OF THIS FRAME, from the SAME sample the palette was built
    // from. Computing them from a second sample — the tick's, say — is how the
    // drawn body and the shootable body come apart by one frame at speed.
    hitbox_pose_ = anim::hitbox_pose(hitboxes_, skeleton_, binding_, sample_);
    // DFN_CHAR_TRACE=1: the posed body's extent IN METRES, once. A frame
    // cannot answer "how tall is he" — a small model close up and a large one
    // far away make the same pixels — and this wave lost an hour to exactly
    // that question about a figure that turned out to be correct.
    static const bool trace = [] {
        const char* e = door_value("DFN_CHAR_TRACE");
        return e != nullptr && *e == '1';
    }();
    if (trace) {
        static bool told = false;
        if (!told) {
            told = true;
            glm::vec3 lo{0.0f};
            glm::vec3 hi{0.0f};
            for (std::size_t i = 0; i < bind_vertices_.size(); ++i) {
                const glm::vec3 p =
                    anim::cpu_skin_position(bind_vertices_[i], palette_);
                lo = i == 0 ? p : glm::min(lo, p);
                hi = i == 0 ? p : glm::max(hi, p);
            }
            std::fprintf(stderr,
                         "[character] posed extent: %.3f x %.3f x %.3f m "
                         "(y from %.3f to %.3f), root (%.2f %.2f %.2f)\n",
                         static_cast<double>(hi.x - lo.x),
                         static_cast<double>(hi.y - lo.y),
                         static_cast<double>(hi.z - lo.z),
                         static_cast<double>(lo.y), static_cast<double>(hi.y),
                         static_cast<double>(root.ground.x),
                         static_cast<double>(root.ground.y),
                         static_cast<double>(root.ground.z));
        }
    }
    return draw;
}

bool SkinnedCharacter::blade_drawn() const {
    return blade_ready_ && playing_clips() && play_.weapon > 0.5f;
}

render::RenderSystem::SkinnedDraw SkinnedCharacter::blade_draw(
    const render::RenderSystem::SkinnedDraw& body) const {
    render::RenderSystem::SkinnedDraw draw = body;
    draw.mesh_asset = anim::HELD_BLADE_MESH_ID;
    return draw;
}

// ДВЕРЬ DFN_FOOT_TRACE. Читается на КАДРЕ, а не на тике, и это не мелочь:
// решатель применяется к позе кадра, поэтому «стоит ли стопа на грунте» —
// вопрос о кадре. `plan` передаётся тот, что померен ДО решателя (его веса и
// решают, какую стопу вообще судить, — контракт foot_penetration).
void SkinnedCharacter::foot_trace_step(const anim::FootIkPlan& plan) {
    if (!foot_trace_) {
        return;
    }
    const anim::FootGap gap =
        anim::foot_gap(skeleton_, foot_setup_, foot_probe_, plan, sample_);
    std::fprintf(stderr,
                 "[foot] %llu ik %.3f root %.4f | L ground a %.4f t %.4f w %.3f "
                 "gap %+.4f%s | R ground a %.4f t %.4f w %.3f gap %+.4f%s\n",
                 static_cast<unsigned long long>(foot_trace_frames_++),
                 static_cast<double>(ik_strength_), static_cast<double>(root_dy_),
                 static_cast<double>(foot_probe_.ankle_ground[0]),
                 static_cast<double>(foot_probe_.toe_ground[0]),
                 static_cast<double>(plan.weight[0]),
                 static_cast<double>(gap.gap[0]), gap.judged[0] != 0 ? "" : " (swing)",
                 static_cast<double>(foot_probe_.ankle_ground[1]),
                 static_cast<double>(foot_probe_.toe_ground[1]),
                 static_cast<double>(plan.weight[1]),
                 static_cast<double>(gap.gap[1]), gap.judged[1] != 0 ? "" : " (swing)");
}

} // namespace dfn::app
