
# engine/core/types

## Responsibility

Foundational vocabulary types: RTTI-free type identification and the generic
typed handle.

## Key types

- `TypeId`, `type_id<T>()` (`sources/TypeId.h`) — process-local unique id per
  type, counter-based, no RTTI. Never serialized (not stable across runs).
- `Handle<Tag, Storage>` (`sources/Handle.h`) — POD typed id, 0 = invalid;
  discriminating `Tag` prevents cross-domain mixups. `HandleHash` for maps.

## Usage example

```cpp
struct TerrainMeshTag;
using TerrainMeshHandle = dfn::types::Handle<TerrainMeshTag>;

TerrainMeshHandle h{42};
if (h.valid()) { cache[h.value] = mesh; }
```

## Dependencies

Uses std only. Used by ecs::World (pool/resource keys), events::EventBus (event
keys), components referencing platform resources by value (Rule 8).
