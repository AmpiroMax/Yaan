/*
Created: 10:08:2026 - 02:16:00
Last updated: 10:08:2026 - 11:24:00
Module: engine/render
File: engine/render/sources/FloraField.h

Responsibility:
- THE CLUMP FIELD (user-ratified в19г: clumping is an AUTHORED FIELD, not
  randomness; design blessed the spec 10.08.2026 with amendments, LANDSCAPE
  §1.7 BR-4). A seeded, deterministic, low-frequency scalar field per
  ground-cover class; core's scatter density for that class MULTIPLIES by it,
  so flowers come in drifts, mushrooms in rings, moss in patches — Tsushima's
  clumping-factor lesson (LIVING_WORLD_RESEARCH §A3/§A7).

Key items:
- ClumpClass, ClumpParams, clump_params(), clump_raw(), clump_field(),
  mushroom_ring_offsets().

Dependencies:
- Uses: glm and <cstdint> ONLY — deliberately dependency-free so the file can
  move to engine/core/math (core's zone) the day worldgen consumes it; the DAG
  forbids engine/world including engine/render, so this header's PRESENT home
  is provisional and its portability is a design property, not tidiness.
- Used by: ProcFloraTests (Rule 31 verification); core's WorldgenScatter once
  the placement wiring lands (messaged 10.08.2026).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md.
- PURE AND DETERMINISTIC. Same (class, xz, seed) -> same value, all platforms.
- THE RAW FIELD IS UNIFORM ON [0,1] BY CONSTRUCTION (rank equalization through
  a deterministic CDF table), and Rule 31 asserts it with the un-equalized
  noise as the failing control. Do not remove the equalization step: value
  noise is bell-shaped, and every threshold below would silently stop meaning
  what it says (this project has already shipped a field that never left the
  top 60 % of its range — every constant tuned against it was tuned against a
  lie).
- COMPOSITION ORDER IS DESIGN'S AMENDMENT AND IT IS BINDING:
      density(class, xz) = base(class) x clump(class, xz)
                           x edge_gradient(dist_to_path) x richness(path_class)
                           x exclusions
  where the edge gradient acts as a FLOOR on the field, so a coverage gap can
  never bare a path margin. THAT FACTOR IS THE CALLER'S AND IS APPLIED ONCE:
  core passes PathSample::edge, which already carries the band shape and
  design's per-class maintenance scoping. This file deliberately computes NO
  ramp of its own — two ramps multiply into a squared band with its peak in
  the wrong place. The trodden-centre zero is core's exclusion mask.
  **THE FLOOR IS SCOPED BY MAINTENANCE** (design, 10.08.2026): the guarantee
  that installs BR-3 is precisely what would garden a cobbled gutter, so it is
  scaled by the path class's richness (FloraEdgeRules.h) and stops applying on
  swept classes. BR-3's ratio is measured on the hint-path; a cobbled street
  failing it is a PASS.
- The per-class parameter values are REGISTRY ROWS (landed 10.08.2026):
  CLUMP_WAVELENGTH_<CLASS> / CLUMP_COVERAGE_<CLASS> / CLUMP_CONTRAST_<CLASS>,
  read from the generated Constants.h. Cite the names, never the values.
- ACCEPTANCE IS CLUMP_R_NORM_MAX (0.85) ON A NORMALISED CLARK-EVANS R:
  R_norm = R(field on) / R(the same placement with the field flattened to that
  class's own mean). The denominator is NOT ideal Poisson and NOT a single
  constant: the placement lattice carries its own regularity, and that
  regularity CHANGES WITH ACCEPTANCE RATE (measured 1.052 at coverage 0.09 up
  to 1.136 at 0.35), so the control is re-taken per class or low-coverage
  classes are divided by a denominator that was never theirs. Full table and
  derivation in docs/specs/flora.md §3.12.
*/
/*
UPD:
- 10:08:2026 - 02:16:00: Created — field machinery per design's blessed spec:
  per-class seeded low-frequency field with WAVELENGTH / COVERAGE / CONTRAST
  as the authorship, rank-equalized raw noise (Rule 31), the edge-floor
  composition (design amendment 2), and the mushroom ring second stage
  (parent-child under the field, not more noise).
- 10:08:2026 - 02:34:52: The authored values became registry rows (lead landed
  CLUMP_* with design's signature); literals replaced by Constants.h names
  (Rule 14). Ring/cluster parity moved onto the PARENT SEED itself so the
  contract "even parents ring" is legible to core's find promotion.
- 10:08:2026 - 11:07:33: clump_field_edged() takes path_richness: the BR-3
  floor is the very machinery that would garden a cobbled gutter, so it is
  scoped by the maintenance column and stops applying on swept classes.
- 10:08:2026 - 11:24:00: clump_field_edged() DELETED — core wired the
  consumer and applies the BR-3 gradient once from PathSample::edge; two ramps
  square the band. A tombstone records where its two invariants went.
*/

