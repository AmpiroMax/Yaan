/*
Created: 10:08:2026 - 02:36:59
Last updated: 10:08:2026 - 11:24:00
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
  clump(class, xz) x edge_gradient(dist_to_path) x richness(path_class) x
  exclusions, with the edge gradient FLOORING the field so BR-3 holds whatever
  the clump field says. The trodden CENTRE is ~0 by core's exclusion mask.
  Core's PathSample.edge IS the edge_gradient factor and it is applied ONCE, by
  them, with plain clump_field(). Flora's clump_field_edged() is DELETED as of
  10.08.2026 for exactly this reason — two ramps would square the band and move
  its peak inward, and the symptom is "the verge looks thin", which nobody
  diagnoses as a units bug.
- `per_100m` IS A TOTAL COUNT ACROSS THE BAND, not a density: see its field
  comment for the normalisation the ramp requires. Wiring it as a peak density
  places a different number of instances than any figure quoted here.
- BR-3's RICH_EDGE_RATIO IS SCOPED TO THE UNMAINTAINED CLASSES (design's
  ruling, 10.08.2026). A cobbled street failing the ratio is a **PASS**: the
  margin is suppressed there on purpose, because a rich verge is what grows
  where NOBODY SWEEPS and a settlement sweeps its own street. A suite that
  reds on a cobbled street is measuring the rule's scope, not the world.
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
- 10:08:2026 - 11:07:33: Design's maintenance ruling: PathClassRichness per
  row (hint >= dirt > cobble; moss in stair joints, flowers never). The weight
  scales the edge PEAK, not the base presence — a kept verge is not bare
  ground. BR-3's ratio scoped to the unmaintained classes.
- 10:08:2026 - 11:24:00: per_100m's dimension made unambiguous — a TOTAL
  COUNT across the band, with the normalisation the ramp requires, after core
  asked whether it was a peak or a mean density (it is neither).
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

/// PER-PATH-CLASS MARGIN RICHNESS (design's ruling, 10.08.2026), and the
/// fiction that decides every number in it, recorded because a multiplier
/// without a reason drifts: **A RICH MARGIN IS WHAT GROWS WHERE NOBODY
/// SWEEPS.** Cobble through a settlement is swept by the people who live
/// there; a generator that gardens a town gutter has made maintenance
/// invisible. A hint-path is BR-3's specimen class and carries the full band.
///
/// ORDINAL CONTRACT, and it is a real seam rather than a tidy one: these four
/// fields correspond POSITIONALLY to core's `PathClass`
/// (0 Cobble, 1 Dirt, 2 FaintTrail, 3 StoneSteps, WorldgenPaths.h). Flora
/// cannot include that enum — `world` and `render` are SIBLINGS in the
/// dependency DAG and neither may include the other — so this is a coupling
/// checked by nothing today. It stops being one at the migration already
/// scheduled for this table (Rule 5, core's JSON reader): in core's zone both
/// declarations are visible and the mapping becomes a direct key plus a
/// static_assert on the count. **Until then, a reorder of `PathClass` silently
/// permutes these weights** — the honest statement of the risk, kept here so
/// the migration has a stated reason beyond tidiness.
struct PathClassRichness {
    float cobble = 1.0f;
    float dirt = 1.0f;
    float faint_trail = 1.0f;
    float stone_steps = 1.0f;

    [[nodiscard]] constexpr float by_ordinal(uint8_t path_class) const {
        switch (path_class) {
        case 0: return cobble;
        case 1: return dirt;
        case 2: return faint_trail;
        case 3: return stone_steps;
        default: return 1.0f;
        }
    }
};

/// WHAT THE WEIGHT SCALES, and design's second consequence turns on it: it
/// scales the EDGE PEAK — the `edge_gradient` factor — and NOT the base
/// presence. So a maintained class loses its rich BAND while the ground there
/// keeps whatever `base x clump` gives it.
///
/// **A KEPT VERGE IS NOT BARE GROUND** (design, 10.08.2026): §1.1 does not
/// stop at the town gate, and a margin suppressed to nothing would re-make
/// «земля плоская и мёртвая» inside the settlement — the very complaint this
/// stage exists to answer. Weight 0 therefore means "ratio ~ 1, no peak",
/// never "no life": the swept ground between the surviving pockets is what
/// makes those pockets read as SPARED rather than as leftover.
///
/// The profiles are named rather than inlined so the fiction is applied once
/// per KIND of plant, not re-guessed per row.
///
/// IDENTITY (all 1.0) is the deliberate default for non-PathMargin habitats:
/// the column is meaningless beside a lake or in a forest interior, and a
/// consumer that multiplies by it anyway must be harmless. Misuse costs
/// nothing; forgetting to apply it costs a swept street full of flowers.
inline constexpr PathClassRichness RICHNESS_IRRELEVANT{1.0f, 1.0f, 1.0f, 1.0f};
/// Flowers: NO PEAK on a swept street (a gardened town gutter is exactly the
/// artefact this ruling exists to prevent) and NONE on a stair — flowers in a
/// stone stair read as ruin, not as a tended margin. Design named both.
inline constexpr PathClassRichness RICHNESS_FLOWER{0.0f, 0.55f, 1.0f, 0.0f};
/// Moss is the exception the fiction PREDICTS rather than contradicts: it is
/// what survives a broom, because it lives in the JOINTS — between setts, at
/// wall bases, in the lee of thresholds — and in the shaded joints of a stair.
/// Design named the stair case explicitly; the small cobble weight is flora
/// reading their «мох и сорняки в швах» as licensing a residual peak for the
/// broom-survivor specifically, and it is flagged as an interpretation.
inline constexpr PathClassRichness RICHNESS_MOSS{0.25f, 0.55f, 1.0f, 0.90f};
/// Fungus on a swept street reads as a different kind of neglect; damp stair
/// joints hold a little.
inline constexpr PathClassRichness RICHNESS_MUSHROOM{0.0f, 0.45f, 1.0f, 0.20f};
/// A pebble border beside geometry that IS dressed stone is noise, so both
/// stone classes take none; the border is a dirt-and-trail idiom.
inline constexpr PathClassRichness RICHNESS_PEBBLE{0.0f, 0.50f, 1.0f, 0.0f};

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
    /// A TOTAL COUNT, not a density — core asked and the honest answer is
    /// "neither of your two options" (10.08.2026). Dimension: **instances per
    /// 100 linear metres of feature, per side, SUMMED ACROSS THE WHOLE BAND**
    /// [band_min_m, band_max_m]. It is not instances/m², so it is neither a
    /// peak density nor a band-mean density.
    ///
    /// **THE RAMP THEREFORE SHAPES THE DISTRIBUTION, NOT THE AMOUNT.** Placing
    /// with `PathSample::edge` as a weight and this number as a magnitude
    /// requires normalising by the ramp's own integral, or the count comes out
    /// low by exactly that integral:
    ///
    ///     rho(x) = per_100m * edge(x) / (100 m * INTEGRAL of edge(x) dx)
    ///
    /// over the band, so that the placed total is `per_100m` by construction
    /// whatever shape the ramp has. With core's measured ramp (peak just
    /// outside the worn edge, decaying to 0 by 2.5 m, mean ~0.4) the integral
    /// is ~1.0 m, but **read it from the ramp rather than pasting 1.0** — the
    /// whole point of normalising is that the ramp may be retuned and the
    /// counts must not silently move with it.
    ///
    /// Wiring it as a PEAK density instead would place a different number of
    /// instances than any figure quoted in this table or in flora.md §3.12,
    /// and the error would look like "the margin feels sparse" rather than
    /// like a units bug.
    float per_100m;
    ClumpClass clump;     ///< the field this density MULTIPLIES by
    bool clump_applies;   ///< false: density is not field-modulated
    bool common_scatter;  ///< false: placement-budget only (finds/pearls)
    EdgeAssociation assoc;
    /// The maintenance column. Ignored for non-PathMargin habitats (identity).
    PathClassRichness richness;
};

/// The normative rows. Densities are flora's proposals under design's roles;
/// design accepts per-species from the species-line frame.
inline constexpr FloraEdgeRule FLORA_EDGE_RULES[] = {
    // --- path margins: the richest strip in the world (BR-3) ---------------
    {FloraSpecies::MossPatch, EdgeHabitat::PathMargin, 0.0f, 1.2f, 12.0f,
     ClumpClass::Moss, true, true, EdgeAssociation::Nothing, RICHNESS_MOSS},
    {FloraSpecies::MossPatch, EdgeHabitat::PathMargin, 0.0f, 2.5f, 6.0f,
     ClumpClass::Moss, true, true, EdgeAssociation::ShadeOfStone, RICHNESS_MOSS},
    {FloraSpecies::FlowerCarpet, EdgeHabitat::PathMargin, 0.3f, 2.5f, 18.0f,
     ClumpClass::Flowers, true, true, EdgeAssociation::Nothing, RICHNESS_FLOWER},
    {FloraSpecies::FlowerAccent, EdgeHabitat::PathMargin, 0.3f, 2.0f, 8.0f,
     ClumpClass::Flowers, true, true, EdgeAssociation::Nothing, RICHNESS_FLOWER},
    {FloraSpecies::Mushroom, EdgeHabitat::PathMargin, 0.5f, 3.0f, 4.0f,
     ClumpClass::Mushrooms, true, true, EdgeAssociation::ShadeOfTrunk,
     RICHNESS_MUSHROOM},
    // «аккуратно выложенные камешки»: the pebble line hugs the very border.
    {FloraSpecies::PebbleCluster, EdgeHabitat::PathMargin, 0.0f, 1.0f, 10.0f,
     ClumpClass::Pebbles, true, true, EdgeAssociation::Nothing, RICHNESS_PEBBLE},
    // THE JEWEL: never in the common scatter — budgeted at finds and pearls.
    // Its richness column is the flower profile for coherence, but it is inert:
    // per_100m is 0 and the budget places it.
    {FloraSpecies::FlowerJewel, EdgeHabitat::PathMargin, 0.5f, 4.0f, 0.0f,
     ClumpClass::Flowers, false, false, EdgeAssociation::NearFindOnly,
     RICHNESS_FLOWER},

    // --- water margins ------------------------------------------------------
    {FloraSpecies::FlowerUmbel, EdgeHabitat::WaterMargin, 0.5f, 4.0f, 6.0f,
     ClumpClass::Flowers, true, true, EdgeAssociation::Nothing,
     RICHNESS_IRRELEVANT},
    {FloraSpecies::MossPatch, EdgeHabitat::WaterMargin, 0.0f, 2.0f, 8.0f,
     ClumpClass::Moss, true, true, EdgeAssociation::ShadeOfStone,
     RICHNESS_IRRELEVANT},

    // --- forest floor (no path needed) -------------------------------------
    {FloraSpecies::Mushroom, EdgeHabitat::ForestFloor, 0.0f, 0.0f, 0.0f,
     ClumpClass::Mushrooms, true, true, EdgeAssociation::ShadeOfTrunk,
     RICHNESS_IRRELEVANT},
    {FloraSpecies::MossPatch, EdgeHabitat::ForestFloor, 0.0f, 0.0f, 0.0f,
     ClumpClass::Moss, true, true, EdgeAssociation::ShadeOfTrunk,
     RICHNESS_IRRELEVANT},

    // --- §5.12 talus apron --------------------------------------------------
    // Scree texture + the stunted pines; boulders themselves are render's
    // Stone class — "boulder with moss" is a COMPOSITION (stone + MossPatch on
    // its shade side), not a new mesh.
    {FloraSpecies::PebbleCluster, EdgeHabitat::TalusApron, 0.0f, 0.0f, 0.0f,
     ClumpClass::Pebbles, true, true, EdgeAssociation::Nothing,
     RICHNESS_IRRELEVANT},
    {FloraSpecies::MossPatch, EdgeHabitat::TalusApron, 0.0f, 0.0f, 0.0f,
     ClumpClass::Moss, true, true, EdgeAssociation::ShadeOfStone,
     RICHNESS_IRRELEVANT},
    // Krummholz. NOTE FOR THE WIND HANDOFF (design-ruled): the dwarf's dead
    // flagged tip is authentic and stays — and when the shared wind field
    // reaches trees, the FLAG DIRECTION must sample that field at placement
    // (a krummholz flagged against the prevailing wind is a continuity bug
    // waiting). Static today; field-aligned then.
    {FloraSpecies::StuntedPine, EdgeHabitat::TalusApron, 0.0f, 0.0f, 0.0f,
     ClumpClass::GrassTufts, false, true, EdgeAssociation::Nothing,
     RICHNESS_IRRELEVANT},
};

inline constexpr size_t FLORA_EDGE_RULE_COUNT =
    sizeof(FLORA_EDGE_RULES) / sizeof(FLORA_EDGE_RULES[0]);

} // namespace dfn::render
