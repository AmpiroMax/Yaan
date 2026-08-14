/*
Created: 10:08:2026 - 02:59:28
Last updated: 14:08:2026 - 22:27:28
Module: engine/world
File: engine/world/sources/WorldgenForest.h

Responsibility:
- The FOREST STAND (LANDSCAPE §8.1 «лесок», user-ratified в1/в9): layout
  factory + the stand's macro height field. Declared landform composition
  (§2.10 rule 4): LF-1 rolling plain (glades + the one preserved calm plain),
  LF-2 ridge-and-swale hills, LF-5 crest/outcrop, LF-7 forest floor,
  LF-8 erosion overlay. No massif, no water, no L0 — deliberately.

Key items:
- forest_stand_layout(): a TestbedLayout with stand = Forest and every
  testbed feature neutralized (massif radius 0, lake moved off-domain, no
  troughs/sites/corridors/carves; oak mass covers the stand).
- forest_stand_height(): base rolls + LF-2 grive field + glade taper +
  general §2.7 micro-relief (pure position function; LF-8/path carves are
  context passes applied above this in Worldgen.cpp).
- forest_grive_component() / forest_grive_amplitude(): the LF-2 field alone,
  exposed for the Rule 30/31 acceptance tests (elongation vs the isotropic
  control; amplitude distribution over the declared 2-5 m band).

Dependencies:
- Uses: TestbedLayout.h, WorldgenMacro.h (streams, ground_micro_relief),
  WorldgenNoise.h, config.
- Used by: WorldgenMacro.cpp (macro_height branch), Worldgen.cpp, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM (Rule 13.1): pure position-based functions only in this header;
  everything stateful (erosion grid, path network) lives in WorldGenContext.
- LF2_* literals below are REQUESTED NUMBERS rows (flagged to the lead with
  derivations, Rule 14/35); replace with dfn::config names when they land.
*/
/*
UPD:
- 10:08:2026 - 02:59:28: Created — stand selector mechanism + LF-1/LF-2
  ground per §8.1 (grive field: anisotropic, direction-coherent, 2-5 m over
  ~100 m, swale floors flattened for fog/BR-5; glade = authored calm plain
  per в9; §2.7 micro applied generally — the stand has no water, so the
  shore taper clause is vacuous here).
- 10:08:2026 - 10:29:50: THE EQUALIZER'S SECOND HALF (recovered work): LF2_SWALE_FLOOR_FRAC
  0.55 — the swale-floor lip is a PERCOLATION threshold, derived and measured,
  not taste. Below it the floors are disconnected potholes at every noise
  tuning.
- 11:08:2026 - 15:15:55: glade_factor published with the measurement that forced it (glade relief 3.43 m against the stand's own 3.0 m budget).
- 14:08:2026 - 22:27:28: one_tree_stand_layout() — смотровой стенд одного дерева. Слой объявляет
  ЗЕМЛЮ (поляна кроет весь пролёт, эрозия выключена, дубовой массы нет);
  само дерево ставит скаттер-проход, потому что раскладка говорит про землю,
  а что на ней стоит — не её слово.
*/

#pragma once

#include "engine/world/sources/TestbedLayout.h"

#include <cstdint>
#include <glm/vec2.hpp>

