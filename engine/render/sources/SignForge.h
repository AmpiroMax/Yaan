/*
Created: 17:08:2026 - 14:46:25
Last updated: 17:08:2026 - 14:55:07
Module: engine/render
File: engine/render/sources/SignForge.h

Responsibility:
- ТАБЛИЧКИ С ТЕКСТОМ (user, 17.08: «сделай объекты — таблички: навесные, на
  столбике, с текстом»): one forge that turns ANY string into a readable object
  — a board carrying raised letters, hung from a bracket, set on a post, or
  nailed to a wall.

Key items:
- SignShape / SignParams / forge_sign(): one sign, any text.
- sign_name(): the file name, carrying a HASH of the text.
- glyph_rects(): the bitmap font's ink as merged rectangles (the mechanism).

WHY THE LETTERS ARE GEOMETRY AND NOT A TEXTURE. The engine has one font and it
is a 5x8 bitmap (BitmapFont.h) drawn into a PixelCanvas — a screen-space
overlay, which a board standing in the world is not. Baking the letters into
the mesh means they light, shadow and occlude like everything else, and it
costs nothing at runtime: a sign is a .dfo like any part.

WHY RAISED AND NEVER CUT THROUGH. Rule 52 and this zone's own rule: a world
object is a closed volume and a house has no holes. Letters sunk through a
board would be daylight in the shape of words. They stand 8 mm PROUD of the
face — carved-and-tarred lettering, which is how the reference's signs were
actually made.

WHY RECTANGLES AND NOT PIXELS. A glyph is up to 40 ink pixels; a box per pixel
would be 480 triangles per letter and a two-line sign would cost more than the
house it names. Merged rectangles (a vertical stroke is ONE box) bring a glyph
to 3-6 boxes — measured 4.2 average over the whole alphabet in the suite.

Dependencies:
- Uses: BitmapFont.h (the ink), HewnBar.h (the boards and posts), PartForge.h
  (materials), ObjectRegistry.h, core serialization (the text hash).
- Used by: tools/forge_signs.cpp, tests/render/SignForgeTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE TEXT IS CONTENT (Rule 5). It arrives as a parameter, from a data file
  outside this zone's code. No user-facing string may be typed in here.
- PURE AND DETERMINISTIC: same params, same bytes, same content hash.
- A NEWLINE NEVER REACHES THE FONT. BitmapFont draws a solid block for '\n' on
  purpose; this forge takes LINES and never hands it one.
*/
/*
UPD:
- 17:08:2026 - 14:46:25: Создан — работа 4 заказа 17.08 (таблички навесные / на столбике /
  настенные, текст — параметр).
- 17:08:2026 - 14:55:07: read_signs_file() — разбор .signs переехал сюда из dfn_signs: одна
  функция на двоих потребителей (инструмент и печь приложения).
*/

#pragma once

#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/PartForge.h"

#include <cstdint>
#include <string>
#include <vector>

