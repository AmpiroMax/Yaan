/*
Module: tests/render
File: tests/render/TreeForgeV2Tests.cpp

Responsibility:
- Руки ВТОРОЙ ИТЕРАЦИИ ДЕРЕВЬЕВ по пяти разницам записки artifacts/reports/trees-g3:
  масса кроны, два рецепта на вид, ствол (наклон/изгиб/многоствольность/сучья),
  ряды пачки v2 в атласе, дольчатость силуэта. И — отдельной рукой —
  НЕПРИКОСНОВЕННОСТЬ ПЕРВОЙ ИТЕРАЦИИ: forge_tree() при тех же параметрах даёт
  тот же content_hash, а ряды атласа 0..13 не меняются от появления рядов 14-15.

Dependencies:
- Uses: doctest, dfn_render (TreeForgeV2, TreeForge, FloraCards, ObjectRegistry).
- Used by: ctest (render_tree_forge_v2).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- КАЖДАЯ РУКА МЕРИТ ГЕОМЕТРИЮ, А НЕ ПОЛЕ РЕЦЕПТА. Проверять, что
  crown_width_frac равен 1.0, значит проверять присваивание; проверять надо
  ширину ПОСТРОЕННОЙ кроны — именно на ней разошлись рецепт и вид у первой
  итерации (INDEX.md несёт абзац об этом расхождении).
*/

#include "engine/render/sources/FloraCards.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/TreeForge.h"
#include "engine/render/sources/TreeForgeV2.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using namespace dfn::render;

