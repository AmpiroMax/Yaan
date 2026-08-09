/*
Created: 10:08:2026 - 02:36:59
Last updated: 10:08:2026 - 02:49:15
Module: engine/render
File: engine/render/sources/FloraEdgeRules.h

Responsibility:
- THE RICH EDGE SET AS DATA (в8/в19в; research §A6.3 «обочина — самая богатая
  полоса» and §A7's associative grammar). One row per (species, habitat): the
  lateral band, the linear density, the clump class the density multiplies by,
  and the associations. Built BEFORE paths exist so the day core's path
  generator lands, the edges are ready.

Key items:
- EdgeHabitat, FloraEdgeRule, FLORA_EDGE_RULES, TALUS_APRON_NOTE.

Dependencies:
- Uses: FloraSpecies.h, FloraField.h (ClumpClass).
- Used by: core's placement passes once the path generator lands (they ruled
  the eventual format goes through their new JSON reader — this header is the
  NORMATIVE content those files will carry, reviewed by design; the C++ table
  exists so the data is compiled, tested and readable today, Rule 5 flagged).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md.
- COMPOSITION ORDER IS DESIGN'S AND BINDING: density(class, xz) = base x
  clump(class, xz) x edge_gradient(dist_to_path) x exclusions, with the edge
  gradient FLOORING the field (clump_field_edged) so BR-3 holds whatever the
  clump field says. The trodden CENTRE is ~0 by core's exclusion mask.
- THE JEWEL IS A PLACEMENT BUDGET, NOT A PROBABILITY (design's ruling):
  common_scatter=false rows may ONLY be placed as/near finds (BR-6) and
  wilderness pearls (в8в). A promoted mushroom RING is a find and owes
  BR-5/BR-6 siting — the promotion predicate lives with FIND placement, not
  in the ring parity (design 10.08.2026).
- Associations are the difference between a living ground and a decal dump
  (§A7): a rule with `shade_of_neighbour` places the instance on the
  north/shade azimuth of the associated neighbour, touching it.
*/
/*
UPD:
- 10:08:2026 - 02:36:59: Created — edge set + water margin + talus apron rows,
  with the jewel budget flag and the shade-side moss association.
- 10:08:2026 - 02:49:15: Design's asks: the band DATUM is named (0 = the worn
  edge of the trodden surface, outward — never the centreline), and the
  krummholz flag-direction wind note is recorded at the row.
*/

#pragma once

#include "engine/render/sources/FloraField.h"
#include "engine/render/sources/FloraSpecies.h"

#include <cstdint>

