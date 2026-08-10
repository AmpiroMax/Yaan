/*
Created: 10:08:2026 - 11:19:15
Last updated: 10:08:2026 - 11:19:15
Module: engine/world
File: engine/world/sources/WorldgenVantages.h

Responsibility:
- WHERE THE §8.1 FOREST STAND'S EVIDENCE IS. The stand knows which of its own
  ground can prove or disprove each claim it makes; render owns the Tour and
  cannot see `dfn::world`. This is the query that closes that gap, so a stand
  that loads and walks can also be PHOTOGRAPHED.

Key items:
- forest_vantages(): the stand's acceptance standpoints, controls included.

Dependencies:
- Uses: core/math/StandVantage.h, TestbedLayout.h, WorldgenPaths.h,
  WorldgenFinds.h.
- Used by: ChunkManager::stand_vantages (the render handoff), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- RULE 27: A VANTAGE THAT CANNOT FAIL IS NOT EVIDENCE. Every claim-carrying
  standpoint below ships with the standpoint that would look the same if the
  claim were false — the flat glade beside the swale floor, the goal-visible
  station beside the goal-hidden one. If you add a vantage, add its control in
  the same commit or do not add it.
- DETERMINISM (Rule 13.1): pure function of the inputs. The searches below scan
  a fixed lattice in a fixed order and break ties on the FIRST hit, because
  "the best of several equal candidates" chosen by container order is not
  reproducible.
*/
/*
UPD:
- 10:08:2026 - 11:19:15: Created — the stand loads and the bot walks it, but
  the tour's vantages were the testbed's, so a tour on the forest stand shot
  one frame and stopped.
*/

#pragma once

#include "engine/core/math/sources/StandVantage.h"
#include "engine/world/sources/TestbedLayout.h"
#include "engine/world/sources/WorldgenFinds.h"
#include "engine/world/sources/WorldgenPaths.h"

#include <cstdint>
#include <vector>

namespace dfn::world {

/// The §8.1 stand's acceptance standpoints, in a fixed order:
///
///   path_along_<class>       ON the tread, aimed down the longest straight
///                            run of that class — BR-3's rich edge is a GROUND
///                            feature, so these are pitched down far enough to
///                            put both margins in frame. One per class the
///                            network actually built; the SET is the evidence
///                            for design's maintenance scoping (a cobbled
///                            street with a suppressed edge peak is a PASS),
///                            because richness that orders by class is a claim
///                            a single frame cannot make.
///   goal_<kind>              short of each goal on the route that reaches it
///                            — BR-2 clause (i) made visible: every route ends
///                            at something.
///   br1_hidden_r<N> +        THE PAIR. Same route, same goal, matched range;
///   br1_visible_control_r<N> the goal is absent from one frame and present in
///                            the other, and nothing else differs.
///   lf2_swale_floor          on a swale floor, aimed ALONG it (the channel
///                            fog pools in — W5).
///   lf2_crest                on a grive crest, aimed ACROSS the grain, so
///                            successive ridge-and-swale reads as rhythm.
///   lf2_glade_control        THE CONTROL for both: в9's authored calm plain,
///                            same pitch and same compass bearing as the swale
///                            frame. If the LF-2 frames look like this one, the
///                            landform is not there.
///   find_<regime>            a find of each BR-6 regime at approach range.
///
/// Empty when the layout declares no paths and no finds. Vantages whose
/// subject does not exist are OMITTED rather than emitted at a fallback
/// position: a frame aimed at nothing is worse than a missing frame, because
/// it gets archived and cited.
[[nodiscard]] std::vector<math::StandVantage> forest_vantages(
    uint64_t seed, const TestbedLayout& layout, const PathNetwork& net,
    const std::vector<Find>& finds);

} // namespace dfn::world
