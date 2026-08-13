/*
Created: 09:08:2026 - 23:12:44
Last updated: 13:08:2026 - 19:45:00
Module: engine/render
File: engine/render/sources/FloraSkeleton.cpp

Responsibility:
- Space-colonization growth (Runions, Lane & Prusinkiewicz 2007), the conifer
  whorl grower, pipe-model radii, and the skeleton post-passes. No triangles.

Key items:
- colonize(), whorl_skeleton(), assign_pipe_radii(), decimate(), soften_forks(),
  gather_foliage_anchors(), envelope_radius_at().

Dependencies:
- Uses: FloraSkeleton.h, FloraSpecies.h, glm.
- Used by: ProcFlora, ProcFloraTests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; docs/specs/flora_algorithms.md is the
  algorithm record and cites every equation used here by number.
- PURE AND DETERMINISTIC. Same (params, seed) -> byte-identical output.
- DO NOT add a path that places foliage without a node index. The whole reason
  this file exists is that the previous crown was scattered through a volume
  and the user rejected it (flora_algorithms.md §0.1).
*/
/*
UPD:
- 09:08:2026 - 23:12:44: Created.
- 12:08:2026 - 00:20:00: THE CROWN GAINED AN AXIS AND A THIRD GROWER.
  (a) CrownVolume::axis — the envelope follows the top of a LEANING bole
  instead of standing over the roots, which is what let the lean band open from
  0.12 rad to the 15-25 deg the reference frames actually show.
  (b) The Sphere profile became BOTTOM-HEAVY (widest at u~0.38, not 0.5): the
  user's «листва должна быть пониже» is a statement about where the crown's
  MASS sits, and moving the widest ring down answers it without touching the
  clearance floor that keeps the wood walkable.
  (c) fractal_skeleton() — the great oak's recursive grower.
- 12:08:2026 - 00:24:00: CrownVolume::axis honoured in attractor sampling and
  in the growth clip; the Sphere profile made bottom-heavy (widest ring at 0.42
  of the crown span, not 0.50) with its minimum radius raised to 0.42 so the
  narrowed apex does not starve — measured, the first draft pinched the top and
  the tree topped out at 0.39 of its own height; and fractal_skeleton(), the
  great oak's recursive grower.
- 12:08:2026 - 00:36:00: fractal_skeleton clamps every node into the bounding
  cylinder (see the header entry: the wood was reaching twice the declared
  height).
- 13:08:2026 - 16:20:00: gather_shoot_anchors() -- foliage sites ON the wood,
  weighted to the outer end of every shoot, every tip carrying one. And the
  majors now leave the seeded bole over `major_base_drop` metres of it rather
  than from its last node.
- 13:08:2026 - 19:45:00: The shyness veto in fractal_skeleton, with the wobble
  that keeps a channel from being a razor cut.
*/

#include "engine/render/sources/FloraSkeleton.h"

// For FloraShape::CrownEdge, the shyness boundaries. Deliberately included in
// the .cpp and held as `const void*` in the header: the skeleton must not
// depend on the public flora API, it is the layer under it.
#include "engine/render/sources/ProcFlora.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace dfn::render {

namespace {

constexpr float TAU_F = 6.28318530718f;
/// 137.5 deg. Phyllotaxis: consecutive elements around an axis at the golden
/// angle never line up, which is why real shoots do it and why a regular fan
/// reads as a radio mast.
constexpr float GOLDEN_ANGLE_F = 2.39996323f;

struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed | 1ull) {}
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return static_cast<uint32_t>(s >> 32);
    }
    /// 0..1
    float unit() { return static_cast<float>(next() & 0xFFFFFFu) / 16777215.0f; }
    /// -1..1
    float sym() { return unit() * 2.0f - 1.0f; }
};

glm::vec3 norm_or(glm::vec3 v, glm::vec3 fallback) {
    const float l = glm::length(v);
    return l > 1e-6f ? v / l : fallback;
}

} // namespace

float envelope_radius_at(const CrownVolume& v, float y) {
    const float span = v.top - v.base;
    if (span <= 1e-4f || y < v.base || y > v.top) return 0.0f;
    const float u = (y - v.base) / span; // 0 at crown base, 1 at apex
    switch (v.shape) {
    case CrownEnvelope::Sphere:
        // Widest BELOW mid-crown, closing at both ends. sin gives a rounded mass
        // rather than a lens, and never reaches exactly 0 at the base so the
        // lowest foliage has somewhere to sit.
        //
        // THE EXPONENT IS THE «ЛИСТВА ПОНИЖЕ» FIX, and it is deliberately here
        // rather than in the crown base. Lowering CROWN_BASE_FRACTION moves the
        // whole crown down and takes the walkable trunk with it — the fraction
        // is the one number that has already been raised to fix a proportion
        // and broken the storey below (NUMBERS, BIRCH_CROWN_BASE_FRACTION_MIN).
        // The exponent moves only where the crown's WIDEST RING sits: u^0.80
        // puts the maximum at u = 0.5^(1/0.80) = 0.42 of the crown span instead
        // of at 0.50, so the mass hangs low and the profile tapers above it —
        // the umbrella outline of reference frame 16. Clearance is untouched by
        // construction, because the crown BASE has not moved.
        // THE MINIMUM FACTOR ROSE WITH THE EXPONENT, and it had to. Moving the
        // widest ring down narrows everything above it, and the crown's TOP is
        // where the envelope was already tightest — so the first build of the
        // bottom-heavy profile pinched its apex, every cluster up there shrank
        // under the scrap floor and was dropped, and the tree topped out at
        // 0.39 of its own height (the REJECTION-3 foliage-span test caught it,
        // which is what that test is for). 0.42 is the same argument the Cone
        // arm two cases below already carries in this file: an envelope that
        // closes to a point starves its own tip.
        {
            const float lo = flora_control_arm() ? 0.30f : 0.42f;
            const float p_exp = flora_control_arm() ? 1.0f : 0.80f;
            return v.radius
                * (lo + (1.0f - lo) * std::sin(std::pow(u, p_exp) * 3.14159265f));
        }
    case CrownEnvelope::GreatCrown:
        // The great oak's LOWER crown is the widest thing in the world (user:
        // radius equal to the tree's height). It is widest at its very base and
        // domes over: an u^1.35 fall-off holds most of the radius over the
        // bottom half and then closes, so the silhouette is a hill rather than
        // a ball. Never zero at the top for the same reason the cone is not.
        return v.radius * (0.16f + 0.84f * std::pow(std::max(0.0f, 1.0f - u), 0.55f));
    case CrownEnvelope::Cone:
        // Widest at the crown base, and DELIBERATELY NOT closing to a point.
        // Design §5.2 requires the top of the cone to stay >= 1.5 m wide so the
        // tip survives quantization, and 0.18 of a 4.4 m crown radius is exactly
        // 1.58 m. A cone that tapers to zero also starves its top whorls — the
        // reach test drops any branch under 0.4 m — and what is left is a bare
        // leader standing above the foliage: the «острые пики» the user rejected
        // on the birch, arriving at the pine by a different route.
        return v.radius * (0.18f + 0.82f * (1.0f - u));
    case CrownEnvelope::Vase:
        // Narrow below, opening above, rounding over at the top. The birch's
        // "high open crown".
        return v.radius * (0.25f + 0.75f * std::sin(std::pow(u, 0.62f) * 3.14159265f * 0.92f));
    case CrownEnvelope::Weeping:
        // Wide shoulder high up, falling skirt below.
        return v.radius * (0.45f + 0.55f * std::sin(std::pow(u, 0.45f) * 3.14159265f));
    case CrownEnvelope::None:
    default:
        return 0.0f;
    }
}