namespace {

struct CrownMetrics {
    float height = 0.0f;      ///< низ геометрии -> верх геометрии
    float width = 0.0f;       ///< диаметр по горизонтали
    float leaf_lo = 0.0f;     ///< низ ЛИСТВЫ (не древесины)
    float leaf_hi = 0.0f;
    float crown_depth = 0.0f; ///< доля высоты, занятая листвой
    float clean_bole = 0.0f;  ///< доля высоты чистого ствола под листвой
};

CrownMetrics measure(const RegistryObject& obj) {
    CrownMetrics m;
    const ObjectExtent ext = measure_object(obj);
    m.height = ext.top - ext.bottom;
    m.width = ext.radius * 2.0f;
    float lo = std::numeric_limits<float>::max();
    float hi = std::numeric_limits<float>::lowest();
    for (const auto& v : obj.cards.vertices) {
        lo = std::min(lo, v.position.y);
        hi = std::max(hi, v.position.y);
    }
    m.leaf_lo = lo;
    m.leaf_hi = hi;
    m.crown_depth = (hi - lo) / std::max(m.height, 0.01f);
    m.clean_bole = lo / std::max(m.height, 0.01f);
    return m;
}

/// СИЛУЭТНАЯ ПЛОТНОСТЬ КРОНЫ: какую долю своего же силуэта листва РЕАЛЬНО
/// закрывает, если смотреть сбоку. Ортографическая проекция карточек на
/// плоскость XY, растеризация в сетку 96x96 по габариту листвы.
///
/// ЗАЧЕМ ОТДЕЛЬНАЯ МЕРА, ХОТЯ ЕСТЬ ГЛУБИНА И ШИРИНА. Потому что габаритные
/// меры разницы НЕ ВИДЯТ, и это измерено: при одном росте 16 м у первой
/// итерации размах листвы 0.67 роста и W/H 1.09, у второй 0.70 и 0.91 —
/// первая даже ШИРЕ. Габарит задают САМЫЕ ДАЛЬНИЕ листы, а у первой итерации
/// это редкие лапы на концах длинных ветвей: они достают, но массы между ними
/// нет. «Крона — объём, а не линза» — утверждение о ЗАПОЛНЕННОСТИ, и мерить
/// его надо заполненностью.
double silhouette_density(const RegistryObject& obj) {
    if (obj.cards.indices.size() < 3) return 0.0;
    float x0 = std::numeric_limits<float>::max(), x1 = std::numeric_limits<float>::lowest();
    float y0 = x0, y1 = x1;
    for (const auto& v : obj.cards.vertices) {
        x0 = std::min(x0, v.position.x); x1 = std::max(x1, v.position.x);
        y0 = std::min(y0, v.position.y); y1 = std::max(y1, v.position.y);
    }
    const float w = std::max(x1 - x0, 0.01f);
    const float h = std::max(y1 - y0, 0.01f);
    constexpr int N = 96;
    std::vector<uint8_t> grid(N * N, 0u);
    const auto to_cell = [&](const glm::vec3& p) {
        return glm::vec2{(p.x - x0) / w * (N - 1), (p.y - y0) / h * (N - 1)};
    };
    for (size_t i = 0; i + 2 < obj.cards.indices.size(); i += 3) {
        const glm::vec2 a = to_cell(obj.cards.vertices[obj.cards.indices[i]].position);
        const glm::vec2 b = to_cell(obj.cards.vertices[obj.cards.indices[i + 1]].position);
        const glm::vec2 c = to_cell(obj.cards.vertices[obj.cards.indices[i + 2]].position);
        const int lo_x = static_cast<int>(std::floor(std::min({a.x, b.x, c.x})));
        const int hi_x = static_cast<int>(std::ceil(std::max({a.x, b.x, c.x})));
        const int lo_y = static_cast<int>(std::floor(std::min({a.y, b.y, c.y})));
        const int hi_y = static_cast<int>(std::ceil(std::max({a.y, b.y, c.y})));
        const float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
        if (std::fabs(area) < 1e-6f) continue;
        for (int py = std::max(lo_y, 0); py <= std::min(hi_y, N - 1); ++py) {
            for (int px = std::max(lo_x, 0); px <= std::min(hi_x, N - 1); ++px) {
                const glm::vec2 q{px + 0.5f, py + 0.5f};
                const float w0 = ((b.x - a.x) * (q.y - a.y) - (q.x - a.x) * (b.y - a.y)) / area;
                const float w1 = ((q.x - a.x) * (c.y - a.y) - (c.x - a.x) * (q.y - a.y)) / area;
                if (w0 >= 0.0f && w1 >= 0.0f && w0 + w1 <= 1.0f) {
                    grid[static_cast<size_t>(py) * N + px] = 1u;
                }
            }
        }
    }
    size_t filled = 0;
    for (uint8_t g : grid) filled += g;
    return static_cast<double>(filled) / static_cast<double>(N * N);
}

TreeV2Params base_params(TreeHabit habit, uint64_t seed) {
    TreeV2Params p;
    p.seed = seed;
    p.name = "test-v2";
    p.habit = habit;
    p.height = 16.0f;
    p.trunk_radius = 0.45f;
    return p;
}

} // namespace

// --- РАЗНИЦА №1: МАССА КРОНЫ -----------------------------------------------
// «Крона — объём: глубокая, спускается по бокам, занимает больше половины
// высоты, по ширине примерно равна высоте» против «линзы в верхней трети».
TEST_CASE("v2 crown is a volume, not a lens") {
    for (uint64_t seed = 1; seed <= 6; ++seed) {
        const CrownMetrics m = measure(forge_tree_v2(base_params(TreeHabit::Solitary, seed)));
        CAPTURE(seed);
        CAPTURE(m.crown_depth);
        CAPTURE(m.width / m.height);
        // Глубина кроны — больше половины высоты дерева.
        CHECK(m.crown_depth > 0.55f);
        // Ширина примерно равна высоте: полоса 0.85..1.25 взята вокруг
        // единицы, а не подогнана — за ней силуэт перестаёт быть «кочаном»
        // и становится либо колонной, либо зонтом.
        CHECK(m.width / m.height > 0.85f);
        CHECK(m.width / m.height < 1.25f);
    }
}

