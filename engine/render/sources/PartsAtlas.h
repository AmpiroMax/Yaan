/*
Module: engine/render
File: engine/render/sources/PartsAtlas.h

Responsibility:
- THE BUILDING KIT'S OWN TEXTURE SHEET: a procedurally generated, deterministic
  RGBA8 atlas laid out as SURFACE x TONE, plus its normal sheet and the uv
  rectangle of one tile. What the leaf atlas is to flora, this is to the parts
  — the kit's albedo now comes from a texture instead of from a flat vertex
  colour.

Key items:
- PartSurface / PartTone: the columns and the rows.
- PartsAtlas, generate_parts_atlas(), generate_parts_normal_atlas().
- parts_tile_uv(), parts_tile_base(), PARTS_ATLAS_REVISION.

WHY ITS OWN SHEET AND NOT FLORA'S BARK COLUMN (lead's ruling, 17.08.2026). The
flora atlas' last column is BARK — tree colourways — and the kit needs hewn
timber, sawn board, end grain, stone, fired clay, plaster, thatch, turf. Bark on
a log wall is an accident that looks right; bark on a planed board is a lie. The
precedent for a zone generating its own sheet is flora's own (FloraCards
generates, RenderSystem only uploads), so this file generates and the lead binds.

WHY THE MASONRY BOND IS NOT IN HERE. The kit lays stone, brick and log walls as
COURSES OF BLOCKS — real geometry, with real shadow between the courses
(PartForgeWalls.cpp). A bond painted into a texture would be a second, flatter
copy of something the kit already tells the truth about, and the two would
disagree at every corner. The sheet carries what geometry cannot afford at
0.25 m: GRAIN, GRIT, TROWEL MARKS, STALKS — the millimetre scale.

Dependencies:
- Uses: ProcTexture.h (tileable_fbm / tileable_cells / tileable_cell_id — the
  project's tested periodic noise primitives; a second copy of a noise chain is
  Rule 39's shadow defect), glm, stdlib.
- Used by: PartForge (tile choice per material), RenderSystem/app (upload),
  tests/render/PartsAtlasTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/HOUSES.md.
- PURE AND DETERMINISTIC: same arguments -> byte-identical pixels. No IO, no
  clock, no globals.
- EVERY TILE IS OPAQUE (alpha 255). A part is a closed volume (Rule 52); there
  is no cutout anywhere on this sheet, and a transparent texel here would be a
  hole in a wall.
- THE TILE IS TORUS-PERIODIC except the two columns that are never repeated
  (EndGrain, Pane) — the mesh side maps with a plain wrap, and a field that
  does not wrap shows a seam line down every beam.
- PARTS_ATLAS_REVISION IS PART OF THE DISK/GPU CACHE KEY. Flora was burned
  twice by a cached sheet under new uvs (white conifers); the same trap is here.
*/

#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <vector>

