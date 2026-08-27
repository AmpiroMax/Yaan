/*
Created: 09:08:2026 - 00:18:26
Last updated: 27:08:2026 - 11:30:36
Module: engine/platform/physics
File: engine/platform/physics/interfaces/IPhysics.h

Responsibility:
- The platform physics contract (Rule 0). Everything the engine may ask of a
  physics backend goes through this interface; Jolt lives only behind it.

Key items:
- IPhysics: init/shutdown, fixed step, static terrain/box bodies, kinematic
  character controller (capsule, move-with-slide, grounded query), raycast.
- TerrainMeshDesc: voxel-terrain collision from extracted triangles (tunnels,
  overhangs) — the terrain path for the 3D world.
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
- 09:08:2026 - 16:51:22: ADDITIVE: TerrainMeshDesc + create_terrain_mesh() for
                         voxel terrain (overhangs/tunnels). create_terrain and
                         every other call are untouched, so the null backend
                         and existing tests stay honest.
- 09:08:2026 - 22:18:17: ADDITIVE: set_character_height()/character_height()
                         for crouch. Every existing signature and semantic is
                         untouched; the only implementers of IPhysics are the
                         two backends in this zone (checked repo-wide), so no
                         foreign code has to change. Rationale for a real
                         capsule resize rather than a camera-only crouch is in
                         the declaration.
- 13:08:2026 - 18:20:00: ADDITIVE: set_body_transform() — a static body may be
                         MOVED (a swinging door leaf carries its own ray
                         target; leaving the box behind makes the drawn door
                         and the touchable door two different objects). Every
                         existing signature and semantic is untouched, and the
                         only implementers are the two backends in this zone.
- 27:08:2026 - 11:30:36: ADDITIVE: sphere_cast() — свёрнутый ОБЪЁМ вместо луча
                         нулевой ширины. Заведена ради стрелы камеры третьего
                         лица: луч проходит сквозь стену, внутри которой уже
                         стоит ближняя плоскость, и кадр показывает комнату
                         снаружи дома (жалоба владельца 27.08). Ни одна
                         существующая сигнатура и ни одна семантика не тронуты;
                         реализаций у IPhysics три (Jolt, null и двойник
                         PlayerMovementTests) — обновлены все.
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

// Static terrain collision built from an EXTRACTED SURFACE MESH (the voxel
// world). This is the terrain path for 3D terrain: unlike TerrainDesc it
// represents overhangs, caves and tunnels, which a heightfield structurally
// cannot. Triangles are world-space; still one body per chunk.
// An empty mesh (no triangles) is legal — a chunk may be all air or all rock —
// and yields an INVALID handle meaning "no body needed", not an error.
struct TerrainMeshDesc {
    std::span<const glm::vec3> positions; // world space, meters
    std::span<const uint32_t> indices;    // 3 per triangle, indices into positions
    CollisionMask layer = 0;
    uint64_t user_data = 0;               // EntityId bits (chunk terrain entity)
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
    // Voxel terrain (the 3D world): the path that supports tunnels/overhangs.
    [[nodiscard]] virtual PhysicsBodyHandle create_terrain_mesh(
        const TerrainMeshDesc& desc) = 0;

    // Heightmap terrain. Retained for heightfield-only worlds and tests; it
    // CANNOT represent overhangs, so voxel terrain must use create_terrain_mesh.
    [[nodiscard]] virtual PhysicsBodyHandle create_terrain(const TerrainDesc& desc) = 0;
    [[nodiscard]] virtual PhysicsBodyHandle create_static_box(const StaticBoxDesc& desc) = 0;

    // Moves a static body. Its SHAPE is unchanged; only where it stands.
    //
    // WHY A STATIC BODY MOVES AT ALL. A door leaf swings, and its ray target is
    // the leaf, not the doorway: leave the box behind and the player aims at
    // the drawn door and hits nothing, while empty air answers the crosshair.
    // That is the same "what you see is not what you touch" defect a whole day
    // went into removing from the trees, arriving through the other door.
    // Kinematic bodies are the general answer to moving collision and this is
    // not that: a door leaf has no velocity anything needs to read, and giving
    // it one would put it in the solver's integration for nothing.
    //
    // A no-op on an invalid handle (a caller that never got a body is not an
    // error; it has nothing to move).
    virtual void set_body_transform(PhysicsBodyHandle body, const glm::vec3& position,
                                    const glm::quat& rotation) = 0;

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

    // Replaces the capsule's total height, keeping the radius and the BOTTOM
    // point fixed (the feet stay where they are; the head moves). This is what
    // crouching is: in a voxel world with carved tunnels and real ceilings, a
    // crouch that only lowers the camera is a lie — the player ducks and is
    // still blocked by the same ceiling. Heights that cannot form a capsule
    // (height <= 2 * radius) are rejected and change nothing.
    // Callers are responsible for checking there is room to grow again before
    // standing up (a raycast up from the head); a backend is free to refuse a
    // resize that would leave the capsule inside geometry, but is not required
    // to, so growing blindly is a caller bug, not a backend one.
    virtual void set_character_height(CharacterHandle character, float height) = 0;
    [[nodiscard]] virtual float character_height(CharacterHandle character) const = 0;

    // Instant placement without collision resolution (spawn, chunk streaming).
    virtual void teleport_character(CharacterHandle character, const glm::vec3& position) = 0;

    // Queries ------------------------------------------------------------------
    // direction must be unit length. Returns the closest hit within max_distance
    // among bodies/characters matching mask.
    [[nodiscard]] virtual RayHit raycast(const glm::vec3& origin, const glm::vec3& direction,
                                         float max_distance,
                                         CollisionMask mask = COLLIDE_ALL) const = 0;

    // Sweeps a SPHERE of `radius` from `origin` along `direction` (unit) for at
    // most `max_distance` metres and returns the FIRST contact.
    //
    // WHY A SWEPT VOLUME AND NOT A RAY. The caller that needs this is the
    // third-person camera boom: a ray is a zero-width probe, so it clears a
    // wall the camera's near plane is already inside of, and the frame shows
    // the room from outside the house. A sphere carries the width the eye
    // actually occupies, and the same query answers for a doorframe edge the
    // ray would slip past entirely.
    //
    // `distance` is measured to the SPHERE CENTRE at the moment of contact, so
    // placing the camera at `origin + direction * distance` already leaves
    // `radius` of air between it and the surface — no extra skin required for
    // the sphere itself. `position` is the contact point on the surface,
    // `normal` its outward normal, both as raycast() defines them.
    //
    // A sphere that already overlaps geometry at `origin` reports hit == true
    // with distance == 0: the caller is inside something, and the honest answer
    // is "you may not move at all", not "the way is clear".
    //
    // Null backend: always misses, like raycast().
    [[nodiscard]] virtual RayHit sphere_cast(const glm::vec3& origin,
                                             const glm::vec3& direction, float radius,
                                             float max_distance,
                                             CollisionMask mask = COLLIDE_ALL) const = 0;
};

} // namespace dfn::platform
