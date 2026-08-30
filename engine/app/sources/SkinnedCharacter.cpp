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
#include <cstdio>
#include <cstdlib>
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
    name_ = obj->name;
    triangles_ = obj->skin.indices.size() / 3;
    palette_.assign(skeleton_.size(), glm::mat4{1.0f});
    sample_.assign(skeleton_.size(), anim::JointLocal{});
    library_ = anim::build_clip_library(skeleton_, binding_, clips_);
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
    return true;
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
    // THE PROCEDURAL PAIR, kept whether or not the door is open: it costs one
    // pose evaluation a tick and it is what makes DFN_PROC_GAIT switchable
    // without a second code path through this file.
    pose_prev_ = ticked_ ? pose_curr_ : anim::evaluate_body_pose(rig, drive);
    root_prev_ = ticked_ ? root_curr_ : anim::body_root_for(drive, standing_ground);
    pose_curr_ = anim::evaluate_body_pose(rig, drive);
    root_curr_ = anim::body_root_for(drive, standing_ground);
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

} // namespace dfn::app