namespace dfn::render {

/// Where a rule applies.
///
/// THE DATUM, named once so it can never silently drift (design's ask — the
/// root-flare lesson: a rule can be right while its datum is not): band
/// distance 0 is the OUTER EDGE OF THE WORN SURFACE — the line where the
/// trodden centre's material gradient ends — measured OUTWARD. Never the path
/// centreline. BR-3's «≈ 0 decoration on the trodden centre» is therefore
/// satisfied by construction: the trodden surface is entirely at negative
/// datum and no band reaches it. For water, 0 is the waterline at the
/// stillwater level.
enum class EdgeHabitat : uint8_t {
    PathMargin,  ///< dist from the worn edge of the trodden surface, outward
    WaterMargin, ///< dist from the waterline (banks, pond rims), outward
    ForestFloor, ///< inside forest masses, no linear feature needed
    TalusApron,  ///< §5.12: the scree band under the massif's cliffline
};

/// What the instance must stand against (associative grammar, §A7).
enum class EdgeAssociation : uint8_t {
    Nothing,
    ShadeOfStone,  ///< touching a stone/boulder, on its shade azimuth
    ShadeOfTrunk,  ///< touching a tree trunk, on its shade azimuth
    NearFindOnly,  ///< placed only by the find/pearl budget (BR-5/BR-6)
};

struct FloraEdgeRule {
    FloraSpecies species;
    EdgeHabitat habitat;
    float band_min_m;     ///< lateral band start (from the feature edge)
    float band_max_m;     ///< lateral band end
    float per_100m;       ///< instances per 100 m of feature, per side
    ClumpClass clump;     ///< the field this density MULTIPLIES by
    bool clump_applies;   ///< false: density is not field-modulated
    bool common_scatter;  ///< false: placement-budget only (finds/pearls)
    EdgeAssociation assoc;
};

/// The normative rows. Densities are flora's proposals under design's roles;
/// design accepts per-species from the species-line frame.
inline constexpr FloraEdgeRule FLORA_EDGE_RULES[] = {
    // --- path margins: the richest strip in the world (BR-3) ---------------
    {FloraSpecies::MossPatch, EdgeHabitat::PathMargin, 0.0f, 1.2f, 12.0f,
     ClumpClass::Moss, true, true, EdgeAssociation::Nothing},
    {FloraSpecies::MossPatch, EdgeHabitat::PathMargin, 0.0f, 2.5f, 6.0f,
     ClumpClass::Moss, true, true, EdgeAssociation::ShadeOfStone},
    {FloraSpecies::FlowerCarpet, EdgeHabitat::PathMargin, 0.3f, 2.5f, 18.0f,
     ClumpClass::Flowers, true, true, EdgeAssociation::Nothing},
    {FloraSpecies::FlowerAccent, EdgeHabitat::PathMargin, 0.3f, 2.0f, 8.0f,
     ClumpClass::Flowers, true, true, EdgeAssociation::Nothing},
    {FloraSpecies::Mushroom, EdgeHabitat::PathMargin, 0.5f, 3.0f, 4.0f,
     ClumpClass::Mushrooms, true, true, EdgeAssociation::ShadeOfTrunk},
    // «аккуратно выложенные камешки»: the pebble line hugs the very border.
    {FloraSpecies::PebbleCluster, EdgeHabitat::PathMargin, 0.0f, 1.0f, 10.0f,
     ClumpClass::Pebbles, true, true, EdgeAssociation::Nothing},
    // THE JEWEL: never in the common scatter — budgeted at finds and pearls.
    {FloraSpecies::FlowerJewel, EdgeHabitat::PathMargin, 0.5f, 4.0f, 0.0f,
     ClumpClass::Flowers, false, false, EdgeAssociation::NearFindOnly},

    // --- water margins ------------------------------------------------------
    {FloraSpecies::FlowerUmbel, EdgeHabitat::WaterMargin, 0.5f, 4.0f, 6.0f,
     ClumpClass::Flowers, true, true, EdgeAssociation::Nothing},
    {FloraSpecies::MossPatch, EdgeHabitat::WaterMargin, 0.0f, 2.0f, 8.0f,
     ClumpClass::Moss, true, true, EdgeAssociation::ShadeOfStone},

    // --- forest floor (no path needed) -------------------------------------
    {FloraSpecies::Mushroom, EdgeHabitat::ForestFloor, 0.0f, 0.0f, 0.0f,
     ClumpClass::Mushrooms, true, true, EdgeAssociation::ShadeOfTrunk},
    {FloraSpecies::MossPatch, EdgeHabitat::ForestFloor, 0.0f, 0.0f, 0.0f,
     ClumpClass::Moss, true, true, EdgeAssociation::ShadeOfTrunk},

    // --- §5.12 talus apron --------------------------------------------------
    // Scree texture + the stunted pines; boulders themselves are render's
    // Stone class — "boulder with moss" is a COMPOSITION (stone + MossPatch on
    // its shade side), not a new mesh.
    {FloraSpecies::PebbleCluster, EdgeHabitat::TalusApron, 0.0f, 0.0f, 0.0f,
     ClumpClass::Pebbles, true, true, EdgeAssociation::Nothing},
    {FloraSpecies::MossPatch, EdgeHabitat::TalusApron, 0.0f, 0.0f, 0.0f,
     ClumpClass::Moss, true, true, EdgeAssociation::ShadeOfStone},
    // Krummholz. NOTE FOR THE WIND HANDOFF (design-ruled): the dwarf's dead
    // flagged tip is authentic and stays — and when the shared wind field
    // reaches trees, the FLAG DIRECTION must sample that field at placement
    // (a krummholz flagged against the prevailing wind is a continuity bug
    // waiting). Static today; field-aligned then.
    {FloraSpecies::StuntedPine, EdgeHabitat::TalusApron, 0.0f, 0.0f, 0.0f,
     ClumpClass::GrassTufts, false, true, EdgeAssociation::Nothing},
};

inline constexpr size_t FLORA_EDGE_RULE_COUNT =
    sizeof(FLORA_EDGE_RULES) / sizeof(FLORA_EDGE_RULES[0]);

} // namespace dfn::render
