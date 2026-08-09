/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 11:05:22
Module: engine/world
File: engine/world/sources/ChunkManager.h

Responsibility:
- Chunk streaming: decides which chunks are resident around the focus position,
  loads/unloads them through the world file reader + save-delta overlay, spawns
  and destroys chunk entities with BATCH ECS operations (Rule 11), and announces
  changes via ChunkLoaded/ChunkUnloaded events.

Key items:
- ChunkLoaded / ChunkUnloaded: events published on the core EventBus.
- ChunkStreamingParams: radii (values from generated constants; entries pending
  in NUMBERS.md).
- ChunkManager: open/update/queries/height sampling.

Dependencies:
- Uses: Chunk.h, WorldFormat.h, SaveDelta.h, engine/core/{ecs,events,math,config}.
- Used by: engine/app (owns and updates it); render/physics consume the
  HeightFieldView announced by its events (agreed Rule 26 contract).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Streaming paths use ONLY batch ECS ops (Rule 11); a per-entity spawn/destroy
  here is a violation.
- publish() (synchronous) is used for both events; ChunkUnloaded handlers run
  BEFORE the chunk memory is freed — that ordering is part of the frozen
  HeightFieldView lifetime contract.
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — streaming interface with batch ECS
  ops (Q22, Rule 11), event-driven handoff agreed with render/sim (Rule 26).
- 09:08:2026 - 00:42:03: Stage 2 — added open_generated() (in-memory generator
  path, lead directive: no .dfw IO this stage); open(file) documented as
  deferred to stage 3. Additive change only.
- 09:08:2026 - 11:05:22: Stage 3b (additive): surfacefield()/scatter() per
  resident chunk + water_bodies() (render handoff agreement); open_generated
  builds the WorldGenContext once; chunk load attaches Transform/RenderMesh/
  LocalBounds/SiteMarker to P4 site entities via batch ops.
*/

#pragma once

#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/core/math/sources/HeightField.h"
#include "engine/world/sources/Chunk.h"
#include "engine/world/sources/SaveDelta.h"
#include "engine/world/sources/WorldFormat.h"
#include "engine/world/sources/Worldgen.h"

#include <filesystem>
#include <glm/vec3.hpp>
#include <memory>
#include <optional>
#include <span>

namespace dfn::world {

/// Published (synchronously) right AFTER a chunk became fully resident: its
/// entities exist in the ecs::World (group = chunk_group(coord)) and
/// heightfield(coord) is valid. Render creates the terrain mesh, physics the
/// terrain body, in their handlers (wired by app — siblings can't include world).
struct ChunkLoaded {
    ChunkCoord coord;
};

/// Published (synchronously) right BEFORE a chunk's memory is freed and its
/// entity group destroyed. heightfield(coord) is still valid during dispatch;
/// consumers release meshes/bodies here (frozen lifetime contract, Rule 26).
struct ChunkUnloaded {
    ChunkCoord coord;
};

/// Streaming radii in chunks around the focus chunk. Values come from the
/// generated constants — NUMBERS.md entries CHUNK_LOAD_RADIUS /
/// CHUNK_UNLOAD_RADIUS are pending (flagged for the group sync). Hysteresis:
/// unload_radius > load_radius so chunks don't thrash at boundaries.
struct ChunkStreamingParams {
    uint32_t load_radius = 0;   ///< Chunks within this Chebyshev radius are loaded.
    uint32_t unload_radius = 0; ///< Chunks beyond this radius are unloaded.
};

/// Owns the resident chunk set. Single-threaded like ecs::World: update() runs
/// on the simulation thread; disk IO may later move to a worker inside the
/// implementation, but all ECS/event effects happen inside update().
class ChunkManager {
public:
    ChunkManager();
    ~ChunkManager();
    ChunkManager(const ChunkManager&) = delete;
    ChunkManager& operator=(const ChunkManager&) = delete;

    /// Opens the world file and (optionally) a save delta to overlay (Q56).
    /// No chunks are loaded yet — the first update() does that. False on error.
    /// STAGE 2: world file IO is deferred (lead directive); this returns false
    /// until stage 3 — use open_generated() for the skeleton.
    [[nodiscard]] bool open(const std::filesystem::path& world_file,
                            const SaveDelta* delta,
                            ChunkStreamingParams params);

    /// Stage-2 path (lead directive, sync of 09:08:2026): serve chunks straight
    /// from the deterministic in-memory generator instead of a .dfw file —
    /// each chunk is produced by Worldgen's generate_chunk() on first load and
    /// discarded on unload (regeneration is deterministic, Rule 13.1). The
    /// extent in `gen_params` clips streaming. Always succeeds.
    void open_generated(const WorldGenParams& gen_params, ChunkStreamingParams params);

    /// Streams around `focus_position` (the player, meters): loads every missing
    /// chunk within load_radius, unloads residents beyond unload_radius.
    /// Per chunk load (one batch each, Rule 11): decode chunk -> apply save
    /// delta -> ecs.spawn_batch(group = chunk_group) -> attach component
    /// prototypes -> publish ChunkLoaded. Per unload: publish ChunkUnloaded ->
    /// ecs.destroy_group -> free chunk data.
    void update(const glm::vec3& focus_position, ecs::World& ecs, events::EventBus& bus);

    /// Unloads everything (shutdown / world switch), with the same per-chunk
    /// unload protocol so consumers release resources.
    void unload_all(ecs::World& ecs, events::EventBus& bus);

    // --- Queries --------------------------------------------------------------

    [[nodiscard]] bool is_loaded(ChunkCoord coord) const;

    /// Coordinates of all resident chunks. Valid until the next update().
    [[nodiscard]] std::span<const ChunkCoord> loaded_chunks() const;

    /// The frozen cross-zone heightfield view of a resident chunk (render
    /// meshing, physics terrain — agreed Rule 26). nullopt if not resident.
    [[nodiscard]] std::optional<math::HeightFieldView> heightfield(ChunkCoord coord) const;

    /// The stage-3b surface view of a resident chunk (render splat/water —
    /// agreed with render). Same lifetime as heightfield(). nullopt if not
    /// resident.
    [[nodiscard]] std::optional<math::SurfaceFieldView> surfacefield(ChunkCoord coord) const;

    /// P5 scatter instances of a resident chunk (render decides drawing).
    /// Empty span if not resident. Same lifetime as heightfield().
    [[nodiscard]] std::span<const math::ScatterInstance> scatter(ChunkCoord coord) const;

    /// Explicit water-body primitives of the whole open world (render's
    /// plane/ribbon water materials). River stations are ordered source ->
    /// mouth with monotonically non-increasing surface heights; segment i is
    /// stations [river_segment_offsets[i], river_segment_offsets[i+1]).
    /// Valid until the manager is re-opened.
    struct WaterBodies {
        std::span<const math::LakePlane> lakes;
        std::span<const math::RiverStation> river_stations;
        std::span<const uint32_t> river_segment_offsets;
    };
    [[nodiscard]] WaterBodies water_bodies() const;

    /// Full chunk data of a resident chunk (editor, save encoding). nullptr if
    /// not resident.
    [[nodiscard]] const Chunk* chunk(ChunkCoord coord) const;

    /// Terrain height at a world position (bilinear), nullopt when the owning
    /// chunk is not resident. Convenience over Heightmap::sample_world.
    [[nodiscard]] std::optional<float> height_at(glm::vec2 world_xz) const;

private:
    struct Impl; // reader, delta overlay, resident map, scratch batch buffers
    std::unique_ptr<Impl> impl_;
};

} // namespace dfn::world
