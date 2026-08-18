/*
Created: 18:08:2026 - 17:03:12
Last updated: 18:08:2026 - 17:47:28
Module: engine/world
File: engine/world/sources/HouseFile.cpp

Responsibility:
- Реализация чтения и записи .dfh. Устройство — в заголовке.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 18:08:2026 - 17:03:12: Создан вместе с заголовком.
- 18:08:2026 - 17:06:23: разбор до неподвижной точки (см. заголовок).
- 18:08:2026 - 17:47:28: запись и чтение чисел и замкнутости (см. заголовок).
*/

#include "engine/world/sources/HouseFile.h"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dfn::world {
namespace {

const char* anchoring_word(Anchoring a) {
    switch (a) {
        case Anchoring::Free: return "free";
        case Anchoring::OnGround: return "ground";
        case Anchoring::OnEdge: return "on_edge";
    }
    return "free";
}

std::string vname(VertexId id) { return "v" + std::to_string(id); }
std::string ename(ElementId id) { return "e" + std::to_string(id); }

/// Число с ФИКСИРОВАННЫМ числом знаков. Не косметика: круговой прогон обязан
/// сходиться, а формат по умолчанию у разных реализаций печатает по-разному.
std::string num(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", static_cast<double>(v));
    return buf;
}

} // namespace

std::string write_house(const HouseGraph& g) {
    // Собираем имена в порядке возрастания: порядок хранения — внутреннее дело
    // графа и может измениться от удаления, а файл обязан быть устойчивым.
    std::vector<VertexId> vids;
    std::vector<ElementId> eids;
    for (VertexId i = 1; i <= static_cast<VertexId>(g.vertex_count() + g.element_count() + 1); ++i) {
        if (g.vertex(i) != nullptr) {
            vids.push_back(i);
        }
    }
    for (ElementId i = 1; i <= static_cast<ElementId>(g.vertex_count() + g.element_count() + 1); ++i) {
        if (g.element(i) != nullptr) {
            eids.push_back(i);
        }
    }

    std::ostringstream out;
    out << "# dfh 1\n";
    for (const VertexId id : vids) {
        const Vertex* v = g.vertex(id);
        out << "vertex " << vname(id) << ' ' << anchoring_word(v->anchoring);
        if (v->anchoring == Anchoring::OnEdge) {
            out << ' ' << ename(v->host) << ' ' << num(v->host_t);
        } else {
            out << ' ' << num(v->local.x) << ' ' << num(v->local.y) << ' '
                << num(v->local.z);
        }
        out << '\n';
    }
    for (const ElementId id : eids) {
        const Element* e = g.element(id);
        out << (e->kind == ElementKind::Line ? "line " : "surface ") << ename(id);
        for (const VertexId r : e->refs) {
            out << ' ' << vname(r);
        }
        out << " style=" << (e->style.empty() ? "-" : e->style);
        if (e->closed) {
            out << " closed=1";
        }
        if (e->facing_flipped) {
            out << " facing=flip";
        }
        // ЧИСЛА ПОСЛЕДНИМИ И КАЖДОЕ СВОИМ ТОКЕНОМ. Порядок их хранения — это
        // порядок, в котором их задал дизайнер, и переставлять его не за чем:
        // круговой прогон обязан сойтись побайтово, а сортировка сделала бы
        // файл, отличающийся от того, что человек написал руками.
        for (const auto& kv : e->params) {
            out << ' ' << kv.first << '=' << kv.second;
        }
        out << '\n';
    }
    return out.str();
}