namespace {

/// Samples one attraction point inside `v`. `surface_bias` pushes points toward
/// the crown SHELL, which is the paper's fig. 7 (open branch system, twigs on
/// the crown surface) and what flora.md §3.10 measured in the reference photos.
glm::vec3 sample_attractor(const CrownVolume& v, const ColonizeParams& p, Rng& rng,
                           uint32_t index) {
    const float surface_bias = p.surface_bias;
    // STRATIFY the height rather than sampling it uniformly. With N in the low
    // hundreds a uniform draw leaves bald bands, and a bald band in an attractor
    // cloud is a bald band in the crown — Rule 31 in miniature: the bounds were
    // never the problem, the distribution was.
    const float span = v.top - v.base;
    const float h = (static_cast<float>(index % 64) + rng.unit()) / 64.0f;
    const float y = v.base + span * h;
    const float env = envelope_radius_at(v, y);
    // Uniform in a disc is r = R*sqrt(t); the bias exponent lifts that toward R.
    const float t = rng.unit();
    const float exp_r = 0.5f * (1.0f - surface_bias) + 0.08f * surface_bias;
    const float r = env * std::pow(t, exp_r);
    // Golden-angle azimuth plus jitter: a plain uniform azimuth on a few hundred
    // points clumps, and a clump of attractors is a lump of crown.
    const float az = GOLDEN_ANGLE_F * static_cast<float>(index) + rng.sym() * 0.35f;
    float shy = 1.0f;
    if (p.shyness > 0.0f) {
        const float align = std::cos(az) * p.shy_dir.x + std::sin(az) * p.shy_dir.y;
        shy = 1.0f - p.shyness * std::max(0.0f, align);
    }
    return {v.axis.x + std::cos(az) * r * shy, y, v.axis.y + std::sin(az) * r * shy};
}

} // namespace

void colonize(Skeleton& sk, const CrownVolume& volume, const ColonizeParams& p,
              uint64_t seed) {
    if (sk.nodes.empty() || p.attractors == 0) return;
    Rng rng{seed * 0x9E3779B97F4A7C15ull + 0x1234567ull};

    std::vector<glm::vec3> points;
    points.reserve(p.attractors);
    for (uint32_t i = 0; i < p.attractors; ++i) {
        points.push_back(sample_attractor(volume, p, rng, i));
    }
    std::vector<uint8_t> dead(points.size(), 0);

    const float D = std::max(p.step, 0.05f);
    const float di = p.influence_d * D;
    const float dk = p.kill_d * D;

    // Per-node accumulation of eq. (2)'s n-vector, reused each iteration.
    std::vector<glm::vec3> accum;
    std::vector<uint32_t> hits;
    const float grow_floor = (p.grow_from >= 0.0f) ? p.grow_from : volume.base;

    for (uint32_t iter = 0; iter < p.max_iterations; ++iter) {
        const size_t n_nodes = sk.nodes.size();
        accum.assign(n_nodes, glm::vec3{0.0f});
        hits.assign(n_nodes, 0u);

        // --- Step 1, ASSOCIATE. Each surviving point influences the ONE node
        // closest to it, if that node is within di. This asymmetry (one point ->
        // one node, one node <- many points) is what makes two nearby tips
        // diverge instead of merging, and it is why the paper can say branch
        // intersections are prevented by construction.
        bool any = false;
        for (size_t s = 0; s < points.size(); ++s) {
            if (dead[s]) continue;
            int best = -1;
            float best_d2 = di * di;
            for (size_t v = 0; v < n_nodes; ++v) {
                // The bole below `grow_floor` stays a clean stem; above it,
                // limbs may leave the trunk and rise into the crown even though
                // the FOLIAGE still starts higher up.
                if (sk.nodes[v].pos.y < grow_floor - 0.01f) continue;
                const glm::vec3 d = points[s] - sk.nodes[v].pos;
                const float d2 = glm::dot(d, d);
                if (d2 < best_d2) {
                    best_d2 = d2;
                    best = static_cast<int>(v);
                }
            }
            if (best < 0) continue;
            accum[static_cast<size_t>(best)] +=
                norm_or(points[s] - sk.nodes[static_cast<size_t>(best)].pos,
                        glm::vec3{0.0f, 1.0f, 0.0f});
            ++hits[static_cast<size_t>(best)];
            any = true;
        }
        if (!any || sk.nodes.size() >= p.max_nodes) break;

        // --- Step 2, GROW. v' = v + D * n_hat, optionally biased by g (eq. 3).
        const size_t before = sk.nodes.size();
        for (size_t v = 0; v < before; ++v) {
            if (hits[v] == 0) continue;
            glm::vec3 dir = accum[v];
            // A tip exactly between symmetric attractors sums to ~0 and NaNs on
            // normalize. Deterministic jitter cures it and also stops the tree
            // being suspiciously straight (a pitfall every implementer reports).
            dir += glm::vec3{rng.sym(), rng.sym(), rng.sym()} * 0.06f;
            glm::vec3 n_hat = norm_or(dir, glm::vec3{0.0f, 1.0f, 0.0f});
            if (glm::dot(p.tropism, p.tropism) > 1e-8f) {
                n_hat = norm_or(n_hat + p.tropism, n_hat);
            }
            const glm::vec3 np = sk.nodes[v].pos + n_hat * D;
            // Never grow outside the envelope: the silhouette is the guarantee
            // this zone cannot give up (flora.md §3.1 stage D). Attractors are
            // inside it, so this fires only on tropism overshoot.
            // Below the crown the envelope is zero, which would pin an
            // ascending limb to the trunk axis. Under the crown a limb is free
            // laterally — it is the FOLIAGE the envelope governs — so the clip
            // there uses the crown's widest radius instead of its profile.
            float env = (np.y < volume.base) ? volume.radius
                                             : envelope_radius_at(volume, np.y);
            glm::vec3 clipped = np;
            // Radial distance from the CROWN AXIS, which is the top of a leaning
            // bole rather than the local origin. Measuring it from the origin
            // was correct only while trees stood plumb.
            const float ox = np.x - volume.axis.x;
            const float oz = np.z - volume.axis.y;
            const float rr = std::sqrt(ox * ox + oz * oz);
            // Shyness limits the GROWTH, not only the foliage. Shrinking the
            // attractor cloud alone is not enough: a branch can still overshoot
            // toward a point and be clipped at the full envelope, so the shy
            // side quietly gets its width back. Same clause-implemented-in-half
            // failure as flora.md §3.7, caught here by the invariant that exists
            // to reject exactly it.
            if (p.shyness > 0.0f && rr > 1e-4f) {
                const float align = (ox * p.shy_dir.x + oz * p.shy_dir.y) / rr;
                env *= 1.0f - p.shyness * std::max(0.0f, align);
            }
            if (np.y > volume.top) clipped.y = volume.top;
            if (rr > env && rr > 1e-4f) {
                clipped.x = volume.axis.x + ox * env / rr;
                clipped.z = volume.axis.y + oz * env / rr;
            }
            SkeletonNode child;
            child.pos = clipped;
            child.parent = static_cast<int>(v);
            child.order = static_cast<uint8_t>(
                std::min<int>(255, sk.nodes[v].order + (sk.nodes[v].children > 0 ? 1 : 0)));
            sk.nodes.push_back(child);
            ++sk.nodes[v].children;
        }
        if (sk.nodes.size() == before) break;

        // --- Step 3, KILL. A point dies once a node is closer than dk; the node
        // that killed it becomes the anchor of the foliage that will hang there.
        // THIS IS THE ATTACHMENT PROOF: a leaf site exists only where a branch
        // reached it.
        for (size_t s = 0; s < points.size(); ++s) {
            if (dead[s]) continue;
            int killer = -1;
            float best_d2 = dk * dk;
            for (size_t v = before; v < sk.nodes.size(); ++v) {
                const glm::vec3 d = points[s] - sk.nodes[v].pos;
                const float d2 = glm::dot(d, d);
                if (d2 < best_d2) {
                    best_d2 = d2;
                    killer = static_cast<int>(v);
                }
            }
            if (killer >= 0) {
                dead[s] = 1;
                sk.leaf_sites.push_back(points[s]);
                sk.leaf_anchor.push_back(killer);
            }
        }
    }

    // Tips that never consumed a point still carry foliage: they are branches
    // that reached the crown and stopped. Anchoring on the node itself keeps the
    // gap at zero, so this cannot reintroduce floating foliage.
    for (size_t v = 0; v < sk.nodes.size(); ++v) {
        if (sk.nodes[v].children != 0) continue;
        if (sk.nodes[v].pos.y < volume.base) continue;
        sk.leaf_sites.push_back(sk.nodes[v].pos);
        sk.leaf_anchor.push_back(static_cast<int>(v));
    }
}

