/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
Module: engine/core/ecs
File: engine/core/ecs/sources/EntityId.h

Responsibility:
- Generational entity identifier. Detects use-after-destroy: a slot index is reused,
  but the generation counter makes stale ids compare dead.

Key items:
- EntityId: {index, generation} pair with null sentinel and hashing support.
- GroupId: opaque batch/streaming group key (Rule 11, Q22) — chunk association.

Dependencies:
- Uses: <cstdint>, <functional>.
- Used by: World, ComponentPool, View, all systems and game code.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- POD only; must stay trivially copyable and backend-agnostic.
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — generational EntityId + GroupId, based on
  Quicky ECS with the streaming-group addition (Q22, Rule 11).
*/

#pragma once

#include <cstdint>
#include <functional>

namespace dfn::ecs {

/// Opaque streaming/batch group key (Rule 11). 0 = "no group".
/// engine/world packs chunk grid coordinates into a GroupId (see world::chunk_group);
/// the ECS itself treats it as an opaque 64-bit key.
using GroupId = uint64_t;

inline constexpr GroupId NO_GROUP = 0;

/// Generational entity identifier.
/// - `index` is a slot in World's entity storage, reused after destruction.
/// - `generation` increments on each reuse; comparison checks BOTH fields, so a
///   stale id held across a destroy never matches a live entity.
struct EntityId {
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;

    [[nodiscard]] constexpr bool operator==(const EntityId& o) const {
        return index == o.index && generation == o.generation;
    }
    [[nodiscard]] constexpr bool operator!=(const EntityId& o) const { return !(*this == o); }

    [[nodiscard]] constexpr bool is_null() const { return index == UINT32_MAX; }

    /// Sentinel meaning "no entity". Default-constructed EntityId equals null().
    [[nodiscard]] static constexpr EntityId null() { return {UINT32_MAX, 0}; }

    /// Packs the id into a single 64-bit value (serialization, hashing, logging).
    [[nodiscard]] constexpr uint64_t packed() const {
        return (static_cast<uint64_t>(index) << 32) | generation;
    }
};

/// Hash functor for unordered containers keyed by EntityId.
struct EntityIdHash {
    [[nodiscard]] std::size_t operator()(const EntityId& id) const {
        return std::hash<uint64_t>{}(id.packed());
    }
};

} // namespace dfn::ecs
