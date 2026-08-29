/*
Module: engine/world
File: engine/world/sources/WorldgenPaths.cpp

Responsibility:
- The §8.1 path network: goal siting, the slope-aware cost search, BR-1 as a
  term inside that search, class assignment, the wear/edge fields and the
  flatten delta.

Key items:
- build_path_network, PathNetwork::sample / ::flatten_at, path_half_width.

Dependencies:
- Uses: WorldgenPaths.h, config (BR constants), glm.
- Used by: Worldgen.cpp, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 13.1: no unordered containers on the output path; the A* frontier
  breaks ties on the node INDEX so the expansion order is reproducible.
*/

#include "engine/world/sources/WorldgenPaths.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenNoise.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <queue>

namespace dfn::world {

namespace {

constexpr float EYE_M = static_cast<float>(config::PLAYER_EYE_HEIGHT);
constexpr float HIDE_MIN_RUN_M = static_cast<float>(config::HIDE_REVEAL_MIN_RUN_M);

/// Range from the goal at which BR-1's acceptance PAIR is shot (m).
///
/// DERIVED FROM THE FRAME, not chosen: the claim is "the destination is not in
/// this picture", and it is only falsifiable at a range where the destination
/// WOULD be in the picture. A goal reads at roughly 3 m; at INTERNAL_RES_H
/// lines over CAMERA_FOV_Y that is 3/d radians spread over (FOV/H) radians per
/// line, i.e. about 3*H/(FOV*d) lines tall — 8 lines at 120 m and 2.5 at
/// 400 m. Two or three lines of a 360-line frame is not a shrine anyone can
/// see, so a frame taken out there cannot fail and is not evidence.
constexpr float BR1_FRAME_RANGE_M = 120.0f;

/// Worn half-widths. REQUESTED NUMBERS rows (Rule 35 — render sizes its splat
/// from these, so they stop belonging to core the moment render reads them):
/// cobble 1.6 m (a cart track: two wheels plus a verge), dirt 1.1 m (a walked
/// road), faint trail 0.45 m (single file — «намёк», it must read as one line
/// of feet), steps 0.9 m (a flight one person climbs, hands free).
float half_width_of(PathClass c) {
    switch (c) {
    case PathClass::Cobble: return 1.6f;
    case PathClass::Dirt: return 1.1f;
    case PathClass::FaintTrail: return 0.45f;
    case PathClass::StoneSteps: return 0.9f;
    }
    return 1.1f;
}

/// Grade above which a walked route stops being walked and becomes steps.
/// 0.22 (~12.5 deg). MEASURED, not assumed: the first cut used 0.30 and the
/// class NEVER APPEARED — the router contours, so route grades on this stand
/// top out at 0.15 while the ground itself reaches 0.32. Two things follow.
/// (a) The threshold belongs to the grade a WORN EARTH RAMP survives, and at
/// ~12.5 deg on a wooded slope it starts gullying under rain, which is exactly
/// when people lay stone. (b) A route only reaches that grade where it stops
/// contouring, which is the summit approach below — the class needed a REASON
/// to exist, not a lower number.
constexpr float STEPS_GRADE = 0.22f;
/// Within this distance of a summit-kind goal the router nearly stops paying
/// for slope: nobody spirals the last thirty metres to a cairn, they go up.
/// This is what puts real grade on a route and therefore what makes steps a
/// consequence rather than a decoration.
constexpr float SUMMIT_APPROACH_M = 45.0f;
constexpr float SUMMIT_SLOPE_K = 0.5f;
/// Radius around a SMALL goal within which the road thins to a hint-path.
/// The class must change ALONG a route (§8.1 item 1); the first cut keyed it
/// off the destination's importance and painted whole routes faint.
constexpr float FAINT_RADIUS_M = 90.0f;
/// Radius around the largest goal where the surface is paved (в7's «мостовая»
/// is a made thing: it exists where somebody built it, near the goal worth
/// building for).
constexpr float COBBLE_RADIUS_M = 55.0f;

struct Grid {
    glm::vec2 origin{};
    float cell = 4.0f;
    int n = 0;
    std::vector<float> h;
    [[nodiscard]] int idx(int x, int z) const { return z * n + x; }
    [[nodiscard]] glm::vec2 world(int x, int z) const {
        return origin + glm::vec2{static_cast<float>(x), static_cast<float>(z)} * cell;
    }
    [[nodiscard]] float at(int x, int z) const {
        return h[static_cast<std::size_t>(idx(std::clamp(x, 0, n - 1), std::clamp(z, 0, n - 1)))];
    }
    /// Bilinear height in world space (the same surface the router walks, so a
    /// visibility ray and a route cost never disagree about the ground).
    [[nodiscard]] float sample(glm::vec2 w) const {
        const float fx = std::clamp((w.x - origin.x) / cell, 0.0f, static_cast<float>(n - 1));
        const float fz = std::clamp((w.y - origin.y) / cell, 0.0f, static_cast<float>(n - 1));
        const int x0 = std::min(static_cast<int>(fx), n - 2);
        const int z0 = std::min(static_cast<int>(fz), n - 2);
        const float tx = fx - static_cast<float>(x0);
        const float tz = fz - static_cast<float>(z0);
        const float a = at(x0, z0) + (at(x0 + 1, z0) - at(x0, z0)) * tx;
        const float b = at(x0, z0 + 1) + (at(x0 + 1, z0 + 1) - at(x0, z0 + 1)) * tx;
        return a + (b - a) * tz;
    }
};

/// Is `to` visible from `from` at eye height over the grid? Marching in ~4 m
/// steps: the same station spacing BR-1 measures at, so the test and the
/// generator use one instrument.
bool visible(const Grid& g, glm::vec2 from, glm::vec2 to, float target_lift) {
    const float d = glm::length(to - from);
    if (d < 1e-3f) {
        return true;
    }
    const float eye = g.sample(from) + EYE_M;
    const float tgt = g.sample(to) + target_lift;
    const int steps = std::max(2, static_cast<int>(d / 4.0f));
    for (int i = 1; i < steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const glm::vec2 p = from + (to - from) * t;
        if (g.sample(p) > eye + (tgt - eye) * t) {
            return false;
        }
    }
    return true;
}

/// A* on the 8-neighbourhood. `visible_mask` (may be empty) adds BR-1's cost:
/// ground from which the destination can already be seen is more expensive to
/// walk, so the cheapest route prefers to stay behind the meso tier and comes
/// out where it must. BR-1 is therefore a PROPERTY OF THE COST FIELD, not a
/// bend applied afterwards — which is what keeps it inside BR-2's ceiling
/// instead of fighting it.
std::vector<int> astar(const Grid& g, int start, int goal, float slope_k,
                       const std::vector<uint8_t>& visible_mask, float hide_weight,
                       bool summit_approach) {
    const int total = g.n * g.n;
    std::vector<float> gscore(static_cast<std::size_t>(total), 1e30f);
    std::vector<int> came(static_cast<std::size_t>(total), -1);
    // (f, node): the node index is part of the key so ties break on a total
    // order. A heap that compares only f resolves ties by heap layout, which
    // is an implementation detail and therefore not reproducible (Rule 13.1).
    using Item = std::pair<float, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<>> open;
    gscore[static_cast<std::size_t>(start)] = 0.0f;
    open.emplace(0.0f, start);
    const int gx = goal % g.n;
    const int gz = goal / g.n;
    const auto heuristic = [&](int node) {
        const float dx = static_cast<float>(node % g.n - gx);
        const float dz = static_cast<float>(node / g.n - gz);
        return std::sqrt(dx * dx + dz * dz) * g.cell; // admissible: cost >= distance
    };
    while (!open.empty()) {
        const auto [f, cur] = open.top();
        open.pop();
        if (cur == goal) {
            break;
        }
        if (f > gscore[static_cast<std::size_t>(cur)] + heuristic(cur) + 1e-4f) {
            continue; // stale entry
        }
        const int cx = cur % g.n;
        const int cz = cur / g.n;
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dz == 0) {
                    continue;
                }
                const int nx = cx + dx;
                const int nz = cz + dz;
                if (nx < 0 || nz < 0 || nx >= g.n || nz >= g.n) {
                    continue;
                }
                const int nb = g.idx(nx, nz);
                const float dist = g.cell * ((dx != 0 && dz != 0) ? 1.41421356f : 1.0f);
                const float dh = std::fabs(g.at(nx, nz) - g.at(cx, cz));
                float k = slope_k;
                if (summit_approach) {
                    const float ddx = static_cast<float>(nx - gx) * g.cell;
                    const float ddz = static_cast<float>(nz - gz) * g.cell;
                    if (ddx * ddx + ddz * ddz < SUMMIT_APPROACH_M * SUMMIT_APPROACH_M) {
                        k = SUMMIT_SLOPE_K;
                    }
                }
                float step = dist * (1.0f + k * (dh / dist));
                if (!visible_mask.empty()) {
                    step *= 1.0f + hide_weight * static_cast<float>(
                                                     visible_mask[static_cast<std::size_t>(nb)]);
                }
                const float ng = gscore[static_cast<std::size_t>(cur)] + step;
                if (ng < gscore[static_cast<std::size_t>(nb)] - 1e-5f) {
                    gscore[static_cast<std::size_t>(nb)] = ng;
                    came[static_cast<std::size_t>(nb)] = cur;
                    open.emplace(ng + heuristic(nb), nb);
                }
            }
        }
    }
    std::vector<int> path;
    for (int c = goal; c >= 0; c = came[static_cast<std::size_t>(c)]) {
        path.push_back(c);
        if (c == start) {
            break;
        }
    }
    std::reverse(path.begin(), path.end());
    if (path.empty() || path.front() != start) {
        return {};
    }
    return path;
}

