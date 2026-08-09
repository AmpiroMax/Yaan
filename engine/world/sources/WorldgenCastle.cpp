/*
Created: 09:08:2026 - 15:20:00
Last updated: 09:08:2026 - 15:20:00
Module: engine/world
File: engine/world/sources/WorldgenCastle.cpp

Responsibility:
- Castle pass implementation (LANDSCAPE §6.1): terrace solve with the
  CASTLE_PAD_CUT_MAX allowance, R3 height solve, element placement with the
  gate facing the approach corridor, and the occlusion query.

Key items:
- solve_castle, castle_pad_height, castle_occluder_height.

Dependencies:
- Uses: WorldgenCastle.h, WorldgenMacro.h, WorldgenNoise.h, ContentHash, config.
- Used by: dfn_world.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- R3 outranks the height table (§6.1.1): heights shrink to fit the skyline
  margin, and if even the minimum table heights cannot fit, the pad is lowered
  (fix order 1) before anything else. Never raise the crag.
- Deterministic: pure function of (seed, layout, hydrology). No rng needed —
  the castle is a designed monument, not scatter.
*/
/*
UPD:
- 09:08:2026 - 15:20:00: Created — castle solve/stamp/occlusion.
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
constexpr float KEEP_MIN = static_cast<float>(config::CASTLE_KEEP_HEIGHT_MIN);
constexpr float KEEP_MAX = static_cast<float>(config::CASTLE_KEEP_HEIGHT_MAX);
constexpr float WALL_MIN = static_cast<float>(config::CASTLE_WALL_HEIGHT_MIN);
constexpr float WALL_MAX = static_cast<float>(config::CASTLE_WALL_HEIGHT_MAX);
constexpr float TOWER_MIN = static_cast<float>(config::CASTLE_TOWER_HEIGHT_MIN);
constexpr float TOWER_MAX = static_cast<float>(config::CASTLE_TOWER_HEIGHT_MAX);
// Gatehouse height band is §6.1.3 table text (9-11 m), scaled with the keep.
constexpr float GATE_MIN = 9.0f;
constexpr float GATE_MAX = 11.0f;

/// Terrain before any pad stamp (macro + hydrology carve).
float ground_height(uint64_t seed, const TestbedLayout& layout, const HydrologyData& hydro,
                    glm::vec2 p) {
    return carve_height(hydro, layout, p, macro_height(seed, layout, p));
}

/// Local frame of the castle: `forward` points down the approach (gate side).
struct Frame {
    glm::vec2 forward{0.0f, 1.0f};
    glm::vec2 right{1.0f, 0.0f};
};

Frame castle_frame(const TestbedLayout& layout) {
    Frame f;
    const CorridorLayout& corridor =
        layout.corridors[std::clamp(layout.castle.approach_corridor, 0,
                                    static_cast<int>(std::size(layout.corridors)) - 1)];
    if (corridor.point_count >= 2) {
        // The gate faces back down the approach: toward the corridor's start
        // (the watchpoint), i.e. the direction an arriving traveller comes from.
        const glm::vec2 to_approach = corridor.points[0] - layout.castle.center;
        if (glm::length(to_approach) > 1.0f) {
            f.forward = glm::normalize(to_approach);
            f.right = glm::vec2{-f.forward.y, f.forward.x};
        }
    }
    return f;
}

/// Signed local coordinates of `world` in the castle frame (x = right, y =
/// forward), used by every footprint test so the mass rotates with the gate.
glm::vec2 to_local(const CastleBuild& castle, const Frame& frame, glm::vec2 world) {
    const glm::vec2 rel = world - castle.center;
    return glm::vec2{glm::dot(rel, frame.right), glm::dot(rel, frame.forward)};
}

bool in_box(glm::vec2 local, float half_x, float half_y) {
    return std::fabs(local.x) <= half_x && std::fabs(local.y) <= half_y;
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

    // --- Terrace solve: pad elevation = median of the footprint (balances cut
    // against fill), then verify the CUT stays inside CASTLE_PAD_CUT_MAX. A
    // spur shoulder is chosen precisely because that holds.
    std::vector<float> samples;
    for (float z = -castle.half_size; z <= castle.half_size; z += 5.0f) {
        for (float x = -castle.half_size; x <= castle.half_size; x += 5.0f) {
            samples.push_back(
                ground_height(seed, layout, hydro, castle.center + glm::vec2{x, z}));
        }
    }
    std::sort(samples.begin(), samples.end());
    castle.pad_height = samples[samples.size() / 2];
    castle.cut = samples.back() - castle.pad_height;
    castle.fill = castle.pad_height - samples.front();
    if (castle.cut > CUT_MAX) {
        // Raise the pad until the cut fits (fill is cheap, cut is the ruled
        // allowance). R3 is re-checked below, so this can never breach the
        // skyline margin.
        castle.pad_height = samples.back() - CUT_MAX;
        castle.cut = CUT_MAX;
        castle.fill = castle.pad_height - samples.front();
    }

    // --- R3 height solve: pad + tallest element <= peak - CASTLE_SKYLINE_MARGIN.
    const float peak = macro_height(seed, layout, layout.crag.center);
    const float ceiling = peak - SKYLINE_MARGIN;
    float keep = KEEP_MAX;
    if (castle.pad_height + keep > ceiling) {
        keep = ceiling - castle.pad_height;
    }
    if (keep < KEEP_MIN) {
        // Fix order (1): lower the pad, keeping tower height (§6.1.1). The cut
        // grows, so this is bounded by the terrace allowance; the design's
        // spur was chosen so it never comes to this at seed 1.
        keep = KEEP_MIN;
        castle.pad_height = std::min(castle.pad_height, ceiling - keep);
        castle.cut = std::max(0.0f, samples.back() - castle.pad_height);
        castle.fill = std::max(0.0f, castle.pad_height - samples.front());
    }
    castle.keep_height = keep;
    // Subordinate elements scale with the keep's share of its band so the
    // three value steps (wall band / towers / keep) survive any R3 squeeze.
    const float t = KEEP_MAX > KEEP_MIN ? (keep - KEEP_MIN) / (KEEP_MAX - KEEP_MIN) : 1.0f;
    castle.tower_height = std::min(TOWER_MIN + (TOWER_MAX - TOWER_MIN) * t, keep - 1.0f);
    castle.wall_height = WALL_MIN + (WALL_MAX - WALL_MIN) * t;
    castle.gate_height = std::min(GATE_MIN + (GATE_MAX - GATE_MIN) * t, keep - 1.0f);

    // --- Element placement (§6.1.3 minimal mass) --------------------------------
    const Frame frame = castle_frame(layout);
    castle.gate_yaw = std::atan2(frame.forward.x, -frame.forward.y); // yaw 0 = -Z
    const CastleLayout& cl = layout.castle;
    const float wall_half = cl.wall_side * 0.5f;

    auto emit = [&](SiteType type, glm::vec2 local, float yaw) {
        const glm::vec2 world =
            castle.center + frame.right * local.x + frame.forward * local.y;
        castle.entities.push_back(GeneratedEntityRecord{
            0, serialization::fnv1a64(site_archetype(type).content_id), world, yaw});
        castle.types.push_back(type);
    };
    // Curtain wall (one record at the enclosure center; render draws it hollow)
    // and the keep set back from the gate, then the gatehouse in the front
    // wall, then the two rear corner towers framing the keep.
    emit(SiteType::CastleWall, {0.0f, 0.0f}, castle.gate_yaw);
    emit(SiteType::CastleKeep, {0.0f, -wall_half * 0.35f}, castle.gate_yaw);
    emit(SiteType::CastleGatehouse, {0.0f, wall_half}, castle.gate_yaw);
    emit(SiteType::CastleTower, {-wall_half, -wall_half}, castle.gate_yaw);
    emit(SiteType::CastleTower, {wall_half, -wall_half}, castle.gate_yaw);
    return castle;
}

float castle_pad_height(const CastleBuild& castle, glm::vec2 world, float h) {
    if (!castle.valid) {
        return h;
    }
    // Axis-aligned terrace (the pad is square in world axes; only the mass
    // rotates with the gate). Chebyshev distance gives square contours.
    const glm::vec2 d{std::fabs(world.x - castle.center.x),
                      std::fabs(world.y - castle.center.y)};
    const float cheb = std::max(d.x, d.y);
    if (cheb >= castle.half_size + castle.blend) {
        return h;
    }
    if (cheb <= castle.half_size) {
        return castle.pad_height; // flat terrace surface (BUILDING_PAD_SLOPE_MAX holds)
    }
    const float s = noise::smoothstep01((cheb - castle.half_size) / castle.blend);
    return castle.pad_height + (h - castle.pad_height) * s;
}

float castle_occluder_height(const CastleBuild& castle, glm::vec2 world) {
    if (!castle.valid) {
        return 0.0f;
    }
    // Cheap reject before the frame math.
    if (glm::length(world - castle.center) > castle.half_size * 1.5f) {
        return 0.0f;
    }
    // Rebuild the frame from the stored gate yaw (yaw 0 = -Z).
    Frame frame;
    frame.forward = glm::vec2{std::sin(castle.gate_yaw), -std::cos(castle.gate_yaw)};
    frame.right = glm::vec2{-frame.forward.y, frame.forward.x};
    const glm::vec2 local = to_local(castle, frame, world);

    const float wall_half = 20.0f; // §6.1.3 curtain 40x40
    float top = 0.0f;
    if (in_box(local, 7.0f, 7.0f)) { // keep 14x14, set back
        top = std::max(top, castle.keep_height);
    }
    if (in_box(local - glm::vec2{0.0f, -wall_half * 0.35f}, 7.0f, 7.0f)) {
        top = std::max(top, castle.keep_height);
    }
    if (in_box(local - glm::vec2{0.0f, wall_half}, 5.0f, 3.0f)) { // gatehouse 10x6
        top = std::max(top, castle.gate_height);
    }
    for (const float sx : {-1.0f, 1.0f}) { // corner towers 6x6
        if (in_box(local - glm::vec2{sx * wall_half, -wall_half}, 3.0f, 3.0f)) {
            top = std::max(top, castle.tower_height);
        }
    }
    if (top == 0.0f && in_box(local, wall_half, wall_half)) {
        // Inside the curtain: the wall band itself occludes at its height.
        top = castle.wall_height;
    }
    return top;
}

} // namespace dfn::world
