/*
Module: engine/render
File: engine/render/sources/DebugDraw.cpp

Responsibility:
- Debug-draw helper implementation over IRenderer::debug_line.

Key items:
- debug_draw_axes / debug_draw_aabb / debug_draw_grid / debug_draw_arrow.

Dependencies:
- Uses: DebugDraw.h, IRenderer.
- Used by: dfn_render target.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/

#include "engine/render/sources/DebugDraw.h"

#include "engine/platform/render/interfaces/IRenderer.h"

#include <glm/geometric.hpp>

#include <cmath>

namespace dfn::render {

void debug_draw_axes(platform::IRenderer& renderer, const glm::mat4& transform,
                     float size) {
    const glm::vec3 origin(transform[3]);
    const glm::vec3 x = glm::vec3(transform[0]) * size;
    const glm::vec3 y = glm::vec3(transform[1]) * size;
    const glm::vec3 z = glm::vec3(transform[2]) * size;
    renderer.debug_line(origin, origin + x, 0xFF0000FFu); // +X red   (ABGR)
    renderer.debug_line(origin, origin + y, 0xFF00FF00u); // +Y green
    renderer.debug_line(origin, origin + z, 0xFFFF0000u); // +Z blue
}

void debug_draw_aabb(platform::IRenderer& renderer, const glm::vec3& min,
                     const glm::vec3& max, uint32_t color_rgba) {
    const glm::vec3 c[8] = {
        {min.x, min.y, min.z}, {max.x, min.y, min.z},
        {max.x, min.y, max.z}, {min.x, min.y, max.z},
        {min.x, max.y, min.z}, {max.x, max.y, min.z},
        {max.x, max.y, max.z}, {min.x, max.y, max.z},
    };
    constexpr int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0},  // bottom
                                  {4, 5}, {5, 6}, {6, 7}, {7, 4},  // top
                                  {0, 4}, {1, 5}, {2, 6}, {3, 7}}; // pillars
    for (const auto& e : edges) {
        renderer.debug_line(c[e[0]], c[e[1]], color_rgba);
    }
}

void debug_draw_grid(platform::IRenderer& renderer, const glm::vec3& center,
                     float height, float half_extent, float cell,
                     uint32_t color_rgba) {
    if (cell <= 0.0f || half_extent <= 0.0f) {
        return;
    }
    const int lines = static_cast<int>(half_extent / cell);
    for (int i = -lines; i <= lines; ++i) {
        const float offset = static_cast<float>(i) * cell;
        renderer.debug_line({center.x + offset, height, center.z - half_extent},
                            {center.x + offset, height, center.z + half_extent},
                            color_rgba);
        renderer.debug_line({center.x - half_extent, height, center.z + offset},
                            {center.x + half_extent, height, center.z + offset},
                            color_rgba);
    }
}

void debug_draw_arrow(platform::IRenderer& renderer, const glm::vec3& from,
                      const glm::vec3& to, uint32_t color_rgba) {
    renderer.debug_line(from, to, color_rgba);
    const glm::vec3 dir = to - from;
    const float len = glm::length(dir);
    if (len <= 1e-5f) {
        return;
    }
    const glm::vec3 n = dir / len;
    // Any vector not collinear with n gives a stable side vector.
    const glm::vec3 up = std::fabs(n.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f)
                                                : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 side = glm::normalize(glm::cross(n, up));
    const float head = len * 0.15f;
    const glm::vec3 base = to - n * head;
    renderer.debug_line(to, base + side * head * 0.5f, color_rgba);
    renderer.debug_line(to, base - side * head * 0.5f, color_rgba);
}

} // namespace dfn::render
