/*
Created: 10:08:2026 - 11:52:00
Last updated: 10:08:2026 - 11:52:00
Module: engine/core/math
File: engine/core/math/sources/FloraField.h

Responsibility:
- THE CLUMP FIELD and THE MATURITY DRAW (user-ratified в19г / design §5.10,
  LANDSCAPE §1.7 BR-4). Position-keyed, seeded, deterministic scalar fields
  that decide WHERE ground cover bunches and HOW BIG a tree is. Authored by
  flora; transplanted here and owned by core the day worldgen became the
  consumer, because the DAG forbids engine/world including engine/render.

Key items:
- ClumpClass, ClumpParams, clump_params(), clump_raw(), clump_field(),
  mushroom_ring_offsets().
- flora_maturity_for(): the §5.10 25/60/12/3 tier draw.

Dependencies:
- Uses: config/Constants.h, glm, std. Nothing else, deliberately.
- Used by: world::WorldgenScatter (placement, the consumer this move was for),
  render/flora (via the forwarding header at engine/render/sources/FloraField.h
  and ProcFlora.h), tests in both zones.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- PURE AND DETERMINISTIC. Same (class, xz, seed) -> same value, all platforms.
- **DO NOT REMOVE THE RANK EQUALIZATION** (clump_detail::cdf_u). The raw field
  is uniform on [0,1] BY CONSTRUCTION, and every `coverage` below means
  literally "the top N% of ground" only because of it. Value noise is
  bell-shaped; without the equalizer every threshold silently stops meaning
  what it says. This project has already shipped a field that never left the
  top 60% of its range and tuned a dozen constants against the lie. Rule 31
  asserts the uniformity WITH the un-equalized noise as the failing control
  (tests/core/MathTests.cpp), and that control is the whole test.
- COMPOSITION ORDER IS DESIGN'S AMENDMENT AND IT IS BINDING:
      density(class, xz) = base(class) x clump(class, xz)
                           x edge_gradient(dist_to_path) x richness(path_class)
                           x exclusions
  THE EDGE GRADIENT IS THE CALLER'S AND IS APPLIED EXACTLY ONCE. This file
  computes NO ramp of its own — flora's clump_field_edged() did, and with the
  caller also applying PathSample::edge the band would have been SQUARED with
  its peak moved inward. It is deleted; the caller passes plain clump_field().
- The per-class values are REGISTRY ROWS: CLUMP_WAVELENGTH_<CLASS> /
  CLUMP_COVERAGE_<CLASS> / CLUMP_CONTRAST_<CLASS>. Cite names, never values.
*/
/*
UPD:
- 10:08:2026 - 11:52:00: TRANSPLANTED from engine/render/sources/FloraField.h
  (flora's authorship, unchanged machinery) into core's zone, because
  WorldgenScatter is now the consumer and world may not include render.
  flora_maturity_for() came in the same move: it is keyed the same way and its
  multiplier bands are what core's canopy occlusion envelope is defined from
  (SPECIES_HEIGHT_MAX x TREE_MATURITY_GIANT_MULT_MAX), so one home, Rule 35.
  The equalizer and the Rule 31 control came with it and are now core's to
  keep. Namespace dfn::render -> dfn::math; the old path is a forwarding
  header so flora's suite keeps running against the one definition.
*/

#pragma once

#include "engine/core/config/sources/Constants.h"

#include <glm/vec2.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cmath>

namespace dfn::math {

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

/// THE MATURITY-TIER DRAW (design §5.10: TREE_MATURITY_GIANT/MATURE/SUBMATURE/
/// YOUNG_PCT = 25/60/12/3). Returns the size multiplier for a tree standing at
/// this position: giant 1.15-1.50, mature 0.85-1.15, sub-mature 0.50-0.70
/// (design's mid-canopy layer — NOT a synonym for sapling, they are different
/// structural jobs), sapling 0.40-0.60. Position-keyed, so it is stable across
/// runs and chunk borders and needs no field on ScatterInstance.
///
/// CORE READS THIS FOR THE CANOPY OCCLUSION ENVELOPE, which is what brought it
/// out of render: the ceiling a sightline must clear is
/// SPECIES_HEIGHT_MAX x TREE_MATURITY_GIANT_MULT_MAX, not SPECIES_HEIGHT_MAX.
/// A 1.5x giant oak is 48 m against the 32 m the raycast used to cite — the
/// "model half the world's height" defect, caught before it shipped. Two
/// consumers, one definition (Rule 35).
[[nodiscard]] inline float flora_maturity_for(glm::vec2 world_xz) {
    using clump_detail::mix64;
    const auto xi = static_cast<uint64_t>(static_cast<int64_t>(std::lround(world_xz.x * 2.0f)));
    const auto zi = static_cast<uint64_t>(static_cast<int64_t>(std::lround(world_xz.y * 2.0f)));
    const uint64_t h = mix64(xi * 0x9E3779B1ull ^ mix64(zi ^ 0x5F0AB1ull));
    const float u = static_cast<float>(h >> 40) / 16777216.0f;        // tier draw
    const float w = static_cast<float>(mix64(h) >> 40) / 16777216.0f; // position in band

    const float giant = static_cast<float>(config::TREE_MATURITY_GIANT_PCT) / 100.0f;
    const float mature = static_cast<float>(config::TREE_MATURITY_MATURE_PCT) / 100.0f;
    const float sub = static_cast<float>(config::TREE_MATURITY_SUBMATURE_PCT) / 100.0f;
    auto band = [w](double lo, double hi) {
        return static_cast<float>(lo) + w * static_cast<float>(hi - lo);
    };
    if (u < giant) {
        return band(config::TREE_MATURITY_GIANT_MULT_MIN, config::TREE_MATURITY_GIANT_MULT_MAX);
    }
    if (u < giant + mature) {
        return band(config::TREE_MATURITY_MATURE_MULT_MIN, config::TREE_MATURITY_MATURE_MULT_MAX);
    }
    if (u < giant + mature + sub) {
        return band(config::TREE_MATURITY_SUBMATURE_MULT_MIN,
                    config::TREE_MATURITY_SUBMATURE_MULT_MAX);
    }
    return band(config::TREE_MATURITY_SAPLING_MULT_MIN, config::TREE_MATURITY_SAPLING_MULT_MAX);
}

} // namespace dfn::math
