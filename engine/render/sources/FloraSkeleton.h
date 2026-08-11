/*
Created: 09:08:2026 - 23:12:44
Last updated: 12:08:2026 - 00:36:00
Module: engine/render
File: engine/render/sources/FloraSkeleton.h

Responsibility:
- The branch SKELETON: a space-colonization grower (Runions, Lane &
  Prusinkiewicz 2007) plus the pipe-model radii that thicken it, and the whorl
  grower conifers need instead. Produces nodes and the foliage anchors that
  hang off them; emits no triangles.

Key items:
- CrownVolume, envelope_radius_at(), ColonizeParams, Skeleton, SkeletonNode,
  colonize(), whorl_skeleton(), assign_pipe_radii(), decimate(),
  soften_forks(), gather_foliage_anchors().

Dependencies:
- Uses: FloraSpecies.h (CrownEnvelope), glm.
- Used by: ProcFlora (mesh emission), ProcFloraTests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md,
  algorithm record docs/specs/flora_algorithms.md.
- PURE AND DETERMINISTIC: no GPU, no ECS, no globals, no wall-clock, no IO.
- THE INVARIANT THIS FILE EXISTS TO CREATE: foliage cannot float. Every anchor
  returned by gather_foliage_anchors() names a node index, and a leaf site
  survives only if a node came within the kill distance of it. Anything that
  places foliage from a volume instead of from this skeleton reintroduces the
  defect the user rejected on 09.08.2026 — see flora_algorithms.md §0.1.
*/
/*
UPD:
- 09:08:2026 - 23:12:44: Created — space colonization + whorls, replacing the
  recursive branch generator and the envelope-scattered crown.
- 12:08:2026 - 00:20:00: CrownVolume::axis (the crown follows a leaning bole)
  and FractalParams / fractal_skeleton() -- the great oak's recursive grower,
  the third growth model in this file.
- 12:08:2026 - 00:36:00: FractalParams gains a bounding cylinder (top_y,
  max_radius, axis). Measured: unclipped ramification grew a 46 m great oak's
  wood to 90.8 m -- twice its declared height -- because every generation adds
  its length to whatever the last one reached, and the species height band is a
  cross-zone contract.
*/

#pragma once

#include "engine/render/sources/FloraSpecies.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace dfn::render {

/// The crown as a solid of revolution: the species silhouette intent, resolved
/// to numbers. Under space colonization this stops being a clip applied after
/// growth and becomes the CAUSE of growth (flora_algorithms.md §1.3.2) — the
/// attraction points fill it, so the silhouette guarantee of flora.md §3.1
/// stage D is strengthened rather than given up.
struct CrownVolume {
    CrownEnvelope shape = CrownEnvelope::Sphere;
    float base = 0.0f;   ///< m, lowest foliage height
    float top = 1.0f;    ///< m, crown apex
    float radius = 1.0f; ///< m, crown radius at its widest
    /// XZ CENTRE OF THE CROWN, and it is not always the stem's base.
    /// A tree that LEANS carries its crown over the top of its leaning bole,
    /// not over its roots (LANDSCAPE §10.3, reference frame 16). Before this
    /// field the envelope was pinned to the local origin, so any lean past a
    /// few degrees walked the trunk out through the side of its own crown —
    /// which is why the old lean cap of 0.12 rad was doing structural work it
    /// was never meant to do.
    glm::vec2 axis{0.0f};
};

/// Crown radius at height y. Zero outside [base, top].
[[nodiscard]] float envelope_radius_at(const CrownVolume& v, float y);

/// One skeleton node. A node IS a point on a branch axis; the edge to its
/// parent is the branch segment.
struct SkeletonNode {
    glm::vec3 pos{0.0f};
    int parent = -1;      ///< -1 = root
    float radius = 0.0f;  ///< m, filled by assign_pipe_radii()
    uint16_t children = 0;
    uint8_t order = 0;    ///< 0 on the trunk axis, +1 past every fork
    bool trunk = false;   ///< part of the authored bole, never decimated away
};

