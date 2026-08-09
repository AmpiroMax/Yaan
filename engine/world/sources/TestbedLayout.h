/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 16:47:51
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
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — testbed layout table per LANDSCAPE.md §7.1
  (lead-approved location as an additive WorldGenParams field).
- 09:08:2026 - 13:12:19: Stage 3b amendments (design 12:44:58): fords removed from RiverLayout (derived in P2 per §7.1a); pine annulus -> radial ridge strips with count/duty knobs (§1.3 C1, tuned seed 1); crag treeline knob; corridor_distance moved here (shared layout geometry).
- 09:08:2026 - 15:18:34: Castle (§6.1): CastleLayout — Harrowward's stamp target on the crag SW foot, §6.1.3 footprints, approach-corridor index (gate is valley-facing, pad never rotated).
- 09:08:2026 - 16:47:51: P7 carves: CarveCorridor/CarveChamber/CarveLayout — the crag switchback tunnel (8 waypoints, 4 legs, 3 landings, starts and ends in open air so the portals form where the path meets rock) and the Backbarrow passage + chamber.
*/

#pragma once

#include <cmath>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace dfn::world {

/// L0 ridged-noise crag stamp (LANDSCAPE §7.1 "Ravenscar Crag").
struct CragStamp {
    glm::vec2 center{830.0f, 200.0f}; ///< peak (§7.1)
    float radius = 180.0f;            ///< footprint (§7.1)
    float peak_height = 52.0f;        ///< target peak, m (§7.1; needs WORLDGEN_MAX_HEIGHT 64)
    float rockline = 34.0f;           ///< rock splat above this height on the stamp (§7.1)
    float treeline = 26.0f;           ///< treeless band starts here on the stamp (§1.3
                                      ///< C4 knob: keeps foothill canopy from out-angling
                                      ///< the L0; widen by lowering)
    float ridge_cell = 48.0f;         ///< ridged-noise lattice cell, m (stamp shape)
    float ridge_amp_frac = 0.25f;     ///< ridged modulation fraction of peak (stamp shape)
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
struct TestbedLayout {
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
        {{180.0f, 350.0f}, SiteKind::DungeonEntrance}, // dungeon 3: lakeshore cave (§7.1)
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
    CarveLayout carves{
        // Crag tunnel: mouth at the SW foot, four switchback legs with
        // landings, exit high on the SW flank overlooking the valley.
        CarveCorridor{{{778.0f, 21.0f, 296.0f},
                       {816.0f, 25.0f, 268.0f},
                       {820.0f, 25.5f, 264.0f},
                       {790.0f, 30.0f, 248.0f},
                       {786.0f, 30.5f, 244.0f},
                       {812.0f, 35.0f, 232.0f},
                       {816.0f, 35.5f, 228.0f},
                       {788.0f, 39.5f, 238.0f}},
                      8, 2.0f, 3.2f},
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
