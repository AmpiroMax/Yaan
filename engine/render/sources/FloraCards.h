/*
Created: 09:08:2026 - 20:21:13
Last updated: 16:08:2026 - 22:40:39
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
*/

#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
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
inline constexpr uint32_t LEAF_ATLAS_TONES = 8;
/// Bumped on every change to the tiles' ART (masks, packs, bark) — the disk
/// cache key must change when the pixels would, or the game paints with the
/// previous session's atlas (measured: the 4-column cache under 5-column uvs
/// painted the conifers white with birch tiles).
inline constexpr uint32_t LEAF_ATLAS_REVISION = 11;
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
enum class LeafTone : uint8_t {
    OakMid = 0,
    OakDeep = 1,
    OakSunlit = 2,
    BirchLight = 3,
    BirchPale = 4,
    WillowDark = 5,
    WillowOlive = 6,
    ConiferDark = 7,
};

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
