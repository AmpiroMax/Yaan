/*
Created: 18:08:2026 - 17:21:51
Last updated: 21:08:2026 - 00:40:00
Module: engine/world
File: engine/world/sources/HouseMesh.cpp

Responsibility:
- Реализация геометрии постройки. Устройство — docs/DESIGN_HOUSE_GRAPH.md §5.4.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- НИ ОДНОГО ВЫЧИТАНИЯ РАДИУСОВ. Если однажды здесь появится «укоротим стену на
  радиус столба», значит правило §7.0 забыто: крепление идёт к ОСИ, нахлёст
  тел — норма. Цена ошибки не косметическая: подгонка к телу соседа делает
  смену радиуса пересчётом всей стены, а сегодня она не стоит ничего.
- ВСЁ ЗАМКНУТО (правило 52). У прямой есть торцы, у пластины — рант, у стены —
  торцы. Открытая оболочка сойдёт за тело ровно до первого взгляда сбоку и до
  первого выпуклого разложения в физике.
*/
/*
UPD:
- 18:08:2026 - 17:21:51: Создан вместе с заголовком.
- 18:08:2026 - 18:26:06: ДВА ШВА СОШЛИСЬ С МОДЕЛЬЮ. (1) Числа берутся из Element::params, а строка
  стиля осталась ЗАПАСНЫМ ходом. Расстыковка была настоящая и молчаливая:
  инструменты редактора пишут через set_param, то есть в поле, — и ни одно
  заданное человеком число не доехало бы до геометрии. Ни рукав построителя, ни
  рукав модели этого не видят: каждый прав в своей половине.
  (2) Цепочка или контур — решает Element::closed, а не «высота больше нуля».
  Прежнее правило было честно помечено временным; угадывание молча ломается на
  плоском поле, которому задали высоту, и на стене нулевой высоты.
- 18:08:2026 - 22:20:15: equidistant_point (МНК-центр окружности в плоскости контура) и surface_centre.
- 18:08:2026 - 23:52:10: surface_centre считает среднее; расчёт равноудалённой точки убран.
- 19:08:2026 - 04:05:50: Поворот текстуры — вокруг СРЕДНЕЙ точки грани (крышка и каждая грань ранта); sides доезжает из поля params; свойства чужих слоёв (mat/tone/door/hinge) — не находка.
- 19:08:2026 - 05:26:10: Обшивка по раскладке HouseStyle стала ГЕОМЕТРИЕЙ: доски, раскосы (выступают дальше досок), рамы проёмов по периметру; push_wall_slab выправляет обход по знаку площади; clad/windows в разборе свойств.
- 20:08:2026 - 00:58:40: Кладка рядами с перевязкой и детерминированной дрожью глубины (хэш ряда и колонки — две сборки дают один меш); под-части по материалу через MeshBuilder.set_material; фахверк — брус, кирпич — глина, блоки — камень.
- 20:08:2026 - 01:47:30: Лестница между якорями: ступени коробами (подъём 17.5 см, шаг РОВНЫЙ делением нацело, физика — те же коробы), тетивы по бокам; дверной проём от пола (окна уступают вслух); паркет пола рядами с перевязкой, кусок вне контура выпадает.
- 20:08:2026 - 12:10:00: Паркет режется по контуру (Сазерленд–Ходжман) и лежит встык; лестница fill=6 на четырёх точках; форма plank; paint — чужой слой.
- 20:08:2026 - 12:55:00: element_params_of вынесена из безымянного пространства имён (нужна пикингу).
- 20:08:2026 - 15:30:00: Дверной проём СКВОЗНОЙ: пролёт с doors>0 собирается из простенков и перемычки той же раскладкой, что у обшивки.
- 20:08:2026 - 17:30:00: Окна СКВОЗНЫЕ с листом остекления; кровля рядами поперёк уклона (build_roof_courses); износ роняет куски и углубляет дрожь; детали стены: перерубы, ставни+подоконник, крыльцо, завалинка (обходит дверь).
- 20:08:2026 - 18:40:00: Венцы сруба fill=4 (ряды бруса, проёмы обходятся); кровля не роняет куски (заплатки читались багом); камень крыльца/завалинки одного тона; ставни шире и с выносом.
- 20:08:2026 - 19:05:00: fill=4 в обшивке — только рамы проёмов: доски фахверка поверх венцов читались сайдингом.
- 20:08:2026 - 20:30:00: check_roof_support (эвристика гиперграфа: вершина крыши без стены — находка); марши open=1 доски / open=2 каменные блоки с вывалами; ветхий паркет (ширина рядов, щели, обломки, дыры); сколотые углы кладки; потолочные балки; кровля/паркет — швы циклов в хвостах.
- 20:08:2026 - 22:40:00: param_slots() — одна таблица числовых полей на лексер и слияние (не-число больше не подменяет значение дефолтом); обшивка только для fill 2/3/4; мёртвые ветка и параметр сняты.
- 21:08:2026 - 00:40:00: Разрезан по файлу на алгоритм (решение пользователя 21.08): здесь осталась СБОРКА (ordered_elements, build_house_mesh, surface_centre/normal); алгоритмы — House{Geom,Params,Bodies,Stairs,Parquet,Roof,Plate,Walls,Rules}.cpp, общие руки — HouseMeshDetail.h.
*/

