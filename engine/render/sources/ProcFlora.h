/*
Created: 09:08:2026 - 19:26:55
Last updated: 13:08:2026 - 19:26:00
Module: engine/render
File: engine/render/sources/ProcFlora.h

Responsibility:
- Public API of the parametric flora generator: build a species mesh from
  (species, variant, per-instance shape, LOD), and derive per-instance shapes
  from a neighbourhood of scatter instances.

Key items:
- FloraLod, FloraShape, FloraMesh, build_flora_mesh(), append_flora(),
  analyse_neighbourhood(),
  species_nominal_height/crown_radius/crown_base/trunk_radius(),
  FLORA_VARIANTS.

Dependencies:
- Uses: FloraSpecies.h, FloraCards.h, ProcMesh.h (MeshData + tri/quad/pack,
  render's, public by agreement 09:08:2026), core math ScatterInstance, glm.
- Used by: ScatterBatcher (render), RenderSystem (the leaf atlas),
  ProcFloraTests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md.
- PURE AND DETERMINISTIC: no GPU, no ECS, no globals, no wall-clock, no IO.
  Same inputs must give byte-identical buffers (Rule 13.1 discipline).
- Does NOT decide placement (core), the catalog (design), LOD SELECTION,
  instancing or materials (render), or collision (sim).
- A tree comes out as TWO meshes and they must never be merged: see FloraMesh.
*/
/*
UPD:
- 09:08:2026 - 19:26:55: Created — stage-4 parametric branching system.
- 09:08:2026 - 20:21:13: Alpha-cutout leaf cards: build_flora_mesh now returns
  FloraMesh (the wood stream and the card stream, which carry different
  vertex-colour meanings and so must be different draws); append_flora() for
  the batcher; FloraShape::wind_phase; season argument.
- 10:08:2026 - 01:59:06: flora_maturity_for() — the §5.10 maturity-tier draw
  gets its one home, for core to call when filling ScatterInstance.scale.
- 10:08:2026 - 11:51:23: flora_maturity_for() moved to core/math and is
  imported here — core's canopy occlusion envelope is defined from its
  multiplier bands, so the draw gained a second zone (Rule 35).
- 10:08:2026 - 11:59:40: flora_owns() — the ROUTING PREDICATE, so render asks
  flora which ordinals take the flora path instead of keeping a list that can
  drift. Added after render found that core's 5->18 enum growth left every new
  ordinal drawing NOTHING, silently, with both suites green.
- 12:08:2026 - 00:20:00: FloraShape::crown_width_mult (width varies on its own
  axis, or trees of equal height are still copies), FloraShape::chained (the
  named лукоморье oak), and the lean's docstring restated as a WIND response
  with an azimuth source rather than a per-tree jitter.
- 13:08:2026 - 18:30:00: FloraStructure / build_flora_structure() -- the wood as
  STRUCTURE for sim and collide (requested by collide the same day). Until now
  this zone published three scalars and a triangle soup, so a body could only
  collide with a plumb capsule at the stump; the lean band has since opened to
  15-25 deg and a bole is a SWEPT axis, so at crown height the wood is metres
  from where a plumb capsule puts it. Measured on the built tree: an oak's bole
  walks 4.2 m sideways, a great oak's 10.4 m. Foliage is deliberately absent
  and must stay absent -- cards have no volume.
- 13:08:2026 - 19:45:00: FloraShape::crowd -- CROWN SHYNESS as a list of
  BOUNDARIES, one per crowding neighbour (user, 13.08.2026, with two photographs
  of canopy from below: «деревья не должны налезать кронами друг на друга»). The
  phenomenon has a name and this is it. The existing `shyness` scalar cannot
  express it: one direction, and it SHRINKS the crown, which is a smaller tree
  rather than a shy one.
- 13:08:2026 - 21:40:00: FloraShape::crowding -- how crowded this tree grew
  up, 0 open-grown .. 1 closed-forest.
- 13:08:2026 - 19:26:00: FloraShape::crown_base_override -- THE CONTROL DOOR FOR A
  REJECTED ARTEFACT. design §10.15.1 makes rebuilding the rejected birch a
  PRECONDITION of the palm gate, and no door existed to rebuild it with. This
  one replaces the crown base AFTER the per-instance spread is drawn, so the
  control tree and the accepted tree differ in THE BOLE AND NOTHING ELSE. A
  struct field and not an env var on purpose: the env arms are read once per
  process, and a separating threshold is a claim about two populations measured
  in ONE run.
*/

