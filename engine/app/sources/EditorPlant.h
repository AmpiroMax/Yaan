/*
Created: 17:08:2026 - 20:06:53
Last updated: 17:08:2026 - 20:06:53
Module: engine/app
File: engine/app/sources/EditorPlant.h

Responsibility:
- WHETHER A PLANT MAY STAND WHERE THE BRUSH WANTS IT. The judge-facing half of
  the planting hand: it plants a dab, edits what is already standing, and both
  times the answer comes from world::check_scene and from nowhere else.

Key items:
- PlantDabReport / plant_dab(): a whole dab, each candidate judged against the
  ones already accepted from it.
- PlantParams / edit_placement(): change what stands, and put it back on a
  refusal.

WHY IT IS A SEPARATE FILE FROM EditorBrush, and the reason is a layer rather
than a taste: the brush panel must live in engine/editor (Dear ImGui is allowed
there and nowhere else), so the brush's settings and mechanics had to go there
with it. But the WORDING of a refusal belongs to BuildTool, BuildTool lives in
engine/app, and engine/editor may not look upward (tools/dag_check.py). One of
the two had to be split, and this is the honest seam: WHERE a dab wants to
plant is the brush's question, WHETHER it may is this file's.

The alternative was a second copy of the rule -> sentence table down in the
editor layer, which is precisely the thing worth more than a seam: two answers
to "why not" drift the first time a rule is added, and the brush would show a
blank line where the build hand showed a reason (Rule 32).

Dependencies:
- Uses: engine/editor (EditorBrush: PlantCandidate), BuildTool (the verdict
  translation — ONE definition, two hands), engine/world (Scene, check_scene).
- Used by: engine/app (App, editor mode), tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NEVER answer "allowed" from a rule written in this file. Every refusal here
  came out of the judge; a new kind of refusal belongs in the judge, where the
  tools and the agents see it too.
*/
/*
UPD:
- 17:08:2026 - 20:06:53: Создан расколом EditorBrush — судейская половина посадки.
  Раскол вынужден слоями: панель кисти обязана жить в engine/editor (только там
  разрешён Dear ImGui), а перевод вердикта живёт в BuildTool, который выше.
*/

#pragma once

#include "engine/app/sources/BuildTool.h"
#include "engine/editor/sources/EditorBrush.h"
#include "engine/world/sources/Scene.h"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace dfn::app {

/// What a dab of planting did.
struct PlantDabReport {
    /// One verdict per candidate, in order. A refused candidate carries the
    /// judge's reason key, so the tool can say WHY nothing appeared where the
    /// builder clicked — silence there is the failure mode that makes a tool
    /// feel broken when it is merely being correct.
    std::vector<BuildVerdict> verdicts;
    int planted = 0;
    int refused = 0;
};

/// Plants a whole dab into `doc`, judging every candidate with `world`'s rules.
///
/// EACH CANDIDATE IS JUDGED AGAINST THE DOC INCLUDING THE ONES ALREADY ACCEPTED
/// FROM THIS SAME DAB, which is why this is a loop of judge passes and not one
/// pass over all of them. Judged all at once, two candidates that overlap each
/// other are both refused and the builder gets an empty click where one tree
/// was perfectly possible; judged in sequence, the first stands and the second
/// steps aside. The cost is one pass per candidate, and a dab is single digits.
///
/// Refused candidates change nothing: `doc` receives exactly the accepted ones.
[[nodiscard]] PlantDabReport plant_dab(world::SceneDoc& doc,
                                       std::span<const PlantCandidate> candidates,
                                       const world::SceneWorld& world,
                                       const world::SceneLimits& limits = {});

/// The parameters of something already standing, for editing it in place.
struct PlantParams {
    std::string object; ///< empty = keep what is there
    float yaw = 0.0f;
    float scale = 1.0f;
    bool set_yaw = false;
    bool set_scale = false;
};

/// Changes a placed thing and RE-JUDGES it. On a refusal `doc` is left exactly
/// as it was: an editor that applies what the judge forbids has taught the
/// builder that red means nothing, and from then on it means nothing.
[[nodiscard]] BuildVerdict edit_placement(world::SceneDoc& doc, std::size_t index,
                                          const PlantParams& to,
                                          const world::SceneWorld& world,
                                          const world::SceneLimits& limits = {});

} // namespace dfn::app
