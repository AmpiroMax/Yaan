/*
Created: 09:08:2026 - 19:31:02
Last updated: 12:08:2026 - 00:36:00
Module: engine/render
File: engine/render/sources/ProcFlora.cpp

Responsibility:
- Assembles one tree: the authored trunk with its root flare, the CROWN grown by
  space colonization (broadleaves) or by whorls (conifers), the foliage hung off
  the nodes that grew to reach it, the LOD ladder, and logs/snags/bushes.

Key items:
- build_flora_mesh, append_flora, build_crown, build_trunk, build_silhouette,
  flora_variant_for, species metadata.

Dependencies:
- Uses: ProcFlora.h, FloraBuild.h (Tree + the geometry primitives),
  FloraSkeleton.h (the growers), FloraSpecies.h, Constants.h.
- Used by: ScatterBatcher (render), ProcFloraTests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md,
  algorithm record docs/specs/flora_algorithms.md.
- PURE + DETERMINISTIC. All randomness from the local splitmix64 keyed by
  (species, variant, node) — never a global RNG, never time.
- FOLIAGE HANGS OFF A NODE. There is no path in this file that places a leaf
  cluster from a volume, and adding one reintroduces exactly the geometry the
  user rejected on 09.08.2026 («листья, не прикрепляющиеся к ветвям»). The one
  exception is scatter_bush_clusters, and a bush is its own foliage.
- TWO HARD FLOORS, both load-bearing (flora.md §3.5): no wood below
  SHADOW_MIN_DIAMETER (thinner casts NO shadow at SHADOW_TEXEL_M 0.156 and
  shimmers at 640x360 — we do not model twigs), enforced by CLAMPING UP rather
  than by terminating a branch; and canopy species keep CANOPY_CLEARANCE_MIN of
  clear trunk.
*/
/*
UPD:
- 09:08:2026 - 19:31:02: Created — stage-4 parametric branching system.
- 09:08:2026 - 20:21:13: Broadleaf foliage is now ALPHA-CUTOUT CARDS. Solid
  blob clusters survive only for bushes and the Silhouette LOD. A tree is built
  as two streams (FloraMesh.wood / .cards) because the two render programs read
  vertex colour differently; per-instance wind phase is derived from the
  instance position in analyse_neighbourhood; append_flora() bakes both streams
  for the batcher.
- 09:08:2026 - 21:02:02: Card legibility floor: a card shrunk by envelope
  containment below a quarter of the crown radius is not emitted, because it
  renders as a detached scrap of foliage rather than as part of the crown.
  Reduced LOD now spends its saving on fewer/larger clusters and one plane less
  per cluster, rather than only on the skeleton.
- 09:08:2026 - 21:18:02: CROWN ASPECT CEILING (design's §5 ruling): the crown
  base is DERIVED from CROWN_ASPECT_MAX rather than exempted per species, and
  the card's vertical clamps now use its CORNER reach instead of its
  half-height — a tilted, rolled card reaches its corners, and clamping on
  half-height pushed the measured foliage box outside its container, which is
  the very quantity the ceiling is measured on.
- 09:08:2026 - 23:52:07: THE ALGORITHM CHANGED (user rejected all three trees).
  scatter_envelope_clusters() and the recursive grow_branch() are GONE. Crowns
  are now grown by space colonization (Runions/Lane/Prusinkiewicz 2007) into the
  species envelope, so every leaf cluster hangs off a node that grew to reach
  it; conifers get an explicit whorl generator instead, because a conifer is
  monopodial and rhythmic rather than competitive, and a stack of solid cone
  tiers is precisely «юбки». Branch radii come from the pipe model over trunk
  and crown as one structure. File split for Rule 21: the geometry primitives
  moved to FloraBuild.{h,cpp} and the neighbour analysis to FloraNeighbours.cpp.
- 10:08:2026 - 01:59:06: §5.10 forest floor built as OBJECTS: snags gain a
  broken blunt top with shards and truncated stubs on the real swept axis
  (TrunkRing path — the §3.7 lesson applied in advance), with the snag split
  as one geometry / two materials (SnagPale seeds as Snag); fallen logs gain
  butt swell, an upturned root plate, snapped stubs and upper-side moss in
  cell-noise patches, ground contact along their whole length asserted with a
  floated control. flora_maturity_for() gives the 25/60/12/3 tier draw its one
  home. Card LODs keep >= 3 planes per cluster (render-spec floor: plane count
  buys angular coverage, and the edge-on failure is angle-, not distance-,
  dependent).
- 10:08:2026 - 11:51:23: flora_maturity_for() definition removed (moved to
  core/math, bit-identical: same key, same splitmix64). flora_species_of()
  spells out §5.10/§5.11/§5.12 explicitly — its `default` returns Bush, so an
  unmapped species does not fail to build, it draws a forest floor of snags and
  logs as a field of shrubs.
- 10:08:2026 - 11:59:40: flora_owns() implemented as an exhaustive switch with
  NO default, so a new ScatterSpecies breaks the build here rather than
  silently answering for a species nobody has considered.
- 10:08:2026 - 22:54:58: DFN_FLORA_ONLY=1|2 (wood only / cards only) — a Rule 30
  control with the same standing as render's DFN_NO_SCATTER, added because the
  cheap instrument was wrong: classifying the flipping PIXELS by colour counts
  every shaded leaf card as trunk (an oak's card and its bark are both under
  luma 55) and answered "93 % wood" with confidence. Drawing one mesh at a time
  says instead that the WOOD carries the near canopy (0.864 -> 0.382 % with the
  trunks gone) and the CARDS carry the treeline, and that wood alone is worse
  than the whole tree at BOTH vantages — a crown in front of a bole does not add
  a flicker, it buries one.
- 12:08:2026 - 00:20:00: THE FOREST GAINED ITS SPREAD, ITS WIDTH AND ITS LEAN,
  and the great oak arrived as a species (user, 11.08.2026). Per-instance crown
  width on three axes (species ratio re-derived, allometry against maturity,
  own per-instance draw); the crown BASE drawn over the lower half of design's
  approved band; the crown envelope now follows the top of a LEANING bole
  (t.crown_axis) instead of standing over the roots -- which is what the old
  0.12 rad lean cap was really protecting. GreatOak builds through
  build_great_crown() with the fractal grower, climb treads, fork platforms and
  the golden chain. DFN_FLORA_CONTROL=1 is the zero-dose arm for all of it and
  DFN_FLORA_GREAT_OAK the capture-only placement stand-in.
  MEASURED WRONG TWICE ON THE WAY, both recorded at their site: the crown axis
  may not follow the bole's own SWEEP (a plumb birch lost 65 % of its presented
  area to a shift its branches had already made), and the conifer is exempt
  from it entirely because whorl_skeleton builds on the straight stem.
- 12:08:2026 - 00:36:00: The great oak's fractal growth is bounded by the crown
  volume, its node budget raised to 1500 (the tip count, not cluster_count, is
  what caps foliage) and its segment cap lowered to 300. FOUND BY SHOOTING THE
  FRAME AT ITS OWN READ DISTANCE, which is the thing that had never been done:
  from 400 m the giants read as DEAD, and the two defects behind that -- zero
  cards, and wood at twice the declared height -- were both invisible from
  underneath and invisible to a green suite.
*/

#include "engine/render/sources/ProcFlora.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/render/sources/FloraBuild.h"
#include "engine/render/sources/FloraSkeleton.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace dfn::render {

