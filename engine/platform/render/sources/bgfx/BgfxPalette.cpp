/*
Created: 09:08:2026 - 10:55:00
Last updated: 10:08:2026 - 00:15:26
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/BgfxPalette.cpp

Responsibility:
- build_dfn_palette(): nine colour families of UNEQUAL depth, each interpolated
  dark -> light with a slight gamma curve (denser darks — Daggerfall-ish).
- The CPU mirror of the shader's quantiser and the shade-step metric.

Key items:
- RAMPS table, build_dfn_palette, dfn_palette_ramps, palette_quantise,
  palette_mean_shade_step, palette_separation_steps.

Dependencies:
- Uses: BgfxPalette.h, glm.
- Used by: BgfxRenderer.cpp, tests/render/PaletteTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure and deterministic; covered by PaletteTests, which measures the pine/rock
  case that motivated the conifer family and carries the OLD palette as its
  control.
*/
/*
UPD:
- 09:08:2026 - 10:55:00: Stage 3 — initial implementation.
- 09:08:2026 - 23:52:26: Conifer family + non-uniform ramp depths + the CPU
  quantiser and shade-step metric (see the header UPD for the reasoning).
- 10:08:2026 - 00:20:00: Ramp depths re-allocated (design's measured second pass, plus
  a searched correction): olive 8->5 funds water 5->8. Water is the largest
  smooth gradient in the world and banding, not surface area, is what its
  shades buy. Water 7 — design's number — steals the lit needle tone back into
  the water family; 8 does not.
- 10:08:2026 - 00:15:26: conifer 6->8, paid by dry olive and sand at 4 (design's
  ruling, 4e4df0b). Taken for the FRAGILITY and not the headline number: at
  conifer 6 the allocation held only because water happened to be 8, and an
  allocation that survives by coincidence still leaves the forest's family to a
  nearest-colour accident. pine vs lit rock 2.24 -> 2.34.
*/

#include "engine/platform/render/sources/bgfx/BgfxPalette.h"

#include <cmath>
#include <glm/common.hpp>
#include <glm/vec3.hpp>

