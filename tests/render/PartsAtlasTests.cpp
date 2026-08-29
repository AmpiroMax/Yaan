/*
Module: tests
File: tests/render/PartsAtlasTests.cpp

Responsibility:
- The building kit's own texture sheet, held to the four claims it makes: it is
  OPAQUE, it does not move the kit's PALETTE, it actually carries CONTRAST (the
  thing a flat vertex colour does not), and it REPEATS without a seam. Plus the
  mesh side: a textured part keeps its shape and gains only uv and a tile index.

Key items:
- tile mean vs parts_tile_base (with the separating control: the neighbouring
  tone must NOT pass), luma spread against the flat-face control, seam vs
  interior difference, normal-sheet relief order, the skinned/plain bar pair.

Dependencies:
- Uses: PartsAtlas.h, HewnBar.h, PartForge.h, FloraCards.h (the transparency
  control arm), MeshMeters.h.
- Used by: tests/render.cmake (render_parts_atlas).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- EVERY THRESHOLD HERE NAMES ITS REJECTED INSTANCE (Rule 45). The contrast
  floor's rejected instance is the kit as it shipped yesterday: a flat face,
  whose luma spread is exactly 0.
*/

#include "engine/render/sources/FloraCards.h"
#include "engine/render/sources/HewnBar.h"
#include "engine/render/sources/PartForge.h"
#include "engine/render/sources/PartsAtlas.h"
#include "tests/render/MeshMeters.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace dfn::render;

namespace {

/// The kit's sheet at a smaller tile: every claim below is about the FIELD,
/// which is scale-free, and 64 px keeps the suite at a second instead of a
/// minute. The shipped density is asserted separately, once.
constexpr uint32_t TEST_TILE = 64;

struct TileStats {
    glm::vec3 mean{0.0f};
    double luma_sd = 0.0;
    double seam = 0.0;     ///< mean |difference| across the wrap seam
    double interior = 0.0; ///< ...and between two neighbouring interior columns
};

[[nodiscard]] TileStats stats_of(const PartsAtlas& a, PartSurface s, PartTone t) {
    const uint32_t T = a.tile_px;
    const uint32_t x0 = static_cast<uint32_t>(s) * T;
    const uint32_t y0 = static_cast<uint32_t>(t) * T;
    const auto at = [&](uint32_t x, uint32_t y, int c) {
        return static_cast<double>(
            a.pixels[(static_cast<size_t>(y0 + y) * a.width + x0 + x) * 4
                     + static_cast<size_t>(c)]) / 255.0;
    };
    TileStats out;
    double sum_l = 0.0;
    double sum_l2 = 0.0;
    glm::dvec3 sum{0.0};
    for (uint32_t y = 0; y < T; ++y) {
        for (uint32_t x = 0; x < T; ++x) {
            const double r = at(x, y, 0);
            const double g = at(x, y, 1);
            const double b = at(x, y, 2);
            sum += glm::dvec3{r, g, b};
            const double l = 0.30 * r + 0.59 * g + 0.11 * b;
            sum_l += l;
            sum_l2 += l * l;
        }
        for (int c = 0; c < 3; ++c) {
            out.seam += std::fabs(at(0, y, c) - at(T - 1, y, c));
            out.interior += std::fabs(at(T / 2, y, c) - at(T / 2 + 1, y, c));
        }
    }
    const double n = static_cast<double>(T) * T;
    out.mean = glm::vec3{sum / n};
    out.luma_sd = std::sqrt(std::max(0.0, sum_l2 / n - (sum_l / n) * (sum_l / n)));
    out.seam /= 3.0 * static_cast<double>(T);
    out.interior /= 3.0 * static_cast<double>(T);
    return out;
}

/// ДЕТАЛЬ ПРИБОРА ПРИЁМКИ, но по ПЛИТКЕ, а не по кадру: средний модуль
/// отклонения яркости текселя от окрестности 3x3, в шкале 0..255. Тот же
/// метод, что у tools/quality/measure_surface.py (критерий К1) — здесь он
/// применён на шаг раньше, к листу, потому что кадр в наборе не снять, а
/// утверждение «структура в листе есть» проверяемо и без него. Окрестность
/// берётся С ЗАВОРОТОМ: плитка торическая, и край — не особое место.
[[nodiscard]] double tile_detail(const PartsAtlas& a, PartSurface s, PartTone t) {
    const uint32_t T = a.tile_px;
    const uint32_t x0 = static_cast<uint32_t>(s) * T;
    const uint32_t y0 = static_cast<uint32_t>(t) * T;
    const auto lum = [&](int x, int y) {
        const auto wx = static_cast<uint32_t>(((x % static_cast<int>(T)) + static_cast<int>(T))
                                              % static_cast<int>(T));
        const auto wy = static_cast<uint32_t>(((y % static_cast<int>(T)) + static_cast<int>(T))
                                              % static_cast<int>(T));
        const size_t o = (static_cast<size_t>(y0 + wy) * a.width + x0 + wx) * 4;
        return 0.2126 * a.pixels[o] + 0.7152 * a.pixels[o + 1] + 0.0722 * a.pixels[o + 2];
    };
    double acc = 0.0;
    for (uint32_t y = 0; y < T; ++y) {
        for (uint32_t x = 0; x < T; ++x) {
            double m = 0.0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    m += lum(static_cast<int>(x) + dx, static_cast<int>(y) + dy);
                }
            }
            acc += std::fabs(lum(static_cast<int>(x), static_cast<int>(y)) - m / 9.0);
        }
    }
    return acc / (static_cast<double>(T) * T);
}

