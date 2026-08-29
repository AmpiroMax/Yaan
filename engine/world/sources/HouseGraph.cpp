/*
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

#include "engine/world/sources/HouseGraph.h"

#include <algorithm>
#include <unordered_map>

namespace dfn::world {

VertexId HouseGraph::add_vertex(Anchoring anchoring, glm::vec3 local) {
    // ВЕРСИЯ РАСТЁТ ТОЛЬКО ПРИ УДАВШЕМСЯ ИЗМЕНЕНИИ (аудит 20.08: бамп до
    // валидации заставлял отказ пересобирать меш и коллайдер) — довод у
    // version().
    ++version_;
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
    // ВЕРСИЯ РАСТЁТ ТОЛЬКО ПРИ УДАВШЕМСЯ ИЗМЕНЕНИИ (аудит 20.08: бамп до
    // валидации заставлял отказ пересобирать меш и коллайдер) — довод у
    // version().
    ++version_;
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
    // ВЕРСИЯ РАСТЁТ ТОЛЬКО ПРИ УДАВШЕМСЯ ИЗМЕНЕНИИ (аудит 20.08: бамп до
    // валидации заставлял отказ пересобирать меш и коллайдер) — довод у
    // version().
    ++version_;
    Element e;
    e.id = next_element_++;
    e.kind = kind;
    e.refs = std::move(refs);
    e.style = std::move(style);
    elements_.push_back(std::move(e));
    out = elements_.back().id;
    return {};
}

GraphResult HouseGraph::adopt_vertex(VertexId id, Anchoring anchoring, glm::vec3 local) {
    if (id == NO_VERTEX) {
        return {false, "имя вершины не может быть нулём", {}};
    }
    if (vertex(id) != nullptr) {
        return {false, "вершина с таким именем уже есть", {}};
    }
    // ВЕРСИЯ РАСТЁТ ТОЛЬКО ПРИ УДАВШЕМСЯ ИЗМЕНЕНИИ (аудит 20.08: бамп до
    // валидации заставлял отказ пересобирать меш и коллайдер) — довод у
    // version().
    ++version_;
    Vertex v;
    v.id = id;
    v.anchoring = anchoring;
    v.local = local;
    vertices_.push_back(v);
    // Счётчик двигается ЗА самым большим взятым именем: иначе следующая
    // добавленная вершина получит имя, которое уже занято прочитанной.
    next_vertex_ = std::max(next_vertex_, id + 1);
    return {};
}

GraphResult HouseGraph::adopt_vertex_on_edge(VertexId id, ElementId host, float t) {
    if (id == NO_VERTEX) {
        return {false, "имя вершины не может быть нулём", {}};
    }
    if (vertex(id) != nullptr) {
        return {false, "вершина с таким именем уже есть", {}};
    }
    const Element* e = element(host);
    if (e == nullptr) {
        return {false, "хозяина нет", {}};
    }
    if (e->kind != ElementKind::Line) {
        return {false, "вершина сидит только на оси прямой", {}};
    }
    // ВЕРСИЯ РАСТЁТ ТОЛЬКО ПРИ УДАВШЕМСЯ ИЗМЕНЕНИИ (аудит 20.08: бамп до
    // валидации заставлял отказ пересобирать меш и коллайдер) — довод у
    // version().
    ++version_;
    Vertex v;
    v.id = id;
    v.anchoring = Anchoring::OnEdge;
    v.host = host;
    v.host_t = std::clamp(t, 0.0f, 1.0f);
    vertices_.push_back(v);
    next_vertex_ = std::max(next_vertex_, id + 1);
    return {};
}

GraphResult HouseGraph::adopt_element(ElementId id, ElementKind kind,
                                      std::vector<VertexId> refs, std::string style) {
    if (id == NO_ELEMENT) {
        return {false, "имя элемента не может быть нулём", {}};
    }
    if (element(id) != nullptr) {
        return {false, "элемент с таким именем уже есть", {}};
    }
    if (refs.size() < min_refs_for(kind)) {
        return {false, "слишком мало вершин для этого вида", {}};
    }
    for (const VertexId r : refs) {
        if (vertex(r) == nullptr) {
            return {false, "элемент ссылается на несуществующую вершину", {}};
        }
    }
    for (std::size_t i = 1; i < refs.size(); ++i) {
        if (refs[i] == refs[i - 1]) {
            return {false, "одна и та же вершина дважды подряд", {}};
        }
    }
    // ВЕРСИЯ РАСТЁТ ТОЛЬКО ПРИ УДАВШЕМСЯ ИЗМЕНЕНИИ (аудит 20.08: бамп до
    // валидации заставлял отказ пересобирать меш и коллайдер) — довод у
    // version().
    ++version_;
    Element e;
    e.id = id;
    e.kind = kind;
    e.refs = std::move(refs);
    e.style = std::move(style);
    elements_.push_back(std::move(e));
    next_element_ = std::max(next_element_, id + 1);
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
    // ВЕРСИЯ РАСТЁТ ТОЛЬКО ПРИ УДАВШЕМСЯ ИЗМЕНЕНИИ (аудит 20.08: бамп до
    // валидации заставлял отказ пересобирать меш и коллайдер) — довод у
    // version().
    ++version_;
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
    // ВЕРСИЯ РАСТЁТ ТОЛЬКО ПРИ УДАВШЕМСЯ ИЗМЕНЕНИИ (аудит 20.08: бамп до
    // валидации заставлял отказ пересобирать меш и коллайдер) — довод у
    // version().
    ++version_;
    elements_.erase(std::remove_if(elements_.begin(), elements_.end(),
                                   [id](const Element& e) { return e.id == id; }),
                    elements_.end());
    return {};
}

GraphResult HouseGraph::move_vertex(VertexId id, glm::vec3 new_local) {
    Vertex* v = find_vertex(id);
    if (v == nullptr) {
        return {false, "такой вершины нет", {}};
    }
    // ВЕРШИНА НА ОСИ НЕ ДВИГАЕТСЯ НАПРЯМУЮ: её положение принадлежит хозяину.
    // Отказ, а не тихое игнорирование — молча проигнорированная правка
    // выглядит как сломанный инструмент, и мы это сегодня уже проходили.
    if (v->anchoring == Anchoring::OnEdge) {
        return {false, "вершина сидит на оси: двигай хозяина или параметр вдоль", {v->host}};
    }
    // ВЕРСИЯ РАСТЁТ ТОЛЬКО ПРИ УДАВШЕМСЯ ИЗМЕНЕНИИ (аудит 20.08: бамп до
    // валидации заставлял отказ пересобирать меш и коллайдер) — довод у
    // version().
    ++version_;
    v->local = new_local;
    // Ничего пересчитывать не надо: геометрия элементов НИГДЕ НЕ ХРАНИТСЯ, она
    // выводится из вершин. «За вершиной потянулось» — не работа, а отсутствие
    // второй копии. Вершины на осях разрешаются в resolved_local по требованию.
    return {};
}

GraphResult HouseGraph::slide_vertex(VertexId id, float t) {
    Vertex* v = find_vertex(id);
    if (v == nullptr) {
        return {false, "такой вершины нет", {}};
    }
    if (v->anchoring != Anchoring::OnEdge) {
        return {false, "вершина не сидит на оси: двигай её move_vertex", {}};
    }
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    if (clamped == v->host_t) {
        return {};
    }
    // ВЕРСИЯ РАСТЁТ ТОЛЬКО ПРИ УДАВШЕМСЯ ИЗМЕНЕНИИ (аудит 20.08: бамп до
    // валидации заставлял отказ пересобирать меш и коллайдер) — довод у
    // version().
    ++version_;
    v->host_t = clamped;
    return {};
}

GraphResult HouseGraph::merge_from(
    const HouseGraph& src, const std::function<glm::vec3(glm::vec3)>& map_local) {
    // Сдвиг имён: чужие id занимают полосу за самым большим своим.
    const VertexId v_base = next_vertex_;
    const ElementId e_base = next_element_;
    const auto vmap = [&](VertexId v) { return v_base + v; };
    const auto emap = [&](ElementId e) { return e_base + e; };

    // ДО НЕПОДВИЖНОЙ ТОЧКИ: обычная вершина не ждёт никого, элемент ждёт
    // свои вершины, вершина на оси ждёт хозяина-элемент. Проход без
    // прогресса значит цикл или битую ссылку — отказ вслух.
    std::vector<const Vertex*> pend_v;
    std::vector<const Element*> pend_e;
    for (const Vertex& v : src.vertices()) {
        pend_v.push_back(&v);
    }
    for (const Element& e : src.elements()) {
        pend_e.push_back(&e);
    }
    while (!pend_v.empty() || !pend_e.empty()) {
        bool progress = false;
        for (std::size_t i = 0; i < pend_v.size();) {
            const Vertex& v = *pend_v[i];
            GraphResult r;
            if (v.anchoring == Anchoring::OnEdge) {
                if (element(emap(v.host)) == nullptr) {
                    ++i;
                    continue; // хозяин ещё не перенесён
                }
                r = adopt_vertex_on_edge(vmap(v.id), emap(v.host), v.host_t);
            } else {
                r = adopt_vertex(vmap(v.id), v.anchoring, map_local(v.local));
            }
            if (!r.ok) {
                return r;
            }
            pend_v.erase(pend_v.begin() + static_cast<std::ptrdiff_t>(i));
            progress = true;
        }
        for (std::size_t i = 0; i < pend_e.size();) {
            const Element& e = *pend_e[i];
            bool ready = true;
            for (const VertexId r0 : e.refs) {
                if (vertex(vmap(r0)) == nullptr) {
                    ready = false;
                    break;
                }
            }
            if (!ready) {
                ++i;
                continue;
            }
            std::vector<VertexId> refs;
            refs.reserve(e.refs.size());
            for (const VertexId r0 : e.refs) {
                refs.push_back(vmap(r0));
            }
            GraphResult r = adopt_element(emap(e.id), e.kind, std::move(refs), e.style);
            if (!r.ok) {
                return r;
            }
            if (e.closed) {
                (void)set_closed(emap(e.id), true);
            }
            if (e.facing_flipped) {
                (void)set_facing(emap(e.id), true);
            }
            for (const auto& kv : e.params) {
                (void)set_param(emap(e.id), kv.first, kv.second);
            }
            pend_e.erase(pend_e.begin() + static_cast<std::ptrdiff_t>(i));
            progress = true;
        }
        if (!progress) {
            return {false, "слияние не сходится: цикл или битая ссылка", {}};
        }
    }
    return {};
}

GraphResult HouseGraph::set_facing(ElementId id, bool flipped) {
    Element* e = find_element(id);
    if (e == nullptr) {
        return {false, "такого элемента нет", {}};
    }
    // ВЕРСИЯ РАСТЁТ ТОЛЬКО ПРИ УДАВШЕМСЯ ИЗМЕНЕНИИ (аудит 20.08: бамп до
    // валидации заставлял отказ пересобирать меш и коллайдер) — довод у
    // version().
    ++version_;
    e->facing_flipped = flipped;
    return {};
}

GraphResult HouseGraph::set_style(ElementId id, std::string style) {
    Element* e = find_element(id);
    if (e == nullptr) {
        return {false, "такого элемента нет", {}};
    }
    // ВЕРСИЯ РАСТЁТ ТОЛЬКО ПРИ УДАВШЕМСЯ ИЗМЕНЕНИИ (аудит 20.08: бамп до
    // валидации заставлял отказ пересобирать меш и коллайдер) — довод у
    // version().
    ++version_;
    e->style = std::move(style);
    return {};
}

GraphResult HouseGraph::set_param(ElementId id, std::string key, std::string value) {
    Element* e = find_element(id);
    if (e == nullptr) {
        return {false, "такого элемента нет", {}};
    }
    for (auto& kv : e->params) {
        if (kv.first == key) {
            // ТО ЖЕ ЗНАЧЕНИЕ — НЕ ИЗМЕНЕНИЕ: без бампа, иначе ползунок,
            // вернувшийся в ту же цифру, пересобирал бы дом.
            if (kv.second == value) {
                return {};
            }
            ++version_;
            kv.second = std::move(value);
            return {};
        }
    }
    // ВЕРСИЯ РАСТЁТ ТОЛЬКО ПРИ УДАВШЕМСЯ ИЗМЕНЕНИИ (аудит 20.08: бамп до
    // валидации заставлял отказ пересобирать меш и коллайдер) — довод у
    // version().
    ++version_;
    e->params.emplace_back(std::move(key), std::move(value));
    return {};
}

GraphResult HouseGraph::set_closed(ElementId id, bool closed) {
    Element* e = find_element(id);
    if (e == nullptr) {
        return {false, "такого элемента нет", {}};
    }
    // ВЕРСИЯ РАСТЁТ ТОЛЬКО ПРИ УДАВШЕМСЯ ИЗМЕНЕНИИ (аудит 20.08: бамп до
    // валидации заставлял отказ пересобирать меш и коллайдер) — довод у
    // version().
    ++version_;
    e->closed = closed;
    return {};
}

std::string HouseGraph::param(ElementId id, const std::string& key) const {
    const Element* e = element(id);
    if (e == nullptr) {
        return {};
    }
    for (const auto& kv : e->params) {
        if (kv.first == key) {
            return kv.second;
        }
    }
    return {};
}

glm::vec3 HouseGraph::resolved_local(VertexId id) const {
    // Глубина спуска ограничена числом вершин: длиннее честной цепочки не
    // бывает, а всё, что длиннее, — цикл.
    glm::vec3 last{0.0f};
    VertexId cur = id;
    for (std::size_t guard = 0; guard <= vertices_.size(); ++guard) {
        const Vertex* v = vertex(cur);
        if (v == nullptr) {
            return last;
        }
        last = v->local;
        if (v->anchoring != Anchoring::OnEdge) {
            return v->local;
        }
        const Element* host = element(v->host);
        if (host == nullptr || host->refs.size() < 2) {
            return last; // осиротевшая вершина остаётся там, где была
        }
        // Точка на оси: линейно между концами. Концы разрешаются ТЕМ ЖЕ
        // спуском, поэтому вершина на прямой, чей конец сам сидит на прямой,
        // работает без отдельного случая.
        const glm::vec3 a = resolved_local_step(host->refs.front(), guard);
        const glm::vec3 b = resolved_local_step(host->refs.back(), guard);
        return a + (b - a) * v->host_t;
    }
    // Сюда попадаем только по циклу. Возвращаем последнее осмысленное
    // положение: инструмент нарисует вершину не там, где надо, но НЕ ЗАВИСНЕТ,
    // а судья назовёт цикл отдельной находкой.
    return last;
}

glm::vec3 HouseGraph::resolved_local_step(VertexId id, std::size_t depth) const {
    if (depth > vertices_.size()) {
        const Vertex* v = vertex(id);
        return v != nullptr ? v->local : glm::vec3{0.0f};
    }
    const Vertex* v = vertex(id);
    if (v == nullptr) {
        return glm::vec3{0.0f};
    }
    if (v->anchoring != Anchoring::OnEdge) {
        return v->local;
    }
    const Element* host = element(v->host);
    if (host == nullptr || host->refs.size() < 2) {
        return v->local;
    }
    const glm::vec3 a = resolved_local_step(host->refs.front(), depth + 1);
    const glm::vec3 b = resolved_local_step(host->refs.back(), depth + 1);
    return a + (b - a) * v->host_t;
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
