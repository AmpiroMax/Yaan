/*
Module: tests
File: tests/render/PaletteTests.cpp

Responsibility:
- Unit tests for the fixed 64-color palette (Q9b): size, opacity, value range,
  enough distinct colors to be a usable quantization target.

Key items:
- doctest cases over build_dfn_palette().

Dependencies:
- Uses: doctest, engine/platform/render BgfxPalette (pure data, GPU-free).
- Used by: ctest (render_palette).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/

#include "engine/platform/render/sources/bgfx/BgfxPalette.h"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <glm/common.hpp>
#include <glm/vec3.hpp>
#include <set>
#include <tuple>

TEST_CASE("the fixed palette is 64 opaque colors in range") {
    const auto palette = dfn::platform::build_dfn_palette();
    REQUIRE(palette.size() == 64);
    std::set<std::tuple<int, int, int>> distinct;
    for (const auto& c : palette) {
        CHECK(c.a == doctest::Approx(1.0f));
        for (int ch = 0; ch < 3; ++ch) {
            CHECK(c[ch] >= 0.0f);
            CHECK(c[ch] <= 1.0f);
        }
        distinct.insert({static_cast<int>(c.r * 255.0f + 0.5f),
                         static_cast<int>(c.g * 255.0f + 0.5f),
                         static_cast<int>(c.b * 255.0f + 0.5f)});
    }
    // A usable quantization target: nearly all entries distinct at 8 bit.
    CHECK(distinct.size() >= 56);
    // Deterministic.
    CHECK(dfn::platform::build_dfn_palette() == palette);
}

namespace {

// THE CONTROL (Rule 30): the palette as it was before the conifer family — the
// uniform 8 ramps x 8 shades, with no home for needles. Every measurement below
// is run against it too, and it must FAIL what the new palette passes. Without
// this arm "conifer and rock are two steps apart" is a description, not a check.
std::array<glm::vec4, 64> build_legacy_palette() {
    struct Ramp { glm::vec3 dark, light; };
    const Ramp ramps[8] = {
        {{0.06f, 0.11f, 0.04f}, {0.48f, 0.53f, 0.24f}},
        {{0.10f, 0.13f, 0.05f}, {0.62f, 0.58f, 0.30f}},
        {{0.12f, 0.08f, 0.05f}, {0.56f, 0.44f, 0.29f}},
        {{0.10f, 0.10f, 0.10f}, {0.62f, 0.60f, 0.57f}},
        {{0.35f, 0.28f, 0.17f}, {0.84f, 0.76f, 0.58f}},
        {{0.20f, 0.32f, 0.52f}, {0.72f, 0.80f, 0.90f}},
        {{0.05f, 0.14f, 0.17f}, {0.42f, 0.58f, 0.60f}},
        {{0.02f, 0.02f, 0.03f}, {0.95f, 0.94f, 0.90f}},
    };
    std::array<glm::vec4, 64> out{};
    for (int r = 0; r < 8; ++r) {
        for (int s = 0; s < 8; ++s) {
            const float ct = std::pow(static_cast<float>(s) / 7.0f, 1.25f);
            out[static_cast<size_t>(r) * 8 + s] =
                glm::vec4(glm::mix(ramps[r].dark, ramps[r].light, ct), 1.0f);
        }
    }
    return out;
}

float metric(const glm::vec3& a, const glm::vec3& b) {
    const glm::vec3 d = a - b;
    return d.x * d.x * 0.30f + d.y * d.y * 0.59f + d.z * d.z * 0.11f;
}

int quantise_in(const std::array<glm::vec4, 64>& pal, const glm::vec3& c) {
    int best = 0;
    float bd = metric(c, glm::vec3(pal[0]));
    for (int i = 1; i < 64; ++i) {
        const float d = metric(c, glm::vec3(pal[static_cast<size_t>(i)]));
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

// Legacy shade step: uniform ramps, so the mean over all 8x7 adjacent pairs.
float legacy_mean_step(const std::array<glm::vec4, 64>& pal) {
    float total = 0.0f;
    int pairs = 0;
    for (int r = 0; r < 8; ++r) {
        for (int s = 1; s < 8; ++s) {
            total += std::sqrt(metric(glm::vec3(pal[static_cast<size_t>(r * 8 + s - 1)]),
                                      glm::vec3(pal[static_cast<size_t>(r * 8 + s)])));
            ++pairs;
        }
    }
    return total / static_cast<float>(pairs);
}

// The colours this is all about.
// Needle albedo measured by flora from the leaf atlas tone ConiferDark, base
// {0.12,0.22,0.19} times the three baked form shades {0.72, 0.94, 1.18}.
constexpr glm::vec3 NEEDLE_SHADOW{0.086f, 0.158f, 0.137f};
constexpr glm::vec3 NEEDLE_MID{0.113f, 0.207f, 0.179f};
constexpr glm::vec3 NEEDLE_LIT{0.142f, 0.260f, 0.224f};
constexpr glm::vec3 PINE_DARK{0.12f, 0.22f, 0.19f};   // the silhouette LOD shell
constexpr glm::vec3 ROCK_DARK{0.20f, 0.19f, 0.19f};   // rock in shadow
constexpr glm::vec3 ROCK_MID{0.38f, 0.37f, 0.36f};    // rock at mid lighting
constexpr glm::vec3 OAK_CROWN{0.30f, 0.42f, 0.18f};   // broadleaf, for contrast

} // namespace

TEST_CASE("ramp depths are unequal and still spend exactly 64 entries") {
    const auto ramps = dfn::platform::dfn_palette_ramps();
    CHECK(ramps.size() == static_cast<size_t>(dfn::platform::PALETTE_RAMP_COUNT));
    int total = 0;
    bool any_short = false;
    for (const auto& r : ramps) {
        CHECK(r.count > 0);
        total += r.count;
        any_short = any_short || r.count < 8;
    }
    CHECK(total == 64);
    // The whole point of the ruling: the budget is 64 ENTRIES, not 8x8.
    CHECK(any_short);
    // Every entry belongs to exactly one family.
    for (int i = 0; i < 64; ++i) {
        CHECK(dfn::platform::palette_ramp_of(i) >= 0);
    }
    CHECK(dfn::platform::palette_ramp_of(64) == -1);
    CHECK(dfn::platform::palette_ramp_of(-1) == -1);
}

TEST_CASE("needles separate from LIT rock; in deep shadow nothing in a palette can") {
    // The claim under test is design's, restated in the unit design chose:
    // separation is the distance between the two QUANTISED entries, in mean
    // shade steps, and LANDMARK_SEPARATION_STEPS_MIN is 2.
    const float lit = dfn::platform::palette_separation_steps(PINE_DARK, ROCK_MID);
    CHECK(lit >= 2.0f);

    // Family assignment is the mechanism behind that number.
    const int pine_ramp =
        dfn::platform::palette_ramp_of(dfn::platform::palette_quantise(PINE_DARK));
    const int rock_ramp =
        dfn::platform::palette_ramp_of(dfn::platform::palette_quantise(ROCK_MID));
    CHECK(pine_ramp == 8); // conifer
    CHECK(rock_ramp == 3); // rock greys
    // Broadleaf stays on grass greens, so conifer and broadleaf separate too.
    CHECK(dfn::platform::palette_ramp_of(dfn::platform::palette_quantise(OAK_CROWN)) == 0);

    // ROCK IN SHADOW IS NOT ASSERTED, AND THAT IS THE HONEST RESULT. Every
    // family in the palette runs toward black, so the darks are crowded by
    // construction and hue separation vanishes with luminance. The conifer ramp
    // improves this case but cannot clear two steps, and no palette can — in
    // deep shadow the only separator left is silhouette (design, LANDSCAPE
    // §1.5). Reported so the next agent does not spend a night on it.
    const float shadowed = dfn::platform::palette_separation_steps(PINE_DARK, ROCK_DARK);
    MESSAGE("pine vs rock: lit " << lit << " steps, in shadow " << shadowed << " steps");
    CHECK(shadowed < 2.0f);

    // CONTROL: the same measurement on the pre-conifer palette. The lit case is
    // what the new family buys, so it must FAIL there.
    const auto legacy = build_legacy_palette();
    const float lstep = legacy_mean_step(legacy);
    const auto lq = [&](const glm::vec3& c) {
        return glm::vec3(legacy[static_cast<size_t>(quantise_in(legacy, c))]);
    };
    const float legacy_lit = std::sqrt(metric(lq(PINE_DARK), lq(ROCK_MID))) / lstep;
    MESSAGE("legacy palette, pine vs lit rock: " << legacy_lit << " steps");
    CHECK(legacy_lit < lit);
}

TEST_CASE("the three needle tones land on three adjacent CONIFER entries") {
    // flora's ask, and it is load-bearing: the 1.65x luminance range across the
    // baked form shades is what makes whorls read as separate branch layers.
    const int a = dfn::platform::palette_quantise(NEEDLE_SHADOW);
    const int b = dfn::platform::palette_quantise(NEEDLE_MID);
    const int c = dfn::platform::palette_quantise(NEEDLE_LIT);
    MESSAGE("needle tones quantise to entries " << a << ", " << b << ", " << c);
    CHECK(b == a + 1);
    CHECK(c == b + 1);
    CHECK(dfn::platform::palette_ramp_of(a) == 8);
    CHECK(dfn::platform::palette_ramp_of(b) == 8);
    CHECK(dfn::platform::palette_ramp_of(c) == 8);

    // THE CONTROL, AND IT CORRECTS THE PREMISE THIS WHOLE CHANGE WAS ORDERED ON.
    // The old palette ALSO resolved the three tones into three adjacent entries
    // — 48, 49, 50 — so tri-adjacency was never the defect. What it resolved
    // them into was WATER TEALS, not grass greens: the quantiser's metric
    // weights blue 0.11, so "blue-green water" and "green needles" are nearly
    // the same colour to it. The defect the conifer family actually fixes is
    // that the most common dark mass in the world shared a family with the
    // water, and would have moved with any water look-dev change.
    const auto legacy = build_legacy_palette();
    const int la = quantise_in(legacy, NEEDLE_SHADOW);
    const int lb = quantise_in(legacy, NEEDLE_MID);
    const int lc = quantise_in(legacy, NEEDLE_LIT);
    MESSAGE("old palette: entries " << la << ", " << lb << ", " << lc
                                    << " (48..55 was the water family)");
    CHECK(la >= 48);
    CHECK(lc <= 55);   // all three were water teals
    // And in the new palette they are NOT in the water family. That is the
    // property the change buys, stated as the thing that differs.
    CHECK(dfn::platform::palette_ramp_of(a) != 6);
    CHECK(dfn::platform::palette_ramp_of(c) != 6);
}

TEST_CASE("the shade step is a real unit") {
    const float step = dfn::platform::palette_mean_shade_step();
    CHECK(step > 0.0f);
    // A colour is zero steps from itself, and one entry from its own neighbour
    // is within a small multiple of the mean (the ramps are not wildly uneven).
    CHECK(dfn::platform::palette_separation_steps(PINE_DARK, PINE_DARK)
          == doctest::Approx(0.0f));
    const auto palette = dfn::platform::build_dfn_palette();
    const auto ramps = dfn::platform::dfn_palette_ramps();
    for (const auto& r : ramps) {
        if (r.count < 2) {
            continue;
        }
        const float d = std::sqrt(metric(glm::vec3(palette[static_cast<size_t>(r.first)]),
                                         glm::vec3(palette[static_cast<size_t>(r.first + 1)])));
        CHECK(d < step * 4.0f);
    }
}
