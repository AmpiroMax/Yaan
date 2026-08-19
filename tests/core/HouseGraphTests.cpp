/*
Created: 18:08:2026 - 16:48:18
Last updated: 20:08:2026 - 12:55:00
Module: tests
File: tests/core/HouseGraphTests.cpp

Responsibility:
- ПЕРВЫЙ СРЕЗ ГИПЕРГРАФА ПОСТРОЙКИ. Держит то, ради чего вся модель затевалась:
  связи существуют и их видно, а занятую вершину нельзя удалить молча.

Key items:
- Гиперрёбра: элемент на пяти вершинах — это ОДИН элемент, а не четыре ребра.
- Мультирёбра: балка И стена между теми же двумя якорями сосуществуют.
- Отказ на удаление занятой вершины СО СПИСКОМ держателей.
- Вершина на оси: удалить хозяина, пока на нём сидят, нельзя.

Dependencies:
- Uses: doctest, dfn_world.
- Used by: ctest (test_house_graph).

AI Agents Notice (must follow):
- Правило 30: проверка обязана уметь краснеть. Здесь она краснеет, если
  инцидентность перестанет находить элемент, если отказ станет молчаливым, и
  если гиперребро развалится на пары.
*/
/*
UPD:
- 18:08:2026 - 16:48:18: Создан вместе с первым срезом модели.
- 18:08:2026 - 16:59:04: компоненты и мосты. Случай про гиперребро-мост исправил МЕНЯ, а не код.
  Контрфакт: выродил гиперребро в связь первой пары — 2 случая красных.
- 18:08:2026 - 17:06:23: каскад, круговой прогон, отказы читателя. Случай про цикл нашёл изъян
  НЕ ТАМ, где искал: читатель не давал сослаться на вершину, сидящую на оси.
- 18:08:2026 - 17:47:28: числа и замкнутость в круговом прогоне; параметр против ссылки.
- 18:08:2026 - 17:54:03: регистрация типа. ПЕРВАЯ ВЕРСИЯ СЛУЧАЯ БЫЛА СЛАБОЙ, и это поймал
  контрфакт: я убирал ПЕРВУЮ сторону каркаса, и «проверять только одну пару
  вместо всех» оставалось ЗЕЛЁНЫМ — сломанная проверка смотрит как раз первую
  пару и находит ровно ту дыру, которую я проделал. Теперь убирается СРЕДНЯЯ
  сторона (ловится только обходом всех пар) и отдельным заходом ЗАМЫКАЮЩАЯ: у
  замкнутого контура последняя пара считается через возврат к началу, и её
  легко потерять именно на этом.
- 18:08:2026 - 18:26:06: случай про запись после удалений. Изъян нашла ЗОНА ИНСТРУМЕНТОВ, замером
  отмены: писатель перебирал имена по счётчику живых, а имена не
  переиспользуются — после «поставил 5, удалил 4» выживает v5, цикл смотрел
  1..2, снимок выходил ПУСТЫМ, и cmd+Z стирал постройку. Читатель на пустой
  файл не жалуется, поэтому круговой прогон выглядел успешным.
- 18:08:2026 - 18:43:05: имена переживают круговой прогон. И ИСПРАВЛЕН МОЙ СОБСТВЕННЫЙ СЛУЧАЙ,
  написанный часом раньше: он проверял vertex(1), то есть закреплял ТОГДАШНЕЕ
  ошибочное поведение читателя. Проверка, написанная под наблюдаемое, а не под
  нужное, охраняет дефект.
- 20:08:2026 - 12:55:00: Версия растёт только от удавшегося изменения; скольжение OnEdge-вершины.
*/

#include <doctest/doctest.h>

#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseGraph.h"
#include "engine/world/sources/HouseRegister.h"

#include <algorithm>
#include <cmath>

using dfn::world::Anchoring;
using dfn::world::ElementId;
using dfn::world::ElementKind;
using dfn::world::HouseGraph;
using dfn::world::VertexId;

namespace {

/// Четыре угла на земле — самый частый набросок: пол комнаты.
struct Room {
    HouseGraph g;
    VertexId a, b, c, d;
    Room() {
        a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
        b = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 0.0f});
        c = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 3.0f});
        d = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 3.0f});
    }
};

} // namespace

TEST_CASE("пол на четырёх вершинах — ОДИН элемент, а не четыре ребра") {
    Room r;
    ElementId floor = 0;
    // ЭТО И ЕСТЬ ГИПЕРРЕБРО, ради которого выбран гиперграф. Обычное ребро
    // соединяет двоих; пол соединяет четверых ОДНИМ элементом. Разложи его на
    // пары — и «выбрал пол, подсветились его вершины» перестанет работать: пар
    // будет четыре, а пол один.
    REQUIRE(r.g.add_element(ElementKind::Surface, {r.a, r.b, r.c, r.d}, "oak", floor).ok);
    CHECK(r.g.element_count() == 1);
    REQUIRE(r.g.element(floor) != nullptr);
    CHECK(r.g.element(floor)->refs.size() == 4);

    // И обратный ход: каждая из четырёх вершин знает про этот пол.
    for (const VertexId v : {r.a, r.b, r.c, r.d}) {
        const auto inc = r.g.incident(v);
        REQUIRE(inc.size() == 1);
        CHECK(inc.front() == floor);
    }
}

TEST_CASE("между двумя якорями живут ДВА элемента разом") {
    Room r;
    ElementId beam = 0;
    ElementId wall = 0;
    // МУЛЬТИРЁБРА. Пользователь назвал это прямо: «будут мультирёбра просто
    // разных свойств». Балка и стена между теми же двумя якорями — не одно и то
    // же, и модель обязана держать оба, не считая второй элемент дубликатом.
    REQUIRE(r.g.add_element(ElementKind::Line, {r.a, r.b}, "oak-beam", beam).ok);
    REQUIRE(r.g.add_element(ElementKind::Surface, {r.a, r.b}, "frame-oak", wall).ok);
    CHECK(beam != wall);
    CHECK(r.g.element_count() == 2);

    const auto inc = r.g.incident(r.a);
    CHECK(inc.size() == 2);
    CHECK(std::find(inc.begin(), inc.end(), beam) != inc.end());
    CHECK(std::find(inc.begin(), inc.end(), wall) != inc.end());
}