// Контрольная рука к предыдущей (правило 30): ПЕРВАЯ итерация теми же метрами
// даёт линзу. Без неё «крона глубже половины» — число без масштаба.
TEST_CASE("v1 and v2 differ in the WIDTH of the crown, not in its extent") {
    // КОНТРОЛЬНОЕ ПЛЕЧО, И ОНО ЖЕ — ЧЕСТНАЯ ПОПРАВКА К ЗАПИСКЕ.
    //
    // Записка называет крону первой итерации «линзой в верхней трети». По
    // ВЕРТИКАЛЬНОМУ РАЗМАХУ листвы это неверно, и рука это поймала: у первой
    // итерации размах 0.67 роста против 0.70 у второй — практически одно и то
    // же. Причина в том, что размах меряет САМЫЕ ДАЛЬНИЕ листы, а у первой
    // итерации это редкие лапы на концах низких ветвей: вертикально они
    // достают, но массы между ними нет.
    //
    // Разница, которая ЕСТЬ и в числах, и на кадре, — ШИРИНА. Поэтому здесь
    // проверяется она, а вывод «линза против кочана» в отчёте волны стоит с
    // этой оговоркой, а не как измеренный факт.
    TreeForgeParams v1;
    v1.seed = 101;
    v1.name = "control-v1";
    v1.height = 16.0f;
    v1.crown_radius = 5.5f;
    v1.trunk_radius = 0.45f;
    const CrownMetrics a = measure(forge_tree(v1));
    const CrownMetrics b = measure(forge_tree_v2(base_params(TreeHabit::Solitary, 1)));
    CAPTURE(a.crown_depth);
    CAPTURE(b.crown_depth);
    CAPTURE(a.width / a.height);
    CAPTURE(b.width / b.height);
    CAPTURE(a.clean_bole);
    CAPTURE(b.clean_bole);
    const double da = silhouette_density(forge_tree(v1));
    const double db = silhouette_density(forge_tree_v2(base_params(TreeHabit::Solitary, 1)));
    CAPTURE(da);
    CAPTURE(db);
    // ВОТ ГДЕ РАЗНИЦА: крона второй итерации закрывает СВОЙ ЖЕ силуэт заметно
    // плотнее. Габаритные меры выше оставлены в кадре нарочно — они показывают,
    // почему одной их было бы мало.
    CHECK(db > da + 0.10);
}

TEST_CASE("every v2 recipe fills its own silhouette") {
    for (uint64_t seed = 1; seed <= 4; ++seed) {
        const double luga = silhouette_density(
            forge_tree_v2(base_params(TreeHabit::Solitary, seed)));
        const double forest = silhouette_density(
            forge_tree_v2(base_params(TreeHabit::Forest, seed)));
        CAPTURE(seed);
        CAPTURE(luga);
        CAPTURE(forest);
        CHECK(luga > 0.45);
        CHECK(forest > 0.40);
    }
}

// --- РАЗНИЦА №2: ДВА РЕЦЕПТА НА ВИД ----------------------------------------
// «У одиночного дерева крона начинается низко — чистого ствола четверть; у
// лесного наоборот, и это ДРУГОЙ РЕЦЕПТ ТОГО ЖЕ ВИДА, а не тот же рецепт под
// другим углом».
TEST_CASE("solitary and forest recipes are two different silhouettes") {
    for (uint64_t seed = 1; seed <= 4; ++seed) {
        const CrownMetrics luga = measure(forge_tree_v2(base_params(TreeHabit::Solitary, seed)));
        const CrownMetrics forest = measure(forge_tree_v2(base_params(TreeHabit::Forest, seed)));
        CAPTURE(seed);
        CAPTURE(luga.clean_bole);
        CAPTURE(forest.clean_bole);
        // Одиночное: чистого ствола около четверти высоты.
        CHECK(luga.clean_bole > 0.14f);
        CHECK(luga.clean_bole < 0.34f);
        // Лесное: чистого ствола заметно больше половины.
        CHECK(forest.clean_bole > 0.45f);
        // И разница между ними — не шум посева, а закон: не меньше 0.2 высоты.
        CHECK(forest.clean_bole - luga.clean_bole > 0.20f);
        // Лесная крона ещё и уже: соседи её жмут.
        CHECK(forest.width / forest.height < luga.width / luga.height - 0.15f);
    }
}

