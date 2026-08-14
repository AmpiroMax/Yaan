/*
Created: 09:08:2026 - 20:21:13
Last updated: 15:08:2026 - 00:45:20
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
// INTERIOR GAPS are placed EXPLICITLY, one or two per tile, rather than left to
// a noise threshold. Two reasons, and both are the point of this file:
//   1) size is then guaranteed — a gap is ~0.19 of the card, about 1 m on a
//      5.5 m oak card, five times render's ~0.31 m floor. A noise threshold
//      tuned for 2-3 % coverage produces gaps at whatever size the noise
//      happens to give, which is how "a few real holes" degenerates into lace.
//   2) the measured budget is tiny — reference crown interiors are 1.6-2.9 %
//      sky — and at that budget "a couple of holes" is literally what it is.
// This is the only place the canopy is see-through in the sense the user asked
// for; the rest of «сквозь листву можно смотреть» is the gaps BETWEEN cards.
constexpr float GAP_RADIUS = 0.095f;

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
        // RoundLobed — the broadleaf default: a wide mass with six soft lobes.
        {0.97f, 0.90f, 0.00f, 6, 0.20f, 11, 0.09f, 0.0f, 1.7f, 0.00f, 101u},
        // OvalSpray — narrow and leaning: rim fill and clump crowns.
        {0.64f, 0.98f, 0.38f, 5, 0.23f, 9, 0.10f, 0.9f, 2.4f, 0.20f, 211u},
        // RaggedTip — a wedge for branch tips and the crown top.
        {0.78f, 0.98f, -0.16f, 4, 0.26f, 9, 0.12f, 2.0f, 0.4f, 0.55f, 331u},
        // NeedleFan — flatter and spikier (conifer; pine still uses cone tiers,
        // so this column is generated but not yet placed).
        {1.00f, 0.66f, 0.10f, 9, 0.28f, 17, 0.11f, 0.5f, 1.1f, 0.15f, 457u},
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
    case LeafTone::ConiferDark: default: return {0.12f, 0.22f, 0.19f};
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
    case LeafTone::ConiferDark: default: return {0.12f, 0.22f, 0.19f};
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
    case LeafTone::ConiferDark: default: return {0.11f, 0.20f, 0.18f};
    }
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
            const int blob_count = needle ? 7 : ((sd.ax * sd.ay > 0.8f) ? 4 : 3);
            PackBlob blobs[8];
            const glm::vec2 axis{cs, sn};
            for (int bi = 0; bi < blob_count; ++bi) {
                const float t = -0.62f + 1.35f * (static_cast<float>(bi) + 0.5f)
                                             / static_cast<float>(blob_count);
                // Off-axis scatter: leaves clump on SIDES of a twig, not on it.
                const float off = (hash01(bi + 3, 7, sd.seed) - 0.5f)
                                * (needle ? 0.24f : 0.42f);
                blobs[bi].c = axis * (t * 0.92f)
                            + glm::vec2{-axis.y, axis.x} * off;
                // Outer blobs shrink — the tip of a spray is its youngest wood.
                const float shrink = 1.0f - 0.45f * std::fabs(t + 0.1f);
                blobs[bi].r = (needle ? 0.20f : 0.34f) * sd.ax * shrink
                            * (0.8f + 0.4f * hash01(bi + 9, 13, sd.seed));
                blobs[bi].seed = sd.seed + static_cast<uint32_t>(bi) * 977u;
            }

            for (uint32_t py = 0; py < atlas.tile_px; ++py) {
                for (uint32_t px = 0; px < atlas.tile_px; ++px) {
                    const float x = (static_cast<float>(px) + 0.5f) / n * 2.0f - 1.0f;
                    const float y = 1.0f - (static_cast<float>(py) + 0.5f) / n * 2.0f;

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
                    atlas.pixels[o + 0] = static_cast<uint8_t>(c.r * 255.0f + 0.5f);
                    atlas.pixels[o + 1] = static_cast<uint8_t>(c.g * 255.0f + 0.5f);
                    atlas.pixels[o + 2] = static_cast<uint8_t>(c.b * 255.0f + 0.5f);
                    atlas.pixels[o + 3] = 255u; // binary alpha (cutout program)
                }
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
