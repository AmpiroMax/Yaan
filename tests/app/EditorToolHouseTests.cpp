/*
Created: 18:08:2026 - 18:02:11
Last updated: 18:08:2026 - 19:14:22
Module: tests/app
File: tests/app/EditorToolHouseTests.cpp

Responsibility:
- ЧТО ТРИ ИНСТРУМЕНТА ПОСТРОЙКИ РЕШАЮТ, ЧИСЛАМИ. Каждый вопрос здесь — из тех,
  на которые кадр не отвечает:
  1. отвес отличает вершину В ВОЗДУХЕ от вершины НА ЗЕМЛЕ — и это число
     штрихов, а не впечатление от картинки;
  2. щелчок по оси прямой сажает вершину НА ОСЬ, а щелчок мимо — не сажает;
  3. якорь тащится, и привязанная геометрия едет за ним;
  4. удаление занятой вершины ОТКАЗЫВАЕТ И НАЗЫВАЕТ ДЕРЖАТЕЛЕЙ;
  5. отмена возвращает граф ровно в то состояние, что было;
  6. одна механика прямой даёт ДВА исхода, и зажим длины выбирает якорь;
  7. НОРМАЛЬ ЧЕРНОВИКА ПЕРЕВОРАЧИВАЕТСЯ ПОРЯДКОМ ОБХОДА — до подтверждения;
  8. выбор подсвечивает то, что просил пользователь: вершина → её элементы,
     элемент → его вершины.

Dependencies:
- Uses: doctest, EditorToolHouse.h, EditorHistory.h, engine/world (HouseGraph,
  HouseFile, HouseMesh). НИ ОДНОЙ строки ImGui и ни одного окна: панели живут
  в EditorToolHouseUi.cpp, которого эта цель не линкует (правило 3).
- Used by: ctest (app_editor_house).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- КАЖДОЕ УТВЕРЖДЕНИЕ СО СВОИМ КОНТРОЛЕМ (правило 30). Половина ответов здесь —
  «ничего не произошло» (щелчок мимо оси не сажает вершину, отказ не меняет
  граф), а такой ответ проходит и на инструменте, который не делает НИЧЕГО.
  Поэтому рядом с каждым стоит плечо, где то же действие СРАБАТЫВАЕТ.
*/
/*
UPD:
- 18:08:2026 - 18:02:11: Создан — рукав трёх инструментов постройки.
- 18:08:2026 - 18:40:24: три случая после починок зоны core: (1) снимок переживает удаления —
  бывший WARN стал CHECK, запись больше не теряет выживший якорь; (2) имя
  вершины после отмены достаётся другой точке (v4 в 99 м, затем v4 в -77 м) и
  рука это переживает — обход брошен, а не достроен на чужих якорях; (3) числа
  инструмента доходят до геометрии: стена в 2.5 м даёт 12 треугольников
  высотой ровно 2.5 м, и лицо открытой цепочки из ТРЁХ якорей теперь сходится
  с world::surface_normal (до миграции построителя расходилось: я считал по
  closed, он по height). 19 случаев, 176 утверждений.
- 18:08:2026 - 18:56:09: случай про имена рассказывает, ПОЧЕМУ два замера разошлись: между ними
  лёг коммит 146f484. Держит нынешнее поведение (v5 остаётся v5) и отсылает к
  ловушке со счётчиком, которую сохранение имён не закрывает.
- 18:08:2026 - 18:58:40: Два случая про ось: без неё прямая уезжает по земле на 40 м (снимок дефекта с кадра пользователя), с ней конец стоит над якорем на 3.1 м и растёт с наклоном прицела; земля перестаёт быть нужной только пока ось включена И рука на якоре.
- 18:08:2026 - 19:14:22: Случай про Enter: один якорь черновиком не считается и подтверждение ничего не создаёт, два — создают элемент.
*/

#include "engine/editor/sources/EditorToolHouse.h"

#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseMesh.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/geometric.hpp>
#include <string>
#include <vector>

using dfn::app::append_ball;
using dfn::app::append_plumb;
using dfn::app::EditorHistory;
using dfn::app::house_angles_from_dir;
using dfn::app::house_clamp_length;
using dfn::app::house_dir_from_angles;
using dfn::app::HouseClamp;
using dfn::app::HouseClampHit;
using dfn::app::HouseLineTool;
using dfn::app::HouseSession;
using dfn::app::HouseSurfaceTool;
using dfn::app::HouseVertexTool;
using dfn::app::HouseWire;
using dfn::app::ToolAim;
using dfn::app::ToolPreview;
using dfn::app::ToolWorld;
using dfn::world::Anchoring;
using dfn::world::ElementId;
using dfn::world::ElementKind;
using dfn::world::NO_ELEMENT;
using dfn::world::NO_VERTEX;
using dfn::world::VertexId;

namespace {

/// Стенд: сессия, история, крючки мира с ПЛОСКОЙ землёй на нуле. Земля плоская
/// нарочно — вопросы этого рукава про руку, а не про рельеф, и наклон только
/// сдвинул бы все числа на одну и ту же величину.
struct Bench {
    HouseSession session;
    EditorHistory history{100};
    ToolWorld world;
    float ground_y = 0.0f;

    Bench() {
        session.set_history(&history);
        world.ground_height = [this](glm::vec2) { return ground_y; };
    }

    /// Прицел, попавший в точку. distance_m заполняется честно: потолок
    /// дальности живёт в ящике, но подпись инструмента его читает.
    static ToolAim at(glm::vec3 p) {
        ToolAim aim;
        aim.origin = {0.0f, 2.0f, -10.0f};
        aim.point = p;
        aim.distance_m = glm::length(p - aim.origin);
        aim.hit = true;
        return aim;
    }

    /// Один щелчок целиком: нажал и отпустил там же.
    void click(dfn::app::IEditorTool& tool, glm::vec3 p) {
        tool.on_press(at(p), world);
        tool.on_release(world);
    }

    /// Протаскивание: нажал в одном месте, отпустил в другом.
    void drag(dfn::app::IEditorTool& tool, glm::vec3 from, glm::vec3 to) {
        tool.on_press(at(from), world);
        tool.on_drag(at(to), 0.016f, world);
        tool.on_release(world);
    }

    /// Сколько отрезков в картинке инструмента (обе стопки).
    static std::size_t wire_size(const ToolPreview& p) {
        return (p.handles != nullptr ? p.handles->size() : 0)
             + (p.accent != nullptr ? p.accent->size() : 0);
    }
};

/// Число точек в шарике — постоянная картинки, а не магия: три кольца по восемь
/// отрезков, отрезок это две точки.
constexpr std::size_t BALL_POINTS = 3 * 8 * 2;

} // namespace

// ---------------------------------------------------------------------------
// 1. ОТВЕС
// ---------------------------------------------------------------------------