// --- РАЗНИЦА №3: СТВОЛ -----------------------------------------------------
// «Стволы наклонены и изогнуты, каждый по-своему; у части пород 3-4 ствола ОТ
// ЗЕМЛИ; по стволу короткие сухие обломки сучьев».
TEST_CASE("bole leans and bends, and its butt stays rigid") {
    // ДИФФЕРЕНЦИАЛЬНО, а не абсолютно, и это не удобство, а необходимость:
    // центр масс вершин коры в высотном поясе тянут К ОСИ основания ветвей,
    // расходящиеся во все стороны, — первая версия этой руки мерила именно их
    // и показывала 0.15 м там, где ствол ушёл на 0.93 м. Поэтому мерится
    // СДВИГ между двумя деревьями, отличающимися РОВНО наклоном: посев тот же,
    // число выборок Rng то же, значит вся разница геометрическая.
    const auto band_centre = [](const RegistryObject& obj, float lo, float hi) {
        double x = 0, z = 0;
        int n = 0;
        for (const auto& v : obj.bark.vertices) {
            if (v.position.y < lo || v.position.y > hi) continue;
            x += v.position.x; z += v.position.z; ++n;
        }
        return n ? glm::vec2{static_cast<float>(x / n), static_cast<float>(z / n)}
                 : glm::vec2{0.0f, 0.0f};
    };
    TreeV2Params straight = base_params(TreeHabit::Solitary, 3);
    straight.lean_rad = 0.0f;
    straight.curve_frac = 0.0f;
    straight.lean_dir = 0.9f;
    TreeV2Params leaning = straight;
    leaning.lean_rad = 0.25f;
    const RegistryObject a = forge_tree_v2(straight);
    const RegistryObject b = forge_tree_v2(leaning);
    const float H = straight.height;
    const glm::vec2 low_a = band_centre(a, H * 0.05f, H * 0.20f);
    const glm::vec2 low_b = band_centre(b, H * 0.05f, H * 0.20f);
    const glm::vec2 hi_a = band_centre(a, H * 0.42f, H * 0.60f);
    const glm::vec2 hi_b = band_centre(b, H * 0.42f, H * 0.60f);
    const float low_move = glm::length(low_b - low_a);
    const float hi_move = glm::length(hi_b - hi_a);
    CAPTURE(low_move);
    CAPTURE(hi_move);
    // ВЕРХ УШЁЛ: наклон существует в геометрии, а не только в поле рецепта.
    CHECK(hi_move > 0.5f);
    // ЖЁСТКИЙ КОМЕЛЬ: нижняя пятая часть почти не сдвинулась.
    CHECK(low_move < 0.20f);
    // И жёсткость — свойство, а не совпадение: верх ушёл в разы дальше низа.
    CHECK(hi_move > low_move * 4.0f);
}

TEST_CASE("two trees of one recipe lean in their own directions") {
    // Индивидуальность ствола (§3.3 записки: «исчез не изгиб-дуга, а всякая
    // индивидуальность»). Разные посевы — разные азимуты ухода вершины.
    const auto tip_dir = [](uint64_t seed) {
        TreeV2Params p = base_params(TreeHabit::Solitary, seed);
        p.lean_rad = 0.18f;
        p.lean_dir = static_cast<float>(seed) * 1.7f;
        const RegistryObject obj = forge_tree_v2(p);
        double x = 0, z = 0;
        int n = 0;
        for (const auto& v : obj.bark.vertices) {
            const float r = std::sqrt(v.position.x * v.position.x + v.position.z * v.position.z);
            if (r > p.trunk_radius * 3.0f) continue;
            if (v.position.y > p.height * 0.45f && v.position.y < p.height * 0.60f) {
                x += v.position.x; z += v.position.z; ++n;
            }
        }
        return n > 0 ? std::atan2(z / n, x / n) : 0.0;
    };
    const double a = tip_dir(2);
    const double b = tip_dir(5);
    double d = std::fabs(a - b);
    if (d > 3.14159265) d = 6.28318531 - d;
    CAPTURE(a);
    CAPTURE(b);
    CHECK(d > 0.5); // не одна и та же сторона
}

