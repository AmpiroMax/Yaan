/*
Module: engine/render
File: engine/render/sources/MorphFollow.cpp

Responsibility:
- Реализация переноса дельт тела на части. См. MorphFollow.h — там довод.

Dependencies:
- Uses: MorphFollow.h, MorphBlend.h (нормали разницей), glm.
- Used by: dfn_render.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ПОИСК СОСЕДЕЙ ТОЧНЫЙ. Сетка — ускоритель; вершина, которой сетка «не
  нашла» соседа, получила бы карту из нулей и молча отстала бы от тела — ровно
  тот тихий брак, ради которого прибор follow_gap_change и существует.
*/

#include "engine/render/sources/MorphFollow.h"

#include "engine/render/sources/MorphBlend.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>

namespace dfn::render {
namespace {

/// РАВНОМЕРНАЯ СЕТКА ПО ВЕРШИНАМ ТЕЛА: ключ ячейки — три целых, значения —
/// номера вершин. Поиск идёт кольцами ячеек от точки, пока найдены k соседей и
/// следующее кольцо не может лежать ближе k-го найденного.
struct Grid {
    float cell = 0.02f;
    glm::vec3 origin{0.0f};
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> cells;

    [[nodiscard]] static std::uint64_t key(int x, int y, int z) {
        const auto u = [](int v) {
            return static_cast<std::uint64_t>(static_cast<std::uint32_t>(v + (1 << 20))
                                              & 0x1FFFFFu);
        };
        return (u(x) << 42) | (u(y) << 21) | u(z);
    }
    void coords(const glm::vec3& p, int& x, int& y, int& z) const {
        x = static_cast<int>(std::floor((p.x - origin.x) / cell));
        y = static_cast<int>(std::floor((p.y - origin.y) / cell));
        z = static_cast<int>(std::floor((p.z - origin.z) / cell));
    }
    void build(std::span<const platform::SkinnedVertex> body, float cell_m) {
        cell = cell_m;
        glm::vec3 lo{std::numeric_limits<float>::max()};
        for (const platform::SkinnedVertex& v : body) {
            lo = glm::min(lo, v.position);
        }
        origin = lo;
        cells.clear();
        cells.reserve(body.size() / 2 + 1);
        for (std::size_t i = 0; i < body.size(); ++i) {
            int x = 0;
            int y = 0;
            int z = 0;
            coords(body[i].position, x, y, z);
            cells[key(x, y, z)].push_back(static_cast<std::uint32_t>(i));
        }
    }
};

struct Candidate {
    float d2 = std::numeric_limits<float>::max();
    std::uint32_t index = 0;
};

/// k БЛИЖАЙШИХ К `p` — точный ответ, кольцами.
void nearest_k(const Grid& grid, std::span<const platform::SkinnedVertex> body,
               const glm::vec3& p, Candidate (&best)[FOLLOW_K]) {
    for (Candidate& c : best) {
        c = Candidate{};
    }
    const auto offer = [&best](float d2, std::uint32_t idx) {
        if (d2 >= best[FOLLOW_K - 1].d2) {
            return;
        }
        std::size_t at = FOLLOW_K - 1;
        while (at > 0 && best[at - 1].d2 > d2) {
            best[at] = best[at - 1];
            --at;
        }
        best[at] = Candidate{d2, idx};
    };
    int cx = 0;
    int cy = 0;
    int cz = 0;
    grid.coords(p, cx, cy, cz);
    // Дальше 64 колец (1.28 м при ячейке 2 см) соседа не бывает у части,
    // прикреплённой к телу; остановка на этом — от вечного цикла на пустом теле.
    for (int ring = 0; ring < 64; ++ring) {
        // Всё внутри кольца ring уже перебрано; ближайшая точка кольца ring+1
        // лежит не ближе ring·cell, — если k-й найденный ближе, ответ полный.
        if (best[FOLLOW_K - 1].d2 < std::numeric_limits<float>::max()) {
            const float bound = static_cast<float>(ring) * grid.cell;
            if (bound * bound >= best[FOLLOW_K - 1].d2) {
                return;
            }
        }
        for (int dx = -ring; dx <= ring; ++dx) {
            for (int dy = -ring; dy <= ring; ++dy) {
                for (int dz = -ring; dz <= ring; ++dz) {
                    if (std::max({std::abs(dx), std::abs(dy), std::abs(dz)}) != ring) {
                        continue; // только оболочка кольца
                    }
                    const auto it = grid.cells.find(Grid::key(cx + dx, cy + dy, cz + dz));
                    if (it == grid.cells.end()) {
                        continue;
                    }
                    for (const std::uint32_t idx : it->second) {
                        const glm::vec3 d = body[idx].position - p;
                        offer(glm::dot(d, d), idx);
                    }
                }
            }
        }
    }
}

/// ГРАНИ ПРИ ВЕРШИНЕ: список номеров треугольников на каждую вершину тела.
void incident_triangles(std::size_t vertices, std::span<const std::uint32_t> indices,
                        std::vector<std::vector<std::uint32_t>>& out) {
    out.assign(vertices, {});
    for (std::size_t t = 0; t + 2 < indices.size(); t += 3) {
        for (std::size_t k = 0; k < 3; ++k) {
            if (indices[t + k] < vertices) {
                out[indices[t + k]].push_back(static_cast<std::uint32_t>(t / 3));
            }
        }
    }
}

/// БЛИЖАЙШАЯ ТОЧКА ТРЕУГОЛЬНИКА к `p` (Ericson, Real-Time Collision Detection
/// 5.1.5) и её барицентрика (u по ab, v по ac).
glm::vec3 closest_on_triangle(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b,
                              const glm::vec3& c, float& u, float& v) {
    const glm::vec3 ab = b - a;
    const glm::vec3 ac = c - a;
    const glm::vec3 ap = p - a;
    const float d1 = glm::dot(ab, ap);
    const float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) {
        u = 0.0f;
        v = 0.0f;
        return a;
    }
    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp);
    const float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) {
        u = 1.0f;
        v = 0.0f;
        return b;
    }
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        u = d1 / (d1 - d3);
        v = 0.0f;
        return a + ab * u;
    }
    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp);
    const float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) {
        u = 0.0f;
        v = 1.0f;
        return c;
    }
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        u = 0.0f;
        v = d2 / (d2 - d6);
        return a + ac * v;
    }
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        u = 1.0f - w;
        v = w;
        return b + (c - b) * w;
    }
    const float denom = 1.0f / (va + vb + vc);
    u = vb * denom;
    v = vc * denom;
    return a + ab * u + ac * v;
}

