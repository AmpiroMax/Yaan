/*
Created: 10:08:2026 - 02:57:10
Last updated: 10:08:2026 - 02:57:10
Module: engine/render
File: engine/render/sources/CloudModel.h

Responsibility:
- The cloud drift model (WEATHER.md W4, user decisions в4/в10: clouds first,
  all three kinds). Turns render-side visual time plus the SHARED wind (W3)
  into the drift offset of the ONE cloud-coverage field. The field itself is
  sampled twice on the GPU — by the sky sheet and by the ground shadow — and
  both samplers read THIS offset, which is what makes two drifting copies
  structurally impossible (W4's named reject: a shadow crossing land while
  its cloud stands still).

Key items:
- apply_clouds(env, seconds): the single per-frame call (RenderSystem).
- cloud_drift_offset(direction, wind_mult, seconds): the pure math, exposed
  for tests and for gameplay-side questions like "is this spot in shadow".
- WIND_FIELD_DRIFT_SPEED_MPS / CLOUD_WAVELENGTH_M: the two wind-field NUMBERS
  rows (requested from the lead with derivations, 10:08:2026), consumed here
  first per the enter-when-consumed rule.

Dependencies:
- Uses: IRenderer.h (RenderEnvironment), glm.
- Used by: engine/render (RenderSystem, once per frame after apply_wind),
  tests.

Notes:
- The drift is a PURE FUNCTION of (wind direction, state multiplier, time) —
  no accumulated state — the same construction as the lunar phase and the
  W2.5 schedule ruling: any reported frame reproduces from its timestamp
  alone, and DFN_VISTIME can pin it for the acceptance pair.
- The offset is SUBTRACTIVE in the samplers (field sampled at p + offset with
  offset = -dir * speed * t), so the pattern travels DOWNWIND across the
  world: weather arrives from upwind (W2.3), passes overhead, and leaves.
- The weather STATE tuple (cloud_cover / cloud_cumulus / cloud_shadow /
  weather_wind_mult) is NOT written here: those are RenderEnvironment fields
  with "scattered" defaults, fed later by core's schedule via the app. This
  model only moves the field the state describes.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep this pure: no GPU, no ECS, no clock reads. Time arrives as a parameter.
*/
/*
UPD:
- 10:08:2026 - 02:57:10: Created for the cloud pass (W4: one field, two
  samplers; drift off the shared wind).
*/

#pragma once

#include "engine/core/config/sources/Constants.h"
#include "engine/platform/render/interfaces/IRenderer.h"

#include <glm/vec2.hpp>

namespace dfn::render {

/// Drift speed of the cloud-coverage field along the shared wind, meters per
/// second. READ FROM NUMBERS (WIND_FIELD_DRIFT_SPEED, landed with derivation
/// 10:08:2026): 10 m/s crosses the 640 m overview frame in ~64 s — weather
/// visibly CHANGING, W2.2 — and moves half a wavelength over the 30 s
/// acceptance pair; real cumulus drift is 8-12 m/s.
inline constexpr float WIND_FIELD_DRIFT_SPEED_MPS =
    static_cast<float>(config::WIND_FIELD_DRIFT_SPEED);

/// Feature size of the coverage field, meters. READ FROM NUMBERS
/// (WIND_FIELD_WAVELENGTH, landed with derivation 10:08:2026): 1-3 distinct
/// shadow patches inside the 85 m overview vantage's frame; sky cells that
/// read as clouds, not texture, at 640x360 (Rule 33).
inline constexpr float CLOUD_WAVELENGTH_M =
    static_cast<float>(config::WIND_FIELD_WAVELENGTH);

/// World-space offset the coverage-field samplers add to their sample point.
/// Negative along the wind so the PATTERN moves downwind (see header note).
/// Pure; `direction` need not be normalized (it is re-normalized here so a
/// stale or zero env value cannot scale the speed).
[[nodiscard]] glm::vec2 cloud_drift_offset(glm::vec2 direction, float wind_mult,
                                           float seconds);

/// Writes cloud_offset_m and cloud_wavelength_m into `env` from the wind
/// fields already present (call AFTER apply_wind). Every other field is
/// untouched — the state tuple (cover/cumulus/shadow/mult) stays whatever
/// the app or the defaults put there.
void apply_clouds(platform::RenderEnvironment& env, float seconds);

} // namespace dfn::render
