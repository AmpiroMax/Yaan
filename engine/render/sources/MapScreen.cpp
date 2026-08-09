/*
Created: 09:08:2026 - 17:24:30
Last updated: 09:08:2026 - 17:24:30
Module: engine/render
File: engine/render/sources/MapScreen.cpp

Responsibility:
- MapScreen implementation: per-chunk tile baking (elevation ramp + hill shade
  + water), site marker memory, and screen composition (plate, frame, north
  tick, markers, player arrow).

Key items:
- MapScreen::note_chunk / note_site / compose; the marker stamp table.

Dependencies:
- Uses: MapScreen.h, PixelCanvas, core math views, generated Constants.h.
- Used by: RenderSystem.

Notes:
- Sampling: one map pixel covers (HEIGHTMAP_RESOLUTION-1)/MAP_TILE_PX samples
  per axis (2 at the shipping numbers). Height is averaged over that block but
  WATER is an OR over it: the river is 4-8 m wide, i.e. one map pixel — an
  averaging test would break it into dashes.
- Hill shade uses the neighbouring map pixels (8 m baseline), light from the
  north-west; it is what makes ridges read at 4 m per pixel, where a pure
  elevation ramp turns the valley into a smooth blur.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure CPU: no GPU handles, no ECS, no world includes.
*/
/*
UPD:
- 09:08:2026 - 17:24:30: Created with the map screen.
*/

#include "engine/render/sources/MapScreen.h"

#include "engine/core/config/sources/Constants.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace dfn::render {

namespace {

// --- Map palette (look-dev, NUMBERS.md migration list) ---------------------
constexpr Color MAP_BACKDROP{10, 10, 13};      // screen outside the plate
constexpr Color MAP_UNEXPLORED{28, 26, 32};    // plate, never visited
constexpr Color MAP_FRAME{150, 134, 96};       // parchment rule around the plate
constexpr Color MAP_OUTLINE{14, 12, 16};       // marker halo
constexpr Color MAP_WATER{34, 62, 86};
constexpr Color MAP_PLAYER{255, 236, 120};

// Elevation ramp stops (0 = sea level .. 1 = WORLDGEN_MAX_HEIGHT).
struct RampStop {
    float t;
    Color color;
};
constexpr std::array<RampStop, 5> MAP_RAMP{{
    {0.00f, {48, 60, 42}},    // valley floor, deep green
    {0.32f, {80, 94, 54}},    // meadow
    {0.58f, {126, 118, 76}},  // slopes, dry tan
    {0.80f, {154, 150, 140}}, // rock grey
    {1.00f, {212, 212, 206}}, // crag tops, near white
}};

// Hill-shade light: from the north-west, high. Terrain is lit as if the sun
// sat over the player's left shoulder when facing north — the classic
// topographic convention, so shapes read as ridges and not as trenches.
const glm::vec3 MAP_LIGHT = glm::normalize(glm::vec3(-0.55f, 0.72f, -0.42f));
// Vertical exaggeration for the SHADING ONLY (the standard cartographic
// z-factor). The valley floor rolls by a metre or two over a 3.2 m map pixel:
// at true scale every ground normal is within a few degrees of straight up and
// the whole plate shades to one flat value. Heights themselves are untouched.
constexpr float MAP_SHADE_Z_FACTOR = 4.0f;
// Shade factor relative to FLAT ground (which lands on 1.0 by construction):
// slopes facing the light brighten to MAP_SHADE_MAX, slopes facing away sink
// to MAP_SHADE_MIN. Stored quantized in the tile, decoded at compose().
constexpr float MAP_SHADE_MIN = 0.34f;
constexpr float MAP_SHADE_SPAN = 0.76f;
constexpr float MAP_SHADE_MAX = 1.55f;

Color ramp_color(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    for (size_t i = 1; i < MAP_RAMP.size(); ++i) {
        if (t <= MAP_RAMP[i].t) {
            const RampStop& a = MAP_RAMP[i - 1];
            const RampStop& b = MAP_RAMP[i];
            const float k = (t - a.t) / std::max(b.t - a.t, 1e-6f);
            return Color{
                static_cast<uint8_t>(static_cast<float>(a.color.r)
                                     + k * (static_cast<float>(b.color.r) - a.color.r)),
                static_cast<uint8_t>(static_cast<float>(a.color.g)
                                     + k * (static_cast<float>(b.color.g) - a.color.g)),
                static_cast<uint8_t>(static_cast<float>(a.color.b)
                                     + k * (static_cast<float>(b.color.b) - a.color.b))};
        }
    }
    return MAP_RAMP.back().color;
}

uint64_t pack_chunk(glm::ivec2 c) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(c.x)) << 32)
         | static_cast<uint64_t>(static_cast<uint32_t>(c.y));
}