HouseIoResult read_house(const std::string& text, HouseGraph& out) {
    out = HouseGraph{}; // чистим ДО чтения: иначе результат зависит от прошлого

    // РАЗБОР В ДВА ЭТАПА: сначала строки превращаются в НАМЕРЕНИЯ, потом
    // намерения исполняются по мере того, как становятся исполнимыми.
    //
    // Почему не проще. Первая версия читателя откладывала только вершины на
    // осях и исполняла элементы сразу — и на этом ЛОМАЛСЯ ГЛАВНЫЙ СЦЕНАРИЙ:
    // «поставил якорь на столб, от него балка». Балка ссылается на вершину,
    // которая ещё отложена, читатель звал её неизвестной и отвергал файл.
    // Нашлось рукавом, написанным совсем для другого — для защиты от цикла.
    struct VertexWish {
        std::string name;
        bool on_edge = false;
        Anchoring anchoring = Anchoring::Free;
        glm::vec3 local{0.0f};
        std::string host;
        float t = 0.0f;
        int line = 0;
        bool done = false;
    };
    struct ElementWish {
        std::string name;
        ElementKind kind = ElementKind::Line;
        std::vector<std::string> refs;
        std::string style;
        bool flip = false;
        bool closed = false;
        std::vector<std::pair<std::string, std::string>> params;
        int line = 0;
        bool done = false;
    };
    std::vector<VertexWish> vwish;
    std::vector<ElementWish> ewish;

    std::istringstream in(text);
    std::string raw;
    int line_no = 0;
    while (std::getline(in, raw)) {
        ++line_no;
        std::istringstream ln(raw);
        std::string word;
        if (!(ln >> word) || word.empty() || word[0] == '#') {
            continue;
        }
        if (word == "vertex") {
            VertexWish w;
            std::string kind;
            if (!(ln >> w.name >> kind)) {
                return {false, "у вершины нет имени или вида привязки", line_no};
            }
            w.line = line_no;
            if (kind == "on_edge") {
                w.on_edge = true;
                if (!(ln >> w.host >> w.t)) {
                    return {false, "у вершины на оси нет хозяина или параметра", line_no};
                }
            } else {
                w.anchoring = kind == "ground" ? Anchoring::OnGround : Anchoring::Free;
                if (!(ln >> w.local.x >> w.local.y >> w.local.z)) {
                    return {false, "у вершины нет трёх координат", line_no};
                }
            }
            for (const VertexWish& other : vwish) {
                if (other.name == w.name) {
                    return {false, "вершина с таким именем уже объявлена", line_no};
                }
            }
            vwish.push_back(std::move(w));
            continue;
        }
        if (word == "line" || word == "surface") {
            ElementWish w;
            w.kind = word == "line" ? ElementKind::Line : ElementKind::Surface;
            if (!(ln >> w.name)) {
                return {false, "у элемента нет имени", line_no};
            }
            w.line = line_no;
            std::string tok;
            while (ln >> tok) {
                if (tok.rfind("style=", 0) == 0) {
                    w.style = tok.substr(6);
                    if (w.style == "-") {
                        w.style.clear();
                    }
                    continue;
                }
                if (tok == "facing=flip") {
                    w.flip = true;
                    continue;
                }
                if (tok == "closed=1") {
                    w.closed = true;
                    continue;
                }
                // ЛЮБОЙ ТОКЕН С РАВНО — ПАРАМЕТР, всё остальное — ссылка на
                // вершину. Правило простое нарочно: имена вершин задаём мы
                // сами, и знака равенства в них нет по построению. Разбирать
                // иначе значило бы вести список известных параметров в двух
                // местах — здесь и в построителе меша.
                const std::size_t eq = tok.find('=');
                if (eq != std::string::npos && eq > 0) {
                    w.params.emplace_back(tok.substr(0, eq), tok.substr(eq + 1));
                    continue;
                }
                w.refs.push_back(tok);
            }
            for (const ElementWish& other : ewish) {
                if (other.name == w.name) {
                    return {false, "элемент с таким именем уже объявлен", line_no};
                }
            }
            ewish.push_back(std::move(w));
            continue;
        }
        return {false, "неизвестное слово в начале строки: " + word, line_no};
    }

    std::unordered_map<std::string, VertexId> vmap;
    std::unordered_map<std::string, ElementId> emap;

    // ИСПОЛНЕНИЕ ДО НЕПОДВИЖНОЙ ТОЧКИ. Каждый проход делает всё, что стало
    // исполнимо; как только проход не сделал НИЧЕГО, дальше не станет — значит
    // оставшееся либо ссылается в пустоту, либо замкнуто в круг.
    bool progress = true;
    while (progress) {
        progress = false;
        for (VertexWish& w : vwish) {
            if (w.done) {
                continue;
            }
            if (!w.on_edge) {
                vmap[w.name] = out.add_vertex(w.anchoring, w.local);
                w.done = true;
                progress = true;
                continue;
            }
            const auto host = emap.find(w.host);
            if (host == emap.end()) {
                continue; // хозяин ещё не создан — вернёмся следующим проходом
            }
            VertexId made = 0;
            const GraphResult r = out.add_vertex_on_edge(host->second, w.t, made);
            if (!r.ok) {
                return {false, r.why, w.line};
            }
            vmap[w.name] = made;
            w.done = true;
            progress = true;
        }
        for (ElementWish& w : ewish) {
            if (w.done) {
                continue;
            }
            std::vector<VertexId> refs;
            bool ready = true;
            for (const std::string& r : w.refs) {
                const auto it = vmap.find(r);
                if (it == vmap.end()) {
                    ready = false;
                    break;
                }
                refs.push_back(it->second);
            }
            if (!ready) {
                continue;
            }
            ElementId made = 0;
            const GraphResult r = out.add_element(w.kind, std::move(refs), w.style, made);
            if (!r.ok) {
                return {false, r.why, w.line};
            }
            if (w.flip) {
                out.set_facing(made, true);
            }
            if (w.closed) {
                out.set_closed(made, true);
            }
            for (auto& kv : w.params) {
                out.set_param(made, kv.first, kv.second);
            }
            emap[w.name] = made;
            w.done = true;
            progress = true;
        }
    }

    // ЧТО ОСТАЛОСЬ — ТО НЕИСПОЛНИМО, и надо сказать ЧТО ИМЕННО, а не «файл
    // плохой». Две разные беды выглядят одинаково с этой стороны, поэтому
    // различаем их: ссылка в пустоту или замкнутый круг.
    for (const VertexWish& w : vwish) {
        if (w.done) {
            continue;
        }
        const bool host_exists = std::any_of(
            ewish.begin(), ewish.end(),
            [&w](const ElementWish& e) { return e.name == w.host; });
        return {false,
                host_exists ? "круговая ссылка: вершина и её хозяин ждут друг друга"
                            : "вершина сидит на неизвестном элементе: " + w.host,
                w.line};
    }
    for (const ElementWish& w : ewish) {
        if (w.done) {
            continue;
        }
        for (const std::string& r : w.refs) {
            if (vmap.count(r) != 0) {
                continue;
            }
            const bool declared = std::any_of(
                vwish.begin(), vwish.end(),
                [&r](const VertexWish& v) { return v.name == r; });
            return {false,
                    declared ? "круговая ссылка: элемент и его вершина ждут друг друга"
                             : "ссылка на неизвестную вершину: " + r,
                    w.line};
        }
    }
    return {};
}

} // namespace dfn::world
