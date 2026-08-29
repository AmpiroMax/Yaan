/*
Module: engine/world
File: engine/world/sources/TestbedLayout.h

Responsibility:
- The deterministic generator INPUT DATA for worldgen v2: feature stamp centers,
  water layout, site positions, POI-chain corridors and forest regions of the
  testbed, exactly as designed in docs/design/LANDSCAPE.md §7.1 (every default
  cites its row). Lead approved this as a WorldGenParams field (stage-3b sync):
  coordinates are layout data, not tunable constants — NUMBERS.md is not their
  home. Serialization into WorldInfo lands with .dfw IO.

Key items:
- CragStamp / BumpStamp / LakeStamp: macro feature stamp geometry (P1).
- RiverLayout: source, fords (P2).
- SiteLayout / SiteKind: POI placement inputs (P4).
- CorridorLayout: POI-chain corridor polylines (§2.4 mask).
- ForestRegions: oak/pine region shapes, forced clearings (P5).
- TestbedLayout: the aggregate, default = the §7.1 table.

Dependencies:
- Uses: glm only.
- Used by: Worldgen (all passes), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every value here must be traceable to LANDSCAPE.md §7 (design zone owns the
  layout). Tuning requests go to design, not silently edited here.
- Everything influencing generation lives in WorldGenParams (Worldgen.h rule);
  this struct is part of it — never read layout from anywhere else.
*/

#pragma once

#include "engine/core/config/sources/Constants.h"

#include <cmath>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace dfn::world {

/// Which stand map this layout describes (LANDSCAPE §8, user-ratified в1):
/// stands are SEPARATE maps built from generation rules — each declares its
/// own composition of §2.10 landforms. The app selects one via the DFN_MAP
/// env (lead's wiring); the generator branches on this id at the macro pass.
/// Future stands (river+castle, sea, town, mirror) extend this enum.
enum class StandId : uint8_t {
    Testbed = 0, ///< the §7.1 testbed — today's world, byte-identical default
    Forest = 1,  ///< §8.1 «лесок»: LF-1, LF-2, LF-5, LF-7, LF-8 — no massif, no water
    /// ONE tree on calm ground — the inspection stand (user, 14.08: «стенд с
    /// деревом, ровно одним, я покажу все детали, какие мне не нравятся»).
    /// Exists because a defect named on a single tree is actionable, while the
    /// same defect named on a forest is an argument about which tree was meant.
    OneTree = 2,
    /// The REGISTRY GALLERY: the same calm ground as OneTree, but the exhibits
    /// come from assets/objects (.dfo) — nothing scattered, everything placed.
    /// The stand that shows what the forge made.
    Gallery = 3,
};

/// The flora-family stands: no water landform, no P4 sites, no carve works —
/// the generator serves them all through its Forest path. A PREDICATE rather
/// than four scattered `== Forest` comparisons, because the day a comparison
/// is missed the new stand gets the testbed's massif on a map that never
/// declared one, and nothing goes red anywhere near the edit (Rule 32).
[[nodiscard]] constexpr bool stand_is_floral(StandId s) {
    return s == StandId::Forest || s == StandId::OneTree || s == StandId::Gallery;
}