TEST_CASE("отвес отличает вершину в воздухе от вершины на земле") {
    // ЭТО ГЛАВНОЕ ТРЕБОВАНИЕ ПОЛЬЗОВАТЕЛЯ ПРО ОТРИСОВКУ и единственное, которое
    // невозможно проверить кадром: на экране высота и дальность выглядят
    // одинаково, поэтому «отвес нарисован» и «отвес нарисован ТУДА» — разные
    // утверждения, и первое из них глазом не отличить от второго.
    std::vector<glm::vec3> seg;
    const int on_ground = append_plumb(seg, {0.0f, 0.0f, 0.0f}, 0.0f);
    CHECK(on_ground == 0);
    CHECK(seg.empty());

    const int in_air = append_plumb(seg, {0.0f, 3.0f, 0.0f}, 0.0f);
    CHECK(in_air > 0);
    CHECK(seg.size() == static_cast<std::size_t>(in_air) * 2);
    MESSAGE("отвес с 3 м: штрихов " << in_air);

    // ОТВЕС УПИРАЕТСЯ В ЗЕМЛЮ, А НЕ ПРОХОДИТ СКВОЗЬ: последняя точка не ниже
    // грунта. Без этой проверки пунктир мог бы уезжать под рельеф, и «над какой
    // точкой висит» отвечалось бы неправильно ровно на склоне.
    for (const glm::vec3& p : seg) {
        CHECK(p.y >= 0.0f);
    }

    // КОНТРОЛЬ ВЫСОТЫ: чем выше вершина, тем длиннее отвес.
    std::vector<glm::vec3> far_seg;
    const int high = append_plumb(far_seg, {0.0f, 12.0f, 0.0f}, 0.0f);
    CHECK(high > in_air);

    // И ПОТОЛОК: вершина на километре не имеет права съесть буфер линий.
    std::vector<glm::vec3> sky;
    const int capped = append_plumb(sky, {0.0f, 5000.0f, 0.0f}, 0.0f);
    CHECK(capped <= dfn::app::HOUSE_PLUMB_MAX_DASHES);
    MESSAGE("отвес с 5000 м: штрихов " << capped << " (потолок "
                                       << dfn::app::HOUSE_PLUMB_MAX_DASHES << ")");
}

TEST_CASE("вершина в воздухе — Free с отвесом, вершина по земле — OnGround без него") {
    Bench b;
    HouseVertexTool tool(b.session);
    tool.set_world(&b.world);

    // ПЛЕЧО «ПО ЗЕМЛЕ».
    b.click(tool, {1.0f, 0.0f, 0.0f});
    REQUIRE(b.session.graph().vertex_count() == 1);
    const VertexId ground_v = b.session.selected_vertex();
    REQUIRE(ground_v != NO_VERTEX);
    CHECK(b.session.graph().vertex(ground_v)->anchoring == Anchoring::OnGround);
    CHECK(b.session.vertex_world(ground_v).y == doctest::Approx(0.0f));
    const std::size_t after_ground = Bench::wire_size(tool.preview(Bench::at({50.0f, 0.0f, 50.0f})));

    // ПЛЕЧО «В ВОЗДУХЕ»: то же действие, одно изменённое число.
    tool.air_height_m() = 3.0f;
    b.click(tool, {5.0f, 0.0f, 0.0f});
    REQUIRE(b.session.graph().vertex_count() == 2);
    const VertexId air_v = b.session.selected_vertex();
    CHECK(b.session.graph().vertex(air_v)->anchoring == Anchoring::Free);
    CHECK(b.session.vertex_world(air_v).y == doctest::Approx(3.0f));
    const std::size_t after_air = Bench::wire_size(tool.preview(Bench::at({50.0f, 0.0f, 50.0f})));

    // ЧИСЛО: воздушная вершина принесла шарик И отвес, земляная — только шарик.
    const std::size_t added = after_air - after_ground;
    CHECK(added > BALL_POINTS);
    MESSAGE("вершина по земле дала " << after_ground << " точек, в воздухе — "
            << after_air << "; разница " << added << " при шарике в " << BALL_POINTS);
}

// ---------------------------------------------------------------------------
// 2. ВЕРШИНА НА ОСИ
// ---------------------------------------------------------------------------

TEST_CASE("щелчок по оси сажает вершину на ось, щелчок мимо — не сажает") {
    Bench b;
    HouseVertexTool vt(b.session);
    vt.set_world(&b.world);
    HouseLineTool lt(b.session);
    lt.set_world(&b.world);

    b.click(vt, {0.0f, 0.0f, 0.0f});
    b.click(vt, {6.0f, 0.0f, 0.0f});
    // Прямая между ними: тянем от первого якоря ко второму.
    b.drag(lt, {0.0f, 0.0f, 0.0f}, {6.0f, 0.0f, 0.0f});
    REQUIRE(b.session.graph().element_count() == 1);
    const ElementId line = lt.last_element();
    REQUIRE(line != NO_ELEMENT);

    // ПЛЕЧО «ПО ОСИ»: щелчок в середину бревна.
    b.click(vt, {3.0f, 0.0f, 0.0f});
    REQUIRE(b.session.graph().vertex_count() == 3);
    const VertexId rider = b.session.selected_vertex();
    const dfn::world::Vertex* rv = b.session.graph().vertex(rider);
    REQUIRE(rv != nullptr);
    CHECK(rv->anchoring == Anchoring::OnEdge);
    CHECK(rv->host == line);
    CHECK(rv->host_t == doctest::Approx(0.5f).epsilon(0.01));

    // КОНТРОЛЬ: щелчок В СТОРОНЕ от оси даёт ОБЫЧНУЮ вершину. Без него
    // утверждение выше проходило бы на инструменте, который сажает на ось
    // вообще всё.
    b.click(vt, {3.0f, 0.0f, 4.0f});
    const VertexId aside = b.session.selected_vertex();
    CHECK(b.session.graph().vertex(aside)->anchoring == Anchoring::OnGround);
    CHECK(b.session.graph().vertex(aside)->host == NO_ELEMENT);

    // И ГЛАВНОЕ СВОЙСТВО ТАКОЙ ВЕРШИНЫ: она ЕДЕТ ЗА ХОЗЯИНОМ. Двигаем конец
    // бревна — сидящая на оси вершина обязана переехать сама, потому что её
    // положение нигде не хранится.
    const glm::vec3 before = b.session.vertex_world(rider);
    (void)b.session.graph().move_vertex(b.session.graph().vertices()[1].id,
                                        {6.0f, 4.0f, 0.0f});
    const glm::vec3 after = b.session.vertex_world(rider);
    CHECK(after.y > before.y);
    MESSAGE("вершина на оси переехала с y=" << before.y << " на y=" << after.y);
}

// ---------------------------------------------------------------------------
// 3. ПРОТАСКИВАНИЕ
// ---------------------------------------------------------------------------

