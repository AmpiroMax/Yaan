/*
Created: 09:08:2026 - 11:05:22
Last updated: 13:08:2026 - 17:28:00
Module: engine/world
File: engine/world/sources/WorldgenMacro.cpp

Responsibility:
- P1 macro heightfield implementation: base fBm (constants from dfn::config —
  the stage-2 local octave table is deleted per the stage-3 sync), valley
  redistribution, crag/knoll/bluff/lake stamps.

Key items:
- macro_height, crag_distance.

Dependencies:
- Uses: WorldgenMacro.h, WorldgenNoise.h, generated constants.
- Used by: dfn_world.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM (Rule 13.1): pure position-based function; the valley curve uses
  sqrt (IEEE correctly-rounded) for the 1.25 exponent — keep the std::pow
  fallback only for non-quarter exponents.
- The quantization range is WORLDGEN_MAX_HEIGHT shared by ALL chunks (offset
  0) — edge stitching depends on this function being position-based.
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — P1 macro v2; octave constants now consumed
  from dfn::config (WORLDGEN_OCTAVE*), local constexprs removed.
- 09:08:2026 - 13:28:27: P1 anisotropy retune (§2.1, gated on HILL_ANISOTROPY): mid octave input-stretched along a drifting per-valley axis field via bilinear blending of fixed-frame samples (position-varying rotation rejected — |world|*grad(theta) distortion; cross-axis rhythm pinned at the 128 m contract by construction).
- 09:08:2026 - 14:03:23: Micro-relief batch: path groove applied in macro_height before the river carve (channel clamp overrides in-water; constant along-path depth keeps CORRIDOR_SLOPE_MAX untouched; ~6 deg edge slopes stay under the blend threshold).
- 09:08:2026 - 19:13:01: crag_height honours ridge_amp_meters when set (absolute flank relief), falling back to the fractional form otherwise.
- 09:08:2026 - 21:37:57: LANDSCAPE §2.8 banded contour massif: crag_height -> massif_height. Per-bearing profile exponent p>1 (concave, the anti-dome fix), per-bearing radial lobing, non-uniform contour bands with a per-bearing cliff/ramp riser class. Bearing fields sample a CIRCLE in the lattice (periodic by construction, no branch cut at +-pi). Benches keep the profile compressed to a per-bearing angle under MASSIF_BENCH_SLOPE_MAX rather than being flattened; risers are planar cliff faces whose width is solved from the drawn angle; bench width narrows with elevation. Measured seed 1: I1 15.0deg (need 12), I3 16.5% surface over 55deg (need 12), I4 fullest bin 24.2% (max 30), I5 100% of radials (need 70), I6 CV 0.518 (need 0.35).
- 09:08:2026 - 21:37:57: §2.8 ANGULAR LOBES (I7/I8). ROOT CAUSE of the flat lobing found and it was a geometry error in my own helper: making a sampling circle's circumference span `lobes` cells forces radius = lobes*CELL/2pi, so at 3 aretes the circle is 61 m across INSIDE a 64 m cell — it fits in one cell, the noise reads as one smooth patch, and contour radius varied +-4% where the amplitude constants ask +-18-35%. A circle cannot be both small enough to carry few lobes and large enough to cross cells. Replaced by polygon_radius(): the cross-section is an irregular ROUNDED POLYGON via support function min_i d_i/cos(theta-alpha_i), whose boundary is FLAT FACETS meeting at corners — which is what an arete is — with re-entrant COULOIRS cut on the facet mid-bearings (a support function is convex-only, capped at n*tan(pi/n)/pi, and couloirs are exactly what convexity cannot express). Outline blends circle->polygon with elevation (§2.8.2 'eps increasing with elevation' = I8's rise clause), and couloirs FADE toward the summit because they are flank features that merge into the aretes — which also resolves the I7/I8 tug, since summit contours stay clean facets while flanks keep re-entrant perimeter. Periodic with no branch cut: theta enters only through cos(theta-alpha). Seed 1: I7 4 persistent aretes (need 3), I8 1.37/1.36/1.52 each >= 1.35 with rise 0.15. GROUND_MICRO_* implemented (previously unused constants) but SCOPED to the massif above the cliffline: applied globally it moved the shoreline and broke the §3.3 bed/mud cap (22.3 m vs 21.5 m), so the general 'земля слишком плоская' pass stays a separate job designed against hydrology/corridors/fords.
- 09:08:2026 - 21:37:57: §2.8.4 SUMMIT TOR (I2): the top SUMMIT_TOR_HEIGHT is a stack of tilted, laterally offset slabs over a SUMMIT_TOR_RADIUS footprint. HEIGHT-FUNCTION work, not the cancelled placed-mesh class — §2.8.4's own scale table puts >=3 m features in the terrain SDF, and these slabs are metres thick. Slab count DERIVED from the ~3 m Nyquist floor expressed as VOXEL_SIZE rather than borrowed from the cancelled ROCK_STACK_* constants. The cone is capped at the tor's base ONLY inside the stack, so the peak stays at the ruled L0_RELIEF (measured exactly 115.0) instead of inflating to 116.1. Two bugs found by measuring: deriving the base from _MIN while drawing slab height from _MIN..MAX overshot the peak, and returning a flat platform outside the slabs built a MESA — cost 3.6 deg of summit slope, which is the shape I2 exists to reject. I2 now 52.9 deg surface / 32.5 deg footprint against a 40.1 floor.
- 09:08:2026 - 21:37:57: §2.8.2 UNIT CHANGE (design's ruling): couloir depth is ABSOLUTE metres, not a fraction of local radius — a quantity held as a fraction of local radius is self-similar by construction, which is what I8's rise clause exists to detect. Scale is the CLIFF BAND height (a couloir incises the bands), not the massif radius: taking MASSIF_RADIAL_LOBE_AMP off the 180 m base gives 32-63 m insets, wider than the upper mountain, so the clamp binds everywhere and silently restores the old fraction behaviour (measured: levels 1.50/1.50/1.60 but rise 0.10 and I7 gone). Angular width stays RELATIVE. Result across 12 seeds: I8 rise now fails ZERO seeds (was the blocking clause), level fails 1.
- 09:08:2026 - 21:37:57: §2.8.7 STEEPNESS CASCADE. (1) L0_RELIEF is RELIEF ABOVE THE FOOT, not an absolute elevation — the code read it as absolute, so the peak sat at 115.0 over a 18.8 m valley floor and the user approved 115 m while looking at 96.2. Datum is now base_height at the crag centre; measured relief is exactly 115.0. (2) THE PROFILE DECAYED TO ZERO INSTEAD OF TO THE DATUM: h = H*(1-t)^p buried the whole concave tail under the base terrain's max(), leaving only the steep crossing where the cone cuts the valley floor visible — which is why the built envelope measured shallowest at the summit and steepest at the foot, the exact inverse of what p>1 exists to produce. The concave profile was in the formula and clipped out of the surface. Now datum + relief*(1-t)^p. (3) Summit tor footprint DERIVED from MASSIF_SUMMIT_RADIUS_FRAC of the base radius instead of drawn from SUMMIT_TOR_RADIUS_MIN/MAX: at 5-10 m on a 190 m massif, disabling the tor entirely gave an identical silhouette TO THE DECIMAL, so it certified through I2's surface weighting while being invisible to the camera. Measured after the cascade, 12 seeds: I1 (envelope basis) 30.3-52.0 deg, I2 64.8-74.3, I3 62.0-71.7%, I10 1.23-1.63 — all four now pass on EVERY seed. I4 and I8 regressed and are reported, not patched.
- 09:08:2026 - 21:48:23: SYSTEMIC FIX: bearing_field is a sum of INTEGER HARMONICS with seeded phases, not noise sampled on a circle. The circle construction was degenerate for the same reason the radial one was — rc = lobes*CELL/2pi puts the whole circle inside a couple of lattice cells. MEASURED: the field NEVER RETURNED A VALUE BELOW 0.4 and was lumpy above it (26% of samples at 0.6, 30% at 0.8) against a perfectly uniform raw lattice, so every per-bearing 'seeded spread' silently used only the top 60% of its declared range — the profile exponent never approached MASSIF_PROFILE_EXPONENT_MIN, cliff risers were never drawn near MASSIF_CLIFF_SLOPE_MIN (50-60 deg bin held 4% of surface), and the 0.5 cliff/ramp split did not split evenly. I had fixed this geometry once for the lobe field and left the broken helper feeding four other consumers: fixing a symptom is not fixing a mechanism. Riser angle additionally drawn uniform in sin(theta) so surface area spreads evenly in the measure I4 actually reads. Result across 12 seeds: I6 now passes EVERY seed (was failing), I1/I2/I3/I5/I10 robust; I4 and I8-rise still fail and are reported, not patched.
- 09:08:2026 - 22:04:20: §2.8.2 facet rulings 1+2 (per-facet parameters, couloirs as PLANAR FACET PAIRS via line-through-two-points rather than smoothed dents). Ruling 3 (crest sized to acceptance distance) MEASURED AND REVERTED: it moved I11 at 600 m from 1/1/0/0 to 2/1/2/0 against a floor of 3 -- failing either way -- while dropping I7 from a passing 3 to 1. Also removed a dead outer notch term that subtracted a FRACTION as if it were METRES (a ~1 m no-op, inert but one refactor from mattering).
- 10:08:2026 - 02:59:28: Stand selector (§8): macro_height branches whole to forest_stand_height when layout.stand == Forest; testbed path untouched (pinned-heightmap guard). ground_micro_relief exported — the §2.7 octave gains its second consumer (Rule 32: one implementation).
- 10:08:2026 - 20:20:20: breaks_massif_apron implemented against base_height +
  MASSIF_CLIFFLINE_FRAC; no new constant.
- 11:08:2026 - 15:15:55: aniso_value_noise extracted from aniso_mid_octave and exported as aniso_octave_sample: an isotropic octave laid over the ridgelets ERASES the grain rather than lying beside it (§2.1 anisotropy 3.61 -> 2.22 with HILL_ANISOTROPY untouched).
- 13:08:2026 - 16:35:00: aniso_value_noise takes the stretch as a parameter.
- 13:08:2026 - 17:28:00: aniso_value_noise takes the theta offset.
*/