/// РАМКА ТРЕУГОЛЬНИКА: t̂ вдоль ab, n̂ — нормаль, b̂ = n̂ × t̂. False —
/// вырожденный (нулевая площадь).
bool triangle_frame(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                    glm::vec3& t, glm::vec3& bt, glm::vec3& n) {
    const glm::vec3 ab = b - a;
    const glm::vec3 cross = glm::cross(ab, c - a);
    const float area2 = glm::length(cross);
    const float ab_len = glm::length(ab);
    if (area2 < 1e-12f || ab_len < 1e-6f) {
        return false;
    }
    n = cross / area2;
    t = ab / ab_len;
    bt = glm::cross(n, t);
    return true;
}

/// Вершины треугольника `tri` тела — false, если номер вне списка.
bool triangle_corners(std::span<const platform::SkinnedVertex> body,
                      std::span<const std::uint32_t> indices, std::uint32_t tri,
                      glm::vec3& a, glm::vec3& b, glm::vec3& c) {
    const std::size_t at = static_cast<std::size_t>(tri) * 3;
    if (at + 2 >= indices.size()) {
        return false;
    }
    const std::uint32_t ia = indices[at];
    const std::uint32_t ib = indices[at + 1];
    const std::uint32_t ic = indices[at + 2];
    if (ia >= body.size() || ib >= body.size() || ic >= body.size()) {
        return false;
    }
    a = body[ia].position;
    b = body[ib].position;
    c = body[ic].position;
    return true;
}

