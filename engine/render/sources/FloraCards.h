/*
Created: 09:08:2026 - 20:21:13
Last updated: 23:08:2026 - 23:55:00
Module: engine/render
File: engine/render/sources/FloraCards.h

Responsibility:
- The alpha-cutout leaf CARD vocabulary: the procedurally generated leaf mask
  atlas (no image files, Q13) laid out as SHAPE x COLOUR, and the single card
  emitter that writes the foliage vertex contract render's "foliage" program
  reads.

Key items:
- FloraSeason, LeafAtlas, generate_leaf_atlas(), leaf_tile_uv(),
  leaf_tone_color(), LeafCardParams, emit_leaf_card().

Dependencies:
- Uses: glm (MeshData is forward-declared; ProcMesh.h is included by the .cpp).
- Used by: ProcFlora (card placement), RenderSystem (atlas upload),
  tests/render/ProcFloraTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md §3.8.
- PURE AND DETERMINISTIC. Same arguments -> byte-identical pixels and vertices.
- THE VERTEX COLOUR CONTRACT ON THE "foliage" PROGRAM IS NOT ALBEDO (render's
  vs_foliage/fs_foliage): r = sway weight, g = per-instance phase, b = per-card
  value jitter, a = sky visibility (render's interior-lighting channel — leave
  it at 1.0). Albedo comes from the atlas tile. Do not "fix" this to look like
  the opaque path.
- b is filled PER CARD, identically on all six vertices. Varying it per vertex
  turns the jitter into a gradient across the card, which is not what it is.
*/
/*
UPD:
- 09:08:2026 - 20:21:13: Created — leaf mask atlas + card emitter (user
  direction «листву плоскими прозрачными большими плоскими наборами
  листочков»; render's SHAPE x COLOUR atlas ruling).
- 15:08:2026 - 01:04:30: LEAF_ATLAS_TILE_PX 64 → 128 (вердикт: «фигуры сильно пиксельные, не надо
  пикселить»): 64px на 3-метровой лапе — ~5 см/тексель НА ОБЪЕКТЕ, зубец
  листа на таком шаге не существует.
- 15:08:2026 - 02:14:30: LeafShape::BarkPlate (5-я колонка атласа: 8 колорвеев коры) и
  LEAF_ATLAS_REVISION в ключ дискового кэша — кэш 4-колоночного атласа под
  5-колоночными uv красил хвою берёзовыми тайлами В БЕЛЫЙ, при верном коде везде.
- 15:08:2026 - 15:54:46: v6 (FullHD-пивот, лид 552d9ab/09f75eb): тайл 128->512 (~5 мм/тексель на 2.5 м фронде), LEAF_TILE_MARGIN 0.055 (поле прозрачности по периметру — «полоски по краям» умирают по построению), REVISION 3->4, контракт альфы: градиентная кромка внутри рваной формы (A2C бэкенда).
- 15:08:2026 - 16:02:49: generate_leaf_normal_atlas(): лист нормалей под aux_texture лида (09f75eb) — та же раскладка, xyz->RGB, нейтраль (128,128,255) на прозрачном и не-коре. REVISION 4->5 (арт коры).
- 15:08:2026 - 16:08:30: REVISION 5->6 (штампы листьев — арт пачек).
- 15:08:2026 - 16:15:28: REVISION 6->7 (пер-видовые штампы).
- 16:08:2026 - 20:23:55: REVISION 7->8 (кора v3 по фото, периодический тайл).
- 16:08:2026 - 20:31:55: REVISION 8->9 (штампы по сканам).
- 16:08:2026 - 20:42:17: REVISION 9->10 (плотный фронд по скану).
- 16:08:2026 - 22:40:39: REVISION 10->11 (плотность хвои).
- 17:08:2026 - 10:55:52: LEAF_ATLAS_REVISION 12 — новая берёста (штрихи вместо клякс).
- 23:08:2026 - 23:55:00: ПЯТЬ ЦВЕТНЫХ РЯДОВ, ДОБАВЛЕНЫ, А НЕ ЗАМЕНЕНЫ (владелец, 24.08:
  «добавим розовые листья, не уберём старые, а добавим новые; также добавить
  красные, жёлтые, синие, фиолетовые необходимо сразу»): BlossomPink,
  MapleRed, AutumnGold, ArcaneBlue, DuskViolet. LEAF_ATLAS_TONES 8->13,
  REVISION 12->13 (ключ кэша обязан смениться вместе с высотой листа).
  Восемь ЗЕЛЁНЫХ рядов стоят на местах 0..7 нетронутыми — ни один старый
  рецепт не меняет ряд; поэтому вид старых деревьев прежний, а uv у них
  пересчитаны (высота атласа 8*512 -> 16*512, v делится на новую высоту) и
  все 77 .dfo перепечены. LEAF_ATLAS_GREEN_TONES — граница старой полосы: на
  неё, а не на общий счёт рядов, опирается страховочный зажим FloraBuild,
  иначе выход species-полосы за 7 покрасил бы хвою в розовый.
- 23:08:2026 - 23:55:00: ШЕСТНАДЦАТЬ РЯДОВ, А НЕ ТРИНАДЦАТЬ, И ШОВНЫЙ РЯД —
  оба решения ИЗМЕРЕНЫ, а не выбраны по вкусу. Наивная выкладка (8+5=13)
  дала кадр рощи, отличающийся от собственной базы на 0.161 % пикселей при
  нетронутых тайлах: (1) v старых тайлов делится на некратную высоту, и
  точечная выборка на полутексельном отступе местами перескакивает на соседний
  тексель; (2) линейная МИНИФИКАЦИЯ (путь coverage) на нижней кромке ряда 7
  раньше упиралась в край текстуры и клампилась, а с новым соседом стала
  подмешивать чужой цвет. Степень двойки чинит (1) — v ровно делится пополам,
  адрес текселя побитово тот же; зеркальный SeamGuard чинит (2) — блендинг на
  шве возвращает собственную кромку ряда 7. Итог: 0.000 %. ЦЕНА СКАЗАНА
  ВСЛУХ: лист 2560x4096 -> 2560x8192 (~42 -> ~84 МБ), и столько же у листа
  нормалей. Дешёвый вариант (13 рядов, +26 МБ, расхождение 0.161 % при maxch
  23) существует и отвергнут только потому, что неприкосновенность старых
  деревьев — главный критерий этой волны; смена решения — одна константа.
- 23:08:2026 - 23:55:00: leaf_tone_name/leaf_tone_by_name — паспортное ИМЯ ряда
  («pink», «red», «gold», «blue», «violet», зелёные — по старым именам).
  Рецепту кузницы цвет задаётся полем TreeForgeParams::tone; имя нужно, чтобы
  паспорт города заказывал рощу словом, а не числом enum.
*/

