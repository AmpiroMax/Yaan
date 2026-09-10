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

#include "engine/core/config/sources/Constants.h"

#include "engine/app/sources/AppDoors.h"
#include "engine/app/sources/CharacterTextures.h"
#include "engine/anim/sources/RestFit.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
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

/// ОПОРА ПО РАСПИСАНИЮ КЛИПА: фаза внутри [постановка, отрыв) любого окна;
/// окно «полный круг» (покой: постановка == отрыв) — всегда.
[[nodiscard]] bool schedule_planted(const anim::ClipEntry& e, std::size_t side, float ph) {
    bool planted = false;
    for (uint8_t i = 0; i < e.plant_count[side]; ++i) {
        const float p = e.plant_phase[side][i];
        const float l = e.lift_phase[side][i];
        planted = planted || (p == l) || (p < l ? (ph >= p && ph < l) : (ph >= p || ph < l));
    }
    return planted;
}

/// How fast the whole solve fades in and out (a jump, sitting down). Its own
/// number: it gates a MECHANISM rather than tracks a surface, and it wants to
/// be off before the take-off frame rather than a tick after it.
constexpr float FOOT_IK_GATE_TAU_S = 0.06f;

} // namespace

void scale_registry_object(render::RegistryObject& obj, float k) {
    if (std::fabs(k - 1.0f) < 1e-6f) {
        return;
    }
    for (platform::SkinnedVertex& v : obj.skin.vertices) {
        v.position *= k;
    }
    for (skel::SkeletonJoint& j : obj.skeleton.joints) {
        j.bind_translation *= k;
        // IB' = S_k · IB · S_k⁻¹: строка переносов умножается на k, а
        // ЛИНЕЙНАЯ часть остаётся — сопряжение однородным масштабом её не
        // трогает.
        j.inverse_bind[3] = glm::vec4(glm::vec3(j.inverse_bind[3]) * k, 1.0f);
    }
    for (skel::AnimClip& clip : obj.clips) {
        for (skel::AnimChannel& ch : clip.channels) {
            if (ch.path != skel::AnimPath::Translation) {
                continue;
            }
            for (glm::vec4& value : ch.values) {
                value.x *= k;
                value.y *= k;
                value.z *= k;
            }
        }
    }
    // СОКЕТЫ И ЧАСТИ ЗНАЮТ ПРО МЕТРЫ ТОЧНО ТАК ЖЕ: точка сокета — вершина с
    // одним весом, вершины части — вершины.
    for (render::Socket& sock : obj.sockets) {
        sock.rest_point *= k;
    }
    for (render::SkinPart& part : obj.parts) {
        for (platform::SkinnedVertex& v : part.mesh.vertices) {
            v.position *= k;
        }
    }
}

bool SkinnedCharacter::load(render::RenderSystem& render_system,
                            platform::IRenderer& renderer, const anim::Rig& rig,
                            const std::filesystem::path& path, bool legacy_rest,
                            uint32_t mesh_asset, uint32_t blade_asset) {
    if (!std::filesystem::exists(path)) {
        std::fprintf(stderr,
                     "[character] \"%s\" is not there — the body stays as the "
                     "fifteen boxes. Bake it with dfn_import_gltf (the CMake "
                     "target dfn_characters does it).\n",
                     path.string().c_str());
        return false;
    }
    auto obj = render::read_object(path);
    if (!obj.has_value()) {
        std::fprintf(stderr, "[character] \"%s\" refused by the registry\n",
                     path.string().c_str());
        return false;
    }
    return load_object(render_system, renderer, rig, std::move(*obj), path, legacy_rest,
                       mesh_asset, blade_asset);
}