void whorl_skeleton(Skeleton& sk, const WhorlParams& p, uint64_t seed) {
    if (sk.nodes.empty() || p.whorls == 0) return;
    Rng rng{seed * 0xD1B54A32D192ED03ull + 0x9E37ull};
    const float span = p.top - p.base;
    if (span <= 0.5f) return;
    const uint32_t W = p.whorls;

    // --- Internodes, and the fact that decides the whole silhouette ---------
    // A WHORL IS A YEAR. The leader puts on one internode and flushes one ring
    // of laterals at the top of it, so whorl spacing IS that year's height
    // increment and the number of whorls is the tree's age since its crown base
    // died back. That means spacing is NOT uniform: short at the very top (this
    // year, barely grown), longest through the vigorous middle years, short
    // again at the bottom (the slow juvenile years). Evenly stacked rings are
    // half of why the old conifer read as «юбки» — a regular stack integrates
    // into a solid, an irregular one does not.
    // The SAME per-year vigour also sets how many branches that whorl carries
    // (forestry: branch count tracks the current or previous year's shoot
    // length), so both are drawn from one variable. That correlation is what
    // makes a real conifer look irregular rather than merely noisy.
    std::vector<float> vigour(W, 1.0f);
    std::vector<float> internode(W, 1.0f);
    float total = 0.0f;
    for (uint32_t k = 0; k < W; ++k) { // k = 0 is the TOP-most whorl
        const float u = (W > 1) ? static_cast<float>(k) / static_cast<float>(W - 1) : 0.0f;
        // Hump: 0.28 at the apex, peak near a third of the way down, 0.35 at the
        // base. Multiplied by a per-year draw so no two trees share a rhythm.
        const float hump = 0.28f + 0.72f * std::sin(std::pow(u, 0.62f) * 3.14159265f);
        vigour[k] = std::clamp(hump * (0.62f + rng.unit() * 0.76f), 0.12f, 1.6f);
        internode[k] = vigour[k];
        total += internode[k];
    }
    if (total <= 1e-4f) return;
    // Scale the whole ladder onto the crown span: the pattern is the model, the
    // absolute size is the species contract.
    const float k_scale = span / total;

    // --- The leader. A conifer is MONOPODIAL: one axis, straight, to the tip.
    int leader = static_cast<int>(sk.nodes.size()) - 1;
    std::vector<int> whorl_node(W, leader);
    std::vector<float> whorl_u(W, 0.0f);
    float y = p.top;
    for (uint32_t k = 0; k < W; ++k) {
        y -= internode[k] * k_scale;
        const float yy = std::clamp(y, p.base, p.top - 0.15f);
        SkeletonNode n;
        n.pos = {0.0f, yy, 0.0f};
        n.parent = leader;
        n.trunk = true;
        sk.nodes.push_back(n);
        ++sk.nodes[static_cast<size_t>(leader)].children;
        leader = static_cast<int>(sk.nodes.size()) - 1;
        whorl_node[k] = leader;
        whorl_u[k] = (yy - p.base) / span; // 1 near the tip, 0 at the crown base
    }

    // --- Branches ------------------------------------------------------------
    for (uint32_t k = 0; k < W; ++k) {
        const float u = whorl_u[k];
        // Crown radius tapers to the leader, so the OUTLINE is a cone while no
        // cone surface exists anywhere. Per-whorl jitter keeps the profile from
        // being a ruled line — a perfectly tapering stack is a skirt again.
        const float reach = p.radius * (1.0f - u) * (0.62f + vigour[k] * 0.55f);
        if (reach < 0.4f) continue;
        // Elevation above horizontal. Measured spruce insertion angles are
        // 40-70 deg from the stem, left-skewed; the ascent-to-horizontal
        // transition happens FAST in the upper crown and then plateaus, so this
        // is a power curve. The oldest branches sag past horizontal under their
        // own weight, which is the bottom of the curve going negative.
        const float elev = p.angle_bottom
            + (p.angle_top - p.angle_bottom) * std::pow(u, 2.2f);
        // Branch count follows that year's vigour, floored at a "complete"
        // whorl of three where the year allows it.
        const auto range = static_cast<float>(p.branches_max - p.branches_min);
        auto slots = static_cast<uint32_t>(
            static_cast<float>(p.branches_min) + range * std::clamp(vigour[k] * 0.8f, 0.0f, 1.0f));
        slots = std::clamp(slots, p.branches_min, p.branches_max);
        // SELF-PRUNING. A branch's death is predicted by its age and its size
        // relative to its own whorl, so the lower (older) whorls lose more of
        // their positions. A whorl that is complete every time is a lampshade.
        const float miss = p.miss_bottom + (p.miss_top - p.miss_bottom) * u;
        // Each ring is offset from the last by the golden angle, so consecutive
        // whorls never stack their branches into vertical curtains.
        const float base_az = GOLDEN_ANGLE_F * static_cast<float>(k) + rng.sym() * 0.5f;
        for (uint32_t b = 0; b < slots; ++b) {
            if (rng.unit() < miss) continue;
            const float az = base_az
                + TAU_F * static_cast<float>(b) / static_cast<float>(slots)
                + rng.sym() * 0.28f;
            // ENVELOPE CLIP, and it is not optional. Crown width is a
            // cross-zone contract — design derived TREE_SPACING_FOREST FROM it —
            // and the vigour and length jitters multiply to as much as 1.9x, so
            // an unclipped whorl puts a 14 m pine inside a 6-9 m brief. This is
            // the same defect as flora.md §3.7.2, in the one generator that did
            // not inherit the fix.
            const float env_here = p.radius * (1.0f - u);
            const float len = std::min(reach * (0.66f + rng.unit() * 0.6f),
                                       std::max(env_here, 0.4f));
            const glm::vec3 out{std::cos(az), 0.0f, std::sin(az)};
            glm::vec3 dir = norm_or(out + glm::vec3{0.0f, std::tan(elev), 0.0f}, out);
            glm::vec3 pos = sk.nodes[static_cast<size_t>(whorl_node[k])].pos;
            int parent = whorl_node[k];
            const int segs = 2;
            for (int s = 0; s < segs; ++s) {
                // The primary rises then sags: a conifer limb is an arc, and the
                // sag grows toward the tip where the lever arm is longest.
                const float sag = p.droop * (static_cast<float>(s) + 1.0f)
                    / static_cast<float>(segs);
                dir = norm_or(dir + glm::vec3{0.0f, -sag * 0.5f, 0.0f}, dir);
                pos += dir * (len / static_cast<float>(segs));
                SkeletonNode n;
                n.pos = pos;
                n.parent = parent;
                n.order = 1;
                sk.nodes.push_back(n);
                ++sk.nodes[static_cast<size_t>(parent)].children;
                parent = static_cast<int>(sk.nodes.size()) - 1;
            }
            // NEEDLES LIVE ONLY ON THE LAST FEW YEARS OF SHOOT. They persist
            // 2-6 years, so the inboard length of every branch is bare wood and
            // the foliage sits at the END. Anchoring the foliage at the tip is
            // therefore botany, not a budget saving — and it is the other half
            // of the cure for «юбки», because a solid tier covers the inboard
            // wood that ought to be showing daylight.
            sk.leaf_sites.push_back(pos);
            sk.leaf_anchor.push_back(parent);
            // The pendulous SECOND-ORDER shoots. On Picea abies the first-order
            // branch is near-horizontal and its second-order shoots hang
            // vertically off it — that curtain, not the branch count, IS the
            // spruce silhouette, and it is exactly what a solid cone erases.
            // They are emitted as foliage anchors rather than as tubes: a shoot
            // is one pixel wide at any gameplay distance, so it is a hanging
            // CARD, and modelling it as wood would spend the whole budget on
            // geometry the player resolves as a smudge (Rule 33).
            for (uint32_t j = 0; j < p.shoots; ++j) {
                const float f = (static_cast<float>(j) + 0.7f)
                    / (static_cast<float>(p.shoots) + 0.4f);
                const glm::vec3 along = sk.nodes[static_cast<size_t>(parent)].pos;
                const glm::vec3 root_p = sk.nodes[static_cast<size_t>(whorl_node[k])].pos;
                const glm::vec3 at = root_p + (along - root_p) * f
                    // The hang is bounded so the spray still OVERLAPS the
                    // branch it grows on. A pendulous shoot that falls further
                    // than its own card is wide reads as detached foliage, which
                    // is the defect this whole rewrite exists to remove — and
                    // the attachment invariant caught it at 1.63 card-reaches.
                    + glm::vec3{0.0f, -len * (0.10f + rng.unit() * 0.14f), 0.0f};
                sk.leaf_sites.push_back(at);
                sk.leaf_anchor.push_back(parent);
            }
        }
    }

    // --- The dead-stub band -------------------------------------------------
    // Below the live crown a conifer carries a zone of dead branch stubs, and
    // below THAT a clean stem. Forestry models it explicitly as a self-pruning
    // ratio. It costs almost nothing, it is the difference between a trunk and a
    // pole, and it is precisely the "old and mighty" the brief asks for.
    if (p.stubs > 0 && p.stub_base < p.base - 0.5f) {
        const float band = p.base - p.stub_base;
        for (uint32_t i = 0; i < p.stubs; ++i) {
            const float f = (static_cast<float>(i) + 0.5f) / static_cast<float>(p.stubs);
            const float sy = p.stub_base + band * f;
            const float az = GOLDEN_ANGLE_F * static_cast<float>(i) * 2.0f + rng.sym() * 0.6f;
            // Stubs shorten downward: the lower a dead branch, the longer it has
            // been decaying and breaking back toward the bole.
            const float slen = (0.5f + 1.5f * f) * (0.6f + rng.unit() * 0.7f);
            SkeletonNode a;
            a.pos = {0.0f, sy, 0.0f};
            a.trunk = true;
            a.parent = -1;
            // Attach to the leader chain by finding the trunk node nearest in y.
            int best = -1;
            float best_d = 1e9f;
            for (size_t v = 0; v < sk.nodes.size(); ++v) {
                if (!sk.nodes[v].trunk) continue;
                const float d = std::fabs(sk.nodes[v].pos.y - sy);
                if (d < best_d) {
                    best_d = d;
                    best = static_cast<int>(v);
                }
            }
            if (best < 0) continue;
            SkeletonNode n;
            n.pos = {std::cos(az) * slen, sy - slen * 0.28f, std::sin(az) * slen};
            n.parent = best;
            n.order = 1;
            sk.nodes.push_back(n);
            ++sk.nodes[static_cast<size_t>(best)].children;
        }
    }
}

