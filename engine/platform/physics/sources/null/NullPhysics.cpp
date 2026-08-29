/*
Module: engine/platform/physics
File: engine/platform/physics/sources/null/NullPhysics.cpp

Responsibility:
- Null IPhysics backend (Rule 3, Q31) — a runnable mode, not a stub: bodies are
  valid-but-inert, characters glide on their spawn plane, everything succeeds
  and stays deterministic (Rule 13.2).

Key items:
- NullPhysics (file-local): handle registries + the contracted null semantics.
- create_null_physics(): the factory (CreateNullPhysics.h).

Dependencies:
- Uses: interfaces/IPhysics.h, C++ stdlib.
- Used by: engine/app wiring, gameplay tests, headless tours.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Semantics here are contract (IPhysics.h notes): horizontal displacement
  applied fully, vertical ignored, grounded always true, raycasts miss.
*/

#include "engine/platform/physics/sources/null/CreateNullPhysics.h"

#include <unordered_map>

namespace dfn::platform {
namespace {

class NullPhysics final : public IPhysics {
public:
    // Lifecycle ----------------------------------------------------------------
    bool init() override { return true; }
    void shutdown() override {
        bodies_.clear();
        characters_.clear();
    }

    // Simulation ---------------------------------------------------------------
    void step(float dt) override {
        (void)dt; // fixed SIM_DT by contract (Rule 12); null needs no time.
        for (auto& [id, character] : characters_) {
            // Contract: horizontal applied fully, vertical ignored (Q31 —
            // the capsule glides on its spawn plane).
            character.position.x += character.pending.x;
            character.position.z += character.pending.z;
            character.pending = glm::vec3{0.0f};
        }
    }

    // Static bodies ------------------------------------------------------------
    // layer == 0 is rejected here exactly as in the Jolt backend: null is a
    // runnable mode, so it must catch the same authoring mistakes (a body no
    // mask can select is never intentional) rather than mask them.
    PhysicsBodyHandle create_terrain_mesh(const TerrainMeshDesc& desc) override {
        if (desc.layer == 0 || desc.indices.size() < 3) {
            return {}; // empty mesh: nothing to collide, not an error
        }
        return make_body();
    }

    PhysicsBodyHandle create_terrain(const TerrainDesc& desc) override {
        if (desc.layer == 0) {
            return {};
        }
        return make_body();
    }
    PhysicsBodyHandle create_static_box(const StaticBoxDesc& desc) override {
        if (desc.layer == 0) {
            return {};
        }
        return make_body();
    }
    // The null backend has no geometry to move, so a move is a no-op — the
    // same shape as every other body call here. Deterministic and inert.
    void set_body_transform(PhysicsBodyHandle body, const glm::vec3& position,
                            const glm::quat& rotation) override {
        if (const auto it = bodies_.find(body.id); it != bodies_.end()) {
            it->second.pose = BodyPose{position, rotation};
        }
    }
    void destroy_body(PhysicsBodyHandle body) override { bodies_.erase(body.id); }

    // Dynamic bodies -----------------------------------------------------------
    // VALID BUT INERT, like every other body here: it is created where the
    // caller put it, it never moves, and it is always asleep. A null backend
    // that dropped props to y=0 would make every headless prop run a lie.
    PhysicsBodyHandle create_dynamic_body(const DynamicBodyDesc& desc) override {
        if (desc.layer == 0 || desc.mass_kg <= 0.0f || desc.points.size() < 4) {
            return {}; // same refusals as Jolt: null catches the same mistakes
        }
        const PhysicsBodyHandle handle = make_body();
        bodies_[handle.id] = Body{BodyPose{desc.position, desc.rotation}};
        return handle;
    }
    BodyPose body_pose(PhysicsBodyHandle body) const override {
        const auto it = bodies_.find(body.id);
        return it != bodies_.end() ? it->second.pose : BodyPose{};
    }
    glm::vec3 body_velocity(PhysicsBodyHandle) const override { return glm::vec3{0.0f}; }
    void set_body_velocity(PhysicsBodyHandle, const glm::vec3&, const glm::vec3&) override {
        // Inert: a velocity nothing integrates would be a number nobody reads.
    }
    void set_body_gravity_factor(PhysicsBodyHandle, float) override {}
    bool body_asleep(PhysicsBodyHandle body) const override {
        return bodies_.contains(body.id); // nothing ever wakes here
    }
    void activate_body(PhysicsBodyHandle) override {}

    // Character controller -----------------------------------------------------
    CharacterHandle create_character(const CharacterDesc& desc) override {
        if (desc.layer == 0 || desc.collides_with == 0) {
            return {};
        }
        const CharacterHandle handle{next_id_++};
        characters_[handle.id] =
            Character{desc.position, glm::vec3{0.0f}, desc.radius, desc.height};
        return handle;
    }
    void destroy_character(CharacterHandle character) override {
        characters_.erase(character.id);
    }

    void move_character(CharacterHandle character, const glm::vec3& displacement) override {
        if (auto it = characters_.find(character.id); it != characters_.end()) {
            it->second.pending += displacement;
        }
    }

    glm::vec3 character_position(CharacterHandle character) const override {
        const auto it = characters_.find(character.id);
        return it != characters_.end() ? it->second.position : glm::vec3{0.0f};
    }
    bool character_grounded(CharacterHandle character) const override {
        return characters_.contains(character.id); // contract: always grounded
    }

    // The null backend has no geometry, so a resize can never be obstructed:
    // it simply records the height so callers read back what they set and
    // crouch logic is testable headless (Rule 3 — a runnable mode).
    void set_character_height(CharacterHandle character, float height) override {
        if (auto it = characters_.find(character.id); it != characters_.end()) {
            if (height > 2.0f * it->second.radius) {
                it->second.height = height;
            }
        }
    }
    float character_height(CharacterHandle character) const override {
        const auto it = characters_.find(character.id);
        return it != characters_.end() ? it->second.height : 0.0f;
    }

    void teleport_character(CharacterHandle character, const glm::vec3& position) override {
        if (auto it = characters_.find(character.id); it != characters_.end()) {
            it->second.position = position;
            it->second.pending = glm::vec3{0.0f};
        }
    }

    // Queries ------------------------------------------------------------------
    RayHit raycast(const glm::vec3& origin, const glm::vec3& direction,
                   float max_distance, CollisionMask mask) const override {
        (void)origin;
        (void)direction;
        (void)max_distance;
        (void)mask;
        return RayHit{}; // contract: null raycasts always miss
    }

    RayHit sphere_cast(const glm::vec3& origin, const glm::vec3& direction, float radius,
                       float max_distance, CollisionMask mask) const override {
        (void)origin;
        (void)direction;
        (void)radius;
        (void)max_distance;
        (void)mask;
        return RayHit{}; // contract: null sweeps always miss, like the ray
    }

private:
    struct Character {
        glm::vec3 position{0.0f};
        glm::vec3 pending{0.0f};
        float radius = 0.0f;
        float height = 0.0f;
    };

    struct Body {
        BodyPose pose{};
    };

    PhysicsBodyHandle make_body() {
        const PhysicsBodyHandle handle{next_id_++};
        bodies_[handle.id] = Body{};
        return handle;
    }

    uint32_t next_id_ = 1; // 0 is the invalid handle
    std::unordered_map<uint32_t, Body> bodies_;
    std::unordered_map<uint32_t, Character> characters_;
};

} // namespace

std::unique_ptr<IPhysics> create_null_physics() {
    return std::make_unique<NullPhysics>();
}

} // namespace dfn::platform
