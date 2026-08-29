/*
Module: engine/render
File: engine/render/sources/BitmapFont.h

Responsibility:
- The engine's only font: a fixed-cell 1-bit bitmap atlas covering printable
  ASCII plus the Cyrillic alphabet, and the two calls that draw a UTF-8 string
  into a PixelCanvas. Four finished features in three zones (interaction
  prompts, inventory item names, map labels, the start/settings menus) could
  not draw a single pixel of text before this existed.

Key items:
- FONT_CELL_W / FONT_CELL_H — the fixed cell, which IS the advance (the 1 px
  right and bottom gaps are inside the cell, so advance == cell size).
- font_atlas() — the baked atlas: one coverage byte per pixel, FONT_ATLAS_COLS
  x FONT_ATLAS_ROWS cells. Built once, deterministic, GPU-free.
- font_slot_for_codepoint / utf8_next — codepoint -> cell, and a UTF-8 decoder
  that reports malformed input rather than skipping it.
- draw_text / text_width_px — the whole drawing surface. There is no wrapping,
  no kerning, no bidi and no newline handling ON PURPOSE (see Notes).

Dependencies:
- Uses: PixelCanvas.h, C++ stdlib. No GPU, no ECS, no platform headers.
- Used by: MapScreen, the HUD layer in RenderSystem, and later the start menu
  / inventory screen — all through PixelCanvas, i.e. the existing textured
  overlay path. Nothing new reaches IRenderer (Rule 26).

Notes:
- A MISSING GLYPH DRAWS A SOLID BLOCK, never nothing. Nothing else in the font
  is solid, so an unmapped codepoint, malformed UTF-8, or an authoring hole is
  impossible to mistake for a rendering bug or for empty space. This is the
  build_site_mesh lesson applied to text: absence must not present as a neutral
  state. A newline is deliberately NOT handled — it draws the block, because a
  caller passing a multi-line string to a single-line drawer has a bug that
  should be visible in the frame.
- No user-facing string appears here or in any caller inside engine/ (Rule 5):
  every entry point takes UTF-8 the caller resolved from a localization file.
  The glyph chart under DFN_FONT_PROBE is a verification hook, not shipped text.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep this pure: no IRenderer, no world/ECS access, no globals beyond the
  immutable baked atlas.
- If you add glyphs, add the codepoint to font_slot_for_codepoint AND to the
  table; FontTests asserts that every mappable codepoint has real art, so a
  half-done addition fails a test instead of shipping a block.
*/

#pragma once

#include "engine/render/sources/PixelCanvas.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace dfn::render {

// The fixed cell. Ink occupies FONT_INK_W x FONT_INK_H at the cell's top-left;
// the remaining column and row are the inter-glyph and inter-line gaps, so the
// advance IS the cell and no caller has to add spacing of its own.
inline constexpr int FONT_INK_W = 5;
inline constexpr int FONT_INK_H = 8;
inline constexpr int FONT_CELL_W = 6;
inline constexpr int FONT_CELL_H = 9;

// Cell grid of the baked atlas. 16 x 11 = 176 slots holds the 166 mapped
// codepoints with room for a few more without a re-layout.
inline constexpr int FONT_ATLAS_COLS = 16;
inline constexpr int FONT_ATLAS_ROWS = 11;
inline constexpr int FONT_SLOT_COUNT = FONT_ATLAS_COLS * FONT_ATLAS_ROWS;

// Slot of the "I do not have this character" block. It sits where ASCII DEL
// would be (0x20 + 95), which is unprintable anyway.
inline constexpr int FONT_MISSING_SLOT = 95;

// Baseline is the last ink row of a capital; row 7 is the descender row used by
// g j p q y and by д р у ф ц щ.
inline constexpr int FONT_BASELINE_ROW = 6;

/// Replacement codepoint produced by utf8_next for malformed input. It maps to
/// FONT_MISSING_SLOT, so bad encoding is visible in the frame.
inline constexpr uint32_t FONT_REPLACEMENT_CP = 0xFFFDu;

/// The baked fixed-cell atlas. One byte per pixel: 0 (background) or 255 (ink).
/// Row 0 is the top, matching PixelCanvas and texture upload order.
struct FontAtlas {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> mask;

    /// Ink test for pixel (x, y) of `slot`'s cell. Out-of-range reads are 0.
    [[nodiscard]] bool ink(int slot, int x, int y) const;
};

/// The one atlas, baked on first use. Deterministic and immutable.
[[nodiscard]] const FontAtlas& font_atlas();

/// Cell index for a Unicode codepoint; FONT_MISSING_SLOT when unmapped.
/// Mapped ranges: U+0020..U+007E, U+0410..U+044F, U+0401, U+0451, U+00AB,
/// U+00BB, U+2014.
[[nodiscard]] int font_slot_for_codepoint(uint32_t codepoint);

/// Decodes the codepoint starting at `pos` and advances `pos` past it. On a
/// malformed sequence it advances exactly one byte and returns
/// FONT_REPLACEMENT_CP — never silently resynchronizes, never loops forever.
[[nodiscard]] uint32_t utf8_next(std::string_view text, size_t& pos);

/// Number of glyph cells the string occupies (one per codepoint, including
/// unmapped ones — a missing character still takes its space).
[[nodiscard]] int text_glyph_count(std::string_view utf8);

/// Pixel width of the string when drawn: glyph count * FONT_CELL_W * scale,
/// i.e. including the trailing 1 px gap. Use it for right/centre alignment.
[[nodiscard]] int text_width_px(std::string_view utf8, int scale = 1);

/// Draws `utf8` with the first cell's top-left at (x, y). With `shadow` the
/// whole string is first drawn at (x+1, y+1) in `shadow_color`, which is what
/// makes 5 px letters readable over terrain of any value. Returns the advance
/// in pixels (== text_width_px), so a caller can chain runs of two colours.
///
/// `scale` — ЦЕЛЫЙ множитель пикселя, и целый он не по лени: дробный давал бы
/// растяжку с округлением, а у шрифта в 5 px ширины округление съедает штрих
/// целиком. Заведён 27.08, когда холст интерфейса вырос с 640 до 1920 (заказ
/// владельца про FullHD): текст, рисуемый 1:1, стал вчетверо мельче того же
/// текста вчера, и это касается КАЖДОГО потребителя блочного шрифта, а не
/// одного экрана — поэтому множитель живёт здесь, а не у вызывающего.
int draw_text(PixelCanvas& canvas, int x, int y, std::string_view utf8,
              Color color, bool shadow = false, Color shadow_color = Color{0, 0, 0},
              int scale = 1);

/// VERIFICATION ONLY (Rule 27, gated by DFN_FONT_PROBE) — draws every glyph the
/// font claims to have onto a dark plate, plus a row of characters it does NOT
/// have so the block is in the same frame as the letters, plus one unplated
/// line over whatever is behind (the readability case an interaction prompt
/// actually faces). The character rows are GENERATED from the mapped ranges,
/// not typed out, so the chart cannot silently disagree with the font.
/// The canvas must already be cleared; this draws no background of its own
/// outside its plate.
void draw_font_specimen(PixelCanvas& canvas);

} // namespace dfn::render