void fractal_skeleton(Skeleton& sk, const FractalParams& p, uint64_t seed) {
    if (sk.nodes.empty()) return;
    Rng rng{seed * 0xD1B54A32D192ED03ull + 0x9E3779B9ull};

    // The bole's top is the first fork. Everything below it was seeded by the
    // caller and stays trunk.
    const int root = static_cast<int>(sk.nodes.size()) - 1;
    // THE MAJORS LEAVE THE BOLE OVER A LENGTH OF IT, NOT AT ONE POINT.
    // `major_base_drop` metres of the seeded trunk are eligible, so limb k
    // starts from a different height. Without this every first-order limb
    // radiates from a single node and the tree is a wine glass — which is the
    // candelabra failure seed_trunk_nodes() was written to avoid, arriving in
    // the grower that did not read the seeds.
    std::vector<int> bole;
    {
        const float top_y = sk.nodes[static_cast<size_t>(root)].pos.y;
        for (size_t i = 0; i < sk.nodes.size(); ++i) {
            if (!sk.nodes[i].trunk) continue;
            if (sk.nodes[i].pos.y >= top_y - p.major_base_drop - 1e-3f) {
                bole.push_back(static_cast<int>(i));
            }
        }
    }
    if (bole.empty()) bole.push_back(root);

    struct Limb {
        int node;
        glm::vec3 dir;
        float len;
        uint32_t depth;
    };
    std::vector<Limb> queue;
    std::vector<Limb> next;

    const uint32_t majors = p.majors_min
        + static_cast<uint32_t>(rng.unit()
                                * static_cast<float>(p.majors_max - p.majors_min + 1));
    const uint32_t n_major = std::clamp(majors, p.majors_min, p.majors_max);

    // THE FIRST ORDER IS SIZED FROM THE SPREAD IT HAS TO COVER, not chosen.
    // A limb bends upward as it goes, so its HORIZONTAL reach is less than its
    // length; summing the geometric series of the decaying children gives the
    // total run, and length0 is solved from it. Getting this from a number
    // typed by hand is how a tree ends up with a 30 m crown brief and a 12 m
    // crown (the pine's own history, NUMBERS: crown_width_frac "calibrated
    // against the built tree").
    float chain = 0.0f;
    float f = 1.0f;
    for (uint32_t d = 0; d <= p.depth; ++d) {
        chain += f;
        f *= p.length_decay;
    }
    const float horizontal_fraction = std::max(0.35f, std::sin(p.major_pitch));
    const float length0 = (chain > 1e-3f)
        ? p.spread_target / (chain * horizontal_fraction)
        : p.length0;

    // A deterministic azimuth offset per tree: two great oaks with the same
    // limb count must still not be the same tree.
    const float az0 = rng.unit() * TAU_F;
    // ASYMMETRY IS PER-LIMB AND LARGE. Equal limbs give a rotationally
    // symmetric dome; the user asked for two-lobed, elliptical AND "many
    // different", and the difference between those outcomes IS the spread of
    // this multiplier. A tree whose limbs differ by 40 % reads as an individual.
    for (uint32_t k = 0; k < n_major; ++k) {
        const float az = az0 + TAU_F * static_cast<float>(k) / static_cast<float>(n_major)
            + rng.sym() * (0.9f / static_cast<float>(n_major));
        const float pitch = p.major_pitch + rng.sym() * p.pitch_jitter;
        const glm::vec3 dir = norm_or({std::cos(az) * std::sin(pitch), std::cos(pitch),
                                       std::sin(az) * std::sin(pitch)},
                                      {0.0f, 1.0f, 0.0f});
        // Walk DOWN the eligible bole as k rises, so the limbs are staggered in
        // height as well as in azimuth. The topmost limb keeps the apex.
        const size_t bi = (bole.size() > 1)
            ? std::min(bole.size() - 1,
                       static_cast<size_t>(static_cast<float>(k)
                                           / static_cast<float>(n_major)
                                           * static_cast<float>(bole.size())))
            : 0;
        const int from = bole[bole.size() - 1 - bi];
        queue.push_back(Limb{from, dir, length0 * (0.78f + rng.unit() * 0.55f), 0});
    }

    for (uint32_t depth = 0; depth <= p.depth; ++depth) {
        // FEWER NODES PER LIMB AS THE LIMBS GET SMALLER, and this is a budget
        // rule rather than a botanical one: limb COUNT grows geometrically with
        // depth, so a constant node count per limb spends the whole budget on
        // the twigs and runs out before the far side of the tree is grown —
        // which does not look like a cheap tree, it looks like a broken one.
        // A first-order limb is 14 m long and needs to curve; a fifth-order one
        // is 2 m and does not.
        const int seg_count =
            std::max(1, static_cast<int>(p.segments_per_limb) - static_cast<int>(depth));
        next.clear();
        for (const Limb& limb : queue) {
            if (sk.nodes.size() >= p.max_nodes) break;
            // Walk the limb in a few steps so it CURVES: a straight segment per
            // order is a lightning bolt, and the thing that makes an oak limb
            // read as an oak limb is that it bends up as it goes out.
            int cur = limb.node;
            glm::vec3 dir = limb.dir;
            const float step = limb.len / static_cast<float>(seg_count);
            bool vetoed = false;
            for (int s = 0; s < seg_count; ++s) {
                // Gravity pulls the tip down, light pulls it up, and the wind
                // lean drifts the whole crown. One vector, as in colonize().
                const float up = p.phototropism - p.droop * static_cast<float>(depth + 1);
                dir = norm_or(dir + glm::vec3{p.lean.x, up, p.lean.y} * 0.30f
                                  + glm::vec3{rng.sym(), rng.sym() * 0.4f, rng.sym()}
                                      * 0.10f,
                              dir);
                SkeletonNode n;
                n.pos = sk.nodes[static_cast<size_t>(cur)].pos + dir * step;
                // The bounding cylinder (FractalParams::top_y / max_radius).
                n.pos.y = std::min(n.pos.y, p.top_y);
                {
                    const float ox = n.pos.x - p.axis.x;
                    const float oz = n.pos.z - p.axis.y;
                    const float rr = std::sqrt(ox * ox + oz * oz);
                    if (rr > p.max_radius && rr > 1e-4f) {
                        n.pos.x = p.axis.x + ox * p.max_radius / rr;
                        n.pos.z = p.axis.y + oz * p.max_radius / rr;
                    }
                }
                // CROWN SHYNESS: does this step cross a neighbour's boundary?
                // Checked on the PROPOSED position, so the shoot stops at the
                // channel instead of poking through it and being pulled back —
                // a clipped branch still points at the neighbour and still
                // reads as contact.
                if (p.crowd != nullptr && p.crowd_count > 0) {
                    const auto* edges =
                        static_cast<const FloraShape::CrownEdge*>(p.crowd);
                    const float ox = n.pos.x - p.crowd_origin.x;
                    const float oz = n.pos.z - p.crowd_origin.y;
                    for (uint32_t e = 0; e < p.crowd_count; ++e) {
                        const float along = ox * edges[e].dir.x + oz * edges[e].dir.y;
                        // The wobble is keyed to the LIMB, not to the step, so
                        // one shoot stops on one line rather than zig-zagging
                        // along its own length — the channel wanders between
                        // branches, which is what a real one does.
                        const float wobble = p.crowd_jitter * rng.sym();
                        const float bound = std::max(edges[e].limit - p.crowd_inset,
                                                     p.crowd_floor);
                        if (along > bound + wobble) {
                            vetoed = true;
                            break;
                        }
                    }
                    if (vetoed) break;
                }
                n.parent = cur;
                n.order = static_cast<uint8_t>(std::min<uint32_t>(255u, depth + 1u));
                sk.nodes.push_back(n);
                ++sk.nodes[static_cast<size_t>(cur)].children;
                cur = static_cast<int>(sk.nodes.size()) - 1;
            }
            if (vetoed) {
                // The shoot died back at the channel. It is still a live tip —
                // a branch that stops growing toward its neighbour does not
                // shed its leaves — so it carries foliage here and spawns no
                // children.
                sk.leaf_sites.push_back(sk.nodes[static_cast<size_t>(cur)].pos);
                sk.leaf_anchor.push_back(cur);
                continue;
            }
            if (depth == p.depth) {
                // Terminal tip: this is where foliage may hang, and it names its
                // own node, so the attachment invariant holds here too.
                sk.leaf_sites.push_back(sk.nodes[static_cast<size_t>(cur)].pos);
                sk.leaf_anchor.push_back(cur);
                continue;
            }
            const uint32_t kids = p.children_min
                + static_cast<uint32_t>(rng.unit()
                                        * static_cast<float>(p.children_max
                                                             - p.children_min + 1));
            const uint32_t n_kids = std::clamp(kids, p.children_min, p.children_max);
            const glm::vec3 side = norm_or(glm::cross(dir, glm::vec3{0.0f, 1.0f, 0.0f}),
                                           glm::vec3{1.0f, 0.0f, 0.0f});
            const glm::vec3 up = glm::cross(side, dir);
            for (uint32_t c = 0; c < n_kids; ++c) {
                const float roll = GOLDEN_ANGLE_F * static_cast<float>(c)
                    + rng.sym() * 0.5f + static_cast<float>(depth) * 1.1f;
                const float open = p.pitch_spread + rng.sym() * p.pitch_jitter;
                const glm::vec3 cdir =
                    norm_or(dir + (side * std::cos(roll) + up * std::sin(roll))
                                      * std::tan(std::clamp(open, 0.10f, 1.20f)),
                            dir);
                next.push_back(Limb{cur, cdir,
                                    limb.len * p.length_decay * (0.80f + rng.unit() * 0.45f),
                                    depth + 1});
            }
        }
        queue.swap(next);
        if (queue.empty()) break;
    }
    // Anything still queued when the node budget ran out is a live tip too:
    // dropping it would leave a branch with no foliage on it, which is the one
    // failure mode this file exists to prevent, arriving from the other side.
    for (const Limb& limb : queue) {
        sk.leaf_sites.push_back(sk.nodes[static_cast<size_t>(limb.node)].pos);
        sk.leaf_anchor.push_back(limb.node);
    }
}