/// Знаковое расстояние `p` до ближайшего треугольника среди граней опорных
/// вершин `bind` (знак — по нормали грани). Нет граней — +∞ (не под кожей).
float signed_surface_distance(std::span<const platform::SkinnedVertex> body,
                              std::span<const std::uint32_t> indices,
                              const std::vector<std::vector<std::uint32_t>>& incident,
                              const FollowBind& bind, const glm::vec3& p) {
    float best = std::numeric_limits<float>::max();
    float sign = 1.0f;
    for (std::uint32_t k = 0; k < FOLLOW_K; ++k) {
        if (k > 0 && bind.weight[k] == 0.0f) {
            continue;
        }
        if (bind.index[k] >= incident.size()) {
            continue;
        }
        for (const std::uint32_t tri : incident[bind.index[k]]) {
            glm::vec3 a;
            glm::vec3 b;
            glm::vec3 c;
            if (!triangle_corners(body, indices, tri, a, b, c)) {
                continue;
            }
            float u = 0.0f;
            float v = 0.0f;
            const glm::vec3 q = closest_on_triangle(p, a, b, c, u, v);
            const float d = glm::length(p - q);
            if (d < best) {
                best = d;
                sign = glm::dot(p - q, glm::cross(b - a, c - a)) >= 0.0f ? 1.0f : -1.0f;
            }
        }
    }
    return best * sign;
}

} // namespace

void build_follow_map(std::span<const platform::SkinnedVertex> body,
                      std::span<const std::uint32_t> body_indices,
                      std::span<const platform::SkinnedVertex> part, FollowMap& out,
                      float power, float cell_m) {
    out.binds.clear();
    out.surface.clear();
    if (body.size() < FOLLOW_K || part.empty()) {
        return;
    }
    Grid grid;
    grid.build(body, cell_m > 1e-4f ? cell_m : 0.02f);
    std::vector<std::vector<std::uint32_t>> incident;
    incident_triangles(body.size(), body_indices, incident);
    out.binds.resize(part.size());
    out.surface.resize(part.size());
    for (std::size_t i = 0; i < part.size(); ++i) {
        Candidate best[FOLLOW_K];
        nearest_k(grid, body, part[i].position, best);
        FollowBind& b = out.binds[i];
        // Совпавшая вершина (шов, дублированная вершина) берёт всё: обратное
        // расстояние там бесконечно, и именно это и означает «та же точка».
        if (best[0].d2 < 1e-12f) {
            b.index[0] = best[0].index;
            b.weight[0] = 1.0f;
            for (std::uint32_t k = 1; k < FOLLOW_K; ++k) {
                b.index[k] = best[0].index;
                b.weight[k] = 0.0f;
            }
        } else {
            float sum = 0.0f;
            for (std::uint32_t k = 0; k < FOLLOW_K; ++k) {
                if (best[k].d2 == std::numeric_limits<float>::max()) {
                    b.index[k] = best[0].index;
                    b.weight[k] = 0.0f;
                    continue;
                }
                b.index[k] = best[k].index;
                b.weight[k] = 1.0f / std::pow(std::sqrt(best[k].d2), power);
                sum += b.weight[k];
            }
            for (float& w : b.weight) {
                w /= sum;
            }
        }
        // ШОВ К КОЖЕ: ближайший треугольник среди вееров опорных вершин — та
        // самая точка кожи, над которой вершина стоит в ресте.
        SurfaceBind& s = out.surface[i];
        float best_d = std::numeric_limits<float>::max();
        for (std::uint32_t k = 0; k < FOLLOW_K; ++k) {
            if (k > 0 && b.weight[k] == 0.0f) {
                continue;
            }
            for (const std::uint32_t tri : incident[b.index[k]]) {
                glm::vec3 a;
                glm::vec3 bb;
                glm::vec3 c;
                if (!triangle_corners(body, body_indices, tri, a, bb, c)) {
                    continue;
                }
                float u = 0.0f;
                float v = 0.0f;
                const glm::vec3 q = closest_on_triangle(part[i].position, a, bb, c, u, v);
                const float d = glm::length(part[i].position - q);
                glm::vec3 t;
                glm::vec3 bt;
                glm::vec3 n;
                if (d < best_d && triangle_frame(a, bb, c, t, bt, n)) {
                    best_d = d;
                    s.tri = tri;
                    s.u = u;
                    s.v = v;
                    const glm::vec3 o = part[i].position - q;
                    s.local = glm::vec3{glm::dot(o, t), glm::dot(o, bt), glm::dot(o, n)};
                }
            }
        }
    }
}

