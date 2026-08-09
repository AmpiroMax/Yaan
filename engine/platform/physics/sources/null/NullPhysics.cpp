/*
Created: 09:08:2026 - 00:45:08
Last updated: 09:08:2026 - 00:45:08
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
/*
UPD:
- 09:08:2026 - 00:45:08: Stage 2 — initial null backend implementation.
*/

#include "engine/platform/physics/sources/null/CreateNullPhysics.h"

#include <unordered_map>
#include <unordered_set>

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
    PhysicsBodyHandle create_terrain(const TerrainDesc& desc) override {
        (void)desc;
        return make_body();
    }
    PhysicsBodyHandle create_static_box(const StaticBoxDesc& desc) override {
        (void)desc;
        return make_body();
    }
    void destroy_body(PhysicsBodyHandle body) override { bodies_.erase(body.id); }

    // Character controller -----------------------------------------------------
    CharacterHandle create_character(const CharacterDesc& desc) override {
        const CharacterHandle handle{next_id_++};
        characters_[handle.id] = Character{desc.position, glm::vec3{0.0f}};
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

private:
    struct Character {
        glm::vec3 position{0.0f};
        glm::vec3 pending{0.0f};
    };

    PhysicsBodyHandle make_body() {
        const PhysicsBodyHandle handle{next_id_++};
        bodies_.insert(handle.id);
        return handle;
    }

    uint32_t next_id_ = 1; // 0 is the invalid handle
    std::unordered_set<uint32_t> bodies_;
    std::unordered_map<uint32_t, Character> characters_;
};

} // namespace

std::unique_ptr<IPhysics> create_null_physics() {
    return std::make_unique<NullPhysics>();
}

} // namespace dfn::platform