/// The inspection stands: calm ground, no testbed props, exhibits only. One
/// predicate, because "the door prop does not spawn here" must hold for every
/// stand of this family, not for the ones somebody remembered (Rule 32).
[[nodiscard]] constexpr bool stand_is_inspection(StandId s) {
    return s == StandId::OneTree || s == StandId::Gallery;
}
struct CragStamp {
    glm::vec2 center{830.0f, 200.0f}; ///< peak (§7.1)
    /// Footprint (§7.1). Governed by MASSIF_ASPECT_MIN now that I10 exists, so
    /// it is a NUMBERS constant rather than the bare literal it used to be.
    float radius = static_cast<float>(config::L0_BASE_RADIUS);
    /// Absolute summit elevation, m. L0_RELIEF is the approved figure (52 ->
    /// 115): the fortress hierarchy rules are RATIOS to this, so the castle's
    /// R4 dominance and crown clearance both depend on it directly.
    float peak_height = static_cast<float>(config::L0_RELIEF);
    /// Splat and tree lines scale with the summit rather than sitting at the
    /// absolute heights tuned for a 52 m crag — otherwise a 115 m Ravenscar is
    /// bald and bare rock from a third of the way up.
    float rockline = static_cast<float>(config::L0_RELIEF) * 0.65f;
    float treeline = static_cast<float>(config::L0_RELIEF) * 0.50f;           ///< treeless band starts here on the stamp (§1.3
                                      ///< C4 knob: keeps foothill canopy from out-angling
                                      ///< the L0; widen by lowering)
    /// Arete count (L0_ARETE_COUNT): how many outward lobes/inward couloirs
    /// the per-bearing radial extent carries. This is what the barrow's
    /// couloir search needs to exist at all.
    /// Facet count for the §2.8.2 support-polygon cross-section. NOT pinned to
    /// L0_ARETE_COUNT_MIN: reading one bound of a range as if it were the range
    /// is the recurring bug in this zone, and it left I7 with zero margin (the
    /// invariant needs >= 3 detected aretes, so a 3-facet massif fails if one
    /// corner is missed). Measured across 12 seeds: 3 facets -> I8 level fails
    /// 3 seeds; 4 -> fails 1; 5 -> fails ALL TWELVE, which is design's convex
    /// cap n*tan(pi/n)/pi = 1.16 for a pentagon showing up as a measurement.
    /// Whether this should be a per-seed DRAW in [MIN, MAX] rather than an
    /// authored layout value is with design.
    ///
    /// HELD AT 3 PENDING DESIGN. Moving to 4 measurably improves I8, but it
    /// reshapes the flank enough to break the Backbarrow sightline from the
    /// castle yard AND gate -- a story-binding constraint -- and to re-close
    /// the crag tunnel. That is §7.0a's cross-cutting dependency, and spending
    /// a story invariant to buy an invariant margin is not a call this zone
    /// gets to make on its own.
    /// Re-derived on the FIXED bearing field; the 3-vs-4 sweep that chose 4 ran
    /// entirely on the broken one and is void. Measured 12 seeds, rulings 1+2:
    ///   n=3  I11@600 3/1/1/0  I4 fails 4/12  I8 level fails 4/12
    ///   n=4  I11@600 1/1/0/0  I4 fails 8/12  I8 level fails 7/12
    /// Fewer, wider facets read BETTER at distance and cost less elsewhere.
    /// Only I7 prefers 4, and I7 is structurally unreachable at n=3 (three
    /// corners against a floor of three detections = zero margin), so it is not
    /// a real trade. Design ruled 4 off the void sweep and should re-rule.
    ///
    /// I first blamed 3 for breaking sim_tunnel_walk and render_terrain_mesher.
    /// It did not: BOTH FAIL AT 4 AS WELL -- render on a LOD scale assertion
    /// from their in-flight work, sim on a NullPhysics build error. Identical
    /// test outcomes at 3 and 4, so the cascade I attributed to this constant
    /// does not exist. Checking the counterfactual is what settles a cascade
    /// claim; correlation with my own edit is not evidence.
    int arete_count = 3;
    float ridge_cell = 48.0f;         ///< ridged-noise lattice cell, m (stamp shape)
    /// Flank sub-relief as a FRACTION of peak height (legacy parameterization).
    /// Used only when ridge_amp_meters <= 0.
    float ridge_amp_frac = 0.25f;
    /// Flank sub-relief as an ABSOLUTE amplitude in metres. Preferred: with the
    /// fractional form the ridges that occlude the summit scale WITH the summit,
    /// so the occluder is pinned to the thing it occludes and raising the peak
    /// can never buy clearance. Expressed in metres the two are independent.
    float ridge_amp_meters = 13.0f; ///< = 0.25 * 52, i.e. today's 52 m crag unchanged
};

/// Small radial bump stamp (shrine knoll +6 m, lakeshore bluff +10 m — §7.1).
struct BumpStamp {
    glm::vec2 center{0.0f};
    float radius = 0.0f;
    float height = 0.0f; ///< added at center, smooth falloff to rim
};

/// Lake basin stamp (§7.1 lake + §3.2 rules). The rim ring guarantees the
/// LAKE_LEVEL_TESTBED water plane is contained regardless of base terrain;
/// the rim is lowered toward outlet_dir so the spill (river exit) heads for
/// the §7.1 south-edge outlet deterministically.
struct LakeStamp {
    glm::vec2 center{230.0f, 520.0f};     ///< §7.1
    glm::vec2 half_extent{45.0f, 70.0f};  ///< ≈ 90x140 m footprint (§7.1)
    float rim_rise = 1.2f;                ///< levee crest above water, m (stamp shape)
    float rim_band_frac = 0.2f;           ///< crest at q = 1 + this (normalized radius)
    float rim_fade_frac = 0.3f;           ///< crest blends back to terrain over this
    glm::vec2 outlet_dir{0.14f, 0.99f};   ///< toward the §7.1 outlet ≈ (300, 1024)
    float outlet_bias = 0.8f;             ///< rim lowered by this toward outlet, m
};

