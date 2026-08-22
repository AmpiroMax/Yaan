/*
Created: 18:08:2026 - 18:12:54
Last updated: 23:08:2026 - 02:52:13
Module: tests
File: tests/core/HouseStyleTests.cpp

Responsibility:
- СТИЛЬ СТЕНЫ КАК ПОРОЖДЕНИЕ, А НЕ РАСТЯНУТЫЙ МЕШ. Держит требование
  пользователя ЧИСЛАМИ И БЕЗ ОКНА: обшивка тянется шагом в метрах, раскос
  меняет угол и растягивается, проём размера НЕ МЕНЯЕТ, стоит по центру
  симметрично, «сколько влезло» говорится вслух, а доска не проходит сквозь
  проём.

Key items:
- Правило важнее всех: растяжение стены вдвое не меняет окно НИ НА 0.1 ММ,
  тогда как уже испечённый набор растягивает его РОВНО ВДВОЕ.
- Полоса фактического угла раскоса, ВЫВЕДЕННАЯ из округления, и сметка обоих
  её концов против руки с неизменным числом пролётов.
- Симметрия ряда проёмов ЧИСЛОМ (сумма зеркальных центров равна длине стены).
- Пересечения обшивки с проёмом: 0 против 10 у неразрезанных колонн.

Dependencies:
- Uses: doctest, dfn_world.
- Used by: ctest (test_house_style).

AI Agents Notice (must follow):
- Правило 30: у каждого утверждения тут есть рука, которая обязана краснеть, и
  она считает ЧИСЛО, а не говорит «стало лучше».
- ОТВЕРГНУТЫЙ ОБРАЗЕЦ ЗДЕСЬ НАСТОЯЩИЙ (правило 45): это не выдуманный
  худший случай, а формула из PartForgeWalls.cpp:holes_of(), которая сегодня
  печёт стеновые панели набора. Она воспроизведена здесь И СЧИТАЕТСЯ В ТОМ ЖЕ
  БИНАРНИКЕ (правило 47), потому что «раньше было плохо» без числа — это
  воспоминание, а не контроль.
*/
/*
UPD:
- 18:08:2026 - 18:12:54: Создан вместе с HouseStyle.
- 23:08:2026 - 02:52:13: контракт зазора: между досками ровно HOUSE_BOARD_GAP_M (§2 уточнён 23.08), лицо доски = шаг − зазор.
*/

#include <doctest/doctest.h>

#include "engine/world/sources/HouseStyle.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using dfn::world::BoardRun;
using dfn::world::BracePlacement;
using dfn::world::LayoutFinding;
using dfn::world::LayoutIssue;
using dfn::world::OpeningKind;
using dfn::world::OpeningPlacement;
using dfn::world::WallLayout;
using dfn::world::WallSpec;
using dfn::world::WallStyle;
using dfn::world::lay_out_wall;
using dfn::world::parse_wall_styles;
using dfn::world::style_pier;
using dfn::world::style_pitch;
using dfn::world::wall_opening_capacity;

namespace {

constexpr float STEP = dfn::world::HOUSE_BOARD_STEP_DEFAULT; // 0.23
constexpr float POST_R = 0.12f;
constexpr float WALL_H = 3.25f; // ОСНОВНАЯ ВЫСОТА ЖИЛЬЯ, HOUSES.md §6 (13 клеток)
constexpr float DEG = 57.29577951308232f;

/// Полоса фактического угла раскоса, ВЫВЕДЕННАЯ в HouseStyle.h из округления к
/// ближайшему: atan(tan(60°)/1.5) и atan(tan(60°)/0.5). Здесь стоят числа, а не
/// вызов той же формулы, — иначе рукав проверял бы собственную арифметику.
constexpr float BRACE_DEG_MIN = 49.11f;
constexpr float BRACE_DEG_MAX = 73.90f;

/// Квантование до 0.1 мм — ровно та точность, с которой числа переживают файл
/// (.dfh печатает "%.4f"). «Не изменилось» проверяется ею, а не eq на float.
std::int64_t quant(float v) { return static_cast<std::int64_t>(std::llround(v * 10000.0f)); }

WallStyle plain_style() {
    WallStyle s;
    s.name = "plain";
    return s; // шаг 0.23, зазор 0, раскос 60°, проёмов нет
}

WallStyle window_style() {
    WallStyle s = plain_style();
    s.name = "win";
    s.opening = OpeningKind::Window; // 1.00 x 1.00, подоконник 1.10
    return s;
}

WallSpec wall(float len, int openings = 0, float height = WALL_H) {
    WallSpec s;
    s.length = len;
    s.height = height;
    s.openings = openings;
    s.post_radius = POST_R;
    return s;
}

bool rects_overlap(float a0, float a1, float b0, float b1, float c0, float c1, float d0,
                   float d1) {
    const float eps = 1e-5f;
    return a1 > c0 + eps && a0 < c1 - eps && b1 > d0 + eps && b0 < d1 - eps;
}

// -- ОТВЕРГНУТЫЙ ОБРАЗЕЦ, СПИСАННЫЙ С ЖИВОГО КОДА ---------------------------
// engine/render/sources/PartForgeWalls.cpp, holes_of(): ширина окна берётся как
// min(WIN_W_M, w * 0.30f), а два окна ставятся в точках 0.30w и 0.70w. То есть
// и размер проёма, и расстояние между проёмами ЕДУТ ВМЕСТЕ С ДЛИНОЙ СТЕНЫ —
// ровно то поведение, которое пользователь отверг словами «окна менять размер
// не должны». Формула воспроизведена здесь ради одного: чтобы у утверждения
// «новое правило это чинит» был измеренный, а не вспоминаемый противовес.
float legacy_window_width(float wall_len) { return std::min(1.0f, wall_len * 0.30f); }
float legacy_two_window_gap(float wall_len) { return wall_len * 0.40f; }

} // namespace

