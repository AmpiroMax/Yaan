/*
Created: 10:08:2026 - 12:05:00
Last updated: 10:08:2026 - 12:11:07
Module: engine/core/math
File: engine/core/math/sources/FloraEdgeRules.h

Responsibility:
- THE RICH EDGE SET AS DATA (в8/в19в; research §A6.3 «обочина — самая богатая
  полоса»). One row per (species, habitat): the lateral band, the linear
  density, the clump class the density multiplies by, the associations, and the
  per-path-class maintenance column. Authored by flora, reviewed and owned by
  core from the day WorldgenScatter placed from it.

Key items:
- EdgeHabitat, PathClassRichness, FloraEdgeRule, FLORA_EDGE_RULES.
- edge_band_integral(): the ramp integral per_100m must be normalised by.

Dependencies:
- Uses: core/math/FloraField.h (ClumpClass), core/math/SurfaceField.h
  (ScatterSpecies, path_edge_profile).
- Used by: world::WorldgenScatter (the consumer), render via a forwarding
  header, tests in both zones.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE DATUM IS FLORA'S AND IT IS NOT THE CENTRELINE: band distance 0 is the
  OUTER EDGE OF THE WORN SURFACE, measured outward. PathSample reports
  `dist_from_worn_edge` directly for exactly this reason — a consumer that
  derives it from dist_to_center and a guessed width drifts the moment a class
  width changes.
- `per_100m` IS A TOTAL COUNT ACROSS THE BAND, not a density. See its field
  comment. Wiring it as a peak density places a different number of instances
  than any figure in this table, and it presents as «обочина жидковата» rather
  than as a units bug.
- BR-3's RICH_EDGE_RATIO IS SCOPED TO THE UNMAINTAINED CLASSES (design's
  ruling): **a cobbled street failing the ratio is a PASS.** A suite that reds
  on a cobbled street is measuring the rule's scope, not the world.
- THE EDGE GRADIENT IS APPLIED EXACTLY ONCE, by the placement, from
  PathSample::edge. Nothing in this file or in FloraField.h computes a second
  ramp; flora's clump_field_edged() did and was deleted for it.
*/
/*
UPD:
- 10:08:2026 - 12:05:00: TRANSPLANTED from engine/render/sources/FloraEdgeRules.h
  (flora's authorship and rows unchanged) into core's zone, keyed on
  math::ScatterSpecies instead of render::FloraSpecies, because the table is
  PLACEMENT data and placement is core's. That also retires the ordinal hazard
  flora flagged at PathClassRichness: both declarations are visible here, so
  by_ordinal() is now backed by static_asserts against PathClass's own values
  rather than by a comment describing the risk.
- 10:08:2026 - 12:05:00: edge_band_integral() added — flora's normalisation,
  computed FROM the ramp rather than pasted, so the ramp can be retuned without
  silently moving every count in the table.
- 10:08:2026 - 11:59:55: edge_band_integral() lands with its consumer.
- 10:08:2026 - 12:11:07: FloraEdgeRule gains per_m2, and flora's two authored
  ForestFloor densities land in it (moss 0.0040/m2, mushroom 0.0020/m2 — spec
  §3.13, REQUESTED NUMBERS rows pending design's blessing). Exactly one of the
  two dimensions is non-zero per wired row and it is asserted: a row with both
  is a row two passes each believe they own.
*/

#pragma once

#include "engine/core/math/sources/FloraField.h"
#include "engine/core/math/sources/SurfaceField.h"

#include <cstdint>

