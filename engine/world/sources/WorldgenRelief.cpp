/*
Created: 11:08:2026 - 14:31:10
Last updated: 12:08:2026 - 23:38:00
Module: engine/world
File: engine/world/sources/WorldgenRelief.cpp

Responsibility:
- Implementation of §2.7's general ground relief and its two masks.

Key items:
- ground_meso_relief, ground_relief.

Dependencies:
- Uses: WorldgenRelief.h, WorldgenMacro.h, WorldgenNoise.h, config.
- Used by: Worldgen.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic and position-based: no chunk state, no neighbour queries, so
  chunk borders agree without communication (Rule 13.1).
*/
/*
UPD:
- 11:08:2026 - 14:31:10: Created.
- 12:08:2026 - 23:38:00: DFN_MESO_LAMBDA_MAX — the wavelength sweep door
  (measurement only). The result is in tests/core/GroundReliefTests.cpp and it
  is negative: shorter waves buy slope and buy no ground-hiding, so the top of
  GROUND_MESO_WAVELENGTH should NOT be lowered for F7's sake. The door stays
  because the next person will have the same idea.
*/

#include "engine/world/sources/WorldgenRelief.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenNoise.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <glm/geometric.hpp>

namespace dfn::world {

namespace {

/// Smooth 0->1 ramp, the same shape used across the macro pass.
float smoothstep01(float t) {
    const float x = std::clamp(t, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

/// The slow field that drifts an octave's amplitude. 256 m is the same figure
/// ground_micro_relief already uses for the same job — one drift scale for both
/// tiers, so the roughness varies together rather than beating against itself.
constexpr float AMP_DRIFT_CELL = 256.0f;


/// THE WAVELENGTH SWEEP DOOR (measurement only, never a shipping path — the
/// same standing as DFN_MESO_ISO below and DFN_NO_RELIEF further down).
///
/// `DFN_MESO_LAMBDA_MAX=<m>` replaces the TOP of GROUND_MESO_WAVELENGTH for one
/// run. It exists because the arithmetic that matters here is about a quantity
/// the contract never named: at the σ this field produces, RMS slope 2*pi*σ/L
/// clears the grazing angle only below L ~ 52 m, while the approved band runs
/// to 60 m — so the top third of our own band cannot hide ground at the
/// amplitude we make. Shortening the wave is FREE against the σ ceiling,
/// against corridor slope and against PLAYER_STEP_HEIGHT, where raising the
/// amplitude is not. Whether the band's top actually moves is design's ruling;
/// this door is what lets the ruling be made on a measurement.
float meso_lambda_max() {
    if (const char* e = std::getenv("DFN_MESO_LAMBDA_MAX")) {
        const float v = std::strtof(e, nullptr);
        if (v >= 4.0f && v <= 400.0f) return v;
    }
    return static_cast<float>(config::GROUND_MESO_WAVELENGTH_MAX);
}

/// The meso field BEFORE amplitude: the two ruled wavelengths, averaged, both
/// sampled along the land's grain.
float meso_field(uint64_t seed, glm::vec2 world) {
    // COUNTERFACTUAL ARM (Rule 30): DFN_MESO_ISO=1 samples the same octave
    // isotropically. It exists because sharing the land's grain and opening
    // ground-hiding-ground pull against each other by arithmetic — stretching a
    // wavelength by HILL_ANISOTROPY divides its slope by the same factor — and
    // that trade has to be measurable rather than argued.
    if (std::getenv("DFN_MESO_ISO") != nullptr) {
        return (noise::value_noise(seed, STREAM_GROUND_MESO,
                                   static_cast<float>(config::GROUND_MESO_WAVELENGTH_MIN), world)
                + noise::value_noise(seed, STREAM_GROUND_MESO + 1, meso_lambda_max(), world))
             * 0.5f;
    }
    const float a = aniso_octave_sample(seed, STREAM_GROUND_MESO,
                                        static_cast<float>(config::GROUND_MESO_WAVELENGTH_MIN),
                                        world);
    const float b = aniso_octave_sample(seed, STREAM_GROUND_MESO + 1, meso_lambda_max(), world);
    return (a + b) * 0.5f;
}

/// RANK EQUALIZATION onto uniform [0,1] through the field's own empirical CDF —
/// the technique §8.1's grive field already uses, applied to THIS construction
/// (its table samples grive_noise and would be wrong here; the shape of the
/// distribution is a property of the construction, so each construction needs
/// its own table, and reusing one across two would be the shadow copy).
///
/// WHY IT IS NEEDED, MEASURED. A sum of smoothstep-interpolated value noise is
/// strongly bell-shaped, so the DECLARED band and the REALIZED band are not the
/// same thing: with the raw sum, GROUND_MESO_AMPLITUDE 1.5-4 m produced a
/// detrended σ of 0.270 m at the A1 standpoint against §10.1's 0.35 m floor —
/// the ground was only ever using the middle of its own approved amplitude.
/// This is Rule 31's defect and the third time this project has met it (the
/// massif's bearing field, the grive field, now this one).
///
/// Seed-independent by construction: a fixed probe seed and a fixed domain
/// capture the SHAPE of the distribution, not one world's draw.
float meso_cdf_u(float n) {
    static const std::array<float, 257> table = [] {
        std::array<uint32_t, 1024> hist{};
        constexpr int SAMPLES = 384;
        for (int iz = 0; iz < SAMPLES; ++iz) {
            for (int ix = 0; ix < SAMPLES; ++ix) {
                const glm::vec2 p{static_cast<float>(ix) * 7.13f + 0.37f,
                                  static_cast<float>(iz) * 9.41f + 0.19f};
                const float v = std::clamp(meso_field(0xC0FFEEull, p), 0.0f, 0.999f);
                ++hist[static_cast<std::size_t>(v * 1024.0f)];
            }
        }
        std::array<float, 257> t{};
        const float total = static_cast<float>(SAMPLES) * SAMPLES;
        uint64_t run = 0;
        std::size_t coarse = 0;
        for (std::size_t i = 0; i < 1024; ++i) {
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
    const auto i = static_cast<std::size_t>(std::min(x, 255.0f));
    const float f = x - static_cast<float>(i);
    return table[i] * (1.0f - f) + table[i + 1] * f;
}

} // namespace

float ground_meso_relief(uint64_t seed, glm::vec2 world) {
    const float amp = static_cast<float>(config::GROUND_MESO_AMPLITUDE_MIN)
                    + noise::value_noise(seed, STREAM_GROUND_MESO_AMP, AMP_DRIFT_CELL, world)
                          * static_cast<float>(config::GROUND_MESO_AMPLITUDE_MAX
                                               - config::GROUND_MESO_AMPLITUDE_MIN);
    // AMPLITUDE IS PEAK-TO-TROUGH, which is how §10.1.2 does the arithmetic
    // ("mid-range values as sinusoids ... semi-amplitude near 1.4 m" out of a
    // 1.5-4 m band). Equalized, the field covers +-amp/2 uniformly, so the
    // realized relief over one wavelength IS the declared amplitude rather
    // than the middle third of it.
    return amp * 0.5f * (2.0f * meso_cdf_u(meso_field(seed, world)) - 1.0f);
}


float ground_relief(uint64_t seed, const TestbedLayout& layout, glm::vec2 world,
                    float dist_to_water, float meso_scale) {
    // --- The shore mask (§2.7's ruling) --------------------------------------
    // Zero at the waterline, full at SHORE_SAND_DIST. §3.3 already sizes that
    // band as the deposited margin, so the taper reuses it rather than naming a
    // second distance. THIS IS WHY THE PREVIOUS ATTEMPT WAS BACKED OUT: without
    // it, 0.3-0.6 m dips on the bank fall under the water surface and render as
    // WaterBed past the §3.3 bed/mud cap.
    const float shore = smoothstep01(dist_to_water / static_cast<float>(config::SHORE_SAND_DIST));

    // --- The corridor mask (§2.4) --------------------------------------------
    // Zero on the graded corridor, full one corridor-width outside it. The
    // corridor's average slope is a validated invariant; hollows inside it
    // would be spending that invariant on scenery.
    const float half = static_cast<float>(config::CORRIDOR_WIDTH) * 0.5f;
    const float d_cor = corridor_distance(layout, world);
    const float corridor = smoothstep01((d_cor - half) / half);

    // --- The massif mask (§2.8 owns its own surface) -------------------------
    // Zero on the stamp, full one stamp-radius clear of it. Not timidity: the
    // massif already carries §2.7's micro octave on its benches by §2.8.2's own
    // rule, its band/bench/tor language is validated by I2-I8, and the crag
    // tunnel's portals are cut from that surface. Summing a second relief field
    // into it re-buried the uphill portal.
    // The fade band is a QUARTER of the stamp radius, not a whole one. At a
    // full radius the dead zone reached 240 m from a 120 m mountain and left
    // measurably billiard-flat legal ground on the hem (σ 0.037 m at
    // (949, 277), 142 m out) — the mask has to protect §2.8's surface, not a
    // ring of open country twice the mountain's size.
    const float crag_r = layout.crag.radius;
    const float massif =
        crag_r > 0.0f
            ? smoothstep01((glm::length(world - layout.crag.center) - crag_r) / (crag_r * 0.25f))
            : 1.0f;

    if (std::getenv("DFN_NO_RELIEF") != nullptr) return 0.0f;
    const float mask = shore * corridor * massif;
    if (mask <= 0.0f) {
        return 0.0f;
    }
    return (ground_meso_relief(seed, world) * std::clamp(meso_scale, 0.0f, 1.0f)
            + ground_micro_relief(seed, world))
         * mask;
}

} // namespace dfn::world