TEST_CASE("обшивка тянется: на длине L ложится floor((L - зазоры)/шаг) досок") {
    // Решение пользователя: шаг задан В МЕТРАХ, «ну и пусть не влезает, текстуру
    // покручу». Значит счёт досок — это floor, а не подгонка шага под длину.
    struct Row {
        float len;
        int columns; // посчитано руками: floor(len / 0.23)
    };
    const Row rows[] = {{1.00f, 4},  {2.30f, 10}, {3.00f, 13}, {4.60f, 20},
                        {6.00f, 26}, {7.25f, 31}, {12.00f, 52}};

    const WallStyle st = plain_style();
    int overshoot_ours = 0;
    int overshoot_greedy = 0;
    int gap_count = 0;
    float worst_asym = 0.0f;

    for (const Row& r : rows) {
        const WallLayout lay = lay_out_wall(wall(r.len), st);
        CHECK(lay.board_columns == r.columns);
        REQUIRE(!lay.boards.empty());

        // КРАЙНИЕ НЕ ВЫЛЕЗАЮТ ЗА ТОРЦЫ.
        if (lay.boards.front().u0 < -1e-5f || lay.boards.back().u1 > r.len + 1e-5f) {
            ++overshoot_ours;
        }
        // МЕЖДУ СОСЕДНИМИ ДОСКАМИ — РОВНО HOUSE_BOARD_GAP_M (23.08, §2
        // уточнён: за досками сплошная пластина, канавка 12 мм обязана быть —
        // без неё фасад читался сплошной плитой). Считаем пары, где зазор НЕ
        // равен контрактному.
        for (std::size_t i = 0; i + 1 < lay.boards.size(); ++i) {
            if (quant(lay.boards[i + 1].u0 - lay.boards[i].u1)
                != quant(dfn::world::HOUSE_BOARD_GAP_M)) {
                ++gap_count;
            }
        }
        // ОСТАТОК ДЕЛИТСЯ ПОРОВНУ: слева ровно столько же голой стены, сколько
        // справа. Иначе стена перестала бы быть симметричной ради остатка.
        worst_asym = std::max(
            worst_asym, std::fabs(lay.boards.front().u0 - (r.len - lay.boards.back().u1)));

        // КОНТРОЛЬ (правило 30): прибор обязан уметь покраснеть. Положим на одну
        // доску больше и прижмём ряд к левому торцу — вылезает на КАЖДОЙ длине.
        if (static_cast<float>(r.columns + 1) * STEP > r.len + 1e-5f) {
            ++overshoot_greedy;
        }
    }
    CHECK(overshoot_ours == 0);
    CHECK(gap_count == 0);
    CHECK(worst_asym < 1e-5f);
    CHECK(overshoot_greedy == 7); // все семь длин

    // ВТОРАЯ КРАСНАЯ РУКА, И ОНА ПРО САМО ТРЕБОВАНИЕ: шаг, заданный ДОЛЕЙ длины,
    // ведёт себя как растянутый меш — доска на шестиметровой стене вдвое шире,
    // чем на трёхметровой. Наш шаг не меняется вовсе.
    const WallLayout l3 = lay_out_wall(wall(3.00f), st);
    const WallLayout l6 = lay_out_wall(wall(6.00f), st);
    const float ours3 = l3.boards.front().u1 - l3.boards.front().u0;
    const float ours6 = l6.boards.front().u1 - l6.boards.front().u0;
    CHECK(quant(ours3) == quant(ours6));
    CHECK(quant(ours3) == 2300 - quant(dfn::world::HOUSE_BOARD_GAP_M)); // лицо = шаг − зазор
    const float frac3 = 3.00f / 13.0f;
    const float frac6 = 6.00f / 13.0f;
    CHECK(frac6 / frac3 == doctest::Approx(2.0f).epsilon(1e-5));
    // Растяжение добавляет ДОСОК, а не ширины: 13 -> 26.
    CHECK(l3.board_columns == 13);
    CHECK(l6.board_columns == 26);
}