#include "engine/world/sources/WorldgenMacro.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenForest.h"
#include "engine/world/sources/WorldgenNoise.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace dfn::world {

namespace {

using noise::ridged_noise;
using noise::smoothstep01;
using noise::value_noise;

constexpr float MAX_HEIGHT_M = static_cast<float>(config::WORLDGEN_MAX_HEIGHT);
constexpr float BASE_AMPLITUDE_M = static_cast<float>(
    config::WORLDGEN_OCTAVE1_AMP + config::WORLDGEN_OCTAVE2_AMP + config::WORLDGEN_OCTAVE3_AMP);
constexpr float LAKE_LEVEL_M = static_cast<float>(config::LAKE_LEVEL_TESTBED);
constexpr float LAKE_DEPTH_M = static_cast<float>(config::LAKE_DEPTH_MAX);

/// Valley redistribution (LANDSCAPE §2.1): n^WORLDGEN_VALLEY_EXPONENT on the
/// normalized base height. For the 1.25 exponent this is n * n^(1/4) via two
/// square roots — IEEE-exact, platform-deterministic (std::pow is not).
float valley_curve(float n) {
    constexpr double EXP = config::WORLDGEN_VALLEY_EXPONENT;
    if constexpr (EXP == 1.25) {
        return n * std::sqrt(std::sqrt(n));
    } else {
        return std::pow(n, static_cast<float>(EXP));
    }
}

/// Landform anisotropy (LANDSCAPE §2.1): the MID octave (the round-bump
/// layer) input-stretched by HILL_ANISOTROPY along a drifting per-valley
/// axis field. Implemented as bilinear blending of four FIXED-FRAME samples:
/// each axis-lattice cell (WORLDGEN_OCTAVE1_CELL — "per-valley" is the
/// octave-1 wavelength) carries one axis angle in [0, pi); within a frame the
/// rotation is constant, so the along-axis input is compressed exactly
/// 1/HILL_ANISOTROPY and the cross-axis input stays 1:1 — the ~128 m
/// cross-axis rhythm the corridors and the C1 grid feel is pinned by
/// construction (design contract; domain-warp rejected in §2.1, and a
/// POSITION-VARYING rotation is rejected here too: its |world|*grad(theta)
/// distortion term shreds the octave far from the origin). Frames with equal
/// angles produce identical fields (seamless); drifting angles crossfade.
/// EXPORTED (see WorldgenMacro.h) because §2.7's meso octave must share this
/// grain rather than run isotropically across it. Measured before it did:
/// seed-1 structure-tensor anisotropy over open meadow fell 3.61 -> 2.22
/// against a 2.5 floor when an isotropic 25-60 m octave was laid on top, with
/// the hill octave untouched. An isotropic layer does not merely fail to help
/// the grain, it ERASES it — which is §10.3.1's rule about azimuth sources
/// showing up in the terrain rather than in the props.
float aniso_value_noise(uint64_t seed, uint32_t stream, float cell, glm::vec2 world,
                        float stretch, float theta_offset) {
    const float axis_cell = static_cast<float>(config::WORLDGEN_OCTAVE1_CELL);
    const float cx = world.x / axis_cell;
    const float cz = world.y / axis_cell;
    const int64_t gx = static_cast<int64_t>(std::floor(cx));
    const int64_t gz = static_cast<int64_t>(std::floor(cz));
    const float tx = smoothstep01(cx - static_cast<float>(gx));
    const float tz = smoothstep01(cz - static_cast<float>(gz));

    float vals[2][2];
    for (int dz = 0; dz <= 1; ++dz) {
        for (int dx = 0; dx <= 1; ++dx) {
            const float theta =
                noise::lattice_value(seed, STREAM_HILL_AXIS, gx + dx, gz + dz)
                    * 3.14159265358979f
                + theta_offset;
            const glm::vec2 axis{std::cos(theta), std::sin(theta)};
            const glm::vec2 stretched{
                glm::dot(world, axis) / stretch,
                world.y * axis.x - world.x * axis.y}; // dot(world, across)
            vals[dz][dx] = value_noise(seed, stream, cell, stretched);
        }
    }
    const float v0 = vals[0][0] + (vals[0][1] - vals[0][0]) * tx;
    const float v1 = vals[1][0] + (vals[1][1] - vals[1][0]) * tx;
    return v0 + (v1 - v0) * tz;
}

/// The §2.1 ridgelet octave itself — unchanged, now spelled as one call.
float aniso_mid_octave(uint64_t seed, glm::vec2 world) {
    return aniso_value_noise(seed, STREAM_OCTAVE_BASE + 1,
                             static_cast<float>(config::WORLDGEN_OCTAVE2_CELL), world,
                             static_cast<float>(config::HILL_ANISOTROPY), 0.0f);
}

/// Base gentle-hills fBm in meters, then valley redistribution. Octaves 1
/// (macro rolls) and 3 (fine texture) are isotropic; octave 2 is the
/// anisotropic ridgelet layer above.
float base_height(uint64_t seed, glm::vec2 world) {
    float h = 0.0f;
    h += value_noise(seed, STREAM_OCTAVE_BASE + 0,
                     static_cast<float>(config::WORLDGEN_OCTAVE1_CELL), world)
         * static_cast<float>(config::WORLDGEN_OCTAVE1_AMP);
    h += aniso_mid_octave(seed, world) * static_cast<float>(config::WORLDGEN_OCTAVE2_AMP);
    h += value_noise(seed, STREAM_OCTAVE_BASE + 2,
                     static_cast<float>(config::WORLDGEN_OCTAVE3_CELL), world)
         * static_cast<float>(config::WORLDGEN_OCTAVE3_AMP);
    const float n = std::clamp(h / BASE_AMPLITUDE_M, 0.0f, 1.0f);
    return valley_curve(n) * BASE_AMPLITUDE_M;
}

/// A field of BEARING, sampled periodically. Sampling any noise on the angle
/// VALUE puts a branch cut at +-pi and draws a vertical seam from summit to
/// foot; sampling on a CIRCLE embedded in the noise field is periodic by
/// construction. `lobes` sets how many wiggles go round: the circle's
/// circumference spans that many noise cells.
/// `band` decorrelates successive contour bands by walking the sampling circle
/// to a distant part of the SAME lattice -- adding to the stream id instead
/// would collide with unrelated streams a few slots up.
/// SUM OF INTEGER HARMONICS with seeded phases, NOT noise sampled on a circle.
///
/// The circle construction was degenerate for the same reason it was degenerate
/// for the radial field: rc = lobes*CELL/2pi puts the whole circle inside a
/// couple of lattice cells, so it only ever reaches a handful of underlying
/// lattice values. MEASURED on the shipped build: this field NEVER RETURNED A
/// VALUE BELOW 0.4, and was lumpy above it (26% of samples at 0.6, 30% at 0.8)
/// against a perfectly uniform raw lattice. Every "seeded spread" drawing from
/// it silently used only the top 60% of its declared range -- the profile
/// exponent never approached MASSIF_PROFILE_EXPONENT_MIN, cliff risers were
/// never drawn near MASSIF_CLIFF_SLOPE_MIN (the 50-60 deg bin held 4% of the
/// surface), and the 0.5 cliff/ramp split did not split anywhere near evenly.
///
/// I fixed this geometry once for the lobe field by replacing it with the
/// support polygon, and left the broken helper feeding four other consumers.
/// Fixing a symptom is not fixing a mechanism.
///
/// Integer harmonics are periodic by construction (cos(m*theta) is 2pi-periodic
/// for integer m, so the +-pi branch cut in atan2 is invisible), give exact
/// control of the angular frequency, and draw their phases and amplitudes from
/// the RAW lattice, which is uniform.
float bearing_field(uint64_t seed, uint32_t stream, glm::vec2 unit_dir, float lobes, int band) {
    constexpr float TAU = 6.28318530717958647692f;
    const float theta = std::atan2(unit_dir.y, unit_dir.x);
    const int harmonics = std::clamp(static_cast<int>(lobes), 2, 6);
    float sum = 0.0f;
    float norm = 0.0f;
    for (int m = 1; m <= harmonics; ++m) {
        const int64_t key = static_cast<int64_t>(band) * 64 + m;
        const float phase = noise::lattice_value(seed, stream, key, 0) * TAU;
        const float amp = 0.5f + noise::lattice_value(seed, stream, key, 1);
        sum += amp * std::cos(static_cast<float>(m) * theta + phase);
        norm += amp;
    }
    return std::clamp(0.5f + 0.5f * sum / std::max(norm, 1e-3f), 0.0f, 1.0f);
}

/// The massif's horizontal cross-section as an irregular ROUNDED POLYGON,
/// evaluated by support function: r(theta) = min_i d_i / cos(theta - alpha_i).
/// Its boundary is FLAT FACETS meeting at sharp corners, which is what an
/// arete is (§2.8: "ребро — это ПЛОСКИЕ ГРАНИ, сходящиеся по линии"), and the
/// facet midpoints are the re-entrant couloirs between them.
///
/// This replaces sampling ridged noise on a circle in the lattice, which was
/// geometrically incapable of the job: making the circumference span `lobes`
/// cells forces radius = lobes*CELL/2pi, so at 3 aretes the sampling circle is
/// 61 m across inside a 64 m cell -- it fits INSIDE one cell, the noise reads
/// as a single smooth patch, and the contour radius varied +-4% where the
/// amplitude constants ask for +-18-35%. A circle cannot be both small enough
/// to carry few lobes and large enough to cross cells.
///
/// Periodic with no branch cut: theta enters only through cos(theta - alpha),
/// which is 2pi-periodic, so the +-pi jump in atan2 is invisible here.
float polygon_radius(uint64_t seed, const CragStamp& crag, float theta, float& notch_out) {
    constexpr float TAU = 6.28318530717958647692f;
    const int n = std::max(3, crag.arete_count);
    const float amp_lo = static_cast<float>(config::MASSIF_RADIAL_LOBE_AMP_MIN);
    const float amp_hi = static_cast<float>(config::MASSIF_RADIAL_LOBE_AMP_MAX);

    // Facet parameters are drawn ONCE PER FACET and never modulated across it.
    // A continuous per-bearing term applied to R(theta) bends a flat face into
    // an arc, which is why a support-function construction -- polygonal BY
    // DEFINITION -- was producing curved facets, and why I7's flatness test
    // never passed.
    const auto facet = [&](int i, float& alpha, float& d) {
        const float jitter =
            noise::lattice_value(seed, STREAM_MASSIF_LOBE, static_cast<int64_t>(i), 0) - 0.5f;
        alpha = TAU * (static_cast<float>(i) + jitter) / static_cast<float>(n);
        const float amp =
            amp_lo
            + noise::lattice_value(seed, STREAM_MASSIF_LOBE, static_cast<int64_t>(i), 1)
                  * (amp_hi - amp_lo);
        d = crag.radius * (1.0f - amp);
    };

    // Convex hull: r(theta) = min_i d_i / cos(theta - alpha_i).
    const auto convex_at = [&](float th) {
        float best = crag.radius * 3.0f;
        for (int i = 0; i < n; ++i) {
            float alpha = 0.0f, d = 0.0f;
            facet(i, alpha, d);
            const float c = std::cos(th - alpha);
            if (c > 0.05f) {
                best = std::min(best, d / c);
            }
        }
        return best;
    };
    float best = convex_at(theta);

    // A COULOIR IS A PAIR OF FACETS, NOT A DENT IN ONE. Two planar walls
    // meeting at an apex preserve flatness and ADD corners; a smooth
    // re-entrant SUBTRACTS them by curving the face it sits in. That is also
    // why deepening the old dent dropped persistent aretes from 4 to 0 while
    // raising I8 -- as facet pairs the two invariants stop trading.
    //
    // Radius of the straight line through two polar points, along bearing th:
    // the line is N.X = N.P1, so r = (N.P1) / (N.u(th)).
    const auto line_radius = [](float th, float th1, float r1, float th2, float r2) {
        const glm::vec2 p1{r1 * std::cos(th1), r1 * std::sin(th1)};
        const glm::vec2 p2{r2 * std::cos(th2), r2 * std::sin(th2)};
        const glm::vec2 dvec = p2 - p1;
        const glm::vec2 nvec{-dvec.y, dvec.x};
        const glm::vec2 u{std::cos(th), std::sin(th)};
        const float den = glm::dot(nvec, u);
        if (std::fabs(den) < 1e-4f) {
            return 1e9f;
        }
        const float r = glm::dot(nvec, p1) / den;
        return r > 0.0f ? r : 1e9f;
    };

    const float half_w = TAU / static_cast<float>(n) * 0.35f;
    float cut = 0.0f;
    for (int i = 0; i < n; ++i) {
        float alpha = 0.0f, d = 0.0f;
        facet(i, alpha, d);
        float dth = theta - alpha;
        while (dth > TAU * 0.5f) { dth -= TAU; }
        while (dth < -TAU * 0.5f) { dth += TAU; }
        if (std::fabs(dth) >= half_w) {
            continue;
        }
        // Depth is ABSOLUTE metres at the cliff-band scale (a couloir incises
        // the bands); the V's apex sits that far inside the convex outline.
        // CREST STRUCTURE IS SIZED AGAINST THE ACCEPTANCE DISTANCE, not against
        // the feature it cuts. The cliff-band scale (8-15 m) is below the
        // readable size at 600 m (~20 m), so band-deep couloirs vanish exactly
        // where the massif has to read -- I11 measured 5/8/12/4 breaks at 300 m
        // collapsing to 1/1/0/0 at 600 m. This is the SUMMIT TOR LESSON a
        // second time: detail sized against the object shrinks out of
        // legibility as the object recedes.
        //
        // MASSIF_RADIAL_LOBE_AMP is the right unit because it is a fraction of
        // the MASSIF, so the couloir keeps a constant angular size whatever
        // the massif's own angular size is. At L0_BASE_RADIUS 120 it gives
        // 22-42 m, which clears the readable floor out to ~700 m.
        const float amp_d =
            static_cast<float>(config::MASSIF_RADIAL_LOBE_AMP_MIN)
            + noise::lattice_value(seed, STREAM_MASSIF_LOBE, static_cast<int64_t>(i), 2)
                  * static_cast<float>(config::MASSIF_RADIAL_LOBE_AMP_MAX
                                       - config::MASSIF_RADIAL_LOBE_AMP_MIN);
        const float depth =
            static_cast<float>(config::MASSIF_CLIFF_BAND_MIN)
            + noise::lattice_value(seed, STREAM_MASSIF_LOBE, static_cast<int64_t>(i), 2)
                  * static_cast<float>(config::MASSIF_CLIFF_BAND_MAX
                                       - config::MASSIF_CLIFF_BAND_MIN);
        const float r_lo = convex_at(alpha - half_w);
        const float r_hi = convex_at(alpha + half_w);
        const float r_apex = std::max(convex_at(alpha) - depth, 1.0f);
        const float wall = dth <= 0.0f ? line_radius(theta, alpha - half_w, r_lo, alpha, r_apex)
                                       : line_radius(theta, alpha, r_apex, alpha + half_w, r_hi);
        if (wall < best) {
            cut = std::max(cut, (best - wall) / std::max(best, 1.0f));
            best = wall;
        }
    }
    notch_out = cut; // reported for diagnostics only; the cut is already applied
    return best;
}

/// Ridged version of the same: sharp crests are aretes, the troughs between
/// them are couloirs.
float bearing_ridged(uint64_t seed, uint32_t stream, glm::vec2 unit_dir, float lobes, int band) {
    return 1.0f - std::fabs(2.0f * bearing_field(seed, stream, unit_dir, lobes, band) - 1.0f);
}

/// Banded contour massif (LANDSCAPE §2.8). Four seeded per-sample fields:
///   1. a per-bearing PROFILE EXPONENT p in [1.3, 2.2] applied as
///      h = H*(1 - d/R)^p. p > 1 is CONCAVE — steep at the summit, shallowing
///      to the foot, which is what real mountains do. smoothstep is the
///      opposite curve, and that single inversion is why the old stamp read as
///      a dome no matter how it was tuned.
///   2. a per-bearing RADIAL EXTENT R(theta) with lobe amplitude rising with
///      elevation: outward lobes are aretes, inward folds are couloirs.
///   3. non-uniform CONTOUR BANDS above the cliffline.
///   4. a RISER CLASS per (band, angular sector): CLIFF or RAMP, so the
///      terracing is discontinuous AROUND the mountain as well as up it —
///      otherwise it reads as a wedding cake.
/// Fine ground relief, EVERYWHERE (the user's "земля слишком плоская"). Two
/// octaves at the ruled wavelengths, amplitude drifting between the ruled
/// bounds so the roughness itself is not constant. At 0.3-0.6 m over 8-16 m
/// this is ~5 deg: no threat to a step or a corridor grade.
///
/// It also carries §2.7 onto the massif's BENCHES (design's ruling: a bench is
/// GROUND, and "terrain never flattens" is general, not a forest rule), and it
/// restores the contour crenulation that the authored outline displaced --
/// structural lobing has to EXCEED what the old fBm bleed gave for free, not
/// merely replace it.
// (body moved to the exported ground_micro_relief below; this alias keeps the
// massif code reading as before)
float ground_micro(uint64_t seed, glm::vec2 world) {
    return ground_micro_relief(seed, world);
}

/// §2.8.4 THE SUMMIT TOR: the top SUMMIT_TOR_HEIGHT of the massif is a stack
/// of tilted slabs over a SUMMIT_TOR_RADIUS footprint, not a terrain vertex.
/// A granite tor is a real landform, it is literally «кубы на кубах», and it
/// converts the one part of the mountain the eye always lands on from a
/// rounded crown into a broken rock crest.
///
/// This is HEIGHT-FUNCTION work, not the cancelled placed-mesh class. §2.8.4's
/// own scale table puts >= 8 m features in the terrain SDF for free and 3-8 m
/// features there with ~1 m lip rounding accepted; only blocks under 3 m need
/// placed meshes, because they cannot survive 1 m voxels at all. Slabs here
/// are metres thick over a 5-10 m footprint, so they sit in the SDF band.
///
/// Slab count is DERIVED, not borrowed: the rock-stack constants belong to the
/// cancelled asset class, so the count comes from the thinnest slab that can
/// survive extraction (§2.8.4's ~3 m Nyquist floor, expressed as VOXEL_SIZE).
float summit_tor_height(uint64_t seed) {
    return static_cast<float>(config::SUMMIT_TOR_HEIGHT_MIN)
         + noise::lattice_value(seed, STREAM_MASSIF_TOR, 0, 0)
               * static_cast<float>(config::SUMMIT_TOR_HEIGHT_MAX
                                    - config::SUMMIT_TOR_HEIGHT_MIN);
}

/// Returns -1e9 when the point lies outside every slab, so the caller can keep
/// the cone there instead of flattening it.
float summit_tor(uint64_t seed, const CragStamp& crag, glm::vec2 world, float tor_base) {
    constexpr float TAU = 6.28318530717958647692f;
    const float height = summit_tor_height(seed);
    const float min_slab = 3.0f * static_cast<float>(config::VOXEL_SIZE);
    const int slabs = std::clamp(static_cast<int>(height / min_slab), 2, 5);
    const glm::vec2 rel = world - crag.center;

    float top = -1e9f;
    float z = tor_base;
    for (int i = 0; i < slabs; ++i) {
        // Each slab: its own radius, its own lateral offset, its own tilt.
        // Radius shrinks upward so the stack reads as a stack rather than a
        // cylinder, but the draw keeps it inside the ruled footprint.
        const float shrink = 1.0f - 0.5f * static_cast<float>(i) / static_cast<float>(slabs);
        // Footprint is DERIVED from the acceptance distance, not drawn from
        // SUMMIT_TOR_RADIUS_MIN/MAX. Measured: at 5-10 m on a 190 m massif the
        // tor changed the outline by NOTHING -- disabling it entirely gave an
        // identical silhouette to the decimal. A 5 m ornament is below
        // SILHOUETTE_MIN_PX at every acceptance distance, so it certified
        // through I2's surface weighting while being invisible to the camera.
        // MASSIF_SUMMIT_RADIUS_FRAC of the base radius is the same fraction I2
        // already calls "the summit", which keeps the feature and its test on
        // one definition. The drawn range now varies the stack ABOUT that size
        // rather than setting it.
        const float tor_r = std::max(
            static_cast<float>(config::MASSIF_SUMMIT_RADIUS_FRAC) * crag.radius,
            static_cast<float>(config::SUMMIT_TOR_RADIUS_MIN));
        const float vary = static_cast<float>(config::SUMMIT_TOR_RADIUS_MIN)
                         / static_cast<float>(config::SUMMIT_TOR_RADIUS_MAX);
        const float r = tor_r
                        * (vary + (1.0f - vary) * noise::lattice_value(
                                      seed, STREAM_MASSIF_TOR, i, 1))
                        * shrink;
        // Lateral offset: a tor leans, and slabs sit off-axis. Bounded by the
        // slab radius so the stack cannot walk off its own footprint.
        const float off_a = noise::lattice_value(seed, STREAM_MASSIF_TOR, i, 2) * TAU;
        const float off_d = noise::lattice_value(seed, STREAM_MASSIF_TOR, i, 3) * r * 0.35f;
        const glm::vec2 centre{std::cos(off_a) * off_d, std::sin(off_a) * off_d};
        const glm::vec2 local = rel - centre;
        const float dist = glm::length(local);
        if (dist > r) {
            z += height / static_cast<float>(slabs);
            continue;
        }
        // Tilt: the slab's top is a plane, not a dome. THIS is what makes the
        // silhouette a broken crest instead of an arc.
        const float tilt_a = noise::lattice_value(seed, STREAM_MASSIF_TOR, i, 4) * TAU;
        const glm::vec2 tilt_dir{std::cos(tilt_a), std::sin(tilt_a)};
        const float tilt = (noise::lattice_value(seed, STREAM_MASSIF_TOR, i, 5) - 0.5f) * 0.5f;
        const float thickness = height / static_cast<float>(slabs);
        top = std::max(top, z + thickness + glm::dot(local, tilt_dir) * tilt);
        z += thickness;
    }
    return top;
}

/// Everything here is a pure function of position; nothing touches the voxel
/// pipeline.
float massif_height(uint64_t seed, const CragStamp& crag, glm::vec2 world) {
    const glm::vec2 rel = world - crag.center;
    const float d = glm::length(rel);
    if (d >= crag.radius * (1.0f + static_cast<float>(config::MASSIF_RADIAL_LOBE_AMP_MAX))) {
        return 0.0f;
    }
    const glm::vec2 dir = d > 1e-3f ? rel / d : glm::vec2{1.0f, 0.0f};

    // L0_RELIEF IS RELIEF ABOVE THE FOOT, NOT AN ABSOLUTE ELEVATION. Reading it
    // as an absolute peak put the summit at 115.0 with the valley floor at
    // 18.8, i.e. 96.2 m of actual relief -- the user approved 115 and was
    // looking at 96. The datum is the terrain that WOULD be here without the
    // massif, so the stamp delivers its ruled relief wherever it is placed.
    const float datum = base_height(seed, crag.center);
    const float relief = crag.peak_height;
    const float H = datum + relief; // absolute summit elevation

    // --- Field 1: per-bearing profile exponent ---------------------------------
    const float lobes = static_cast<float>(crag.arete_count);
    const float p = static_cast<float>(config::MASSIF_PROFILE_EXPONENT_MIN)
                  + bearing_field(seed, STREAM_MASSIF_PROFILE, dir, lobes, 0)
                        * static_cast<float>(config::MASSIF_PROFILE_EXPONENT_MAX
                                             - config::MASSIF_PROFILE_EXPONENT_MIN);

    // --- Field 2: per-bearing radial extent, lobes growing with elevation ------
    // Elevation is what we are solving for, so take one cheap pass at the mean
    // amplitude, then re-solve with the amplitude that height implies.
    const float amp_lo = static_cast<float>(config::MASSIF_RADIAL_LOBE_AMP_MIN);
    const float amp_hi = static_cast<float>(config::MASSIF_RADIAL_LOBE_AMP_MAX);
    // The outline BECOMES the polygon as it rises: round talus at the foot,
    // sharp faceted aretes at the summit. This is §2.8.2's "eps increasing
    // with elevation", and it is also exactly what I8 asks for -- lobing that
    // RISES with height rather than a self-similar cone.
    const float theta = std::atan2(dir.y, dir.x);
    float notch = 0.0f;
    const float r_poly = polygon_radius(seed, crag, theta, notch);
    float R_solved = 0.0f;
    const auto solve = [&](float k_raw) {
        const float k = std::clamp(k_raw, 0.0f, 1.0f);
        // The outline blends circle -> faceted polygon with elevation (§2.8.2's
        // "eps increasing with elevation"). The COULOIRS ARE ALREADY IN
        // r_poly, cut as planar facet pairs rather than smoothed dents, so
        // there is no separate notch term here -- the previous one subtracted
        // a FRACTION as if it were METRES and was a ~1 m no-op hiding a unit
        // error.
        R_solved = crag.radius + (r_poly - crag.radius) * k;
        const float t = std::clamp(d / std::max(R_solved, 1.0f), 0.0f, 1.0f);
        // Decays to the DATUM, not to zero. Decaying to zero buries the
        // profile's entire concave tail under the base terrain's max(), and
        // what stays visible is the steep crossing where the cone cuts through
        // the valley floor -- which is why the built envelope measured
        // shallowest at the summit and steepest at the foot, the exact inverse
        // of what p>1 exists to produce. The concave profile was in the
        // formula and clipped out of the surface.
        return datum + relief * std::pow(1.0f - t, p);
    };
    // Elevation is what we are solving for, so take one pass at the midpoint
    // and re-solve with the blend that height implies.
    float h = solve(0.5f);
    h = solve((h - datum) / relief);
    if (h <= 0.0f) {
        return 0.0f;
    }

    // --- Fields 3 & 4: contour bands with per-sector riser class ---------------
    const float cliffline =
        datum + relief * static_cast<float>(config::MASSIF_CLIFFLINE_FRAC);
    if (h <= cliffline) {
        return h; // lower slopes stay smooth: the bands are a summit feature
    }
    // THE SUMMIT TOR (§2.8.4). The cone is truncated at the tor's base and the
    // slabs rise from there, so the tor REPLACES the top SUMMIT_TOR_HEIGHT
    // rather than being piled on top of it -- the peak stays at the ruled
    // L0_RELIEF and nothing downstream (R3, C1, the skyline budget) shifts.
    const float tor_base = H - summit_tor_height(seed);
    const float tor = summit_tor(seed, crag, world, tor_base);
    if (tor > -1e8f) {
        // Inside the stack: the cone is CAPPED at the tor's base and the slabs
        // stand on it, so the tor replaces the top rather than piling onto it
        // and the peak stays at the ruled L0_RELIEF.
        return std::max(std::min(h, tor_base), tor);
    }
    // Outside every slab the cone is untouched. Returning a flat platform here
    // instead cost 3.6 deg of summit slope -- a truncated cone is a MESA, and
    // I2 exists to reject exactly that.
    if (h > tor_base) {
        return h;
    }

    // §2.7 on the benches (design's ruling: a bench is GROUND, and "terrain
    // never flattens" is general, not a forest rule).
    //
    // Deliberately scoped to the massif ABOVE the cliffline rather than the
    // whole world. Applied globally it perturbs the shoreline: 0.3-0.6 m dips
    // near the bank fall under the water surface and read as WaterBed past the
    // §3.3 bed/mud cap (measured 22.3 m against a 21.5 m cap). The general
    // "земля слишком плоская" pass is a separate job that has to be designed
    // against hydrology, corridors and fords; it does not belong inside the
    // massif step, and shipping it here would have traded a water invariant
    // for a cosmetic win.
    h += ground_micro(seed, world);
    const float band_min = static_cast<float>(config::MASSIF_CLIFF_BAND_MIN);
    const float band_max = static_cast<float>(config::MASSIF_CLIFF_BAND_MAX);

    // Local radial gradient of the smooth profile, which converts the band's
    // VERTICAL span into the horizontal width the player walks. §2.8.2 sizes
    // benches horizontally (MASSIF_BENCH_WIDTH_*), so the vertical split has
    // to be derived from this, not picked.
    const float t_h = std::clamp(d / std::max(R_solved, 1.0f), 0.0f, 1.0f);
    const float grad = std::max(relief * p * std::pow(1.0f - t_h, std::max(p - 1.0f, 0.0f))
                                    / std::max(R_solved, 1.0f),
                                1e-3f);

    float lo = cliffline;
    // Enough iterations to reach the summit at the thinnest legal band, +1 for
    // the partial band at the top. Derived, so it cannot silently truncate.
    const int band_limit = static_cast<int>((H - cliffline) / std::max(band_min, 1e-3f)) + 2;
    for (int k = 0; k < band_limit; ++k) {
        const float span = band_min
                         + noise::lattice_value(seed, STREAM_MASSIF_BAND,
                                                static_cast<int64_t>(k), 0)
                               * (band_max - band_min);
        const float hi = lo + span;
        if (h > hi) {
            lo = hi;
            continue;
        }
        // Riser class for THIS band at THIS bearing. Periodic in theta by
        // construction (bearing_field samples the unit circle), so there is no
        // sector count to pick and no branch cut at +-pi. The band index rides
        // in as a separate stream offset, which is what lets one band be a
        // cliff on the north face and a ramp on the south.
        //
        // The 0.5 split is not taste: I5 wants >= 3 cliff/bench ALTERNATIONS
        // per radial, and alternation probability p(1-p) is maximised at 0.5.
        const bool cliff = bearing_field(seed, STREAM_MASSIF_RISER, dir, lobes, k) < 0.5f;
        // Cliff: a flat bench holding the walkable part of the band, then a
        // steep riser. Constant OUTPUT over a range of input height is what
        // makes ground flat; a fast rise over a short range is what makes it
        // vertical. The bench takes MASSIF_BENCH_WIDTH_* metres HORIZONTALLY,
        // converted through the local gradient, and the riser gets the rest.
        const float band_width = span / grad; // horizontal metres of this band

        // A bench is NOT dead flat. MASSIF_BENCH_SLOPE_MAX is a ceiling ("you
        // can run a road along it"), not an instruction to zero the gradient.
        // Flattening benches outright puts most of the mountain in one slope
        // bin, which is precisely what I4 forbids and what the user called a
        // wedding cake. So the bench keeps the concave profile, COMPRESSED
        // only as far as the ceiling requires.
        // ...and the bench slope VARIES per band. Pinning every bench at the
        // ceiling is just a different constant gradient -- measured 75% of the
        // surface in the 20-30 deg bin, failing I4 exactly as hard as flat
        // benches failed it. The ceiling bounds the draw; it is not the draw.
        // Drawn per BEARING as well as per band. Per-band alone gives one
        // bench angle for the whole ring -- about eight discrete values over
        // the massif, which quantises the slope histogram into a few tall
        // spikes and fails I4 for a reason that has nothing to do with shape.
        const float tan_bench = std::tan(
            static_cast<float>(config::MASSIF_BENCH_SLOPE_MAX)
            * bearing_field(seed, STREAM_MASSIF_BAND, dir, lobes, k + 64));
        const float squash = std::min(1.0f, tan_bench / grad);

        // The riser is a CLIFF FACE, so it is planar: its angle is drawn
        // between MASSIF_CLIFF_SLOPE_MIN and vertical, and its width is then
        // SOLVED for rather than clamped. Smoothstep here was a mistake worth
        // recording -- easing the ends spends the riser's width on sub-cliff
        // slope, so a 68 deg riser only cleared 55 deg over 60% of itself and
        // I3 measured 7.1% where the reserved width said 12%. A hard lip is
        // also what §2.8.5 already promised render a splat exception for.
        // A RAMP band is still a terrace -- it just has a walkable riser
        // instead of a cliff. Letting a ramp return the bare cone (as this did
        // first) leaves half the massif unbanded, and that raw cone slope is a
        // narrow range: it piled 50% of the surface into the 30-40 deg bin and
        // failed I4 on its own.
        constexpr float HALF_PI = 1.57079632679489661923f;
        const float cliff_min = static_cast<float>(config::MASSIF_CLIFF_SLOPE_MIN);
        const float bench_max = static_cast<float>(config::MASSIF_BENCH_SLOPE_MAX);
        const float riser_lo = cliff ? cliff_min : bench_max;
        const float riser_hi = cliff ? HALF_PI : cliff_min;
        // The angle is drawn so the SURFACE AREA spreads evenly, not the angle.
        // Drawing theta uniformly looks like variety but is not, because I4
        // measures surface and surface per unit footprint goes as 1/cos(theta):
        // a uniform angle draw piles surface into the middle of the steep range
        // (measured 26% in the 60-70 bin with 50-60 at only 4%). Sampling
        // sin(theta) uniformly gives a density proportional to cos(theta),
        // which cancels the 1/cos weighting exactly and flattens the histogram
        // in the units the invariant actually reads. No new constant -- it is
        // the same range, sampled in the measure I4 uses.
        const float u_r = bearing_field(seed, STREAM_MASSIF_BAND, dir, lobes, k + 128);
        const float s_lo = std::sin(riser_lo);
        const float s_hi = std::sin(riser_hi);
        const float tan_riser =
            std::tan(std::asin(std::clamp(s_lo + u_r * (s_hi - s_lo), -1.0f, 1.0f)));
        // Width shares that make the bench climb squash*grad and the riser
        // climb the remainder at exactly tan_riser.
        float rf = grad * (1.0f - squash) / std::max(tan_riser - grad * squash, 1e-3f);
        // Nothing thinner than a voxel survives extraction, so a riser narrower
        // than one cell is not a cliff, it is an aliasing artefact.
        // The bench that is left over must still be a bench you can walk:
        // MASSIF_BENCH_WIDTH_MIN..MAX bounds it, which in turn bounds rf. This
        // is where that constant earns its place -- without it the bench width
        // is whatever the slope solve happens to leave.
        // Benches NARROW with height: the widest terraces belong to the talus
        // shoulders, the summit gets ledges. This needs no new constant (it
        // just slides the draw from _MAX down to _MIN across the relief) and
        // it is what makes the upper third genuinely steeper than the lower,
        // which is I1 -- with a height-independent bench the difference
        // measured 8.6 deg against a 12 deg floor.
        const float bench_cap = static_cast<float>(config::MASSIF_BENCH_WIDTH_MIN)
                              + static_cast<float>(config::MASSIF_BENCH_WIDTH_MAX
                                                   - config::MASSIF_BENCH_WIDTH_MIN)
                                    * (1.0f - std::clamp((h - datum) / relief, 0.0f, 1.0f));
        const float rf_min = std::max(
            static_cast<float>(config::VOXEL_SIZE) / std::max(band_width, 1.0f),
            1.0f - bench_cap / std::max(band_width, 1.0f));
        const float rf_max =
            1.0f - static_cast<float>(config::MASSIF_BENCH_WIDTH_MIN) / std::max(band_width, 1.0f);
        rf = std::clamp(rf, std::min(rf_min, 1.0f), std::clamp(rf_max, rf_min, 1.0f));

        // u is the NATURAL height fraction through the band, and because the
        // smooth profile is locally linear in distance, u also tracks
        // horizontal position -- so the bench/riser split sits at the WIDTH
        // share (1 - rf), while bench_frac is what the bench CLIMBS.
        const float bench_frac = squash * (1.0f - rf);
        const float u = (h - lo) / span;
        if (u <= 1.0f - rf) {
            return lo + (h - lo) * squash;
        }
        const float bench_top = lo + span * bench_frac;
        return bench_top + (span - span * bench_frac) * ((u - (1.0f - rf)) / rf);
    }
    return h;
}

/// Drainage valley (layout data): clamps terrain DOWN to a monotone floor
/// profile inside the valley and UP to a shoulder crest just outside — the
/// deterministic watershed the §3.1 descent trace follows. Applied to the
/// base BEFORE the crag max() so the crag flank stays intact and the valley
/// takes over at its foot.
float trough_shape(const ValleyTrough& trough, float h, glm::vec2 world) {
    if (trough.point_count < 2) {
        return h;
    }
    const float shoulder_band = trough.half_width * trough.shoulder_frac;
    const float reach = trough.half_width + shoulder_band;
    // Distance to the polyline and the normalized along-path position of the
    // closest point (for the floor lerp).
    float best_d = 1e9f;
    float best_t = 0.0f;
    float cum = 0.0f;
    float total = 0.0f;
    for (int i = 0; i + 1 < trough.point_count; ++i) {
        total += glm::length(trough.points[i + 1] - trough.points[i]);
    }
    if (total <= 0.0f) {
        return h;
    }
    for (int i = 0; i + 1 < trough.point_count; ++i) {
        const glm::vec2 a = trough.points[i];
        const glm::vec2 b = trough.points[i + 1];
        const glm::vec2 ab = b - a;
        const float len = glm::length(ab);
        const float t = len > 0.0f
                          ? std::clamp(glm::dot(world - a, ab) / (len * len), 0.0f, 1.0f)
                          : 0.0f;
        const float d = glm::length(world - (a + ab * t));
        if (d < best_d) {
            best_d = d;
            best_t = (cum + t * len) / total;
        }
        cum += len;
    }
    if (best_d >= reach) {
        return h;
    }
    const float floor_h =
        trough.floor_source + (trough.floor_mouth - trough.floor_source) * best_t;
    if (best_d < trough.half_width) {
        // Valley interior: terrain is clamped to the designed cross-section
        // (floor + parabolic rise, 1.5 m noise allowance below) so the floor
        // descends monotonically all the way to the lake rim — no stray dips
        // that would pond against the levee, no rises that would divert the
        // §3.1 descent out of the valley.
        const float cross = best_d / trough.half_width;
        const float surface = floor_h + trough.wall_height * cross * cross;
        return std::clamp(h, surface - 1.5f, surface);
    }
    // Shoulder: raise terrain to the crest, fading back to the base outward —
    // this is the watershed that keeps the drainage in the valley.
    const float crest = floor_h + trough.wall_height;
    const float s = smoothstep01((best_d - trough.half_width) / shoulder_band);
    return std::max(h, (1.0f - s) * crest + s * h);
}

/// Additive radial bump (knoll, bluff).
float bump_height(const BumpStamp& bump, glm::vec2 world) {
    if (bump.radius <= 0.0f) {
        return 0.0f;
    }
    const float d = glm::length(world - bump.center);
    if (d >= bump.radius) {
        return 0.0f;
    }
    const float t = smoothstep01(1.0f - d / bump.radius);
    return bump.height * t * t;
}

/// Lake basin stamp (§3.2): carve the bed below LAKE_LEVEL inside the ellipse;
/// raise a soft rim ring so the water plane is contained by construction. The
/// rim crest is lowered toward outlet_dir so the spill point (river exit) is
/// where the design wants it.
float lake_stamp(const LakeStamp& lake, float h, glm::vec2 world) {
    const float q = lake_norm_radius(lake, world);
    if (q < 1.0f) {
        const float bed = LAKE_LEVEL_M - LAKE_DEPTH_M * smoothstep01(1.0f - q);
        return std::min(h, bed);
    }
    const float band = lake.rim_band_frac;
    const float fade = lake.rim_fade_frac;
    if (q >= 1.0f + band + fade) {
        return h;
    }
    const glm::vec2 out = world - lake.center;
    const float out_len = glm::length(out);
    const float alignment =
        out_len > 0.0f ? std::max(0.0f, glm::dot(out / out_len, glm::normalize(lake.outlet_dir)))
                       : 0.0f;
    const float crest_top = LAKE_LEVEL_M + lake.rim_rise - lake.outlet_bias * alignment;
    if (q < 1.0f + band) {
        const float crest =
            LAKE_LEVEL_M + (crest_top - LAKE_LEVEL_M) * smoothstep01((q - 1.0f) / band);
        return std::max(h, crest);
    }
    const float s = smoothstep01((q - 1.0f - band) / fade);
    const float floor_h = (1.0f - s) * crest_top + s * h;
    return std::max(h, floor_h);
}

} // namespace

