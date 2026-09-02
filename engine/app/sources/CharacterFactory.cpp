/*
Module: engine/app
File: engine/app/sources/CharacterFactory.cpp

Responsibility:
- Implements the one character build: SkinnedCharacter load, the Jolt
  hitboxes at the rest pose, the optional capsule, and the release.

Dependencies:
- Uses: CharacterFactory.h, engine/physics CollisionLayers, generated
  constants (PLAYER_CAPSULE_*), glm.
- Used by: dfn_app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона app (lead) владеет этим файлом.
*/

#include "engine/app/sources/CharacterFactory.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/physics/sources/CollisionLayers.h"

#include "engine/anim/sources/BodyGaps.h"
#include "engine/anim/sources/SkinnedBody.h"

#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace dfn::app {
namespace {

/// The half the two builds share once the body is loaded: boxes at rest,
/// capsule on request.
void finish_bodies(SkinnedCharacter& body, CharacterBodies& bodies,
                   platform::IPhysics* physics, const CharacterSpec& spec) {
    if (physics == nullptr) {
        return;
    }
    bodies.hitboxes.create(*physics, spec.owner, body.hitboxes(), body.hitbox_pose(),
                           spec.to_world);
    if (spec.make_capsule && !bodies.capsule.valid()) {
        platform::CharacterDesc desc;
        desc.position = spec.capsule_feet;
        desc.radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
        desc.height = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
        desc.max_slope_radians = static_cast<float>(config::PLAYER_MAX_SLOPE);
        desc.step_height = static_cast<float>(config::PLAYER_STEP_HEIGHT);
        desc.layer = physics::LAYER_CHARACTER;
        desc.collides_with = physics::LAYER_STATIC | physics::LAYER_LOOSE;
        desc.user_data = spec.owner.packed();
        bodies.capsule = physics->create_character(desc);
    }
}

} // namespace

bool build_character(SkinnedCharacter& body, CharacterBodies& bodies,
                     render::RenderSystem& render_system, platform::IRenderer& renderer,
                     platform::IPhysics* physics, const std::filesystem::path& path,
                     const CharacterSpec& spec) {
    if (spec.proportions == nullptr) {
        return false;
    }
    if (physics != nullptr) {
        bodies.hitboxes.destroy(*physics);
    }
    if (!body.load(render_system, renderer, *spec.proportions, path, spec.legacy_rest,
                   spec.mesh_asset, spec.blade_asset)) {
        return false;
    }
    finish_bodies(body, bodies, physics, spec);
    return true;
}

bool build_character_object(SkinnedCharacter& body, CharacterBodies& bodies,
                            render::RenderSystem& render_system,
                            platform::IRenderer& renderer, platform::IPhysics* physics,
                            render::RegistryObject object,
                            const std::filesystem::path& label, const CharacterSpec& spec) {
    if (spec.proportions == nullptr) {
        return false;
    }
    if (physics != nullptr) {
        bodies.hitboxes.destroy(*physics);
    }
    if (!body.load_object(render_system, renderer, *spec.proportions, std::move(object),
                          label, spec.legacy_rest, spec.mesh_asset, spec.blade_asset)) {
        return false;
    }
    finish_bodies(body, bodies, physics, spec);
    return true;
}

void release_character(SkinnedCharacter& body, CharacterBodies& bodies,
                       render::RenderSystem& render_system, platform::IRenderer& renderer,
                       platform::IPhysics* physics) {
    if (physics != nullptr) {
        bodies.hitboxes.destroy(*physics);
        if (bodies.capsule.valid()) {
            physics->destroy_character(bodies.capsule);
            bodies.capsule = platform::CharacterHandle{};
        }
    }
    body.release(render_system, renderer);
}

anim::BodyGaps character_rest_gaps(const SkinnedCharacter& body) {
    anim::BodyGaps gaps;
    if (!body.ready()) {
        return gaps;
    }
    const skel::Skeleton& skeleton = body.skeleton();
    std::vector<anim::JointLocal> sample(skeleton.size());
    anim::pose_local_transforms(body.rig(), skeleton, body.binding(), anim::LocalPose{},
                                sample);
    const std::vector<platform::SkinnedVertex>& verts = body.current_vertices();
    const anim::SkinParts parts = anim::label_skin_parts(skeleton, body.binding(), verts);
    return anim::measure_body_gaps(skeleton, body.binding(), body.hitboxes(), verts, parts,
                                   sample);
}

void debug_draw_hitboxes(platform::IRenderer& renderer, const anim::HitboxSet& set,
                         const anim::HitboxPose& pose, const glm::mat4& to_world,
                         uint32_t color_rgba) {
    for (uint32_t i = 0; i < anim::HITBOX_COUNT; ++i) {
        if (pose.valid[i] == 0) {
            continue;
        }
        const glm::mat4 m = to_world * pose.frame[i];
        const glm::vec3 h = pose.half[i];
        const auto at = [&](float sx, float sy, float sz) {
            return glm::vec3{m * glm::vec4{sx * h.x, sy * h.y, sz * h.z, 1.0f}};
        };
        if (set.slot[i].shape == anim::HitShape::Sphere) {
            const glm::vec3 c = at(0.0f, 0.0f, 0.0f);
            renderer.debug_line(at(-1.0f, 0.0f, 0.0f), at(1.0f, 0.0f, 0.0f), color_rgba);
            renderer.debug_line(at(0.0f, -1.0f, 0.0f), at(0.0f, 1.0f, 0.0f), color_rgba);
            renderer.debug_line(at(0.0f, 0.0f, -1.0f), at(0.0f, 0.0f, 1.0f), color_rgba);
            (void)c;
            continue;
        }
        const glm::vec3 c[8] = {at(-1, -1, -1), at(1, -1, -1), at(1, 1, -1), at(-1, 1, -1),
                                at(-1, -1, 1),  at(1, -1, 1),  at(1, 1, 1),  at(-1, 1, 1)};
        constexpr int E[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                  {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (const auto& e : E) {
            renderer.debug_line(c[e[0]], c[e[1]], color_rgba);
        }
    }
}

} // namespace dfn::app
