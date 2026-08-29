/*
Module: engine/world
File: engine/world/sources/HouseGeom.cpp

Responsibility:
- ГЕОМЕТРИЯ КОНТУРА: МНК-плоскость (Якоби 3x3), проекция, отсечение ушей
  (невыпуклые контуры), самопересечение, клип прямоугольником, кадры UV.

Key items:
- fit_contour_plane / project_contour / triangulate_contour /
  contour_self_intersects / clip_contour_to_rect / UvFrame::at.

Dependencies:
- Uses: HouseMeshDetail.h (общие руки), glm
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

/// Циклический метод Якоби для симметричной 3x3. Нужен ровно за одним: за
/// собственным вектором при НАИМЕНЬШЕМ собственном значении ковариации, то есть
/// за нормалью наилучшей по МНК плоскости.
void jacobi_eigen_3x3(float a[3][3], float evec[3][3], float eval[3]) {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            evec[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    for (int sweep = 0; sweep < 32; ++sweep) {
        const float off = a[0][1] * a[0][1] + a[0][2] * a[0][2] + a[1][2] * a[1][2];
        if (off < 1e-20f) {
            break;
        }
        for (int p = 0; p < 2; ++p) {
            for (int q = p + 1; q < 3; ++q) {
                if (std::fabs(a[p][q]) < 1e-20f) {
                    continue;
                }
                const float theta = (a[q][q] - a[p][p]) / (2.0f * a[p][q]);
                const float sign = theta >= 0.0f ? 1.0f : -1.0f;
                const float t = sign / (std::fabs(theta) + std::sqrt(theta * theta + 1.0f));
                const float c = 1.0f / std::sqrt(t * t + 1.0f);
                const float s = t * c;
                for (int k = 0; k < 3; ++k) {
                    const float akp = a[k][p];
                    const float akq = a[k][q];
                    a[k][p] = c * akp - s * akq;
                    a[k][q] = s * akp + c * akq;
                }
                for (int k = 0; k < 3; ++k) {
                    const float apk = a[p][k];
                    const float aqk = a[q][k];
                    a[p][k] = c * apk - s * aqk;
                    a[q][k] = s * apk + c * aqk;
                }
                for (int k = 0; k < 3; ++k) {
                    const float ekp = evec[k][p];
                    const float ekq = evec[k][q];
                    evec[k][p] = c * ekp - s * ekq;
                    evec[k][q] = s * ekp + c * ekq;
                }
            }
        }
    }
    for (int k = 0; k < 3; ++k) {
        eval[k] = a[k][k];
    }
}

/// Нормаль по Ньюэллу: площадно-взвешенная сумма по рёбрам. Она НЕ работает

float cross_2d(glm::vec2 a, glm::vec2 b) { return a.x * b.y - a.y * b.x; }

/// плоскостью (для кривого контура МНК точнее), но она единственная знает
/// ПОРЯДОК ОБХОДА, и потому решает знак.
glm::vec3 newell_normal(std::span<const glm::vec3> pts) {
    glm::vec3 n{0.0f};
    const std::size_t count = pts.size();
    for (std::size_t i = 0; i < count; ++i) {
        const glm::vec3& a = pts[i];
        const glm::vec3& b = pts[(i + 1) % count];
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    return n * 0.5f;
}

bool point_in_triangle(glm::vec2 p, glm::vec2 a, glm::vec2 b, glm::vec2 c) {
    const float d1 = cross_2d(b - a, p - a);
    const float d2 = cross_2d(c - b, p - b);
    const float d3 = cross_2d(a - c, p - c);
    const bool has_neg = d1 < 0.0f || d2 < 0.0f || d3 < 0.0f;
    const bool has_pos = d1 > 0.0f || d2 > 0.0f || d3 > 0.0f;
    return !(has_neg && has_pos);
}

bool segments_cross(glm::vec2 p1, glm::vec2 p2, glm::vec2 q1, glm::vec2 q2) {
    const float d1 = cross_2d(p2 - p1, q1 - p1);
    const float d2 = cross_2d(p2 - p1, q2 - p1);
    const float d3 = cross_2d(q2 - q1, p1 - q1);
    const float d4 = cross_2d(q2 - q1, p2 - q1);
    return ((d1 > 0.0f) != (d2 > 0.0f)) && ((d3 > 0.0f) != (d4 > 0.0f));
}

} // namespace

float polygon_area_2d(std::span<const glm::vec2> poly) {
    float acc = 0.0f;
    const std::size_t count = poly.size();
    for (std::size_t i = 0; i < count; ++i) {
        const glm::vec2& a = poly[i];
        const glm::vec2& b = poly[(i + 1) % count];
        acc += a.x * b.y - b.x * a.y;
    }
    return acc * 0.5f;
}

/// Ось, заведомо не параллельная данной. Выбор ДЕТЕРМИНИРОВАН и не зависит от
/// того, откуда смотрит камера: две сборки одного графа обязаны дать один меш.
glm::vec3 stable_reference_axis(glm::vec3 dir) {
    return std::fabs(dir.y) < 0.9f ? glm::vec3{0.0f, 1.0f, 0.0f} : glm::vec3{1.0f, 0.0f, 0.0f};
}

// -- сборка меша -----------------------------------------------------------

FittedPlane fit_contour_plane(std::span<const glm::vec3> pts) {
    FittedPlane out;
    if (pts.size() < 3) {
        return out;
    }
    glm::vec3 centroid{0.0f};
    for (const glm::vec3& p : pts) {
        centroid += p;
    }
    centroid /= static_cast<float>(pts.size());
    out.origin = centroid;

    float cov[3][3] = {};
    for (const glm::vec3& p : pts) {
        const glm::vec3 d = p - centroid;
        const float v[3] = {d.x, d.y, d.z};
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                cov[i][j] += v[i] * v[j];
            }
        }
    }
    float evec[3][3] = {};
    float eval[3] = {};
    jacobi_eigen_3x3(cov, evec, eval);
    int smallest = 0;
    for (int k = 1; k < 3; ++k) {
        if (eval[k] < eval[smallest]) {
            smallest = k;
        }
    }
    glm::vec3 n{evec[0][smallest], evec[1][smallest], evec[2][smallest]};
    const float n_len = glm::length(n);
    if (n_len < HOUSE_GEOM_EPS) {
        return out;
    }
    n /= n_len;

    // ЗНАК БЕРЁТСЯ У ПОРЯДКА ОБХОДА. МНК знает плоскость, но не знает, где у неё
    // лицо: собственный вектор определён с точностью до знака. Правило правой
    // руки (§2.3) — единственный источник ответа, и он же тот, который дизайнер
    // видит на экране, обходя контур.
    const glm::vec3 nw = newell_normal(pts);
    if (glm::length(nw) >= HOUSE_GEOM_EPS && glm::dot(n, nw) < 0.0f) {
        n = -n;
    }
    // Ньюэлл нулевой — у обхода нет стороны: вершины на одной прямой или контур
    // сложен бантиком, где заметённая площадь взаимно уничтожается. Знак тогда
    // остаётся тот, что дал МНК, и это НЕ подмена ответа: плоскость дальше
    // нужна ровно за тем, чтобы спроецировать контур и НАЗВАТЬ беду. Ранний
    // выход отсюда стоил бы разбора: бантик уходил бы в «нулевую площадь», а
    // «нулевая площадь» и «сам себя пересёк» чинятся по-разному.
    out.normal = n;

    for (const glm::vec3& p : pts) {
        out.max_deviation = std::max(out.max_deviation, std::fabs(glm::dot(p - centroid, n)));
    }
    for (std::size_t i = 0; i < pts.size(); ++i) {
        for (std::size_t j = i + 1; j < pts.size(); ++j) {
            out.span = std::max(out.span, glm::length(pts[j] - pts[i]));
        }
    }
    out.flatness = out.span > HOUSE_GEOM_EPS ? out.max_deviation / out.span : 0.0f;
    // Площадь считается по ПРОЕКЦИИ на найденную плоскость: это та самая
    // площадь, которую потом получат треугольники, а значит проверять её надо
    // ту же, а не ньюэллову.
    glm::vec3 u{0.0f};
    glm::vec3 v{0.0f};
    const std::vector<glm::vec2> flat = project_contour(pts, out, u, v);
    out.area = std::fabs(polygon_area_2d(flat));
    out.degenerate = out.area < HOUSE_GEOM_EPS;
    return out;
}

std::vector<glm::vec2> project_contour(std::span<const glm::vec3> pts, const FittedPlane& plane,
                                       glm::vec3& u_out, glm::vec3& v_out) {
    std::vector<glm::vec2> flat;
    if (pts.size() < 2) {
        return flat;
    }
    // Ось u вдоль ПЕРВОГО РЕБРА КОНТУРА. Не вдоль мировой оси: тогда развёртка
    // одного и того же пола зависела бы от того, как дом повёрнут, и типовой
    // дом получал бы разную текстуру в разных местах карты.
    glm::vec3 hint = pts[1] - pts[0];
    hint -= plane.normal * glm::dot(plane.normal, hint);
    if (glm::length(hint) < HOUSE_GEOM_EPS) {
        hint = stable_reference_axis(plane.normal);
        hint -= plane.normal * glm::dot(plane.normal, hint);
    }
    u_out = glm::normalize(hint);
    v_out = glm::cross(plane.normal, u_out);
    flat.reserve(pts.size());
    for (const glm::vec3& p : pts) {
        const glm::vec3 d = p - plane.origin;
        flat.push_back({glm::dot(d, u_out), glm::dot(d, v_out)});
    }
    return flat;
}

std::vector<std::uint32_t> triangulate_contour(std::span<const glm::vec2> poly) {
    std::vector<std::uint32_t> tris;
    const std::size_t count = poly.size();
    if (count < 3) {
        return tris;
    }
    // Работаем против часовой стрелки. Если контур пришёл по часовой, обходим
    // его задом наперёд и НЕ ТРОГАЕМ выходные имена: наружу идут индексы
    // исходного контура, иначе вызывающий получит чужую нумерацию.
    const bool ccw = polygon_area_2d(poly) >= 0.0f;
    std::vector<std::uint32_t> ring;
    ring.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        ring.push_back(static_cast<std::uint32_t>(ccw ? i : count - 1 - i));
    }

    std::size_t guard = 0;
    const std::size_t guard_max = count * count + 16;
    while (ring.size() > 3) {
        bool clipped = false;
        for (std::size_t i = 0; i < ring.size(); ++i) {
            const std::size_t ip = (i + ring.size() - 1) % ring.size();
            const std::size_t in = (i + 1) % ring.size();
            const glm::vec2 a = poly[ring[ip]];
            const glm::vec2 b = poly[ring[i]];
            const glm::vec2 c = poly[ring[in]];
            // Ухо обязано быть ВЫПУКЛЫМ углом...
            if (cross_2d(b - a, c - b) <= 0.0f) {
                continue;
            }
            // ...и ПУСТЫМ. Проверка на пустоту — ровно то, чем отсечение ушей
            // отличается от веера: без неё Г-образная комната получила бы
            // треугольник поперёк выемки, то есть пол там, где его нет.
            bool empty = true;
            for (std::size_t k = 0; k < ring.size() && empty; ++k) {
                if (k == i || k == ip || k == in) {
                    continue;
                }
                const std::size_t kp = (k + ring.size() - 1) % ring.size();
                const std::size_t kn = (k + 1) % ring.size();
                // Смотрим ТОЛЬКО на вогнутые вершины: выпуклая внутри уха
                // оказаться не может, а лишние проверки дают ложные отказы на
                // касающихся точках.
                if (cross_2d(poly[ring[k]] - poly[ring[kp]], poly[ring[kn]] - poly[ring[k]]) >
                    0.0f) {
                    continue;
                }
                if (point_in_triangle(poly[ring[k]], a, b, c)) {
                    empty = false;
                }
            }
            if (!empty) {
                continue;
            }
            tris.push_back(ring[ip]);
            tris.push_back(ring[i]);
            tris.push_back(ring[in]);
            ring.erase(ring.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
            break;
        }
        if (!clipped || ++guard > guard_max) {
            // Уши кончились раньше треугольников. Отдаём ПУСТО, а не половину:
            // половина триангуляции — это дыра в полу, которую никто не свяжет
            // с причиной.
            return {};
        }
    }
    tris.push_back(ring[0]);
    tris.push_back(ring[1]);
    tris.push_back(ring[2]);
    return tris;
}

bool contour_self_intersects(std::span<const glm::vec2> poly) {
    const std::size_t count = poly.size();
    if (count < 4) {
        return false;
    }
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t j = i + 1; j < count; ++j) {
            // Смежные рёбра делят вершину и «пересекаются» всегда — их пропуск
            // это определение, а не послабление.
            if ((j + 1) % count == i || (i + 1) % count == j) {
                continue;
            }
            if (segments_cross(poly[i], poly[(i + 1) % count], poly[j], poly[(j + 1) % count])) {
                return true;
            }
        }
    }
    return false;
}

glm::vec2 UvFrame::at(glm::vec3 p) const {
    const glm::vec3 d = p - origin;
    return {glm::dot(d, u), glm::dot(d, v)};
}

UvFrame make_uv_frame(glm::vec3 origin, glm::vec3 normal, glm::vec3 u_hint, float tex_deg) {
    UvFrame f;
    f.origin = origin;
    const float n_len = glm::length(normal);
    const glm::vec3 n = n_len > HOUSE_GEOM_EPS ? normal / n_len : glm::vec3{0.0f, 1.0f, 0.0f};
    glm::vec3 u0 = u_hint - n * glm::dot(n, u_hint);
    if (glm::length(u0) < HOUSE_GEOM_EPS) {
        u0 = stable_reference_axis(n);
        u0 -= n * glm::dot(n, u0);
    }
    u0 = glm::normalize(u0);
    const glm::vec3 v0 = glm::cross(n, u0);
    // ПОВОРОТ ВОКРУГ НОРМАЛИ. Оба орта остаются В ПЛОСКОСТИ грани и остаются
    // единичными, поэтому расстояние в развёртке равно расстоянию по
    // поверхности при ЛЮБОМ угле. Именно этим поворот вокруг нормали
    // отличается от поворота вокруг мировой вертикали с последующей проекцией:
    // тот на наклонной грани растягивает узор в 1/cos(наклон) раз.
    const float c = std::cos(tex_deg * HOUSE_DEG2RAD);
    const float s = std::sin(tex_deg * HOUSE_DEG2RAD);
    f.u = u0 * c + v0 * s;
    f.v = -u0 * s + v0 * c;
    return f;
}

/// Клип контура по прямоугольнику доски: Сазерленд–Ходжман, субъект может
/// быть НЕВЫПУКЛЫМ (Г-образный пол), отсекатель — четыре полуплоскости ячейки.
/// Многосвязный результат склеен мостиками нулевой площади — они дают
/// вырожденные треугольники и не видны ни глазом, ни коллайдером.
std::vector<glm::vec2> clip_contour_to_rect(std::span<const glm::vec2> poly,
                                                   glm::vec2 lo, glm::vec2 hi) {
    std::vector<glm::vec2> in(poly.begin(), poly.end());
    std::vector<glm::vec2> out;
    const auto pass = [&](auto inside, auto cross) {
        out.clear();
        for (std::size_t i = 0; i < in.size(); ++i) {
            const glm::vec2 a = in[i];
            const glm::vec2 b = in[(i + 1) % in.size()];
            const bool ia = inside(a);
            if (ia) {
                out.push_back(a);
            }
            if (ia != inside(b)) {
                out.push_back(cross(a, b));
            }
        }
        in = out;
    };
    const auto at_x = [](glm::vec2 a, glm::vec2 b, float x) {
        return glm::vec2{x, a.y + (b.y - a.y) * ((x - a.x) / (b.x - a.x))};
    };
    const auto at_y = [](glm::vec2 a, glm::vec2 b, float y) {
        return glm::vec2{a.x + (b.x - a.x) * ((y - a.y) / (b.y - a.y)), y};
    };
    pass([&](glm::vec2 q) { return q.x >= lo.x; },
         [&](glm::vec2 a, glm::vec2 b) { return at_x(a, b, lo.x); });
    if (in.empty()) return in;
    pass([&](glm::vec2 q) { return q.x <= hi.x; },
         [&](glm::vec2 a, glm::vec2 b) { return at_x(a, b, hi.x); });
    if (in.empty()) return in;
    pass([&](glm::vec2 q) { return q.y >= lo.y; },
         [&](glm::vec2 a, glm::vec2 b) { return at_y(a, b, lo.y); });
    if (in.empty()) return in;
    pass([&](glm::vec2 q) { return q.y <= hi.y; },
         [&](glm::vec2 a, glm::vec2 b) { return at_y(a, b, hi.y); });
    return in;
}

} // namespace dfn::world