struct Skeleton {
    std::vector<SkeletonNode> nodes;
    /// Attraction points that were consumed, and the node that consumed each.
    /// This is the attachment proof: a point is only consumed when a node comes
    /// within the kill distance of it.
    std::vector<glm::vec3> leaf_sites;
    std::vector<int> leaf_anchor;

    [[nodiscard]] bool is_tip(size_t i) const { return nodes[i].children == 0; }
};

/// Parameters of the grower. Distances that the paper expresses as multiples of
/// D are kept in that form, because that is how they are reported and how they
/// transfer between species of different size.
struct ColonizeParams {
    uint32_t attractors = 320;  ///< N. Small N gives IRREGULAR branches (wanted)
    float step = 0.9f;          ///< D, m
    float influence_d = 9.0f;   ///< di / D. Paper: 8 for trees, 17 for shrubs
    float kill_d = 2.0f;        ///< dk / D. Paper: 2 (fine) .. 20 (smooth, sparse)
    uint32_t max_iterations = 48;
    glm::vec3 tropism{0.0f};    ///< g in eq. 3: +Y light, -Y weight, xz lean
    /// 0 = points uniform through the crown volume, 1 = points only on the
    /// crown SHELL. The paper's fig. 7: shell-only gives an open branch system
    /// with twigs limited to the crown surface, which is what the user's
    /// reference photographs measure as (flora.md §3.10 — porosity is a RIM
    /// effect over a near-opaque core).
    float surface_bias = 0.55f;
    float min_radius = 0.175f;  ///< m, the shadow-caster floor as a RADIUS
    float pipe_exponent = 2.5f; ///< n in r^n = r1^n + r2^n; paper says 2..3
    /// Hard ceiling on skeleton size. The paper has no such parameter because it
    /// is not spending a triangle budget; we are, and N/D tuning alone is the
    /// kind of thing that is right for the variant it was tuned on and wrong two
    /// maturity tiers away. Growth stops here, cleanly, with the crown it has.
    uint32_t max_nodes = 90;
    /// CROWN SHYNESS, applied where it belongs. Previously the crown grew to its
    /// full envelope and only the foliage CLUSTERS were scaled down, which is
    /// backwards: shyness is a growth response, so a shy tree should not send
    /// branches toward its neighbour in the first place. Shrinking the attractor
    /// cloud on that side is the whole implementation.
    glm::vec2 shy_dir{0.0f};
    float shyness = 0.0f;
    /// Lowest height a node may grow FROM. Defaults to the crown base, but a
    /// real bole sheds ASCENDING limbs well below its foliage line, and a tree
    /// whose branches begin exactly where its leaves begin is the palm
    /// silhouette. The foliage line is unaffected: attractors still fill only
    /// the crown volume, so CANOPY_CLEARANCE_MIN and design's crown-base rule
    /// hold untouched — what changes is where the WOOD leaves the trunk.
    float grow_from = -1.0f; ///< < 0 = use volume.base
};

/// Grows a crown into `volume` from the seed nodes already in `sk`.
/// Only seeds at or above `volume.base` may grow, so the authored bole and
/// CANOPY_CLEARANCE_MIN survive untouched.
void colonize(Skeleton& sk, const CrownVolume& volume, const ColonizeParams& p,
              uint64_t seed);

