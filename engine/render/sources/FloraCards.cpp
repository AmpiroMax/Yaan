/*
Created: 09:08:2026 - 20:21:13
Last updated: 15:08:2026 - 16:02:49
Module: engine/render
File: engine/render/sources/FloraCards.cpp

Responsibility:
- Rasterises the procedural leaf mask atlas (SHAPE x COLOUR tiles) and emits the
  card quads that carry it, including the foliage vertex-colour contract.

Key items:
- generate_leaf_atlas, leaf_tile_uv, leaf_tone_color, leaf_tone_has_foliage,
  emit_leaf_card.

Dependencies:
- Uses: FloraCards.h, ProcMesh.h, glm.
- Used by: ProcFlora, RenderSystem, ProcFloraTests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md §3.8.
- THE MASK IS MOSTLY OPAQUE AND ITS HOLES SIT AT THE EDGES. That is measured,
  not stylistic: crown interiors in the reference photos are 79-86 % leaf and
  only the outer ~25 % of the radius reaches 16-28 % sky (§3.10). Even lace is
  WRONG twice over — it does not match the reference, and 99 % of reference
  branch widths fall under render's ~0.31 m mask-feature floor, so it would be
  invisible in the direct view AND alias in the shadow map.
- No image files anywhere (Q13). Everything here is generated from integer
  hashes; same arguments -> byte-identical output on every platform.
*/
/*
UPD:
- 09:08:2026 - 20:21:13: Created — mask atlas rasteriser + card emitter.
- 15:08:2026 - 00:45:20: ТАЙЛ СТАЛ ПАЧКОЙ (вердикт пользователя по галерее: «листва — всё ещё
  прямоугольники»; SpeedTree Cluster, исследование §1.1): вместо одного
  лопастного кома — 3-7 листовых масс вдоль оси-веточки с ТЁМНЫМ ПРУТОМ,
  видимым в окнах между ними, у каждой массы свой светлый-верх/тёмный-низ.
  Кромочная эрозия §3.10 сохранена, в кадре покадровой рамки блоба. NeedleFan
  наконец получил потребителя — хвойную лапу кузницы (7 узких масс).
- 15:08:2026 - 01:04:30: ХВОЙНОЕ ПЕРО (дословно: «у хвои листочки — иголочки... прозрачную
  текстуру мелкой ёлочки») — тайл NeedleFan рисуется напрямую: центральный
  прут, парные иголки-зубцы с прозрачностью между ними, укорачивающиеся к
  кончику; лиственные пачки — на массу больше при 128px.
- 15:08:2026 - 02:14:30: Тайлы коры (борозды зеркально-симметричные — треугольная развёртка трубы
  не встречает шва; берёста с чечевичками; мшистые ряды растят плёнку В БОРОЗДАХ —
  мох живёт где вода). Пачки листвы УКРУПНЕНЫ (масса 0.29→0.46, разброс шире):
  дамп атласа показал ~15% заполнения тайла — кроны были призрачными, потому что
  mip-альфа на дистанции стремилась к нулю. Перо хвои: гребёнка 0.07/скважность
  0.021 — первая правка слила иголки в сплошной клин (порог шире полушага).
- 15:08:2026 - 15:54:46: v6: хвойный тайл — ПЕРИСТЫЙ ФРОНД (стержень + 24 веточки с гребёнками иголок, градиентная альфа на кончиках; иголки короче полушага веточек — иначе клин, ловится gap/ragged-тестом); листовые пачки: градиентная кромка к той же изъеденной границе + маргин; калибровка по фотосканам (паспорта §1): хвоя олива {0.26 0.31 0.17}, кора сосны {0.27 0.22 0.16}.
- 15:08:2026 - 16:02:49: Кора двухслойная ОТ ПОЛЯ ВЫСОТ (паспорта §1: пластины F2-F1 с трещинами + зерно 1.5-3 см; берёста — бумага с ямками чечевичек): цвет затеняется полем, нормал-атлас дифференцирует ТО ЖЕ поле — трещина в цвете и в свете не может разойтись. Мох растёт в трещинах.
*/

#include "engine/render/sources/FloraCards.h"

