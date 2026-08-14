/*
Created: 10:08:2026 - 02:59:28
Last updated: 14:08:2026 - 23:36:19
Module: engine/world
File: engine/world/sources/WorldgenForest.cpp

Responsibility:
- Forest stand (LANDSCAPE §8.1) implementation: neutralized layout factory and
  the stand's P1 field — base rolls, the LF-2 ridge-and-swale grive field
  (anisotropic, direction-coherent, 2-5 m over ~100 m, swale floors
  flattened), glade taper (в9's authored calm plain), general §2.7 micro.

Key items:
- forest_stand_layout, forest_stand_height, forest_grive_component,
  forest_grive_amplitude.

Dependencies:
- Uses: WorldgenForest.h, WorldgenMacro.h, WorldgenNoise.h, config.
- Used by: WorldgenMacro.cpp (macro_height branch), Worldgen.cpp, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM (Rule 13.1): pure position functions; all draws via the seeded
  lattice streams declared in WorldgenMacro.h.
- The swale-floor flattening (the smoothstep window's lower lip) is
  LOAD-BEARING, not styling: continuous near-level swale floors are what the
  future fog pass (WEATHER W5) fills and what LF-2's own acceptance ("from a
  swale floor the neighboring swale is hidden; the crest reveals it")
  measures. Do not "simplify" it to raw noise.
*/
/*
UPD:
- 10:08:2026 - 02:59:28: Created — §8.1 stand mechanism + LF-1/LF-2 ground.
- 10:08:2026 - 10:29:50: Swale-floor lip 0.35 -> LF2_SWALE_FLOOR_FRAC 0.55: the
  continuity acceptance is a percolation problem. Largest connected floor
  component went 0.23 -> 0.84 (2048 m, seed 1) and now GROWS with the domain
  instead of shrinking. Derivation table in the header.
- 10:08:2026 - 10:40:28: Forest stand declares LF-8 (в17).
- 11:08:2026 - 15:15:55: glade_factor exported (§2.7's meso tier must taper through в9's authored calm plain too); the stand stops applying the micro octave itself now that compose_passes applies relief generally.
- 14:08:2026 - 22:27:28: one_tree_stand_layout() строится РАЗНИЦЕЙ от forest_stand_layout(), а не с
  нуля: новая нейтрализация тестбедного штампа, добавленная там, наследуется,
  а не пропускается здесь.
- 14:08:2026 - 23:36:19: gallery_stand_layout() — разницей от one_tree_stand_layout, тот же довод.
*/

#include "engine/world/sources/WorldgenForest.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenNoise.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <glm/geometric.hpp>

