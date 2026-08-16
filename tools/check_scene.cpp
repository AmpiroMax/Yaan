/*
Created: 15:08:2026 - 16:24:04
Last updated: 16:08:2026 - 21:50:43
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
- 16:08:2026 - 21:08:52: Мерка детали расширена до верха и следа (object_top/object_box) — из той
  же сетки, что и всё остальное: высота опоры, вписанная руками, протухает
  первой при перековке детали. И --ground <x> <z> [<пролёт>]: строителю нужна
  земля ДО того, как он напишет, где лежит подошва, а --fix ему не поможет — он
  членов групп не двигает.
- 16:08:2026 - 21:50:43: --objects принимает список полок через ';' — судья обязан видеть ровно
  те же полки, что и игра, иначе он судит другую сцену.
*/

#include "engine/render/sources/ObjectRegistry.h"
#include "engine/world/sources/LayoutLoad.h"
#include "engine/world/sources/Scene.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/world/sources/WorldgenForest.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <glm/common.hpp>
#include <map>
#include <optional>
#include <vector>
#include <string>

namespace {

struct Ctx {
    const dfn::world::WorldGenContext* gen = nullptr;
    /// Shelves, in the order the caller gave them; first hit wins. Several,
    /// because a town scene stands on parts, props and trees at once.
    std::vector<std::filesystem::path> shelves;
    /// name -> (radius, bottom, top). Memoised: a scene of 200 trees — or of
    /// 200 identical beams in one house — would otherwise read and hash the
    /// same .dfo two hundred times.
    struct Extent {
        float radius = 0.0f;
        float bottom = 0.0f;
        float top = 0.0f;
        glm::vec2 lo{0.0f};   ///< local xz footprint, about the origin
        glm::vec2 hi{0.0f};
    };
    std::map<std::string, Extent> extents;
};

float ground_at(void* ctx, glm::vec2 p) {
    const auto* c = static_cast<Ctx*>(ctx);
    return dfn::world::terrain_height(*c->gen, p);
}

const Ctx::Extent* measure(Ctx* c, const std::string& name) {
    if (const auto it = c->extents.find(name); it != c->extents.end()) {
        return &it->second;
    }
    std::optional<dfn::render::RegistryObject> obj;
    for (const auto& shelf : c->shelves) {
        obj = dfn::render::read_object(shelf / (name + ".dfo"));
        if (obj) {
            break;
        }
    }
    if (!obj) {
        return nullptr;
    }
    Ctx::Extent e;
    const auto scan = [&](const dfn::render::MeshData& mesh) {
        for (const dfn::platform::Vertex& v : mesh.vertices) {
            e.radius = std::max(e.radius, std::sqrt(v.position.x * v.position.x
                                                    + v.position.z * v.position.z));
            e.bottom = std::min(e.bottom, v.position.y);
            e.top = std::max(e.top, v.position.y);
            e.lo = glm::min(e.lo, glm::vec2{v.position.x, v.position.z});
            e.hi = glm::max(e.hi, glm::vec2{v.position.x, v.position.z});
        }
    };
    scan(obj->wood);
    scan(obj->cards);
    scan(obj->ground);
    scan(obj->bark);
    return &c->extents.emplace(name, e).first->second;
}

bool object_extent(void* ctx, const std::string& name, float& radius, float& bottom) {
    const Ctx::Extent* e = measure(static_cast<Ctx*>(ctx), name);
    if (e == nullptr) {
        return false;
    }
    radius = e->radius;
    bottom = e->bottom;
    return true;
}

/// The object's footprint about its own origin. A building part's origin is at
/// one END of it, so this is the only measurement that tells the truth about
/// where the part actually sits.
bool object_box(void* ctx, const std::string& name, glm::vec2& lo, glm::vec2& hi) {
    const Ctx::Extent* e = measure(static_cast<Ctx*>(ctx), name);
    if (e == nullptr) {
        return false;
    }
    lo = e->lo;
    hi = e->hi;
    return true;
}

/// How tall the object stands above its own origin — what another part rests
/// on. Measured from the same mesh as everything else: a support height typed
/// in by hand is the first thing to go stale when a part is re-forged.
bool object_top(void* ctx, const std::string& name, float& top) {
    const Ctx::Extent* e = measure(static_cast<Ctx*>(ctx), name);
    if (e == nullptr) {
        return false;
    }
    top = e->top;
    return true;
}

} // namespace

