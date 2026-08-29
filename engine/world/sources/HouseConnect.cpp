/*
Module: engine/world
File: engine/world/sources/HouseConnect.cpp

Responsibility:
- СВЯЗНОСТЬ ПОСТРОЙКИ: граф контактов выпуклых тел коллайдера и его компоненты.
  Постройка обязана быть ОДНИМ островом, и остров, не держащий самое нижнее
  тело, — висящий ярус.

Key items:
- check_house_connectivity / island_name.

Dependencies:
- Uses: HouseMesh.h (ConvexPart, HouseIsland), glm.
- Used by: кузницы dfn_houses / dfn_furniture (отказ на выпечке), приёмка.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
- ПО ФАЙЛУ НА АЛГОРИТМ (решение пользователя 21.08): модуль держит ОДИН
  алгоритм постройки; общие руки — в HouseMeshDetail.h.
*/

#include "engine/world/sources/HouseMesh.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>

#include <glm/geometric.hpp>

namespace dfn::world {

namespace {

constexpr float INF_F = std::numeric_limits<float>::infinity();

/// Насколько далеко ищется «ближайшее чужое тело» для отчёта о висящем острове.
/// Это ДИАГНОСТИКА, а не правило: на ответ «связно / не связно» число не
/// влияет, оно только называет расстояние в сообщении.
constexpr float PROBE_M = 1.0f;

struct Aabb {
    glm::vec3 lo{INF_F, INF_F, INF_F};
    glm::vec3 hi{-INF_F, -INF_F, -INF_F};
    void add(const glm::vec3& p) {
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
};

/// Зазор между коробками: положительный — расходятся, отрицательный — лезут
/// друг в друга. Нижняя оценка настоящего расстояния, поэтому годится ТОЛЬКО
/// как быстрый отсев (коробка диагонального стропила огромна и пуста).
float aabb_gap(const Aabb& a, const Aabb& b) {
    float g = -INF_F;
    for (int k = 0; k < 3; ++k) {
        g = std::max(g, std::max(a.lo[k] - b.hi[k], b.lo[k] - a.hi[k]));
    }
    return g;
}

/// Зазор двух наборов точек вдоль оси n (не нормированной). Вырожденную ось
/// (векторное произведение почти параллельных рёбер) пропускаем: делить на
/// её длину значит умножать шум на миллион.
float axis_gap(std::span<const glm::vec3> a, std::span<const glm::vec3> b, glm::vec3 n) {
    const float len = glm::length(n);
    if (len < 1e-6f) {
        return -INF_F;
    }
    n /= len;
    float a_lo = INF_F, a_hi = -INF_F, b_lo = INF_F, b_hi = -INF_F;
    for (const glm::vec3& p : a) {
        const float d = glm::dot(p, n);
        a_lo = std::min(a_lo, d);
        a_hi = std::max(a_hi, d);
    }
    for (const glm::vec3& p : b) {
        const float d = glm::dot(p, n);
        b_lo = std::min(b_lo, d);
        b_hi = std::max(b_hi, d);
    }
    return std::max(a_lo - b_hi, b_lo - a_hi);
}

/// Оси-кандидаты треугольной призмы: нормаль основания, три нормали боковых
/// граней, четыре направления рёбер (три ребра основания и ось выдавливания).
/// Тела коллайдера рождаются ТОЛЬКО в push_prism (HouseBodies.cpp) и всегда
/// имеют ровно шесть точек в этом порядке — на другом числе точек честно
/// падаем на коробку, чтобы правило не соврало молча.
struct PrismAxes {
    glm::vec3 faces[4];
    glm::vec3 edges[4];
    bool valid = false;
};

PrismAxes prism_axes(std::span<const glm::vec3> p) {
    PrismAxes out;
    if (p.size() != 6) {
        return out;
    }
    const glm::vec3 e0 = p[1] - p[0];
    const glm::vec3 e1 = p[2] - p[1];
    const glm::vec3 e2 = p[0] - p[2];
    const glm::vec3 ax = p[3] - p[0];
    out.faces[0] = glm::cross(e0, -e2);
    out.faces[1] = glm::cross(e0, ax);
    out.faces[2] = glm::cross(e1, ax);
    out.faces[3] = glm::cross(e2, ax);
    out.edges[0] = e0;
    out.edges[1] = e1;
    out.edges[2] = e2;
    out.edges[3] = ax;
    out.valid = true;
    return out;
}

/// НАСТОЯЩИЙ зазор двух выпуклых тел — максимум по разделяющим осям. Для
/// выпуклых многогранников это точное расстояние всюду, кроме встречи
/// «вершина к вершине», где оценка занижена, то есть ошибается В СТОРОНУ
/// СВЯЗНОСТИ. Осей 24: по четыре грани с каждой стороны и шестнадцать
/// векторных произведений рёбер.
float prism_gap(std::span<const glm::vec3> a, const PrismAxes& aa,
                std::span<const glm::vec3> b, const PrismAxes& ba, float cutoff) {
    if (!aa.valid || !ba.valid) {
        return -INF_F; // не разобрать формой — считаем контактом, отсев уже был
    }
    float best = -INF_F;
    for (const glm::vec3& n : aa.faces) {
        best = std::max(best, axis_gap(a, b, n));
        if (best >= cutoff) {
            return best;
        }
    }
    for (const glm::vec3& n : ba.faces) {
        best = std::max(best, axis_gap(a, b, n));
        if (best >= cutoff) {
            return best;
        }
    }
    for (const glm::vec3& u : aa.edges) {
        for (const glm::vec3& v : ba.edges) {
            best = std::max(best, axis_gap(a, b, glm::cross(u, v)));
            if (best >= cutoff) {
                return best;
            }
        }
    }
    return best;
}

/// Система непересекающихся множеств со сжатием пути.
struct Dsu {
    std::vector<std::uint32_t> up;
    explicit Dsu(std::size_t n) : up(n) { std::iota(up.begin(), up.end(), 0u); }
    std::uint32_t find(std::uint32_t x) {
        while (up[x] != x) {
            up[x] = up[up[x]];
            x = up[x];
        }
        return x;
    }
    void join(std::uint32_t a, std::uint32_t b) {
        a = find(a);
        b = find(b);
        if (a != b) {
            up[b] = a;
        }
    }
};

} // namespace

HouseConnectivity check_house_connectivity(const HouseMesh& mesh, float tolerance_m) {
    HouseConnectivity out;
    out.tolerance_m = tolerance_m;
    out.bodies = mesh.convex.size();
    const std::size_t n = mesh.convex.size();
    if (n == 0) {
        return out;
    }

    std::vector<Aabb> box(n);
    std::vector<PrismAxes> axes(n);
    for (std::size_t i = 0; i < n; ++i) {
        for (const glm::vec3& p : mesh.convex[i].points) {
            box[i].add(p);
        }
        axes[i] = prism_axes(mesh.convex[i].points);
    }

    // ЗАМЕТАНИЕ ПО САМОЙ ДЛИННОЙ ОСИ. У постройки тела вытянуты вдоль стен, и
    // ось выбирается по РАЗБРОСУ центров, а не по габариту: у башни габарит
    // выше, чем шире, а тел больше по горизонтали.
    int sweep = 0;
    {
        glm::vec3 lo{INF_F, INF_F, INF_F};
        glm::vec3 hi{-INF_F, -INF_F, -INF_F};
        for (std::size_t i = 0; i < n; ++i) {
            const glm::vec3 c = (box[i].lo + box[i].hi) * 0.5f;
            lo = glm::min(lo, c);
            hi = glm::max(hi, c);
        }
        const glm::vec3 span = hi - lo;
        sweep = (span.y > span.x && span.y > span.z) ? 1 : (span.z > span.x ? 2 : 0);
    }
    std::vector<std::uint32_t> order(n);
    std::iota(order.begin(), order.end(), 0u);
    std::stable_sort(order.begin(), order.end(), [&](std::uint32_t a, std::uint32_t b) {
        return box[a].lo[sweep] < box[b].lo[sweep];
    });

    Dsu dsu(n);
    std::vector<std::uint32_t> active;
    for (const std::uint32_t i : order) {
        const float front = box[i].lo[sweep] - tolerance_m;
        std::size_t keep = 0;
        for (std::size_t k = 0; k < active.size(); ++k) {
            const std::uint32_t j = active[k];
            if (box[j].hi[sweep] < front) {
                continue; // ушёл за фронт — больше ни с кем не встретится
            }
            active[keep++] = j;
        }
        active.resize(keep);
        for (const std::uint32_t j : active) {
            if (dsu.find(i) == dsu.find(j)) {
                continue; // уже в одном острове — считать зазор незачем
            }
            if (aabb_gap(box[i], box[j]) >= tolerance_m) {
                continue;
            }
            const std::span<const glm::vec3> pa{mesh.convex[i].points};
            const std::span<const glm::vec3> pb{mesh.convex[j].points};
            if (prism_gap(pa, axes[i], pb, axes[j], tolerance_m) < tolerance_m) {
                dsu.join(i, j);
            }
        }
        active.push_back(i);
    }

    // Компоненты: корень -> номер острова, порядок первого появления по
    // возрастанию номера тела — сборка обязана быть детерминированной.
    std::vector<std::uint32_t> island_of(n, 0u);
    std::vector<std::uint32_t> roots;
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint32_t r = dsu.find(static_cast<std::uint32_t>(i));
        const auto it = std::find(roots.begin(), roots.end(), r);
        if (it == roots.end()) {
            island_of[i] = static_cast<std::uint32_t>(roots.size());
            roots.push_back(r);
        } else {
            island_of[i] = static_cast<std::uint32_t>(it - roots.begin());
        }
    }
    std::vector<HouseIsland> islands(roots.size());
    for (HouseIsland& is : islands) {
        is.y_lo = INF_F;
        is.y_hi = -INF_F;
        is.nearest_gap = INF_F;
    }
    std::uint32_t lowest = 0;
    for (std::size_t i = 0; i < n; ++i) {
        HouseIsland& is = islands[island_of[i]];
        is.bodies += 1;
        is.y_lo = std::min(is.y_lo, box[i].lo.y);
        is.y_hi = std::max(is.y_hi, box[i].hi.y);
        is.elements.push_back(mesh.convex[i].element);
        if (box[i].lo.y < box[lowest].lo.y) {
            lowest = static_cast<std::uint32_t>(i);
        }
    }
    for (HouseIsland& is : islands) {
        std::sort(is.elements.begin(), is.elements.end());
        is.elements.erase(std::unique(is.elements.begin(), is.elements.end()),
                          is.elements.end());
    }
    islands[island_of[lowest]].grounded = true;