TEST_CASE("занятую вершину не удалить, и отказ НАЗЫВАЕТ держателей") {
    Room r;
    ElementId floor = 0;
    REQUIRE(r.g.add_element(ElementKind::Surface, {r.a, r.b, r.c, r.d}, "oak", floor).ok);

    // ОТКАЗ СО СПИСКОМ, а не флагом. Решение пользователя: «удалять бревно
    // нельзя давать, пока к нему что-то привязано». Список проверяется отдельно
    // от самого отказа: молчаливый отказ на пятом этаже превращается в поиск
    // виноватого руками, и это ровно тот сорт молчания, которым нас уже били.
    const auto refused = r.g.remove_vertex(r.a);
    CHECK_FALSE(refused.ok);
    CHECK_FALSE(refused.why.empty());
    REQUIRE(refused.blockers.size() == 1);
    CHECK(refused.blockers.front() == floor);
    CHECK(r.g.vertex(r.a) != nullptr); // и вершина на месте

    // СНЯЛИ ДЕРЖАТЕЛЯ — УДАЛЯЕТСЯ. Контроль обязателен: без него утверждение
    // прошло бы и на модели, которая не удаляет вершины ВООБЩЕ НИКОГДА.
    REQUIRE(r.g.remove_element(floor).ok);
    CHECK(r.g.remove_vertex(r.a).ok);
    CHECK(r.g.vertex(r.a) == nullptr);
}

TEST_CASE("вершина сидит на оси прямой, и хозяина при ней не убрать") {
    HouseGraph g;
    const VertexId low = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId high = g.add_vertex(Anchoring::Free, {0.0f, 3.0f, 0.0f});
    ElementId post = 0;
    REQUIRE(g.add_element(ElementKind::Line, {low, high}, "oak", post).ok);

    // «НА ПРЯМЫЕ ПО НАШЕЙ ЗАДАННОЙ ВЫСОТЕ ПОСТАВИЛИ ЕЩЁ ЯКОРЯ» — его слова, и
    // это то, из чего получается второй ярус.
    VertexId mid = 0;
    REQUIRE(g.add_vertex_on_edge(post, 0.5f, mid).ok);
    REQUIRE(g.vertex(mid) != nullptr);
    CHECK(g.vertex(mid)->anchoring == Anchoring::OnEdge);
    CHECK(g.vertex(mid)->host == post);

    // УДАЛИТЬ ХОЗЯИНА НЕЛЬЗЯ, и это ОТДЕЛЬНАЯ проверка от предыдущей: там
    // держатели ссылались НА вершину, здесь вершина сидит НА элементе. Стороны
    // ссылки разные, и объединять их нельзя — объединённая проверка пропустила
    // бы одну из двух.
    const auto refused = g.remove_element(post);
    CHECK_FALSE(refused.ok);
    REQUIRE(refused.blockers.size() == 1);
    CHECK(refused.blockers.front() == mid);
}

TEST_CASE("вырожденный ввод отвергается на входе, а не в построителе меша") {
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    ElementId out = 0;

    // Поверхности нужны минимум две вершины, прямой — одна.
    CHECK_FALSE(g.add_element(ElementKind::Surface, {a}, "oak", out).ok);
    CHECK(g.add_element(ElementKind::Line, {a}, "oak", out).ok);

    // ОДНА И ТА ЖЕ ВЕРШИНА ДВАЖДЫ ПОДРЯД — нулевая длина у прямой, вырожденная
    // грань у поверхности. Пропусти это здесь — и дальше будет деление на ноль,
    // а виноватым будет выглядеть построитель меша.
    CHECK_FALSE(g.add_element(ElementKind::Line, {a, a}, "oak", out).ok);

    // Ссылка на несуществующую вершину.
    CHECK_FALSE(g.add_element(ElementKind::Line, {a, 999u}, "oak", out).ok);
}

TEST_CASE("постройка = компонента связности, и гиперребро связывает ВСЕХ разом") {
    HouseGraph g;
    // Две отдельные постройки: слева пара вершин, справа пятиугольник.
    const VertexId l1 = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId l2 = g.add_vertex(Anchoring::Free, {0.0f, 3.0f, 0.0f});
    ElementId post = 0;
    REQUIRE(g.add_element(ElementKind::Line, {l1, l2}, "oak", post).ok);

    std::vector<VertexId> ring;
    for (int i = 0; i < 5; ++i) {
        ring.push_back(g.add_vertex(Anchoring::OnGround,
                                    {20.0f + static_cast<float>(i), 0.0f, 0.0f}));
    }
    ElementId floor = 0;
    // ПЯТЬ ВЕРШИН ОДНИМ ЭЛЕМЕНТОМ. Если бы гиперребро раскладывалось в цепочку
    // пар, первая и пятая остались бы в разных компонентах при разрыве
    // середины — а они части одного пола и обязаны быть вместе.
    REQUIRE(g.add_element(ElementKind::Surface, ring, "oak", floor).ok);

    const auto comps = g.components();
    REQUIRE(comps.size() == 2);
    CHECK(g.component_of(l1) == g.component_of(l2));
    for (const VertexId v : ring) {
        CHECK(g.component_of(v) == g.component_of(ring.front()));
    }
    CHECK(g.component_of(l1) != g.component_of(ring.front()));

    // СВЯЗАЛИ ДВЕ — СТАЛА ОДНА. Прямое требование пользователя.
    ElementId bridge = 0;
    REQUIRE(g.add_element(ElementKind::Line, {l2, ring.front()}, "oak", bridge).ok);
    CHECK(g.components().size() == 1);
    CHECK(g.component_of(l1) == g.component_of(ring.back()));
}

