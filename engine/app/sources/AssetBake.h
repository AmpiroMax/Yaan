/*
Created: 17:08:2026 - 14:43:34
Last updated: 17:08:2026 - 14:43:34
Module: engine/app
File: engine/app/sources/AssetBake.h

Responsibility:
- FIRST-RUN PREPARATION: bakes the registry shelves the game reads, when they
  are missing, and reports progress so the app can draw a bar while it happens.

Key items:
- BakeStep / BakePlan: what has to be made, and how much of it is left.
- plan_asset_bake(): what is missing right now.
- run_asset_bake(): makes it, calling back once per object.

WHY THIS EXISTS (user, 17.08.2026): «в гит результаты работы кода не сохраняем /
я же если друзьям буду код давать запуска, они не будут со всеми ассетами его
получать, только с чем-то / а шейдеры компилить и ассеты запекать они при первом
запуске будут / и должны при загрузке игры видеть соответствующее сообщение и
полоску загрузки что m из n шейдеров / ассетов готовы».

The registry was 210 MB of .dfo in git, every byte of it a DETERMINISTIC OUTPUT
of a forge that also lives in this repository. Storing it was paying repository
weight — forever, because history does not shrink — for something the code can
produce in seconds. A clone should carry the code and the compositions; the
objects are made on arrival.

Dependencies:
- Uses: engine/render (PartForge, ObjectRegistry), std::filesystem.
- Used by: engine/app (App::init, before the menu opens).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- MISSING IS NOT AN ERROR, it is the first run. But a shelf that FAILS to bake
  is an error and must say so out loud: a silently empty shelf becomes "the
  houses disappeared" three sessions later.
- The plan is computed from the FILESYSTEM, never remembered: a half-finished
  bake killed by a crash must be finishable by running again.
*/
/*
UPD:
- 17:08:2026 - 14:43:34: Создан — печь ассетов при первом запуске (см. выше).
*/

#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace dfn::app {

/// One shelf that has to exist before the game can read it.
struct BakeStep {
    std::string name;       ///< human name for the progress line ("детали домов")
    std::string directory;  ///< where the .dfo go
    std::size_t count = 0;  ///< how many objects this step will write
};

/// Everything missing right now. Empty = nothing to do, which is the normal
/// state of every run after the first.
struct BakePlan {
    std::vector<BakeStep> steps;
    std::size_t total = 0;  ///< objects across all steps
    [[nodiscard]] bool empty() const { return steps.empty(); }
};

/// What is missing. Cheap: counts files, forges nothing.
[[nodiscard]] BakePlan plan_asset_bake();

/// Makes everything the plan lists. `on_progress(done, total, what)` is called
/// after each object so the caller can draw; it must be cheap and must not
/// throw. Returns false if any object could not be written (and says why on
/// stderr) — a shelf that half exists is worse than one that does not.
bool run_asset_bake(const BakePlan& plan,
                    const std::function<void(std::size_t, std::size_t,
                                             const std::string&)>& on_progress);

} // namespace dfn::app