// --- Marker silhouettes (§ art rule: silhouette + value, never detail) -----
const char* const DWELLING_ROWS[] = {"###", "###", "###"};
const char* const TRADER_ROWS[] = {"  #  ", " ### ", "#####", " ### ", "  #  "};
const char* const TAVERN_ROWS[] = {"#   #", "#   #", "#####", " ### ", "  #  "};
const char* const BARN_ROWS[] = {"#######", "#######", "#######"};
const char* const SHRINE_ROWS[] = {"  #  ", "  #  ", "#####", "#####", "  #  ", "  #  ",
                                   "  #  "};
const char* const DUNGEON_ROWS[] = {"#######", "#######", " ##### ", " ##### ",
                                    "  ###  ", "  ###  ", "   #   "};
const char* const TOWER_ROWS[] = {"# #", "###", "###", "###", "###", "###", "###"};
const char* const CASTLE_ROWS[] = {"# # # # #", "#########", "#########",
                                   "#########", "###   ###"};

struct MarkerStyle {
    Stamp stamp;
    Color color;
};

const MarkerStyle& marker_style(MapMarkerKind kind) {
    static const MarkerStyle STYLES[] = {
        {{3, 3, DWELLING_ROWS}, {222, 214, 196}}, // Dwelling — plain dot
        {{5, 5, TRADER_ROWS}, {236, 196, 96}},    // Trader — diamond
        {{5, 5, TAVERN_ROWS}, {232, 140, 72}},    // Tavern — goblet
        {{7, 3, BARN_ROWS}, {176, 132, 84}},      // Barn — wide low block
        {{5, 7, SHRINE_ROWS}, {176, 214, 255}},   // Shrine — cross
        {{7, 7, DUNGEON_ROWS}, {226, 72, 60}},    // Dungeon — dark mouth
        {{3, 7, TOWER_ROWS}, {176, 176, 188}},    // Tower ruin — broken column
        {{9, 5, CASTLE_ROWS}, {238, 206, 140}},   // Castle — crenellated mass
    };
    return STYLES[static_cast<size_t>(kind)];
}

} // namespace

MapMarkerKind map_marker_kind(uint32_t mesh_asset_id) {
    switch (mesh_asset_id) {
    case 1: return MapMarkerKind::Dwelling;
    case 2: return MapMarkerKind::Trader;
    case 3: return MapMarkerKind::Tavern;
    case 4: return MapMarkerKind::Barn;
    case 5: return MapMarkerKind::Shrine;
    case 6: return MapMarkerKind::Dungeon;
    case 7: return MapMarkerKind::TowerRuin;
    // 8..11 are the castle parts (hall / wall / gatehouse / solar): one place
    // on the map, not four overlapping marks.
    case 8:
    case 9:
    case 10:
    case 11: return MapMarkerKind::Castle;
    default: return MapMarkerKind::COUNT;
    }
}

