/*
Created: 10:08:2026 - 10:44:13
Last updated: 10:08:2026 - 11:11:16
Module: engine/world
File: engine/world/sources/WorldgenPaths.h

Responsibility:
- THE PATH NETWORK (LANDSCAPE §8.1 item 1, user-ratified в7/в24): ONE generator
  producing all four path classes as one network. Core owns the ROUTE, the
  CLASS, the WIDTH and the WEAR FIELD; render draws the surface; flora
  populates the edge (в24). Carries BR-1 (hide the destination once) and BR-2
  (real goals, near-shortest) as properties of the trace, not as decoration.

Key items:
- PathClass: cobble / dirt / faint trail / stone steps (в7's four, one system).
- Goal / GoalKind: the REAL endpoints BR-2 requires — no path to nowhere.
- PathSample: the per-position query render and flora both read.
- PathNetwork: goals, routes, the baked flatten delta, and sample().
- build_path_network(): sites the goals, routes the network, measures itself.

Dependencies:
- Uses: TestbedLayout.h, glm, <vector>, <functional>.
- Used by: Worldgen.cpp (context + terrain_height), scatter, tests; render and
  flora read PathSample.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE EDGE DATUM IS FLORA'S, AND IT IS NOT THE CENTRELINE (FloraEdgeRules.h,
  design-ratified): band distance 0 is the OUTER EDGE OF THE WORN SURFACE,
  measured outward. PathSample therefore reports dist_from_worn_edge directly —
  a consumer that derives it from dist_to_center and guesses the width will
  drift the moment a class width changes.
- DETERMINISM (Rule 13.1): the whole network is a pure function of
  (seed, layout, height field). The A* uses a total order on ties (see the
  .cpp) because a heap that breaks ties by address is not reproducible.
*/
/*
UPD:
- 10:08:2026 - 10:44:13: Created — §8.1 path network: goal siting, slope-aware
  cost routing, BR-1 as a term IN the cost field, class assignment, the
  three-band wear field and the flatten delta.
- 10:08:2026 - 11:11:16: PathClass ordinals declared a cross-zone contract (flora's
  PathClassRichness maps positionally; siblings in the DAG, so a reorder is
  silent). Pinned by test.
*/

#pragma once

#include "engine/world/sources/TestbedLayout.h"

#include <cstdint>
#include <functional>
#include <glm/vec2.hpp>
#include <vector>

namespace dfn::world {

/// в7's four types as ONE system. The class changes ALONG a route by rule
/// (§8.1 item 1): paved near the largest goal, dirt between goals, hint-paths
/// to the small ones, steps where the slope demands them.
///
/// THESE ORDINALS ARE A CROSS-ZONE CONTRACT. FLORA'S MAINTENANCE COLUMN
/// (FloraEdgeRules.h, PathClassRichness) MAPS TO THEM POSITIONALLY, and world
/// and render are SIBLINGS in the DAG — neither can see the other's
/// declaration, so no static_assert can catch a reorder. Renumbering or
/// reordering this enum silently permutes flora's per-class edge weights and
/// gardens a cobbled gutter while leaving a hint-path swept. The values are
/// therefore written out explicitly and PathClassTests pins them; if a class
/// is ever added it goes on the END, and flora is told in the same commit.
enum class PathClass : uint8_t {
    Cobble = 0,     ///< «мостовая» — the approach to the largest goal
    Dirt = 1,       ///< the road between goals
    FaintTrail = 2, ///< «тропинка-намёк» — spurs to small goals and finds
    StoneSteps = 3, ///< where the grade exceeds what a walked route holds
};

/// What a route may END at. BR-2 clause (i): both endpoints are registered
/// goals — a path to nowhere fails the rule by construction, so "nowhere" is
/// not representable here.
enum class GoalKind : uint8_t {
    ClearingShrine = 0, ///< the largest goal: the glade's shrine (cobble approach)
    Spring = 1,         ///< a spring in a swale floor
    WoodcuttersHut = 2, ///< a hut on flat ground
    SpireGroup = 3,     ///< a pale-spire group against canopy (§2.9)
    CrestCairn = 4,     ///< a cairn on a grive crest — the reveal vantage
};

struct Goal {
    GoalKind kind = GoalKind::Spring;
    glm::vec2 position{0.0f, 0.0f};
    float height = 0.0f;
    float importance = 0.0f; ///< [0,1]; drives the class of the routes reaching it
};

/// Per-position path query. One struct for render (surface) and flora (edge)
/// so the two zones cannot disagree about where the path is.
struct PathSample {
    float dist_to_center = 1e9f;     ///< m from the route centreline
    /// m from the OUTER EDGE OF THE WORN SURFACE, outward (flora's datum —
    /// FloraEdgeRules.h). Negative on the trodden surface itself.
    float dist_from_worn_edge = 1e9f;
    float worn_half_width = 0.0f;    ///< half-width of the trodden surface here
    PathClass path_class = PathClass::Dirt;
    /// The three-band wear profile in ONE number, [0,1]:
    ///   +1 .. 0 across the trodden surface (1 = bare worn centre),
    ///   0 outside it.
    float wear = 0.0f;
    /// The RICH EDGE weight (BR-3), [0,1]: 0 on the trodden surface, peaking
    /// just outside the worn edge, decaying to 0 by RICH_EDGE_BAND_M. This is
    /// the factor flora's edge rules multiply by; it is a separate number from
    /// `wear` because they are opposite claims about the same ground.
    float edge = 0.0f;
};

/// A traced route between two goals.
struct PathRoute {
    int goal_a = -1;
    int goal_b = -1;
    std::vector<glm::vec2> points;    ///< centreline, ~4 m stations
    std::vector<float> heights;       ///< the SMOOTHED longitudinal profile (the tread)
    std::vector<PathClass> classes;   ///< per station
    float length_m = 0.0f;            ///< the traced length
    float optimal_length_m = 0.0f;    ///< the same search WITHOUT the BR-1 term
    /// BR-1 measurement: the longest contiguous run (m) of stations from which
    /// the destination goal is occluded at eye height.
    float longest_hidden_run_m = 0.0f;
};

struct PathParams {
    float grid_cell = 4.0f;    ///< routing/raster resolution (m)
    float station_m = 4.0f;    ///< BR-1's station spacing, ruled by §1.7
    float slope_k = 6.0f;      ///< slope penalty: cost = dist * (1 + slope_k * grade)
    float hide_weight = 0.45f; ///< BR-1: extra cost on ground from which the destination is VISIBLE
    float flatten_blend_m = 3.0f; ///< how far past the tread the flattening eases out
    float rich_edge_band_m = 2.5f; ///< BR-3's margin band, outward from the worn edge
};

/// The whole network plus the terrain delta that makes a path flatter than its
/// surroundings.
struct PathNetwork {
    std::vector<Goal> goals;
    std::vector<PathRoute> routes;