TEST_CASE("мост назван ДО удаления, и гиперребро тоже бывает мостом") {
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 0.0f});
    const VertexId c = g.add_vertex(Anchoring::OnGround, {8.0f, 0.0f, 0.0f});
    ElementId ab = 0;
    ElementId bc = 0;
    REQUIRE(g.add_element(ElementKind::Line, {a, b}, "oak", ab).ok);
    REQUIRE(g.add_element(ElementKind::Line, {b, c}, "oak", bc).ok);

    // ЦЕПОЧКА: оба звена — мосты, и это надо знать ДО удаления. Узнать после —
    // значит узнать, когда чинить уже нечего.
    const auto br = g.bridges();
    CHECK(br.size() == 2);
    CHECK(std::find(br.begin(), br.end(), ab) != br.end());
    CHECK(std::find(br.begin(), br.end(), bc) != br.end());

    // ЗАМКНУЛИ КОЛЬЦО — МОСТОВ НЕ СТАЛО. Контроль обязателен: без него
    // утверждение прошло бы и на функции, которая зовёт мостом ВСЁ подряд.
    ElementId ca = 0;
    REQUIRE(g.add_element(ElementKind::Line, {c, a}, "oak", ca).ok);
    CHECK(g.bridges().empty());

    // ГИПЕРРЕБРО ТОЖЕ БЫВАЕТ МОСТОМ, и этот случай исправил МЕНЯ, а не код.
    // Я написал в заголовке, что гиперребро мостом быть не может — «рвём сразу
    // все N связей, надвое из этого не следует». Рукав показал обратное:
    // единственный пол на трёх якорях, будучи убран, рассыпает их на ТРИ
    // постройки. Это распад, и предупреждать о нём надо так же.
    //
    // Значит «мост» = «без него компонент станет БОЛЬШЕ», а не «ровно надвое».
    HouseGraph h;
    const VertexId p = h.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId q = h.add_vertex(Anchoring::OnGround, {1.0f, 0.0f, 0.0f});
    const VertexId r = h.add_vertex(Anchoring::OnGround, {1.0f, 0.0f, 1.0f});
    ElementId tri = 0;
    REQUIRE(h.add_element(ElementKind::Surface, {p, q, r}, "oak", tri).ok);
    CHECK(h.components().size() == 1);
    REQUIRE(h.bridges().size() == 1);
    CHECK(h.bridges().front() == tri);

    // КОНТРОЛЬ: добавили второй элемент, держащий те же три вершины, — ни один
    // из двух больше не мост, потому что второй удержит постройку.
    ElementId brace = 0;
    REQUIRE(h.add_element(ElementKind::Surface, {p, q, r}, "brace", brace).ok);
    CHECK(h.bridges().empty());
}

TEST_CASE("вершина на оси принадлежит постройке хозяина") {
    HouseGraph g;
    const VertexId low = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId high = g.add_vertex(Anchoring::Free, {0.0f, 3.0f, 0.0f});
    ElementId post = 0;
    REQUIRE(g.add_element(ElementKind::Line, {low, high}, "oak", post).ok);

    VertexId mid = 0;
    REQUIRE(g.add_vertex_on_edge(post, 0.5f, mid).ok);

    // Якорь, посаженный на столб и пока ничем не занятый, — НЕ отдельная
    // постройка. Он физически часть той же: он на ней сидит.
    CHECK(g.components().size() == 1);
    CHECK(g.component_of(mid) == g.component_of(low));
}

TEST_CASE("двинул вершину — поехало всё, что на ней висит") {
    HouseGraph g;
    const VertexId low = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId high = g.add_vertex(Anchoring::Free, {0.0f, 4.0f, 0.0f});
    ElementId post = 0;
    REQUIRE(g.add_element(ElementKind::Line, {low, high}, "oak", post).ok);

    VertexId mid = 0;
    REQUIRE(g.add_vertex_on_edge(post, 0.5f, mid).ok);
    // Середина столба — ровно посередине между концами.
    CHECK(g.resolved_local(mid).y == doctest::Approx(2.0f));

    // ДВИНУЛИ ВЕРХ — СЕРЕДИНА ПОЕХАЛА САМА. Это и есть «за якорем тянутся все
    // элементы»: геометрия НИГДЕ НЕ ХРАНИТСЯ, поэтому тянуть нечего — она
    // выводится заново. Хранили бы копию — здесь была бы работа и был бы шанс
    // её забыть.
    REQUIRE(g.move_vertex(high, {0.0f, 10.0f, 0.0f}).ok);
    CHECK(g.resolved_local(mid).y == doctest::Approx(5.0f));

    // И ВБОК тоже: параметр вдоль оси не меняется, а точка едет.
    REQUIRE(g.move_vertex(high, {6.0f, 10.0f, 0.0f}).ok);
    CHECK(g.resolved_local(mid).x == doctest::Approx(3.0f));
    CHECK(g.resolved_local(mid).y == doctest::Approx(5.0f));
}

TEST_CASE("вершину на оси нельзя двигать напрямую, и отказ говорит куда идти") {
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::Free, {0.0f, 4.0f, 0.0f});
    ElementId post = 0;
    REQUIRE(g.add_element(ElementKind::Line, {a, b}, "oak", post).ok);
    VertexId mid = 0;
    REQUIRE(g.add_vertex_on_edge(post, 0.25f, mid).ok);

    // Её положение принадлежит хозяину. МОЛЧА ПРОИГНОРИРОВАТЬ правку нельзя:
    // молча проигнорированная правка выглядит как сломанный инструмент, и
    // ровно этим нас сегодня били трижды.
    const auto refused = g.move_vertex(mid, {5.0f, 5.0f, 5.0f});
    CHECK_FALSE(refused.ok);
    CHECK_FALSE(refused.why.empty());
    REQUIRE(refused.blockers.size() == 1);
    CHECK(refused.blockers.front() == post); // и говорит, кто хозяин
    CHECK(g.resolved_local(mid).y == doctest::Approx(1.0f)); // не сдвинулась
}

