/*
Created: 09:08:2026 - 17:15:10
Last updated: 09:08:2026 - 23:32:07
Module: engine/render
File: engine/render/sources/PixelCanvas.h

Responsibility:
- The first UI primitive of the project: a tiny CPU raster canvas in INTERNAL
  pixel space (Q9 low-res target). Screens draw into it with rectangles,
  1-bit stamps and triangles; the caller uploads the result as one RGBA8
  texture and blits it over the frame.

Key items:
- Color: 8-bit RGB, always uploaded opaque.
- Stamp: 1-bit silhouette mask defined as text rows ("art rule": at 640x360
  meaning is carried by silhouette + value, not detail).
- PixelCanvas: resize/clear/put/fill_rect/frame_rect/stamp/fill_triangle +
  pixels() (RGBA8, row-major, row 0 = top).

Dependencies:
- Uses: C++ stdlib, glm (Rule 2). No GPU, no ECS, no platform headers.
- Used by: MapScreen (the map screen) and, later, the start menu / HUD — this
  is deliberately screen-agnostic.

Notes:
- Everything is clipped: out-of-bounds coordinates are dropped, never asserted.
  Screens are laid out from the internal resolution at runtime and must not
  crash when it changes (320x180 is a shipping preset).
- Text lives in BitmapFont.h (draw_text / text_width_px), NOT here: this stays
  a raster surface. Shapes and value still carry meaning on their own — at
  640x360 a 5 px letter is a last resort, not the first one.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep this pure: no IRenderer, no world/ECS access, no globals.
*/
/*
UPD:
- 09:08:2026 - 17:15:10: Created for the map screen (user request "миникарта
  как в скайриме"): the project's first UI drawing surface, kept general so a
  menu screen can reuse it.
- 09:08:2026 - 23:32:07: clear_transparent() — the HUD layer (interaction
  prompts) composites over the world, unlike the opaque map screen. Text
  itself arrives as BitmapFont.h free functions rather than as a member, so
  this stays a raster surface and does not grow a text engine.
*/

#pragma once

#include <cstdint>
#include <glm/vec2.hpp>
#include <span>
#include <vector>

namespace dfn::render {

/// Opaque 8-bit RGB color. Alpha is implicit (255) — screens composite on the
/// CPU, so the GPU never needs a blend for them.
struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

/// Multiplies a color by a scalar factor (value shading), clamped to 0..255.
[[nodiscard]] Color shade(Color base, float factor);

/// A 1-bit silhouette: `rows[y][x] != ' '` marks a set pixel. Rows are plain
/// C strings so shapes are readable in source (see MapScreen's marker table).
struct Stamp {
    int width = 0;
    int height = 0;
    const char* const* rows = nullptr;
};

/// CPU raster surface in internal-resolution pixels. Origin is TOP-LEFT, +y
/// down (texture order), so the buffer uploads to IRenderer::create_texture
/// without a flip.
class PixelCanvas {
public:
    /// Allocates (or reuses) the pixel buffer. Contents are undefined after a
    /// size change; call clear() first.
    void resize(uint32_t width, uint32_t height);

    [[nodiscard]] uint32_t width() const { return width_; }
    [[nodiscard]] uint32_t height() const { return height_; }

    void clear(Color color);
    /// Clears to fully TRANSPARENT (alpha 0). Every primitive writes alpha
    /// 255, so a canvas cleared this way composites as "only what I drew" —
    /// which is what a HUD is, as opposed to a full-screen screen like the map.
    void clear_transparent();
    void put(int x, int y, Color color);
    void fill_rect(int x, int y, int w, int h, Color color);
    /// 1-pixel outline on the rect boundary (inclusive of x..x+w-1).
    void frame_rect(int x, int y, int w, int h, Color color);
    /// Horizontal / vertical 1-pixel runs (frames, ticks, rules).
    void hline(int x, int y, int length, Color color);
    void vline(int x, int y, int length, Color color);

    /// Draws `stamp` with its top-left at (x, y) in `color`. When
    /// `outline_first` is true the stamp is first drawn 8-way dilated in
    /// `outline`, so the silhouette reads against any background value.
    void draw_stamp(int x, int y, const Stamp& stamp, Color color,
                    bool outline_first = false, Color outline = Color{0, 0, 0});

    /// Filled triangle (used for the player arrow and the north tick).
    void fill_triangle(glm::vec2 a, glm::vec2 b, glm::vec2 c, Color color);

    /// RGBA8 pixels, row-major, row 0 = top. Size = width * height * 4.
    [[nodiscard]] std::span<const uint8_t> pixels() const { return pixels_; }

private:
    std::vector<uint8_t> pixels_;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};

} // namespace dfn::render
