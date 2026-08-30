/*
Module: engine/app
File: engine/app/sources/BodyHitboxes.h

Responsibility:
- CARRIES anim's hitbox table INTO THE WORLD: one physics body per slot, moved
  to the pose every fixed tick, on their own collision layer, plus the query
  that turns a ray into "whose body, which part".

Key items:
- BodyHitboxes::create() / destroy(): the bodies for one character.
- BodyHitboxes::update(): the per-tick move.
- BodyHitboxes::part_at(): a world ray -> owner + BodyPart.

Dependencies:
- Uses: engine/anim (Hitbox), engine/physics (CollisionLayers), platform physics
  interface, core ecs (EntityId).
- Used by: engine/app (App, one instance per body that has hitboxes).

Notes:
- WHY THE APP AND NOT anim. anim sits below everything and owns no world
  (Rule 1): it can say where a shape is, not that a shape exists in Jolt. This
  file is the same ferry SkinnedCharacter is for the mesh, one layer over.
- WHY THE PART IS RESOLVED GEOMETRICALLY AND NOT FROM user_data. A physics
  body carries ONE 64-bit word and the EntityId already fills it, so the
  backend can say WHO was hit and not WHICH PART. The part therefore comes from
  anim::hitbox_raycast against the SAME transforms these bodies were placed
  with — one description of the shapes, two readers (Rule 35). Jolt's job here
  is the broad question ("is this character in the way at all"), which is the
  one a pure function cannot answer for a whole world.
- SPHERES ARRIVE AS BOXES, said out loud. `IPhysics` exposes create_static_box
  and no sphere, so the head's Jolt body is its bounding box while the table —
  and the ray query above, which is what names the part — keeps the sphere.
  The discrepancy is 21 % of the skull's footprint at its corners and it is a
  named tail, not a rounding: adding a sphere to the platform contract is a
  group sync (Rule 26), not a line in a wave that has four other items.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The bodies are created ONCE and MOVED; creating them per tick would churn
  Jolt's broadphase sixteen times a frame.
*/

#pragma once

#include "engine/anim/sources/Hitbox.h"
#include "engine/core/ecs/sources/EntityId.h"
#include "engine/platform/physics/interfaces/IPhysics.h"

#include <array>
#include <glm/mat4x4.hpp>

namespace dfn::app {

class BodyHitboxes {
public:
    /// Creates one body per placed slot. `to_world` places the pose (the same
    /// matrix the draw uses). A second call destroys the previous set first.
    void create(platform::IPhysics& physics, ecs::EntityId owner,
                const anim::HitboxSet& set, const anim::HitboxPose& pose,
                const glm::mat4& to_world);

    /// Moves every body to this tick's pose. Slots that were not placed at
    /// creation stay where they are; a body cannot appear later without a
    /// create(), which is deliberate — the table's size is fixed at load.
    void update(platform::IPhysics& physics, const anim::HitboxPose& pose,
                const glm::mat4& to_world);

    void destroy(platform::IPhysics& physics);

    [[nodiscard]] bool live() const { return live_; }
    [[nodiscard]] uint32_t body_count() const { return count_; }
    [[nodiscard]] ecs::EntityId owner() const { return owner_; }

    /// WHAT A WORLD RAY WENT THROUGH, using the transforms of the last
    /// update(). Returns None when the ray misses this body.
    [[nodiscard]] anim::HitboxHit part_at(const anim::HitboxSet& set,
                                          const glm::vec3& origin,
                                          const glm::vec3& direction,
                                          float max_distance) const;

private:
    std::array<platform::PhysicsBodyHandle, anim::HITBOX_COUNT> body_{};
    /// The last posed shapes IN WORLD SPACE — what part_at() answers against,
    /// and what update() wrote into the bodies.
    anim::HitboxPose world_{};
    ecs::EntityId owner_{};
    uint32_t count_ = 0;
    bool live_ = false;
};

} // namespace dfn::app