#pragma once

#include "engine/core/math/sources/FloraField.h"
#include "engine/core/math/sources/SurfaceField.h"
#include "engine/render/sources/FloraSpecies.h"
#include "engine/render/sources/ProcMesh.h"

#include <glm/vec2.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace dfn::render {

/// Geometry detail level. SELECTION IS RENDER'S DECISION — flora only supplies
/// the geometry for each level (docs/specs/flora.md §3.6).
enum class FloraLod : uint8_t {
    Full = 0,       ///< complete skeleton + clusters, <= TREE_TRI_BUDGET_MAX
    Reduced = 1,    ///< trunk + primaries + fewer clusters
    Silhouette = 2, ///< trunk column + one envelope shell
};

/// Pre-built skeleton variants per species. The whole trick that makes
/// unique-looking forests affordable: cost is O(species x variants x lods),
/// never O(instances), while FloraShape supplies per-instance difference.
inline constexpr uint32_t FLORA_VARIANTS = 12;

/// Per-instance shape modifiers. Derived from the neighbourhood, NOT carried on
/// math::ScatterInstance — that contract is frozen (Rule 26) and gains nothing.
struct FloraShape {
    float maturity = 1.0f;     ///< 0.4 sapling .. 1.5 giant; scales height
    /// Per-instance multiplier on the crown's WIDTH ONLY, on top of the
    /// species allometry. Its own axis of variation on purpose: height spread
    /// alone still lets two trees of equal height be identical, and a forest
    /// where every 28 m tree has the same 20 m crown is a plantation with a
    /// height histogram.
    float crown_width_mult = 1.0f;
    glm::vec2 lean_dir{0.0f};  ///< unit; direction the crown leans toward
    /// Rad off vertical. NOT a per-instance jitter: LANDSCAPE §10.3.1 rules
    /// that every tilt has an AZIMUTH SOURCE and only boulders may use a free
    /// one, and reference frame 16 is the evidence — the canopy leans TOGETHER.
    /// The azimuth here is the wind field's (see FloraNeighbours.cpp); the
    /// magnitude carries crowding on top of it.
    float lean = 0.0f;
    glm::vec2 shy_dir{0.0f};   ///< unit; direction of strongest crowding
    float shyness = 0.0f;      ///< 0..1 crown pullback along shy_dir
    /// CROWN SHYNESS AS A BOUNDARY, one per crowding neighbour (user,
    /// 13.08.2026, with two photographs of canopy from below: «деревья не
    /// должны налезать кронами друг на друга… чтобы крона выглядела
    /// естественно и листва не перекрывает друг друга»).
    ///
    /// The phenomenon has a name — crown shyness — and it is a real one:
    /// neighbouring trees of the same storey stop short of each other and leave
    /// winding channels of sky between their crowns. It is what makes a canopy
    /// read as many trees instead of as felt.
    ///
    /// WHY A LIST OF BOUNDARIES AND NOT THE `shyness` SCALAR ABOVE. That scalar
    /// is ONE direction — the worst crowding — and it SHRINKS the whole crown
    /// toward the trunk. A tree with three neighbours does not become a smaller
    /// ball; it becomes a crown with three flats on it, and the flats are where
    /// the sky channels are. Shrinking cannot produce a channel: it moves the
    /// whole outline inward, so the gap it opens is the same gap on every
    /// bearing, which is a smaller tree rather than a shy one.
    ///
    /// `dir` is the unit XZ bearing to the neighbour; `limit` is how far the
    /// wood may reach along it, measured from the crown axis. Growth is vetoed
    /// past it — see fractal_skeleton, where a shoot that would cross simply
    /// stops and carries its foliage at the stop.
    struct CrownEdge {
        glm::vec2 dir{0.0f};
        float limit = 0.0f; ///< m from the crown axis along dir
    };
    /// EIGHT, AND THE NUMBER IS THE STAND'S, NOT A GUESS. On a jittered
    /// lattice at the spacing the user has asked for (2-3x closer than the
    /// 12-18 m brief, i.e. 5-8 m) a tree has about eight neighbours whose
    /// crowns would reach it. Four slots leave the other four bearings
    /// unconstrained, and an unconstrained bearing is where the crowns weld:
    /// measured on a six-by-six stand at 8 m, four slots left 44.9 % of the
    /// canopy double-covered where eight leave far less. A slot is 12 bytes.
    static constexpr int CROWD_MAX = 8;
    CrownEdge crowd[CROWD_MAX]{};
    uint8_t crowd_count = 0;
    /// HOW CROWDED THIS TREE GREW UP, 0 open-grown .. 1 closed-forest.
    ///
    /// The user asked for «7-10 видов лиственных для ПЛОТНОГО стояния и ещё 5
    /// для свободного роста». This is that request answered as ONE LAW with two
    /// ends instead of fifteen hand-fitted sets, and the law is not invented —
    /// it is the botany already written into this catalog's own oak row: an
    /// open-grown Quercus carries a crown 0.8-1.0 of its height across, a
    /// closed-forest one 0.4-0.5. A tree that grew up in a crowd IS a different
    /// shape, and it is the SAME species.
    ///
    /// It also resolves what looked like two contradictory requests two days
    /// apart — «сделать деревья шире» and «лес плотнее» — into one rule that
    /// grants both: room makes a tree wide, crowding makes it narrow.
    float crowding = 0.0f;
    bool understory = false;   ///< raise crown base, narrow crown, shorten
    /// THE REJECTED ARTEFACT, REBUILDABLE — the control design §10.15.1 makes a
    /// PRECONDITION of the palm gate, not an optional extra: "the rejected
    /// birch is rebuilt as the control, because a synthetic control is the easy
    /// reject and the artefact the user actually turned down is the hard one."
    ///
    /// 0 means "the species' own value" and every shipped caller leaves it
    /// there; a positive value replaces the crown base fraction AFTER the
    /// per-instance spread is drawn, so the rng stream is untouched and the
    /// control tree differs from the accepted one in THE BOLE LENGTH AND
    /// NOTHING ELSE — same width draw, same lean, same variant. That is the
    /// whole point: the accepted birch and the rejected birch differ in exactly
    /// one authored input, and a control that also reshuffles the draw answers
    /// "did anything change" instead of "did THIS change".
    ///
    /// A door and not an env var ON PURPOSE. The env arms in FloraSpecies.h are
    /// read once per process, so a test cannot hold both arms side by side —
    /// and a separating threshold is a statement about two populations measured
    /// in ONE run, on one binary, from one build of the geometry.
    float crown_base_override = 0.0f;
    /// THE NAMED TREE. Set by the placer for the ONE great oak on the sea
    /// cliff, which carries a golden chain around its bole (the user's
    /// лукоморье). Everything about WHERE it stands is core's and design's;
    /// this flag is the whole of flora's side of the contract.
    bool chained = false;
    /// 0..1, this instance's wind phase (vertex GREEN). Derived from the
    /// instance POSITION in analyse_neighbourhood, because a stand whose trees
    /// share a phase does not ripple, it pulses as one object.
    float wind_phase = 0.0f;
};

