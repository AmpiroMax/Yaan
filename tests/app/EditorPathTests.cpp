/*
Module: tests/app
File: tests/app/EditorPathTests.cpp

Responsibility:
- ТРОПА КАК ЛИНИЯ, А НЕ КАК МАЗОК ПО КЛЕТКАМ. Держит ровно то, ради чего работа
  затевалась: диагональная тропа рисуется ДИАГОНАЛЬЮ, и это число, а не мнение
  о скриншоте.

Key claims (каждое с рукой, которая КРАСНЕЕТ, — правило 30):
- изолиния 0.5 износа отходит от настоящей прямой не более чем на 0.15 м, а
  та же тропа, выраженная полем 0/1 (как её выражает КЛАСС поверхности —
  сегодняшний песок), — на 0.4 м и больше. Вторая рука и есть лесенка.
- пол мягкости края (PATH_MIN_FADE_M) выбран между ИЗМЕРЕННЫМИ образцами
  (правило 45): развёртка по ширине полосы спада показывает, где лесенка
  возвращается.
- дуга проходит ЧЕРЕЗ поставленные точки и остаётся дугой после разложения в
  отсчёты;
- тропа, стёртая целиком, не оставляет в канале ни одного отсчёта;
- И ГЛАВНОЕ, ЧЕГО НЕ ВИДЯТ ОСТАЛЬНЫЕ: нарисованный износ ДОЕЗЖАЕТ до отсчётов
  чанка через живой generate_chunk, с рукой «до правки» рядом;
- файл возит ТОЧКИ, а отсчёты пересчитываются: круговой прогон даёт тот же
  канал до последнего отсчёта.

Dependencies:
- Uses: doctest, engine/world (ReliefLayer). NO ImGui, no window, no renderer:
  всё, что здесь проверяется, — решения, а не рисование (правило 3).
- Used by: рукав app_editor_brush (тот же предмет — рука на земле).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- МОДЕЛЬ ОТРИСОВКИ ЗДЕСЬ — НЕ ВЫДУМКА. mesh_sample() повторяет РОВНО то, что
  делает TerrainMesher: значение отсчёта кладётся в вершину, треугольники
  {i00,i11,i10} и {i00,i01,i11}, растеризатор интерполирует линейно. Меняется
  разбиение там — меняется и здесь, иначе проверка перестанет мерить экран.
*/

#include <doctest/doctest.h>

#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/world/sources/ChunkManager.h"
#include "engine/world/sources/ReliefLayer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <glm/geometric.hpp>
#include <vector>

using dfn::world::ReliefLayer;
using dfn::world::ReliefPath;
using dfn::world::RELIEF_STEP_M;