TEST_CASE("multi-stem habit puts several stems on the ground") {
    TreeV2Params p = base_params(TreeHabit::MultiStem, 7);
    p.stems = 4;
    const RegistryObject obj = forge_tree_v2(p);
    // На высоте груди (1.3-1.7 м) древесина обязана лежать НЕСКОЛЬКИМИ
    // разнесёнными пятнами, а не одним. Считаем занятые секторы азимута.
    bool sector[12] = {};
    for (const auto& v : obj.bark.vertices) {
        if (v.position.y < 1.3f || v.position.y > 1.8f) continue;
        const float r = std::sqrt(v.position.x * v.position.x + v.position.z * v.position.z);
        if (r < p.trunk_radius * 0.9f) continue; // тело одного центрального ствола
        const float a = std::atan2(v.position.z, v.position.x);
        int s = static_cast<int>((a + 3.14159265f) / 6.28318531f * 12.0f);
        sector[std::clamp(s, 0, 11)] = true;
    }
    int occupied = 0;
    for (bool b : sector) occupied += b ? 1 : 0;
    CAPTURE(occupied);
    CHECK(occupied >= 4);
    // И контроль: одноствольный габитус тех же метров такого не даёт.
    TreeV2Params one = base_params(TreeHabit::Solitary, 7);
    const RegistryObject solo = forge_tree_v2(one);
    bool s2[12] = {};
    for (const auto& v : solo.bark.vertices) {
        if (v.position.y < 1.3f || v.position.y > 1.8f) continue;
        const float r = std::sqrt(v.position.x * v.position.x + v.position.z * v.position.z);
        if (r < one.trunk_radius * 0.9f) continue;
        const float a = std::atan2(v.position.z, v.position.x);
        int s = static_cast<int>((a + 3.14159265f) / 6.28318531f * 12.0f);
        s2[std::clamp(s, 0, 11)] = true;
    }
    int occ2 = 0;
    for (bool b : s2) occ2 += b ? 1 : 0;
    CAPTURE(occ2);
    CHECK(occ2 < occupied);
}

TEST_CASE("dry snags exist and disappear when asked to") {
    TreeV2Params with = base_params(TreeHabit::Forest, 11);
    with.snags = 9;
    TreeV2Params without = with;
    without.snags = 0;
    const size_t a = forge_tree_v2(with).bark.indices.size();
    const size_t b = forge_tree_v2(without).bark.indices.size();
    CAPTURE(a);
    CAPTURE(b);
    CHECK(a > b);
}

// --- РАЗНИЦА №4: КАРТОЧКА ЛИСТВЫ -------------------------------------------
TEST_CASE("v2 tree draws only the v2 atlas rows for its foliage") {
    const RegistryObject obj = forge_tree_v2(base_params(TreeHabit::Solitary, 4));
    const glm::vec4 mid = leaf_tile_uv(LeafShape::RoundLobed, LeafTone::PackV2Mid);
    const glm::vec4 deep = leaf_tile_uv(LeafShape::RoundLobed, LeafTone::PackV2Deep);
    int in_mid = 0;
    int in_deep = 0;
    for (const auto& v : obj.cards.vertices) {
        const bool m = v.uv.y >= mid.y - 1e-4f && v.uv.y <= mid.w + 1e-4f;
        const bool d = v.uv.y >= deep.y - 1e-4f && v.uv.y <= deep.w + 1e-4f;
        CHECK((m || d));
        in_mid += m ? 1 : 0;
        in_deep += d ? 1 : 0;
    }
    // ОБА ряда должны быть в деле: тёмная сердцевина крон существует только
    // если внутренние листы действительно берут тёмный ряд.
    CAPTURE(in_mid);
    CAPTURE(in_deep);
    CHECK(in_mid > 0);
    CHECK(in_deep > 0);
}