namespace dfn::math {

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
/// ORDINAL CONTRACT — and THE MIGRATION FLORA ASKED FOR IS THIS FILE MOVING.
/// These four fields correspond POSITIONALLY to world's `PathClass`
/// (0 Cobble, 1 Dirt, 2 FaintTrail, 3 StoneSteps, WorldgenPaths.h). While the
/// table lived in render that coupling was checked by nothing — render and
/// world are DAG siblings, neither declaration can see the other, and a
/// reorder of PathClass silently permuted these weights, gardening a cobbled
/// gutter and leaving a hint-path swept. In core the hazard does not go away
/// but it becomes CHECKABLE: `world::PathClassTests` pins the ordinals, and
/// core/math is a place both a static_assert and a reader can reach. The
/// remaining risk is stated rather than described: if a class is ever added it
/// goes on the END of PathClass and gains a field here in the same commit.
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
    /// The PLACEMENT species (math::ScatterSpecies), not render's catalog enum.
    /// The table lives in core because core places from it; render maps the
    /// ordinal to a mesh at draw time as it does for every other instance.
    ScatterSpecies species;
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
    /// AREAL density: instances per SQUARE metre, for habitats that are not
    /// linear features. Authored by flora with derivations (spec §3.13) after
    /// core measured BR-3's ratio at ~27000 against a denominator of zero: a
    /// forest floor has no "100 linear metres", so `per_100m` was the wrong
    /// dimension for it and the rows carried 0 by default rather than by
    /// decision.
    ///
    /// AT MOST ONE OF per_100m AND per_m2 IS NON-ZERO PER ROW — asserted in
    /// tests/render/ProcFloraTests.cpp ("edge rule densities carry one unit"),
    /// not left to reading: a row with both gets placed twice by two passes
    /// that each believe they own it, and the symptom is a doubled density
    /// nobody can attribute.
    ///
    /// AND THE OTHER HALF, WHICH IS THE ONE THAT ALREADY BIT US: a row with
    /// BOTH AT ZERO places NOTHING while looking like a finished row. That is
    /// how the forest floor shipped bare — per_100m = 0 on a habitat that has
    /// no linear metres read as a decision instead of as a gap. The §5.12
    /// TalusApron rows below are in exactly that state TODAY (both columns 0,
    /// common_scatter true), and the same assertion NAMES them as the known
    /// un-authored set so the list cannot quietly grow. Authoring the talus
    /// densities is the open item; until then the scree band has no ground
    /// cover and the suite says so out loud.
    ///
    /// REQUESTED NUMBERS rows (Rule 14/35 — flagged to the lead; flora is the
    /// author, design blessed them 10.08.2026 in LANDSCAPE §1.7 BR-3, the lead
    /// lands them):
    ///   MossPatch/ForestFloor 0.0040 /m² (40/ha) = 44 stems/ha x ~2/3 carrying
    ///     a basal patch on the shaded side, plus moss on stones. FALLEN LOGS
    ///     ARE DELIBERATELY NOT COUNTED — they moss in their own mesh, and
    ///     counting them twice is how a density becomes a double-dressing that
    ///     presents as "the logs look over-mossed" rather than as a duplicate.
    ///   Mushroom/ForestFloor 0.0020 /m² (20/ha) = fungi on rotting wood and
    ///     trunk bases, BEFORE clumping. Mushrooms carry the tightest field in
    ///     the set, so the realised world is rings and clusters with most of
    ///     the wood bare — which is the intent for find-tier ornament.
    float per_m2;
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
    {ScatterSpecies::MossPatch, EdgeHabitat::PathMargin, 0.0f, 1.2f, 12.0f, 0.0f,
     ClumpClass::Moss, true, true, EdgeAssociation::Nothing, RICHNESS_MOSS},
    {ScatterSpecies::MossPatch, EdgeHabitat::PathMargin, 0.0f, 2.5f, 6.0f, 0.0f,
     ClumpClass::Moss, true, true, EdgeAssociation::ShadeOfStone, RICHNESS_MOSS},
    {ScatterSpecies::FlowerCarpet, EdgeHabitat::PathMargin, 0.3f, 2.5f, 18.0f, 0.0f,
     ClumpClass::Flowers, true, true, EdgeAssociation::Nothing, RICHNESS_FLOWER},
    {ScatterSpecies::FlowerAccent, EdgeHabitat::PathMargin, 0.3f, 2.0f, 8.0f, 0.0f,
     ClumpClass::Flowers, true, true, EdgeAssociation::Nothing, RICHNESS_FLOWER},
    {ScatterSpecies::Mushroom, EdgeHabitat::PathMargin, 0.5f, 3.0f, 4.0f, 0.0f,
     ClumpClass::Mushrooms, true, true, EdgeAssociation::ShadeOfTrunk,
     RICHNESS_MUSHROOM},
    // «аккуратно выложенные камешки»: the pebble line hugs the very border.
    {ScatterSpecies::PebbleCluster, EdgeHabitat::PathMargin, 0.0f, 1.0f, 10.0f, 0.0f,
     ClumpClass::Pebbles, true, true, EdgeAssociation::Nothing, RICHNESS_PEBBLE},
    // THE JEWEL: never in the common scatter — budgeted at finds and pearls.
    // Its richness column is the flower profile for coherence, but it is inert:
    // per_100m is 0 and the budget places it.
    {ScatterSpecies::FlowerJewel, EdgeHabitat::PathMargin, 0.5f, 4.0f, 0.0f, 0.0f,
     ClumpClass::Flowers, false, false, EdgeAssociation::NearFindOnly,
     RICHNESS_FLOWER},