#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace dfn::render {

/// Declared, not included: FloraSpecies.h needs this file's vocabulary
/// (LeafShape / LeafTone / FloraSeason) and must not drag in the whole mesh
/// header to get it.
struct MeshData;

/// Seasons are a DATA SWAP, never a world rebuild (LANDSCAPE §5.11): the mesh
/// stores an atlas tile, the atlas stores the colour. Changing season
/// regenerates ONE texture and re-uploads it — no mesh, no chunk, no baked
/// jitter is invalidated.
enum class FloraSeason : uint8_t {
    Summer = 0,
    Autumn = 1,
    Winter = 2,
};

/// Atlas geometry. Column = leaf SHAPE, row = leaf TONE, so one tile IS the
/// (shape, colour) pair: colour costs zero extra vertex bytes and is not welded
/// to shape — the same shape can appear light and dark in one crown, which is
/// what a crown that is 79-86 % leaf in its core needs to read as volume
/// (docs/specs/flora.md §3.10).
inline constexpr uint32_t LEAF_ATLAS_SHAPES = 5;
/// SIXTEEN ROWS, AND THE POWER OF TWO IS THE WHOLE POINT (23.08, the colour
/// wave). Eight greens + one seam guard + five colours is FOURTEEN; the sheet
/// is sized to sixteen anyway because a tile's v is `row * tile / height`, and
/// only a power-of-two growth of `height` leaves every existing v EXACTLY
/// halved — an exact scaling by 2 in IEEE floats, so `v * height` (the texel
/// the sampler actually addresses) comes out bit-identical for every old tile.
/// MEASURED, not assumed: at thirteen rows the grove frame differed from its
/// own baseline by 0.161 % of pixels (max channel 23) with nothing but the row
/// count changed — point-sampled fetches flipping to the neighbouring texel
/// where the half-texel inset landed on the other side of a rounding boundary.
/// At sixteen it is 0.000 %.
inline constexpr uint32_t LEAF_ATLAS_TONES = 16;
/// The GREEN band — rows 0..7, the eight tones the shipped catalog was built
/// on. It is a separate constant from the row COUNT on purpose: the colour
/// rows added on 23.08 sit BELOW it, so any code that used "the last row" as a
/// safety clamp must clamp to the end of the green band or it silently starts
/// resolving out-of-band tones to pink (FloraBuild's card tone draw).
///
/// The band being EVEN is load-bearing too: create_texture's mip chain halves
/// by pairs, so rows 0..7 average only among themselves at every level and no
/// colour ever reaches a green mip.
inline constexpr uint32_t LEAF_ATLAS_GREEN_TONES = 8;
/// Bumped on every change to the tiles' ART (masks, packs, bark) — the disk
/// cache key must change when the pixels would, or the game paints with the
/// previous session's atlas (measured: the 4-column cache under 5-column uvs
/// painted the conifers white with birch tiles).
inline constexpr uint32_t LEAF_ATLAS_REVISION = 13;
/// 512 under the Full HD pivot (lead, 552d9ab: internal res 1920x1080, bake
/// density for it; frame cost measured independent of texture density). A
/// 512 px tile over a ~2.5 m frond is ~5 mm per texel on the object — leaf
/// serration and needle combs exist at that pitch. 128 was sized for the
/// retired 640x360 target.
inline constexpr uint32_t LEAF_ATLAS_TILE_PX = 512;
/// Transparent margin around every LEAF tile, as a fraction of the tile side.
/// The masses of a pack must DIE OUT before the tile border: a blob clipped by
/// the border rasterises as a dead-straight cut, which the user read as
/// «полоски по краям листвы, словно полигон недорезали». Alpha ramps to zero
/// across the inner half of this band; nothing solid may touch the border.
inline constexpr float LEAF_TILE_MARGIN = 0.055f;