namespace dfn::render {

/// How the sign is held up. The three the user named, and they differ in what
/// they attach TO — which is the only thing a composer has to know.
enum class SignShape : uint8_t {
    /// НАВЕСНАЯ: an arm out of a wall with the board swinging under it, the
    /// shop sign over a street. Origin AT THE WALL FACE, at the arm's height;
    /// the arm runs +X, the board hangs under it and reads across ±Z.
    Hanging = 0,
    /// НА СТОЛБИКЕ: a post in the ground with the board on top — the waymarker
    /// and the exhibit label. Origin AT THE POST'S FOOT on the ground; the
    /// board faces -Z (toward a viewer standing south of it, the stand's own
    /// approach).
    Post = 1,
    /// НАСТЕННАЯ: nailed flat to a wall through four cleats. Origin AT THE
    /// WALL FACE, at the board's BOTTOM edge; the board reads toward -Z.
    Wall = 2,
};

/// Letter heights of the kit, metres of CAP HEIGHT (the font's 8 ink rows).
/// Sized against the distance the sign is READ FROM (Rule 33), not against the
/// board: at 2 m a 0.05 m cap subtends ~1.4°, which is an order over the eye's
/// limit and still ~0.5° at 6 m — a label you read walking past. The large
/// size is for a name seen from across a square.
inline constexpr float SIGN_CAP_SMALL_M = 0.04f;  ///< passports: many words
inline constexpr float SIGN_CAP_MEDIUM_M = 0.06f; ///< exhibit labels
inline constexpr float SIGN_CAP_LARGE_M = 0.10f;  ///< a house's name

/// How far a letter stands out of the board. Under a millimetre would vanish
/// into the shading; a centimetre would read as a block. 8 mm is a chisel's
/// depth, and it is the same number the lead's ruling named (0.5-1 cm).
inline constexpr float SIGN_LETTER_RELIEF_M = 0.008f;

struct SignParams {
    uint64_t seed = 1;
    /// The TEXT, one entry per line, UTF-8, already localized (Rule 5). Empty
    /// lines are legal and make a gap; a '\n' inside a line is not, and would
    /// draw the font's missing-glyph block, loudly.
    std::vector<std::string> lines;
    SignShape shape = SignShape::Post;
    float cap_height_m = SIGN_CAP_MEDIUM_M;
    /// The board. Timber by default; a stone slab is a memorial, plaster is a
    /// painted panel.
    PartMaterial board = PartMaterial::Timber;
    /// The lettering. Dark by default: dark on light is the pairing that
    /// survives a grey day, and the kit's TimberDark is tar.
    PartMaterial ink = PartMaterial::TimberDark;
    float wear = 0.5f;
    /// FALSE bakes the sign with flat vertex-colour albedo instead of atlas
    /// tiles — the same object, on the untextured path. It exists because the
    /// kit's own catalogue is still baked flat while the renderer's parts-sheet
    /// binding lands: a textured sign standing among untextured houses would
    /// not be a preview of anything. It is also the control arm for every
    /// claim about the textured one (Rule 47).
    bool textured = true;
    /// Overrides the derived name. Leave empty and sign_name() decides.
    std::string name;
};

/// One ink rectangle of a glyph, in FONT CELL pixels (x right, y down from the
/// cell's top-left).
struct GlyphRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

/// The font's ink for one slot, merged into maximal rectangles. Exposed
/// because it IS the mechanism this file claims (and the suite measures the
/// merge factor against a per-pixel control).
[[nodiscard]] std::vector<GlyphRect> glyph_rects(int slot);

/// The name a sign gets: shape, cap height, and a 16-hex-digit HASH OF THE
/// TEXT. The hash is not decoration — two different texts must never land on
/// one file name, or the second sign silently becomes the first.
[[nodiscard]] std::string sign_name(const SignParams& params);

/// Forges one sign. Ready for write_object(); kind == "part", so it collides
/// by its triangles and can be walked around like anything else built.
[[nodiscard]] RegistryObject forge_sign(const SignParams& params);

/// The board's outer size in metres (width, height), before the post or the
/// bracket. A composer needs it to leave room, and the suite needs it to check
/// that the text fits INSIDE the board it was given.
[[nodiscard]] glm::vec2 sign_board_size(const SignParams& params);

/// Reads a `.signs` file — the format documented in tools/forge_signs.cpp —
/// into one SignParams per `[sign]` block, in file order, seeds 1..n.
///
/// WHY THE PARSER LIVES IN THE LIBRARY AND NOT IN THE TOOL (Rule 32, lead's
/// ruling 17.08). Two consumers read this format: the CLI dfn_signs and the
/// first-run bake in engine/app, which cannot call into a tool. Two copies of
/// a parser is two answers to "what does `board = stone` mean", and the copy
/// that drifts is always the one nobody tests.
///
/// Returns false if ANY line is not understood (stray line, unknown key,
/// unreadable file) and says which on stderr; `out` then holds what was read
/// before the refusal. `textured` is left at its default — the caller decides
/// which arm it is baking.
[[nodiscard]] bool read_signs_file(const std::string& path,
                                   std::vector<SignParams>& out);

} // namespace dfn::render
