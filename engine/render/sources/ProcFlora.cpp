/*
Created: 09:08:2026 - 19:31:02
Last updated: 09:08:2026 - 23:52:07
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
*/

#include "engine/render/sources/ProcFlora.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/render/sources/FloraBuild.h"
#include "engine/render/sources/FloraSkeleton.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace dfn::render {

namespace {

constexpr float CLEARANCE_MIN = static_cast<float>(config::CANOPY_CLEARANCE_MIN);

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
        tube_segment(m, a, b, safe_normalize(b - a, glm::vec3{0.0f, 1.0f, 0.0f}), r0,
                     r1, sides, t.wood);
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

void build_crown(MeshData& m, Tree& t, glm::vec3 stem_base, glm::vec3 stem_top,
                 uint64_t seed, float branch_floor) {
    const SpeciesParams& sp = t.sp;
    if (sp.envelope == CrownEnvelope::None) return;

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
    emit_anchored_foliage(m, t, sk, clusters,
                          (t.lod == FloraLod::Reduced)
                              ? std::max(1, sp.cards_per_cluster - 1)
                              : -1);
}


/// Trunk with root flare. Returns the top point and its direction.
glm::vec3 build_trunk(MeshData& m, Tree& t, glm::vec3 base, float height,
                      float radius, glm::vec3* out_dir) {
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
    }
    if (out_dir) *out_dir = d;
    return p;
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
    build_trunk(m, t, glm::vec3{0.0f}, t.height, t.trunk_r, &dir);
    if (t.sp.envelope == CrownEnvelope::None) return;
    if (t.sp.envelope == CrownEnvelope::Cone) {
        build_cone_shell(m, t);
        return;
    }
    const float mid = (t.crown_base + t.crown_top) * 0.5f;
    blob_cluster(m, glm::vec3{0.0f, mid, 0.0f},
            glm::vec3{t.crown_r, (t.crown_top - t.crown_base) * 0.5f, t.crown_r}, 6, 3,
            t.leaf);
}

} // namespace

uint32_t flora_variant_for(glm::vec2 world_xz) {
    const auto xi = static_cast<uint64_t>(static_cast<int64_t>(std::lround(world_xz.x * 4.0f)));
    const auto zi = static_cast<uint64_t>(static_cast<int64_t>(std::lround(world_xz.y * 4.0f)));
    return static_cast<uint32_t>(mix64(xi * 0x9E3779B1ull ^ mix64(zi)) % FLORA_VARIANTS);
}

FloraSpecies flora_species_of(math::ScatterSpecies species) {
    switch (species) {
    case math::ScatterSpecies::OakTree: return FloraSpecies::DaleOak;
    case math::ScatterSpecies::PineTree: return FloraSpecies::HighlandPine;
    case math::ScatterSpecies::BirchTree: return FloraSpecies::RiverBirch;
    case math::ScatterSpecies::Bush: return FloraSpecies::Bush;
    default: return FloraSpecies::Bush;
    }
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

    Rng rng{mix64(static_cast<uint64_t>(species) * 0x1000193ull
                  ^ (static_cast<uint64_t>(variant) + 1) * 0x9E3779B97F4A7C15ull)};

    float height = sp.height_min + rng.unit() * (sp.height_max - sp.height_min);
    height *= std::max(0.2f, shape.maturity);

    float crown_base_frac = sp.crown_base_frac;
    float crown_width_frac = sp.crown_width_frac;
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

    const bool woody_tree = is_canopy_tree(species) || species == FloraSpecies::Snag;
    Tree t{sp,
           shape,
           height,
           height * crown_base_frac,
           height,
           height * crown_width_frac * 0.5f,
           height * sp.trunk_radius_frac,
           pack(sp.trunk_color),
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
    // trunk. Enforced by construction, never by inspection.
    if (is_canopy_tree(species) && t.crown_base < CLEARANCE_MIN) {
        t.crown_base = CLEARANCE_MIN;
    }

    if (species == FloraSpecies::FallenLog || species == FloraSpecies::Deadfall) {
        // A log IS the trunk generator, laid down: built along +X, half-sunk.
        // Placement lays it ACROSS the fall line (design's binding doctrine);
        // the yaw for that is the batcher's, not ours.
        const float len = height;
        const float r = len * sp.trunk_radius_frac;
        const int segs = sp.trunk_segments;
        glm::vec3 p{-len * 0.5f, r * 0.45f, 0.0f};
        const glm::vec3 d{1.0f, 0.0f, 0.0f};
        for (int s = 0; s < segs; ++s) {
            const float f0 = static_cast<float>(s) / static_cast<float>(segs);
            const float f1 = static_cast<float>(s + 1) / static_cast<float>(segs);
            const glm::vec3 next = p + d * (len / static_cast<float>(segs));
            tube_segment(m, p, next, d, r * (1.0f - 0.35f * f0),
                         r * (1.0f - 0.35f * f1), sp.trunk_sides, t.wood);
            p = next;
        }
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
        const glm::vec3 top = build_trunk(m, t, off, stem_h, t.trunk_r, &dir);

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
    const FloraMesh parts = build_flora_mesh(species, variant, shape, lod, season);
    // Scale 1.0: the generator has ALREADY applied FloraShape::maturity, and a
    // tree does not sink — it stands on its root flare (§3.5).
    append_transformed(wood, parts.wood, position, yaw, 1.0f);
    append_transformed(cards, parts.cards, position, yaw, 1.0f);
}

} // namespace dfn::render
