/*
Created: 21:08:2026 - 00:40:00
Last updated: 21:08:2026 - 00:40:00
Module: engine/world
File: engine/world/sources/HouseStairs.cpp

Responsibility:
- ЛЕСТНИЧНЫЙ МАРШ: ступени считаются из высоты (ровный шаг), коробы в
  коллайдер, тетивы; марш на четырёх точках; открытые доски и каменные
  блоки с зазорами.

Key items:
- build_stairs / build_stairs_contour; HOUSE_STAIR_RISE_M.

Dependencies:
- Uses: HouseMeshDetail.h
- Used by: сборка build_house_mesh (HouseMesh.cpp) и соседние модули постройки.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
- ПО ФАЙЛУ НА АЛГОРИТМ (решение пользователя 21.08): модуль держит ОДИН
  алгоритм постройки; общие руки — в HouseMeshDetail.h.
*/
/*
UPD:
- 21:08:2026 - 00:40:00: Вырезан из HouseMesh.cpp (1942 строки, девять алгоритмов в одном файле).
*/

#include "engine/world/sources/HouseMeshDetail.h"

#include <algorithm>
#include <cmath>

namespace dfn::world {

namespace {


} // namespace

inline constexpr float HOUSE_STAIR_RISE_M = 0.175f;
inline constexpr float HOUSE_STRINGER_BAND_M = 0.26f; ///< высота тетивы
inline constexpr float HOUSE_STRINGER_TH_M = 0.045f;  ///< толщина тетивы

void build_stairs(const Element& e, const ElementParams& p, glm::vec3 a, glm::vec3 b,
                         float half_w, MeshBuilder& mb, HouseMesh& mesh) {
    // Низ — тот якорь, что ниже: лестницу можно тянуть в обе стороны.
    if (b.y < a.y) {
        std::swap(a, b);
    }
    const glm::vec3 d = b - a;
    const float run = std::sqrt(d.x * d.x + d.z * d.z);
    const float rise_total = d.y;
    if (run < HOUSE_GEOM_EPS || rise_total < HOUSE_GEOM_EPS) {
        mesh.findings.push_back({e.id, MeshIssue::Degenerate, run,
                                 "лестнице нужны и длина, и высота: тяни к якорю выше"});
        return;
    }
    const glm::vec3 dir{d.x / run, 0.0f, d.z / run};
    const glm::vec3 side = glm::normalize(glm::cross(dir, glm::vec3{0.0f, 1.0f, 0.0f}));
    const int steps = std::max(1, static_cast<int>(std::round(rise_total / HOUSE_STAIR_RISE_M)));
    const float rise = rise_total / static_cast<float>(steps);
    const float tread = run / static_cast<float>(steps);
    const std::uint32_t quad[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < steps; ++i) {
        const glm::vec3 base = a + dir * (tread * static_cast<float>(i))
                             + glm::vec3{0.0f, rise * static_cast<float>(i), 0.0f};
        if (p.open > 1.5f) {
            // СТУПЕНЬ ИЗ КАМЕННЫХ БЛОКОВ (EXTERIOR_CATALOG.md, image copy 12):
            // 2-3 блока по ширине, зазор 2-4 см, каждый блок дышит высотой и
            // глубиной; при износе блок может ВЫВАЛИТЬСЯ (один из пяти).
            const int nblocks = half_w > 0.55f ? 3 : 2;
            const float gap_b = 0.03f;
            const float bw = (half_w * 2.0f - gap_b * (nblocks - 1))
                           / static_cast<float>(nblocks);
            const glm::vec3 top = base + glm::vec3{0.0f, rise, 0.0f};
            for (int bkk = 0; bkk < nblocks; ++bkk) {
                if (p.wear > 0.0f
                    && course_jitter(i * 29 + bkk * 7, 11) < p.wear * 0.2f) {
                    continue; // вывалившийся блок — читается провалом ступени
                }
                const float j_h = 0.012f * (course_jitter(i * 3 + bkk, 19) - 0.5f);
                const float j_d = 0.03f * course_jitter(bkk * 5 + 1, i * 7 + 2);
                const glm::vec3 s0 = top - side * half_w
                                   + side * ((bw + gap_b) * static_cast<float>(bkk))
                                   + glm::vec3{0.0f, j_h, 0.0f};
                const glm::vec3 loop[4] = {
                    s0 - dir * (0.02f + j_d), s0 + side * bw - dir * (0.02f + j_d),
                    s0 + side * bw + dir * (tread + 0.02f), s0 + dir * (tread + 0.02f)};
                glm::vec3 sunk[4];
                for (int k = 0; k < 4; ++k) {
                    sunk[k] = loop[k] - glm::vec3{0.0f, 0.16f, 0.0f};
                }
                push_prism(mb, sunk, quad, glm::vec3{0.0f, 0.16f, 0.0f},
                           p.tex_deg, mesh, e.id);
            }
            continue;
        }
        if (p.open > 0.5f) {
            // ОТКРЫТАЯ СТУПЕНЬ (20.08): доска на тетивах, под ней воздух.
            // Нос выступает на 3 см; износ грызёт края — ширина дышит.
            const float bite =
                p.wear * 0.06f * course_jitter(i * 13 + 7, static_cast<int>(half_w * 10));
            const float w0 = half_w - bite;
            const glm::vec3 top = base + glm::vec3{0.0f, rise, 0.0f};
            const glm::vec3 loop[4] = {top - side * w0 - dir * 0.03f,
                                       top + side * w0 - dir * 0.03f,
                                       top + side * w0 + dir * (tread + 0.02f),
                                       top - side * w0 + dir * (tread + 0.02f)};
            glm::vec3 sunk[4];
            for (int k = 0; k < 4; ++k) {
                sunk[k] = loop[k] - glm::vec3{0.0f, 0.05f, 0.0f};
            }
            push_prism(mb, sunk, quad, glm::vec3{0.0f, 0.05f, 0.0f}, p.tex_deg,
                       mesh, e.id);
            continue;
        }
        // Короб ступени: от её пола до её верха, глубиной в одну проступь.
        const glm::vec3 loop[4] = {base - side * half_w, base + side * half_w,
                                   base + side * half_w + dir * tread,
                                   base - side * half_w + dir * tread};
        push_prism(mb, loop, quad, glm::vec3{0.0f, rise, 0.0f}, p.tex_deg, mesh, e.id);
    }
    // ТЕТИВЫ: наклонные плиты по бокам, полосой вниз от линии марша.
    for (const float s_side : {-1.0f, 1.0f}) {
        const glm::vec3 off = side * (half_w * s_side);
        const glm::vec3 lo0 = a + off;
        const glm::vec3 hi0 = a + off + dir * run + glm::vec3{0.0f, rise_total, 0.0f};
        const glm::vec3 band{0.0f, -HOUSE_STRINGER_BAND_M, 0.0f};
        const glm::vec3 loop[4] = {lo0 + band, lo0, hi0, hi0 + band};
        // Наружу от марша, толщиной тетивы.
        glm::vec3 out_n = side * s_side;
        std::uint32_t tris[6] = {0, 1, 2, 0, 2, 3};
        if (s_side < 0.0f) {
            std::swap(tris[1], tris[2]);
            std::swap(tris[4], tris[5]);
        }
        push_prism(mb, loop, tris, out_n * HOUSE_STRINGER_TH_M, p.tex_deg, mesh, e.id);
    }
}

/// ЛЕСТНИЦА НА ЧЕТЫРЁХ ТОЧКАХ (правка 20.08: «лестница же на 4 точках
/// держится» — она деталь раздела стен, а не форма палки). Две нижние вершины
/// контура — ширина марша и его начало, две верхние — куда он приходит; всё
/// остальное считает тот же построитель, что и раньше: число ступеней из
/// высоты, ровный шаг, коробы-ступени в коллайдер, тетивы по бокам.
void build_stairs_contour(const Element& e, const ElementParams& p,
                                 std::span<const glm::vec3> pts, MeshBuilder& mb,
                                 HouseMesh& mesh) {
    if (pts.size() < 4) {
        mesh.findings.push_back({e.id, MeshIssue::Degenerate,
                                 static_cast<float>(pts.size()),
                                 "лестнице нужны четыре точки: две внизу, две наверху"});
        return;
    }
    std::vector<std::size_t> order(pts.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(),
              [&](std::size_t l, std::size_t r) { return pts[l].y < pts[r].y; });
    if (pts.size() != 4) {
        mesh.findings.push_back({e.id, MeshIssue::Degenerate,
                                 static_cast<float>(pts.size()),
                                 "лестница считает по четырём точкам: лишние не участвуют"});
    }
    const glm::vec3& lo0 = pts[order[0]];
    const glm::vec3& lo1 = pts[order[1]];
    const glm::vec3& hi0 = pts[order[order.size() - 1]];
    const glm::vec3& hi1 = pts[order[order.size() - 2]];
    const glm::vec3 a = (lo0 + lo1) * 0.5f;
    const glm::vec3 b = (hi0 + hi1) * 0.5f;
    const float half_w = std::max(glm::length(lo1 - lo0) * 0.5f, 0.15f);
    build_stairs(e, p, a, b, half_w, mb, mesh);
}


} // namespace dfn::world
