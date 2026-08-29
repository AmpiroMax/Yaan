/*
Module: engine/render
File: engine/render/sources/PartsAtlas.cpp

Responsibility:
- The rasteriser behind PartsAtlas.h: ONE height field per (surface, tone),
  shading the albedo tile and differentiated into the normal tile, so a groove
  is dark exactly where the surface says it is deep.

Key items:
- surface_texel(): the whole art, one switch over PartSurface.
- generate_parts_atlas() / generate_parts_normal_atlas() / parts_tile_uv().

Dependencies:
- Uses: ProcTexture.h (tileable_fbm / tileable_cells / tileable_cell_id), glm.
- Used by: PartForge, RenderSystem/app, PartsAtlasTests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- PURE AND DETERMINISTIC. Integer-hash lattice noise only (ProcTexture's), no
  trigonometry-seeded hashes, no clock, no IO.
- FREQUENCIES ARE CYCLES PER TILE and one tile is PARTS_TILE_SPAN_M metres, so
  every number in here is a real millimetre pitch. Wood fibre 3-8 mm, thatch
  stalk ~8 mm, stone grit 2-5 mm. Changing the span without re-reading these
  turns them into decoration.
*/

#include "engine/render/sources/PartsAtlas.h"

#include "engine/render/sources/ProcTexture.h"

