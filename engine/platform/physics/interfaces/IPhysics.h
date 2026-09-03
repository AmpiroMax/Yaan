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
- PhysicsBodyHandle / CharacterHandle / RagdollHandle: opaque POD handles
  (0 = invalid).
- FootBodyDesc / FootMode / FootContact: PHYSICAL FEET (LOCOMOTION_GROUNDED
  §12) — a kinematic box in the swing, a dynamic body held by friction when
  planted; the contact query answers "holds / slips" by Coulomb.
- CharacterContact + character_add_impulse / character_velocity /
  character_contacts: the capsule takes impulses (dv = J / m) and reports who
  it touched and who moved whom (HIT_REACTIONS_PHYSICS §3).
- RagdollDesc / RagdollPartDesc: a chain of dynamic parts with swing-twist
  joints, posed from the skeleton, driven back to a pose by motors.

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
  Feet on null stand on the same flat plane the capsule glides on: always
  touching, normal +Y, holding, slip zero; a planted foot stays where the
  swing left it. Impulses give the contracted dv; contacts are never reported;
  a ragdoll keeps the pose it was given.
- HOW THIS CONTRACT GROWS (Rule 26 allows growth, not reshaping): a new
  question is a NEW virtual with a body that answers "not here" (an invalid
  handle, an empty span, a zero) and a new descriptor field carries a default
  that reproduces yesterday's behaviour — exactly as TextureParams.mip_chain
  and SkinnedVertex grew the render contract. Every signature that existed
  before the feet, the impulses and the ragdoll still exists unchanged, and a
  backend (or a test double) that overrides none of the new calls still
  compiles and still answers honestly.

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
    // MASS OF THE WALKING BODY, kilograms: the m in dv = J / m of
    // character_add_impulse, and the weight the capsule sets against a body
    // that shoves it. 0 means "the registry's number" (CHARACTER_MASS_KG): an
    // existing caller that never heard of mass gets the same body everyone
    // else gets, never a weightless one. Immunity to a shove is THIS number
    // and nothing else — a heavy thing is hard to move because it is heavy.
    float mass_kg = 0.0f;
};

struct RayHit {
    bool hit = false;
    float distance = 0.0f;    // meters from origin along the ray
    glm::vec3 position{0.0f}; // world-space hit point
    glm::vec3 normal{0.0f};   // unit surface normal at the hit
    uint64_t user_data = 0;   // EntityId bits of the hit body/character, 0 = none
};

// --- Physical feet (docs/design/LOCOMOTION_GROUNDED.md §11–§12) ---------------
//
// A FOOT IS A BODY, NOT A LOCK. The foot lock closed the gap between the clip
// and the ground by pulling the foot back to an anchor; the owner's order
// (04.09) replaces it with a foot that is a physical object on physical
// ground, held by friction: on a gentle slope it stands, on a steep one it
// slides — by the friction law, not by an animation of sliding.
//
// Two modes. SWING: the foot is KINEMATIC; the animation owns it and moves it
// with set_foot_kinematic_pose(); it still touches the world (a swinging foot
// kicks a cup, and foot_contact() reports the landing). PLANT: the foot is
// DYNAMIC with its translation free and its rotation locked; gravity presses
// it into the ground, the pair's friction (sole substance x ground substance,
// sqrt(f1*f2) — the rule stated once in PhysicsSubstance.h) holds it or lets
// it creep, and the solver — not a special case — decides which.
//
// The foot collides with the WORLD (terrain, statics, loose props) and with
// nothing that belongs to its own body: not the capsule, not the hitboxes.
// `collides_with` names the world; `layer` is the foot's own bit so a query
// can ask for feet or skip them.
struct FootBodyDesc {
    // Box half extents, metres: x sideways, y half the sole's thickness,
    // z fore-and-aft — the foot hitbox's own numbers (anim::HitboxSlot).
    glm::vec3 half_extents{0.0f};
    glm::vec3 position{0.0f}; // body ORIGIN = box centre, world space
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    // THE LOAD THE PLANTED FOOT CARRIES, kilograms (FOOT_BODY_MASS_KG). Not
    // the mass of a foot: the friction law cancels mass out of "holds or
    // slides", but a prop resting on the foot and a prop the foot lands on
    // both feel the body's weight through it.
    float mass_kg = 0.0f;
    // WHAT THE SOLE IS MADE OF — a name into the substance table, never a
    // coefficient (the same rule as DynamicBodyDesc, for the same reason).
    core::SubstanceId substance = core::SUBSTANCE_DEFAULT;
    CollisionMask layer = 0;                    // the foot's own bit
    CollisionMask collides_with = COLLIDE_ALL;  // the world it may touch
    uint64_t user_data = 0;                     // EntityId bits of the owner
};

