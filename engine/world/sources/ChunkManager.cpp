/*
Created: 09:08:2026 - 00:42:03
Last updated: 09:08:2026 - 00:42:03
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
*/
/*
UPD:
- 09:08:2026 - 00:42:03: Stage 2 — in-memory generator streaming (open_generated),
  hysteresis load/unload ring, batch spawn/destroy, event protocol.
*/

#include "engine/world/sources/ChunkManager.h"

#include <cstdlib>
#include <unordered_map>
#include <vector>

namespace dfn::world {

struct ChunkManager::Impl {
    bool opened = false;
    WorldGenParams gen_params;
    ChunkStreamingParams params;
    const SaveDelta* delta = nullptr; // stage 3: overlay on load

    std::unordered_map<uint64_t, Chunk> resident; // key = chunk_group(coord)
    std::vector<ChunkCoord> loaded_coords;        // cache for loaded_chunks()

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
ChunkManager::~ChunkManager() = default;

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

    // --- Load pass: missing chunks within the load radius, clipped to extent. -
    const int32_t r = static_cast<int32_t>(impl_->params.load_radius);
    for (int32_t dz = -r; dz <= r; ++dz) {
        for (int32_t dx = -r; dx <= r; ++dx) {
            const ChunkCoord coord{focus.x + dx, focus.z + dz};
            const uint64_t key = chunk_group(coord);
            if (!impl_->in_extent(coord) || impl_->resident.contains(key)) {
                continue;
            }
            Chunk chunk = generate_chunk(impl_->gen_params, coord);
            // Stage 3: apply impl_->delta overlay here before spawning (Q56).

            // Batch entity spawn for the chunk's generated records (Rule 11).
            // Stage-2 worldgen emits none; the path stays batch-only regardless.
            if (!chunk.entities.empty()) {
                std::vector<ecs::EntityId> ids(chunk.entities.size());
                ecs.spawn_batch(ids, key);
                // Component attachment from archetypes is gameplay/lead wiring
                // (arrives with content archetypes; world stays data-only).
            }

            impl_->resident.emplace(key, std::move(chunk));
            bus.publish(ChunkLoaded{coord});
            changed = true;
        }
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

} // namespace dfn::world