namespace {

constexpr float CLEARANCE_MIN = static_cast<float>(config::CANOPY_CLEARANCE_MIN);

// --- THE GREAT OAK'S OWN BUDGET ---------------------------------------------
// TREE_TRI_BUDGET_MAX is 700 and it is the right number for a tree that stands
// forty to a hectare. This species stands about one to fifty hectares and its
// crown is 80 m across, so by the project's own read-distance rule (Rule 33:
// readable size = distance / 30) it is still an OBJECT at 2.4 km — it is on
// screen for the length of a journey rather than for the length of a glance.
// Spending 6x the ordinary tree on ~1/2000 of the instances is a net saving
// against drawing it as a forest oak and being asked why the landmark looks
// like scenery.
// NUMBERS ROWS REQUESTED FROM LEAD (Rule 14/35), carried locally until they
// land: GREAT_OAK_TRI_BUDGET 3500, GREAT_OAK_STEP_RISE 0.42 m,
// GREAT_OAK_STEP_REACH 0.75 m, GREAT_OAK_PLATFORM_RADIUS 2.6 m.
// RAISED 700 -> 1500 FROM A FRAME, not from a budget calculation. At 700 the
// grower produced ~156 tips, the foliage path takes ONE mass per tip, and 156
// masses over an 80 m crown is transparent: photographed from 400 m the great
// oaks towered over the canopy exactly as intended and read as DEAD — bare
// branch systems against the sky. The node budget was the binding constraint on
// foliage the whole time and nothing in the code said so, because the count
// that looks like the foliage lever (cluster_count) is applied after it.
// Wood cost is unchanged: decimate_to(GREAT_OAK_MAX_SEGMENTS) still caps the
// skeleton at 400 segments and preserves foliage anchors, so the extra nodes
// buy leaf sites and are then dissolved.
constexpr uint32_t GREAT_OAK_MAX_NODES = 1500;
constexpr uint32_t GREAT_OAK_MAX_SEGMENTS = 300;
constexpr float GREAT_OAK_STEP_RISE = 0.42f;   ///< m of climb per tread
constexpr float GREAT_OAK_STEP_REACH = 0.75f;  ///< m the tread sticks out
constexpr float GREAT_OAK_STEP_RADIUS = 0.15f; ///< m, half-thickness of a tread
constexpr float GREAT_OAK_PLATFORM_R = 2.6f;   ///< m, a deck at a major fork
/// The golden chain of the user's лукоморье. Its own value band, well above
/// every bark and leaf tone in the catalog, because the whole point of it is to
/// be the one bright thing on a dark bole.
constexpr glm::vec3 CHAIN_GOLD{0.86f, 0.70f, 0.24f};

/// One sample of the swept trunk axis: where the bole actually IS at a given
/// parameter, so dead-wood detail (stubs, the broken top) attaches to the real
/// swept axis instead of to the notional straight one. Attaching to the
/// notional element instead of the thing that exists is the §3.7 pattern, and
/// this struct exists so the snag does not become its fifth instance.
struct TrunkRing {
    glm::vec3 pos{0.0f};
    glm::vec3 dir{0.0f, 1.0f, 0.0f};
    float radius = 0.0f;
};

/// Emits one skeleton as tapered tubes. Segments whose BOTH ends are authored
/// trunk nodes are skipped: the trunk mesh already covers that axis, and drawing
/// it twice doubles the bole's triangles for nothing.
void emit_skeleton(MeshData& m, Tree& t, const Skeleton& sk) {
    for (size_t i = 0; i < sk.nodes.size(); ++i) {
        const int par = sk.nodes[i].parent;
        if (par < 0) continue;
        const SkeletonNode& child = sk.nodes[i];
        const SkeletonNode& parent = sk.nodes[static_cast<size_t>(par)];
        if (child.trunk && parent.trunk) continue;
        const glm::vec3 a = parent.pos;
        const glm::vec3 b = child.pos;
        if (glm::length(b - a) < 1e-3f) continue;
        // Sides scale with the limb. A 0.35 m twig is one to three pixels at
        // gameplay distance and does not need five faces; the bole does. This is
        // Rule 33 applied inside one object — detail sized against what will be
        // resolved, not against the part's importance.
        const float r0 = parent.radius;
        const float r1 = child.radius;
        const int sides = (r0 > t.trunk_r * 0.45f) ? 5 : (r0 > t.trunk_r * 0.20f ? 4 : 3);
        // Thin wood takes the TWIG value, not the bole's. §3.10's measurement is
        // that the tracery reads by value contrast against the foliage, so a
        // pale-boled species whose twigs are also pale has no tracery — it has
        // scaffolding.
        const uint32_t col = (r0 > t.trunk_r * t.sp.twig_radius_frac) ? t.wood : t.twig;
        tube_segment(m, a, b, safe_normalize(b - a, glm::vec3{0.0f, 1.0f, 0.0f}), r0,
                     r1, sides, col);
    }
}

/// Seeds the skeleton with the authored bole so that branches can leave the
/// trunk ANYWHERE in the crown span, not only at its tip. An oak whose limbs all
/// spring from one point is a candelabra; a real bole sheds limbs over several
/// metres of its length.
void seed_trunk_nodes(Skeleton& sk, glm::vec3 stem_base, glm::vec3 stem_top) {
    const int steps = 5;
    SkeletonNode root;
    root.pos = stem_base;
    root.trunk = true;
    sk.nodes.push_back(root);
    for (int s = 1; s <= steps; ++s) {
        const float f = static_cast<float>(s) / static_cast<float>(steps);
        SkeletonNode n;
        n.pos = stem_base + (stem_top - stem_base) * f;
        n.parent = static_cast<int>(sk.nodes.size()) - 1;
        n.trunk = true;
        sk.nodes.push_back(n);
        ++sk.nodes[static_cast<size_t>(n.parent)].children;
    }
}

/// Hangs one foliage cluster on a node that actually exists. `anchor` is the
/// skeleton node the cluster grew from, and it becomes the card's sway origin,
/// so the sway gradient runs outward from the LIMB rather than from the crown
/// base — which is both physically right and a better-looking rustle.
void emit_anchored_foliage(MeshData& m, Tree& t, const Skeleton& sk,
                           uint32_t target_clusters, int cards_per_cluster) {
    if (!emits_clusters(t.sp)) return;
    std::vector<glm::vec3> centres;
    std::vector<int> anchors;
    std::vector<float> reach;
    if (t.sp.envelope == CrownEnvelope::Cone) {
        // CONIFERS DO NOT MERGE THEIR FOLIAGE. Merging is right for a broadleaf,
        // whose leaf sites genuinely form a few big cloud masses; it is wrong
        // for a conifer, whose foliage is a SLEEVE on each shoot. Merged, two
        // branches' needles collapse into one cluster hanging in the air between
        // them — which is both uglier and, measurably, further from its wood.
        // One spray per shoot, subsampled to the budget, is what a spruce is.
        const size_t n = sk.leaf_sites.size();
        const size_t want = std::max<size_t>(1, target_clusters);
        const size_t stride = std::max<size_t>(1, n / want);
        for (size_t i = 0; i < n; i += stride) {
            centres.push_back(sk.leaf_sites[i]);
            anchors.push_back(sk.leaf_anchor[i]);
            reach.push_back(0.0f);
        }
    } else {
        gather_foliage_anchors(sk, target_clusters, centres, anchors, reach);
    }
    for (size_t i = 0; i < centres.size(); ++i) {
        const int a = anchors[i];
        if (a < 0) continue;
        t.sway_from = sk.nodes[static_cast<size_t>(a)].pos;
        // Cluster size is the species' declared fraction of the crown, floored
        // by what the merge actually gathered. Few and LARGE is the user's
        // «большими плоскими наборами листочков» and it is also what makes a
        // crown read as one mass rather than as confetti.
        const float r = std::max(t.crown_r * t.sp.cluster_radius_frac, reach[i]);
        emit_cluster(m, t, centres[i], r * shy_scale(t, centres[i] - t.stem_off),
                     cards_per_cluster);
    }
    // THE APEX CLUSTER. Merging leaf sites into clusters puts each centre at the
    // MEAN of its members, which pulls the topmost cluster down — measured, the
    // crown consistently topped out at 0.93 of the species height, and the height
    // band is a CROSS-ZONE CONTRACT (core's canopy occlusion and design's C4
    // arithmetic key off OAK/PINE/BIRCH_HEIGHT_MAX). Undershooting is the safe
    // direction, but a tree that never reaches its own declared height is still a
    // tree measured against a model nobody else shares. A real crown's topmost
    // shoot IS at its top, so this is honest geometry rather than a fudge: one
    // cluster on the HIGHEST node there is, lifted by no more than its own reach
    // so it still overlaps the wood it hangs on.
    int apex = -1;
    float best_y = t.crown_base;
    for (size_t v = 0; v < sk.nodes.size(); ++v) {
        if (sk.nodes[v].pos.y > best_y) {
            best_y = sk.nodes[v].pos.y;
            apex = static_cast<int>(v);
        }
    }
    if (apex >= 0) {
        const glm::vec3 n = sk.nodes[static_cast<size_t>(apex)].pos;
        t.sway_from = n;
        const float r = t.crown_r * t.sp.cluster_radius_frac;
        emit_cluster(m, t, glm::vec3{n.x, std::min(n.y + r * 0.80f, t.crown_top), n.z},
                     r * shy_scale(t, n - t.stem_off), cards_per_cluster);
    }
    t.sway_from = glm::vec3{t.stem_off.x, t.crown_base, t.stem_off.z};
}

/// Bushes only. A bush has no branch skeleton worth growing — it IS its
/// foliage, at 1-4 m, and design ruled that only TREE foliage becomes cards. So
/// this is the one place foliage is still placed from a volume, and that is
/// legitimate rather than an exemption: the complaint the rest of this file
/// answers is that a LEAF CLUSTER hung in the air where no branch reaches, and a
/// bush's mass reaches the ground it grows out of.
void scatter_bush_clusters(MeshData& m, Tree& t, glm::vec3 off) {
    const SpeciesParams& sp = t.sp;
    if (!emits_clusters(sp)) return;
    const float span = t.crown_top - t.crown_base;
    if (span <= 0.0f) return;
    const int count = (t.lod == FloraLod::Reduced)
        ? std::max(2, sp.cluster_count * 3 / 5)
        : sp.cluster_count;
    for (int i = 0; i < count; ++i) {
        const float u = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
        const float y = t.crown_base + span * std::pow(u, 0.7f);
        const float env = envelope_radius(t, y);
        const float az = GOLDEN_ANGLE * static_cast<float>(i);
        const float rf = 0.45f * (0.3f + 0.7f * std::fabs(std::sin(az * 1.7f)));

        const glm::vec3 at{std::cos(az) * env * rf, y, std::sin(az) * env * rf};
        emit_cluster(m, t, at + off, t.crown_r * sp.cluster_radius_frac);
    }
}

/// THE CROWN, and the one function whose construction answers the user's
/// complaint. Space colonization (Runions, Lane & Prusinkiewicz 2007) grows the
/// branches INTO the foliage volume: an attraction point survives only until a
/// node comes within the kill distance of it, so foliage placed on consumed
/// points cannot be detached from wood. See docs/specs/flora_algorithms.md
/// §1.3.1. This REPLACES the previous pairing of a recursive skeleton with a
/// crown scattered through the envelope "independently of the skeleton", which
/// measured a mean 2.6 m and a worst 6.9 m of air between an oak's leaves and
/// its nearest branch.
/// How many branch SEGMENTS the triangle budget still affords once the trunk and
/// the foliage cards are paid for, at up to 10 triangles per segment.
uint32_t max_crown_segments(const SpeciesParams& sp) {
    const auto cap = static_cast<uint32_t>(config::TREE_TRI_BUDGET_MAX);
    const auto committed = static_cast<uint32_t>(
        sp.trunk_sides * sp.trunk_segments * 2 + sp.trunk_sides * 2
        + sp.cluster_count * sp.cards_per_cluster * 2);
    // 11, not 10: a segment is 3, 4 or 5 sided (Rule 33 — sides sized against
    // what the limb will actually resolve to), so 10 is the worst case and 11
    // leaves the apex cluster somewhere to live.
    return ((cap > committed) ? (cap - committed) : 60u) / 11u;
}

/// CLIMBING FURNITURE — what flora hands to the zones that will make this a
/// place people live in. GEOMETRY ONLY, and the boundary is worth stating
/// because three zones meet on it:
///   - flora (here) emits the treads and the decks, at real climbable rise and
///     reach, on the ACTUAL swept bole rather than on a notional straight one;
///   - sim owns whether they are solid and how a body moves on them;
///   - core/design own who lives up there and what they build;
///   - render owns how they are lit and batched.
/// The contract flora offers outward is: a tread every GREAT_OAK_STEP_RISE of
/// climb, spiralling by the golden angle so no two are stacked; a deck of
/// GREAT_OAK_PLATFORM_R at each first-order fork; both centred on the bole axis
/// at their own height, so anything that wants to snap to them can find them by
/// re-walking the trunk path this function was handed.
void build_climb_steps(MeshData& m, Tree& t, const std::vector<TrunkRing>& path,
                       float top_y) {
    const SpeciesParams& sp = t.sp;
    if (sp.climb_steps == 0 || path.size() < 2) return;
    const float y0 = std::max(1.2f, path.front().pos.y + 0.6f);
    const int n = static_cast<int>(sp.climb_steps);
    for (int i = 0; i < n; ++i) {
        const float y = y0 + GREAT_OAK_STEP_RISE * static_cast<float>(i);
        if (y > top_y - 0.5f) break;
        // Find the bole at this height on the REAL swept path (the §3.7 lesson:
        // attach to the thing that exists, not to the notional axis).
        size_t k = 0;
        while (k + 1 < path.size() && path[k + 1].pos.y < y) ++k;
        const TrunkRing& ring = path[k];
        const float az = GOLDEN_ANGLE * static_cast<float>(i);
        const glm::vec3 u = perp_of(ring.dir);
        const glm::vec3 v = glm::cross(ring.dir, u);
        const glm::vec3 out = u * std::cos(az) + v * std::sin(az);
        const glm::vec3 base = glm::vec3{ring.pos.x, y, ring.pos.z}
            + out * (ring.radius * 0.75f);
        // Slight downward slope outward: a tread you can stand on reads as one
        // only if it is not a spike, and 4 sides at 0.30 m across is 4 px of
        // silhouette at 20 m — the distance this tree is climbed from.
        const glm::vec3 dir = safe_normalize(out - glm::vec3{0.0f, 0.08f, 0.0f}, out);
        tube_segment(m, base, base + dir * GREAT_OAK_STEP_REACH, dir,
                     GREAT_OAK_STEP_RADIUS, GREAT_OAK_STEP_RADIUS * 0.8f, 4, t.wood);
    }
}

/// A deck at a fork: the flat thing a dwelling can stand on. Deliberately a
/// disc rather than a platform with a rail — the rail is architecture and
/// architecture is not this zone's.
void build_climb_platforms(MeshData& m, Tree& t, glm::vec3 fork, int count,
                           Rng& rng) {
    for (int i = 0; i < count; ++i) {
        const float az = GOLDEN_ANGLE * static_cast<float>(i) + rng.unit();
        const float d = (i == 0) ? 0.0f : GREAT_OAK_PLATFORM_R * (1.1f + rng.unit());
        const float r = GREAT_OAK_PLATFORM_R * (0.85f + rng.unit() * 0.5f);
        const glm::vec3 c = fork
            + glm::vec3{std::cos(az) * d, 1.0f + rng.unit() * 2.5f, std::sin(az) * d};
        blob_cluster(m, c, {r, 0.18f, r}, 7, 2, t.twig);
    }
}

/// THE GOLDEN CHAIN (user: «на дубе будет золотая цепь, как из сказки
/// Пушкина»). One loop around the bole with a sag between its anchor points,
/// which is the whole of what makes a chain read as a chain rather than as a
/// gold ring: a hoop looks machined, a catenary looks hung.
void build_golden_chain(MeshData& m, Tree& t, const std::vector<TrunkRing>& path,
                        float y) {
    if (path.size() < 2) return;
    size_t k = 0;
    while (k + 1 < path.size() && path[k + 1].pos.y < y) ++k;
    const TrunkRing& ring = path[k];
    const glm::vec3 u = perp_of(ring.dir);
    const glm::vec3 v = glm::cross(ring.dir, u);
    const uint32_t gold = pack(CHAIN_GOLD);
    constexpr int LINKS = 18;
    const float r = ring.radius * 1.18f;
    const float sag = ring.radius * 0.42f;
    glm::vec3 prev{0.0f};
    for (int i = 0; i <= LINKS; ++i) {
        const float f = static_cast<float>(i) / static_cast<float>(LINKS);
        const float az = TAU * f;
        // Two anchor points (front and back), so the sag is a double swag.
        const float swag = std::fabs(std::sin(az)) * sag;
        const glm::vec3 p = glm::vec3{ring.pos.x, y, ring.pos.z}
            + (u * std::cos(az) + v * std::sin(az)) * r
            - glm::vec3{0.0f, swag, 0.0f};
        if (i > 0) {
            tube_segment(m, prev, p, safe_normalize(p - prev, {1.0f, 0.0f, 0.0f}),
                         0.09f, 0.09f, 3, gold);
        }
        prev = p;
    }
}

/// THE GREAT OAK'S CROWN. Recursive ramification (FloraSkeleton's third
/// grower) instead of space colonization, because what the user asked to see is
/// the BRANCH SYSTEM: «ветки будут расти как фракталы». The envelope is still
/// enforced afterwards — the silhouette guarantee of flora.md §3.1 stage D is
/// not something this species is exempt from, it just is not what SHAPES the
/// tree here.
void build_great_crown(MeshData& m, Tree& t, glm::vec3 stem_base, glm::vec3 stem_top,
                       uint64_t seed) {
    const SpeciesParams& sp = t.sp;
    Skeleton sk;
    seed_trunk_nodes(sk, stem_base, stem_top);

    FractalParams fp;
    fp.base_y = stem_top.y;
    fp.trunk_top_r = t.trunk_r;
    fp.depth = sp.fractal_depth;
    fp.majors_min = sp.fractal_majors_min;
    fp.majors_max = sp.fractal_majors_max;
    fp.children_min = sp.fractal_children_min;
    fp.children_max = sp.fractal_children_max;
    fp.major_pitch = sp.fractal_major_pitch;
    fp.pitch_spread = sp.fractal_pitch_spread;
    fp.length_decay = sp.fractal_length_decay;
    fp.droop = sp.droop * 0.5f;
    fp.phototropism = sp.phototropism;
    // THE SPREAD TARGET IS THE USER'S RULE, PASSED THROUGH UNCHANGED: the
    // first-order limbs have to physically reach the crown radius, because on
    // this species the radius is the point. Inset slightly so the foliage that
    // hangs off the tips — not the wood — is what touches the envelope, which
    // is the lesson build_crown() paid for below.
    fp.spread_target = t.crown_r * (1.0f - sp.cluster_radius_frac * 0.9f);
    fp.lean = t.shape.lean > 0.0f
        ? t.shape.lean_dir * (t.shape.lean * 0.8f)
        : glm::vec2{0.0f};
    fp.top_y = t.crown_top;
    fp.max_radius = t.crown_r;
    fp.axis = t.crown_axis;
    fp.max_nodes = GREAT_OAK_MAX_NODES;
    fractal_skeleton(sk, fp, seed);

    assign_pipe_radii(sk, t.trunk_r, sp.pipe_exponent, SHADOW_MIN_DIAMETER * 0.5f);
    decimate_to(sk, GREAT_OAK_MAX_SEGMENTS);
    assign_pipe_radii(sk, t.trunk_r, sp.pipe_exponent, SHADOW_MIN_DIAMETER * 0.5f);
    emit_skeleton(m, t, sk);

    // ONE FOLIAGE MASS PER TIP, subsampled to the cluster budget — the conifer
    // rule, and for the same reason: merging leaf sites is right when they form
    // a few big clouds and wrong when each one is the end of a named limb. On a
    // tree whose branch system is the subject, merging would hide it.
    if (!emits_clusters(sp)) return;
    const size_t n = sk.leaf_sites.size();
    if (n == 0) return;
    const size_t want = std::max<size_t>(1, sp.cluster_count);
    const size_t stride = std::max<size_t>(1, n / want);
    for (size_t i = 0; i < n; i += stride) {
        const int a = sk.leaf_anchor[i];
        if (a < 0 || static_cast<size_t>(a) >= sk.nodes.size()) continue;
        t.sway_from = sk.nodes[static_cast<size_t>(a)].pos;
        emit_cluster(m, t, sk.leaf_sites[i], t.crown_r * sp.cluster_radius_frac);
    }
    t.sway_from = glm::vec3{t.stem_off.x, t.crown_base, t.stem_off.z};
}

void build_crown(MeshData& m, Tree& t, glm::vec3 stem_base, glm::vec3 stem_top,
                 uint64_t seed, float branch_floor) {
    const SpeciesParams& sp = t.sp;
    if (sp.envelope == CrownEnvelope::None) return;
    if (sp.fractal_depth > 0) {
        build_great_crown(m, t, stem_base, stem_top, seed);
        return;
    }

    Skeleton sk;
    // THE GROWTH ENVELOPE IS INSET FROM THE SILHOUETTE ENVELOPE, and getting
    // this wrong was worth more than every parameter in the table.
    //
    // Branch tips were grown to the species envelope, and the foliage that hangs
    // off a tip then has nowhere to go: containment allows a cluster only what
    // is left between its centre and the envelope, so a tip sitting exactly ON
    // the envelope got a cluster of 0.30 x env and the crowns came out as a big
    // skeleton wearing small tufts. It is the same confusion as measuring a
    // card's centre instead of its corner (flora.md §3.7), one level up: THE
    // ENVELOPE IS WHERE THE FOLIAGE ENDS, NOT WHERE THE WOOD ENDS. Real crowns
    // work this way too — the twigs stop short and the leaves make the surface.
    const CrownVolume shape_vol = crown_volume(t);
    CrownVolume vol = shape_vol;
    vol.radius = shape_vol.radius
        * std::clamp(1.0f - sp.cluster_radius_frac * 0.85f, 0.45f, 0.92f);

    if (sp.envelope == CrownEnvelope::Cone) {
        // CONIFERS DO NOT GET SPACE COLONIZATION (flora_algorithms.md §2). That
        // algorithm models COMPETITION; a conifer is monopodial and rhythmic —
        // one leader, a ring of laterals per growth flush. Feeding a
        // deterministic developmental pattern to a competition algorithm throws
        // away the only thing that makes a spruce recognisable.
        WhorlParams wp;
        wp.base = t.crown_base;
        wp.top = t.crown_top;
        wp.radius = vol.radius;
        wp.whorls = sp.whorl_count;
        // Reduced thins the pendulous SHOOTS and leans on the shared decimation
        // below; it deliberately does NOT change the whorl count. Changing a
        // count that seeds the rng makes Reduced a DIFFERENT tree rather than a
        // cheaper one, and a different tree can cost more — measured, a Reduced
        // pine came out at 322 triangles against a Full 318 on one variant. A
        // LOD must be a subset of the thing it replaces, or "cheaper" is a
        // statistical hope instead of a guarantee.
        wp.shoots = sp.whorl_shoots;
        wp.branches_min = sp.whorl_branches_min;
        wp.branches_max = sp.whorl_branches_max;
        wp.miss_bottom = sp.whorl_miss_bottom;
        wp.miss_top = sp.whorl_miss_top;
        wp.stubs = sp.whorl_stubs;
        wp.angle_top = sp.whorl_angle_top;
        wp.angle_bottom = sp.whorl_angle_bottom;
        wp.droop = sp.droop;
        // Seed the BARE BOLE first so the dead-stub band has trunk to hang on
        // and the leader continues an existing chain rather than starting in
        // mid-air.
        seed_trunk_nodes(sk, stem_base, glm::vec3{stem_base.x, t.crown_base, stem_base.z});
        wp.stub_base = stem_base.y + (t.crown_base - stem_base.y) * sp.stub_band_frac;
        whorl_skeleton(sk, wp, seed);
    } else {
        seed_trunk_nodes(sk, stem_base, stem_top);
        ColonizeParams cp;
        cp.attractors = sp.attractors;
        // D scales with the crown, so a sapling and a giant get the same NUMBER
        // of branch segments rather than the same segment length — which is what
        // keeps the triangle budget flat across the maturity tiers.
        cp.step = std::max(t.crown_r * sp.colonize_step_frac, 0.25f);
        cp.influence_d = sp.influence_d;
        cp.kill_d = sp.kill_d;
        cp.surface_bias = sp.surface_bias;
        cp.max_iterations = (t.lod == FloraLod::Full) ? 40u : 26u;
        cp.min_radius = SHADOW_MIN_DIAMETER * 0.5f;
        cp.pipe_exponent = sp.pipe_exponent;
        // Derived from what is LEFT of the triangle budget after the trunk and
        // the cards, at ~10 tris per segment. Space colonization branches at
        // almost every node, so decimation alone cannot recover an over-grown
        // crown: nearly every node is a fork or a tip and therefore unremovable.
        // The ceiling has to bind during growth.
        // Reduced spends its saving on the SKELETON, which is what stops
        // resolving first: at that range the crown mass still reads and the
        // individual limbs do not.
        cp.max_nodes = std::max(24u, (t.lod == FloraLod::Reduced)
                                         ? max_crown_segments(sp) * 45u / 100u
                                         : max_crown_segments(sp));
        // Eq. (3)'s g: phototropism up, gravity down, and the open side for an
        // edge tree. One vector carries all three, which is why the paper needs
        // no separate tropism stage.
        glm::vec3 g{0.0f, sp.phototropism - sp.droop, 0.0f};
        if (t.shape.lean > 0.0f) {
            g += glm::vec3{t.shape.lean_dir.x, 0.0f, t.shape.lean_dir.y} * 0.35f;
        }
        cp.tropism = g;
        cp.shy_dir = t.shape.shy_dir;
        cp.shyness = t.shape.shyness;
        cp.grow_from = branch_floor;
        colonize(sk, vol, cp, seed);
    }

    // Pipe model over trunk AND crown as one structure: limb thickness becomes a
    // record of how much crown is above it, which is the property that makes a
    // tree read as a tree at eight pixels. The root radius is the species'
    // trunk radius, so sim's collision capsule (species_trunk_radius) is
    // untouched — the model supplies the hierarchy, the contract the scale.
    assign_pipe_radii(sk, t.trunk_r, sp.pipe_exponent, SHADOW_MIN_DIAMETER * 0.5f);
    if (sp.envelope != CrownEnvelope::Cone) {
        // The paper's post-process (e). Raw space colonization forks near a
        // right angle and reads as a candelabra.
        soften_forks(sk, sp.fork_softening);
    }
    // THE TRIANGLE BUDGET, DERIVED RATHER THAN TUNED. Wood is emitted at 3-5
    // sides, so a segment costs at most 10 triangles; the trunk and the foliage
    // cards are already committed. Decimating to what is LEFT keeps every
    // species, every variant and every maturity tier under TREE_TRI_BUDGET_MAX
    // by construction, instead of by a per-species step size that is green when
    // it is written and red two sizes later.
    uint32_t max_segments = max_crown_segments(sp);
    if (t.lod == FloraLod::Reduced) max_segments = max_segments * 45u / 100u;
    decimate_to(sk, max_segments);
    // Radii are recomputed AFTER decimation: the pipe model counts supported
    // tips, and dissolving a chain node does not change the tip count while
    // re-parenting does change which node carries which.
    assign_pipe_radii(sk, t.trunk_r, sp.pipe_exponent, SHADOW_MIN_DIAMETER * 0.5f);
    emit_skeleton(m, t, sk);
    const uint32_t clusters = (t.lod == FloraLod::Reduced)
        ? std::max<uint32_t>(3u, sp.cluster_count * 3u / 5u)
        : sp.cluster_count;
    // THREE PLANES IS THE FLOOR AT EVERY CARD LOD (render-spec constraint,
    // 10.08.2026). The edge-on failure is a property of viewing ANGLE, not of
    // viewing distance, so Reduced may not spend its saving on plane count —
    // it spends on cluster count and on the skeleton instead. A 2-plane
    // cluster at 150 m vanishes at the same azimuths a near one does.
    emit_anchored_foliage(m, t, sk, clusters,
                          (t.lod == FloraLod::Reduced)
                              ? std::max(3, sp.cards_per_cluster - 1)
                              : -1);
}


/// Trunk with root flare. Returns the top point and its direction.
glm::vec3 build_trunk(MeshData& m, Tree& t, glm::vec3 base, float height,
                      float radius, glm::vec3* out_dir,
                      std::vector<TrunkRing>* out_path = nullptr) {
    const SpeciesParams& sp = t.sp;
    const int segments = std::max<int>(2, (t.lod == FloraLod::Silhouette)
                                              ? 2
                                              : sp.trunk_segments);
    const int sides = sp.trunk_sides;

    // Root flare: widen and sink so the skirt buries itself in whatever the
    // terrain does. Kept above the shadow-caster floor at its narrowest.
    const float flare_r = std::max(radius * FLARE_WIDEN, SHADOW_MIN_DIAMETER * 0.5f);
    tube_segment(m, base + glm::vec3{0.0f, -t.flare_depth, 0.0f},
                 base + glm::vec3{0.0f, t.flare_h, 0.0f},
                 glm::vec3{0.0f, 1.0f, 0.0f}, flare_r,
                 std::max(radius, SHADOW_MIN_DIAMETER * 0.5f), sides, t.wood);

    glm::vec3 p = base + glm::vec3{0.0f, t.flare_h, 0.0f};
    glm::vec3 d{0.0f, 1.0f, 0.0f};
    const float span = std::max(height - t.flare_h, 0.5f);
    const float seg_len = span / static_cast<float>(segments);
    const glm::vec3 sweep_dir =
        t.shape.lean > 0.0f
            ? safe_normalize(glm::vec3{t.shape.lean_dir.x, 0.0f, t.shape.lean_dir.y},
                             glm::vec3{1.0f, 0.0f, 0.0f})
            : glm::vec3{1.0f, 0.0f, 0.0f};
    const float bend = sp.trunk_sweep + t.shape.lean;

    const float path_floor_r = SHADOW_MIN_DIAMETER * 0.5f;
    if (out_path) {
        out_path->push_back(TrunkRing{p, d, std::max(radius, path_floor_r)});
    }
    for (int s = 0; s < segments; ++s) {
        d = safe_normalize(d + sweep_dir * (bend / static_cast<float>(segments)), d);
        const float t0 = static_cast<float>(s) / static_cast<float>(segments);
        const float t1 = static_cast<float>(s + 1) / static_cast<float>(segments);
        const float r0 = radius * std::pow(1.0f - t0 * 0.92f, sp.taper_exp);
        const float r1 = radius * std::pow(1.0f - t1 * 0.92f, sp.taper_exp);
        const glm::vec3 next = p + d * seg_len;
        // THE SHADOW FLOOR APPLIES TO THE TRUNK TOO. The old 0.03 m clamp let a
        // conifer's leader taper to a hair, and a hair above the crown is the
        // «острые пики» the user rejected on the birch, arriving by a different
        // route. Nothing in this world is thinner than the shadow map can see.
        const float floor_r = SHADOW_MIN_DIAMETER * 0.5f;
        tube_segment(m, p, next, d, std::max(r0, floor_r), std::max(r1, floor_r), sides,
                     t.wood);
        p = next;
        if (out_path) {
            out_path->push_back(TrunkRing{p, d, std::max(r1, floor_r)});
        }
    }
    if (out_dir) *out_dir = d;
    return p;
}

// --- Ground cover: the §5.10 patch classes and the rich edge set ------------

/// A faceted octahedron: the cheapest solid that reads from every bearing —
/// flower heads, mushroom caps, pebbles. 8 triangles.
void diamond(MeshData& m, glm::vec3 c, float r_xz, float r_y, uint32_t color,
             float az0) {
    glm::vec3 e[4];
    for (int k = 0; k < 4; ++k) {
        const float az = az0 + TAU * static_cast<float>(k) / 4.0f;
        e[k] = c + glm::vec3{std::cos(az) * r_xz, 0.0f, std::sin(az) * r_xz};
    }
    const glm::vec3 top = c + glm::vec3{0.0f, r_y, 0.0f};
    const glm::vec3 bot = c - glm::vec3{0.0f, r_y, 0.0f};
    for (int k = 0; k < 4; ++k) {
        tri(m, e[k], top, e[(k + 1) % 4], color);
        tri(m, e[(k + 1) % 4], bot, e[k], color);
    }
}

/// One ground-cover patch: moss, flowers, mushrooms, pebbles. A patch species
/// is a GroundForm plus numbers (FloraSpecies.h). Everything here CONTACTS the
/// ground — domes and stones sink, heads sit on tufts or stems — because the
/// one complaint this zone exists to answer is geometry hanging where nothing
/// supports it, and it applies at 0.2 m exactly as it did at 20 m.
void build_ground_patch(MeshData& m, Tree& t, Rng& rng) {
    const SpeciesParams& sp = t.sp;
    const float h = t.height; // patch height, from the species band
    const float pr = sp.patch_radius;
    const int elements = (t.lod == FloraLod::Full)
        ? sp.ground_elements
        : std::max<int>(2, sp.ground_elements * 3 / 5);
    const uint32_t tone_a = pack(sp.accent_color);
    const uint32_t tone_b = pack(sp.accent_color_b);
    const uint32_t green = pack(sp.foliage_color);
    const uint32_t stem = t.wood;

    switch (sp.ground_form) {
    case GroundForm::MossDome: {
        // Overlapping flattened domes, sunk so the rim never hovers. The moss
        // that dresses a stone's shade side is THIS mesh placed against the
        // stone by core's associative rule — one asset, two habitats.
        const int domes = (t.lod == FloraLod::Silhouette) ? 1 : elements;
        for (int i = 0; i < domes; ++i) {
            const float az = rng.unit() * TAU;
            const float d = rng.unit() * pr * 0.55f;
            const float r = pr * (0.45f + rng.unit() * 0.45f);
            blob_cluster(m, {std::cos(az) * d, -0.35f * h, std::sin(az) * d},
                         {r, h * (0.9f + rng.unit() * 0.4f), r}, 5, 2,
                         ((i & 1) != 0) ? tone_b : tone_a);
        }
        return;
    }
    case GroundForm::HeadsTuft: {
        // A green tuft with the heads ON it: low flowers live in their own
        // foliage. The tuft dome is sunk; a head's centre sits at most one
        // element radius above the tuft surface, so its lower half is inside
        // the green — attached by construction.
        const float tuft_r = pr * 0.85f;
        const float tuft_h = h * 0.55f;
        blob_cluster(m, {0.0f, -tuft_h * 0.35f, 0.0f}, {tuft_r, tuft_h, tuft_r}, 5,
                     2, green);
        if (t.lod == FloraLod::Silhouette) return;
        for (int i = 0; i < elements; ++i) {
            const float az = GOLDEN_ANGLE * static_cast<float>(i) + rng.unit() * 0.4f;
            const float d = tuft_r * (0.15f + 0.75f * rng.unit());
            // Tuft surface height at distance d (ellipse), minus the sink.
            const float surf =
                tuft_h * std::sqrt(std::max(0.0f, 1.0f - (d / tuft_r) * (d / tuft_r)))
                    - tuft_h * 0.35f;
            const float er = sp.element_radius * (0.8f + rng.unit() * 0.5f);
            diamond(m, {std::cos(az) * d, std::max(surf, 0.02f) + er * 0.4f,
                        std::sin(az) * d},
                    er, er * 1.15f, (rng.unit() < 0.72f) ? tone_a : tone_b,
                    rng.unit() * TAU);
        }
        return;
    }
    case GroundForm::HeadsStem: {
        // Tall flowers: a small basal tuft, then visible stems each carrying
        // one head at its top — the head touches its stem by construction.
        const float tuft_r = pr * 0.7f;
        blob_cluster(m, {0.0f, -h * 0.10f, 0.0f}, {tuft_r, h * 0.22f, tuft_r}, 4, 2,
                     green);
        if (t.lod == FloraLod::Silhouette) return;
        for (int i = 0; i < elements; ++i) {
            const float az = rng.unit() * TAU;
            const float d = rng.unit() * pr * 0.6f;
            const glm::vec3 base{std::cos(az) * d, 0.0f, std::sin(az) * d};
            const float sh = h * (0.75f + rng.unit() * 0.35f);
            const glm::vec3 lean{rng.sym() * 0.12f, 1.0f, rng.sym() * 0.12f};
            const glm::vec3 dir = safe_normalize(lean, {0.0f, 1.0f, 0.0f});
            const glm::vec3 top_p = base + dir * sh;
            tube_segment(m, base, top_p, dir, 0.020f, 0.014f, 3, stem);
            const float er = sp.element_radius * (0.85f + rng.unit() * 0.4f);
            // The umbel's plate is FLAT, a jewel's head is full — one number.
            const float ry = er * sp.element_aspect;
            diamond(m, top_p + glm::vec3{0.0f, ry * 0.5f, 0.0f}, er, ry,
                    (rng.unit() < 0.7f) ? tone_a : tone_b, rng.unit() * TAU);
        }
        return;
    }
    case GroundForm::Caps: {
        for (int i = 0; i < elements; ++i) {
            const float az = GOLDEN_ANGLE * static_cast<float>(i) + rng.unit() * 0.5f;
            const float d = pr * (0.2f + 0.75f * rng.unit());
            const glm::vec3 base{std::cos(az) * d, 0.0f, std::sin(az) * d};
            const float sh = h * (0.5f + rng.unit() * 0.5f);
            const float cap_r = sp.element_radius * (0.7f + rng.unit() * 0.6f);
            tube_segment(m, base - glm::vec3{0.0f, 0.02f, 0.0f},
                         base + glm::vec3{0.0f, sh, 0.0f}, {0.0f, 1.0f, 0.0f},
                         cap_r * 0.32f, cap_r * 0.26f, 3, stem);
            diamond(m, base + glm::vec3{0.0f, sh + cap_r * 0.18f, 0.0f}, cap_r,
                    cap_r * 0.55f, (rng.unit() < 0.65f) ? tone_a : tone_b,
                    rng.unit() * TAU);
            if (t.lod == FloraLod::Silhouette && i >= 1) return;
        }
        return;
    }
    case GroundForm::Stones: {
        for (int i = 0; i < elements; ++i) {
            const float az = GOLDEN_ANGLE * static_cast<float>(i) + rng.unit() * 0.6f;
            const float d = pr * (0.15f + 0.8f * rng.unit());
            const float er = sp.element_radius * (0.6f + rng.unit() * 0.8f);
            // Part-buried: centre near the ground so the lower half sinks.
            blob_cluster(m, {std::cos(az) * d, er * 0.25f, std::sin(az) * d},
                         {er, er * 0.72f, er * (0.75f + rng.unit() * 0.4f)}, 3, 2,
                         ((i % 3) == 0) ? tone_b : tone_a);
            if (t.lod == FloraLod::Silhouette && i >= 2) return;
        }
        return;
    }
    case GroundForm::None:
        return;
    }
}

// --- Dead wood: the §5.10 forest-floor classes ------------------------------

/// A SNAPPED dead limb: one tapered tube ending in a short splinter cone. The
/// splinter is what says "broken", the truncation is what says "dead" — a limb
/// of live length with no foliage says "winter", which is a different object.
void snapped_stub(MeshData& m, glm::vec3 base, glm::vec3 dir, float len,
                  float r0, uint32_t color) {
    if (len <= 0.05f || r0 <= 0.02f) return;
    dir = safe_normalize(dir, glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::vec3 snap = base + dir * len;
    const int sides = (r0 > 0.15f) ? 4 : 3;
    tube_segment(m, base, snap, dir, r0, r0 * 0.45f, sides, color);
    // The splinter: a very short cone past the break.
    tube_segment(m, snap, snap + dir * std::min(0.18f * len, 0.25f), dir, r0 * 0.45f,
                 0.0f, 3, color);
}

/// What makes a snag ITS OWN OBJECT rather than a bare pole or a leafless tree:
/// a broken BLUNT top with shards, and a handful of truncated stubs where the
/// limbs snapped. Limb reach is deliberately BETWEEN the two controls the suite
/// runs — above a pole's (which has none), below a winter broadleaf's (whose
/// limbs measure >= 0.166 of height).
void build_snag_detail(MeshData& m, Tree& t, const std::vector<TrunkRing>& path,
                       Rng& rng) {
    if (path.size() < 2) return;
    const SpeciesParams& sp = t.sp;
    const float h = t.height;

    // Broken top: three shards standing off the top ring. Small (0.4-0.8 m),
    // so they read as splintered wood on a blunt break, never as a spire.
    const TrunkRing& top = path.back();
    const glm::vec3 u = perp_of(top.dir);
    const glm::vec3 v = glm::cross(top.dir, u);
    for (int k = 0; k < 3; ++k) {
        const float az = TAU * (static_cast<float>(k) + rng.unit() * 0.5f) / 3.0f;
        const float az2 = az + 0.45f + rng.unit() * 0.3f;
        const glm::vec3 rim_a = top.pos + (u * std::cos(az) + v * std::sin(az)) * top.radius;
        const glm::vec3 rim_b =
            top.pos + (u * std::cos(az2) + v * std::sin(az2)) * top.radius;
        const float shard_h = 0.4f + rng.unit() * 0.4f;
        const glm::vec3 tip = (rim_a + rim_b) * 0.5f + top.dir * shard_h
            + (u * rng.sym() + v * rng.sym()) * 0.08f;
        tri(m, rim_a, tip, rim_b, t.wood);
        tri(m, rim_b, tip, rim_a, t.wood); // two-sided: a shard is a sliver
    }

    // Truncated limb stubs over the upper two thirds of the bole. The FIRST one
    // is guaranteed long and near-horizontal so the limb-reach floor holds by
    // construction, not by luck of the draw; the rest are the variation.
    const int n = static_cast<int>(sp.stub_count);
    for (int i = 0; i < n; ++i) {
        const float f = 0.36f + (0.92f - 0.36f) * rng.unit();
        const float fi = f * (static_cast<float>(path.size()) - 1.0f);
        const auto i0 = static_cast<size_t>(fi);
        const size_t i1 = std::min(i0 + 1, path.size() - 1);
        const float k = fi - static_cast<float>(i0);
        const glm::vec3 at = path[i0].pos * (1.0f - k) + path[i1].pos * k;
        const float r_at = path[i0].radius * (1.0f - k) + path[i1].radius * k;

        const float az = rng.unit() * TAU;
        const float pitch = (i == 0) ? 0.25f + rng.unit() * 0.2f
                                     : -0.30f + rng.unit() * 0.95f;
        const glm::vec3 dir{std::cos(az) * std::cos(pitch), std::sin(pitch),
                            std::sin(az) * std::cos(pitch)};
        const float len = sp.stub_len_frac * h
            * ((i == 0) ? 0.95f + rng.unit() * 0.25f : 0.55f + rng.unit() * 0.65f);
        const float r0 = std::clamp(r_at * 0.5f, SHADOW_MIN_DIAMETER * 0.5f,
                                    r_at * 0.85f);
        snapped_stub(m, at + dir * (r_at * 0.6f), dir, len, r0, t.twig);
    }
}

/// A fallen tree, not a floating cylinder: the trunk generator laid down along
/// +X, part-buried along its WHOLE length (the suite asserts every axial slice
/// dips below the ground datum, with a floated copy as the control), plus the
/// marks of the fall — an upturned root plate at the butt for the big class,
/// snapped stubs, and moss on the upper side.
void build_fallen_log(MeshData& m, Tree& t, Rng& rng) {
    const SpeciesParams& sp = t.sp;
    const float len = t.height; // "height" is LENGTH once laid down
    const float r = len * sp.trunk_radius_frac;
    const int segs = sp.trunk_segments;
    const float y0 = r * 0.45f; // axis height: lower half buried on flat ground

    glm::vec3 p{-len * 0.5f, y0, 0.0f};
    const glm::vec3 d{1.0f, 0.0f, 0.0f};
    for (int s = 0; s < segs; ++s) {
        const float f0 = static_cast<float>(s) / static_cast<float>(segs);
        const float f1 = static_cast<float>(s + 1) / static_cast<float>(segs);
        // Butt swell on the first ring; then the usual taper to the crown end.
        const float rr0 = r * ((s == 0) ? 1.12f : (1.0f - 0.35f * f0));
        const float rr1 = r * (1.0f - 0.35f * f1);
        const glm::vec3 next = p + d * (len / static_cast<float>(segs));
        tube_segment(m, p, next, d, rr0, rr1, sp.trunk_sides, t.wood);
        p = next;
    }

    if (t.lod != FloraLod::Silhouette) {
        // The upturned ROOT PLATE: a fallen tree tore out of the ground, and
        // the disc of roots and earth standing on the butt is the single
        // strongest "this fell" signal the medium has. Two fans, front and
        // back, jagged rim; big class only (deadfall is shed wood, it never
        // had roots).
        if (sp.root_plate) {
            const glm::vec3 c{-len * 0.5f, y0, 0.0f};
            const glm::vec3 n = safe_normalize(glm::vec3{-1.0f, 0.22f + rng.sym() * 0.1f,
                                                         rng.sym() * 0.15f},
                                               glm::vec3{-1.0f, 0.0f, 0.0f});
            const glm::vec3 pu = perp_of(n);
            const glm::vec3 pv = glm::cross(n, pu);
            const float rp = r * (1.9f + rng.unit() * 0.4f);
            constexpr int SPOKES = 8;
            glm::vec3 rim[SPOKES];
            for (int k = 0; k < SPOKES; ++k) {
                const float az = TAU * static_cast<float>(k) / SPOKES;
                const float rk = rp * (0.68f + rng.unit() * 0.45f);
                rim[k] = c + (pu * std::cos(az) + pv * std::sin(az)) * rk;
            }
            const glm::vec3 off = n * (r * 0.10f);
            for (int k = 0; k < SPOKES; ++k) {
                const glm::vec3& a = rim[k];
                const glm::vec3& b = rim[(k + 1) % SPOKES];
                tri(m, c + off, a + off, b + off, t.twig); // face away from the log
                tri(m, c - off, b - off, a - off, t.wood); // face toward it
            }
        }

        // Snapped stubs on the exposed upper side.
        for (int i = 0; i < static_cast<int>(sp.stub_count); ++i) {
            const float fx = -0.32f + rng.unit() * 0.74f;
            const float f01 = fx + 0.5f;
            const float r_x = r * (1.0f - 0.35f * f01);
            const float theta = rng.sym() * 1.15f; // from up, around the axis
            const float tilt = rng.sym() * 0.4f;   // lean along the axis
            const glm::vec3 dir =
                safe_normalize(glm::vec3{tilt, std::cos(theta), std::sin(theta)},
                               glm::vec3{0.0f, 1.0f, 0.0f});
            const glm::vec3 base = glm::vec3{fx * len, y0, 0.0f} + dir * (r_x * 0.7f);
            const float slen = sp.stub_len_frac * len * (0.7f + rng.unit() * 0.7f);
            const float r0 = std::clamp(r_x * 0.42f, SHADOW_MIN_DIAMETER * 0.5f,
                                        r_x * 0.8f);
            snapped_stub(m, base, dir, slen, r0, t.twig);
        }
    }

    // MOSS, upper side only, in patches. Recoloured per FACE (faces own their
    // vertices under flat shading), gated by the face normal — moss grows where
    // rain and light land — and by a low-frequency cell noise along the log so
    // it arrives in patches rather than as paint.
    if (sp.moss_cover > 0.0f) {
        const uint32_t moss_a = pack(sp.moss_color);
        const uint32_t moss_b = pack(sp.moss_color * MOSS_TONE_B);
        bool any_moss = false;
        size_t first_up = m.indices.size();
        for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
            platform::Vertex& a = m.vertices[m.indices[i]];
            platform::Vertex& b = m.vertices[m.indices[i + 1]];
            platform::Vertex& c = m.vertices[m.indices[i + 2]];
            if (a.normal.y < 0.40f) continue; // flat-shaded: one normal per face
            if (first_up == m.indices.size()) first_up = i;
            const glm::vec3 mid = (a.position + b.position + c.position) / 3.0f;
            const auto cx = static_cast<uint64_t>(
                static_cast<int64_t>(std::floor(mid.x / 0.7f)));
            const auto cz = static_cast<uint64_t>(
                static_cast<int64_t>(std::floor(mid.z / 0.7f)));
            const uint64_t hash = mix64(cx * 0x9E3779B1ull ^ mix64(cz ^ t.rng.s));
            const float gate = static_cast<float>(hash >> 40) / 16777216.0f;
            if (gate >= sp.moss_cover) continue;
            const uint32_t moss = ((hash >> 17) & 1u) ? moss_a : moss_b;
            a.color_rgba = moss;
            b.color_rgba = moss;
            c.color_rgba = moss;
            any_moss = true;
        }
        // The user's brief is «поваленные деревья … с мохом»: a log that
        // declares moss CARRIES moss. A small deadfall has few up-faces, and
        // at 22 % cover the cell noise can legitimately miss all of them — so
        // the first up-face mosses as a floor. Colour only; geometry untouched.
        if (!any_moss && first_up < m.indices.size()) {
            for (size_t k = 0; k < 3; ++k) {
                m.vertices[m.indices[first_up + k]].color_rgba = moss_a;
            }
        }
    }
}

/// Conifer SILHOUETTE-LOD shell: a single cone. This is the ONLY place a solid
/// cone survives, and the distinction matters — at Silhouette range the outline
/// is the entire information and a whorled skeleton resolves to the same
/// handful of pixels for six times the triangles. What was wrong before was
/// using this shape at FULL LOD, where the player is close enough to see that a
/// spruce is a stack of branch rings and instead sees «юбки».
void build_cone_shell(MeshData& m, Tree& t) {
    const float span = t.crown_top - t.crown_base;
    if (span <= 0.0f) return;
    tube_segment(m, glm::vec3{0.0f, t.crown_base, 0.0f},
                 glm::vec3{0.0f, t.crown_top, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f},
                 t.crown_r * shy_scale(t, {1.0f, 0.0f, 0.0f}), 0.0f, 6, t.leaf);
}

/// Silhouette LOD: trunk column + one envelope shell. Deliberately close to the
/// pre-flora mesh — at that range the silhouette is the entire information.
void build_silhouette(MeshData& m, Tree& t) {
    glm::vec3 dir{0.0f, 1.0f, 0.0f};
    const glm::vec3 top =
        build_trunk(m, t, glm::vec3{0.0f}, t.height, t.trunk_r, &dir);
    if (t.sp.envelope == CrownEnvelope::None) return;
    if (t.sp.envelope == CrownEnvelope::Cone) {
        build_cone_shell(m, t);
        return;
    }
    // The shell sits over the LEANING bole, like the full crown does. A LOD
    // whose silhouette is in a different place from the mesh it replaces pops
    // sideways at the switch distance, which is a worse artefact than any
    // amount of missing detail.
    const float mid = (t.crown_base + t.crown_top) * 0.5f;
    blob_cluster(m, glm::vec3{top.x, mid, top.z},
            glm::vec3{t.crown_r, (t.crown_top - t.crown_base) * 0.5f, t.crown_r}, 6, 3,
            t.leaf);
}

} // namespace

