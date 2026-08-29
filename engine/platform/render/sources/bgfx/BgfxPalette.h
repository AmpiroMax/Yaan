/*
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/BgfxPalette.h

Responsibility:
- The fixed 64-color palette for the optional palette post-process (Q9b), and
  the CPU mirror of the shader's quantiser so separation can be MEASURED rather
  than argued about.

Key items:
- build_dfn_palette(): pure, deterministic; uploaded to u_palette by the
  BgfxRenderer when RendererInitParams::palette_post is set.
- dfn_palette_ramps(): where each colour family starts and how deep it is.
  RAMP DEPTH IS NOT UNIFORM — see the note below.
- palette_quantise() / palette_mean_shade_step() / palette_separation_steps():
  the measurement design's LANDMARK_SEPARATION_STEPS_MIN is written against.

Dependencies:
- Uses: glm, C++ stdlib only (no bgfx — pure data, unit-testable).
- Used by: BgfxRenderer.cpp, tests/render/PaletteTests.cpp.

Notes:
- THE BUDGET IS 64 ENTRIES, NOT EIGHT FAMILIES OF EIGHT (design ruling,
  LANDSCAPE §4.2). Reclaiming shades from one family funds another, so a CONIFER
  family was added without deleting anything.
- WHAT A FAMILY'S SHADES BUY IS FREEDOM FROM BANDING, NOT SCREEN AREA. The first
  allocation took shades from water on the grounds that water covers one lake
  and a river; that was measured wrong. Water is the largest SMOOTH GRADIENT in
  the world, which is where banding shows, while sand is a thin dithered shore
  strip and dry olive only catches bright-grass highlights on already-dithered
  ground. Depths now: grass 8, dry olive 5, dirt 8, rock 8, sand 5, sky 8,
  water 8, neutrals 8, conifer 6.
- A SEPARATOR MUST MOVE RED OR GREEN. The quantiser's metric weights green 0.59,
  red 0.30 and blue 0.11, so a hue change that lives in blue is worth about 0.9
  shade steps against a floor of 2 and buys nothing. This is why "blue-green
  water" and "green needles" are nearly the same colour to it, and it is the
  rule that the first conifer endpoints broke.
- Look-dev values (stage-2 precedent), not gameplay constants; the user judges
  the result from the tour, and the quantiser is now a user-facing graphics
  setting, so every readability rule must hold with it ON.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep this header bgfx-free so tests can link it without a GPU.
- palette_quantise MIRRORS fs_upscale.sc. The luma weights (0.30/0.59/0.11) and
  the nearest-colour rule are the contract between them; change both or neither.
*/

#pragma once

#include <array>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <span>

namespace dfn::platform {

/// Number of colour families. Depths differ per family; see PALETTE_RAMPS.
inline constexpr int PALETTE_RAMP_COUNT = 9;
inline constexpr int PALETTE_SIZE_ENTRIES = 64;

/// One colour family's slice of the 64 entries.
struct PaletteRamp {
    int first = 0;  ///< index of its darkest entry
    int count = 0;  ///< how many shades it was given
};

/// 64 RGBA colors (alpha always 1), families laid out back to back dark->light.
[[nodiscard]] std::array<glm::vec4, PALETTE_SIZE_ENTRIES> build_dfn_palette();

/// Where each family sits. Sum of counts == PALETTE_SIZE_ENTRIES.
[[nodiscard]] std::span<const PaletteRamp> dfn_palette_ramps();

/// Index of the family containing `entry`, or -1.
[[nodiscard]] int palette_ramp_of(int entry);

/// Nearest palette entry to `linear_rgb` under the SHADER's metric (see the
/// header notice). This is what the frame will actually show with the
/// quantiser on.
[[nodiscard]] int palette_quantise(const glm::vec3& linear_rgb);

/// Mean distance between adjacent entries inside a family, averaged over all
/// families. The unit LANDMARK_SEPARATION_STEPS_MIN is written in.
[[nodiscard]] float palette_mean_shade_step();

/// Separation of two colours AS THE PLAYER SEES THEM: the distance between the
/// entries they quantise to, in mean shade steps.
///
/// Measured on the quantised pair rather than on "did the ramp change", because
/// a ramp change is a heuristic and not a guarantee — two families can sit
/// closer at their dark ends than one shade step, in which case "different ramp"
/// is a label rather than a distance (design's correction to its own wording).
[[nodiscard]] float palette_separation_steps(const glm::vec3& a, const glm::vec3& b);

} // namespace dfn::platform