float aniso_octave_sample(uint64_t seed, uint32_t stream, float cell, glm::vec2 world,
                          float stretch, float theta_offset) {
    return aniso_value_noise(seed, stream, cell, world,
                             stretch > 0.0f ? stretch
                                            : static_cast<float>(config::HILL_ANISOTROPY),
                             theta_offset);
}

float ground_micro_relief(uint64_t seed, glm::vec2 world) {
    const float amp = static_cast<float>(config::GROUND_MICRO_AMPLITUDE_MIN)
                    + value_noise(seed, STREAM_MASSIF_MICRO_AMP, 256.0f, world)
                          * static_cast<float>(config::GROUND_MICRO_AMPLITUDE_MAX
                                               - config::GROUND_MICRO_AMPLITUDE_MIN);
    const float a = value_noise(seed, STREAM_MASSIF_MICRO,
                                static_cast<float>(config::GROUND_MICRO_WAVELENGTH_MIN), world)
                        * 2.0f - 1.0f;
    const float b = value_noise(seed, STREAM_MASSIF_MICRO + 1,
                                static_cast<float>(config::GROUND_MICRO_WAVELENGTH_MAX), world)
                        * 2.0f - 1.0f;
    return amp * 0.5f * (a + b);
}

float macro_height(uint64_t seed, const TestbedLayout& layout, glm::vec2 world) {
    if (layout.stand == StandId::Forest) {
        // §8.1: the forest stand is a different landform composition, not a
        // re-tune of the testbed — it branches whole here so the testbed path
        // below stays byte-identical (guarded by the pinned-heightmap test).
        return forest_stand_height(seed, layout, world);
    }
    float h = base_height(seed, world);
    for (const ValleyTrough& trough : layout.troughs) {
        h = trough_shape(trough, h, world);
    }
    h = std::max(h, massif_height(seed, layout.crag, world));
    h += bump_height(layout.knoll, world);
    h += bump_height(layout.bluff, world);
    h = lake_stamp(layout.lake, h, world);
    h -= path_groove_depth(layout, world);
    return std::clamp(h, 0.0f, MAX_HEIGHT_M);
}

bool breaks_massif_apron(uint64_t seed, const CragStamp& crag, glm::vec2 world,
                         float canopy_top_y) {
    // Off the massif's own stamp there is no apron. massif_height returns 0
    // outside the lobed footprint, which is the seed's real extent rather than
    // a radius anyone chose.
    if (massif_height(seed, crag, world) <= 0.0f) {
        return false;
    }
    const float cliffline = base_height(seed, crag.center)
                          + crag.peak_height * static_cast<float>(config::MASSIF_CLIFFLINE_FRAC);
    return canopy_top_y > cliffline;
}

float crag_distance(const TestbedLayout& layout, glm::vec2 world) {
    return glm::length(world - layout.crag.center);
}

float path_groove_depth(const TestbedLayout& layout, glm::vec2 world) {
    constexpr float HALF_W = static_cast<float>(config::PATH_GROOVE_HALF_WIDTH);
    const float d = corridor_distance(layout, world);
    if (d >= HALF_W) {
        return 0.0f;
    }
    return static_cast<float>(config::PATH_GROOVE_DEPTH)
         * (1.0f - smoothstep01(d / HALF_W));
}

} // namespace dfn::world