void apply_follow(std::span<const platform::SkinnedVertex> body_rest,
                  std::span<const std::uint32_t> body_indices,
                  std::span<const platform::SkinnedVertex> body_now,
                  const FollowMap& map,
                  std::span<const platform::SkinnedVertex> part_rest,
                  std::span<const std::uint32_t> indices,
                  std::vector<platform::SkinnedVertex>& out) {
    out.assign(part_rest.begin(), part_rest.end());
    if (map.binds.size() != part_rest.size() || body_now.size() != body_rest.size()) {
        return;
    }
    const bool sewn = map.surface.size() == part_rest.size();
    bool moved = false;
    for (std::size_t i = 0; i < out.size(); ++i) {
        glm::vec3 target = part_rest[i].position;
        bool placed = false;
        if (sewn && map.surface[i].tri != FOLLOW_NO_TRIANGLE) {
            const SurfaceBind& s = map.surface[i];
            glm::vec3 a0;
            glm::vec3 b0;
            glm::vec3 c0;
            glm::vec3 a1;
            glm::vec3 b1;
            glm::vec3 c1;
            if (triangle_corners(body_rest, body_indices, s.tri, a0, b0, c0)
                && triangle_corners(body_now, body_indices, s.tri, a1, b1, c1)) {
                placed = true;
                // Грань не сдвинулась — вершина стоит ПОБИТОВО (нулевые веса
                // обязаны дать рест, а не рест ± ulp).
                if (a0 != a1 || b0 != b1 || c0 != c1) {
                    const glm::vec3 q0 = a0 + (b0 - a0) * s.u + (c0 - a0) * s.v;
                    const glm::vec3 q1 = a1 + (b1 - a1) * s.u + (c1 - a1) * s.v;
                    const glm::vec3 o0 = part_rest[i].position - q0;
                    glm::vec3 o1 = o0;
                    if (FOLLOW_FRAME_ROTATES) {
                        glm::vec3 t1;
                        glm::vec3 bt1;
                        glm::vec3 n1;
                        if (triangle_frame(a1, b1, c1, t1, bt1, n1)) {
                            const float len = glm::length(o0);
                            o1 = t1 * s.local.x + bt1 * s.local.y + n1 * s.local.z;
                            // Далёкая вершина не крутится с гранью: плавно к переносу.
                            if (len > FOLLOW_FRAME_NEAR_M) {
                                const float k =
                                    std::clamp((FOLLOW_FRAME_FAR_M - len)
                                                   / (FOLLOW_FRAME_FAR_M - FOLLOW_FRAME_NEAR_M),
                                               0.0f, 1.0f);
                                const glm::vec3 mixed = o0 * (1.0f - k) + o1 * k;
                                const float ml = glm::length(mixed);
                                o1 = ml > 1e-9f ? mixed * (len / ml) : o0;
                            }
                        }
                    }
                    target = q1 + o1;
                }
            }
        }
        if (!placed) {
            // Запасной перенос: взвешенная сумма дельт опорных вершин.
            const FollowBind& b = map.binds[i];
            glm::vec3 shift{0.0f};
            for (std::uint32_t k = 0; k < FOLLOW_K; ++k) {
                if (b.weight[k] == 0.0f || b.index[k] >= body_rest.size()) {
                    continue;
                }
                shift += (body_now[b.index[k]].position - body_rest[b.index[k]].position)
                         * b.weight[k];
            }
            target = part_rest[i].position + shift;
        }
        if (target != part_rest[i].position) {
            out[i].position = target;
            moved = true;
        }
    }
    if (moved) {
        shift_normals_by_difference(part_rest, indices, out);
    }
}