/// СРЕДНИЙ шаг между соседними столбцами плитки — эталон, против которого
/// судится шов. `TileStats::interior` берёт ОДНУ пару столбцов в середине, и на
/// листе волны 3 этого хватало: поля там гладкие, и соседняя пара мало
/// отличается от любой другой. С зерном на масштабе текселя разность соседей —
/// сама случайная величина, у одной пары разброс сравним с ней самой, и
/// отношение шва к ней гуляет вокруг единицы. Эталон обязан быть средним, иначе
/// критерий меряет удачу выбора столбца, а не шов.
[[nodiscard]] double mean_column_step(const PartsAtlas& a, PartSurface s, PartTone t) {
    const uint32_t T = a.tile_px;
    const uint32_t x0 = static_cast<uint32_t>(s) * T;
    const uint32_t y0 = static_cast<uint32_t>(t) * T;
    const auto at = [&](uint32_t x, uint32_t y, int c) {
        return static_cast<double>(
            a.pixels[(static_cast<size_t>(y0 + y) * a.width + x0 + x) * 4
                     + static_cast<size_t>(c)]) / 255.0;
    };
    double acc = 0.0;
    for (uint32_t y = 0; y < T; ++y) {
        for (uint32_t x = 0; x + 1 < T; ++x) {
            for (int c = 0; c < 3; ++c) {
                acc += std::fabs(at(x, y, c) - at(x + 1, y, c));
            }
        }
    }
    return acc / (3.0 * static_cast<double>(T) * static_cast<double>(T - 1));
}

[[nodiscard]] float rel_gap(const glm::vec3& a, const glm::vec3& b) {
    const float scale = std::max({b.r, b.g, b.b, 0.05f});
    return std::max({std::fabs(a.r - b.r), std::fabs(a.g - b.g),
                     std::fabs(a.b - b.b)}) / scale;
}

} // namespace

TEST_CASE("parts atlas: opaque everywhere, and the meter can see a hole") {
    const PartsAtlas a = generate_parts_atlas(TEST_TILE);
    REQUIRE(a.width == PARTS_ATLAS_SURFACES * TEST_TILE);
    REQUIRE(a.height == PARTS_ATLAS_TONES * TEST_TILE);
    size_t transparent = 0;
    for (size_t i = 3; i < a.pixels.size(); i += 4) {
        transparent += a.pixels[i] < 255u ? 1u : 0u;
    }
    // A part is a closed volume (Rule 52); a transparent texel on this sheet
    // would be a hole in a wall.
    CHECK(transparent == 0);

    // THE CONTROL, and it is a real sheet rather than a synthetic one: flora's
    // leaf atlas is a cutout mask, so the same meter must find plenty of
    // transparency there. Without this arm "0 transparent texels" could just
    // as well mean the meter reads the wrong byte.
    const LeafAtlas leaves = generate_leaf_atlas(64, FloraSeason::Summer);
    size_t leaf_transparent = 0;
    for (size_t i = 3; i < leaves.pixels.size(); i += 4) {
        leaf_transparent += leaves.pixels[i] < 255u ? 1u : 0u;
    }
    CHECK(leaf_transparent > 1000);
}

