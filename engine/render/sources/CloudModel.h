/*
Created: 10:08:2026 - 02:57:10
Last updated: 12:08:2026 - 22:45:00
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
- 11:08:2026 - 14:43:13: cloud_field3 — the same field in 3D, for the horizon cumulus band
  (R3.1). Its own MEASURED mean/SD 0.5000/0.1185; the 2D pair fails the
  uniformity assertion here, which is the shipped control. Needed because the
  2D read made the band's silhouette a single-valued function of azimuth, so a
  hole in it was impossible by construction and the masses came out as
  mushroom caps.
- 12:08:2026 - 22:45:00: R3.3 — the field is renormalised onto the mean and spread that
  SURVIVE its own per-octave LOD (cloud_lod_residual / sd_lod / mean_lod),
  so `cover` means coverage at EVERY sampling rate and not only at full
  resolution, and the far-field convergence is keyed to how much field is
  left rather than to cells-per-pixel. cloud_field_fixed_sd is the form
  that shipped until now, kept as the control: measured at 0.50 cells/px
  it drew 0.0000 of the plane for a requested cover of 0.15 and 1.0000
  for 0.60. Rule 31's premise (uncorrelated octaves of equal marginal
  variance) was MEASURED before it was used — predicted SD tracks measured
  SD within 0.03 % from 0.0 to 0.80 cells/px.
*/

#pragma once

#include "engine/core/config/sources/Constants.h"
#include "engine/platform/render/interfaces/IRenderer.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

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

/// sqrt(W0^2 + W1^2 + W2^2) — the octave weights' quadratic norm at FULL
/// resolution, 0.640156. It is the denominator of the residual spread below:
/// after the LOD has replaced octaves by their mean the surviving weights are
/// w_i = W_i * l_i, and the sum's spread scales with sqrt(sum w_i^2).
inline constexpr float CLOUD_OCTAVE_W_NORM = 0.640156f;

/// THE OUTER CONVERGENCE WINDOW, and it is stated on the RESIDUAL SPREAD (the
/// fraction of the field's full SD that survives the per-octave LOD), not on
/// cells-per-pixel. That change of quantity IS the R3.3 fix.
///
/// What it replaced: `mix(a, cover, smoothstep(0.20, 0.60, cells_px))`, i.e.
/// the sheet was thrown away for its own area average once one pixel covered
/// 0.6 wavelengths — while at cells_px 0.60 the base octave's own LOD still
/// carries 21% of its amplitude, so 21% of a fully resolvable field was being
/// discarded. The two convergences were redundant and the outer one ran far
/// ahead of the inner one; the gap between them, projected into the frame, was
/// the hard bright band at the horizon (per-row SD of the cloud-only
/// difference collapsing 38..51 -> 8.2..13.0 at rows 144..152 while the mean
/// stayed the HIGHEST in the frame). See docs/specs/render.md, R3.3.
///
/// Driven by the residual instead, the convergence fires only where the field
/// is genuinely dead: res 0.18 lands at cells_px 0.59 and res 0.04 at 0.68,
/// against the old window's 0.20..0.60.
inline constexpr float CLOUD_LOD_RES_LIVE = 0.18f;
inline constexpr float CLOUD_LOD_RES_DEAD = 0.04f;

/// The RAW octave sum — Gaussian, NOT uniform. Shipped as the Rule 30 control
/// for the distribution tests: it must FAIL them. Not for drawing.
[[nodiscard]] float cloud_field_raw(glm::vec2 p_m, float wavelength_m);

/// The fraction of the field's full standard deviation that survives the
/// per-octave LOD at this sampling rate: sqrt(sum (W_i*l_i)^2) / 0.640156.
/// 1 at full resolution, 0 once every octave has been replaced by its mean.
///
/// RULE 31 NOTICE. This rests on the octaves being uncorrelated with equal
/// marginal variance — the same premise the shipped CLOUD_FIELD_MEAN/SD pair
/// rests on, and NOT assumed here: measured over 200k samples the predicted SD
/// tracks the measured SD of the LOD'd sum within 0.03% at every rate from 0.0
/// to 0.80 cells/px (ratio 0.9996..1.0003). CloudModelTests.cpp asserts it.
[[nodiscard]] float cloud_lod_residual(float cells_per_pixel);

