/*
Created: 09:08:2026 - 17:52:40
Last updated: 09:08:2026 - 17:52:40
Module: tests
File: tests/render/MapScreenTests.cpp

Responsibility:
- Unit tests for the map screen: explored-chunk memory, marker kind mapping and
  de-duplication, and the composed image (size, water, player arrow, the
  unexplored plate staying dark).

Key items:
- doctest cases over MapScreen and PixelCanvas.

Dependencies:
- Uses: doctest, engine/render MapScreen/PixelCanvas.
- Used by: ctest (render_map_screen).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. GPU-free (Rule 3 spirit).
*/
/*
UPD:
- 09:08:2026 - 17:52:40: Initial tests with the map screen.
*/

#include "engine/render/sources/MapScreen.h"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using dfn::math::HeightFieldView;
using dfn::math::NO_WATER;
using dfn::math::SurfaceFieldView;
using dfn::render::MapMarkerKind;
using dfn::render::map_marker_kind;
using dfn::render::MapScreen;
using dfn::render::PixelCanvas;

namespace {

constexpr uint32_t RES = 129;

// A chunk that rises to the east, with a north-south water stripe in the
// middle — enough shape to exercise the ramp, the shade and the water OR.
struct FakeChunk {
    std::vector<uint16_t> heights;
    std::vector<float> water;
    std::vector<float> dist;
    std::vector<uint8_t> classes;

    FakeChunk() : heights(RES * RES), water(RES * RES, NO_WATER), dist(RES * RES, 50.0f),
                  classes(RES * RES, 0) {
        for (uint32_t z = 0; z < RES; ++z) {
            for (uint32_t x = 0; x < RES; ++x) {
                heights[z * RES + x] = static_cast<uint16_t>(x * 500);
                if (x == 64) {
                    water[z * RES + x] = 12.0f;
                }
            }
        }
    }

    HeightFieldView height_view(glm::ivec2 coord) const {
        HeightFieldView v;
        v.chunk_coord = coord;
        v.origin = {static_cast<float>(coord.x) * 256.0f,
                    static_cast<float>(coord.y) * 256.0f};
        v.resolution = RES;
        v.step = 2.0f;
        v.heights = heights;
        v.height_scale = 64.0f / 65535.0f;
        v.height_offset = 0.0f;
        return v;
    }

    SurfaceFieldView surface_view(glm::ivec2 coord) const {
        SurfaceFieldView v;
        v.chunk_coord = coord;
        v.resolution = RES;
        v.step = 2.0f;
        v.dist_to_water = dist;
        v.water_surface = water;
        v.surface_class = classes;
        return v;
    }
};

bool has_color(const PixelCanvas& canvas, uint8_t r, uint8_t g, uint8_t b) {
    const auto px = canvas.pixels();
    for (size_t i = 0; i + 3 < px.size(); i += 4) {
        if (px[i] == r && px[i + 1] == g && px[i + 2] == b) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("marker kinds follow the blessed mesh ids, castle parts merge") {
    CHECK(map_marker_kind(1) == MapMarkerKind::Dwelling);
    CHECK(map_marker_kind(6) == MapMarkerKind::Dungeon);
    CHECK(map_marker_kind(7) == MapMarkerKind::TowerRuin);
    for (uint32_t id = 8; id <= 11; ++id) {
        CHECK(map_marker_kind(id) == MapMarkerKind::Castle);
    }
    CHECK(map_marker_kind(0) == MapMarkerKind::COUNT);
    CHECK(map_marker_kind(12) == MapMarkerKind::COUNT);
}

TEST_CASE("sites are remembered once per place, castle parts collapse") {
    MapScreen map;
    map.note_site(1, {100.0f, 10.0f, 100.0f});
    map.note_site(1, {100.5f, 11.0f, 100.5f}); // same place, same frame later
    CHECK(map.known_sites() == 1);
    map.note_site(1, {140.0f, 10.0f, 100.0f}); // a neighbour house
    CHECK(map.known_sites() == 2);
    map.note_site(8, {800.0f, 40.0f, 200.0f});
    map.note_site(9, {812.0f, 40.0f, 214.0f});
    map.note_site(10, {790.0f, 40.0f, 190.0f});
    map.note_site(11, {805.0f, 40.0f, 205.0f});
    CHECK(map.known_sites() == 3); // one castle, not four
    map.note_site(99, {10.0f, 0.0f, 10.0f}); // not a site mesh
    CHECK(map.known_sites() == 3);
}

TEST_CASE("only visited chunks are drawn and they survive unloading") {
    const FakeChunk chunk;
    MapScreen map;
    CHECK(map.explored_chunks() == 0);
    const auto surface = chunk.surface_view({1, 1});
    map.note_chunk(chunk.height_view({1, 1}), &surface);
    CHECK(map.explored_chunks() == 1);
    map.note_chunk(chunk.height_view({1, 1}), nullptr); // re-upload is idempotent
    CHECK(map.explored_chunks() == 1);
    map.note_chunk(chunk.height_view({2, 1}), nullptr);
    CHECK(map.explored_chunks() == 2);
}

TEST_CASE("composed screen has the requested size, water and a player mark") {
    const FakeChunk chunk;
    MapScreen map;
    const auto surface = chunk.surface_view({0, 0});
    map.note_chunk(chunk.height_view({0, 0}), &surface);
    map.note_site(6, {60.0f, 10.0f, 60.0f});

    const PixelCanvas& canvas = map.compose(640, 360, {128.0f, 12.0f, 128.0f}, 0.0f);
    CHECK(canvas.width() == 640);
    CHECK(canvas.height() == 360);
    CHECK(canvas.pixels().size() == 640u * 360u * 4u);
    CHECK(has_color(canvas, 34, 62, 86));    // the water stripe
    CHECK(has_color(canvas, 255, 236, 120)); // the player arrow
    CHECK(has_color(canvas, 226, 72, 60));   // the dungeon marker
    CHECK(has_color(canvas, 10, 10, 13));    // backdrop outside the plate
}

TEST_CASE("smaller internal resolutions downscale by an integer factor") {
    const FakeChunk chunk;
    MapScreen map;
    for (int z = 0; z < 4; ++z) {
        for (int x = 0; x < 4; ++x) {
            map.note_chunk(chunk.height_view({x, z}), nullptr);
        }
    }
    // 4x4 chunks = 320 px at full scale: fits 640x360, must halve for 320x180.
    const PixelCanvas& big = map.compose(640, 360, {512.0f, 12.0f, 512.0f}, 0.0f);
    CHECK(big.width() == 640);
    const PixelCanvas& small = map.compose(320, 180, {512.0f, 12.0f, 512.0f}, 0.0f);
    CHECK(small.width() == 320);
    CHECK(small.height() == 180);
    // The plate must stay inside the screen: the corner pixel is backdrop.
    const auto px = small.pixels();
    CHECK(px[0] == 10);
    CHECK(px[1] == 10);
    CHECK(px[2] == 13);
}