/// A tree is TWO meshes, and merging them is a bug that looks like an
/// optimisation. On render's "prop" program vertex colour is ALBEDO; on their
/// "foliage" program the same four bytes are WIND DATA (r sway weight, g
/// instance phase, b per-card value jitter, a sky visibility) and the albedo
/// comes from the leaf atlas instead. Same bytes, different meaning, therefore
/// different draws.
struct FloraMesh {
    MeshData wood;  ///< trunk, branches, cone tiers, silhouette shells ("prop")
    MeshData cards; ///< alpha-cutout leaf cards ("foliage" + the leaf atlas)
};

/// --- THE WOOD AS STRUCTURE, FOR THE ZONES THAT HAVE TO TOUCH IT -----------
///
/// Requested by `collide` 13.08.2026 and it is a real gap rather than a
/// convenience: until now flora handed out three scalars (trunk radius, crown
/// base, crown radius) and a triangle soup, so a body could only ever collide
/// with a VERTICAL CAPSULE at the stump. That was adequate while every tree
/// stood plumb. It is not adequate now — the lean band opened to 15-25 deg and
/// the bole is a SWEPT axis, so at crown height the wood is metres away from
/// where a plumb capsule puts it, and no test either zone could write would
/// catch the divergence: both would be self-consistent and disagree about the
/// world. The user also asked to CLIMB the great oak, and there is nothing to
/// climb without branch positions and radii.
///
/// IT IS THE SAME WOOD THAT IS DRAWN, BY CONSTRUCTION. The structure is filled
/// by the same emitters that produce the mesh, on the same pass, from the same
/// (species, variant, shape, seed) — not rebuilt by a parallel routine. A second
/// derivation of the same geometry is a divergence with a date on it, and this
/// zone has spent two days on exactly that failure in a smaller place (the card
/// legibility floor, which existed in three copies).
struct FloraBranch {
    glm::vec3 a{0.0f};      ///< parent end, TREE-LOCAL (y = 0 at the root flare)
    glm::vec3 b{0.0f};      ///< child end
    float radius_a = 0.0f;  ///< m
    float radius_b = 0.0f;  ///< m
    bool trunk = false;     ///< part of the authored bole
    uint8_t order = 0;      ///< 0 on the trunk axis, +1 past every fork
};