#include <algorithm>
#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace dfn::render {
namespace {

constexpr uint32_t ATLAS_SEED = 0x5B0Du;

[[nodiscard]] float smooth01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/// A ridge: 1 on the crest, 0 in the valley. The transform that turns value
/// noise into a connected net of lines instead of a field of blobs.
[[nodiscard]] float ridge(float n) { return 1.0f - std::fabs(2.0f * n - 1.0f); }

[[nodiscard]] float band(float x, float lo, float hi) {
    return smooth01((x - lo) / std::max(hi - lo, 1e-5f));
}

[[nodiscard]] float fbm(float u, float v, int pu, int pv, uint32_t seed, int oct) {
    return tileable_fbm({u, v}, {pu, pv}, seed, oct);
}

/// Distance from the tile centre in tile units, for the two columns that are
/// NOT repeated and may therefore have a centre at all.
[[nodiscard]] float radius_from_centre(float u, float v) {
    const float dx = u - 0.5f;
    const float dy = v - 0.5f;
    return std::sqrt(dx * dx + dy * dy);
}

/// What one row does to a column, over and above its own base colour: how much
/// the surface has been WORKED OVER by weather. Kept separate from the colour
/// table because the geometry of ageing (lichen patches, softened crests,
/// opened cracks) is the same wherever it happens.
struct ToneWeather {
    float lichen; ///< 0..1 budget of lichen/moss film
    float crack;  ///< how far cracks and checks have opened
    float polish; ///< 1 = crisp fresh surface, 0 = crests worn round
};

[[nodiscard]] ToneWeather weather_of(PartTone tone) {
    switch (tone) {
    case PartTone::Light: return {0.00f, 0.35f, 1.00f};
    case PartTone::Mid: return {0.00f, 0.55f, 0.90f};
    case PartTone::Dark: return {0.05f, 0.70f, 0.80f};
    case PartTone::Weathered: default: return {0.62f, 1.00f, 0.55f};
    }
}

/// Lichen and moss: pale grey-green crusts in the low places. Returns the
/// FILM WEIGHT at this texel, 0..1. It follows the height field downward on
/// purpose — moss grows where water sits, which is what makes it read as
/// growth rather than as a stain.
[[nodiscard]] float lichen_film(float u, float v, float height, float budget,
                                uint32_t seed) {
    if (budget <= 0.0f) {
        return 0.0f;
    }
    const float patch = fbm(u, v, 5, 5, seed ^ 0x77u, 3);
    const float edge = band(patch, 0.52f - 0.22f * budget, 0.72f);
    const float wet = 1.0f - smooth01(height * 1.4f);
    return std::clamp(edge * (0.35f + 0.65f * wet) * budget, 0.0f, 1.0f);
}

struct Texel {
    glm::vec3 rgb{0.5f};
    float height = 0.5f; ///< 0..1, the RELIEF the normal sheet differentiates
};

// --- ЗЕРНО: структура на масштабе ТЕКСЕЛЯ ----------------------------------
//
// ПОЧЕМУ ЭТОГО НЕЛЬЗЯ БЫЛО СДЕЛАТЬ ЧЕРЕЗ u,v. Все поля выше строятся из
// tileable_fbm и tileable_cells, а обе интерполируют между узлами решётки —
// значит любое их значение ГЛАДКО на масштабе текселя, сколько бы октав им ни
// дали. Поднять частоту не помогает: октава с периодом в два текселя после
// квинтической прокладки превращается в пандус, а пандус — ровно то, чего
// мера ДЕТАЛЬ (модуль отклонения от окрестности 3x3) не видит по построению.
// Поэтому зерно берёт решётку БЕЗ интерполяции и индексируется НОМЕРОМ
// ТЕКСЕЛЯ, а не координатой поверхности.
//
// И ЭТО НЕ УКРАШЕНИЕ, А ЧЕСТНОЕ ИЗОБРАЖЕНИЕ ПОДТЕКСЕЛЬНОЙ ПРАВДЫ. При плитке
// 512 px на метровый шаг тексель равен 1.95 мм, а зерно всякого вещества
// набора мельче: минеральное 0.5-3 мм, волокно 0.5-2 мм, песок в штукатурке
// 0.5-1 мм. Структура ниже Найквиста ОБЯЗАНА выглядеть шумом — именно так её
// несёт всякая снятая с натуры текстура, и именно этого у нас не было.
struct Grain {
    bool on = false;
    int x = 0;   ///< номер текселя в плитке по горизонтали
    int y = 0;   ///< ...и по вертикали (ось куска: волокно бежит вдоль y)
    int n = 1;   ///< сторона плитки в текселях — она же период заворота
};

[[nodiscard]] float ghash(const Grain& g, int dx, int dy, uint32_t seed) {
    return lattice_hash01({g.x + dx, g.y + dy}, {g.n, g.n}, seed);
}

/// ЗЕРНО: шум с нулевым средним и максимумом энергии на Найквисте плитки.
/// Вычитание среднего четырёх соседей — высокочастотный фильтр, и он здесь не
/// для красоты: без него шум несёт «облака» низких частот, которые увеличение
/// размажет, а на пиксель кадра не доедет ничего. Отдача ~[-0.75, 0.75].
[[nodiscard]] float grit_noise(const Grain& g, uint32_t seed) {
    const float c = ghash(g, 0, 0, seed);
    const float m = 0.25f * (ghash(g, 1, 0, seed) + ghash(g, -1, 0, seed)
                             + ghash(g, 0, 1, seed) + ghash(g, 0, -1, seed));
    return c - m;
}

/// ВОЛОКНО: то же зерно, но РАСТЯНУТОЕ вдоль оси куска. Волокно дерева — это
/// 0.5-2 мм поперёк и сантиметры вдоль, стебель соломы и травинка дёрна устроены
/// так же; изотропный шум на их месте читается как песок на доске. Резкость
/// оставлена только поперёк (высокочастотный фильтр по x), вдоль поле
/// постоянно на `along` текселей. Заворот точен, пока `along` делит сторону.
[[nodiscard]] float fibre_noise(const Grain& g, uint32_t seed, int along) {
    along = std::max(1, along);
    const int ny = std::max(1, g.n / along);
    // ПОЛОСА СЧИТАЕТСЯ ОТ СТОРОНЫ ПЛИТКИ, А НЕ ДЕЛЕНИЕМ НА `along`, и это не
    // стиль. `y / along` даёт ceil(n/along) полос, а решётка заворачивается на
    // ny = n/along: как только `along` не делит сторону нацело, последняя
    // НЕПОЛНАЯ полоса накладывается на первую, и по плитке идёт лишний стык.
    // Замерено на брусе (along 12): при стороне 64 шов вырастал до 1.62
    // среднего шага между столбцами против 1.29 у соседей — ровно четыре
    // плитки колонки, то есть систематически, а не случайно.
    const auto h = [&](int dx) {
        const int band = (g.y * ny) / std::max(1, g.n);
        return lattice_hash01({g.x + dx, band}, {g.n, ny}, seed);
    };
    return h(0) - 0.5f * (h(1) + h(-1));
}

/// РАЗБРОС ЦВЕТА ПО ТЕКСЕЛЮ. До этой волны вся плитка красилась ОДНИМ скаляром
/// на три канала (`rgb = base * (a + b*h)`), то есть была одномерной лестницей
/// одного тона: цветность 1.3-10.7 на 1000 пикселей при пороге К3 200 —
/// «монотонных цветов» владельца буквально измерено. Три независимых зерна
/// дают веществу микро-оттенки, как несёт их всякая минеральная поверхность.
[[nodiscard]] glm::vec3 chroma_noise(const Grain& g, uint32_t seed) {
    return {grit_noise(g, seed ^ 0x0A13u), grit_noise(g, seed ^ 0x0B27u),
            grit_noise(g, seed ^ 0x0C3Du)};
}

/// Сколько зерна какого рода носит вещество. Таблица, а не ветки по колонкам:
/// зерно у всех устроено одинаково и отличается только количеством, а
/// колоночные функции выше рисуют ФОРМУ вещества и остаются нетронутыми.
struct GrainSpec {
    float value;  ///< амплитуда зерна значения, доля альбедо
    float fibre;  ///< амплитуда растянутого зерна (волокно, стебель, травинка)
    int along;    ///< во сколько раз волокно длиннее, чем шире
    float hue;    ///< амплитуда разброса цвета по каналам
    float relief; ///< какая доля зерна уходит в РЕЛЬЕФ (лист нормалей)
};

[[nodiscard]] GrainSpec grain_of(PartSurface s) {
    switch (s) {
    // Дерево: волокно ведёт, зерно поддерживает. Тёсаное грубее пилёного.
    case PartSurface::HewnTimber: return {0.140f, 0.250f, 12, 0.075f, 0.55f};
    case PartSurface::SawnBoard: return {0.130f, 0.235f, 16, 0.070f, 0.55f};
    // Торец: волокно короткое (кольца идут поперёк), зерно крупнее.
    case PartSurface::EndGrain: return {0.165f, 0.145f, 4, 0.075f, 0.55f};
    // Камень: зерно ведёт — скол, кристалл, выщербина. Самое сильное на листе,
    // и это не вкус: минеральная поверхность и есть зерно, всё остальное у неё
    // крупнее текселя и уже нарисовано выше.
    case PartSurface::Stone: return {0.225f, 0.060f, 2, 0.090f, 0.60f};
    // Обожжённая глина: песок в черепке плюс кладочный шов даёт зерно чуть
    // тише камня, но цветнее — обжиг красит зёрна по-разному.
    case PartSurface::FiredClay: return {0.240f, 0.060f, 2, 0.120f, 0.55f};
    // Штукатурка: зуб тёрки. Мельче камня по природе и потому тише — но именно
    // она была худшим участком замера (ДЕТАЛЬ 0.52), и тише не значит нисколько.
    case PartSurface::Plaster: return {0.215f, 0.055f, 2, 0.085f, 0.50f};
    // Солома: стебель. Единственная колонка, уже нарисованная в своём шаге, —
    // ей добавлен только подтексельный ворс расщепления.
    case PartSurface::Thatch: return {0.140f, 0.230f, 10, 0.070f, 0.50f};
    // Дёрн: травинка 1-3 мм, то есть ровно подтексельная, плюс комок земли.
    case PartSurface::Turf: return {0.180f, 0.235f, 6, 0.120f, 0.50f};
    // ГЛУХОЕ ОКНО — ЕДИНСТВЕННОЕ ГЛАДКОЕ ВЕЩЕСТВО ЛИСТА, И ЗЕРНА ОНО НЕ
    // ПОЛУЧАЕТ ВОВСЕ. Это прямое требование расхождения Р1 (MATERIALS.md §0.2):
    // гладкое судится ПОВЕДЕНИЕМ БЛИКА, а не К1, и исполнитель, который добавит
    // зерна в стекло ради красного числа, получит шершавое стекло и назовёт это
    // успехом. Ноль здесь — утверждение, а не недоделка.
    case PartSurface::Pane: default: return {0.0f, 0.0f, 1, 0.0f, 0.0f};
    }
}

// --- the columns -----------------------------------------------------------
// Every field below is built from PERIODIC noise at INTEGER frequencies, so
// the tile wraps as a torus and the mesh may repeat it with a plain wrap. The
// two exceptions (EndGrain, Pane) are declared non-periodic in the header and
// are never repeated by the mesh side.

/// Wood, shared by the two timber columns. `coarse` widens the fibre and adds
/// the axe scallops; a sawn board runs finer and straighter.
[[nodiscard]] Texel wood_texel(float u, float v, const ToneWeather& w, bool hewn,
                               uint32_t seed) {
    // FIBRE runs along v (the piece's own axis, by the mesh contract). High
    // frequency across, low along: 26 cycles across one metre is a ~38 mm
    // fibre band, and the second octave lands on ~10 mm — the pitch a plane
    // leaves on oak.
    const int across = hewn ? 26 : 34;
    const float fibre = fbm(u, v, across, 2, seed, 3);
    const float fine = fbm(u, v, across * 3, 5, seed ^ 0x2Au, 2);
    float h = 0.42f + 0.30f * fibre + 0.14f * fine;

    if (hewn) {
        // AXE SCALLOPS: shallow bands across the piece, ~3 per metre, their
        // phase wandering so they are struck by a hand and not by a comb.
        const float wob = fbm(u, v, 3, 3, seed ^ 0x91u, 2) - 0.5f;
        const float s = std::fabs(std::fmod(v * 3.0f + wob * 0.35f + 9.0f, 1.0f) - 0.5f);
        h += 0.16f * (0.5f - s) * w.polish;
    } else {
        // SAW RIPPLE: a fine regular chatter across the board, the mark a
        // frame saw leaves. Faint on purpose — at 4 mm/texel it is a sheen,
        // not a corduroy.
        const float ripple = fbm(u, v, 2, 48, seed ^ 0x55u, 1);
        h += 0.05f * (ripple - 0.5f);
    }

    // CHECKS: shakes that follow the grain. They OPEN with wear, which is the
    // whole visual difference between new joinery and a barn wall.
    const float check = ridge(fbm(u, v, hewn ? 7 : 9, 2, seed ^ 0xC3u, 2));
    const float cut = band(check, 0.86f, 0.985f) * w.crack;
    h -= 0.34f * cut;

    // KNOTS: a hard dark eye with rings around it, one every few tiles. The
    // cell id gates rarity so knots are not on a lattice.
    // РЕЖЕ И МЕЛЬЧЕ (приёмка кадров 21.08: «клякса-сучок повторяется
    // читаемой сеткой ~230 px») — порог редкости поднят, глаз меньше, и
    // контраст смолы снижен: сучок остаётся сучком вблизи, но перестаёт
    // быть маяком, по которому взгляд ловит период тайла.
    const float id = tileable_cell_id({u, v}, {2, 3}, seed ^ 0x1Fu, 0.85f);
    float knot = 0.0f;
    if (id > 0.86f) {
        const float d = tileable_cells({u, v}, {2, 3}, seed ^ 0x1Fu, 0.85f);
        knot = 1.0f - smooth01(d * 10.0f);
        h -= 0.16f * knot;
    }

    const glm::vec3 tint{1.0f - 0.10f * cut, 1.0f - 0.13f * cut, 1.0f - 0.16f * cut};
    glm::vec3 rgb = tint * (0.70f + 0.52f * h);
    // A knot is DARKER AND REDDER than the board it sits in — resin, not shade.
    rgb *= glm::vec3{1.0f - 0.28f * knot, 1.0f - 0.36f * knot, 1.0f - 0.42f * knot};
    h = std::clamp(h, 0.0f, 1.0f);
    return {rgb, h};
}

/// The cut end: rings around a pith, radial checks splitting outward. NOT
/// periodic, and it must not be — rings have a centre, and an end face is one
/// tile wide by construction (the mesh never repeats a cap).
[[nodiscard]] Texel end_grain_texel(float u, float v, const ToneWeather& w,
                                    uint32_t seed) {
    const float wob = fbm(u, v, 4, 4, seed ^ 0x3Bu, 2) - 0.5f;
    const float r = radius_from_centre(u, v) * (1.0f + 0.22f * wob);
    // 14 rings across a half-tile: on a 0.25 m post that is ~9 mm a year,
    // which is a fast-grown pine and reads as rings rather than as a target.
    const float rings = 0.5f + 0.5f * std::cos(r * 28.0f * 3.14159265f);
    float h = 0.40f + 0.26f * rings + 0.16f * fbm(u, v, 20, 20, seed ^ 0x7Du, 2);
    // RADIAL CHECKS: the star of splits every drying log end carries. They
    // start at the pith and die out toward the bark.
    const float ang = std::atan2(v - 0.5f, u - 0.5f);
    const float spokes = ridge(fbm(ang * 1.2f + 4.0f, r * 2.0f, 8, 4, seed ^ 0xB7u, 2));
    const float split = band(spokes, 0.80f, 0.97f) * w.crack
                      * (1.0f - smooth01(r * 2.2f));
    h -= 0.42f * split;
    // The pith itself: a small dark core.
    h -= 0.30f * (1.0f - smooth01(r * 22.0f));
    h = std::clamp(h, 0.0f, 1.0f);
    glm::vec3 rgb = glm::vec3{0.72f + 0.46f * h};
    rgb *= glm::vec3{1.0f - 0.16f * split, 1.0f - 0.20f * split, 1.0f - 0.24f * split};
    return {rgb, h};
}

/// Stone: granular, with mica catching the light and lichen taking the low
/// places on the weathered row. NO joints — the kit lays courses as blocks.
[[nodiscard]] Texel stone_texel(float u, float v, const ToneWeather& w,
                                uint32_t seed) {
    const float grit = fbm(u, v, 30, 30, seed, 3);
    const float macro = fbm(u, v, 4, 4, seed ^ 0x19u, 2);
    // Bedding streaks: sedimentary stone is not isotropic, and a purely
    // isotropic mottle is what makes procedural rock read as porridge.
    const float bed = fbm(u, v, 3, 17, seed ^ 0x64u, 2);
    float h = 0.34f + 0.30f * grit + 0.20f * macro + 0.16f * bed;
    // PITS: the chisel bruises and blown grains of a worked face.
    const float pit = tileable_cells({u, v}, {9, 9}, seed ^ 0x2Du, 0.9f);
    h -= 0.26f * (1.0f - smooth01(pit * 5.0f)) * (0.4f + 0.6f * w.crack);
    h = std::clamp(h, 0.0f, 1.0f);

    glm::vec3 rgb = glm::vec3{0.66f + 0.62f * h};
    // MICA: sparse single-texel glints. A real granite face sparkles, and at
    // 4 mm per texel a sparkle IS one texel.
    const float mica = fbm(u, v, 64, 64, seed ^ 0xE1u, 1);
    rgb += glm::vec3{0.30f, 0.30f, 0.34f} * band(mica, 0.90f, 0.985f);
    const float film = lichen_film(u, v, h, w.lichen, seed);
    rgb = glm::mix(rgb, glm::vec3{0.62f, 0.66f, 0.46f} * (0.8f + 0.4f * h), film * 0.75f);
    return {rgb, h + 0.05f * film};
}

/// Fired clay — brick and roof tile. Sand grain, firing pores, and the faint
/// cloudiness of a kiln. The BOND is geometry; this is one brick's face.
[[nodiscard]] Texel clay_texel(float u, float v, const ToneWeather& w,
                               uint32_t seed) {
    const float sand = fbm(u, v, 40, 40, seed, 3);
    const float cloud = fbm(u, v, 3, 3, seed ^ 0x4Cu, 2);
    float h = 0.44f + 0.24f * sand + 0.16f * cloud;
    // PORES: air the clay kept. Round, sparse, and deeper than the grain.
    const float pore = tileable_cells({u, v}, {12, 12}, seed ^ 0x8Bu, 0.95f);
    h -= 0.30f * (1.0f - smooth01(pore * 6.0f));
    h = std::clamp(h, 0.0f, 1.0f);
    glm::vec3 rgb = glm::vec3{0.74f + 0.44f * h};
    // Kiln blush: fired clay is never one colour across a face.
    rgb *= glm::vec3{1.0f + 0.10f * (cloud - 0.5f), 1.0f, 1.0f - 0.10f * (cloud - 0.5f)};
    const float film = lichen_film(u, v, h, w.lichen, seed ^ 0x31u);
    rgb = glm::mix(rgb, glm::vec3{0.58f, 0.62f, 0.48f} * (0.85f + 0.3f * h), film * 0.7f);
    return {rgb, h};
}

/// Plaster and daub: the float's sweeps, hairline cracks, and the odd
/// pockmark. Flat by nature — its whole character is in a shallow band.
[[nodiscard]] Texel plaster_texel(float u, float v, const ToneWeather& w,
                                  uint32_t seed) {
    const float sweep = fbm(u, v, 5, 4, seed, 2);
    const float tooth = fbm(u, v, 44, 44, seed ^ 0x27u, 2);
    float h = 0.52f + 0.16f * sweep + 0.10f * tooth;
    // HAIRLINE CRACKS: a connected net, not scratches. Ridged noise gives the
    // net for free; the wear budget decides how far it has opened.
    const float net = ridge(fbm(u, v, 6, 6, seed ^ 0xA4u, 3));
    const float crack = band(net, 0.90f, 0.99f) * w.crack;
    h -= 0.30f * crack;
    // POCKS: where the float lifted the skin.
    const float pock = tileable_cells({u, v}, {7, 7}, seed ^ 0x5Eu, 0.95f);
    h -= 0.18f * (1.0f - smooth01(pock * 8.0f)) * w.crack;
    h = std::clamp(h, 0.0f, 1.0f);
    glm::vec3 rgb = glm::vec3{0.80f + 0.30f * h};
    rgb *= glm::vec3{1.0f - 0.22f * crack, 1.0f - 0.24f * crack, 1.0f - 0.26f * crack};
    const float film = lichen_film(u, v, h, w.lichen * 0.7f, seed ^ 0x12u);
    rgb = glm::mix(rgb, glm::vec3{0.55f, 0.58f, 0.45f}, film * 0.55f);
    return {rgb, h};
}

/// Thatch: stalks lying along v, gathered in bundles, cut ends showing at the
/// courses. The one column whose frequency is a real object — a rye stalk is
/// 6-9 mm, so 120 stalks per metre is the count, not a taste.
[[nodiscard]] Texel thatch_texel(float u, float v, const ToneWeather& w,
                                 uint32_t seed) {
    const float stalk = fbm(u, v, 120, 3, seed, 2);
    const float bundle = fbm(u, v, 9, 2, seed ^ 0x3Du, 2);
    const float wisp = fbm(u, v, 60, 22, seed ^ 0x71u, 2);
    float h = 0.34f + 0.34f * stalk + 0.22f * bundle + 0.12f * wisp;
    // The shadowed gaps BETWEEN bundles, which is what a thatched roof reads
    // by from ten metres.
    h -= 0.26f * band(1.0f - bundle, 0.62f, 0.92f);
    h = std::clamp(h, 0.0f, 1.0f);
    glm::vec3 rgb = glm::vec3{0.62f + 0.62f * h};
    // Straw greys from the top down as it weathers; the wear row carries it.
    rgb = glm::mix(rgb, glm::vec3{0.78f * (0.6f + 0.5f * h)}, w.lichen * 0.5f);
    return {rgb, h};
}

/// Turf: blade-fine grass over dark earth, with bare patches where the roof
/// dries out. Green belongs to the ground tufts' family — a turf roof and the
/// grass below it are the same world.
[[nodiscard]] Texel turf_texel(float u, float v, const ToneWeather& w,
                               uint32_t seed) {
    const float blade = fbm(u, v, 70, 14, seed, 2);
    const float clump = fbm(u, v, 6, 6, seed ^ 0x66u, 3);
    float h = 0.36f + 0.34f * blade + 0.28f * clump;
    const float bare = band(1.0f - clump, 0.70f, 0.95f) * (1.0f - w.lichen * 0.5f);
    h -= 0.22f * bare;
    h = std::clamp(h, 0.0f, 1.0f);
    glm::vec3 rgb = glm::vec3{0.70f + 0.52f * h};
    // The bare patch is EARTH, so it changes hue and not only value.
    rgb = glm::mix(rgb, glm::vec3{0.92f, 0.74f, 0.56f} * (0.7f + 0.4f * h), bare * 0.8f);
    return {rgb, h};
}

/// The blind pane. Not glass: the IMITATION of a room behind an opening
/// (HOUSES §2). A faint warm pool low and centre — an interior has a hearth —
/// falling to near-black at the reveals, plus the vertical bar of a shutter
/// batten. The old flat dark rectangle is exactly what read as a hole.
[[nodiscard]] Texel pane_texel(float u, float v, const ToneWeather& w,
                               uint32_t seed) {
    const float dx = (u - 0.5f) * 1.6f;
    const float dy = (v - 0.72f) * 1.3f;
    const float glow = std::exp(-(dx * dx + dy * dy) * 3.2f);
    const float grime = fbm(u, v, 16, 16, seed, 2);
    float h = 0.50f + 0.10f * (grime - 0.5f);
    // A batten across the opening: something SOLID must be visible in there,
    // or the eye keeps reading depth it cannot resolve.
    const float batten = 1.0f - smooth01(std::fabs(u - 0.5f) * 26.0f);
    h += 0.20f * batten;
    h = std::clamp(h, 0.0f, 1.0f);
    glm::vec3 rgb = glm::vec3{0.55f + 0.35f * grime};
    rgb += glm::vec3{1.55f, 0.95f, 0.42f} * (glow * (0.55f - 0.25f * w.lichen));
    rgb *= 1.0f - 0.35f * batten;
    return {rgb, h};
}

[[nodiscard]] Texel column_texel(PartSurface s, const ToneWeather& w, float u,
                                 float v, uint32_t seed) {
    switch (s) {
    case PartSurface::HewnTimber: return wood_texel(u, v, w, true, seed);
    case PartSurface::SawnBoard: return wood_texel(u, v, w, false, seed);
    case PartSurface::EndGrain: return end_grain_texel(u, v, w, seed);
    case PartSurface::Stone: return stone_texel(u, v, w, seed);
    case PartSurface::FiredClay: return clay_texel(u, v, w, seed);
    case PartSurface::Plaster: return plaster_texel(u, v, w, seed);
    case PartSurface::Thatch: return thatch_texel(u, v, w, seed);
    case PartSurface::Turf: return turf_texel(u, v, w, seed);
    case PartSurface::Pane: default: return pane_texel(u, v, w, seed);
    }
}

[[nodiscard]] Texel surface_texel(PartSurface s, PartTone t, float u, float v,
                                  const Grain& g) {
    const ToneWeather w = weather_of(t);
    const uint32_t seed = ATLAS_SEED + static_cast<uint32_t>(s) * 977u;
    Texel out = column_texel(s, w, u, v, seed);
    if (!g.on) {
        return out; // доза Flat: ни одной операции сверх листа волны 3
    }
    const GrainSpec gs = grain_of(s);
    if (gs.value <= 0.0f && gs.fibre <= 0.0f && gs.hue <= 0.0f) {
        return out; // гладкое вещество: зерна не получает (расхождение Р1)
    }
    // ЗЕРНО КЛАДЁТСЯ МНОЖИТЕЛЕМ С НУЛЕВЫМ СРЕДНИМ — иначе оно сдвинуло бы
    // палитру, а рисунок листа обязан менять ПОВЕРХНОСТЬ, а не цвет принятой
    // витрины (это утверждение проверяется набором: плитка усредняется к своей
    // клетке parts_tile_base).
    const float value = gs.value * grit_noise(g, seed ^ 0x0051u)
                      + gs.fibre * fibre_noise(g, seed ^ 0x008Du, gs.along);
    // ИЗНОС ДОБАВЛЯЕТ ЗЕРНА, А НЕ ЗАМЕНЯЕТ ЕГО: выветренный камень зернист
    // сильнее свежего скола, а свежая штукатурка глаже старой. Ряд уже несёт
    // эту ось (weather_of), и вторую заводить незачем.
    const float wear = 0.85f + 0.90f * (1.0f - w.polish);
    out.rgb *= glm::vec3{1.0f + value * wear}
             + gs.hue * chroma_noise(g, seed ^ 0x00E7u);
    out.height = std::clamp(out.height + gs.relief * value * wear, 0.0f, 1.0f);
    return out;
}

/// How deep this surface's relief is, in MILLIMETRES, so the normal sheet says
/// something true rather than something strong. A hewn scallop is a few
/// millimetres, a plaster crack a fraction of one, a thatch gap a centimetre.
[[nodiscard]] float relief_mm(PartSurface s) {
    // ПРАВКА ПОЛИТИКИ 20.08: у камня и глины числа больше физических.
    // Честные миллиметры дали своч, чей рельеф глаз не читает (проба:
    // размах затенения 37/255 — «плоские текстуры», сказал пользователь), а
    // борозда, которую видно с трёх метров в игре без AO, обязана врать
    // вдвое-втрое. Дерево оставлено ближе к правде — его читает волокно.
    switch (s) {
    case PartSurface::HewnTimber: return 7.0f;
    case PartSurface::SawnBoard: return 3.5f;
    case PartSurface::EndGrain: return 5.0f;
    case PartSurface::Stone: return 16.0f;
    case PartSurface::FiredClay: return 7.0f;
    case PartSurface::Plaster: return 2.5f;
    case PartSurface::Thatch: return 14.0f;
    case PartSurface::Turf: return 9.0f;
    case PartSurface::Pane: default: return 1.0f;
    }
}

[[nodiscard]] uint8_t to_byte(float v) {
    return static_cast<uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
}

} // namespace