/// Normalized elliptical radius of `world` in a lake stamp (1 = rim).
[[nodiscard]] inline float lake_norm_radius(const LakeStamp& lake, glm::vec2 world) {
    const float vx = (world.x - lake.center.x) / lake.half_extent.x;
    const float vz = (world.y - lake.center.y) / lake.half_extent.y;
    return std::sqrt(vx * vx + vz * vz);
}

/// River layout (§7.1): the source stamp target. The trace is algorithmic
/// (§3.1); FORDS ARE DERIVED, NEVER TABLED (§7.1a design ruling) — P2 places
/// them where the POI-chain corridors cross the generated trace, plus the
/// FORD_SPACING_MAX gap fill.
struct RiverLayout {
    glm::vec2 source{760.0f, 300.0f};    ///< §7.1; snapped to the coarse-grid argmax
    float source_search_radius = 30.0f;  ///< argmax search around `source` (§3.1 step 1)
};

/// Drainage valley stamp along source -> lake: an explicit monotone FLOOR
/// profile (terrain clamped down to it inside the valley) plus raised
/// shoulders (terrain clamped up just outside) forming the watershed divide.
/// This is what makes the §3.1 greedy descent drain where the §7.1
/// composition wants the river — without the shoulders, the sub-15 m eastern
/// lowland would capture the flow (macro "must never create local minima with
/// no hydrology resolution"). floor_mouth sits above the lake's rim crest so
/// the river crosses the levee into the basin without ponding.
struct ValleyTrough {
    glm::vec2 points[4]{};
    int point_count = 0;
    float half_width = 80.0f;    ///< valley floor half width
    float floor_source = 22.0f;  ///< floor height at the upstream end, m
    float floor_mouth = 16.5f;   ///< floor at the downstream end, m
    float wall_height = 5.0f;    ///< cross-section rise floor -> valley edge
    float shoulder_frac = 0.6f;  ///< shoulder band width as a fraction of half_width
};

/// What stands at a site (P4). Kinds map 1:1 to placeholder archetypes.
enum class SiteKind : uint8_t {
    Hamlet = 0,          ///< building cluster, placed procedurally around the common
    Shrine = 1,
    DungeonEntrance = 2,
    TowerRuin = 3,       ///< L0 topper on the crag peak
};

/// One site placement input (§7.1 rows). Hamlet expands into buildings in P4.
struct SiteLayout {
    glm::vec2 position{0.0f};
    SiteKind kind = SiteKind::Shrine;
};

/// One POI-chain corridor as a waypoint polyline (§2.4; §7.2 chain edges).
/// Width comes from CORRIDOR_WIDTH. Up to 4 waypoints (straight-ish bands).
struct CorridorLayout {
    glm::vec2 points[4]{};
    int point_count = 0;
};

/// A walkable carved corridor (P7): a polyline of floor waypoints with a flat
/// floor and flat ceiling. Waypoints may sit outside the terrain — the portal
/// forms where the path meets rock (see WorldgenCarve.h).
struct CarveCorridor {
    glm::vec3 points[10]{}; ///< x, FLOOR y, z
    int point_count = 0;
    float half_width = 2.0f; ///< corridor is 2x this wide
    float height = 3.2f;     ///< floor to ceiling: real headroom, not a crawl
    /// AUTHORED INTENT: this corridor's endpoints must stand in open air so
    /// portals form where the path meets rock. When set, worldgen DERIVES the
    /// daylight ends (open_daylight_portals — endpoints pushed along their own
    /// leg until the floor clears the terrain): the massif above the survey
    /// has reshaped twice and buried the exit both times, so "ends in open
    /// air" is a property to enforce, not a coordinate to keep re-surveying.
    /// False for corridors that deliberately end inside rock (barrow passage
    /// ends in its chamber).
    bool daylight_portals = false;
};

/// A carved chamber (rectangular room): the Backbarrow's interior.
struct CarveChamber {
    glm::vec3 center{0.0f};    ///< x, FLOOR y, z
    glm::vec3 half_extent{0.0f}; ///< x/z half sizes; y component is the height
};