TEST_CASE("parts atlas: deterministic to the byte") {
    const PartsAtlas a = generate_parts_atlas(TEST_TILE);
    const PartsAtlas b = generate_parts_atlas(TEST_TILE);
    REQUIRE(a.pixels.size() == b.pixels.size());
    CHECK(a.pixels == b.pixels);
    const PartsAtlas na = generate_parts_normal_atlas(TEST_TILE);
    const PartsAtlas nb = generate_parts_normal_atlas(TEST_TILE);
    CHECK(na.pixels == nb.pixels);
}

TEST_CASE("parts atlas: tiles are laid out inside their own cell, inset") {
    const float du = 1.0f / static_cast<float>(PARTS_ATLAS_SURFACES);
    const float dv = 1.0f / static_cast<float>(PARTS_ATLAS_TONES);
    for (uint32_t s = 0; s < PARTS_ATLAS_SURFACES; ++s) {
        for (uint32_t t = 0; t < PARTS_ATLAS_TONES; ++t) {
            const glm::vec4 r = parts_tile_uv(static_cast<PartSurface>(s),
                                              static_cast<PartTone>(t), TEST_TILE);
            const float cu = static_cast<float>(s) * du;
            const float cv = static_cast<float>(t) * dv;
            // Inside its cell AND strictly inset: a sampler that straddles two
            // tiles paints stone with thatch at the border.
            CHECK(r.x > cu);
            CHECK(r.y > cv);
            CHECK(r.z < cu + du);
            CHECK(r.w < cv + dv);
            CHECK(r.z > r.x);
            CHECK(r.w > r.y);
        }
    }
}

TEST_CASE("parts atlas: the tile averages to the kit's own colour, and not to the next row's") {
    const PartsAtlas a = generate_parts_atlas(TEST_TILE);
    // 0.08 of the row's own brightest channel. IT SEPARATES, and that is the
    // measurement: the worst tile sits at 0.062 from its own row, while the
    // NEAREST OTHER row of the same column is asserted below to be farther
    // than 0.08 — so no tile can pass for its neighbour.
    constexpr float TOL = 0.08f;
    for (uint32_t s = 0; s < PARTS_ATLAS_SURFACES; ++s) {
        const auto surface = static_cast<PartSurface>(s);
        // THE PANE IS EXEMPT AND SAYS WHY: its tile carries a hearth glow, so
        // its mean is deliberately not its base — the base is the dark it
        // falls to at the reveals. A criterion applied where it cannot hold
        // is Rule 48's failure, not strictness.
        if (surface == PartSurface::Pane) {
            continue;
        }
        for (uint32_t t = 0; t < PARTS_ATLAS_TONES; ++t) {
            const auto tone = static_cast<PartTone>(t);
            const TileStats st = stats_of(a, surface, tone);
            const glm::vec3 own = parts_tile_base(surface, tone);
            CHECK(rel_gap(st.mean, own) < TOL);
            for (uint32_t o = 0; o < PARTS_ATLAS_TONES; ++o) {
                if (o == t) {
                    continue;
                }
                const glm::vec3 other =
                    parts_tile_base(surface, static_cast<PartTone>(o));
                CHECK(rel_gap(st.mean, other) > TOL);
            }
        }
    }
}

TEST_CASE("parts atlas: every tile carries contrast — the flat face is the control") {
    const PartsAtlas a = generate_parts_atlas(TEST_TILE);
    // THE REJECTED INSTANCE IS THE KIT AS IT SHIPPED YESTERDAY: one packed
    // colour per face, luma spread exactly 0.000. The floor sits below the
    // quietest generated tile (0.0104, fired clay at its darkest row) and far
    // above the control.
    constexpr double FLOOR = 0.008;
    double quietest = 1.0;
    for (uint32_t s = 0; s < PARTS_ATLAS_SURFACES; ++s) {
        for (uint32_t t = 0; t < PARTS_ATLAS_TONES; ++t) {
            const TileStats st = stats_of(a, static_cast<PartSurface>(s),
                                          static_cast<PartTone>(t));
            CHECK(st.luma_sd > FLOOR);
            quietest = std::min(quietest, st.luma_sd);
        }
    }
    MESSAGE("quietest tile luma sd = " << quietest);

    // The control arm, measured by the SAME meter: a tile of one flat colour.
    PartsAtlas flat = a;
    const uint32_t T = flat.tile_px;
    for (uint32_t y = 0; y < T; ++y) {
        for (uint32_t x = 0; x < T; ++x) {
            const size_t o = (static_cast<size_t>(y) * flat.width + x) * 4;
            flat.pixels[o + 0] = 112u;
            flat.pixels[o + 1] = 94u;
            flat.pixels[o + 2] = 74u;
        }
    }
    const TileStats flat_st = stats_of(flat, PartSurface::HewnTimber, PartTone::Light);
    CHECK(flat_st.luma_sd < FLOOR);
}