TEST_CASE("протаскивание двигает якорь, и привязанная геометрия едет за ним") {
    Bench b;
    HouseVertexTool vt(b.session);
    vt.set_world(&b.world);
    HouseLineTool lt(b.session);
    lt.set_world(&b.world);

    b.click(vt, {0.0f, 0.0f, 0.0f});
    b.click(vt, {4.0f, 0.0f, 0.0f});
    b.drag(lt, {0.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f});
    const ElementId line = lt.last_element();
    REQUIRE(line != NO_ELEMENT);

    glm::vec3 a{0.0f};
    glm::vec3 c{0.0f};
    REQUIRE(b.session.line_ends_world(line, a, c));
    const float before_len = glm::length(c - a);
    CHECK(before_len == doctest::Approx(4.0f));

    // ТАЩИМ ДАЛЬНИЙ КОНЕЦ. Ни одна строка кода не «обновляет прямую»: её
    // геометрия нигде не хранится, и в этом весь смысл модели.
    b.drag(vt, {4.0f, 0.0f, 0.0f}, {9.0f, 0.0f, 0.0f});
    REQUIRE(b.session.line_ends_world(line, a, c));
    const float after_len = glm::length(c - a);
    CHECK(after_len == doctest::Approx(9.0f));
    MESSAGE("длина прямой: " << before_len << " м -> " << after_len << " м");

    // КОНТРОЛЬ: щелчок по якорю БЕЗ протаскивания ничего не двигает и — что
    // важнее — не пишет шаг в историю. Пустой шаг делает cmd+Z «срабатывающим
    // вхолостую», и это неотличимо от сломанной отмены.
    const std::size_t depth = b.history.undo_depth();
    b.click(vt, {9.0f, 0.0f, 0.0f});
    CHECK(b.history.undo_depth() == depth);
    REQUIRE(b.session.line_ends_world(line, a, c));
    CHECK(glm::length(c - a) == doctest::Approx(after_len));
}

// ---------------------------------------------------------------------------
// 4. ОТКАЗ СО СПИСКОМ ДЕРЖАТЕЛЕЙ
// ---------------------------------------------------------------------------

TEST_CASE("удаление занятой вершины отказывает и называет держателей") {
    Bench b;
    HouseVertexTool vt(b.session);
    vt.set_world(&b.world);
    HouseLineTool lt(b.session);
    lt.set_world(&b.world);

    b.click(vt, {0.0f, 0.0f, 0.0f});
    b.click(vt, {4.0f, 0.0f, 0.0f});
    b.drag(lt, {0.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f});
    const ElementId line = lt.last_element();
    // Свободная вершина рядом — плечо, которое УДАЛЯЕТСЯ.
    b.click(vt, {0.0f, 0.0f, 8.0f});
    const VertexId lonely = b.session.selected_vertex();

    // ПЛЕЧО ОТКАЗА.
    b.click(vt, {0.0f, 0.0f, 0.0f});
    const VertexId held = b.session.selected_vertex();
    const std::size_t before = b.session.graph().vertex_count();
    CHECK_FALSE(vt.delete_selected());
    CHECK(b.session.graph().vertex_count() == before);
    // СПИСОК, А НЕ ФЛАГ: имя держателя обязано быть в предложении, иначе
    // человек пойдёт искать его руками по всему дому.
    char name[16];
    std::snprintf(name, sizeof(name), "e%u", static_cast<unsigned>(line));
    CHECK(vt.refusal().find(name) != std::string::npos);
    MESSAGE("отказ: " << vt.refusal());

    // ПЛЕЧО СОГЛАСИЯ: свободная вершина удаляется, и счётчик это подтверждает.
    // Без него «отказал» проходило бы на инструменте, который отказывает всегда.
    b.session.select_vertex(lonely);
    CHECK(vt.delete_selected());
    CHECK(b.session.graph().vertex_count() == before - 1);
    CHECK(vt.refusal().empty());
    CHECK(b.session.graph().vertex(held) != nullptr);
}

// ---------------------------------------------------------------------------
// 5. ОТМЕНА
// ---------------------------------------------------------------------------

TEST_CASE("отмена возвращает граф, повтор возвращает обратно") {
    Bench b;
    HouseVertexTool vt(b.session);
    vt.set_world(&b.world);

    b.click(vt, {0.0f, 0.0f, 0.0f});
    b.click(vt, {2.0f, 0.0f, 0.0f});
    b.click(vt, {4.0f, 0.0f, 0.0f});
    REQUIRE(b.session.graph().vertex_count() == 3);
    REQUIRE(b.history.undo_depth() == 3);

    // ОТМЕНА.
    const std::string back = b.history.undo();
    REQUIRE_FALSE(back.empty());
    REQUIRE(b.session.apply_snapshot(back));
    CHECK(b.session.graph().vertex_count() == 2);

    // ПОВТОР.
    const std::string fwd = b.history.redo();
    REQUIRE_FALSE(fwd.empty());
    REQUIRE(b.session.apply_snapshot(fwd));
    CHECK(b.session.graph().vertex_count() == 3);

    // ОТМЕНА ПРОТАСКИВАНИЯ ВОЗВРАЩАЕТ КООРДИНАТУ, а не только счётчик: счётчик
    // не изменился бы и на сломанной отмене, потому что двигание вершин их
    // число не меняет.
    const VertexId last = b.session.graph().vertices().back().id;
    b.session.select_vertex(last);
    const glm::vec3 was = b.session.vertex_world(last);
    b.drag(vt, was, {12.0f, 0.0f, 7.0f});
    CHECK(b.session.vertex_world(last).x == doctest::Approx(12.0f));
    const std::string undo_move = b.history.undo();
    REQUIRE_FALSE(undo_move.empty());
    REQUIRE(b.session.apply_snapshot(undo_move));
    CHECK(b.session.vertex_world(last).x == doctest::Approx(was.x));
    CHECK(b.session.vertex_world(last).z == doctest::Approx(was.z));

    // КОНТРОЛЬ ПУСТОЙ ИСТОРИИ: отменять нечего — пустая строка, и App по ней
    // отличает «нечего» от «состояние пустое». Снимок НИКОГДА не пуст: файл
    // начинается со строки «# dfh 1».
    EditorHistory fresh;
    CHECK(fresh.undo().empty());
    CHECK_FALSE(b.session.snapshot().empty());
}

TEST_CASE("отказ модели в историю не пишется") {
    // Шаг отмены, который ничего не откатывает, выглядит как сломанная отмена —
    // и ловится он только счётчиком, потому что на экране он ничем не отличим
    // от исправного.
    Bench b;
    HouseVertexTool vt(b.session);
    vt.set_world(&b.world);
    HouseLineTool lt(b.session);
    lt.set_world(&b.world);
    b.click(vt, {0.0f, 0.0f, 0.0f});
    b.click(vt, {4.0f, 0.0f, 0.0f});
    b.drag(lt, {0.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f});
    const std::size_t depth = b.history.undo_depth();

    b.click(vt, {0.0f, 0.0f, 0.0f});
    CHECK_FALSE(vt.delete_selected()); // отказ: вершину держит прямая
    CHECK(b.history.undo_depth() == depth);

    // КОНТРОЛЬ: разрешённое удаление шаг ПИШЕТ.
    b.click(vt, {0.0f, 0.0f, 9.0f});
    CHECK(vt.delete_selected());
    CHECK(b.history.undo_depth() > depth);
}

// ---------------------------------------------------------------------------
// 6. ПРЯМАЯ: ДВА ИСХОДА И ЗАЖИМ
// ---------------------------------------------------------------------------