/// P7 carve set (3D terrain). Two sites in the crunch variant: the switchback
/// route up the crag, and the Backbarrow interior behind its entrance.
struct CarveLayout {
    CarveCorridor crag_tunnel{};
    CarveCorridor barrow_passage{};
    CarveChamber barrow_chamber{};
    /// Which TestbedLayout::sites entry each carved entrance belongs to, so P4
    /// can DERIVE that marker from the carve mouth instead of scoring a pad for
    /// it. -1 = this site has no carve yet and falls back to the scorer.
    /// (Derived-only now covers carve-adjacent placement, not just water.)
    int barrow_site_index = 2;
    int lakeshore_site_index = -1; ///< pending design's bluff-face ruling
    CarveCorridor lakeshore_adit{};
};

/// Castle layout (LANDSCAPE §6.1 ruling): House Corvane's seat on the crag's
/// SW foot spur. Position is a stamp target (§6.1.4) — core solves the exact
/// pad against the C1 re-validation; the §6.1.1 invariants are the contract.
/// Element footprints from the §6.1.3 table; heights come from dfn::config
/// and are SUBORDINATE to R3 (pad + tallest element <= peak - margin).
struct CastleLayout {
    glm::vec2 center{760.0f, 330.0f}; ///< §6.1.4 candidate, +-20 m
    /// Corridor whose direction the gate faces (§6.1.2: the watchpoint ->
    /// barrow corridor becomes the castle approach; the gate is valley-facing
    /// and the pad is never rotated — settled). Index into
    /// TestbedLayout::corridors.
    int approach_corridor = 2;
};

/// Forest mass regions (§7.1 "Forest masses"): oak = S + SE bands as axis
/// rects; pine = crag-foothill RIDGE STRIPS (§5.2: strips 20-60 m wide
/// following ridgelines, never a solid ring — a closed pine ring around the
/// L0 walls it off from every valley standpoint, §1.3 amendment) + the north
/// ridge strip. Strips are radial wedges of the foothill annulus: pine where
/// fract(bearing_from_crag / TAU * strip_count) < strip_duty.
struct ForestRegions {
    // Oak rects: {min_x, min_z, max_x, max_z}.
    glm::vec4 oak_rects[2] = {{0.0f, 700.0f, 1024.0f, 1024.0f},
                              {500.0f, 600.0f, 1024.0f, 1024.0f}};
    float pine_annulus_r0 = 140.0f;  ///< foothill band around the crag center
    float pine_annulus_r1 = 280.0f;
    float pine_strip_count = 4.0f;   ///< radial strips around the annulus
    float pine_strip_duty = 0.25f;   ///< strip fraction of each sector (§1.3 C1 knob;
                                     ///< tuned seed 1: C1 = 0.618 >= 0.6 with the
                                     ///< canopy-aware clearance raycast)
    glm::vec4 pine_strip{200.0f, 0.0f, 1024.0f, 120.0f}; ///< north ridge strip
    glm::vec2 forced_clearing_center{620.0f, 850.0f};    ///< forest-ruin clearing (§7.1)
    float forced_clearing_radius = 25.0f;                ///< §7.1 dungeon 2
};

/// The testbed layout table (LANDSCAPE.md §7.1) as generator parameters.
/// Defaults reproduce the design; tests may perturb copies.
/// Despite the name this struct is the layout of ANY stand (renaming it would
/// touch every consumer for zero behavior; the stand id below is the truth):
/// forest_stand_layout() (WorldgenForest.h) returns one with stand = Forest
/// and every testbed feature neutralized.
struct TestbedLayout {
    /// LF-8 (§2.10, в17): does this stand DECLARE the erosion overlay? Default
    /// false — the testbed's composition does not list it, and §2.10 rule 4
    /// makes an undeclared form a bug rather than a bonus. Setting it false on
    /// a stand that does declare it is that landform's NAMED CONTROL ("the same
    /// map with the pass OFF must fail the gully acceptance"), which is why the
    /// switch is here in the layout and not a build flag.
    bool erosion = false;

    /// Which stand this layout generates. Testbed default — the generator's
    /// testbed path must remain byte-identical when this field says Testbed.
    StandId stand = StandId::Testbed;