/// Climbing furniture (the great oak only). GEOMETRY, and whether a body may
/// stand on it is sim's to decide — flora states where the shapes are.
struct FloraFurniture {
    glm::vec3 centre{0.0f}; ///< tread root / platform centre, tree-local
    glm::vec3 out{0.0f};    ///< tread direction and length; zero for a platform
    float radius = 0.0f;    ///< tread half-thickness, or platform radius
    bool platform = false;
};

struct FloraStructure {
    std::vector<FloraBranch> branches;
    std::vector<FloraFurniture> furniture;
    float height = 0.0f;
    float crown_base = 0.0f;
    float crown_top = 0.0f;
    float crown_radius = 0.0f;
    float trunk_radius = 0.0f; ///< at the base, EXCLUDING the flare
    /// XZ centre of the crown. NOT the origin once a tree leans (LANDSCAPE
    /// §10.3): a leaning tree carries its crown over the top of its bole.
    glm::vec2 crown_axis{0.0f};
};

/// The wood of one tree, in the same local frame as build_flora_mesh().
///
/// FOLIAGE IS DELIBERATELY ABSENT AND MUST STAY ABSENT: leaf cards are
/// alpha-cutout quads with no volume, and colliding with them would put a wall
/// where the player can see sky. Nothing in this struct is foliage.
[[nodiscard]] FloraStructure build_flora_structure(FloraSpecies species,
                                                   uint32_t variant,
                                                   const FloraShape& shape,
                                                   FloraLod lod = FloraLod::Full);

/// The canonical builder. Deterministic in every argument.
///
/// `season` changes GEOMETRY only for winter, where deciduous species emit no
/// cards at all (LANDSCAPE §5.11's one boolean) and the bare skeleton becomes
/// the tree. Summer and autumn produce identical geometry: the card stores an
/// atlas TILE, the atlas stores the colour, so those two seasons are a texture
/// regeneration and nothing else — no mesh, no chunk, no baked jitter.
[[nodiscard]] FloraMesh build_flora_mesh(FloraSpecies species, uint32_t variant,
                                         const FloraShape& shape, FloraLod lod,
                                         FloraSeason season = FloraSeason::Summer);

