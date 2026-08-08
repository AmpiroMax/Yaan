<!--
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
-->
<!--
UPD:
- 09:08:2026 - 00:16:55: Stage 1 — public contract documented (headers only, no implementation yet).
-->

# engine/core/events

## Responsibility

Typed publish/subscribe event bus for decoupled cross-module notification.
Two modes: `publish` (synchronous, ordering-critical structural events like
`ChunkUnloaded`) and `post` + `pump` (queued gameplay events).

## Key types

- `EventBus` (`sources/EventBus.h`) — `subscribe<E>` / `unsubscribe` /
  `publish<E>` / `post<E>` / `pump`.
- `SubscriptionId` — typed handle returned by subscribe.

## Usage example

```cpp
dfn::events::EventBus bus;
auto sub = bus.subscribe<ChunkLoaded>([&](const ChunkLoaded& e) {
    terrain.build_mesh(chunks.heightfield(e.coord).value());
});
bus.publish(ChunkLoaded{coord});   // handlers run before this returns
bus.post(SkillUsed{npc, skill});   // delivered at the app loop's pump()
bus.pump();
```

## Dependencies

Uses `engine/core/types` (TypeId, Handle). Used by engine/world (chunk events),
gameplay (NpcSpoke, SkillUsed, ...), engine/app (wiring between sibling
modules). Single-threaded by contract, like `ecs::World`.
