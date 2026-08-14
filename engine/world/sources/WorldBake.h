/*
Created: 14:08:2026 - 20:47:52
Last updated: 14:08:2026 - 20:47:52
Module: engine/world
File: engine/world/sources/WorldBake.h

Responsibility:
- The BAKE: run the generator over a whole extent once, offline, and write the
  result as a .dfw the game only reads. The second half of the tooling pivot's
  baker (в1: nothing is generated in the frame).

Key items:
- BakeReport: what one bake produced, so a caller can say it out loud.
- bake_world(): params -> file.

Dependencies:
- Uses: Worldgen.h (the generator), WorldFormat.h (the container).
- Used by: tools/bake_world.cpp (the CLI), the app's first-run cache, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THIS IS NOT A SECOND GENERATOR AND MUST NEVER BECOME ONE. It calls
  generate_chunk and nothing else; the day it grows a "small adjustment" of its
  own, the baked world stops being the world the determinism tests measure, and
  the difference will surface as a bug in a system that has no idea a baker
  exists (Rule 32/35).
- The chunk ORDER is part of the format's determinism promise (Rule 13.1): row
  major, z outer. Two bakes of one seed are byte-identical, which is what makes
  a cached bake trustworthy rather than merely convenient.
*/
/*
UPD:
- 14:08:2026 - 20:47:52: Created — the generator finally has an offline caller.
  A LIBRARY function rather than only a CLI, because the app has to be able to
  bake a map on its first run (docs/MAP_LAYOUT.md: the manifest is the recipe,
  the .dfw is the cache) and a composition root shelling out to a binary it
  shipped beside itself would be a second way to do one thing.
*/

#pragma once

#include "engine/world/sources/WorldFormat.h"
#include "engine/world/sources/Worldgen.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace dfn::world {

/// What a bake produced. Reported rather than returned as a bare bool because
/// "it worked" is not the interesting part: a bake that wrote ZERO chunks also
/// worked, and it is the number that says the extent was wrong.
struct BakeReport {
    uint32_t chunks = 0;      ///< chunks written
    uint64_t bytes = 0;       ///< size of the finished file
    double seconds = 0.0;     ///< wall time of the whole bake
    std::string error;        ///< empty when ok
};

/// Generates every chunk of `params`' extent and writes them to `path`.
///
/// `progress` is called once per chunk with (done, total) so a caller with a
/// loading screen has something honest to show; pass nullptr when nobody is
/// watching. It is called from this thread — the bake is synchronous on
/// purpose, since the two callers (a CLI and a loading screen) both want it
/// finished before they continue.
[[nodiscard]] BakeReport bake_world(const WorldGenParams& params,
                                    const std::filesystem::path& path,
                                    void (*progress)(uint32_t done, uint32_t total) = nullptr);

} // namespace dfn::world