TEST_CASE("просили три окна, влезло два — и это СКАЗАНО, а не проглочено") {
    const WallStyle st = window_style();
    // Выведенные значения стиля: простенок — один шаг доски, шаг ряда — самый
    // плотный законный.
    CHECK(style_pier(st) == doctest::Approx(0.23f).epsilon(1e-4));
    CHECK(style_pitch(st) == doctest::Approx(1.23f).epsilon(1e-4));

    // ПОРОГ МЕЖДУ ИЗМЕРЕННЫМИ ОБРАЗЦАМИ (правило 45): три окна требуют
    // 2*1.23 + 1.00 + 2*0.23 = 3.92 м, и на 3.92 они встают, а на 3.90 — нет.
    CHECK(wall_opening_capacity(3.92f, st) == 3);
    CHECK(wall_opening_capacity(3.90f, st) == 2);
    CHECK(wall_opening_capacity(3.00f, st) == 2);
    CHECK(wall_opening_capacity(1.46f, st) == 1);
    CHECK(wall_opening_capacity(1.40f, st) == 0);

    const WallLayout tight = lay_out_wall(wall(3.00f, 3), st);
    CHECK(tight.openings_requested == 3);
    CHECK(tight.openings_placed == 2);
    CHECK(tight.openings.size() == 2u);

    const LayoutFinding* dropped = tight.find(LayoutIssue::OpeningsDropped);
    REQUIRE(dropped != nullptr);
    CHECK(dropped->requested == 3);
    CHECK(dropped->placed == 2);
    CHECK(dropped->value == doctest::Approx(3.92f).epsilon(0.01));
    CHECK(dropped->what.find("просили 3, влезло 2") != std::string::npos);
    CHECK(dropped->what.find("3.92") != std::string::npos);

    // КОНТРОЛЬ: там, где влезли все три, находки НЕТ. Находка, которая есть
    // всегда, ничего не сообщает.
    const WallLayout roomy = lay_out_wall(wall(4.00f, 3), st);
    CHECK(roomy.openings_placed == 3);
    CHECK(roomy.openings.size() == 3u);
    CHECK(!roomy.has(LayoutIssue::OpeningsDropped));

    // ВТОРАЯ ПРИЧИНА НЕ ВЛЕЗТЬ — ВЫСОТА, и она названа своим именем, а не
    // свалена в общую «не влезло». Подоконник 1.10 + окно 1.00 = 2.10.
    const WallLayout low = lay_out_wall(wall(6.00f, 2, 1.50f), st);
    CHECK(low.openings_placed == 0);
    const LayoutFinding* tall = low.find(LayoutIssue::OpeningTooTall);
    REQUIRE(tall != nullptr);
    CHECK(tall->value == doctest::Approx(2.10f).epsilon(1e-3));
    const LayoutFinding* low_dropped = low.find(LayoutIssue::OpeningsDropped);
    REQUIRE(low_dropped != nullptr);
    CHECK(low_dropped->what.find("ВЫСОТЕ") != std::string::npos);
    // КОНТРОЛЬ ТОЙ ЖЕ ПАРЫ: стена на 0.7 м выше — обе находки исчезают.
    const WallLayout tallish = lay_out_wall(wall(6.00f, 2, 2.20f), st);
    CHECK(tallish.openings_placed == 2);
    CHECK(!tallish.has(LayoutIssue::OpeningTooTall));
    CHECK(!tallish.has(LayoutIssue::OpeningsDropped));
}

