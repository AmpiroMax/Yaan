/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 14:41:26
Module: engine/world
File: engine/world/sources/WorldgenHydrology.cpp

Responsibility:
- P2 hydrology implementation (LANDSCAPE.md §3.1-§3.2): coarse-grid descent
  trace with pond-and-spill, Chaikin smoothing + sinuosity jitter, monotonic
  water levels, ford-adjusted trapezoid carve, coarse distance-to-water field.

Key items:
- build_hydrology, water_at.

Dependencies:
- Uses: WorldgenHydrology.h, WorldgenMacro.h, WorldgenNoise.h, config.
- Used by: dfn_world.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM (Rule 13.1): every heap/argmin breaks ties by lowest index;
  iteration orders fixed; randomness only via WorldgenNoise streams.
- MONOTONIC WATER INVARIANT: w[i] = min(w[i-1], effective terrain) by
  construction; build sets ok=false on any violation instead of emitting a
  climbing river.
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — P2 implementation.
- 09:08:2026 - 13:12:19: Stage 3b amendments: §3.3 mud cap prunes pond water beyond max(SHORE_SAND_DIST, 2x width) of the trace; fords derived from corridor x trace crossings + FORD_SPACING_MAX gap fill; channel bed clamped into the trapezoid band (fords raise the bed); corridor-mask stations ford-shallow; pond beds raised on corridor crossings; dist_to_water saturated at DIST_TO_WATER_RANGE; station bins built early + binned nearest queries (wilderness contexts were quadratic: 9.8 s -> 0.9 s at 21x21 chunks).
- 09:08:2026 - 13:28:27: Split: per-sample query side (water_sample_impl/water_at/carve_height) moved to WorldgenWater.cpp (file was at 780/800 lines); build side stays here.
- 09:08:2026 - 14:41:26: Frame-05 bed fix (ROOT CAUSE): fill_level no longer doubles as the Dijkstra seed set — river trace cells seed a LOCAL set instead, so trace cells stop flooding their whole 16 m coarse cell in water_at's pond branch (67 station-only cells -> 0; WaterBed 24.6k -> 18.7k m2, water coverage 2.30% -> 1.78%). Pond primitives built from cell FOOTPRINTS (water_at floods whole cells).
*/

#include "engine/world/sources/WorldgenHydrology.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenNoise.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <limits>
#include <numbers>
#include <queue>

namespace dfn::world {

namespace {

using noise::smoothstep01;

constexpr float CELL = static_cast<float>(config::WORLDGEN_HYDRO_GRID_STEP);
constexpr float STATION_STEP = static_cast<float>(config::RIVER_STATION_SPACING);
constexpr float LAKE_LEVEL_M = static_cast<float>(config::LAKE_LEVEL_TESTBED);
constexpr float RIVER_DEPTH_M = static_cast<float>(config::RIVER_DEPTH);
constexpr float FORD_DEPTH_M = static_cast<float>(config::FORD_DEPTH_MAX);
constexpr float FORD_SPAN_M = static_cast<float>(config::FORD_SPAN);
constexpr float WIDTH_MIN_M = static_cast<float>(config::RIVER_WIDTH_MIN);
constexpr float WIDTH_MAX_M = static_cast<float>(config::RIVER_WIDTH_MAX);
constexpr float SINUOSITY_MIN = static_cast<float>(config::RIVER_SINUOSITY_MIN);
constexpr float SAND_DIST_M = static_cast<float>(config::SHORE_SAND_DIST);
constexpr float FORD_SPACING_MAX_M = static_cast<float>(config::FORD_SPACING_MAX);
constexpr float BIN_SIZE = 2.0f * CELL; // station bin span; 3x3 bins cover >= 32 m

/// 2D segment intersection (proper crossings and touching endpoints both
/// count — a corridor grazing the channel still needs its ford).
[[nodiscard]] bool segments_cross(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 d) {
    const auto orient = [](glm::vec2 p, glm::vec2 q, glm::vec2 r) {
        const float v = (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x);
        return v > 0.0f ? 1 : (v < 0.0f ? -1 : 0);
    };
    const int o1 = orient(a, b, c), o2 = orient(a, b, d);
    const int o3 = orient(c, d, a), o4 = orient(c, d, b);
    return o1 != o2 && o3 != o4;
}

/// Heap entry ordered by (value, index) — index tie-break keeps every
/// selection deterministic.
struct HeapEntry {
    float value;
    uint32_t index;
    bool operator>(const HeapEntry& o) const {
        return value > o.value || (value == o.value && index > o.index);
    }
};
using MinHeap = std::priority_queue<HeapEntry, std::vector<HeapEntry>, std::greater<HeapEntry>>;

struct Grid {
    glm::vec2 origin{0.0f};
    uint32_t w = 0, h = 0;
    std::vector<float> height; ///< macro terrain at nodes
    std::vector<float> eff;    ///< water-filled terrain (ponds/lake raised)