void assign_pipe_radii(Skeleton& sk, float root_radius, float exponent,
                       float min_radius) {
    const size_t n = sk.nodes.size();
    if (n == 0) return;
    const float e = std::clamp(exponent, 1.5f, 4.0f);
    // Basipetal: children always have a HIGHER index than their parent (both
    // growers append), so one reverse sweep is a correct topological order.
    std::vector<float> pow_r(n, 0.0f);
    for (size_t i = n; i-- > 0;) {
        if (sk.nodes[i].children == 0) pow_r[i] = 1.0f; // r0 = 1, unitless
        const int par = sk.nodes[i].parent;
        if (par >= 0) pow_r[static_cast<size_t>(par)] += pow_r[i];
    }
    // pow_r now holds the TIP COUNT supported by each node, which under
    // r^n = sum(r_child^n) with r_tip = 1 is exactly r^n.
    float root_pow = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        if (sk.nodes[i].parent < 0) root_pow = std::max(root_pow, pow_r[i]);
    }
    if (root_pow <= 0.0f) root_pow = 1.0f;
    const float scale = root_radius / std::pow(root_pow, 1.0f / e);
    for (size_t i = 0; i < n; ++i) {
        const float r = std::pow(std::max(pow_r[i], 1.0f), 1.0f / e) * scale;
        // Clamp UP to the shadow-caster floor. A thinner caster is invisible to
        // the shadow map (flora.md §3.5) — but note the difference from the old
        // code, which TERMINATED such a branch and took its foliage with it
        // (defect 3). Clamping up cannot detach anything.
        sk.nodes[i].radius = std::max(r, min_radius);
    }
}