namespace dfn::render {

/// ATLAS COLUMNS — what the surface IS. Nine, and each one earns its column by
/// a rhythm no other column has: timber runs in fibres, stone is granular,
/// thatch is stalks, plaster is a trowel. A material whose only difference is
/// COLOUR is a ROW here, never a column (fired brick and roof tile share the
/// clay column; plaster and daub share the trowel one).
enum class PartSurface : uint8_t {
    /// Тёсаный брус: axe facets across a fibre grain that runs along the piece.
    HewnTimber = 0,
    /// Пилёная доска и дранка: straighter, finer grain, saw ripple, knots.
    SawnBoard = 1,
    /// ТОРЕЦ: annual rings and radial checks. Its own column because a log
    /// house shows its cut ends at every corner tie, and side grain drawn on
    /// an end is the one wood mistake everybody sees.
    EndGrain = 2,
    /// Камень: grain, mica glint, lichen on the weathered row.
    Stone = 3,
    /// Обожжённая глина — кирпич и черепица: sand grain and firing pores.
    FiredClay = 4,
    /// Штукатурка и глина по каркасу: float sweeps and hairline cracks.
    Plaster = 5,
    /// Солома: stalks along the slope, tuft ends, wisps.
    Thatch = 6,
    /// Дёрн: blade-fine grass over dark earth.
    Turf = 7,
    /// ГЛУХОЕ ОКНО: the closed insert that imitates a view inside (HOUSES §2).
    /// Its own column so the imitation can carry a faint warm depth instead of
    /// being flat dark — a flat dark rectangle is what read as a hole.
    Pane = 8,
};

/// ATLAS ROWS — the tone AND the wear, together, because in this kit they are
/// one axis: weathering IS a tone change (grey down, moss in). The kit's
/// `wear` parameter picks the row; the per-face jitter it also used to carry
/// rides the vertex BLUE channel instead (the foliage program multiplies
/// albedo by 0.78..1.26 from it), so nothing that made a wall of boards differ
/// from itself is lost.
enum class PartTone : uint8_t {
    Light = 0,      ///< fresh and pale — new plaster, sapwood, straw
    Mid = 1,        ///< the kit's own default tone for the material
    Dark = 2,       ///< heartwood, tar, fired dark, shadowed clay
    Weathered = 3,  ///< greyed, lichened, mossed: the old building
};

inline constexpr uint32_t PARTS_ATLAS_SURFACES = 9;
inline constexpr uint32_t PARTS_ATLAS_TONES = 4;

/// Bumped on EVERY change to the pixels or the layout. It is part of the cache
/// key the uploader must use: flora lost a session to a cached 4-column sheet
/// being sampled by 5-column uvs (white conifers, correct code everywhere).
inline constexpr uint32_t PARTS_ATLAS_REVISION = 4; // 4: зерно на масштабе текселя (28.08)

/// Tile side in texels. 256 over PARTS_TILE_SPAN_M gives ~3.9 mm per texel ON
/// THE OBJECT — finer than the leaf atlas' ~5 mm, which is the density the
/// Full HD pivot accepted (FloraCards.h). A whole sheet is 2304x1024, i.e.
/// 9.4 MB — the kit is a texture the player stands 1 m from, but it is also
/// nine columns, and 512 would spend 38 MB per sheet to buy detail below the
/// grain pitch it is drawing.
inline constexpr uint32_t PARTS_ATLAS_TILE_PX = 256;

/// ДОЗА ЛИСТА (правило 47: обе руки замера выходят из одной сборки). `Flat` —
/// лист волны 3 БИТ-В-БИТ, включая размер плитки; `Grain` — структура на
/// масштабе текселя. Доза едет ПАРАМЕТРОМ, а не переменной среды, потому что
/// генератор обязан остаться чистым: две дозы должны печататься в одном
/// процессе, и тест на «доза 0 не сдвинулась ни на байт» иначе не написать.
enum class PartsSheetDose : uint8_t {
    Flat = 0,  ///< лист 21.08: плитка 256 px, только интерполированные поля
    Grain = 1, ///< волна 4: плитка 512 px плюс зерно на масштабе текселя
};

/// ПЛИТКА ДОЗЫ `Grain` — 512 px, и это НЕ «побольше значит получше», а
/// единственное число, при котором критерий вообще достижим. Замерено: экран
/// FullHD при `CAMERA_FOV_Y` несёт 703.5 пикселя на метр поверхности с одного
/// метра — дистанции, на которой К1 и меряется. Плитка 256 px на метровый шаг
/// даёт 256 текселей на метр, то есть УВЕЛИЧЕНИЕ в 2.75 раза, и билинейная
/// растяжка режет локальную детальность в 6.2 раза: даже белый шум амплитуды
/// 32/255 (ДЕТАЛЬ 24 в плитке — текстура, которую никто не назовёт материалом)
/// приходит на кадр как 3.88 при пороге 4.0. 512 px дают увеличение 1.37 и
/// потерю в 2.25 раза — достижимо честной структурой. ЦЕНА НАЗВАНА В ОТЧЁТЕ:
/// лист 4608x2048 (37.7 МБ) вместо 2304x1024 (9.4 МБ), два листа 75.5 МБ
/// вместо 18.8, и это CPU-память на время запуска (RenderSystem::parts_sheet
/// освобождает её в shutdown); на GPU уезжает 1 МБ на плитку вместо 0.25.
inline constexpr uint32_t PARTS_ATLAS_TILE_PX_GRAIN = 512;

/// Сторона плитки этой дозы. ОДНО определение на всех, потому что ключ кэша
/// плитки, вырезка из листа и uv обязаны согласиться о размере: разойдись они,
/// и лист прочитался бы со сдвигом на четверть, а выглядело бы это как чужая
/// раскладка (флора теряла на этом сессию — см. предупреждение про ревизию).
[[nodiscard]] inline constexpr uint32_t parts_tile_px(PartsSheetDose dose) {
    return dose == PartsSheetDose::Grain ? PARTS_ATLAS_TILE_PX_GRAIN
                                         : PARTS_ATLAS_TILE_PX;
}

/// METRES OF SURFACE ONE TILE COVERS. The mesh side repeats the tile by
/// SPLITTING faces at multiples of this, so the number is a trade with two
/// ends: smaller means finer texels and more triangles, larger means fewer
/// triangles and a coarser, more obviously repeating surface. 1 m puts a
/// board's grain repeat above the board's own width (0.30 m) — the repetition
/// never lands twice on one piece — and keeps a 4 m panel face at 4x4 cells.
inline constexpr float PARTS_TILE_SPAN_M = 1.0f;

/// One generated sheet, ready for IRenderer::create_texture (RGBA8, row-major,
/// row 0 = top). `pixels.size() == width * height * 4`.
struct PartsAtlas {
    std::vector<uint8_t> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t tile_px = 0;
    uint32_t surfaces = PARTS_ATLAS_SURFACES;
    uint32_t tones = PARTS_ATLAS_TONES;
};

/// The ALBEDO sheet. `tile_px` is the side of one tile (>= 16).
[[nodiscard]] PartsAtlas generate_parts_atlas(
    uint32_t tile_px = PARTS_ATLAS_TILE_PX,
    PartsSheetDose dose = PartsSheetDose::Flat);

/// The NORMAL sheet: same layout, tangent-space normals packed xyz -> RGB.
/// Derived from THE SAME height field that shades the albedo tile, so a groove
/// is dark exactly where the surface says it is deep — flora's contract, and
/// the reason a trunk stopped looking painted (FloraCards' aux sheet).
[[nodiscard]] PartsAtlas generate_parts_normal_atlas(
    uint32_t tile_px = PARTS_ATLAS_TILE_PX,
    PartsSheetDose dose = PartsSheetDose::Flat);

/// uv rectangle of one tile: (u_min, v_min, u_max, v_max), inset by half a
/// texel so a sampler can never straddle two tiles.
[[nodiscard]] glm::vec4 parts_tile_uv(PartSurface surface, PartTone tone,
                                      uint32_t tile_px = PARTS_ATLAS_TILE_PX);

/// The tile's MEAN albedo, exposed so the palette claim is testable without
/// decoding pixels: the kit's material colours are these values, and the suite
/// asserts that a generated tile still averages to its own row (a texture that
/// moves the palette would silently re-colour an accepted showcase).
[[nodiscard]] glm::vec3 parts_tile_base(PartSurface surface, PartTone tone);

/// True for the columns that are TORUS-PERIODIC, i.e. safe to repeat by a
/// plain wrap. False for EndGrain (rings have a centre; an end face is never
/// repeated) and Pane (one insert, one tile). Stated as a function because the
/// seam test must know which tiles it may hold to that standard — a criterion
/// applied where it cannot hold is Rule 48's failure, not a strictness.
[[nodiscard]] bool parts_tile_is_periodic(PartSurface surface);

// --- THE MESH SIDE OF THE CONTRACT ----------------------------------------
//
// A textured part writes into RegistryObject::bark, which the app already
// feeds to the foliage program (App.cpp: obj.bark goes into the `cards`
// stream, and into the triangle COLLISION body for kind == "part" — so
// texturing a part does not make it walk-through).
//
// ON THAT PROGRAM THE VERTEX COLOUR IS NOT ALBEDO, and this is the whole
// reason the layout below exists:
//   r = the atlas COLUMN index / 255   (was: wind sway weight)
//   g = the atlas ROW index / 255      (was: per-instance wind phase)
//   b = the per-face value jitter, the thing hewn_tone used to bake into the
//       albedo — the shader multiplies albedo by mix(0.78, 1.26, b)
//   a = sky visibility, 1.0 for everything built above ground
//
// AND THE UV IS RAW TILE SPACE, NOT AN ATLAS RECTANGLE: `u` is metres of
// surface divided by PARTS_TILE_SPAN_M, so a 4 m panel face carries u in
// 0..4. The tile it lands in comes from r/g, and the fragment wraps with
// fract(). WHY, in one line: the alternative is splitting every face at every
// tile boundary in the mesh, which measured ~2.3x the triangles and would add
// hundreds of megabytes of .dfo to a shared repository to say something a
// fract() already says. The tiles are torus-periodic by construction, so the
// wrap has no seam to hide.
//
// THE ONE THING THE SHEET NEEDS FROM THE RENDERER, and it is a single flag on
// the draw, not four: a part is a SOLID BUILT SURFACE, so for these draws the
// foliage program must (1) wrap uv per the above, (2) not apply wind, (3) not
// apply the edge-on card fade, (4) not apply leaf translucency. Items 2-4 are
// all leaf behaviour that a wall must never wear — a fading wall and a glowing
// stone are the two ways this can go wrong, and both are the same decision.

/// The vertex-BLUE value-jitter band of the foliage fragment program
/// (`albedo *= mix(0.78, 1.26, v_color0.b)`). TWO ZONES MUST AGREE ON THESE
/// (Rule 35): they are written in fs_foliage.sc and consumed here to encode
/// the jitter the kit used to bake into vertex colour. If render moves the
/// band, every baked part's shading moves with it — flagged to the lead.
inline constexpr float PARTS_VALUE_JITTER_LO = 0.78f;
inline constexpr float PARTS_VALUE_JITTER_HI = 1.26f;

/// Which tiles a piece of geometry wears. The CAP tiles are separate because
/// a sawn end is a different surface from the face it ends (PartSurface::
/// EndGrain) — on stone and clay the two are simply the same tile.
struct PartSkin {
    PartSurface side = PartSurface::HewnTimber;
    PartTone side_tone = PartTone::Mid;
    PartSurface end = PartSurface::EndGrain;
    PartTone end_tone = PartTone::Mid;
    /// Metres one tile covers. A field rather than the constant so a piece
    /// whose scale differs (a shingle, a straw bundle) can say so.
    float span_m = PARTS_TILE_SPAN_M;
    /// False = this piece keeps flat vertex-colour albedo on the "prop"
    /// program and stays in the wood stream. The untextured path must remain
    /// BIT-IDENTICAL, because it is the control arm for every claim about the
    /// textured one (Rule 47: both arms out of one binary).
    bool textured = false;
};

/// Packs one vertex's colour for the textured path: tile index in r/g, the
/// face's value jitter in b, sky visibility in a. `jitter_scale` is the
/// multiplier hewn_tone would have applied to the albedo (1.0 = no jitter).
[[nodiscard]] inline uint32_t parts_skin_color(PartSurface surface, PartTone tone,
                                               float jitter_scale) {
    const float b = (jitter_scale - PARTS_VALUE_JITTER_LO)
                  / (PARTS_VALUE_JITTER_HI - PARTS_VALUE_JITTER_LO);
    const auto byte = [](float v) {
        const float c = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        return static_cast<uint32_t>(c * 255.0f + 0.5f) & 0xFFu;
    };
    const uint32_t r = static_cast<uint32_t>(surface) & 0xFFu;
    const uint32_t g = static_cast<uint32_t>(tone) & 0xFFu;
    return 0xFF000000u | (byte(b) << 16) | (g << 8) | r; // 0xAABBGGRR
}

} // namespace dfn::render
