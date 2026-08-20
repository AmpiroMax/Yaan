/*
Created: 21:08:2026 - 00:40:00
Last updated: 21:08:2026 - 00:40:00
Module: engine/world
File: engine/world/sources/HousePlate.cpp

Responsibility:
- ПЛАСТИНА ЗАМКНУТОГО КОНТУРА: триангуляция в МНК-плоскости, симметричная
  толщина, разворот лица; маршрутизация покрытий (паркет, кровля, марш),
  потолочные балки.

Key items:
- build_contour_surface.

Dependencies:
- Uses: HouseMeshDetail.h, HouseParquet, HouseRoof, HouseStairs
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

void build_contour_surface(const Element& e, const ElementParams& p,
                           std::span<const glm::vec3> pts, MeshBuilder& mb, HouseMesh& mesh) {
    const FittedPlane plane = fit_contour_plane(pts);
    // САМОПЕРЕСЕЧЕНИЕ ПРОВЕРЯЕТСЯ ПЕРВЫМ, до отказа по вырождению, и порядок
    // здесь — часть ответа: у бантика заметённая площадь ровно ноль, поэтому
    // обратный порядок назвал бы его «контуром нулевой площади» и отправил
    // дизайнера искать совпавшие вершины, которых нет.
    glm::vec3 u{0.0f};
    glm::vec3 v{0.0f};
    const std::vector<glm::vec2> flat = project_contour(pts, plane, u, v);
    if (contour_self_intersects(flat)) {
        mesh.findings.push_back(
            {e.id, MeshIssue::ContourSelfIntersects, 0.0f, "контур самопересекается"});
    }
    if (plane.degenerate) {
        mesh.findings.push_back(
            {e.id, MeshIssue::Degenerate, plane.area, "у контура нулевая площадь"});
        return;
    }
    if (plane.flatness > HOUSE_CONTOUR_FLATNESS_MAX) {
        // ГОВОРИТСЯ, НО НЕ ЗАПРЕЩАЕТСЯ. Дизайнер посреди правки обязан видеть
        // то, что сделал; отказ строить превратил бы промах по якорю в
        // исчезнувший пол, а исчезнувший пол объясняет причину хуже, чем
        // сложенный пополам.
        mesh.findings.push_back({e.id, MeshIssue::ContourNonPlanar, plane.flatness,
                                 "контур слишком кривой для наклонной поверхности"});
    }
    // ЛЕСТНИЦА ЗАМЕЩАЕТ ПЛАСТИНУ: контур с fill=6 — это марш, а не плита с
    // маршем поверх; наклонная плита под ступенями лишь спорила бы с их
    // коллайдером.
    if (fill_kind(p) == WallFill::Stairs) {
        build_stairs_contour(e, p, pts, mb, mesh);
        return;
    }
    const std::vector<std::uint32_t> tris = triangulate_contour(flat);
    if (tris.empty()) {
        mesh.findings.push_back({e.id, MeshIssue::TriangulationFailed, 0.0f,
                                 "уши кончились раньше треугольников"});
        return;
    }
    // ЛИЦО — по обходу, переключатель разворачивает (§2.3).
    const glm::vec3 n = e.facing_flipped ? -plane.normal : plane.normal;
    const float half = std::max(p.thickness, HOUSE_GEOM_EPS) * 0.5f;

    // ТОЛЩИНА СИММЕТРИЧНА: срединная плоскость проходит ровно по якорям, как
    // тело прямой сидит вокруг оси, а не сбоку от неё. Одно правило на два
    // вида, и разворот лица не двигает ни одной точки.
    std::vector<glm::vec3> loop;
    loop.reserve(pts.size());
    std::vector<std::uint32_t> use = tris;
    if (e.facing_flipped) {
        // Кольцо обязано обходиться против часовой стрелки со стороны +n. При
        // развороте лица переворачивается и порядок, и нумерация в тройках.
        for (std::size_t i = pts.size(); i-- > 0;) {
            loop.push_back(pts[i] - n * half);
        }
        const std::uint32_t last = static_cast<std::uint32_t>(pts.size() - 1);
        for (std::size_t t = 0; t + 2 < use.size(); t += 3) {
            const std::uint32_t x = last - use[t];
            const std::uint32_t y = last - use[t + 1];
            const std::uint32_t z = last - use[t + 2];
            use[t] = z;
            use[t + 1] = y;
            use[t + 2] = x;
        }
    } else {
        for (const glm::vec3& q : pts) {
            loop.push_back(q - n * half);
        }
    }
    push_prism(mb, loop, use, n * (half * 2.0f), p.tex_deg, mesh, e.id);
    if (fill_kind(p) == WallFill::Parquet) {
        build_parquet(e, p, plane, flat, u, v, mb, mesh);
    }
    if (fill_kind(p) == WallFill::Shingle || fill_kind(p) == WallFill::Tile) {
        build_roof_courses(e, p, plane, flat, u, v, mb, mesh);
    }
    // ПОТОЛОЧНЫЕ БАЛКИ (beams=1): брусья под ИЗНАНКОЙ пластины вдоль
    // короткой оси bbox, шаг ~1.4 м, режутся контуром — как паркет, только
    // редкие, толстые и снизу (INTERIOR_CATALOG.md: балка 0.2x0.25, шаг
    // 1.2-1.6, встреча со стеной — врезкой, что нахлёст и даёт).
    if (p.beams > 0.5f) {
        glm::vec2 blo = flat.front();
        glm::vec2 bhi = blo;
        for (const glm::vec2& f : flat) {
            blo = glm::min(blo, f);
            bhi = glm::max(bhi, f);
        }
        const bool along_u = (bhi.x - blo.x) < (bhi.y - blo.y);
        const float span_c = along_u ? (bhi.y - blo.y) : (bhi.x - blo.x);
        const int nb = std::max(1, static_cast<int>(span_c / 1.4f));
        mb.set_material(0, 2); // тёмный тёсаный брус
        for (int bi = 0; bi < nb; ++bi) {
            const float c = (along_u ? blo.y : blo.x)
                          + span_c * (static_cast<float>(bi) + 0.5f)
                                / static_cast<float>(nb);
            const glm::vec2 rlo = along_u ? glm::vec2{blo.x, c - 0.1f}
                                          : glm::vec2{c - 0.1f, blo.y};
            const glm::vec2 rhi = along_u ? glm::vec2{bhi.x, c + 0.1f}
                                          : glm::vec2{c + 0.1f, bhi.y};
            const std::vector<glm::vec2> strip = clip_contour_to_rect(flat, rlo, rhi);
            if (strip.size() < 3) {
                continue;
            }
            const std::vector<std::uint32_t> btris = triangulate_contour(strip);
            if (btris.empty()) {
                continue;
            }
            std::vector<glm::vec3> loop2;
            loop2.reserve(strip.size());
            // Под изнанкой: от нижней грани пластины вниз на 0.25.
            const glm::vec3 drop = n * (half + 0.25f);
            for (const glm::vec2& q : strip) {
                loop2.push_back(plane.origin + u * q.x + v * q.y - drop);
            }
            push_prism(mb, loop2, btris, n * 0.25f, p.tex_deg, mesh, e.id);
        }
        mb.set_material(-1, -1);
    }
}

// ---------------------------------------------------------------------------
// Обшивка по раскладке (HouseStyle): доски, раскосы, рамы проёмов
// ---------------------------------------------------------------------------


} // namespace dfn::world
