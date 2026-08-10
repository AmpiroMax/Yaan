/*
Created: 09:08:2026 - 00:16:55
Last updated: 10:08:2026 - 11:37:17
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
- ChunkManager: open/update/queries/height sampling, coarse LOD node streaming.

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
- 09:08:2026 - 16:30:44: Representation swap: voxel_mesh(coord) — the 3D geometry handoff, same lifetime as heightfield().
- 09:08:2026 - 21:37:57: NEW darkness_at(world) — §6.3 authored darkness (0 open daylight .. 1 pitch black) as a one-position query; keeps worldgen internals out of the app's frame loop.
- 09:08:2026 - 22:10:12: NEW water_surface_at(vec2) for sim's swimming — resolves against the analytic water field, NOT the drawable primitives (whose coverage guarantee runs field->primitive only, so they can extend past real water) and NOT the sampled grid (quantised at the shoreline).
- 09:08:2026 - 23:49:27: LOD STREAMING HALF (the agreed seam with render): world_bounds_xz / request_coarse_nodes / coarse_heightfield / coarse_surfacefield / release_coarse_node, plus the two residency counters. Coarse nodes are built incrementally under a per-update row budget inside update(), nearest-to-focus first, and are freed ONLY by release_coarse_node — render drops its mesh before it calls it.
- 10:08:2026 - 02:05:00: surface_class_at(vec2) for sim's footstep sound — nearest sample of the SAMPLED field render splats from (see doc comment for why it differs from the analytic water_surface_at).
- 10:08:2026 - 11:37:17: path_surface() and stand_vantages() — the §8.1 path
  network and the stand's own acceptance standpoints, whole-world and built at
  open, exactly like water_bodies(). Render owns the Tour and cannot see
  dfn::world; without these a tour on DFN_MAP=forest shot the TESTBED's
  coordinates.
*/

#pragma once

#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/core/math/sources/HeightField.h"
#include "engine/core/math/sources/StandVantage.h"
#include "engine/core/math/sources/VoxelField.h"
#include "engine/world/sources/Chunk.h"
#include "engine/world/sources/CoarseTerrain.h"
#include "engine/world/sources/SaveDelta.h"
#include "engine/world/sources/WorldFormat.h"
#include "engine/world/sources/Worldgen.h"

