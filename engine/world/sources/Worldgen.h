/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
Module: engine/world
File: engine/world/sources/Worldgen.h

Responsibility:
- The offline world generation API (Q13): seeded, strictly deterministic
  (Rule 13.1), produces a .dfw world file. The game NEVER generates at runtime —
  tools/worldgen (lead-owned CLI) calls this library and the game only reads
  the result.

Key items:
- WorldGenParams: seed + extent.
- generate_world(): params -> .dfw file on disk.
- generate_chunk(): single-chunk generation, exposed for determinism tests.

Dependencies:
- Uses: Chunk.h, WorldFormat.h, engine/core/config.
- Used by: tools/worldgen CLI, determinism tests (Rule 13.1, first-commit test),
  editor re-generation flows (via lead).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM IS NON-NEGOTIABLE (Rule 13.1): same params -> byte-identical .dfw
  on every platform. No std::rand, no unordered iteration into output, no float
  fast-math dependence; all randomness flows from the seeded engine below.
- WorldEntityIds are assigned deterministically (chunk-ordered, stable across
  regeneration with the same params) — save deltas depend on it (Q56).
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — offline deterministic worldgen API
  (Q13, Rule 13.1) with per-chunk entry point for tests.
*/

#pragma once

#include "engine/world/sources/Chunk.h"
#include "engine/world/sources/WorldFormat.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace dfn::world {

/// Inputs of a full world generation. Everything influencing output is HERE —
/// if a knob is added, it must be serialized into WorldInfo so a world file
/// records exactly how it was made.
struct WorldGenParams {
    uint64_t seed = 0;
    ChunkCoord min_chunk;   ///< Inclusive extent; the testbed is 4x4 chunks (Q45).
    ChunkCoord max_chunk;
};

/// Result of generate_world with a human-readable failure reason (tool output).
struct WorldGenResult {
    bool ok = false;
    std::string error; ///< Empty on success. Tool-facing text, not localized
                       ///< (developer tooling, not a user-facing game string).
};

/// Generates the whole region and writes `out_file` (.dfw) atomically.
/// Deterministic: identical params produce a byte-identical file (Rule 13.1);
/// the determinism test regenerates twice and compares hashes.
[[nodiscard]] WorldGenResult generate_world(const WorldGenParams& params,
                                            const std::filesystem::path& out_file);

/// Generates a single chunk in isolation. Must produce bit-identical data to
/// the same chunk inside generate_world (chunk generation depends only on
/// params + coord — neighbor-aware passes derive shared edges from the same
/// noise field, not from neighbor state). Exposed for the determinism test and
/// the editor's preview.
[[nodiscard]] Chunk generate_chunk(const WorldGenParams& params, ChunkCoord coord);

/// The deterministic PRNG used by all worldgen passes: SplitMix64 streams keyed
/// by (seed, coord, pass tag). Declared here so tests can pin its outputs;
/// gameplay dice do NOT use this (they roll on the simulation's own RNG).
struct WorldGenRng {
    /// Stream for one (chunk, pass) pair. Same inputs -> same sequence, on
    /// every platform.
    [[nodiscard]] static WorldGenRng for_chunk(uint64_t seed, ChunkCoord coord, uint32_t pass_tag);

    /// Next raw 64 bits.
    [[nodiscard]] uint64_t next_u64();
    /// Uniform float in [0, 1).
    [[nodiscard]] float next_float01();
    /// Uniform integer in [min, max] (inclusive), rejection-sampled (no modulo
    /// bias — bias would silently diverge across value ranges).
    [[nodiscard]] uint32_t next_range(uint32_t min, uint32_t max);

    uint64_t state = 0;
};

} // namespace dfn::world