void decimate(Skeleton& sk, uint32_t keep_every) {
    const size_t n = sk.nodes.size();
    if (n < 3 || keep_every < 2) return;
    std::vector<uint8_t> keep(n, 0);
    std::vector<uint32_t> run(n, 0);
    std::vector<uint8_t> anchored(n, 0);
    for (int a : sk.leaf_anchor) {
        if (a >= 0 && static_cast<size_t>(a) < n) anchored[static_cast<size_t>(a)] = 1;
    }
    for (size_t i = 0; i < n; ++i) {
        const SkeletonNode& nd = sk.nodes[i];
        const bool must =
            nd.parent < 0 || nd.children != 1 || nd.trunk || anchored[i] != 0;
        const int par = nd.parent;
        const uint32_t prev = par >= 0 ? run[static_cast<size_t>(par)] : 0u;
        if (must || prev + 1 >= keep_every) {
            keep[i] = 1;
            run[i] = 0;
        } else {
            run[i] = prev + 1;
        }
    }
    // Re-parent every kept node onto its nearest kept ancestor, then compact.
    std::vector<int> remap(n, -1);
    std::vector<SkeletonNode> out;
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        if (!keep[i]) continue;
        SkeletonNode nd = sk.nodes[i];
        int par = nd.parent;
        while (par >= 0 && !keep[static_cast<size_t>(par)]) par = sk.nodes[static_cast<size_t>(par)].parent;
        nd.parent = par >= 0 ? remap[static_cast<size_t>(par)] : -1;
        nd.children = 0;
        remap[i] = static_cast<int>(out.size());
        out.push_back(nd);
    }
    for (SkeletonNode& nd : out) {
        if (nd.parent >= 0) ++out[static_cast<size_t>(nd.parent)].children;
    }
    for (size_t i = 0; i < sk.leaf_anchor.size(); ++i) {
        int a = sk.leaf_anchor[i];
        while (a >= 0 && !keep[static_cast<size_t>(a)]) a = sk.nodes[static_cast<size_t>(a)].parent;
        sk.leaf_anchor[i] = a >= 0 ? remap[static_cast<size_t>(a)] : -1;
    }
    sk.nodes.swap(out);
}

