/*
Created: 09:08:2026 - 15:20:00
Last updated: 09:08:2026 - 15:20:00
Module: engine/world
File: engine/world/sources/WorldgenCastle.cpp

Responsibility:
- Castle pass implementation (LANDSCAPE §6.1, hall-castle): terrace solve with
  the CASTLE_PAD_CUT_MAX allowance, the graded approach ramp (access
  invariant), R3 height solve, hall/solar/wall/gatehouse placement with the
  gate facing the valley, and the occlusion query.

Key items:
- solve_castle, castle_pad_height, castle_occluder_height, standpoints.

Dependencies:
- Uses: WorldgenCastle.h, WorldgenMacro.h, WorldgenNoise.h, ContentHash, config.
- Used by: dfn_world.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- R3 outranks the height table (§6.1.1): heights shrink to fit the skyline
  margin; if even the minimum table heights cannot fit, the pad is lowered
  (fix order 1) before anything else. Never raise the crag.
- The ramp surface is LINEAR in the approach direction on purpose: constant
  slope, no step (PLAYER_STEP_HEIGHT), which is exactly what the access
  invariant checks. Do not "smooth" it into an S-curve — that reintroduces a
  steep mid-section.
- Deterministic: pure function of (seed, layout, hydrology). No rng — the
  castle is a designed monument, not scatter.
*/
/*
UPD:
- 09:08:2026 - 15:20:00: Created — castle solve/stamp/occlusion for the
  hall-castle revision (hall + solar + wall + gatehouse, access ramp).
*/

#include "engine/world/sources/WorldgenCastle.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenNoise.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <vector>