float polyline_length(const std::vector<glm::vec2>& p) {
    float l = 0.0f;
    for (std::size_t i = 1; i < p.size(); ++i) {
        l += glm::length(p[i] - p[i - 1]);
    }
    return l;
}

/// Chaikin-style corner cutting, twice: an 8-neighbour A* result is a
/// staircase of 45-degree turns, and a staircase is not a desire line. Two
/// rounds is enough to remove the lattice and few enough to keep the route on
/// the ground the search chose.
std::vector<glm::vec2> smooth(std::vector<glm::vec2> p) {
    for (int pass = 0; pass < 2; ++pass) {
        if (p.size() < 3) {
            break;
        }
        std::vector<glm::vec2> out;
        out.push_back(p.front());
        for (std::size_t i = 0; i + 1 < p.size(); ++i) {
            out.push_back(p[i] * 0.75f + p[i + 1] * 0.25f);
            out.push_back(p[i] * 0.25f + p[i + 1] * 0.75f);
        }
        out.push_back(p.back());
        p = std::move(out);
    }
    return p;
}

/// Resample a polyline at fixed spacing — BR-1 measures at 4 m stations, and a
/// route whose station density varies would weight its own measurement.
std::vector<glm::vec2> resample(const std::vector<glm::vec2>& p, float step) {
    std::vector<glm::vec2> out;
    if (p.empty()) {
        return out;
    }
    out.push_back(p.front());
    float carry = 0.0f;
    for (std::size_t i = 1; i < p.size(); ++i) {
        glm::vec2 a = p[i - 1];
        const glm::vec2 b = p[i];
        float seg = glm::length(b - a);
        while (carry + seg >= step) {
            const float t = (step - carry) / seg;
            a = a + (b - a) * t;
            out.push_back(a);
            seg = glm::length(b - a);
            carry = 0.0f;
        }
        carry += seg;
    }
    if (glm::length(out.back() - p.back()) > 0.5f) {
        out.push_back(p.back());
    }
    return out;
}

} // namespace