namespace dfn::world {

namespace {

using noise::smoothstep01;
using noise::value_noise;

/// LF-2 grive noise: two octaves of value noise on axis-stretched input, the
/// same fixed-frame bilinear axis blending the §2.1 anisotropy retune proved
/// (a position-varying rotation was rejected there for its |world|*grad(theta)
/// distortion; the reasoning transfers unchanged). Axis frames are 512 m —
/// grive direction drifts across the stand but is coherent over several
/// wavelengths, which is what "direction-coherent" means as a requirement.
float grive_noise(uint64_t seed, glm::vec2 world, bool isotropic_control) {
    const auto sample = [&](glm::vec2 q) {
        return (value_noise(seed, STREAM_FOREST_GRIVE, LF2_HILL_WAVELENGTH, q) * 2.0f
                + value_noise(seed, STREAM_FOREST_GRIVE + 1, LF2_HILL_WAVELENGTH * 0.53f, q))
             / 3.0f;
    };
    if (isotropic_control) {
        // The dictionary's named control (§2.10 LF-2): round bumps of the same
        // amplitude — the shape the user already rejected (Запрос 1).
        return sample(world);
    }
    constexpr float AXIS_CELL = 512.0f;
    const float cx = world.x / AXIS_CELL;
    const float cz = world.y / AXIS_CELL;
    const auto gx = static_cast<int64_t>(std::floor(cx));
    const auto gz = static_cast<int64_t>(std::floor(cz));
    const float tx = smoothstep01(cx - static_cast<float>(gx));
    const float tz = smoothstep01(cz - static_cast<float>(gz));
    float vals[2][2];
    for (int dz = 0; dz <= 1; ++dz) {
        for (int dx = 0; dx <= 1; ++dx) {
            const float theta =
                noise::lattice_value(seed, STREAM_FOREST_GRIVE_AXIS, gx + dx, gz + dz)
                * 3.14159265358979f;
            const glm::vec2 axis{std::cos(theta), std::sin(theta)};
            const glm::vec2 stretched{
                glm::dot(world, axis) / static_cast<float>(config::HILL_ANISOTROPY),
                world.y * axis.x - world.x * axis.y};
            vals[dz][dx] = sample(stretched);
        }
    }
    const float v0 = vals[0][0] + (vals[0][1] - vals[0][0]) * tx;
    const float v1 = vals[1][0] + (vals[1][1] - vals[1][0]) * tx;
    return v0 + (v1 - v0) * tz;
}

/// Rank equalization of the grive noise onto UNIFORM [0,1] through its own
/// empirical CDF (the FloraField technique, applied to THIS construction).
/// Without it the two-octave sum is bell-shaped: measured on the first cut,
/// the realized 128 m window relief had median 1.41 m against a declared
/// 2-5 m band — Rule 31's defect, the same one the massif model shipped.
/// The table samples the BLENDED ANISOTROPIC construction itself (fixed
/// probe seed, fixed domain): the axis-frame bilinear blend averages four
/// correlated samples and therefore compresses the distribution, so a table
/// built from the raw two-octave sum leaves the blended field mid-heavy
/// (measured: a 0.35 lip caught only 20.6% of ground instead of 35%).
/// Seed-independent: it captures the construction's SHAPE.
float grive_cdf_u(float n) {
    static const std::array<float, 257> table = [] {
        std::array<uint32_t, 1024> hist{};
        constexpr int SAMPLES = 512;
        for (int iz = 0; iz < SAMPLES; ++iz) {
            for (int ix = 0; ix < SAMPLES; ++ix) {
                const glm::vec2 p{static_cast<float>(ix) * 7.13f + 0.37f,
                                  static_cast<float>(iz) * 9.41f + 0.19f};
                const float v =
                    std::clamp(grive_noise(0xC0FFEEull, p, false), 0.0f, 0.999f);
                ++hist[static_cast<size_t>(v * 1024.0f)];
            }
        }
        std::array<float, 257> t{};
        const float total = static_cast<float>(SAMPLES) * SAMPLES;
        uint64_t run = 0;
        size_t coarse = 0;
        for (size_t i = 0; i < 1024; ++i) {
            run += hist[i];
            if ((i + 1) % 4 == 0) {
                t[++coarse] = static_cast<float>(run) / total;
            }
        }
        t[0] = 0.0f;
        t[256] = 1.0f;
        return t;
    }();
    const float x = std::clamp(n, 0.0f, 1.0f) * 256.0f;
    const auto i = static_cast<size_t>(std::min(x, 255.0f));
    const float f = x - static_cast<float>(i);
    return table[i] * (1.0f - f) + table[i + 1] * f;
}

} // namespace

float glade_factor(const TestbedLayout& layout, glm::vec2 world) {
    const float r = layout.forests.forced_clearing_radius;
    if (r <= 0.0f) {
        return 1.0f;
    }
    const float d = glm::length(world - layout.forests.forced_clearing_center);
    if (d <= r) {
        return 0.0f;
    }
    if (d >= r * 1.5f) {
        return 1.0f;
    }
    return smoothstep01((d - r) / (r * 0.5f));
}

TestbedLayout forest_stand_layout() {
    TestbedLayout l;
    l.stand = StandId::Forest;

    // §8.1: no massif, no water, no L0 — every testbed stamp neutralized so
    // no rule fires on a feature this stand does not declare (§2.10 rule 4:
    // a form appearing on a map that does not declare it is a bug).
    l.crag.center = {-4096.0f, -4096.0f};
    l.crag.radius = 0.0f;
    l.crag.peak_height = 0.0f;
    l.knoll = BumpStamp{};
    l.bluff = BumpStamp{};
    l.lake.center = {-8192.0f, -8192.0f}; // off-domain: its distance query then
    l.lake.half_extent = {1.0f, 1.0f};    // never wins against DIST_TO_WATER_RANGE
    l.troughs[0].point_count = 0;
    l.troughs[1].point_count = 0;
    for (SiteLayout& s : l.sites) {
        s.position = {-4096.0f, -4096.0f}; // P4 is skipped for this stand; belt and braces
    }
    for (CorridorLayout& c : l.corridors) {
        c.point_count = 0; // the §8.1 path NETWORK replaces the corridor system here
    }
    l.watchpoint = {-4096.0f, -4096.0f};
    l.carves.crag_tunnel.point_count = 0;
    l.carves.barrow_passage.point_count = 0;
    l.carves.barrow_chamber = CarveChamber{};
    l.carves.barrow_site_index = -1;
    l.carves.lakeshore_site_index = -1;

    // LF-7: one broadleaf mass covering the stand; glades come from the
    // clearing lattice (LF-1) and the authored calm plain below.
    l.forests.oak_rects[0] = {0.0f, 0.0f, 1024.0f, 1024.0f};
    l.forests.oak_rects[1] = {0.0f, 0.0f, 0.0f, 0.0f};
    l.forests.pine_annulus_r0 = 0.0f;
    l.forests.pine_annulus_r1 = 0.0f;
    l.forests.pine_strip = {0.0f, 0.0f, 0.0f, 0.0f};
    // в9's one preserved plain = the forced clearing: trees already respect
    // it, and the grive field tapers to calm inside it (glade_factor).
    l.forests.forced_clearing_center = {512.0f, 640.0f};
    l.forests.forced_clearing_radius = 80.0f;

    // LF-8 is in this stand's declared composition (§8.1) — в17.
    l.erosion = true;
    return l;
}

TestbedLayout one_tree_stand_layout() {
    // The inspection stand IS the forest stand's ground with everything that
    // is not ground turned off — built by DIFFERENCE from forest_stand_layout
    // rather than from scratch, so a future neutralization added there (a new
    // testbed stamp zeroed out) is inherited instead of missed here.
    TestbedLayout l = forest_stand_layout();
    l.stand = StandId::OneTree;

    // No oak mass: the stand's one tree is emitted by the scatter pass, and
    // ZERO regions is what guarantees "exactly one" — a shrunken region would
    // still scatter however many trees its lattice happens to fit.
    l.forests.oak_rects[0] = {0.0f, 0.0f, 0.0f, 0.0f};

    // The clearing covers the WHOLE domain (a chunk is 256 m; 512 m of radius
    // covers any extent this stand will ever open at), which drives the LF-2
    // grive amplitude to zero everywhere: the ground under an inspected tree
    // must not be a variable of the inspection. The ~2 % base rolls stay —
    // perfectly flat ground reads as a test grid, and the complaint list this
    // stand exists for is about the TREE.
    l.forests.forced_clearing_center = {0.0f, 0.0f};
    l.forests.forced_clearing_radius = 512.0f;

    // No LF-8: erosion gullies are terrain detail, and terrain detail is the
    // other stand's subject.
    l.erosion = false;
    return l;
}

TestbedLayout gallery_stand_layout() {
    // The gallery IS the one-tree stand's ground; only the stand id differs,
    // and with it what stands on the ground (nothing from the generator — the
    // exhibits arrive from the object registry, placed by the app).
    TestbedLayout l = one_tree_stand_layout();
    l.stand = StandId::Gallery;
    return l;
}

float forest_grive_amplitude(uint64_t seed, glm::vec2 world) {
    // Slow field, 300 m features, RANK-EQUALIZED so the draw actually covers
    // the declared band (a single interpolated octave is close enough in
    // shape to the table's two-octave sum for equalization to spread it; the
    // Rule 31 test asserts the result with quartiles, not bounds).
    const float u = grive_cdf_u(value_noise(seed, STREAM_FOREST_GRIVE_AMP, 300.0f, world));
    return LF2_HILL_RELIEF_MIN + u * (LF2_HILL_RELIEF_MAX - LF2_HILL_RELIEF_MIN);
}

float forest_grive_component(uint64_t seed, glm::vec2 world, bool isotropic_control) {
    const float n = grive_cdf_u(grive_noise(seed, world, isotropic_control));
    // Swale-floor flattening: the lower LF2_SWALE_FLOOR_FRAC of ground (exact
    // — the field is uniform after equalization) maps to the floor, so swales
    // are near-level CONTINUOUS corridors: what fog pools fill (WEATHER W5)
    // and what makes a grive read as a grive rather than a swell.
    //
    // CONTINUITY IS A PERCOLATION PROBLEM, and that is what fixes the number.
    // Measured (seed 1, 2048 m at 4 m, largest 4-connected floor component as
    // a fraction of all floor cells): lip 0.35 -> 0.23, 0.45 -> 0.43,
    // 0.50 -> 0.47, 0.55 -> 0.84, 0.60 -> 0.99. The jump between 0.50 and
    // 0.55 is the site-percolation transition (realized floor area 0.60 at
    // lip 0.55, against the square-lattice p_c = 0.593) — BELOW it the floors
    // are disconnected potholes no matter how the noise is tuned, above it
    // they are one channel network. The first cut sat at 0.35 and measured a
    // 0.23 largest component: fog would have pooled in puddles.
    //
    // Above the lip the profile is an EASE-OUT, not a symmetric smoothstep:
    // the divide between two swales must carry real height (BR-5's "2-5 m
    // crests beat a 1.7 m eye" is arithmetic about the DIVIDE, not about the
    // rare highest crest). Measured with the symmetric profile: zero
    // floor-to-floor pairs at 60-110 m had a >= 1.5 m divide between them —
    // the shape kept all its height in the top decile of ground.
    const float t = std::clamp((n - LF2_SWALE_FLOOR_FRAC) / (1.0f - LF2_SWALE_FLOOR_FRAC),
                               0.0f, 1.0f);
    const float profile = 1.0f - (1.0f - t) * (1.0f - t);
    return forest_grive_amplitude(seed, world) * profile;
}

float forest_stand_height(uint64_t seed, const TestbedLayout& layout, glm::vec2 world) {
    float h = FOREST_BASE_ELEV;
    // LF-1 base: gentle macro rolls (~2% at the 512 m octave) — enough
    // elevation change for the stone-steps path class, never competing with
    // the grives for the middle distance.
    h += value_noise(seed, STREAM_FOREST_BASE, 512.0f, world) * FOREST_BASE_AMP;
    // LF-2 grives, tapered inside the authored glade.
    h += forest_grive_component(seed, world, false) * glade_factor(layout, world);
    // §2.7's relief is NOT applied here any more. It moved to compose_passes,
    // where it is general to every stand and carries its shore, corridor and
    // massif masks — one implementation, one application site (Rule 32). This
    // function stayed the stand's P1 field; adding the octave here as well
    // would have doubled it on exactly one map.
    return std::clamp(h, 0.0f, static_cast<float>(config::WORLDGEN_MAX_HEIGHT));
}

} // namespace dfn::world
