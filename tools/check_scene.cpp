/*
Created: 15:08:2026 - 16:24:04
Last updated: 15:08:2026 - 16:24:04
Module: tools
File: tools/check_scene.cpp

Responsibility:
- dfn_scene_check: judges a composed map (.scene) against the rules in
  engine/world/sources/Scene.h, measuring against the REAL generated ground
  and the REAL registry objects. Exit code 1 when anything is wrong, so it can
  stand in a build or a hook.

Usage:
    dfn_scene_check <file.scene> [--stand Testbed|Forest|OneTree|Gallery]
                                 [--objects <dir>] [--fix]

  Run from the repo ROOT (it opens the shipped layout asset by relative path,
  exactly as the app and the tests do).

Dependencies:
- Uses: engine/world (Scene, Worldgen, layout), engine/render (ObjectRegistry).
- Used by: agents composing maps, the human, and whoever wires it into CI.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- MEASURE, NEVER GUESS: the ground comes from the generator and the object
  extents from the registry file. A checker that assumed a tree is "about 5 m"
  would pass a colossus and fail a shrub, and its report would be worth
  nothing.
- --fix moves objects onto the ground and NOTHING else, and it says how many
  it moved. Anything cleverer belongs in a tool a human watches.
*/
/*
UPD:
- 15:08:2026 - 16:24:04: Created — the composition tool's first half (the
  rules), per the user's ask for an application where agents and humans lay
  out the world by its own rules.
*/

#include "engine/render/sources/ObjectRegistry.h"
#include "engine/world/sources/LayoutLoad.h"
#include "engine/world/sources/Scene.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/world/sources/WorldgenForest.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>

namespace {

struct Ctx {
    const dfn::world::WorldGenContext* gen = nullptr;
    std::filesystem::path objects_dir;
    /// name -> (radius, bottom). Memoised: a scene of 200 trees would
    /// otherwise read and hash the same .dfo two hundred times.
    std::map<std::string, std::pair<float, float>> extents;
};

float ground_at(void* ctx, glm::vec2 p) {
    const auto* c = static_cast<Ctx*>(ctx);
    return dfn::world::terrain_height(*c->gen, p);
}

bool object_extent(void* ctx, const std::string& name, float& radius, float& bottom) {
    auto* c = static_cast<Ctx*>(ctx);
    if (const auto it = c->extents.find(name); it != c->extents.end()) {
        radius = it->second.first;
        bottom = it->second.second;
        return true;
    }
    const auto obj = dfn::render::read_object(c->objects_dir / (name + ".dfo"));
    if (!obj) {
        return false;
    }
    float r = 0.0f;
    float b = 0.0f;
    const auto scan = [&](const dfn::render::MeshData& mesh) {
        for (const dfn::platform::Vertex& v : mesh.vertices) {
            r = std::max(r, std::sqrt(v.position.x * v.position.x
                                      + v.position.z * v.position.z));
            b = std::min(b, v.position.y);
        }
    };
    scan(obj->wood);
    scan(obj->cards);
    scan(obj->ground);
    scan(obj->bark);
    c->extents.emplace(name, std::make_pair(r, b));
    radius = r;
    bottom = b;
    return true;
}

} // namespace

int main(int argc, char** argv) {
    using namespace dfn::world;
    if (argc < 2) {
        std::fprintf(stderr, "usage: dfn_scene_check <file.scene> [--stand <id>] "
                             "[--objects <dir>] [--fix]\n");
        return 2;
    }
    const std::filesystem::path scene_path = argv[1];
    std::string stand = "Gallery";
    std::filesystem::path objects_dir = "assets/objects/trees";
    bool fix = false;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fix") == 0) {
            fix = true;
        } else if (std::strcmp(argv[i], "--stand") == 0 && i + 1 < argc) {
            stand = argv[++i];
        } else if (std::strcmp(argv[i], "--objects") == 0 && i + 1 < argc) {
            objects_dir = argv[++i];
        } else {
            std::fprintf(stderr, "[scene] unknown argument \"%s\" -- REFUSED\n", argv[i]);
            return 2;
        }
    }

    SceneDoc doc;
    std::string error;
    if (!read_scene(scene_path, doc, error)) {
        std::fprintf(stderr, "[scene] %s: %s\n", scene_path.string().c_str(),
                     error.c_str());
        return 1;
    }

    WorldGenParams params;
    params.seed = 1;
    params.min_chunk = {0, 0};
    params.max_chunk = {7, 7};
    const auto lr = load_layout_file("games/daggerfall_n/assets/world/testbed_layout.json",
                                     params.layout);
    if (!lr.ok) {
        std::fprintf(stderr, "[scene] layout: %s\n", lr.error.c_str());
        return 1;
    }
    if (stand == "Forest") {
        params.layout = forest_stand_layout();
    } else if (stand == "OneTree") {
        params.layout = one_tree_stand_layout();
    } else if (stand == "Gallery") {
        params.layout = gallery_stand_layout();
    } else if (stand != "Testbed") {
        std::fprintf(stderr, "[scene] unknown stand \"%s\" -- REFUSED\n", stand.c_str());
        return 2;
    }
    const WorldGenContext gen = build_world_context(params);

    Ctx ctx;
    ctx.gen = &gen;
    ctx.objects_dir = objects_dir;
    SceneWorld world;
    world.ground_at = &ground_at;
    world.object_extent = &object_extent;
    world.ctx = &ctx;

    if (fix) {
        const std::size_t moved = fix_scene_ground(doc, world);
        if (moved > 0 && !write_scene(doc, scene_path)) {
            std::fprintf(stderr, "[scene] could not write %s\n",
                         scene_path.string().c_str());
            return 1;
        }
        std::printf("[scene] sat %zu placement(s) back on the ground\n", moved);
    }

    const auto findings = check_scene(doc, world);
    for (const SceneFinding& f : findings) {
        std::printf("%s\n", describe(f).c_str());
    }
    std::printf("[scene] %s: %zu placement(s), %zu finding(s)\n",
                scene_path.filename().string().c_str(), doc.placements.size(),
                findings.size());
    return findings.empty() ? 0 : 1;
}