/// THE field: uniform on [0,1] AT EVERY SAMPLING RATE by construction, so a
/// threshold at 1-cover covers exactly `cover` of the plane whether the field
/// is fully resolved or down to its last octave. `cells_per_pixel` is the
/// sampling rate at this point (world distance covered by one pixel, in
/// wavelengths, along the WORST screen axis); octaves that have gone sub-pixel
/// are replaced by their mean, and the SURVIVORS are then renormalised onto
/// their own mean and spread (mean_lod / sd_lod) so the loss of an octave costs
/// detail and nothing else. Pass 0 for an unsampled/analytic query.
///
/// The renormalisation is the R3.3 fix and it is not cosmetic: without it the
/// spread collapses with the octaves and a fixed threshold walks straight off
/// the distribution. MEASURED on the shipped form at cells_px 0.50 — cover 0.15
/// drew 0.0000 of the plane and cover 0.60 drew 1.0000, i.e. both ends of the
/// range inverted into the two constants a field can be. cloud_field_fixed_sd
/// is that form, kept as the control the tests reject.
[[nodiscard]] float cloud_field(glm::vec2 p_m, float wavelength_m,
                                float cells_per_pixel);

/// The SHIPPED-BEFORE-R3.3 field: same octave LOD, but remapped through the
/// FULL-RESOLUTION mean/SD at every rate. Rule 30 control — it must fail the
/// rate-dependent uniformity and coverage assertions. Not for drawing.
[[nodiscard]] float cloud_field_fixed_sd(glm::vec2 p_m, float wavelength_m,
                                         float cells_per_pixel);

/// Coverage opacity at a point. cover 0 returns exactly 0 (the pass's control
/// erases sheet and shadow together); cover 1 returns 1 everywhere. Where the
/// field is DEAD — every octave replaced by its mean, so there is no spread
/// left to threshold — the value converges to `cover` itself, the honest area
/// average. The convergence is keyed to CLOUD_LOD_RES_LIVE/DEAD, i.e. to how
/// much field is actually left, not to cells-per-pixel.
[[nodiscard]] float cloud_alpha(glm::vec2 p_m, float wavelength_m, float cover,
                                float cells_per_pixel);

// ===========================================================================
// THE SAME FIELD IN 3D — reference implementation for the horizon cumulus.
//
// MIRROR NOTICE. dfn_env.sh carries dfn_cloud_field3; this is its CPU side and
// exists for the same reason the 2D one does: so the DISTRIBUTION can be
// asserted rather than assumed (Rule 31).
//
// WHY A THIRD DIMENSION AT ALL. The cumulus band read the 2D field on a ring at
// fixed distance, making it a function of AZIMUTH ALONE against a height-rising
// threshold. For a fixed azimuth that makes opacity monotone in height, so the
// silhouette was a single-valued function of azimuth and no hole or overlap was
// POSSIBLE in it — provably, not incidentally. Inverting a squared threshold
// then gives height ~ sqrt(field): vertical where a lobe crosses the threshold,
// flat at its peak, i.e. a mushroom cap. Sampling in 3D is what removes the
// constraint; no exponent could have.
// ===========================================================================

/// Mean and standard deviation of the RAW 3-D octave sum, MEASURED over 400k
/// samples: 0.5000 / 0.1185. THEY ARE NOT THE 2-D PAIR (0.4980 / 0.1368), and
/// the difference is the whole reason they exist. The sum is Gaussian and the
/// remap is that Gaussian's own CDF, so feeding it the 2-D SD would re-run
/// Rule 31 exactly — `cover` would stop meaning coverage. A trilinear blend of
/// eight iid uniforms is simply tighter than a bilinear blend of four. The
/// control test asserts the 2-D pair FAILS here.
inline constexpr float CLOUD_FIELD3_MEAN = 0.5000f;
inline constexpr float CLOUD_FIELD3_SD = 0.1185f;

/// THE 3-D field: uniform on [0,1] by construction, same octave weights and
/// same CDF remap as cloud_field, so a threshold at 1-cover admits exactly
/// `cover` of space. `q` is in FIELD UNITS (already divided by wavelength and
/// by any scale), matching dfn_cloud_field3's parameter exactly.
[[nodiscard]] float cloud_field3(glm::vec3 q);

/// cloud_field3 with the mean/SD injected — the Rule 30 control. Passing the
/// 2-D pair must FAIL the uniformity assertion; that is the test's whole point.
[[nodiscard]] float cloud_field3_with(glm::vec3 q, float mean, float sd);

} // namespace dfn::render
