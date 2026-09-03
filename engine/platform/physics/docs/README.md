
# engine/platform/physics

## Responsibility

The platform physics contract (Rule 0): fixed-step world, static terrain/box
collision, kinematic capsule character controller, raycasts. Jolt Physics lives
only behind `interfaces/IPhysics.h`.

## Key types

- `IPhysics` — init/shutdown, `step(SIM_DT)`, `create_terrain_mesh` (voxel
  terrain: tunnels/overhangs) / `create_terrain` (legacy heightmap) /
  `create_static_box` / `destroy_body`, character create/move/teleport/queries,
  `raycast`.
- `TerrainDesc`, `StaticBoxDesc`, `CharacterDesc`, `RayHit` — plain-data descriptors.
- `FootBodyDesc` / `FootMode` / `FootContact` — physical feet: `create_foot_body`,
  `set_foot_kinematic_pose` (swing), `set_foot_mode` (Plant = dynamic, held by
  friction), `foot_contact` (holds / slips by Coulomb, what it stands on).
- `CharacterContact`, `character_add_impulse` / `character_velocity` /
  `character_contacts` — the capsule takes dv = J / m and reports who pushed whom.
- `RagdollDesc` / `RagdollPartDesc` / `RagdollHandle` — parts with swing-twist
  joints, posed, struck, driven by motors.
- `PhysicsBodyHandle`, `CharacterHandle`, `RagdollHandle` — opaque POD handles (0 = invalid).
- Growth rule: new calls have default bodies, new fields have defaults that
  reproduce the old behaviour (see the header note) — a backend or test double
  that overrides nothing new still compiles.
- `CollisionMask` — opaque bits; semantics defined by `engine/physics` (stage 2).

## Usage example

```cpp
dfn::platform::TerrainDesc terrain{ /* from world HeightFieldView, floats */ };
auto body = physics.create_terrain(terrain);
auto character = physics.create_character(desc); // desc from NUMBERS constants
physics.move_character(character, displacement); // intent for this tick
physics.step(SIM_DT);                            // once per fixed tick (Rule 12)
bool grounded = physics.character_grounded(character);
```

## Dependencies

- Uses: stdlib + glm only (Rule 1).
- Used by: `engine/physics` (controller, layers), `engine/gameplay` (queries via
  engine/physics), tests.
- Backends (implemented, stage 2): `sources/jolt/` (JoltPhysics v5.2.0,
  FetchContent-pinned in this layer's CMakeLists) and `sources/null/` —
  runnable mode: no-op step, full horizontal displacement, always grounded,
  raycasts miss (see interface header notes). Factories:
  `create_jolt_physics()` / `create_null_physics()`.
- Jolt backend notes: terrain is a static MeshShape built from the float
  samples (Jolt's HeightFieldShape block-size constraint does not fit our
  129x129 chunks — revisit at a sync); characters are CharacterVirtual with a
  raycastable inner body; the opaque CollisionMask is stored per body and
  filtered by AND — the backend never interprets bits (engine/physics owns
  semantics). Feet are kinematic/dynamic boxes on their own Jolt layer
  (world + props only, never the capsule); their masks also gate solver pairs
  (`MaskContactListener`); ragdolls are dynamic parts + `SwingTwistConstraint`
  with position motors. Numbers and the contract note: engine/physics/docs/README.md.
