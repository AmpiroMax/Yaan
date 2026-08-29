/*
Module: engine/render
File: engine/render/sources/SignForge.cpp

Responsibility:
- The sign forge's geometry: the bitmap font's ink merged into rectangles and
  extruded onto a board, and the three ways a board is held up.

Key items:
- glyph_rects(), sign_board_size(), sign_name(), forge_sign().

Dependencies:
- Uses: SignForge.h, BitmapFont.h, HewnBar.h, PartForgeDetail.h (the material
  table), core serialization (Fnv1a64).
- Used by: tools/forge_signs.cpp, SignForgeTests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NO USER-FACING STRING IN THIS FILE (Rule 5). Text arrives as a parameter.
- THE BOARD AND THE LETTER MUST NOT SHARE A TILE (lead, 17.08): they carry
  their own tile index in the vertex colour, and a sign whose ink and board
  land on the same row is a sign with no text on it.
*/

#include "engine/render/sources/SignForge.h"

#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/render/sources/BitmapFont.h"
#include "engine/render/sources/HewnBar.h"
#include "engine/render/sources/PartForgeDetail.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <string_view>

namespace dfn::render {
namespace {

using part_detail::material_of;
using Material = HewnMaterial;
using Rng = HewnRng;

/// The board's own timber: thick enough to be a board and not a card, thin
/// enough to hang. 40 mm is a plank.
constexpr float BOARD_THICK_M = 0.04f;
/// Margin from the ink to the board's edge, in FONT PIXELS. Three pixels reads
/// as a frame at every cap height because it scales with the letters.
constexpr float MARGIN_PX = 3.0f;
/// A post's section and the height it carries the board's CENTRE to. 1.2 m
/// against PLAYER_EYE_HEIGHT 1.7 (docs/NUMBERS.md): a label is read looking
/// slightly down, the way a museum label is hung.
constexpr float POST_SIDE_M = 0.08f;
constexpr float POST_CENTRE_M = 1.20f;
/// A hanging sign's arm, out of the wall, and how far the board swings below.
constexpr float ARM_LEN_M = 0.75f;
constexpr float ARM_SIDE_M = 0.07f;
constexpr float HANGER_DROP_M = 0.12f;
constexpr float HANGER_SIDE_M = 0.035f;
/// A wall sign's cleats: what holds it off the wall so it casts its own line
/// of shadow instead of being painted onto the boards.
constexpr float CLEAT_DEPTH_M = 0.025f;
constexpr float CLEAT_SIDE_M = 0.05f;

[[nodiscard]] Material crisp(const Material& src) {
    // A LETTER HAS NO CHAMFER AND NO WOBBLE. Both are wear on a hewn timber
    // and both are right there; on an 8 mm relief the material's 22% chamfer
    // would eat the letter's whole depth and the wobble would bend a stroke.
    Material m = src;
    m.chamfer = 0.0f;
    m.wobble = 0.0f;
    return m;
}

/// The font's ink for one slot, merged greedily into maximal rectangles: take
/// the longest run right, then grow it DOWN as far as the same span stays ink.
/// A vertical stroke becomes one box, which is the whole saving.
[[nodiscard]] std::vector<GlyphRect> merge_ink(int slot) {
    const FontAtlas& atlas = font_atlas();
    bool used[FONT_INK_H][FONT_INK_W] = {};
    std::vector<GlyphRect> out;
    for (int y = 0; y < FONT_INK_H; ++y) {
        for (int x = 0; x < FONT_INK_W; ++x) {
            if (used[y][x] || !atlas.ink(slot, x, y)) {
                continue;
            }
            int w = 0;
            while (x + w < FONT_INK_W && !used[y][x + w] && atlas.ink(slot, x + w, y)) {
                ++w;
            }
            int h = 1;
            while (y + h < FONT_INK_H) {
                bool row_ok = true;
                for (int k = 0; k < w; ++k) {
                    if (used[y + h][x + k] || !atlas.ink(slot, x + k, y + h)) {
                        row_ok = false;
                        break;
                    }
                }
                if (!row_ok) {
                    break;
                }
                ++h;
            }
            for (int dy = 0; dy < h; ++dy) {
                for (int dx = 0; dx < w; ++dx) {
                    used[y + dy][x + dx] = true;
                }
            }
            out.push_back({x, y, w, h});
        }
    }
    return out;
}

[[nodiscard]] int longest_line_glyphs(const SignParams& p) {
    int widest = 0;
    for (const std::string& line : p.lines) {
        widest = std::max(widest, text_glyph_count(line));
    }
    return widest;
}

/// Lays the raised letters of one line onto the board's face. `x0` is the left
/// edge of the first cell, `y_top` the top of the ink band, both in metres;
/// `z` is the face the letters stand out of.
void emit_line(MeshData& m, std::string_view line, float x0, float y_top, float z,
               float px, const Material& ink, float wear, Rng& rng) {
    size_t pos = 0;
    int cell = 0;
    while (pos < line.size()) {
        const uint32_t cp = utf8_next(line, pos);
        const int slot = font_slot_for_codepoint(cp);
        for (const GlyphRect& r : merge_ink(slot)) {
            const float bx = x0 + (static_cast<float>(cell * FONT_CELL_W + r.x)) * px;
            // Font y runs DOWN from the cell top; the world's runs up.
            const float by = y_top - static_cast<float>(r.y + r.h) * px;
            hewn_block(m, {bx, by, z},
                       {static_cast<float>(r.w) * px, static_cast<float>(r.h) * px,
                        SIGN_LETTER_RELIEF_M},
                       ink, wear, rng, 1);
        }
        ++cell;
    }
}

} // namespace

std::vector<GlyphRect> glyph_rects(int slot) { return merge_ink(slot); }

glm::vec2 sign_board_size(const SignParams& p) {
    const float px = p.cap_height_m / static_cast<float>(FONT_INK_H);
    const auto lines = static_cast<float>(std::max<size_t>(p.lines.size(), 1));
    const float text_w = static_cast<float>(longest_line_glyphs(p) * FONT_CELL_W) * px;
    const float text_h = lines * static_cast<float>(FONT_CELL_H) * px;
    const float margin = MARGIN_PX * px;
    return {text_w + 2.0f * margin, text_h + 2.0f * margin};
}

std::string sign_name(const SignParams& p) {
    if (!p.name.empty()) {
        return p.name;
    }
    // THE HASH IS THE POINT. A sign is named by WHAT IT SAYS, so two different
    // texts can never collide on one file name and quietly become one sign —
    // and the same text re-forged keeps its name, so a scene that places it
    // does not have to be edited when the catalogue is rebuilt.
    serialization::Fnv1a64 h;
    for (const std::string& line : p.lines) {
        h.update_length_prefixed(line);
    }
    h.update_u64(static_cast<uint64_t>(p.shape));
    h.update_u64(static_cast<uint64_t>(std::lround(p.cap_height_m * 1000.0f)));
    h.update_u64(static_cast<uint64_t>(p.board));
    h.update_u64(static_cast<uint64_t>(p.ink));
    const char* shape = p.shape == SignShape::Hanging ? "hang"
                        : (p.shape == SignShape::Post ? "post" : "wall");
    char buf[96];
    std::snprintf(buf, sizeof(buf), "sign-%s-h%02d-%016llx", shape,
                  static_cast<int>(std::lround(p.cap_height_m * 100.0f)),
                  static_cast<unsigned long long>(h.digest()));
    return buf;
}

RegistryObject forge_sign(const SignParams& p) {
    RegistryObject obj;
    obj.name = sign_name(p);
    // A PART, not a kind of its own: the app gives kind == "part" triangle
    // collision, and a sign on a post is something you walk around and cannot
    // walk through. Nothing about it needs a new kind.
    obj.kind = "part";
    {
        char src[160];
        std::snprintf(src, sizeof(src), "sign:%zu line(s) cap=%.2f seed=%llu",
                      p.lines.size(), static_cast<double>(p.cap_height_m),
                      static_cast<unsigned long long>(p.seed));
        obj.source = src;
    }

    Material board_mat = material_of(p.board, p.wear, p.textured);
    Material ink_mat = crisp(material_of(p.ink, p.wear, p.textured));
    if (!p.textured) {
        board_mat.skin.textured = false;
        ink_mat.skin.textured = false;
    }
    // THE LETTER MUST NOT WEAR THE BOARD'S TILE (lead's warning): a sign whose
    // ink and board land on the same atlas row is a board with a texture on it
    // and no text. The kit's default pairing is timber over dark timber, which
    // are different ROWS of one column — asserted in the suite by luma, not by
    // hope.
    if (board_mat.skin.textured && ink_mat.skin.side == board_mat.skin.side
        && ink_mat.skin.side_tone == board_mat.skin.side_tone) {
        ink_mat.skin.side_tone = PartTone::Dark;
        ink_mat.skin.end_tone = PartTone::Dark;
    }
    MeshData& out = board_mat.skin.textured ? obj.bark : obj.wood;

    const float px = p.cap_height_m / static_cast<float>(FONT_INK_H);
    const glm::vec2 size = sign_board_size(p);
    const float margin = MARGIN_PX * px;
    Rng rng(p.seed);

    // The board's own frame: x centred on the origin, y from `base`, the face
    // that carries the text at +Z (the stand's viewer stands south of it).
    float base = 0.0f;
    switch (p.shape) {
    case SignShape::Post: base = POST_CENTRE_M - size.y * 0.5f; break;
    case SignShape::Hanging: base = -(HANGER_DROP_M + size.y); break;
    case SignShape::Wall: base = 0.0f; break;
    }
    float board_z = 0.0f;
    if (p.shape == SignShape::Wall) {
        board_z = CLEAT_DEPTH_M; // held off the wall, so it casts its own line
    } else if (p.shape == SignShape::Hanging) {
        board_z = ARM_LEN_M * 0.5f - BOARD_THICK_M * 0.5f;
    }

    hewn_block(out, {-size.x * 0.5f, base, board_z},
               {size.x, size.y, BOARD_THICK_M}, board_mat, p.wear, rng, 1);

    // The text, top line first.
    const float face_z = board_z + BOARD_THICK_M;
    const float x0 = -size.x * 0.5f + margin;
    for (size_t i = 0; i < p.lines.size(); ++i) {
        const float y_top = base + size.y - margin
                          - static_cast<float>(i) * static_cast<float>(FONT_CELL_H) * px;
        emit_line(out, p.lines[i], x0, y_top, face_z, px, ink_mat, p.wear, rng);
    }

    // ...and what holds it up.
    switch (p.shape) {
    case SignShape::Post: {
        // ONE post under a narrow board, TWO under a wide one — a 2 m label on
        // a single 8 cm stick reads as about to fall over, and the eye is
        // right: it would.
        const float top = base + 0.10f; // into the board, no gap at the joint
        const std::vector<float> xs = size.x > 0.8f
            ? std::vector<float>{-size.x * 0.5f + 0.12f, size.x * 0.5f - 0.12f - POST_SIDE_M}
            : std::vector<float>{-POST_SIDE_M * 0.5f};
        for (const float x : xs) {
            hewn_block(out, {x, 0.0f, board_z + BOARD_THICK_M * 0.5f - POST_SIDE_M * 0.5f},
                       {POST_SIDE_M, top, POST_SIDE_M}, board_mat, p.wear, rng, 2);
        }
        break;
    }
    case SignShape::Hanging: {
        // The arm out of the wall, and two hangers down to the board's top
        // corners. The board swings UNDER the arm, so the arm's own length is
        // what keeps the sign clear of the wall it advertises.
        hewn_block(out, {-ARM_SIDE_M * 0.5f, -ARM_SIDE_M, 0.0f},
                   {ARM_SIDE_M, ARM_SIDE_M, ARM_LEN_M}, board_mat, p.wear, rng, 2);
        const float top = base + size.y;
        for (int i = 0; i < 2; ++i) {
            const float z = board_z + (i == 0 ? 0.0f : BOARD_THICK_M - HANGER_SIDE_M);
            hewn_block(out, {-HANGER_SIDE_M * 0.5f, top - 0.02f, z},
                       {HANGER_SIDE_M, HANGER_DROP_M + 0.04f, HANGER_SIDE_M},
                       ink_mat, p.wear, rng, 1);
        }
        break;
    }
    case SignShape::Wall: {
        // Four cleats, one per corner, spanning the whole gap to the wall so
        // there is no daylight between the sign and what it is nailed to.
        for (int i = 0; i < 4; ++i) {
            const float cx = (i & 1) ? size.x * 0.5f - CLEAT_SIDE_M - 0.03f
                                     : -size.x * 0.5f + 0.03f;
            const float cy = (i & 2) ? base + size.y - CLEAT_SIDE_M - 0.03f
                                     : base + 0.03f;
            hewn_block(out, {cx, cy, 0.0f},
                       {CLEAT_SIDE_M, CLEAT_SIDE_M, CLEAT_DEPTH_M}, ink_mat,
                       p.wear, rng, 1);
        }
        break;
    }
    }
    return obj;
}

namespace {

[[nodiscard]] std::string trim(std::string_view s) {
    size_t a = 0;
    size_t b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) {
        ++a;
    }
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) {
        --b;
    }
    return std::string(s.substr(a, b - a));
}