namespace {

constexpr float ISO = 0.5f;

/// ВАЛЮТА ЭТОГО ФАЙЛА: значение в точке так, как его увидит ЭКРАН — линейной
/// интерполяцией по треугольникам TerrainMesher, а не билинейно и не «примерно».
float mesh_sample(const ReliefLayer& layer, glm::vec2 p) {
    const int32_t x0 = dfn::world::relief_index_floor(p.x);
    const int32_t z0 = dfn::world::relief_index_floor(p.y);
    const float u = (p.x - dfn::world::relief_world_of(x0)) / RELIEF_STEP_M;
    const float w = (p.y - dfn::world::relief_world_of(z0)) / RELIEF_STEP_M;
    const float f00 = layer.path_wear_of(x0, z0);
    const float f10 = layer.path_wear_of(x0 + 1, z0);
    const float f01 = layer.path_wear_of(x0, z0 + 1);
    const float f11 = layer.path_wear_of(x0 + 1, z0 + 1);
    if (w <= u) {
        return f00 + (f10 - f00) * u + (f11 - f10) * w;
    }
    return f00 + (f01 - f00) * w + (f11 - f01) * u;
}

/// Где изолиния ISO пересекает поперечник в точке `c` по нормали `n`, в метрах
/// от `c`. Отрицательное — изолинии на поперечнике нет.
float iso_offset(const ReliefLayer& layer, glm::vec2 c, glm::vec2 n, float reach_m) {
    if (mesh_sample(layer, c) < ISO || mesh_sample(layer, c + n * reach_m) > ISO) {
        return -1.0f;
    }
    float lo = 0.0f;
    float hi = reach_m;
    for (int i = 0; i < 40; ++i) {
        const float mid = 0.5f * (lo + hi);
        if (mesh_sample(layer, c + n * mid) > ISO) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return 0.5f * (lo + hi);
}

struct Deviation {
    float max_m = 0.0f;
    float rms_m = 0.0f;
    int samples = 0;
};

/// Отклонение изолинии от НАСТОЯЩЕЙ прямой края, вдоль всей тропы.
Deviation straight_deviation(const ReliefLayer& layer, glm::vec2 a, glm::vec2 b,
                             float true_edge_m) {
    const glm::vec2 dir = glm::normalize(b - a);
    const glm::vec2 nrm{-dir.y, dir.x};
    const float len = glm::length(b - a);
    Deviation d;
    double acc = 0.0;
    for (float t = 4.0f; t <= len - 4.0f; t += 0.05f) {
        const float off = iso_offset(layer, a + dir * t, nrm, 4.0f * true_edge_m);
        if (off < 0.0f) {
            continue;
        }
        const float dev = off - true_edge_m;
        d.max_m = std::max(d.max_m, std::fabs(dev));
        acc += static_cast<double>(dev) * dev;
        ++d.samples;
    }
    d.rms_m = d.samples > 0 ? static_cast<float>(std::sqrt(acc / d.samples)) : 0.0f;
    return d;
}

/// ЛЕСЕНКА КАК КОНТРОЛЬ: тот же коридор, но выраженный ПЕРЕЧИСЛЕНИЕМ — отсчёт
/// либо тропа, либо нет. Ровно то, что делает класс поверхности, и ровно то,
/// на что жаловался пользователь про песок.
void stamp_binary_corridor(ReliefLayer& layer, glm::vec2 a, glm::vec2 b, float half_m) {
    const glm::vec2 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    for (int32_t z = -20; z <= 80; ++z) {
        for (int32_t x = -20; x <= 80; ++x) {
            const glm::vec2 p{dfn::world::relief_world_of(x), dfn::world::relief_world_of(z)};
            const float t = std::clamp(glm::dot(p - a, ab) / len2, 0.0f, 1.0f);
            const float d = glm::length(p - (a + ab * t));
            layer.set_path_wear(x, z, d <= half_m ? 1.0f : 0.0f);
        }
    }
}

ReliefPath straight_path(float deg, float half_m, float softness) {
    const float r = deg * 3.14159265358979f / 180.0f;
    ReliefPath p;
    p.points = {{0.0f, 0.0f}, {60.0f * std::cos(r), 60.0f * std::sin(r)}};
    p.half_width_m = half_m;
    p.edge_softness = softness;
    return p;
}

} // namespace

TEST_CASE("тропа: диагональ рисуется диагональю, а не лесенкой") {
    // ЧТО ИМЕННО ДОКАЗЫВАЕТСЯ. Экран рисует износ ПОСЛЕ линейной интерполяции
    // по треугольникам, так что видимый край — изолиния 0.5. Значит вопрос
    // «лесенка или нет» это вопрос «насколько эта изолиния отходит от прямой»,
    // и у него есть число.
    constexpr float HALF = 1.5f;
    // Край настоящей тропы: профиль 1-u^2 равен 0.5 при u = sqrt(0.5), а спад
    // при мягкости 1 начинается на осевой линии.
    const float true_edge = HALF * std::sqrt(0.5f);

    const float ANGLES[] = {7.0f, 15.0f, 22.5f, 30.0f, 63.4f};
    float worst_curve = 0.0f;
    float worst_binary = 0.0f;
    for (const float deg : ANGLES) {
        const ReliefPath path = straight_path(deg, HALF, 1.0f);
        const glm::vec2 a = path.points.front();
        const glm::vec2 b = path.points.back();

        ReliefLayer curve;
        curve.add_path(path);
        const Deviation dc = straight_deviation(curve, a, b, true_edge);
        REQUIRE(dc.samples > 900);

        // РУКА, КОТОРАЯ ОБЯЗАНА БЫТЬ КРАСНОЙ (правило 30): тот же коридор
        // полем 0/1. Без неё утверждение «0.12 м это хорошо» не с чем сравнить.
        ReliefLayer binary;
        stamp_binary_corridor(binary, a, b, true_edge);
        const Deviation db = straight_deviation(binary, a, b, true_edge);
        REQUIRE(db.samples > 900);

        std::printf("[тропа] %5.1f°: кривая max %.4f м (скз %.4f) | клетки 0/1 "
                    "max %.4f м (скз %.4f)\n",
                    static_cast<double>(deg), static_cast<double>(dc.max_m),
                    static_cast<double>(dc.rms_m), static_cast<double>(db.max_m),
                    static_cast<double>(db.rms_m));
        worst_curve = std::max(worst_curve, dc.max_m);
        worst_binary = std::max(worst_binary, db.max_m);
    }
    // ПОРОГИ МЕЖДУ ИЗМЕРЕННЫМИ ОБРАЗЦАМИ (правило 45): измерено 0.123 против
    // 0.493, порог посередине по порядку величины и с запасом в обе стороны.
    CHECK(worst_curve < 0.20f);
    CHECK(worst_binary > 0.35f);
    // И РАЗНИЦА МЕЖДУ РУКАМИ, а не только каждая по себе: клетки врут в разы,
    // и это то самое «по квадратам», от которого пользователь и отказался.
    CHECK(worst_binary > 2.5f * worst_curve);
}

TEST_CASE("тропа: пол мягкости края выбран между измеренными образцами") {
    // ПОЧЕМУ ЭТО ЧИСЛО ВООБЩЕ ЕСТЬ. Мягкость — доля полуширины, но лесенка
    // возвращается не при какой-то доле, а при ширине полосы спада МЕНЬШЕ
    // решётки: интерполировать нечем, если между «есть» и «нет» нет ни одного
    // отсчёта. Развёртка ниже — это и есть образцы, между которыми стоит порог.
    constexpr float HALF = 4.0f;
    const float BANDS[] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f};
    float prev = 1.0e9f;
    for (const float band : BANDS) {
        ReliefPath path = straight_path(22.5f, HALF, band / HALF);
        // Настоящий край: у профиля 1-u^2 значение 0.5 стоит на u = sqrt(0.5)
        // ВНУТРИ полосы спада, а плоская вершина занимает остальное.
        const float flat = HALF - std::max(band, dfn::world::PATH_MIN_FADE_M);
        const float true_edge = flat + std::max(band, dfn::world::PATH_MIN_FADE_M)
                                         * std::sqrt(0.5f);
        ReliefLayer layer;
        layer.add_path(path);
        const Deviation d = straight_deviation(layer, path.points.front(),
                                               path.points.back(), true_edge);
        REQUIRE(d.samples > 900);
        std::printf("[тропа] полоса спада %.2f м -> отклонение max %.4f м\n",
                    static_cast<double>(band), static_cast<double>(d.max_m));
        // Шире полоса — ровнее край, монотонно. Если это перестанет быть так,
        // значит спад считается не по той формуле, по которой рисуется.
        CHECK(d.max_m <= prev + 0.01f);
        prev = d.max_m;
        // И ГЛАВНОЕ: даже САМАЯ узкая просьба не даёт лесенки, потому что пол
        // PATH_MIN_FADE_M её не пускает. Контрфакт этого утверждения — рука
        // 0/1 из предыдущего случая: 0.49 м.
        CHECK(d.max_m < 0.20f);
    }
}