TEST_CASE("окна стоят СИММЕТРИЧНО, и одно стоит РОВНО в центре") {
    const WallStyle st = window_style();

    // ОДНО ОКНО — РОВНО ЦЕНТР. Проверяется числом на пяти длинах, включая
    // некруглую: «примерно в центре» и «в центре» — разные утверждения.
    for (const float len : {2.00f, 3.00f, 4.37f, 6.00f, 11.50f}) {
        const WallLayout lay = lay_out_wall(wall(len, 1), st);
        REQUIRE(lay.openings.size() == 1u);
        CHECK(quant(lay.openings[0].center_u()) == quant(len * 0.5f));
    }

    // РЯД ЛЮБОЙ ДЛИНЫ СИММЕТРИЧЕН: сумма зеркальных центров равна длине стены.
    float worst_ours = 0.0f;
    float worst_left_packed = 0.0f;
    const float len = 12.0f;
    for (int n = 1; n <= 5; ++n) {
        const WallLayout lay = lay_out_wall(wall(len, n), st);
        REQUIRE(static_cast<int>(lay.openings.size()) == n);
        for (int i = 0; i < n; ++i) {
            const float sum =
                lay.openings[i].center_u() + lay.openings[n - 1 - i].center_u();
            worst_ours = std::max(worst_ours, std::fabs(sum - len));

            // КОНТРФАКТ: тот же ряд, прижатый к левому простенку. Он тоже «ставит
            // столько, сколько влезло», и тоже не меняет размер окна — то есть
            // проходит все прочие проверки этого рукава и валится ровно здесь.
            const float packed_i = 0.23f + 0.5f + static_cast<float>(i) * 1.23f;
            const float packed_j = 0.23f + 0.5f + static_cast<float>(n - 1 - i) * 1.23f;
            worst_left_packed =
                std::max(worst_left_packed, std::fabs(packed_i + packed_j - len));
        }
    }
    CHECK(worst_ours < 1e-4f);
    CHECK(worst_left_packed > 10.0f); // измерено: 10.54 м на одном окне

    // И РЯД НЕ ВЫЛЕЗАЕТ ЗА ПРОСТЕНКИ: у крайних проёмов до торца не меньше 0.23.
    const WallLayout five = lay_out_wall(wall(len, 5), st);
    REQUIRE(five.openings.size() == 5u);
    CHECK(five.openings.front().u0 >= 0.23f - 1e-4f);
    CHECK(five.openings.back().u1 <= len - 0.23f + 1e-4f);
}

TEST_CASE("обшивка НЕ ПРОХОДИТ сквозь проём, а кончается у него") {
    const WallStyle st = window_style();
    const WallLayout lay = lay_out_wall(wall(6.00f, 2), st);
    REQUIRE(lay.openings.size() == 2u);
    CHECK(lay.board_columns == 26);

    int hits = 0;
    for (const BoardRun& b : lay.boards) {
        for (const OpeningPlacement& o : lay.openings) {
            if (rects_overlap(b.u0, b.u1, b.v0, b.v1, o.u0, o.u1, o.v0, o.v1)) {
                ++hits;
            }
        }
    }
    CHECK(hits == 0);

    // КОНТРФАКТ, СЧИТАЮЩИЙ ЧИСЛО: те же самые колонны, но во всю высоту, без
    // разреза. Это ровно та раскладка, которая получилась бы, если бы обшивку
    // клали, не глядя на проёмы.
    int hits_uncut = 0;
    int cut_columns = 0;
    for (int c = 0; c < lay.board_columns; ++c) {
        const float u0 = lay.end_bare + static_cast<float>(c) * STEP;
        const float u1 = u0 + STEP;
        bool any = false;
        for (const OpeningPlacement& o : lay.openings) {
            if (rects_overlap(u0, u1, 0.0f, WALL_H, o.u0, o.u1, o.v0, o.v1)) {
                ++hits_uncut;
                any = true;
            }
        }
        if (any) {
            ++cut_columns;
        }
    }
    CHECK(hits_uncut == 10);
    CHECK(cut_columns == 10);

    // РАЗРЕЗАННАЯ КОЛОННА НЕ ИСЧЕЗАЕТ, А ДАЁТ ДВА КУСКА — под проёмом и над ним.
    // Без этой строки «ноль пересечений» брался бы выкинутой обшивкой.
    CHECK(static_cast<int>(lay.boards.size()) == lay.board_columns + cut_columns);
    CHECK(lay.boards.size() == 36u);

    // И КАЖДЫЙ КУСОК ПРИМЫКАЕТ К ПРОЁМУ, а не отходит от него: под окном доска
    // кончается ровно на подоконнике, над окном начинается ровно на перемычке.
    int under = 0;
    int over = 0;
    for (const BoardRun& b : lay.boards) {
        if (quant(b.v1) == quant(1.10f)) {
            ++under;
        }
        if (quant(b.v0) == quant(2.10f)) {
            ++over;
        }
    }
    CHECK(under == 10);
    CHECK(over == 10);
}