namespace dfn::world {

/// REQUESTED NUMBERS rows (Rule 35 — design accepts against these; flagged to
/// the lead 10.08.2026 with derivations):
/// - LF2_HILL_RELIEF_MIN/MAX = 2/5 m: user в9 (б) «холмистость 2-5 м» verbatim;
///   also §2.10 LF-2's recipe band.
/// - LF2_HILL_WAVELENGTH = 100 m: §8.1 brief ("2-5 m relief over ~100 m
///   wavelengths"); BR-5 arithmetic wants crests a walker passes between
///   finds — at 100 m a 40-80 m occlusion ring (FIND_OCCLUSION test) always
///   crosses at least one crest.
/// - FOREST_BASE_AMP = 10 m / FOREST_BASE_ELEV = 20 m: gentle macro rolls
///   (~2% slopes at 512 m cell) so routes gain the elevation change the
///   stone-steps class needs, atop a datum clear of the quantization floor.
/// - LF2_SWALE_FLOOR_FRAC = 0.55: the fraction of ground held at the swale
///   floor. DERIVED FROM PERCOLATION, not taste — the swale floors must form
///   ONE connected channel network for fog to pool along (WEATHER W5) and for
///   LF-2's own acceptance to mean anything. Measured largest-connected-floor
///   fraction vs the lip: 0.35 -> 0.23, 0.45 -> 0.43, 0.50 -> 0.47,
///   0.55 -> 0.84, 0.60 -> 0.99. The transition between 0.50 and 0.55 is site
///   percolation (realized floor AREA 0.60 at this lip, square-lattice
///   p_c = 0.593); the value sits just above the threshold so floors connect
///   while grives still hold 40% of the ground.
inline constexpr float LF2_HILL_RELIEF_MIN = 2.0f;
inline constexpr float LF2_HILL_RELIEF_MAX = 5.0f;
inline constexpr float LF2_HILL_WAVELENGTH = 100.0f;
inline constexpr float LF2_SWALE_FLOOR_FRAC = 0.55f;
inline constexpr float FOREST_BASE_AMP = 10.0f;
inline constexpr float FOREST_BASE_ELEV = 20.0f;

/// The §8.1 forest stand as generator input data: stand = Forest, testbed
/// features neutralized, oak mass covering the stand, the authored glade
/// (в9's one preserved calm plain) as the forced clearing.
[[nodiscard]] TestbedLayout forest_stand_layout();

/// The ONE-TREE inspection stand: the forest stand's neutralized layout with
/// no oak mass, no erosion, and the forced clearing covering the whole domain
/// so the grives go calm everywhere. The single tree itself is emitted by the
/// scatter pass (its position derives from the domain, not from this table —
/// the layout declares GROUND, the scatter pass declares what stands on it).
[[nodiscard]] TestbedLayout one_tree_stand_layout();

/// The stand's P1 height: base rolls + LF-2 grives (tapered inside the glade)
/// + §2.7 micro-relief. Pure function of (seed, world); the LF-8 erosion
/// delta and the path carve are context passes composed on top by
/// stand_height_adjust (Worldgen.h).
/// в9's ONE authored calm plain, as a 0..1 factor: 0 inside the forced
/// clearing, 1 half a radius outside it. Exported because §2.7's meso tier
/// must taper through it too — a 25-60 m octave at 1.5-4 m inside the
/// deliberately calm plain is exactly the thing that plain exists not to be
/// (measured: glade relief 3.43 m against the stand's own 3.0 m budget).
[[nodiscard]] float glade_factor(const TestbedLayout& layout, glm::vec2 world);

[[nodiscard]] float forest_stand_height(uint64_t seed, const TestbedLayout& layout,
                                        glm::vec2 world);

/// The LF-2 meso component alone (meters above the swale floor, >= 0),
/// WITHOUT the glade taper — the object the acceptance tests measure.
/// `isotropic_control` swaps the anisotropic stretch for a round-bump field
/// of the same amplitude: the shape the user already rejected (Запрос 1), the
/// dictionary's named control — it must FAIL the elongation acceptance.
[[nodiscard]] float forest_grive_component(uint64_t seed, glm::vec2 world,
                                           bool isotropic_control = false);

/// The LF-2 slow amplitude field (meters, declared distribution: covers
/// [LF2_HILL_RELIEF_MIN, MAX] with real spread — Rule 31 asserts quartiles,
/// not bounds).
[[nodiscard]] float forest_grive_amplitude(uint64_t seed, glm::vec2 world);

} // namespace dfn::world
