/*
Created: 09:08:2026 - 00:42:03
Last updated: 13:08:2026 - 18:59:13
Module: engine/world
File: engine/world/sources/ChunkManager.cpp

Responsibility:
- Chunk streaming implementation: residency ring around the focus position,
  in-memory generation (stage 2), batch ECS spawn/destroy per chunk (Rule 11),
  ChunkLoaded/ChunkUnloaded events with the frozen lifetime ordering.

Key items:
- ChunkManager::open_generated / update / unload_all / queries.

Dependencies:
- Uses: ChunkManager.h, Worldgen (generate_chunk).
- Used by: dfn_world.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ChunkUnloaded is published BEFORE the chunk leaves the resident map and
  before its entity group is destroyed — consumers release meshes/bodies in the
  handler while heightfield(coord) is still valid. Keep that order.
- Batch ECS ops only on the streaming paths (Rule 11).
- A delivered coarse node is freed ONLY by release_coarse_node(). Adding an
  eviction policy here (age, count, distance) breaks the agreement with render,
  which drops its GPU mesh first and then calls release.
*/
/*
UPD:
- 09:08:2026 - 00:42:03: Stage 2 — in-memory generator streaming (open_generated),
  hysteresis load/unload ring, batch spawn/destroy, event protocol.
- 09:08:2026 - 11:05:22: Stage 3b — WorldGenContext built once at
  open_generated; surfacefield/scatter/water_bodies queries; site entities get
  Transform/PreviousTransform/RenderMesh/LocalBounds/SiteMarker prototypes via
  add_batch (Rule 11 — one pool visit per component type).
- 09:08:2026 - 14:41:26: Frame-05 bed fix: water_bodies().lakes now carries the lake plus one plane per surviving pond (additive, lead-blessed; render iterates the same span).
- 09:08:2026 - 16:30:44: Representation swap: voxel_mesh accessor.
- 09:08:2026 - 17:36:42: §6.2: honour ground_y when spawning site entities.
- 09:08:2026 - 18:19:09: Streaming LOAD BUDGET: at most CHUNK_LOAD_BUDGET chunks admitted per update, nearest-to-focus first with a deterministic tie-break, remainder deferred to following updates. Unbounded admission was the multi-second freeze (a cold ring is ~2 s of synchronous work at ~83 ms/chunk including sim's collision build). Nearest-first is what makes deferral safe: the ground under the player is distance 0, so it is always next and the queue cannot reorder into a hole beneath them.
- 09:08:2026 - 21:37:57: NEW darkness_at(world) — the §6.3 authored-darkness query wrapped at the ChunkManager level (lead's call) so the app holds only its one world handle and never the layout, the worldgen context or a GroundSampler; HOW darkness is computed stays in this zone and the sampler is guaranteed to be the one the carve mouths were derived with.
- 09:08:2026 - 22:10:12: water_surface_at(vec2) implemented over the analytic water_at, for sim's swim test.
- 09:08:2026 - 23:49:27: LOD STREAMING HALF. Coarse node residency (requested -> the one active build -> held until release_coarse_node), nearest-to-focus first, advanced only in updates that admitted NO chunk so two budgets never land in one frame. world_bounds_xz reports the extent the generator was OPENED with. Nothing leaves the held set on its own -- an eviction render did not ask for pulls the ground out from under a mesh it is still drawing.
- 10:08:2026 - 02:05:00: surface_class_at(vec2) — sampled-field point query (sim request; the world->sample decoder stays in this zone, Rule 35).
- 10:08:2026 - 11:37:17: path_surface() / stand_vantages() storage, flattened once per open.
- 13:08:2026 - 16:45:00: DFN_DARK_TRACE=<путь> — по строке на КАЖДЫЙ вызов darkness_at (приложение зовёт его раз в кадр) с разложением на ветви через enclosure_trace. Открывается ГРОМКО; выключен, пока переменная не названа. Этим прибором найдено, что ambient_darkness переключается 0↔1 за один кадр 13 раз за проход по тоннелю, каждый раз на пересечении carve_distance нуля в пределах 2 см.
- 13:08:2026 - 18:40:00: DFN_DARK_TRACE пишет roof_y и open_to_sky вместо above_ground — вслед за воротами, которые теперь судят крышу.
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
*/

#include "engine/world/sources/ChunkManager.h"