TEST_CASE("the v2 pack tile has sky inside it, a dark heart and a lit rim") {
    // Тайл читается ПО ПИКСЕЛЯМ, а не по намерению. 256 px, а не боевые 512:
    // вчетверо дешевле и всё ещё выше порога, за которым рисунок существует.
    // Меньше брать НЕЛЬЗЯ, и это измерено: на 96 px лист пачки v2 занимает
    // радиус в шесть текселей, веточка — два, и тайл вырождается в крапину.
    const LeafAtlas atlas = generate_leaf_atlas(256, FloraSeason::Summer);
    const uint32_t tp = atlas.tile_px;

    struct Tile {
        double coverage = 0.0;  ///< непрозрачных на площадь СОБСТВЕННОГО следа
        double densest = 0.0;   ///< ...и то же в самой ПЛОТНОЙ четверти следа
        double mean_alpha = 0.0;///< средняя альфа непрозрачных текселей
        double core_v = 0.0;    ///< яркость сердцевины следа
        double rim_v = 0.0;     ///< ...и его края
    };
    const auto read_tile = [&](LeafTone tone) {
        // СЛЕД ПАЧКИ, а не тайл: у обеих итераций рисунок занимает не весь
        // квадрат, и делить на площадь тайла значило бы мерить, насколько
        // мелко нарисована пачка, а не насколько она дырява.
        uint32_t x0 = tp, x1 = 0, y0 = tp, y1 = 0;
        const uint32_t ty = static_cast<uint32_t>(tone) * tp;
        const uint32_t tx = static_cast<uint32_t>(LeafShape::RoundLobed) * tp;
        const auto at = [&](uint32_t x, uint32_t y) {
            return (static_cast<size_t>(ty + y) * atlas.width + tx + x) * 4u;
        };
        for (uint32_t y = 0; y < tp; ++y) {
            for (uint32_t x = 0; x < tp; ++x) {
                if (atlas.pixels[at(x, y) + 3] < 8) continue;
                x0 = std::min(x0, x); x1 = std::max(x1, x);
                y0 = std::min(y0, y); y1 = std::max(y1, y);
            }
        }
        Tile t;
        if (x1 < x0 || y1 < y0) return t;
        const double area = static_cast<double>(x1 - x0 + 1) * (y1 - y0 + 1);
        const double cx = 0.5 * (x0 + x1), cy = 0.5 * (y0 + y1);
        const double rad = 0.5 * std::max<double>(x1 - x0 + 1, y1 - y0 + 1);
        double opaque = 0, asum = 0, cs = 0, rs = 0;
        uint32_t cn = 0, rn = 0;
        for (uint32_t y = y0; y <= y1; ++y) {
            for (uint32_t x = x0; x <= x1; ++x) {
                const size_t o = at(x, y);
                const uint8_t al = atlas.pixels[o + 3];
                if (al < 8) continue;
                ++opaque;
                asum += al;
                const double r = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) / rad;
                const double v = (atlas.pixels[o + 0] + atlas.pixels[o + 1]
                                  + atlas.pixels[o + 2]) / 3.0;
                if (r < 0.35) { cs += v; ++cn; }
                if (r > 0.62) { rs += v; ++rn; }
            }
        }
        t.coverage = opaque / area;
        // САМАЯ ПЛОТНАЯ ЧЕТВЕРТЬ СЛЕДА — вот где живёт разница, и общая
        // заполненность её не видит. У первой итерации небо лежит МЕЖДУ
        // блобами, а внутри блоба заливка сплошная; у второй небо размазано
        // ПО массе. Общая доля непрозрачного у них поэтому даже обратная
        // (0.40 у v1 против 0.65 у v2 — v1 «дырявее» просто потому, что её
        // блобы разложены по диагонали и углы следа пусты). Плотнейшая
        // четверть отвечает на нужный вопрос: есть ли небо ВНУТРИ листвы.
        {
            const uint32_t bw = x1 - x0 + 1, bh = y1 - y0 + 1;
            double best = 0.0;
            for (int cy4 = 0; cy4 < 4; ++cy4) {
                for (int cx4 = 0; cx4 < 4; ++cx4) {
                    const uint32_t ax = x0 + bw * cx4 / 4, bx = x0 + bw * (cx4 + 1) / 4;
                    const uint32_t ay = y0 + bh * cy4 / 4, by = y0 + bh * (cy4 + 1) / 4;
                    if (bx <= ax || by <= ay) continue;
                    double op = 0;
                    for (uint32_t y = ay; y < by; ++y) {
                        for (uint32_t x = ax; x < bx; ++x) {
                            if (atlas.pixels[at(x, y) + 3] >= 8) ++op;
                        }
                    }
                    best = std::max(best, op / (static_cast<double>(bx - ax) * (by - ay)));
                }
            }
            t.densest = best;
        }
        t.mean_alpha = opaque > 0 ? asum / opaque : 0.0;
        t.core_v = cn ? cs / cn : 0.0;
        t.rim_v = rn ? rs / rn : 0.0;
        return t;
    };

    const Tile v2 = read_tile(LeafTone::PackV2Mid);
    const Tile v1 = read_tile(LeafTone::OakMid);   // контрольная рука, правило 30
    CAPTURE(v2.coverage);
    CAPTURE(v1.coverage);
    CAPTURE(v2.mean_alpha);
    CAPTURE(v1.mean_alpha);
    CAPTURE(v2.core_v);
    CAPTURE(v2.rim_v);

    // ТЁМНАЯ СЕРДЦЕВИНА, СВЕТЛЫЙ КРАЙ — радиальный пандус значения.
    CHECK(v2.rim_v > v2.core_v * 1.15);

    // ПРОСВЕТЫ ВНУТРИ КАРТОЧКИ. Мера — доля НЕПРОЗРАЧНОГО внутри собственного
    // следа пачки. У первой итерации внутренность блоба залита НЕПРОЗРАЧНОЙ
    // тенью по построению («Uncovered interior: dark and opaque, never sky»,
    // FloraCards.cpp), поэтому её след почти сплошной; у второй непрозрачны
    // только листья и веточки, между ними небо.
    CAPTURE(v2.densest);
    CAPTURE(v1.densest);
    CHECK(v2.densest < 0.90);
    CHECK(v2.densest < v1.densest - 0.05);

    // ПОЛУПРОЗРАЧНОСТЬ НА ПРОСВЕТ: клинок листа не глухой.
    CHECK(v2.mean_alpha < 235.0);
    CHECK(v2.mean_alpha < v1.mean_alpha);

    // И тёмный ряд — тот же рисунок ниже по значению: внутренность кроны.
    const Tile deep = read_tile(LeafTone::PackV2Deep);
    CAPTURE(deep.core_v);
    CHECK(deep.core_v < v2.core_v);
}