RigidFrame rigid_frame(std::span<const platform::SkinnedVertex> body,
                       std::span<const std::uint32_t> mask) {
    RigidFrame f;
    std::size_t n = 0;
    glm::vec3 sum{0.0f};
    for (const std::uint32_t i : mask) {
        if (i < body.size()) {
            sum += body[i].position;
            ++n;
        }
    }
    if (n == 0) {
        return f;
    }
    f.centroid = sum / static_cast<float>(n);
    float r2 = 0.0f;
    for (const std::uint32_t i : mask) {
        if (i < body.size()) {
            const glm::vec3 d = body[i].position - f.centroid;
            r2 += glm::dot(d, d);
        }
    }
    f.radius = std::sqrt(r2 / static_cast<float>(n));
    return f;
}

void apply_rigid(const RigidFrame& rest, const RigidFrame& now, bool scale,
                 std::span<const std::uint32_t> subset,
                 std::vector<platform::SkinnedVertex>& out) {
    const float s = scale && rest.radius > 1e-6f ? now.radius / rest.radius : 1.0f;
    if (now.centroid == rest.centroid && s == 1.0f) {
        return;
    }
    const auto move = [&](platform::SkinnedVertex& v) {
        v.position = now.centroid + (v.position - rest.centroid) * s;
    };
    if (subset.empty()) {
        for (platform::SkinnedVertex& v : out) {
            move(v);
        }
        return;
    }
    for (const std::uint32_t i : subset) {
        if (i < out.size()) {
            move(out[i]);
        }
    }
}

void follow_gap_change(std::span<const platform::SkinnedVertex> body_rest,
                       std::span<const std::uint32_t> body_indices,
                       std::span<const platform::SkinnedVertex> body_now,
                       const FollowMap& map,
                       std::span<const platform::SkinnedVertex> part_rest,
                       std::span<const platform::SkinnedVertex> part_now, float& grow_m,
                       float& shrink_m) {
    grow_m = 0.0f;
    shrink_m = 0.0f;
    if (map.binds.size() != part_rest.size() || part_now.size() != part_rest.size()
        || body_now.size() != body_rest.size()) {
        return;
    }
    std::vector<std::vector<std::uint32_t>> incident;
    incident_triangles(body_rest.size(), body_indices, incident);
    for (std::size_t i = 0; i < part_rest.size(); ++i) {
        const float d0 = std::fabs(signed_surface_distance(body_rest, body_indices, incident,
                                                           map.binds[i], part_rest[i].position));
        const float d1 = std::fabs(signed_surface_distance(body_now, body_indices, incident,
                                                           map.binds[i], part_now[i].position));
        if (d0 == std::numeric_limits<float>::max() || d1 == std::numeric_limits<float>::max()) {
            continue;
        }
        grow_m = std::max(grow_m, d1 - d0);
        shrink_m = std::max(shrink_m, d0 - d1);
    }
}

float follow_vertex_gap_error(std::span<const platform::SkinnedVertex> body_rest,
                              std::span<const platform::SkinnedVertex> body_now,
                              const FollowMap& map,
                              std::span<const platform::SkinnedVertex> part_rest,
                              std::span<const platform::SkinnedVertex> part_now) {
    float worst = 0.0f;
    if (map.binds.size() != part_rest.size() || part_now.size() != part_rest.size()
        || body_now.size() != body_rest.size()) {
        return worst;
    }
    for (std::size_t i = 0; i < part_rest.size(); ++i) {
        const std::uint32_t j = map.binds[i].index[0];
        if (j >= body_rest.size()) {
            continue;
        }
        const float d0 = glm::length(part_rest[i].position - body_rest[j].position);
        const float d1 = glm::length(part_now[i].position - body_now[j].position);
        worst = std::max(worst, std::fabs(d1 - d0));
    }
    return worst;
}

std::size_t follow_penetrations(std::span<const platform::SkinnedVertex> body,
                                std::span<const std::uint32_t> body_indices,
                                const FollowMap& map,
                                std::span<const platform::SkinnedVertex> part,
                                float threshold_m) {
    if (map.binds.size() != part.size()) {
        return 0;
    }
    std::vector<std::vector<std::uint32_t>> incident;
    incident_triangles(body.size(), body_indices, incident);
    std::size_t under = 0;
    for (std::size_t i = 0; i < part.size(); ++i) {
        const float d = signed_surface_distance(body, body_indices, incident, map.binds[i],
                                                part[i].position);
        if (d < -threshold_m) {
            ++under;
        }
    }
    return under;
}

} // namespace dfn::render