TEST_CASE("одна механика прямой даёт два исхода") {
    Bench b;
    HouseVertexTool vt(b.session);
    vt.set_world(&b.world);
    HouseLineTool lt(b.session);
    lt.set_world(&b.world);

    b.click(vt, {0.0f, 0.0f, 0.0f});
    b.click(vt, {4.0f, 0.0f, 0.0f});

    // ИСХОД ПЕРВЫЙ: отпустил НА якоре — якоря соединились.
    b.drag(lt, {0.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f});
    const ElementId joined = lt.last_element();
    REQUIRE(joined != NO_ELEMENT);
    CHECK(b.session.graph().element(joined)->refs.size() == 2);
    CHECK(b.session.graph().param(joined, "length").empty());

    // ИСХОД ВТОРОЙ: отпустил в пустоте — прямая с длиной и углами из жеста.
    const glm::vec3 gesture{0.0f, 3.0f, 0.0f};
    b.drag(lt, glm::vec3{4.0f, 0.0f, 0.0f}, glm::vec3{4.0f, 0.0f, 0.0f} + gesture);
    const ElementId hanging = lt.last_element();
    REQUIRE(hanging != joined);
    REQUIRE(b.session.graph().element(hanging)->refs.size() == 1);
    const std::string len = b.session.graph().param(hanging, "length");
    REQUIRE_FALSE(len.empty());
    CHECK(std::stof(len) == doctest::Approx(3.0f).epsilon(0.001));

    // УГЛЫ ОБЯЗАНЫ ВОСПРОИЗВОДИТЬ ЖЕСТ, и проверяется это ТОЙ ЖЕ формулой,
    // которой пользуется построитель меша: иначе бревно встанет не туда, куда
    // его тянули, и виноватым будет выглядеть построитель.
    const float ax = std::stof(b.session.graph().param(hanging, "angle_x"));
    const float ay = std::stof(b.session.graph().param(hanging, "angle_y"));
    const glm::vec3 dir = house_dir_from_angles(ax, ay) * std::stof(len);
    CHECK(glm::length(dir - gesture) < 1e-3f);
    MESSAGE("жест (" << gesture.x << " " << gesture.y << " " << gesture.z
            << ") записан как length=" << len << " angle_x=" << ax << " angle_y=" << ay);

    // КРУГОВОЙ ПРОГОН: числа переживают запись и чтение. Без этого «параметры
    // записаны» значит только «записаны в память».
    HouseSession copy;
    REQUIRE(copy.apply_snapshot(b.session.snapshot()));
    CHECK(copy.graph().param(hanging, "length") == len);
}

