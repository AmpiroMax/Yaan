/*
Created: 17:08:2026 - 14:43:34
Last updated: 17:08:2026 - 15:00:28
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
- 17:08:2026 - 15:00:28: печь табличек через render::read_signs_file — ТУ ЖЕ функцию, которую
  зовёт dfn_signs (зона домов вынесла её из инструмента в библиотеку по этой
  просьбе). Печёт ПЛОСКИМИ, как и весь каталог, пока лист набора не привязан:
  текстурная табличка среди нетекстурных домов — не предпросмотр ничего.
*/

#include "engine/app/sources/AssetBake.h"

#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/PartForge.h"
#include "engine/render/sources/SignForge.h"

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
constexpr const char* SIGNS_DIR = "assets/objects/signs";
constexpr const char* SIGNS_SRC = "assets/signs";

/// Every sign every .signs file asks for. The TEXT is content in git; the
/// baked object is not — so this reads the sources and forges what they name.
[[nodiscard]] std::vector<render::SignParams> planned_signs() {
    std::vector<render::SignParams> all;
    std::error_code ec;
    if (!fs::is_directory(SIGNS_SRC, ec)) {
        return all;
    }
    for (const auto& e : fs::directory_iterator(SIGNS_SRC, ec)) {
        if (e.path().extension() != ".signs") {
            continue;
        }
        std::vector<render::SignParams> one;
        if (!render::read_signs_file(e.path(), one)) {
            // The reader already said what it did not understand. A sign file
            // with a typo must not silently produce a shorter shelf.
            std::fprintf(stderr, "[bake] %s разобран не полностью\n",
                         e.path().string().c_str());
        }
        for (render::SignParams& p : one) {
            // FLAT, like the rest of the kit, until the parts sheet is bound.
            // A textured plaque among untextured houses is not a preview of
            // anything (Rule 47's control arm, and the user's leaf houses).
            p.textured = false;
            all.push_back(std::move(p));
        }
    }
    return all;
}

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
    const std::size_t want_signs = planned_signs().size();
    if (want_signs > 0 && dfo_count(SIGNS_DIR) < want_signs) {
        plan.steps.push_back({"таблички", SIGNS_DIR, want_signs});
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
        if (step.directory == SIGNS_DIR) {
            for (const render::SignParams& p : planned_signs()) {
                const render::RegistryObject obj = render::forge_sign(p);
                const fs::path path = fs::path(step.directory) / (obj.name + ".dfo");
                if (!fs::exists(path) && !render::write_object(obj, path)) {
                    std::fprintf(stderr, "[bake] не могу записать %s -- ОТКАЗ\n",
                                 path.string().c_str());
                    ok = false;
                }
                ++done;
                on_progress(done, plan.total, step.name);
            }
            continue;
        }
        if (step.directory != PARTS_DIR) {
            continue; // shelves whose catalogue still lives in tools/
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