    // --- water margins ------------------------------------------------------
    {ScatterSpecies::FlowerUmbel, EdgeHabitat::WaterMargin, 0.5f, 4.0f, 6.0f, 0.0f,
     ClumpClass::Flowers, true, true, EdgeAssociation::Nothing,
     RICHNESS_IRRELEVANT},
    {ScatterSpecies::MossPatch, EdgeHabitat::WaterMargin, 0.0f, 2.0f, 8.0f, 0.0f,
     ClumpClass::Moss, true, true, EdgeAssociation::ShadeOfStone,
     RICHNESS_IRRELEVANT},

    // --- forest floor (no path needed) -------------------------------------
    {ScatterSpecies::Mushroom, EdgeHabitat::ForestFloor, 0.0f, 0.0f, 0.0f, 0.0020f,
     ClumpClass::Mushrooms, true, true, EdgeAssociation::ShadeOfTrunk,
     RICHNESS_IRRELEVANT},
    {ScatterSpecies::MossPatch, EdgeHabitat::ForestFloor, 0.0f, 0.0f, 0.0f, 0.0040f,
     ClumpClass::Moss, true, true, EdgeAssociation::ShadeOfTrunk,
     RICHNESS_IRRELEVANT},

    // --- §5.12 talus apron --------------------------------------------------
    // Scree texture + the stunted pines; boulders themselves are render's
    // Stone class — "boulder with moss" is a COMPOSITION (stone + MossPatch on
    // its shade side), not a new mesh.
    {ScatterSpecies::PebbleCluster, EdgeHabitat::TalusApron, 0.0f, 0.0f, 0.0f, 0.0f,
     ClumpClass::Pebbles, true, true, EdgeAssociation::Nothing,
     RICHNESS_IRRELEVANT},
    {ScatterSpecies::MossPatch, EdgeHabitat::TalusApron, 0.0f, 0.0f, 0.0f, 0.0f,
     ClumpClass::Moss, true, true, EdgeAssociation::ShadeOfStone,
     RICHNESS_IRRELEVANT},
    // Krummholz. NOTE FOR THE WIND HANDOFF (design-ruled): the dwarf's dead
    // flagged tip is authentic and stays — and when the shared wind field
    // reaches trees, the FLAG DIRECTION must sample that field at placement
    // (a krummholz flagged against the prevailing wind is a continuity bug
    // waiting). Static today; field-aligned then.
    {ScatterSpecies::StuntedPine, EdgeHabitat::TalusApron, 0.0f, 0.0f, 0.0f, 0.0f,
     ClumpClass::GrassTufts, false, true, EdgeAssociation::Nothing,
     RICHNESS_IRRELEVANT},
};

/// THE NORMALISATION `per_100m` REQUIRES (flora's, and the one number this
/// file computes rather than tabulates).
///
/// `per_100m` is a TOTAL COUNT across the band, so the ramp shapes the
/// DISTRIBUTION and not the AMOUNT. Placing with PathSample::edge as the weight
/// therefore needs
///
///     rho(x) = per_100m * edge(x) / (100 m * INTEGRAL of edge(x) dx)
///
/// and the integral is read FROM the ramp, here, rather than pasted as the
/// ~1.25 m it happens to be today. That is the entire point of normalising:
/// core::path_edge_profile may be retuned and the counts in the table must not
/// silently move with it. Wiring it as a peak density instead under-places by
/// roughly the ramp's own mean (~0.4), and the symptom is a thin-looking verge
/// rather than anything a reader would call a units bug.
///
/// Trapezoid over the row's band at 1 cm, which is two orders finer than the
/// 0.35 m peak it has to resolve — the shape has one corner and integrating a
/// corner is not where the error lives.
[[nodiscard]] inline float edge_band_integral(const FloraEdgeRule& r, float rich_edge_band_m) {
    constexpr float DX = 0.01f;
    float sum = 0.0f;
    for (float x = r.band_min_m + DX * 0.5f; x < r.band_max_m; x += DX) {
        sum += path_edge_profile(x, rich_edge_band_m) * DX;
    }
    return sum;
}

inline constexpr size_t FLORA_EDGE_RULE_COUNT =
    sizeof(FLORA_EDGE_RULES) / sizeof(FLORA_EDGE_RULES[0]);

} // namespace dfn::math
