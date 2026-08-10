/*
Created: 10:08:2026 - 10:55:03
Last updated: 10:08:2026 - 10:55:03
Module: engine/world
File: engine/world/sources/WorldgenFinds.cpp

Responsibility:
- BR-6 find placement: the road regime along the network, the wilderness
  regime on the ground between, both sited so BR-5's occlusion holds.

Key items:
- build_finds, find_spacing_m.

Dependencies:
- Uses: WorldgenFinds.h, WorldgenNoise.h, config.
- Used by: Worldgen.cpp, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 13.1: fixed traversal order (routes in order, stations in order, then
  the wilderness lattice in row-major order) and one seeded stream.
*/
/*
UPD:
- 10:08:2026 - 10:55:03: Created.
*/

#include "engine/world/sources/WorldgenFinds.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenNoise.h"

#include <algorithm>
#include <span>
#include <cmath>
#include <glm/geometric.hpp>

namespace dfn::world {

namespace {

constexpr float EYE_M = static_cast<float>(config::PLAYER_EYE_HEIGHT);
/// A find stands about half a metre proud of the ground — the height the
/// occlusion ray aims at. Using 0 would make every blade of micro-relief an
/// occluder and the acceptance would measure the ground, not the composition.
constexpr float FIND_TOP_M = 0.5f;

bool ray_clear(const std::function<float(glm::vec2)>& h, glm::vec2 from, glm::vec2 to) {
    const float d = glm::length(to - from);
    if (d < 1e-3f) {
        return true;
    }
    const float eye = h(from) + EYE_M;
    const float tgt = h(to) + FIND_TOP_M;
    const int steps = std::max(4, static_cast<int>(d / 4.0f));
    for (int i = 1; i < steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        if (h(from + (to - from) * t) > eye + (tgt - eye) * t) {
            return false;
        }
    }
    return true;
}

float occluded_fraction(const std::function<float(glm::vec2)>& h, glm::vec2 p,
                        const FindParams& params) {
    int blocked = 0;
    for (int i = 0; i < params.ring_bearings; ++i) {
        const float ang = 6.2831853f * static_cast<float>(i)
                        / static_cast<float>(params.ring_bearings);
        // Two radii per bearing across the ruled band: sampling only the outer
        // radius would credit the rule for distance rather than for relief.
        for (const float r : {params.ring_min_m, params.ring_max_m}) {
            const glm::vec2 q = p + glm::vec2{std::cos(ang), std::sin(ang)} * r;
            if (!ray_clear(h, q, p)) {
                ++blocked;
            }
        }
    }
    return static_cast<float>(blocked)
         / static_cast<float>(params.ring_bearings * 2);
}

FindKind kind_for(uint64_t seed, uint32_t stream, int64_t i, FindRegime regime) {
    const float u = noise::lattice_value(seed, stream, i, static_cast<int64_t>(regime));
    // Near roads the mailbox tier is human leavings; in the wild it is the
    // land's own oddities (в8's «редкие жемчужины в глуши»).
    if (regime == FindRegime::NearRoad) {
        if (u < 0.4f) return FindKind::MushroomRing;
        if (u < 0.7f) return FindKind::AbandonedCart;
        return FindKind::StrangeStone;
    }
    if (u < 0.35f) return FindKind::StrangeStone;
    if (u < 0.6f) return FindKind::Spring;
    if (u < 0.85f) return FindKind::MushroomRing;
    return FindKind::SpireCluster;
}

} // namespace

std::vector<OccluderDisc> build_find_occluders(std::span<const math::ScatterInstance> scatter,
                                               const OccluderGeometry& geom,
                                               const std::function<float(glm::vec2)>& height) {
    std::vector<OccluderDisc> discs;
    discs.reserve(scatter.size() / 4);
    for (const math::ScatterInstance& i : scatter) {
        const glm::vec2 p{i.position.x, i.position.z};
        OccluderDisc d;
        d.center = p;
        switch (i.species) {
        case math::ScatterSpecies::OakTree:
            // Only the CLEAR TRUNK occludes: above crown_base the ray is in
            // foliage, which is the C1 model's business and not this one.
            d.radius = geom.oak_trunk_radius_m * i.scale;
            d.top_y = height(p) + geom.oak_height_m * i.scale * geom.oak_trunk_top_frac;
            break;
        case math::ScatterSpecies::Bush:
            d.radius = geom.bush_radius_m * i.scale;
            d.top_y = height(p) + geom.bush_height_m * i.scale;
            break;
        case math::ScatterSpecies::BigBush:
            d.radius = geom.big_bush_radius_m * i.scale;
            d.top_y = height(p) + geom.big_bush_height_m * i.scale;
            break;
        default:
            // EXCLUDED BY CAUSE (design's ruling), not by size: dead wood,
            // pines, birches, stones and ground cover are not classes this
            // gate is permitted to depend on.
            continue;
        }
        if (d.radius > 0.0f) {
            discs.push_back(d);
        }
    }
    return discs;
}

namespace {

/// True if the eye->find segment is stopped by `d`. Two conditions, and the
/// height one is what stops a bush "occluding" a find it is nowhere near
/// vertically.
///
/// SEGMENT-vs-DISC, computed exactly, NOT sampled at the march step. A 4 m
/// stride past a 0.4 m trunk steps over it about nine times out of ten, so a
/// sampled test would report the stride rather than the forest — the same
/// class of error as measuring a groove on a lattice too coarse to hold it.
bool disc_blocks(const OccluderDisc& d, glm::vec2 from, glm::vec2 to, float eye_y, float tgt_y) {
    const glm::vec2 seg = to - from;
    const float len2 = glm::dot(seg, seg);
    if (len2 < 1e-6f) {
        return false;
    }
    float t = glm::dot(d.center - from, seg) / len2;
    t = std::clamp(t, 0.0f, 1.0f);
    const glm::vec2 closest = from + seg * t;
    const glm::vec2 off = d.center - closest;
    if (glm::dot(off, off) > d.radius * d.radius) {
        return false;
    }
    // The ray's height where it passes the disc. Below the occluder's top means
    // blocked; a ray sailing over a bush is not blocked by that bush.
    return eye_y + (tgt_y - eye_y) * t <= d.top_y;
}

} // namespace

float occluded_fraction_at(const std::function<float(glm::vec2)>& height,
                           std::span<const OccluderDisc> discs, glm::vec2 find,
                           float ring_radius_m, int bearings) {
    if (bearings <= 0) {
        return 0.0f;
    }
    const float tgt_y = height(find) + FIND_TOP_M;
    int blocked = 0;
    for (int i = 0; i < bearings; ++i) {
        const float ang = 6.2831853f * static_cast<float>(i) / static_cast<float>(bearings);
        const glm::vec2 eye = find + glm::vec2{std::cos(ang), std::sin(ang)} * ring_radius_m;
        const float eye_y = height(eye) + EYE_M;
        bool hit = false;
        // Terrain first: it is the cheap test and it is the one that was here
        // before, unchanged, so the control really is this instrument minus the
        // scatter rather than a different instrument.
        const int steps = std::max(4, static_cast<int>(ring_radius_m / 4.0f));
        for (int k = 1; k < steps && !hit; ++k) {
            const float t = static_cast<float>(k) / static_cast<float>(steps);
            if (height(eye + (find - eye) * t) > eye_y + (tgt_y - eye_y) * t) {
                hit = true;
            }
        }
        for (std::size_t di = 0; di < discs.size() && !hit; ++di) {
            hit = disc_blocks(discs[di], eye, find, eye_y, tgt_y);
        }
        blocked += hit ? 1 : 0;
    }
    return static_cast<float>(blocked) / static_cast<float>(bearings);
}

float find_spacing_m(FindRegime regime) {
    const float base_m = static_cast<float>(config::FIND_SPACING_BASE_S)
                       * static_cast<float>(config::WALK_SPEED);
    const float mult = (regime == FindRegime::NearRoad)
                         ? static_cast<float>(config::FIND_NEAR_ROAD_MULT)
                         : static_cast<float>(config::FIND_WILD_MULT);
    return base_m * mult;
}

std::vector<Find> build_finds(uint64_t seed, const PathNetwork& net, glm::vec2 domain_min,
                              glm::vec2 domain_max, const FindParams& params,
                              const std::function<float(glm::vec2)>& height) {
    std::vector<Find> finds;
    const float road_spacing = find_spacing_m(FindRegime::NearRoad);

    // ---- regime 1: along the network ---------------------------------------
    // Arc-length placement, jittered. NOT one per N stations: station spacing
    // is uniform today and would silently stop meaning metres the moment it
    // is not.
    int64_t counter = 0;
    for (std::size_t ri = 0; ri < net.routes.size(); ++ri) {
        const PathRoute& r = net.routes[ri];
        float since = road_spacing * 0.5f; // do not stack a find on the goal itself
        for (std::size_t i = 1; i < r.points.size(); ++i) {
            since += glm::length(r.points[i] - r.points[i - 1]);
            const float jitter =
                0.75f + 0.5f * noise::lattice_value(seed, 90, static_cast<int64_t>(ri),
                                                    static_cast<int64_t>(i));
            if (since < road_spacing * jitter) {
                continue;
            }
            since = 0.0f;
            const glm::vec2 d = glm::normalize(r.points[i] - r.points[i - 1]);
            const glm::vec2 nrm{-d.y, d.x};
            const float side =
                (noise::lattice_value(seed, 91, static_cast<int64_t>(ri),
                                      static_cast<int64_t>(i)) < 0.5f) ? -1.0f : 1.0f;
            const float lat = params.lateral_min_m
                            + (params.lateral_max_m - params.lateral_min_m)
                                  * noise::lattice_value(seed, 92, static_cast<int64_t>(ri),
                                                         static_cast<int64_t>(i));
            const glm::vec2 p = r.points[i] + nrm * (side * lat);
            if (p.x < domain_min.x || p.y < domain_min.y || p.x > domain_max.x
                || p.y > domain_max.y) {
                continue;
            }
            Find f;
            f.regime = FindRegime::NearRoad;
            f.kind = kind_for(seed, 93, counter++, f.regime);
            f.position = p;
            f.height = height(p);
            // §1.7's BR-5 interaction, stated in the bible: "near roads a find
            // may sit visible from the road (the road is its reveal)". So the
            // occlusion is MEASURED and recorded here, not enforced.
            f.occluded_fraction = occluded_fraction(height, p, params);
            finds.push_back(f);
        }
    }

    // ---- regime 2: the wilderness ------------------------------------------
    // The wild cadence is a LINEAR density along a cross-country walk, so it
    // has to be converted to an AREAL one before anything can be placed: a
    // walker sweeps a corridor 2 x FIND_ENCOUNTER_RADIUS wide, so one find per
    // `wild_spacing` metres of route means one per
    // (2 x radius x wild_spacing) square metres.
    const float wild_spacing = find_spacing_m(FindRegime::Wilderness);
    const float radius = static_cast<float>(config::FIND_ENCOUNTER_RADIUS);
    const float area_per_find = 2.0f * radius * wild_spacing;
    const float lattice = std::sqrt(area_per_find);
    const auto nx = static_cast<int>((domain_max.x - domain_min.x) / lattice);
    const auto nz = static_cast<int>((domain_max.y - domain_min.y) / lattice);
    for (int z = 0; z < nz; ++z) {
        for (int x = 0; x < nx; ++x) {
            const float jx = noise::lattice_value(seed, 94, x, z);
            const float jz = noise::lattice_value(seed, 95, x, z);
            glm::vec2 p = domain_min
                        + glm::vec2{(static_cast<float>(x) + jx) * lattice,
                                    (static_cast<float>(z) + jz) * lattice};
            if (p.x < domain_min.x + 20.0f || p.y < domain_min.y + 20.0f
                || p.x > domain_max.x - 20.0f || p.y > domain_max.y - 20.0f) {
                continue;
            }
            // The two regimes must not populate the same ground, or the road
            // regime's own statistics are measured against finds it did not
            // place.
            if (net.sample(p).dist_to_center < params.road_band_m) {
                continue;
            }
            // BR-5 SITING: nudge to the best of a small candidate set by
            // occlusion. This is the rule the wilderness find exists to
            // satisfy — "small finds are visible only from crests" is a
            // PLACEMENT requirement, and a find dropped where it happens to
            // land satisfies it only by luck.
            float best = -1.0f;
            glm::vec2 best_p = p;
            for (int k = 0; k < 5; ++k) {
                const float ax = noise::lattice_value(seed, 96 + static_cast<uint32_t>(k), x, z);
                const float az = noise::lattice_value(seed, 101 + static_cast<uint32_t>(k), x, z);
                const glm::vec2 q = p + (glm::vec2{ax, az} - glm::vec2{0.5f, 0.5f}) * lattice;
                if (net.sample(q).dist_to_center < params.road_band_m) {
                    continue;
                }
                const float occ = occluded_fraction(height, q, params);
                if (occ > best) {
                    best = occ;
                    best_p = q;
                }
            }
            Find f;
            f.regime = FindRegime::Wilderness;
            f.kind = kind_for(seed, 106, counter++, f.regime);
            f.position = best_p;
            f.height = height(best_p);
            f.occluded_fraction = std::max(0.0f, best);
            finds.push_back(f);
        }
    }
    return finds;
}

} // namespace dfn::world