    CragStamp crag{};
    BumpStamp knoll{{560.0f, 620.0f}, 45.0f, 6.0f};  ///< shrine knoll +6 m (§7.1)
    BumpStamp bluff{{180.0f, 350.0f}, 35.0f, 10.0f}; ///< lakeshore cave bluff +10 m (§7.1)
    LakeStamp lake{};
    RiverLayout river{};

    /// Drainage valleys carrying the §7.1 river: [0] crag -> lake (inflow),
    /// [1] lake south rim -> south edge ≈ (300, 1024) (outflow, carries the
    /// (330, 840) ford). Outflow floor starts under the outlet-biased rim
    /// crest so the lake spill lands in it deterministically.
    /// Inflow bends SOUTH of the hamlet (360, 500) — the "river inflow bend"
    /// of §7.1 with the settlement on the dry north bank.
    ValleyTrough troughs[2] = {
        {{{760.0f, 300.0f}, {640.0f, 420.0f}, {480.0f, 555.0f}, {310.0f, 565.0f}},
         4, 80.0f, 22.0f, 16.5f, 5.0f, 0.6f},
        {{{240.0f, 600.0f}, {300.0f, 760.0f}, {330.0f, 900.0f}, {300.0f, 1024.0f}},
         4, 60.0f, 14.8f, 9.5f, 4.0f, 0.6f},
    };

    /// Sites in deterministic placement order (WorldEntityIds follow it).
    SiteLayout sites[6] = {
        {{360.0f, 500.0f}, SiteKind::Hamlet},          // hamlet "Vaelmere" (§7.1)
        {{560.0f, 620.0f}, SiteKind::Shrine},          // shrine knoll (§7.1)
        {{780.0f, 290.0f}, SiteKind::DungeonEntrance}, // dungeon 1: barrow (§7.1)
        {{620.0f, 850.0f}, SiteKind::DungeonEntrance}, // dungeon 2: forest ruin (§7.1)
        // Dungeon 3: lakeshore cave. HELD at the design position pending a
        // ruling: the blessed "move to the bluff foot" is unsatisfiable as the
        // bluff stands today — its base sits at ~12-14 m, BELOW the 15 m lake
        // plane, so a foot-level mouth breaks the ">= 2 m above the lake"
        // condition (measured, reported to design). Left here it generates a
        // valid sunken-barrow entrance rather than a flooded one.
        {{180.0f, 350.0f}, SiteKind::DungeonEntrance},
        {{830.0f, 200.0f}, SiteKind::TowerRuin},       // watchtower ruin on the crag (§7.1)
    };

    /// POI-chain corridors (§7.2 links; town→cave routed around the NE lake
    /// rim so the corridor never crosses open water — §3.4).
    CorridorLayout corridors[5] = {
        {{{360.0f, 500.0f}, {560.0f, 620.0f}}, 2},                     // town → shrine
        {{{560.0f, 620.0f}, {660.0f, 430.0f}}, 2},                     // shrine → watchpoint
        {{{660.0f, 430.0f}, {780.0f, 290.0f}}, 2},                     // watchpoint → barrow
        {{{560.0f, 620.0f}, {620.0f, 850.0f}}, 2},                     // shrine → forest ruin
        {{{360.0f, 500.0f}, {290.0f, 420.0f}, {180.0f, 350.0f}}, 3},   // town → cave (shore)
    };

    /// Foothill watchpoint minor POI (§7.1): forced outcrop cluster + lone pine.
    glm::vec2 watchpoint{660.0f, 430.0f};

    CastleLayout castle{}; ///< §6.1 — the seat of state power (CASTLE_COUNT_TESTBED)

