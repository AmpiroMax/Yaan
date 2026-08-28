/*
Created: 28:08:2026 - 12:40:00
Last updated: 28:08:2026 - 12:40:00
Module: tests
File: tests/app/SeatAimTests.cpp

Responsibility:
- ПРИЦЕЛ МЕБЕЛИ И МЕТА, ВЫВЕДЕННАЯ ИЗ ГЕОМЕТРИИ. Два утверждения, которых кадр
  предъявить не может: (1) правило «самая широкая горизонтальная площадка»,
  прогнанное по НАСТОЯЩИМ чертежам полки, находит у обеих лавок сиденье 0.45,
  у кровати настил 0.50 и НЕ находит ни одного ложного сиденья у остальных 43
  предметов семейства; (2) подсказка «Сесть» горит тогда и только тогда, когда человек
  рядом И СМОТРИТ — с настоящим отвергнутым образцом (взгляд мимо, спина,
  три метра), а не с синтетическим.

Dependencies:
- Uses: doctest, SeatAim.cpp + FurnitureSeats.cpp (без App и без окна),
  dfn_world (чтение .dfh и сборка меша — та же, что рисует игра).
- Used by: ctest (app_seat_aim). Рабочая папка рукавов — корень репозитория
  (tests/CMakeLists.txt), поэтому чертежи читаются по относительному пути.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ЧИСЛА ЗДЕСЬ — ЗАМЕРЫ С ЧЕРТЕЖЕЙ, а не копии таблицы: если furn-bench поднимут
  на сантиметр, красным станет ЭТОТ файл, и это правильно.
*/
/*
UPD:
- 28:08:2026 - 12:40:00: Создан. Обязательство эпохи «сидеть и лежать».
*/

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <glm/geometric.hpp>

#include "engine/app/sources/FurnitureSeats.h"
#include "engine/app/sources/SeatAim.h"
#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseMesh.h"

using namespace dfn;
using namespace dfn::app;

