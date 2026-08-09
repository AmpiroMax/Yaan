<!--
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:42:03
-->
<!--
UPD:
- 09:08:2026 - 00:16:55: Stage 1 — public contract documented (headers only, no implementation yet).
- 09:08:2026 - 00:42:03: Stage 2 — implemented (header-only templates + World.cpp for non-template internals); doctest suite tests/core/EcsTests.cpp.
-->

# engine/core/ecs

## Responsibility

Sparse-set ECS with generational entity ids, evolved from Quicky (Q17, Q22): entity
lifecycle, component pools, multi-component views, resources, plus the streaming
additions — batch spawn/destroy and an entity-to-group (chunk) index (Rule 11).

## Key types

- `EntityId` (`sources/EntityId.h`) — `{index, generation}` id; `GroupId` — opaque
  streaming group key (chunks pack their coords into it).
- `IComponentPool` / `ComponentPool<T>` (`sources/ComponentPool.h`) — dense
  sparse-set storage, swap-and-pop removal, batch removal.
- `View<Ts...>` (`sources/View.h`) — intersection query driven by the smallest pool.
- `World` (`sources/World.h`) — the registry: spawn/destroy (+deferred, +batch),
  component CRUD (+`add_batch`), `view<Ts...>()`, resources, group index
  (`set_group` / `group_of` / `entities_in_group` / `destroy_group`).

## Usage example

```cpp
dfn::ecs::World world;
std::array<dfn::ecs::EntityId, 64> ids;
world.spawn_batch(ids, dfn::world::chunk_group({3, -2}));   // chunk load (Rule 11)
world.add_batch<Transform>(ids, Transform{});
for (auto [id, tf, vel] : world.view<Transform, Velocity>()) {
    tf.position += vel.value * dt;
}
world.flush_destroyed();
world.destroy_group(dfn::world::chunk_group({3, -2}));      // chunk unload
```

## Dependencies

Uses `engine/core/types` (TypeId). Used by every system, `engine/world`
(ChunkManager streaming), `engine/app`, tests. Implemented in stage 2:
template bodies in-header, non-template internals in `sources/World.cpp`
(builds into `dfn_core`); covered by `tests/core/EcsTests.cpp`.
