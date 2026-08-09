/*
Created: 09:08:2026 - 11:12:00
Last updated: 09:08:2026 - 11:12:00
Module: tests
File: tests/render/ProcTextureTests.cpp

Responsibility:
- Unit tests for the procedural texture module: tileability (periodic noise
  wraps exactly), determinism (byte-identical output), seed/kind variation,
  limited color count (quantized ramps), atlas layout contract.

Key items:
- doctest cases over tileable_fbm / value_noise01 / generate_proc_texture /
  generate_terrain_atlas.

Dependencies:
- Uses: doctest, engine/render ProcTexture.
- Used by: ctest (render_proc_texture).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 11:12:00: Stage 3 — initial tests.
*/

#include "engine/render/sources/ProcTexture.h"

#include <doctest/doctest.h>

#include <cstring>
#include <set>

namespace {

using dfn::render::generate_proc_texture;
using dfn::render::generate_terrain_atlas;
using dfn::render::ProcTextureDesc;
using dfn::render::ProcTextureKind;
using dfn::render::tileable_fbm;
using dfn::render::value_noise01;

size_t distinct_colors(const std::vector<uint8_t>& pixels) {
    std::set<uint32_t> colors;
    for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
        uint32_t c = 0;
        std::memcpy(&c, pixels.data() + i, 4);
        colors.insert(c);
    }
    return colors.size();
}

} // namespace

TEST_CASE("tileable_fbm wraps exactly at the tile border (both axes)") {
    const glm::ivec2 period{6, 4};
    for (float t : {0.0f, 0.13f, 0.5f, 0.77f}) {
        CHECK(tileable_fbm({t, 0.0f}, period, 7u, 4)
              == tileable_fbm({t, 1.0f}, period, 7u, 4));
        CHECK(tileable_fbm({0.0f, t}, period, 7u, 4)
              == tileable_fbm({1.0f, t}, period, 7u, 4));
    }
}

TEST_CASE("noise fields are deterministic and vary with the seed") {
    const glm::vec2 p{0.31f, 0.64f};
    CHECK(tileable_fbm(p, {8, 8}, 42u, 3) == tileable_fbm(p, {8, 8}, 42u, 3));
    CHECK(tileable_fbm(p, {8, 8}, 42u, 3) != tileable_fbm(p, {8, 8}, 43u, 3));
    CHECK(value_noise01({123.4f, 567.8f}, 1u) == value_noise01({123.4f, 567.8f}, 1u));
    CHECK(value_noise01({123.4f, 567.8f}, 1u) != value_noise01({123.4f, 567.8f}, 2u));
    // Range invariant.
    for (int i = 0; i < 50; ++i) {
        const float v = tileable_fbm({0.02f * static_cast<float>(i), 0.5f},
                                     {8, 8}, 9u, 4);
        CHECK(v >= 0.0f);
        CHECK(v <= 1.0f);
    }
}

TEST_CASE("generated textures are deterministic, opaque and low-color") {
    for (const auto kind : {ProcTextureKind::GRASS, ProcTextureKind::ROCK,
                            ProcTextureKind::SAND, ProcTextureKind::DIRT,
                            ProcTextureKind::WATER}) {
        ProcTextureDesc desc;
        desc.kind = kind;
        desc.size = 64;
        desc.seed = 5;
        const auto a = generate_proc_texture(desc);
        const auto b = generate_proc_texture(desc);
        REQUIRE(a.size() == static_cast<size_t>(64) * 64 * 4);
        CHECK(a == b); // byte-identical (cache-by-params soundness)
        for (size_t i = 3; i < a.size(); i += 4) {
            REQUIRE(a[i] == 255); // opaque
        }
        // Limited palette read (Q9 aesthetic): quantized ramps stay small.
        CHECK(distinct_colors(a) <= 16);
        CHECK(distinct_colors(a) >= 3); // but not degenerate
    }

    // Different seeds -> different pixels (same kind).
    ProcTextureDesc d1;
    d1.size = 64;
    d1.seed = 5;
    ProcTextureDesc d2 = d1;
    d2.seed = 6;
    CHECK(generate_proc_texture(d1) != generate_proc_texture(d2));
}

TEST_CASE("terrain atlas honors the 2x2 layout contract") {
    constexpr uint32_t CELL = 32;
    constexpr uint32_t SIDE = CELL * 2;
    const auto atlas = generate_terrain_atlas(CELL, 5u);
    REQUIRE(atlas.size() == static_cast<size_t>(SIDE) * SIDE * 4);

    const ProcTextureKind cells[4] = {ProcTextureKind::GRASS, ProcTextureKind::ROCK,
                                      ProcTextureKind::SAND, ProcTextureKind::DIRT};
    for (uint32_t i = 0; i < 4; ++i) {
        ProcTextureDesc desc;
        desc.kind = cells[i];
        desc.size = CELL;
        desc.seed = 5u;
        const auto solo = generate_proc_texture(desc);
        const uint32_t x0 = (i & 1u) * CELL;
        const uint32_t y0 = (i >> 1u) * CELL;
        for (uint32_t y = 0; y < CELL; ++y) {
            const uint8_t* atlas_row =
                atlas.data() + (static_cast<size_t>(y0 + y) * SIDE + x0) * 4;
            const uint8_t* solo_row = solo.data() + static_cast<size_t>(y) * CELL * 4;
            REQUIRE(std::memcmp(atlas_row, solo_row, CELL * 4) == 0);
        }
    }
}

TEST_CASE("degenerate sizes yield empty buffers, never UB") {
    ProcTextureDesc desc;
    desc.size = 0;
    CHECK(generate_proc_texture(desc).empty());
    CHECK(generate_terrain_atlas(0, 1u).empty());
}
