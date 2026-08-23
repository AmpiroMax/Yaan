/*
Created: 21:08:2026 - 00:40:00
Last updated: 23:08:2026 - 18:20:00
Module: engine/world
File: engine/world/sources/HouseRoof.cpp

Responsibility:
- КРОВЛЯ РЯДАМИ: дранка/черепица поперёк уклона с перевязкой, нахлёстом
  и дрожью; износ — только дрожь (дыра светила настилом).

Key items:
- build_roof_courses.

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
- 23:08:2026 - 02:01:25: подъём дранки += th*0.8*row — ряды перестали лежать на одной высоте (пронизывание 38% и лотерея глубины; заказ архитектора волны 23.08).
- 23:08:2026 - 18:20:00: подошва нижнего ряда — РОВНО на лице настила, дрожь топит кусок
  внутрь настила, а не поднимает над ним (судья связности: черепица храма и
  каменного дома висела полем в 0.021 м над скатом, отдельным островом).
*/

#include "engine/world/sources/HouseMeshDetail.h"

#include <algorithm>
#include <cmath>

namespace dfn::world {

namespace {


} // namespace

/// КРОВЛЯ РЯДАМИ (заказ 20.08: «приблизить к скайримским постройкам» — вид
/// держит крыша). Скат перестаёт быть плоской плитой: поверх настила ложатся
/// РЯДЫ отдельных дранок (fill=7) или черепиц (fill=8) поперёк уклона, с
/// перевязкой, нахлёстом ряда на ряд и глубинной дрожью — тем же ходом, что
/// объём кладки. Износ роняет отдельные куски и углубляет дрожь.
void build_roof_courses(const Element& e, const ElementParams& p,
                               const FittedPlane& plane,
                               std::span<const glm::vec2> flat, const glm::vec3& ax,
                               const glm::vec3& ay, MeshBuilder& mb, HouseMesh& mesh) {
    const bool tile = fill_kind(p) == WallFill::Tile;
    // Числа настоящие, строительные: дранка 0.18x0.45 с выпуском 0.28,
    // черепица 0.24x0.40 с выпуском 0.30. Толщина — видимая кромка куска.
    const float piece_w = tile ? 0.24f : 0.18f;
    const float piece_l = tile ? 0.40f : 0.45f;
    const float expose = tile ? 0.30f : 0.28f;
    const float th = tile ? 0.035f : 0.022f;

    // РЯДЫ — ПОПЕРЁК УКЛОНА: вниз по скату смотрит проекция мировой вертикали
    // на плоскость. Горизонтальному «скату» (навес без уклона) направление
    // безразлично — берётся ось u контура.
    const glm::vec3 down3 = glm::vec3{0.0f, -1.0f, 0.0f}
                          - plane.normal * glm::dot(plane.normal, glm::vec3{0.0f, -1.0f, 0.0f});
    glm::vec2 down{0.0f, 1.0f};
    if (glm::dot(down3, down3) > 1e-6f) {
        down = glm::normalize(glm::vec2{glm::dot(down3, ax), glm::dot(down3, ay)});
    }
    const glm::vec2 across{-down.y, down.x};

    // Контур в осях кровли (a вдоль ряда, d вниз по скату).
    std::vector<glm::vec2> rot;
    rot.reserve(flat.size());
    glm::vec2 lo{1e9f};
    glm::vec2 hi{-1e9f};
    for (const glm::vec2& f : flat) {
        const glm::vec2 q{glm::dot(f, across), glm::dot(f, down)};
        rot.push_back(q);
        lo = glm::min(lo, q);
        hi = glm::max(hi, q);
    }
    mb.set_material(tile ? 4 : 1, -1);
    int row = 0;
    // НИЖНИЙ РЯД — ПЕРВЫМ (от карниза вверх), каждый следующий ложится ВЫШЕ
    // по скату и ПОВЕРХ предыдущего: порядок укладки настоящей кровли.
    for (float v = hi.y - piece_l; v > lo.y - piece_l; v -= expose, ++row) {
        float u = lo.x + ((row % 2 == 0) ? 0.0f : -piece_w * 0.5f);
        for (int col = 0; u < hi.x; u += piece_w, ++col) {
            // Износ кровли — ТОЛЬКО дрожь: выпавший кусок открывал настил,
            // и заплатка чужого материала читалась багом, а не прорехой
            // (приёмка кадров 20.08). Прореха — работа отдельного слоя.
            const std::vector<glm::vec2> piece =
                clip_contour_to_rect(rot, {u, v}, {u + piece_w, v + piece_l});
            if (piece.size() < 3
                || std::abs(polygon_area_2d(piece)) < piece_w * piece_w * 0.2f) {
                continue;
            }
            const std::vector<std::uint32_t> tris = triangulate_contour(piece);
            if (tris.empty()) {
                continue;
            }
            // Ряд выше лежит поверх нижнего: подъём растёт с рядом, плюс
            // дрожь куска (износ её углубляет).
            // ЧЛЕН ПО НОМЕРУ РЯДА, которого формула не имела, хотя
            // комментарий выше это утверждал (архитектор, волна 23.08): без
            // него все ряды лежали НА ОДНОЙ высоте, дранка пронизывала
            // соседнюю на 38% длины, а нижняя грань каждой призмы выигрывала
            // лотерею глубины у верхней — скат читался чёрным.
            // ДРОЖЬ ИДЁТ ВНИЗ, В НАСТИЛ, А НЕ ВВЕРХ, ОТ НЕГО (судья связности
            // 23.08). Прежняя подошва лежала на th*0.6 ВЫШЕ лица настила: у
            // черепицы (th = 0.035) это 0.021 м воздуха под всем полем, и
            // судья честно называл кровлю храма и каменного дома ОТДЕЛЬНЫМ
            // ОСТРОВОМ — 1818 черепиц, не касающихся дома ничем. У дранки
            // (th = 0.022) те же 0.6 давали 0.013 и проскакивали под допуск,
            // то есть беда была одна, а видна на половине образцов.
            // Нижний ряд теперь садится подошвой РОВНО на лицо настила, а
            // дрожь топит кусок в настил на глубину до th — рельеф ряда
            // сохраняется (тот же размах), но ни один кусок не висит.
            const float lift = p.thickness * 0.5f
                             + th * 0.8f * static_cast<float>(row)
                             - th * (0.5f + 0.5f * p.wear) * course_jitter(row, col);
            std::vector<glm::vec3> loop;
            loop.reserve(piece.size());
            for (const glm::vec2& q : piece) {
                const glm::vec2 f = across * q.x + down * q.y; // назад в оси контура
                loop.push_back(plane.origin + ax * f.x + ay * f.y
                               + plane.normal * lift);
            }
            push_prism(mb, loop, tris, plane.normal * th, p.tex_deg, mesh, e.id);
        }
    }
    mb.set_material(-1, -1);
}


} // namespace dfn::world
