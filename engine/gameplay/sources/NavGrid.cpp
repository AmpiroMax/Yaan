/*
Module: engine/gameplay
File: engine/gameplay/sources/NavGrid.cpp

Responsibility:
- Постройка сетки проходимости из геометрии (растеризация треугольников по
  ячейкам, пролёты, этажи, эрозия) и путь по ней (A*, натяжение). См.
  NavGrid.h и docs/design/NPC_NAVIGATION.md.

Key items:
- rasterize_triangle(): треугольник режется плоскостями ячейки (по z, потом
  по x — как rasterizeTri в Recast); диапазон y обрезка даёт пролёт столбца.
  Так стена без площади в плане тоже занимает столбцы, через которые
  проходит, — иначе этажи по обе стороны стены слились бы.
- add_span(): пролёты столбца упорядочены и сливаются при перекрытии; флаг
  «верх пологий» переживает слияние, если верхи совпали в квант.
- build_floors(): этаж = верх пологого пролёта; просвет до следующего
  пролёта ≥ рост — clear.
- erode(): расстояние до края связного этажа (фаска 2/3, два прохода);
  меньше радиуса — не walkable. Ширина двери против ячейки — хвост записки.
- astar(): 8 соседей, диагональ только при обоих ортогональных; стоимость
  ячейка/√2·ячейка; эвристика — евклид в плане.
- pull(): жадное натяжение по nav_line_walkable.

Dependencies:
- Uses: NavGrid.h, engine/core/config (реестр), glm, std.
- Used by: NpcAction, NpcBehaviour, engine/app, tests/sim.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Растеризация ТОЧНАЯ (обрезка), не выборочная по центрам ячеек: замена
  на «центр внутри треугольника» теряет стены и наклонные грани уже — это
  не оптимизация, а другая сетка.
*/
#include "engine/gameplay/sources/NavGrid.h"

#include "engine/core/config/sources/Constants.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <queue>