void decimate_to(Skeleton& sk, uint32_t max_segments) {
    for (int pass = 0; pass < 6; ++pass) {
        uint32_t segs = 0;
        for (const SkeletonNode& n : sk.nodes) {
            if (n.parent >= 0) ++segs;
        }
        if (segs <= max_segments) return;
        decimate(sk, 2);
    }
}

void soften_forks(Skeleton& sk, float amount) {
    const size_t n = sk.nodes.size();
    if (n < 2 || amount <= 0.0f) return;
    const float a = std::clamp(amount, 0.0f, 0.5f);
    const std::vector<SkeletonNode> old = sk.nodes;
    for (size_t i = 0; i < n; ++i) {
        const int par = sk.nodes[i].parent;
        if (par < 0 || sk.nodes[i].trunk) continue;
        // Only the first node PAST a fork: the paper moves every node, which
        // also shortens the tree, and our height band is a cross-zone contract.
        if (old[static_cast<size_t>(par)].children < 2) continue;
        sk.nodes[i].pos = old[i].pos + (old[static_cast<size_t>(par)].pos - old[i].pos) * a;
    }
}

void gather_foliage_anchors(const Skeleton& sk, uint32_t target_count,
                            std::vector<glm::vec3>& centres, std::vector<int>& anchors,
                            std::vector<float>& reach) {
    centres.clear();
    anchors.clear();
    reach.clear();
    const size_t m = sk.leaf_sites.size();
    if (m == 0 || target_count == 0) return;

    // Merge leaf sites greedily into `target_count` clusters. The merge radius
    // is derived from the leaf cloud's own extent, so it scales with the tree
    // and needs no per-species tuning.
    glm::vec3 lo = sk.leaf_sites[0];
    glm::vec3 hi = sk.leaf_sites[0];
    for (const glm::vec3& s : sk.leaf_sites) {
        lo = glm::min(lo, s);
        hi = glm::max(hi, s);
    }
    const glm::vec3 ext = hi - lo;
    const float diag = std::max(glm::length(ext), 0.5f);
    float merge = diag / std::max(1.0f, std::cbrt(static_cast<float>(target_count)) * 1.6f);

    std::vector<glm::vec3> sum;
    std::vector<uint32_t> count;
    std::vector<int> anchor_of;
    for (int pass = 0; pass < 6; ++pass) {
        sum.clear();
        count.clear();
        anchor_of.clear();
        for (size_t i = 0; i < m; ++i) {
            const glm::vec3 s = sk.leaf_sites[i];
            int best = -1;
            float best_d2 = merge * merge;
            for (size_t c = 0; c < sum.size(); ++c) {
                const glm::vec3 d = s - sum[c] / static_cast<float>(count[c]);
                const float d2 = glm::dot(d, d);
                if (d2 < best_d2) {
                    best_d2 = d2;
                    best = static_cast<int>(c);
                }
            }
            if (best < 0) {
                sum.push_back(s);
                count.push_back(1);
                anchor_of.push_back(sk.leaf_anchor[i]);
            } else {
                sum[static_cast<size_t>(best)] += s;
                ++count[static_cast<size_t>(best)];
            }
        }
        if (sum.size() <= target_count) break;
        merge *= 1.32f;
    }

    for (size_t c = 0; c < sum.size(); ++c) {
        const glm::vec3 centre = sum[c] / static_cast<float>(count[c]);
        int anchor = anchor_of[c];
        // The cluster centre moved when its members were averaged in, so the
        // anchor is re-chosen as the NEAREST node. That is the only step that
        // could ever break attachment, so it is done by search rather than by
        // assumption, and the suite measures the resulting gap.
        float best_d2 = 1e18f;
        for (size_t v = 0; v < sk.nodes.size(); ++v) {
            const glm::vec3 d = centre - sk.nodes[v].pos;
            const float d2 = glm::dot(d, d);
            if (d2 < best_d2) {
                best_d2 = d2;
                anchor = static_cast<int>(v);
            }
        }
        float r = 0.0f;
        for (size_t i = 0; i < m; ++i) {
            const glm::vec3 d = sk.leaf_sites[i] - centre;
            if (glm::dot(d, d) < merge * merge) r = std::max(r, glm::length(d));
        }
        centres.push_back(centre);
        anchors.push_back(anchor);
        reach.push_back(std::max(r, merge * 0.5f));
    }
}