#include "engine/render/sources/ProcMesh.h" // MeshData + pack (render's, shared)

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace dfn::render {

namespace {

// --- Mask tuning, all of it derived from §3.10's measurements ---------------
// Where the rim begins, as a fraction of the tile's own outline radius. Inside
// this the mask is essentially solid; outside it the outline erodes.
constexpr float RIM_START = 0.62f;
// Noise thresholds for the RIM EROSION: a texel is dropped when the field
// exceeds the threshold for its depth. This is what makes the outline lobed and
// bitten rather than a clean ellipse, and it is where the porosity budget goes,
// exactly as the photographs say (§3.10.1: porosity is a RIM phenomenon).
constexpr float HOLE_T_CORE = 0.930f;
constexpr float HOLE_T_RIM = 0.500f;
// Erosion lattice cells across the tile. 7 cells over 64 px puts one bite at
// ~9-18 px, i.e. ~0.4-0.9 m on a 3 m card — comfortably over render's ~0.31 m
// caster/feature floor. Raising this number is how you would accidentally build
// the lace this file exists to refuse.
constexpr int HOLE_LATTICE = 7;
constexpr int FORM_LATTICE = 4;
// See-through comes from the windows BETWEEN pack blobs and the erosion above;
// the old explicit-gap constant died with the one-mass tile.

uint32_t hash2(int x, int y, uint32_t seed) {
    uint32_t h = seed + 0x9E3779B9u;
    h ^= static_cast<uint32_t>(x) * 0x85EBCA6Bu;
    h = (h ^ (h >> 13)) * 0xC2B2AE35u;
    h ^= static_cast<uint32_t>(y) * 0x27D4EB2Fu;
    h = (h ^ (h >> 16)) * 0x165667B1u;
    return h ^ (h >> 15);
}

float hash01(int x, int y, uint32_t seed) {
    return static_cast<float>(hash2(x, y, seed) >> 8) / 16777216.0f;
}

float smooth5(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

/// Smooth value noise in [0,1] over a lattice of unit cells. Integer-hash only
/// (no trigonometric seeding) so it is bit-stable everywhere, per ProcTexture's
/// determinism discipline.
float value_noise(float x, float y, uint32_t seed) {
    const float fx = std::floor(x);
    const float fy = std::floor(y);
    const auto ix = static_cast<int>(fx);
    const auto iy = static_cast<int>(fy);
    const float tx = smooth5(x - fx);
    const float ty = smooth5(y - fy);
    const float a = hash01(ix, iy, seed);
    const float b = hash01(ix + 1, iy, seed);
    const float c = hash01(ix, iy + 1, seed);
    const float d = hash01(ix + 1, iy + 1, seed);
    return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}

/// One leaf shape's outline: an ellipse whose radius is scalloped by two
/// harmonics. The lobes ARE the porosity budget — a lobe notch is a big edge
/// feature, which is the only kind of feature that survives both the render
/// resolution and the shadow map.
struct ShapeDef {
    float ax, ay;       ///< ellipse half-axes in tile space
    float rot;          ///< rad, rotation of the whole shape
    int n1;             ///< primary lobe count
    float d1;           ///< primary lobe depth (fraction of radius)
    int n2;             ///< secondary harmonic (breaks the symmetry)
    float d2;
    float p1, p2;       ///< harmonic phases
    float taper;        ///< 0 = none, 1 = fully wedge-shaped toward the bottom
    uint32_t seed;
};

const ShapeDef& shape_def(LeafShape s) {
    static const std::array<ShapeDef, LEAF_ATLAS_SHAPES> defs{{
        // RoundLobed — the broadleaf default. Deepened after the colossus
        // inspection (user: «не делать вообще квадратами... линии под разными
        // углами, погугли картинки листьев, особенно дуба»): an oak leaf is
        // round LOBES with deep sinuses — lobe depth up 0.20 -> 0.34, and the
        // second harmonic up so no two lobes repeat at the same angle.
        {0.97f, 0.90f, 0.00f, 7, 0.34f, 13, 0.14f, 0.0f, 1.7f, 0.00f, 101u},
        // OvalSpray — narrow and leaning: rim fill and clump crowns.
        {0.64f, 0.98f, 0.38f, 5, 0.23f, 9, 0.10f, 0.9f, 2.4f, 0.20f, 211u},
        // RaggedTip — a wedge for branch tips and the crown top.
        {0.78f, 0.98f, -0.16f, 4, 0.26f, 9, 0.12f, 2.0f, 0.4f, 0.55f, 331u},
        // NeedleFan — flatter and spikier (conifer; pine still uses cone tiers,
        // so this column is generated but not yet placed).
        {1.00f, 0.66f, 0.10f, 9, 0.28f, 17, 0.11f, 0.5f, 1.1f, 0.15f, 457u},
        // BarkPlate — parameters unused (the bark rasteriser is its own path);
        // the seed feeds its noise.
        {1.00f, 1.00f, 0.00f, 1, 0.00f, 1, 0.00f, 0.0f, 0.0f, 0.00f, 601u},
    }};
    return defs[static_cast<size_t>(s) % LEAF_ATLAS_SHAPES];
}

/// Seasonal tone table. The ORDER of species values must hold in every season
/// (design §5.11's acceptance test): birch brightest, oak mid, willow and
/// conifer dark. Colours are look-dev art values, NOT calibrated from the
/// reference photographs — the same tree in two frames gave leaf/dark splits of
/// 76/10 and 53/40 purely on exposure (§3.10.4), so a photo tells us about
/// structure and never about hue.
glm::vec3 tone_summer(LeafTone t) {
    switch (t) {
    case LeafTone::OakMid: return {0.30f, 0.42f, 0.18f};
    case LeafTone::OakDeep: return {0.19f, 0.30f, 0.13f};
    case LeafTone::OakSunlit: return {0.42f, 0.53f, 0.20f};
    case LeafTone::BirchLight: return {0.52f, 0.61f, 0.27f};
    case LeafTone::BirchPale: return {0.62f, 0.68f, 0.34f};
    case LeafTone::WillowDark: return {0.16f, 0.27f, 0.19f};
    case LeafTone::WillowOlive: return {0.24f, 0.34f, 0.18f};
    // Calibrated against the fir photoscan's needle sheet (passports §1):
    // live needles are warm OLIVE (H≈0.18, V-median 0.38), not blue-green
    // murk — the old {0.12 0.22 0.19} was both too dark and too cold. Value
    // order vs oak (the §5.11 invariant) still holds: L=0.28 vs oak 0.36.
    case LeafTone::ConiferDark: default: return {0.26f, 0.31f, 0.17f};
    }
}

glm::vec3 tone_autumn(LeafTone t) {
    switch (t) {
    case LeafTone::OakMid: return {0.47f, 0.31f, 0.10f};
    case LeafTone::OakDeep: return {0.30f, 0.18f, 0.07f};
    case LeafTone::OakSunlit: return {0.60f, 0.41f, 0.13f};
    case LeafTone::BirchLight: return {0.74f, 0.61f, 0.20f};
    case LeafTone::BirchPale: return {0.82f, 0.71f, 0.31f};
    case LeafTone::WillowDark: return {0.24f, 0.21f, 0.11f};
    case LeafTone::WillowOlive: return {0.34f, 0.29f, 0.13f};
    // Measured: conifers stay green in all three autumn reference frames, so
    // the pine's only seasonal delta is snow — which is render's and core's.
    case LeafTone::ConiferDark: default: return {0.26f, 0.31f, 0.17f};
    }
}

glm::vec3 tone_winter(LeafTone t) {
    // Deciduous rows are unused in winter (leaf_tone_has_foliage is false and
    // the generator emits no cards), but they are defined rather than left
    // undefined so a future "late autumn" or a snow-dusted variant has
    // somewhere to start.
    switch (t) {
    case LeafTone::OakMid: return {0.31f, 0.26f, 0.18f};
    case LeafTone::OakDeep: return {0.21f, 0.17f, 0.12f};
    case LeafTone::OakSunlit: return {0.40f, 0.34f, 0.24f};
    case LeafTone::BirchLight: return {0.55f, 0.50f, 0.40f};
    case LeafTone::BirchPale: return {0.64f, 0.59f, 0.48f};
    case LeafTone::WillowDark: return {0.18f, 0.17f, 0.13f};
    case LeafTone::WillowOlive: return {0.26f, 0.23f, 0.17f};
    case LeafTone::ConiferDark: default: return {0.20f, 0.26f, 0.16f};
    }
}

// --- BARK: one HEIGHT FIELD drives both sheets --------------------------------
// The colour tile shades from this field and the normal tile differentiates
// it, so a crack is dark exactly where the light says it is deep. Coordinates
// are the MIRRORED tile coords (the tube mapping's triangle wave never meets
// a seam). Passports §1: real bark is TWO-LAYER — fine grain (1.5-3 cm) under
// large plates split by deep cracks; birch is smooth paper with lenticel dips.
struct BarkStyle {
    glm::vec3 base;    ///< colourway albedo
    float moss;        ///< 0..1 moss film budget (grows in the cracks)
    bool birch;        ///< paper-with-lenticels instead of plates
    float plate_nx;    ///< plate cells across the tile
    float plate_ny;    ///< ...and along it (fewer = taller plates)
    float plate_depth; ///< how deep the cracks cut, in height units
};

BarkStyle bark_style(LeafTone row) {
    switch (row) {
    case LeafTone::OakMid:    return {{0.32f, 0.24f, 0.16f}, 0.0f, false, 7.0f, 2.2f, 0.55f};
    case LeafTone::OakDeep:   return {{0.30f, 0.23f, 0.15f}, 0.55f, false, 7.0f, 2.2f, 0.55f};
    case LeafTone::OakSunlit: return {{0.36f, 0.27f, 0.18f}, 0.9f, false, 6.0f, 2.0f, 0.50f};
    case LeafTone::BirchLight: return {{0.86f, 0.85f, 0.80f}, 0.0f, true, 0.0f, 0.0f, 0.0f};
    case LeafTone::BirchPale: return {{0.78f, 0.77f, 0.70f}, 0.0f, true, 0.0f, 0.0f, 0.0f};
    case LeafTone::WillowDark: return {{0.38f, 0.36f, 0.33f}, 0.0f, false, 5.0f, 1.6f, 0.35f};
    case LeafTone::WillowOlive: return {{0.44f, 0.40f, 0.34f}, 0.35f, false, 5.0f, 1.6f, 0.35f};
    // Pine: rounded plates in a dark desaturated brown (scan-calibrated,
    // passports §1 — V-median 0.24, warm hue, S≈0.3).
    case LeafTone::ConiferDark: default:
        return {{0.27f, 0.22f, 0.16f}, 0.0f, false, 5.0f, 3.0f, 0.60f};
    }
}

/// Height in [0,1]: plate body high, cracks low, grain everywhere.
float bark_height(float mx, float my, const BarkStyle& st, uint32_t seed) {
    // The fine grain layer (1.5-2.8 cm at trunk scale — the scan's own pitch).
    float h = 0.22f * (value_noise(mx * 9.0f, my * 1.6f, 811u + seed) * 0.65f
                       + value_noise(mx * 21.0f, my * 4.0f, 977u + seed) * 0.35f);
    if (st.birch) {
        // Paper: nearly flat, with shallow horizontal lenticel dips.
        const float lent = value_noise(mx * 2.5f, my * 17.0f, 449u);
        h += 0.55f - (lent > 0.72f ? 0.35f : 0.0f);
        return std::clamp(h, 0.0f, 1.0f);
    }
    // The plate layer: jittered cells; the height falls into the crack where
    // two nearest features come close (F2-F1), and each plate sits at its own
    // level so neighbouring plates never merge visually.
    const float gx = mx * st.plate_nx;
    const float gy = my * st.plate_ny;
    const int cx = static_cast<int>(std::floor(gx));
    const int cy = static_cast<int>(std::floor(gy));
    float d1 = 9.0f, d2 = 9.0f;
    int best_cx = cx, best_cy = cy;
    for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
            const int px = cx + ox;
            const int py = cy + oy;
            const float fx = static_cast<float>(px) + 0.5f
                           + (hash01(px, py, seed ^ 0x51u) - 0.5f) * 0.8f;
            const float fy = static_cast<float>(py) + 0.5f
                           + (hash01(px, py, seed ^ 0x77u) - 0.5f) * 0.8f;
            const float dx = (gx - fx);
            const float dy = (gy - fy) * 1.15f; // cracks bias vertical
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d < d1) {
                d2 = d1; d1 = d; best_cx = px; best_cy = py;
            } else if (d < d2) {
                d2 = d;
            }
        }
    }
    const float crack = std::clamp((d2 - d1) * 2.6f, 0.0f, 1.0f);
    const float plate_level = 0.55f + 0.35f * hash01(best_cx, best_cy, seed ^ 0x9Bu);
    h += st.plate_depth * (smooth5(crack) * 0.8f + 0.2f) * plate_level;
    return std::clamp(h, 0.0f, 1.0f);
}

} // namespace

