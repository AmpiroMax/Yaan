/*
Module: engine/world
File: engine/world/sources/WorldBake.cpp

Responsibility:
- Implements bake_world(): the world-level context once, then every chunk of
  the extent in deterministic order, into a .dfw.

Dependencies:
- Uses: WorldBake.h, Worldgen.h, WorldFormat.h.
- Used by: tools/bake_world.cpp, tests, the app's first-run cache.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The context is built ONCE for the whole bake, exactly as ChunkManager builds
  it once for a session. Building it per chunk would still be correct — it is a
  pure function of the params — and would still be wrong: the world-level
  passes (drainage, sites, the giants) are catchment-scale, and paying for them
  per chunk is how a bake turns into an overnight job.
*/

#include "engine/world/sources/WorldBake.h"

#include <chrono>
#include <filesystem>
#include <system_error>

namespace dfn::world {

BakeReport bake_world(const WorldGenParams& params, const std::filesystem::path& path,
                      void (*progress)(uint32_t done, uint32_t total)) {
    BakeReport report;
    const auto started = std::chrono::steady_clock::now();

    if (params.max_chunk.x < params.min_chunk.x || params.max_chunk.z < params.min_chunk.z) {
        report.error = "empty extent: max_chunk is behind min_chunk";
        return report;
    }
    const auto span_x = static_cast<int64_t>(params.max_chunk.x) - params.min_chunk.x + 1;
    const auto span_z = static_cast<int64_t>(params.max_chunk.z) - params.min_chunk.z + 1;
    const auto total = static_cast<uint32_t>(span_x * span_z);

    // ONE context for the whole bake. The world-level passes it holds —
    // drainage, sites, the landmark oaks — are catchment-scale quantities that
    // a single chunk cannot compute for itself, which is exactly why they are
    // built here and not inside the loop.
    const WorldGenContext ctx = build_world_context(params);

    WorldInfo info;
    info.seed = params.seed;
    // The generator carries no version constant of its own yet, and inventing
    // one HERE would put the world's identity in the baker rather than in the
    // generator that decides it. 0 means "unversioned generator", which is the
    // truth; the day worldgen declares a version this reads it (Rule 35).
    info.worldgen_version = 0;
    info.min_chunk = params.min_chunk;
    info.max_chunk = params.max_chunk;

    WorldFileWriter writer;
    writer.begin(info);
    // ROW MAJOR, z OUTER. The order is part of the determinism promise, not a
    // taste: two bakes of one seed must be byte-identical, and a file whose
    // section order depended on how the loop happened to be nested would break
    // that the first time somebody "tidied" the loop.
    for (int32_t z = params.min_chunk.z; z <= params.max_chunk.z; ++z) {
        for (int32_t x = params.min_chunk.x; x <= params.max_chunk.x; ++x) {
            writer.append_chunk(generate_chunk(ctx, ChunkCoord{x, z}));
            ++report.chunks;
            if (progress != nullptr) {
                progress(report.chunks, total);
            }
        }
    }
    if (!writer.save(path)) {
        report.error = "could not write " + path.string();
        report.chunks = 0;
        return report;
    }

    std::error_code ec;
    report.bytes = static_cast<uint64_t>(std::filesystem::file_size(path, ec));
    if (ec) {
        report.bytes = 0; // the file saved; only its size is unknown
    }
    report.seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    return report;
}

} // namespace dfn::world
