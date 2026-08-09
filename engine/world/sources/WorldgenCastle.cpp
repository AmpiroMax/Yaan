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

    // --- Terrace solve: pad elevation = median of the footprint (balances cut
    // against fill); if the cut exceeds the ruled allowance, raise the pad.
    std::vector<float> samples;
    for (float z = -castle.half_size; z <= castle.half_size; z += 5.0f) {
        for (float x = -castle.half_size; x <= castle.half_size; x += 5.0f) {
            samples.push_back(
                ground_height(seed, layout, hydro, castle.center + glm::vec2{x, z}));
        }
    }
    std::sort(samples.begin(), samples.end());
    castle.pad_height = samples[samples.size() / 2];
    if (samples.back() - castle.pad_height > CUT_MAX) {
        castle.pad_height = samples.back() - CUT_MAX;
    }

    // --- R3 height solve: pad + tallest element <= peak - CASTLE_SKYLINE_MARGIN.
    const float peak = macro_height(seed, layout, layout.crag.center);
    const float ceiling = peak - SKYLINE_MARGIN;
    float solar = SOLAR_MAX;
    if (castle.pad_height + solar > ceiling) {
        solar = ceiling - castle.pad_height;
    }
    if (solar < SOLAR_MIN) {
        // Fix order (1): lower the pad, keeping height (§6.1.1).
        solar = SOLAR_MIN;
        castle.pad_height = std::min(castle.pad_height, ceiling - solar);
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

    // Final cut/fill against the settled pad height.
    castle.cut = std::max(0.0f, samples.back() - castle.pad_height);
    castle.fill = std::max(0.0f, castle.pad_height - samples.front());

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

    // --- Element placement (§6.1.3 minimal mass, local frame: +y = gate side).
    const float wall_half = WALL_SIDE * 0.5f;
    auto emit = [&](SiteType type, glm::vec2 local) {
        const Frame f = castle_frame(castle);
        const glm::vec2 world = castle.center + f.right * local.x + f.forward * local.y;
        castle.entities.push_back(GeneratedEntityRecord{
            0, serialization::fnv1a64(site_archetype(type).content_id), world,
            castle.gate_yaw});
        castle.types.push_back(type);
    };
    // Curtain wall (one record at the enclosure centre; render draws it
    // hollow), the hall along the back range leaving the yard open behind the
    // gate, the solar on the hall's far end, the gatehouse in the front wall.
    emit(SiteType::CastleWall, {0.0f, 0.0f});
    emit(SiteType::CastleHall, {-wall_half + HALL_X * 0.5f + 2.0f, -2.0f});
    emit(SiteType::CastleSolar,
         {-wall_half + HALL_X * 0.5f + 2.0f, -2.0f - HALL_Z * 0.5f - SOLAR_SIDE * 0.5f});
    emit(SiteType::CastleGatehouse, {0.0f, wall_half});
    return castle;
}

float castle_pad_height(const CastleBuild& castle, glm::vec2 world, float h) {
    if (!castle.valid) {
        return h;
    }
    // Terrace: axis-aligned square (the pad is never rotated — settled).
    const float cheb = std::max(std::fabs(world.x - castle.center.x),
                                std::fabs(world.y - castle.center.y));
    if (cheb <= castle.half_size) {
        return castle.pad_height; // flat surface: BUILDING_PAD_SLOPE_MAX holds
    }
    // Everything outside is parameterised by the SAME distance-beyond-the-edge,
    // so the terrace skirt and the approach ramp meet the pad at exactly the
    // same height (no step at the threshold — the access invariant is
    // measured right through here).
    const float beyond = cheb - castle.half_size;
    const float skirt = beyond >= castle.blend
                          ? h
                          : castle.pad_height
                                + (h - castle.pad_height)
                                      * noise::smoothstep01(beyond / castle.blend);

    // Approach ramp: LINEAR grade over a longer run than the skirt, inside a
    // corridor-width band on the gate side — constant slope, no step. Its
    // outer 2 m cross-fade into the skirt so the ramp's flanks are a graded
    // edge rather than a wall.
    const glm::vec2 local = to_local(castle, world);
    constexpr float LATERAL_FADE = 2.0f;
    if (local.y > 0.0f && std::fabs(local.x) <= castle.ramp_half_width + LATERAL_FADE) {
        const float ramp = beyond >= castle.ramp_length
                             ? h
                             : castle.pad_height
                                   + (h - castle.pad_height) * (beyond / castle.ramp_length);
        const float w = std::clamp(
            (castle.ramp_half_width + LATERAL_FADE - std::fabs(local.x)) / LATERAL_FADE,
            0.0f, 1.0f);
        return skirt + (ramp - skirt) * w;
    }
    // Remaining edges: the ordinary terrace skirt (may stay steep — that is
    // the fortification read).
    return skirt;
}

float castle_occluder_height(const CastleBuild& castle, glm::vec2 world) {
    if (!castle.valid) {
        return 0.0f;
    }
    if (glm::length(world - castle.center) > castle.half_size * 1.5f) {
        return 0.0f; // cheap reject
    }
    const glm::vec2 local = to_local(castle, world);
    const float wall_half = WALL_SIDE * 0.5f;
    const glm::vec2 hall_c{-wall_half + HALL_X * 0.5f + 2.0f, -2.0f};
    const glm::vec2 solar_c{hall_c.x, hall_c.y - HALL_Z * 0.5f - SOLAR_SIDE * 0.5f};

    float top = 0.0f;
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
    // Open tithe-yard: in front of the hall range, behind the gate.
    const Frame f = castle_frame(castle);
    return castle.center + f.right * 6.0f;
}

glm::vec2 castle_gate_point(const CastleBuild& castle) {
    // Just inside the gatehouse threshold.
    const Frame f = castle_frame(castle);
    return castle.center + f.forward * (WALL_SIDE * 0.5f - 2.0f);
}

glm::vec2 castle_ramp_foot(const CastleBuild& castle) {
    return castle.center + castle.gate_dir * (castle.half_size + castle.ramp_length);
}

} // namespace dfn::world
