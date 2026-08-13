/*
Created: 13:08:2026 - 16:12:40
Last updated: 13:08:2026 - 16:12:40
Module: engine/world
File: engine/world/sources/WorldgenForms.h

Responsibility:
- LANDSCAPE §10.1.3's subject, approached as SHAPE rather than as amplitude:
  the ground's FORMS — benches and their risers (уступы и полки) — laid on the
  finished elevation so that the ground can hide ground behind it.

Key items:
- terrace_forms(): the bench/riser operator, a signed height delta.

Dependencies:
- Uses: WorldgenNoise.h, WorldgenMacro.h (stream ids), glm.
- Used by: Worldgen.cpp (compose_passes — the single application site, beside
  the general relief it reshapes).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic and position-based (Rule 13.1): a pure function of (seed,
  position, the height handed in). No chunk state, no neighbour queries.

- READ THIS BEFORE REACHING FOR AN AMPLITUDE. Three instruments in a row
  promised to explain "flat like Minecraft" and all three fell, the last one on
  a direct five-point sweep of GROUND_MESO_WAVELENGTH: pointwise slope moved
  monotonically exactly as predicted and ground-hiding-ground did not move at
  all (p5 = 0 at every wavelength, and the 60 m and 25 m frames are the same
  picture). The reason is arithmetic and it retires the whole family: shortening
  a wave raises its slope and shortens its run BY THE SAME FACTOR, and a pocket
  of hidden ground needs a drop that is DIRECTED and SUSTAINED over a run. No
  setting of a noise field produces one, because a noise field has no direction
  and no run — it has a spectrum.
- WHAT THIS PASS DOES INSTEAD, and why it is not a fourth octave: it is a
  MONOTONE TRANSFER OF ELEVATION, not a field added to it. It spends the
  amplitude the ground already has, re-distributing it into flats and steps: on
  a bench the existing gradient is multiplied by (1 - strength), on a riser by
  up to (1 - strength) + 1.5*strength/riser. The steps follow CONTOURS of the
  field they are applied to, so a riser runs as far as its contour does — the
  direction and the run come from the elevation field itself and not from a
  second noise lattice. |delta| never exceeds 0.33 * step * strength, so the
  §10.1.2 σ CEILING (1.20 m) is not the currency being spent here; slope is.
*/
/*
UPD:
- 13:08:2026 - 16:12:40: Created — the bench/riser operator (§10.1.3 F7).
*/

#pragma once

#include "engine/world/sources/TestbedLayout.h"

#include <cstdint>
#include <glm/vec2.hpp>

namespace dfn::world {

/// THE BENCH AND RISER OPERATOR (meters, signed) — add to `h_in`.
///
/// `h_in` is the ground as composed so far (carve + the §2.7 meso tier + §10.5
/// rock), WITHOUT the micro tier: the operator flattens what it is given by
/// (1 - strength) on the benches, and the micro octave is §2.7's "flat, not
/// sterile" clause, which must survive on top of a bench rather than be ironed
/// into it. Callers add micro AFTER this.
///
/// `mask` is the §2.7 relief mask (relief_mask()) — shore band, corridor and
/// massif, the same list §10.1.2 exempts from the bumpiness contract. Terraces
/// are ground relief and inherit exactly the same exemptions; at mask 0 the
/// delta is 0 and the ground is untouched.
///
/// SWEEP DOORS (measurement only, never a shipping path): DFN_TERRACE_STEP,
/// DFN_TERRACE_STRENGTH, DFN_TERRACE_RISER replace the middle of the three
/// drifting fields for one run. DFN_TERRACE_STRENGTH=0 is the pass's OWN named
/// control — the same code path with the operator at identity, not a second
/// path around it.
[[nodiscard]] float terrace_forms(uint64_t seed, glm::vec2 world, float h_in, float mask);

/// THE DRAWS (промоины) — a signed height delta, always <= 0: channels incised
/// along the land's own grain, applied BEFORE terrace_forms so the bench
/// operator sharpens their banks instead of competing with them.
///
/// The second form exists because the first one is a monotone transfer and
/// therefore cannot make relief where there is none — measured, not assumed:
/// with terraces alone the A1 frame's failing columns were one contiguous
/// sector, the bearings whose country has about 2 m of relief in 60 m.
///
/// `mask` is the same §2.7 relief mask terrace_forms takes.
/// DFN_DRAW_DEPTH scales the incision for one run; 0 is this pass's own named
/// control through the same code path.
[[nodiscard]] float draw_forms(uint64_t seed, glm::vec2 world, float mask);

} // namespace dfn::world