uint32_t flora_variant_for(glm::vec2 world_xz) {
    const auto xi = static_cast<uint64_t>(static_cast<int64_t>(std::lround(world_xz.x * 4.0f)));
    const auto zi = static_cast<uint64_t>(static_cast<int64_t>(std::lround(world_xz.y * 4.0f)));
    return static_cast<uint32_t>(mix64(xi * 0x9E3779B1ull ^ mix64(zi)) % FLORA_VARIANTS);
}

/// GREAT-OAK PREVIEW, A VERIFICATION HOOK AND NEVER A SHIPPING PATH — the same
/// standing as DFN_FLORA_ONLY above it and render's DFN_NO_SCATTER.
///
/// It exists because of a zone boundary, not because of a doubt: WHERE a great
/// oak stands is core's and design's decision (rarity, the sea cliff, the named
/// tree), and until `math::ScatterSpecies` carries an ordinal for it there is no
/// instance in the world to photograph. Rule 27 wants a frame from OUR build,
/// so this promotes ordinary oaks to great oaks for the length of one capture:
///   DFN_FLORA_GREAT_OAK=1  one oak in sixteen (what "редкие" looks like)
///   DFN_FLORA_GREAT_OAK=2  every oak (the close-up: structure and steps)
///   DFN_FLORA_GREAT_OAK=3  as 1, and the promoted trees carry the chain
static int great_oak_preview() {
    static const int mode = [] {
        const char* e = std::getenv("DFN_FLORA_GREAT_OAK");
        return e ? std::atoi(e) : 0;
    }();
    return mode;
}

