/*
Created: 13:08:2026 - 21:50:00
Last updated: 13:08:2026 - 21:50:00
Module: engine/world
File: engine/world/sources/WorldgenFlow.h

Responsibility:
- THE DRAINAGE: valleys placed where water would actually run, cut once per
  world on a coarse grid and sampled as a height delta. Replaces the fixed-pitch
  draw comb (WorldgenForms::draw_forms) as the source of ground-hides-ground.

Key items:
- FlowGrid: the baked incision (metres, signed, <= 0) + bilinear sample(),
  and the drainage area behind it so consumers can ask "how big is the stream
  here" without a second solve.
- FlowParams: the transport/shape constants, each a REQUESTED NUMBERS row.
- build_flow(): the pass. `enabled = false` returns the ZERO grid through the
  SAME entry point -- this pass's own named control (Rule 30).

Dependencies:
- Uses: glm, <vector>, <functional>.
- Used by: Worldgen.cpp (context build + compose_passes).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- WHY THIS EXISTS AND WHAT IT REPLACES, because the number is the whole argument.
  The old draws were a lattice of parallel channels at a FIXED 14 m pitch. Three
  independent measurements said that pitch is wrong by an order of magnitude
  (docs/design/TERRAIN_REFERENCE.md):
    * 8-20 m band power: ours 9.2 %, real land 0.00-0.05 % -- x184-920;
    * first-order valley spacing (Perron 2009, lidar, five sites): 30-321 m,
      and 30 m is the SHORTEST ever measured, in the weakest material there is;
    * drainage density: ours ~71 km/km2, nature 0.5-10.
  A comb also cannot put its valleys where water goes, because it has no idea
  where water goes. This pass computes that instead of guessing it.
- DETERMINISM (Rule 13.1). The grid is a pure function of (seed, domain, params,
  base field). Depression filling, receiver choice and accumulation all run in a
  fixed order; the accumulation order is elevation-sorted with the cell index as
  the tie-break, so equal heights cannot reorder the sum. Float addition is not
  associative -- do not reorder or parallelise without making the reduction
  order-independent, or the world file stops being reproducible.
- THE GRID IS GLOBAL ON PURPOSE. Drainage area is a catchment-wide quantity: a
  chunk cannot compute it, and a chunk that tried would produce channels that
  stop at its own border. One grid per world, built at context build, read and
  interpolated by every chunk -- the same shape ErosionGrid already uses.
*/
/*
UPD:
- 13:08:2026 - 21:50:00: Created -- valleys where the water runs, replacing the
  fixed-pitch comb.
*/

#pragma once

#include <cstdint>
#include <functional>
#include <glm/vec2.hpp>
#include <vector>

namespace dfn::world {

/// The baked drainage incision: a height delta in metres, always <= 0.
struct FlowGrid {
    glm::vec2 origin{0.0f, 0.0f}; ///< world position of cell (0,0)
    float cell = 0.0f;            ///< cell size (m); 0 => the grid is empty
    int n = 0;                    ///< n x n cells
    std::vector<float> delta;     ///< n*n, row-major (z-major), <= 0
    std::vector<float> area;      ///< n*n, drainage area (m^2) behind each cell

    /// Bilinear sample of the incision, 0 outside the grid, so the pass fades
    /// out at the domain margin instead of stepping.
    [[nodiscard]] float sample(glm::vec2 world) const;

    /// Drainage area (m^2) at a position, 0 outside. Nearest-cell: this is a
    /// catchment label, and interpolating across a divide would invent a stream
    /// halfway up a hillside.
    [[nodiscard]] float area_at(glm::vec2 world) const;

    [[nodiscard]] bool empty() const { return cell <= 0.0f || n <= 0; }
};

/// REQUESTED NUMBERS ROWS (Rule 35), each stated beside the term it scales.
/// Derivations live in the .cpp next to the code that spends them.
struct FlowParams {
    float cell = 6.0f;    ///< grid resolution (m). Must resolve a valley FLOOR,
                          ///< not a valley: at 6 m a 60 m-spaced network has 10
                          ///< cells between channels, and the narrowest channel
                          ///< we cut is 3 cells wide (Houdini's own rule of
                          ///< thumb: an erosion feature needs >= 3 voxels or it
                          ///< aliases into a line)
    float margin = 128.0f; ///< domain padding (m), so a catchment that drains
                           ///< off the map still has its upper reaches
    /// THE CHANNEL THRESHOLD. Below this drainage area a cell is hillslope and
    /// is not cut at all -- this is the support area of a channel head, and it
    /// is what sets valley SPACING without ever naming a pitch. Perron 2009
    /// gives first-order spacing 30-321 m; a support area A_c produces a mean
    /// spacing of roughly 2*sqrt(A_c), so 3000 m^2 lands near 110 m, inside the
    /// measured band and an order above the 14 m comb it replaces.
    float channel_area_m2 = 20000.0f;
    /// Incision depth as a function of drainage area: d = depth_coef *
    /// (A/channel_area)^depth_exp, clamped to depth_max. The exponent is the
    /// hydraulic-geometry one (depth ~ Q^0.4, Q ~ A), so a trunk is deeper than
    /// its tributaries BY THE SAME LAW that makes it wider -- one relation, two
    /// consequences, instead of two tuned fields.
    float depth_coef = 2.5f;
    float depth_exp = 0.40f;
    float depth_max = 18.0f; ///< safety rail on |delta|, not a shaping tool
    /// Valley WIDTH: the incision is spread sideways by this many cells per
    /// unit of the same area law, which is what turns a slot into a valley.
    /// A slot at grid resolution is exactly the "claw mark" being removed.
    float width_coef = 2.2f;  ///< cells of half-width at the channel threshold
    float width_exp = 0.35f;
    float width_max_cells = 14.0f;
    /// How many smoothing passes shape the valley cross-section. Each pass is a
    /// width-aware blur of the depth field, so a big valley gets a wide gentle
    /// section and a headwater rill stays tight.
    int shape_passes = 3;
};

/// Runs the pass over [domain_min, domain_max] (expanded by params.margin) on
/// `base_height`, and returns the baked incision.
///
/// `enabled == false` returns a grid of the right geometry with every delta 0 --
/// this pass's named control, reached through the SAME entry point, because a
/// control reached by a different code path proves the other path.
[[nodiscard]] FlowGrid build_flow(uint64_t seed, glm::vec2 domain_min, glm::vec2 domain_max,
                                  const FlowParams& params,
                                  const std::function<float(glm::vec2)>& base_height,
                                  bool enabled = true);

} // namespace dfn::world