TEST_CASE("тропа: дуга проходит через поставленные точки") {
    ReliefPath path;
    path.points = {{0.0f, 0.0f}, {10.0f, 6.0f}, {22.0f, 2.0f}, {31.0f, 14.0f}};
    path.half_width_m = 2.0f;
    const std::vector<glm::vec2> poly = dfn::world::relief_path_polyline(path, 0.5f);
    REQUIRE(poly.size() > 60);
    for (const glm::vec2& p : path.points) {
        float best = 1.0e9f;
        for (const glm::vec2& q : poly) {
            best = std::min(best, glm::length(q - p));
        }
        // ЧЕРЕЗ, А НЕ МИМО. Безье с ручками вне кривой промахнулся бы здесь на
        // метры, и целиться в неё было бы нельзя.
        CHECK(best < 1.0e-3f);
    }
    // И ЭТО НЕ ЛОМАНАЯ: между вторым и третьим узлом дуга обязана отойти от
    // прямой, иначе «кривая» — это слово, а не форма.
    float bulge = 0.0f;
    const glm::vec2 a = path.points[1];
    const glm::vec2 b = path.points[2];
    const glm::vec2 ab = b - a;
    for (const glm::vec2& q : poly) {
        const float t = glm::dot(q - a, ab) / glm::dot(ab, ab);
        if (t < 0.05f || t > 0.95f) {
            continue;
        }
        bulge = std::max(bulge, glm::length(q - (a + ab * t)));
    }
    std::printf("[тропа] прогиб дуги между узлами 2 и 3: %.3f м\n",
                static_cast<double>(bulge));
    CHECK(bulge > 0.20f);
}

