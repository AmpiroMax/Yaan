
# engine/physics

## Responsibility

Engine-level physics on top of the `IPhysics` interface: the meaning of the
opaque `CollisionMask` bits, and the bridge from world heightmap data
(`math::HeightFieldView`, raw uint16 + scale/offset) to physics terrain bodies.
Never includes backend headers (Rule 1).

## Key types

- `sources/CollisionLayers.h` — `LAYER_STATIC` (terrain, buildings, prefabs),
  `LAYER_CHARACTER` (player + NPCs). Backends store & AND these bits blindly;
  extending the set is an engine-side change only.
- `sources/TerrainCollision.h`:
  - `create_terrain_mesh_body(physics, voxel_mesh_view, user_data)` — THE
    terrain call for the voxel world: one static mesh body per chunk on
    `LAYER_STATIC`, built from core's extracted surface triangles. Represents
    tunnels/caves/overhangs. An invalid handle means "chunk has no triangles",
    not an error. Collision uses the render-resolution extraction (VOXEL_SIZE
    1 m); see the measurement note below.
  - `create_terrain_body(physics, height_view, user_data, scratch)` — legacy
    heightmap path: decodes the frozen formula (`height = offset + raw*scale`)
    into reused scratch and creates a heightfield-derived body. CANNOT
    represent overhangs; kept for heightfield worlds and tests.

## Usage example

```cpp
// Voxel world (current): on ChunkLoaded
if (const auto mesh = chunks.voxel_mesh(coord)) {
    auto body = dfn::physics::create_terrain_mesh_body(physics, *mesh,
                                                       terrain_entity.packed());
    if (body.valid()) { /* store per coord */ }  // invalid = empty chunk, skip
}
// on ChunkUnloaded: physics.destroy_body(stored_body);
```

Measured collision cost (seed 1 testbed, load radius 2, 12 resident chunks):
~143k triangles per chunk, ~68 ms of Jolt `MeshShape` build per chunk (813 ms
for the initial 12). Correctness first — a ~2 m coarse extraction would cut
that ~4x but risks the tunnel's 3.24 m minimum headroom against a 1.8 m capsule
and 0.35 m step, and no coarse extraction API exists yet. Revisit next stage
with core (coarse extraction and/or off-thread shape building).

## Dependencies

- Uses: `engine/core/math` (HeightFieldView), `engine/platform/physics`
  interface, `dfn_headers`.
- Used by: `engine/gameplay` (layer constants for characters/raycasts),
  chunk-load handlers in `engine/app`, jolt-backed tests.
- Planned here (stage 2+): full `CharacterController` for NPCs, layer bits for
  interactables/projectiles (via sync).
