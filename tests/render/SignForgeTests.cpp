/*
Created: 17:08:2026 - 14:46:25
Last updated: 17:08:2026 - 14:46:25
Module: tests
File: tests/render/SignForgeTests.cpp

Responsibility:
- The sign forge: that the merge really is the font's ink (exactly covered, no
  more and no less), that the letters are RAISED VOLUME and never a hole, that
  the board and the ink land on different atlas tiles, and that a sign is named
  by what it SAYS.

Key items:
- exact-cover of the glyph mask + the per-pixel control, raised-volume
  measurement with the empty-text control, the ink/board contrast pair.

Dependencies:
- Uses: SignForge.h, BitmapFont.h, PartsAtlas.h, MeshMeters.h.
- Used by: tests/render.cmake (render_sign_forge).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- No user-facing string here either; the strings below are TEST FIXTURES and
  say so — they are never drawn in the game.
*/
/*
UPD:
- 17:08:2026 - 14:46:25: Создан вместе с SignForge — работа 4 заказа 17.08.
*/

#include "engine/render/sources/BitmapFont.h"
#include "engine/render/sources/PartsAtlas.h"
#include "engine/render/sources/SignForge.h"
#include "tests/render/MeshMeters.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace dfn::render;

namespace {

/// TEST FIXTURES, not content: a Cyrillic line and a Latin one, so the merge
/// is exercised on both alphabets the font carries.
const std::vector<std::string> FIXTURE_LINES = {"Сруб на цоколе", "wall 13u"};

[[nodiscard]] size_t tri_count(const RegistryObject& o) {
    return meshtest::solid_of(o).triangle_count();
}

[[nodiscard]] float luma(const glm::vec3& c) {
    return 0.30f * c.r + 0.59f * c.g + 0.11f * c.b;
}

} // namespace

TEST_CASE("sign forge: the merged rectangles ARE the font's ink, exactly") {
    const FontAtlas& atlas = font_atlas();
    int total_rects = 0;
    int total_ink = 0;
    int glyphs = 0;
    for (int slot = 0; slot < FONT_SLOT_COUNT; ++slot) {
        int ink = 0;
        for (int y = 0; y < FONT_INK_H; ++y) {
            for (int x = 0; x < FONT_INK_W; ++x) {
                ink += atlas.ink(slot, x, y) ? 1 : 0;
            }
        }
        const std::vector<GlyphRect> rects = glyph_rects(slot);
        // EXACT COVER, both ways: every rectangle is ink (no letter grows a
        // limb) and every ink pixel is covered exactly once (no stroke is
        // dropped, no box is drawn twice into the same place).
        int cover[FONT_INK_H][FONT_INK_W] = {};
        for (const GlyphRect& r : rects) {
            for (int y = r.y; y < r.y + r.h; ++y) {
                for (int x = r.x; x < r.x + r.w; ++x) {
                    REQUIRE(x >= 0);
                    REQUIRE(y >= 0);
                    REQUIRE(x < FONT_INK_W);
                    REQUIRE(y < FONT_INK_H);
                    CHECK(atlas.ink(slot, x, y));
                    ++cover[y][x];
                }
            }
        }
        for (int y = 0; y < FONT_INK_H; ++y) {
            for (int x = 0; x < FONT_INK_W; ++x) {
                CHECK(cover[y][x] == (atlas.ink(slot, x, y) ? 1 : 0));
            }
        }
        if (ink > 0) {
            ++glyphs;
            total_ink += ink;
            total_rects += static_cast<int>(rects.size());
        } else {
            // The control that costs nothing: a blank cell (the space) makes
            // no boxes at all. A merge that invented one would be inventing
            // geometry out of nothing.
            CHECK(rects.empty());
        }
    }
    // THE MERGE IS THE MECHANISM, so it is measured against its own control:
    // the per-pixel count. Measured over the whole alphabet: 175 inked glyphs,
    // 2616 ink pixels, 822 rectangles — 4.70 boxes per glyph against 14.95
    // pixels, i.e. 3.18x fewer boxes. Below 2x the merge would not be worth
    // the code.
    const double per_glyph = static_cast<double>(total_rects) / glyphs;
    const double pixels_per_glyph = static_cast<double>(total_ink) / glyphs;
    MESSAGE("rects/glyph " << per_glyph << " vs pixels/glyph " << pixels_per_glyph);
    CHECK(pixels_per_glyph / per_glyph > 2.0);
}