float path_half_width(PathClass c) { return half_width_of(c); }

namespace {

/// Squared distance from p to segment [a,b], plus the parameter along it.
struct SegHit { float d2; float t; };
SegHit seg_distance(glm::vec2 p, glm::vec2 a, glm::vec2 b) {
    const glm::vec2 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    const float t = (len2 < 1e-9f) ? 0.0f : std::clamp(glm::dot(p - a, ab) / len2, 0.0f, 1.0f);
    const glm::vec2 q = a + ab * t;
    return {glm::dot(p - q, p - q), t};
}

} // namespace

/// The one traversal both queries share: nearest centreline segment in the
/// query point's bin. Every segment registers itself into EVERY bin its reach
/// touches, so one bin is enough and the query is O(few).
PathNetwork::Nearest PathNetwork::nearest(glm::vec2 world) const {
    Nearest best;
    if (bins <= 0) {
        return best;
    }
    const int bx = static_cast<int>(std::floor((world.x - origin.x) / bin_m));
    const int bz = static_cast<int>(std::floor((world.y - origin.y) / bin_m));
    if (bx < 0 || bz < 0 || bx >= bins || bz >= bins) {
        return best;
    }
    const auto cell_i = static_cast<std::size_t>(bz) * static_cast<std::size_t>(bins)
                      + static_cast<std::size_t>(bx);
    for (uint32_t k = bin_start[cell_i]; k < bin_start[cell_i + 1]; ++k) {
        const uint32_t packed = bin_items[k];
        const auto ri = static_cast<std::size_t>(packed >> 16);
        const auto si = static_cast<std::size_t>(packed & 0xFFFFu);
        const PathRoute& r = routes[ri];
        if (si + 1 >= r.points.size()) {
            continue;
        }
        const SegHit hit = seg_distance(world, r.points[si], r.points[si + 1]);
        if (hit.d2 < best.d2) {
            best.d2 = hit.d2;
            // The class and the tread height belong to the nearer END of the
            // segment: interpolating a class id would invent classes that do
            // not exist, and the tread profile is defined at the stations.
            const std::size_t at = (hit.t < 0.5f) ? si : si + 1;
            best.cls = r.classes[at];
            best.height = r.heights[at];
            best.found = true;
        }
    }
    return best;
}

