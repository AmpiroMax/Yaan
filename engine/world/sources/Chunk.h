/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:42:03
Module: engine/world
File: engine/world/sources/Chunk.h

Responsibility:
- Chunk data types: chunk grid coordinates, the packed streaming group key, the
  owned heightmap storage (129x129 uint16 + per-chunk scale/offset, Q48), stable
  world entity ids, and the loaded-chunk aggregate.

Key items:
- ChunkCoord / chunk_group(): grid coordinate and its ecs::GroupId packing.
- Heightmap: owned height data; view() yields the agreed math::HeightFieldView.
- WorldEntityId: stable id of a generated entity (save-delta anchor, Q56).
- Chunk: one loaded chunk (heightmap + generated entity records).

Dependencies:
- Uses: engine/core/{math,ecs,config}, glm, std.
- Used by: ChunkManager, WorldFormat, Worldgen, SaveDelta.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure data (Rule 1: world = core only). No physics, no rendering, no IO here —
  IO lives in WorldFormat.h.
- Heightmap layout/formula is the frozen cross-zone contract in
  engine/core/math/sources/HeightField.h; this file must stay consistent with it.
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — chunk types per NUMBERS.md (Q48),
  group packing for batch ECS streaming (Q22, Rule 11), stable entity ids (Q56).
- 09:08:2026 - 00:42:03: Stage 2 — explicit size_t casts in height_at (generated
  constants are int64); no interface change.
*/

#pragma once

#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/EntityId.h"
#include "engine/core/math/sources/HeightField.h"

#include <cstdint>
#include <glm/vec2.hpp>
#include <vector>

namespace dfn::world {

/// Chunk grid coordinate. World-space origin of chunk (x, z) is
/// (x * CHUNK_SIZE, z * CHUNK_SIZE); the grid is signed and sparse.
struct ChunkCoord {
    int32_t x = 0;
    int32_t z = 0;

    [[nodiscard]] constexpr bool operator==(const ChunkCoord& o) const {
        return x == o.x && z == o.z;
    }
    [[nodiscard]] constexpr bool operator!=(const ChunkCoord& o) const { return !(*this == o); }
};

/// Packs a chunk coordinate into the opaque ecs::GroupId used by the batch
/// spawn/destroy API (Rule 11). Bit 63 is set so no chunk group ever collides
/// with ecs::NO_GROUP (0) or small hand-assigned group ids.
[[nodiscard]] constexpr ecs::GroupId chunk_group(ChunkCoord c) {
    return (1ull << 63)
         | (static_cast<uint64_t>(static_cast<uint32_t>(c.x)) << 32)
         | static_cast<uint64_t>(static_cast<uint32_t>(c.z));
}

/// Inverse of chunk_group(). Precondition: `group` was made by chunk_group().
[[nodiscard]] constexpr ChunkCoord chunk_of_group(ecs::GroupId group) {
    return ChunkCoord{static_cast<int32_t>(static_cast<uint32_t>(group >> 32)),
                      static_cast<int32_t>(static_cast<uint32_t>(group))};
}

/// The chunk containing world-space position (x, z) in meters.
[[nodiscard]] ChunkCoord chunk_at_position(glm::vec2 world_xz);

/// Owned height data of one chunk (HEIGHT_FORMAT, Q48): raw uint16 samples plus
/// per-chunk decode scale/offset. ~33 KB per chunk. Layout and decode formula
/// are defined by the frozen math::HeightFieldView contract.
struct Heightmap {
    /// resolution^2 raw samples, row-major, x fastest (see HeightFieldView).
    std::vector<uint16_t> samples;
    float height_scale = 0.0f;  ///< meters per raw unit ((max - min) / 65535).
    float height_offset = 0.0f; ///< meters (chunk minimum height).

    /// The agreed non-owning cross-zone view for `coord`'s data (render meshing,
    /// physics terrain). Valid while this Heightmap is alive and unmodified.
    [[nodiscard]] math::HeightFieldView view(ChunkCoord coord) const;

    /// Decoded height in meters at integer sample (x, z). Bounds unchecked.
    [[nodiscard]] float height_at(uint32_t x, uint32_t z) const {
        const std::size_t row = static_cast<std::size_t>(config::HEIGHTMAP_RESOLUTION);
        return height_offset
             + static_cast<float>(samples[static_cast<std::size_t>(z) * row + x]) * height_scale;
    }

    /// Bilinearly interpolated height at world position (meters), given the
    /// owning chunk's coord. Clamps to the chunk's edge samples.
    [[nodiscard]] float sample_world(ChunkCoord coord, glm::vec2 world_xz) const;
};

/// Stable identifier of a WORLD-GENERATED entity, assigned deterministically by
/// worldgen (Rule 13.1) and persisted in the world file. Save deltas reference
/// generated entities by this id, never by runtime ecs::EntityId (Q56).
/// 0 = invalid. Runtime-spawned dynamic entities have no WorldEntityId.
using WorldEntityId = uint64_t;

/// One generated entity record inside a chunk (a tree, an NPC spawn, a chest...).
/// `archetype` names a content-defined archetype (fnv1a64 of its data-file id);
/// gameplay systems instantiate components from the archetype at chunk load.
struct GeneratedEntityRecord {
    WorldEntityId world_id = 0;
    uint64_t archetype = 0;        ///< content id hash (serialization::fnv1a64).
    glm::vec2 position_xz{0.0f};   ///< meters, world space; y from terrain.
    float yaw = 0.0f;              ///< radians.
};

/// One loaded chunk: pure data, produced by the world file reader (plus the
/// save-delta overlay) and owned by ChunkManager.
struct Chunk {
    ChunkCoord coord;
    Heightmap heightmap;
    std::vector<GeneratedEntityRecord> entities;
};

} // namespace dfn::world
