/*
Module: engine/render
File: engine/render/sources/DebugDraw.h

Responsibility:
- Debug-draw helpers built on IRenderer::debug_line: world axes, AABBs, grids,
  direction arrows. One-frame immediate primitives for visual diagnosis.

Key items:
- debug_draw_axes / debug_draw_aabb / debug_draw_grid / debug_draw_arrow.

Dependencies:
- Uses: engine/platform/render (IRenderer, forward declaration only), glm.
- Used by: engine/app, engine/editor, tour-time overlays, teammates' visual
  debugging (physics capsules, chunk borders).

Notes:
- Free functions, no state (Rule 9 spirit): each call forwards lines to the
  renderer for the current frame only. IRenderer::debug_line is a release-build
  no-op, so these inherit that for free.
- Colors are 0xAABBGGRR packed, matching IRenderer's convention.
- AABB parameters are raw min/max vectors; once core/math's Aabb type is agreed
  at the sync, an overload taking it is added additively (no breakage).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Public contract, frozen for the stage (Rule 26): changes only via group sync.
- Debug visuals only — never gameplay-relevant rendering.
*/

#pragma once

#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace dfn::platform {
class IRenderer;
}

namespace dfn::render {

// RGB axes at `transform`'s origin: +X red, +Y green, +Z blue, `size` meters.
void debug_draw_axes(platform::IRenderer& renderer, const glm::mat4& transform,
                     float size);

// Wireframe axis-aligned box, world space.
void debug_draw_aabb(platform::IRenderer& renderer, const glm::vec3& min,
                     const glm::vec3& max, uint32_t color_rgba);

// Horizontal grid on the XZ plane at `height`, centered on `center`,
// `half_extent` meters each way, `cell` meters per cell. Chunk-border diagnosis.
void debug_draw_grid(platform::IRenderer& renderer, const glm::vec3& center,
                     float height, float half_extent, float cell,
                     uint32_t color_rgba);

// Line with an arrowhead — velocities, facing directions, normals.
void debug_draw_arrow(platform::IRenderer& renderer, const glm::vec3& from,
                      const glm::vec3& to, uint32_t color_rgba);

} // namespace dfn::render