    /// P7 carves (3D terrain). Waypoints are floor levels; see WorldgenCarve.h.
    /// The tunnel starts and ends in open air so both portals form naturally
    /// where the path enters and leaves the massif.
    /// L0 ascent scale: the switchback route was surveyed against a 52 m
    /// Ravenscar. With the summit at L0_RELIEF the whole climb has to grow with
    /// it or the tunnel ends up buried inside the mountain with no portals at
    /// all — the waypoints are absolute elevations, not fractions.
    static constexpr float ASCENT_SCALE = static_cast<float>(config::L0_RELIEF) / 52.0f;
    static constexpr float MOUTH_Y = 21.0f; ///< valley floor, unchanged by the summit
    static constexpr float lift(float y) { return MOUTH_Y + (y - MOUTH_Y) * ASCENT_SCALE; }
    /// The horizontal footprint stays PUT. Scaling it outward with the summit
    /// was the wrong instinct: it pushes the route toward the rim where the
    /// massif is thin, and the tunnel surfaced into a 349 m trench with only 25
    /// of 179 stations under rock. A steeper mountain wants the SAME tight
    /// footprint, where the rock above is deepest — the climb gets steeper
    /// instead of longer, which is what a switchback route is for.
    /// ...but it does have to move OUT far enough to clear the taller cone.
    /// At 115 m the stamp lifts ground to ~38 m where the 52 m survey put the
    /// mouth, burying it by 17 m. Pushing the footprint out by this much puts
    /// the mouth back where the cone falls below valley level, while keeping
    /// the switchbacks in deep rock. Far less than the summit's own scale —
    /// scaling by that put the route on the thin rim.
    static constexpr float FOOTPRINT_SCALE = 1.30f;
    static constexpr float spanx(float x) { return 830.0f + (x - 830.0f) * FOOTPRINT_SCALE; }
    static constexpr float spanz(float z) { return 200.0f + (z - 200.0f) * FOOTPRINT_SCALE; }

    CarveLayout carves{
        // Crag tunnel: mouth at the SW foot, four switchback legs with
        // landings, exit high on the SW flank overlooking the valley. Surveyed
        // at 52 m and scaled to the summit (see ASCENT_SCALE).
        // wp[6] moved (816, 228) -> (814.9, 225.6) in survey space: LATERAL
        // SWITCHBACK CLEARANCE. Legs 4->5 and 6->7 ran 3.3 m apart center to
        // center with 4 m wide boxes — a 0.7 m overlap strip where the upper
        // ramp's floor is silently the LOWER corridor's floor. At the 52 m
        // survey the floors differed by 1.7 m there (fragile but passable);
        // ASCENT_SCALE stretched the drop to 3.7 m, and the acceptance walker
        // drifted over the invisible edge, fell into leg 4->5 and was trapped
        // under the ramp (sim_tunnel_walk red, measured at (803.7, 241.4)).
        // The nudge holds the whole final leg >= 6 m from leg 4->5's line:
        // 2 x half_width + 2 m of real rock between the stacked passages.
        CarveCorridor{{{spanx(778.0f), MOUTH_Y, spanz(296.0f)},
                       {spanx(816.0f), lift(25.0f), spanz(268.0f)},
                       {spanx(820.0f), lift(25.5f), spanz(264.0f)},
                       {spanx(790.0f), lift(30.0f), spanz(248.0f)},
                       {spanx(786.0f), lift(30.5f), spanz(244.0f)},
                       {spanx(812.0f), lift(35.0f), spanz(232.0f)},
                       {spanx(814.9f), lift(35.5f), spanz(225.6f)},
                       {spanx(788.0f), lift(39.5f), spanz(238.0f)}},
                      8, 2.0f, 3.2f, /*daylight_portals=*/true},
        // Backbarrow: a short passage from the entrance into the hillside.
        CarveCorridor{{{780.0f, 20.2f, 292.0f}, {780.0f, 20.2f, 272.0f}}, 2, 1.8f, 2.6f},
        // ... opening into a burial chamber.
        CarveChamber{{780.0f, 20.2f, 264.0f}, {7.0f, 3.4f, 7.0f}},
    };

    ForestRegions forests{};
};

/// Distance (meters) from `world` to the nearest POI-chain corridor
/// centerline (§2.4 mask; pure layout geometry — used by scatter exclusion,
/// hydrology ford beds and validation). Corridor mask = distance <=
/// CORRIDOR_WIDTH / 2.
[[nodiscard]] inline float corridor_distance(const TestbedLayout& layout, glm::vec2 world) {
    float best = 3.402823466e+38f;
    for (const CorridorLayout& c : layout.corridors) {
        for (int i = 0; i + 1 < c.point_count; ++i) {
            const glm::vec2 a = c.points[i];
            const glm::vec2 ab = c.points[i + 1] - a;
            const float len2 = ab.x * ab.x + ab.y * ab.y;
            float t = 0.0f;
            if (len2 > 0.0f) {
                t = ((world.x - a.x) * ab.x + (world.y - a.y) * ab.y) / len2;
                t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            }
            const float dx = world.x - (a.x + ab.x * t);
            const float dz = world.y - (a.y + ab.y * t);
            const float d = std::sqrt(dx * dx + dz * dz);
            best = d < best ? d : best;
        }
    }
    return best;
}

} // namespace dfn::world