TEST_CASE("тропа: канал пуст, пока тропы нет, и пуст снова, когда её стёрли") {
    ReliefLayer layer;
    CHECK_FALSE(layer.has_path_wear());
    CHECK(layer.path_wear_at({12.0f, -4.0f}) == 0.0f);

    const std::size_t idx = layer.add_path(straight_path(30.0f, 1.5f, 1.0f));
    CHECK(layer.has_path_wear());
    const std::size_t worn = layer.path_wear_size();
    // 190 отсчётов на 60 м тропы полуширины 1.5 м — это и есть разрежённость:
    // канал держит только то, что тропа тронула, а не поле нулей на карту.
    CHECK(worn > 150);

    // ВТОРАЯ ТРОПА ПОВЕРХ ПЕРВОЙ — ПЕРЕСЕЧЕНИЕ, А НЕ СУММА.
    ReliefPath cross = straight_path(30.0f, 1.5f, 1.0f);
    cross.points = {{40.0f, 0.0f}, {0.0f, 30.0f}};
    layer.add_path(cross);
    float peak = 0.0f;
    for (int32_t z = -5; z <= 40; ++z) {
        for (int32_t x = -5; x <= 60; ++x) {
            peak = std::max(peak, layer.path_wear_of(x, z));
        }
    }
    CHECK(peak <= 1.0f);

    layer.erase_path(1);
    CHECK(layer.path_wear_size() == worn);
    layer.erase_path(idx);
    // СТЁРТАЯ ПРАВКА НЕ ОСТАВЛЯЕТ СЛЕДА — ни одного отсчёта, а не «почти ноль».
    CHECK_FALSE(layer.has_path_wear());
    CHECK(layer.path_wear_size() == 0);
    CHECK(layer.paths().empty());
}

TEST_CASE("тропа: файл возит точки, отсчёты пересчитываются") {
    ReliefLayer written;
    ReliefPath a = straight_path(22.5f, 2.25f, 0.6f);
    a.points.push_back({70.0f, 40.0f});
    written.add_path(a);
    written.set_delta(3, 4, 1.25f); // высота — свой канал, обязана уцелеть рядом

    const std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "dfn_path_roundtrip.relief";
    REQUIRE(dfn::world::write_relief(written, tmp));

    ReliefLayer read;
    std::string error;
    REQUIRE_MESSAGE(dfn::world::read_relief(tmp, read, error), error);
    REQUIRE(read.paths().size() == 1);
    REQUIRE(read.paths()[0].points.size() == a.points.size());
    for (std::size_t i = 0; i < a.points.size(); ++i) {
        CHECK(read.paths()[0].points[i].x == doctest::Approx(a.points[i].x));
        CHECK(read.paths()[0].points[i].y == doctest::Approx(a.points[i].y));
    }
    CHECK(read.delta_at(3, 4) == doctest::Approx(1.25f));
    // КАНАЛ СОШЁЛСЯ ДО ОТСЧЁТА. Файл не возит износ — значит эта проверка
    // держит то, что пересчёт при чтении даёт ровно то же поле.
    REQUIRE(read.path_wear_size() == written.path_wear_size());
    int compared = 0;
    for (int32_t z = -10; z <= 60; ++z) {
        for (int32_t x = -10; x <= 90; ++x) {
            const float w = written.path_wear_of(x, z);
            if (w > 0.0f) {
                ++compared;
                REQUIRE(read.path_wear_of(x, z) == doctest::Approx(w).epsilon(0.002));
            }
        }
    }
    CHECK(compared > 150);
    std::filesystem::remove(tmp);
}