void MapScreen::note_chunk(const math::HeightFieldView& field,
                           const math::SurfaceFieldView* surface) {
    if (field.resolution < 2 || field.heights.empty()) {
        return;
    }
    const uint32_t res = field.resolution;
    const uint32_t cells = res - 1; // 128 at the shipping numbers
    const float px_meters = static_cast<float>(config::CHUNK_SIZE)
                          / static_cast<float>(MAP_TILE_PX);

    const bool has_water = surface != nullptr && !surface->water_surface.empty()
                        && surface->resolution == res;

    // Pass 1: reduce the sample grid to map pixels. The sample block of pixel p
    // is [p * cells / TILE, (p+1) * cells / TILE) — exact for any ratio, so no
    // sample is skipped when the tile is finer than the block grid.
    const size_t tile_pixels = static_cast<size_t>(MAP_TILE_PX) * MAP_TILE_PX;
    Tile tile;
    tile.height.assign(tile_pixels, 0.0f);
    tile.shade.assign(tile_pixels, 0);
    tile.water.assign(tile_pixels, 0);
    for (uint32_t pz = 0; pz < MAP_TILE_PX; ++pz) {
        const uint32_t z0 = pz * cells / MAP_TILE_PX;
        const uint32_t z1 = std::max((pz + 1) * cells / MAP_TILE_PX, z0 + 1);
        for (uint32_t px = 0; px < MAP_TILE_PX; ++px) {
            const uint32_t x0 = px * cells / MAP_TILE_PX;
            const uint32_t x1 = std::max((px + 1) * cells / MAP_TILE_PX, x0 + 1);
            float sum = 0.0f;
            uint32_t count = 0;
            bool wet = false;
            for (uint32_t sz = z0; sz < z1; ++sz) {
                for (uint32_t sx = x0; sx < x1; ++sx) {
                    sum += field.height_at(sx, sz);
                    ++count;
                    if (has_water
                        && surface->water_surface[sz * res + sx] > math::NO_WATER) {
                        wet = true; // ANY sample: a 4-8 m river is one map pixel
                    }
                }
            }
            const size_t i = static_cast<size_t>(pz) * MAP_TILE_PX + px;
            tile.height[i] = sum / static_cast<float>(std::max(count, 1u));
            tile.water[i] = wet ? 1u : 0u;
        }
    }

    // Pass 2: hill shade from the neighbouring map pixels (3.2 m baseline).
    // Stored, not applied — the ramp it multiplies is resolved at compose().
    for (uint32_t pz = 0; pz < MAP_TILE_PX; ++pz) {
        for (uint32_t px = 0; px < MAP_TILE_PX; ++px) {
            const uint32_t xm = px > 0 ? px - 1 : px;
            const uint32_t xp = px + 1 < MAP_TILE_PX ? px + 1 : px;
            const uint32_t zm = pz > 0 ? pz - 1 : pz;
            const uint32_t zp = pz + 1 < MAP_TILE_PX ? pz + 1 : pz;
            const float dx = tile.height[static_cast<size_t>(pz) * MAP_TILE_PX + xp]
                           - tile.height[static_cast<size_t>(pz) * MAP_TILE_PX + xm];
            const float dz = tile.height[static_cast<size_t>(zp) * MAP_TILE_PX + px]
                           - tile.height[static_cast<size_t>(zm) * MAP_TILE_PX + px];
            const float run_x = px_meters * static_cast<float>(xp - xm);
            const float run_z = px_meters * static_cast<float>(zp - zm);
            const glm::vec3 normal = glm::normalize(
                glm::vec3(-dx * MAP_SHADE_Z_FACTOR / std::max(run_x, 1e-3f), 1.0f,
                          -dz * MAP_SHADE_Z_FACTOR / std::max(run_z, 1e-3f)));
            const float lambert = std::max(glm::dot(normal, MAP_LIGHT), 0.0f);
            const float factor =
                std::clamp(MAP_SHADE_MIN + MAP_SHADE_SPAN * (lambert / MAP_LIGHT.y),
                           MAP_SHADE_MIN, MAP_SHADE_MAX);
            tile.shade[static_cast<size_t>(pz) * MAP_TILE_PX + px] =
                static_cast<uint8_t>(factor / MAP_SHADE_MAX * 255.0f);
        }
    }

    const glm::ivec2 coord = field.chunk_coord;
    const auto [lo, hi] = std::minmax_element(tile.height.begin(), tile.height.end());
    if (!has_tiles_) {
        min_chunk_ = coord;
        max_chunk_ = coord;
        min_height_ = *lo;
        max_height_ = *hi;
        has_tiles_ = true;
    } else {
        min_chunk_ = glm::min(min_chunk_, coord);
        max_chunk_ = glm::max(max_chunk_, coord);
        min_height_ = std::min(min_height_, *lo);
        max_height_ = std::max(max_height_, *hi);
    }
    tiles_[pack_chunk(coord)] = std::move(tile);
}