#include <filesystem>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
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

    /// The extracted 3D terrain surface of a resident chunk (render draws it,
    /// physics builds its static collision from it). Same lifetime as
    /// heightfield(); nullopt if not resident. HeightFieldView remains valid
    /// alongside it as the ground-height query.
    [[nodiscard]] std::optional<math::VoxelMeshView> voxel_mesh(ChunkCoord coord) const;

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

    /// The §8.1 PATH NETWORK of the whole open world, as render-side
    /// primitives. Same shape and lifetime as water_bodies(): whole-world,
    /// built once at open, valid until the manager is re-opened. Empty on
    /// stands that declare no paths — an empty span is a valid answer, not a
    /// failure, so no consumer needs a stand check (Rule 32).
    ///
    /// Route i occupies stations [route_offsets[i], route_offsets[i+1]);
    /// route_offsets always ends with the total, so the last route is not a
    /// special case.
    struct PathSurface {
        std::span<const math::PathStation> stations;
        std::span<const uint32_t> route_offsets;
        std::span<const math::PathGoalMark> goals;
        /// BR-3's margin band reach (m), outward from the worn edge — the
        /// `band_m` argument of math::path_edge_profile.
        float rich_edge_band_m = 0.0f;
        /// BR-1, per route. `hidden_run_m` is the longest contiguous run with
        /// the destination occluded; `hidden_station` is the station at the
        /// MIDDLE of that run, and `visible_station` is the paired CONTROL —
        /// the station on the same route at the same range to the same goal
        /// from which the goal IS visible.
        ///
        /// The pair is the point. A single frame of ground with no shrine in it
        /// cannot fail (Rule 27): a stand made entirely of trees would produce
        /// it by accident. Two frames that differ only in where along the trace
        /// the walker stands can. Both indices are -1 where they do not exist.
        std::span<const int32_t> hidden_station;
        std::span<const int32_t> visible_station;
        std::span<const float> hidden_run_m;
    };
    [[nodiscard]] PathSurface path_surface() const;

    /// THE OPEN STAND'S ACCEPTANCE STANDPOINTS (WorldgenVantages.h), controls
    /// included and paired with their claims. Built once at open, valid until
    /// re-open; empty on stands that publish none, which is a valid answer.
    ///
    /// This exists because the Tour lives in render and render cannot see
    /// `dfn::world`. Without it a tour on a stand other than the testbed shoots
    /// the testbed's coordinates, which on the forest stand means one frame and
    /// a stop — a stand nobody can photograph cannot be accepted by anyone.
    [[nodiscard]] std::span<const math::StandVantage> stand_vantages() const;

    /// Full chunk data of a resident chunk (editor, save encoding). nullptr if
    /// not resident.
    [[nodiscard]] const Chunk* chunk(ChunkCoord coord) const;

    /// Terrain height at a world position (bilinear), nullopt when the owning
    /// chunk is not resident. Convenience over Heightmap::sample_world.
    [[nodiscard]] std::optional<float> height_at(glm::vec2 world_xz) const;

    /// LANDSCAPE §6.3 authored darkness at a world position: 0 = open daylight,
    /// 1 = pitch black. Darkness is EARNED by depth — fully enclosed AND
    /// >= DARKNESS_DEPTH_MIN walked along the passage from the nearest mouth.
    ///
    /// This wrapper exists so the app never has to hold the layout, the
    /// worldgen context or a GroundSampler: HOW darkness is computed stays in
    /// this zone, and the sampler is guaranteed to be the same one the carve
    /// mouths were derived with. Ask it per frame for the player's position.
    [[nodiscard]] float darkness_at(glm::vec3 world) const;

    // --- Coarse terrain (far LOD) ---------------------------------------------
    //
    // THE OTHER HALF OF THE LOD CONTRACT WITH RENDER. Chunk streaming reaches
    // CHUNK_LOAD_RADIUS chunks from the player and stops; everything past it
    // simply does not exist, which is why half the map is missing at any
    // moment. These five calls stream the rest as coarse quadtree nodes.
    //
    // Agreed with render (Rule 26), and every clause of it is load-bearing:
    // - A COARSE NODE IS A HeightFieldView. 129 samples with the shared edge
    //   row, step = the level's voxel size (1/4/8/16/32/64 m), 128 voxels per
    //   node at every level. No second mesh format exists.
    // - NODE IDS SIT ON A FIXED WORLD GRID (world origin = coord * node size),
    //   so growing the world from 2x2 km to 10x10 km renumbers nothing.
    // - REQUESTS ARE ASYNC. coarse_heightfield() returns nullopt for several
    //   updates after a request: nodes are built under the same per-update
    //   budget discipline that fixed the streaming freezes. Render retries
    //   against its pending set, not against its one-shot to_load list.
    // - NOTHING IS EVICTED BEHIND RENDER'S BACK. A delivered node stays valid
    //   until release_coarse_node; render drops its mesh first.

    /// The extent of the GENERATED world on xz, metres: (min_x, min_z, max_x,
    /// max_z). Render's LOD descent needs it to know where the world ends.
    ///
    /// It comes from here rather than from generated config because config
    /// describes the CONFIGURED extent while this describes what the generator
    /// was actually opened with, and those two have already diverged once this
    /// stage. Zero-extent (all four components 0) when nothing is open.
    [[nodiscard]] glm::vec4 world_bounds_xz() const;

    /// Asks for these nodes to be built. Returns immediately: the work happens
    /// inside following update() calls, nearest-to-focus first, under a
    /// per-update budget. Requesting a node that is already resident or already
    /// queued is a no-op, so render may pass the same list every frame.
    void request_coarse_nodes(std::span<const CoarseNode> nodes);

    /// The node's heightfield, or nullopt while it is still being built (or was
    /// never requested). Valid until release_coarse_node(node).
    [[nodiscard]] std::optional<math::HeightFieldView>
    coarse_heightfield(const CoarseNode& node) const;

    /// The node's surface field (splat classes, water), same grid and lifetime
    /// as coarse_heightfield. nullopt exactly when that is nullopt.
    ///
    /// It ships WITH the geometry rather than after it: without it the
    /// cross-fade between two levels changes the MATERIAL as well as the shape,
    /// which moves popping off the silhouette and onto the colour.
    [[nodiscard]] std::optional<math::SurfaceFieldView>
    coarse_surfacefield(const CoarseNode& node) const;

    /// Frees the node (or cancels its pending build). Render calls its own
    /// drop_lod_node first — this side never frees anything render still holds.
    void release_coarse_node(const CoarseNode& node);

    /// Nodes fully built and held. Diagnostics for the app and tests.
    [[nodiscard]] std::size_t coarse_resident_count() const;
    /// Nodes requested and not yet delivered.
    [[nodiscard]] std::size_t coarse_pending_count() const;

    /// Height of the water surface covering `world_xz`, or nullopt where there
    /// is no water. THE FIELD IS THE TRUTH: this resolves against the analytic
    /// hydrology query, not against the LakePlane/RiverStation primitives and
    /// not against the sampled heightmap grid.
    ///
    /// That distinction is the whole reason this exists. The drawable
    /// primitives are DERIVED from the field, and the invariant that pins them
    /// runs one way only -- every WaterBed sample is covered by a primitive,
    /// with nothing guaranteeing the reverse -- so a primitive may extend past
    /// real water. A point-in-ellipse test would let a swimmer swim on grass,
    /// which is the pond bounding-box bug (1.7-6x over-cover) with a player in
    /// it instead of a tree. Being analytic rather than grid-sampled, it is
    /// also exact at the shoreline instead of quantised to the sample spacing.
    [[nodiscard]] std::optional<float> water_surface_at(glm::vec2 world_xz) const;

    /// Surface class under a world column, from the SAMPLED per-chunk field —
    /// deliberately the same array render splats from, so what the player
    /// SEES underfoot is what a consumer (sim's footstep sound) reports.
    /// Nearest-sample, no interpolation (it is a class enum). If the sample
    /// is water-covered this still returns the BED's class — wade/splash is
    /// the caller's decision, made against water_surface_at. nullopt when the
    /// chunk is not resident. Sibling of water_surface_at, which is analytic
    /// on purpose (see above): coverage must be exact at the shoreline, but
    /// the LOOK of the ground is the sampled field by definition.
    [[nodiscard]] std::optional<math::SurfaceClass> surface_class_at(glm::vec2 world_xz) const;

private:
    struct Impl; // reader, delta overlay, resident map, scratch batch buffers
    std::unique_ptr<Impl> impl_;
};

} // namespace dfn::world