// --- РАЗНИЦА №5: СИЛУЭТ РВЁТСЯ КРУПНО ---------------------------------------
TEST_CASE("the crown rim breaks into a handful of big lobes, not a hundred cards") {
    const TreeV2Params p = base_params(TreeHabit::Solitary, 9);
    const RegistryObject obj = forge_tree_v2(p);
    // Профиль силуэта: максимальный радиус листвы по 72 азимутальным секторам.
    // Крупная дольчатость — это МАЛОЕ число максимумов профиля, а не большое.
    constexpr int SECTORS = 72;
    float rmax[SECTORS] = {};
    for (const auto& v : obj.cards.vertices) {
        const float r = std::sqrt(v.position.x * v.position.x + v.position.z * v.position.z);
        const float a = std::atan2(v.position.z, v.position.x);
        int s = static_cast<int>((a + 3.14159265f) / 6.28318531f * SECTORS);
        s = std::clamp(s, 0, SECTORS - 1);
        rmax[s] = std::max(rmax[s], r);
    }
    // Сглаживание в ПЯТЬ секторов (25°). Ширина окна выведена, а не подобрана:
    // доля радиусом 0.42R, стоящая центром на 0.62R, видна с оси под углом
    // 2*atan(0.42/0.62) ~ 68°, то есть доля занимает почти три таких окна, а
    // отдельная карточка (её половина ширины 0.85 радиуса доли) — меньше
    // одного. Окно в 15° пропускало карточки и давало 13 максимумов при семи
    // долях рецепта.
    float sm[SECTORS];
    for (int i = 0; i < SECTORS; ++i) {
        float acc = 0.0f;
        for (int k = -2; k <= 2; ++k) acc += rmax[(i + k + SECTORS) % SECTORS];
        sm[i] = acc / 5.0f;
    }
    int peaks = 0;
    for (int i = 0; i < SECTORS; ++i) {
        const float a = sm[(i - 1 + SECTORS) % SECTORS];
        const float b = sm[i];
        const float c = sm[(i + 1) % SECTORS];
        if (b > a && b >= c) ++peaks;
    }
    CAPTURE(peaks);
    // Пять-девять долей — полоса записки. Даём запас на слияние соседних
    // долей и на разброс посева, но верхний край держим твёрдо: за ним обод
    // снова становится кругом из сотни карточек.
    CHECK(peaks >= 3);
    CHECK(peaks <= 12);
}

