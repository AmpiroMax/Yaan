/*
Created: 18:08:2026 - 16:47:20
Last updated: 18:08:2026 - 16:59:04
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
- 18:08:2026 - 16:59:04: components() и bridges() (см. заголовок).
*/

#include "engine/world/sources/HouseGraph.h"

#include <algorithm>
#include <unordered_map>

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

namespace {

/// Система непересекающихся множеств со сжатием путей. Без ранга: постройки —
/// сотни вершин, и сжатия достаточно; ранг добавил бы код, которого никто не
/// сможет отличить по времени работы.
class DisjointSets {
public:
    explicit DisjointSets(std::size_t n) : parent_(n) {
        for (std::size_t i = 0; i < n; ++i) {
            parent_[i] = i;
        }
    }
    std::size_t find(std::size_t i) {
        while (parent_[i] != i) {
            parent_[i] = parent_[parent_[i]];
            i = parent_[i];
        }
        return i;
    }
    void unite(std::size_t a, std::size_t b) { parent_[find(a)] = find(b); }

private:
    std::vector<std::size_t> parent_;
};

} // namespace

std::vector<std::vector<VertexId>> HouseGraph::components() const {
    // Индекс вершины в массиве — рабочее имя внутри этой функции. Наружу
    // выходят настоящие VertexId: индексы съезжают при удалении, а имена нет.
    std::unordered_map<VertexId, std::size_t> slot;
    slot.reserve(vertices_.size());
    for (std::size_t i = 0; i < vertices_.size(); ++i) {
        slot[vertices_[i].id] = i;
    }

    DisjointSets sets(vertices_.size());
    for (const Element& e : elements_) {
        // ГИПЕРРЕБРО СВЯЗЫВАЕТ ВСЕ СВОИ ВЕРШИНЫ РАЗОМ, а не попарно по цепочке.
        // Пол на пяти вершинах делает все пять одной постройкой, даже если
        // первая и пятая ничем больше не соединены.
        if (e.refs.empty()) {
            continue;
        }
        const auto first = slot.find(e.refs.front());
        if (first == slot.end()) {
            continue;
        }
        for (std::size_t k = 1; k < e.refs.size(); ++k) {
            const auto it = slot.find(e.refs[k]);
            if (it != slot.end()) {
                sets.unite(first->second, it->second);
            }
        }
    }
    // ВЕРШИНА НА ОСИ ПРИНАДЛЕЖИТ ХОЗЯИНУ. Без этого якорь, посаженный на
    // столб, но пока ничем не занятый, считался бы отдельной постройкой — а он
    // физически часть той же, он на ней сидит.
    for (std::size_t i = 0; i < vertices_.size(); ++i) {
        if (vertices_[i].anchoring != Anchoring::OnEdge) {
            continue;
        }
        const Element* host = element(vertices_[i].host);
        if (host == nullptr || host->refs.empty()) {
            continue;
        }
        const auto it = slot.find(host->refs.front());
        if (it != slot.end()) {
            sets.unite(i, it->second);
        }
    }

    // ПОРЯДОК ДЕТЕРМИНИРОВАН: группы идут в порядке первого появления вершины,
    // внутри группы — в порядке хранения. Две загрузки одного файла обязаны
    // назвать постройки одинаково, иначе «постройка №2» будет значить разное.
    std::unordered_map<std::size_t, std::size_t> group_of_root;
    std::vector<std::vector<VertexId>> out;
    for (std::size_t i = 0; i < vertices_.size(); ++i) {
        const std::size_t root = sets.find(i);
        const auto found = group_of_root.find(root);
        if (found == group_of_root.end()) {
            group_of_root[root] = out.size();
            out.push_back({vertices_[i].id});
        } else {
            out[found->second].push_back(vertices_[i].id);
        }
    }
    return out;
}

std::size_t HouseGraph::component_of(VertexId id) const {
    const auto groups = components();
    for (std::size_t g = 0; g < groups.size(); ++g) {
        if (std::find(groups[g].begin(), groups[g].end(), id) != groups[g].end()) {
            return g;
        }
    }
    return static_cast<std::size_t>(-1);
}

std::vector<ElementId> HouseGraph::bridges() const {
    const std::size_t before = components().size();
    std::vector<ElementId> out;
    for (const Element& e : elements_) {
        // Копия без одного элемента. Дорого по меркам алгоритмов и дёшево по
        // меркам наших размеров: сотни элементов, и считается это не в кадре, а
        // когда человек навёл курсор на элемент.
        HouseGraph probe = *this;
        probe.elements_.erase(std::remove_if(probe.elements_.begin(),
                                             probe.elements_.end(),
                                             [&e](const Element& x) { return x.id == e.id; }),
                              probe.elements_.end());
        // Вершины, сидевшие на оси удалённого, теряют хозяина. Для подсчёта
        // компонент это правильно: без хозяина они и правда сами по себе.
        if (probe.components().size() > before) {
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
