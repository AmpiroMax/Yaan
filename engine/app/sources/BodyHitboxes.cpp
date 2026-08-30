/*
Module: engine/app
File: engine/app/sources/BodyHitboxes.cpp

Responsibility:
- Implements the per-slot physics bodies and the world-space part query.

Dependencies:
- Uses: BodyHitboxes.h, engine/physics CollisionLayers.
- Used by: dfn_app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/

#include "engine/app/sources/BodyHitboxes.h"

#include "engine/physics/sources/CollisionLayers.h"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dfn::app {
namespace {

/// A posed slot carried into world space. The scale is dropped on purpose:
/// the frames anim builds are orthonormal by construction, and a matrix with
/// a scale in it would silently change the half extents Jolt was given once.
struct WorldFrame {
    glm::vec3 centre{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

[[nodiscard]] WorldFrame to_world_frame(const glm::mat4& to_world, const glm::mat4& local) {
    const glm::mat4 m = to_world * local;
    WorldFrame f;
    f.centre = glm::vec3{m[3]};
    glm::mat3 r{m};
    for (int c = 0; c < 3; ++c) {
        const float len = glm::length(r[c]);
        r[c] = len > 1.0e-8f ? r[c] / len : glm::vec3{c == 0, c == 1, c == 2};
    }
    f.rotation = glm::normalize(glm::quat_cast(r));
    return f;
}

void carry(const anim::HitboxPose& in, const glm::mat4& to_world, anim::HitboxPose& out) {
    out = in;
    for (uint32_t i = 0; i < anim::HITBOX_COUNT; ++i) {
        if (in.valid[i] == 0) {
            continue;
        }
        out.frame[i] = to_world * in.frame[i];
    }
}

} // namespace

void BodyHitboxes::create(platform::IPhysics& physics, ecs::EntityId owner,
                          const anim::HitboxSet& set, const anim::HitboxPose& pose,
                          const glm::mat4& to_world) {
    destroy(physics);
    owner_ = owner;
    carry(pose, to_world, world_);
    for (uint32_t i = 0; i < anim::HITBOX_COUNT; ++i) {
        if (pose.valid[i] == 0) {
            continue;
        }
        const WorldFrame f = to_world_frame(to_world, pose.frame[i]);
        platform::StaticBoxDesc desc;
        desc.center = f.centre;
        // A SPHERE BECOMES ITS BOUNDING BOX HERE and stays a sphere in the
        // table (see the header). The ray query, which is what names the part,
        // reads the table.
        desc.half_extents = pose.half[i];
        desc.rotation = f.rotation;
        desc.layer = physics::LAYER_HITBOX;
        desc.user_data = owner.packed();
        body_[i] = physics.create_static_box(desc);
        count_ += body_[i].valid() ? 1u : 0u;
    }
    live_ = count_ > 0;
}

void BodyHitboxes::update(platform::IPhysics& physics, const anim::HitboxPose& pose,
                          const glm::mat4& to_world) {
    if (!live_) {
        return;
    }
    carry(pose, to_world, world_);
    for (uint32_t i = 0; i < anim::HITBOX_COUNT; ++i) {
        if (!body_[i].valid() || pose.valid[i] == 0) {
            continue;
        }
        const WorldFrame f = to_world_frame(to_world, pose.frame[i]);
        physics.set_body_transform(body_[i], f.centre, f.rotation);
    }
}

void BodyHitboxes::destroy(platform::IPhysics& physics) {
    for (platform::PhysicsBodyHandle& h : body_) {
        if (h.valid()) {
            physics.destroy_body(h);
        }
        h = platform::PhysicsBodyHandle{};
    }
    count_ = 0;
    live_ = false;
    world_ = anim::HitboxPose{};
}

anim::HitboxHit BodyHitboxes::part_at(const anim::HitboxSet& set, const glm::vec3& origin,
                                      const glm::vec3& direction,
                                      float max_distance) const {
    if (!live_) {
        return anim::HitboxHit{};
    }
    return anim::hitbox_raycast(set, world_, origin, direction, max_distance);
}

} // namespace dfn::app
