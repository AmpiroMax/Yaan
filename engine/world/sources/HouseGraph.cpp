/*
Created: 18:08:2026 - 16:47:20
Last updated: 18:08:2026 - 16:47:20
Module: engine/world
File: engine/world/sources/HouseGraph.cpp

Responsibility:
- Реализация гиперграфа постройки. Устройство — в docs/DESIGN_HOUSE_GRAPH.md.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Индекс инцидентности здесь СЧИТАЕТСЯ ПЕРЕБОРОМ, а не хранится. Это осознанно
  для первого среза: постройка — сотни элементов, перебор дешевле кэша, который
  надо не забыть обновить. Когда счёт пойдёт на тысячи, кэш заводится ОДНИМ
  местом и с рукавом, который ловит его расхождение с перебором.
*/
/*
UPD:
- 18:08:2026 - 16:47:20: Создан вместе с заголовком.
*/

#include "engine/world/sources/HouseGraph.h"

#include <algorithm>

namespace dfn::world {

VertexId HouseGraph::add_vertex(Anchoring anchoring, glm::vec3 local) {
    Vertex v;
    v.id = next_vertex_++;
    v.anchoring = anchoring;
    v.local = local;
    vertices_.push_back(v);
    return v.id;
}

GraphResult HouseGraph::add_vertex_on_edge(ElementId host, float t, VertexId& out) {
    out = NO_VERTEX;
    const Element* e = element(host);
    if (e == nullptr) {
        return {false, "хозяина нет", {}};
    }
    // ТОЛЬКО НА ПРЯМУЮ, и это не произвол. Вершина на оси задана ОДНИМ числом
    // вдоль неё; у поверхности такого числа нет — там их два, и «параметр t»
    // означал бы разное для контура и для цепочки.
    if (e->kind != ElementKind::Line) {
        return {false, "вершина сидит только на оси прямой", {}};
    }
    Vertex v;
    v.id = next_vertex_++;
    v.anchoring = Anchoring::OnEdge;
    v.host = host;
    v.host_t = std::clamp(t, 0.0f, 1.0f);
    vertices_.push_back(v);
    out = v.id;
    return {};
}

GraphResult HouseGraph::add_element(ElementKind kind, std::vector<VertexId> refs,
                                    std::string style, ElementId& out) {
    out = NO_ELEMENT;
    if (refs.size() < min_refs_for(kind)) {
        return {false, "слишком мало вершин для этого вида", {}};
    }
    for (const VertexId id : refs) {
        if (vertex(id) == nullptr) {
            return {false, "элемент ссылается на несуществующую вершину", {}};
        }
    }
    // ДВЕ ОДИНАКОВЫЕ ВЕРШИНЫ ПОДРЯД — это не элемент, а ошибка ввода: у прямой
    // нулевая длина, у поверхности вырожденная грань. Ловится здесь, потому что
    // дальше оно превратится в геометрию с делением на ноль, и виноватым будет
    // выглядеть построитель меша.
    for (std::size_t i = 1; i < refs.size(); ++i) {
        if (refs[i] == refs[i - 1]) {
            return {false, "одна и та же вершина дважды подряд", {}};
        }
    }
    Element e;
    e.id = next_element_++;
    e.kind = kind;
    e.refs = std::move(refs);
    e.style = std::move(style);
    elements_.push_back(std::move(e));
    out = elements_.back().id;
    return {};
}

GraphResult HouseGraph::remove_vertex(VertexId id) {
    if (vertex(id) == nullptr) {
        return {false, "такой вершины нет", {}};
    }
    // ОТКАЗ СО СПИСКОМ ДЕРЖАТЕЛЕЙ. Решение пользователя 18.08: «удалять бревно
    // нельзя давать, пока к нему что-то привязано». Список, а не флаг: голый
    // отказ на пятом этаже превращается в поиск виноватого руками.
    //
    // Сюда же попадают вершины, СИДЯЩИЕ НА ОСИ удаляемого... нет, наоборот:
    // здесь ловятся элементы, ссылающиеся на вершину. Вершины на оси ловит
    // remove_element, и это разные проверки — не объединять.
    std::vector<ElementId> held = incident(id);
    if (!held.empty()) {
        return {false, "на вершине висят элементы", std::move(held)};
    }
    vertices_.erase(std::remove_if(vertices_.begin(), vertices_.end(),
                                   [id](const Vertex& v) { return v.id == id; }),
                    vertices_.end());
    return {};
}

GraphResult HouseGraph::remove_element(ElementId id) {
    if (element(id) == nullptr) {
        return {false, "такого элемента нет", {}};
    }
    // НА ОСИ УДАЛЯЕМОГО МОГУТ СИДЕТЬ ВЕРШИНЫ, и они осиротеют молча: их
    // положение выводится из хозяина, а хозяина не станет. Это тот же отказ со
    // списком, только держатели — вершины, а не элементы. Отдельная проверка, а
    // не расширение прежней: у них разные стороны ссылки.
    std::vector<ElementId> riders;
    for (const Vertex& v : vertices_) {
        if (v.anchoring == Anchoring::OnEdge && v.host == id) {
            riders.push_back(v.id);
        }
    }
    if (!riders.empty()) {
        return {false, "на оси элемента сидят вершины", std::move(riders)};
    }
    elements_.erase(std::remove_if(elements_.begin(), elements_.end(),
                                   [id](const Element& e) { return e.id == id; }),
                    elements_.end());
    return {};
}

const Vertex* HouseGraph::vertex(VertexId id) const {
    const auto it = std::find_if(vertices_.begin(), vertices_.end(),
                                 [id](const Vertex& v) { return v.id == id; });
    return it == vertices_.end() ? nullptr : &*it;
}

const Element* HouseGraph::element(ElementId id) const {
    const auto it = std::find_if(elements_.begin(), elements_.end(),
                                 [id](const Element& e) { return e.id == id; });
    return it == elements_.end() ? nullptr : &*it;
}

std::vector<ElementId> HouseGraph::incident(VertexId id) const {
    std::vector<ElementId> out;
    for (const Element& e : elements_) {
        if (std::find(e.refs.begin(), e.refs.end(), id) != e.refs.end()) {
            out.push_back(e.id);
        }
    }
    return out;
}

Vertex* HouseGraph::find_vertex(VertexId id) {
    const auto it = std::find_if(vertices_.begin(), vertices_.end(),
                                 [id](const Vertex& v) { return v.id == id; });
    return it == vertices_.end() ? nullptr : &*it;
}

Element* HouseGraph::find_element(ElementId id) {
    const auto it = std::find_if(elements_.begin(), elements_.end(),
                                 [id](const Element& e) { return e.id == id; });
    return it == elements_.end() ? nullptr : &*it;
}

} // namespace dfn::world