glm::vec3 parts_tile_base(PartSurface surface, PartTone tone) {
    // THE KIT'S OWN COLOURS, cell by cell. Every value that PartForge's
    // material table already ships appears here in the cell its material maps
    // to, so texturing changes the SURFACE and not the palette the showcase
    // was accepted with: Timber -> HewnTimber/Mid, TimberDark ->
    // HewnTimber/Dark, Shingle -> SawnBoard/Weathered, Stone -> Stone/Mid,
    // Brick -> FiredClay/Mid, Tile -> FiredClay/Light, Plaster ->
    // Plaster/Light, Clay -> Plaster/Mid, Thatch -> Thatch/Light, Turf ->
    // Turf/Mid, Pane -> Pane/Mid.
    const int s = static_cast<int>(surface);
    const int t = static_cast<int>(tone);
    static const glm::vec3 table[PARTS_ATLAS_SURFACES][PARTS_ATLAS_TONES] = {
        // Light                  Mid                     Dark                    Weathered
        {{0.52f, 0.45f, 0.34f}, {0.44f, 0.37f, 0.29f}, {0.25f, 0.21f, 0.18f}, {0.38f, 0.35f, 0.31f}},
        {{0.58f, 0.50f, 0.38f}, {0.48f, 0.41f, 0.31f}, {0.28f, 0.24f, 0.20f}, {0.33f, 0.30f, 0.27f}},
        {{0.60f, 0.52f, 0.40f}, {0.52f, 0.44f, 0.34f}, {0.30f, 0.26f, 0.21f}, {0.42f, 0.39f, 0.35f}},
        {{0.55f, 0.55f, 0.53f}, {0.45f, 0.45f, 0.43f}, {0.33f, 0.33f, 0.32f}, {0.40f, 0.44f, 0.36f}},
        {{0.48f, 0.26f, 0.18f}, {0.42f, 0.24f, 0.17f}, {0.32f, 0.18f, 0.13f}, {0.37f, 0.31f, 0.23f}},
        {{0.71f, 0.67f, 0.57f}, {0.60f, 0.50f, 0.38f}, {0.48f, 0.42f, 0.34f}, {0.62f, 0.60f, 0.54f}},
        {{0.66f, 0.56f, 0.33f}, {0.56f, 0.47f, 0.29f}, {0.42f, 0.35f, 0.22f}, {0.50f, 0.46f, 0.33f}},
        {{0.38f, 0.46f, 0.22f}, {0.30f, 0.38f, 0.18f}, {0.22f, 0.28f, 0.14f}, {0.33f, 0.33f, 0.17f}},
        {{0.22f, 0.16f, 0.09f}, {0.15f, 0.10f, 0.055f}, {0.10f, 0.07f, 0.04f}, {0.14f, 0.11f, 0.07f}},
    };
    if (s < 0 || s >= static_cast<int>(PARTS_ATLAS_SURFACES) || t < 0
        || t >= static_cast<int>(PARTS_ATLAS_TONES)) {
        return {0.5f, 0.5f, 0.5f};
    }
    return table[s][t];
}