TEST_CASE("цепочка вершин на осях разрешается насквозь, а цикл не вешает") {
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::Free, {0.0f, 8.0f, 0.0f});
    ElementId post = 0;
    REQUIRE(g.add_element(ElementKind::Line, {a, b}, "oak", post).ok);

    // Ярус: вершина на столбе, от неё балка, на балке ещё вершина.
    VertexId mid = 0;
    REQUIRE(g.add_vertex_on_edge(post, 0.5f, mid).ok);
    const VertexId far = g.add_vertex(Anchoring::Free, {6.0f, 4.0f, 0.0f});
    ElementId beam = 0;
    REQUIRE(g.add_element(ElementKind::Line, {mid, far}, "oak", beam).ok);
    VertexId on_beam = 0;
    REQUIRE(g.add_vertex_on_edge(beam, 0.5f, on_beam).ok);

    // ДВА УРОВНЯ СПУСКА: точка на балке зависит от вершины на столбе, а та — от
    // концов столба. Это его «на прямые по нашей заданной высоте поставили ещё
    // якоря, на них также поставили якоря и считай новый ярус делаем».
    CHECK(g.resolved_local(on_beam).x == doctest::Approx(3.0f));
    CHECK(g.resolved_local(on_beam).y == doctest::Approx(4.0f));

    // Двинули основание столба — поехал весь ярус.
    REQUIRE(g.move_vertex(b, {0.0f, 16.0f, 0.0f}).ok);
    CHECK(g.resolved_local(mid).y == doctest::Approx(8.0f));
    CHECK(g.resolved_local(on_beam).y == doctest::Approx(6.0f));
}

TEST_CASE("круговой прогон .dfh сходится побайтово") {
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 0.0f});
    const VertexId c = g.add_vertex(Anchoring::Free, {4.0f, 2.6f, 3.0f});
    const VertexId d = g.add_vertex(Anchoring::Free, {0.0f, 2.6f, 3.0f});
    ElementId post = 0;
    ElementId floor = 0;
    REQUIRE(g.add_element(ElementKind::Line, {a, c}, "oak", post).ok);
    REQUIRE(g.add_element(ElementKind::Surface, {a, b, c, d}, "frame-oak", floor).ok);
    REQUIRE(g.set_facing(floor, true).ok);
    VertexId mid = 0;
    REQUIRE(g.add_vertex_on_edge(post, 0.25f, mid).ok);

    const std::string text = dfn::world::write_house(g);

    HouseGraph back;
    const auto r = dfn::world::read_house(text, back);
    REQUIRE_MESSAGE(r.ok, r.why);

    // ПОБАЙТОВО, а не «похоже». Прогон, сходящийся приблизительно, не ловит
    // потерю четвёртого знака — а именно она и превратит дом в дом с щелью
    // через десять правок.
    CHECK(dfn::world::write_house(back) == text);

    // И содержательно: вершина на оси доехала вместе с хозяином.
    CHECK(back.vertex_count() == g.vertex_count());
    CHECK(back.element_count() == g.element_count());
}

TEST_CASE("вершина на оси читается, даже если хозяин объявлен НИЖЕ") {
    // Порядок строк в файле не должен быть значимым: иначе файл нельзя
    // отсортировать, слить или дописать руками.
    const std::string text =
        "# dfh 1\n"
        "vertex v1 ground 0.0000 0.0000 0.0000\n"
        "vertex v3 on_edge e1 0.5000\n"
        "vertex v2 free 0.0000 4.0000 0.0000\n"
        "line e1 v1 v2 style=oak\n";
    HouseGraph g;
    const auto r = dfn::world::read_house(text, g);
    REQUIRE_MESSAGE(r.ok, r.why);
    CHECK(g.vertex_count() == 3);
    CHECK(g.element_count() == 1);
}

TEST_CASE("битый файл ОТВЕРГАЕТСЯ с номером строки, а не чинится молча") {
    HouseGraph g;

    // Ссылка на несуществующую вершину. Молча подставленная замена превратила
    // бы ошибку записи в дом со сдвинутой стеной, который никто не свяжет с
    // причиной — этот урок в репозитории уже оплачен форматом мира.
    const auto bad_ref = dfn::world::read_house(
        "vertex v1 ground 0.0000 0.0000 0.0000\n"
        "line e1 v1 v9 style=oak\n", g);
    CHECK_FALSE(bad_ref.ok);
    CHECK(bad_ref.line == 2);
    CHECK_FALSE(bad_ref.why.empty());

    // Хозяин, которого нет.
    const auto bad_host = dfn::world::read_house(
        "vertex v1 on_edge e7 0.5000\n", g);
    CHECK_FALSE(bad_host.ok);
    CHECK(bad_host.line == 1);

    // Две вершины с одним именем: молча выиграла бы последняя, и ссылки выше
    // стали бы значить не то, что написано.
    const auto dup = dfn::world::read_house(
        "vertex v1 ground 0.0000 0.0000 0.0000\n"
        "vertex v1 free 1.0000 1.0000 1.0000\n", g);
    CHECK_FALSE(dup.ok);
    CHECK(dup.line == 2);

    // Неизвестное слово: формат обязан быть закрытым, иначе опечатка в имени
    // вида молча выкинет целый элемент.
    const auto junk = dfn::world::read_house("wall e1 v1 v2\n", g);
    CHECK_FALSE(junk.ok);
    CHECK(junk.line == 1);
}

TEST_CASE("цикл ОТВЕРГАЕТСЯ, и отказ отличает круг от ссылки в пустоту") {
    // ЗДЕСЬ У ЗАЩИТЫ ПОЯВИЛСЯ ПРИБОР, и он оказался строже задуманного. Через
    // API графа цикл не построить — ссылки элемента задаются при создании.
    // Текстом можно, и читатель обязан такой файл ОТВЕРГНУТЬ, а не читать
    // наполовину: дом с половиной вершин — это не «почти дом».
    const std::string cyclic =
        "# dfh 1\n"
        "vertex v1 on_edge e2 0.5000\n"
        "vertex v2 free 0.0000 4.0000 0.0000\n"
        "vertex v3 on_edge e1 0.5000\n"
        "vertex v4 free 4.0000 0.0000 0.0000\n"
        "line e1 v1 v2 style=oak\n"
        "line e2 v3 v4 style=oak\n";
    HouseGraph g;
    const auto r = dfn::world::read_house(cyclic, g);
    CHECK_FALSE(r.ok);
    CHECK(r.why.find("руг") != std::string::npos); // назван именно КРУГ

    // КОНТРОЛЬ, без которого утверждение выше не стоит ничего: тот же файл без
    // круга читается. Иначе «отвергает цикл» прошло бы и на читателе, который
    // отвергает вообще всё.
    const std::string sane =
        "# dfh 1\n"
        "vertex v1 ground 0.0000 0.0000 0.0000\n"
        "vertex v2 free 0.0000 4.0000 0.0000\n"
        "vertex v3 on_edge e1 0.5000\n"
        "vertex v4 free 4.0000 0.0000 0.0000\n"
        "line e1 v1 v2 style=oak\n"
        "line e2 v3 v4 style=oak\n";
    HouseGraph h;
    const auto ok = dfn::world::read_house(sane, h);
    REQUIRE_MESSAGE(ok.ok, ok.why);
    CHECK(h.vertex_count() == 4);
    CHECK(h.element_count() == 2);
}