    [[nodiscard]] glm::vec2 pos(uint32_t i) const {
        const uint32_t x = i % w;
        const uint32_t z = i / w;
        return origin
             + glm::vec2{static_cast<float>(x) * CELL, static_cast<float>(z) * CELL};
    }
    [[nodiscard]] bool boundary(uint32_t i) const {
        const uint32_t x = i % w, z = i / w;
        return x == 0 || z == 0 || x == w - 1 || z == h - 1;
    }
    /// 8-neighbors in fixed order (row-major offsets) — deterministic.
    template <typename Fn> void neighbors(uint32_t i, Fn&& fn) const {
        const int32_t x = static_cast<int32_t>(i % w), z = static_cast<int32_t>(i / w);
        for (int32_t dz = -1; dz <= 1; ++dz) {
            for (int32_t dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dz == 0) continue;
                const int32_t nx = x + dx, nz = z + dz;
                if (nx < 0 || nz < 0 || nx >= static_cast<int32_t>(w)
                    || nz >= static_cast<int32_t>(h)) {
                    continue;
                }
                fn(static_cast<uint32_t>(nz) * w + static_cast<uint32_t>(nx));
            }
        }
    }
};

constexpr uint32_t INVALID = std::numeric_limits<uint32_t>::max();

/// Greedy descent with pond-and-spill (§3.1 step 2). Returns the cell path;
/// fills ponds into `grid.eff` / `fill_level` / `ponds`. `avoid_lake` keeps
/// the outlet trace from falling back into the lake basin.
std::vector<uint32_t> trace_descent(Grid& grid, uint32_t start, const LakeStamp& lake,
                                    bool avoid_lake, std::vector<float>& fill_level,
                                    std::vector<Pond>& ponds, bool& reached_lake, bool& ok) {
    std::vector<uint32_t> path{start};
    reached_lake = false;
    uint32_t cur = start;
    const uint32_t cap = grid.w * grid.h * 4;
    std::vector<uint8_t> flood_mark(grid.eff.size(), 0);

    auto in_lake = [&](uint32_t i) { return lake_norm_radius(lake, grid.pos(i)) < 1.0f; };

    for (uint32_t iter = 0; iter < cap; ++iter) {
        if (grid.boundary(cur)) {
            return path;
        }
        if (!avoid_lake && in_lake(cur)) {
            reached_lake = true;
            return path;
        }
        uint32_t best = INVALID;
        grid.neighbors(cur, [&](uint32_t n) {
            if (avoid_lake && in_lake(n)) return;
            if (best == INVALID || grid.eff[n] < grid.eff[best]
                || (grid.eff[n] == grid.eff[best] && n < best)) {
                best = n;
            }
        });
        if (best != INVALID && grid.eff[best] < grid.eff[cur]) {
            cur = best;
            path.push_back(cur);
            continue;
        }
        // Local minimum: pond-and-spill (fill to the lowest saddle, exit there).
        std::fill(flood_mark.begin(), flood_mark.end(), 0);
        MinHeap heap;
        std::vector<uint32_t> region{cur};
        flood_mark[cur] = 1;
        float level = grid.eff[cur];
        grid.neighbors(cur, [&](uint32_t n) {
            if (avoid_lake && in_lake(n)) return;
            if (!flood_mark[n]) {
                flood_mark[n] = 1;
                heap.push({grid.eff[n], n});
            }
        });
        uint32_t spill = INVALID;
        while (!heap.empty()) {
            const HeapEntry e = heap.top();
            heap.pop();
            if (e.value < level) {
                spill = e.index; // downhill beyond the saddle — water exits here
                break;
            }
            level = e.value;
            region.push_back(e.index);
            if (grid.boundary(e.index)) {
                spill = e.index; // pond overflows off the map edge
                break;
            }
            grid.neighbors(e.index, [&](uint32_t n) {
                if (avoid_lake && in_lake(n)) return;
                if (!flood_mark[n]) {
                    flood_mark[n] = 1;
                    heap.push({grid.eff[n], n});
                }
            });
        }
        if (spill == INVALID) {
            ok = false; // the whole domain is a bowl — failed generation
            return path;
        }
        Pond pond;
        pond.level = level;
        for (const uint32_t c : region) {
            if (grid.height[c] < level) {
                pond.cells.push_back(c);
                grid.eff[c] = std::max(grid.eff[c], level);
                fill_level[c] = std::max(fill_level[c] == math::NO_WATER ? level : fill_level[c],
                                         level);
            }
        }
        if (!pond.cells.empty()) {
            std::sort(pond.cells.begin(), pond.cells.end());
            ponds.push_back(std::move(pond));
        }
        path.push_back(spill);
        cur = spill;
    }
    ok = false; // iteration cap — failed generation
    return path;
}

/// Chaikin corner-cutting, endpoints kept. Two rounds.
std::vector<glm::vec2> chaikin(const std::vector<glm::vec2>& in) {
    std::vector<glm::vec2> pts = in;
    for (int round = 0; round < 2; ++round) {
        if (pts.size() < 3) break;
        std::vector<glm::vec2> out;
        out.reserve(pts.size() * 2);
        out.push_back(pts.front());
        for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
            out.push_back(pts[i] * 0.75f + pts[i + 1] * 0.25f);
            out.push_back(pts[i] * 0.25f + pts[i + 1] * 0.75f);
        }
        out.push_back(pts.back());
        pts = std::move(out);
    }
    return pts;
}

