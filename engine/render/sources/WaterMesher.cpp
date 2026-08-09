/*
Created: 09:08:2026 - 11:57:20
Last updated: 09:08:2026 - 11:57:20
Module: engine/render
File: engine/render/sources/WaterMesher.cpp

Responsibility:
- build_lake_mesh / build_river_mesh implementation: ellipse fan and ribbon
  strip construction with world-space UVs.

Key items:
- build_lake_mesh(); build_river_mesh().

Dependencies:
- Uses: WaterMesher.h, glm.
- Used by: dfn_render target.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic pure functions; covered by WaterMesherTests.
*/
/*
UPD:
- 09:08:2026 - 11:57:20: Stage 3b — initial implementation.
*/

#include "engine/render/sources/WaterMesher.h"

#include <glm/geometric.hpp>

#include <cmath>

namespace dfn::render {

namespace {

constexpr float TAU = 6.28318530718f;
constexpr uint32_t WHITE = 0xFFFFFFFFu;
constexpr glm::vec3 UP{0.0f, 1.0f, 0.0f};

platform::Vertex water_vertex(glm::vec3 pos, float inv_tile) {
    return {pos, UP, {pos.x * inv_tile, pos.z * inv_tile}, WHITE};
}

} // namespace

MeshData build_lake_mesh(const math::LakePlane& lake, float uv_tile_m,
                         float edge_margin_m, uint32_t segments) {
    MeshData m;
    if (segments < 3 || uv_tile_m <= 0.0f) {
        return m;
    }
    const float inv_tile = 1.0f / uv_tile_m;
    const glm::vec2 half = lake.half_extent + glm::vec2{edge_margin_m};
    const float y = lake.surface_height;

    m.vertices.reserve(segments + 1);
    m.vertices.push_back(
        water_vertex({lake.center.x, y, lake.center.y}, inv_tile)); // center = 0
    for (uint32_t i = 0; i < segments; ++i) {
        const float a = TAU * static_cast<float>(i) / static_cast<float>(segments);
        m.vertices.push_back(water_vertex({lake.center.x + half.x * std::cos(a), y,
                                           lake.center.y + half.y * std::sin(a)},
                                          inv_tile));
    }
    m.indices.reserve(static_cast<size_t>(segments) * 3);
    for (uint32_t i = 0; i < segments; ++i) {
        const uint32_t r0 = 1 + i;
        const uint32_t r1 = 1 + (i + 1) % segments;
        // CCW seen from +Y: angle grows toward +Z (south), so r1 -> r0.
        m.indices.insert(m.indices.end(), {0, r1, r0});
    }
    return m;
}

MeshData build_river_mesh(std::span<const math::RiverStation> stations,
                          float uv_tile_m, float edge_margin_m) {
    MeshData m;
    if (stations.size() < 2 || uv_tile_m <= 0.0f) {
        return m;
    }
    const float inv_tile = 1.0f / uv_tile_m;
    const size_t count = stations.size();
    m.vertices.reserve(count * 2);

    for (size_t i = 0; i < count; ++i) {
        // Central-difference flow direction (contract: dir_i = next - this;
        // averaged with the previous segment for a smooth rim).
        const size_t prev = i > 0 ? i - 1 : i;
        const size_t next = i + 1 < count ? i + 1 : i;
        glm::vec2 dir = stations[next].position - stations[prev].position;
        const float len = glm::length(dir);
        dir = len > 1e-6f ? dir / len : glm::vec2{1.0f, 0.0f};
        const glm::vec2 perp{-dir.y, dir.x};
        const float half_w = stations[i].half_width + edge_margin_m;
        const float y = stations[i].surface_height;
        const glm::vec2 left = stations[i].position + perp * half_w;
        const glm::vec2 right = stations[i].position - perp * half_w;
        m.vertices.push_back(water_vertex({left.x, y, left.y}, inv_tile));
        m.vertices.push_back(water_vertex({right.x, y, right.y}, inv_tile));
    }

    m.indices.reserve((count - 1) * 6);
    for (size_t i = 0; i + 1 < count; ++i) {
        const auto l0 = static_cast<uint32_t>(i * 2);
        const auto r0 = l0 + 1;
        const auto l1 = l0 + 2;
        const auto r1 = l0 + 3;
        m.indices.insert(m.indices.end(), {l0, r1, r0, l0, l1, r1});
    }
    return m;
}

} // namespace dfn::render