TEST_CASE("на вершину, сидящую на оси, МОЖНО сослаться элементом") {
    // ГЛАВНЫЙ СЦЕНАРИЙ ПОЛЬЗОВАТЕЛЯ: «на прямые по нашей заданной высоте
    // поставили ещё якоря, на них также поставили якоря и считай новый ярус
    // делаем». Первая версия читателя это ЛОМАЛА: он исполнял элементы сразу, а
    // вершины на осях откладывал, поэтому балка ссылалась на ещё не созданную
    // вершину и файл отвергался. Нашлось рукавом, написанным для другого.
    const std::string text =
        "# dfh 1\n"
        "vertex v1 ground 0.0000 0.0000 0.0000\n"
        "vertex v2 free 0.0000 8.0000 0.0000\n"
        "line e1 v1 v2 style=oak\n"
        "vertex v3 on_edge e1 0.5000\n"
        "vertex v4 free 6.0000 4.0000 0.0000\n"
        "line e2 v3 v4 style=oak\n"
        "vertex v5 on_edge e2 0.5000\n";
    HouseGraph g;
    const auto r = dfn::world::read_house(text, g);
    REQUIRE_MESSAGE(r.ok, r.why);
    CHECK(g.vertex_count() == 5);
    CHECK(g.element_count() == 2);
    // Второй ярус разрешается насквозь: v5 на балке, балка от вершины на столбе.
    CHECK(g.components().size() == 1);
}

TEST_CASE("числа и замкнутость переживают круговой прогон") {
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 0.0f});
    const VertexId c = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 3.0f});
    ElementId post = 0;
    ElementId floor = 0;
    REQUIRE(g.add_element(ElementKind::Line, {a, b}, "oak", post).ok);
    REQUIRE(g.add_element(ElementKind::Surface, {a, b, c}, "planks", floor).ok);

    REQUIRE(g.set_param(post, "form", "square").ok);
    REQUIRE(g.set_param(post, "radius", "0.15").ok);
    REQUIRE(g.set_param(floor, "thickness", "0.25").ok);
    REQUIRE(g.set_param(floor, "tex_deg", "45").ok);
    REQUIRE(g.set_closed(floor, true).ok);

    const std::string text = dfn::world::write_house(g);
    HouseGraph back;
    const auto r = dfn::world::read_house(text, back);
    REQUIRE_MESSAGE(r.ok, r.why);

    // ПОБАЙТОВО. Числа, потерянные при записи, обнаружатся не сразу: дом
    // прочитается, построится и будет выглядеть почти так же — с квадратным
    // столбом, ставшим круглым.
    CHECK(dfn::world::write_house(back) == text);
    CHECK(back.param(post, "form") == "square");
    CHECK(back.param(post, "radius") == "0.15");
    CHECK(back.element(floor)->closed);
    CHECK_FALSE(back.element(post)->closed);

    // ЗАМКНУТОСТЬ — ЖЕСТ, А НЕ СЛЕДСТВИЕ ЧИСЕЛ. Раньше построитель угадывал её
    // по «высота больше нуля»; угадывание ломается молча на плоском поле с
    // заданной высотой или на стене нулевой высоты.
    CHECK(back.param(floor, "thickness") == "0.25");
}

TEST_CASE("параметр отличается от ссылки на вершину по знаку равенства") {
    // Имена вершин задаём мы сами, и знака равенства в них нет по построению.
    // Правило простое нарочно: разбирать иначе значило бы вести список
    // известных параметров в двух местах — в читателе и в построителе меша.
    const std::string text =
        "# dfh 1\n"
        "vertex v1 ground 0.0000 0.0000 0.0000\n"
        "vertex v2 ground 4.0000 0.0000 0.0000\n"
        "line e1 v1 v2 style=oak form=round radius=0.12 unknown_key=42\n";
    HouseGraph g;
    const auto r = dfn::world::read_house(text, g);
    REQUIRE_MESSAGE(r.ok, r.why);
    REQUIRE(g.element_count() == 1);
    CHECK(g.element(1)->refs.size() == 2);
    // Незнакомый параметр НЕ ОТВЕРГАЕТСЯ: модель их не толкует, толкует
    // построитель. Отвергать здесь значило бы держать список параметров в
    // модели, которой они безразличны.
    CHECK(g.param(1, "unknown_key") == "42");
}

namespace {

/// Сарайчик: четыре угла, каркас по периметру, пол. Годен к регистрации.
struct Shed {
    HouseGraph g;
    VertexId a, b, c, d;
    Shed() {
        a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
        b = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 0.0f});
        c = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 3.0f});
        d = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 3.0f});
        ElementId tmp = 0;
        g.add_element(ElementKind::Line, {a, b}, "oak", tmp);
        g.add_element(ElementKind::Line, {b, c}, "oak", tmp);
        g.add_element(ElementKind::Line, {c, d}, "oak", tmp);
        g.add_element(ElementKind::Line, {d, a}, "oak", tmp);
        ElementId floor = 0;
        g.add_element(ElementKind::Surface, {a, b, c, d}, "planks", floor);
        g.set_closed(floor, true);
    }
};

} // namespace