TEST_CASE("наклонный жест: углы отыгрываются обратно") {
    // Три направления, включая горизонтальное (где angle_x = 90) и почти
    // вертикальное. Обратная формула обязана совпасть с прямой на всех трёх,
    // иначе призрак и дом разойдутся на наклонных балках — то есть на всех
    // стропилах.
    const glm::vec3 samples[] = {
        {3.0f, 0.0f, 0.0f}, {0.0f, 2.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {-2.0f, 0.5f, 4.0f}};
    for (const glm::vec3& v : samples) {
        float ax = 0.0f;
        float ay = 0.0f;
        house_angles_from_dir(v, ax, ay);
        const glm::vec3 back = house_dir_from_angles(ax, ay) * glm::length(v);
        CHECK(glm::length(back - v) < 1e-4f);
    }
}

TEST_CASE("зажим длины выбирает якорь сверху или снизу") {
    Bench b;
    HouseVertexTool vt(b.session);
    vt.set_world(&b.world);

    // Якорь-начало и два якоря НА ОДНОЙ ПРЯМОЙ с ним, на 2 и на 5 метрах.
    b.click(vt, {0.0f, 0.0f, 0.0f});
    const VertexId from = b.session.selected_vertex();
    b.click(vt, {2.0f, 0.0f, 0.0f});
    const VertexId near_v = b.session.selected_vertex();
    b.click(vt, {5.0f, 0.0f, 0.0f});
    const VertexId far_v = b.session.selected_vertex();
    // И ЯКОРЬ В СТОРОНЕ — он обязан быть НЕ ВЫБРАН ни при каком зажиме.
    b.click(vt, {3.4f, 0.0f, 6.0f});

    const glm::vec3 dir{1.0f, 0.0f, 0.0f};
    const float raw = 3.5f; // рука между двумя якорями

    const HouseClampHit none =
        house_clamp_length(b.session, from, dir, raw, dfn::app::HOUSE_CLAMP_AXIS_TOL_M,
                           HouseClamp::None);
    CHECK_FALSE(none.found);

    const HouseClampHit above =
        house_clamp_length(b.session, from, dir, raw, dfn::app::HOUSE_CLAMP_AXIS_TOL_M,
                           HouseClamp::Above);
    REQUIRE(above.found);
    CHECK(above.at == far_v);
    CHECK(above.length_m == doctest::Approx(5.0f));

    const HouseClampHit below =
        house_clamp_length(b.session, from, dir, raw, dfn::app::HOUSE_CLAMP_AXIS_TOL_M,
                           HouseClamp::Below);
    REQUIRE(below.found);
    CHECK(below.at == near_v);
    CHECK(below.length_m == doctest::Approx(2.0f));
    MESSAGE("рука на 3.5 м: вверх зажало до " << above.length_m << " м, вниз до "
            << below.length_m << " м");

    // И ЗАЖИМ ДОХОДИТ ДО САМОЙ ПРЯМОЙ, а не только до счётчика: тянем мимо
    // дальнего якоря с зажимом «ниже» — длина обязана сесть на ближний.
    HouseLineTool lt(b.session);
    lt.set_world(&b.world);
    lt.clamp_mode() = HouseClamp::Below;
    lt.on_press(Bench::at({0.0f, 0.0f, 0.0f}), b.world);
    lt.on_drag(Bench::at({3.5f, 0.0f, 0.0f}), 0.016f, b.world);
    CHECK(lt.ghost_end().x == doctest::Approx(2.0f));
    lt.on_release(b.world);
    const ElementId made = lt.last_element();
    REQUIRE(made != NO_ELEMENT);
    CHECK(std::stof(b.session.graph().param(made, "length")) == doctest::Approx(2.0f));

    // КОНТРОЛЬ: без зажима та же рука даёт 3.5 м. Без этого плеча утверждение
    // выше проходило бы на инструменте, который всегда строит по два метра.
    HouseLineTool free_tool(b.session);
    free_tool.set_world(&b.world);
    free_tool.on_press(Bench::at({0.0f, 0.0f, 0.0f}), b.world);
    free_tool.on_drag(Bench::at({3.5f, 0.0f, 0.0f}), 0.016f, b.world);
    CHECK(free_tool.ghost_end().x == doctest::Approx(3.5f));
}

// ---------------------------------------------------------------------------
// 7. ПОВЕРХНОСТЬ И НОРМАЛЬ
// ---------------------------------------------------------------------------

TEST_CASE("нормаль черновика переворачивается порядком обхода — до подтверждения") {
    // ЭТО ТРЕБОВАНИЕ §4.3 ЗАМЫСЛА ЦЕЛИКОМ: «порядок обхода задаёт нормаль,
    // поэтому подсказка обязана показывать, куда она смотрит, ДО подтверждения.
    // Иначе дизайнер узнает об этом по текстуре».
    Bench b;
    HouseVertexTool vt(b.session);
    vt.set_world(&b.world);
    HouseSurfaceTool st(b.session);
    st.set_world(&b.world);

    b.click(vt, {0.0f, 0.0f, 0.0f});
    b.click(vt, {4.0f, 0.0f, 0.0f});
    b.click(vt, {4.0f, 0.0f, 4.0f});
    b.click(vt, {0.0f, 0.0f, 4.0f});
    const std::vector<glm::vec3> corners = {
        {0.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 4.0f}, {0.0f, 0.0f, 4.0f}};

    // ЗАМЫСЕЛ «ЭТО КОНТУР» ОБЪЯВЛЕН ДО ОБХОДА: у четырёх точек два будущих —
    // пол и стена, — и лицо у них смотрит в разные стороны. Стрелка обязана
    // показывать то из них, которое человек строит, а не выбранное за него.
    st.closing() = true;
    for (const glm::vec3& c : corners) {
        st.on_press(Bench::at(c), b.world);
    }
    REQUIRE(st.refs().size() == 4);
    glm::vec3 n_forward{0.0f};
    REQUIRE(st.draft_normal(n_forward));
    CHECK(std::fabs(n_forward.y) > 0.99f); // пол лежит: лицо смотрит по вертикали

    // ОБРАТНЫЙ ОБХОД — тот же контур, те же щелчки, порядок наоборот.
    st.clear_draft();
    st.closing() = true;
    for (auto it = corners.rbegin(); it != corners.rend(); ++it) {
        st.on_press(Bench::at(*it), b.world);
    }
    glm::vec3 n_back{0.0f};
    REQUIRE(st.draft_normal(n_back));
    CHECK(glm::dot(n_forward, n_back) < -0.99f);
    MESSAGE("нормаль по обходу: (" << n_forward.x << " " << n_forward.y << " "
            << n_forward.z << "), против обхода: (" << n_back.x << " " << n_back.y
            << " " << n_back.z << ")");

    // ПОДПИСЬ ГОВОРИТ ТО ЖЕ САМОЕ СЛОВАМИ. Стрелку с ребра камеры видно плохо,
    // и подпись — второй прибор на тот же вопрос, а не украшение.
    const dfn::app::ToolStatus s = st.status(Bench::at({2.0f, 0.0f, 2.0f}));
    CHECK(s.text.find("лицо") != std::string::npos);
    MESSAGE("подпись: " << s.text);

    // И СТРЕЛКА ЕСТЬ В КАРТИНКЕ: без неё «показывает» значило бы «знает, но
    // молчит».
    const ToolPreview p = st.preview(Bench::at({2.0f, 0.0f, 2.0f}));
    REQUIRE(p.accent != nullptr);
    CHECK(p.accent->size() >= 8); // ствол + две зазубрины
}

TEST_CASE("замкнул на первой — контур, оставил цепочкой — стена") {
    Bench b;
    HouseVertexTool vt(b.session);
    vt.set_world(&b.world);
    HouseSurfaceTool st(b.session);
    st.set_world(&b.world);

    const std::vector<glm::vec3> corners = {
        {0.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 4.0f}, {0.0f, 0.0f, 4.0f}};
    for (const glm::vec3& c : corners) {
        b.click(vt, c);
    }

    // ПЛЕЧО КОНТУРА: обошёл и ЗАМКНУЛ НА ПЕРВОЙ.
    for (const glm::vec3& c : corners) {
        st.on_press(Bench::at(c), b.world);
    }
    st.on_press(Bench::at(corners.front()), b.world); // замыкание
    const ElementId contour = st.last_element();
    REQUIRE(contour != NO_ELEMENT);
    CHECK(b.session.graph().element(contour)->closed);
    CHECK(b.session.graph().element(contour)->refs.size() == 4);
    CHECK(b.session.graph().param(contour, "height").empty());
    CHECK(st.refs().empty()); // обход закончился

    // ПЛЕЧО ЦЕПОЧКИ: два якоря и подтверждение — стена с высотой.
    st.on_press(Bench::at(corners[0]), b.world);
    st.on_press(Bench::at(corners[1]), b.world);
    st.height_m() = 2.5f;
    st.on_confirm(b.world);
    const ElementId wall = st.last_element();
    REQUIRE(wall != contour);
    CHECK_FALSE(b.session.graph().element(wall)->closed);
    CHECK(std::stof(b.session.graph().param(wall, "height")) == doctest::Approx(2.5f));

    // ЗАМКНУТОСТЬ — ЖЕСТ, А НЕ СЛЕДСТВИЕ ЧИСЕЛ: у контура высоты нет, у стены
    // есть, и различает их поле closed, а не сравнение высоты с нулём.
    CHECK(b.session.graph().element(contour)->closed
          != b.session.graph().element(wall)->closed);

    // ОТКАЗ: контур из двух якорей — не контур. Проверяется потому, что модель
    // такого элемента не создаст, а инструмент обязан сказать об этом ДО, а не
    // молча ничего не сделать.
    st.on_press(Bench::at(corners[0]), b.world);
    st.on_press(Bench::at(corners[1]), b.world);
    st.on_press(Bench::at(corners[0]), b.world); // замкнуть на двух
    CHECK(st.last_element() == wall);            // ничего нового не создалось
}

TEST_CASE("нормаль черновика и нормаль готового элемента — одно и то же") {
    // ЕСЛИ ЭТИ ДВА ЧИСЛА РАЗОЙДУТСЯ, стрелка станет врать ровно в тот момент,
    // когда на неё смотрят: человек подтвердит контур, увидев одно, и получит
    // текстуру на изнанке.
    Bench b;
    HouseVertexTool vt(b.session);
    vt.set_world(&b.world);
    HouseSurfaceTool st(b.session);
    st.set_world(&b.world);

    const std::vector<glm::vec3> corners = {
        {0.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 3.0f}};
    for (const glm::vec3& c : corners) {
        b.click(vt, c);
    }
    st.closing() = true;
    for (const glm::vec3& c : corners) {
        st.on_press(Bench::at(c), b.world);
    }
    glm::vec3 draft{0.0f};
    REQUIRE(st.draft_normal(draft));
    st.on_press(Bench::at(corners.front()), b.world); // замкнуть и создать
    const ElementId made = st.last_element();
    REQUIRE(made != NO_ELEMENT);

    glm::vec3 built{0.0f};
    REQUIRE(dfn::world::surface_normal(b.session.graph(), made, built));
    CHECK(glm::dot(draft, built) > 0.999f);

    // ПЕРЕВОРОТ ЛИЦА ТОЖЕ ДОХОДИТ ДО ЭЛЕМЕНТА, а не остаётся галочкой в панели.
    st.flipped() = true;
    st.closing() = true;
    for (const glm::vec3& c : corners) {
        st.on_press(Bench::at(c), b.world);
    }
    glm::vec3 draft_flipped{0.0f};
    REQUIRE(st.draft_normal(draft_flipped));
    CHECK(glm::dot(draft, draft_flipped) < -0.99f);
    st.on_press(Bench::at(corners.front()), b.world);
    glm::vec3 built_flipped{0.0f};
    REQUIRE(dfn::world::surface_normal(b.session.graph(), st.last_element(), built_flipped));
    CHECK(glm::dot(draft_flipped, built_flipped) > 0.999f);
}

// ---------------------------------------------------------------------------
// 8. ВЫБОР И ПОДСВЕТКА
// ---------------------------------------------------------------------------

TEST_CASE("выбрал вершину — светятся её элементы, выбрал элемент — его вершины") {
    Bench b;
    HouseVertexTool vt(b.session);
    vt.set_world(&b.world);
    HouseLineTool lt(b.session);
    lt.set_world(&b.world);

    b.click(vt, {0.0f, 0.0f, 0.0f});
    const VertexId hub = b.session.selected_vertex();
    b.click(vt, {4.0f, 0.0f, 0.0f});
    b.click(vt, {0.0f, 0.0f, 4.0f});
    b.click(vt, {-4.0f, 0.0f, 0.0f}); // одинокая: ни к чему не привязана
    const VertexId lonely = b.session.selected_vertex();
    b.drag(lt, {0.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f});
    const ElementId e1 = lt.last_element();
    b.drag(lt, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 4.0f});
    const ElementId e2 = lt.last_element();

    b.session.select_vertex(hub);
    CHECK(b.session.lit_elements().size() == 2);
    CHECK(b.session.lit_vertices().empty());
    const auto& lit = b.session.lit_elements();
    CHECK(std::find(lit.begin(), lit.end(), e1) != lit.end());
    CHECK(std::find(lit.begin(), lit.end(), e2) != lit.end());

    // КОНТРОЛЬ: одинокая вершина светит НИЧЕМ. Без него «подсветка работает»
    // проходило бы на коде, который подсвечивает всё подряд.
    b.session.select_vertex(lonely);
    CHECK(b.session.lit_elements().empty());

    // ОБРАТНАЯ СТОРОНА: выбрал элемент — светятся его вершины.
    b.session.select_element(e1);
    CHECK(b.session.lit_vertices().size() == 2);
    CHECK(b.session.lit_elements().empty());

    // И ПОДСВЕТКА ДОХОДИТ ДО КАРТИНКИ ОТДЕЛЬНЫМ ЦВЕТОМ: одна стопка на всё
    // означала бы, что выбранное и невыбранное выглядят одинаково.
    const ToolPreview p = vt.preview(Bench::at({50.0f, 0.0f, 50.0f}));
    REQUIRE(p.accent != nullptr);
    REQUIRE(p.handles != nullptr);
    CHECK_FALSE(p.accent->empty());
    CHECK(p.accent_color != p.line_color);

    // ВЫБОР НЕ ПЕРЕЖИВАЕТ УДАЛЕНИЯ. Подсветка, указывающая на исчезнувший
    // элемент, — это правда, разъехавшаяся с моделью (правило 39).
    (void)b.session.graph().remove_element(e1);
    b.session.refresh_selection();
    CHECK(b.session.selected_element() == NO_ELEMENT);
    CHECK(b.session.lit_vertices().empty());
}