enum class FootMode : uint8_t {
    Swing = 0, // kinematic, animation-driven
    Plant = 1, // dynamic, held by friction
};

// What the foot is standing on, and whether it stays there. Valid after
// step(); computed from the deepest contact of the foot's shape against the
// bodies it may touch (plus FOOT_BODY_SKIN_M of air, so a foot resting
// exactly on the surface still counts as touching).
struct FootContact {
    bool touching = false;
    glm::vec3 point{0.0f};        // contact point on the ground, world space
    glm::vec3 normal{0.0f};       // ground's outward unit normal at the contact
    float depth = 0.0f;           // penetration (+) or gap (−), metres
    core::SubstanceId ground = core::SUBSTANCE_DEFAULT; // what it stands on
    uint64_t ground_user_data = 0;                     // whose body that is
    // COULOMB. slope_tan = tan of the ground's angle from horizontal at the
    // contact; friction_pair = sqrt(mu_sole * mu_ground); holds iff
    // slope_tan <= friction_pair. This is the CRITERION, stated so a caller
    // can predict the solver — the solver's own answer is slip_speed_mps.
    float slope_tan = 0.0f;
    float friction_pair = 0.0f;
    bool holds = false;
    // Velocity of the foot relative to the ground, tangent to it: zero when
    // the foot stands, the creep when it slides. Measured, not predicted.
    glm::vec3 slip_velocity{0.0f};
    float slip_speed_mps = 0.0f;
};

// --- Character impulses and contacts (docs/design/HIT_REACTIONS_PHYSICS.md §3)
//
// One contact the capsule had during the last step(): who it touched, where,
// and which way the push went. Whether the body moves the character or the
// character moves the body is decided by MASS against CHARACTER_PUSH_MASS_KG:
// lighter than that, the body is shoved and reports pushed_body; heavier (or
// static), it shoves the capsule and reports pushed_character. A plank
// swinging into the player moves him; the player walking into a bowl moves
// the bowl; nothing is on a list.
struct CharacterContact {
    uint64_t user_data = 0;          // EntityId bits of the body touched
    glm::vec3 point{0.0f};           // world space
    glm::vec3 normal{0.0f};          // unit, pointing INTO the character
    glm::vec3 relative_velocity{0.0f}; // body velocity − character velocity
    float mass_kg = 0.0f;            // the body's mass; +inf for a static body
    bool pushed_character = false;   // the body's velocity moved the capsule
    bool pushed_body = false;        // the capsule was allowed to move the body
};

// --- Ragdoll -------------------------------------------------------------------
//
// A ragdoll is the hitbox set made dynamic: one body per part on the same
// bones, a swing-twist joint to the parent with limits from the descriptor,
// parent and child never colliding with each other. It is posed from the
// skeleton (set_ragdoll_pose), read back into it (ragdoll_pose), struck
// (ragdoll_add_impulse) and, while the body is alive, driven back towards a
// pose by joint motors (ragdoll_drive_to_pose) — strength 1 holds the pose
// against gravity, 0 is a dead body.
struct RagdollHandle {
    uint32_t id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
};

struct RagdollPartDesc {
    int32_t parent = -1;              // index into RagdollDesc::parts, −1 = root
    // Shape in the PART's own frame: a box of half_extents, or a sphere of
    // radius when radius > 0 (the skull). Same two shapes as the hitboxes.
    glm::vec3 half_extents{0.0f};
    float radius = 0.0f;
    BodyPose pose;                    // world pose at creation (the hitbox's)
    float mass_kg = 0.0f;             // > 0
    core::SubstanceId substance = core::SUBSTANCE_DEFAULT;
    uint64_t user_data = 0;           // EntityId bits + BodyPart, caller's tag
    // Joint to the parent, in WORLD space at creation: the anchor (the rig
    // joint), the twist axis (along the child segment) and a plane axis
    // perpendicular to it; a cone of swing_limit_rad around the twist axis,
    // twist between twist_min_rad and twist_max_rad. Ignored on the root.
    glm::vec3 joint_position{0.0f};
    glm::vec3 twist_axis{0.0f, 1.0f, 0.0f};
    glm::vec3 plane_axis{1.0f, 0.0f, 0.0f};
    float swing_limit_rad = 0.0f;
    float twist_min_rad = 0.0f;
    float twist_max_rad = 0.0f;
};

struct RagdollDesc {
    std::span<const RagdollPartDesc> parts; // parents before children
    CollisionMask layer = 0;                // the ragdoll's own bit
    CollisionMask collides_with = COLLIDE_ALL;
    uint64_t user_data = 0;
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