TEST_CASE("регистрация строже правки: поверхность обязана опираться на каркас") {
    Shed s;
    // Полный сарай проходит.
    CHECK(dfn::world::check_registrable(s.g).empty());

    // УБИРАЕМ СТОРОНУ c—d, ТРЕТЬЮ ПО СЧЁТУ, И ЭТО НЕ КАПРИЗ.
    //
    // Сначала я убирал ПЕРВУЮ сторону — и контрфакт «проверять только одну пару
    // вместо всех» остался ЗЕЛЁНЫМ: сломанная проверка смотрит как раз первую
    // пару и находит ровно ту дыру, которую я проделал. Проверка, которая
    // ловит только первый случай, доказывает не «все стороны проверены», а
    // «первая проверена», и разница видна только на контрфакте.
    //
    // Средняя сторона ловится ТОЛЬКО обходом всех пар.
    for (const dfn::world::ElementId id : s.g.incident(s.c)) {
        const auto* e = s.g.element(id);
        if (e->kind == ElementKind::Line && e->refs.size() == 2
            && ((e->refs[0] == s.c && e->refs[1] == s.d)
                || (e->refs[0] == s.d && e->refs[1] == s.c))) {
            REQUIRE(s.g.remove_element(id).ok);
            break;
        }
    }
    const auto found = dfn::world::check_registrable(s.g);
    REQUIRE_FALSE(found.empty());
    CHECK(found.front().why.find("каркас") != std::string::npos);

    // И ЗАМЫКАЮЩАЯ СТОРОНА d—a тоже, отдельным заходом: у замкнутого контура
    // последняя пара считается через возврат к началу, и её легко потерять
    // ровно на этом.
    Shed t;
    for (const dfn::world::ElementId id : t.g.incident(t.d)) {
        const auto* e = t.g.element(id);
        if (e->kind == ElementKind::Line && e->refs.size() == 2
            && ((e->refs[0] == t.d && e->refs[1] == t.a)
                || (e->refs[0] == t.a && e->refs[1] == t.d))) {
            REQUIRE(t.g.remove_element(id).ok);
            break;
        }
    }
    CHECK_FALSE(dfn::world::check_registrable(t.g).empty());
}

TEST_CASE("недостроенное состояние — норма при правке и НЕ норма при регистрации") {
    HouseGraph g;
    const VertexId lone = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    (void)lone;
    // Пустая постройка: очевидно нельзя, и именно поэтому легко забыть — пустой
    // тип поставился бы без единой жалобы и был бы невидим.
    const auto empty = dfn::world::check_registrable(g);
    REQUIRE_FALSE(empty.empty());
    CHECK(empty.front().why.find("ни одного элемента") != std::string::npos);

    // Вершина, ни к чему не подключённая: при стройке это обычный ход работы
    // (поставил якоря, ещё не соединил), при регистрации — мусор в библиотеке.
    Shed s;
    const VertexId orphan = s.g.add_vertex(Anchoring::OnGround, {50.0f, 0.0f, 0.0f});
    const auto found = dfn::world::check_registrable(s.g);
    const bool named = std::any_of(found.begin(), found.end(),
                                   [orphan](const dfn::world::RegisterFinding& f) {
                                       return f.vertex == orphan;
                                   });
    CHECK(named);
}

TEST_CASE("вырезанная постройка — самостоятельный граф со своими именами") {
    // Две постройки в одной карте.
    Shed s;
    HouseGraph& g = s.g;
    const VertexId far1 = g.add_vertex(Anchoring::OnGround, {100.0f, 0.0f, 0.0f});
    const VertexId far2 = g.add_vertex(Anchoring::Free, {100.0f, 3.0f, 0.0f});
    ElementId post = 0;
    REQUIRE(g.add_element(ElementKind::Line, {far1, far2}, "oak", post).ok);
    REQUIRE(g.components().size() == 2);

    HouseGraph type;
    REQUIRE(dfn::world::extract_component(g, s.a, type));
    // Взялась ТОЛЬКО одна постройка: сарай, а не всё подряд.
    CHECK(type.components().size() == 1);
    CHECK(type.vertex_count() == 4);
    CHECK(type.element_count() == 5);
    // И она сама по себе годна к регистрации.
    CHECK(dfn::world::check_registrable(type).empty());

    // Исходная карта НЕ ТРОНУТА: регистрация — это копия, а не переезд
    // (решение пользователя «давай как копию»).
    CHECK(g.components().size() == 2);
    CHECK(g.vertex_count() == 6);
}

TEST_CASE("вырезание переносит вершины на осях вместе с хозяином") {
    HouseGraph g;
    const VertexId low = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId high = g.add_vertex(Anchoring::Free, {0.0f, 6.0f, 0.0f});
    ElementId post = 0;
    REQUIRE(g.add_element(ElementKind::Line, {low, high}, "oak", post).ok);
    VertexId mid = 0;
    REQUIRE(g.add_vertex_on_edge(post, 0.5f, mid).ok);
    const VertexId far = g.add_vertex(Anchoring::Free, {5.0f, 3.0f, 0.0f});
    ElementId beam = 0;
    REQUIRE(g.add_element(ElementKind::Line, {mid, far}, "oak", beam).ok);

    HouseGraph type;
    REQUIRE(dfn::world::extract_component(g, low, type));
    // ЯРУС ДОЕХАЛ ЦЕЛИКОМ: вершина на оси не может быть создана раньше хозяина,
    // а хозяин ссылается на вершины — то же исполнение до неподвижной точки, что
    // и в читателе файла, и по той же причине.
    CHECK(type.vertex_count() == 4);
    CHECK(type.element_count() == 2);
    CHECK(type.resolved_local(3).y == doctest::Approx(3.0f));
}