// ---------------------------------------------------------------------------
// 9. СНИМОК — ОСНОВА ОТМЕНЫ
// ---------------------------------------------------------------------------

TEST_CASE("снимок переживает круговой прогон") {
    // ЭТО ПРОВЕРКА НЕ ИНСТРУМЕНТА, А ФУНДАМЕНТА ПОД НИМ: отмена хранит
    // состояние ТЕКСТОМ, и если запись теряет вершины, отмена вернёт не то
    // состояние, а урезанное — молча.
    Bench b;
    HouseVertexTool vt(b.session);
    vt.set_world(&b.world);
    HouseLineTool lt(b.session);
    lt.set_world(&b.world);
    for (int i = 0; i < 5; ++i) {
        b.click(vt, {static_cast<float>(i) * 3.0f, 0.0f, 0.0f});
    }
    b.drag(lt, {0.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f});
    REQUIRE(b.session.graph().vertex_count() == 5);
    REQUIRE(b.session.graph().element_count() == 1);

    HouseSession copy;
    REQUIRE(copy.apply_snapshot(b.session.snapshot()));
    CHECK(copy.graph().vertex_count() == 5);
    CHECK(copy.graph().element_count() == 1);
    // ПОБАЙТОВО ТО ЖЕ САМОЕ: иначе «до» и «после» в истории значат разное, и
    // отмена начинает возвращать похожее состояние вместо того же.
    CHECK(copy.snapshot() == b.session.snapshot());
}

TEST_CASE("запись переживает удаления: выживший якорь доезжает до снимка") {
    // ЗДЕСЬ БЫЛА НАХОДКА, И ЕЁ ПОЧИНИЛИ. write_house перебирал возможные имена
    // от 1 до vertex_count() + element_count() + 1, а после удалений имена
    // выживших БОЛЬШЕ этого числа — они не попадали в файл вообще. Измерено
    // этим случаем: 5 якорей, 4 удалить, выживает v5, снимок целиком —
    // «# dfh 1», то есть cmd+Z стирал дом молча. Зона core заменила перебор на
    // vertices()/elements() (18.08), и утверждение ниже стало настоящим CHECK
    // вместо WARN.
    Bench b;
    HouseVertexTool vt(b.session);
    vt.set_world(&b.world);
    for (int i = 0; i < 5; ++i) {
        b.click(vt, {static_cast<float>(i) * 3.0f, 0.0f, 0.0f});
    }
    for (int i = 0; i < 4; ++i) {
        b.click(vt, {static_cast<float>(i) * 3.0f, 0.0f, 0.0f});
        REQUIRE(vt.delete_selected());
    }
    REQUIRE(b.session.graph().vertex_count() == 1);
    const glm::vec3 place = b.session.vertex_world(b.session.graph().vertices().front().id);

    HouseSession copy;
    REQUIRE(copy.apply_snapshot(b.session.snapshot()));
    CHECK(copy.graph().vertex_count() == 1);
    // ОПОЗНАЁМ ПО МЕСТУ, А НЕ ПО ИМЕНИ: имена чтение выдаёт заново (следующий
    // случай меряет это числом).
    CHECK(copy.unique_vertex_at(place, dfn::app::HOUSE_REID_TOL_M) != NO_VERTEX);
}