glm::vec3 leaf_tone_color(LeafTone tone, FloraSeason season) {
    switch (season) {
    case FloraSeason::Autumn: return tone_autumn(tone);
    case FloraSeason::Winter: return tone_winter(tone);
    case FloraSeason::Summer: default: return tone_summer(tone);
    }
}

bool leaf_tone_has_foliage(LeafTone tone, FloraSeason season) {
    if (season != FloraSeason::Winter) {
        return true;
    }
    // Winter costs ONE boolean (LANDSCAPE §5.11): deciduous cards are not
    // emitted and the bare skeleton — which is generated regardless — becomes
    // the tree. Conifers keep their needles.
    return tone == LeafTone::ConiferDark;
}

glm::vec4 leaf_tile_uv(LeafShape shape, LeafTone tone, uint32_t tile_px) {
    const uint32_t px = std::max<uint32_t>(tile_px, 4);
    const float w = static_cast<float>(LEAF_ATLAS_SHAPES * px);
    const float h = static_cast<float>(LEAF_ATLAS_TONES * px);
    const auto sx = static_cast<float>(static_cast<uint32_t>(shape) % LEAF_ATLAS_SHAPES);
    const auto ty = static_cast<float>(static_cast<uint32_t>(tone) % LEAF_ATLAS_TONES);
    // Half-texel inset: the sampler is point-sampled, so a uv landing exactly
    // on a tile boundary could fetch the neighbouring tile's edge column.
    const float ix = 0.5f / w;
    const float iy = 0.5f / h;
    const float u0 = sx * static_cast<float>(px) / w + ix;
    const float u1 = (sx + 1.0f) * static_cast<float>(px) / w - ix;
    const float v0 = ty * static_cast<float>(px) / h + iy;
    const float v1 = (ty + 1.0f) * static_cast<float>(px) / h - iy;
    return {u0, v0, u1, v1};
}