/// Leaf shape columns. A species names the band it may draw from; the SHAPE is
/// silhouette work, the TONE is value work, and they are deliberately free to
/// vary independently.
enum class LeafShape : uint8_t {
    RoundLobed = 0, ///< broad lobed mass — the broadleaf default
    OvalSpray = 1,  ///< elongated leaning spray — clump and rim fill
    RaggedTip = 2,  ///< tapering wedge — branch tips and crown top
    NeedleFan = 3,  ///< conifer feather (needle barbs on a twig)
    /// BARK — fully opaque tiles for TEXTURED trunks and limbs (user, on the
    /// gallery: «текстур на деревьях нет... нужны текстуры и учёт
    /// освещённости»). The tone ROW selects the bark colourway, not a leaf
    /// tone: oak furrows, birch paper with lenticels, pine plates, mossy oak
    /// and so on — see bark_palette() in the rasteriser. Rides the foliage
    /// program (albedo from the atlas, real lighting), wind zeroed.
    BarkPlate = 4,
};

/// Global foliage tone table. Rows are shared by every species in a chunk
/// because scatter bakes ONE merged buffer per chunk — all species, all cards,
/// one draw, one texture. Each species owns a contiguous band.
///
/// ROWS 0..7 ARE THE GREEN BAND AND THEY ARE FROZEN. Their indices are baked
/// into every .dfo on the shelves, and every species band in FloraSpecies is
/// expressed as an offset inside them. Colours are APPENDED below the band,
/// never inserted into it (owner, 24.08: «не уберём старые, а добавим новые»).
enum class LeafTone : uint8_t {
    OakMid = 0,
    OakDeep = 1,
    OakSunlit = 2,
    BirchLight = 3,
    BirchPale = 4,
    WillowDark = 5,
    WillowOlive = 6,
    ConiferDark = 7,
    /// THE SEAM GUARD — row 8 is not a tone anyone may forge with. It is a
    /// VERTICAL MIRROR of row 7, and it exists because the leaf sheet is
    /// minified through a LINEAR filter (render's coverage path: MAG point,
    /// MIN/MIP linear). Before the colour rows, row 7's bottom edge was the
    /// texture edge, so a bilinear fetch that straddled it CLAMPED and read
    /// row 7 twice. Append any row under it and that same fetch starts mixing
    /// in the new row — a pink fringe along the bottom texel of every conifer
    /// tile, at every mip, on trees nobody touched. Mirroring row 7 into row 8
    /// makes guard_top == row7_bottom exactly, so the blend returns row 7's
    /// own edge for any weight: the clamp is reproduced, not approximated, and
    /// it survives the mip chain because a box filter commutes with a mirror.
    SeamGuard = 8,
    // --- THE COLOUR ROWS (owner, 24.08.2026, verbatim: «добавим розовые
    // листья... также добавить красные, жёлтые, синие, фиолетовые необходимо
    // сразу»). One row per colour, ALL FIVE at once, because a passport that
    // can order only pink is a passport that gets edited five times.
    //
    // They are TONES, not shapes: a maple keeps the broadleaf silhouette and
    // changes row, exactly as an oak's three greens do. That is the whole
    // reason the atlas is SHAPE x COLOUR — colour costs no vertex byte and no
    // second mesh, so an autumn grove is the same trees under a different row.
    BlossomPink = 9,   ///< сакура / Гилдергрин — цветущая крона
    MapleRed = 10,     ///< осенний клён
    AutumnGold = 11,   ///< осенняя берёза/осина
    ArcaneBlue = 12,   ///< магическая (ночная) листва
    DuskViolet = 13,   ///< фиолетовая
    /// Rows 14-15 are PAID FOR ALREADY (the sheet is sixteen rows tall so the
    /// green band's v halves exactly) and are therefore left named and filled
    /// rather than blank: the next colour wave takes them without moving a
    /// single uv or re-baking a single .dfo. Until then they carry the green
    /// default, which is the cheapest thing that satisfies the atlas contract
    /// (the bark column is opaque in EVERY row).
    SpareA = 14,
    SpareB = 15,
};

