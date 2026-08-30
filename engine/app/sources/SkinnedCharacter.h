/*
Module: engine/app
File: engine/app/sources/SkinnedCharacter.h

Responsibility:
- The app's half of the skinned character: load the baked .dfo, hand the mesh
  to render, and turn each tick's LocalPose into the frame's bone palette.

Key items:
- SkinnedCharacter::load(): .dfo -> registered skinned mesh + rig binding.
- SkinnedCharacter::build_draw(): pose + root -> RenderSystem::SkinnedDraw.
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

#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/skeleton/sources/Skeleton.h"
#include "engine/render/sources/RenderSystem.h"

#include "engine/platform/render/interfaces/IRenderer.h"

#include <filesystem>
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
    [[nodiscard]] bool load(render::RenderSystem& render_system,
                            platform::IRenderer& renderer, const anim::Rig& rig,
                            const std::filesystem::path& path);

    [[nodiscard]] bool ready() const { return ready_; }
    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] uint32_t bound_bones() const { return binding_.bound_count(); }
    [[nodiscard]] std::size_t joint_count() const { return skeleton_.size(); }
    [[nodiscard]] std::size_t triangle_count() const { return triangles_; }
    [[nodiscard]] const skel::Skeleton& skeleton() const { return skeleton_; }
    [[nodiscard]] const std::vector<skel::AnimClip>& clips() const { return clips_; }

    /// Builds the palette for one pose and returns the draw that shows it.
    /// `hide_head` collapses the head bone so a first-person camera inside the
    /// skull sees the world instead of the inside of a face -- the skinned
    /// equivalent of the box body's hidden head segment.
    [[nodiscard]] render::RenderSystem::SkinnedDraw build_draw(
        const anim::Rig& rig, const anim::LocalPose& pose, const anim::BodyRoot& root,
        bool hide_head);

private:
    bool ready_ = false;
    std::string name_;
    std::size_t triangles_ = 0;
    skel::Skeleton skeleton_;
    /// A copy of the bind vertices, kept ONLY so the DFN_CHAR_TRACE door can
    /// measure the posed body in metres. The picture cannot answer "how tall
    /// is he" -- perspective makes a small model near the camera and a large
    /// one far away look the same, which is exactly the confusion that cost
    /// this wave an hour.
    std::vector<platform::SkinnedVertex> bind_vertices_;
    std::vector<skel::AnimClip> clips_;
    anim::SkinnedRigBinding binding_;
    /// One frame's palette. A member so the span handed to render points at
    /// storage that outlives the call and is never reallocated mid-frame.
    std::vector<glm::mat4> palette_;
};

} // namespace dfn::app
