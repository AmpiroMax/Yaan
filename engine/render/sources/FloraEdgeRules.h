/*
Created: 10:08:2026 - 02:36:59
Last updated: 10:08:2026 - 12:05:00
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
/*
UPD:
- 10:08:2026 - 02:36:59: Created — edge set + water margin + talus apron rows,
  with the jewel budget flag and the shade-side moss association.
- 10:08:2026 - 02:49:15: Design's asks: the band DATUM is named (0 = the worn
  edge of the trodden surface, outward — never the centreline), and the
  krummholz flag-direction wind note is recorded at the row.
- 10:08:2026 - 11:07:33: Design's maintenance ruling: PathClassRichness per
  row (hint >= dirt > cobble; moss in stair joints, flowers never). The weight
  scales the edge PEAK, not the base presence — a kept verge is not bare
  ground. BR-3's ratio scoped to the unmaintained classes.
- 10:08:2026 - 11:24:00: per_100m's dimension made unambiguous — a TOTAL
  COUNT across the band, with the normalisation the ramp requires, after core
  asked whether it was a peak or a mean density (it is neither).
- 10:08:2026 - 12:05:00: CONTENTS MOVED to engine/core/math/sources/FloraEdgeRules.h,
  re-keyed on math::ScatterSpecies. This is the migration flora's own ordinal
  note asked for: in core both PathClass and PathClassRichness are visible, so
  the positional coupling is checkable instead of merely described.
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