FloraSpecies flora_species_of(math::ScatterSpecies species) {
    switch (species) {
    case math::ScatterSpecies::OakTree: return FloraSpecies::DaleOak;
    case math::ScatterSpecies::PineTree: return FloraSpecies::HighlandPine;
    case math::ScatterSpecies::BirchTree: return FloraSpecies::RiverBirch;
    case math::ScatterSpecies::Bush: return FloraSpecies::Bush;
    // §5.10/§5.11/§5.12, wired by core 10.08.2026. THE DEFAULT BELOW IS WHY
    // THESE ARE SPELLED OUT: an unmapped species does not fail to compile, it
    // silently draws as a Bush — so a forest floor of snags and fallen logs
    // would have shipped as a field of shrubs and read as "the placement is
    // broken" rather than "the mapping is missing".
    case math::ScatterSpecies::Snag: return FloraSpecies::Snag;
    case math::ScatterSpecies::SnagPale: return FloraSpecies::SnagPale;
    case math::ScatterSpecies::BigBush: return FloraSpecies::BigBush;
    case math::ScatterSpecies::FallenLog: return FloraSpecies::FallenLog;
    case math::ScatterSpecies::Deadfall: return FloraSpecies::Deadfall;
    case math::ScatterSpecies::MossPatch: return FloraSpecies::MossPatch;
    case math::ScatterSpecies::FlowerCarpet: return FloraSpecies::FlowerCarpet;
    case math::ScatterSpecies::FlowerAccent: return FloraSpecies::FlowerAccent;
    case math::ScatterSpecies::FlowerJewel: return FloraSpecies::FlowerJewel;
    case math::ScatterSpecies::FlowerUmbel: return FloraSpecies::FlowerUmbel;
    case math::ScatterSpecies::Mushroom: return FloraSpecies::Mushroom;
    case math::ScatterSpecies::PebbleCluster: return FloraSpecies::PebbleCluster;
    case math::ScatterSpecies::StuntedPine: return FloraSpecies::StuntedPine;
    default: return FloraSpecies::Bush;
    }
}