bool SkinnedCharacter::load_object(render::RenderSystem& render_system,
                                   platform::IRenderer& renderer, const anim::Rig& rig,
                                   render::RegistryObject object,
                                   const std::filesystem::path& path, bool legacy_rest,
                                   uint32_t mesh_asset, uint32_t blade_asset) {
    // A SECOND LOAD GIVES THE FIRST ONE'S IDS BACK FIRST: the screen rebuilds
    // its character on every settled slider, and a register on an occupied id
    // is refused out loud (RenderSystem), which is right for two owners and
    // wrong for one owner twice.
    if (ready_) {
        release(render_system, renderer);
    }
    mesh_asset_ = mesh_asset;
    blade_asset_ = blade_asset;
    const render::RegistryObject* obj = &object;
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
    // THE REST POSE IS SOLVED ON THIS SKIN (owner's order 02.09): legs
    // vertical under the hips, arms along the sides at the abduction that
    // clears the thighs by REST_GAP_*. The app's rig supplies proportions
    // only; a rest right for the box body crossed this model's ankles.
    {
        const anim::RestFit fit = anim::fit_rest_pose(
            rig.proportions, skeleton_, obj->skin.vertices,
            anim::BodyGapTargets::from_config(), legacy_rest);
        rig_ = fit.rig;
        std::fprintf(stderr,
                     "[character] rest pose%s: leg splay %.1f deg, arm abduction %.1f "
                     "deg, elbow %.1f deg, %u passes, %s; %s\n",
                     legacy_rest ? " (LEGACY, the box body's)" : "",
                     static_cast<double>(rig_.stance.leg_splay_rad * 57.29578f),
                     static_cast<double>(rig_.stance.arm_abduction_rad * 57.29578f),
                     static_cast<double>(rig_.stance.elbow_flex_rad * 57.29578f),
                     fit.passes, fit.met ? "REST_GAP_* met" : "REST_GAP_* NOT MET",
                     anim::describe_gaps(fit.gaps).c_str());
    }
    binding_ = anim::bind_skinned_rig(rig_, skeleton_);
    if (binding_.bound_count() == 0) {
        std::fprintf(stderr,
                     "[character] \"%s\": not one joint name maps to a rig bone "
                     "— it would draw its bind pose forever. REFUSED; see the "
                     "synonym table in engine/anim/sources/BoneMap.cpp\n",
                     path.string().c_str());
        return false;
    }
    if (!render_system.register_skinned_mesh(renderer, mesh_asset_, obj->skin.vertices,
                                             obj->skin.indices)) {
        return false;
    }
    // КОЖА: секция TEX → PNG → GPU с мипами, кэш по sha (один лист на мир,
    // экран и смотровую). Ноль — палитра вершин, и каждая причина нуля уже
    // сказана вслух там, где она найдена.
    texture_asset_ = body_albedo_asset(render_system, renderer, *obj, path);
    normal_asset_ = body_normal_asset(render_system, renderer, *obj, path);
    bind_vertices_ = obj->skin.vertices;
    skin_indices_ = obj->skin.indices;
    draw_indices_ = obj->skin.indices; // ничего не закрыто, пока нет частей
    sockets_ = obj->sockets;
    morphs_ = obj->morphs;
    morph_.resize_for(morphs_);
    source_path_ = path;
    source_object_ = *obj;
    name_ = obj->name;
    triangles_ = obj->skin.indices.size() / 3;
    palette_.assign(skeleton_.size(), glm::mat4{1.0f});
    sample_.assign(skeleton_.size(), anim::JointLocal{});
    {
        const char* tr = door_value("DFN_CLIP_TRANSITIONS");
        transitions_ = !(tr != nullptr && tr[0] == '0');
        const char* in = door_value("DFN_CLIP_INERTIAL");
        inertial_on_ = !(in != nullptr && in[0] == '0');
        const char* rt = door_value("DFN_ROOT_TRACK");
        root_track_ = !(rt != nullptr && rt[0] == '0');
        const char* rh = door_value("DFN_ROOT_HEIGHT");
        root_height_feet_ = !(rh != nullptr && std::string_view{rh} == "capsule");
    }
    const char* roles = door_value("DFN_CLIP_ROLES");
    library_ = anim::build_clip_library(rig_, skeleton_, binding_, clips_, bind_vertices_,
                                        roles != nullptr ? std::string_view{roles}
                                                         : std::string_view{});
    library_.transitions = transitions_;
    library_.inertial = inertial_on_;
    if (const char* is = door_value("DFN_IDLE_SYMMETRY"); is != nullptr && is[0] == '0') {
        library_.idle_symmetry = 0.0f;
    }
    foot_setup_ = anim::build_foot_ik(skeleton_, binding_, library_.contacts);
    {
        const char* lh = door_value("DFN_LOCO_HUD");
        const char* lc = door_value("DFN_LOCO_CSV");
        if ((lh != nullptr && lh[0] == '1') || (lc != nullptr && lc[0] != '\0')) {
            set_telemetry(true, lc != nullptr ? std::string{lc} : std::string{});
        }
    }
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
    // ХИТБОКСЫ ПО КОЖЕ ЭТОГО ТЕЛА, а не по канону (решение владельца 01.09:
    // отгружается СЫРОЕ тело, у которого бедро на треть толще канона, а талия
    // на пятую часть уже). Луч, спрошенный «во что попал», обязан спрашивать
    // про то тело, которое нарисовано.
    hitboxes_ = anim::build_hitboxes(rig_.proportions);
    anim::fit_hitboxes_to_skin(hitboxes_, rig_, skeleton_, binding_, bind_vertices_);
    // THE BOXES START AT THE REST POSE, so a caller that creates the Jolt
    // bodies right after load() has a pose to place them by — before any frame
    // was drawn. build_draw overwrites it every frame.
    {
        std::vector<anim::JointLocal> rest(skeleton_.size());
        anim::pose_local_transforms(rig_, skeleton_, binding_, anim::LocalPose{}, rest);
        hitbox_pose_ = anim::hitbox_pose(hitboxes_, skeleton_, binding_, rest);
    }
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
    } else if (!render_system.register_skinned_mesh(renderer, blade_asset_,
                                                    blade_.vertices, blade_.indices)) {
        std::fprintf(stderr,
                     "[character] the blade mesh (id %u) was refused by the registry "
                     "— the guard pose will play over an EMPTY hand\n",
                     blade_asset_);
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
        if (e.root.valid && e.root.mps > 0.0f) {
            std::fprintf(stderr, ", %.2fm/s, plant %.2f", static_cast<double>(e.root.mps),
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
    if (!render_system.replace_skinned_mesh(renderer, mesh_asset_, morphed_,
                                            draw_indices_)) {
        // ГРЯЗНЫМ ОСТАВЛЯЕМ НАРОЧНО: неудачная перекладка — это тело, которое
        // на экране не то, что в состоянии, и следующий вызов обязан
        // попробовать ещё раз, а не считать себя сделанным.
        return false;
    }
    morph_dirty_ = false;
    // ЧАСТИ — ЗА ТЕЛОМ, ТЕМ ЖЕ ДВИЖЕНИЕМ РУЧКИ: волосы над сдвинутым лбом, глаза
    // в уехавшей глазнице. Отказ перекладки части сказан вслух там, а тело
    // считается сделанным — часть, отставшая на кадр, лучше тела, повторяющего
    // бленд каждый кадр.
    (void)parts_.follow(render_system, renderer, morphed_);
    return true;
}

void SkinnedCharacter::release(render::RenderSystem& render_system,
                               platform::IRenderer& renderer) {
    if (!ready_) {
        return;
    }
    (void)render_system.drop_skinned_mesh(renderer, mesh_asset_);
    if (blade_ready_) {
        (void)render_system.drop_skinned_mesh(renderer, blade_asset_);
    }
    parts_.release(render_system, renderer);
    draw_indices_.clear();
    sockets_.clear();
    blade_ready_ = false;
    ready_ = false;
    ticked_ = false;
    play_ = anim::ClipPlayback{};
    morph_dirty_ = false;
    morphed_.clear();
}

void SkinnedCharacter::rest_positions(std::vector<glm::vec3>& out) const {
    out.clear();
    if (!ready_) {
        return;
    }
    std::vector<glm::mat4> palette(skeleton_.size());
    anim::skinning_palette(rig_, skeleton_, binding_, anim::LocalPose{}, palette);
    const std::vector<platform::SkinnedVertex>& verts = current_vertices();
    out.reserve(verts.size());
    for (const platform::SkinnedVertex& v : verts) {
        out.push_back(anim::cpu_skin_position(v, palette));
    }
}

bool SkinnedCharacter::replace_vertices(render::RenderSystem& render_system,
                                        platform::IRenderer& renderer,
                                        std::span<const platform::SkinnedVertex> vertices) {
    if (!ready_ || vertices.size() != bind_vertices_.size()) {
        return false;
    }
    if (!render_system.replace_skinned_mesh(renderer, mesh_asset_, vertices,
                                            draw_indices_)) {
        return false;
    }
    bind_vertices_.assign(vertices.begin(), vertices.end());
    morphed_.clear();
    morph_dirty_ = false;
    // Части — за новой кожей (экран создания тянет ползунок этим путём).
    (void)parts_.follow(render_system, renderer, bind_vertices_);
    // THE BOXES FOLLOW THE FLESH: a wider hip is a wider hip box, on the same
    // rest the rest of the table was fitted on.
    hitboxes_ = anim::build_hitboxes(rig_.proportions);
    anim::fit_hitboxes_to_skin(hitboxes_, rig_, skeleton_, binding_, bind_vertices_);
    return true;
}

bool SkinnedCharacter::attach_parts(render::RenderSystem& render_system,
                                    platform::IRenderer& renderer,
                                    const std::filesystem::path& path,
                                    uint32_t first_mesh_id, uint32_t max_parts,
                                    const char* selection) {
    if (!ready_) {
        std::fprintf(stderr, "[parts] \"%s\": тело не загружено — части не к чему "
                             "крепить\n", path.string().c_str());
        return false;
    }
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        std::fprintf(stderr, "[parts] \"%s\": файла нет — части не прикреплены (печёт "
                             "цель dfn_characters)\n", path.string().c_str());
        return false;
    }
    const auto obj = render::read_object(path);
    if (!obj.has_value()) {
        std::fprintf(stderr, "[parts] \"%s\": отвергнут реестром — части не "
                             "прикреплены\n", path.string().c_str());
        return false;
    }
    // НЕЙТРАЛЬ ДЛЯ СЛЕДОВАНИЯ — ТЕЛО, К КОТОРОМУ ЧАСТИ ПЕЧЕНЫ (--like
    // <тело>.dfo рядом с <тело>.parts.dfo), ИЗ ФАЙЛА и в масштабе файла:
    // множитель по костям части приложат сами. Не из памяти этого тела —
    // оно могло прийти уже вылепленным (экран после settle, мир из выпечки)
    // или масштабированным ростом (scale_registry_object), и «это тело и
    // есть нейтраль» не проверить ничем. Файл читается один раз на процесс.
    std::span<const platform::SkinnedVertex> neutral;
    const std::filesystem::path neutral_path = neutral_body_path(path);
    if (neutral_path.empty()) {
        std::fprintf(stderr, "[parts] \"%s\": не <тело>.parts/.clothes.dfo — нейтраль "
                             "неизвестна, части морфам не следуют\n",
                     path.string().c_str());
    } else if (const auto* cached = neutral_skin_cached(neutral_path); cached != nullptr) {
        neutral = *cached;
    }
    if (!neutral.empty() && neutral.size() != bind_vertices_.size()) {
        std::fprintf(stderr,
                     "[parts] \"%s\": нейтраль %s на %zu вершин против %zu у тела — не то "
                     "тело, части морфам не следуют\n",
                     path.string().c_str(), neutral_path.string().c_str(), neutral.size(),
                     bind_vertices_.size());
        neutral = {};
    }
    if (!parts_.attach(render_system, renderer, *obj, path, skeleton_, first_mesh_id,
                       max_parts, selection, neutral,
                       neutral.empty() ? std::span<const uint32_t>{} : skin_indices_,
                       face_masks_cached())) {
        return false;
    }
    // Тело могло прийти уже вылепленным (экран после settle, мир из выпечки):
    // части садятся на него сразу, а не при первом движении ручки.
    (void)parts_.follow(render_system, renderer, current_vertices());
    return rebuild_draw_indices(render_system, renderer);
}

std::filesystem::path SkinnedCharacter::neutral_body_path(const std::filesystem::path& parts) {
    std::string stem = parts.stem().string();
    for (const char* suffix : {".parts", ".clothes"}) {
        const std::size_t n = std::strlen(suffix);
        if (stem.size() > n && stem.compare(stem.size() - n, n, suffix) == 0) {
            stem.erase(stem.size() - n);
            std::filesystem::path out = parts;
            out.replace_filename(stem + parts.extension().string());
            return out;
        }
    }
    return {};
}

const std::vector<platform::SkinnedVertex>*
SkinnedCharacter::neutral_skin_cached(const std::filesystem::path& path) {
    // КЭШ НА ПРОЦЕСС, по пути: экран перестраивает тело на каждом отпускании
    // ручки, и читать 5 МБ нейтрали на каждый settle — 30 мс, которых у
    // settle нет. Нейтраль неизменна, пока жив процесс (перепечка — новый
    // запуск), поэтому кэш без инвалидации честен.
    static std::map<std::string, std::unique_ptr<std::vector<platform::SkinnedVertex>>>
        cache;
    const std::string key = path.string();
    if (const auto it = cache.find(key); it != cache.end()) {
        return it->second.get();
    }
    std::unique_ptr<std::vector<platform::SkinnedVertex>> skin;
    if (const auto obj = render::read_object(path); obj.has_value() && !obj->skin.empty()) {
        skin = std::make_unique<std::vector<platform::SkinnedVertex>>(obj->skin.vertices);
        std::fprintf(stderr, "[parts] нейтраль частей: \"%s\", %zu вершин (кэш процесса)\n",
                     key.c_str(), skin->size());
    } else {
        std::fprintf(stderr, "[parts] нейтраль частей \"%s\" не прочиталась — части "
                             "морфам не следуют\n", key.c_str());
    }
    return cache.emplace(key, std::move(skin)).first->second.get();
}

const FaceMasks* SkinnedCharacter::face_masks_cached() {
    static const std::unique_ptr<FaceMasks> masks = [] {
        auto m = std::make_unique<FaceMasks>();
        if (!read_face_masks(FACE_MASKS_PATH, *m)) {
            return std::unique_ptr<FaceMasks>{};
        }
        std::fprintf(stderr, "[parts] маски лица: %zu областей на %u вершин\n",
                     m->masks.size(), m->verts);
        return m;
    }();
    return masks.get();
}

bool SkinnedCharacter::rebuild_draw_indices(render::RenderSystem& render_system,
                                            platform::IRenderer& renderer) {
    const std::vector<uint32_t> hidden = parts_.hidden_body_vertices();
    std::vector<uint32_t> next;
    if (hidden.empty()) {
        next = skin_indices_;
    } else {
        // ТРЕУГОЛЬНИК СНИМАЕТСЯ, ТОЛЬКО ЕСЛИ ЗАКРЫТЫ ВСЕ ТРИ ЕГО ВЕРШИНЫ: кромка
        // костюма проходит по треугольникам с одной открытой вершиной, и снять
        // их значило бы показать дыру между рукавом и кистью.
        next.reserve(skin_indices_.size());
        const auto covered = [&hidden](uint32_t i) {
            return std::binary_search(hidden.begin(), hidden.end(), i);
        };
        for (std::size_t t = 0; t + 2 < skin_indices_.size(); t += 3) {
            if (covered(skin_indices_[t]) && covered(skin_indices_[t + 1])
                && covered(skin_indices_[t + 2])) {
                continue;
            }
            next.push_back(skin_indices_[t]);
            next.push_back(skin_indices_[t + 1]);
            next.push_back(skin_indices_[t + 2]);
        }
    }
    if (next.size() == draw_indices_.size()) {
        draw_indices_ = std::move(next);
        return true; // ничего не изменилось — тело на GPU уже такое
    }
    if (!render_system.replace_skinned_mesh(renderer, mesh_asset_, current_vertices(),
                                            next)) {
        std::fprintf(stderr, "[parts] тело «%s»: перекладка индексов без закрытых "
                             "треугольников не удалась — кожа под костюмом останется\n",
                     name_.c_str());
        return false;
    }
    std::fprintf(stderr,
                 "[parts] тело «%s»: закрыто %zu вершин, снято %zu из %zu треугольников "
                 "тела\n",
                 name_.c_str(), hidden.size(), (skin_indices_.size() - next.size()) / 3,
                 skin_indices_.size() / 3);
    draw_indices_ = std::move(next);
    return true;
}

bool SkinnedCharacter::socket_frame(std::string_view name, glm::mat4& out) const {
    for (const render::Socket& sock : sockets_) {
        if (sock.name != name) {
            continue;
        }
        const glm::mat4 bone = sock.joint < palette_.size()
                                   ? palette_[sock.joint]
                                   : glm::mat4{1.0f};
        out = bone * glm::translate(glm::mat4{1.0f}, sock.rest_point);
        return true;
    }
    return false;
}

bool SkinnedCharacter::bake_morphs(const std::filesystem::path& out, float scale) const {
    // ПИШЕМ ОБЪЕКТ, КАКИМ ОН ПРИШЁЛ, а не то, с чем работает класс: в работе у
    // нас скин, скелет и клипы — но НЕ материалы кусков, не происхождение и
    // не всё, что читатель мог принести; выпечка, потерявшая половину объекта,
    // была бы «почти тем же телом», а такого не бывает. Копия держится с
    // загрузки, а не перечитывается: тело экрана может быть собрано из
    // памяти, и файла у него нет.
    if (!ready_ || source_object_.skin.empty()) {
        std::fprintf(stderr, "[character] выпечка: тело не загружено (%s)\n",
                     source_path_.string().c_str());
        return false;
    }
    render::RegistryObject baked = source_object_;
    std::vector<platform::SkinnedVertex> blended;
    render::blend_morphs(baked.skin.vertices, baked.morphs, morph_.weights,
                         baked.skin.indices, blended);
    baked.skin.vertices = std::move(blended);
    scale_registry_object(baked, scale);
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
    // ПРЫЖОК КАПСУЛЫ НА СТУПЕНЬ ГАСИТСЯ КОРНЕМ СРАЗУ, а не фильтром: капсула
    // Jolt встаёт на проступь за один тик (0.18 м), фильтр подъёма корня
    // (FOOT_IK_ROOT_TAU_S) догоняет за пять — и пять тиков опорная стопа
    // парила над своей ступенью на 20 см (прибор app_grounded_locomotion,
    // синтетический марш). Корень вычитает прыжок в тот же тик, тело
    // остаётся, где было, и уже плавно поднимается за планом.
    if (ticked_ && dt > 0.0f) {
        const float vy = (standing_ground.y - last_ground_y_) / dt;
        ground_vy_mps_ += (vy - ground_vy_mps_) * gate_k;
        // гистерезис: вниз быстрее 0,1 м/с — спуск; вверх/ровно — конец спуска
        if (ground_vy_mps_ < -0.1f) {
            capsule_descending_ = true;
        } else if (ground_vy_mps_ > -0.02f) {
            capsule_descending_ = false;
        }
    }
    const bool feet_own = root_height_feet_ && loco_active_ && !capsule_descending_;
    if (ticked_ && !feet_own) {
        const float jump = standing_ground.y - last_ground_y_;
        if (std::abs(jump) > 0.02f) {
            root_dy_ = std::clamp(root_dy_ - jump, -anim::FOOT_IK_ROOT_LIMIT_M,
                                  anim::FOOT_IK_ROOT_LIMIT_M);
        }
    }
    last_ground_y_ = standing_ground.y;
    foot_probe_.valid = false;
    if (!ground_probe_ || !foot_setup_.valid() || !playing_clips()) {
        root_dy_ += (0.0f - root_dy_) * gate_k;
        has_root_world_ = false;
        return;
    }
    // THE POSE THIS TICK ENDED ON, which is where the rays go from. The frame
    // will re-measure the needs against its own interpolated pose; only the
    // GROUND is sampled here, because a raycast is the expensive half and the
    // ground under a foot does not change inside one tick.
    if (!tick_sampled_) {
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
        // ОДНА СТУПЕНЬ НА ОПОРУ: земля поставленной по расписанию стопы берётся
        // на постановке и держится до отрыва (в мире; сюда — относительно
        // земли капсулы этого тика). Без дорожки/машины — как прежде, щуп.
        if (feet_own) {
            if (height_owner_[side]) {
                if (!ground_frozen_[side]) {
                    frozen_ankle_ground_[side] = root.ground.y + foot_probe_.ankle_ground[side];
                    frozen_toe_ground_[side] = root.ground.y + foot_probe_.toe_ground[side];
                    ground_frozen_[side] = true;
                }
                foot_probe_.ankle_ground[side] = frozen_ankle_ground_[side] - root.ground.y;
                foot_probe_.toe_ground[side] = frozen_toe_ground_[side] - root.ground.y;
            } else {
                ground_frozen_[side] = false;
            }
        } else {
            ground_frozen_[side] = false;
        }
    }
    foot_probe_.valid = true;
    anim::FootIkPlan tick_plan =
        anim::plan_foot_ik(skeleton_, foot_setup_, foot_probe_, tick_sample_);
    plan_ = tick_plan;
    const float k = dt > 0.0f ? 1.0f - std::exp(-dt / FOOT_IK_ROOT_TAU_S) : 1.0f;
    // РАМПА БЕЗ ЛАГА, СКАЧОК — ЧЕРЕЗ ФИЛЬТР (склоны, 07.09): на подъёме план
    // опускания корня — непрерывная рампа (опорная стопа уходит всё ниже
    // капсулы, −7 мм за тик), и фильтр первого порядка отстаёт от рампы на
    // τ·скорость = 0,08 с × 0,42 м/с = 34 мм — прибор показывал парение 17 мм
    // (MX_Walking — 49). Изменение плана до FOOT_IK_ROOT_RAMP_MPS корень
    // берёт сразу; больше (смена опорной стопы) — сглаживается, как прежде.
    // (Пробовано 07.09: на тике скачка капсулы ставить корень ровно в план —
    // проникание на марше выросло 13,5 → 25,7 мм: план тика не то, что нужно
    // кадру между тиками. Оставлено вычитание скачка + фильтр.)
    if (feet_own) {
        // ЧЬЯ ЗЕМЛЯ ВЕДЁТ ВЫСОТУ — ОПОРНАЯ СТОПА ПО РАСПИСАНИЮ КЛИПА, не по
        // высоте стопы в клипе: на лестнице маховая стопа плоской ходьбы идёт у
        // своей высоты покоя и по модели «стоит», и план тянул тело к земле под
        // ней (на спуске — вниз к нижней ступени, пока опорная ещё наверху:
        // парение 26 см). Двойная опора — среднее двух земель.
        const std::array<float, 2> need = tick_plan.need;
        if (height_owner_[0] && height_owner_[1]) {
            tick_plan.root_dy = 0.5f * (need[0] + need[1]);
        } else if (height_owner_[0]) {
            tick_plan.root_dy = need[0];
        } else if (height_owner_[1]) {
            tick_plan.root_dy = need[1];
        }
        tick_plan.root_dy = std::clamp(tick_plan.root_dy, -anim::FOOT_IK_ROOT_LIMIT_M,
                                       anim::FOOT_IK_ROOT_LIMIT_M);
        plan_ = tick_plan;
    }
    if (!feet_own) {
        has_root_world_ = false;
    }
    if (feet_own) {
        // ЛЕСТНИЦА (§16.6, 11.09): хозяин высоты рисуемого тела — земля под
        // опорной стопой, а не капсула. Капсула (радиус больше проступи)
        // въезжает на подступёнок раньше стопы и поднимается плавно 0,8 м/с;
        // фильтр ОТНОСИТЕЛЬНО капсулы отставал от этого подъёма, и задняя
        // стопа парила до 41 см. Цель держится в МИРЕ: план (взвешенная земля
        // под опорной стопой) + земля капсулы = мировая высота корня; к ней
        // идём не быстрее ROOT_HEIGHT_RATE_MPS (смена опорной стопы на ступень
        // — прыжок цели на подступёнок за двойную опору) с тем же фильтром;
        // смещение от капсулы — разность ТОЧНО за тик, подъём капсулы в него
        // не входит. Контроль — DFN_ROOT_HEIGHT=capsule (ветка ниже).
        const float target_world = root.ground.y + tick_plan.root_dy;
        if (!has_root_world_) {
            root_world_y_ = target_world;
            has_root_world_ = true;
        }
        const float rate_cap = static_cast<float>(config::ROOT_HEIGHT_RATE_MPS) * dt;
        const float toward = (target_world - root_world_y_) * k;
        root_world_y_ += std::clamp(toward, -rate_cap, rate_cap);
        root_dy_ = std::clamp(root_world_y_ - root.ground.y, -anim::FOOT_IK_ROOT_LIMIT_M,
                              anim::FOOT_IK_ROOT_LIMIT_M);
        root_world_y_ = root.ground.y + root_dy_;
        plan_root_dy_prev_ = tick_plan.root_dy;
        has_plan_root_dy_ = true;
        return;
    }
    if (has_plan_root_dy_) {
        const float ramp_cap = static_cast<float>(config::FOOT_IK_ROOT_RAMP_MPS) * dt;
        const float d = tick_plan.root_dy - plan_root_dy_prev_;
        root_dy_ += std::clamp(d, -ramp_cap, ramp_cap);
    }
    plan_root_dy_prev_ = tick_plan.root_dy;
    has_plan_root_dy_ = true;
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

void SkinnedCharacter::advance(const anim::BodyDrive& drive,
                               const glm::vec3& standing_ground, float dt) {
    if (!ready_) {
        return;
    }
    const anim::Rig& rig = rig_;
    // ФАКТИЧЕСКИЙ ХОД КОРНЯ ЗА ПРОШЛЫЙ ТИК (§11.1): подтверждённый корень
    // прошлого тика минус позапрошлого; по нему идут часы клипа на ходу.
    anim::BodyDrive drive_p = drive;
    if (ticked_) {
        const glm::vec3 d = root_curr_.ground - root_prev_.ground;
        drive_p.travelled_m = glm::length(glm::vec2{d.x, d.z});
    }
    const anim::BodyDrive& drive_ref = drive_p;
    // МАШИНА ЛОКОМОЦИИ (§16, фаза 3): наземный ход — роль и часы от машины;
    // присед, посадка, плавание — прежний выбор по состоянию привода.
    loco_active_ = root_track_ && drive.grounded && drive.posture_blend < 0.5f
                   && drive.crouch_blend < 0.5f && library_.has(anim::ClipRole::Idle);
    if (loco_active_) {
        anim::LocoInput in;
        in.want_dir_model = drive.move_dir_model;
        in.want_speed_mps = drive.want_speed_mps;
        in.gait = drive.gait;
        in.view_yaw = drive.view_yaw;
        in.body_yaw = drive.facing_yaw;
        in.view_valid = drive.view_valid;
        in.grounded = drive.grounded;
        in.push_mps = glm::length(glm::vec2{drive.push_mps_model.x, drive.push_mps_model.z});
        anim::loco_step(library_, in, dt, loco_m_);
        const anim::ClipEntry& e = library_[loco_m_.role];
        for (std::size_t side = 0; side < 2; ++side) {
            sched_planted_[side] = schedule_planted(e, side, loco_m_.phase);
            height_owner_[side] = foot_phys_[side].sensed ? foot_phys_[side].planted
                                                          : sched_planted_[side];
        }
    } else {
        sched_planted_ = {false, false};
        height_owner_ = {false, false};
        if (root_track_) {
            loco_m_ = anim::LocoMachine{}; // вернёмся на землю — с покоя
        }
    }
    anim::advance_playback(library_, drive_ref, dt, play_, loco_active_ ? &loco_m_ : nullptr);
    tick_sample_.resize(skeleton_.size());
    tick_sampled_ = playing_clips()
                    && anim::playback_sample(skeleton_, binding_, clips_, library_, play_,
                                             1.0f, tick_sample_);
    // ИНЕРЦИАЛИЗАЦИЯ СТЫКА (§13.7): роль сменилась срезом — разница «прошлая
    // показанная поза − новая» со скоростью прошлой снимается и гасится за
    // INERTIAL_BLEND_S; кадр читает остаток по своему времени.
    inertial_dt_ = dt;
    // ВЕС УХОДЯЩЕЙ ПОЗЫ ДО НОВОГО ЗАХВАТА: остаток контрвращения прошлого
    // клипа гаснет этим весом; брать вес ПОСЛЕ захвата (≈1) значило бы
    // копить остаток от поворота к повороту — замер 11.09: точка опоры
    // «ехала» до 25 м/с на каждом стыке, всё сильнее с каждым циклом сценария.
    const float frozen_w_prev = turn_frozen_w();
    // РЫСК ТАЗА — С ЧИСТОЙ ПОЗЫ КЛИПА, до наложения остатка стыка: угол
    // поворота принадлежит клипу; остаток стыка — картинке, он гаснет сам, и
    // приписать его повороту значит довернуть тело на разницу поз покоя и
    // клипа (замер 07.09: после остановки бега тело стреляло вторым
    // поворотом).
    if (tick_sampled_ && library_.inertial) {
        const std::size_t n = tick_sample_.size();
        if (play_.switched && shown_prev_.size() == n && shown_prev2_.size() == n) {
            inertial_.capture(shown_prev2_, shown_prev_, tick_sample_, dt,
                              static_cast<float>(config::INERTIAL_BLEND_S));
        }
        inertial_.advance(dt);
        inertial_.apply(inertial_.time(), tick_sample_);
        shown_prev2_.swap(shown_prev_);
        shown_prev_ = tick_sample_;
    } else {
        shown_prev_.clear();
        shown_prev2_.clear();
    }
    // ОСТАТОК ВАРПА ПОВОРОТА НА МЕСТЕ (§16.4): рыск тика берётся из дорожки
    // корня (loco_ ниже), а на позу идёт контрвращение на «варп − 1» от
    // пройденного рыска клипа — нарисованный корпус смотрит туда же, куда
    // едет капсула. Контрвращение прошлого тика снято в конце прошлого
    // advance; ушедший клип поворота гаснет весом уходящей позы
    // (turn_frozen_), чтобы стык не дёрнул корпус.
    turn_counter_prev_rad_ = turn_counter_end_rad_;
    if (play_.switched) {
        turn_frozen_rad_ = turn_frozen_rad_ * frozen_w_prev
                           + (anim::transit_role(play_.previous) ? turn_accum_rad_ : 0.0f);
        turn_accum_rad_ = 0.0f;
    }
    if (loco_active_ && loco_m_.state == anim::LocoState::TurnInPlace) {
        const anim::ClipEntry& cur_ent = library_[loco_m_.role];
        if (cur_ent.root.valid) {
            turn_accum_rad_ = -(loco_m_.turn_warp - 1.0f)
                              * anim::root_track_yaw_at(cur_ent.root, loco_m_.phase);
        }
    }
    if (tick_sampled_ && turn_counter_rad() != 0.0f) {
        anim::rotate_root_joints(skeleton_, foot_setup_.roots, turn_counter_rad(), tick_sample_);
    }
    leg_warp_prev_rad_ = leg_warp_rad_;
    {
        // ПОВОРОТ НОГ К ВВОДУ ВЫКЛЮЧЕН НА ДОРОЖКЕ КОРНЯ (§16.7): капсула едет
        // ровно по дорожке вдоль корпуса, и повёрнутые к диагонали ноги шли
        // бы по земле вбок; диагональ — доворот корпуса (сим) или класс
        // направления (машина). Механизм warp_legs оставлен с дозой 0 —
        // прибор восьми направлений решит его судьбу.
        const float want_warp = 0.0f;
        const float tau = static_cast<float>(config::LEG_WARP_SMOOTH_S);
        const float k = (tau > 0.0f && dt > 0.0f) ? 1.0f - std::exp(-dt / tau) : 1.0f;
        leg_warp_rad_ += (want_warp - leg_warp_rad_) * k;
    }
    if (tick_sampled_ && foot_setup_.valid()) {
        anim::warp_legs(skeleton_, foot_setup_, leg_warp_rad_, tick_sample_);
    }
    probe_ground(drive, standing_ground, dt);
    // КОНТАКТЫ ЭТОГО ТИКА. Без луча в мир (смотровая, тесты) вес опоры
    // читается по плоскому грунту — та же поза и тот же вес, что у стенда;
    // заявка от стопы не зависит от того, есть ли кому спросить высоту.
    contact_curr_ = anim::ContactState{};
    if (tick_sampled_ && foot_setup_.valid()) {
        anim::FootIkProbe flat;
        flat.valid = true;
        const anim::FootIkPlan plan = anim::plan_foot_ik(
            skeleton_, foot_setup_, foot_probe_.valid ? foot_probe_ : flat, tick_sample_);
        contact_curr_ = anim::contact_state(skeleton_, foot_setup_, plan, tick_sample_);
    }
    loco_ = anim::LocomotionOut{};
    if (loco_active_) {
        // ОДИН ХОЗЯИН ДВИЖЕНИЯ (§16): ход и рыск за тик — из дорожки корня
        // текущего клипа; никакого интегратора по стопам, сглаживания и
        // модели скорости. Постановки — из расписания контактов клипа.
        const anim::RootDelta rd = anim::loco_root_delta(library_, loco_m_);
        loco_.root_delta_model = glm::vec3{rd.xz.x, 0.0f, rd.xz.y};
        loco_.root_yaw_delta = rd.yaw;
        loco_.phase = play_.phase;
        const anim::ClipEntry& e = library_[loco_m_.role];
        for (std::size_t side = 0; side < 2; ++side) {
            bool crossed = false;
            for (uint8_t i = 0; i < e.plant_count[side]; ++i) {
                const float pp = e.plant_phase[side][i];
                const float a = loco_m_.prev_phase;
                const float b = loco_m_.phase;
                crossed = crossed || (a <= b ? (pp > a && pp <= b) : (pp > a || pp <= b));
            }
            loco_.footfall[side] = crossed;
            loco_.planted[side] = sched_planted_[side];
        }
        loco_.dir = loco_m_.dir;
        loco_.yaw_owned_by_clip = anim::loco_yaw_owned_by_clip(library_, loco_m_);
        loco_.verbatim = true;
        if (slip_count_ > 0) {
            const glm::vec3 mean = slip_sum_world_ / static_cast<float>(slip_count_);
            const float yaw = anim::body_root_for(drive, standing_ground).yaw;
            loco_.root_delta_model += glm::vec3{
                glm::rotate(glm::mat4{1.0f}, yaw, glm::vec3{0.0f, 1.0f, 0.0f})
                * glm::vec4{mean, 0.0f}};
        }
        loco_.valid = true;
    }
    pose_prev_ = ticked_ ? pose_curr_ : anim::evaluate_body_pose(rig, drive);
    root_prev_ = ticked_ ? root_curr_ : anim::body_root_for(drive, standing_ground);
    pose_curr_ = anim::evaluate_body_pose(rig, drive);
    root_curr_ = anim::body_root_for(drive, standing_ground);
    // СКОЛЬКО РЕЕСТРА ПОВЕРХ КЛИПА. Ферма, а не второе решение: вес ведёт
    // update_bodies, здесь он только запоминается до кадра.
    pose_weight_ = std::clamp(drive.pose_weight, 0.0f, 1.0f);
    last_role_ = play_.role;
    turn_counter_end_rad_ = turn_counter_rad();
    ticked_ = true;
}

void SkinnedCharacter::commit_root(const anim::BodyDrive& drive,
                                   const glm::vec3& standing_ground, float dt) {
    if (!ready_) {
        return;
    }
    root_curr_ = anim::body_root_for(drive, standing_ground);
    const glm::mat4 to_world =
        glm::translate(glm::mat4{1.0f}, root_curr_.ground)
        * glm::rotate(glm::mat4{1.0f}, -root_curr_.yaw, glm::vec3{0.0f, 1.0f, 0.0f});
    // КОРОБКИ СТОП ПО ПОЗЕ ТИКА (§12): физическим стопам нужен кадр
    // хитбокса стопы в мире — кинематическая поза маха и место постановки.
    slip_sum_world_ = glm::vec3{0.0f};
    slip_count_ = 0;
    foot_box_valid_ = {false, false};
    if (tick_sampled_) {
        const anim::HitboxPose hp = anim::hitbox_pose(hitboxes_, skeleton_, binding_, tick_sample_);
        for (std::size_t i = 0; i < anim::HITBOX_COUNT; ++i) {
            const anim::BodyPart part = hitboxes_.slot[i].part;
            const std::size_t side = part == anim::BodyPart::FootL ? 0
                                     : part == anim::BodyPart::FootR ? 1 : 2;
            if (side < 2 && hp.valid[i] != 0) {
                foot_box_model_[side] = hp.frame[i];
                // СТОПА ПОСЛЕ IK, А НЕ СЫРАЯ ПОЗА КЛИПА: подъём, который IK даёт
                // лодыжке к её земле (plan_.need > 0), — и коробке. Иначе на
                // склоне 5° плоская стопа клипа втыкается углом на 2,4 см и
                // тело стопы считается вдавленным (07.09).
                if (foot_probe_.valid && plan_.need[side] > 0.0f) {
                    foot_box_model_[side][3].y += plan_.need[side];
                }
                foot_box_half_[side] = hp.half[i];
                foot_box_valid_[side] = true;
            }
        }
    }
    std::array<glm::vec3, 2> world{};
    std::array<glm::vec3, 2> ankle_world{};
    for (std::size_t side = 0; side < 2; ++side) {
        world[side] = glm::vec3{to_world * glm::vec4{contact_curr_.toe[side], 1.0f}};
        ankle_world[side] = glm::vec3{to_world * glm::vec4{contact_curr_.ankle[side], 1.0f}};
    }
    feed_telemetry(drive, dt, world, ankle_world);
}

void SkinnedCharacter::set_telemetry(bool on, const std::string& csv_path) {
    if (telemetry_csv_ != nullptr) {
        std::fclose(telemetry_csv_);
        telemetry_csv_ = nullptr;
    }
    telemetry_on_ = on && foot_setup_.valid();
    telemetry_report_s_ = 0.0f;
    if (!telemetry_on_) {
        return;
    }
    telemetry_.reset(skeleton_, foot_setup_);
    if (!csv_path.empty()) {
        telemetry_csv_ = std::fopen(csv_path.c_str(), "w");
        if (telemetry_csv_ != nullptr) {
            std::fprintf(telemetry_csv_, "%s\n", anim::LocoTelemetry::csv_header().c_str());
        } else {
            std::fprintf(stderr, "[loco] cannot open csv %s\n", csv_path.c_str());
        }
    }
}

bool SkinnedCharacter::foot_box_world(std::size_t side, glm::mat4& frame, glm::vec3& half) const {
    if (side >= 2 || !foot_box_valid_[side]) {
        return false;
    }
    const glm::mat4 to_world =
        glm::translate(glm::mat4{1.0f}, root_curr_.ground)
        * glm::rotate(glm::mat4{1.0f}, -root_curr_.yaw, glm::vec3{0.0f, 1.0f, 0.0f});
    frame = to_world * foot_box_model_[side];
    half = foot_box_half_[side];
    return true;
}

void SkinnedCharacter::feed_telemetry(const anim::BodyDrive& drive, float dt,
                                      const std::array<glm::vec3, 2>& contact_world,
                                      const std::array<glm::vec3, 2>& ankle_world) {
    if (!telemetry_on_ || !tick_sampled_) {
        return;
    }
    anim::LocoTick t;
    t.dt = dt;
    t.pose = tick_sample_;
    t.contacts = &contact_curr_;
    t.machine = loco_active_ ? &loco_m_ : nullptr;
    t.blend = turn_frozen_w();
    t.contact_world = contact_world;
    t.ankle_world = ankle_world;
    for (std::size_t side = 0; side < 2; ++side) {
        t.phys_planted[side] = foot_phys_[side].planted;
        t.phys_holds[side] = foot_phys_[side].holds;
        t.phys_slip_mps[side] = foot_phys_[side].slip_mps;
    }
    t.gap = last_gap_;
    t.play = &play_;
    t.loco = &loco_;
    t.drive = &drive;
    t.root = root_curr_;
    t.root_prev = root_prev_;
    telemetry_.push(t);
    if (telemetry_csv_ != nullptr) {
        std::fprintf(telemetry_csv_, "%s\n", telemetry_.csv_row().c_str());
    }
    telemetry_report_s_ += dt;
    if (telemetry_report_s_ >= static_cast<float>(config::LOCO_REPORT_S)) {
        telemetry_report_s_ = 0.0f;
        std::fputs(telemetry_.report().c_str(), stderr);
    }
}

SkinnedCharacter::~SkinnedCharacter() {
    if (telemetry_on_ && telemetry_.ticks() > 0) {
        std::fputs(telemetry_.report().c_str(), stderr);
    }
    if (telemetry_csv_ != nullptr) {
        std::fclose(telemetry_csv_);
    }
}

render::RenderSystem::SkinnedDraw SkinnedCharacter::build_draw(bool hide_head,
                                                              float alpha) {
    render::RenderSystem::SkinnedDraw draw;
    if (!ready_) {
        return draw;
    }
    draw.texture_asset = texture_asset_;
    draw.normal_asset = normal_asset_;
    const anim::Rig& rig = rig_;
    const float a = std::clamp(alpha, 0.0f, 1.0f);
    const anim::BodyRoot root{glm::mix(root_prev_.ground, root_curr_.ground, a),
                              root_prev_.yaw
                                  + a * shortest_turn(root_prev_.yaw, root_curr_.yaw)};
    // THE POSE OF THIS FRAME, not of this tick. Both arms interpolate: the
    // clip arm by its own clip time (playback_sample), the procedural arm by
    // slerping the two ticks it was evaluated at -- which is exactly what
    // render does to a Transform, one level up from a matrix.
    if (rest_only_) {
        // THE NEUTRAL ITSELF: no clip, no layer, no foot solve — the rig's
        // rest through the retarget, which is what the rest fit was solved
        // on and what the "screen = world" hash is taken over.
        anim::pose_local_transforms(rig, skeleton_, binding_, anim::LocalPose{}, sample_);
        anim::sample_palette(skeleton_, sample_, palette_);
    } else if (!playing_clips()
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
        if (library_.inertial) {
            inertial_.apply(inertial_.time() - (1.0f - a) * inertial_dt_, sample_);
        }
        if (foot_setup_.valid()) {
            // КОНТРВРАЩЕНИЕ И В КАДРЕ, не только на тике (§13.3): без него
            // нарисованное тело поворачивалось дважды — позой и рыском, — а
            // детектор «кадр против тиков» этого не видел: он сравнивает
            // бёдра и колени, а контрвращение сидит в корне (дыра прибора,
            // §14.3, найдена 07.09).
            anim::rotate_root_joints(skeleton_, foot_setup_.roots,
                                      glm::mix(turn_counter_prev_rad_, turn_counter_rad(), a),
                                      sample_);
            anim::warp_legs(skeleton_, foot_setup_,
                            glm::mix(leg_warp_prev_rad_, leg_warp_rad_, a), sample_);
        }
        anim::FootIkPlan frame_plan{};
        bool frame_planned = false;
        if (foot_probe_.valid) {
            anim::FootIkPlan plan =
                anim::plan_foot_ik(skeleton_, foot_setup_, foot_probe_, sample_);
            plan.root_dy = root_dy_;
            frame_plan = plan;
            frame_planned = true;
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
        if (frame_planned) {
            last_gap_ = anim::foot_gap(skeleton_, foot_setup_, foot_probe_, frame_plan, sample_);
        }
        if (telemetry_on_) {
            telemetry_.push_frame(sample_, root, a);
        }
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
    draw.mesh_asset = mesh_asset_;
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
    draw.mesh_asset = blade_asset_;
    // Клинок красится своими вершинами; лист кожи развёрнут по телу, а не
    // по стали.
    draw.texture_asset = 0;
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