namespace dfn::platform {

namespace {

struct RampDef {
    glm::vec3 dark;
    glm::vec3 light;
    int shades;
};

// Depths are the design ruling, not a layout convenience: a family gets shades
// in proportion to the lighting range it carries and the screen area it covers.
//
// The allocation is design's SECOND, measured one, and it corrects an argument
// I helped make. "Water only serves one lake, so take its shades" is a claim
// about surface AREA; the quantity that matters is BANDING VISIBILITY, and
// water is the largest smooth gradient in the world while sand is a thin
// dithered shore strip. Taking the shades from DRY OLIVE instead is better on
// both axes at the same 64 entries: water's shade step 0.105 -> 0.070 (smoother
// gradient) and pine vs lit rock 2.19 -> 2.22 steps. Design also withdrew the
// reason it first offered for cutting dry olive — no MATERIAL targets it, but
// the quantiser runs on the final image and bright and dry grass pixels reach
// it, so it is not unused, merely the cheapest place to economise.
//
// WATER GETS 8, NOT THE 7 DESIGN PROPOSED, AND THE DIFFERENCE IS NOT COSMETIC.
// At water 7 an entry lands on the needle line and STEALS the lit needle tone
// back into the water family — the exact defect the conifer family exists to
// remove — while at 8 it does not. Searched rather than guessed: this is the
// allocation that minimises water's shade step (0.060, against 0.070 for
// design's proposal and 0.105 for the first landing) while KEEPING the three
// needle tones on three adjacent conifer entries and pine/lit-rock at 2.24.
//
// CONIFER IS 8, AND THE REASON IS THE FRAGILITY AND NOT THE HEADLINE NUMBER
// (design's ruling, §4.2, commit 4e4df0b). At conifer 6 the allocation holds
// only because water happens to be 8: at water 7 an entry lands on the needle
// line and steals the lit tone back into the water family. An allocation that
// survives by coincidence is still leaving the forest's family to a
// nearest-colour accident, which is the exact thing this change was bought to
// end. Worse, it breaks SILENTLY and it breaks toward "the forest quietly
// becomes water-coloured again" the moment flora ships new needle tones — a
// silent regression into the bug the change was made to prevent.
//
// Conifer 8 removes the coincidence. It is paid for by dry olive and sand at 4,
// which is design's banding-visibility principle applied where it costs: a thin
// dithered shore strip and a highlight extension on already-dithered grass,
// against the largest dark mass in the world.
//
// RE-VERIFY WHEN FLORA'S NEEDLE TONES LAND. A derivation is only as current as
// what it was derived from; PaletteTests goes red if the three tones stop
// landing on three adjacent conifer entries.
constexpr RampDef RAMPS[PALETTE_RAMP_COUNT] = {
    {{0.06f, 0.11f, 0.04f}, {0.48f, 0.53f, 0.24f}, 8},  // grass greens
    {{0.10f, 0.13f, 0.05f}, {0.62f, 0.58f, 0.30f}, 4},  // dry olive / upland
    {{0.12f, 0.08f, 0.05f}, {0.56f, 0.44f, 0.29f}, 8},  // dirt browns (bark too)
    {{0.10f, 0.10f, 0.10f}, {0.62f, 0.60f, 0.57f}, 8},  // rock greys
    {{0.35f, 0.28f, 0.17f}, {0.84f, 0.76f, 0.58f}, 4},  // sand tans (shore)
    {{0.20f, 0.32f, 0.52f}, {0.72f, 0.80f, 0.90f}, 8},  // sky blues / haze
    {{0.05f, 0.14f, 0.17f}, {0.42f, 0.58f, 0.60f}, 8},  // water teals (see note)
    {{0.02f, 0.02f, 0.03f}, {0.95f, 0.94f, 0.90f}, 8},  // neutrals (shadow->bone)
    // CONIFER NEEDLES, DERIVED AND NOT PICKED. The family runs along the ray
    // through flora's measured needle albedo {0.12, 0.22, 0.19}, from 0.17x to
    // 1.66x of it, so its entries land ON the three baked form tones the leaf
    // atlas actually emits instead of near them.
    //
    // THE FIRST ATTEMPT WAS PICKED BY HUE AND FAILED MEASUREMENT. A "cold
    // blue-green" reads as obviously distinct to a human and is nearly invisible
    // to this quantiser, whose metric weights green 0.59, red 0.30 and BLUE ONLY
    // 0.11: a hue change that moves blue moves almost nothing. Water teals and
    // needles sit at practically the same point in the (r, g) plane, which is
    // why the needle tones quantised to WATER — in the old palette too, not to
    // grass greens as everyone including this file assumed. Aim a separator at
    // red and green, or measure before believing it.
    //
    // Six shades because the three form tones span a 1.65x luminance range
    // (flora, measured) and must land on three ADJACENT entries — collapse them
    // onto two and the whorls read as one dark mass again, which is the thing
    // the tree rewrite exists to fix.
    {{0.020f, 0.045f, 0.038f}, {0.20f, 0.365f, 0.315f}, 8}, // conifer needles
};

constexpr PaletteRamp make_ramps_table(int i, int running) {
    return PaletteRamp{running, RAMPS[i].shades};
}

std::array<PaletteRamp, PALETTE_RAMP_COUNT> build_ramp_table() {
    std::array<PaletteRamp, PALETTE_RAMP_COUNT> table{};
    int running = 0;
    for (int i = 0; i < PALETTE_RAMP_COUNT; ++i) {
        table[static_cast<size_t>(i)] = make_ramps_table(i, running);
        running += RAMPS[i].shades;
    }
    return table;
}

// The shader's metric (fs_upscale.sc): squared difference weighted by luma.
float metric(const glm::vec3& a, const glm::vec3& b) {
    const glm::vec3 d = a - b;
    return d.x * d.x * 0.30f + d.y * d.y * 0.59f + d.z * d.z * 0.11f;
}

} // namespace

std::array<glm::vec4, PALETTE_SIZE_ENTRIES> build_dfn_palette() {
    std::array<glm::vec4, PALETTE_SIZE_ENTRIES> palette{};
    int at = 0;
    for (const RampDef& ramp : RAMPS) {
        for (int s = 0; s < ramp.shades; ++s) {
            const float t = ramp.shades > 1
                                ? static_cast<float>(s) / static_cast<float>(ramp.shades - 1)
                                : 0.0f;
            // Slight curve: more resolution in the darks (CRT-era ramps).
            const float ct = std::pow(t, 1.25f);
            const glm::vec3 c = glm::mix(ramp.dark, ramp.light, ct);
            palette[static_cast<size_t>(at)] = glm::vec4(c, 1.0f);
            ++at;
        }
    }
    return palette;
}

std::span<const PaletteRamp> dfn_palette_ramps() {
    static const std::array<PaletteRamp, PALETTE_RAMP_COUNT> table = build_ramp_table();
    return table;
}

int palette_ramp_of(int entry) {
    const auto ramps = dfn_palette_ramps();
    for (int i = 0; i < static_cast<int>(ramps.size()); ++i) {
        if (entry >= ramps[static_cast<size_t>(i)].first
            && entry < ramps[static_cast<size_t>(i)].first + ramps[static_cast<size_t>(i)].count) {
            return i;
        }
    }
    return -1;
}

int palette_quantise(const glm::vec3& linear_rgb) {
    const auto palette = build_dfn_palette();
    int best = 0;
    float best_d = metric(linear_rgb, glm::vec3(palette[0]));
    for (int i = 1; i < PALETTE_SIZE_ENTRIES; ++i) {
        const float d = metric(linear_rgb, glm::vec3(palette[static_cast<size_t>(i)]));
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

float palette_mean_shade_step() {
    const auto palette = build_dfn_palette();
    const auto ramps = dfn_palette_ramps();
    float total = 0.0f;
    int pairs = 0;
    for (const PaletteRamp& ramp : ramps) {
        for (int s = 1; s < ramp.count; ++s) {
            const auto a = glm::vec3(palette[static_cast<size_t>(ramp.first + s - 1)]);
            const auto b = glm::vec3(palette[static_cast<size_t>(ramp.first + s)]);
            total += std::sqrt(metric(a, b));
            ++pairs;
        }
    }
    return pairs > 0 ? total / static_cast<float>(pairs) : 0.0f;
}

float palette_separation_steps(const glm::vec3& a, const glm::vec3& b) {
    const auto palette = build_dfn_palette();
    const auto qa = glm::vec3(palette[static_cast<size_t>(palette_quantise(a))]);
    const auto qb = glm::vec3(palette[static_cast<size_t>(palette_quantise(b))]);
    const float step = palette_mean_shade_step();
    if (step <= 0.0f) {
        return 0.0f;
    }
    return std::sqrt(metric(qa, qb)) / step;
}

} // namespace dfn::platform
