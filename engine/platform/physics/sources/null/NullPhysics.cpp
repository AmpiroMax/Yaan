/*
Module: engine/platform/physics
File: engine/platform/physics/sources/null/NullPhysics.cpp

Responsibility:
- Null IPhysics backend (Rule 3, Q31) — a runnable mode, not a stub: bodies are
  valid-but-inert, characters glide on their spawn plane, feet stand on that
  same plane (touching, holding, no slip), impulses give the contracted
  dv = J / m, ragdolls keep the pose they are given; everything succeeds and
  stays deterministic (Rule 13.2).

Key items:
- NullPhysics (file-local): handle registries + the contracted null semantics.
- create_null_physics(): the factory (CreateNullPhysics.h).

Dependencies:
- Uses: interfaces/IPhysics.h, generated constants (CHARACTER_MASS_KG,
  CHARACTER_PUSH_DECAY_S), C++ stdlib.
- Used by: engine/app wiring, gameplay tests, headless tours.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Semantics here are contract (IPhysics.h notes): horizontal displacement
  applied fully, vertical ignored, grounded always true, raycasts miss.
*/

#include "engine/platform/physics/sources/null/CreateNullPhysics.h"

#include "engine/core/config/sources/Constants.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

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
        // dt is SIM_DT by contract (Rule 12); null spends it only on the
        // shove a character carries (dv = J / m must integrate to a path).
        for (auto& [id, character] : characters_) {
            // Contract: horizontal applied fully, vertical ignored (Q31 —
            // the capsule glides on its spawn plane). The push rides on top,
            // and decays as on ground — null is always grounded.
            const glm::vec3 move = character.pending + character.push_velocity * dt;
            character.position.x += move.x;
            character.position.z += move.z;
            character.velocity = glm::vec3{move.x / dt, 0.0f, move.z / dt};
            character.pending = glm::vec3{0.0f};
            character.push_velocity *=
                std::exp(-dt / static_cast<float>(config::CHARACTER_PUSH_DECAY_S));
        }
        // A swinging foot arrives where it was sent; a planted one stays.
        for (auto& [id, foot] : feet_) {
            if (foot.mode == FootMode::Swing) {
                bodies_[id].pose = foot.target;
            }
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
    void destroy_body(PhysicsBodyHandle body) override {
        bodies_.erase(body.id);
        feet_.erase(body.id);
    }

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
        Character character;
        character.position = desc.position;
        character.radius = desc.radius;
        character.height = desc.height;
        character.mass = desc.mass_kg > 0.0f
                             ? desc.mass_kg
                             : static_cast<float>(config::CHARACTER_MASS_KG);
        characters_[handle.id] = character;
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

    // Character impulses and contacts -----------------------------------------
    // dv = J / m exactly, then the same decay as on ground (null is always
    // grounded). Contacts: null has no geometry, so nobody is ever touched.
    void character_add_impulse(CharacterHandle character, const glm::vec3& impulse_ns) override {
        if (auto it = characters_.find(character.id); it != characters_.end()) {
            it->second.push_velocity += impulse_ns / it->second.mass;
        }
    }
    glm::vec3 character_velocity(CharacterHandle character) const override {
        const auto it = characters_.find(character.id);
        return it != characters_.end() ? it->second.velocity : glm::vec3{0.0f};
    }
    float character_mass(CharacterHandle character) const override {
        const auto it = characters_.find(character.id);
        return it != characters_.end() ? it->second.mass : 0.0f;
    }
    std::span<const CharacterContact> character_contacts(CharacterHandle) const override {
        return {};
    }

    // Physical feet ---------------------------------------------------------------
    // The foot stands on the plane the capsule glides on: always touching,
    // normal up, holding, no slip. A swinging foot arrives where it is sent
    // at the next step; a planted foot stays where the swing left it.
    PhysicsBodyHandle create_foot_body(const FootBodyDesc& desc) override {
        if (desc.layer == 0 || desc.collides_with == 0 || desc.mass_kg <= 0.0f
            || desc.half_extents.x <= 0.0f || desc.half_extents.y <= 0.0f
            || desc.half_extents.z <= 0.0f) {
            return {}; // the same refusals as Jolt
        }
        const PhysicsBodyHandle handle = make_body();
        bodies_[handle.id] = Body{BodyPose{desc.position, desc.rotation}};
        Foot foot;
        foot.target = BodyPose{desc.position, desc.rotation};
        foot.half_y = desc.half_extents.y;
        foot.substance = desc.substance;
        feet_[handle.id] = foot;
        return handle;
    }
    void set_foot_kinematic_pose(PhysicsBodyHandle foot, const BodyPose& pose) override {
        if (auto it = feet_.find(foot.id); it != feet_.end() && it->second.mode == FootMode::Swing) {
            it->second.target = pose;
        }
    }
    void set_foot_mode(PhysicsBodyHandle foot, FootMode mode) override {
        if (auto it = feet_.find(foot.id); it != feet_.end()) {
            if (mode == FootMode::Swing) {
                it->second.target = bodies_[foot.id].pose; // hold where it is
            }
            it->second.mode = mode;
        }
    }
    FootMode foot_mode(PhysicsBodyHandle foot) const override {
        const auto it = feet_.find(foot.id);
        return it != feet_.end() ? it->second.mode : FootMode::Swing;
    }
    FootContact foot_contact(PhysicsBodyHandle foot) const override {
        FootContact contact;
        const auto it = feet_.find(foot.id);
        if (it == feet_.end()) {
            return contact;
        }
        const BodyPose& pose = bodies_.at(foot.id).pose;
        contact.touching = true;
        contact.point = pose.position - glm::vec3{0.0f, it->second.half_y, 0.0f};
        contact.normal = glm::vec3{0.0f, 1.0f, 0.0f};
        contact.ground = core::SUBSTANCE_DEFAULT;
        contact.slope_tan = 0.0f;
        contact.friction_pair = std::sqrt(core::substance(it->second.substance).friction
                                          * core::substance(core::SUBSTANCE_DEFAULT).friction);
        contact.holds = true;
        return contact;
    }

    // Ragdoll -----------------------------------------------------------------
    // Valid, inert, always at rest: the pose given is the pose read back.
    RagdollHandle create_ragdoll(const RagdollDesc& desc) override {
        if (desc.layer == 0 || desc.collides_with == 0 || desc.parts.empty()) {
            return {};
        }
        std::vector<BodyPose> poses;
        poses.reserve(desc.parts.size());
        for (size_t i = 0; i < desc.parts.size(); ++i) {
            const RagdollPartDesc& part = desc.parts[i];
            const bool has_volume = part.radius > 0.0f
                                    || (part.half_extents.x > 0.0f && part.half_extents.y > 0.0f
                                        && part.half_extents.z > 0.0f);
            if (part.mass_kg <= 0.0f || !has_volume || part.parent >= static_cast<int32_t>(i)) {
                return {}; // the same refusals as Jolt
            }
            poses.push_back(part.pose);
        }
        const RagdollHandle handle{next_id_++};
        ragdolls_[handle.id] = std::move(poses);
        return handle;
    }
    void destroy_ragdoll(RagdollHandle ragdoll) override { ragdolls_.erase(ragdoll.id); }
    void set_ragdoll_pose(RagdollHandle ragdoll, std::span<const BodyPose> parts) override {
        if (auto it = ragdolls_.find(ragdoll.id); it != ragdolls_.end()) {
            const size_t count = std::min(parts.size(), it->second.size());
            std::copy_n(parts.begin(), count, it->second.begin());
        }
    }
    void ragdoll_pose(RagdollHandle ragdoll, std::span<BodyPose> out) const override {
        if (const auto it = ragdolls_.find(ragdoll.id); it != ragdolls_.end()) {
            const size_t count = std::min(out.size(), it->second.size());
            std::copy_n(it->second.begin(), count, out.begin());
        }
    }
    void ragdoll_add_impulse(RagdollHandle, uint32_t, const glm::vec3&, const glm::vec3&) override {
        // Inert: an impulse nothing integrates would be a number nobody reads.
    }
    void ragdoll_drive_to_pose(RagdollHandle, std::span<const BodyPose>, float) override {}
    bool ragdoll_asleep(RagdollHandle ragdoll) const override {
        return ragdolls_.contains(ragdoll.id); // nothing ever moves here
    }

private:
    struct Character {
        glm::vec3 position{0.0f};
        glm::vec3 pending{0.0f};
        float radius = 0.0f;
        float height = 0.0f;
        float mass = 0.0f;
        glm::vec3 push_velocity{0.0f};
        glm::vec3 velocity{0.0f};
    };

    struct Body {
        BodyPose pose{};
    };

    struct Foot {
        FootMode mode = FootMode::Swing;
        BodyPose target{};
        float half_y = 0.0f;
        core::SubstanceId substance = core::SUBSTANCE_DEFAULT;
    };

    PhysicsBodyHandle make_body() {
        const PhysicsBodyHandle handle{next_id_++};
        bodies_[handle.id] = Body{};
        return handle;
    }

    uint32_t next_id_ = 1; // 0 is the invalid handle
    std::unordered_map<uint32_t, Body> bodies_;
    std::unordered_map<uint32_t, Character> characters_;
    std::unordered_map<uint32_t, Foot> feet_;
    std::unordered_map<uint32_t, std::vector<BodyPose>> ragdolls_;
};

} // namespace

std::unique_ptr<IPhysics> create_null_physics() {
    return std::make_unique<NullPhysics>();
}

} // namespace dfn::platform