#include "engine/world/sources/HouseMesh.h"

#include "engine/world/sources/HouseMeshDetail.h"

#include "engine/world/sources/HouseStyle.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <glm/geometric.hpp>

namespace dfn::world {



// ---------------------------------------------------------------------------
// Свойства
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Тела
// ---------------------------------------------------------------------------

namespace {

/// ЦЕПОЧКА ИЛИ КОНТУР — РЕШАЕТ ПРИЗНАК ЗАМКНУТОСТИ, а не высота.
///
/// Здесь стояло временное правило «высота больше нуля значит цепочка», честно
/// помеченное временным: признака замкнутости в Element тогда не было. Теперь
/// он есть (Element::closed, заведён 18.08 по этой же просьбе), и угадывание
/// снято. Разница не косметическая: угадывание молча ломается на плоском поле,
/// которому задали высоту, и на стене нулевой высоты — а это не выдуманные
/// случаи, а два первых, до которых дойдёт человек.
///
/// Двух вершин на контур не хватает по определению, поэтому они всегда цепочка.
bool is_chain_surface(const Element& e, const ElementParams&) {
    return e.refs.size() < 3 || !e.closed;
}

} // namespace

// ---------------------------------------------------------------------------
// Сборка
// ---------------------------------------------------------------------------

std::vector<ElementId> ordered_elements(const HouseGraph& g) {
    // У графа сегодня нет перебора элементов, зато есть перебор вершин через
    // components() и обратный ход incident(). Каждый элемент по построению
    // ссылается хотя бы на одну существующую вершину, поэтому такой обход
    // ничего не теряет; сортировка по имени делает порядок тем же, что в файле.
    std::vector<ElementId> ids;
    for (const std::vector<VertexId>& group : g.components()) {
        for (const VertexId v : group) {
            for (const ElementId e : g.incident(v)) {
                ids.push_back(e);
            }
        }
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

const MeshPart* HouseMesh::part_of(ElementId id) const {
    for (const MeshPart& p : parts) {
        if (p.element == id) {
            return &p;
        }
    }
    return nullptr;
}

HouseMesh build_house_mesh(const HouseGraph& g) {
    HouseMesh mesh;
    MeshBuilder mb{&mesh};
    for (const ElementId id : ordered_elements(g)) {
        const Element* e = g.element(id);
        if (e == nullptr || e->refs.empty()) {
            continue;
        }
        std::vector<ParamIssue> issues;
        const ElementParams p = element_params_of(*e, &issues);
        for (const ParamIssue& is : issues) {
            mesh.findings.push_back({id, MeshIssue::UnknownParam, 0.0f, is.token + ": " + is.why});
        }
        mb.begin_element(id);
        if (e->kind == ElementKind::Line) {
            build_line(g, *e, p, mb, mesh);
        } else {
            std::vector<glm::vec3> pts;
            pts.reserve(e->refs.size());
            for (const VertexId r : e->refs) {
                pts.push_back(g.resolved_local(r));
            }
            if (is_chain_surface(*e, p)) {
                build_chain_surface(*e, p, pts, mb, mesh);
            } else {
                build_contour_surface(*e, p, pts, mb, mesh);
            }
        }
        mb.end_element();
    }
    // ПРАВИЛО ОПОРЫ КРЫШ — говорится при каждой сборке (проверка, не запрет).
    for (MeshFinding& f : check_roof_support(g)) {
        mesh.findings.push_back(std::move(f));
    }
    return mesh;
}

bool surface_centre(const HouseGraph& g, ElementId id, glm::vec3& out) {
    const Element* e = g.element(id);
    if (e == nullptr || e->kind != ElementKind::Surface || e->refs.empty()) {
        return false;
    }
    const ElementParams p = element_params_of(*e, nullptr);
    std::vector<glm::vec3> pts;
    pts.reserve(e->refs.size());
    for (const VertexId r : e->refs) {
        pts.push_back(g.resolved_local(r));
    }
    glm::vec3 mid{0.0f};
    for (const glm::vec3& v : pts) {
        mid += v;
    }
    mid /= static_cast<float>(pts.size());
    if (is_chain_surface(*e, p)) {
        // ПОЛОВИНА ВЫСОТЫ: полотно уходит вверх от этой ломаной, и его видное
        // место — посередине, а не на нижней кромке.
        out = mid + glm::vec3{0.0f, p.height * 0.5f, 0.0f};
        return true;
    }
    // СРЕДНЕЕ ПО ВСЕМ ВЕРШИНАМ, И ЭТО РЕШЕНИЕ ПОЛЬЗОВАТЕЛЯ (18.08, уточнение к
    // его же прежней просьбе): «стрелка всегда должна стоять между mean от всех
    // координат точек, не важно 2, 3 или 100 их».
    //
    // ПЕРВЫЙ ЗАХОД СЧИТАЛ РАВНОУДАЛЁННУЮ ТОЧКУ — центр окружности по МНК. Она
    // отвечает на другой вопрос («откуда все углы одинаково далеко») и на
    // вытянутом или невыпуклом контуре уезжает далеко от самого полотна, вплоть
    // до места, где полотна нет вовсе. Среднее такого не умеет по построению:
    // оно всегда внутри выпуклой оболочки вершин.
    out = mid;
    return true;
}

bool surface_normal(const HouseGraph& g, ElementId id, glm::vec3& out) {
    const Element* e = g.element(id);
    if (e == nullptr || e->kind != ElementKind::Surface || e->refs.size() < 2) {
        return false;
    }
    const ElementParams p = element_params_of(*e, nullptr);
    std::vector<glm::vec3> pts;
    pts.reserve(e->refs.size());
    for (const VertexId r : e->refs) {
        pts.push_back(g.resolved_local(r));
    }
    if (is_chain_surface(*e, p)) {
        const glm::vec3 d = pts[1] - pts[0];
        if (std::sqrt(d.x * d.x + d.z * d.z) < HOUSE_GEOM_EPS) {
            return false;
        }
        out = glm::normalize(glm::cross(d, glm::vec3{0.0f, 1.0f, 0.0f}));
    } else {
        const FittedPlane plane = fit_contour_plane(pts);
        if (plane.degenerate) {
            return false;
        }
        out = plane.normal;
    }
    if (e->facing_flipped) {
        out = -out;
    }
    return true;
}

} // namespace dfn::world