#include "engine/world/sources/WorldgenCarve.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenVantages.h"

#include "engine/core/components/sources/Components.h"
#include "engine/world/sources/SiteComponents.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <glm/gtc/quaternion.hpp>
#include <unordered_map>
#include <vector>

namespace dfn::world {

struct ChunkManager::Impl {
    bool opened = false;
    WorldGenParams gen_params;
    ChunkStreamingParams params;
    const SaveDelta* delta = nullptr; // stage 3: overlay on load

    WorldGenContext gen_ctx;                      // built once per open_generated
    std::vector<math::LakePlane> lakes;           // water_bodies() storage
    // path_surface() storage — flattened once per open, same lifetime as lakes.
    std::vector<math::PathStation> path_stations;
    std::vector<uint32_t> path_route_offsets;
    std::vector<math::PathGoalMark> path_goals;
    std::vector<int32_t> path_hidden_station;
    std::vector<int32_t> path_visible_station;
    std::vector<float> path_hidden_run_m;
    std::vector<math::StandVantage> vantages;      // stand_vantages() storage
    std::unordered_map<uint64_t, Chunk> resident; // key = chunk_group(coord)
    std::vector<ChunkCoord> loaded_coords;        // cache for loaded_chunks()

    // DFN_DARK_TRACE (see darkness_at): opened on first use, closed with the
    // manager. mutable-by-pointer: darkness_at is const and stays const.
    std::FILE* dark_trace = nullptr;
    bool dark_trace_tried = false;
    uint64_t dark_trace_call = 0;

    // --- Coarse LOD nodes ----------------------------------------------------
    // Three states, and the split is the contract with render: `requested` is
    // asked for but untouched, `active` is the ONE node under construction, and
    // `coarse` holds finished nodes until release_coarse_node. Nothing moves out
    // of `coarse` on its own — an eviction render did not ask for would pull the
    // ground out from under a mesh it is still drawing.
    std::vector<CoarseNode> requested;
    std::optional<CoarseNodeData> active;
    std::unordered_map<uint64_t, CoarseNodeData> coarse;

    /// Shortest distance from the focus to the node's footprint on xz (0 when
    /// the focus is inside it) — the same measure render selects nodes with.
    [[nodiscard]] static float coarse_distance(const CoarseNode& n, glm::vec2 focus) {
        const glm::vec2 o = coarse_node_origin_m(n);
        const float s = coarse_node_size_m(n.level);
        const float dx = std::max({o.x - focus.x, 0.0f, focus.x - (o.x + s)});
        const float dz = std::max({o.y - focus.y, 0.0f, focus.y - (o.y + s)});
        return std::sqrt(dx * dx + dz * dz);
    }

    /// Advances the ONE node under construction, starting the nearest requested
    /// node when there is none. Nearest-first for the same reason the chunk
    /// queue is nearest-first: what the player is looking at should arrive
    /// before what is behind them, and the order must not depend on the
    /// enumeration order of a hash map.
    void advance_coarse(glm::vec2 focus) {
        if (!active) {
            if (requested.empty()) {
                return;
            }
            auto best = requested.begin();
            float best_d = coarse_distance(*best, focus);
            for (auto it = requested.begin() + 1; it != requested.end(); ++it) {
                const float d = coarse_distance(*it, focus);
                // Deterministic tie-break: a coarser level first (it covers more
                // ground for the same cost), then node coords.
                bool better = d < best_d;
                if (!better && d == best_d) {
                    if (it->level != best->level) {
                        better = it->level > best->level;
                    } else if (it->x != best->x) {
                        better = it->x < best->x;
                    } else {
                        better = it->z < best->z;
                    }
                }
                if (better) {
                    best = it;
                    best_d = d;
                }
            }
            active = begin_coarse_node(*best);
            requested.erase(best);
        }
        build_coarse_rows(gen_ctx, *active, COARSE_NODE_ROW_BUDGET);
        if (active->complete()) {
            coarse.insert_or_assign(coarse_node_key(active->node), std::move(*active));
            active.reset();
        }
    }

    [[nodiscard]] bool coarse_known(const CoarseNode& n) const {
        if (coarse.contains(coarse_node_key(n))) {
            return true;
        }
        if (active && active->node == n) {
            return true;
        }
        return std::find(requested.begin(), requested.end(), n) != requested.end();
    }