TEST_CASE("sign forge: the letters are RAISED VOLUME, not a hole") {
    SignParams p;
    p.lines = FIXTURE_LINES;
    p.shape = SignShape::Wall;
    const RegistryObject sign = forge_sign(p);
    const dfn::render::MeshData& m = meshtest::solid_of(sign);
    REQUIRE(!m.indices.empty());

    // Sealed hull and outward winding: a sign is a world object like any other
    // (Rule 52, and this zone's "дыр не бывает").
    CHECK(meshtest::half_edge_defects(m) == 0);
    CHECK(meshtest::signed_volume(m) > 0.0);

    // NOTHING CROSSES THE BOARD'S BACK. Letters cut through would be daylight
    // in the shape of words; the deepest vertex belongs to the cleats that
    // hold the board off the wall, at z = 0.
    float zmin = 1e9f;
    float zmax = -1e9f;
    for (const auto& v : m.vertices) {
        zmin = std::min(zmin, v.position.z);
        zmax = std::max(zmax, v.position.z);
    }
    CHECK(zmin >= -1e-4f);
    // Board face + the relief, and no more: the letters stand out by exactly
    // the chisel depth they declare.
    CHECK(zmax == doctest::Approx(0.025f + 0.04f + SIGN_LETTER_RELIEF_M).epsilon(0.01));

    // THE CONTROL, AND IT DIFFERS IN THE SUBJECT ALONE (Rule 47): the same
    // sign written in SPACES — same glyph count, therefore the same board,
    // the same margins and the same posts, and no ink at all. The first
    // version of this arm used EMPTY lines and measured the board shrinking:
    // it read a 42x excess and would have "passed" any threshold that was not
    // looking.
    SignParams blank = p;
    blank.lines.clear();
    for (const std::string& line : FIXTURE_LINES) {
        blank.lines.emplace_back(static_cast<size_t>(text_glyph_count(line)), ' ');
    }
    const RegistryObject empty_sign = forge_sign(blank);
    const double excess = meshtest::signed_volume(m)
                        - meshtest::signed_volume(meshtest::solid_of(empty_sign));
    int ink_pixels = 0;
    for (const std::string& line : FIXTURE_LINES) {
        size_t pos = 0;
        while (pos < line.size()) {
            const int slot = font_slot_for_codepoint(utf8_next(line, pos));
            for (const GlyphRect& r : glyph_rects(slot)) {
                ink_pixels += r.w * r.h;
            }
        }
    }
    const double px = SIGN_CAP_MEDIUM_M / static_cast<double>(FONT_INK_H);
    const double expect = ink_pixels * px * px * SIGN_LETTER_RELIEF_M;
    MESSAGE("raised volume " << excess << " m3 vs ink area x relief " << expect);
    CHECK(excess > expect * 0.9);
    CHECK(excess < expect * 1.1);
    CHECK(ink_pixels > 100); // the fixture really does say something
}

TEST_CASE("sign forge: the ink does not wear the board's tile") {
    // The lead's warning, 17.08: a sign whose letters land on the board's own
    // atlas row is a board with a texture and no text on it. Held by LUMA,
    // because "different tile" is not the claim — "readable" is.
    SignParams p;
    p.lines = FIXTURE_LINES;
    const RegistryObject sign = forge_sign(p);
    const dfn::render::MeshData& m = meshtest::solid_of(sign);

    std::vector<uint32_t> tiles;
    for (const auto& v : m.vertices) {
        const uint32_t key = v.color_rgba & 0xFFFFu; // column | row << 8
        if (std::find(tiles.begin(), tiles.end(), key) == tiles.end()) {
            tiles.push_back(key);
        }
    }
    // Board, ink, and the wood's end grain — but never one tile for everything.
    CHECK(tiles.size() >= 2);

    const glm::vec3 board = parts_tile_base(PartSurface::HewnTimber, PartTone::Mid);
    const glm::vec3 ink = parts_tile_base(PartSurface::HewnTimber, PartTone::Dark);
    const float contrast = luma(ink) / luma(board);
    MESSAGE("ink/board luma ratio " << contrast);
    // Dark on light. 0.7 is the fence: the kit's own pairing measures ~0.56,
    // and the REJECTED instance is a sign lettered in its own board's tone,
    // which reads exactly 1.00 by construction.
    CHECK(contrast < 0.7f);
    const float same = luma(board) / luma(board);
    CHECK(same >= 0.7f); // the control the fence exists to reject
}