LeafAtlas generate_leaf_atlas(uint32_t tile_px, FloraSeason season) {
    LeafAtlas atlas;
    atlas.tile_px = std::max<uint32_t>(tile_px, 16);
    atlas.shapes = LEAF_ATLAS_SHAPES;
    atlas.tones = LEAF_ATLAS_TONES;
    atlas.width = atlas.tile_px * LEAF_ATLAS_SHAPES;
    atlas.height = atlas.tile_px * LEAF_ATLAS_TONES;
    atlas.pixels.assign(static_cast<size_t>(atlas.width) * atlas.height * 4u, 0u);

    const auto n = static_cast<float>(atlas.tile_px);
    for (uint32_t tone_i = 0; tone_i < LEAF_ATLAS_TONES; ++tone_i) {
        const glm::vec3 base =
            leaf_tone_color(static_cast<LeafTone>(tone_i), season);
        for (uint32_t shape_i = 0; shape_i < LEAF_ATLAS_SHAPES; ++shape_i) {
            const ShapeDef& sd = shape_def(static_cast<LeafShape>(shape_i));
            const float cs = std::cos(sd.rot);
            const float sn = std::sin(sd.rot);

            // --- THE PACK (user ruling on the forge gallery: «листва — всё ещё
            // прямоугольники»; SpeedTree's Cluster doctrine, research §1.1): a
            // tile is not ONE leaf mass, it is a BRANCH WITH SEVERAL, laid
            // along a diagonal axis with a visible dark twig under them. The
            // blob count and sizes come from the shape row, so the four shapes
            // stay four different silhouettes rather than one pack recolored.
            struct PackBlob {
                glm::vec2 c;   ///< centre, tile space
                float r;       ///< base radius before lobing
                uint32_t seed;
            };
            const bool needle = static_cast<LeafShape>(shape_i) == LeafShape::NeedleFan;
            // More masses per pack since the 128 px tile can resolve them —
            // finer detail is drawn, not dithered (the user's «более точно
            // рисовать»).
            const int blob_count = needle ? 7 : ((sd.ax * sd.ay > 0.8f) ? 6 : 5);
            PackBlob blobs[8];
            const glm::vec2 axis{cs, sn};
            for (int bi = 0; bi < blob_count; ++bi) {
                const float t = -0.62f + 1.35f * (static_cast<float>(bi) + 0.5f)
                                             / static_cast<float>(blob_count);
                // Off-axis scatter: leaves clump on SIDES of a twig, not on it.
                const float off = (hash01(bi + 3, 7, sd.seed) - 0.5f)
                                * (needle ? 0.24f : 0.62f);
                blobs[bi].c = axis * (t * 0.92f)
                            + glm::vec2{-axis.y, axis.x} * off;
                // Outer blobs shrink — the tip of a spray is its youngest wood.
                const float shrink = 1.0f - 0.45f * std::fabs(t + 0.1f);
                blobs[bi].r = (needle ? 0.20f : 0.46f) * sd.ax * shrink
                            * (0.8f + 0.4f * hash01(bi + 9, 13, sd.seed));
                // THE TILE MARGIN (user: «полоски по краям листвы, словно
                // полигон недорезали»): a blob clipped by the tile border
                // rasterises as a dead-straight cut. Every mass dies out
                // before the margin band by construction.
                const float head_room = 1.0f - 2.0f * LEAF_TILE_MARGIN
                    - std::max(std::fabs(blobs[bi].c.x), std::fabs(blobs[bi].c.y));
                blobs[bi].r = std::clamp(blobs[bi].r, 0.05f, std::max(head_room, 0.05f));
                blobs[bi].seed = sd.seed + static_cast<uint32_t>(bi) * 977u;
            }

            for (uint32_t py = 0; py < atlas.tile_px; ++py) {
                for (uint32_t px = 0; px < atlas.tile_px; ++px) {
                    const float x = (static_cast<float>(px) + 0.5f) / n * 2.0f - 1.0f;
                    const float y = 1.0f - (static_cast<float>(py) + 0.5f) / n * 2.0f;

                    // --- BARK TILES (user: «нужны текстуры и учёт
                    // освещённости на них; мох — просто зеленушка... надо
                    // текстурами рисовать»): a fully OPAQUE tile per tone row,
                    // each row its own colourway. Vertical furrows: ridge/
                    // groove value noise stretched tall, MIRROR-SYMMETRIC in
                    // both axes so the tube mapping's triangle-wave wrap never
                    // meets a seam. Moss rows grow their film in the grooves
                    // first — moss lives where water does.
                    if (static_cast<LeafShape>(shape_i) == LeafShape::BarkPlate) {
                        const size_t ob = (static_cast<size_t>(tone_i * atlas.tile_px + py)
                                           * atlas.width + shape_i * atlas.tile_px + px) * 4u;
                        // Mirrored coordinates: the tile is its own reflection,
                        // so mirror-repeat mapping is seamless by construction.
                        const float mx = 1.0f - std::fabs(x);
                        const float my = 1.0f - std::fabs(y);
                        // TWO-LAYER HEIGHT FIELD (passports §1): plates split
                        // by cracks over fine grain. The colour below and the
                        // normal sheet both read THIS field — they cannot
                        // disagree about where a crack runs.
                        const BarkStyle st = bark_style(static_cast<LeafTone>(tone_i));
                        const float h = bark_height(mx, my, st, sd.seed);
                        // Shade FROM the height: plate tops lit, cracks dark.
                        const float shade = st.birch ? 0.30f + 0.95f * h
                                                     : 0.34f + 0.88f * h;
                        glm::vec3 c = st.base * shade;
                        if (st.moss > 0.0f) {
                            // Moss film: grows in the cracks, patchy.
                            const float patch = value_noise(mx * 5.0f, my * 5.0f, 733u);
                            const float moss = st.moss * (1.0f - h)
                                             * std::clamp(patch * 1.6f - 0.3f, 0.0f, 1.0f);
                            c = c * (1.0f - moss)
                              + glm::vec3{0.20f, 0.33f, 0.12f} * (moss * (0.3f + 0.9f * h));
                        }
                        c = glm::clamp(c, glm::vec3{0.0f}, glm::vec3{1.0f});
                        atlas.pixels[ob + 0] = static_cast<uint8_t>(c.r * 255.0f + 0.5f);
                        atlas.pixels[ob + 1] = static_cast<uint8_t>(c.g * 255.0f + 0.5f);
                        atlas.pixels[ob + 2] = static_cast<uint8_t>(c.b * 255.0f + 0.5f);
                        atlas.pixels[ob + 3] = 255u; // OPAQUE: bark, not cutout
                        continue;
                    }

                    // --- THE FROND SHEET (passports §2.1; the fir photoscan's
                    // twig material): a conifer tile is not a twig with barbs —
                    // it is a PINNATE BRANCH. A central stem, side branchlets
                    // leaving it alternately, each branchlet carrying its own
                    // needle comb; needle tips fade through GRADIENT alpha
                    // (the backend resolves it via alpha-to-coverage, lead's
                    // Full HD contract 15.08).
                    if (needle) {
                        const float along = x * axis.x + y * axis.y;   // -1..1
                        const float across = -x * axis.y + y * axis.x; // signed
                        const size_t on = (static_cast<size_t>(tone_i * atlas.tile_px + py)
                                           * atlas.width + shape_i * atlas.tile_px + px) * 4u;
                        const float lim = 1.0f - 2.0f * LEAF_TILE_MARGIN;
                        float alpha = 0.0f;
                        float lit = 0.0f;
                        // The stem: butt to tip, tapering.
                        const float stem_t = (along + 0.82f) / 1.68f; // 0 butt, 1 tip
                        if (along > -0.82f && along < 0.86f && stem_t >= 0.0f) {
                            const float w = 0.024f * (1.0f - 0.55f * stem_t);
                            if (std::fabs(across) < w) {
                                alpha = 1.0f;
                                lit = 0.42f; // bare wood, darker than needles
                            }
                        }
                        // Branchlets, alternating sides, shorter toward the tip
                        // and RAKED toward it — the fir frame's own geometry.
                        constexpr int PINNAE = 12;
                        for (int side = -1; side <= 1 && alpha < 1.0f; side += 2) {
                            for (int pi = 0; pi < PINNAE; ++pi) {
                                const float t0 = -0.76f
                                    + 1.55f * (static_cast<float>(pi)
                                               + (side > 0 ? 0.5f : 0.0f))
                                          / static_cast<float>(PINNAE);
                                if (t0 > 0.80f) continue;
                                const float tip01 = (t0 + 0.76f) / 1.55f;
                                const float len = 0.42f * (1.0f - 0.72f * tip01)
                                    * (0.75f + 0.5f * hash01(pi, side + 7, sd.seed));
                                if (len < 0.05f) continue;
                                // Branchlet frame: u along it, v across.
                                const float rake = 0.42f + 0.18f * tip01;
                                float bu_x = rake, bu_y = static_cast<float>(side) * (1.0f - rake);
                                const float bl = std::sqrt(bu_x * bu_x + bu_y * bu_y);
                                bu_x /= bl; bu_y /= bl;
                                const float dx = along - t0;
                                const float dy = across;
                                const float u = dx * bu_x + dy * bu_y;
                                const float v = -dx * bu_y + dy * bu_x;
                                if (u < 0.0f || u > len) continue;
                                const float u01 = u / len;
                                // Needle length: longest mid-branchlet. Kept
                                // UNDER half the branchlet spacing (0.129), or
                                // neighbouring branchlets merge into a solid
                                // wedge and the frond stops being pinnate
                                // (caught by the gap/ragged suite on sight).
                                const float nl = 0.058f * (1.0f - 0.5f * u01)
                                               * (0.7f + 0.6f * (1.0f - std::fabs(u01 - 0.35f)));
                                const float av = std::fabs(v);
                                if (av > nl && av > 0.012f) continue;
                                // The comb: needles PITCH apart along the
                                // branchlet, a few missing; sky between them.
                                constexpr float PITCH = 0.030f;
                                const float ph = u / PITCH;
                                const int ni = static_cast<int>(std::round(ph));
                                if (hash01(ni, pi * 2 + (side > 0 ? 1 : 0), sd.seed) < 0.10f
                                    && av > 0.012f) continue;
                                const float comb = std::fabs(ph - std::round(ph)) * PITCH
                                                 + av * 0.16f; // needles rake tipward
                                float a_here = 0.0f;
                                if (av < 0.012f && u01 < 0.9f) {
                                    a_here = 1.0f; // the branchlet spine itself
                                } else if (comb < 0.0055f) {
                                    // Gradient at the needle's own tip.
                                    a_here = std::clamp((nl - av) / (0.30f * nl + 1e-4f),
                                                        0.0f, 1.0f);
                                }
                                if (a_here <= alpha) continue;
                                alpha = a_here;
                                // Needles read lighter than wood; outer half of
                                // the frond catches more sky.
                                lit = 0.78f + 0.45f * std::clamp(v * 3.0f + 0.4f, 0.0f, 1.0f)
                                    - 0.22f * tip01;
                            }
                        }
                        if (alpha <= 0.0f) continue;
                        // The margin band kills everything near the border.
                        const float border = std::min(1.0f - std::fabs(x), 1.0f - std::fabs(y));
                        alpha *= std::clamp((border - (1.0f - lim)) / (1.0f - lim), 0.0f, 1.0f);
                        if (alpha <= 0.004f) continue;
                        const glm::vec3 c = glm::clamp(base * lit, glm::vec3{0.0f}, glm::vec3{1.0f});
                        atlas.pixels[on + 0] = static_cast<uint8_t>(c.r * 255.0f + 0.5f);
                        atlas.pixels[on + 1] = static_cast<uint8_t>(c.g * 255.0f + 0.5f);
                        atlas.pixels[on + 2] = static_cast<uint8_t>(c.b * 255.0f + 0.5f);
                        atlas.pixels[on + 3] = static_cast<uint8_t>(alpha * 255.0f + 0.5f);
                        continue;
                    }

                    // Nearest pack blob, in each blob's own lobed metric.
                    float best = 1e9f;      // r / outline of the best blob
                    float best_local_y = 0.0f;
                    uint32_t best_seed = sd.seed;
                    for (int bi = 0; bi < blob_count; ++bi) {
                        const float bx = (x - blobs[bi].c.x) / blobs[bi].r;
                        const float by = (y - blobs[bi].c.y) / blobs[bi].r
                                       / (needle ? 0.62f : 0.88f);
                        const float r = std::sqrt(bx * bx + by * by);
                        const float th = std::atan2(by, bx);
                        float outline =
                            1.0f - sd.d1 * (0.5f + 0.5f * std::cos(static_cast<float>(sd.n1) * th + sd.p1))
                                 - sd.d2 * (0.5f + 0.5f * std::cos(static_cast<float>(sd.n2) * th + sd.p2));
                        outline = std::max(outline, 0.2f);
                        const float d = r / outline;
                        if (d < best) {
                            best = d;
                            best_local_y = by;
                            best_seed = blobs[bi].seed;
                        }
                    }

                    // The TWIG: a thin dark line along the pack axis, visible
                    // in the windows between blobs — the internal structure a
                    // cluster texture carries (§1.1: leaves WITH their twig).
                    const float along = x * axis.x + y * axis.y;
                    const float perp = std::fabs(-x * axis.y + y * axis.x
                                                 - 0.02f * std::sin(along * 9.0f));
                    const bool on_twig = !needle
                        && perp < 0.035f * (1.0f - 0.5f * std::fabs(along))
                        && along > -0.68f && along < 0.55f;

                    const size_t o =
                        (static_cast<size_t>(tone_i * atlas.tile_px + py) * atlas.width
                         + shape_i * atlas.tile_px + px) * 4u;

                    if (best >= 1.0f) {
                        if (on_twig) { // bare twig between the leaf masses
                            const glm::vec3 tw = base * 0.30f;
                            atlas.pixels[o + 0] = static_cast<uint8_t>(tw.r * 255.0f + 0.5f);
                            atlas.pixels[o + 1] = static_cast<uint8_t>(tw.g * 255.0f + 0.5f);
                            atlas.pixels[o + 2] = static_cast<uint8_t>(tw.b * 255.0f + 0.5f);
                            atlas.pixels[o + 3] = 255u;
                        }
                        continue; // outside every blob: sky through the pack
                    }

                    // Edge-concentrated erosion per blob — the measured §3.10
                    // porosity profile, unchanged in spirit from the one-mass
                    // tile, applied in the blob's own frame.
                    const float rim = std::clamp((best - RIM_START) / (1.0f - RIM_START),
                                                 0.0f, 1.0f);
                    const float threshold =
                        HOLE_T_CORE + (HOLE_T_RIM - HOLE_T_CORE) * smooth5(rim);
                    const float hole = value_noise(
                        (x + 1.0f) * 0.5f * static_cast<float>(HOLE_LATTICE),
                        (y + 1.0f) * 0.5f * static_cast<float>(HOLE_LATTICE), best_seed);
                    if (hole > threshold) {
                        continue;
                    }

                    // Baked form, now PER BLOB: each mass carries its own
                    // lit-top / dark-under ramp, so the pack reads as several
                    // clumps under one light instead of one flat sticker.
                    const float form = value_noise(
                        (x + 1.0f) * 0.5f * static_cast<float>(FORM_LATTICE) + 11.0f,
                        (y + 1.0f) * 0.5f * static_cast<float>(FORM_LATTICE) + 7.0f,
                        best_seed ^ 0x5BD1u);
                    const float k = std::clamp(0.38f * (0.5f + 0.5f * best_local_y)
                                                   + 0.30f * best + 0.32f * form,
                                               0.0f, 0.999f);
                    static constexpr float SHADES[3] = {0.72f, 0.94f, 1.18f};
                    const float shade = SHADES[static_cast<int>(k * 3.0f)];
                    const glm::vec3 c = glm::clamp(base * shade, glm::vec3{0.0f},
                                                   glm::vec3{1.0f});
                    // GRADIENT RIM (Full HD contract): alpha fades over the
                    // outer quarter of the blob toward the SAME bitten
                    // boundary the erosion draws — the shape stays ragged,
                    // only its edge texels soften for alpha-to-coverage.
                    float alpha = 0.35f + 0.65f
                        * std::clamp((1.0f - best) / (1.0f - RIM_START) * 2.0f,
                                     0.0f, 1.0f);
                    const float border = std::min(1.0f - std::fabs(x),
                                                  1.0f - std::fabs(y));
                    alpha *= std::clamp((border - 2.0f * LEAF_TILE_MARGIN)
                                            / (2.0f * LEAF_TILE_MARGIN),
                                        0.0f, 1.0f);
                    if (alpha <= 0.004f) {
                        continue;
                    }
                    atlas.pixels[o + 0] = static_cast<uint8_t>(c.r * 255.0f + 0.5f);
                    atlas.pixels[o + 1] = static_cast<uint8_t>(c.g * 255.0f + 0.5f);
                    atlas.pixels[o + 2] = static_cast<uint8_t>(c.b * 255.0f + 0.5f);
                    atlas.pixels[o + 3] = static_cast<uint8_t>(alpha * 255.0f + 0.5f);
                }
            }
        }
    }
    return atlas;
}

