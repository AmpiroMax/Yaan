/*
Created: 11:08:2026 - 15:12:44
Last updated: 11:08:2026 - 15:12:44
Module: engine/world
File: engine/world/sources/WorldgenOutcrop.cpp

Responsibility:
- Implementation of §10.5 B2: the outcrop lattice, the convex-curvature anchor,
  the bedding field, and the two sub-forms' profiles.

Key items:
- outcrop_at, outcrop_height, outcrops_in.

Dependencies:
- Uses: WorldgenOutcrop.h, WorldgenMacro.h, WorldgenNoise.h, WorldgenSites.h,
  config.
- Used by: Worldgen.cpp, WorldgenScatter.cpp, WorldgenCensus.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic and position-based (Rule 13.1).
*/
/*
UPD:
- 11:08:2026 - 15:12:44: Created.
*/

#include "engine/world/sources/WorldgenOutcrop.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenNoise.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <glm/geometric.hpp>

namespace dfn::world {

namespace {

constexpr float TAU = 6.28318530717958647692f;

/// The placement lattice. OUTCROP_CELL is the §2.2 outcrop-cluster spacing and
/// this is HALF of it, which is not a taste: at one candidate per cell the
/// realised density is (keep probability)/cell^2, and a 120 m cell caps out at
/// 0.69/ha — under OUTCROP_DENSITY_MAX. At 60 m the whole approved 0.4-1.2/ha
/// band is reachable with a keep probability under 1, so the DENSITY CONSTANT
/// governs the density rather than the lattice silently clamping it.
constexpr float CELL = static_cast<float>(config::OUTCROP_CELL) * 0.5f;

/// Half the biggest outcrop, i.e. how far one can reach out of its own cell.
constexpr float MAX_RADIUS = static_cast<float>(config::OUTCROP_EXTENT_MAX) * 0.5f;

float smoothstep01(float t) {
    const float x = std::clamp(t, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

/// A cheap deterministic draw for cell (gx, gz), slot `k`.
float cell_val(uint64_t seed, int64_t gx, int64_t gz, int k) {
    return noise::lattice_value(seed, STREAM_OUTCROP + static_cast<uint32_t>(k), gx, gz);
}

/// THE BEDDING FIELD (§10.3.2, §10.5 B2). Dip azimuth is sampled on a lattice
/// of BEDDING_AZIMUTH_COHERENCE so neighbouring outcrops AGREE about which way
/// the rock dips — that shared plane is the entire reason a group of them reads
/// as one bedrock instead of as scattered props. Interpolated, not stepped, so
/// the agreement does not end at a cell boundary.
void bedding_at(uint64_t seed, glm::vec2 world, float& dip_out, glm::vec2& dir_out) {
    const float cell = static_cast<float>(config::BEDDING_AZIMUTH_COHERENCE);
    const float u = noise::value_noise(seed, STREAM_OUTCROP_BEDDING, cell, world);
    const float v = noise::value_noise(seed, STREAM_OUTCROP_BEDDING + 1, cell, world);
    const float theta = u * TAU;
    dir_out = {std::cos(theta), std::sin(theta)};
    dip_out = static_cast<float>(config::BEDDING_DIP_MIN)
            + v * static_cast<float>(config::BEDDING_DIP_MAX - config::BEDDING_DIP_MIN);
    dip_out *= 0.01745329252f; // degrees in NUMBERS.md, radians in the field
}

/// §10.5 B2's anchor: OUTCROPS APPEAR WHERE EROSION STRIPS, NEVER WHERE IT
/// DEPOSITS. Mean curvature of the macro field, measured over the meso band:
/// negative Laplacian = convex = a shoulder, a spur nose, a scarp lip. Positive
/// = a hollow, which collects soil and is forbidden.
float convexity(uint64_t seed, const TestbedLayout& layout, glm::vec2 p) {
    // The arm is the meso octave's short wavelength: curvature read at a
    // shorter arm than the band that shapes the ground would report noise, and
    // read at a longer one would report the hill band's shape instead.
    constexpr float ARM = static_cast<float>(config::GROUND_MESO_WAVELENGTH_MIN) * 0.5f;
    const float h0 = macro_height(seed, layout, p);
    const float hx1 = macro_height(seed, layout, {p.x + ARM, p.y});
    const float hx0 = macro_height(seed, layout, {p.x - ARM, p.y});
    const float hz1 = macro_height(seed, layout, {p.x, p.y + ARM});
    const float hz0 = macro_height(seed, layout, {p.x, p.y - ARM});
    return -(hx1 + hx0 + hz1 + hz0 - 4.0f * h0); // > 0 == convex
}

/// The per-bearing outline wobble. A circular outcrop reads as a bubble; the
/// support-polygon lesson from §2.8.2 in miniature, done with harmonics because
/// at this size the outline is a few pixels of silhouette and needs shape, not
/// facets.
float outline_scale(uint64_t seed, int64_t gx, int64_t gz, float bearing) {
    float s = 1.0f;
    for (int m = 2; m <= 4; ++m) {
        const float phase = cell_val(seed, gx, gz, 10 + m) * TAU;
        const float amp = 0.10f + cell_val(seed, gx, gz, 20 + m) * 0.12f;
        s += amp * std::cos(static_cast<float>(m) * bearing + phase);
    }
    return std::max(0.35f, s);
}

/// The candidate of cell (gx, gz), if that cell carries one.
bool cell_outcrop(uint64_t seed, const TestbedLayout& layout, int64_t gx, int64_t gz,
                  Outcrop& out) {
    // THE COUNTERFACTUAL ARM, in the shipping binary on purpose (Rule 30): the
    // same env that stands §2.7's relief down stands the rock down, so "the
    // world before the object grammar" is one run away and never has to be
    // reconstructed from memory or from a stash on a shared tree.
    if (std::getenv("DFN_NO_RELIEF") != nullptr) return false;
    // Density draw, then the keep roll. Both from the cell's own stream, so a
    // cell decides alone and neighbours never need to talk (Rule 13.1).
    const float density = static_cast<float>(config::OUTCROP_DENSITY_MIN)
                        + cell_val(seed, gx, gz, 0)
                              * static_cast<float>(config::OUTCROP_DENSITY_MAX
                                                   - config::OUTCROP_DENSITY_MIN);
    const float keep = density * CELL * CELL / 10000.0f; // per hectare -> per cell
    if (cell_val(seed, gx, gz, 1) > keep) {
        return false;
    }

    // FOUR FIXED SUB-POSITIONS, AND THE MOST CONVEX ONE WINS. A rejection test
    // would have thrown away exactly the ground the anchor is about and left
    // the realised density below the approved band with no way to see it; a
    // search spends the same draw and lands the rock where erosion would have
    // stripped it. Fixed order, first-best on ties (Rule 13.1).
    // Confined to the middle half of the cell so nothing reaches further than
    // one cell away and the 3x3 neighbourhood below is exact.
    glm::vec2 best{0.0f};
    float best_c = -1e9f;
    for (int k = 0; k < 4; ++k) {
        const glm::vec2 p{
            (static_cast<float>(gx) + 0.25f + cell_val(seed, gx, gz, 2 + k) * 0.5f) * CELL,
            (static_cast<float>(gz) + 0.25f + cell_val(seed, gx, gz, 6 + k) * 0.5f) * CELL};
        const float c = convexity(seed, layout, p);
        if (c > best_c) {
            best_c = c;
            best = p;
        }
    }
    // FORBIDDEN IN CONCAVITIES. A hollow collects soil; rock in it explains
    // nothing and reads as sprinkled.
    if (best_c <= 0.0f) {
        return false;
    }

    // Never on graded ground: §2.4 corridors and building pads are cut flat on
    // purpose, and §10.1.2 exempts them from the bumpiness floor for the same
    // reason.
    if (corridor_distance(layout, best)
        < static_cast<float>(config::CORRIDOR_WIDTH) * 0.5f + MAX_RADIUS) {
        return false;
    }
    // Never above the massif's cliffline: §2.8 owns that surface, bands, tor
    // and all. The HEM is deliberately still allowed — it is the single largest
    // siting ground in the world and the place design wants band rhythm.
    if (layout.crag.radius > 0.0f
        && glm::length(best - layout.crag.center) < layout.crag.radius) {
        const float cliffline = layout.crag.peak_height
                              * static_cast<float>(config::MASSIF_CLIFFLINE_FRAC);
        if (macro_height(seed, layout, best) >= cliffline) {
            return false;
        }
    }

    // --- Form, size and bedding ----------------------------------------------
    const float form = cell_val(seed, gx, gz, 30);
    out.boss = form >= 0.5f;
    const float t = cell_val(seed, gx, gz, 31);
    const float ext_min = static_cast<float>(config::OUTCROP_EXTENT_MIN);
    const float ext_max = static_cast<float>(config::OUTCROP_EXTENT_MAX);
    if (out.boss) {
        // Boss/tor: the big half of the extent band, proud by OUTCROP_PROUD_BOSS.
        out.extent = (ext_min + (ext_max - ext_min) * (0.2f + 0.8f * t)) * 0.5f;
        out.proud = static_cast<float>(config::OUTCROP_PROUD_BOSS_MIN)
                  + cell_val(seed, gx, gz, 32)
                        * static_cast<float>(config::OUTCROP_PROUD_BOSS_MAX
                                             - config::OUTCROP_PROUD_BOSS_MIN);
    } else {
        // Pavement/slab: the small half, proud by OUTCROP_PROUD_SLAB.
        out.extent = (ext_min + (ext_max - ext_min) * (0.5f * t)) * 0.5f;
        out.proud = static_cast<float>(config::OUTCROP_PROUD_SLAB_MIN)
                  + cell_val(seed, gx, gz, 32)
                        * static_cast<float>(config::OUTCROP_PROUD_SLAB_MAX
                                             - config::OUTCROP_PROUD_SLAB_MIN);
    }
    out.centre = best;
    bedding_at(seed, best, out.dip, out.dip_dir);
    return true;
}

/// The rock's own surface above the soil at `world`, given its parameters.
float rock_profile(uint64_t seed, int64_t gx, int64_t gz, const Outcrop& r, glm::vec2 world) {
    const glm::vec2 rel = world - r.centre;
    const float dist = glm::length(rel);
    if (dist <= 1e-4f) {
        return r.proud;
    }
    const float bearing = std::atan2(rel.y, rel.x);
    const float radius = r.extent * outline_scale(seed, gx, gz, bearing);
    if (dist >= radius) {
        return 0.0f;
    }
    const float q = dist / radius;

    // A PLATEAU WITH A RIM, NOT A DOME. The read of an exposed slab is the dark
    // line under its lip (§10.2, point 2) — noise has gradients and no edges,
    // and a dome would have reproduced the very thing this class exists to
    // break. So the top is flat over most of the footprint and falls in the
    // outer fifth. The 1 m voxel rounds the lip; it cannot round away the step.
    const float top = 1.0f - smoothstep01((q - 0.8f) / 0.2f);
    float h = r.proud * top;

    // BEDDING. The surface is cut into shelves normal to the dip plane, so the
    // rock steps DOWN-DIP: what makes frame 06's shelves read as bedrock rather
    // than as a lump. Neighbours share the azimuth (bedding_at), which is what
    // makes a group of them one geology.
    if (r.boss && h > 0.0f) {
        // Shelf thickness from the mass's own relief: enough shelves to read as
        // steps, never so many they alias at 1 m voxels.
        const float shelf = std::max(static_cast<float>(config::VOXEL_SIZE), r.proud / 4.0f);
        const float plane = std::tan(r.dip) * glm::dot(rel, r.dip_dir);
        const float s = h - plane;
        h = std::floor(s / shelf) * shelf + plane + shelf * 0.5f;
        h = std::clamp(h, 0.0f, r.proud);
    }
    return h;
}

} // namespace

OutcropHit outcrop_at(uint64_t seed, const TestbedLayout& layout, glm::vec2 world) {
    const int64_t cx = static_cast<int64_t>(std::floor(world.x / CELL));
    const int64_t cz = static_cast<int64_t>(std::floor(world.y / CELL));
    OutcropHit best;
    float best_extent = -1.0f;
    for (int64_t dz = -1; dz <= 1; ++dz) {
        for (int64_t dx = -1; dx <= 1; ++dx) {
            Outcrop r;
            if (!cell_outcrop(seed, layout, cx + dx, cz + dz, r)) continue;
            if (glm::length(world - r.centre) > r.extent * 1.35f) continue; // outline max
            if (r.extent > best_extent) {
                best_extent = r.extent;
                best.hit = true;
                best.rock = r;
            }
        }
    }
    return best;
}

float outcrop_height(uint64_t seed, const TestbedLayout& layout, glm::vec2 world) {
    const int64_t cx = static_cast<int64_t>(std::floor(world.x / CELL));
    const int64_t cz = static_cast<int64_t>(std::floor(world.y / CELL));
    float h = 0.0f;
    for (int64_t dz = -1; dz <= 1; ++dz) {
        for (int64_t dx = -1; dx <= 1; ++dx) {
            const int64_t gx = cx + dx;
            const int64_t gz = cz + dz;
            // Cheap reject before the curvature test: the cell's candidate can
            // only be inside its own middle half, so anything further than
            // CELL/4 + max outline is out of reach.
            const glm::vec2 cell_centre{(static_cast<float>(gx) + 0.5f) * CELL,
                                        (static_cast<float>(gz) + 0.5f) * CELL};
            if (glm::length(world - cell_centre) > CELL * 0.25f + MAX_RADIUS * 1.35f) {
                continue;
            }
            Outcrop r;
            if (!cell_outcrop(seed, layout, gx, gz, r)) continue;
            // MAX, not sum: two overlapping masses are one rock, and adding
            // them would build a spike exactly where they meet.
            h = std::max(h, rock_profile(seed, gx, gz, r, world));
        }
    }
    return h;
}

std::vector<Outcrop> outcrops_in(uint64_t seed, const TestbedLayout& layout, glm::vec2 area_min,
                                 glm::vec2 area_max) {
    std::vector<Outcrop> out;
    const int64_t x0 = static_cast<int64_t>(std::floor(area_min.x / CELL));
    const int64_t x1 = static_cast<int64_t>(std::floor(area_max.x / CELL));
    const int64_t z0 = static_cast<int64_t>(std::floor(area_min.y / CELL));
    const int64_t z1 = static_cast<int64_t>(std::floor(area_max.y / CELL));
    for (int64_t gz = z0; gz <= z1; ++gz) {
        for (int64_t gx = x0; gx <= x1; ++gx) {
            Outcrop r;
            if (!cell_outcrop(seed, layout, gx, gz, r)) continue;
            if (r.centre.x < area_min.x || r.centre.x >= area_max.x) continue;
            if (r.centre.y < area_min.y || r.centre.y >= area_max.y) continue;
            out.push_back(r);
        }
    }
    return out;
}

} // namespace dfn::world
