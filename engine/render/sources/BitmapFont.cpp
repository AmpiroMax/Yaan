/*
Created: 09:08:2026 - 23:32:07
Last updated: 27:08:2026 - 15:10:00
Module: engine/render
File: engine/render/sources/BitmapFont.cpp

Responsibility:
- The glyph art (printable ASCII + the Cyrillic alphabet), the atlas bake, the
  UTF-8 decoder and the string drawer.

Key items:
- GLYPHS / ALIASES tables, MISSING_ART, bake(), font_atlas(),
  font_slot_for_codepoint, utf8_next, draw_text, draw_font_specimen.

Dependencies:
- Uses: BitmapFont.h, PixelCanvas.h, C++ stdlib.
- Used by: MapScreen, RenderSystem's HUD layer, future menu/inventory screens.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every art row is exactly FONT_INK_W characters and every glyph exactly
  FONT_INK_H rows; the bake trusts that and a test pins it.
- Cyrillic letters whose shape IS the Latin letter (А В Е К М Н О Р С Т Х,
  а е о р с у х) are ALIASES, not copies of the art. Two drawings of the same
  shape drift; one drawing cannot.
*/
/*
UPD:
- 09:08:2026 - 23:32:07: Created.
- 27:08:2026 - 15:10:00: целый множитель пикселя у draw_text/text_width_px; тень
  смещается на множитель. При scale == 1 — прежний вывод бит-в-бит.
*/

#include "engine/render/sources/BitmapFont.h"

#include <algorithm>
#include <string>

