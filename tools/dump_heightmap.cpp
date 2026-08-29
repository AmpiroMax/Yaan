/*
Module: tools
File: tools/dump_heightmap.cpp

Responsibility:
- Dump a square of the shipped world's terrain height as float32, row major, so
  the SAME instruments can be run on our ground as on a real lidar DEM
  (tools/measure_terrain_stats.py, tools/measure_occlusion_real.py).

Usage:
    x0 z0 side_m step_m out.f32
  Not registered with CMake on purpose (like tools/measure_shadow_jitter.cpp) --
  it is a measuring instrument, not part of the game. Build it against an
  already configured tree:

    clang++ -std=c++23 -O2 -I . -I build_core/dfn_generated \
        -isystem build_core/_deps/glm-src tools/dump_heightmap.cpp \
        build_core/engine/world/libdfn_world.a build_core/engine/core/libdfn_core.a \
        -o <scratch>/dump_heightmap

  Run it from the REPO ROOT: it loads the shipped testbed layout asset by
  relative path, exactly as the tests do, so the world it dumps is the world the
  tests measure. The pass's own doors work here unchanged --
  DFN_TERRACE_STRENGTH=0 DFN_DRAW_DEPTH=0 gives the control arm through the same
  code path rather than around it (Rule 27, Rule 30).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/LayoutLoad.h"
#include "engine/world/sources/Worldgen.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace dfn;

int main(int argc, char** argv) {
    if (argc < 6) {
        std::fprintf(stderr, "usage: %s x0 z0 side_m step_m out.f32\n", argv[0]);
        return 2;
    }
    const float x0 = std::strtof(argv[1], nullptr);
    const float z0 = std::strtof(argv[2], nullptr);
    const float side = std::strtof(argv[3], nullptr);
    const float step = std::strtof(argv[4], nullptr);

    world::WorldGenParams p;
    p.seed = 1;
    p.min_chunk = {0, 0};
    p.max_chunk = {static_cast<int>(config::WORLD_EXTENT_CHUNKS) - 1,
                   static_cast<int>(config::WORLD_EXTENT_CHUNKS) - 1};
    const auto lr =
        world::load_layout_file("games/daggerfall_n/assets/world/testbed_layout.json", p.layout);
    if (!lr.ok) {
        std::fprintf(stderr, "layout load failed (run from repo root)\n");
        return 1;
    }
    const world::WorldGenContext ctx = world::build_world_context(p);

    const int n = static_cast<int>(side / step);
    std::vector<float> h(static_cast<size_t>(n) * n);
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < n; ++i) {
            h[static_cast<size_t>(j) * n + i] =
                world::terrain_height(ctx, {x0 + i * step, z0 + j * step});
        }
    }
    std::FILE* f = std::fopen(argv[5], "wb");
    if (!f) return 1;
    std::fwrite(h.data(), sizeof(float), h.size(), f);
    std::fclose(f);
    std::fprintf(stderr, "%d x %d at %.2f m -> %s\n", n, n, step, argv[5]);
    return 0;
}