float PathNetwork::flatten_at(glm::vec2 world, float ground_height) const {
    const Nearest nr = nearest(world);
    if (!nr.found) {
        return 0.0f;
    }
    const float d = std::sqrt(nr.d2);
    const float hw = path_half_width(nr.cls);
    const float outer = hw + flatten_blend_m;
    if (d > outer) {
        return 0.0f;
    }
    // Full flattening on the tread, easing to nothing by `outer`. The groove
    // (PATH_GROOVE_DEPTH) rides ON TOP of the flattening, not instead of it: a
    // path is both LEVEL and SUNK, and only the level part is what makes it
    // read as a path from a distance.
    const float w = (d <= hw) ? 1.0f : 1.0f - (d - hw) / flatten_blend_m;
    const float ease = w * w * (3.0f - 2.0f * w);
    return (nr.height - ground_height) * ease
         - static_cast<float>(config::PATH_GROOVE_DEPTH) * ease;
}

PathSample PathNetwork::sample(glm::vec2 world) const {
    PathSample s;
    const Nearest nr = nearest(world);
    if (!nr.found) {
        return s;
    }
    s.dist_to_center = std::sqrt(nr.d2);
    s.path_class = nr.cls;
    s.worn_half_width = path_half_width(nr.cls);
    // FLORA'S DATUM (FloraEdgeRules.h): 0 is the outer edge of the worn
    // surface, outward. Reported, never left to the consumer to reconstruct.
    s.dist_from_worn_edge = s.dist_to_center - s.worn_half_width;

    // The three bands (research A6, §8.1 item 1) as a GRADIENT, never a
    // ribbon: worn centre -> pressed margins -> rich edge. BOTH RAMPS LIVE IN
    // core/math (SurfaceField.h) because render draws the tread from them and
    // flora plants the verge against them — see the notice in that header.
    s.wear = math::path_wear_profile(s.dist_to_center / s.worn_half_width);
    s.edge = math::path_edge_profile(s.dist_from_worn_edge, rich_edge_band_m);
    return s;
}

void path_render_stations(const PathNetwork& net, std::vector<math::PathStation>& stations,
                          std::vector<uint32_t>& route_offsets,
                          std::vector<math::PathGoalMark>& goals) {
    stations.clear();
    route_offsets.clear();
    goals.clear();
    route_offsets.reserve(net.routes.size() + 1);
    for (const PathRoute& r : net.routes) {
        route_offsets.push_back(static_cast<uint32_t>(stations.size()));
        for (std::size_t i = 0; i < r.points.size(); ++i) {
            math::PathStation st;
            st.position = r.points[i];
            st.tread_height = r.heights[i];
            st.worn_half_width = path_half_width(r.classes[i]);
            st.path_class = static_cast<uint8_t>(r.classes[i]);
            stations.push_back(st);
        }
    }
    // CSR: the terminator is what makes `route i = [off[i], off[i+1])` work for
    // the LAST route as well. Emitted unconditionally, so an empty network is a
    // single-element offsets array and zero routes rather than a special case.
    route_offsets.push_back(static_cast<uint32_t>(stations.size()));
    for (const Goal& g : net.goals) {
        math::PathGoalMark m;
        m.position = g.position;
        m.height = g.height;
        m.kind = static_cast<uint8_t>(g.kind);
        m.importance = g.importance;
        goals.push_back(m);
    }
}