/// The passport-facing NAME of a colour row, and its inverse. City passports
/// and recipe books are DATA (Rules 5-6); they must be able to say «pink»
/// without knowing that pink is row 8.
[[nodiscard]] const char* leaf_tone_name(LeafTone tone);
/// Resolves a passport word to a row. Returns false — and leaves `out`
/// untouched — for a word no row answers to, so a typo in a passport is a
/// loud refusal and not a silently green grove.
[[nodiscard]] bool leaf_tone_by_name(std::string_view name, LeafTone& out);

/// One generated atlas, ready for IRenderer::create_texture (RGBA8, row-major,
/// row 0 = top). `pixels.size() == width * height * 4`.
struct LeafAtlas {
    std::vector<uint8_t> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t tile_px = 0;
    uint32_t shapes = LEAF_ATLAS_SHAPES;
    uint32_t tones = LEAF_ATLAS_TONES;
};

/// Generates the mask atlas. `tile_px` is the side of one tile (>= 16).
///
/// ALPHA IS A GRADIENT AT THE RIM, OPAQUE IN THE BODY (Full HD pivot). The
/// backend builds alpha-weighted mips and resolves coverage through
/// alpha-to-coverage under MSAA (lead's contract, 15.08); the single-sample
/// path keeps a 0.5 alpha test as the control arm. The SHAPE boundary is
/// still the bitten, eroded outline — the gradient fades WITHIN the shape
/// toward that boundary, it does not smooth the boundary itself.
[[nodiscard]] LeafAtlas generate_leaf_atlas(uint32_t tile_px = LEAF_ATLAS_TILE_PX,
                                            FloraSeason season = FloraSeason::Summer);

/// The NORMAL sheet for the atlas (lead's 09f75eb aux_texture contract):
/// same tile layout as the colour atlas, tangent-space normals packed
/// xyz -> RGB, neutral (128,128,255) everywhere the colour sheet is
/// transparent AND on every non-bark column. Only the BarkPlate column
/// carries relief — it is derived from the SAME height field that shades the
/// colour tile, so the two sheets can never disagree about where a crack is.
[[nodiscard]] LeafAtlas generate_leaf_normal_atlas(
    uint32_t tile_px = LEAF_ATLAS_TILE_PX);

/// uv rectangle of one tile: (u_min, v_min, u_max, v_max), inset by half a
/// texel so a point sampler can never straddle two tiles.
[[nodiscard]] glm::vec4 leaf_tile_uv(LeafShape shape, LeafTone tone,
                                     uint32_t tile_px = LEAF_ATLAS_TILE_PX);

/// The tone's base colour for a season. Exposed so the value-ORDER invariant
/// (design §5.11: species value order must hold in EVERY season) is testable
/// without decoding pixels.
[[nodiscard]] glm::vec3 leaf_tone_color(LeafTone tone, FloraSeason season);

/// True when a species with this tone still carries foliage in this season.
/// Winter costs one boolean: deciduous cards are simply not emitted, and the
/// skeleton is generated regardless (LANDSCAPE §5.11).
[[nodiscard]] bool leaf_tone_has_foliage(LeafTone tone, FloraSeason season);

/// Everything one card needs. All of it is per CARD — nothing here is looked up
/// from a global, and nothing varies per vertex except position and the sway
/// weight derived from it.
struct LeafCardParams {
    glm::vec3 center{0.0f};          ///< card centre, model space (m)
    glm::vec3 normal{0.0f, 0.0f, 1.0f}; ///< card facing; FIXED, never camera-facing
    float half_width = 1.0f;         ///< m, across the card
    float half_height = 1.0f;        ///< m, up the card
    float roll = 0.0f;               ///< rad, rotation inside the card plane
    LeafShape shape = LeafShape::RoundLobed;
    LeafTone tone = LeafTone::OakMid;
    float value_jitter = 0.5f;       ///< 0..1 -> vertex BLUE, one value per card
    float phase = 0.0f;              ///< 0..1 -> vertex GREEN, per instance
    glm::vec3 sway_origin{0.0f};     ///< the attachment: sway weight is 0 here
    float sway_span = 1.0f;          ///< m at which the sway weight reaches 1
    uint32_t tile_px = LEAF_ATLAS_TILE_PX;
};

/// Emits one card: a quad, two triangles, six vertices, both faces the same
/// plane normal (fs_foliage flips it toward the viewer — a flat card has no
/// meaningful back face).
void emit_leaf_card(MeshData& m, const LeafCardParams& p);

} // namespace dfn::render
