/*
Created: 09:08:2026 - 11:12:00
Last updated: 09:08:2026 - 11:12:00
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
/*
UPD:
- 09:08:2026 - 11:12:00: Stage 3 — initial tests.
*/

#include "engine/platform/render/sources/bgfx/BgfxPalette.h"

#include <doctest/doctest.h>

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