bool flora_owns(math::ScatterSpecies species) {
    // Enumerated POSITIVELY and without a default, so adding a ScatterSpecies
    // makes this switch non-exhaustive and the compiler names the file that
    // has to decide. A `default` here would answer for a species nobody has
    // thought about yet, which is exactly how the forest floor came to draw as
    // bare earth: the fallthrough was silent and looked like a neutral state.
    switch (species) {
    case math::ScatterSpecies::OakTree:
    case math::ScatterSpecies::PineTree:
    case math::ScatterSpecies::BirchTree:
    case math::ScatterSpecies::Bush:
    case math::ScatterSpecies::Snag:
    case math::ScatterSpecies::SnagPale:
    case math::ScatterSpecies::BigBush:
    case math::ScatterSpecies::FallenLog:
    case math::ScatterSpecies::Deadfall:
    case math::ScatterSpecies::MossPatch:
    case math::ScatterSpecies::FlowerCarpet:
    case math::ScatterSpecies::FlowerAccent:
    case math::ScatterSpecies::FlowerJewel:
    case math::ScatterSpecies::FlowerUmbel:
    case math::ScatterSpecies::Mushroom:
    case math::ScatterSpecies::PebbleCluster:
    case math::ScatterSpecies::StuntedPine:
        return true;
    // Stone is render's own mesh (ProcMesh's boulder path), not a plant.
    // NOTE the trap this predicate exists to keep shut: flora_species_of()
    // maps Stone to Bush through its default, so routing a Stone down the
    // flora path would draw a SHRUB where a boulder belongs — wrong, and
    // wrong in a way that reads as a placement bug rather than a routing one.
    case math::ScatterSpecies::Stone:
        return false;
    }
    return false;
}

