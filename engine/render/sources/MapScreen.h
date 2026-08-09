/*
Created: 09:08:2026 - 17:20:05
Last updated: 09:08:2026 - 17:20:05
Module: engine/render
File: engine/render/sources/MapScreen.h

Responsibility:
- The world map screen (user request: "миникарта как в скайриме по нажатию на
  клавишу"). Bakes a top-down tile per VISITED chunk from the heightfield
  (elevation value + hill shade + water), remembers discovered sites, and
  composes the whole screen — terrain, markers, player position and facing —
  into a PixelCanvas at the internal resolution.

Key items:
- MapMarkerKind: the marker silhouettes (dwelling..castle).
- MapScreen: set_open/toggle/open, note_chunk, note_site, compose.
- MAP_TILE_PX and the map look-dev constants (pending NUMBERS.md, Rule 14).

Dependencies:
- Uses: PixelCanvas, core math (HeightFieldView / SurfaceFieldView), generated
  Constants.h, glm. No GPU, no ECS, no world headers.
- Used by: RenderSystem (bakes on upload_terrain, notes sites while drawing,
  blits the canvas as a fullscreen overlay).

Notes:
- EXPLORED MAP: a chunk enters the map when it is first uploaded (i.e. when the
  player streamed it in) and NEVER leaves — unloading drops the GPU mesh, not
  the memory of having been there. Same for sites. Nothing is drawn for chunks
  the player never visited, which is the Skyrim-style reveal the user asked for.
- Scale is fixed (MAP_TILE_PX pixels per chunk = 4 m per pixel at 640x360);
  compose() only picks an integer DOWNSCALE so the map still fits smaller
  internal resolutions (320x180) with square, crisp pixels. No zoom levels.
- The map is opaque: at 640x360 (and under the 64-colour palette post) a
  translucent overlay destroys the legibility of both layers.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep this pure and GPU-free; the renderer owns texture/mesh lifetime.
- The numeric constants below are look-dev/layout values on the NUMBERS.md
  migration list (messaged to the lead) — do not scatter new ones elsewhere.
*/
/*
UPD:
- 09:08:2026 - 17:20:05: Created — first map screen (terrain value + hill
  shade + water, site silhouettes, player arrow, explored-chunk reveal).
*/

#pragma once

#include "engine/core/math/sources/HeightField.h"
#include "engine/core/math/sources/SurfaceField.h"
#include "engine/render/sources/PixelCanvas.h"

#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dfn::render {

// --- Map layout constants (look-dev; NUMBERS.md migration requested) --------

/// Baked map pixels per chunk side. 256 m / 64 px = 4 m per map pixel, so the
/// 1024 m testbed is a 256x256 px map inside the 640x360 internal target.
inline constexpr uint32_t MAP_TILE_PX = 64;
/// Border between the map plate and the screen edge, internal pixels.
inline constexpr int MAP_MARGIN_PX = 6;
/// Site silhouette box (the stamps below are drawn inside it).
inline constexpr int MAP_MARKER_PX = 7;
/// Player arrow length along the facing direction, internal pixels.
inline constexpr int MAP_PLAYER_ARROW_PX = 9;
/// Position quantization for marker de-duplication (meters): buildings of one
/// hamlet stay separate dots, the multi-part castle collapses into one mark.
inline constexpr float MAP_MARKER_MERGE_M = 6.0f;
inline constexpr float MAP_CASTLE_MERGE_M = 64.0f;

/// What a marker looks like. Derived from the blessed RenderMesh ids 1..11
/// (1 dwelling .. 7 tower ruin, 8..11 castle parts collapse into Castle).
enum class MapMarkerKind : uint8_t {
    Dwelling = 0,
    Trader,
    Tavern,
    Barn,
    Shrine,
    Dungeon,
    TowerRuin,
    Castle,
    COUNT
};

/// Maps a RenderMesh asset id to a marker kind; returns COUNT for ids that are
/// not sites (the map ignores them).
[[nodiscard]] MapMarkerKind map_marker_kind(uint32_t mesh_asset_id);

class MapScreen {
public:
    // Visibility ---------------------------------------------------------------
    void set_open(bool open) { open_ = open; }
    void toggle() { open_ = !open_; }
    [[nodiscard]] bool open() const { return open_; }

    // Discovery ----------------------------------------------------------------
    /// Bakes (or re-bakes) the chunk's map tile. Called from
    /// RenderSystem::upload_terrain, i.e. exactly when the player streams the
    /// chunk in. `surface` may be nullptr (no water information).
    void note_chunk(const math::HeightFieldView& field,
                    const math::SurfaceFieldView* surface);

    /// Remembers a site seen at `position` (world space). Cheap and idempotent:
    /// repeats within the merge quantum are dropped, so it is safe to call for
    /// every site entity every frame. Non-site mesh ids are ignored.
    void note_site(uint32_t mesh_asset_id, glm::vec3 position);

    [[nodiscard]] size_t explored_chunks() const { return tiles_.size(); }
    [[nodiscard]] size_t known_sites() const { return markers_.size(); }

    // Drawing ------------------------------------------------------------------
    /// Composes the full screen image at the internal resolution. `eye` is the
    /// camera position (the player marker) and `yaw` its facing (radians, the
    /// frozen camera convention: 0 = -Z north, positive turns east).
    [[nodiscard]] const PixelCanvas& compose(uint32_t width, uint32_t height,
                                             glm::vec3 eye, float yaw);

private:
    struct Tile {
        std::vector<uint8_t> rgb; // MAP_TILE_PX^2 * 3, row 0 = north edge
    };
    struct Marker {
        glm::vec2 position{0.0f};
        MapMarkerKind kind = MapMarkerKind::Dwelling;
    };

    std::unordered_map<uint64_t, Tile> tiles_; // packed chunk coord -> tile
    glm::ivec2 min_chunk_{0, 0};
    glm::ivec2 max_chunk_{0, 0};
    std::vector<Marker> markers_;
    std::unordered_set<uint64_t> marker_keys_;
    PixelCanvas canvas_;
    bool open_ = false;
    bool has_tiles_ = false;
};

} // namespace dfn::render