TEST_CASE("parts atlas: the periodic columns wrap without a seam") {
    const PartsAtlas a = generate_parts_atlas(TEST_TILE);
    // The mesh repeats a tile by a plain wrap, so the two edges meet. If the
    // field were not built on a periodic lattice, the difference ACROSS that
    // meeting would stand out against the difference between any two
    // neighbouring columns inside the tile. Measured as a RATIO, because the
    // interior difference is what a seamless join should look like.
    for (uint32_t s = 0; s < PARTS_ATLAS_SURFACES; ++s) {
        const auto surface = static_cast<PartSurface>(s);
        if (!parts_tile_is_periodic(surface)) {
            continue; // never repeated: end grain and the window pane
        }
        for (uint32_t t = 0; t < PARTS_ATLAS_TONES; ++t) {
            const TileStats st = stats_of(a, surface, static_cast<PartTone>(t));
            CHECK(st.seam <= st.interior * 1.6 + 1e-4);
        }
    }

    // THE CONTROL IS SYNTHETIC AND SAYS SO: no shipped column is
    // non-periodic-and-repeated, so there is no real rejected instance to
    // point at. A linear ramp laid across one tile leaves its two edges as far
    // apart as a field can be while every interior step stays small — exactly
    // the defect the meter exists to catch.
    PartsAtlas ramped = a;
    const uint32_t T = ramped.tile_px;
    for (uint32_t y = 0; y < T; ++y) {
        for (uint32_t x = 0; x < T; ++x) {
            const size_t o = (static_cast<size_t>(y) * ramped.width + x) * 4;
            const auto v = static_cast<uint8_t>(40 + (170 * x) / T);
            ramped.pixels[o + 0] = v;
            ramped.pixels[o + 1] = v;
            ramped.pixels[o + 2] = v;
        }
    }
    const TileStats bad = stats_of(ramped, PartSurface::HewnTimber, PartTone::Light);
    CHECK(bad.seam > bad.interior * 10.0);
}

TEST_CASE("parts atlas: the normal sheet is unit, outward, and as deep as declared") {
    const PartsAtlas n = generate_parts_normal_atlas(TEST_TILE);
    const auto tilt_of = [&](PartSurface s, PartTone t) {
        const uint32_t T = n.tile_px;
        const uint32_t x0 = static_cast<uint32_t>(s) * T;
        const uint32_t y0 = static_cast<uint32_t>(t) * T;
        double tilt = 0.0;
        for (uint32_t y = 0; y < T; ++y) {
            for (uint32_t x = 0; x < T; ++x) {
                const size_t o = (static_cast<size_t>(y0 + y) * n.width + x0 + x) * 4;
                const double nx = n.pixels[o + 0] / 255.0 * 2.0 - 1.0;
                const double ny = n.pixels[o + 1] / 255.0 * 2.0 - 1.0;
                const double nz = n.pixels[o + 2] / 255.0 * 2.0 - 1.0;
                // A tangent-space normal always leans OUT of the surface.
                CHECK(nz > 0.0);
                CHECK(std::fabs(std::sqrt(nx * nx + ny * ny + nz * nz) - 1.0) < 0.02);
                tilt += std::sqrt(nx * nx + ny * ny);
            }
        }
        return tilt / (static_cast<double>(T) * T);
    };
    // Relief is a millimetre claim, not a decoration: thatch is declared at
    // 12 mm of stalk and plaster at 1.5 mm of float mark, so the sheet must
    // put them an order of magnitude apart. A sheet where every surface tilts
    // the same is one that says nothing about material.
    const double thatch = tilt_of(PartSurface::Thatch, PartTone::Mid);
    const double plaster = tilt_of(PartSurface::Plaster, PartTone::Mid);
    MESSAGE("mean tilt: thatch " << thatch << " plaster " << plaster);
    CHECK(thatch > plaster * 4.0);
}

