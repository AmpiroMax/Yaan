/*
Module: engine/world
File: engine/world/sources/HouseParquet.cpp

Responsibility:
- ПАРКЕТ ПОЛА: доски рядами с перевязкой, рез по контуру, ветхость
  (неровные ряды, щели, обломки, выпавшие доски).

Key items:
- build_parquet.

Dependencies:
- Uses: HouseMeshDetail.h (clip_contour_to_rect, course_jitter)
- Used by: сборка build_house_mesh (HouseMesh.cpp) и соседние модули постройки.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
- ПО ФАЙЛУ НА АЛГОРИТМ (решение пользователя 21.08): модуль держит ОДИН
  алгоритм постройки; общие руки — в HouseMeshDetail.h.
*/

#include "engine/world/sources/HouseMeshDetail.h"

#include <algorithm>
#include <cmath>

namespace dfn::world {

namespace {

} // namespace

/// ПАРКЕТ ПОЛА (заказ 20.08: «деревянных как ламинат и паркет полов нет,
/// только срезы»): доски рядами по плоскости контура, с перевязкой, как у
/// кладки. Правки 20.08 по игре: доска РЕЖЕТСЯ по кромке контура (раньше
/// торчала наружу), доски лежат ВСТЫК — щелей нет, а читаются они глубинной
/// дрожью, тем же ходом, что даёт объём кладке.
void build_parquet(const Element& e, const ElementParams& p,
                          const FittedPlane& plane,
                          std::span<const glm::vec2> flat, const glm::vec3& ax,
                          const glm::vec3& ay, MeshBuilder& mb, HouseMesh& mesh) {
    const float plank_l = 1.2f;
    const float th = 0.018f;
    const float jig = 0.002f; ///< дрожь высоты доски: рельеф вместо щелей
    glm::vec2 lo = flat.front();
    glm::vec2 hi = lo;
    for (const glm::vec2& f : flat) {
        lo = glm::min(lo, f);
        hi = glm::max(hi, f);
    }
    mb.set_material(1, -1); // пилёная доска, тон элементный
    int row = 0;
    // ВЕТХОСТЬ (20.08: «доски неровные по ширине, где-то щели, обломки»):
    // износ делает ширину ряда СВОЕЙ (0.13..0.22), открывает постоянные щели
    // между рядами, роняет отдельные куски (дыры до плиты) и обламывает
    // концы досок. Всё по хэшу — сборка детерминирована.
    for (float v = lo.y; v < hi.y; ++row) {
        const float plank_w =
            0.16f + p.wear * (course_jitter(row * 3 + 1, 17) - 0.5f) * 0.12f;
        const float row_gap = p.wear * (0.004f + 0.014f * course_jitter(row, 29));
        float u = lo.x + ((row % 2 == 0) ? 0.0f : -plank_l * 0.5f);
        for (int col = 0; u < hi.x; ++col) {
            // Обломок: конец доски съеден на треть-половину.
            float len = plank_l;
            if (p.wear > 0.0f && course_jitter(row * 7 + 2, col * 5 + 3) < p.wear * 0.18f) {
                len *= 0.45f + 0.3f * course_jitter(col, row);
            }
            // Дыра: доска выпала целиком, виден настил.
            if (p.wear > 0.0f && course_jitter(row * 11 + 4, col * 7 + 6) < p.wear * 0.07f) {
                u += plank_l;
                continue;
            }
            const std::vector<glm::vec2> piece =
                clip_contour_to_rect(flat, {u, v}, {u + len, v + plank_w});
            if (piece.size() < 3
                || std::abs(polygon_area_2d(piece)) < plank_w * plank_w * 0.25f) {
                u += plank_l; // шаг в хвосте: continue без шага вертел бы цикл вечно
                continue;
            }
            const std::vector<std::uint32_t> tris = triangulate_contour(piece);
            if (tris.empty()) {
                u += plank_l;
                continue;
            }
            // Поверх лицевой стороны пластины: половина толщины + своя дрожь.
            const glm::vec3 lift =
                plane.normal * (p.thickness * 0.5f + jig * course_jitter(row, col));
            std::vector<glm::vec3> loop;
            loop.reserve(piece.size());
            for (const glm::vec2& q : piece) {
                loop.push_back(plane.origin + ax * q.x + ay * q.y + lift);
            }
            // ФАЗА ПЛИТКИ — СВОЯ У КАЖДОЙ ДОСКИ (владелец 23.08: «деревянный
            // пол всё ещё из одинаковых тайликов, все дощечки как клоны»).
            // Рамка uv центрируется на грани, поэтому без сдвига любая доска
            // резала плитку одинаково. Иррациональные шаги — чтобы фазы не
            // складывались в период по ряду.
            const glm::vec2 phase{course_jitter(row * 13 + 5, col * 17 + 7) * 0.731f,
                                  course_jitter(row * 19 + 11, col * 23 + 3) * 0.517f};
            push_prism(mb, loop, tris, plane.normal * th, p.tex_deg, mesh, e.id,
                       phase);
            u += plank_l;
        }
        v += plank_w + row_gap;
    }
    mb.set_material(-1, -1);
}

} // namespace dfn::world
