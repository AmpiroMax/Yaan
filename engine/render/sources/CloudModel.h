/*
Created: 10:08:2026 - 02:57:10
Last updated: 10:08:2026 - 10:45:06
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
- cloud_field / cloud_alpha: THE coverage field, uniform on [0,1] by
  construction, mirrored on the GPU by dfn_env.sh. cloud_field_raw is the
  rejected pre-remap form the distribution tests use as their control.
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
- 10:08:2026 - 10:45:06: The coverage field's reference implementation and its
  constants. The shipped field was a Gaussian octave sum thresholded as if it
  were uniform, so `cover` did not mean coverage (cover 0.10 drew nothing);
  the remap makes it mean coverage, and cloud_field_raw stays as the control.
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

// ===========================================================================
// THE COVERAGE FIELD — reference implementation.
//
// MIRROR NOTICE. dfn_env.sh carries the same math for the GPU and names this
// file. Both are engine/render's, so the constants are one zone's look-dev
// values and do not yet belong in NUMBERS — but Rule 35's predictive form says
// exactly when they will: the moment core lands the gameplay query "is this
// spot in shadow", the field gains a second ZONE and every constant below
// moves to NUMBERS with a derivation. The WIND_FIELD_WAVELENGTH row already
// names that consumer as pending.
//
// This side exists so the field's DISTRIBUTION can be asserted (Rule 31). The
// shipped field could not be: it was a sum of three value-noise octaves, which
// is Gaussian, and it was thresholded as though it were uniform. Measured over
// 400k samples it occupied 0.045..0.945 with 98% of its mass inside
// 0.200..0.797 — 60% of the range it declared — so `cover` did not mean
// coverage: cover 0.10 drew NOTHING (0.0000 of the sky), cover 0.20 drew
// 0.0005, and the shipped default 0.45 drew 0.19. Light overcast was
// unreachable by construction and the sky read as empty. cloud_field_raw is
// kept below as that rejected case, and it is the control the distribution
// tests reject.
// ===========================================================================

/// Feature-size scale of the finest octave relative to the base, and the
/// weights they are summed with. Read by the tests and mirrored in dfn_env.sh.
inline constexpr float CLOUD_OCTAVE_W0 = 0.55f;
inline constexpr float CLOUD_OCTAVE_W1 = 0.28f;
inline constexpr float CLOUD_OCTAVE_W2 = 0.17f;
inline constexpr float CLOUD_OCTAVE_F1 = 2.03f;
inline constexpr float CLOUD_OCTAVE_F2 = 4.07f;

/// Mean and standard deviation of the RAW octave sum, MEASURED over 400k
/// samples across a 40 km square (not assumed): 0.4980 / 0.1368. They are the
/// parameters of the CDF the remap below inverts, so they are measurements
/// first and constants second — change an octave weight and they must be
/// re-measured, which the distribution test enforces.
inline constexpr float CLOUD_FIELD_MEAN = 0.4980f;
inline constexpr float CLOUD_FIELD_SD = 0.1368f;

/// Half-width of the coverage threshold's soft edge, in UNIFORM field units
/// (post-remap). Scale-free by construction: after the remap the field's units
/// are probability, so this is "the softest 10% of the distribution", the same
/// edge at every cover value.
inline constexpr float CLOUD_EDGE_U = 0.10f;

/// The RAW octave sum — Gaussian, NOT uniform. Shipped as the Rule 30 control
/// for the distribution tests: it must FAIL them. Not for drawing.
[[nodiscard]] float cloud_field_raw(glm::vec2 p_m, float wavelength_m);

/// THE field: uniform on [0,1] by construction, so a threshold at 1-cover
/// covers exactly `cover` of the plane. `cells_per_pixel` is the sampling rate
/// at this point (world distance covered by one pixel, in wavelengths, along
/// the WORST screen axis); octaves that have gone sub-pixel are replaced by
/// their mean rather than faded out, so detail is lost without the
/// distribution moving. Pass 0 for an unsampled/analytic query.
[[nodiscard]] float cloud_field(glm::vec2 p_m, float wavelength_m,
                                float cells_per_pixel);

/// Coverage opacity at a point. cover 0 returns exactly 0 (the pass's control
/// erases sheet and shadow together); cover 1 returns 1 everywhere. Where the
/// field is unresolvable the value converges to `cover` itself — the honest
/// area average — which is what turns the far sheet into a haze veil instead
/// of a speckle field, with no distance cut-off needed to hide aliasing.
[[nodiscard]] float cloud_alpha(glm::vec2 p_m, float wavelength_m, float cover,
                                float cells_per_pixel);

} // namespace dfn::render
