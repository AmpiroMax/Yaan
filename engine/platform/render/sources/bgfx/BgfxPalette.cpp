/*
Created: 09:08:2026 - 10:55:00
Last updated: 09:08:2026 - 10:55:00
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/BgfxPalette.cpp

Responsibility:
- build_dfn_palette(): eight hand-picked dark/light ramp pairs interpolated
  into 8 shades each with a slight gamma curve (denser darks — Daggerfall-ish).

Key items:
- build_dfn_palette().

Dependencies:
- Uses: BgfxPalette.h, glm.
- Used by: BgfxRenderer.cpp, tests/render/PaletteTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure and deterministic; covered by PaletteTests.
*/
/*
UPD:
- 09:08:2026 - 10:55:00: Stage 3 — initial implementation.
*/

#include "engine/platform/render/sources/bgfx/BgfxPalette.h"

#include <cmath>
#include <glm/common.hpp>
#include <glm/vec3.hpp>

namespace dfn::platform {

namespace {

struct Ramp {
    glm::vec3 dark;
    glm::vec3 light;
};

// Ramps tuned to the stage-3 scene: terrain splat colors, sky, water.
constexpr Ramp RAMPS[8] = {
    {{0.06f, 0.11f, 0.04f}, {0.48f, 0.53f, 0.24f}},  // grass greens
    {{0.10f, 0.13f, 0.05f}, {0.62f, 0.58f, 0.30f}},  // dry olive / upland
    {{0.12f, 0.08f, 0.05f}, {0.56f, 0.44f, 0.29f}},  // dirt browns
    {{0.10f, 0.10f, 0.10f}, {0.62f, 0.60f, 0.57f}},  // rock greys
    {{0.35f, 0.28f, 0.17f}, {0.84f, 0.76f, 0.58f}},  // sand tans
    {{0.20f, 0.32f, 0.52f}, {0.72f, 0.80f, 0.90f}},  // sky blues / haze
    {{0.05f, 0.14f, 0.17f}, {0.42f, 0.58f, 0.60f}},  // water teals
    {{0.02f, 0.02f, 0.03f}, {0.95f, 0.94f, 0.90f}},  // neutrals (shadow->bone)
};

} // namespace

std::array<glm::vec4, 64> build_dfn_palette() {
    std::array<glm::vec4, 64> palette{};
    for (int r = 0; r < 8; ++r) {
        for (int s = 0; s < 8; ++s) {
            const float t = static_cast<float>(s) / 7.0f;
            // Slight curve: more resolution in the darks (CRT-era ramps).
            const float ct = std::pow(t, 1.25f);
            const glm::vec3 c = glm::mix(RAMPS[r].dark, RAMPS[r].light, ct);
            palette[static_cast<size_t>(r) * 8 + s] = glm::vec4(c, 1.0f);
        }
    }
    return palette;
}

} // namespace dfn::platform