/// Bakes one instance into the two world-space streams (the batcher's entry
/// point). Scale is 1.0 by contract: maturity is already inside the mesh, and
/// trees do not sink — they stand on their root flare (§3.5).
void append_flora(MeshData& wood, MeshData& cards, FloraSpecies species,
                  uint32_t variant, const FloraShape& shape, FloraLod lod,
                  glm::vec3 position, float yaw,
                  FloraSeason season = FloraSeason::Summer);

/// Variant index for a world position (stable across runs and chunk borders).
[[nodiscard]] uint32_t flora_variant_for(glm::vec2 world_xz);

/// The MATURITY-TIER DRAW (design §5.10: TREE_MATURITY_GIANT/MATURE/SUBMATURE/
/// YOUNG_PCT = 25/60/12/3, until now sixteen constants with zero consumers).
/// Returns the FloraShape::maturity multiplier for a tree standing at this
/// position: giant 1.15-1.50, mature 0.85-1.15, sub-mature 0.50-0.70 (design's
/// mid-canopy layer — do NOT collapse it into sapling, they are different
/// structural jobs), sapling 0.40-0.60. Deterministic and position-keyed like
/// flora_variant_for, so core can call it when filling ScatterInstance.scale
/// and the rule has exactly one home (Rule 35). Distribution is asserted over
/// its whole declared range in the suite (Rule 31).
/// MOVED to engine/core/math/sources/FloraField.h (10.08.2026) and imported
/// here so flora's call sites are unchanged: core's canopy occlusion envelope
/// is SPECIES_HEIGHT_MAX x TREE_MATURITY_GIANT_MULT_MAX, so the draw gained a
/// second zone and had to stop belonging to one (Rule 35). Same key, same
/// mixer, bit-identical results.
using math::flora_maturity_for;

/// Derives a FloraShape per instance from the instance array itself.
/// `all` may include neighbouring-chunk instances; results are returned for
/// the first `count` entries (pass instances.size() for all of them).
[[nodiscard]] std::vector<FloraShape>
analyse_neighbourhood(std::span<const math::ScatterInstance> all, size_t count);

/// Maps core's placement species onto the flora catalog.
[[nodiscard]] FloraSpecies flora_species_of(math::ScatterSpecies species);

/// **THE ROUTING PREDICATE — one source of truth, so render never keeps a
/// species list that can drift from this one.** True when the instance's mesh
/// comes from `append_flora`/`build_flora_mesh`; false only for the classes
/// render meshes itself (today: `Stone`).
///
/// This exists because the alternative already bit us. `ScatterBatcher` named
/// Oak/Pine/Birch in a local `is_tree()` and `build_scatter_mesh` switched over
/// the original five species, so when core grew `ScatterSpecies` from 5 to 18
/// every new ordinal fell through to an EMPTY MeshData and was silently
/// skipped: core placed snags, big bushes, fallen logs and deadfall, and the
/// forest floor drew as bare earth with both zones' suites green. Absence
/// presenting as a neutral state — the same failure that hid the missing site
/// meshes for a whole stage.
///
/// So the predicate is asked of flora rather than restated by render, and the
/// suite asserts that every species this returns true for actually BUILDS
/// non-empty geometry. A new ordinal is then either meshed or loudly missing,
/// never quietly nothing.
[[nodiscard]] bool flora_owns(math::ScatterSpecies species);

// --- Metadata other zones need without pulling in the tables ---------------
[[nodiscard]] float species_nominal_height(FloraSpecies);
[[nodiscard]] float species_crown_radius(FloraSpecies);
[[nodiscard]] float species_crown_base(FloraSpecies);
/// Base trunk radius INCLUDING the root flare's widest point (sim's collision
/// query — see docs/specs/flora.md §3.5).
[[nodiscard]] float species_trunk_radius(FloraSpecies);

} // namespace dfn::render