float species_nominal_height(FloraSpecies s) {
    const SpeciesParams& sp = species_params(s);
    return (sp.height_min + sp.height_max) * 0.5f;
}

float species_crown_radius(FloraSpecies s) {
    const SpeciesParams& sp = species_params(s);
    return species_nominal_height(s) * sp.crown_width_frac * 0.5f;
}

float species_crown_base(FloraSpecies s) {
    const SpeciesParams& sp = species_params(s);
    return species_nominal_height(s) * sp.crown_base_frac;
}

float species_trunk_radius(FloraSpecies s) {
    const SpeciesParams& sp = species_params(s);
    return species_nominal_height(s) * sp.trunk_radius_frac * FLARE_WIDEN;
}

FloraMesh build_flora_mesh(FloraSpecies species, uint32_t variant,
                           const FloraShape& shape, FloraLod lod,
                           FloraSeason season) {
    const SpeciesParams& sp = species_params(species);
    FloraMesh parts;
    MeshData& m = parts.wood;

    // THE SNAG SPLIT IS TWO MATERIALS ON ONE ASSET (design §5.10): SnagPale
    // seeds its rng as Snag, so a given variant is byte-identical geometry in
    // both looks — asserted in the suite. Fork the seed and you have forked
    // the asset.
    const FloraSpecies geo_species =
        (species == FloraSpecies::SnagPale) ? FloraSpecies::Snag : species;
    Rng rng{mix64(static_cast<uint64_t>(geo_species) * 0x1000193ull
                  ^ (static_cast<uint64_t>(variant) + 1) * 0x9E3779B97F4A7C15ull)};

    float height = sp.height_min + rng.unit() * (sp.height_max - sp.height_min);
    // THE GREAT OAK IS ALREADY THE GIANT TIER, so it does not take the tier
    // multiplier on top of its own band — 1.5 x 46 m would be a 69 m tree, and
    // design §5.7's binding rule is that the forest stays under the landmark it
    // frames. Its variation lives in the 34-46 m band and in the crown, which
    // is where a viewer reads a great oak's size from anyway.
    const float maturity = (sp.fractal_depth > 0)
        ? std::clamp(shape.maturity, 0.92f, 1.10f)
        : std::max(0.2f, shape.maturity);
    height *= maturity;

    float crown_base_frac = sp.crown_base_frac;
    float crown_width_frac = sp.crown_width_frac;
    if (flora_control_arm()) {
        // The two species rows this change moved, restored to their 11.08.2026
        // values so the control arm differs from the shipped arm in EXACTLY the
        // things under test and in nothing else.
        if (species == FloraSpecies::DaleOak) {
            crown_width_frac = 0.48f;
            crown_base_frac = 0.40f;
        } else if (species == FloraSpecies::RiverBirch) {
            crown_width_frac = 0.34f;
        }
    }

    // --- WIDTH: THE ALLOMETRY AND THE TWO INDEPENDENT DRAWS -----------------
    // The user asked for two things in one sentence — most trees wider, and
    // small ones still present — and they are not the same lever. Sliding the
    // mean would grant the first and cancel the second, so width moves on THREE
    // separate axes here and none of them is a global multiplier:
    //   1. the species ratio (FloraSpecies.cpp), re-derived from the frames;
    //   2. ALLOMETRY against maturity, exp > 1, so a giant is wider FOR ITS
    //      HEIGHT and a sapling narrower for its own — this is the clause that
    //      widens the spread instead of shifting it;
    //   3. a per-instance draw on width ALONE, so two trees of equal height are
    //      still different trees.
    if (sp.crown_allometry_exp != 1.0f && !flora_control_arm()) {
        crown_width_frac *=
            std::pow(maturity, sp.crown_allometry_exp - 1.0f);
    }
    if (sp.crown_width_jitter > 0.0f && !flora_control_arm()) {
        crown_width_frac *= 1.0f + rng.sym() * sp.crown_width_jitter;
    }
    crown_width_frac *= std::max(0.2f, shape.crown_width_mult);

    // --- THE BOLE LENGTH ALSO VARIES, and DOWNWARD from the species value.
    // «Листва пониже» is answered mostly by the bottom-heavy envelope profile
    // (FloraSkeleton), but a stand where every bole is exactly 0.35 of its own
    // height still reads as a set: the crowns line up in a band. Drawing the
    // fraction over the bottom half of design's approved 0.35-0.45 band keeps
    // every tree inside a ruling that already exists while breaking the band.
    if (is_canopy_tree(species) && sp.envelope != CrownEnvelope::Cone
        && !flora_control_arm()) {
        const auto lo = static_cast<float>(config::CROWN_BASE_FRACTION_MIN);
        const auto hi = static_cast<float>(config::CROWN_BASE_FRACTION_MAX);
        crown_base_frac = std::min(crown_base_frac, hi)
            + rng.unit() * (hi - lo) * 0.5f;
        crown_base_frac = std::clamp(crown_base_frac, std::min(lo, sp.crown_base_frac),
                                     hi);
    }
    if (shape.understory) {
        crown_base_frac = std::min(0.75f, crown_base_frac + 0.10f);
        crown_width_frac *= 0.8f;
    }
    // CROWN ASPECT CEILING (design's ruling, §5). The crown's container is
    // (1 - crown_base_frac) tall by crown_width_frac wide, both in units of
    // height, so the ceiling is a lower bound on the crown base and the base is
    // DERIVED rather than exempted per species. Conifers are exempt as a
    // property of their silhouette brief — a cone is MEANT to be narrow, and a
    // pine at 4.2 is the anti-oak, not a defect.
    //
    // Why this exists at all, in one line, because it is the most expensive
    // lesson in the zone: two authored rules multiplied into a container 2.3x
    // taller than wide, and FOUR attempts at rearranging its CONTENTS failed
    // because the mass IS the container. Checked here, on the first build,
    // instead of on the fourth screenshot.
    if (sp.envelope != CrownEnvelope::Cone && sp.envelope != CrownEnvelope::None) {
        const auto ceiling = static_cast<float>(config::CROWN_ASPECT_MAX);
        // 0.97: derive just inside the ceiling, so the assertion on the BUILT
        // tree has somewhere to fail if the geometry ever drifts outward again.
        const float from_aspect = 1.0f - ceiling * 0.97f * crown_width_frac;
        crown_base_frac = std::max(crown_base_frac, from_aspect);
    }

    const bool is_snag =
        species == FloraSpecies::Snag || species == FloraSpecies::SnagPale;
    const bool woody_tree = is_canopy_tree(species) || is_snag
        || species == FloraSpecies::StuntedPine;
    Tree t{sp,
           shape,
           height,
           height * crown_base_frac,
           height,
           height * crown_width_frac * 0.5f,
           height * sp.trunk_radius_frac,
           pack(sp.trunk_color),
           pack(sp.twig_color),
           pack(sp.foliage_color),
           lod,
           rng,
           woody_tree ? std::clamp(height * 0.045f, 0.5f, 1.4f)
                      : std::clamp(height * 0.08f, 0.08f, 0.4f),
           woody_tree ? FLARE_DEPTH : std::clamp(height * 0.10f, 0.10f, 0.4f)};

    // Card foliage exists unless winter has stripped it. WINTER IS ONE BOOLEAN
    // (LANDSCAPE §5.11): do not emit the cards and the already-generated
    // skeleton IS the bare tree. Conifers keep their needles.
    // The Silhouette LOD deliberately keeps its solid shell: at that range the
    // silhouette is the entire information and a cutout buys nothing but
    // shimmer and overdraw.
    if (sp.foliage == FoliageShape::Card && lod != FloraLod::Silhouette
        && leaf_tone_has_foliage(sp.tone_first, season)) {
        t.cards = &parts.cards;
    }
    t.phase = shape.wind_phase;

    // HARD FLOOR (§3.5): canopy species keep CANOPY_CLEARANCE_MIN of clear
    // trunk. Enforced by construction, never by inspection. Non-canopy card
    // species (krummholz) are exempt BY CLASSIFICATION, like bushes: their
    // foliage-to-the-ground is the point, and t.clearance_floor stays 0.
    if (is_canopy_tree(species)) {
        t.clearance_floor = CLEARANCE_MIN;
        if (t.crown_base < CLEARANCE_MIN) t.crown_base = CLEARANCE_MIN;
    }

    if (species == FloraSpecies::FallenLog || species == FloraSpecies::Deadfall) {
        // A log IS the trunk generator, laid down along +X, part-buried, with
        // the marks of the fall (root plate, stubs, upper-side moss).
        // Placement lays it ACROSS the fall line (design's binding doctrine);
        // the yaw for that is the batcher's, not ours.
        build_fallen_log(m, t, rng);
        return parts;
    }

    if (sp.ground_form != GroundForm::None) {
        // Ground cover (moss, flowers, mushrooms, pebbles): a patch has no
        // trunk and never goes near the tree pipeline — a flower on a bole
        // would merely be absurd, but a trunk under a moss dome would be a
        // bug that ships.
        build_ground_patch(m, t, t.rng);
        return parts;
    }

    if (lod == FloraLod::Silhouette) {
        build_silhouette(m, t);
        return parts;
    }

    // --- The stems ----------------------------------------------------------
    // Multi-trunk clumps (the user's "сколько стволов"). NOTE the birch is no
    // longer one of them: 2-3 bare pale poles from a single root, crowned by a
    // tuft, is the palm silhouette the user rejected, and the clump was half of
    // what produced it. A river birch really is multi-stemmed; a birch that
    // reads as a birch at 640x360 is not, and §1.5 outranks the field guide.
    const int stems = sp.trunk_count_min
        + static_cast<int>(t.rng.unit()
                           * static_cast<float>(sp.trunk_count_max - sp.trunk_count_min + 1));
    const int stem_count = std::clamp(stems, static_cast<int>(sp.trunk_count_min),
                                      static_cast<int>(sp.trunk_count_max));

    for (int k = 0; k < stem_count; ++k) {
        const float ang = TAU * static_cast<float>(k) / static_cast<float>(stem_count)
            + t.rng.unit() * 0.6f;
        const glm::vec3 off = stem_count > 1
            ? glm::vec3{std::cos(ang) * sp.trunk_spread, 0.0f,
                        std::sin(ang) * sp.trunk_spread}
            : glm::vec3{0.0f};
        t.stem_off = off;
        t.sway_from = glm::vec3{off.x, t.crown_base, off.z};
        // In a clump the LEAD stem carries the species height and the others are
        // shorter — that is what makes it read as one multi-stemmed tree rather
        // than as N small trees.
        const float stem_scale = (k == 0) ? 1.0f : (0.74f + t.rng.unit() * 0.22f);
        const float stem_h = height * trunk_height_frac(sp.envelope) * stem_scale;
        glm::vec3 dir{0.0f, 1.0f, 0.0f};
        std::vector<TrunkRing> path;
        const bool is_great = sp.fractal_depth > 0;
        const bool wants_detail =
            (is_snag || is_great) && lod != FloraLod::Silhouette;
        const glm::vec3 top =
            build_trunk(m, t, off, stem_h, t.trunk_r, &dir,
                        wants_detail ? &path : nullptr);
        if (is_snag && wants_detail) {
            // The broken top and the truncated stubs are what make a snag its
            // own object instead of a pole (docs/specs/flora.md §3.4).
            build_snag_detail(m, t, path, t.rng);
        }
        // THE CROWN FOLLOWS THE BOLE IT SITS ON. Extrapolated along the trunk's
        // own direction up to the crown's mid-height, because branches continue
        // the lean rather than snapping back to plumb. Without this the trunk
        // walks out through the side of its own crown at anything past a few
        // degrees, which is why the lean was capped at 0.12 rad and could not
        // reach the 15-25 deg the reference frames show.
        // NOT FOR THE CONIFER, and this is a fact about the whorl grower rather
        // than a taste: whorl_skeleton() builds its leader on the STRAIGHT stem
        // axis and ignores the trunk sweep entirely, so moving its envelope onto
        // the swept top would put the container somewhere the contents are not.
        // Measured when it was wrong for one run: the pine and the krummholz
        // lost a quarter of their presented area, because a 1 m axis offset on
        // a 4 m crown radius clips one whole flank away.
        if (sp.envelope != CrownEnvelope::Cone) {
            const float y_mid = (t.crown_base + t.crown_top) * 0.5f;
            const float span = std::max(top.y - (off.y + t.flare_h), 0.5f);
            const float k = std::clamp((y_mid - (off.y + t.flare_h)) / span, 0.0f, 1.6f);
            // ONLY THE LEAN'S SHARE OF THE BEND MOVES THE CROWN. The bole's
            // own `trunk_sweep` is a CURVE in the stem, and the crown grows off
            // that stem wherever it went, so following it would double-count:
            // measured, a plumb birch (sweep 0.18 rad, crown radius 3.4 m) lost
            // 65 % of its presented area to an axis shift its own branches had
            // already made. The whole-tree LEAN is different — it tips the
            // crown bodily downwind — and it is the only part taken here.
            const float bend = sp.trunk_sweep + t.shape.lean;
            const float lean_share =
                (bend > 1e-4f) ? std::clamp(t.shape.lean / bend, 0.0f, 1.0f) : 0.0f;
            t.crown_axis = glm::vec2{off.x, off.z}
                + (glm::vec2{top.x, top.z} - glm::vec2{off.x, off.z}) * k * lean_share;
        }
        if (is_great && wants_detail) {
            // Furniture BEFORE the crown, so the treads exist on the bole even
            // if the crown budget is exhausted: a climbable tree whose steps are
            // the first thing dropped is a tree that is climbable in the design
            // document only.
            build_climb_steps(m, t, path, t.crown_base);
            build_climb_platforms(m, t, top, static_cast<int>(sp.climb_platforms),
                                  t.rng);
            if (shape.chained) {
                build_golden_chain(m, t, path, std::max(2.0f, t.height * 0.075f));
            }
        }

        if (!sp.has_skeleton) {
            // Bushes have no skeleton worth growing: they ARE their foliage, and
            // a solid blob at 1-4 m is the right medium (design's ruling: only
            // TREE foliage is cards).
            scatter_bush_clusters(m, t, off);
            continue;
        }
        // THE CROWN. Branches are grown INTO the foliage volume and the foliage
        // hangs off the nodes that reached it — so «листья, не прикрепляющиеся
        // к ветвям» is not a thing this code can express.
        // Branches may leave the bole BELOW the foliage line: a real trunk sheds
        // ascending limbs well under the crown, and a crown that starts exactly
        // where the branches start is the palm.
        const glm::vec3 branch_base{off.x,
                                    std::min(height * sp.branch_base_frac, stem_h * 0.9f),
                                    off.z};
        build_crown(m, t, branch_base, top,
                    mix64(static_cast<uint64_t>(variant) * 977ull
                          + static_cast<uint64_t>(species) * 31ull
                          + static_cast<uint64_t>(k) * 7919ull),
                    branch_base.y);
    }
    return parts;
}