TEST_CASE("растяжение стены ВДВОЕ не меняет окно — а испечённый набор его удваивает") {
    const WallStyle st = window_style();

    const WallLayout half = lay_out_wall(wall(1.50f, 1), st);
    const WallLayout full = lay_out_wall(wall(3.00f, 1), st);
    REQUIRE(half.openings.size() == 1u);
    REQUIRE(full.openings.size() == 1u);
    const float w_half = half.openings[0].u1 - half.openings[0].u0;
    const float w_full = full.openings[0].u1 - full.openings[0].u0;
    CHECK(quant(w_half) == quant(w_full));
    CHECK(quant(w_half) == 10000); // ровно 1.00 м на обеих длинах

    // ОТВЕРГНУТЫЙ ОБРАЗЕЦ, ЖИВОЙ И ИЗМЕРЕННЫЙ В ЭТОМ ЖЕ БИНАРНИКЕ (правило 47).
    const float legacy_half = legacy_window_width(1.50f);
    const float legacy_full = legacy_window_width(3.00f);
    CHECK(legacy_half == doctest::Approx(0.45f).epsilon(1e-4));
    CHECK(legacy_full == doctest::Approx(0.90f).epsilon(1e-4));
    CHECK(legacy_full / legacy_half == doctest::Approx(2.0f).epsilon(1e-4));
    CHECK(w_full / w_half == doctest::Approx(1.0f).epsilon(1e-6));

    // ТОТ ЖЕ ДЕФЕКТ ОСЬЮ В СТОРОНУ: расстояние между двумя окнами. Оно тоже не
    // имеет права ехать, и в старой раскладке едет ровно вдвое.
    const WallLayout two_short = lay_out_wall(wall(3.00f, 2), st);
    const WallLayout two_long = lay_out_wall(wall(6.00f, 2), st);
    const float gap_short =
        two_short.openings[1].center_u() - two_short.openings[0].center_u();
    const float gap_long = two_long.openings[1].center_u() - two_long.openings[0].center_u();
    CHECK(quant(gap_short) == quant(gap_long));
    CHECK(quant(gap_short) == 12300);
    CHECK(legacy_two_window_gap(6.00f) / legacy_two_window_gap(3.00f) ==
          doctest::Approx(2.0f).epsilon(1e-4));

    // РАСТЯЖЕНИЕ ПО ВЫСОТЕ — ТА ЖЕ ПРОВЕРКА ДРУГОЙ ОСЬЮ. Окно не растёт и не
    // всплывает: подоконник задан стилем в метрах, а не долей стены.
    const WallLayout tall = lay_out_wall(wall(3.00f, 1, WALL_H * 2.0f), st);
    CHECK(quant(tall.openings[0].v0) == quant(full.openings[0].v0));
    CHECK(quant(tall.openings[0].v1) == quant(full.openings[0].v1));
    CHECK(quant(tall.openings[0].v0) == 11000);

    // А ОБШИВКИ СТАЛО ВДВОЕ БОЛЬШЕ. Растяжение добавляет МАТЕРИАЛ, а не масштаб,
    // и это положительная половина того же утверждения.
    CHECK(two_long.board_columns == two_short.board_columns * 2);
}