#pragma once

#include "engine/core/config/sources/Constants.h"

#include <glm/vec2.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace dfn::render {

/// Ground-cover classes that clump. Each gets an INDEPENDENT field (same
/// machinery, different salt), so a flower drift and a mushroom colony do not
/// mysteriously share a shoreline.
enum class ClumpClass : uint8_t {
    Flowers = 0,
    Mushrooms = 1,
    Moss = 2,
    GrassTufts = 3,
    Pebbles = 4,
};
inline constexpr uint8_t CLUMP_CLASS_COUNT = 5;

/// The authorship. Tsushima paints clump maps by hand; our author is three
/// numbers per class (design's ruling: "the per-class parameters ARE the
/// paint").
struct ClumpParams {
    float wavelength_m = 20.0f; ///< drift size: field feature scale
    float coverage = 0.2f;      ///< fraction of ground carrying the class AT ALL
    float contrast = 0.6f;      ///< 0 = gentle waxing, 1 = hard drift edges
};

namespace clump_detail {

inline uint64_t mix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

inline float lattice(int32_t x, int32_t z, uint32_t seed) {
    const uint64_t h = mix64((static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32)
                             ^ static_cast<uint32_t>(z) ^ (static_cast<uint64_t>(seed) << 17));
    return static_cast<float>(h >> 40) / 16777216.0f;
}

/// One octave of value noise with smoothstep interpolation, plus a second
/// octave at half amplitude for interior variety. NOT uniform — see cdf_u().
inline float value_noise(glm::vec2 p, uint32_t seed) {
    auto octave = [&](glm::vec2 q, uint32_t s) {
        const float fx = std::floor(q.x);
        const float fz = std::floor(q.y);
        const auto x0 = static_cast<int32_t>(fx);
        const auto z0 = static_cast<int32_t>(fz);
        const float tx0 = q.x - fx;
        const float tz0 = q.y - fz;
        const float tx = tx0 * tx0 * (3.0f - 2.0f * tx0);
        const float tz = tz0 * tz0 * (3.0f - 2.0f * tz0);
        const float a = lattice(x0, z0, s);
        const float b = lattice(x0 + 1, z0, s);
        const float c = lattice(x0, z0 + 1, s);
        const float d = lattice(x0 + 1, z0 + 1, s);
        return (a * (1.0f - tx) + b * tx) * (1.0f - tz)
            + (c * (1.0f - tx) + d * tx) * tz;
    };
    return (octave(p, seed) * 2.0f + octave(p * 2.03f, seed ^ 0x9E37u)) / 3.0f;
}

/// Rank equalization: maps the bell-shaped value noise onto UNIFORM [0,1]
/// through its own empirical CDF, sampled once, deterministically. After this,
/// "coverage 0.18" means EXACTLY the top 18 % of ground — a threshold on a
/// non-uniform field means whatever the bell decides, which is Rule 31's
/// defect wearing a hat.
inline float cdf_u(float n) {
    static const std::array<float, 257> table = [] {
        std::array<uint32_t, 1024> hist{};
        constexpr int SAMPLES = 512;
        for (int ix = 0; ix < SAMPLES; ++ix) {
            for (int iz = 0; iz < SAMPLES; ++iz) {
                const glm::vec2 p{static_cast<float>(ix) * 0.173f + 0.031f,
                                  static_cast<float>(iz) * 0.229f + 0.017f};
                const float v = std::clamp(value_noise(p, 0xC1F0u), 0.0f, 0.999f);
                ++hist[static_cast<size_t>(v * 1024.0f)];
            }
        }
        std::array<float, 257> t{};
        const float total = static_cast<float>(SAMPLES) * SAMPLES;
        uint64_t run = 0;
        // t[i] = CDF at n = i/256.
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

} // namespace clump_detail

/// The authored values are REGISTRY ROWS (landed by the lead 10.08.2026,
/// design-signed): CLUMP_WAVELENGTH/COVERAGE/CONTRAST_<CLASS>.
[[nodiscard]] inline ClumpParams clump_params(ClumpClass c) {
    auto f = [](double v) { return static_cast<float>(v); };
    switch (c) {
    case ClumpClass::Flowers:
        return {f(config::CLUMP_WAVELENGTH_FLOWERS), f(config::CLUMP_COVERAGE_FLOWERS),
                f(config::CLUMP_CONTRAST_FLOWERS)};
    case ClumpClass::Mushrooms:
        return {f(config::CLUMP_WAVELENGTH_MUSHROOMS),
                f(config::CLUMP_COVERAGE_MUSHROOMS), f(config::CLUMP_CONTRAST_MUSHROOMS)};
    case ClumpClass::Moss:
        return {f(config::CLUMP_WAVELENGTH_MOSS), f(config::CLUMP_COVERAGE_MOSS),
                f(config::CLUMP_CONTRAST_MOSS)};
    case ClumpClass::GrassTufts:
        return {f(config::CLUMP_WAVELENGTH_GRASSTUFTS),
                f(config::CLUMP_COVERAGE_GRASSTUFTS),
                f(config::CLUMP_CONTRAST_GRASSTUFTS)};
    case ClumpClass::Pebbles:
        return {f(config::CLUMP_WAVELENGTH_PEBBLES), f(config::CLUMP_COVERAGE_PEBBLES),
                f(config::CLUMP_CONTRAST_PEBBLES)};
    }
    return {};
}

/// The RAW field: uniform on [0,1] by construction (rank-equalized), feature
/// scale = the class wavelength, independent per class. This is the object
/// Rule 31 verifies; everything below is shaping.
[[nodiscard]] inline float clump_raw(ClumpClass c, glm::vec2 world_xz, uint32_t seed) {
    const ClumpParams p = clump_params(c);
    const uint32_t salt = seed ^ (0x517CC1B7u * (static_cast<uint32_t>(c) + 1u));
    return clump_detail::cdf_u(
        clump_detail::value_noise(world_xz / std::max(p.wavelength_m, 1.0f), salt));
}

/// The FIELD: what scatter density multiplies by. Zero outside the covered
/// fraction; rises toward 1 in drift interiors, with contrast setting how hard
/// the edge snaps. Because the raw field is uniform, `coverage` is exact:
/// the field is non-zero on exactly that fraction of ground (asserted).
[[nodiscard]] inline float clump_field(ClumpClass c, glm::vec2 world_xz, uint32_t seed) {
    const ClumpParams p = clump_params(c);
    const float u = clump_raw(c, world_xz, seed);
    const float t = (u - (1.0f - p.coverage)) / std::max(p.coverage, 1e-4f);
    if (t <= 0.0f) return 0.0f;
    // Contrast: the drift saturates after `1 - contrast` of its rise, so high
    // contrast = a plateau with a crisp rim, low contrast = a gentle swell.
    const float edge = std::max(1.0f - p.contrast * 0.85f, 0.10f);
    const float s = std::min(t / edge, 1.0f);
    return s * s * (3.0f - 2.0f * s);
}

/// NOTE — `clump_field_edged()` WAS HERE AND IS DELETED (10.08.2026), by
/// agreement with core once they wired the consumer. The BR-3 edge gradient is
/// now applied ONCE, by the caller, from `PathSample::edge` — which already
/// carries the band shape AND design's per-class maintenance scoping. This
/// header computing a second ramp would have SQUARED the band and moved its
/// peak inward, and the symptom would have been "the verge looks a bit thin",
/// which nobody diagnoses. Deleted rather than kept as a pass-through on
/// core's reasoning, which is right: a function whose name promises an edge
/// and no longer computes one is the next reader's trap.
///
/// The two invariants it carried moved WITH it, to core, with their controls:
/// (1) the floor never SUBTRACTS from the field, and (2) a kept verge is not
/// bare ground — at maintenance 0 the margin falls back to the field value,
/// never to zero. They are composition-level properties now and cannot be
/// asserted from this file; losing them silently is the failure mode to avoid.

/// SECOND STAGE under the mushroom field (design's blessed split): within a
/// drift, mushrooms arrive as ring/cluster CHILDREN of a parent point, not as
/// more noise. Given the parent's seed, returns up to `max_out` XZ offsets in
/// metres. Even parents ring (the find-catalog entry design named — a ring can
/// be promoted to a BR-6 find), odd parents clump.
inline int mushroom_ring_offsets(uint64_t parent_seed, glm::vec2* out, int max_out) {
    using clump_detail::mix64;
    uint64_t s = mix64(parent_seed);
    auto unit = [&s] {
        s = mix64(s);
        return static_cast<float>(s >> 40) / 16777216.0f;
    };
    // Parity of the PARENT SEED itself, not of a hash of it: "even parents
    // ring" is a contract core can rely on when promoting a ring to a BR-6
    // find, so it must be legible from the seed, not an accident of mixing.
    const bool ring = (parent_seed & 1u) == 0u;
    const int n = std::min(max_out, ring ? 6 + static_cast<int>(unit() * 4.0f)
                                         : 4 + static_cast<int>(unit() * 3.0f));
    const float radius = ring ? 0.9f + unit() * 1.1f : 0.35f + unit() * 0.4f;
    for (int i = 0; i < n; ++i) {
        const float az = 6.2831853f * (static_cast<float>(i) + unit() * 0.35f)
            / static_cast<float>(n);
        const float r = radius * (ring ? 0.9f + unit() * 0.2f : unit());
        out[i] = glm::vec2{std::cos(az) * r, std::sin(az) * r};
    }
    return n;
}

} // namespace dfn::render