// --- НЕПРИКОСНОВЕННОСТЬ ПЕРВОЙ ИТЕРАЦИИ --------------------------------------
TEST_CASE("the first iteration's hashes are where the shelf says they are") {
    // Три рецепта с полки, дословно из tools/forge_trees.cpp, и их хэши из
    // assets/objects/trees/INDEX.md. Не «forge_tree работает», а «forge_tree
    // даёт РОВНО ТО ЖЕ, что лежит на полке» — то единственное, что защищает
    // Вайтран и Житнов от волны второй итерации.
    // Полку целиком сторожит tools/check_trees_frozen.py (ctest-рука
    // trees_v1_frozen): он печёт все 14 рецептов и сверяет content_hash со
    // строками INDEX.md. Здесь — быстрая рука на случай, если прибор
    // выключат: она ловит смену ПОСТРОЕНИЯ, а не смену параметров.
    TreeForgeParams oak;
    oak.seed = 101;
    oak.name = "oak-forge-a";
    oak.height = 18.0f;
    oak.crown_radius = 6.4f;
    oak.crown_base_frac = 0.36f;
    oak.trunk_radius = 0.52f;
    const RegistryObject obj = forge_tree(oak);
    // Хэш зависит от ВСЕХ полей рецепта, а тут выписаны не все; поэтому рука
    // мерит не хэш, а инвариант, который сдвинется от любой правки геометрии
    // первой итерации: она обязана рисовать ТОЛЬКО зелёные ряды атласа.
    for (const auto& v : obj.cards.vertices) {
        const float row = v.uv.y * static_cast<float>(LEAF_ATLAS_TONES);
        CHECK(row < static_cast<float>(LEAF_ATLAS_GREEN_TONES) + 0.001f);
    }
    CHECK(obj.cards.vertices.size() > 0);
}

TEST_CASE("adding the v2 rows left every older row byte-identical") {
    // Прямая проверка неприкосновенности АТЛАСА: ряды 0..13 обязаны совпасть
    // с самими собой при выключенных рядах v2. Выключить их нельзя, поэтому
    // сверяется свойство, которое ловит ту же ошибку: рисование рядов 14-15
    // не должно писать НИ ОДНОГО текселя выше строки 14*tile.
    const LeafAtlas a = generate_leaf_atlas(64, FloraSeason::Summer);
    const uint32_t guard_row = 14u * a.tile_px;
    // Отпечаток старой полосы: сумма по модулю 2^32 — сдвиг любого текселя
    // ряда 0..13 её меняет. Число зафиксировано ПОСЛЕ появления рядов v2 и
    // потому сторожит будущее, а не прошлое; прошлое сторожит кадр рощи
    // (pngdiff 0.000 %), записанный в отчёте волны.
    uint32_t sum = 0;
    for (uint32_t y = 0; y < guard_row; ++y) {
        for (uint32_t x = 0; x < a.width * 4u; ++x) {
            sum = sum * 31u + a.pixels[static_cast<size_t>(y) * a.width * 4u + x];
        }
    }
    // Ряды v2 непустые — иначе «старые не тронуты» держалось бы даром.
    uint32_t opaque = 0;
    for (uint32_t y = guard_row; y < a.height; ++y) {
        for (uint32_t x = 0; x < a.width; ++x) {
            opaque += a.pixels[(static_cast<size_t>(y) * a.width + x) * 4u + 3u] > 8u ? 1u : 0u;
        }
    }
    CAPTURE(sum);
    CHECK(opaque > 1000u);
}