namespace dfn::gameplay {

namespace {

/// ТОЛЩА РЕЛЬЕФА ПОД ПОВЕРХНОСТЬЮ, м: пролёт земли тянется вниз на столько,
/// чтобы фундаменты и ямы домов сливались с ним, а не висели отдельными
/// пролётами с «этажом» под землёй. Структурная величина, не порог.
constexpr float GROUND_DEPTH_M = 100.0f;
/// Столбцов сверх этого — отказ вслух: 256 м при 0,25 = 1 048 576; город
/// тайлами (условие 2 синка) — отдельная волна.
constexpr uint32_t NAV_COLUMNS_MAX = 4u * 1024u * 1024u;

struct Span {
    int32_t smin = 0;
    int32_t smax = 0;
    uint32_t next = UINT32_MAX;
    bool walk_top = false;
};

struct Heightfield {
    std::vector<uint32_t> head;  ///< голова списка пролётов столбца
    std::vector<Span> spans;
    uint32_t nx = 0, nz = 0;
};

uint64_t fnv1a(uint64_t h, const void* data, std::size_t bytes) {
    const auto* p = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < bytes; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

/// Обрезка выпуклого многоугольника полуплоскостью axis <= v (keep_below)
/// или axis >= v. Вход ≤ 7 вершин (треугольник после двух обрезок), выход ≤ 8.
int clip_poly(const glm::vec3* in, int n, glm::vec3* out, int axis, float v, bool keep_below) {
    int m = 0;
    for (int i = 0; i < n; ++i) {
        const glm::vec3& a = in[i];
        const glm::vec3& b = in[(i + 1) % n];
        const float da = keep_below ? (v - a[axis]) : (a[axis] - v);
        const float db = keep_below ? (v - b[axis]) : (b[axis] - v);
        const bool ina = da >= 0.0f;
        const bool inb = db >= 0.0f;
        if (ina) {
            out[m++] = a;
        }
        if (ina != inb) {
            const float t = da / (da - db);
            out[m++] = a + (b - a) * t;
        }
    }
    return m;
}

void add_span(Heightfield& hf, uint32_t col, int32_t smin, int32_t smax, bool walk_top) {
    // упорядоченная вставка со слиянием перекрывающихся пролётов
    uint32_t prev = UINT32_MAX;
    uint32_t cur = hf.head[col];
    while (cur != UINT32_MAX) {
        Span& s = hf.spans[cur];
        if (s.smin > smax) {
            break; // дальше все выше
        }
        if (s.smax < smin) {
            prev = cur;
            cur = s.next;
            continue;
        }
        // перекрытие: поглощаем s в новый пролёт
        smin = std::min(smin, s.smin);
        if (s.smax > smax) {
            smax = s.smax;
            walk_top = s.walk_top;
        } else if (s.smax == smax) {
            walk_top = walk_top || s.walk_top;
        }
        // удалить s из списка
        const uint32_t nxt = s.next;
        if (prev == UINT32_MAX) {
            hf.head[col] = nxt;
        } else {
            hf.spans[prev].next = nxt;
        }
        cur = nxt;
    }
    Span ns;
    ns.smin = smin;
    ns.smax = smax;
    ns.walk_top = walk_top;
    ns.next = cur;
    hf.spans.push_back(ns);
    const uint32_t id = static_cast<uint32_t>(hf.spans.size() - 1);
    if (prev == UINT32_MAX) {
        hf.head[col] = id;
    } else {
        hf.spans[prev].next = id;
    }
}

struct Raster {
    Heightfield* hf;
    glm::vec2 origin;
    float cell;
    float inv_cell;
    float y0;
    float inv_q;
    float cos_slope;
};

void rasterize_triangle(Raster& r, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
    const glm::vec3 n = glm::cross(b - a, c - a);
    const float len = glm::length(n);
    if (len < 1.0e-12f) {
        return;
    }
    // ПОЛОГОСТЬ — СВОЙСТВО НАКЛОНА, НЕ ОБХОДА: |n.y|, потому что нижняя грань
    // плиты и верхняя дают один пролёт, а обход граней у ящиков и рельефа
    // здесь не нормирован (первый прогон: нормаль земли вниз — этажей ноль).
    const bool walk_top = (std::abs(n.y) / len) >= r.cos_slope;
    const float minx = std::min({a.x, b.x, c.x});
    const float maxx = std::max({a.x, b.x, c.x});
    const float minz = std::min({a.z, b.z, c.z});
    const float maxz = std::max({a.z, b.z, c.z});
    const int nx = static_cast<int>(r.hf->nx);
    const int nz = static_cast<int>(r.hf->nz);
    int ix0 = static_cast<int>(std::floor((minx - r.origin.x) * r.inv_cell));
    int ix1 = static_cast<int>(std::floor((maxx - r.origin.x) * r.inv_cell));
    int iz0 = static_cast<int>(std::floor((minz - r.origin.y) * r.inv_cell));
    int iz1 = static_cast<int>(std::floor((maxz - r.origin.y) * r.inv_cell));
    if (ix1 < 0 || iz1 < 0 || ix0 >= nx || iz0 >= nz) {
        return;
    }
    ix0 = std::max(ix0, 0);
    iz0 = std::max(iz0, 0);
    ix1 = std::min(ix1, nx - 1);
    iz1 = std::min(iz1, nz - 1);
    glm::vec3 tri[3] = {a, b, c};
    glm::vec3 row_a[8], row_b[8], col_a[8], col_b[8];
    for (int iz = iz0; iz <= iz1; ++iz) {
        const float z0 = r.origin.y + static_cast<float>(iz) * r.cell;
        int n1 = clip_poly(tri, 3, row_a, 2, z0, false);
        if (n1 < 3) {
            continue;
        }
        int n2 = clip_poly(row_a, n1, row_b, 2, z0 + r.cell, true);
        if (n2 < 3) {
            continue;
        }
        for (int ix = ix0; ix <= ix1; ++ix) {
            const float x0 = r.origin.x + static_cast<float>(ix) * r.cell;
            int m1 = clip_poly(row_b, n2, col_a, 0, x0, false);
            if (m1 < 3) {
                continue;
            }
            int m2 = clip_poly(col_a, m1, col_b, 0, x0 + r.cell, true);
            if (m2 < 3) {
                continue;
            }
            float ymin = col_b[0].y;
            float ymax = col_b[0].y;
            for (int k = 1; k < m2; ++k) {
                ymin = std::min(ymin, col_b[k].y);
                ymax = std::max(ymax, col_b[k].y);
            }
            const int32_t smin = static_cast<int32_t>(std::floor((ymin - r.y0) * r.inv_q));
            const int32_t smax = std::max(smin, static_cast<int32_t>(std::ceil((ymax - r.y0) * r.inv_q)));
            add_span(*r.hf, static_cast<uint32_t>(iz) * r.hf->nx + static_cast<uint32_t>(ix), smin, smax,
                     walk_top);
        }
    }
}

void box_corners(const NavBox& b, glm::vec3 out[8]) {
    const float c = std::cos(b.yaw);
    const float s = std::sin(b.yaw);
    int k = 0;
    for (int dy = -1; dy <= 1; dy += 2) {
        for (int dz = -1; dz <= 1; dz += 2) {
            for (int dx = -1; dx <= 1; dx += 2) {
                const glm::vec3 l{static_cast<float>(dx) * b.half_extents.x,
                                  static_cast<float>(dy) * b.half_extents.y,
                                  static_cast<float>(dz) * b.half_extents.z};
                // конвенция сцены: местный +X при yaw → (cos, −sin)
                out[k++] = b.center + glm::vec3{l.x * c + l.z * s, l.y, -l.x * s + l.z * c};
            }
        }
    }
}

// грани ящика по индексам углов (dx быстрый, потом dz, потом dy)
constexpr int BOX_TRIS[12][3] = {
    {0, 2, 1}, {1, 2, 3}, // низ (y−)
    {4, 5, 6}, {5, 7, 6}, // верх (y+)
    {0, 1, 4}, {1, 5, 4}, // z−
    {2, 6, 3}, {3, 6, 7}, // z+
    {0, 4, 2}, {2, 4, 6}, // x−
    {1, 3, 5}, {3, 7, 5}, // x+
};

} // namespace

NavAgent nav_agent_from_registry() {
    NavAgent a;
    a.radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
    a.height = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
    a.step = static_cast<float>(config::PLAYER_STEP_HEIGHT);
    a.max_slope_rad = static_cast<float>(config::PLAYER_MAX_SLOPE);
    return a;
}

uint64_t nav_geometry_hash(const NavInput& in) {
    uint64_t h = 1469598103934665603ull;
    for (const NavMeshSoup& m : in.meshes) {
        h = fnv1a(h, m.positions.data(), m.positions.size_bytes());
        h = fnv1a(h, m.indices.data(), m.indices.size_bytes());
    }
    for (const NavBox& b : in.boxes) {
        h = fnv1a(h, &b.center, sizeof b.center);
        h = fnv1a(h, &b.half_extents, sizeof b.half_extents);
        h = fnv1a(h, &b.yaw, sizeof b.yaw);
    }
    return h;
}

uint32_t nav_geometry_triangles(const NavInput& in) {
    std::size_t n = 0;
    for (const NavMeshSoup& m : in.meshes) {
        n += m.indices.size() / 3;
    }
    n += in.boxes.size() * 12;
    return static_cast<uint32_t>(n);
}

bool nav_build(const NavInput& in, NavGrid& out, std::string* err) {
    auto fail = [&](const char* why) {
        if (err != nullptr) {
            *err = why;
        }
        out = NavGrid{};
        return false;
    };
    const float cell = in.cell > 0.0f ? in.cell : static_cast<float>(config::NAV_CELL_M);
    const float hq = in.height_q > 0.0f ? in.height_q : static_cast<float>(config::NAV_HEIGHT_Q_M);
    if (!(cell > 0.0f) || !(hq > 0.0f)) {
        return fail("nav_build: ячейка или квант высоты не число > 0");
    }
    const glm::vec2 size = in.max_xz - in.min_xz;
    if (!(size.x > 0.0f) || !(size.y > 0.0f)) {
        return fail("nav_build: охват пуст");
    }
    const double cols_d = std::ceil(size.x / cell) * std::ceil(size.y / cell);
    if (cols_d > static_cast<double>(NAV_COLUMNS_MAX)) {
        return fail("nav_build: столбцов больше NAV_COLUMNS_MAX — город тайлами, не одной сеткой");
    }
    out = NavGrid{};
    out.origin = in.min_xz;
    out.cell = cell;
    out.height_q = hq;
    out.agent = in.agent;
    out.nx = static_cast<uint32_t>(std::ceil(size.x / cell));
    out.nz = static_cast<uint32_t>(std::ceil(size.y / cell));

    // --- y0: ниже всего, что есть ------------------------------------------
    float ymin = std::numeric_limits<float>::max();
    for (const NavMeshSoup& m : in.meshes) {
        for (const glm::vec3& p : m.positions) {
            ymin = std::min(ymin, p.y);
        }
    }
    for (const NavBox& b : in.boxes) {
        ymin = std::min(ymin, b.center.y - glm::length(b.half_extents));
    }
    Heightfield hf;
    hf.nx = out.nx;
    hf.nz = out.nz;
    hf.head.assign(static_cast<std::size_t>(out.nx) * out.nz, UINT32_MAX);
    std::vector<float> ground_h;
    if (in.ground != nullptr) {
        // высота в узлах (nx+1)×(nz+1) — наклон и верх столбца из четырёх углов
        ground_h.resize(static_cast<std::size_t>(out.nx + 1) * (out.nz + 1));
        for (uint32_t iz = 0; iz <= out.nz; ++iz) {
            for (uint32_t ix = 0; ix <= out.nx; ++ix) {
                const glm::vec2 p = out.origin + glm::vec2{static_cast<float>(ix) * cell, static_cast<float>(iz) * cell};
                const float h = in.ground(in.ground_ctx, p);
                ground_h[static_cast<std::size_t>(iz) * (out.nx + 1) + ix] = h;
                ymin = std::min(ymin, h);
            }
        }
        ymin -= GROUND_DEPTH_M;
    }
    if (ymin == std::numeric_limits<float>::max()) {
        ymin = 0.0f;
    }
    out.y0 = ymin - 1.0f;

    Raster r;
    r.hf = &hf;
    r.origin = out.origin;
    r.cell = cell;
    r.inv_cell = 1.0f / cell;
    r.y0 = out.y0;
    r.inv_q = 1.0f / hq;
    r.cos_slope = std::cos(in.agent.max_slope_rad);

    // --- рельеф: пролёт столбца от толщи до верха, наклон из углов -----------
    if (!ground_h.empty()) {
        const uint32_t w = out.nx + 1;
        for (uint32_t iz = 0; iz < out.nz; ++iz) {
            for (uint32_t ix = 0; ix < out.nx; ++ix) {
                const float h00 = ground_h[static_cast<std::size_t>(iz) * w + ix];
                const float h10 = ground_h[static_cast<std::size_t>(iz) * w + ix + 1];
                const float h01 = ground_h[static_cast<std::size_t>(iz + 1) * w + ix];
                const float h11 = ground_h[static_cast<std::size_t>(iz + 1) * w + ix + 1];
                const float top = 0.25f * (h00 + h10 + h01 + h11);
                // наклон по двум диагональным треугольникам квада
                const glm::vec3 p00{0.0f, h00, 0.0f}, p10{cell, h10, 0.0f}, p01{0.0f, h01, cell}, p11{cell, h11, cell};
                const glm::vec3 n1 = glm::cross(p10 - p00, p01 - p00);
                const glm::vec3 n2 = glm::cross(p01 - p11, p10 - p11);
                const float l1 = glm::length(n1);
                const float l2 = glm::length(n2);
                const bool walk = l1 > 0.0f && l2 > 0.0f && (std::abs(n1.y) / l1) >= r.cos_slope
                                  && (std::abs(n2.y) / l2) >= r.cos_slope;
                const int32_t smax = static_cast<int32_t>(std::lround((top - out.y0) / hq));
                const int32_t smin = static_cast<int32_t>(std::floor((top - GROUND_DEPTH_M - out.y0) / hq));
                add_span(hf, iz * out.nx + ix, smin, smax, walk);
            }
        }
    }
    // --- треугольники и ящики --------------------------------------------
    for (const NavMeshSoup& m : in.meshes) {
        const std::size_t n = m.indices.size() / 3;
        for (std::size_t t = 0; t < n; ++t) {
            const uint32_t i0 = m.indices[t * 3], i1 = m.indices[t * 3 + 1], i2 = m.indices[t * 3 + 2];
            if (i0 >= m.positions.size() || i1 >= m.positions.size() || i2 >= m.positions.size()) {
                continue;
            }
            rasterize_triangle(r, m.positions[i0], m.positions[i1], m.positions[i2]);
        }
    }
    for (const NavBox& b : in.boxes) {
        glm::vec3 c[8];
        box_corners(b, c);
        for (const auto& t : BOX_TRIS) {
            rasterize_triangle(r, c[t[0]], c[t[1]], c[t[2]]);
        }
    }

    // --- этажи --------------------------------------------------------------
    const int32_t height_q = static_cast<int32_t>(std::ceil(in.agent.height / hq));
    out.columns.resize(hf.head.size());
    out.floors.reserve(hf.head.size());
    for (std::size_t col = 0; col < hf.head.size(); ++col) {
        NavColumn& c = out.columns[col];
        c.first = static_cast<uint32_t>(out.floors.size());
        uint32_t cur = hf.head[col];
        uint16_t count = 0;
        while (cur != UINT32_MAX) {
            const Span& s = hf.spans[cur];
            if (s.walk_top) {
                NavFloor f;
                f.top = s.smax;
                f.ceiling = s.next != UINT32_MAX ? hf.spans[s.next].smin : std::numeric_limits<int32_t>::max();
                f.clear = (f.ceiling == std::numeric_limits<int32_t>::max()) || (f.ceiling - f.top >= height_q);
                f.walkable = f.clear;
                out.floors.push_back(f);
                ++count;
            }
            cur = s.next;
        }
        c.count = count;
    }
    out.floors.shrink_to_fit();

    // --- эрозия на радиус: расстояние до края связного этажа -------------------
    // Фаска 2 (прямо) / 3 (наискось) в половинах ячейки, два прохода — как
    // erodeWalkableArea (Recast). Край — этаж, у которого в одном из четырёх
    // направлений нет связанного по шагу проходимого этажа.
    {
        const int32_t step_q = static_cast<int32_t>(std::ceil(in.agent.step / hq));
        std::vector<uint16_t> dist(out.floors.size(), 0xffff);
        auto conn = [&](uint32_t ix, uint32_t iz, int32_t top, int dx, int dz) -> uint32_t {
            const int64_t jx = static_cast<int64_t>(ix) + dx;
            const int64_t jz = static_cast<int64_t>(iz) + dz;
            if (jx < 0 || jz < 0 || jx >= out.nx || jz >= out.nz) {
                return UINT32_MAX;
            }
            const NavColumn& c = out.columns[static_cast<std::size_t>(jz) * out.nx + static_cast<std::size_t>(jx)];
            for (uint32_t k = 0; k < c.count; ++k) {
                const NavFloor& f = out.floors[c.first + k];
                if (f.clear && std::abs(f.top - top) <= step_q) {
                    return c.first + k;
                }
            }
            return UINT32_MAX;
        };
        // край
        for (uint32_t iz = 0; iz < out.nz; ++iz) {
            for (uint32_t ix = 0; ix < out.nx; ++ix) {
                const NavColumn& c = out.columns[static_cast<std::size_t>(iz) * out.nx + ix];
                for (uint32_t k = 0; k < c.count; ++k) {
                    const NavFloor& f = out.floors[c.first + k];
                    if (!f.clear) {
                        continue;
                    }
                    const int dirs[4][2] = {{-1, 0}, {0, 1}, {1, 0}, {0, -1}};
                    for (const auto& d : dirs) {
                        if (conn(ix, iz, f.top, d[0], d[1]) == UINT32_MAX) {
                            dist[c.first + k] = 0;
                            break;
                        }
                    }
                }
            }
        }
        auto relax = [&](uint32_t me, uint32_t ix, uint32_t iz, int32_t top, int dx, int dz, int ddx, int ddz) {
            const uint32_t n = conn(ix, iz, top, dx, dz);
            if (n == UINT32_MAX) {
                return;
            }
            dist[me] = static_cast<uint16_t>(std::min<int>(dist[me], dist[n] + 2));
            const NavFloor& nf = out.floors[n];
            const uint32_t nn = conn(static_cast<uint32_t>(static_cast<int64_t>(ix) + dx),
                                     static_cast<uint32_t>(static_cast<int64_t>(iz) + dz), nf.top, ddx, ddz);
            if (nn != UINT32_MAX) {
                dist[me] = static_cast<uint16_t>(std::min<int>(dist[me], dist[nn] + 3));
            }
        };
        // проход 1: (−x) и (−z) с диагоналями через них
        for (uint32_t iz = 0; iz < out.nz; ++iz) {
            for (uint32_t ix = 0; ix < out.nx; ++ix) {
                const NavColumn& c = out.columns[static_cast<std::size_t>(iz) * out.nx + ix];
                for (uint32_t k = 0; k < c.count; ++k) {
                    const uint32_t me = c.first + k;
                    if (!out.floors[me].clear) {
                        continue;
                    }
                    const int32_t top = out.floors[me].top;
                    relax(me, ix, iz, top, -1, 0, 0, -1);
                    relax(me, ix, iz, top, 0, -1, 1, 0);
                }
            }
        }
        // проход 2: (+x) и (+z)
        for (uint32_t iz = out.nz; iz-- > 0;) {
            for (uint32_t ix = out.nx; ix-- > 0;) {
                const NavColumn& c = out.columns[static_cast<std::size_t>(iz) * out.nx + ix];
                for (uint32_t k = 0; k < c.count; ++k) {
                    const uint32_t me = c.first + k;
                    if (!out.floors[me].clear) {
                        continue;
                    }
                    const int32_t top = out.floors[me].top;
                    relax(me, ix, iz, top, 1, 0, 0, 1);
                    relax(me, ix, iz, top, 0, 1, -1, 0);
                }
            }
        }
        // ПОРОГ В ПОЛОВИНАХ ЯЧЕЙКИ, ОТ ЦЕНТРА ЯЧЕЙКИ: этаж с фаской d стоит
        // центром на (d/2 + 0,5)·ячейка от края столбца препятствия, а тот край
        // не дальше грани. Съедается, если центр ближе радиуса:
        // d < 2·(r/ячейка − 0,5). Recast берёт ceil(r/ячейка) — на 0,25 при
        // радиусе 0,35 это отодвигало путь от стены на 0,75 м и закрывало
        // проходы уже 1,75 м; здесь проход в три свободные ячейки (0,75) уже
        // открыт, центр пути ≥ r от любой грани по построению.
        const float thresh = 2.0f * std::max(0.0f, in.agent.radius / cell - 0.5f);
        for (std::size_t i = 0; i < out.floors.size(); ++i) {
            if (out.floors[i].clear && static_cast<float>(dist[i]) < thresh) {
                out.floors[i].walkable = false;
            }
        }
    }

    out.stats.columns = static_cast<uint32_t>(out.columns.size());
    out.stats.floors = static_cast<uint32_t>(out.floors.size());
    out.stats.walkable = 0;
    for (const NavFloor& f : out.floors) {
        out.stats.walkable += f.walkable ? 1u : 0u;
    }
    out.stats.spans = static_cast<uint32_t>(hf.spans.size());
    out.stats.build_peak_bytes = hf.spans.capacity() * sizeof(Span) + hf.head.capacity() * sizeof(uint32_t)
                                 + ground_h.capacity() * sizeof(float) + out.memory_bytes();
    out.stats.triangles = nav_geometry_triangles(in);
    out.stats.geometry_hash = nav_geometry_hash(in);
    return true;
}

namespace {

std::optional<NavRef> floor_in_column(const NavGrid& g, uint32_t ix, uint32_t iz, float y, float snap,
                                      float* dy_out) {
    const NavColumn& c = g.columns[static_cast<std::size_t>(iz) * g.nx + ix];
    std::optional<NavRef> best;
    float best_dy = snap;
    for (uint32_t k = 0; k < c.count; ++k) {
        const NavFloor& f = g.floors[c.first + k];
        if (!f.walkable) {
            continue;
        }
        const float dy = std::abs(g.top_y(f) - y);
        if (dy <= best_dy) {
            best_dy = dy;
            best = NavRef{ix, iz, c.first + k};
        }
    }
    if (best && dy_out != nullptr) {
        *dy_out = best_dy;
    }
    return best;
}

bool cell_of(const NavGrid& g, const glm::vec3& p, int64_t& ix, int64_t& iz) {
    ix = static_cast<int64_t>(std::floor((p.x - g.origin.x) / g.cell));
    iz = static_cast<int64_t>(std::floor((p.z - g.origin.y) / g.cell));
    return ix >= 0 && iz >= 0 && ix < g.nx && iz < g.nz;
}

} // namespace

std::optional<NavRef> nav_locate(const NavGrid& g, const glm::vec3& p, float snap_m) {
    if (!g.valid()) {
        return std::nullopt;
    }
    int64_t cx, cz;
    if (!cell_of(g, p, cx, cz)) {
        return std::nullopt;
    }
    const int rings = static_cast<int>(std::ceil(snap_m / g.cell));
    std::optional<NavRef> best;
    float best_d = std::numeric_limits<float>::max();
    for (int ring = 0; ring <= rings; ++ring) {
        for (int dz = -ring; dz <= ring; ++dz) {
            for (int dx = -ring; dx <= ring; ++dx) {
                if (std::max(std::abs(dx), std::abs(dz)) != ring) {
                    continue;
                }
                const int64_t ix = cx + dx, iz = cz + dz;
                if (ix < 0 || iz < 0 || ix >= g.nx || iz >= g.nz) {
                    continue;
                }
                float dy = 0.0f;
                const auto f = floor_in_column(g, static_cast<uint32_t>(ix), static_cast<uint32_t>(iz), p.y, snap_m, &dy);
                if (!f) {
                    continue;
                }
                const glm::vec2 cc = g.cell_center(f->ix, f->iz);
                const float d = glm::length(glm::vec3{cc.x - p.x, dy, cc.y - p.z});
                if (d < best_d) {
                    best_d = d;
                    best = f;
                }
            }
        }
        if (best) {
            return best; // ближе кольцо — ближе точка; дальше не ищем
        }
    }
    return best;
}

glm::vec3 nav_point(const NavGrid& g, const NavRef& r) {
    const glm::vec2 c = g.cell_center(r.ix, r.iz);
    return glm::vec3{c.x, g.top_y(g.floors[r.floor]), c.y};
}

std::optional<uint32_t> nav_neighbour(const NavGrid& g, const NavRef& from, int dx, int dz) {
    const int64_t jx = static_cast<int64_t>(from.ix) + dx;
    const int64_t jz = static_cast<int64_t>(from.iz) + dz;
    if (jx < 0 || jz < 0 || jx >= g.nx || jz >= g.nz) {
        return std::nullopt;
    }
    const int32_t step_q = static_cast<int32_t>(std::ceil(g.agent.step / g.height_q));
    const int32_t top = g.floors[from.floor].top;
    const NavColumn& c = g.columns[static_cast<std::size_t>(jz) * g.nx + static_cast<std::size_t>(jx)];
    for (uint32_t k = 0; k < c.count; ++k) {
        const NavFloor& f = g.floors[c.first + k];
        if (f.walkable && std::abs(f.top - top) <= step_q) {
            return c.first + k;
        }
    }
    return std::nullopt;
}

namespace {

bool is_blocked(std::span<const NavCellBlock> blocked, uint32_t ix, uint32_t iz) {
    for (const NavCellBlock& b : blocked) {
        if (b.ix == ix && b.iz == iz) {
            return true;
        }
    }
    return false;
}

bool line_walkable(const NavGrid& g, const glm::vec3& a, const glm::vec3& b, std::span<const NavCellBlock> blocked);

} // namespace

bool nav_cell_of(const NavGrid& g, const glm::vec3& p, NavCellBlock& out) {
    int64_t ix, iz;
    if (!g.valid() || !cell_of(g, p, ix, iz)) {
        return false;
    }
    out = NavCellBlock{static_cast<uint32_t>(ix), static_cast<uint32_t>(iz)};
    return true;
}

bool nav_line_walkable(const NavGrid& g, const glm::vec3& a, const glm::vec3& b) {
    return line_walkable(g, a, b, {});
}

namespace {

bool line_walkable(const NavGrid& g, const glm::vec3& a, const glm::vec3& b, std::span<const NavCellBlock> blocked) {
    // выборка отрезка через полъячейки: каждая ячейка по нему должна нести
    // этаж, связанный по шагу с прежним и проходимый. Полъячейки не
    // пропускают ячейку по ходу; угол, задетый по касательной, закрыт
    // эрозией (полоса ≥ 1 ячейки у любого препятствия).
    const auto start = nav_locate(g, a, g.agent.step + g.height_q);
    if (!start) {
        return false;
    }
    int64_t ax, az;
    if (!cell_of(g, a, ax, az)) {
        return false;
    }
    NavRef cur = *start;
    const glm::vec2 d{b.x - a.x, b.z - a.z};
    const float len = glm::length(d);
    const int steps = std::max(1, static_cast<int>(std::ceil(len / (0.5f * g.cell))));
    for (int i = 1; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const glm::vec3 p{a.x + d.x * t, 0.0f, a.z + d.y * t};
        int64_t ix, iz;
        if (!cell_of(g, p, ix, iz)) {
            return false;
        }
        if (ix == cur.ix && iz == cur.iz) {
            continue;
        }
        const int dx = static_cast<int>(ix - static_cast<int64_t>(cur.ix));
        const int dz = static_cast<int>(iz - static_cast<int64_t>(cur.iz));
        if (std::abs(dx) > 1 || std::abs(dz) > 1) {
            return false; // полъячейки не дают прыгать через ячейку
        }
        const auto n = nav_neighbour(g, cur, dx, dz);
        if (!n || is_blocked(blocked, static_cast<uint32_t>(ix), static_cast<uint32_t>(iz))) {
            return false;
        }
        if (dx != 0 && dz != 0) {
            // диагональ — только при обоих ортогональных соседях
            if (!nav_neighbour(g, cur, dx, 0) || !nav_neighbour(g, cur, 0, dz)) {
                return false;
            }
        }
        cur = NavRef{static_cast<uint32_t>(ix), static_cast<uint32_t>(iz), *n};
    }
    // конец должен быть у того же этажа по высоте
    return std::abs(g.top_y(g.floors[cur.floor]) - b.y) <= g.agent.step + g.height_q;
}

} // namespace

float NavPath::length_m() const {
    float l = 0.0f;
    for (std::size_t i = 1; i < points.size(); ++i) {
        l += glm::length(points[i] - points[i - 1]);
    }
    return l;
}

bool nav_find_path(const NavGrid& g, NavSearch& search, const glm::vec3& from, const glm::vec3& to, NavPath& out,
                   bool pull, std::span<const NavCellBlock> blocked) {
    out = NavPath{};
    if (!g.valid()) {
        return false;
    }
    const float snap = static_cast<float>(config::NAV_SNAP_M);
    const auto s = nav_locate(g, from, snap);
    const auto t = nav_locate(g, to, snap);
    if (!s || !t) {
        return false;
    }
    if (search.g.size() != g.floors.size()) {
        search.g.assign(g.floors.size(), 0.0f);
        search.parent.assign(g.floors.size(), UINT32_MAX);
        search.stamp.assign(g.floors.size(), 0);
        search.closed.assign(g.floors.size(), 0);
        search.generation = 0;
    }
    ++search.generation;
    if (search.generation == 0) { // переполнение стемпа — честный сброс
        std::fill(search.stamp.begin(), search.stamp.end(), 0);
        search.generation = 1;
    }
    const uint32_t gen = search.generation;
    search.expanded = 0;
    // столбец этажа — обратно по индексу: ищем через бинарный поиск по columns
    auto column_of = [&](uint32_t floor, uint32_t& ix, uint32_t& iz) {
        // columns.first монотонно неубывает по индексу столбца
        std::size_t lo = 0, hi = g.columns.size();
        while (hi - lo > 1) {
            const std::size_t mid = (lo + hi) / 2;
            if (g.columns[mid].first <= floor) {
                lo = mid;
            } else {
                hi = mid;
            }
        }
        // столбцы с count 0 делят first с соседом справа — сдвигаемся к тому, кто содержит
        while (lo + 1 < g.columns.size() && g.columns[lo].count == 0 && g.columns[lo + 1].first <= floor) {
            ++lo;
        }
        ix = static_cast<uint32_t>(lo % g.nx);
        iz = static_cast<uint32_t>(lo / g.nx);
    };
    struct Node {
        float f;
        uint32_t id;
        bool operator<(const Node& o) const { return f > o.f; }
    };
    const glm::vec2 goal_c = g.cell_center(t->ix, t->iz);
    auto h = [&](uint32_t ix, uint32_t iz) { return glm::length(g.cell_center(ix, iz) - goal_c); };
    std::priority_queue<Node> open;
    search.g[s->floor] = 0.0f;
    search.parent[s->floor] = UINT32_MAX;
    search.stamp[s->floor] = gen;
    search.closed[s->floor] = 0;
    open.push(Node{h(s->ix, s->iz), s->floor});
    bool found = false;
    const float diag = g.cell * std::sqrt(2.0f);
    while (!open.empty()) {
        const Node n = open.top();
        open.pop();
        if (search.closed[n.id] != 0 && search.stamp[n.id] == gen) {
            continue;
        }
        search.closed[n.id] = 1;
        ++search.expanded;
        if (n.id == t->floor) {
            found = true;
            break;
        }
        uint32_t ix, iz;
        column_of(n.id, ix, iz);
        const NavRef cur{ix, iz, n.id};
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dz == 0) {
                    continue;
                }
                const auto nb = nav_neighbour(g, cur, dx, dz);
                if (!nb) {
                    continue;
                }
                if (dx != 0 && dz != 0 && (!nav_neighbour(g, cur, dx, 0) || !nav_neighbour(g, cur, 0, dz))) {
                    continue;
                }
                if (!blocked.empty()
                    && is_blocked(blocked, static_cast<uint32_t>(static_cast<int64_t>(ix) + dx),
                                  static_cast<uint32_t>(static_cast<int64_t>(iz) + dz))) {
                    continue;
                }
                const float ng = search.g[n.id] + ((dx != 0 && dz != 0) ? diag : g.cell);
                if (search.stamp[*nb] == gen && (search.closed[*nb] != 0 || ng >= search.g[*nb])) {
                    continue;
                }
                search.stamp[*nb] = gen;
                search.closed[*nb] = 0;
                search.g[*nb] = ng;
                search.parent[*nb] = n.id;
                open.push(Node{ng + h(static_cast<uint32_t>(static_cast<int64_t>(ix) + dx),
                                      static_cast<uint32_t>(static_cast<int64_t>(iz) + dz)),
                               *nb});
            }
        }
    }
    if (!found) {
        return false;
    }
    std::vector<uint32_t> chain;
    for (uint32_t id = t->floor; id != UINT32_MAX; id = search.parent[id]) {
        chain.push_back(id);
        if (id == s->floor) {
            break;
        }
    }
    std::reverse(chain.begin(), chain.end());
    std::vector<glm::vec3> pts;
    pts.reserve(chain.size() + 2);
    for (const uint32_t id : chain) {
        uint32_t ix, iz;
        column_of(id, ix, iz);
        pts.push_back(nav_point(g, NavRef{ix, iz, id}));
    }
    out.cells = static_cast<uint32_t>(chain.size());
    for (std::size_t i = 1; i < pts.size(); ++i) {
        out.cells_length_m += glm::length(pts[i] - pts[i - 1]);
    }
    // концы — как даны (по высоте — этаж), середина — центры ячеек
    if (!pts.empty()) {
        pts.front() = glm::vec3{from.x, pts.front().y, from.z};
        pts.back() = glm::vec3{to.x, pts.back().y, to.z};
    }
    if (!pull || pts.size() <= 2) {
        out.points = std::move(pts);
        return true;
    }
    // жадное натяжение: от текущей точки — самая дальняя видимая
    out.points.push_back(pts.front());
    std::size_t i = 0;
    constexpr std::size_t LOOKAHEAD = 400; // ячеек вперёд за один поиск дальней точки
    while (i + 1 < pts.size()) {
        std::size_t j = std::min(pts.size() - 1, i + LOOKAHEAD);
        while (j > i + 1 && !line_walkable(g, pts[i], pts[j], blocked)) {
            --j;
        }
        out.points.push_back(pts[j]);
        i = j;
    }
    return true;
}

} // namespace dfn::gameplay
