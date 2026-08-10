/*
Created: 10:08:2026 - 10:39:07
Last updated: 10:08:2026 - 10:39:07
Module: engine/world
File: engine/world/sources/WorldgenErosion.h

Responsibility:
- LF-8, the EROSION OVERLAY (LANDSCAPE §2.10, user-ratified в17): seeded
  droplet (hydraulic) erosion on a coarse grid, run once per world at context
  build and sampled as a height DELTA by the final height field. Not a place
  but a pass — maps declare it like a landform, so it lives beside them.

Key items:
- ErosionGrid: the baked delta field (meters, signed) + bilinear sample().
- ErosionParams: the transport constants, each a REQUESTED NUMBERS row.
- build_erosion(): the pass. `enabled = false` returns the ZERO grid, which
  is §2.10 LF-8's own named control ("the same map with the pass OFF must fail
  the gully acceptance") — the control is the same code path, not a second one.

Dependencies:
- Uses: glm, <vector>, <functional>.
- Used by: Worldgen.cpp (context build + terrain_height), forest-stand tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM (Rule 13.1) IS THE WHOLE DIFFICULTY HERE. The grid is a pure
  function of (seed, domain, params, base field): droplets are drawn from one
  seeded stream in a fixed order, every droplet is simulated to completion
  before the next starts, and all accumulation is into the same grid in that
  order. Do NOT parallelize this loop without making the reduction
  order-independent — float addition is not associative and the world file
  would stop being reproducible.
- Rule 24: written from the transport math (see the .cpp's derivation), not
  vendored from a reference implementation.
*/
/*
UPD:
- 10:08:2026 - 10:39:07: Created — LF-8 droplet erosion (в17) with the
  pass-OFF control built into the same entry point.
*/

#pragma once

#include <cstdint>
#include <functional>
#include <glm/vec2.hpp>
#include <vector>

namespace dfn::world {

/// The baked erosion field: a signed height delta in meters on a regular grid.
/// Negative = cut (gully, hollow), positive = deposit (outwash fan).
struct ErosionGrid {
    glm::vec2 origin{0.0f, 0.0f}; ///< world position of cell (0,0)
    float cell = 0.0f;            ///< cell size (m); 0 => the grid is empty
    int n = 0;                    ///< n x n cells
    std::vector<float> delta;     ///< n*n, row-major (z-major)

    /// Bilinear sample, 0 outside the grid (so the pass fades out at the
    /// domain margin instead of stepping — a step here would read as a wall).
    [[nodiscard]] float sample(glm::vec2 world) const;

    /// Raw cell read, 0 out of range (tests and the fan/gully association).
    [[nodiscard]] float at(int x, int z) const;
};

/// Transport constants. REQUESTED NUMBERS rows (Rule 35 — design accepts gully
/// and fan acceptances against the shapes these produce); derivations in the
/// .cpp beside the term each one scales.
struct ErosionParams {
    float cell = 4.0f;          ///< grid resolution (m)
    float margin = 96.0f;       ///< domain padding (m) so edge droplets are not clipped mid-gully
    int droplets_per_km2 = 6000; ///< droplet DENSITY, not a count: the pass must not weaken as a stand grows
    int max_steps = 96;         ///< droplet lifetime in cells
    float inertia = 0.35f;      ///< direction carry-over [0,1)
    float capacity = 5.0f;      ///< sediment capacity per unit (slope * speed * water)
    float min_slope = 0.001f;   ///< capacity floor. THIS IS THE FAN MECHANISM: a droplet reaching level ground must lose its capacity, so it drops its load AT THE GULLY MOUTH. Raising it to 0.02 smears the same sediment over the whole run and the fans stop being associated with their gullies (measured: uphill/downhill association ratio 1.26 at 0.02, 1.46 at 0.001)
    float erode_rate = 0.10f;   ///< fraction of the capacity deficit taken per step
    float deposit_rate = 0.70f; ///< fraction of the excess dropped per step; deliberately >> erode_rate, so cutting is patient and dropping is decisive
    float evaporation = 0.020f; ///< water lost per step
    float gravity = 6.0f;       ///< speed integration
    float brush_radius = 2.0f;  ///< erosion is spread over this radius (cells) — a point cut makes 1-cell spikes, not gullies
    float initial_water = 1.0f;
    float initial_speed = 1.0f;
    float max_cut = 1.5f;       ///< clamp on |delta| (m) so the pass decorates the landform, never replaces it: a SAFETY RAIL, not a shaping tool — at the shipped rates the 99th percentile cut is 0.6 m and only 0.1% of cells reach it
};

/// Runs the pass over [domain_min, domain_max] (expanded by params.margin) on
/// the field `base_height`, and returns the baked delta.
///
/// `enabled == false` returns a grid of the right geometry with every delta 0
/// — §2.10 LF-8's named control. It is the SAME entry point on purpose: a
/// control reached by a different code path proves the other path, not this
/// one.
[[nodiscard]] ErosionGrid build_erosion(uint64_t seed, glm::vec2 domain_min,
                                        glm::vec2 domain_max, const ErosionParams& params,
                                        const std::function<float(glm::vec2)>& base_height,
                                        bool enabled = true);

} // namespace dfn::world