namespace dfn::world {

namespace {

constexpr float PAD_SIZE = static_cast<float>(config::CASTLE_PAD_SIZE);
constexpr float CUT_MAX = static_cast<float>(config::CASTLE_PAD_CUT_MAX);
constexpr float SKYLINE_MARGIN = static_cast<float>(config::CASTLE_SKYLINE_MARGIN);
constexpr float HALL_MIN = static_cast<float>(config::CASTLE_HALL_HEIGHT_MIN);
constexpr float HALL_MAX = static_cast<float>(config::CASTLE_HALL_HEIGHT_MAX);
constexpr float SOLAR_MIN = static_cast<float>(config::CASTLE_SOLAR_HEIGHT_MIN);
constexpr float SOLAR_MAX = static_cast<float>(config::CASTLE_SOLAR_HEIGHT_MAX);
constexpr float WALL_MIN = static_cast<float>(config::CASTLE_WALL_HEIGHT_MIN);
constexpr float WALL_MAX = static_cast<float>(config::CASTLE_WALL_HEIGHT_MAX);
constexpr float GATE_MIN = static_cast<float>(config::CASTLE_GATE_HEIGHT_MIN);
constexpr float GATE_MAX = static_cast<float>(config::CASTLE_GATE_HEIGHT_MAX);
constexpr float CORRIDOR_HALF = static_cast<float>(config::CORRIDOR_WIDTH) * 0.5f;
// Ramp grade target: well under CORRIDOR_SLOPE_MAX so the invariant passes
// with margin even where natural grade fights it (design: "commoner on foot").
constexpr float RAMP_GRADE = 0.12f; // ~7 deg

// §6.1.3 footprints.
constexpr float WALL_SIDE = 40.0f;
constexpr float HALL_X = 10.0f, HALL_Z = 22.0f;
constexpr float SOLAR_SIDE = 8.0f;
constexpr float GATE_X = 10.0f, GATE_Z = 6.0f;

/// Terrain before any pad stamp (macro + hydrology carve).
float ground_height(uint64_t seed, const TestbedLayout& layout, const HydrologyData& hydro,
                    glm::vec2 p) {
    return carve_height(hydro, layout, p, macro_height(seed, layout, p));
}

/// Castle frame: `forward` points out of the gate, down the approach.
struct Frame {
    glm::vec2 forward{0.0f, 1.0f};
    glm::vec2 right{1.0f, 0.0f};
};

Frame frame_of(glm::vec2 forward) {
    Frame f;
    f.forward = forward;
    f.right = glm::vec2{-forward.y, forward.x};
    return f;
}

Frame castle_frame(const CastleBuild& castle) { return frame_of(castle.gate_dir); }

/// Local coordinates in the castle frame (x = right, y = forward).
glm::vec2 to_local(const CastleBuild& castle, glm::vec2 world) {
    const Frame f = castle_frame(castle);
    const glm::vec2 rel = world - castle.center;
    return glm::vec2{glm::dot(rel, f.right), glm::dot(rel, f.forward)};
}

bool in_box(glm::vec2 local, glm::vec2 center, float half_x, float half_y) {
    return std::fabs(local.x - center.x) <= half_x && std::fabs(local.y - center.y) <= half_y;
}

} // namespace

CastleBuild solve_castle(uint64_t seed, const TestbedLayout& layout,
                         const HydrologyData& hydro) {
    CastleBuild castle;
    if (config::CASTLE_COUNT_TESTBED < 1) {
        return castle; // no castle configured
    }
    castle.valid = true;
    castle.center = layout.castle.center;
    castle.half_size = PAD_SIZE * 0.5f;
    // §6.1.2: "pad edges blend over 1.5x pad size" — the skirt extends the
    // footprint to 1.5x, i.e. a quarter-pad band on each side.
    castle.blend = PAD_SIZE * 0.25f;

    // --- Gate direction: out toward the valley/ford approach (§6.1.2, settled:
    // valley-facing, never rotated toward the barrow).
    const CorridorLayout& corridor =
        layout.corridors[std::clamp(layout.castle.approach_corridor, 0,
                                    static_cast<int>(std::size(layout.corridors)) - 1)];
    glm::vec2 forward{0.0f, 1.0f};
    if (corridor.point_count >= 2) {
        const glm::vec2 to_approach = corridor.points[0] - castle.center;
        if (glm::length(to_approach) > 1.0f) {
            forward = glm::normalize(to_approach);
        }
    }
    castle.gate_dir = forward;
    castle.gate_yaw = std::atan2(forward.x, -forward.y); // yaw 0 = -Z

    // --- Ward chain: terraces stepping DOWN the spur, away from the crag.
    // Ward 0 sits uphill (nearest the barrow, oldest); each later ward is one
    // step further down the approach. Chaining small terraces is what keeps
    // every cut inside CASTLE_PAD_CUT_MAX on a slope where one 120 m slab
    // needed ~10 m — and it stops the terrace reaching the Backbarrow carve.
    const float span = static_cast<float>(config::CASTLE_PAD_SIZE);
    castle.ward_count = 3;
    const float ward_half = span / 6.0f;        // three wards across the span
    const float ward_blend = ward_half * 0.5f;
    const float step = span / 3.0f;
    castle.half_size = span * 0.5f;
    castle.blend = ward_blend;

    const auto median_height = [&](glm::vec2 c, float half, float& out_cut, float& out_fill) {
        std::vector<float> s;
        for (float z = -half; z <= half; z += 4.0f) {
            for (float x = -half; x <= half; x += 4.0f) {
                s.push_back(ground_height(seed, layout, hydro, c + glm::vec2{x, z}));
            }
        }
        std::sort(s.begin(), s.end());
        float h = s[s.size() / 2];
        if (s.back() - h > CUT_MAX) {
            h = s.back() - CUT_MAX; // raise until the cut fits the allowance
        }
        // NEVER TERRACE BELOW THE WATERLINE. The ward chain reaches down the
        // spur toward the valley, and a terrace solved from terrain alone
        // happily cut a ward floor under a pond — which flooded the ward and,
        // where the approach corridor crosses it, drowned a ford that the C3
        // chain depends on staying wade-shallow.
        float water_top = -1e9f;
        for (float z = -half; z <= half; z += 4.0f) {
            for (float x = -half; x <= half; x += 4.0f) {
                const glm::vec2 q = c + glm::vec2{x, z};
                const WaterSample ws =
                    water_at(hydro, layout, q, macro_height(seed, layout, q));
                if (ws.water_surface != math::NO_WATER) {
                    water_top = std::max(water_top, ws.water_surface);
                }
            }
        }
        if (water_top > -1e8f) {
            h = std::max(h, water_top + static_cast<float>(config::BUILDING_WATER_MARGIN));
        }
        out_cut = std::max(0.0f, s.back() - h);
        out_fill = std::max(0.0f, h - s.front());
        return h;
    };

    castle.cut = 0.0f;
    castle.fill = 0.0f;
    for (int i = 0; i < castle.ward_count; ++i) {
        CastleWard& w = castle.wards[i];
        // Ward 0 at the anchor, later wards stepping downhill (-forward is
        // uphill because `forward` points out of the gate toward the valley).
        w.center = castle.center + forward * (static_cast<float>(i) * step);
        w.half_size = ward_half;
        w.blend = ward_blend;
        w.height = median_height(w.center, ward_half, w.cut, w.fill);
        castle.cut = std::max(castle.cut, w.cut);
        castle.fill = std::max(castle.fill, w.fill);
    }
    // Terraces must STEP DOWN along the approach; a lower ward that solved
    // higher than its uphill neighbour would read as a bowl, not a fortress.
    for (int i = 1; i < castle.ward_count; ++i) {
        castle.wards[i].height =
            std::min(castle.wards[i].height, castle.wards[i - 1].height - 1.0f);
    }
    castle.pad_height = castle.wards[0].height;

    // --- R3 height solve: pad + tallest element <= peak - CASTLE_SKYLINE_MARGIN.
    const float peak = macro_height(seed, layout, layout.crag.center);
    const float ceiling = peak - SKYLINE_MARGIN;
    float solar = SOLAR_MAX;
    if (castle.pad_height + solar > ceiling) {
        solar = ceiling - castle.pad_height;
    }
    if (solar < SOLAR_MIN) {
        // Fix order (1): lower the terraces, keeping height (§6.1.1).
        solar = SOLAR_MIN;
        const float drop = castle.pad_height - (ceiling - solar);
        if (drop > 0.0f) {
            for (int i = 0; i < castle.ward_count; ++i) {
                castle.wards[i].height -= drop;
            }
            castle.pad_height = castle.wards[0].height;
        }
    }
    castle.solar_height = solar;
    // Subordinate elements scale with the solar's share of its band so the
    // horizontal-dominant read (wall band < hall roof < solar) survives any
    // R3 squeeze.
    const float t =
        SOLAR_MAX > SOLAR_MIN ? (solar - SOLAR_MIN) / (SOLAR_MAX - SOLAR_MIN) : 1.0f;
    castle.hall_height = std::min(HALL_MIN + (HALL_MAX - HALL_MIN) * t, solar - 1.0f);
    castle.wall_height = std::min(WALL_MIN + (WALL_MAX - WALL_MIN) * t,
                                  castle.hall_height - 0.5f);
    castle.gate_height = std::min(GATE_MIN + (GATE_MAX - GATE_MIN) * t, solar - 0.5f);
    castle.tower_height = std::min(
        static_cast<float>(config::CASTLE_TOWER_HEIGHT_MIN)
            + (static_cast<float>(config::CASTLE_TOWER_HEIGHT_MAX
                                  - config::CASTLE_TOWER_HEIGHT_MIN)) * t,
        solar - 1.0f);


    // --- Access ramp (binding invariant): grade from the pad edge out to
    // natural ground on the gate side. Length is derived from the actual drop
    // so the slope lands at RAMP_GRADE, and is at least the skirt width so the
    // ramp always replaces the scarp rather than sitting beside it.
    const glm::vec2 pad_edge = castle.center + forward * castle.half_size;
    float drop = 0.0f;
    for (float probe = 10.0f; probe <= 80.0f; probe += 10.0f) {
        const float g = ground_height(seed, layout, hydro, pad_edge + forward * probe);
        drop = std::max(drop, castle.pad_height - g);
    }
    castle.ramp_half_width = CORRIDOR_HALF;
    castle.ramp_length = std::max(castle.blend + 5.0f, std::fabs(drop) / RAMP_GRADE);

    // --- Element placement, DISTRIBUTED ACROSS THE WARDS -----------------------
    // Ward 0 (upper, oldest, nearest the barrow) carries the hall and solar —
    // the original seat. Ward 1 is the bailey with the curtain and its corner
    // towers. Ward 2 is the outer works with the gatehouse on the approach.
    // Spreading the mass down the spur is also what pulls the silhouette off
    // the crag's crown: stacked on one pad it occluded it.
    auto emit = [&](SiteType type, int ward, glm::vec2 local) {
        const CastleWard& w = castle.wards[std::clamp(ward, 0, castle.ward_count - 1)];
        const Frame f = castle_frame(castle);
        const glm::vec2 world = w.center + f.right * local.x + f.forward * local.y;
        castle.entities.push_back(GeneratedEntityRecord{
            0, serialization::fnv1a64(site_archetype(type).content_id), world,
            castle.gate_yaw, w.height});
        castle.types.push_back(type);
    };
    const float bailey_half = castle.wards[1].half_size * 0.8f;
    emit(SiteType::CastleHall, 0, {0.0f, 0.0f});
    emit(SiteType::CastleSolar, 0, {0.0f, -castle.wards[0].half_size * 0.55f});
    emit(SiteType::CastleWall, 1, {0.0f, 0.0f});
    // Corner towers on the bailey's uphill corners: one of them must see the
    // Backbarrow entrance from its top (story constraint, validated).
    emit(SiteType::CastleTower, 1, {-bailey_half, -bailey_half});
    emit(SiteType::CastleTower, 1, {bailey_half, -bailey_half});
    emit(SiteType::CastleGatehouse, 2, {0.0f, castle.wards[2].half_size});
    return castle;
}

float castle_pad_height(const CastleBuild& castle, glm::vec2 world, float h) {
    if (!castle.valid) {
        return h;
    }
    // Each ward is its own terrace. A FLAT ward surface always wins over a
    // neighbour's skirt: wards are only `step` apart while each skirt reaches
    // half_size + blend, so the chain overlaps by design. Resolving in array
    // order instead let ward 0's skirt overwrite ward 1's floor, which tilted
    // a terrace that is supposed to be level and put phantom steps on the ramp.
    for (int i = 0; i < castle.ward_count; ++i) {
        const CastleWard& w = castle.wards[i];
        const float cheb = std::max(std::fabs(world.x - w.center.x),
                                    std::fabs(world.y - w.center.y));
        if (cheb <= w.half_size) {
            return w.height;
        }
    }
    // Outside every ward floor: take the NEAREST ward's skirt, so the blend
    // belongs to the terrace it actually descends from.
    int best = -1;
    float best_beyond = 1e9f;
    for (int i = 0; i < castle.ward_count; ++i) {
        const CastleWard& w = castle.wards[i];
        const float cheb = std::max(std::fabs(world.x - w.center.x),
                                    std::fabs(world.y - w.center.y));
        const float beyond = cheb - w.half_size;
        if (beyond < best_beyond && beyond <= w.blend) {
            best_beyond = beyond;
            best = i;
        }
    }
    if (best >= 0) {
        const CastleWard& w = castle.wards[best];
        const float skirt =
            w.height + (h - w.height) * noise::smoothstep01(best_beyond / w.blend);
        // The approach ramp descends from the OUTER ward on the gate side.
        if (best == castle.ward_count - 1) {
            const glm::vec2 local = to_local(castle, world);
            constexpr float LATERAL_FADE = 2.0f;
            if (local.y > 0.0f
                && std::fabs(local.x) <= castle.ramp_half_width + LATERAL_FADE) {
                const float ramp =
                    best_beyond >= castle.ramp_length
                        ? h
                        : w.height + (h - w.height) * (best_beyond / castle.ramp_length);
                const float wgt = std::clamp(
                    (castle.ramp_half_width + LATERAL_FADE - std::fabs(local.x))
                        / LATERAL_FADE,
                    0.0f, 1.0f);
                return skirt + (ramp - skirt) * wgt;
            }
        }
        return skirt;
    }
    return h;
}

float castle_occluder_height(const CastleBuild& castle, glm::vec2 world) {
    if (!castle.valid) {
        return 0.0f;
    }
    // The mass now sits on THREE terraces spread down the spur, so occlusion is
    // resolved per element against its own ward rather than against one pad.
    float best = 0.0f;
    for (std::size_t i = 0; i < castle.entities.size(); ++i) {
        const SiteArchetype& a = site_archetype(castle.types[i]);
        const glm::vec2 d = world - castle.entities[i].position_xz;
        const float hx = a.bounds_max.x;
        const float hz = a.bounds_max.z;
        if (std::fabs(d.x) <= hx && std::fabs(d.y) <= hz) {
            float h = 0.0f;
            switch (castle.types[i]) {
            case SiteType::CastleSolar: h = castle.solar_height; break;
            case SiteType::CastleHall: h = castle.hall_height; break;
            case SiteType::CastleTower: h = castle.tower_height; break;
            case SiteType::CastleGatehouse: h = castle.gate_height; break;
            case SiteType::CastleWall: h = castle.wall_height; break;
            default: break;
            }
            // Height above the LOCAL ward, lifted to the reference terrace so
            // callers can keep treating the result as "height above ground".
            best = std::max(best, h + (castle.entities[i].ground_y - castle.pad_height));
        }
    }
    if (best > 0.0f) {
        return best;
    }
    if (glm::length(world - castle.center) > castle.half_size * 1.5f) {
        return 0.0f; // cheap reject
    }
    const glm::vec2 local = to_local(castle, world);
    const float wall_half = WALL_SIDE * 0.5f;
    const glm::vec2 hall_c{-wall_half + HALL_X * 0.5f + 2.0f, -2.0f};
    const glm::vec2 solar_c{hall_c.x, hall_c.y - HALL_Z * 0.5f - SOLAR_SIDE * 0.5f};

    float top = 0.0f;
    (void)hall_c;
    (void)solar_c;
    if (in_box(local, solar_c, SOLAR_SIDE * 0.5f, SOLAR_SIDE * 0.5f)) {
        top = std::max(top, castle.solar_height);
    }
    if (in_box(local, hall_c, HALL_X * 0.5f, HALL_Z * 0.5f)) {
        top = std::max(top, castle.hall_height);
    }
    if (in_box(local, {0.0f, wall_half}, GATE_X * 0.5f, GATE_Z * 0.5f)) {
        top = std::max(top, castle.gate_height);
    }
    if (top == 0.0f && in_box(local, {0.0f, 0.0f}, wall_half, wall_half)) {
        top = castle.wall_height; // the curtain band
    }
    return top;
}

glm::vec2 castle_yard_point(const CastleBuild& castle) {
    // The tithe-yard is the bailey (ward 1), between hall and gate.
    const Frame f = castle_frame(castle);
    const glm::vec2 c = castle.ward_count > 1 ? castle.wards[1].center : castle.center;
    return c + f.right * 6.0f;
}

glm::vec2 castle_gate_point(const CastleBuild& castle) {
    // Just inside the gatehouse threshold, which stands on the OUTER ward.
    const CastleWard& w = castle.wards[std::max(0, castle.ward_count - 1)];
    return w.center + castle.gate_dir * (w.half_size - 2.0f);
}

glm::vec2 castle_ramp_foot(const CastleBuild& castle) {
    // The ramp descends from the outer ward, not from the chain's centre —
    // measuring it from the centre walked the player down the terrace STEPS
    // between wards and reported them as ramp discontinuities.
    const CastleWard& w = castle.wards[std::max(0, castle.ward_count - 1)];
    return w.center + castle.gate_dir * (w.half_size + castle.ramp_length);
}

} // namespace dfn::world
