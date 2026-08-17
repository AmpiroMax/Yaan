/*
Created: 17:08:2026 - 14:43:34
Last updated: 17:08:2026 - 14:43:34
Module: engine/app
File: engine/app/sources/AssetBake.cpp

Responsibility:
- The first-run bake declared in AssetBake.h.

Dependencies:
- Uses: AssetBake.h, engine/render (PartForge, ObjectRegistry).
- Used by: App::init.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ONE DEFINITION OF WHAT THE SHELF CONTAINS. The catalogue comes from
  kit_catalogue() — the same call the dfn_kit tool makes — so the shelf the
  game bakes on first run and the shelf the tool bakes are the same shelf. A
  second catalogue here would drift, and the drift would show up as a house
  missing one part in one of the two paths.
*/
/*
UPD:
- 17:08:2026 - 14:43:34: Создан вместе с AssetBake.h.
*/

#include "engine/app/sources/AssetBake.h"

#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/PartForge.h"

#include <cstdio>
#include <filesystem>

namespace dfn::app {
namespace {

namespace fs = std::filesystem;

/// How many .dfo a directory already holds. A missing directory answers 0,
/// which is the first run and not a failure.
[[nodiscard]] std::size_t dfo_count(const fs::path& dir) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return 0;
    }
    std::size_t n = 0;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (e.path().extension() == ".dfo") {
            ++n;
        }
    }
    return n;
}

constexpr const char* PARTS_DIR = "assets/objects/parts";

} // namespace

BakePlan plan_asset_bake() {
    BakePlan plan;
    // THE PLAN IS READ FROM THE FILESYSTEM, never from a stamp file. A bake
    // killed halfway must be finishable by running again, and a stamp would
    // claim the shelf was done while half of it was missing.
    const std::size_t want_parts = render::kit_catalogue().size();
    if (dfo_count(PARTS_DIR) < want_parts) {
        plan.steps.push_back({"детали домов", PARTS_DIR, want_parts});
    }
    for (const BakeStep& s : plan.steps) {
        plan.total += s.count;
    }
    return plan;
}

bool run_asset_bake(const BakePlan& plan,
                    const std::function<void(std::size_t, std::size_t,
                                             const std::string&)>& on_progress) {
    std::size_t done = 0;
    bool ok = true;
    for (const BakeStep& step : plan.steps) {
        std::error_code ec;
        fs::create_directories(step.directory, ec);
        if (ec) {
            std::fprintf(stderr, "[bake] не могу создать %s: %s\n",
                         step.directory.c_str(), ec.message().c_str());
            return false;
        }
        if (step.directory != PARTS_DIR) {
            continue; // only shelf wired so far; see the header's note
        }
        for (const render::PartParams& p : render::kit_catalogue()) {
            const render::RegistryObject obj = render::forge_part(p);
            const fs::path path = fs::path(step.directory) / (obj.name + ".dfo");
            // ALREADY THERE = ALREADY DONE. This is what makes a half-finished
            // bake resumable, and it is why the plan counts files rather than
            // trusting a marker.
            if (!fs::exists(path) && !render::write_object(obj, path)) {
                std::fprintf(stderr, "[bake] не могу записать %s -- ОТКАЗ\n",
                             path.string().c_str());
                ok = false;
            }
            ++done;
            on_progress(done, plan.total, step.name);
        }
    }
    return ok;
}

} // namespace dfn::app
