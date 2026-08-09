<!--
Created: 09:08:2026 - 01:02:15
Last updated: 09:08:2026 - 01:02:15
-->
<!--
UPD:
- 09:08:2026 - 01:02:15: Stage 2 — created: collision layer semantics + the
  HeightFieldView -> terrain body conversion.
-->

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
- `sources/TerrainCollision.h` — `create_terrain_body(physics, view, user_data,
  scratch)`: decodes the frozen height formula
  (`height = offset + raw * scale`) into reused scratch storage and creates one
  static terrain body per chunk on `LAYER_STATIC`.

## Usage example

```cpp
std::vector<float> scratch; // reuse across chunk loads (streaming path)
auto body = dfn::physics::create_terrain_body(physics, chunk_view,
                                              terrain_entity.packed(), scratch);
// on ChunkUnloaded: physics.destroy_body(body);
```

## Dependencies

- Uses: `engine/core/math` (HeightFieldView), `engine/platform/physics`
  interface, `dfn_headers`.
- Used by: `engine/gameplay` (layer constants for characters/raycasts),
  chunk-load handlers in `engine/app`, jolt-backed tests.
- Planned here (stage 2+): full `CharacterController` for NPCs, layer bits for
  interactables/projectiles (via sync).