float polyline_length(const std::vector<glm::vec2>& pts) {
    float len = 0.0f;
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        len += glm::length(pts[i + 1] - pts[i]);
    }
    return len;
}

/// Resamples a polyline at STATION_STEP intervals (endpoints included).
std::vector<glm::vec2> resample(const std::vector<glm::vec2>& pts) {
    std::vector<glm::vec2> out;
    if (pts.empty()) return out;
    out.push_back(pts.front());
    float carry = 0.0f;
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        glm::vec2 a = pts[i];
        const glm::vec2 b = pts[i + 1];
        float seg = glm::length(b - a);
        while (carry + seg >= STATION_STEP) {
            const float t = (STATION_STEP - carry) / seg;
            a = a + (b - a) * t;
            out.push_back(a);
            seg = glm::length(b - a);
            carry = 0.0f;
        }
        carry += seg;
    }
    if (carry > STATION_STEP * 0.25f) {
        out.push_back(pts.back());
    }
    return out;
}

/// Perpendicular sinuosity jitter (§3.1 step 3): deterministic 1D noise along
/// arclength, amplitude grown until sinuosity >= RIVER_SINUOSITY_MIN.
std::vector<glm::vec2> ensure_sinuosity(uint64_t seed, const std::vector<glm::vec2>& pts) {
    if (pts.size() < 3) return pts;
    const float straight = glm::length(pts.back() - pts.front());
    if (straight < 1.0f) return pts;
    if (polyline_length(pts) / straight >= SINUOSITY_MIN) return pts;

    std::vector<glm::vec2> best = pts;
    for (int attempt = 1; attempt <= 4; ++attempt) {
        const float amp = 2.0f * static_cast<float>(attempt);
        std::vector<glm::vec2> jittered = pts;
        float s = 0.0f;
        for (std::size_t i = 1; i + 1 < pts.size(); ++i) {
            s += glm::length(pts[i] - pts[i - 1]);
            const glm::vec2 dir = glm::normalize(pts[i + 1] - pts[i - 1]);
            const glm::vec2 perp{-dir.y, dir.x};
            const float t = s / polyline_length(pts);
            const float envelope = 4.0f * t * (1.0f - t);
            const float n =
                noise::value_noise(seed, STREAM_RIVER_JITTER, 40.0f, glm::vec2{s, 7.5f});
            jittered[i] = pts[i] + perp * ((n * 2.0f - 1.0f) * amp * envelope);
        }
        best = jittered;
        if (polyline_length(jittered) / straight >= SINUOSITY_MIN) break;
    }
    return best;
}

} // namespace