TEST_CASE("тропа: узел под прицелом — ближайший, а не первый попавшийся") {
    ReliefPath p;
    p.points = {{0.0f, 0.0f}, {2.0f, 0.0f}, {20.0f, 0.0f}};
    CHECK(dfn::world::relief_path_pick(p, {1.9f, 0.1f}, 3.0f) == 1);
    CHECK(dfn::world::relief_path_pick(p, {0.1f, 0.1f}, 3.0f) == 0);
    // Дальше хватки — «ничего», и это отдельный ответ, а не нулевой узел:
    // иначе щелчок в стороне таскал бы первую точку тропы через всю карту.
    CHECK(dfn::world::relief_path_pick(p, {10.0f, 0.0f}, 3.0f) == p.points.size());
}

// ====================== И ДОЕЗЖАЕТ ЛИ ЭТО ДО ЗЕМЛИ ==========================
//
// Всё выше меряет СЛОЙ ПРАВОК. Слой, из которого земля ничего не берёт, — это
// запись в файле, а не тропа, и разницу между ними не видно ни в одной из
// проверок выше. Ниже — единственная, которая её видит: живой ChunkManager,
// настоящий generate_chunk, и вопрос «что лежит в surface.path_wear того
// чанка, по которому проведена тропа».

TEST_CASE("тропа: нарисованная рукой доезжает до отсчётов чанка") {
    dfn::ecs::World ecs;
    dfn::events::EventBus bus;
    dfn::world::ChunkManager chunks;
    chunks.open_generated(dfn::world::WorldGenParams{123, {-10, -10}, {10, 10}},
                          dfn::world::ChunkStreamingParams{0, 1});
    for (int i = 0; i < 64; ++i) {
        const std::size_t before = chunks.loaded_chunks().size();
        chunks.update({48.0f, 0.0f, 48.0f}, ecs, bus);
        if (chunks.loaded_chunks().size() == before) {
            break;
        }
    }
    REQUIRE(chunks.loaded_chunks().size() >= 1);
    const dfn::world::ChunkCoord home{0, 0};
    REQUIRE(chunks.is_loaded(home));

    // ДИАГОНАЛЬ ВНУТРИ ОДНОГО ЧАНКА, наискось к решётке — то есть ровно то, что
    // пользователь и просил провести «не по квадратам».
    ReliefPath path;
    path.points = {{20.0f, 20.0f}, {60.0f, 44.0f}, {96.0f, 96.0f}};
    path.half_width_m = 2.0f;

    const auto wear_along = [&]() {
        const auto sf = chunks.surfacefield(home);
        REQUIRE(sf.has_value());
        if (sf->path_wear.empty()) {
            return 0.0f; // «троп на этом мире нет» — законный ответ, и это ноль
        }
        float peak = 0.0f;
        for (const glm::vec2& p : dfn::world::relief_path_polyline(path, 2.0f)) {
            const int gx = static_cast<int>(std::lround((p.x - sf->origin.x) / sf->step));
            const int gz = static_cast<int>(std::lround((p.y - sf->origin.y) / sf->step));
            const int res = static_cast<int>(sf->resolution);
            if (gx < 0 || gz < 0 || gx >= res || gz >= res) {
                continue;
            }
            peak = std::max(peak, sf->path_wear[static_cast<std::size_t>(gz) * sf->resolution
                                                + static_cast<std::size_t>(gx)]);
        }
        return peak;
    };

    // РУКА «ДО»: на этой земле по этой линии износа нет. Без неё «после» ничего
    // не значит — карта могла нести сгенерированную тропу ровно там же.
    const float before = wear_along();
    std::printf("[тропа] износ на линии ДО правки: %.3f\n",
                static_cast<double>(before));
    CHECK(before < 0.05f);

    dfn::world::ReliefLayer relief;
    (void)relief.add_path(path);
    chunks.set_composed_relief(relief);
    glm::vec2 lo{0.0f};
    glm::vec2 hi{0.0f};
    REQUIRE(dfn::world::relief_path_bounds(path, lo, hi));
    CHECK(chunks.invalidate_area(lo, hi) >= 1);
    for (int i = 0; i < 16 && chunks.rebuild_dirty(ecs, bus, 2) > 0; ++i) {
    }

    const float after = wear_along();
    std::printf("[тропа] износ на линии ПОСЛЕ правки: %.3f\n",
                static_cast<double>(after));
    // Осевая линия обязана быть изношена ПОЛНОСТЬЮ: полуширина 2 м на решётке
    // 1 м — на осевую попадают отсчёты, а не промежутки.
    CHECK(after > 0.9f);
}