void append_flora(MeshData& wood, MeshData& cards, FloraSpecies species,
                  uint32_t variant, const FloraShape& shape, FloraLod lod,
                  glm::vec3 position, float yaw, FloraSeason season) {
    // The great-oak preview (see great_oak_preview above): a capture-only
    // promotion, keyed by POSITION so the same oaks are promoted every run and
    // the frame is re-shootable.
    FloraSpecies sp_use = species;
    FloraShape shape_use = shape;
    if (const int preview = great_oak_preview();
        preview != 0 && species == FloraSpecies::DaleOak) {
        const auto xi = static_cast<uint64_t>(
            static_cast<int64_t>(std::lround(position.x * 0.25f)));
        const auto zi = static_cast<uint64_t>(
            static_cast<int64_t>(std::lround(position.z * 0.25f)));
        const uint64_t h = mix64(xi * 0x9E3779B1ull ^ mix64(zi));
        const bool pick = (preview == 2) || ((h % 16ull) == 0ull);
        if (pick) {
            sp_use = FloraSpecies::GreatOak;
            shape_use.chained = (preview == 3);
        }
    }
    const FloraMesh parts =
        build_flora_mesh(sp_use, variant, shape_use, lod, season);
    // Scale 1.0: the generator has ALREADY applied FloraShape::maturity, and a
    // tree does not sink — it stands on its root flare (§3.5).
    // VERIFICATION HOOK, NEVER A SHIPPING PATH (Rule 27/30, the same standing
    // as render's DFN_NO_SCATTER). DFN_FLORA_ONLY=1 draws WOOD only, =2 draws
    // CARDS only; unset draws the tree.
    //
    // IT EXISTS BECAUSE THE OBVIOUS INSTRUMENT WAS WRONG. Asked which of the
    // two meshes produces the running shimmer, the cheap answer is to classify
    // the flipping PIXELS by colour — and it gives a confident, wrong number:
    // an oak's shaded leaf card and its bark are BOTH under luma 55, so every
    // card in shadow was counted as trunk (measured "93 % wood"; the arms below
    // say otherwise). A claim about which MESH drew a pixel has to be settled
    // by not drawing one of them.
    //
    // Measured with DFN_FLORA_PROBE + DFN_WIND_FREEZE, control 0.000 % / maxL 0,
    // one 0.05 m stride at RUN_SPEED, 640x360, share of screen flipping by more
    // than 64 luma (near canopy / treeline):
    //     both meshes (shipped)   0.864 % / 0.093 %
    //     wood only               1.069 % / 0.164 %
    //     cards only              0.382 % / 0.095 %
    // TWO READINGS, AND THEY POINT AT DIFFERENT MESHES AT THE TWO VANTAGES:
    // under the crowns the WOOD carries it (dropping the trunks takes 0.864 to
    // 0.382, i.e. 56 % of the near shimmer is bole silhouette); at the treeline
    // the CARDS carry it (cards alone measure the shipped number, 0.095 vs
    // 0.093). And wood alone is WORSE than the whole tree at BOTH vantages,
    // which is the mechanism in one line: what flickers is high-contrast edge
    // standing against SKY, so a crown in front of a bole does not add a
    // flicker, it BURIES one. Detail is not the variable; occlusion is.
    //
    // FIRST DRAFT OF THIS COMMENT QUOTED SIX DIFFERENT NUMBERS and concluded
    // the opposite ("removing either mesh makes it worse"). They were taken
    // before DFN_WIND_FREEZE existed, i.e. against a control that was not zero.
    // Left recorded rather than silently corrected: on this probe a dirty
    // control does not add noise, it reverses the verdict.
    static const int only = [] {
        const char* e = std::getenv("DFN_FLORA_ONLY");
        return e ? std::atoi(e) : 0;
    }();
    if (only != 2) append_transformed(wood, parts.wood, position, yaw, 1.0f);
    if (only != 1) append_transformed(cards, parts.cards, position, yaw, 1.0f);
}

} // namespace dfn::render
