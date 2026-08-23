/*
Created: 18:08:2026 - 17:21:51
Last updated: 23:08:2026 - 18:09:35
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
- 22:08:2026 - 17:45:00: тело bake_house_sky_visibility().
- 22:08:2026 - 22:46:52: AO крупных горизонтальных панелей усредняется по части: интерполяция от тёмного угла тянула КЛИН через плиту 2х2 м, триангуляция проступала тенью (владелец: «чёрные треугольники на террасе»). Порог 3 м², грани n.y>0.7.
- 23:08:2026 - 02:01:25: AO-печка: косинусные веса и деление на их сумму — мера перестала штрафовать за наклон (скат терял половину веера по касательной, потолок стены был 0.5).
- 23:08:2026 - 18:09:35: mb.glow из ElementParams — самосветные элементы (пламя, стекло).
*/

#include "engine/world/sources/HouseMesh.h"

#include "engine/core/math/sources/Intersect.h"
#include "engine/core/math/sources/Ray.h"
#include "engine/world/sources/HouseMeshDetail.h"

#include "engine/world/sources/HouseStyle.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

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
        mb.glow = p.glow > 0.5f;
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

std::vector<std::uint8_t> bake_house_sky_visibility(const HouseMesh& mesh) {
    std::vector<std::uint8_t> out(mesh.vertices.size(), 255u);
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        return out;
    }
    // Заслонители — видимые части (collider_only не темнит видимое).
    std::vector<std::uint32_t> tris; // по три индекса
    tris.reserve(mesh.indices.size());
    for (const MeshPart& p : mesh.parts) {
        if (p.collider_only) {
            continue;
        }
        for (std::uint32_t i = 0; i + 2 < p.index_count; i += 3) {
            tris.push_back(mesh.indices[p.index_begin + i]);
            tris.push_back(mesh.indices[p.index_begin + i + 1]);
            tris.push_back(mesh.indices[p.index_begin + i + 2]);
        }
    }
    if (tris.empty()) {
        return out;
    }
    // Равномерная сетка ускорения по XZ (лучи идут вверх — колонка XZ
    // перечисляет всех кандидатов; вертикальная разбивка дала бы мало при
    // домах высотой в один-два десятка метров).
    glm::vec3 lo{1e9f};
    glm::vec3 hi{-1e9f};
    for (const HouseVertex& v : mesh.vertices) {
        lo = glm::min(lo, v.pos);
        hi = glm::max(hi, v.pos);
    }
    constexpr float CELL = 1.0f;
    const int nx = std::max(1, static_cast<int>((hi.x - lo.x) / CELL) + 1);
    const int nz = std::max(1, static_cast<int>((hi.z - lo.z) / CELL) + 1);
    std::vector<std::vector<std::uint32_t>> grid(
        static_cast<size_t>(nx) * static_cast<size_t>(nz));
    const auto cell_of = [&](float x, float z) {
        const int cx = std::clamp(static_cast<int>((x - lo.x) / CELL), 0, nx - 1);
        const int cz = std::clamp(static_cast<int>((z - lo.z) / CELL), 0, nz - 1);
        return static_cast<size_t>(cz) * static_cast<size_t>(nx)
             + static_cast<size_t>(cx);
    };
    for (std::uint32_t t = 0; t < tris.size(); t += 3) {
        const glm::vec3& a = mesh.vertices[tris[t]].pos;
        const glm::vec3& b = mesh.vertices[tris[t + 1]].pos;
        const glm::vec3& c = mesh.vertices[tris[t + 2]].pos;
        const float x0 = std::min({a.x, b.x, c.x});
        const float x1 = std::max({a.x, b.x, c.x});
        const float z0 = std::min({a.z, b.z, c.z});
        const float z1 = std::max({a.z, b.z, c.z});
        // Целочисленный обход колонок: цикл по float с NaN в вырожденной
        // вершине не завершился бы никогда.
        if (!(x0 <= x1) || !(z0 <= z1)) {
            continue;
        }
        const int cx0 = std::clamp(static_cast<int>((x0 - lo.x) / CELL), 0, nx - 1);
        const int cx1 = std::clamp(static_cast<int>((x1 - lo.x) / CELL), 0, nx - 1);
        const int cz0 = std::clamp(static_cast<int>((z0 - lo.z) / CELL), 0, nz - 1);
        const int cz1 = std::clamp(static_cast<int>((z1 - lo.z) / CELL), 0, nz - 1);
        for (int cz = cz0; cz <= cz1; ++cz) {
            for (int cx = cx0; cx <= cx1; ++cx) {
                grid[static_cast<size_t>(cz) * static_cast<size_t>(nx)
                     + static_cast<size_t>(cx)].push_back(t);
            }
        }
    }
    // 16 лучей в верхнюю полусферу: 8 азимутов x 2 возвышения. Возвышения
    // 25° и 60° — нижний ярус ловит свесы и соседние стены, верхний — небо
    // над головой. Набор ФИКСИРОВАН: тот же меш даёт те же байты.
    constexpr int AZ = 8;
    constexpr float ELEV[2] = {0.4363f, 1.0472f}; // 25°, 60° в радианах
    glm::vec3 dirs[AZ * 2];
    for (int e = 0; e < 2; ++e) {
        for (int a = 0; a < AZ; ++a) {
            const float az = (static_cast<float>(a) + 0.5f) * 6.2831853f
                           / static_cast<float>(AZ);
            const float ce = std::cos(ELEV[e]);
            dirs[e * AZ + a] = {ce * std::cos(az), std::sin(ELEV[e]),
                                ce * std::sin(az)};
        }
    }
    constexpr float REACH_M = 8.0f;   // дальше своя постройка не заслоняет
    constexpr float PUSH_M = 0.03f;   // отжим от собственной грани
    constexpr float FLOOR = 0.30f;    // запечатанный интерьер тёмен, не чёрен
    const auto blocked = [&](glm::vec3 org, const glm::vec3& dir) {
        // Шаг по колонкам сетки вдоль XZ-проекции луча; при почти
        // вертикальном луче достаточно своей колонки.
        const float horiz = std::sqrt(dir.x * dir.x + dir.z * dir.z);
        const float span = horiz * REACH_M;
        const int steps = std::max(1, static_cast<int>(span / CELL) + 1);
        math::Ray ray{org, dir};
        std::uint32_t last_cell = 0xFFFFFFFFu;
        for (int s = 0; s <= steps; ++s) {
            const float d = (static_cast<float>(s) / static_cast<float>(steps)) * span;
            const float px = org.x + (horiz > 1e-6f ? dir.x / horiz * d : 0.0f);
            const float pz = org.z + (horiz > 1e-6f ? dir.z / horiz * d : 0.0f);
            const auto ci = static_cast<std::uint32_t>(cell_of(px, pz));
            if (ci == last_cell) {
                continue;
            }
            last_cell = ci;
            for (size_t k = 0; k < grid[ci].size(); ++k) {
                const std::uint32_t t = grid[ci][k];
                if (math::ray_vs_triangle(ray, mesh.vertices[tris[t]].pos,
                                          mesh.vertices[tris[t + 1]].pos,
                                          mesh.vertices[tris[t + 2]].pos,
                                          REACH_M)) {
                    return true;
                }
            }
        }
        return false;
    };
    // ОБРАЗЕЦ НА ЯЧЕЙКУ, А НЕ НА ВЕРШИНУ. Дом с паркетом и кровлей несёт
    // десятки тысяч вершин, но доски одного настила стоят в сантиметрах друг
    // от друга и видят одно небо: образец кэшируется по квантованной позиции
    // (0.25 м) и октанту нормали — заслонение меняется на четвертьметрах, а
    // не на миллиметрах доски. Без кэша печка одного богатого дома стоила
    // сотни миллионов пересечений и подвесила загрузку карты (замерено
    // таймаутом приёмки 22.08, а не предположено).
    std::unordered_map<std::uint64_t, std::uint8_t> sample;
    sample.reserve(mesh.vertices.size() / 4);
    for (size_t vi = 0; vi < mesh.vertices.size(); ++vi) {
        const HouseVertex& v = mesh.vertices[vi];
        const auto q = [](float x) {
            return static_cast<std::uint64_t>(
                       static_cast<std::int64_t>(std::floor(x * 4.0f)) + 0x80000)
                 & 0xFFFFFu;
        };
        const std::uint64_t oct = (v.normal.x > 0.0f ? 1u : 0u)
                                | (v.normal.y > 0.0f ? 2u : 0u)
                                | (v.normal.z > 0.0f ? 4u : 0u);
        const std::uint64_t key =
            q(v.pos.x) | (q(v.pos.y) << 20) | (q(v.pos.z) << 40) | (oct << 60);
        if (const auto it = sample.find(key); it != sample.end()) {
            out[vi] = it->second;
            continue;
        }
        // КОСИНУСНОЕ ВЗВЕШИВАНИЕ И ДЕЛЕНИЕ НА СУММУ ВЕСОВ, А НЕ НА 16
        // (заказ архитектора, волна 23.08). Прежняя мера штрафовала предмет
        // ЗА НАКЛОН, а не за заслонение: у ската 25-35° половина веера
        // уходит по касательной и мертва по построению, у вертикальной
        // стены потолок был 0.5 — «открытое небо» стены не existовало.
        // Вес max(0, dot) — вклад луча в облучённость по Ламберту.
        float open_w = 0.0f;
        float total_w = 0.0f;
        for (const glm::vec3& d : dirs) {
            const float wgt = glm::dot(d, v.normal);
            // Луч, уходящий под собственную поверхность, неба не видит —
            // и не входит в знаменатель: он не «закрыт», его просто нет.
            if (wgt <= 0.05f) {
                continue;
            }
            total_w += wgt;
            if (!blocked(v.pos + v.normal * PUSH_M + d * PUSH_M, d)) {
                open_w += wgt;
            }
        }
        const float vis = total_w > 0.0f ? open_w / total_w : 1.0f;
        const auto byte = static_cast<std::uint8_t>(
            std::lround(255.0f * (FLOOR + (1.0f - FLOOR) * vis)));
        sample.emplace(key, byte);
        out[vi] = byte;
    }
    // КРУПНАЯ ГОРИЗОНТАЛЬНАЯ ПАНЕЛЬ НЕСЁТ ОДНО НЕБО, А НЕ ГРАДИЕНТ ПО УГЛАМ
    // (владелец 23.08: «у террасы какие-то чёрные треугольники рисуются»).
    // У плиты 2х2 м вершины стоят только по углам; угол у подпорной стенки
    // видит меньше неба, и интерполяция тянет его затемнение КЛИНОМ через
    // весь треугольник — триангуляция проступает тенью. Небо над открытой
    // плитой не меняется на её же метрах: верхним граням крупных частей
    // видимость усредняется по части. Порог площади 3 м² оставляет ступени,
    // подоконники и мелочь с их честными перепадами; порог нормали 0.7
    // не трогает стены и скаты.
    for (const MeshPart& part : mesh.parts) {
        if (part.collider_only || part.index_count < 3) {
            continue;
        }
        float area2 = 0.0f;
        double sum = 0.0;
        std::size_t n_up = 0;
        for (std::uint32_t i = part.index_begin;
             i + 2 < part.index_begin + part.index_count; i += 3) {
            const glm::vec3& a = mesh.vertices[mesh.indices[i]].pos;
            const glm::vec3& b = mesh.vertices[mesh.indices[i + 1]].pos;
            const glm::vec3& c = mesh.vertices[mesh.indices[i + 2]].pos;
            area2 += glm::length(glm::cross(b - a, c - a));
        }
        if (area2 * 0.5f < 3.0f) {
            continue;
        }
        for (std::uint32_t i = part.index_begin;
             i < part.index_begin + part.index_count; ++i) {
            const std::uint32_t vi2 = mesh.indices[i];
            if (mesh.vertices[vi2].normal.y > 0.7f) {
                sum += out[vi2];
                ++n_up;
            }
        }
        if (n_up < 3) {
            continue;
        }
        const auto avg = static_cast<std::uint8_t>(
            std::lround(sum / static_cast<double>(n_up)));
        for (std::uint32_t i = part.index_begin;
             i < part.index_begin + part.index_count; ++i) {
            const std::uint32_t vi2 = mesh.indices[i];
            if (mesh.vertices[vi2].normal.y > 0.7f) {
                out[vi2] = avg;
            }
        }
    }
    return out;
}

} // namespace dfn::world