TEST_CASE("раскос РАСТЯГИВАЕТСЯ и МЕНЯЕТ УГОЛ, а его полоса выведена из округления") {
    const WallStyle st = plain_style(); // цель 60°

    // ОДНА И ТА ЖЕ НАРЕЗКА (два пролёта), стена длиннее на 0.4 м: раскос стал
    // длиннее и площе. Это и есть требование пользователя, слово в слово.
    const WallLayout a = lay_out_wall(wall(3.00f), st);
    const WallLayout b = lay_out_wall(wall(3.40f), st);
    REQUIRE(a.braces.size() == 2u);
    REQUIRE(b.braces.size() == 2u);
    CHECK(a.braces[0].angle_rad * DEG == doctest::Approx(65.22f).epsilon(0.002));
    CHECK(b.braces[0].angle_rad * DEG == doctest::Approx(62.38f).epsilon(0.002));
    CHECK(a.braces[0].length == doctest::Approx(3.5795f).epsilon(0.002));
    CHECK(b.braces[0].length == doctest::Approx(3.6678f).epsilon(0.002));
    CHECK(b.braces[0].length > a.braces[0].length);
    CHECK(b.braces[0].angle_rad < a.braces[0].angle_rad);

    // НИЗКИЙ КОНЕЦ СНАРУЖИ: раскосы зеркальны относительно середины стены.
    CHECK(a.braces[0].u_low < a.braces[0].u_high);
    CHECK(a.braces[1].u_low > a.braces[1].u_high);
    CHECK(quant(a.braces[0].u_low) == quant(3.00f - a.braces[1].u_low));
    CHECK(quant(a.braces[0].angle_rad) == quant(a.braces[1].angle_rad));

    // ПОЛОСА УГЛА СМЕТАЕТСЯ ЦЕЛИКОМ, оба конца достаются.
    float lo = 1e9f;
    float hi = -1e9f;
    int outside_ours = 0;
    int outside_fixed = 0;
    int swept = 0;
    for (int i = 0; i < 400; ++i) {
        const float len = 0.95f + 0.0475f * static_cast<float>(i);
        const WallLayout lay = lay_out_wall(wall(len), st);
        REQUIRE(!lay.braces.empty());
        for (const BracePlacement& br : lay.braces) {
            const float deg = br.angle_rad * DEG;
            lo = std::min(lo, deg);
            hi = std::max(hi, deg);
            if (deg < BRACE_DEG_MIN || deg > BRACE_DEG_MAX) {
                ++outside_ours;
            }
            ++swept;
        }
        // КОНТРФАКТ: раскос НА ВЕСЬ УЧАСТОК, без перенарезки на пролёты. Он тоже
        // «растягивается и меняет угол» — и уезжает из полосы, превращаясь на
        // длинной стене в почти горизонтальную доску.
        const float fixed_deg = std::atan2(WALL_H, len) * DEG;
        if (fixed_deg < BRACE_DEG_MIN || fixed_deg > BRACE_DEG_MAX) {
            ++outside_fixed;
        }
    }
    CHECK(swept > 400);
    CHECK(outside_ours == 0);
    CHECK(outside_fixed > 350); // измерено: 360 из 400
    CHECK(lo > BRACE_DEG_MIN);
    CHECK(hi <= BRACE_DEG_MAX);
    CHECK(lo == doctest::Approx(49.2f).epsilon(0.01));  // нижний конец достаётся
    CHECK(hi == doctest::Approx(73.7f).epsilon(0.01));  // и верхний тоже
    // И у самой длинной стены рука-контрфакт даёт доску, а не раскос.
    CHECK(std::atan2(WALL_H, 19.9f) * DEG == doctest::Approx(9.27f).epsilon(0.01));

    // РАСКОС НЕ ХОДИТ СКВОЗЬ ОКНО. Свободные участки берутся между проёмами.
    const WallLayout holes = lay_out_wall(wall(6.00f, 2), window_style());
    REQUIRE(holes.openings.size() == 2u);
    CHECK(holes.braces.size() == 2u); // простенок 0.23 м раскоса НЕ получает
    int cross = 0;
    for (const BracePlacement& br : holes.braces) {
        const float b0 = std::min(br.u_low, br.u_high);
        const float b1 = std::max(br.u_low, br.u_high);
        for (const OpeningPlacement& o : holes.openings) {
            if (b1 > o.u0 + 1e-5f && b0 < o.u1 - 1e-5f) {
                ++cross;
            }
        }
    }
    CHECK(cross == 0);

    // КОНТРФАКТ: та же стена, раскосы по всей длине без оглядки на проёмы.
    int cross_blind = 0;
    const float nominal = WALL_H / std::tan(1.0471976f);
    const int bays = static_cast<int>(std::lround(6.00f / nominal));
    const float bay = 6.00f / static_cast<float>(bays);
    for (int i = 0; i < bays; ++i) {
        const float b0 = static_cast<float>(i) * bay;
        const float b1 = b0 + bay;
        for (const OpeningPlacement& o : holes.openings) {
            if (b1 > o.u0 + 1e-5f && b0 < o.u1 - 1e-5f) {
                ++cross_blind;
            }
        }
    }
    CHECK(bays == 3);
    CHECK(cross_blind == 4);
}