int main(int argc, char** argv) {
    using namespace dfn::world;
    if (argc < 2) {
        std::fprintf(stderr, "usage: dfn_scene_check <file.scene> [--stand <id>] "
                             "[--objects <dir>[;<dir>...]] [--fix]\n"
                             "       dfn_scene_check - --ground <x> <z> [<span>] "
                             "[--stand <id>]\n");
        return 2;
    }
    const std::filesystem::path scene_path = argv[1];
    std::string stand = "Gallery";
    std::string objects_dir = "assets/objects/trees";
    bool fix = false;
    bool ground_query = false;
    float query_x = 0.0f;
    float query_z = 0.0f;
    float query_span = 8.0f;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fix") == 0) {
            fix = true;
        } else if (std::strcmp(argv[i], "--ground") == 0 && i + 2 < argc) {
            ground_query = true;
            query_x = std::strtof(argv[++i], nullptr);
            query_z = std::strtof(argv[++i], nullptr);
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                query_span = std::strtof(argv[++i], nullptr);
            }
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
    if (!ground_query && !read_scene(scene_path, doc, error)) {
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

    // ASKING THE WORLD A QUESTION, before there is a scene to check. A builder
    // needs to know the ground before he can write down where a footing goes,
    // and the alternative — placing at a guessed height and letting --fix
    // correct it — cannot work for a house, whose parts rest on each other and
    // are never sat down by the fixer.
    if (ground_query) {
        float lo = 1e9f;
        float hi = -1e9f;
        constexpr int STEPS = 16;
        for (int iz = 0; iz <= STEPS; ++iz) {
            for (int ix = 0; ix <= STEPS; ++ix) {
                const glm::vec2 at{
                    query_x + query_span * (static_cast<float>(ix) / STEPS - 0.5f),
                    query_z + query_span * (static_cast<float>(iz) / STEPS - 0.5f)};
                const float h = terrain_height(gen, at);
                lo = std::min(lo, h);
                hi = std::max(hi, h);
            }
        }
        std::printf("[ground] (%.2f, %.2f) = %.3f m; over %.1f m: %.3f..%.3f "
                    "(spread %.3f m)\n", static_cast<double>(query_x),
                    static_cast<double>(query_z),
                    static_cast<double>(terrain_height(gen, {query_x, query_z})),
                    static_cast<double>(query_span), static_cast<double>(lo),
                    static_cast<double>(hi), static_cast<double>(hi - lo));
        return 0;
    }


    Ctx ctx;
    ctx.gen = &gen;
    for (std::size_t at = 0; at <= objects_dir.size();) {
        const std::size_t sep = objects_dir.find(';', at);
        const std::size_t end = sep == std::string::npos ? objects_dir.size() : sep;
        std::string one = objects_dir.substr(at, end - at);
        while (!one.empty() && (one.front() == ' ' || one.front() == '\t')) {
            one.erase(one.begin());
        }
        while (!one.empty() && (one.back() == ' ' || one.back() == '\t')) {
            one.pop_back();
        }
        if (!one.empty()) {
            ctx.shelves.emplace_back(one);
        }
        if (sep == std::string::npos) {
            break;
        }
        at = sep + 1;
    }
    SceneWorld world;
    world.ground_at = &ground_at;
    world.object_extent = &object_extent;
    world.object_top = &object_top;
    world.object_box = &object_box;
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