float PathNetwork::max_detour_ratio() const {
    float worst = 1.0f;
    for (const PathRoute& r : routes) {
        if (r.optimal_length_m > 1.0f) {
            worst = std::max(worst, r.length_m / r.optimal_length_m);
        }
    }
    return worst;
}

PathNetwork build_path_network(uint64_t seed, const TestbedLayout& layout, glm::vec2 domain_min,
                               glm::vec2 domain_max, const PathParams& params,
                               const std::function<float(glm::vec2)>& height) {
    PathNetwork net;
    Grid g;
    g.origin = domain_min;
    g.cell = params.grid_cell;
    g.n = std::max(8, static_cast<int>((domain_max.x - domain_min.x) / params.grid_cell) + 1);
    g.h.resize(static_cast<std::size_t>(g.n) * static_cast<std::size_t>(g.n));
    for (int z = 0; z < g.n; ++z) {
        for (int x = 0; x < g.n; ++x) {
            g.h[static_cast<std::size_t>(g.idx(x, z))] = height(g.world(x, z));
        }
    }

    // ---- goal siting (BR-2 clause (i): the endpoints are REAL) -------------
    // Each goal is sited by the rule its kind implies, not dropped at random:
    // a spring belongs in a swale floor, a hut on flat ground, a cairn on a
    // crest. A goal placed against its own rule is the "boulder on flat
    // ground" error of §2.10 LF-5 in another costume.
    const auto grade_at = [&](glm::vec2 w) {
        const float hx = g.sample({w.x + 4.0f, w.y}) - g.sample({w.x - 4.0f, w.y});
        const float hz = g.sample({w.x, w.y + 4.0f}) - g.sample({w.x, w.y - 4.0f});
        return std::sqrt(hx * hx + hz * hz) / 8.0f;
    };
    const auto relative_height = [&](glm::vec2 w) {
        // height above the local 60 m neighbourhood mean: >0 crest, <0 swale
        float sum = 0.0f;
        int cnt = 0;
        for (int i = 0; i < 8; ++i) {
            const float ang = static_cast<float>(i) * 0.7853981f;
            sum += g.sample(w + glm::vec2{std::cos(ang), std::sin(ang)} * 30.0f);
            ++cnt;
        }
        return g.sample(w) - sum / static_cast<float>(cnt);
    };
    const glm::vec2 span = domain_max - domain_min;
    const auto site = [&](GoalKind kind, uint32_t stream, glm::vec2 anchor, float radius,
                          float importance) {
        // Best of a seeded candidate set, scored by the kind's own rule.
        glm::vec2 best = anchor;
        float best_score = -1e9f;
        for (int i = 0; i < 64; ++i) {
            const float u = noise::lattice_value(seed, stream, i, 0);
            const float v = noise::lattice_value(seed, stream, i, 1);
            const float ang = u * 6.2831853f;
            const float rr = radius * std::sqrt(v);
            const glm::vec2 c = anchor + glm::vec2{std::cos(ang), std::sin(ang)} * rr;
            if (c.x < domain_min.x + 40.0f || c.y < domain_min.y + 40.0f
                || c.x > domain_max.x - 40.0f || c.y > domain_max.y - 40.0f) {
                continue;
            }
            const float rel = relative_height(c);
            const float gr = grade_at(c);
            float score = 0.0f;
            switch (kind) {
            case GoalKind::ClearingShrine: score = -std::fabs(rel) - gr * 20.0f; break;
            case GoalKind::Spring: score = -rel * 4.0f - gr * 10.0f; break;      // low and level
            case GoalKind::WoodcuttersHut: score = -gr * 30.0f; break;           // flat
            case GoalKind::SpireGroup: score = rel * 2.0f + gr * 4.0f; break;    // broken high ground
            // The cairn wants a crest with STEEP GROUND UNDER IT, not merely a
            // high one: the stone steps class only exists where a route has to
            // climb, and a cairn on a gentle swell gives it nowhere to be.
            case GoalKind::CrestCairn: score = rel * 5.0f + gr * 20.0f; break;
            }
            if (score > best_score) {
                best_score = score;
                best = c;
            }
        }
        net.goals.push_back({kind, best, g.sample(best), importance});
    };
    // The shrine is anchored in the authored glade (в9's preserved plain): the
    // one place on the stand where a made thing reads as made.
    site(GoalKind::ClearingShrine, 300, layout.forests.forced_clearing_center,
         layout.forests.forced_clearing_radius * 0.5f, 1.0f);
    site(GoalKind::Spring, 301, domain_min + span * glm::vec2{0.22f, 0.30f}, 110.0f, 0.4f);
    site(GoalKind::WoodcuttersHut, 302, domain_min + span * glm::vec2{0.78f, 0.24f}, 110.0f, 0.6f);
    site(GoalKind::SpireGroup, 303, domain_min + span * glm::vec2{0.80f, 0.78f}, 110.0f, 0.5f);
    site(GoalKind::CrestCairn, 304, domain_min + span * glm::vec2{0.25f, 0.80f}, 110.0f, 0.3f);

    const auto node_of = [&](glm::vec2 w) {
        const int x = std::clamp(static_cast<int>((w.x - g.origin.x) / g.cell), 1, g.n - 2);
        const int z = std::clamp(static_cast<int>((w.y - g.origin.y) / g.cell), 1, g.n - 2);
        return g.idx(x, z);
    };

    // ---- the network: a star from the largest goal plus one ring hop -------
    // Not the complete graph (that would pave the map) and not a bare tree
    // (a tree gives the walker no loop). Every edge ends at a registered goal,
    // which is BR-2 clause (i) by construction.
    std::vector<std::pair<int, int>> edges;
    for (std::size_t i = 1; i < net.goals.size(); ++i) {
        edges.emplace_back(0, static_cast<int>(i));
    }
    // Two cross-links, one per side. §8.1 item 3 needs >= 2 km of network for
    // the find cadence to yield a DISTRIBUTION rather than an anecdote, and the
    // star alone measured 1.87 km; they also give the walker a loop instead of
    // five there-and-backs.
    edges.emplace_back(1, 4); // spring -> crest cairn
    edges.emplace_back(2, 3); // hut -> spire group

    for (const auto& [ia, ib] : edges) {
        const Goal& a = net.goals[static_cast<std::size_t>(ia)];
        const Goal& b = net.goals[static_cast<std::size_t>(ib)];
        const int start = node_of(a.position);
        const int goal_node = node_of(b.position);

        // BR-1's term: where can the destination already be seen from? Built
        // once per route over the whole grid, on the same height field the
        // router walks.
        std::vector<uint8_t> vis(static_cast<std::size_t>(g.n) * static_cast<std::size_t>(g.n), 0);
        for (int z = 0; z < g.n; ++z) {
            for (int x = 0; x < g.n; ++x) {
                vis[static_cast<std::size_t>(g.idx(x, z))] =
                    visible(g, g.world(x, z), b.position, 2.0f) ? 1u : 0u;
            }
        }

        const bool summit = b.kind == GoalKind::CrestCairn || b.kind == GoalKind::SpireGroup;
        const std::vector<int> bent = astar(g, start, goal_node, params.slope_k, vis,
                                            params.hide_weight, summit);
        // The SAME search without the BR-1 term is the cost-optimal route
        // BR-2 clause (ii) measures against. It is not a second algorithm: the
        // comparison would be meaningless if it were.
        const std::vector<int> plain = astar(g, start, goal_node, params.slope_k, {}, 0.0f, summit);
        if (bent.empty() || plain.empty()) {
            continue;
        }
        const auto to_points = [&](const std::vector<int>& nodes) {
            std::vector<glm::vec2> pts;
            pts.reserve(nodes.size());
            for (const int nd : nodes) {
                pts.push_back(g.world(nd % g.n, nd / g.n));
            }
            pts.front() = a.position;
            pts.back() = b.position;
            return resample(smooth(std::move(pts)), params.station_m);
        };

        PathRoute r;
        r.goal_a = ia;
        r.goal_b = ib;
        r.points = to_points(bent);
        r.length_m = polyline_length(r.points);
        r.optimal_length_m = polyline_length(to_points(plain));

        // BR-1 measurement at the ruled stations: the longest contiguous run
        // from which the destination is occluded at eye height. The run's
        // MIDDLE station is recorded with it, because the acceptance frame has
        // to be shot from the standpoint the number was taken at.
        float run = 0.0f;
        std::size_t run_begin = 0;
        std::size_t best_begin = 0;
        std::size_t best_end = 0;
        bool have_run = false;
        for (std::size_t i = 0; i + 1 < r.points.size(); ++i) {
            // The last stations are excluded by CAUSE, not by magnitude
            // (Rule 36): within one station of the goal the "destination" is
            // under the walker's feet and occlusion is undefined there.
            if (glm::length(r.points[i] - b.position) < params.station_m) {
                continue;
            }
            if (!visible(g, r.points[i], b.position, 2.0f)) {
                if (run <= 0.0f) {
                    run_begin = i;
                }
                run += glm::length(r.points[i + 1] - r.points[i]);
                if (run > r.longest_hidden_run_m) {
                    r.longest_hidden_run_m = run;
                    best_begin = run_begin;
                    best_end = i;
                    have_run = true;
                }
            } else {
                run = 0.0f;
            }
        }

        // ---- the BR-1 acceptance PAIR ------------------------------------
        // Not the middle of the run, and not the longest range: the station
        // inside the run whose distance to the goal is closest to
        // BR1_FRAME_RANGE_M. The middle was the obvious choice and the wrong
        // one — on the long route it sat 400 m out, where a shrine covers
        // three pixels of a 360-line frame whether or not a hill is in front
        // of it. A frame in which the subject would be invisible ANYWAY cannot
        // fail (Rule 27), so the standpoint is chosen at a range where the
        // goal WOULD read if the trace let it.
        if (have_run) {
            float best_err = 1e18f;
            for (std::size_t i = best_begin; i <= best_end; ++i) {
                const float err =
                    std::fabs(glm::length(r.points[i] - b.position) - BR1_FRAME_RANGE_M);
                if (err < best_err) {
                    best_err = err;
                    r.hidden_station = static_cast<int>(i);
                }
            }
        }
        // The paired CONTROL standpoint: same route, same goal, matched RANGE,
        // goal visible. Matching the range is what makes it a control rather
        // than a second picture — an unmatched station would differ in two
        // things at once (where it stands AND how far away the goal is), and a
        // reader could then credit the distance for the disappearance. A route
        // that has no visible station at a comparable range therefore yields NO
        // control, and forest_vantages skips it rather than shipping half a
        // pair.
        if (r.hidden_station >= 0) {
            const float ref =
                glm::length(r.points[static_cast<std::size_t>(r.hidden_station)] - b.position);
            float best_err = 1e18f;
            for (std::size_t i = 0; i + 1 < r.points.size(); ++i) {
                const float d = glm::length(r.points[i] - b.position);
                if (d < params.station_m || !visible(g, r.points[i], b.position, 2.0f)) {
                    continue;
                }
                const float err = std::fabs(d - ref);
                if (err < best_err) {
                    best_err = err;
                    r.visible_station = static_cast<int>(i);
                }
            }
        }

        // ---- class along the route (§8.1 item 1's rule) --------------------
        r.classes.resize(r.points.size(), PathClass::Dirt);
        r.heights.resize(r.points.size(), 0.0f);
        for (std::size_t i = 0; i < r.points.size(); ++i) {
            const glm::vec2 p = r.points[i];
            const std::size_t j = (i + 1 < r.points.size()) ? i + 1 : i - 1;
            const float dh = std::fabs(g.sample(r.points[j]) - g.sample(p));
            const float grade = dh / std::max(1e-3f, glm::length(r.points[j] - p));
            // THE CLASS IS A PROPERTY OF THE STATION, NOT OF THE ROUTE. A
            // walk from the shrine to the spring reads cobble, then dirt, then
            // a hint-path as the destination gets smaller — keying the class
            // off the destination alone painted whole routes faint.
            const float d_big = glm::length(p - net.goals[0].position);
            const float d_small = glm::length(p - b.position);
            PathClass c = PathClass::Dirt;
            if (grade > STEPS_GRADE) {
                c = PathClass::StoneSteps;      // the slope demands them
            } else if (d_big < COBBLE_RADIUS_M) {
                c = PathClass::Cobble;          // the approach to the largest goal
            } else if (b.importance < 0.45f && d_small < FAINT_RADIUS_M) {
                c = PathClass::FaintTrail;      // thinning out toward a small goal
            }
            r.classes[i] = c;
            r.heights[i] = g.sample(p);
        }
        // A PATH IS FLATTER THAN ITS SURROUNDINGS: smooth the longitudinal
        // profile. Three passes of a 1-2-1 kernel with the ends pinned — the
        // tread must still MEET the ground it leaves and arrives on, or the
        // path becomes a ramp hanging in the air at both ends.
        // Eight passes, not three. The kernel is a Gaussian of sigma ~= 2.4 m
        // per three passes at 4 m stations, and the §2.7 micro octave it has to
        // remove is 8-16 m: at three passes the tread still carried it and the
        // path measured only 15% flatter than the ground beside it. Eight
        // passes (sigma ~= 8 m) clears the micro tier while still following the
        // meso relief over the 50-100 m a route actually climbs.
        for (int pass = 0; pass < 8; ++pass) {
            std::vector<float> sm = r.heights;
            for (std::size_t i = 1; i + 1 < sm.size(); ++i) {
                sm[i] = 0.25f * r.heights[i - 1] + 0.5f * r.heights[i] + 0.25f * r.heights[i + 1];
            }
            r.heights = std::move(sm);
        }
        net.routes.push_back(std::move(r));
    }

    // ---- the segment bin index -------------------------------------------
    // Every segment registers into every bin its REACH touches, so a query
    // reads exactly one bin. Reach is the widest thing any consumer asks
    // about: the tread, the flattening ease-out and BR-3's edge band.
    net.origin = domain_min;
    net.flatten_blend_m = params.flatten_blend_m;
    net.rich_edge_band_m = params.rich_edge_band_m;
    float widest = 0.0f;
    for (int c = 0; c < 4; ++c) {
        widest = std::max(widest, half_width_of(static_cast<PathClass>(c)));
    }
    const float reach = widest + params.flatten_blend_m + params.rich_edge_band_m;
    net.bin_m = std::max(8.0f, reach);
    net.bins = std::max(1, static_cast<int>((domain_max.x - domain_min.x) / net.bin_m) + 1);
    const auto bin_cells = static_cast<std::size_t>(net.bins) * static_cast<std::size_t>(net.bins);
    std::vector<uint32_t> counts(bin_cells + 1, 0);
    const auto for_each_bin = [&](const auto& fn) {
        for (std::size_t ri = 0; ri < net.routes.size(); ++ri) {
            const PathRoute& r = net.routes[ri];
            for (std::size_t si = 0; si + 1 < r.points.size(); ++si) {
                const glm::vec2 a2 = r.points[si];
                const glm::vec2 b2 = r.points[si + 1];
                const int x0 = static_cast<int>(
                    std::floor((std::min(a2.x, b2.x) - net.origin.x - reach) / net.bin_m));
                const int x1 = static_cast<int>(
                    std::floor((std::max(a2.x, b2.x) - net.origin.x + reach) / net.bin_m));
                const int z0 = static_cast<int>(
                    std::floor((std::min(a2.y, b2.y) - net.origin.y - reach) / net.bin_m));
                const int z1 = static_cast<int>(
                    std::floor((std::max(a2.y, b2.y) - net.origin.y + reach) / net.bin_m));
                for (int z = std::max(0, z0); z <= std::min(net.bins - 1, z1); ++z) {
                    for (int x = std::max(0, x0); x <= std::min(net.bins - 1, x1); ++x) {
                        fn(static_cast<std::size_t>(z) * static_cast<std::size_t>(net.bins)
                               + static_cast<std::size_t>(x),
                           static_cast<uint32_t>((ri << 16) | si));
                    }
                }
            }
        }
    };
    for_each_bin([&](std::size_t cell, uint32_t) { ++counts[cell + 1]; });
    for (std::size_t i = 1; i <= bin_cells; ++i) {
        counts[i] += counts[i - 1];
    }
    net.bin_start = counts;
    net.bin_items.assign(counts[bin_cells], 0);
    std::vector<uint32_t> cursor(net.bin_start.begin(), net.bin_start.end() - 1);
    for_each_bin([&](std::size_t cell, uint32_t packed) { net.bin_items[cursor[cell]++] = packed; });
    return net;
}

} // namespace dfn::world