void MapScreen::note_site(uint32_t mesh_asset_id, glm::vec3 position) {
    const MapMarkerKind kind = map_marker_kind(mesh_asset_id);
    if (kind == MapMarkerKind::COUNT) {
        return;
    }
    const float quantum =
        kind == MapMarkerKind::Castle ? MAP_CASTLE_MERGE_M : MAP_MARKER_MERGE_M;
    const auto cx = static_cast<int64_t>(std::floor(position.x / quantum));
    const auto cz = static_cast<int64_t>(std::floor(position.z / quantum));
    const uint64_t key = (static_cast<uint64_t>(kind) << 56)
                       | ((static_cast<uint64_t>(cx) & 0xFFFFFFFull) << 28)
                       | (static_cast<uint64_t>(cz) & 0xFFFFFFFull);
    if (!marker_keys_.insert(key).second) {
        return; // seen this cell before — the per-frame fast path
    }
    // Cell keys alone split a site that straddles a cell border (the castle
    // parts did exactly that), so a new cell is still merged by distance
    // against the markers already known. Runs once per cell, not per frame.
    const glm::vec2 here{position.x, position.z};
    for (const Marker& existing : markers_) {
        if (existing.kind != kind) {
            continue;
        }
        const glm::vec2 d = existing.position - here;
        if (glm::dot(d, d) <= quantum * quantum) {
            return;
        }
    }
    markers_.push_back({here, kind});
}