TEST_CASE("запись переживает удаления: имена не выводятся из счётчика") {
    // ПОЙМАНО РУКАВОМ ИНСТРУМЕНТОВ, А НЕ МНОЙ. Писатель перебирал имена
    // 1..vertex_count+element_count+1 — а имена при удалении НЕ
    // переиспользуются, поэтому у выживших они больше их числа. Завёл пять,
    // удалил четыре — уцелевшая зовётся v5, перебор доходил до v1, снимок
    // выходил ПУСТЫМ, читатель на пустой файл не жалуется, и cmd+Z СТИРАЛ
    // постройку.
    HouseGraph g;
    std::vector<VertexId> all;
    for (int i = 0; i < 5; ++i) {
        all.push_back(g.add_vertex(Anchoring::OnGround,
                                   {static_cast<float>(i), 0.0f, 0.0f}));
    }
    for (int i = 0; i < 4; ++i) {
        REQUIRE(g.remove_vertex(all[static_cast<std::size_t>(i)]).ok);
    }
    REQUIRE(g.vertex_count() == 1);

    const std::string text = dfn::world::write_house(g);
    HouseGraph back;
    REQUIRE(dfn::world::read_house(text, back).ok);
    // ВЫЖИВШАЯ ВЕРШИНА НА МЕСТЕ И ПОД СВОИМ ИМЕНЕМ. Без этого утверждения пустой
    // снимок выглядел бы как успешный круговой прогон: пустое пишется и
    // читается прекрасно.
    //
    // ЭТОТ СЛУЧАЙ Я САМ НАПИСАЛ НЕВЕРНО, и это стоит записать. Он проверял
    // vertex(1) — то есть кодировал ТОГДАШНЕЕ поведение, при котором читатель
    // раздавал имена заново. Поведение было ошибочным, а случай его закреплял.
    // Проверка, написанная под наблюдаемое, а не под нужное, охраняет дефект.
    REQUIRE(back.vertex_count() == 1);
    REQUIRE(back.vertex(all.back()) != nullptr);
    CHECK(back.vertex(all.back())->local.x == doctest::Approx(4.0f));

    // И то же для элементов: удалили ранние, уцелел поздний.
    HouseGraph h;
    const VertexId a = h.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = h.add_vertex(Anchoring::OnGround, {1.0f, 0.0f, 0.0f});
    ElementId e1 = 0;
    ElementId e2 = 0;
    ElementId e3 = 0;
    REQUIRE(h.add_element(ElementKind::Line, {a, b}, "oak", e1).ok);
    REQUIRE(h.add_element(ElementKind::Line, {a, b}, "pine", e2).ok);
    REQUIRE(h.add_element(ElementKind::Line, {a, b}, "stone", e3).ok);
    REQUIRE(h.remove_element(e1).ok);
    REQUIRE(h.remove_element(e2).ok);

    HouseGraph hb;
    REQUIRE(dfn::world::read_house(dfn::world::write_house(h), hb).ok);
    CHECK(hb.element_count() == 1);
}

TEST_CASE("ЗАПИСЬ ПОСЛЕ УДАЛЕНИЙ: имя больше числа живых — и всё равно в файле") {
    // НАЙДЕНО ЗОНОЙ ИНСТРУМЕНТОВ ПОСТРОЙКИ, ЗАМЕРОМ (18.08.2026). write_house
    // перебирал возможные имена от 1 до vertex_count() + element_count() + 1,
    // то есть угадывал наибольшее ВЫДАННОЕ имя по числу ЖИВЫХ. Имена не
    // переиспользуются, поэтому после удалений выжившие сидят ВЫШЕ этой
    // границы и не попадали в файл вовсе.
    //
    // ЦЕНА, И ОНА ВСЯ В СЛОВЕ «МОЛЧА»: отмена хранит состояние ЭТИМ текстом
    // (EditorHistory через snapshot()), read_house на «# dfh 1\n» не жалуется —
    // файл формально безупречен, в нём просто ничего нет, — и cmd+Z после
    // нескольких удалений возвращал ПУСТУЮ постройку без единого отказа.
    //
    // КОНТРОЛЬ ЗДЕСЬ — ВТОРАЯ ПОЛОВИНА СЛУЧАЯ, А НЕ ОТДЕЛЬНЫЙ ТЕСТ: тот же
    // граф ДО удалений проходил и на сломанном коде, поэтому проверка на нём
    // не различала бы ничего (правило 30).
    using namespace dfn::world;
    HouseGraph g;
    std::vector<VertexId> made;
    for (int i = 0; i < 5; ++i) {
        made.push_back(g.add_vertex(Anchoring::OnGround,
                                    {static_cast<float>(i), 0.0f, 0.0f}));
    }
    // Контроль: пока ничего не удалено, старый перебор был прав, и новый обязан
    // дать то же самое.
    {
        HouseGraph back;
        REQUIRE(read_house(write_house(g), back).ok);
        CHECK(back.vertex_count() == 5u);
    }
    for (int i = 0; i < 4; ++i) {
        REQUIRE(g.remove_vertex(made[static_cast<std::size_t>(i)]).ok);
    }
    REQUIRE(g.vertex_count() == 1u);
    const VertexId survivor = made.back();
    REQUIRE(survivor > static_cast<VertexId>(g.vertex_count() + g.element_count() + 1));

    const std::string text = write_house(g);
    INFO("снимок: ", text);
    // Имя выжившей ОБЯЗАНО быть в файле. Именно эта строка и пропадала:
    // снимок был «# dfh 1\n» целиком.
    CHECK(text.find("vertex v" + std::to_string(survivor) + ' ') != std::string::npos);

    HouseGraph back;
    const auto r = read_house(text, back);
    REQUIRE_MESSAGE(r.ok, r.why);
    REQUIRE(back.vertex_count() == 1u);
    // Содержательно: доехала именно она, со своим местом.
    REQUIRE(back.vertices().size() == 1u);
    CHECK(back.vertices()[0].local.x == doctest::Approx(4.0f));

    // ИМЕНА ПРИ ЧТЕНИИ ВЫДАЮТСЯ ЗАНОВО (имя в файле — подпись, а не номер), так
    // что побайтово сходится ВТОРОЙ прогон, а не первый. Проверяется именно он:
    // требовать равенства с `text` значило бы требовать, чтобы чтение хранило
    // дыры в нумерации, чего оно не делает и делать не обязано.
    const std::string text2 = write_house(back);
    HouseGraph back2;
    REQUIRE(read_house(text2, back2).ok);
    CHECK(write_house(back2) == text2);
}

