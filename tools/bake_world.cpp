/*
Module: tools
File: tools/bake_world.cpp

Responsibility:
- The bake CLI: run the generator over an extent offline and write a .dfw.
  A thin main() over world::bake_world — every decision lives in the library,
  because the app has to bake the same way on a map's first run.

Usage:
    dfn_bake <out.dfw> [--layout <json>] [--seed N] [--min X,Z] [--max X,Z]

  Run it from the REPO ROOT: the default layout asset is loaded by relative
  path, exactly as the app and the tests load it, so the world baked here is
  the world they measure.

Dependencies:
- Uses: engine/world (bake_world, load_layout_file).
- Used by: humans and agents preparing demo maps (docs/MAP_LAYOUT.md).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- A MALFORMED ARGUMENT IS REFUSED OUT LOUD, never rounded into a default. A
  baker that quietly baked the shipping extent when asked for a 2x2 test world
  would hand back a file that loads, looks plausible and is not what was asked
  for — and it would be found days later, by somebody measuring something else.
*/

#include "engine/world/sources/LayoutLoad.h"
#include "engine/world/sources/WorldBake.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

/// Parses "X,Z". Returns false on anything else — see the refusal note above.
[[nodiscard]] bool parse_coord(const char* text, dfn::world::ChunkCoord& out) {
    int x = 0;
    int z = 0;
    if (text == nullptr || std::sscanf(text, "%d,%d", &x, &z) != 2) {
        return false;
    }
    out = {x, z};
    return true;
}

void usage() {
    std::fprintf(stderr,
                 "usage: dfn_bake <out.dfw> [--layout <json>] [--seed N] "
                 "[--min X,Z] [--max X,Z]\n"
                 "  run from the repo root; defaults match the shipping testbed\n");
}

void report_progress(uint32_t done, uint32_t total) {
    // One line per chunk would be thousands of lines; one line per 16 keeps a
    // long bake visibly alive without burying the result.
    if (done == total || (done % 16) == 0) {
        std::fprintf(stderr, "[bake] %u / %u chunks\n", done, total);
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 2;
    }
    const std::string out = argv[1];
    std::string layout_path = "games/daggerfall_n/assets/world/testbed_layout.json";

    dfn::world::WorldGenParams params;
    params.seed = 1u;
    params.min_chunk = {0, 0};
    // The shipping extent, so a bare `dfn_bake world.dfw` bakes the world the
    // game actually loads rather than a corner of it.
    params.max_chunk = {static_cast<int32_t>(dfn::config::WORLD_EXTENT_CHUNKS) - 1,
                        static_cast<int32_t>(dfn::config::WORLD_EXTENT_CHUNKS) - 1};

    for (int i = 2; i < argc; ++i) {
        const auto next = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "[bake] %s needs a value -- REFUSED\n", what);
                return nullptr;
            }
            return argv[++i];
        };
        if (std::strcmp(argv[i], "--layout") == 0) {
            const char* v = next("--layout");
            if (v == nullptr) { return 2; }
            layout_path = v;
        } else if (std::strcmp(argv[i], "--seed") == 0) {
            const char* v = next("--seed");
            if (v == nullptr) { return 2; }
            params.seed = std::strtoull(v, nullptr, 10);
        } else if (std::strcmp(argv[i], "--min") == 0) {
            const char* v = next("--min");
            if (v == nullptr || !parse_coord(v, params.min_chunk)) {
                std::fprintf(stderr, "[bake] --min wants X,Z -- REFUSED\n");
                return 2;
            }
        } else if (std::strcmp(argv[i], "--max") == 0) {
            const char* v = next("--max");
            if (v == nullptr || !parse_coord(v, params.max_chunk)) {
                std::fprintf(stderr, "[bake] --max wants X,Z -- REFUSED\n");
                return 2;
            }
        } else {
            std::fprintf(stderr, "[bake] unknown argument \"%s\" -- REFUSED\n", argv[i]);
            usage();
            return 2;
        }
    }

    const auto lr = dfn::world::load_layout_file(layout_path, params.layout);
    if (!lr.ok) {
        std::fprintf(stderr, "[bake] layout \"%s\": %s\n", layout_path.c_str(),
                     lr.error.c_str());
        return 1;
    }

    std::fprintf(stderr, "[bake] seed %llu  extent (%d,%d)..(%d,%d)  -> %s\n",
                 static_cast<unsigned long long>(params.seed), params.min_chunk.x,
                 params.min_chunk.z, params.max_chunk.x, params.max_chunk.z, out.c_str());

    const dfn::world::BakeReport r = dfn::world::bake_world(params, out, report_progress);
    if (!r.error.empty()) {
        std::fprintf(stderr, "[bake] FAILED: %s\n", r.error.c_str());
        return 1;
    }
    std::fprintf(stderr, "[bake] %u chunks, %.1f MB, %.1f s (%.0f ms/chunk)\n", r.chunks,
                 static_cast<double>(r.bytes) / (1024.0 * 1024.0), r.seconds,
                 r.chunks > 0 ? r.seconds * 1000.0 / r.chunks : 0.0);
    return 0;
}
