/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
Module: engine/world
File: engine/world/sources/SaveDelta.h

Responsibility:
- Save-game types (Q56): a save is a DELTA against the generated world, never a
  world copy. Defines the delta data model, the .dfs container (magic, sections)
  and the codec, including the registration hook through which gameplay
  contributes its own sections (agreed with sim, Rule 26).

Key items:
- EntityDelta / ChunkDelta: world-side delta records keyed by WorldEntityId.
- SaveDelta: in-memory delta model the ChunkManager overlays at chunk load.
- SaveSectionHooks / SaveDeltaCodec: container IO + module section registry.

Dependencies:
- Uses: Chunk.h, engine/core/{serialization,ecs}.
- Used by: ChunkManager (overlay), engine/app (save/load flow), gameplay
  (registers its sections at startup), save tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 7 in full: sections, little-endian, skip-unknown, migration from day one.
- Unknown sections are PRESERVED on read + rewrite (a save touched by an older
  build must not lose a newer build's data).
- Never serialize runtime ecs::EntityId — only WorldEntityId (generated) or
  per-save DynamicEntityId (spawned) are stable.
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — delta model + codec with gameplay
  section hooks (Q56); hook shape agreed with sim (Rule 26).
*/

#pragma once

#include "engine/core/ecs/sources/World.h"
#include "engine/core/serialization/sources/BinaryReader.h"
#include "engine/core/serialization/sources/BinaryWriter.h"
#include "engine/world/sources/Chunk.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <glm/vec2.hpp>
#include <memory>
#include <vector>

namespace dfn::world {

/// 'DFNS' — Daggerfall N save file (.dfs).
inline constexpr uint32_t SAVE_MAGIC = serialization::make_tag('D', 'F', 'N', 'S');
inline constexpr uint32_t SAVE_FORMAT_VERSION = 1;

/// World-owned section tags of the save container. Gameplay modules register
/// additional tags via SaveSectionHooks; tag uniqueness is coordinated in the
/// group sync (each module's tags are listed in its spec).
namespace save_section {
inline constexpr serialization::SectionTag META = serialization::make_tag('M', 'E', 'T', 'A');
inline constexpr serialization::SectionTag ENTITY_DELTAS = serialization::make_tag('E', 'D', 'L', 'T');
inline constexpr serialization::SectionTag DYNAMIC_SPAWNS = serialization::make_tag('D', 'S', 'P', 'N');
} // namespace save_section

/// Per-save stable id for entities spawned at RUNTIME (dropped loot, summons).
/// Assigned sequentially by the save system; disjoint from WorldEntityId space.
using DynamicEntityId = uint64_t;

/// What happened to one GENERATED entity, relative to the generated world.
struct EntityDelta {
    enum class Kind : uint8_t {
        Destroyed,  ///< Removed from the world (killed, picked up).
        Moved,      ///< New position/orientation (fields below).
        StateChanged, ///< Gameplay flags changed (opened/looted...) — the
                      ///< details live in gameplay's own sections; this record
                      ///< only marks the entity as touched so it isn't respawned
                      ///< pristine.
    };

    WorldEntityId world_id = 0;
    Kind kind = Kind::Destroyed;
    glm::vec2 position_xz{0.0f}; ///< Moved only. Meters.
    float yaw = 0.0f;            ///< Moved only. Radians.
};

/// A runtime-spawned entity that must be recreated on load.
struct DynamicSpawn {
    DynamicEntityId dynamic_id = 0;
    uint64_t archetype = 0;      ///< content id hash, as GeneratedEntityRecord.
    ChunkCoord chunk;            ///< resident chunk at save time.
    glm::vec2 position_xz{0.0f};
    float yaw = 0.0f;
};

/// Per-chunk bucket of deltas, so ChunkManager can overlay a single chunk at
/// load time without scanning the whole save.
struct ChunkDelta {
    ChunkCoord coord;
    std::vector<EntityDelta> entity_deltas;
    std::vector<DynamicSpawn> dynamic_spawns;
};

/// The in-memory delta model. ChunkManager::open() takes a pointer to this and
/// consults it at every chunk load; the save flow builds it from the live world.
struct SaveDelta {
    uint64_t world_seed = 0;      ///< Must match the world file's seed (guard).
    std::vector<ChunkDelta> chunks;

    /// Delta bucket for `coord`, or nullptr if the chunk is untouched.
    [[nodiscard]] const ChunkDelta* chunk_delta(ChunkCoord coord) const;
};

/// A gameplay/engine module's contribution to the save container (agreed with
/// sim, Rule 26). `write` serializes the module's state from the world; `read`
/// restores it, migrating internally from `stored_version` (return false =
/// unrecoverable, load aborts).
struct SaveSectionHooks {
    serialization::SectionTag tag = 0;
    uint16_t version = 0;
    std::function<void(serialization::BinaryWriter&, const ecs::World&)> write;
    std::function<bool(serialization::BinaryReader&, ecs::World&, uint16_t stored_version)> read;
};

/// Owns the .dfs container format: world-owned sections (META, entity deltas,
/// dynamic spawns) plus every registered module section. Container-level
/// version migration lives here; section-level migration lives in each hook.
class SaveDeltaCodec {
public:
    SaveDeltaCodec();
    ~SaveDeltaCodec();

    /// Registers a module's section (gameplay calls at startup, before any
    /// save/load). Duplicate tags are a programming error (asserted).
    void register_section(SaveSectionHooks hooks);

    /// Writes `delta` + all registered sections (queried from `ecs`) atomically.
    /// False on IO failure.
    [[nodiscard]] bool write_save(const std::filesystem::path& path, const SaveDelta& delta,
                                  const ecs::World& ecs);

    /// Reads a save: fills `out_delta`, dispatches registered sections into
    /// `ecs`, preserves unknown sections verbatim for the next write_save.
    /// False on IO error, bad magic, failed migration, or seed mismatch.
    [[nodiscard]] bool read_save(const std::filesystem::path& path, SaveDelta& out_delta,
                                 ecs::World& ecs);

private:
    struct Impl; // hook registry + preserved unknown section blobs
    std::unique_ptr<Impl> impl_;
};

} // namespace dfn::world