bool parts_tile_is_periodic(PartSurface surface) {
    return surface != PartSurface::EndGrain && surface != PartSurface::Pane;
}

glm::vec4 parts_tile_uv(PartSurface surface, PartTone tone, uint32_t tile_px) {
    const float w = static_cast<float>(PARTS_ATLAS_SURFACES * tile_px);
    const float h = static_cast<float>(PARTS_ATLAS_TONES * tile_px);
    const float inset_u = 0.5f / w;
    const float inset_v = 0.5f / h;
    const float u0 = static_cast<float>(surface) / static_cast<float>(PARTS_ATLAS_SURFACES);
    const float v0 = static_cast<float>(tone) / static_cast<float>(PARTS_ATLAS_TONES);
    const float du = 1.0f / static_cast<float>(PARTS_ATLAS_SURFACES);
    const float dv = 1.0f / static_cast<float>(PARTS_ATLAS_TONES);
    return {u0 + inset_u, v0 + inset_v, u0 + du - inset_u, v0 + dv - inset_v};
}

PartsAtlas generate_parts_atlas(uint32_t tile_px, PartsSheetDose dose) {
    PartsAtlas atlas;
    if (tile_px < 16) {
        return atlas;
    }
    const bool grain_on = dose == PartsSheetDose::Grain;
    atlas.tile_px = tile_px;
    atlas.width = PARTS_ATLAS_SURFACES * tile_px;
    atlas.height = PARTS_ATLAS_TONES * tile_px;
    atlas.pixels.assign(static_cast<size_t>(atlas.width) * atlas.height * 4, 0u);

    for (uint32_t si = 0; si < PARTS_ATLAS_SURFACES; ++si) {
        const auto surface = static_cast<PartSurface>(si);
        for (uint32_t ti = 0; ti < PARTS_ATLAS_TONES; ++ti) {
            const auto tone = static_cast<PartTone>(ti);
            const glm::vec3 base = parts_tile_base(surface, tone);
            for (uint32_t py = 0; py < tile_px; ++py) {
                const float v = (static_cast<float>(py) + 0.5f)
                              / static_cast<float>(tile_px);
                for (uint32_t px = 0; px < tile_px; ++px) {
                    const float u = (static_cast<float>(px) + 0.5f)
                                  / static_cast<float>(tile_px);
                    const Grain g{grain_on, static_cast<int>(px),
                                  static_cast<int>(py), static_cast<int>(tile_px)};
                    const Texel t = surface_texel(surface, tone, u, v, g);
                    // The pattern is a MULTIPLIER whose mean is ~1, so the
                    // tile averages to the row's own colour (asserted in the
                    // suite): texture changes the surface, not the palette.
                    const glm::vec3 rgb = glm::clamp(base * t.rgb, glm::vec3{0.0f},
                                                     glm::vec3{1.0f});
                    const size_t o = (static_cast<size_t>(ti * tile_px + py) * atlas.width
                                      + si * tile_px + px) * 4u;
                    atlas.pixels[o + 0] = to_byte(rgb.r);
                    atlas.pixels[o + 1] = to_byte(rgb.g);
                    atlas.pixels[o + 2] = to_byte(rgb.b);
                    atlas.pixels[o + 3] = 255u; // OPAQUE: a part is a volume
                }
            }
        }
    }
    return atlas;
}