    [[nodiscard]] bool in_extent(ChunkCoord c) const {
        return c.x >= gen_params.min_chunk.x && c.x <= gen_params.max_chunk.x
            && c.z >= gen_params.min_chunk.z && c.z <= gen_params.max_chunk.z;
    }

    void rebuild_coord_cache() {
        loaded_coords.clear();
        loaded_coords.reserve(resident.size());
        for (const auto& [key, chunk] : resident) {
            loaded_coords.push_back(chunk.coord);
        }
    }
};

namespace {
[[nodiscard]] uint32_t chebyshev(ChunkCoord a, ChunkCoord b) {
    const int32_t dx = std::abs(a.x - b.x);
    const int32_t dz = std::abs(a.z - b.z);
    return static_cast<uint32_t>(dx > dz ? dx : dz);
}
} // namespace

ChunkManager::ChunkManager() : impl_(std::make_unique<Impl>()) {}
ChunkManager::~ChunkManager() {
    if (impl_ && impl_->dark_trace != nullptr) {
        std::fclose(impl_->dark_trace);
        impl_->dark_trace = nullptr;
    }
}

bool ChunkManager::open(const std::filesystem::path& world_file, const SaveDelta* delta,
                        ChunkStreamingParams params) {
    (void)world_file;
    (void)delta;
    (void)params;
    // Stage 2: world file IO deferred (lead directive). Use open_generated().
    return false;
}

void ChunkManager::open_generated(const WorldGenParams& gen_params,
                                  ChunkStreamingParams params) {
    impl_->opened = true;
    impl_->gen_params = gen_params;
    impl_->params = params;
    impl_->delta = nullptr;
    impl_->resident.clear();
    impl_->loaded_coords.clear();
    // A new world invalidates every coarse node: the ids are on a fixed world
    // grid, but what the field says at those coordinates is not the same world.
    impl_->requested.clear();
    impl_->active.reset();
    impl_->coarse.clear();
    // World-level passes once per open (deterministic; chunks stay independent).
    impl_->gen_ctx = build_world_context(gen_params);
    // Drawable water bodies: the lake plus every surviving pond, so no
    // water-covered sample is left without a body render can draw over it.
    impl_->lakes.assign(1, impl_->gen_ctx.hydrology.lake);
    impl_->lakes.insert(impl_->lakes.end(), impl_->gen_ctx.hydrology.pond_planes.begin(),
                        impl_->gen_ctx.hydrology.pond_planes.end());
    // The §8.1 path network flattened for render. Unconditional: a stand with
    // no paths flattens to zero stations and a one-element offsets array, which
    // is the same code path rather than a branch (Rule 32).
    path_render_stations(impl_->gen_ctx.paths, impl_->path_stations,
                         impl_->path_route_offsets, impl_->path_goals);
    impl_->path_hidden_station.clear();
    impl_->path_visible_station.clear();
    impl_->path_hidden_run_m.clear();
    for (const PathRoute& r : impl_->gen_ctx.paths.routes) {
        impl_->path_hidden_station.push_back(r.hidden_station);
        impl_->path_visible_station.push_back(r.visible_station);
        impl_->path_hidden_run_m.push_back(r.longest_hidden_run_m);
    }
    // The stand's own acceptance standpoints. Gated on the STAND, not on
    // emptiness: the testbed publishes none because its §7.1 route is authored
    // in render's own testbed_steps(), and inventing forest vantages for it
    // would emit frames aimed at ground that carries no such claim.
    impl_->vantages.clear();
    if (impl_->gen_ctx.params.layout.stand == StandId::Forest) {
        impl_->vantages = forest_vantages(impl_->gen_ctx.params.seed,
                                          impl_->gen_ctx.params.layout, impl_->gen_ctx.paths,
                                          impl_->gen_ctx.finds);
    }
}

void ChunkManager::update(const glm::vec3& focus_position, ecs::World& ecs,
                          events::EventBus& bus) {
    if (!impl_->opened) {
        return;
    }
    const ChunkCoord focus = chunk_at_position({focus_position.x, focus_position.z});
    bool changed = false;

    // --- Unload pass: residents beyond the unload radius (hysteresis). --------
    std::vector<ChunkCoord> to_unload;
    for (const auto& [key, chunk] : impl_->resident) {
        if (chebyshev(chunk.coord, focus) > impl_->params.unload_radius) {
            to_unload.push_back(chunk.coord);
        }
    }
    for (const ChunkCoord coord : to_unload) {
        // Order is contract: publish while data is valid, then destroy the
        // entity group (one batch, Rule 11), then free the chunk.
        bus.publish(ChunkUnloaded{coord});
        ecs.destroy_group(chunk_group(coord));
        impl_->resident.erase(chunk_group(coord));
        changed = true;
    }

    // --- Load pass: missing chunks within the load radius, clipped to extent,
    // NEAREST FIRST and rate-limited to CHUNK_LOAD_BUDGET per update.
    //
    // Admitting every missing chunk in one update is what produced the
    // multi-second freezes: a chunk costs ~14.5 ms here plus sim's ~68 ms
    // collision build, so a cold 5x5 ring was ~2 s of synchronous work inside
    // a single frame. Deferring the remainder spreads that over following
    // updates. Nearest-to-focus ordering is what makes deferral safe: the
    // ground under the player is by definition distance 0, so it is always the
    // next chunk admitted and the queue can never reorder into a hole beneath
    // them. Every update admits at least one chunk, so nothing starves.
    const int32_t r = static_cast<int32_t>(impl_->params.load_radius);
    struct Pending {
        ChunkCoord coord;
        int32_t ring;    ///< Chebyshev distance: the streaming ring it sits in
        int64_t dist_sq; ///< tie-break within a ring: true distance
    };
    std::vector<Pending> pending;
    for (int32_t dz = -r; dz <= r; ++dz) {
        for (int32_t dx = -r; dx <= r; ++dx) {
            const ChunkCoord coord{focus.x + dx, focus.z + dz};
            if (!impl_->in_extent(coord) || impl_->resident.contains(chunk_group(coord))) {
                continue;
            }
            pending.push_back({coord, static_cast<int32_t>(chebyshev(coord, focus)),
                               static_cast<int64_t>(dx) * dx + static_cast<int64_t>(dz) * dz});
        }
    }
    std::sort(pending.begin(), pending.end(), [](const Pending& a, const Pending& b) {
        if (a.ring != b.ring) return a.ring < b.ring;
        if (a.dist_sq != b.dist_sq) return a.dist_sq < b.dist_sq;
        // Deterministic final tie-break so load order never depends on the
        // enumeration order of the resident map.
        if (a.coord.x != b.coord.x) return a.coord.x < b.coord.x;
        return a.coord.z < b.coord.z;
    });
    const std::size_t budget = static_cast<std::size_t>(config::CHUNK_LOAD_BUDGET);
    if (pending.size() > budget) {
        pending.resize(budget);
    }

    for (const Pending& entry : pending) {
        {
            const ChunkCoord coord = entry.coord;
            const uint64_t key = chunk_group(coord);
            Chunk chunk = generate_chunk(impl_->gen_ctx, coord);
            // Stage 3: apply impl_->delta overlay here before spawning (Q56).

            // Batch entity spawn for the chunk's generated records (Rule 11):
            // one spawn_batch + one add_batch per component type. Stage 3b
            // records are P4 sites — placeholder RenderMesh ids + SiteMarker
            // (render maps the ids; real archetype instantiation from content
            // files is gameplay/lead wiring, a later stage).
            if (!chunk.entities.empty()) {
                const std::size_t n = chunk.entities.size();
                std::vector<ecs::EntityId> ids(n);
                ecs.spawn_batch(ids, key);

                std::vector<components::Transform> transforms(n);
                std::vector<components::RenderMesh> meshes(n);
                std::vector<components::LocalBounds> bounds(n);
                std::vector<SiteMarker> markers(n);
                for (std::size_t i = 0; i < n; ++i) {
                    const GeneratedEntityRecord& rec = chunk.entities[i];
                    const float y = rec.ground_y != NO_GROUND_Y
                                      ? rec.ground_y
                                      : chunk.heightmap.sample_world(coord, rec.position_xz);
                    transforms[i].position = {rec.position_xz.x, y, rec.position_xz.y};
                    transforms[i].rotation =
                        glm::angleAxis(rec.yaw, glm::vec3{0.0f, 1.0f, 0.0f});
                    if (const auto type = site_type_from_archetype(rec.archetype)) {
                        const SiteArchetype& a = site_archetype(*type);
                        meshes[i] = components::RenderMesh{a.mesh_id, 0};
                        bounds[i] = components::LocalBounds{a.bounds_min, a.bounds_max};
                        markers[i] = SiteMarker{*type};
                    }
                }
                ecs.add_batch<components::Transform>(ids, std::span<const components::Transform>{transforms});
                std::vector<components::PreviousTransform> prev(n);
                for (std::size_t i = 0; i < n; ++i) {
                    prev[i].position = transforms[i].position;
                    prev[i].rotation = transforms[i].rotation;
                }
                ecs.add_batch<components::PreviousTransform>(
                    ids, std::span<const components::PreviousTransform>{prev});
                ecs.add_batch<components::RenderMesh>(
                    ids, std::span<const components::RenderMesh>{meshes});
                ecs.add_batch<components::LocalBounds>(
                    ids, std::span<const components::LocalBounds>{bounds});
                ecs.add_batch<SiteMarker>(ids, std::span<const SiteMarker>{markers});
                // A WALL TORCH IS A LIGHT, and this is where it gets one.
                // components::CarriedLight is the only thing render collects
                // into the frame's point-light array; the name says "carried"
                // but the contract is "a flame at an entity", which a sconce is
                // (IRenderer's own list: "torch, braziers, lit windows").
                // Added after the batches on purpose: only some records are
                // torches, so this is not a batch (Rule 9's structural-change
                // discipline is satisfied — the view above has been consumed).
                for (std::size_t i = 0; i < n; ++i) {
                    if (markers[i].type != SiteType::WallTorch) {
                        continue;
                    }
                    ecs.add(ids[i], components::CarriedLight{
                                        .active = true,
                                        .radius_m = 0.0f, // render's default torch radius
                                        .color_rgb = 0,   // render's default warm flame
                                        // Up the stick from the sconce, the same
                                        // way a held torch's flame sits above the
                                        // grip.
                                        .offset = {0.0f, 0.45f, 0.0f}});
                }
            }

            impl_->resident.emplace(key, std::move(chunk));
            bus.publish(ChunkLoaded{coord});
            changed = true;
        }
    }

    // --- Coarse LOD pass: far terrain is built with the update's LEFTOVER ----
    //
    // Only when no chunk was admitted. Two budgets spent in one update is two
    // budgets' worth of hitch, and the ground under the player outranks the
    // ground on the horizon by definition: a chunk admission costs ~83 ms
    // including sim's collision build, which is already the whole frame. Chunk
    // admissions are bursty (a ring fills over a handful of updates and then
    // nothing), so this starves only while the player is crossing into new
    // ground, which is measured in frames.
    if (pending.empty()) {
        impl_->advance_coarse({focus_position.x, focus_position.z});
    }

    if (changed) {
        impl_->rebuild_coord_cache();
    }
}

void ChunkManager::unload_all(ecs::World& ecs, events::EventBus& bus) {
    // Same per-chunk protocol as streaming unload.
    std::vector<ChunkCoord> coords = impl_->loaded_coords;
    for (const ChunkCoord coord : coords) {
        bus.publish(ChunkUnloaded{coord});
        ecs.destroy_group(chunk_group(coord));
        impl_->resident.erase(chunk_group(coord));
    }
    impl_->rebuild_coord_cache();
}

bool ChunkManager::is_loaded(ChunkCoord coord) const {
    return impl_->resident.contains(chunk_group(coord));
}

std::span<const ChunkCoord> ChunkManager::loaded_chunks() const {
    return impl_->loaded_coords;
}

std::optional<math::HeightFieldView> ChunkManager::heightfield(ChunkCoord coord) const {
    const auto it = impl_->resident.find(chunk_group(coord));
    if (it == impl_->resident.end()) {
        return std::nullopt;
    }
    return it->second.heightmap.view(coord);
}

std::optional<math::SurfaceFieldView> ChunkManager::surfacefield(ChunkCoord coord) const {
    const auto it = impl_->resident.find(chunk_group(coord));
    if (it == impl_->resident.end()) {
        return std::nullopt;
    }
    return it->second.surface.view(coord);
}

std::optional<math::VoxelMeshView> ChunkManager::voxel_mesh(ChunkCoord coord) const {
    const auto it = impl_->resident.find(chunk_group(coord));
    if (it == impl_->resident.end()) {
        return std::nullopt;
    }
    return it->second.voxels.view(coord);
}

std::span<const math::ScatterInstance> ChunkManager::scatter(ChunkCoord coord) const {
    const auto it = impl_->resident.find(chunk_group(coord));
    if (it == impl_->resident.end()) {
        return {};
    }
    return it->second.scatter;
}

ChunkManager::WaterBodies ChunkManager::water_bodies() const {
    if (!impl_->opened) {
        return {};
    }
    return WaterBodies{impl_->lakes, impl_->gen_ctx.hydrology.stations,
                       impl_->gen_ctx.hydrology.segment_offsets};
}

ChunkManager::PathSurface ChunkManager::path_surface() const {
    if (!impl_->opened) {
        return {};
    }
    PathSurface ps;
    ps.stations = impl_->path_stations;
    ps.route_offsets = impl_->path_route_offsets;
    ps.goals = impl_->path_goals;
    ps.rich_edge_band_m = impl_->gen_ctx.paths.rich_edge_band_m;
    ps.hidden_station = impl_->path_hidden_station;
    ps.visible_station = impl_->path_visible_station;
    ps.hidden_run_m = impl_->path_hidden_run_m;
    return ps;
}

std::span<const math::StandVantage> ChunkManager::stand_vantages() const {
    if (!impl_->opened) {
        return {};
    }
    return impl_->vantages;
}

const Chunk* ChunkManager::chunk(ChunkCoord coord) const {
    const auto it = impl_->resident.find(chunk_group(coord));
    return it == impl_->resident.end() ? nullptr : &it->second;
}

std::optional<float> ChunkManager::height_at(glm::vec2 world_xz) const {
    const ChunkCoord coord = chunk_at_position(world_xz);
    const auto it = impl_->resident.find(chunk_group(coord));
    if (it == impl_->resident.end()) {
        return std::nullopt;
    }
    return it->second.heightmap.sample_world(coord, world_xz);
}

// --- Coarse terrain (far LOD) --------------------------------------------------

glm::vec4 ChunkManager::world_bounds_xz() const {
    if (!impl_->opened) {
        return glm::vec4{0.0f};
    }
    // THE GENERATED extent, derived from the params the generator was opened
    // with — not from configured constants, which describe an intent that has
    // already diverged from what exists once this stage. max_chunk is
    // INCLUSIVE, so the far edge is (max_chunk + 1) * CHUNK_SIZE.
    const float size = static_cast<float>(config::CHUNK_SIZE);
    const WorldGenParams& p = impl_->gen_params;
    return glm::vec4{static_cast<float>(p.min_chunk.x) * size,
                     static_cast<float>(p.min_chunk.z) * size,
                     static_cast<float>(p.max_chunk.x + 1) * size,
                     static_cast<float>(p.max_chunk.z + 1) * size};
}

void ChunkManager::request_coarse_nodes(std::span<const CoarseNode> nodes) {
    if (!impl_->opened) {
        return;
    }
    for (const CoarseNode& node : nodes) {
        // Idempotent: render may pass the same standing set every frame, and a
        // duplicate request must not rebuild a node that is already delivered
        // (which would hand render a new view while it draws the old one).
        if (!impl_->coarse_known(node)) {
            impl_->requested.push_back(node);
        }
    }
}

std::optional<math::HeightFieldView>
ChunkManager::coarse_heightfield(const CoarseNode& node) const {
    const auto it = impl_->coarse.find(coarse_node_key(node));
    if (it == impl_->coarse.end()) {
        return std::nullopt; // never requested, or still being built
    }
    return it->second.height_view();
}

std::optional<math::SurfaceFieldView>
ChunkManager::coarse_surfacefield(const CoarseNode& node) const {
    const auto it = impl_->coarse.find(coarse_node_key(node));
    if (it == impl_->coarse.end()) {
        return std::nullopt;
    }
    return it->second.surface_view();
}

void ChunkManager::release_coarse_node(const CoarseNode& node) {
    if (impl_->coarse.erase(coarse_node_key(node)) > 0) {
        return;
    }
    // Cancelling something not yet delivered: the node under construction, or
    // one still queued. Render deselects nodes it never received (it moved on),
    // and without these two branches the work would be finished for nobody.
    if (impl_->active && impl_->active->node == node) {
        impl_->active.reset();
        return;
    }
    const auto it = std::find(impl_->requested.begin(), impl_->requested.end(), node);
    if (it != impl_->requested.end()) {
        impl_->requested.erase(it);
    }
}

std::size_t ChunkManager::coarse_resident_count() const {
    return impl_->coarse.size();
}

std::size_t ChunkManager::coarse_pending_count() const {
    return impl_->requested.size() + (impl_->active ? 1u : 0u);
}

std::optional<float> ChunkManager::water_surface_at(glm::vec2 world_xz) const {
    const WorldGenContext& ctx = impl_->gen_ctx;
    // Macro height first: water_at needs the pre-carve terrain at this column,
    // and it must be the same value the height pipeline uses or the carve and
    // the water surface disagree at the shoreline.
    const float h = macro_height(ctx.params.seed, ctx.params.layout, world_xz);
    const WaterSample s = water_at(ctx.hydrology, ctx.params.layout, world_xz, h);
    if (s.water_surface <= math::NO_WATER) {
        return std::nullopt;
    }
    return s.water_surface;
}

std::optional<math::SurfaceClass> ChunkManager::surface_class_at(glm::vec2 world_xz) const {
    const auto view = surfacefield(chunk_at_position(world_xz));
    if (!view.has_value() || view->resolution == 0) {
        return std::nullopt;
    }
    // Nearest sample of the drawn field: the world->sample convention (origin,
    // step, row-major x fastest) lives HERE so no consumer grows a second
    // decoder copy of it (Rule 35, state form).
    const float limit = static_cast<float>(view->resolution - 1);
    const auto x = static_cast<uint32_t>(
        std::clamp(std::round((world_xz.x - view->origin.x) / view->step), 0.0f, limit));
    const auto z = static_cast<uint32_t>(
        std::clamp(std::round((world_xz.y - view->origin.y) / view->step), 0.0f, limit));
    return static_cast<math::SurfaceClass>(
        view->surface_class[static_cast<std::size_t>(z) * view->resolution + x]);
}

float ChunkManager::darkness_at(glm::vec3 world) const {
    // The GroundSampler is built from the SAME context the carve mouths were
    // derived with -- that guarantee is the reason this wrapper exists rather
    // than the app assembling the call itself.
    const WorldGenContext& ctx = impl_->gen_ctx;
    const GroundSampler ground = [&ctx](glm::vec2 p) { return terrain_height(ctx, p); };
    // DFN_DARK_TRACE=<path>: one line per CALL (the app calls this once per
    // frame), naming WHICH half of the rule decided. The result alone cannot
    // tell "not enclosed" from "enclosed but nothing earned" apart, and the
    // "темнеет, потом мигает" run needs exactly that split. Off unless the
    // variable names a file; opens loudly, and the shipping path below is the
    // SAME evaluation, not a second one.
    if (!impl_->dark_trace_tried) {
        impl_->dark_trace_tried = true;
        if (const char* dt = std::getenv("DFN_DARK_TRACE"); dt != nullptr && *dt != '\0') {
            impl_->dark_trace = std::fopen(dt, "wb");
            if (impl_->dark_trace == nullptr) {
                std::fprintf(stderr, "[dark_trace] cannot open \"%s\" for writing\n", dt);
            } else {
                std::fprintf(impl_->dark_trace,
                             "# call qx qy qz carve_dist ground_y open_to_sky "
                             "path_m path_measured darkness roof_y\n");
            }
        }
    }
    if (impl_->dark_trace != nullptr) {
        const EnclosureTrace tr = enclosure_trace(ctx.params.layout, {}, ground, world);
        std::fprintf(impl_->dark_trace,
                     "%llu %.3f %.3f %.3f %+.6f %.3f %d %.3f %d %.6f %.3f\n",
                     static_cast<unsigned long long>(impl_->dark_trace_call++),
                     static_cast<double>(world.x), static_cast<double>(world.y),
                     static_cast<double>(world.z),
                     static_cast<double>(tr.carve_distance),
                     static_cast<double>(tr.ground_y), tr.open_to_sky ? 1 : 0,
                     static_cast<double>(tr.path_from_mouth), tr.path_measured ? 1 : 0,
                     static_cast<double>(tr.darkness), static_cast<double>(tr.roof_y));
        return tr.darkness;
    }
    return enclosure_darkness(ctx.params.layout, {}, ground, world);
}

} // namespace dfn::world