HydrologyData build_hydrology(uint64_t seed, const TestbedLayout& layout, glm::vec2 domain_min,
                              glm::vec2 domain_max) {
    HydrologyData hydro;
    hydro.ok = true;

    // --- Coarse grid over the domain --------------------------------------------
    Grid grid;
    grid.origin = domain_min;
    grid.w = static_cast<uint32_t>(std::round((domain_max.x - domain_min.x) / CELL)) + 1;
    grid.h = static_cast<uint32_t>(std::round((domain_max.y - domain_min.y) / CELL)) + 1;
    const std::size_t cells = static_cast<std::size_t>(grid.w) * grid.h;
    grid.height.resize(cells);
    grid.eff.resize(cells);
    hydro.fill_level.assign(cells, math::NO_WATER);
    for (uint32_t i = 0; i < cells; ++i) {
        grid.height[i] = macro_height(seed, layout, grid.pos(i));
        grid.eff[i] = grid.height[i];
    }
    hydro.grid_origin = grid.origin;
    hydro.grid_w = grid.w;
    hydro.grid_h = grid.h;

    // Lake pre-fill: basin water raises the effective surface (§3.2).
    for (uint32_t i = 0; i < cells; ++i) {
        if (lake_norm_radius(layout.lake, grid.pos(i)) < 1.0f
            && grid.height[i] < LAKE_LEVEL_M) {
            grid.eff[i] = LAKE_LEVEL_M;
            hydro.fill_level[i] = LAKE_LEVEL_M;
        }
    }
    hydro.lake = math::LakePlane{layout.lake.center, layout.lake.half_extent, LAKE_LEVEL_M};

    // --- Source: coarse-grid argmax near the layout source (§3.1 step 1) --------
    uint32_t source = INVALID;
    for (uint32_t i = 0; i < cells; ++i) {
        if (glm::length(grid.pos(i) - layout.river.source) > layout.river.source_search_radius) {
            continue;
        }
        if (source == INVALID || grid.height[i] > grid.height[source]) {
            source = i;
        }
    }
    if (source == INVALID) {
        hydro.ok = false;
        return hydro;
    }

    // --- Descent traces (source -> lake, lake outlet -> edge) -------------------
    bool reached_lake = false;
    const std::vector<uint32_t> path1 = trace_descent(
        grid, source, layout.lake, false, hydro.fill_level, hydro.ponds, reached_lake, hydro.ok);

    std::vector<uint32_t> path2;
    if (hydro.ok && reached_lake) {
        // Outlet = lowest cell ON THE CREST LINE (the outlet-biased levee
        // minimum). Only the outer part of the crest band qualifies: closer
        // to the waterline the crest ramps down to lake level everywhere, and
        // beyond the band the fade terrain dips below the crest — neither is
        // a point water can escape through without crossing the crest itself.
        uint32_t outlet = INVALID;
        const float ring_min = 1.0f + layout.lake.rim_band_frac * 0.75f;
        const float ring_max = 1.0f + layout.lake.rim_band_frac;
        for (uint32_t i = 0; i < cells; ++i) {
            const float q = lake_norm_radius(layout.lake, grid.pos(i));
            if (q < ring_min || q > ring_max) continue;
            if (outlet == INVALID || grid.eff[i] < grid.eff[outlet]) {
                outlet = i;
            }
        }
        if (outlet != INVALID) {
            bool dummy = false;
            path2 = trace_descent(grid, outlet, layout.lake, true, hydro.fill_level,
                                  hydro.ponds, dummy, hydro.ok);
        }
    }

    // --- Cells -> smoothed, sinuous, resampled stations -------------------------
    auto to_stations = [&](const std::vector<uint32_t>& cell_path) {
        std::vector<glm::vec2> pts;
        pts.reserve(cell_path.size());
        for (const uint32_t c : cell_path) pts.push_back(grid.pos(c));
        return resample(ensure_sinuosity(seed, chaikin(pts)));
    };
    const std::vector<glm::vec2> seg1 = to_stations(path1);
    const std::vector<glm::vec2> seg2 = to_stations(path2);

    hydro.segment_offsets.push_back(0);
    for (const auto* seg : {&seg1, &seg2}) {
        if (seg->size() < 2) continue;
        for (const glm::vec2 p : *seg) {
            hydro.stations.push_back(math::RiverStation{p, 0.0f, 0.0f});
        }
        hydro.segment_offsets.push_back(static_cast<uint32_t>(hydro.stations.size()));
    }

    auto cell_of = [&](glm::vec2 p) -> uint32_t {
        const int32_t x = static_cast<int32_t>(std::floor((p.x - grid.origin.x) / CELL));
        const int32_t z = static_cast<int32_t>(std::floor((p.y - grid.origin.y) / CELL));
        if (x < 0 || z < 0 || x >= static_cast<int32_t>(grid.w)
            || z >= static_cast<int32_t>(grid.h)) {
            return INVALID;
        }
        return static_cast<uint32_t>(z) * grid.w + static_cast<uint32_t>(x);
    };

    // --- Widths (grow source -> mouth; needed by the mud cap and fords) ---------
    std::vector<float> cum(hydro.stations.size(), 0.0f);
    for (std::size_t i = 1; i < hydro.stations.size(); ++i) {
        cum[i] = cum[i - 1]
               + glm::length(hydro.stations[i].position - hydro.stations[i - 1].position);
    }
    const float total = hydro.stations.empty() ? 1.0f : std::max(cum.back(), 1.0f);
    for (std::size_t i = 0; i < hydro.stations.size(); ++i) {
        const float t = cum[i] / total;
        hydro.stations[i].half_width = (WIDTH_MIN_M + (WIDTH_MAX_M - WIDTH_MIN_M) * t) * 0.5f;
    }

    // --- Station spatial bins (also used by the passes below — a wilderness
    // extent can carry tens of thousands of stations; brute-force nearest
    // scans made large-domain context builds quadratic) ---------------------------
    hydro.bin_size = BIN_SIZE;
    hydro.bins_w = static_cast<uint32_t>(std::ceil((domain_max.x - domain_min.x) / BIN_SIZE)) + 1;
    hydro.bins_h = static_cast<uint32_t>(std::ceil((domain_max.y - domain_min.y) / BIN_SIZE)) + 1;
    hydro.station_bins.assign(static_cast<std::size_t>(hydro.bins_w) * hydro.bins_h, {});
    for (uint32_t i = 0; i < hydro.stations.size(); ++i) {
        const glm::vec2 p = hydro.stations[i].position - hydro.grid_origin;
        const uint32_t bx = static_cast<uint32_t>(
            std::clamp(p.x / BIN_SIZE, 0.0f, static_cast<float>(hydro.bins_w - 1)));
        const uint32_t bz = static_cast<uint32_t>(
            std::clamp(p.y / BIN_SIZE, 0.0f, static_cast<float>(hydro.bins_h - 1)));
        hydro.station_bins[static_cast<std::size_t>(bz) * hydro.bins_w + bx].push_back(i);
    }
    /// Nearest station within the 3x3 bin ring (covers >= BIN_SIZE meters);
    /// INVALID when nothing is that close.
    const auto nearest_station_binned = [&](glm::vec2 p, float& out_d) -> uint32_t {
        const glm::vec2 rel = p - hydro.grid_origin;
        const int32_t bx = static_cast<int32_t>(std::floor(rel.x / BIN_SIZE));
        const int32_t bz = static_cast<int32_t>(std::floor(rel.y / BIN_SIZE));
        uint32_t best = INVALID;
        float best_d = std::numeric_limits<float>::max();
        for (int32_t dz = -1; dz <= 1; ++dz) {
            for (int32_t dx = -1; dx <= 1; ++dx) {
                const int32_t x = bx + dx, z = bz + dz;
                if (x < 0 || z < 0 || x >= static_cast<int32_t>(hydro.bins_w)
                    || z >= static_cast<int32_t>(hydro.bins_h)) {
                    continue;
                }
                for (const uint32_t i :
                     hydro.station_bins[static_cast<std::size_t>(z) * hydro.bins_w
                                        + static_cast<std::size_t>(x)]) {
                    const float d = glm::length(hydro.stations[i].position - p);
                    if (d < best_d) {
                        best_d = d;
                        best = i;
                    }
                }
            }
        }
        out_d = best_d;
        return best;
    };

    // --- §3.3 bed/mud cap: drain pond water beyond max(SHORE_SAND_DIST,
    // 2 x local river width) of the trace. The carve later cuts the channel
    // through the drained basin ("narrow the bend", §7.1a ruling) — wide
    // flooded flats cannot survive this pass. Lake water is never pruned.
    if (!hydro.stations.empty()) {
        for (uint32_t c = 0; c < cells; ++c) {
            if (hydro.fill_level[c] == math::NO_WATER) continue;
            const glm::vec2 p = grid.pos(c);
            if (lake_norm_radius(layout.lake, p) < 1.0f) continue;
            float best_d = 0.0f;
            const uint32_t best_i = nearest_station_binned(p, best_d);
            if (best_i == INVALID) {
                // Nothing within the bin ring (> BIN_SIZE >= any cap) — drain.
                hydro.fill_level[c] = math::NO_WATER;
                continue;
            }
            const float cap = std::max(SAND_DIST_M, 4.0f * hydro.stations[best_i].half_width);
            if (best_d > cap) {
                hydro.fill_level[c] = math::NO_WATER;
            }
        }
        for (Pond& pond : hydro.ponds) {
            std::erase_if(pond.cells, [&](uint32_t c) {
                return hydro.fill_level[c] == math::NO_WATER;
            });
        }
        std::erase_if(hydro.ponds, [](const Pond& p) { return p.cells.empty(); });
    }

    // --- Monotonic water levels (§3.1 step 4 — THE invariant; pruned fill) ------
    float w_prev = std::numeric_limits<float>::max();
    for (std::size_t s = 0; s + 1 < hydro.segment_offsets.size(); ++s) {
        if (s == 1) {
            w_prev = std::min(w_prev, LAKE_LEVEL_M); // outlet water starts at the lake plane
        }
        for (uint32_t i = hydro.segment_offsets[s]; i < hydro.segment_offsets[s + 1]; ++i) {
            const glm::vec2 p = hydro.stations[i].position;
            float e = macro_height(seed, layout, p);
            const uint32_t c = cell_of(p);
            if (c != INVALID && hydro.fill_level[c] != math::NO_WATER) {
                e = std::max(e, hydro.fill_level[c]); // flat across ponds/lake
            }
            w_prev = std::min(w_prev, e);
            hydro.stations[i].surface_height = w_prev;
        }
    }
    for (std::size_t i = 1; i < hydro.stations.size(); ++i) {
        if (hydro.stations[i].surface_height > hydro.stations[i - 1].surface_height + 1e-4f) {
            hydro.ok = false; // a climbing river = failed generation (§3.1)
        }
    }

    // --- Derived fords (§3.1 step 6, §7.1a: never tabled) ------------------------
    // One ford where each POI-chain corridor crosses the generated trace, plus
    // fills so no along-river gap exceeds FORD_SPACING_MAX.
    for (const CorridorLayout& corridor : layout.corridors) {
        for (int cs = 0; cs + 1 < corridor.point_count; ++cs) {
            const glm::vec2 a = corridor.points[cs];
            const glm::vec2 b = corridor.points[cs + 1];
            for (std::size_t i = 0; i + 1 < hydro.stations.size(); ++i) {
                if (i + 1 == hydro.segment_offsets[1] && hydro.segment_offsets.size() > 2) {
                    continue; // never bridge the lake gap between segments
                }
                if (segments_cross(a, b, hydro.stations[i].position,
                                   hydro.stations[i + 1].position)) {
                    hydro.ford_stations.push_back(static_cast<uint32_t>(i));
                }
            }
        }
    }
    if (!hydro.stations.empty()) {
        // Spacing minimum: walk gaps (including river start/end) and insert
        // mid-gap fords until every gap <= FORD_SPACING_MAX. Deterministic.
        std::sort(hydro.ford_stations.begin(), hydro.ford_stations.end());
        hydro.ford_stations.erase(
            std::unique(hydro.ford_stations.begin(), hydro.ford_stations.end()),
            hydro.ford_stations.end());
        bool inserted = true;
        while (inserted) {
            inserted = false;
            std::vector<float> marks{0.0f};
            for (const uint32_t f : hydro.ford_stations) marks.push_back(cum[f]);
            marks.push_back(total);
            std::sort(marks.begin(), marks.end());
            for (std::size_t g = 0; g + 1 < marks.size(); ++g) {
                if (marks[g + 1] - marks[g] <= FORD_SPACING_MAX_M) continue;
                const float mid = (marks[g] + marks[g + 1]) * 0.5f;
                uint32_t nearest = 0;
                float best = std::numeric_limits<float>::max();
                for (uint32_t i = 0; i < hydro.stations.size(); ++i) {
                    if (std::fabs(cum[i] - mid) < best) {
                        best = std::fabs(cum[i] - mid);
                        nearest = i;
                    }
                }
                hydro.ford_stations.push_back(nearest);
                std::sort(hydro.ford_stations.begin(), hydro.ford_stations.end());
                inserted = true;
                break;
            }
        }
    }

    // --- Ford-adjusted carve depths ----------------------------------------------
    hydro.carve_depth.assign(hydro.stations.size(), RIVER_DEPTH_M);
    // §2.4 "rivers crossed only at fords": every station inside a corridor's
    // mask is ford-shallow — an oblique crossing must be wade-deep across the
    // corridor's full width, not only at the exact intersection station.
    for (uint32_t i = 0; i < hydro.stations.size(); ++i) {
        if (corridor_distance(layout, hydro.stations[i].position)
            <= static_cast<float>(config::CORRIDOR_WIDTH) * 0.5f + 2.0f) {
            hydro.carve_depth[i] = FORD_DEPTH_M;
        }
    }
    for (const uint32_t ford : hydro.ford_stations) {
        for (uint32_t i = 0; i < hydro.stations.size(); ++i) {
            const float along = std::fabs(cum[i] - cum[ford]);
            if (along <= FORD_SPAN_M * 0.5f) {
                hydro.carve_depth[i] = std::min(hydro.carve_depth[i], FORD_DEPTH_M);
            } else if (along <= FORD_SPAN_M * 1.5f) {
                const float b = smoothstep01((along - FORD_SPAN_M * 0.5f) / FORD_SPAN_M);
                hydro.carve_depth[i] = std::min(
                    hydro.carve_depth[i], FORD_DEPTH_M + (RIVER_DEPTH_M - FORD_DEPTH_M) * b);
            }
        }
    }

    // --- Coarse distance-to-water field (multi-source Dijkstra) -----------------
    // Seeds are STANDING water cells (lake + ponds) PLUS the cells the river
    // trace passes through. The river seeds live in a LOCAL set and never
    // enter fill_level: that array is the standing-water COVERAGE truth read
    // by water_at, and writing 16 m trace cells into it used to flood whole
    // coarse cells beside the channel (~8.5 k m2 of bed with no water body
    // over it — the §3.3 cap violation seen in the stage-3 frames). Coverage
    // and distance seeding are separate concerns; keep them separate.
    hydro.coarse_dist.assign(cells, std::numeric_limits<float>::max());
    MinHeap heap;
    std::vector<uint8_t> is_seed(cells, 0);
    for (uint32_t i = 0; i < cells; ++i) {
        if (hydro.fill_level[i] != math::NO_WATER) {
            is_seed[i] = 1;
        }
    }
    for (const math::RiverStation& st : hydro.stations) {
        const uint32_t c = cell_of(st.position);
        if (c != INVALID) {
            is_seed[c] = 1;
        }
    }
    for (uint32_t i = 0; i < cells; ++i) {
        if (is_seed[i]) {
            hydro.coarse_dist[i] = 0.0f;
            heap.push({0.0f, i});
        }
    }
    const float diag = CELL * std::numbers::sqrt2_v<float>;
    while (!heap.empty()) {
        const HeapEntry e = heap.top();
        heap.pop();
        if (e.value > hydro.coarse_dist[e.index]) continue;
        const uint32_t x = e.index % grid.w;
        grid.neighbors(e.index, [&](uint32_t n) {
            const uint32_t nx = n % grid.w;
            const float cost = (nx == x || n / grid.w == e.index / grid.w) ? CELL : diag;
            const float nd = e.value + cost;
            if (nd < hydro.coarse_dist[n]) {
                hydro.coarse_dist[n] = nd;
                heap.push({nd, n});
            }
        });
    }

    // --- Pond primitives (drawable bodies for the surviving ponds) --------------
    // water_at floods a pond's whole coarse cell, so the primitive must cover
    // the cell FOOTPRINT (cell origin .. origin + CELL), not just its nodes.
    for (const Pond& pond : hydro.ponds) {
        if (pond.cells.empty()) continue;
        float min_x = std::numeric_limits<float>::max();
        float min_z = min_x;
        float max_x = -min_x;
        float max_z = -min_x;
        for (const uint32_t c : pond.cells) {
            const glm::vec2 p = grid.pos(c);
            min_x = std::min(min_x, p.x);
            min_z = std::min(min_z, p.y);
            max_x = std::max(max_x, p.x + CELL);
            max_z = std::max(max_z, p.y + CELL);
        }
        hydro.pond_planes.push_back(
            math::LakePlane{glm::vec2{(min_x + max_x) * 0.5f, (min_z + max_z) * 0.5f},
                            glm::vec2{(max_x - min_x) * 0.5f, (max_z - min_z) * 0.5f},
                            pond.level});
    }

    // (Station spatial bins were built right after the widths pass — they
    // accelerate the mud cap above and every water_at query afterwards.)
    return hydro;
}


} // namespace dfn::world