namespace dfn::render {

namespace {

struct GlyphArt {
    uint32_t cp;
    const char* rows[FONT_INK_H];
};

// The "I do not have this character" block. Solid on purpose: nothing else in
// the font fills its whole cell, so it can never be read as a letter, and two
// of them in a row merge into a bar that is louder still.
constexpr const char* MISSING_ART[FONT_INK_H] = {
    "#####", "#####", "#####", "#####", "#####", "#####", "#####", "#####"};

// Ink grid: 5 wide, 8 tall. Rows 0..6 are the body (capitals fill all seven),
// row 7 is below the baseline and is used only by descenders.
constexpr GlyphArt GLYPHS[] = {
    // ---- printable ASCII, U+0020..U+007E ---------------------------------
    {0x0020, {".....", ".....", ".....", ".....", ".....", ".....", ".....", "....."}},
    {0x0021, {"..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#..", "....."}},
    {0x0022, {".#.#.", ".#.#.", ".....", ".....", ".....", ".....", ".....", "....."}},
    {0x0023, {".#.#.", ".#.#.", "#####", ".#.#.", "#####", ".#.#.", ".#.#.", "....."}},
    {0x0024, {"..#..", ".####", "#.#..", ".###.", "..#.#", "####.", "..#..", "....."}},
    {0x0025, {"##..#", "##..#", "...#.", "..#..", ".#...", "#..##", "#..##", "....."}},
    {0x0026, {".##..", "#..#.", "#.#..", ".#...", "#.#.#", "#..#.", ".##.#", "....."}},
    {0x0027, {"..#..", "..#..", ".....", ".....", ".....", ".....", ".....", "....."}},
    {0x0028, {"...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#.", "....."}},
    {0x0029, {".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#...", "....."}},
    {0x002A, {".....", "#.#.#", ".###.", "#####", ".###.", "#.#.#", ".....", "....."}},
    {0x002B, {".....", "..#..", "..#..", "#####", "..#..", "..#..", ".....", "....."}},
    {0x002C, {".....", ".....", ".....", ".....", ".....", "..##.", "..#..", ".#..."}},
    {0x002D, {".....", ".....", ".....", ".###.", ".....", ".....", ".....", "....."}},
    {0x002E, {".....", ".....", ".....", ".....", ".....", ".##..", ".##..", "....."}},
    {0x002F, {"....#", "...#.", "...#.", "..#..", ".#...", ".#...", "#....", "....."}},
    {0x0030, {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###.", "....."}},
    {0x0031, {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###.", "....."}},
    {0x0032, {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####", "....."}},
    {0x0033, {"#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###.", "....."}},
    {0x0034, {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#.", "....."}},
    {0x0035, {"#####", "#....", "####.", "....#", "....#", "#...#", ".###.", "....."}},
    {0x0036, {"..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###.", "....."}},
    {0x0037, {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#...", "....."}},
    {0x0038, {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###.", "....."}},
    {0x0039, {".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##..", "....."}},
    {0x003A, {".....", ".##..", ".##..", ".....", ".##..", ".##..", ".....", "....."}},
    {0x003B, {".....", ".##..", ".##..", ".....", ".##..", "..#..", ".#...", "....."}},
    {0x003C, {"...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#.", "....."}},
    {0x003D, {".....", ".....", "#####", ".....", "#####", ".....", ".....", "....."}},
    {0x003E, {".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#...", "....."}},
    {0x003F, {".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#..", "....."}},
    {0x0040, {".###.", "#...#", "#.###", "#.#.#", "#.###", "#....", ".###.", "....."}},
    {0x0041, {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#", "....."}},
    {0x0042, {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####.", "....."}},
    {0x0043, {".###.", "#...#", "#....", "#....", "#....", "#...#", ".###.", "....."}},
    {0x0044, {"###..", "#..#.", "#...#", "#...#", "#...#", "#..#.", "###..", "....."}},
    {0x0045, {"#####", "#....", "#....", "####.", "#....", "#....", "#####", "....."}},
    {0x0046, {"#####", "#....", "#....", "####.", "#....", "#....", "#....", "....."}},
    {0x0047, {".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####", "....."}},
    {0x0048, {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#", "....."}},
    {0x0049, {".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###.", "....."}},
    {0x004A, {"..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##..", "....."}},
    {0x004B, {"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#", "....."}},
    {0x004C, {"#....", "#....", "#....", "#....", "#....", "#....", "#####", "....."}},
    {0x004D, {"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#", "....."}},
    {0x004E, {"#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "....."}},
    {0x004F, {".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###.", "....."}},
    {0x0050, {"####.", "#...#", "#...#", "####.", "#....", "#....", "#....", "....."}},
    {0x0051, {".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#", "....."}},
    {0x0052, {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#", "....."}},
    {0x0053, {".####", "#....", "#....", ".###.", "....#", "....#", "####.", "....."}},
    {0x0054, {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#..", "....."}},
    {0x0055, {"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###.", "....."}},
    {0x0056, {"#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#..", "....."}},
    {0x0057, {"#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#", "....."}},
    {0x0058, {"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#", "....."}},
    {0x0059, {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#..", "....."}},
    {0x005A, {"#####", "....#", "...#.", "..#..", ".#...", "#....", "#####", "....."}},
    {0x005B, {".###.", ".#...", ".#...", ".#...", ".#...", ".#...", ".###.", "....."}},
    {0x005C, {"#....", ".#...", ".#...", "..#..", "...#.", "...#.", "....#", "....."}},
    {0x005D, {".###.", "...#.", "...#.", "...#.", "...#.", "...#.", ".###.", "....."}},
    {0x005E, {"..#..", ".#.#.", "#...#", ".....", ".....", ".....", ".....", "....."}},
    {0x005F, {".....", ".....", ".....", ".....", ".....", ".....", ".....", "#####"}},
    {0x0060, {".#...", "..#..", ".....", ".....", ".....", ".....", ".....", "....."}},
    {0x0061, {".....", ".....", ".###.", "....#", ".####", "#...#", ".####", "....."}},
    {0x0062, {"#....", "#....", "####.", "#...#", "#...#", "#...#", "####.", "....."}},
    {0x0063, {".....", ".....", ".###.", "#....", "#....", "#...#", ".###.", "....."}},
    {0x0064, {"....#", "....#", ".####", "#...#", "#...#", "#...#", ".####", "....."}},
    {0x0065, {".....", ".....", ".###.", "#...#", "#####", "#....", ".###.", "....."}},
    {0x0066, {"..##.", ".#..#", ".#...", "####.", ".#...", ".#...", ".#...", "....."}},
    {0x0067, {".....", ".....", ".####", "#...#", "#...#", ".####", "....#", ".###."}},
    {0x0068, {"#....", "#....", "####.", "#...#", "#...#", "#...#", "#...#", "....."}},
    {0x0069, {"..#..", ".....", ".##..", "..#..", "..#..", "..#..", ".###.", "....."}},
    {0x006A, {"...#.", ".....", "..##.", "...#.", "...#.", "...#.", "#..#.", ".##.."}},
    {0x006B, {"#....", "#....", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "....."}},
    {0x006C, {".##..", "..#..", "..#..", "..#..", "..#..", "..#..", ".###.", "....."}},
    {0x006D, {".....", ".....", "##.#.", "#.#.#", "#.#.#", "#.#.#", "#.#.#", "....."}},
    {0x006E, {".....", ".....", "####.", "#...#", "#...#", "#...#", "#...#", "....."}},
    {0x006F, {".....", ".....", ".###.", "#...#", "#...#", "#...#", ".###.", "....."}},
    {0x0070, {".....", ".....", "####.", "#...#", "#...#", "####.", "#....", "#...."}},
    {0x0071, {".....", ".....", ".####", "#...#", "#...#", ".####", "....#", "....#"}},
    {0x0072, {".....", ".....", "#.##.", "##..#", "#....", "#....", "#....", "....."}},
    {0x0073, {".....", ".....", ".####", "#....", ".###.", "....#", "####.", "....."}},
    {0x0074, {".#...", ".#...", "####.", ".#...", ".#...", ".#..#", "..##.", "....."}},
    {0x0075, {".....", ".....", "#...#", "#...#", "#...#", "#..##", ".##.#", "....."}},
    {0x0076, {".....", ".....", "#...#", "#...#", "#...#", ".#.#.", "..#..", "....."}},
    {0x0077, {".....", ".....", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#.", "....."}},
    {0x0078, {".....", ".....", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "....."}},
    {0x0079, {".....", ".....", "#...#", "#...#", "#...#", ".####", "....#", ".###."}},
    {0x007A, {".....", ".....", "#####", "...#.", "..#..", ".#...", "#####", "....."}},
    {0x007B, {"..##.", "..#..", "..#..", ".#...", "..#..", "..#..", "..##.", "....."}},
    {0x007C, {"..#..", "..#..", "..#..", "..#..", "..#..", "..#..", "..#..", "....."}},
    {0x007D, {".##..", "..#..", "..#..", "...#.", "..#..", "..#..", ".##..", "....."}},
    {0x007E, {".....", ".....", ".#...", "#.#.#", "...#.", ".....", ".....", "....."}},
    // ---- Cyrillic capitals that are NOT Latin lookalikes -------------------
    {0x0411, {"#####", "#....", "#....", "####.", "#...#", "#...#", "####.", "....."}}, // Б
    {0x0413, {"#####", "#....", "#....", "#....", "#....", "#....", "#....", "....."}}, // Г
    {0x0414, {".####", "..#.#", "..#.#", "..#.#", ".#..#", "#####", "#...#", "....."}}, // Д
    {0x0416, {"#.#.#", "#.#.#", ".###.", "..#..", ".###.", "#.#.#", "#.#.#", "....."}}, // Ж
    {0x0417, {".###.", "#...#", "....#", "..##.", "....#", "#...#", ".###.", "....."}}, // З
    {0x0418, {"#...#", "#...#", "#..##", "#.#.#", "##..#", "#...#", "#...#", "....."}}, // И
    {0x0419, {".###.", ".....", "#...#", "#..##", "#.#.#", "##..#", "#...#", "....."}}, // Й
    {0x041B, {"..###", "..#.#", "..#.#", ".#..#", ".#..#", "#...#", "#...#", "....."}}, // Л
    {0x041F, {"#####", "#...#", "#...#", "#...#", "#...#", "#...#", "#...#", "....."}}, // П
    {0x0423, {"#...#", "#...#", ".#.#.", "..#..", "..#..", ".#...", "#....", "....."}}, // У
    {0x0424, {"..#..", ".###.", "#.#.#", "#.#.#", "#.#.#", ".###.", "..#..", "....."}}, // Ф
    {0x0426, {"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", "#####", "....#"}}, // Ц
    {0x0427, {"#...#", "#...#", "#...#", ".####", "....#", "....#", "....#", "....."}}, // Ч
    {0x0428, {"#.#.#", "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#####", "....."}}, // Ш
    {0x0429, {"#.#.#", "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#####", "....#"}}, // Щ
    {0x042A, {"##...", ".#...", ".#...", ".###.", ".#..#", ".#..#", ".###.", "....."}}, // Ъ
    {0x042B, {"#...#", "#...#", "#...#", "###.#", "#.#.#", "#.#.#", "###.#", "....."}}, // Ы
    {0x042C, {"#....", "#....", "#....", "###..", "#..#.", "#..#.", "###..", "....."}}, // Ь
    {0x042D, {".###.", "#...#", "....#", "..###", "....#", "#...#", ".###.", "....."}}, // Э
    {0x042E, {"#..#.", "#.#.#", "#.#.#", "###.#", "#.#.#", "#.#.#", "#..#.", "....."}}, // Ю
    {0x042F, {".####", "#...#", "#...#", ".####", "..#.#", ".#..#", "#...#", "....."}}, // Я
    {0x0401, {".#.#.", ".....", "#####", "#....", "####.", "#....", "#####", "....."}}, // Ё
    // ---- Cyrillic lowercase that is NOT a Latin lookalike ------------------
    {0x0431, {"..###", ".#...", "#....", "####.", "#...#", "#...#", ".###.", "....."}}, // б
    {0x0432, {".....", ".....", "####.", "#...#", "####.", "#...#", "####.", "....."}}, // в
    {0x0433, {".....", ".....", "####.", "#....", "#....", "#....", "#....", "....."}}, // г
    {0x0434, {".....", ".....", "..###", "..#.#", ".#..#", ".#..#", "#####", "#...#"}}, // д
    {0x0436, {".....", ".....", "#.#.#", ".###.", "..#..", ".###.", "#.#.#", "....."}}, // ж
    {0x0437, {".....", ".....", ".###.", "....#", "..##.", "....#", ".###.", "....."}}, // з
    {0x0438, {".....", ".....", "#...#", "#..##", "#.#.#", "##..#", "#...#", "....."}}, // и
    {0x0439, {".....", ".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", "....."}}, // й
    {0x043A, {".....", ".....", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "....."}}, // к
    {0x043B, {".....", ".....", "..###", "..#.#", ".#..#", ".#..#", "#...#", "....."}}, // л
    {0x043C, {".....", ".....", "#...#", "##.##", "#.#.#", "#...#", "#...#", "....."}}, // м
    {0x043D, {".....", ".....", "#...#", "#...#", "#####", "#...#", "#...#", "....."}}, // н
    {0x043F, {".....", ".....", "#####", "#...#", "#...#", "#...#", "#...#", "....."}}, // п
    {0x0442, {".....", ".....", "#####", "..#..", "..#..", "..#..", "..#..", "....."}}, // т
    {0x0444, {".....", "..#..", ".###.", "#.#.#", "#.#.#", ".###.", "..#..", "..#.."}}, // ф
    {0x0446, {".....", ".....", "#...#", "#...#", "#...#", "#...#", "#####", "....#"}}, // ц
    {0x0447, {".....", ".....", "#...#", "#...#", ".####", "....#", "....#", "....."}}, // ч
    {0x0448, {".....", ".....", "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#####", "....."}}, // ш
    {0x0449, {".....", ".....", "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#####", "....#"}}, // щ
    {0x044A, {".....", ".....", "##...", ".#...", ".###.", ".#..#", ".###.", "....."}}, // ъ
    {0x044B, {".....", ".....", "#...#", "#...#", "###.#", "#.#.#", "###.#", "....."}}, // ы
    {0x044C, {".....", ".....", "#....", "#....", "###..", "#..#.", "###..", "....."}}, // ь
    {0x044D, {".....", ".....", ".###.", "....#", "..###", "....#", ".###.", "....."}}, // э
    {0x044E, {".....", ".....", "#..#.", "#.#.#", "###.#", "#.#.#", "#..#.", "....."}}, // ю
    {0x044F, {".....", ".....", ".####", "#...#", ".####", "..#.#", ".#..#", "....."}}, // я
    {0x0451, {".#.#.", ".....", ".###.", "#...#", "#####", "#....", ".###.", "....."}}, // ё
    // ---- punctuation Russian copy actually uses ---------------------------
    {0x00AB, {".....", "..#.#", ".#.#.", "#.#..", ".#.#.", "..#.#", ".....", "....."}}, // «
    {0x00BB, {".....", "#.#..", ".#.#.", "..#.#", ".#.#.", "#.#..", ".....", "....."}}, // »
    {0x2014, {".....", ".....", ".....", "#####", ".....", ".....", ".....", "....."}}, // —
};

// Cyrillic letters whose glyph IS the Latin one. Aliased rather than redrawn:
// two drawings of the same shape drift, one drawing cannot.
struct GlyphAlias {
    uint32_t cp;
    uint32_t same_as;
};
constexpr GlyphAlias ALIASES[] = {
    {0x0410, 0x0041}, {0x0412, 0x0042}, {0x0415, 0x0045}, {0x041A, 0x004B},
    {0x041C, 0x004D}, {0x041D, 0x0048}, {0x041E, 0x004F}, {0x0420, 0x0050},
    {0x0421, 0x0043}, {0x0422, 0x0054}, {0x0425, 0x0058}, {0x0430, 0x0061},
    {0x0435, 0x0065}, {0x043E, 0x006F}, {0x0440, 0x0070}, {0x0441, 0x0063},
    {0x0443, 0x0079}, {0x0445, 0x0078},
};

void paint_cell(FontAtlas& atlas, int slot, const char* const rows[FONT_INK_H]) {
    const int cx = (slot % FONT_ATLAS_COLS) * FONT_CELL_W;
    const int cy = (slot / FONT_ATLAS_COLS) * FONT_CELL_H;
    for (int gy = 0; gy < FONT_CELL_H; ++gy) {
        for (int gx = 0; gx < FONT_CELL_W; ++gx) {
            const bool ink = gx < FONT_INK_W && gy < FONT_INK_H
                             && rows[gy][gx] != '.' && rows[gy][gx] != ' ';
            atlas.mask[static_cast<size_t>(cy + gy) * atlas.width + (cx + gx)] =
                ink ? 255 : 0;
        }
    }
}

FontAtlas bake() {
    FontAtlas atlas;
    atlas.width = FONT_ATLAS_COLS * FONT_CELL_W;
    atlas.height = FONT_ATLAS_ROWS * FONT_CELL_H;
    atlas.mask.assign(static_cast<size_t>(atlas.width) * atlas.height, 0);
    // Start from ALL BLOCKS, then overwrite with real art. A slot the mapping
    // can reach but nobody drew therefore ships as a visible block instead of
    // as blank space — an authoring hole must not look like a space.
    for (int slot = 0; slot < FONT_SLOT_COUNT; ++slot) {
        paint_cell(atlas, slot, MISSING_ART);
    }
    for (const GlyphArt& g : GLYPHS) {
        paint_cell(atlas, font_slot_for_codepoint(g.cp), g.rows);
    }
    // Aliases copy the baked cell, so they cannot disagree with their source.
    for (const GlyphAlias& a : ALIASES) {
        const int dst = font_slot_for_codepoint(a.cp);
        const int src = font_slot_for_codepoint(a.same_as);
        const int dx = (dst % FONT_ATLAS_COLS) * FONT_CELL_W;
        const int dy = (dst / FONT_ATLAS_COLS) * FONT_CELL_H;
        const int sx = (src % FONT_ATLAS_COLS) * FONT_CELL_W;
        const int sy = (src / FONT_ATLAS_COLS) * FONT_CELL_H;
        for (int gy = 0; gy < FONT_CELL_H; ++gy) {
            for (int gx = 0; gx < FONT_CELL_W; ++gx) {
                atlas.mask[static_cast<size_t>(dy + gy) * atlas.width + (dx + gx)] =
                    atlas.mask[static_cast<size_t>(sy + gy) * atlas.width + (sx + gx)];
            }
        }
    }
    return atlas;
}

} // namespace

bool FontAtlas::ink(int slot, int x, int y) const {
    if (slot < 0 || slot >= FONT_SLOT_COUNT || x < 0 || y < 0
        || x >= FONT_CELL_W || y >= FONT_CELL_H) {
        return false;
    }
    const int px = (slot % FONT_ATLAS_COLS) * FONT_CELL_W + x;
    const int py = (slot / FONT_ATLAS_COLS) * FONT_CELL_H + y;
    return mask[static_cast<size_t>(py) * width + px] != 0;
}

const FontAtlas& font_atlas() {
    static const FontAtlas atlas = bake();
    return atlas;
}

int font_slot_for_codepoint(uint32_t cp) {
    if (cp >= 0x0020u && cp <= 0x007Eu) {
        return static_cast<int>(cp - 0x0020u); // 0..94
    }
    if (cp >= 0x0410u && cp <= 0x044Fu) {
        return 96 + static_cast<int>(cp - 0x0410u); // А..я -> 96..159
    }
    switch (cp) {
    case 0x0401u: return 160; // Ё
    case 0x0451u: return 161; // ё
    case 0x00ABu: return 162; // «
    case 0x00BBu: return 163; // »
    case 0x2014u: return 164; // —
    default: return FONT_MISSING_SLOT;
    }
}

uint32_t utf8_next(std::string_view text, size_t& pos) {
    if (pos >= text.size()) {
        return 0;
    }
    const auto byte = [&](size_t i) { return static_cast<unsigned char>(text[i]); };
    const unsigned char b0 = byte(pos);
    int extra = 0;
    uint32_t cp = 0;
    if (b0 < 0x80u) {
        ++pos;
        return b0;
    }
    if ((b0 & 0xE0u) == 0xC0u) {
        extra = 1;
        cp = b0 & 0x1Fu;
    } else if ((b0 & 0xF0u) == 0xE0u) {
        extra = 2;
        cp = b0 & 0x0Fu;
    } else if ((b0 & 0xF8u) == 0xF0u) {
        extra = 3;
        cp = b0 & 0x07u;
    } else {
        ++pos; // stray continuation or 0xFE/0xFF: consume one byte, report it
        return FONT_REPLACEMENT_CP;
    }
    if (pos + static_cast<size_t>(extra) >= text.size()) {
        ++pos;
        return FONT_REPLACEMENT_CP;
    }
    for (int i = 1; i <= extra; ++i) {
        const unsigned char bi = byte(pos + static_cast<size_t>(i));
        if ((bi & 0xC0u) != 0x80u) {
            ++pos; // truncated sequence: one byte, one visible block
            return FONT_REPLACEMENT_CP;
        }
        cp = (cp << 6) | (bi & 0x3Fu);
    }
    pos += static_cast<size_t>(extra) + 1;
    return cp;
}

int text_glyph_count(std::string_view utf8) {
    int count = 0;
    size_t pos = 0;
    while (pos < utf8.size()) {
        (void)utf8_next(utf8, pos);
        ++count;
    }
    return count;
}

int text_width_px(std::string_view utf8, int scale) {
    return text_glyph_count(utf8) * FONT_CELL_W * (scale > 0 ? scale : 1);
}

int draw_text(PixelCanvas& canvas, int x, int y, std::string_view utf8, Color color,
              bool shadow, Color shadow_color, int scale) {
    const FontAtlas& atlas = font_atlas();
    const int s = scale > 0 ? scale : 1;
    // A 1 px offset copy underneath, not an 8-way halo: at 5 px tall a full
    // dilation closes the counters of a, e, о and the string turns to mush.
    // ТЕНЬ СМЕЩАЕТСЯ НА МНОЖИТЕЛЬ, а не на пиксель: на увеличенном тексте
    // однопиксельная тень не читается как тень — она читается как грязь на
    // краю штриха.
    const int passes = shadow ? 2 : 1;
    for (int pass = 0; pass < passes; ++pass) {
        const bool is_shadow = shadow && pass == 0;
        const Color ink = is_shadow ? shadow_color : color;
        const int ox = is_shadow ? s : 0;
        int pen = x;
        size_t pos = 0;
        while (pos < utf8.size()) {
            const uint32_t cp = utf8_next(utf8, pos);
            const int slot = font_slot_for_codepoint(cp);
            for (int gy = 0; gy < FONT_CELL_H; ++gy) {
                for (int gx = 0; gx < FONT_CELL_W; ++gx) {
                    if (!atlas.ink(slot, gx, gy)) {
                        continue;
                    }
                    // Пиксель шрифта — квадрат s×s. При s == 1 это ровно
                    // прежний put, бит-в-бит: увеличение не имеет права
                    // изменить ни одного кадра, который его не просил.
                    for (int ry = 0; ry < s; ++ry) {
                        for (int rx = 0; rx < s; ++rx) {
                            canvas.put(pen + gx * s + rx + ox,
                                       y + gy * s + ry + ox, ink);
                        }
                    }
                }
            }
            pen += FONT_CELL_W * s;
        }
    }
    return text_width_px(utf8, s);
}

namespace {

// Minimal UTF-8 encoder, used only to BUILD the specimen rows out of the
// mapped ranges. Generating the chart is the point: a hand-typed chart drifts
// from the font the moment a glyph is added.
void append_utf8(std::string& out, uint32_t cp) {
    if (cp < 0x80u) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800u) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

std::string range_row(uint32_t first, uint32_t last) {
    std::string s;
    for (uint32_t cp = first; cp <= last; ++cp) {
        append_utf8(s, cp);
    }
    return s;
}

} // namespace

void draw_font_specimen(PixelCanvas& canvas) {
    const Color plate{18, 20, 24};
    const Color rule{70, 78, 88};
    const Color ink{232, 228, 214};
    const Color dim{150, 154, 148};

    std::vector<std::string> rows;
    rows.push_back(range_row(0x0020u, 0x003Fu)); // ASCII, 32 per row
    rows.push_back(range_row(0x0040u, 0x005Fu));
    rows.push_back(range_row(0x0060u, 0x007Eu));
    std::string caps;
    append_utf8(caps, 0x0401u); // Ё leads the capitals
    caps += range_row(0x0410u, 0x042Fu);
    rows.push_back(caps);
    std::string lower;
    append_utf8(lower, 0x0451u); // ё leads the lowercase
    lower += range_row(0x0430u, 0x044Fu);
    rows.push_back(lower);
    std::string extras;
    for (uint32_t cp : {0x00ABu, 0x00BBu, 0x2014u}) {
        append_utf8(extras, cp);
    }
    // ... followed by characters the font does NOT have (Greek, CJK, an
    // accented Latin letter) and one deliberately malformed byte. Every one of
    // them must appear as the solid block, in the same frame as the letters.
    for (uint32_t cp : {0x03B1u, 0x4E2Du, 0x00E9u}) {
        append_utf8(extras, cp);
    }
    extras.push_back(static_cast<char>(0x80)); // stray continuation byte
    rows.push_back(extras);

    int widest = 0;
    for (const std::string& r : rows) {
        widest = std::max(widest, text_width_px(r));
    }
    const int pad = 4;
    const int x0 = 4;
    const int y0 = 4;
    const int plate_w = widest + pad * 2;
    const int plate_h = static_cast<int>(rows.size()) * FONT_CELL_H + pad * 2;
    canvas.fill_rect(x0, y0, plate_w, plate_h, plate);
    canvas.frame_rect(x0, y0, plate_w, plate_h, rule);
    for (size_t i = 0; i < rows.size(); ++i) {
        // The last row (the unsupported characters) is drawn dim so the block
        // is obviously the FONT saying "no", not the chart's own emphasis.
        const bool last = i + 1 == rows.size();
        draw_text(canvas, x0 + pad, y0 + pad + static_cast<int>(i) * FONT_CELL_H,
                  rows[i], last ? dim : ink);
    }

    // The unplated case: text straight over whatever the world drew, with the
    // 1 px shadow that is the only thing making 5 px letters survive terrain.
    // These two lines are DEBUG strings under a DFN_ hook, not shipped UI text
    // (Rule 5) — the real prompt string comes from a localization file.
    const int cw = static_cast<int>(canvas.width());
    const int ch = static_cast<int>(canvas.height());
    const std::string prompt = "[E] Открыть сундук";
    const std::string second = "Rusty iron sword — Ржавый железный меч";
    draw_text(canvas, (cw - text_width_px(prompt)) / 2, ch - FONT_CELL_H * 4,
              prompt, ink, true, Color{0, 0, 0});
    draw_text(canvas, (cw - text_width_px(second)) / 2, ch - FONT_CELL_H * 2,
              second, ink, true, Color{0, 0, 0});
}

} // namespace dfn::render
