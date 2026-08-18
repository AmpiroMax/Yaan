/*
Created: 18:08:2026 - 17:50:55
Last updated: 18:08:2026 - 17:50:55
Module: engine/world
File: engine/world/sources/HouseRegister.cpp

Responsibility:
- Реализация регистрации типа. Устройство — в заголовке.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 18:08:2026 - 17:50:55: Создан вместе с заголовком.
*/

#include "engine/world/sources/HouseRegister.h"

#include <algorithm>
#include <unordered_map>

namespace dfn::world {

bool extract_component(const HouseGraph& src, VertexId seed, HouseGraph& out) {
    out = HouseGraph{};
    const std::size_t group = src.component_of(seed);
    if (group == static_cast<std::size_t>(-1)) {
        return false;
    }
    const auto groups = src.components();
    const std::vector<VertexId>& mine = groups[group];

    // ПОРЯДОК ПЕРЕНОСА — ЭТО ЗАВИСИМОСТЬ, А НЕ ПОРЯДОК СТРОК. Вершина на оси не
    // может быть создана раньше хозяина, а хозяин ссылается на вершины. Поэтому
    // то же исполнение до неподвижной точки, что и в читателе файла: там оно
    // появилось после того, как двухпроходная схема сломала главный сценарий.
    std::unordered_map<VertexId, VertexId> vmap;
    for (const VertexId id : mine) {
        const Vertex* v = src.vertex(id);
        if (v == nullptr || v->anchoring == Anchoring::OnEdge) {
            continue;
        }
        vmap[id] = out.add_vertex(v->anchoring, v->local);
    }

    std::unordered_map<ElementId, ElementId> emap;
    bool progress = true;
    while (progress) {
        progress = false;
        for (const Element& e : src.elements()) {
            if (emap.count(e.id) != 0) {
                continue;
            }
            const bool ours = std::any_of(
                e.refs.begin(), e.refs.end(), [&mine](VertexId r) {
                    return std::find(mine.begin(), mine.end(), r) != mine.end();
                });
            if (!ours) {
                continue;
            }
            std::vector<VertexId> refs;
            bool ready = true;
            for (const VertexId r : e.refs) {
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
            if (!out.add_element(e.kind, std::move(refs), e.style, made).ok) {
                continue;
            }
            (void)out.set_facing(made, e.facing_flipped);
            (void)out.set_closed(made, e.closed);
            for (const auto& kv : e.params) {
                (void)out.set_param(made, kv.first, kv.second);
            }
            emap[e.id] = made;
            progress = true;
        }
        for (const VertexId id : mine) {
            if (vmap.count(id) != 0) {
                continue;
            }
            const Vertex* v = src.vertex(id);
            if (v == nullptr || v->anchoring != Anchoring::OnEdge) {
                continue;
            }
            const auto host = emap.find(v->host);
            if (host == emap.end()) {
                continue;
            }
            VertexId made = 0;
            if (out.add_vertex_on_edge(host->second, v->host_t, made).ok) {
                vmap[id] = made;
                progress = true;
            }
        }
    }
    return true;
}

std::vector<RegisterFinding> check_registrable(const HouseGraph& g) {
    std::vector<RegisterFinding> out;

    // ПУСТУЮ ПОСТРОЙКУ РЕГИСТРИРОВАТЬ НЕЛЬЗЯ. Очевидно — и именно поэтому легко
    // забыть: пустой тип поставится без единой жалобы и будет невидим.
    if (g.element_count() == 0) {
        out.push_back({"в постройке нет ни одного элемента", NO_ELEMENT, NO_VERTEX});
        return out;
    }

    // ОДНА КОМПОНЕНТА. Регистрируется ПОСТРОЙКА, а постройка по определению одна
    // компонента; две — это две постройки, и какая из них тип, неизвестно.
    if (g.components().size() > 1) {
        out.push_back({"это не одна постройка, а несколько", NO_ELEMENT, NO_VERTEX});
    }

    // КАЖДАЯ ПОВЕРХНОСТЬ ОПИРАЕТСЯ НА КАРКАС (решение пользователя 18.08).
    // Проверяется ПО СОСЕДНИМ ПАРАМ вершин, а не «есть ли хоть одна прямая
    // рядом»: стена, у которой закреплена одна сторона из четырёх, висит ровно
    // так же, как незакреплённая вовсе.
    for (const Element& e : g.elements()) {
        if (e.kind != ElementKind::Surface || e.refs.size() < 2) {
            continue;
        }
        const std::size_t n = e.refs.size();
        const std::size_t pairs = e.closed ? n : n - 1;
        for (std::size_t i = 0; i < pairs; ++i) {
            const VertexId a = e.refs[i];
            const std::size_t next = (i + 1 == n) ? 0 : i + 1;
            const VertexId b = e.refs[next];
            const bool framed = std::any_of(
                g.elements().begin(), g.elements().end(), [&](const Element& x) {
                    if (x.kind != ElementKind::Line || x.refs.size() < 2) {
                        return false;
                    }
                    return (x.refs.front() == a && x.refs.back() == b)
                        || (x.refs.front() == b && x.refs.back() == a);
                });
            if (!framed) {
                out.push_back({"у поверхности есть сторона без каркаса", e.id, a});
                break; // одной жалобы на элемент достаточно
            }
        }
    }

    // ВЕРШИНА БЕЗ ЕДИНОГО ЭЛЕМЕНТА — мусор. При правке это нормальный ход работы
    // (поставил якоря, ещё не соединил), при регистрации — нет.
    for (const Vertex& v : g.vertices()) {
        if (g.incident(v.id).empty() && v.anchoring != Anchoring::OnEdge) {
            out.push_back({"вершина ни к чему не подключена", NO_ELEMENT, v.id});
        }
    }
    return out;
}

} // namespace dfn::world