TEST_CASE("круговой прогон СОХРАНЯЕТ ИМЕНА, иначе отмена показывает соседа") {
    // ПОЛОМКА БЫЛА НЕ В ФАЙЛЕ, А В ОТМЕНЕ. Читатель раздавал имена заново, и
    // тот же дом возвращался с другими именами: v3 v4 v5 становились v1 v2 v3.
    // Для файла это безобидно — геометрия та же. Но история хранит состояние
    // ЭТИМ ЖЕ текстом, и после cmd+Z выделение, взятое по имени, указывало на
    // ДРУГУЮ вершину: не повисало, а молча показывало соседа. Такой отказ
    // человек замечает через десять правок и не связывает с отменой.
    HouseGraph g;
    std::vector<VertexId> made;
    for (int i = 0; i < 5; ++i) {
        made.push_back(g.add_vertex(Anchoring::OnGround,
                                    {static_cast<float>(i), 0.0f, 0.0f}));
    }
    REQUIRE(g.remove_vertex(made[0]).ok);
    REQUIRE(g.remove_vertex(made[1]).ok);

    HouseGraph back;
    REQUIRE(dfn::world::read_house(dfn::world::write_house(g), back).ok);

    // ИМЯ В ИМЯ, а не «столько же вершин». Проверка на количество прошла бы и
    // на переименовании — именно так эта поломка и дожила до пользователя.
    REQUIRE(back.vertex_count() == 3);
    for (std::size_t i = 2; i < made.size(); ++i) {
        const dfn::world::Vertex* v = back.vertex(made[i]);
        REQUIRE_MESSAGE(v != nullptr, "имя не пережило круговой прогон");
        CHECK(v->local.x == doctest::Approx(static_cast<float>(i)));
    }

    // И СЛЕДУЮЩАЯ ДОБАВЛЕННАЯ НЕ ЗАБИРАЕТ ЧУЖОЕ ИМЯ: счётчик двигается за
    // самым большим прочитанным, иначе новая вершина столкнулась бы с уже
    // существующей — и это была бы та же беда с другого конца.
    const VertexId fresh = back.add_vertex(Anchoring::Free, {9.0f, 9.0f, 9.0f});
    CHECK(fresh > made.back());
    CHECK(back.vertex_count() == 4);

    // То же для элементов.
    HouseGraph h;
    const VertexId a = h.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = h.add_vertex(Anchoring::OnGround, {1.0f, 0.0f, 0.0f});
    ElementId e1 = 0;
    ElementId e2 = 0;
    ElementId e3 = 0;
    REQUIRE(h.add_element(ElementKind::Line, {a, b}, "oak", e1).ok);
    REQUIRE(h.add_element(ElementKind::Line, {a, b}, "pine", e2).ok);
    REQUIRE(h.add_element(ElementKind::Line, {a, b}, "stone", e3).ok);
    REQUIRE(h.remove_element(e1).ok);
    HouseGraph hb;
    REQUIRE(dfn::world::read_house(dfn::world::write_house(h), hb).ok);
    REQUIRE(hb.element(e3) != nullptr);
    CHECK(hb.element(e3)->style == "stone");
    CHECK(hb.element(e1) == nullptr);
}

TEST_CASE("версия растёт ТОЛЬКО от удавшегося изменения") {
    // Аудит 20.08, находка 1: бамп до валидации заставлял отклонённый кадр
    // перетаскивания пересобирать меш и физическое тело впустую.
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::Free, {0.0f, 2.0f, 0.0f});
    ElementId line = 0;
    REQUIRE(g.add_element(ElementKind::Line, {a, b}, "frame", line).ok);

    // КОНТРОЛЬ: удавшаяся правка версию растит.
    const std::uint32_t before = g.version();
    REQUIRE(g.move_vertex(b, {0.0f, 3.0f, 0.0f}).ok);
    CHECK(g.version() == before + 1);

    // Отказ — не растит: вершины нет.
    const std::uint32_t v1 = g.version();
    CHECK_FALSE(g.move_vertex(9999, {0.0f, 0.0f, 0.0f}).ok);
    CHECK(g.version() == v1);
    // Отказ — не растит: элемент держит вершину.
    CHECK_FALSE(g.remove_vertex(a).ok);
    CHECK(g.version() == v1);
    // То же значение параметра — не изменение.
    REQUIRE(g.set_param(line, "radius", "0.25").ok);
    const std::uint32_t v2 = g.version();
    REQUIRE(g.set_param(line, "radius", "0.25").ok);
    CHECK(g.version() == v2);
    // Другое значение — изменение (второе контрольное плечо).
    REQUIRE(g.set_param(line, "radius", "0.3").ok);
    CHECK(g.version() == v2 + 1);
}

TEST_CASE("вершина на оси СКОЛЬЗИТ параметром, а не двигается точкой") {
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::Free, {0.0f, 4.0f, 0.0f});
    ElementId line = 0;
    REQUIRE(g.add_element(ElementKind::Line, {a, b}, "frame", line).ok);
    VertexId rider = dfn::world::NO_VERTEX;
    REQUIRE(g.add_vertex_on_edge(line, 0.25f, rider).ok);
    CHECK(g.resolved_local(rider).y == doctest::Approx(1.0f));

    // move_vertex ей отказывает (место принадлежит оси) — и версия не растёт.
    const std::uint32_t v0 = g.version();
    CHECK_FALSE(g.move_vertex(rider, {0.0f, 2.0f, 0.0f}).ok);
    CHECK(g.version() == v0);

    // slide_vertex — единственная дверь: параметр меняется, точка выводится.
    REQUIRE(g.slide_vertex(rider, 0.75f).ok);
    CHECK(g.version() == v0 + 1);
    CHECK(g.resolved_local(rider).y == doctest::Approx(3.0f));
    // Тот же t — не изменение.
    REQUIRE(g.slide_vertex(rider, 0.75f).ok);
    CHECK(g.version() == v0 + 1);
    // За края не выехать: зажим в [0,1].
    REQUIRE(g.slide_vertex(rider, 7.0f).ok);
    CHECK(g.resolved_local(rider).y == doctest::Approx(4.0f));
    // КОНТРОЛЬ: свободной вершине slide_vertex отказывает.
    CHECK_FALSE(g.slide_vertex(b, 0.5f).ok);
}
