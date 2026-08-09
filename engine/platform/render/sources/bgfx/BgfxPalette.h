/*
Created: 09:08:2026 - 10:55:00
Last updated: 09:08:2026 - 10:55:00
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/BgfxPalette.h

Responsibility:
- The fixed 64-color palette for the optional palette post-process (Q9b):
  eight 8-shade ramps tuned to what the scene actually contains (greens,
  earth, rock grey, sand, sky blues, water teal, neutrals).

Key items:
- build_dfn_palette(): pure, deterministic; uploaded to u_palette by the
  BgfxRenderer when RendererInitParams::palette_post is set.

Dependencies:
- Uses: glm, C++ stdlib only (no bgfx — pure data, unit-testable).
- Used by: BgfxRenderer.cpp, tests/render/PaletteTests.cpp.

Notes:
- Look-dev values (stage-2 precedent), not gameplay constants; the user judges
  the result from the 4-way tour matrix (Q9 deferred decision).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep this header bgfx-free so tests can link it without a GPU.
*/
/*
UPD:
- 09:08:2026 - 10:55:00: Stage 3 — initial 64-color palette.
*/

#pragma once

#include <array>
#include <glm/vec4.hpp>

namespace dfn::platform {

// 64 RGBA colors (alpha always 1); layout: 8 ramps x 8 shades, dark -> light.
[[nodiscard]] std::array<glm::vec4, 64> build_dfn_palette();

} // namespace dfn::platform