const PixelCanvas& MapScreen::compose(uint32_t width, uint32_t height, glm::vec3 eye,
                                      float yaw) {
    canvas_.resize(width, height);
    canvas_.clear(MAP_BACKDROP);

    const int screen_w = static_cast<int>(width);
    const int screen_h = static_cast<int>(height);
    const int avail_w = std::max(screen_w - 2 * MAP_MARGIN_PX, 1);
    const int avail_h = std::max(screen_h - 2 * MAP_MARGIN_PX, 1);

    const glm::ivec2 span = has_tiles_ ? (max_chunk_ - min_chunk_ + glm::ivec2{1, 1})
                                       : glm::ivec2{1, 1};
    const int full_w = span.x * static_cast<int>(MAP_TILE_PX);
    const int full_h = span.y * static_cast<int>(MAP_TILE_PX);

    // Integer downscale only (no zoom levels, no blur): the map keeps square
    // pixels at every internal resolution preset.
    int down = 1;
    while (full_w / down > avail_w || full_h / down > avail_h) {
        ++down;
    }
    const int plate_w = full_w / down;
    const int plate_h = full_h / down;
    const int ox = (screen_w - plate_w) / 2;
    const int oy = (screen_h - plate_h) / 2;

    // Plate: everything not visited stays flat dark — the explored-map reveal.
    canvas_.fill_rect(ox, oy, plate_w, plate_h, MAP_UNEXPLORED);

    // Elevation ramp over the EXPLORED span: on this valley the absolute
    // 0..WORLDGEN_MAX_HEIGHT normalization put every metre of ground into one
    // green band and the shape of the land vanished.
    const float height_span = std::max(max_height_ - min_height_, 1.0f);
    const int tile_px = static_cast<int>(MAP_TILE_PX) / down;
    for (const auto& [key, tile] : tiles_) {
        const glm::ivec2 coord{static_cast<int32_t>(static_cast<uint32_t>(key >> 32)),
                               static_cast<int32_t>(static_cast<uint32_t>(key))};
        const int tx = ox + (coord.x - min_chunk_.x) * tile_px;
        const int ty = oy + (coord.y - min_chunk_.y) * tile_px;
        for (int j = 0; j < tile_px; ++j) {
            const auto sz = static_cast<size_t>(j * static_cast<int>(MAP_TILE_PX) / tile_px);
            for (int i = 0; i < tile_px; ++i) {
                const auto sx =
                    static_cast<size_t>(i * static_cast<int>(MAP_TILE_PX) / tile_px);
                const size_t s = sz * MAP_TILE_PX + sx;
                Color color;
                if (tile.water[s] != 0) {
                    color = MAP_WATER;
                } else {
                    const float factor =
                        static_cast<float>(tile.shade[s]) / 255.0f * MAP_SHADE_MAX;
                    color = shade(ramp_color((tile.height[s] - min_height_) / height_span),
                                  factor);
                }
                canvas_.put(tx + i, ty + j, color);
            }
        }
    }

    canvas_.frame_rect(ox - 1, oy - 1, plate_w + 2, plate_h + 2, MAP_FRAME);

    // North tick above the plate: the map is north-up (world -Z is up), and a
    // pointer costs 6 pixels where a letter would need a font system.
    const float tick_cx = static_cast<float>(ox + plate_w / 2);
    const float tick_y = static_cast<float>(oy - 2);
    canvas_.fill_triangle({tick_cx, tick_y - 4.0f}, {tick_cx - 3.0f, tick_y},
                          {tick_cx + 3.0f, tick_y}, MAP_FRAME);

    // World -> plate pixel. One map pixel is CHUNK_SIZE / MAP_TILE_PX * down
    // meters (4 m at the shipping numbers, 640x360).
    const float world_x0 = static_cast<float>(min_chunk_.x)
                         * static_cast<float>(config::CHUNK_SIZE);
    const float world_z0 = static_cast<float>(min_chunk_.y)
                         * static_cast<float>(config::CHUNK_SIZE);
    const float px_per_m = static_cast<float>(MAP_TILE_PX)
                         / (static_cast<float>(config::CHUNK_SIZE) * static_cast<float>(down));
    const auto to_plate = [&](glm::vec2 world) {
        return glm::vec2{static_cast<float>(ox) + (world.x - world_x0) * px_per_m,
                         static_cast<float>(oy) + (world.y - world_z0) * px_per_m};
    };

    // Markers: dwellings first so the special sites of a hamlet stay on top.
    for (int pass = 0; pass < 2; ++pass) {
        for (const Marker& marker : markers_) {
            const bool plain = marker.kind == MapMarkerKind::Dwelling;
            if ((pass == 0) != plain) {
                continue;
            }
            const MarkerStyle& style = marker_style(marker.kind);
            const glm::vec2 p = to_plate(marker.position);
            if (p.x < static_cast<float>(ox) || p.y < static_cast<float>(oy)
                || p.x >= static_cast<float>(ox + plate_w)
                || p.y >= static_cast<float>(oy + plate_h)) {
                continue; // outside the explored plate
            }
            canvas_.draw_stamp(static_cast<int>(p.x) - style.stamp.width / 2,
                               static_cast<int>(p.y) - style.stamp.height / 2,
                               style.stamp, style.color, true, MAP_OUTLINE);
        }
    }

    // Player: an arrow, not a dot — position AND facing in one silhouette.
    // Camera convention: yaw 0 looks toward -Z (north = up on the map),
    // positive yaw turns east, so the plate-space direction is (sin, -cos).
    const glm::vec2 p = to_plate({eye.x, eye.z});
    const glm::vec2 dir{std::sin(yaw), -std::cos(yaw)};
    const glm::vec2 side{-dir.y, dir.x};
    const auto arrow = static_cast<float>(MAP_PLAYER_ARROW_PX);
    const glm::vec2 tip = p + dir * (arrow * 0.6f);
    const glm::vec2 left = p - dir * (arrow * 0.4f) + side * (arrow * 0.42f);
    const glm::vec2 right = p - dir * (arrow * 0.4f) - side * (arrow * 0.42f);
    const glm::vec2 tail = p - dir * (arrow * 0.15f);
    // Halo first (same arrow, one pixel out in every direction), then the arrow.
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            const glm::vec2 o{static_cast<float>(dx), static_cast<float>(dy)};
            canvas_.fill_triangle(tip + o, left + o, tail + o, MAP_OUTLINE);
            canvas_.fill_triangle(tip + o, tail + o, right + o, MAP_OUTLINE);
        }
    }
    canvas_.fill_triangle(tip, left, tail, MAP_PLAYER);
    canvas_.fill_triangle(tip, tail, right, MAP_PLAYER);

    return canvas_;
}

} // namespace dfn::render