TEST_CASE("после отмены имя вершины ДОСТАЁТСЯ ДРУГОЙ ТОЧКЕ — рука это переживает") {
    // ДВА ЗАМЕРА, ОБА ВЕРНЫЕ, И МЕЖДУ НИМИ ЛЁГ КОММИТ. Зона core предупредила,
    // что чтение выдаёт имена заново («v3 v4 v5» на входе, «v1 v2 v3» на
    // выходе) — так и было. Я мерил после её же починки (146f484, «имена
    // вершин переживают круговой прогон») и увидел обратное. Утверждение ниже
    // держит НЫНЕШНЕЕ поведение: v5 остаётся v5. У читателя есть и свой рукав в
    // зоне core, так что молчаливый откат покраснеет с двух сторон.
    //
    // ЭТО НЕ ОТМЕНЯЕТ ЗАЩИТЫ ИНСТРУМЕНТОВ. Почему — в следующем случае и в
    // комментарии у HouseSession::revision().
    //
    // НО ОПАСНОСТЬ, ПРО КОТОРУЮ ОНА ГОВОРИЛА, НАСТОЯЩАЯ — просто на шаг
    // дальше: СЧЁТЧИК ИМЁН ОТКАТЫВАЕТСЯ ВМЕСТЕ С ГРАФОМ. Измерено отдельным
    // прогоном: три вершины, снимок, четвёртая получает имя v4 в точке (99);
    // отмена до снимка; следующая созданная вершина СНОВА получает имя v4, но
    // стоит уже в (-77). Рука, помнящая v4 с прошлой жизни графа, показывает
    // теперь на другую точку — и молчит об этом.
    Bench b;
    HouseVertexTool vt(b.session);
    vt.set_world(&b.world);
    HouseSurfaceTool st(b.session);
    st.set_world(&b.world);
    HouseLineTool lt(b.session);
    lt.set_world(&b.world);

    for (int i = 0; i < 4; ++i) {
        b.click(vt, {static_cast<float>(i) * 3.0f, 0.0f, 0.0f});
    }
    b.click(vt, {0.0f, 0.0f, 0.0f});
    REQUIRE(vt.delete_selected());
    const std::vector<VertexId> before_ids = {b.session.graph().vertices()[0].id,
                                              b.session.graph().vertices()[1].id,
                                              b.session.graph().vertices()[2].id};

    // РУКА ДЕРЖИТ ИМЕНА: обход поверхности из трёх якорей и якорь прямой.
    for (const VertexId v : before_ids) {
        st.on_press(Bench::at(b.session.vertex_world(v)), b.world);
    }
    REQUIRE(st.refs().size() == 3);
    lt.on_press(Bench::at(b.session.vertex_world(before_ids[0])), b.world);
    REQUIRE(lt.anchor() != NO_VERTEX);
    // Выбор ставится ПОСЛЕДНИМ: щелчок по якорю выбирает его, и снимок застанет
    // именно тот, по которому щёлкнули последним.
    b.session.select_vertex(before_ids[1]);
    const glm::vec3 place = b.session.vertex_world(before_ids[1]);

    const std::uint32_t rev_before = b.session.revision();
    REQUIRE(b.session.apply_snapshot(b.session.snapshot()));
    CHECK(b.session.revision() == rev_before + 1);

    const std::vector<VertexId> after_ids = {b.session.graph().vertices()[0].id,
                                             b.session.graph().vertices()[1].id,
                                             b.session.graph().vertices()[2].id};
    CHECK(after_ids == before_ids); // имена ПЕРЕЖИЛИ прогон — измерено, а не принято
    MESSAGE("имена до снимка: v" << before_ids[0] << " v" << before_ids[1] << " v"
            << before_ids[2] << "; после: v" << after_ids[0] << " v" << after_ids[1]
            << " v" << after_ids[2]);

    // ВЫБОР ПЕРЕУСТАНОВЛЕН ПО МЕСТУ. Он и по имени бы уцелел, но опознание идёт
    // по координате нарочно: имя после отмены — обещание читателя, а место —
    // свойство самой вершины.
    REQUIRE(b.session.selected_vertex() != NO_VERTEX);
    CHECK(glm::length(b.session.vertex_world(b.session.selected_vertex()) - place) < 1e-3f);

    // ОБХОД БРОШЕН, А НЕ ДОСТРОЕН НА ЯКОРЯХ ИЗ ПРОШЛОЙ ЖИЗНИ ГРАФА.
    CHECK(st.stale());
    glm::vec3 n{0.0f};
    CHECK_FALSE(st.draft_normal(n)); // черновик молчит, пока рука не признала отмену
    CHECK_FALSE(st.confirm(b.world));
    CHECK(st.refs().empty());
    CHECK_FALSE(st.refusal().empty());
    MESSAGE("поверхность после отмены: " << st.refusal());

    // ЯКОРЬ ПРЯМОЙ ТОЖЕ БРОШЕН: отпускание после отмены не строит бревно.
    const std::size_t elements_before = b.session.graph().element_count();
    lt.on_release(b.world);
    CHECK(b.session.graph().element_count() == elements_before);
    CHECK(lt.anchor() == NO_VERTEX);

    // КОНТРОЛЬ: БЕЗ ОТМЕНЫ ТА ЖЕ РУКА РАБОТАЕТ. Без этого плеча «инструмент
    // ничего не сделал» проходило бы на инструменте, сломанном насовсем.
    for (const VertexId v : after_ids) {
        st.on_press(Bench::at(b.session.vertex_world(v)), b.world);
    }
    CHECK(st.refs().size() == 3);
    CHECK(st.confirm(b.world));
    CHECK(b.session.graph().element_count() == elements_before + 1);
}

TEST_CASE("имя вершины после отмены достаётся другой точке") {
    // ЧИСЛО, РАДИ КОТОРОГО ЖИВЁТ ВЕСЬ МЕХАНИЗМ stale(). Проверяется на голой
    // модели, без инструментов: счётчик имён откатывается вместе с графом.
    Bench b;
    HouseVertexTool vt(b.session);
    vt.set_world(&b.world);
    b.click(vt, {0.0f, 0.0f, 0.0f});
    b.click(vt, {3.0f, 0.0f, 0.0f});
    b.click(vt, {6.0f, 0.0f, 0.0f});
    const std::string three = b.session.snapshot();
    b.click(vt, {99.0f, 0.0f, 0.0f});
    const VertexId fourth = b.session.selected_vertex();

    REQUIRE(b.session.apply_snapshot(three));
    b.click(vt, {-77.0f, 0.0f, 0.0f});
    const VertexId again = b.session.selected_vertex();
    CHECK(again == fourth); // ТО ЖЕ ИМЯ
    CHECK(b.session.vertex_world(again).x == doctest::Approx(-77.0f)); // ДРУГАЯ ТОЧКА
    MESSAGE("до отмены v" << fourth << " стояла в 99 м, после отмены v" << again
            << " стоит в " << b.session.vertex_world(again).x << " м");
}