/// The conifer's grower. A spruce is not a cone of foliage: it is a straight
/// leader carrying RINGS of branches (whorls) at intervals, the rings spaced
/// further apart lower down, with branches missing and lower whorls dead — see
/// flora_algorithms.md §2/§3.2. A solid tier is what produces «юбки».
struct WhorlParams {
    float base = 0.0f;    ///< m, lowest LIVE whorl (crown base)
    float top = 1.0f;     ///< m, leader tip
    float radius = 1.0f;  ///< m, crown radius at the lowest live whorl
    float stub_base = 0.0f; ///< m, bottom of the dead-stub band (see below)
    uint32_t whorls = 8;  ///< live whorls = years since the crown base died back
    /// Branches per whorl. Forestry: a "complete" whorl is >= 3 and an average
    /// whorl carries 2-7, the count driven by that YEAR'S vigour — so count and
    /// internode length are drawn from ONE variable, not two. That correlation
    /// (long internode -> fat whorl) is what makes real conifers irregular.
    uint32_t branches_min = 3;
    uint32_t branches_max = 6;
    /// Self-pruning. A branch's death is predicted by its age and its relative
    /// size WITHIN its whorl, so the lower (older) whorls lose more. A whorl
    /// that is complete every time is a lampshade frame.
    float miss_bottom = 0.42f;
    float miss_top = 0.06f;
    /// Elevation of a primary branch above horizontal, radians. Measured spruce
    /// insertion angles run 40-70 deg from the stem (i.e. +50 to +20 deg
    /// elevation), left-skewed toward the horizontal, and the ascent-to-
    /// horizontal transition happens FAST in the upper crown and then plateaus —
    /// so this is a power curve, not a lerp.
    float angle_top = 1.00f;
    float angle_bottom = -0.24f; ///< the oldest branches sag past horizontal
    float droop = 0.55f;         ///< sag accumulated along a primary
    uint32_t shoots = 2;         ///< pendulous second-order shoots per primary
    uint32_t stubs = 5;          ///< dead branch stubs below the live crown
};

/// Grows the leader, its whorls, the pendulous second-order shoots and the dead
/// stub band. Foliage anchors land on the SHOOTS, never on the leader: needles
/// persist only a few years, so a conifer's foliage lives on the last growth of
/// each branch and the inboard wood is bare.
void whorl_skeleton(Skeleton& sk, const WhorlParams& p, uint64_t seed);

/// THE FRACTAL GROWER — the great oak's skeleton, and a THIRD growth model
/// rather than a big setting on either of the other two (user request
/// 11.08.2026: «ветки будут расти как фракталы»).
///
/// Why neither existing grower answers it. Space colonization derives its
/// branches from an attractor cloud, so what you see in the finished crown is
/// the CLOUD's shape; the branch system is a means and it reads as one — fine
/// for a 28 m forest oak whose limbs are a dark tracery behind foliage, useless
/// for a tree the user wants to read as STRUCTURE from a kilometre away. The
/// whorl grower is a conifer's developmental rhythm and says nothing about
/// ramification. A recursive dichotomy with length and radius decay is the
/// thing the user actually named, and it has the property the other two lack:
/// the SAME rule at every scale, so the silhouette carries the same signature
/// at 5 m and at 500 m.
///
/// THE LOBES ARE EMERGENT, NOT AUTHORED. The user asked for great oaks shaped
/// like two big masses with a saddle between them, for tall ellipses, and for
/// «много разных». All of those are ONE parameter here — how many major limbs
/// leave the bole and how far they spread — because each major limb carries its
/// own sub-crown: two wide limbs give the two-lobed silhouette, one dominant
/// limb gives the ellipse, four to five give the broad dome. No silhouette is
/// enumerated anywhere; the shapes are what the parameter does.
struct FractalParams {
    float base_y = 0.0f;      ///< m, height of the first fork (top of the bole)
    float trunk_top_r = 1.0f; ///< m, bole radius where the first fork sits
    float length0 = 10.0f;    ///< m, length of a first-order limb
    float length_decay = 0.72f;  ///< child length / parent length
    float radius_decay = 0.72f;  ///< child radius / parent radius (pipe re-runs)
    uint32_t depth = 5;          ///< recursion depth (0 = the bole alone)
    uint32_t majors_min = 2;     ///< first-order limbs: 2 = two lobes, 5 = dome
    uint32_t majors_max = 5;
    uint32_t children_min = 2;   ///< limbs per fork beyond the first order
    uint32_t children_max = 3;
    float major_pitch = 0.95f;   ///< rad from vertical for the FIRST order
    float pitch_spread = 0.45f;  ///< rad, per-fork divergence half-angle
    float pitch_jitter = 0.30f;  ///< rad, per-branch noise on the above
    float droop = 0.10f;         ///< downward drift accumulated with depth
    float phototropism = 0.22f;  ///< upward drift, fought by droop
    /// Horizontal reach the FIRST order must cover, in metres. The user's rule
    /// for the great oak is «нижняя часть кроны в радиусе равна высоте», i.e.
    /// the lower crown's RADIUS equals the tree's height — so the first-order
    /// limbs are not decoration, they are the thing that has to physically span
    /// it. `length0` is derived from this rather than guessed.
    float spread_target = 30.0f;
    float segments_per_limb = 3.0f; ///< nodes along one limb (curvature)
    uint32_t max_nodes = 600;
    glm::vec2 lean{0.0f};        ///< XZ drift of the whole crown (wind lean)
    /// THE BOUNDING CYLINDER, and it is not decoration: the species HEIGHT
    /// band is a cross-zone contract (core's canopy occlusion envelope and
    /// design's C4 arithmetic both key off SPECIES_HEIGHT_MAX). Unclipped,
    /// recursive ramification overshoots badly -- measured, a 46 m great oak
    /// grew wood to 90.8 m, twice its own declared height, because every
    /// generation adds its own length to whatever the last one reached. The
    /// FOLIAGE envelope still governs the crown's SHAPE; this only stops the
    /// WOOD leaving the world it was sized for.
    float top_y = 1e9f;      ///< m, absolute ceiling for any node
    float max_radius = 1e9f; ///< m, from the crown axis
    glm::vec2 axis{0.0f};    ///< XZ centre the radius is measured from
};