TEST_CASE("стиль приходит ТЕКСТОМ, и опечатка ОТВЕРГАЕТСЯ, а не проглатывается") {
    const std::string good = "# dfstyle 1\n"
                             "style frame_oak\n"
                             "  board_step=0.23 edge_margin=0.00\n"
                             "  brace_deg=60          # цель, не приказ\n"
                             "  opening=window opening_w=1.00 opening_h=1.00 opening_sill=1.10\n"
                             "\n"
                             "style barn_door board_step=0.30 opening=door\n";
    std::vector<WallStyle> lib;
    const auto ok = parse_wall_styles(good, lib);
    REQUIRE(ok.ok);
    REQUIRE(lib.size() == 2u);

    CHECK(lib[0].name == "frame_oak");
    CHECK(lib[0].board_step == doctest::Approx(0.23f).epsilon(1e-4));
    // ГРАДУСЫ ТОЛЬКО В ФАЙЛЕ (правило 14): в структуре лежат радианы.
    CHECK(lib[0].brace_rad == doctest::Approx(1.0471976f).epsilon(1e-5));
    CHECK(lib[0].opening == OpeningKind::Window);
    // Выведенные значения: простенок = шаг доски, шаг ряда = ширина + простенок.
    CHECK(style_pier(lib[0]) == doctest::Approx(0.23f).epsilon(1e-4));
    CHECK(style_pitch(lib[0]) == doctest::Approx(1.23f).epsilon(1e-4));

    // ДВЕРЬ ПРИНОСИТ СВОИ УМОЛЧАНИЯ: 2.05 м выше PLAYER_CAPSULE_HEIGHT 1.8, а
    // подоконника у неё нет по определению (HOUSES.md §6).
    CHECK(lib[1].opening == OpeningKind::Door);
    CHECK(lib[1].opening_h == doctest::Approx(2.05f).epsilon(1e-4));
    CHECK(dfn::world::style_sill(lib[1]) == 0.0f);
    // И выведенные значения следуют за ЕГО шагом, а не за чужим.
    CHECK(style_pier(lib[1]) == doctest::Approx(0.30f).epsilon(1e-4));

    // ВЫВОД ИДЁТ ЗА ШИРИНОЙ ПРОЁМА, а не за константой: широкое окно раздвигает
    // ряд само, и два таких окна не налезают друг на друга.
    std::vector<WallStyle> wide_lib;
    REQUIRE(parse_wall_styles("style wide opening=window opening_w=1.60\n", wide_lib).ok);
    CHECK(style_pitch(wide_lib[0]) == doctest::Approx(1.83f).epsilon(1e-4));
    const WallLayout wide = lay_out_wall(wall(5.00f, 2), wide_lib[0]);
    REQUIRE(wide.openings.size() == 2u);
    CHECK(wide.openings[0].u1 <= wide.openings[1].u0 + 1e-5f);

    // ЧИТАТЕЛЬ ОТВЕРГАЕТ, А НЕ ЧИНИТ. Каждая строка ниже — своя беда, и у
    // каждой назван НОМЕР СТРОКИ, иначе искать её в файле на сто стилей нечем.
    struct Bad {
        const char* text;
        int line;
    };
    const Bad bad[] = {
        {"style a\n  bord_step=0.23\n", 2},          // опечатка в ключе
        {"style a\n  board_step=0.23 closed\n", 2},  // голое слово
        {"style a\n  board_step=шире\n", 2},         // не число
        {"style a\nstyle a\n", 2},                   // повтор имени
        {"  board_step=0.23\n", 1},                  // свойство до первого style
        {"style a opening=окно\n", 1},               // неизвестный вид проёма
        {"style\n", 1},                              // стиль без имени
        {"style a board_step=0\n", 0},               // нулевой шаг обшивки
    };
    for (const Bad& t : bad) {
        std::vector<WallStyle> tmp;
        const auto r = parse_wall_styles(t.text, tmp);
        CHECK_FALSE(r.ok);
        CHECK(r.line == t.line);
        CHECK_FALSE(r.why.empty());
    }
    // КОНТРОЛЬ: та же строка с правильным ключом читается. Без него «отвергает
    // всё» выглядело бы точно так же, как «отвергает нужное».
    std::vector<WallStyle> fixed;
    CHECK(parse_wall_styles("style a\n  board_step=0.23\n", fixed).ok);
    CHECK(fixed.size() == 1u);
    // И примечание после значения не считается частью значения.
    std::vector<WallStyle> commented;
    REQUIRE(parse_wall_styles("style a board_step=0.25 # это шаг\n", commented).ok);
    CHECK(commented[0].board_step == doctest::Approx(0.25f).epsilon(1e-4));
}

