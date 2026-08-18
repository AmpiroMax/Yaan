/*
Created: 18:08:2026 - 16:48:18
Last updated: 18:08:2026 - 17:06:23
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
*/

#include <doctest/doctest.h>

#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseGraph.h"

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