/// Grows the fractal crown from the LAST node of `sk` (the bole's top). Every
/// terminal tip becomes a leaf site anchored to itself, so the attachment
/// invariant at the top of this file holds by construction here too.
void fractal_skeleton(Skeleton& sk, const FractalParams& p, uint64_t seed);

/// Pipe model (Shinozaki et al. 1964), basipetal from the tips:
/// r^n = r1^n + r2^n. Computed unitless from r0 = 1 and then SCALED so the root
/// radius equals `root_radius` — the trunk radius is a cross-zone number (sim's
/// collision capsule reads species_trunk_radius()), so the model supplies the
/// hierarchy and the contract supplies the absolute size.
/// Radii below `min_radius` are clamped UP: a thinner caster is invisible to the
/// shadow map (flora.md §3.5), and clamping up can never detach foliage the way
/// terminating a branch did.
void assign_pipe_radii(Skeleton& sk, float root_radius, float exponent,
                       float min_radius);

/// Collapses straight runs so a tree fits the triangle budget. Forks, tips,
/// trunk nodes, foliage anchors and every `keep_every`-th chain node survive;
/// the rest are dissolved and their children re-parented. Node indices change,
/// and leaf_anchor is remapped.
void decimate(Skeleton& sk, uint32_t keep_every);

/// Decimates repeatedly until the skeleton has at most `max_segments` edges.
/// Derived rather than tuned: the triangle budget is a hard cap that must hold
/// across the maturity tiers (x0.4 .. x1.5) and across every variant, and a
/// per-species step size hand-tuned against one variant is exactly the kind of
/// number that is green when written and red two sizes later.
void decimate_to(Skeleton& sk, uint32_t max_segments);

/// The paper's post-process (e): move the first node past a fork toward that
/// fork, which REDUCES THE BRANCHING ANGLE. Raw space colonization forks at
/// close to a right angle and reads as a candelabra.
/// Deviation from the paper, deliberately: it moves only post-fork nodes rather
/// than every node, because the paper's global version also shortens the tree
/// and our height band is a cross-zone contract.
void soften_forks(Skeleton& sk, float amount);

/// Merges leaf sites into `target_count` foliage clusters, each still anchored
/// to a real node. Returns cluster centres and their anchor node indices.
void gather_foliage_anchors(const Skeleton& sk, uint32_t target_count,
                            std::vector<glm::vec3>& centres,
                            std::vector<int>& anchors, std::vector<float>& reach);

} // namespace dfn::render