namespace {

struct Tris {
    std::vector<glm::vec3> pos;
    std::vector<std::uint32_t> idx;
    bool ok = false;
};

/// ЧИТАЕТ ЧЕРТЁЖ ПОЛКИ И СОБИРАЕТ ЕГО ТЕМ ЖЕ ПОСТРОИТЕЛЕМ, ЧТО И ИГРА. Второй
/// разборщик .dfh в проекте отсутствует намеренно (шапка assets/houses/
/// INDEX.txt), и заводить его в тесте значило бы мерить не ту геометрию.
[[nodiscard]] Tris load(const std::string& name) {
    Tris t;
    std::ifstream in("assets/houses/" + name + ".dfh");
    if (!in) {
        return t;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    world::HouseGraph g;
    if (!world::read_house(ss.str(), g).ok) {
        return t;
    }
    const world::HouseMesh built = world::build_house_mesh(g);
    t.pos.reserve(built.vertices.size());
    for (const world::HouseVertex& v : built.vertices) {
        t.pos.push_back(v.pos);
    }
    t.idx = built.indices;
    t.ok = !t.idx.empty();
    return t;
}

/// Габарит предмета — им очерчивается коробка прицела.
void bounds(const Tris& t, glm::vec3& lo, glm::vec3& hi) {
    lo = glm::vec3{1.0e9f};
    hi = glm::vec3{-1.0e9f};
    for (const glm::vec3& p : t.pos) {
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
}

} // namespace

TEST_CASE("мета лавки: сиденье 0.45, пятно 1.60x0.35, СИДЕНЬЕ") {
    const Tris t = load("furn-bench");
    REQUIRE(t.ok);
    const FurnSurface s = furniture_surface(t.pos, t.idx);
    REQUIRE(s.found);
    // Настил e1 стоит на 0.4225 при толщине 0.055 — верх ровно 0.45. Это же
    // число стоит в колонке my1 манифеста полки, и владелец назвал его в
    // заказе («сиденье 0.45»).
    INFO("верх ", s.top_y, ", пятно ", s.long_side(), "x", s.short_side(),
         ", площадь ", s.area_m2);
    CHECK(std::fabs(s.top_y - 0.45f) < 0.005f);
    CHECK(std::fabs(s.long_side() - 1.60f) < 0.02f);
    CHECK(std::fabs(s.short_side() - 0.35f) < 0.02f);
    CHECK(classify_surface(s) == SpotKind::Seat);
    // ДЛИННАЯ ОСЬ — ВДОЛЬ ЛАВКИ, значит взгляд сидящего идёт ПОПЕРЁК неё.
    CHECK(s.long_axis().x == doctest::Approx(1.0f));
}

TEST_CASE("мета кровати: настил 0.50, пятно 0.97x1.90, ЛЕЖАК") {
    const Tris t = load("furn-bed");
    REQUIRE(t.ok);
    const FurnSurface s = furniture_surface(t.pos, t.idx);
    REQUIRE(s.found);
    // Матрас e9 на 0.47 при толщине 0.06 — верх 0.50, и ровно это число стоит
    // в колонке `floor` манифеста полки.
    // ...с точностью до ИЗНОСА: у матраса в чертеже стоит wear=0.3, и
    // построитель честно колеблет его вершины — средняя по площади высота
    // выходит 0.509, а не ровные 0.500. Меряется НАРИСОВАННАЯ поверхность, та
    // самая, на которую ложатся, поэтому допуск назван по износу, а не по
    // строке манифеста (в ней 0.5 — величина ГРАФА, до износа).
    INFO("верх ", s.top_y, ", пятно ", s.long_side(), "x", s.short_side());
    CHECK(std::fabs(s.top_y - 0.50f) < 0.015f);
    CHECK(std::fabs(s.long_side() - 1.90f) < 0.03f);
    CHECK(std::fabs(s.short_side() - 0.97f) < 0.03f);
    CHECK(classify_surface(s) == SpotKind::Lie);
    // ГОЛОВА УХОДИТ ВДОЛЬ ДЛИННОЙ ОСИ — у кровати это местное Z.
    CHECK(s.long_axis().y == doctest::Approx(1.0f));

    // КОНТРОЛЬ ПРАВИЛА «ПО ПЛОЩАДИ, А НЕ ПО ВЫСОТЕ»: у кровати ВЫШЕ матраса
    // стоят навершия столбиков изголовья (e12/e13 на 1.115, 0.18x0.18 каждое).
    // Правило «самая высокая площадка» уложило бы человека на столбик — и
    // именно поэтому найденная высота обязана быть НИЖЕ верха габарита.
    glm::vec3 lo;
    glm::vec3 hi;
    bounds(t, lo, hi);
    INFO("верх габарита ", hi.y);
    CHECK(hi.y > 1.10f);
    CHECK(s.top_y < hi.y - 0.5f);
}

TEST_CASE("вся полка: ни одного ложного сиденья и ни одного ложного лежака") {
    // ОТВЕРГНУТЫЕ ОБРАЗЦЫ НАСТОЯЩИЕ (правило 30): это не выдуманные коробки, а
    // все 46 чертежей семейства furn-, которые сегодня стоят в комнатах и во
    // дворах обоих городов. Стол, полка, очаг, бочка, наковальня, кадка, колонна — на любую
    // из них сесть было бы дефектом, который видно только в игре.
    std::size_t seen = 0;
    std::size_t seats = 0;
    std::size_t lies = 0;
    std::vector<std::string> seat_names;
    std::vector<std::string> lie_names;
    for (const auto& e : std::filesystem::directory_iterator("assets/houses")) {
        const std::string stem = e.path().stem().string();
        if (e.path().extension() != ".dfh" || stem.rfind("furn-", 0) != 0) {
            continue;
        }
        const Tris t = load(stem);
        if (!t.ok) {
            continue;
        }
        ++seen;
        const SpotKind k = classify_surface(furniture_surface(t.pos, t.idx));
        if (k == SpotKind::Seat) {
            ++seats;
            seat_names.push_back(stem);
        } else if (k == SpotKind::Lie) {
            ++lies;
            lie_names.push_back(stem);
        }
    }
    std::string s_list;
    for (const std::string& n : seat_names) {
        s_list += n + " ";
    }
    std::string l_list;
    for (const std::string& n : lie_names) {
        l_list += n + " ";
    }
    INFO("чертежей ", seen, "; сидений: ", s_list, "; лежаков: ", l_list);
    // ЗНАМЕНАТЕЛЬ НАЗВАН (правило 30): проверка обязана обойти всю полку, а не
    // пустой список. Полка семейства furn- на 28.08 — 46 чертежей.
    CHECK(seen >= 40);
    // На сегодняшней полке сидений ровно два (прямая лавка и дуговая) и лежак
    // ровно один. Число вырастет вместе с номенклатурой — и вырастет ЗДЕСЬ,
    // то есть заметно, а не в игре.
    std::sort(seat_names.begin(), seat_names.end());
    std::sort(lie_names.begin(), lie_names.end());
    CHECK(seats == 2);
    CHECK(lies == 1);
    CHECK(seat_names == std::vector<std::string>{"furn-bench", "furn-bench-arc"});
    CHECK(lie_names == std::vector<std::string>{"furn-bed"});

    // ОТВЕРГНУТЫЙ ОБРАЗЕЦ, СТОЯВШИЙ БЛИЖЕ ВСЕХ (правило 30: контроль берётся
    // настоящий, а не синтетический). Сундук furn-chest — крышка 0.56 при
    // пятне 0.94x0.55: он проходит ОБА размера сиденья и отсекается ровно
    // высотой колена. Порог обязан стоять между ним и лавкой, и вот они оба.
    const Tris chest = load("furn-chest");
    REQUIRE(chest.ok);
    const FurnSurface cs = furniture_surface(chest.pos, chest.idx);
    INFO("сундук: верх ", cs.top_y, ", колено ", SEAT_MAX_M, ", лавка 0.45");
    CHECK(cs.top_y > SEAT_MAX_M);
    CHECK(cs.long_side() > SEAT_MIN_LONG_M);   // по длине он ПРОХОДИТ
    CHECK(cs.short_side() < SEAT_MAX_SHORT_M); // и по ширине тоже
    CHECK(classify_surface(cs) == SpotKind::None);
}

TEST_CASE("прицел мебели: рядом и смотрю — да; мимо, спиной, издали — нет") {
    // Лавка 1.6x0.45x0.35 стоит в начале координат, длинной осью по X.
    SeatAim aim;
    aim.centre = {0.0f, 0.225f, 0.0f};
    aim.half = {0.80f, 0.225f, 0.175f};
    aim.yaw = 0.0f;
    // РОСТ ЧЕЛОВЕКА — часть прицела: до лавки 0.45 глаз на 1.70 всегда дальше
    // метра, и без стойки «дотянуться» до неё было бы нельзя ниоткуда.
    aim.stand_m = 1.70f;

    // РУКА 1 — СТОЮ ПЕРЕД ЛАВКОЙ И СМОТРЮ НА НЕЁ. Глаз на 1.7, лавка низкая,
    // значит взгляд идёт ВНИЗ — и это тот самый случай, который прицел обязан
    // засчитывать: на лавку смотрят сверху.
    const glm::vec3 eye{0.0f, 1.70f, 0.80f};
    const SeatAimHit yes = seat_aim(aim, eye, glm::normalize(aim.centre - eye));
    INFO("до лавки ", yes.distance_m, " м");
    CHECK(yes.in_reach);
    CHECK(yes.looking);
    CHECK(yes.ok);
    CHECK(yes.distance_m < SEAT_REACH_M);

    // РУКА 2 — ТА ЖЕ ПОЗА, ВЗГЛЯД МИМО (в стену в двух метрах вбок). Радиус
    // тот же, значит различает ровно ВЗГЛЯД — это и есть отвергнутый образец
    // для «горит только когда смотрим».
    const SeatAimHit aside =
        seat_aim(aim, eye, glm::normalize(glm::vec3{3.0f, 1.6f, 0.9f} - eye));
    CHECK(aside.in_reach);
    CHECK_FALSE(aside.looking);
    CHECK_FALSE(aside.ok);

    // РУКА 3 — СТОЮ РЯДОМ СПИНОЙ.
    const SeatAimHit back = seat_aim(aim, eye, glm::normalize(eye - aim.centre));
    CHECK(back.in_reach);
    CHECK_FALSE(back.looking);
    CHECK_FALSE(back.ok);

    // РУКА 4 — СМОТРЮ ПРЯМО НА ЛАВКУ, НО ИЗ ТРЁХ МЕТРОВ. Взгляд тот же,
    // значит различает ровно РАДИУС.
    const glm::vec3 far_eye{0.0f, 1.70f, 3.0f};
    const SeatAimHit far = seat_aim(aim, far_eye, glm::normalize(aim.centre - far_eye));
    CHECK(far.looking);
    CHECK_FALSE(far.in_reach);
    CHECK_FALSE(far.ok);
}

TEST_CASE("прицел мебели: радиус мерится до ГАБАРИТА, а не до середины") {
    // Кровать 1.15x1.14x2.05: от середины до торца метр с лишним. Радиус,
    // отмеренный до середины, значил бы у изголовья и у бока РАЗНОЕ — то есть
    // был бы не рукой, а формой предмета (та же ошибка, что двери исправили у
    // себя 27.08).
    SeatAim aim;
    aim.centre = {0.0f, 0.57f, 0.0f};
    aim.half = {0.575f, 0.57f, 1.025f};
    aim.stand_m = 1.70f;
    const glm::vec3 at_foot{0.0f, 1.70f, 1.60f}; // 0.575 м от торца
    const SeatAimHit hit = seat_aim(aim, at_foot, glm::normalize(aim.centre - at_foot));
    INFO("от торца ", hit.distance_m, " м (до середины было бы ",
         glm::length(at_foot - aim.centre), ")");
    CHECK(hit.in_reach);
    CHECK(hit.ok);
    // КОНТРОЛЬ: до середины оттуда БОЛЬШЕ радиуса руки — то есть прежняя мера
    // отказала бы там, где рука дотягивается.
    CHECK(glm::length(at_foot - aim.centre) > SEAT_REACH_M);
}

TEST_CASE("поворот предмета: прицел едет вместе с ним") {
    SeatAim aim;
    aim.centre = {0.0f, 0.225f, 0.0f};
    aim.half = {0.80f, 0.225f, 0.175f};
    aim.stand_m = 1.70f;
    // Лавка развёрнута на 90°: её длинная сторона теперь вдоль мировой Z.
    aim.yaw = 1.57079633f;
    // Стою у ДЛИННОЙ стороны в мире (по X) — обязан дотянуться.
    const glm::vec3 eye{0.70f, 1.70f, 0.0f};
    CHECK(seat_aim(aim, eye, glm::normalize(aim.centre - eye)).ok);
    // А с торца (по Z в 1.2 м) — уже нет: торец у повёрнутой лавки далеко.
    const glm::vec3 end{0.0f, 1.70f, 2.20f};
    CHECK_FALSE(seat_aim(aim, end, glm::normalize(aim.centre - end)).in_reach);
}

TEST_CASE("взгляд сидящего: у стола — к столу, у стены — в комнату") {
    // РАСКЛАДКА ТРАКТИРА, замер tools/recipes_props.py: стол 1.8x0.9 стоит на
    // (2.60, 1.00), лавки — на (2.70, 0.50) и (2.70, 2.10). Одна лавка ПЕРЕД
    // столом, другая ЗА ним, и общего «правильного» направления у них нет:
    // без стола любое единое правило развернуло бы одну из двух спиной.
    const glm::vec3 cross{0.0f, 0.0f, 1.0f}; // поперечная ось лавки: по Z
    const glm::vec3 table{3.50f, 0.0f, 1.45f};
    const std::vector<glm::vec3> tables{table};
    const glm::vec3 room{4.0f, 0.0f, 4.0f};

    const glm::vec3 near_side = seat_facing({3.50f, 0.0f, 0.675f}, cross, tables, room);
    CHECK(near_side.z > 0.9f); // смотрит В +Z, то есть на стол
    const glm::vec3 far_side = seat_facing({3.50f, 0.0f, 2.275f}, cross, tables, room);
    CHECK(far_side.z < -0.9f); // смотрит В -Z, то есть тоже на стол

    // КОНТРОЛЬ: убрать стол — и решает уже комната, а не он. Обе лавки
    // развернутся в одну сторону, и это ДРУГОЙ ответ, то есть правило
    // действительно читает стол, а не совпадает с ним случайно.
    const glm::vec3 no_table_near = seat_facing({3.50f, 0.0f, 0.675f}, cross, {}, room);
    const glm::vec3 no_table_far = seat_facing({3.50f, 0.0f, 2.275f}, cross, {}, room);
    CHECK(no_table_near.z > 0.9f);
    CHECK(no_table_far.z > 0.9f);

    // И ДАЛЬНИЙ СТОЛ НЕ СЧИТАЕТСЯ СВОИМ: соседний стол зала стоит в 4.9 м, а
    // порог 1.60. Иначе лавка у стены разворачивалась бы к столу через комнату.
    const std::vector<glm::vec3> far_table{glm::vec3{3.50f, 0.0f, 5.60f}};
    CHECK(seat_facing({3.50f, 0.0f, 0.675f}, cross, far_table, room).z > 0.9f);
}

TEST_CASE("точка позы в мире: середина площадки, ось предмета, габарит прицела") {
    const Tris t = load("furn-bed");
    REQUIRE(t.ok);
    const FurnSurface s = furniture_surface(t.pos, t.idx);
    glm::vec3 lo;
    glm::vec3 hi;
    bounds(t, lo, hi);

    // Кровать стоит на (10, 2, -5) без поворота.
    const FurnitureSpot a =
        furniture_spot(s, SpotKind::Lie, {10.0f, 2.0f, -5.0f}, 0.0f, lo, hi);
    INFO("точка ", a.floor_at.x, " ", a.floor_at.y, " ", a.floor_at.z);
    // Пол позы — ПОСАДКА ЧЕРТЕЖА по высоте: поза считает свои 0.50 от него.
    CHECK(a.floor_at.y == doctest::Approx(2.0f));
    CHECK(a.surface_m == doctest::Approx(s.top_y));
    // Середина матраса по XZ: x около 0.575, z около 1.04 от угла чертежа.
    CHECK(a.floor_at.x == doctest::Approx(10.0f + 0.575f).epsilon(0.02));
    CHECK(a.floor_at.z == doctest::Approx(-5.0f + 1.04f).epsilon(0.02));
    // Голова уходит вдоль местного +Z, то есть при нулевом рыске — в мировое +Z.
    CHECK(a.facing.z > 0.99f);

    // ПОВОРОТ НА 90°: местный +Z уходит в мировой +X (конвенция сцен), и
    // голова обязана уехать туда же. Это утверждение о СВЯЗИ двух конвенций —
    // сцены и рига, — и ошибка в нём кладёт человека поперёк кровати.
    const FurnitureSpot b =
        furniture_spot(s, SpotKind::Lie, {0.0f, 0.0f, 0.0f}, 1.57079633f, lo, hi);
    INFO("после поворота голова ", b.facing.x, " ", b.facing.z);
    CHECK(b.facing.x > 0.99f);
}