TEST_CASE("числа, записанные инструментом, доходят до геометрии") {
    // ШОВ, КОТОРЫЙ БЫЛ РАЗОРВАН И СВЕДЁН 18.08: инструменты пишут числа в
    // Element::params (дверь модели), а построитель меша до 18:26 читал их из
    // СТРОКИ СТИЛЯ. Пока шов был разорван, стена, созданная рукой, получалась
    // без высоты — и увидеть это можно было только на готовом доме.
    Bench b;
    HouseVertexTool vt(b.session);
    vt.set_world(&b.world);
    HouseSurfaceTool st(b.session);
    st.set_world(&b.world);

    b.click(vt, {0.0f, 0.0f, 0.0f});
    b.click(vt, {5.0f, 0.0f, 0.0f});
    st.on_press(Bench::at({0.0f, 0.0f, 0.0f}), b.world);
    st.on_press(Bench::at({5.0f, 0.0f, 0.0f}), b.world);
    st.height_m() = 2.5f;
    st.thickness_m() = 0.2f;
    REQUIRE(st.confirm(b.world));

    const dfn::world::HouseMesh mesh = dfn::world::build_house_mesh(b.session.graph());
    CHECK(mesh.triangle_count() > 0);
    for (const dfn::world::MeshFinding& f : mesh.findings) {
        CHECK(f.issue != dfn::world::MeshIssue::ChainNeedsHeight);
    }
    // ВЫСОТА ДОЕХАЛА ЧИСЛОМ, а не «меш непустой»: стена в 2.5 м обязана быть
    // ровно 2.5 м высотой, иначе число из панели и число в мире — разные.
    float lo = 1e9f;
    float hi = -1e9f;
    for (const dfn::world::HouseVertex& v : mesh.vertices) {
        lo = std::min(lo, v.pos.y);
        hi = std::max(hi, v.pos.y);
    }
    CHECK((hi - lo) == doctest::Approx(2.5f));
    MESSAGE("стена инструмента: треугольников " << mesh.triangle_count() << ", высота "
            << (hi - lo) << " м");

    // И ЛИЦО ЦЕПОЧКИ СОШЛОСЬ С МЕШЕМ. До миграции я считал по closed (жест), а
    // построитель по height (число), и на открытой цепочке из трёх якорей они
    // расходились. Теперь оба смотрят на closed — проверяем на трёх, а не на
    // двух, потому что расходились именно три.
    b.click(vt, {5.0f, 0.0f, 4.0f});
    st.on_press(Bench::at({0.0f, 0.0f, 0.0f}), b.world);
    st.on_press(Bench::at({5.0f, 0.0f, 0.0f}), b.world);
    st.on_press(Bench::at({5.0f, 0.0f, 4.0f}), b.world);
    glm::vec3 draft{0.0f};
    REQUIRE(st.draft_normal(draft));
    REQUIRE(st.confirm(b.world));
    glm::vec3 built{0.0f};
    REQUIRE(dfn::world::surface_normal(b.session.graph(), st.last_element(), built));
    CHECK(glm::dot(draft, built) > 0.999f);
}

// ---------------------------------------------------------------------------
// ПРЯМАЯ ВВЕРХ — ФИКСАЦИЯ ОСИ
// ---------------------------------------------------------------------------

TEST_CASE("прямая вверх: без оси она ложится на землю, с осью встаёт") {
    Bench b;
    HouseVertexTool vertex(b.session);
    vertex.set_world(&b.world);
    b.click(vertex, {0.0f, 0.0f, 0.0f});
    const VertexId a = b.session.selected_vertex();
    REQUIRE(a != NO_VERTEX);
    HouseLineTool line(b.session);
    line.set_world(&b.world);

    // Прицел, ушедший ВЫШЕ ГОРИЗОНТА: земля не встречена, точка — «вытянутая
    // рука в пустоте». Ровно так выглядит попытка потянуть стойку вверх.
    ToolAim up;
    up.origin = {0.0f, 1.6f, -3.0f};
    const glm::vec3 dir = glm::normalize(glm::vec3{0.0f, 0.5f, 1.0f});
    up.point = up.origin + dir * 100.0f;
    up.distance_m = 100.0f;
    up.hit = false;

    SUBCASE("земля: прямая уезжает вдаль и остаётся лежать") {
        // ПЛЕЧО-КОНТРОЛЬ, и оно же снимок дефекта, который пользователь показал
        // кадром 18.08. Луч упирается в землю далеко впереди — конец прямой
        // уходит туда же, а вверх не поднимается ВОВСЕ.
        ToolAim far_ground = Bench::at({0.0f, 0.0f, 40.0f});
        b.session.set_axis(HouseSession::Axis::Ground);
        line.on_press(Bench::at({0.0f, 0.0f, 0.0f}), b.world);
        REQUIRE(line.anchor() == a);
        line.on_drag(far_ground, 0.016f, b.world);
        const glm::vec3 end = line.ghost_end();
        CHECK(end.y == doctest::Approx(0.0f).epsilon(0.01));
        CHECK(std::hypot(end.x, end.z) > 10.0f);
    }

    SUBCASE("вертикаль: конец стоит НАД якорем, и высота растёт с наклоном") {
        b.session.set_axis(HouseSession::Axis::Vertical);
        line.on_press(Bench::at({0.0f, 0.0f, 0.0f}), b.world);
        REQUIRE(line.anchor() == a);
        line.on_drag(up, 0.016f, b.world);
        const glm::vec3 end = line.ghost_end();
        // НАД ЯКОРЕМ — это две координаты из трёх, и обе обязаны совпасть.
        CHECK(end.x == doctest::Approx(0.0f).epsilon(0.001));
        CHECK(end.z == doctest::Approx(0.0f).epsilon(0.001));
        // Ближайшая точка оси к этому лучу считается руками: ось пересекает
        // плоскость z=0 там, где луч в неё приходит, y = 1.6 + 0.4472*3.354.
        CHECK(end.y == doctest::Approx(3.1f).epsilon(0.02));

        // ВЫШЕ ПРИЦЕЛ — ВЫШЕ КОНЕЦ. Без этого утверждения проверку прошёл бы
        // инструмент, который всегда возвращает одну и ту же высоту.
        ToolAim higher = up;
        const glm::vec3 steep = glm::normalize(glm::vec3{0.0f, 1.2f, 1.0f});
        higher.point = higher.origin + steep * 100.0f;
        line.on_drag(higher, 0.016f, b.world);
        CHECK(line.ghost_end().y > end.y + 1.0f);
    }
}

TEST_CASE("ось отпускает ящик к небу только пока она включена") {
    Bench b;
    HouseVertexTool vertex(b.session);
    vertex.set_world(&b.world);
    b.click(vertex, {0.0f, 0.0f, 0.0f});
    HouseLineTool line(b.session);
    line.set_world(&b.world);

    // Без якоря в руке земля нужна всегда: щелчок по небу не должен начинать
    // ничего, и фиксация оси этого не меняет.
    b.session.set_axis(HouseSession::Axis::Vertical);
    CHECK_FALSE(line.aims_without_ground());

    line.on_press(Bench::at({0.0f, 0.0f, 0.0f}), b.world);
    CHECK(line.aims_without_ground());

    // Снятая ось возвращает обычное правило — и это контроль: без него
    // проверка прошла бы на инструменте, который землю не спрашивает НИКОГДА.
    b.session.set_axis(HouseSession::Axis::Ground);
    CHECK_FALSE(line.aims_without_ground());
}

TEST_CASE("Enter подтверждает черновик только когда черновик есть") {
    Bench b;
    HouseVertexTool vertex(b.session);
    vertex.set_world(&b.world);
    HouseSurfaceTool surface(b.session);
    surface.set_world(&b.world);

    b.click(vertex, {0.0f, 0.0f, 0.0f});
    b.click(vertex, {3.0f, 0.0f, 0.0f});
    b.click(vertex, {3.0f, 0.0f, 3.0f});

    // ОДИН ЯКОРЬ — ЕЩЁ НЕ ЧЕРНОВИК, и это плечо-контроль: без него правило
    // прошло бы на инструменте, который отвечает «да» всегда — а такой отнял
    // бы Enter у быстрой заметки навсегда.
    b.click(surface, {0.0f, 0.0f, 0.0f});
    CHECK_FALSE(surface.has_draft());
    const std::size_t before = b.session.graph().element_count();
    surface.on_confirm(b.world);
    CHECK(b.session.graph().element_count() == before);

    b.click(surface, {3.0f, 0.0f, 0.0f});
    CHECK(surface.has_draft());
    surface.on_confirm(b.world);
    CHECK(b.session.graph().element_count() == before + 1);
}