TEST_CASE("parts atlas: the shipped density is a millimetre figure") {
    // 256 px over 1 m is 3.9 mm per texel ON THE OBJECT — finer than the leaf
    // atlas' ~5 mm, which is the density the Full HD pivot accepted. Asserted
    // because both halves are constants that can drift apart silently.
    const float mm_per_texel =
        PARTS_TILE_SPAN_M * 1000.0f / static_cast<float>(PARTS_ATLAS_TILE_PX);
    CHECK(mm_per_texel < 5.0f);
    CHECK(mm_per_texel > 1.0f);
}

TEST_CASE("skinned bar: the texture changes uv and colour, and NOTHING else") {
    // THE CONTROL ARM OUT OF THE SAME BINARY (Rule 47): one material with the
    // skin off, one with it on, same seed, same rng. A texture that moved a
    // vertex would be a shape change wearing a texture's name — and the whole
    // kit's closedness, solidity and stair tests were passed by the geometry
    // this arm reproduces.
    HewnMaterial plain{{0.44f, 0.37f, 0.29f}, 0.16f, 0.22f, 0.012f, {}};
    HewnMaterial skinned = plain;
    skinned.skin.textured = true;
    skinned.skin.side = PartSurface::HewnTimber;
    skinned.skin.side_tone = PartTone::Mid;
    skinned.skin.end = PartSurface::EndGrain;
    skinned.skin.end_tone = PartTone::Mid;

    MeshData flat;
    MeshData tex;
    HewnRng r1(7u);
    HewnRng r2(7u);
    hewn_bar(flat, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f},
             3.0f, 0.125f, 0.125f, plain, 0.8f, r1, 2);
    hewn_bar(tex, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f},
             3.0f, 0.125f, 0.125f, skinned, 0.8f, r2, 2);

    REQUIRE(flat.vertices.size() == tex.vertices.size());
    REQUIRE(flat.indices == tex.indices);
    for (size_t i = 0; i < flat.vertices.size(); ++i) {
        CHECK(flat.vertices[i].position == tex.vertices[i].position);
        CHECK(flat.vertices[i].normal == tex.vertices[i].normal);
    }
    CHECK(meshtest::half_edge_defects(tex) == 0);
    CHECK(meshtest::signed_volume(tex) > 0.0);

    // And the uv is TILE SPACE, not a stretch: a 3 m bar spans 3 tiles along
    // its own axis at a 1 m span. The control is the same bar at half the
    // length — if the mapping stretched to fit, both would read the same.
    float vmax = 0.0f;
    for (const auto& v : tex.vertices) {
        vmax = std::max(vmax, v.uv.y);
    }
    CHECK(vmax == doctest::Approx(3.0f / PARTS_TILE_SPAN_M).epsilon(0.001));

    MeshData half;
    HewnRng r3(7u);
    hewn_bar(half, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f},
             1.5f, 0.125f, 0.125f, skinned, 0.8f, r3, 2);
    float half_vmax = 0.0f;
    for (const auto& v : half.vertices) {
        half_vmax = std::max(half_vmax, v.uv.y);
    }
    CHECK(half_vmax == doctest::Approx(vmax * 0.5f).epsilon(0.001));
}

