/*
Module: engine/render
File: engine/render/sources/FloraEdgeRules.h

Responsibility:
- FORWARDING HEADER ONLY. The rich-edge table moved to core's zone
  (engine/core/math/sources/FloraEdgeRules.h) the day WorldgenScatter placed
  from it: the table is PLACEMENT data and `world` may not include `render`.
  Flora authored it and the rows are unchanged; core reviewed and now owns it.

Key items:
- `using` declarations importing dfn::math's names into dfn::render.

Dependencies:
- Uses: engine/core/math/sources/FloraEdgeRules.h.
- Used by: flora's suite and any render-side reader of the rows.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ADD NO ROWS HERE. A second table is a second answer to "what grows on a path
  margin", and the two would diverge silently — core would place from one and
  flora would document the other.
- The rows are keyed on math::ScatterSpecies now, not render::FloraSpecies.
  render::flora_species_of() maps the ordinal to a mesh, as for every other
  scattered instance.
- This file is a TRANSITION and may be deleted once flora's includes point at
  engine/core/math/sources/FloraEdgeRules.h directly.
*/

#pragma once

#include "engine/core/math/sources/FloraEdgeRules.h"

namespace dfn::render {

using math::EdgeAssociation;
using math::EdgeHabitat;
using math::edge_band_integral;
using math::FLORA_EDGE_RULE_COUNT;
using math::FLORA_EDGE_RULES;
using math::FloraEdgeRule;
using math::PathClassRichness;
using math::RICHNESS_FLOWER;
using math::RICHNESS_IRRELEVANT;
using math::RICHNESS_MOSS;
using math::RICHNESS_MUSHROOM;
using math::RICHNESS_PEBBLE;

} // namespace dfn::render
