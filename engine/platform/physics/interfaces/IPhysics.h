/*
Created: 09:08:2026 - 00:18:26
Last updated: 09:08:2026 - 15:08:24
Module: engine/platform/physics
File: engine/platform/physics/interfaces/IPhysics.h

Responsibility:
- The platform physics contract (Rule 0). Everything the engine may ask of a
  physics backend goes through this interface; Jolt lives only behind it.

Key items:
- IPhysics: init/shutdown, fixed step, static terrain/box bodies, kinematic
  character controller (capsule, move-with-slide, grounded query), raycast.
- TerrainDesc: heightmap collision from plain float samples (engine/world data).
- CharacterDesc / RayHit: plain-data descriptors, no backend types.
- PhysicsBodyHandle / CharacterHandle: opaque POD handles (0 = invalid).

Dependencies:
- Uses: C++ stdlib, glm (Rule 2). Nothing else.
- Used by: engine/physics (character controller, collision layers), engine/gameplay
  (via engine/physics and raycast queries), tests (null backend).

Notes:
- step() is called exactly once per fixed simulation tick with SIM_DT from the
  generated constants (Rule 12). Backends must not sub-step on their own clock.
- move_character() records the desired displacement for the NEXT step(); the
  backend resolves it with collide-and-slide (capsule vs. static world) during
  step(). Queries return post-step state. This keeps the tick order explicit:
  gather intents -> step() -> read results.
- user_data carries an ECS EntityId's bit pattern so raycast hits resolve back
  to entities without the backend knowing what an entity is.
- Collision layer bit meanings are defined by engine/physics (stage 2); this
  interface treats CollisionMask as opaque bits.
- ZERO-MASK REJECTION (contract, all backends): a create_* call whose `layer`
  is 0 — or a character whose `collides_with` is 0 — returns an INVALID handle
  and creates nothing. A body no mask can select, or a character that collides
  with nothing, is never intentional; leaving `layer` at its default is the
  authoring mistake this catches. Callers must check handle.valid(). (Learned
  the hard way: a hand-filled TerrainDesc left `layer` at 0, every terrain body
  collided with nothing, and the player fell through the world.)
- Null backend (Rule 3, Q31 — a runnable mode): step() is a no-op; bodies are
  valid-but-inert; move_character applies the horizontal displacement fully and
  ignores the vertical component; character_grounded() always returns true;
  raycast() always misses. Deterministic, so gameplay tests run on null.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Do not add Jolt types, includes, or assumptions to this header.
- Contract frozen for stage 1 (Rule 26); changes only via group sync.
*/
/*
UPD:
- 09:08:2026 - 00:18:26: Initial stage-1 contract (terrain, static boxes,
                         kinematic character, raycast).
- 09:08:2026 - 15:08:24: Documented the zero-mask rejection rule (behavioral
                         clarification; no API change — field set, signatures
                         and semantics of every existing call are untouched).
*/

#pragma once

#include <cstdint>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <span>

namespace dfn::platform {

// Opaque resource handles. id == 0 means "invalid / none".
struct PhysicsBodyHandle {
    uint32_t id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
};
struct CharacterHandle {
    uint32_t id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
};

// Collision layer bitmask. Bit semantics live in engine/physics, not here.
using CollisionMask = uint16_t;
inline constexpr CollisionMask COLLIDE_ALL = 0xFFFF;

// Static terrain collision built from heightmap samples (one body per chunk).
// Samples are absolute heights in meters, row-major [z][x]; engine/world owns
// the source data (uint16 + scale/offset per NUMBERS) and converts to float.
struct TerrainDesc {
    glm::vec3 origin{0.0f};         // world position of sample (0, 0), meters
    uint32_t sample_count_x = 0;    // HEIGHTMAP_RESOLUTION (NUMBERS.md)
    uint32_t sample_count_z = 0;
    float sample_spacing = 0.0f;    // HEIGHTMAP_STEP, meters
    std::span<const float> heights; // sample_count_x * sample_count_z values
    CollisionMask layer = 0;
    uint64_t user_data = 0;         // EntityId bits (chunk terrain entity)
};

// Static box collider (buildings, dungeon prefab pieces, large props).
struct StaticBoxDesc {
    glm::vec3 center{0.0f};                       // world space, meters
    glm::vec3 half_extents{0.0f};                 // meters
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    CollisionMask layer = 0;
    uint64_t user_data = 0;                       // EntityId bits
};

// Kinematic capsule character (player and NPCs). All values in meters/radians;
// callers pass constants generated from NUMBERS.md, never literals (Rule 14).
struct CharacterDesc {
    glm::vec3 position{0.0f};       // capsule BOTTOM point (feet), world space
    float radius = 0.0f;
    float height = 0.0f;            // total height including both caps
    float max_slope_radians = 0.0f; // steeper ground is not walkable
    float step_height = 0.0f;       // max ledge auto-stepped over
    CollisionMask layer = 0;
    CollisionMask collides_with = COLLIDE_ALL;
    uint64_t user_data = 0;         // EntityId bits
};

struct RayHit {
    bool hit = false;
    float distance = 0.0f;    // meters from origin along the ray
    glm::vec3 position{0.0f}; // world-space hit point
    glm::vec3 normal{0.0f};   // unit surface normal at the hit
    uint64_t user_data = 0;   // EntityId bits of the hit body/character, 0 = none
};

class IPhysics {
public:
    virtual ~IPhysics() = default;

    // Lifecycle ----------------------------------------------------------------
    [[nodiscard]] virtual bool init() = 0;
    virtual void shutdown() = 0;

    // Simulation ---------------------------------------------------------------
    // Advances exactly one fixed step. dt MUST be SIM_DT (Rule 12); passing a
    // variable frame time is a contract violation.
    virtual void step(float dt) = 0;

    // Static bodies ------------------------------------------------------------
    [[nodiscard]] virtual PhysicsBodyHandle create_terrain(const TerrainDesc& desc) = 0;
    [[nodiscard]] virtual PhysicsBodyHandle create_static_box(const StaticBoxDesc& desc) = 0;
    virtual void destroy_body(PhysicsBodyHandle body) = 0;

    // Character controller -----------------------------------------------------
    [[nodiscard]] virtual CharacterHandle create_character(const CharacterDesc& desc) = 0;
    virtual void destroy_character(CharacterHandle character) = 0;

    // Records the desired displacement (meters, this tick) to be resolved with
    // collide-and-slide during the next step(). Vertical motion (gravity, jump)
    // is part of the displacement; the backend only collides and slides.
    virtual void move_character(CharacterHandle character, const glm::vec3& displacement) = 0;

    // Post-step state. Position is the capsule bottom point (feet).
    [[nodiscard]] virtual glm::vec3 character_position(CharacterHandle character) const = 0;
    [[nodiscard]] virtual bool character_grounded(CharacterHandle character) const = 0;

    // Instant placement without collision resolution (spawn, chunk streaming).
    virtual void teleport_character(CharacterHandle character, const glm::vec3& position) = 0;

    // Queries ------------------------------------------------------------------
    // direction must be unit length. Returns the closest hit within max_distance
    // among bodies/characters matching mask.
    [[nodiscard]] virtual RayHit raycast(const glm::vec3& origin, const glm::vec3& direction,
                                         float max_distance,
                                         CollisionMask mask = COLLIDE_ALL) const = 0;
};

} // namespace dfn::platform