TEST_CASE("forged part: geometry in the textured stream, tile index in the colour") {
    PartParams p;
    p.kind = PartKind::Beam;
    p.material = PartMaterial::Timber;
    p.length_u = 12;
    p.wear = 0.3f;
    // ASKED FOR EXPLICITLY. The catalogue's default is the FLAT form until the
    // atlas is bound in the renderer (DFN_PARTS_TEXTURED), so a test of the
    // textured stream that relied on the default was really testing the default
    // — and went red the day it changed, without the textured form breaking.
    p.textured = true;
    const RegistryObject beam = forge_part(p);
    // ONE SURFACE, ONE STREAM. Geometry in both would z-fight itself, and the
    // app draws `bark` with the atlas and `wood` with flat colour.
    CHECK(beam.wood.indices.empty());
    REQUIRE(!beam.bark.indices.empty());

    size_t side = 0;
    size_t end = 0;
    for (const auto& v : beam.bark.vertices) {
        const uint32_t col = v.color_rgba & 0xFFu;
        const uint32_t row = (v.color_rgba >> 8) & 0xFFu;
        const uint32_t blue = (v.color_rgba >> 16) & 0xFFu;
        const uint32_t alpha = (v.color_rgba >> 24) & 0xFFu;
        CHECK(alpha == 255u); // sky visibility: built above ground
        CHECK(blue <= 255u);
        CHECK(row == static_cast<uint32_t>(PartTone::Mid)); // fresh: wear 0.3
        if (col == static_cast<uint32_t>(PartSurface::HewnTimber)) {
            ++side;
        } else if (col == static_cast<uint32_t>(PartSurface::EndGrain)) {
            ++end;
        }
    }
    // A hewn beam shows hewn faces AND two sawn ends — the corner ties of a
    // log wall are nothing but those ends.
    CHECK(side > 0);
    CHECK(end > 0);
    CHECK(side + end == beam.bark.vertices.size());

    // THE CONTROL: a stone footing must land in a different column, or the
    // decode above is reading a constant rather than the material.
    PartParams s;
    s.kind = PartKind::Footing;
    s.material = PartMaterial::Stone;
    s.wear = 0.8f;
    s.textured = true; // same reason as the beam above
    const RegistryObject stone = forge_part(s);
    REQUIRE(!stone.bark.vertices.empty());
    const uint32_t stone_col = stone.bark.vertices.front().color_rgba & 0xFFu;
    const uint32_t stone_row = (stone.bark.vertices.front().color_rgba >> 8) & 0xFFu;
    CHECK(stone_col == static_cast<uint32_t>(PartSurface::Stone));
    CHECK(stone_row == static_cast<uint32_t>(PartTone::Weathered)); // wear 0.8
}

// --- ВОЛНА 4: СТРУКТУРА В ЛИСТ ---------------------------------------------

TEST_CASE("parts sheet dose: Flat is the wave-3 sheet, and it is the DEFAULT") {
    // ОБЕ РУКИ ЗАМЕРА ВЫХОДЯТ ИЗ ОДНОЙ СБОРКИ (правило 47), а значит доза
    // обязана быть ПАРАМЕТРОМ, а не переменной среды: иначе этот случай
    // написать нечем — две дозы не встретились бы в одном процессе.
    const PartsAtlas def = generate_parts_atlas(TEST_TILE);
    const PartsAtlas flat = generate_parts_atlas(TEST_TILE, PartsSheetDose::Flat);
    CHECK(def.pixels == flat.pixels);
    const PartsAtlas ndef = generate_parts_normal_atlas(TEST_TILE);
    const PartsAtlas nflat = generate_parts_normal_atlas(TEST_TILE, PartsSheetDose::Flat);
    CHECK(ndef.pixels == nflat.pixels);

    // И СТОРОНА ПЛИТКИ ТОЖЕ У ДОЗЫ: она входит в ключ кэша, и доза, сменившая
    // сторону, обязана промахнуться мимо вчерашней плитки.
    CHECK(parts_tile_px(PartsSheetDose::Flat) == PARTS_ATLAS_TILE_PX);
    CHECK(parts_tile_px(PartsSheetDose::Grain) == PARTS_ATLAS_TILE_PX_GRAIN);
    CHECK(PARTS_ATLAS_TILE_PX_GRAIN > PARTS_ATLAS_TILE_PX);
}