/// The file's word for a material. Only the kit's own names, and an unknown
/// word falls back rather than refusing: a board is still a board.
[[nodiscard]] PartMaterial material_by_name(const std::string& n,
                                            PartMaterial fallback) {
    if (n == "timber") return PartMaterial::Timber;
    if (n == "dark") return PartMaterial::TimberDark;
    if (n == "stone") return PartMaterial::Stone;
    if (n == "plaster") return PartMaterial::Plaster;
    if (n == "brick") return PartMaterial::Brick;
    return fallback;
}

} // namespace

bool read_signs_file(const std::string& path, std::vector<SignParams>& out) {
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "[signs] %s: cannot open\n", path.c_str());
        return false;
    }
    bool ok = true;
    std::string raw;
    uint64_t seed = 1;
    while (std::getline(in, raw)) {
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line == "[sign]") {
            out.emplace_back();
            out.back().seed = seed++;
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos || out.empty()) {
            std::fprintf(stderr, "[signs] %s: stray line \"%s\"\n", path.c_str(),
                         line.c_str());
            ok = false;
            continue;
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        SignParams& s = out.back();
        if (key == "line") {
            s.lines.push_back(value);
        } else if (key == "shape") {
            s.shape = value == "hanging" ? SignShape::Hanging
                      : (value == "wall" ? SignShape::Wall : SignShape::Post);
        } else if (key == "cap") {
            s.cap_height_m = std::strtof(value.c_str(), nullptr);
        } else if (key == "board") {
            s.board = material_by_name(value, PartMaterial::Timber);
        } else if (key == "ink") {
            s.ink = material_by_name(value, PartMaterial::TimberDark);
        } else if (key == "wear") {
            s.wear = std::strtof(value.c_str(), nullptr);
        } else if (key == "name") {
            s.name = value;
        } else {
            std::fprintf(stderr, "[signs] %s: unknown key \"%s\"\n", path.c_str(),
                         key.c_str());
            ok = false;
        }
    }
    return ok;
}

} // namespace dfn::render
