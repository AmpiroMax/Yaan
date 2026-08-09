/*
Created: 09:08:2026 - 20:55:10
Last updated: 09:08:2026 - 22:39:28
Module: engine/render
File: engine/render/sources/TerrainLod.cpp

Responsibility:
- TerrainLod implementation: quadtree descent by screen error, and the
  residency state machine that fades one level into another.

Key items:
- select_lod_nodes (recursive descent), LodResidency::update.

Dependencies:
- Uses: TerrainLod.h, glm, stdlib.
- Used by: dfn_render.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure: no GPU, no ECS, no clock. Keep it that way — this is the file that
  makes LOD policy testable at all.
*/
/*
UPD:
- 09:08:2026 - 20:55:10: Created with the LOD selection module.
- 09:08:2026 - 22:01:04: Resident-rectangle exclusion in the descent (coarse
  nodes stop where core's chunk streaming begins) + lod_skirt_depth_m.
- 09:08:2026 - 22:39:28: pending() rebuilt each update.
*/

#include "engine/render/sources/TerrainLod.h"

#include <algorithm>
#include <cmath>

namespace dfn::render {

namespace {

// Descends from `node`, appending the nodes that are good enough. A node is
// good enough when the eye is farther than its split distance; otherwise its
// four children are considered. Level 0 is always accepted (nothing finer
// exists), which is what terminates the recursion.
void descend(const LodNode& node, const glm::vec3& eye, glm::vec2 world_min,
             glm::vec2 world_max, const LodRect& resident, size_t max_nodes,
             std::vector<LodNode>& out) {
    if (out.size() >= max_nodes) {
        return;
    }
    // Cull nodes entirely outside the world rectangle: the quadtree root is a
    // power-of-two square that usually overhangs the generated world.
    const float size = lod_node_size_m(node.level);
    const float x0 = static_cast<float>(node.x) * size;
    const float z0 = static_cast<float>(node.z) * size;
    if (x0 >= world_max.x || z0 >= world_max.y || x0 + size <= world_min.x
        || z0 + size <= world_min.y) {
        return;
    }
    // The resident rectangle: ground core already draws at full chunk detail.
    // Wholly inside -> this node must not exist (it would draw the same ground
    // a second time). Straddling -> descend rather than accept, so that the
    // inside/outside decision is only ever taken on nodes small enough for it
    // to be exact. See the header for why level 0 makes that terminate.
    bool straddles_resident = false;
    if (!resident.empty()) {
        const bool outside = x0 >= resident.max.x || z0 >= resident.max.y
                          || x0 + size <= resident.min.x
                          || z0 + size <= resident.min.y;
        if (!outside) {
            const bool inside = x0 >= resident.min.x && z0 >= resident.min.y
                             && x0 + size <= resident.max.x
                             && z0 + size <= resident.max.y;
            if (inside) {
                return; // chunks own this ground
            }
            straddles_resident = true;
        }
    }
    if (node.level == 0) {
        // Nothing finer exists. A level-0 node that still straddles the
        // rectangle means the rectangle is not aligned to the 128 m node grid;
        // drawing it is the safe direction (an overlap band, not a hole).
        out.push_back(node);
        return;
    }
    if (!straddles_resident
        && lod_node_distance_m(node, eye) >= lod_split_distance_m(node.level)) {
        out.push_back(node);
        return;
    }
    const auto child_level = static_cast<uint8_t>(node.level - 1);
    // Children are indexed in the finer level's own coordinates. The ladder is
    // NOT a plain quadtree: 1 -> 4 m is a factor of four, the rest are factors
    // of two, so the child stride comes from the voxel sizes rather than from
    // an assumed 2.
    const auto stride = static_cast<int32_t>(
        std::lround(lod_node_size_m(node.level) / lod_node_size_m(child_level)));
    for (int32_t dz = 0; dz < stride; ++dz) {
        for (int32_t dx = 0; dx < stride; ++dx) {
            descend({child_level, node.x * stride + dx, node.z * stride + dz}, eye,
                    world_min, world_max, resident, max_nodes, out);
        }
    }
}

} // namespace

float lod_node_size_m(uint8_t level) {
    const uint32_t l = level < LOD_LEVEL_COUNT ? level : LOD_LEVEL_COUNT - 1;
    return LOD_VOXEL_SIZE_M[l] * static_cast<float>(LOD_NODE_VOXELS);
}

float lod_split_distance_m(uint8_t level) {
    const uint32_t l = level < LOD_LEVEL_COUNT ? level : LOD_LEVEL_COUNT - 1;
    return LOD_DISTANCE_PER_METRE * LOD_VOXEL_SIZE_M[l];
}

float lod_node_distance_m(const LodNode& node, const glm::vec3& eye) {
    const float size = lod_node_size_m(node.level);
    const float x0 = static_cast<float>(node.x) * size;
    const float z0 = static_cast<float>(node.z) * size;
    // Distance to the footprint RECTANGLE, zero inside it. Height is ignored on
    // purpose: this file knows no terrain heights, and a footprint distance is
    // always <= the true distance, so the error is on the side of more detail.
    const float dx = std::max({x0 - eye.x, 0.0f, eye.x - (x0 + size)});
    const float dz = std::max({z0 - eye.z, 0.0f, eye.z - (z0 + size)});
    return std::sqrt(dx * dx + dz * dz);
}

bool lod_node_is_silhouette(const LodNode& node, const glm::vec3& eye) {
    return lod_node_distance_m(node, eye) > LOD_SILHOUETTE_DISTANCE_M;
}

float lod_skirt_depth_m(uint8_t level, float max_border_step_m) {
    const uint32_t l = level < LOD_LEVEL_COUNT ? level : LOD_LEVEL_COUNT - 1;
    const float voxel = LOD_VOXEL_SIZE_M[l];
    const float measured = std::max(0.0f, max_border_step_m) * LOD_SKIRT_LADDER_RATIO;
    return std::max(measured, voxel * LOD_SKIRT_MIN_VOXELS);
}

std::vector<LodNode> select_lod_nodes(const glm::vec3& eye, glm::vec2 world_min,
                                      glm::vec2 world_max, size_t max_nodes) {
    return select_lod_nodes(eye, world_min, world_max, LodRect{}, max_nodes);
}

std::vector<LodNode> select_lod_nodes(const glm::vec3& eye, glm::vec2 world_min,
                                      glm::vec2 world_max, const LodRect& resident,
                                      size_t max_nodes) {
    std::vector<LodNode> out;
    if (world_max.x <= world_min.x || world_max.y <= world_min.y) {
        return out;
    }
    const auto root_level = static_cast<uint8_t>(LOD_LEVEL_COUNT - 1);
    const float root_size = lod_node_size_m(root_level);
    // Root tiles covering the world rectangle: one root is 8192 m, so a 2 km
    // world is a single root and a 10 km world is a small grid of them. Rooting
    // at a fixed grid rather than at "the world" keeps node ids world-size
    // independent — a world that grows does not renumber the nodes core has.
    const auto ix0 = static_cast<int32_t>(std::floor(world_min.x / root_size));
    const auto iz0 = static_cast<int32_t>(std::floor(world_min.y / root_size));
    const auto ix1 = static_cast<int32_t>(std::ceil(world_max.x / root_size));
    const auto iz1 = static_cast<int32_t>(std::ceil(world_max.y / root_size));
    for (int32_t iz = iz0; iz < iz1; ++iz) {
        for (int32_t ix = ix0; ix < ix1; ++ix) {
            descend({root_level, ix, iz}, eye, world_min, world_max, resident,
                    max_nodes, out);
        }
    }
    return out;
}

bool LodResidency::is_resident(const LodNode& node) const {
    return std::any_of(entries_.begin(), entries_.end(), [&](const Entry& e) {
        return e.node == node && e.resident;
    });
}

void LodResidency::mark_resident(const LodNode& node) {
    for (Entry& e : entries_) {
        if (e.node == node) {
            e.resident = true;
            return;
        }
    }
}

void LodResidency::update(const std::vector<LodNode>& selection, float dt_seconds) {
    to_load_.clear();
    to_release_.clear();
    draws_.clear();
    pending_.clear();

    for (Entry& e : entries_) {
        e.selected = false;
    }
    for (const LodNode& node : selection) {
        const auto it = std::find_if(entries_.begin(), entries_.end(),
                                     [&](const Entry& e) { return e.node == node; });
        if (it != entries_.end()) {
            it->selected = true;
            continue;
        }
        entries_.push_back({node, 0.0f, true, false});
        to_load_.push_back(node); // the ferry asks core for this mesh
    }

    const float step = LOD_FADE_SECONDS > 0.0f ? dt_seconds / LOD_FADE_SECONDS : 1.0f;
    for (Entry& e : entries_) {
        // A node fades in only once its mesh exists; an unfinished request must
        // not start a fade, or the ground has a hole for the duration.
        if (e.selected && e.resident) {
            e.fade = std::min(1.0f, e.fade + step);
        } else if (!e.selected) {
            e.fade = std::max(0.0f, e.fade - step);
        }
        if (e.resident && e.fade > 0.0f) {
            draws_.push_back({e.node, e.fade});
        }
        if (e.selected && !e.resident) {
            pending_.push_back(e.node); // still waiting on core
        }
    }

    // Release only what is deselected AND fully faded: the outgoing level keeps
    // covering the ground until the incoming one is completely in.
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (!it->selected && it->fade <= 0.0f) {
            if (it->resident) {
                to_release_.push_back(it->node);
            }
            it = entries_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace dfn::render
