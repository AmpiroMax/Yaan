/*
Created: 27:08:2026 - 00:12:40
Module: engine/app
File: engine/app/sources/PngImage.h

Responsibility:
- READING A .png OFF THE DISK INTO RGBA8. The one thing this project could not
  do until today: every texture in the tree is PROCEDURAL (ProcTexture,
  PartsAtlas, the font atlas, the canvases), and the only PNG code anywhere is
  the WRITER inside the bgfx backend (bimg, screenshots). The branding the owner
  approved on 26.08 arrives as .png files in git, so the menu needed a reader.

Key items:
- Image: width/height + RGBA8 pixels, row 0 = top (PixelCanvas order).
- decode_png(): bytes -> Image. Empty result means "no", loudly, on stderr.
- load_png(): the same from a path.
- cached_png(): load-once-per-path, for screens that draw every frame.

Dependencies:
- Uses: C++ stdlib only. No third party: bimg lives behind the render backend
  (Rule 1 keeps third-party includes there), and pulling a decoder into
  engine/app to draw a menu would have been a dependency bought for one screen.
- Used by: engine/app MenuArt (the emblem and the studio mark).

Notes:
- WHAT IS SUPPORTED AND WHY EXACTLY THAT. Bit depth 8, colour types 0/2/3/4/6,
  no interlace -- which is every file in assets/branding/ and the shape any
  export tool produces by default. Anything else is REFUSED OUT LOUD rather
  than approximated: a decoder that guesses at a 16-bit image produces a
  picture that looks like a bad export, and the bug then belongs to whoever
  made the art.
- The decoder is complete for what it claims (dynamic and fixed Huffman blocks,
  stored blocks, all five filter types), and it is checked against the real
  branding files in tests/app/PngImageTests.cpp, whose expected pixels were
  produced by an independent implementation (python zlib), not by this one.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone app (lead) owns this file.
- THIS IS A SHARED CAPABILITY LIVING IN A LEAF. It sits in engine/app because
  that is the zone the menu belongs to and a cross-zone move needs the render
  zone's owner. If a second zone needs to read a PNG, that is the moment to
  move it to engine/core or engine/render -- do NOT copy it (Rule 39).
*/
/*
UPD:
- 27:08:2026 - 00:12:40: Создан — чтение .png (inflate + расфильтровка) ради
  герба Империи Яан и знака студии в меню (заказ владельца 26.08).
*/

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace dfn::app {

/// RGBA8 image, row-major, row 0 = top. Same order PixelCanvas and
/// IRenderer::create_texture want, so nothing downstream has to flip.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;

    [[nodiscard]] bool empty() const { return rgba.empty(); }
    /// Pixel at (x, y). Out of range reads as fully transparent black, so a
    /// sampler never has to bounds-check its own arithmetic twice.
    [[nodiscard]] const uint8_t* at(int x, int y) const;
};

/// Decodes a PNG held in memory. On refusal returns an empty Image and says
/// why on stderr -- a silent empty image would draw as "the emblem is missing"
/// and be indistinguishable from a layout bug.
[[nodiscard]] Image decode_png(std::span<const uint8_t> bytes);

/// Reads the file and decodes it. Missing file and undecodable file are both
/// loud and both return empty.
[[nodiscard]] Image load_png(const std::string& path);

/// Load-once by path. The menu draws every frame and must not re-inflate half a
/// megabyte sixty times a second; a miss is cached too, so a missing file
/// complains once rather than once per frame.
[[nodiscard]] const Image& cached_png(const std::string& path);

} // namespace dfn::app