    // ДИАГНОСТИКА ВИСЯЩИХ: до какого чужого тела ближе всего. Считается только
    // для незаземлённых островов и только внутри PROBE_M — это строка отчёта,
    // а не вход правила, и платить за неё полным перебором незачем.
    if (islands.size() > 1) {
        for (std::size_t i = 0; i < n; ++i) {
            HouseIsland& is = islands[island_of[i]];
            if (is.grounded) {
                continue;
            }
            for (std::size_t j = 0; j < n; ++j) {
                if (island_of[j] == island_of[i]) {
                    continue;
                }
                if (aabb_gap(box[i], box[j]) >= std::min(PROBE_M, is.nearest_gap)) {
                    continue;
                }
                const std::span<const glm::vec3> pa{mesh.convex[i].points};
                const std::span<const glm::vec3> pb{mesh.convex[j].points};
                const float g = prism_gap(pa, axes[i], pb, axes[j], PROBE_M);
                if (g < is.nearest_gap) {
                    is.nearest_gap = g;
                    is.nearest_element = mesh.convex[j].element;
                }
            }
        }
    }

    // Заземлённый первым, дальше по убыванию тел: читающему отчёт нужен
    // сначала дом, потом то, что от него отвалилось, крупным вперёд.
    std::stable_sort(islands.begin(), islands.end(),
                     [](const HouseIsland& a, const HouseIsland& b) {
                         if (a.grounded != b.grounded) {
                             return a.grounded;
                         }
                         return a.bodies > b.bodies;
                     });
    out.islands = std::move(islands);
    return out;
}

std::string island_name(const HouseIsland& island) {
    std::string s;
    for (std::size_t i = 0; i < island.elements.size();) {
        std::size_t j = i;
        while (j + 1 < island.elements.size()
               && island.elements[j + 1] == island.elements[j] + 1) {
            ++j;
        }
        if (!s.empty()) {
            s += ",";
        }
        s += "e" + std::to_string(island.elements[i]);
        if (j > i + 1) {
            s += "..e" + std::to_string(island.elements[j]);
        } else if (j == i + 1) {
            s += ",e" + std::to_string(island.elements[j]);
        }
        i = j + 1;
    }
    return s;
}

bool house_connect_refusal(const HouseGraph& g, std::string& why) {
    const HouseMesh mesh = build_house_mesh(g);
    const HouseConnectivity c = check_house_connectivity(mesh);
    if (c.ok()) {
        return false;
    }
    why = "постройка распалась на " + std::to_string(c.islands.size())
        + " островов (допуск контакта " + std::to_string(c.tolerance_m) + " м)";
    for (const HouseIsland& is : c.islands) {
        char tail[192];
        if (is.grounded) {
            std::snprintf(tail, sizeof(tail), " [земля, тел %zu, y %.3f..%.3f]", is.bodies,
                          static_cast<double>(is.y_lo), static_cast<double>(is.y_hi));
        } else if (is.nearest_element != NO_ELEMENT) {
            std::snprintf(tail, sizeof(tail),
                          " [ВИСИТ, тел %zu, y %.3f..%.3f, зазор %.3f м до e%u]", is.bodies,
                          static_cast<double>(is.y_lo), static_cast<double>(is.y_hi),
                          static_cast<double>(is.nearest_gap), is.nearest_element);
        } else {
            std::snprintf(tail, sizeof(tail), " [ВИСИТ, тел %zu, y %.3f..%.3f, вокруг пусто]",
                          is.bodies, static_cast<double>(is.y_lo),
                          static_cast<double>(is.y_hi));
        }
        why += "\n    " + island_name(is) + tail;
    }
    return true;
}

} // namespace dfn::world
