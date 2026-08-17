/*
Created: 17:08:2026 - 20:06:53
Last updated: 17:08:2026 - 20:06:53
Module: engine/app
File: engine/app/sources/EditorPlant.cpp

Responsibility:
- The judge-facing half of the planting hand, declared in EditorPlant.h.

Dependencies:
- Uses: EditorPlant.h, BuildTool.h (verdict translation), engine/world.
- Used by: App (editor mode), tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE VERDICTS BELOW ARE TRANSLATIONS, NOT DECISIONS. Every "refused" here came
  out of world::check_scene. If you catch yourself writing
  `if (on_a_path) return {false, ...}`, the rule belongs in the judge.
- ASK BY DIFFERENCE, NEVER BY INDEX ALONE — see verdict_by_difference for the
  measured defect that rule was bought with.
*/
/*
UPD:
- 17:08:2026 - 20:06:53: Создан расколом EditorBrush (слои: панель кисти обязана быть в
  engine/editor, перевод вердикта — в engine/app).
*/

#include "engine/app/sources/EditorPlant.h"

#include <algorithm>

namespace dfn::app {
namespace {

/// THE VERDICT ON A CANDIDATE, BY DIFFERENCE — the findings that appear when it
/// is added, rather than the findings that happen to carry its index.
///
/// THIS EXISTS BECAUSE INDEX-MATCHING IS NOT ENOUGH, and the gap is not
/// theoretical: world::check_scene reports NoOverlap against the EARLIER of the
/// two placements (`found.push_back({..., i, ...})` with j > i). A candidate
/// appended at the end of the document is therefore ALWAYS the `j`, and never
/// receives its own overlap finding — so a hand that asks "is there a finding
/// at my index" is told green while standing inside a tree that is already
/// there. Measured, not feared: two oaks 20 cm apart, both accepted.
///
/// Asking by DIFFERENCE has no such blind spot and needs no list of which rules
/// blame whom. Whatever the judge started saying the moment this candidate was
/// added is this candidate's doing, wherever the judge chose to hang it.
///
/// The cost is one extra judge pass, and the baseline is computed once per dab
/// rather than per candidate where the caller can arrange it.
[[nodiscard]] BuildVerdict verdict_by_difference(
    const std::vector<world::SceneFinding>& before,
    const std::vector<world::SceneFinding>& after, std::size_t candidate_index) {
    const auto same = [](const world::SceneFinding& a, const world::SceneFinding& b) {
        return a.rule == b.rule && a.placement_index == b.placement_index
            && a.object == b.object && a.detail == b.detail;
    };
    for (const world::SceneFinding& f : after) {
        // A finding naming the candidate itself is its own, always — it cannot
        // have existed before the candidate did.
        bool is_new = f.placement_index == candidate_index;
        if (!is_new) {
            is_new = std::none_of(before.begin(), before.end(),
                                  [&](const world::SceneFinding& b) { return same(f, b); });
        }
        if (is_new) {
            // RELABELLED AND HANDED TO THE ONE TRANSLATOR THERE IS. The
            // rule -> sentence mapping lives in BuildTool.cpp and is the build
            // hand's; a copy of it here would be a second answer to "why not",
            // and the two would drift the first time a rule was added — the
            // brush would show a blank line where the build hand showed a
            // reason (Rule 32). So the finding is re-addressed to the candidate
            // and translated by verdict_from_findings itself.
            world::SceneFinding relabelled = f;
            relabelled.placement_index = candidate_index;
            return verdict_from_findings({relabelled}, candidate_index);
        }
    }
    return {true, {}};
}

} // namespace

PlantDabReport plant_dab(world::SceneDoc& doc, std::span<const PlantCandidate> candidates,
                         const world::SceneWorld& world, const world::SceneLimits& limits) {
    PlantDabReport report;
    report.verdicts.reserve(candidates.size());
    for (const PlantCandidate& c : candidates) {
        // THE JUDGE ITSELF, on the composition PLUS everything this dab has
        // already planted. Appending to the live doc and taking it back on a
        // refusal (rather than judging a copy) is what makes the second
        // candidate see the first: a dab judged all at once refuses both halves
        // of every overlapping pair and hands the builder an empty click where
        // one tree was perfectly possible.
        // The baseline is taken FRESH for each candidate, because the previous
        // candidate may have been accepted and is now part of what "already
        // true" means. Judging against a stale baseline would blame this tree
        // for the one planted a moment ago.
        const std::vector<world::SceneFinding> before = world::check_scene(doc, world, limits);
        world::Placement p;
        p.object = c.object;
        p.position = c.position;
        p.yaw = c.yaw;
        p.scale = c.scale;
        doc.placements.push_back(p);
        const std::size_t index = doc.placements.size() - 1;
        const BuildVerdict v =
            verdict_by_difference(before, world::check_scene(doc, world, limits), index);
        report.verdicts.push_back(v);
        if (v.allowed) {
            ++report.planted;
        } else {
            doc.placements.pop_back();
            ++report.refused;
        }
    }
    return report;
}

BuildVerdict edit_placement(world::SceneDoc& doc, std::size_t index, const PlantParams& to,
                            const world::SceneWorld& world,
                            const world::SceneLimits& limits) {
    if (index >= doc.placements.size()) {
        return {false, "build.no.unknown"};
    }
    const world::Placement before = doc.placements[index];
    // What the judge already said about this composition WITH the placement as
    // it stands. Editing does not append, so an old problem elsewhere keeps its
    // index and cancels out — and an overlap the EDIT creates is caught even
    // when the judge hangs it on the other party (see verdict_by_difference).
    const std::vector<world::SceneFinding> baseline =
        world::check_scene(doc, world, limits);
    world::Placement& p = doc.placements[index];
    if (!to.object.empty()) {
        p.object = to.object;
    }
    if (to.set_yaw) {
        p.yaw = to.yaw;
    }
    if (to.set_scale) {
        p.scale = to.scale;
    }
    const BuildVerdict v =
        verdict_by_difference(baseline, world::check_scene(doc, world, limits), index);
    if (!v.allowed) {
        // PUT IT BACK, EXACTLY. An editor that applies a change the judge just
        // refused has taught the builder that red is decoration, and from that
        // moment on it is: he stops reading it, and the next red one is the one
        // that mattered.
        doc.placements[index] = before;
    }
    return v;
}

} // namespace dfn::app