    /// SEGMENT BIN INDEX, not a raster. The first cut baked dist/class/width
    /// into the 4 m routing grid and the whole cross-section collapsed: a dirt
    /// tread is 2.2 m wide and BR-3's rich edge band is 2.5 m, so a 4 m cell
    /// cannot tell the trodden centre from the margin — the wear at the
    /// centreline measured 0.46 instead of 1.0 because the nearest cell centre
    /// was 0.8 m off the line. A finer raster was the obvious fix and the wrong
    /// one (0.5 m over this stand is 5.9 M cells); the distance to a polyline
    /// is cheap to compute exactly, so it is computed exactly.
    glm::vec2 origin{0.0f, 0.0f};
    float bin_m = 0.0f;              ///< bin size, >= the widest reach
    int bins = 0;                    ///< bins x bins
    std::vector<uint32_t> bin_start; ///< bins*bins + 1, CSR offsets
    std::vector<uint32_t> bin_items; ///< (route << 16) | station
    float flatten_blend_m = 3.0f;
    float rich_edge_band_m = 2.5f;

    /// Nearest centreline segment to a query point (the one traversal both
    /// queries share).
    struct Nearest {
        float d2 = 1e18f;
        float height = 0.0f;   ///< the tread's smoothed height at that station
        PathClass cls = PathClass::Dirt;
        bool found = false;
    };
    [[nodiscard]] Nearest nearest(glm::vec2 world) const;

    /// Height delta making the tread flatter (and slightly sunk) than the
    /// ground around it. Exact: distance to the nearest centreline SEGMENT.
    /// `ground_height` is the height the terrain WOULD have here without the
    /// path — passed in rather than resampled, so the delta is exact against
    /// the field the caller actually built (a resampled copy would disagree
    /// with it by the interpolation error and leave a lip along every tread).
    [[nodiscard]] float flatten_at(glm::vec2 world, float ground_height) const;
    /// The query flora and render both read.
    [[nodiscard]] PathSample sample(glm::vec2 world) const;

    /// BR-2's measured overhead: max over routes of length / optimal_length.
    /// Reported, not merely asserted (Rule 30 — design expects 1.1-1.2 and the
    /// ceiling DETOUR_MAX sits above whatever this actually is).
    [[nodiscard]] float max_detour_ratio() const;
};

[[nodiscard]] PathNetwork build_path_network(uint64_t seed, const TestbedLayout& layout,
                                             glm::vec2 domain_min, glm::vec2 domain_max,
                                             const PathParams& params,
                                             const std::function<float(glm::vec2)>& height);

/// Half-width (m) of the trodden surface per class. Exposed because render
/// sizes its surface splat from the SAME numbers core wears the ground with
/// (Rule 35: two zones, one number).
[[nodiscard]] float path_half_width(PathClass c);

} // namespace dfn::world