TEST_CASE("сквозной просвет у торца ГОВОРИТСЯ числом, а не остаётся дырой") {
    // HOUSES.md §2 запрещает сквозной просвет по жалобе пользователя («у него
    // стены несплошные»). Здесь это правило работает не умолчанием, а замером на
    // каждой стене: полоса без обшивки обязана прятаться в теле угловой стойки.
    WallStyle tight = plain_style();      // шаг 0.23 — проходит потолок 0.24
    WallStyle loose = plain_style();      // шаг 0.30 — «обычная доска», течёт
    loose.board_step = 0.30f;
    WallStyle inset = plain_style();      // шаг тот же, но зазор 0.10
    inset.edge_margin = 0.10f;

    int leaks_tight = 0;
    int leaks_loose = 0;
    int leaks_inset = 0;
    float worst_tight = 0.0f;
    float worst_loose = 0.0f;
    for (int i = 0; i < 1000; ++i) {
        const float len = 1.0f + 0.019f * static_cast<float>(i); // 1.00 .. 19.98
        const WallLayout a = lay_out_wall(wall(len), tight);
        const WallLayout b = lay_out_wall(wall(len), loose);
        const WallLayout c = lay_out_wall(wall(len), inset);
        leaks_tight += a.has(LayoutIssue::CladdingLeaksAtEnd) ? 1 : 0;
        leaks_loose += b.has(LayoutIssue::CladdingLeaksAtEnd) ? 1 : 0;
        leaks_inset += c.has(LayoutIssue::CladdingLeaksAtEnd) ? 1 : 0;
        worst_tight = std::max(worst_tight, a.end_bare);
        worst_loose = std::max(worst_loose, b.end_bare);
    }
    CHECK(leaks_tight == 0);
    CHECK(worst_tight < POST_R);
    CHECK(worst_tight == doctest::Approx(0.115f).epsilon(0.02)); // ровно шаг/2
    // КОНТРФАКТЫ, ОБА С ЧИСЛОМ: шаг выше выведенного потолка течёт на пятой
    // длине, а ненулевой зазор — почти на каждой.
    CHECK(leaks_loose > 150);
    CHECK(worst_loose == doctest::Approx(0.15f).epsilon(0.02));
    CHECK(leaks_inset > 700);
    // И у находки есть ЧИСЛО, а не только флаг.
    const WallLayout bad = lay_out_wall(wall(6.29f), loose);
    const LayoutFinding* f = bad.find(LayoutIssue::CladdingLeaksAtEnd);
    REQUIRE(f != nullptr);
    CHECK(f->value > POST_R);
    CHECK(f->what.find("просвет") != std::string::npos);
}

TEST_CASE("вырожденная стена и стена короче доски ГОВОРЯТ, а не выдумывают") {
    const WallStyle st = window_style();
    const WallLayout none = lay_out_wall(wall(0.0f, 2), st);
    CHECK(none.has(LayoutIssue::WallDegenerate));
    CHECK(none.boards.empty());
    CHECK(none.braces.empty());
    CHECK(none.openings.empty());
    CHECK(none.openings_requested == 2);
    CHECK(none.openings_placed == 0);

    const WallLayout sliver = lay_out_wall(wall(0.15f, 1), st);
    CHECK(sliver.board_columns == 0);
    CHECK(sliver.has(LayoutIssue::NoBoardsFit));
    CHECK(sliver.openings_placed == 0);
    CHECK(sliver.braces.empty()); // 0.15 м короче полуноминала 0.94 м

    // КОНТРОЛЬ: на 0.25 м доска уже одна, и находки нет.
    const WallLayout one = lay_out_wall(wall(0.25f), st);
    CHECK(one.board_columns == 1);
    CHECK(!one.has(LayoutIssue::NoBoardsFit));
}

TEST_CASE("две раскладки одного заказа совпадают до последнего числа") {
    // Порядок обхода детерминирован, иначе «до и после» ничего не значит:
    // «до» каждый раз было бы разным.
    const WallStyle st = window_style();
    const WallLayout a = lay_out_wall(wall(7.77f, 4), st);
    const WallLayout b = lay_out_wall(wall(7.77f, 4), st);
    REQUIRE(a.boards.size() == b.boards.size());
    REQUIRE(a.braces.size() == b.braces.size());
    REQUIRE(a.openings.size() == b.openings.size());
    int diff = 0;
    for (std::size_t i = 0; i < a.boards.size(); ++i) {
        diff += (quant(a.boards[i].u0) != quant(b.boards[i].u0)) ? 1 : 0;
        diff += (quant(a.boards[i].v1) != quant(b.boards[i].v1)) ? 1 : 0;
    }
    for (std::size_t i = 0; i < a.braces.size(); ++i) {
        diff += (quant(a.braces[i].angle_rad) != quant(b.braces[i].angle_rad)) ? 1 : 0;
    }
    CHECK(diff == 0);
    // И доски идут СЛЕВА НАПРАВО, а куски колонны — СНИЗУ ВВЕРХ.
    int disorder = 0;
    for (std::size_t i = 0; i + 1 < a.boards.size(); ++i) {
        const BoardRun& p = a.boards[i];
        const BoardRun& q = a.boards[i + 1];
        if (q.column < p.column || (q.column == p.column && q.v0 < p.v0)) {
            ++disorder;
        }
    }
    CHECK(disorder == 0);
}
