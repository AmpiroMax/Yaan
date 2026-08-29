/*
Module: engine/platform/physics
File: engine/platform/physics/interfaces/IPhysics.h

Responsibility:
- The platform physics contract (Rule 0). Everything the engine may ask of a
  physics backend goes through this interface; Jolt lives only behind it.

Key items:
- IPhysics: init/shutdown, fixed step, static terrain/box bodies, DYNAMIC
  bodies (loose props: convex hull, mass, sleep), kinematic character
  controller (capsule, move-with-slide, grounded query), raycast.
- TerrainMeshDesc: voxel-terrain collision from extracted triangles (tunnels,
  overhangs) — the terrain path for the 3D world.
- TerrainDesc: heightmap collision from plain float samples (engine/world data).
- CharacterDesc / RayHit: plain-data descriptors, no backend types.
- PhysicsBodyHandle / CharacterHandle: opaque POD handles (0 = invalid).

Dependencies:
- Uses: C++ stdlib, glm (Rule 2), engine/core (PhysicsSubstance — the DAG
  allows platform interfaces to reach core, and a body must be able to NAME
  its substance rather than carry loose coefficients).
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

#pragma once

#include "engine/core/materials/sources/PhysicsSubstance.h"

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
    // What the GROUND is made of. Matters the moment anything dynamic rests on
    // it: a bowl on a floor slides by the pair's friction, and half that pair
    // is the floor. SUBSTANCE_DEFAULT reproduces Jolt's own defaults exactly,
    // so a caller that says nothing gets today's world unchanged.
    core::SubstanceId substance = core::SUBSTANCE_DEFAULT;
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
    core::SubstanceId substance = core::SUBSTANCE_DEFAULT; // see TerrainDesc
    uint64_t user_data = 0;               // EntityId bits (chunk terrain entity)
};

// Static box collider (buildings, dungeon prefab pieces, large props).
struct StaticBoxDesc {
    glm::vec3 center{0.0f};                       // world space, meters
    glm::vec3 half_extents{0.0f};                 // meters
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    CollisionMask layer = 0;
    uint64_t user_data = 0;                       // EntityId bits
    core::SubstanceId substance = core::SUBSTANCE_DEFAULT; // see TerrainDesc
};

// DYNAMIC BODY: a loose prop the world may knock about (a jug, a stool, a
// crate). Unlike a static box it has MASS and is integrated by the solver;
// unlike the character capsule it tumbles.
//
// SHAPE IS A CONVEX HULL OF `points`, and that is a deliberate narrowing of
// what Jolt can do: the prop the player grabs is drawn from ONE baked mesh, so
// the honest shape is that mesh's own vertices rather than a box typed in
// beside it — the same "what you see is what you touch" rule the trees and the
// door leaf already pay for. Concavity is lost (a basket carries no inside),
// and that is the price of one shape per prop instead of a decomposition; a
// prop whose hollow interior must hold something is a COMPOUND, and that is a
// later, additive call rather than a re-reading of this one.
//
// `points` are in the body's LOCAL frame — the same frame the drawn mesh uses,
// with the origin the object registry measures from — so the drawn transform
// and the body transform are the same matrix.
//
// A hull that cannot be built (fewer than 4 points, all coplanar, zero mass)
// yields an INVALID handle and creates nothing: an invisible body with no
// volume would be a prop that catches the player and never appears.
struct DynamicBodyDesc {
    std::span<const glm::vec3> points; // hull points, LOCAL space, meters
    glm::vec3 position{0.0f};          // world space, meters
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    // MASS IS THE CALLER'S, AND IT IS NOT THE HULL'S VOLUME TIMES A DENSITY.
    // Read that sentence twice, because the honest reading of the obvious one
    // is a world of two-hundred-kilogram furniture: `points` above are a
    // CONVEX HULL, and a chair's convex hull is very nearly its bounding box —
    // 0.217 m^3, which at pine's density is 152 kg for a chair that weighs 5.
    // Measured on the shelf: chest 217 kg against a real 15-25, table 907
    // against 25-40. And the symptom is silent: "the chair will not lift"
    // reads as a grab-force problem, not as a mass problem.
    //
    // The mass a caller passes is SIGNED MESH VOLUME (divergence theorem over
    // the object's own triangles) times the substance density times a FILL
    // fraction, with a loud refusal when the volume ratio says the shell is
    // not stitched. That arithmetic lives in engine/app (PropPhysics.h) and
    // belongs in the forge that bakes the object, not here — but the contract
    // states the rule, because this field is where the trap is sprung.
    float mass_kg = 0.0f;              // > 0, in kilograms
    // WHAT THIS BODY IS MADE OF — a NAME into the substance table, never loose
    // coefficients on the call. Friction and restitution are read from that
    // record; a caller cannot type "friction 0.5" here at all, on purpose.
    //
    // WHY. Friction is not a property of oak, it is a property of oak ON
    // stone, and the industry stores the number on the substance and combines
    // it by a rule. Put the number on the CALL instead and the rule has no
    // owner: two call sites disagree about oak and nobody can be asked why.
    // (This wave shipped it on the call first and was stopped while the
    // consumer count was still one — the same defect the codebase already
    // named on DrawParams.roughness.)
    //
    // THE COMBINE RULE IS JOLT'S DEFAULT, and it is stated in
    // PhysicsSubstance.h once: friction sqrt(f1*f2), restitution max(r1,r2).
    core::SubstanceId substance = core::SUBSTANCE_DEFAULT;
    // LINEAR/ANGULAR DAMPING. Not decoration: a stack that never damps keeps
    // micro-velocity from the solver's own residual and reads as a shiver.
    float linear_damping = 0.05f;
    float angular_damping = 0.05f;
    CollisionMask layer = 0;
    uint64_t user_data = 0;            // EntityId bits, or the caller's own tag
    // ASLEEP AT BIRTH. A room of thirty props that all wake on load costs a
    // simulated second of nothing happening; a prop is woken by a touch.
    bool start_asleep = true;
};

// Where a body IS. Position is the body ORIGIN (the frame `points` were given
// in), never the centre of mass: the caller draws from this matrix.
struct BodyPose {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
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
    // HOW HARD THE WALKING BODY SHOVES A LOOSE PROP, newtons. This is the
    // SECOND of the two ceilings the prop wave needs, and it is deliberately
    // not the first: how much the player can CARRY (the grab spring's force
    // limit) and how much he shoulders aside while walking are different
    // questions, and one knob for both makes "I cannot lift the cupboard" and
    // "I cannot kick the cupboard out of the way" the same sentence.
    //
    // Expressed as a FORCE and not as Skyrim's mass ceiling (fMoveLimitMass —
    // the one parameter of theirs that is publicly confirmed) because that is
    // the knob the solver actually has: a fixed force against a body's own
    // mass already produces "the bottle skitters, the barrel does not", with
    // no table of weight classes to keep in sync with the substance table.
    float push_force_n = 100.0f;
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

    // Dynamic bodies -----------------------------------------------------------
    // A loose prop: mass, gravity, contacts, sleep. Invalid handle on a zero
    // layer, a non-positive mass, or a hull that cannot be built.
    [[nodiscard]] virtual PhysicsBodyHandle create_dynamic_body(
        const DynamicBodyDesc& desc) = 0;

    // Post-step pose of ANY body (static or dynamic). Identity for an invalid
    // handle — a caller with no body draws nothing, which is not an error.
    [[nodiscard]] virtual BodyPose body_pose(PhysicsBodyHandle body) const = 0;

    // Velocity of a dynamic body, m/s and rad/s. Zero for anything else.
    [[nodiscard]] virtual glm::vec3 body_velocity(PhysicsBodyHandle body) const = 0;

    // DRIVES a dynamic body by writing its velocity, and WAKES it.
    //
    // WHY VELOCITY AND NOT A KINEMATIC TRANSFORM, for the held item. A grabbed
    // prop must still be stopped by a wall and must still shove the cups on
    // the table it is swept across; a kinematic body does neither — it walks
    // through the wall and the wall does not answer. Writing the velocity of a
    // body that stays DYNAMIC keeps every contact honest and makes "the jug
    // jams against the doorframe and slips out of the hand" a consequence
    // rather than a special case.
    virtual void set_body_velocity(PhysicsBodyHandle body, const glm::vec3& linear,
                                   const glm::vec3& angular) = 0;

    // Scales gravity for one body: 1 = normal, 0 = weightless. The held prop
    // is carried at 0 so the spring does not have to fight its own weight, and
    // is handed back its weight the instant it is let go.
    virtual void set_body_gravity_factor(PhysicsBodyHandle body, float factor) = 0;

    // Is this body asleep (removed from the solver until touched)? THE stack
    // acceptance reads this: "three bowls stand still" and "three bowls are
    // asleep" are different claims, and only the second one is cheap.
    [[nodiscard]] virtual bool body_asleep(PhysicsBodyHandle body) const = 0;

    // Wakes a sleeping body without changing its state.
    virtual void activate_body(PhysicsBodyHandle body) = 0;

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
