/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 11:05:22
Module: engine/world
File: engine/world/sources/WorldgenNoise.h

Responsibility:
- Worldgen-internal deterministic noise primitives shared by all v2 passes:
  SplitMix64 mixing, seeded value noise, ridged noise, smoothstep. Extracted
  from the stage-2 Worldgen.cpp so macro/hydrology/scatter draw from ONE
  implementation (identical values everywhere = chunk-independent generation).

Key items:
- mix64, lattice_value, value_noise, ridged_noise, smoothstep01.

Dependencies:
- Uses: glm, <cstdint>, <cmath>.
- Used by: WorldgenMacro, WorldgenHydrology, WorldgenScatter, WorldgenSites.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM IS NON-NEGOTIABLE (Rule 13.1): position-based, seeded, no state.
  Changing any formula here changes every world hash — bump worldgen_version.
- Internal to engine/world; never include from another zone.
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — extracted stage-2 noise (unchanged values)
  + ridged noise for the L0 crag stamp.
*/

#pragma once

#include <cmath>
#include <cstdint>
#include <glm/vec2.hpp>

namespace dfn::world::noise {

/// SplitMix64 finalizer — the single mixing primitive of all worldgen hashing.
constexpr uint64_t mix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

/// Deterministic lattice value in [0, 1) for integer lattice point (gx, gz).
/// `stream` separates octaves AND passes (identical to the stage-2 octave tag
/// for streams 0..2 — base fBm hashes are unchanged).
inline float lattice_value(uint64_t seed, uint32_t stream, int64_t gx, int64_t gz) {
    uint64_t h = mix64(seed ^ (0xA24BAED4963EE407ull + stream));
    h = mix64(h ^ static_cast<uint64_t>(gx));
    h = mix64(h ^ static_cast<uint64_t>(gz));
    return static_cast<float>(h >> 40) * (1.0f / 16777216.0f); // top 24 bits
}

inline float smoothstep01(float t) { return t * t * (3.0f - 2.0f * t); }

/// Bilinear value noise in [0, 1) at world position, on a lattice of cell_size.
inline float value_noise(uint64_t seed, uint32_t stream, float cell_size, glm::vec2 world) {
    const float cx = world.x / cell_size;
    const float cz = world.y / cell_size;
    const int64_t gx = static_cast<int64_t>(std::floor(cx));
    const int64_t gz = static_cast<int64_t>(std::floor(cz));
    const float tx = smoothstep01(cx - static_cast<float>(gx));
    const float tz = smoothstep01(cz - static_cast<float>(gz));

    const float v00 = lattice_value(seed, stream, gx, gz);
    const float v10 = lattice_value(seed, stream, gx + 1, gz);
    const float v01 = lattice_value(seed, stream, gx, gz + 1);
    const float v11 = lattice_value(seed, stream, gx + 1, gz + 1);
    const float v0 = v00 + (v10 - v00) * tx;
    const float v1 = v01 + (v11 - v01) * tx;
    return v0 + (v1 - v0) * tz;
}

/// Ridged noise in [0, 1]: sharp crests where the underlying value noise
/// crosses 0.5 (1 - |2v - 1|). Two octaves (cell, cell/2) weighted 0.7/0.3.
inline float ridged_noise(uint64_t seed, uint32_t stream, float cell_size, glm::vec2 world) {
    const float r0 = 1.0f - std::fabs(2.0f * value_noise(seed, stream, cell_size, world) - 1.0f);
    const float r1 =
        1.0f - std::fabs(2.0f * value_noise(seed, stream + 1, cell_size * 0.5f, world) - 1.0f);
    return 0.7f * r0 + 0.3f * r1;
}

} // namespace dfn::world::noise