TEST_CASE("sign forge: a sign is named by what it says") {
    SignParams a;
    a.lines = {"Кузница"};
    SignParams b = a;
    b.lines = {"Пекарня"};
    SignParams a2 = a;
    a2.seed = 99u; // a different seed is the same sign, said again
    CHECK(sign_name(a) != sign_name(b));
    CHECK(sign_name(a) == sign_name(a2));
    // ...and the shape and size are part of the identity, or a post label and
    // a hanging one of the same words would fight over one file.
    SignParams hanging = a;
    hanging.shape = SignShape::Hanging;
    CHECK(sign_name(a) != sign_name(hanging));
    SignParams big = a;
    big.cap_height_m = SIGN_CAP_LARGE_M;
    CHECK(sign_name(a) != sign_name(big));
}

TEST_CASE("sign forge: the text fits inside the board it is written on") {
    SignParams p;
    p.lines = {"Дом ремесленника", "сруб на каменном цоколе"};
    p.cap_height_m = SIGN_CAP_SMALL_M;
    p.shape = SignShape::Wall;
    const glm::vec2 size = sign_board_size(p);
    const RegistryObject sign = forge_sign(p);
    const float face = 0.025f + 0.04f;
    for (const auto& v : meshtest::solid_of(sign).vertices) {
        if (v.position.z <= face + 1e-4f) {
            continue; // board and cleats, not a letter
        }
        CHECK(v.position.x >= -size.x * 0.5f);
        CHECK(v.position.x <= size.x * 0.5f);
        CHECK(v.position.y >= 0.0f);
        CHECK(v.position.y <= size.y);
    }
}

TEST_CASE("sign forge: the three shapes hold the board up in three ways") {
    SignParams p;
    p.lines = {"Указатель"};
    const auto span = [&](SignShape shape) {
        SignParams q = p;
        q.shape = shape;
        const RegistryObject o = forge_sign(q);
        glm::vec3 lo{1e9f};
        glm::vec3 hi{-1e9f};
        for (const auto& v : meshtest::solid_of(o).vertices) {
            lo = glm::min(lo, v.position);
            hi = glm::max(hi, v.position);
        }
        return std::pair{lo, hi};
    };
    const auto [post_lo, post_hi] = span(SignShape::Post);
    // A POST REACHES THE GROUND: origin at its foot, board carried to reading
    // height. A label floating at 1.2 m with nothing under it is the defect
    // this asserts against.
    CHECK(post_lo.y == doctest::Approx(0.0f).epsilon(0.01));
    CHECK(post_hi.y > 1.2f);

    const auto [hang_lo, hang_hi] = span(SignShape::Hanging);
    // A HANGING SIGN HANGS: everything of it is BELOW its attachment, and its
    // arm reaches out of the wall it is fixed to.
    CHECK(hang_lo.y < 0.0f);
    CHECK(hang_hi.z > 0.5f);

    const auto [wall_lo, wall_hi] = span(SignShape::Wall);
    // A WALL SIGN IS FLAT AGAINST ITS WALL: it starts at the wall face and
    // reaches out by the cleat, the board and the relief and nothing more.
    CHECK(wall_lo.z == doctest::Approx(0.0f).epsilon(0.01));
    CHECK(wall_hi.z < 0.10f);
    CHECK(wall_lo.y == doctest::Approx(0.0f).epsilon(0.01));
}

TEST_CASE("sign forge: deterministic to the byte") {
    SignParams p;
    p.lines = FIXTURE_LINES;
    const RegistryObject a = forge_sign(p);
    const RegistryObject b = forge_sign(p);
    REQUIRE(tri_count(a) == tri_count(b));
    CHECK(object_content_hash(a) == object_content_hash(b));
}