TEST_CASE("parts sheet dose: the grain lands on the TEXEL, and skips the smooth one") {
    const PartsAtlas flat = generate_parts_atlas(TEST_TILE, PartsSheetDose::Flat);
    const PartsAtlas grain = generate_parts_atlas(TEST_TILE, PartsSheetDose::Grain);
    REQUIRE(flat.pixels.size() == grain.pixels.size());

    // КОНТРОЛЬНАЯ РУКА — САМ ЛИСТ ВОЛНЫ 3 (правило 45, «отвергнутый образец»).
    // Критерий, который ничего не валит, — описание, а не критерий: прибор
    // обязан ПРОВАЛИТЬ вчерашнюю плитку той же мерой, какой принимает новую.
    double worst_flat = 1e9;
    double worst_grain = 1e9;
    for (uint32_t s = 0; s < PARTS_ATLAS_SURFACES; ++s) {
        const auto surface = static_cast<PartSurface>(s);
        if (surface == PartSurface::Pane) {
            continue; // гладкое: судится бликом, а не структурой (Р1)
        }
        for (uint32_t t = 0; t < PARTS_ATLAS_TONES; ++t) {
            const auto tone = static_cast<PartTone>(t);
            const double f = tile_detail(flat, surface, tone);
            const double g = tile_detail(grain, surface, tone);
            CHECK(g > f * 2.0);
            worst_flat = std::min(worst_flat, f);
            worst_grain = std::min(worst_grain, g);
        }
    }
    MESSAGE("ДЕТАЛЬ плитки, худшее матовое: доза 0 = " << worst_flat
            << ", доза 1 = " << worst_grain);
    // Полоса замера листа волны 3 — 0.12..1.80 при пороге кадра К1 4.0, то
    // есть лист провалил бы критерий даже при отрисовке тексель-в-пиксель.
    CHECK(worst_flat < 2.0);
    CHECK(worst_grain > 2.0);

    // ГЛАДКОЕ ВЕЩЕСТВО ЗЕРНА НЕ ПОЛУЧАЕТ ВОВСЕ, и это утверждение, а не
    // недоделка: расхождение Р1 (MATERIALS.md §0.2) — гладкое судится
    // ПОВЕДЕНИЕМ БЛИКА, и исполнитель, добавивший зерна в стекло ради
    // красного числа, получит шершавое стекло и назовёт это успехом.
    for (uint32_t t = 0; t < PARTS_ATLAS_TONES; ++t) {
        const auto tone = static_cast<PartTone>(t);
        CHECK(tile_detail(grain, PartSurface::Pane, tone)
              == doctest::Approx(tile_detail(flat, PartSurface::Pane, tone)).epsilon(1e-9));
    }
}

TEST_CASE("parts sheet dose: the grain still wraps, and the palette does not move") {
    const PartsAtlas grain = generate_parts_atlas(TEST_TILE, PartsSheetDose::Grain);
    for (uint32_t s = 0; s < PARTS_ATLAS_SURFACES; ++s) {
        const auto surface = static_cast<PartSurface>(s);
        for (uint32_t t = 0; t < PARTS_ATLAS_TONES; ++t) {
            const auto tone = static_cast<PartTone>(t);
            const TileStats st = stats_of(grain, surface, tone);
            // Рисунок листа меняет ПОВЕРХНОСТЬ, а не палитру принятой витрины:
            // зерно кладётся множителем с нулевым средним ровно ради этого.
            CHECK(rel_gap(st.mean, parts_tile_base(surface, tone)) < 0.16f);
            if (!parts_tile_is_periodic(surface)) {
                continue;
            }
            // Заворот: зерно индексируется НОМЕРОМ ТЕКСЕЛЯ, и решётка обязана
            // замкнуться на стороне плитки — иначе по каждому брусу пошёл бы шов.
            // Эталон — СРЕДНИЙ шаг между соседними столбцами, а не одна пара в
            // середине: с зерном разность соседей сама случайна (см. довод у
            // mean_column_step), и одна пара мерила бы удачу выбора столбца.
            CHECK(st.seam <= mean_column_step(grain, surface, tone) * 1.6 + 1e-4);
        }
    }
}

TEST_CASE("parts sheet dose: 512 px is a millimetre claim, not a bigger number") {
    // Единственная причина поднимать сторону — ТЕКСЕЛЬ, а не «побольше».
    // Зерно всякого вещества набора мельче миллиметра-двух (минеральное
    // 0.5-3 мм, волокно 0.5-2 мм, песок в штукатурке 0.5-1 мм), а плитка
    // 256 px даёт тексель 3.9 мм — крупнее самой структуры, которую рисует.
    const float flat_mm = PARTS_TILE_SPAN_M * 1000.0f
                        / static_cast<float>(PARTS_ATLAS_TILE_PX);
    const float grain_mm = PARTS_TILE_SPAN_M * 1000.0f
                         / static_cast<float>(PARTS_ATLAS_TILE_PX_GRAIN);
    CHECK(flat_mm > 3.5f);
    CHECK(grain_mm < 2.0f);
    CHECK(grain_mm > 1.0f);
}
