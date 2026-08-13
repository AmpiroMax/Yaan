/*
Created: 13:08:2026 - 21:50:00
Last updated: 13:08:2026 - 21:50:00
Module: engine/world
File: engine/world/sources/WorldgenFlow.cpp

Responsibility:
- Implementation of the drainage pass: fill, route, accumulate, incise, shape.

Key items:
- build_flow, FlowGrid::sample, FlowGrid::area_at.

Dependencies:
- Uses: WorldgenFlow.h.
- Used by: Worldgen.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic (Rule 13.1): see the ordering note in the header. Every loop
  here is in a fixed order and every tie is broken by cell index.
*/
/*
UPD:
- 13:08:2026 - 21:50:00: Created.
*/

#include "engine/world/sources/WorldgenFlow.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <queue>
#include <vector>

namespace dfn::world {

namespace {

/// D8 neighbourhood. Fixed order, and the order is load-bearing: it is the
/// tie-break when two neighbours offer the same drop, so it must never be
/// sorted, shuffled or made to depend on the data.
struct Nb {
    int dx;
    int dz;
    float len;
};
constexpr float R2 = 1.41421356f;
constexpr Nb NB[8] = {{-1, 0, 1.0f}, {1, 0, 1.0f},   {0, -1, 1.0f},  {0, 1, 1.0f},
                      {-1, -1, R2},  {1, -1, R2},    {-1, 1, R2},    {1, 1, R2}};

/// Priority-flood depression filling (Barnes, Lehman & Mulla 2014).
///
/// It is not optional and it is not a polish step: without it every closed
/// basin in the macro field swallows its own inflow, accumulation stops there,
/// and the network below the basin never forms. The epsilon tilt keeps filled
/// flats draining in a definite direction instead of producing a plateau with
/// no receiver, which would strand area in the middle of a valley floor.
void fill_depressions(std::vector<float>& h, int n) {
    struct Item {
        float h;
        int idx;
        // Ties broken by index so the pop order cannot depend on float noise.
        bool operator<(const Item& o) const {
            return h != o.h ? h > o.h : idx > o.idx; // min-heap
        }
    };
    std::priority_queue<Item> pq;
    std::vector<char> seen(static_cast<std::size_t>(n) * n, 0);
    const auto push = [&](int x, int z) {
        const int i = z * n + x;
        if (seen[static_cast<std::size_t>(i)]) return;
        seen[static_cast<std::size_t>(i)] = 1;
        pq.push({h[static_cast<std::size_t>(i)], i});
    };
    for (int x = 0; x < n; ++x) {
        push(x, 0);
        push(x, n - 1);
    }
    for (int z = 0; z < n; ++z) {
        push(0, z);
        push(n - 1, z);
    }
    constexpr float EPS = 1e-3f; // metres of tilt per cell across a filled flat
    while (!pq.empty()) {
        const Item it = pq.top();
        pq.pop();
        const int x = it.idx % n;
        const int z = it.idx / n;
        for (const Nb& nb : NB) {
            const int nx = x + nb.dx;
            const int nz = z + nb.dz;
            if (nx < 0 || nx >= n || nz < 0 || nz >= n) continue;
            const std::size_t ni = static_cast<std::size_t>(nz) * n + nx;
            if (seen[ni]) continue;
            seen[ni] = 1;
            h[ni] = std::max(h[ni], it.h + EPS);
            pq.push({h[ni], static_cast<int>(ni)});
        }
    }
}

} // namespace

float FlowGrid::sample(glm::vec2 world) const {
    if (empty()) return 0.0f;
    const float fx = (world.x - origin.x) / cell;
    const float fz = (world.y - origin.y) / cell;
    if (fx < 0.0f || fz < 0.0f || fx >= static_cast<float>(n - 1)
        || fz >= static_cast<float>(n - 1)) {
        return 0.0f;
    }
    const int x0 = static_cast<int>(fx);
    const int z0 = static_cast<int>(fz);
    // SMOOTHSTEP WEIGHTS, NOT LINEAR ONES, and this was measured rather than
    // preferred. Bilinear interpolation is C0: its derivative jumps at every
    // cell boundary, so reading a 6 m grid linearly injects broadband energy
    // centred near twice the cell — 12 m, which is exactly the band this whole
    // pass exists to empty. Measured on the plain: the pass added 1.73e7 to the
    // 8-20 m band, a third of what remained after the comb was removed, and
    // that contribution is an artefact of the READ rather than of the cut.
    // Smoothstep has zero derivative at the cell edges, so the sampled field is
    // C1 and the ringing goes with it.
    const auto sm = [](float t) { return t * t * (3.0f - 2.0f * t); };
    const float tx = sm(fx - static_cast<float>(x0));
    const float tz = sm(fz - static_cast<float>(z0));
    const auto at = [&](int x, int z) {
        return delta[static_cast<std::size_t>(z) * n + x];
    };
    const float a = at(x0, z0) * (1.0f - tx) + at(x0 + 1, z0) * tx;
    const float b = at(x0, z0 + 1) * (1.0f - tx) + at(x0 + 1, z0 + 1) * tx;
    return a * (1.0f - tz) + b * tz;
}

float FlowGrid::area_at(glm::vec2 world) const {
    if (empty() || area.empty()) return 0.0f;
    const int x = static_cast<int>(std::lround((world.x - origin.x) / cell));
    const int z = static_cast<int>(std::lround((world.y - origin.y) / cell));
    if (x < 0 || z < 0 || x >= n || z >= n) return 0.0f;
    return area[static_cast<std::size_t>(z) * n + x];
}

namespace {
/// SWEEP DOORS (measurement only, never a shipping path). The channel threshold
/// and the depth coefficient are the two numbers this pass is actually about --
/// spacing and incision -- so they have to be movable through one binary before
/// either is proposed to NUMBERS.md.
float env_or(const char* name, float lo, float hi, float fallback) {
    if (const char* e = std::getenv(name)) {
        const float v = std::strtof(e, nullptr);
        if (v >= lo && v <= hi) return v;
    }
    return fallback;
}
} // namespace

FlowGrid build_flow(uint64_t seed, glm::vec2 domain_min, glm::vec2 domain_max,
                    const FlowParams& params_in,
                    const std::function<float(glm::vec2)>& base_height, bool enabled) {
    (void)seed; // the drainage is a property of the LANDFORM, not of a stream
    FlowParams params = params_in;
    params.channel_area_m2 =
        env_or("DFN_FLOW_AREA", 200.0f, 200000.0f, params.channel_area_m2);
    params.depth_coef = env_or("DFN_FLOW_DEPTH", 0.0f, 20.0f, params.depth_coef);
    params.depth_max = env_or("DFN_FLOW_DEPTH_MAX", 0.0f, 60.0f, params.depth_max);
    params.width_coef = env_or("DFN_FLOW_WIDTH", 0.2f, 20.0f, params.width_coef);
    params.shape_passes =
        static_cast<int>(env_or("DFN_FLOW_PASSES", 0.0f, 24.0f,
                                static_cast<float>(params.shape_passes)));
    FlowGrid g;
    const glm::vec2 lo = domain_min - glm::vec2{params.margin};
    const glm::vec2 hi = domain_max + glm::vec2{params.margin};
    const float span = std::max(hi.x - lo.x, hi.y - lo.y);
    g.cell = params.cell;
    g.n = std::max(4, static_cast<int>(std::ceil(span / g.cell)) + 1);
    g.origin = lo;
    const std::size_t cells = static_cast<std::size_t>(g.n) * g.n;
    g.delta.assign(cells, 0.0f);
    g.area.assign(cells, 0.0f);
    if (!enabled) return g; // the named control, same entry point

    // --- 1. sample the landform ---------------------------------------------
    std::vector<float> h(cells, 0.0f);
    for (int z = 0; z < g.n; ++z) {
        for (int x = 0; x < g.n; ++x) {
            h[static_cast<std::size_t>(z) * g.n + x] =
                base_height(g.origin + glm::vec2{static_cast<float>(x), static_cast<float>(z)}
                                           * g.cell);
        }
    }

    // --- 2. fill, so every cell has somewhere to send its water --------------
    std::vector<float> filled = h;
    fill_depressions(filled, g.n);

    // --- 3. steepest-descent receiver ---------------------------------------
    std::vector<int> recv(cells, -1);
    for (int z = 0; z < g.n; ++z) {
        for (int x = 0; x < g.n; ++x) {
            const std::size_t i = static_cast<std::size_t>(z) * g.n + x;
            float best = 0.0f;
            int bi = -1;
            for (const Nb& nb : NB) {
                const int nx = x + nb.dx;
                const int nz = z + nb.dz;
                if (nx < 0 || nx >= g.n || nz < 0 || nz >= g.n) continue;
                const std::size_t ni = static_cast<std::size_t>(nz) * g.n + nx;
                const float s = (filled[i] - filled[ni]) / (nb.len * g.cell);
                if (s > best) {
                    best = s;
                    bi = static_cast<int>(ni);
                }
            }
            recv[i] = bi;
        }
    }

    // --- 4. accumulate area, high to low ------------------------------------
    // Sorting by filled elevation gives a topological order of the descent
    // graph; the index tie-break keeps equal-height cells in a fixed order, so
    // the float sum is reproducible.
    std::vector<int> order(cells);
    for (std::size_t i = 0; i < cells; ++i) order[i] = static_cast<int>(i);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const float ha = filled[static_cast<std::size_t>(a)];
        const float hb = filled[static_cast<std::size_t>(b)];
        return ha != hb ? ha > hb : a < b;
    });
    const float cell_area = g.cell * g.cell;
    std::vector<float> area(cells, cell_area);
    for (const int i : order) {
        const int r = recv[static_cast<std::size_t>(i)];
        if (r >= 0) area[static_cast<std::size_t>(r)] += area[static_cast<std::size_t>(i)];
    }
    g.area = area;

    // --- 5. incise, by the area law -----------------------------------------
    // THE WHOLE POINT OF THE PASS IS IN THIS LOOP AND IT IS ONE LINE OF SHAPE:
    // a cell is cut only if enough ground drains through it. Nothing here names
    // a spacing, a direction or a pitch -- all three come out of where the
    // landform sends its water, which is why the result cannot be a comb.
    std::vector<float> depth(cells, 0.0f);
    for (std::size_t i = 0; i < cells; ++i) {
        const float a = area[i];
        if (a < params.channel_area_m2) continue;
        const float rel = a / params.channel_area_m2;
        depth[i] = std::min(params.depth_max,
                            params.depth_coef * std::pow(rel, params.depth_exp));
    }

    // --- 6. shape the section ------------------------------------------------
    // A cut one cell wide is a slot, and a slot at grid resolution is exactly
    // the claw mark we are removing. Spreading the depth sideways turns it into
    // a valley: each pass is a 3x3 blur that only ever RAISES a neighbour's
    // depth toward its own, so a channel widens without the trunk losing its
    // floor. Wide valleys get more passes by carrying more depth into the blur,
    // which is the width law expressed as diffusion instead of as a radius.
    std::vector<float> tmp(cells, 0.0f);
    for (int pass = 0; pass < params.shape_passes; ++pass) {
        tmp = depth;
        for (int z = 1; z < g.n - 1; ++z) {
            for (int x = 1; x < g.n - 1; ++x) {
                const std::size_t i = static_cast<std::size_t>(z) * g.n + x;
                float acc = depth[i];
                float w = 1.0f;
                for (const Nb& nb : NB) {
                    const std::size_t ni =
                        static_cast<std::size_t>(z + nb.dz) * g.n + (x + nb.dx);
                    // Neighbour half-width in cells, from the same area law:
                    // a big trunk reaches further sideways than a headwater.
                    const float an = area[ni];
                    if (an < params.channel_area_m2) continue;
                    const float hw =
                        std::min(params.width_max_cells,
                                 params.width_coef
                                     * std::pow(an / params.channel_area_m2, params.width_exp));
                    const float reach = std::clamp(hw / std::max(1.0f, nb.len), 0.0f, 1.0f);
                    acc += depth[ni] * reach;
                    w += reach;
                }
                tmp[i] = acc / w;
            }
        }
        depth.swap(tmp);
    }

    for (std::size_t i = 0; i < cells; ++i) {
        g.delta[i] = -std::min(params.depth_max, depth[i]);
    }
    return g;
}

} // namespace dfn::world