PartsAtlas generate_parts_normal_atlas(uint32_t tile_px, PartsSheetDose dose) {
    PartsAtlas atlas;
    if (tile_px < 16) {
        return atlas;
    }
    const bool grain_on = dose == PartsSheetDose::Grain;
    atlas.tile_px = tile_px;
    atlas.width = PARTS_ATLAS_SURFACES * tile_px;
    atlas.height = PARTS_ATLAS_TONES * tile_px;
    atlas.pixels.assign(static_cast<size_t>(atlas.width) * atlas.height * 4, 0u);

    const float texel_m = PARTS_TILE_SPAN_M / static_cast<float>(tile_px);
    std::vector<float> height(static_cast<size_t>(tile_px) * tile_px, 0.0f);

    for (uint32_t si = 0; si < PARTS_ATLAS_SURFACES; ++si) {
        const auto surface = static_cast<PartSurface>(si);
        const float relief_m = relief_mm(surface) * 0.001f;
        const bool periodic = parts_tile_is_periodic(surface);
        for (uint32_t ti = 0; ti < PARTS_ATLAS_TONES; ++ti) {
            const auto tone = static_cast<PartTone>(ti);
            for (uint32_t py = 0; py < tile_px; ++py) {
                const float v = (static_cast<float>(py) + 0.5f)
                              / static_cast<float>(tile_px);
                for (uint32_t px = 0; px < tile_px; ++px) {
                    const float u = (static_cast<float>(px) + 0.5f)
                                  / static_cast<float>(tile_px);
                    const Grain g{grain_on, static_cast<int>(px),
                                  static_cast<int>(py), static_cast<int>(tile_px)};
                    height[static_cast<size_t>(py) * tile_px + px] =
                        surface_texel(surface, tone, u, v, g).height;
                }
            }
            // CENTRAL DIFFERENCES OF THE SAME FIELD the albedo shaded with —
            // that is the whole contract: the two sheets cannot disagree about
            // where a groove is, because there is one groove.
            for (uint32_t py = 0; py < tile_px; ++py) {
                for (uint32_t px = 0; px < tile_px; ++px) {
                    const auto at = [&](int x, int y) {
                        if (periodic) {
                            x = (x + static_cast<int>(tile_px)) % static_cast<int>(tile_px);
                            y = (y + static_cast<int>(tile_px)) % static_cast<int>(tile_px);
                        } else {
                            x = std::clamp(x, 0, static_cast<int>(tile_px) - 1);
                            y = std::clamp(y, 0, static_cast<int>(tile_px) - 1);
                        }
                        return height[static_cast<size_t>(y) * tile_px
                                      + static_cast<size_t>(x)];
                    };
                    const int x = static_cast<int>(px);
                    const int y = static_cast<int>(py);
                    const float dhdx = (at(x + 1, y) - at(x - 1, y)) * 0.5f * relief_m;
                    const float dhdy = (at(x, y + 1) - at(x, y - 1)) * 0.5f * relief_m;
                    glm::vec3 n = glm::normalize(
                        glm::vec3{-dhdx / texel_m, -dhdy / texel_m, 1.0f});
                    const size_t o = (static_cast<size_t>(ti * tile_px + py) * atlas.width
                                      + si * tile_px + px) * 4u;
                    atlas.pixels[o + 0] = to_byte(n.x * 0.5f + 0.5f);
                    atlas.pixels[o + 1] = to_byte(n.y * 0.5f + 0.5f);
                    atlas.pixels[o + 2] = to_byte(n.z * 0.5f + 0.5f);
                    atlas.pixels[o + 3] = 255u;
                }
            }
        }
    }
    return atlas;
}

} // namespace dfn::render
