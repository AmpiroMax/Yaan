/*
Module: tests
File: tests/render/BitmapFontTests.cpp

Responsibility:
- Pins the properties of the bitmap font that a caller depends on and that a
  frame cannot show: that every mappable codepoint has real art, that an
  UNmappable one is loud, that malformed UTF-8 is loud, that aliased Cyrillic
  is byte-identical to its Latin source, and that no two different characters
  share a bitmap.

Key items:
- doctest cases over BitmapFont.h. Each carries its control (Rule 30): the
  "every glyph has art" case is paired with the codepoints that MUST come back
  as the block, and the alias case with a pair that must NOT match.

Dependencies:
- Uses: doctest, engine/render BitmapFont + PixelCanvas (pure, GPU-free).
- Used by: ctest (render_bitmap_font).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/

#include "engine/render/sources/BitmapFont.h"
#include "engine/render/sources/PixelCanvas.h"

#include <doctest/doctest.h>

#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace dfn::render;

namespace {

// The 6x9 cell as a comparable blob.
std::vector<uint8_t> cell_of(uint32_t cp) {
    const FontAtlas& atlas = font_atlas();
    const int slot = font_slot_for_codepoint(cp);
    std::vector<uint8_t> out;
    out.reserve(static_cast<size_t>(FONT_CELL_W) * FONT_CELL_H);
    for (int y = 0; y < FONT_CELL_H; ++y) {
        for (int x = 0; x < FONT_CELL_W; ++x) {
            out.push_back(atlas.ink(slot, x, y) ? 1u : 0u);
        }
    }
    return out;
}

bool is_missing_block(uint32_t cp) {
    const FontAtlas& atlas = font_atlas();
    const int slot = font_slot_for_codepoint(cp);
    // The block fills the whole INK area (the cell's gap column/row stay clear).
    for (int y = 0; y < FONT_INK_H; ++y) {
        for (int x = 0; x < FONT_INK_W; ++x) {
            if (!atlas.ink(slot, x, y)) {
                return false;
            }
        }
    }
    return true;
}

// Every codepoint the mapping accepts, in one list.
std::vector<uint32_t> mapped_codepoints() {
    std::vector<uint32_t> cps;
    for (uint32_t cp = 0x0020u; cp <= 0x007Eu; ++cp) {
        cps.push_back(cp);
    }
    for (uint32_t cp = 0x0410u; cp <= 0x044Fu; ++cp) {
        cps.push_back(cp);
    }
    for (uint32_t cp : {0x0401u, 0x0451u, 0x00ABu, 0x00BBu, 0x2014u}) {
        cps.push_back(cp);
    }
    return cps;
}

} // namespace

TEST_CASE("atlas geometry matches the declared cell grid") {
    const FontAtlas& atlas = font_atlas();
    CHECK(atlas.width == FONT_ATLAS_COLS * FONT_CELL_W);
    CHECK(atlas.height == FONT_ATLAS_ROWS * FONT_CELL_H);
    CHECK(atlas.mask.size()
          == static_cast<size_t>(atlas.width) * static_cast<size_t>(atlas.height));
    // Advance IS the cell: the gap column and row belong to the glyph, so a
    // caller never adds spacing and two runs never overlap.
    CHECK(FONT_CELL_W == FONT_INK_W + 1);
    CHECK(FONT_CELL_H == FONT_INK_H + 1);
    // Every slot the mapping can produce exists in the grid.
    CHECK(FONT_SLOT_COUNT >= 165);
}

TEST_CASE("every mappable codepoint has real art, and only the unmappable is a block") {
    // THE CASE. An authoring hole would bake as the block (see bake()), so this
    // is what turns "I forgot щ" into a red test instead of a shipped square.
    for (uint32_t cp : mapped_codepoints()) {
        CAPTURE(cp);
        CHECK_FALSE(is_missing_block(cp));
    }

    // THE CONTROL (Rule 30). These must come back AS the block; if the check
    // above passed because is_missing_block always returns false, this fails.
    for (uint32_t cp : {0x0000u, 0x000Au, 0x0009u, 0x007Fu, 0x4E2Du, 0x00E9u,
                        0x0500u, 0x1F600u, FONT_REPLACEMENT_CP}) {
        CAPTURE(cp);
        CHECK(font_slot_for_codepoint(cp) == FONT_MISSING_SLOT);
        CHECK(is_missing_block(cp));
    }
}

TEST_CASE("a missing glyph is the loudest mark in the font") {
    // Nothing legitimate fills its whole ink area, so the block cannot be read
    // as a letter — the point of the whole convention.
    int solid = 0;
    for (uint32_t cp : mapped_codepoints()) {
        if (is_missing_block(cp)) {
            ++solid;
        }
    }
    CHECK(solid == 0);
    // And it really is solid, not merely "different".
    CHECK(is_missing_block(0x4E2Du));
}

TEST_CASE("no two different characters share a bitmap") {
    // Two characters that draw the same are a readability bug the frame will
    // not show you: й and и differed only by which ROW the breve sat on when
    // this test was first run, and the frame looked fine.
    std::map<std::vector<uint8_t>, uint32_t> seen;
    // Declared aliases: the Cyrillic letters whose shape IS the Latin letter.
    const std::map<uint32_t, uint32_t> aliases{
        {0x0410, 0x0041}, {0x0412, 0x0042}, {0x0415, 0x0045}, {0x041A, 0x004B},
        {0x041C, 0x004D}, {0x041D, 0x0048}, {0x041E, 0x004F}, {0x0420, 0x0050},
        {0x0421, 0x0043}, {0x0422, 0x0054}, {0x0425, 0x0058}, {0x0430, 0x0061},
        {0x0435, 0x0065}, {0x043E, 0x006F}, {0x0440, 0x0070}, {0x0441, 0x0063},
        {0x0443, 0x0079}, {0x0445, 0x0078},
    };
    for (uint32_t cp : mapped_codepoints()) {
        if (aliases.count(cp) != 0) {
            continue; // checked below, on purpose
        }
        const auto cell = cell_of(cp);
        const auto it = seen.find(cell);
        CAPTURE(cp);
        if (it != seen.end()) {
            CAPTURE(it->second);
            FAIL_CHECK("two characters bake to the same bitmap");
        }
        seen.emplace(cell, cp);
    }

    // Aliases must be byte-identical to their source — that is the point of
    // aliasing rather than redrawing.
    for (const auto& [cp, src] : aliases) {
        CAPTURE(cp);
        CHECK(cell_of(cp) == cell_of(src));
    }
    // CONTROL: a Cyrillic letter that is NOT an alias must not match a Latin
    // one, or the comparison above would pass for a trivial reason.
    CHECK(cell_of(0x0411u) != cell_of(0x0042u)); // Б is not B
    CHECK(cell_of(0x0424u) != cell_of(0x004Fu)); // Ф is not O
}

TEST_CASE("utf8 decoding: correct sequences, and malformed input is one loud block") {
    // 1-, 2- and 3-byte sequences.
    const std::string text = "A\xD0\x91\xE2\x80\x94"; // 'A', 'Б', '—'
    size_t pos = 0;
    CHECK(utf8_next(text, pos) == 0x0041u);
    CHECK(utf8_next(text, pos) == 0x0411u);
    CHECK(utf8_next(text, pos) == 0x2014u);
    CHECK(pos == text.size());
    CHECK(text_glyph_count(text) == 3);
    CHECK(text_width_px(text) == 3 * FONT_CELL_W);

    // Malformed: a lone continuation byte, and a truncated 2-byte lead. Each
    // consumes EXACTLY ONE byte (no resync, no infinite loop) and reports the
    // replacement, which maps to the block.
    const std::string bad = "\x80\xD0";
    size_t bpos = 0;
    CHECK(utf8_next(bad, bpos) == FONT_REPLACEMENT_CP);
    CHECK(bpos == 1);
    CHECK(utf8_next(bad, bpos) == FONT_REPLACEMENT_CP);
    CHECK(bpos == 2);
    CHECK(font_slot_for_codepoint(FONT_REPLACEMENT_CP) == FONT_MISSING_SLOT);

    // CONTROL: the same byte COUNT of well-formed input decodes to fewer
    // glyphs, so the case above is not just "every byte is one glyph".
    CHECK(text_glyph_count(std::string("\xD0\x91")) == 1);

    // Every byte value terminates. A decoder that can hang on one input is a
    // decoder that hangs on a corrupt save file.
    for (int b = 0; b < 256; ++b) {
        const std::string one(1, static_cast<char>(b));
        size_t p = 0;
        (void)utf8_next(one, p);
        CHECK(p >= 1);
    }
}

TEST_CASE("draw_text lands inside its advance box and clips at the canvas edge") {
    PixelCanvas canvas;
    canvas.resize(64, 32);
    canvas.clear(Color{0, 0, 0});
    const int advance = draw_text(canvas, 4, 5, "Аб", Color{255, 255, 255});
    CHECK(advance == 2 * FONT_CELL_W);

    const auto pixels = canvas.pixels();
    const auto lit = [&](int x, int y) {
        return pixels[(static_cast<size_t>(y) * 64 + static_cast<size_t>(x)) * 4] != 0;
    };
    int inside = 0;
    int outside = 0;
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 64; ++x) {
            if (!lit(x, y)) {
                continue;
            }
            const bool in_box = x >= 4 && x < 4 + advance && y >= 5 && y < 5 + FONT_CELL_H;
            (in_box ? inside : outside) += 1;
        }
    }
    CHECK(inside > 0);
    CHECK(outside == 0);

    // CONTROL: a space draws nothing at all, so "inside > 0" above measured ink
    // and not the loop.
    canvas.clear(Color{0, 0, 0});
    draw_text(canvas, 4, 5, " ", Color{255, 255, 255});
    int any = 0;
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 64; ++x) {
            any += lit(x, y) ? 1 : 0;
        }
    }
    CHECK(any == 0);

    // Off-canvas coordinates are dropped, never asserted (screens are laid out
    // from a runtime resolution; 320x180 is a shipping preset).
    draw_text(canvas, -20, -20, "Тест", Color{255, 255, 255});
    draw_text(canvas, 900, 900, "Тест", Color{255, 255, 255});
    draw_text(canvas, 60, 28, "Длинная строка", Color{255, 255, 255});
    CHECK(canvas.pixels().size() == 64u * 32u * 4u);
}

TEST_CASE("the shadow pass sits one pixel down-right and never eats the glyph") {
    PixelCanvas canvas;
    canvas.resize(32, 16);
    canvas.clear(Color{0, 0, 0});
    draw_text(canvas, 2, 2, "H", Color{255, 255, 255}, true, Color{60, 60, 60});
    const auto pixels = canvas.pixels();
    const auto at = [&](int x, int y) {
        return pixels[(static_cast<size_t>(y) * 32 + static_cast<size_t>(x)) * 4];
    };
    // Top-left ink of 'H' stays the bright colour ...
    CHECK(at(2, 2) == 255);
    // ... and the shadow shows one step down-right of the stem's bottom.
    CHECK(at(3, 2 + FONT_BASELINE_ROW + 1) == 60);
    // CONTROL: without the shadow that pixel is background.
    canvas.clear(Color{0, 0, 0});
    draw_text(canvas, 2, 2, "H", Color{255, 255, 255});
    CHECK(canvas.pixels()[(static_cast<size_t>(2 + FONT_BASELINE_ROW + 1) * 32 + 3) * 4] == 0);
}