void gather_shoot_anchors(const Skeleton& sk, const ShootFoliage& p, uint64_t seed,
                          std::vector<glm::vec3>& centres,
                          std::vector<int>& anchors) {
    centres.clear();
    anchors.clear();
    if (sk.nodes.empty() || p.target_count == 0) return;
    Rng rng{seed * 0x2545F4914F6CDD1Dull + 0xA24BAull};

    // --- The shoots ---------------------------------------------------------
    // A shoot is an EDGE: the segment between a node and its parent. Eligible
    // edges are the ones a leaf could actually grow on — above the foliage
    // line, thin enough to be a shoot rather than a limb, and not a piece of
    // the authored bole.
    struct Shoot {
        int node;      ///< the child end (the anchor)
        glm::vec3 a;   ///< parent position
        glm::vec3 b;   ///< child position
        float len;
        bool tip;
    };
    std::vector<Shoot> shoots;
    shoots.reserve(sk.nodes.size());
    double total_len = 0.0;
    for (size_t i = 0; i < sk.nodes.size(); ++i) {
        const SkeletonNode& n = sk.nodes[i];
        if (n.parent < 0) continue;
        const SkeletonNode& par = sk.nodes[static_cast<size_t>(n.parent)];
        if (n.trunk && par.trunk) continue;
        if (n.pos.y < p.base_y) continue;
        if (n.radius > p.outer_radius) continue;
        const float len = glm::length(n.pos - par.pos);
        if (len < 1e-3f) continue;
        shoots.push_back(Shoot{static_cast<int>(i), par.pos, n.pos, len,
                               n.children == 0});
        total_len += len;
    }
    // NOTHING ELIGIBLE IS NOT NOTHING TO DRAW. A young or heavily decimated
    // crown can have every surviving node above the thickness gate; refusing to
    // leaf it would ship a bare tree, which is the failure mode this file's
    // header is about, arriving from the other side. Relax the gate rather than
    // return empty.
    if (shoots.empty()) {
        for (size_t i = 0; i < sk.nodes.size(); ++i) {
            const SkeletonNode& n = sk.nodes[i];
            if (n.parent < 0 || n.pos.y < p.base_y) continue;
            const SkeletonNode& par = sk.nodes[static_cast<size_t>(n.parent)];
            const float len = glm::length(n.pos - par.pos);
            if (len < 1e-3f) continue;
            shoots.push_back(Shoot{static_cast<int>(i), par.pos, n.pos, len,
                                   n.children == 0});
            total_len += len;
        }
    }
    if (shoots.empty() || total_len <= 0.0) return;

    // --- Sites --------------------------------------------------------------
    // EVERY TIP CARRIES FOLIAGE FIRST. A branch that ends in nothing is the
    // «winter tree» reading, and it is the failure the great oak's own frame
    // caught; spending the budget on tips before anything else makes it
    // impossible by construction. The remainder is spread over the eligible
    // length, so a long shoot carries more masses than a short one — which is
    // simply what more shoot means.
    const auto want = static_cast<size_t>(p.target_count);
    size_t tips = 0;
    for (const Shoot& s : shoots) {
        if (s.tip) ++tips;
    }
    const double spare = (want > tips) ? static_cast<double>(want - tips) : 0.0;

    auto place = [&](const Shoot& s, float u) {
        // Toward the OUTER end of the shoot: needles and leaves live on the
        // last growth, and the inboard wood shows daylight.
        const glm::vec3 axis = norm_or(s.b - s.a, glm::vec3{0.0f, 1.0f, 0.0f});
        glm::vec3 at = s.a + (s.b - s.a) * u;
        if (p.stand_off > 0.0f) {
            // Roll the offset around the shoot at the golden angle: consecutive
            // masses on one branch never stack on the same side.
            const float roll = GOLDEN_ANGLE_F * static_cast<float>(centres.size())
                + rng.sym() * 0.4f;
            const glm::vec3 side =
                norm_or(glm::cross(axis, glm::vec3{0.0f, 1.0f, 0.0f}),
                        glm::vec3{1.0f, 0.0f, 0.0f});
            const glm::vec3 up = glm::cross(side, axis);
            // AND IT DOES NOT LEAN OUTWARD, which was tried and measured.
            // Pushing the mass toward the light looks right on paper and is
            // right botanically, but it moves the mass into the envelope
            // containment: the willow lost 49 % of its cards and 1.7 m of crown
            // TOP in one run, because a card shoved outward is a card the
            // containment shrinks and the scrap floor then drops. The crown's
            // built width has to come from where the WOOD reaches, not from
            // leaning its leaves off the end.
            const glm::vec3 dir = side * std::cos(roll) + up * std::sin(roll);
            at += dir * (p.stand_off * (0.35f + 0.65f * rng.unit()));
        }
        centres.push_back(at);
        anchors.push_back(s.node);
    };

    // NO TWO MASSES IN THE SAME PLACE. Foliage on shoots puts sites where the
    // WOOD is, and wood forks — so two sites can land centimetres apart, which
    // is not a leaf spray, it is one spray drawn twice at full overdraw. The
    // separation is 0.12 m: below that two card clusters are the same object by
    // any reading, including the suite's, which groups cards into clusters by a
    // 0.05 m proximity key and reported a "cluster of six cards with two flat
    // ones" for every coincident pair.
    constexpr float MIN_SEP = 0.12f;
    auto too_close = [&](const glm::vec3& q) {
        for (const glm::vec3& c : centres) {
            if (glm::dot(c - q, c - q) < MIN_SEP * MIN_SEP) return true;
        }
        return false;
    };
    auto place_spaced = [&](const Shoot& s, float u) {
        // A TIP GETS THREE TRIES BEFORE IT IS GIVEN UP, walking back down its
        // own shoot, because a branch that ends in nothing is the winter-tree
        // reading and that is the more expensive of the two failures.
        for (int attempt = 0; attempt < 3; ++attempt) {
            const float uu = u * (1.0f - 0.18f * static_cast<float>(attempt));
            const glm::vec3 probe = s.a + (s.b - s.a) * uu;
            if (!too_close(probe)) {
                place(s, uu);
                return true;
            }
        }
        return false;
    };

    for (const Shoot& s : shoots) {
        if (!s.tip) continue;
        place_spaced(s, 1.0f);
    }
    if (spare > 0.0) {
        double carry = 0.0;
        for (const Shoot& s : shoots) {
            carry += spare * (static_cast<double>(s.len) / total_len);
            while (carry >= 1.0 && centres.size() < want * 2) {
                carry -= 1.0;
                place_spaced(s, 0.42f + rng.unit() * 0.58f);
            }
        }
    }
}

} // namespace dfn::render