LeafAtlas generate_leaf_normal_atlas(uint32_t tile_px) {
    LeafAtlas atlas;
    atlas.tile_px = std::max<uint32_t>(tile_px, 16);
    atlas.shapes = LEAF_ATLAS_SHAPES;
    atlas.tones = LEAF_ATLAS_TONES;
    atlas.width = atlas.tile_px * LEAF_ATLAS_SHAPES;
    atlas.height = atlas.tile_px * LEAF_ATLAS_TONES;
    // NEUTRAL EVERYWHERE FIRST (lead's contract: transparent texels and
    // non-bark columns are the flat normal, so "no relief" is a value, not a
    // branch in the shader).
    atlas.pixels.assign(static_cast<size_t>(atlas.width) * atlas.height * 4u, 0u);
    for (size_t i = 0; i < atlas.pixels.size(); i += 4) {
        atlas.pixels[i + 0] = 128u;
        atlas.pixels[i + 1] = 128u;
        atlas.pixels[i + 2] = 255u;
        atlas.pixels[i + 3] = 255u;
    }

    const auto n = static_cast<float>(atlas.tile_px);
    const uint32_t bark_i = LEAF_ATLAS_SHAPES - 1; // BarkPlate column
    const ShapeDef& sd = shape_def(LeafShape::BarkPlate);
    // RELIEF_SCALE converts the [0,1] height field into a slope. 1.6 puts the
    // crack walls near 60 deg at 512 px — bark, not corrugated iron; if the
    // light reads inverted in the frame, flip the sign HERE, not in the
    // shader (one producer, one convention).
    constexpr float RELIEF_SCALE = 1.6f;
    for (uint32_t tone_i = 0; tone_i < LEAF_ATLAS_TONES; ++tone_i) {
        const BarkStyle st = bark_style(static_cast<LeafTone>(tone_i));
        for (uint32_t py = 0; py < atlas.tile_px; ++py) {
            for (uint32_t px = 0; px < atlas.tile_px; ++px) {
                const float x = (static_cast<float>(px) + 0.5f) / n * 2.0f - 1.0f;
                const float y = 1.0f - (static_cast<float>(py) + 0.5f) / n * 2.0f;
                const float e = 2.0f / n; // one texel, tile space
                // Central differences THROUGH the mirror transform, so the
                // normals fold seamlessly exactly where the colour does.
                const auto h_at = [&](float sx, float sy) {
                    return bark_height(1.0f - std::fabs(sx), 1.0f - std::fabs(sy),
                                       st, sd.seed);
                };
                const float dhdx = (h_at(x + e, y) - h_at(x - e, y)) / (2.0f * e);
                const float dhdy = (h_at(x, y + e) - h_at(x, y - e)) / (2.0f * e);
                glm::vec3 nrm{-dhdx * RELIEF_SCALE, -dhdy * RELIEF_SCALE, 1.0f};
                const float len = std::sqrt(nrm.x * nrm.x + nrm.y * nrm.y + nrm.z * nrm.z);
                nrm /= len;
                const size_t o = (static_cast<size_t>(tone_i * atlas.tile_px + py)
                                  * atlas.width + bark_i * atlas.tile_px + px) * 4u;
                atlas.pixels[o + 0] = static_cast<uint8_t>((nrm.x * 0.5f + 0.5f) * 255.0f + 0.5f);
                atlas.pixels[o + 1] = static_cast<uint8_t>((nrm.y * 0.5f + 0.5f) * 255.0f + 0.5f);
                atlas.pixels[o + 2] = static_cast<uint8_t>((nrm.z * 0.5f + 0.5f) * 255.0f + 0.5f);
                atlas.pixels[o + 3] = 255u;
            }
        }
    }
    return atlas;
}