    // Physical feet ---------------------------------------------------------------
    // Invalid handle on a zero layer / zero collides_with, non-positive half
    // extents or mass. The handle is a body: body_pose(), body_velocity() and
    // destroy_body() work on it. Born in Swing mode at `position`.
    [[nodiscard]] virtual PhysicsBodyHandle create_foot_body(const FootBodyDesc& desc) {
        (void)desc;
        return {};
    }

    // Where the animation wants the foot at the END of the next step(). The
    // body moves there kinematically during that step — through the props in
    // its way, which it shoves (a foot in the swing is not stopped by a cup).
    // Ignored while the foot is planted: physics owns a planted foot, and a
    // caller that wants it back must set Swing first. Without a new pose the
    // foot stays where its last one put it.
    virtual void set_foot_kinematic_pose(PhysicsBodyHandle foot, const BodyPose& pose) {
        (void)foot;
        (void)pose;
    }

    // Swing → Plant: the foot becomes dynamic AT REST where it is (the swing's
    // residual velocity is dropped: the animation said "down", and a foot that
    // lands is not thrown). Plant → Swing: kinematic again, holding its pose.
    virtual void set_foot_mode(PhysicsBodyHandle foot, FootMode mode) {
        (void)foot;
        (void)mode;
    }
    [[nodiscard]] virtual FootMode foot_mode(PhysicsBodyHandle foot) const {
        (void)foot;
        return FootMode::Swing;
    }

    // Post-step contact of the foot with the world it may touch; see
    // FootContact. Not touching → every other field is zero/default.
    [[nodiscard]] virtual FootContact foot_contact(PhysicsBodyHandle foot) const {
        (void)foot;
        return {};
    }

    // Character impulses and contacts -----------------------------------------
    // Adds dv = impulse / mass to the capsule's velocity for the coming
    // step()s. The velocity persists and decays with CHARACTER_PUSH_DECAY_S
    // (a standing body catches itself; a flying one does not — the decay runs
    // only while grounded). Velocity is also clipped by the world as always:
    // a shove into a wall moves nobody.
    virtual void character_add_impulse(CharacterHandle character, const glm::vec3& impulse_ns) {
        (void)character;
        (void)impulse_ns;
    }
    // The capsule's velocity over the last step(), m/s: displacement asked
    // for plus the push it carries, after collide-and-slide.
    [[nodiscard]] virtual glm::vec3 character_velocity(CharacterHandle character) const {
        (void)character;
        return glm::vec3{0.0f};
    }
    [[nodiscard]] virtual float character_mass(CharacterHandle character) const {
        (void)character;
        return 0.0f;
    }
    // Contacts of the last step(), in the order the solver found them. The
    // span is valid until the next step() or destroy_character().
    [[nodiscard]] virtual std::span<const CharacterContact> character_contacts(
        CharacterHandle character) const {
        (void)character;
        return {};
    }

    // Ragdoll -----------------------------------------------------------------
    // Invalid handle on a zero layer, an empty part list, a part with a parent
    // at or after itself, or a part without volume or mass.
    [[nodiscard]] virtual RagdollHandle create_ragdoll(const RagdollDesc& desc) {
        (void)desc;
        return {};
    }
    virtual void destroy_ragdoll(RagdollHandle ragdoll) { (void)ragdoll; }

    // Places every part (world poses, one per part, in descriptor order) and
    // zeroes their velocities: the skeleton's pose becomes the ragdoll's.
    virtual void set_ragdoll_pose(RagdollHandle ragdoll, std::span<const BodyPose> parts) {
        (void)ragdoll;
        (void)parts;
    }
    // Reads every part's post-step pose back (one per part, descriptor order).
    // Writes nothing past the span's end and nothing for an invalid handle.
    virtual void ragdoll_pose(RagdollHandle ragdoll, std::span<BodyPose> out) const {
        (void)ragdoll;
        (void)out;
    }
    // Strikes one part with an impulse (N·s) at a world point, waking it.
    virtual void ragdoll_add_impulse(RagdollHandle ragdoll, uint32_t part,
                                     const glm::vec3& impulse_ns, const glm::vec3& at_world) {
        (void)ragdoll;
        (void)part;
        (void)impulse_ns;
        (void)at_world;
    }
    // Joint motors pull the ragdoll towards `target` (world poses per part —
    // only the RELATIVE orientation parent→child is used). strength in [0, 1]:
    // 0 switches the motors off (a dead body), 1 is RAGDOLL_MOTOR_TORQUE_NM
    // per joint. Persists until the next call.
    virtual void ragdoll_drive_to_pose(RagdollHandle ragdoll, std::span<const BodyPose> target,
                                       float strength) {
        (void)ragdoll;
        (void)target;
        (void)strength;
    }
    // All parts asleep: the body has come to rest.
    [[nodiscard]] virtual bool ragdoll_asleep(RagdollHandle ragdoll) const {
        (void)ragdoll;
        return true;
    }
};

} // namespace dfn::platform