void emit_leaf_card(MeshData& m, const LeafCardParams& p) {
    const float len = glm::length(p.normal);
    const glm::vec3 nrm = len > 1e-6f ? p.normal / len : glm::vec3{0.0f, 0.0f, 1.0f};
    const glm::vec3 up_ref = std::fabs(nrm.y) < 0.95f ? glm::vec3{0.0f, 1.0f, 0.0f}
                                                      : glm::vec3{1.0f, 0.0f, 0.0f};
    glm::vec3 u = glm::cross(up_ref, nrm);
    const float ul = glm::length(u);
    u = ul > 1e-6f ? u / ul : glm::vec3{1.0f, 0.0f, 0.0f};
    glm::vec3 v = glm::cross(nrm, u);
    const float cs = std::cos(p.roll);
    const float sn = std::sin(p.roll);
    const glm::vec3 ru = u * cs + v * sn;
    const glm::vec3 rv = v * cs - u * sn;

    const glm::vec3 hw = ru * p.half_width;
    const glm::vec3 hh = rv * p.half_height;
    const glm::vec3 corner[4] = {
        p.center - hw + hh, // top-left
        p.center - hw - hh, // bottom-left
        p.center + hw - hh, // bottom-right
        p.center + hw + hh, // top-right
    };
    const glm::vec4 uvr = leaf_tile_uv(p.shape, p.tone, p.tile_px);
    const glm::vec2 uv[4] = {
        {uvr.x, uvr.y}, {uvr.x, uvr.w}, {uvr.z, uvr.w}, {uvr.z, uvr.y},
    };

    const float span = std::max(p.sway_span, 1e-3f);
    const auto base = static_cast<uint32_t>(m.vertices.size());
    for (int i = 0; i < 4; ++i) {
        // r = SWAY WEIGHT: 0 at the attachment, 1 at the free edge. It is the
        // only per-VERTEX channel here, and it must be, or the card translates
        // rigidly and reads as a flag rather than as foliage bending about its
        // branch.
        const float sway =
            std::clamp(glm::length(corner[i] - p.sway_origin) / span, 0.0f, 1.0f);
        m.vertices.push_back({corner[i], nrm, uv[i],
                              pack({sway, p.phase, p.value_jitter})});
    }
    // Winding is CCW seen from +normal; culling is off for this program, and
    // fs_foliage flips the normal toward the viewer.
    m.indices.insert(m.indices.end(),
                     {base, base + 1, base + 2, base, base + 2, base + 3});
}

} // namespace dfn::render
