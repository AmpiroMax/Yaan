/*
Module: engine/render
File: engine/render/sources/FloraWeber.cpp

Responsibility:
- The Weber & Penn 1995 stem grammar: recursive stems with per-level laws for
  declination, rotation, length, taper, curvature and splitting.

Key items:
- weber_skeleton(), weber_shape_ratio().

Dependencies:
- Uses: FloraWeber.h, FloraSkeleton.h, ProcFlora.h (the shyness boundaries), glm.
- Used by: ProcFlora.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md.
- PURE AND DETERMINISTIC. Same (params, seed) -> byte-identical output.
- THE MODEL IS THE PAPER'S, THE SPECIES NUMBERS ARE OURS (see FloraWeber.h).
*/

#include "engine/render/sources/FloraWeber.h"

#include "engine/render/sources/ProcFlora.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace dfn::render {

namespace {

constexpr float DEG = 3.14159265358979f / 180.0f;

struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed | 1ull) {}
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return static_cast<uint32_t>(s >> 32);
    }
    float unit() { return static_cast<float>(next() & 0xFFFFFFu) / 16777215.0f; }
    float sym() { return unit() * 2.0f - 1.0f; }
};

glm::vec3 norm_or(glm::vec3 v, glm::vec3 fallback) {
    const float l = glm::length(v);
    return l > 1e-6f ? v / l : fallback;
}

/// Rotate `v` about `axis` by `a` radians (Rodrigues). The paper works in a
/// per-stem frame; a rotation helper is all that frame really needs.
glm::vec3 rotate_about(glm::vec3 v, glm::vec3 axis, float a) {
    const float c = std::cos(a);
    const float s = std::sin(a);
    return v * c + glm::cross(axis, v) * s + axis * glm::dot(axis, v) * (1.0f - c);
}

} // namespace

float weber_shape_ratio(WeberShape shape, float ratio) {
    const float r = std::clamp(ratio, 0.0f, 1.0f);
    switch (shape) {
    case WeberShape::Conical:       return 0.2f + 0.8f * r;
    case WeberShape::Spherical:     return 0.2f + 0.8f * std::sin(3.14159265f * r);
    case WeberShape::Hemispherical: return 0.2f + 0.8f * std::sin(1.5707963f * r);
    case WeberShape::Cylindrical:   return 1.0f;
    case WeberShape::TaperedCyl:    return 0.5f + 0.5f * r;
    case WeberShape::Flame:
        return (r <= 0.7f) ? (r / 0.7f) : ((1.0f - r) / 0.3f);
    case WeberShape::InverseConical: return 1.0f - 0.8f * r;
    case WeberShape::TendFlame:
    default:
        return (r <= 0.7f) ? (0.5f + 0.5f * r / 0.7f)
                           : (0.5f + 0.5f * (1.0f - r) / 0.3f);
    }
}

namespace {

/// Everything one recursive call needs. Passed by value; the recursion is at
/// most four deep by construction (WeberParams::levels <= 4).
struct StemCtx {
    int parent_node;      ///< skeleton node the stem grows FROM
    glm::vec3 pos;        ///< its position
    glm::vec3 dir;        ///< unit heading
    float length;         ///< m
    float radius;         ///< m at the base of this stem
    uint32_t level;
    bool trunk;
    /// THE AUTHORED-BOLE GUIDE (WeberParams::bole). While a level-0 stem still
    /// has guide ahead of it, its segments WALK the polyline instead of obeying
    /// curvature/lean — the drawn trunk and the branch-carrying trunk are one
    /// object. Children never inherit it (they are real, drawn wood).
    const glm::vec3* guide = nullptr;
    uint32_t guide_count = 0;
};

struct Grower {
    Skeleton& sk;
    const WeberParams& p;
    Rng rng;
    /// The level count AFTER the budget fit below; the model's own `levels` is
    /// what the species asks for, this is what the budget affords.
    uint32_t levels = 3;

    /// Is this step past a neighbour's crown boundary? Crown shyness, the same
    /// rule the fractal grower carries, so the two growers cannot disagree
    /// about what "shy" means (Rule 32: one mechanism, every consumer).
    [[nodiscard]] bool vetoed(const glm::vec3& at) {
        if (p.crowd == nullptr || p.crowd_count == 0) return false;
        const auto* edges = static_cast<const FloraShape::CrownEdge*>(p.crowd);
        const float ox = at.x - p.crowd_origin.x;
        const float oz = at.z - p.crowd_origin.y;
        for (uint32_t e = 0; e < p.crowd_count; ++e) {
            const float along = ox * edges[e].dir.x + oz * edges[e].dir.y;
            const float bound =
                std::max(edges[e].limit - p.crowd_inset, p.crowd_floor);
            if (along > bound + p.crowd_jitter * rng.sym()) return true;
        }
        return false;
    }

    /// The bounding cylinder. The species HEIGHT band and crown width are
    /// cross-zone contracts, and a recursive grower overshoots both cheerfully
    /// — the fractal grower reached twice its declared height before it was
    /// clipped (FloraSkeleton.h records the measurement).
    [[nodiscard]] glm::vec3 clip(glm::vec3 q) const {
        q.y = std::min(q.y, p.top_y);
        const float ox = q.x - p.axis.x;
        const float oz = q.z - p.axis.y;
        const float rr = std::sqrt(ox * ox + oz * oz);
        if (rr > p.max_radius && rr > 1e-4f) {
            q.x = p.axis.x + ox * p.max_radius / rr;
            q.z = p.axis.y + oz * p.max_radius / rr;
        }
        return q;
    }

    int push(const glm::vec3& at, int parent, uint32_t level, bool trunk, float r) {
        SkeletonNode n;
        n.pos = at;
        n.parent = parent;
        n.trunk = trunk;
        n.order = static_cast<uint8_t>(std::min<uint32_t>(255u, level));
        n.radius = r;
        sk.nodes.push_back(n);
        if (parent >= 0) ++sk.nodes[static_cast<size_t>(parent)].children;
        return static_cast<int>(sk.nodes.size()) - 1;
    }

    /// Grows ONE stem and appends the stems it spawns to `out`.
    /// Deliberately NOT recursive -- see weber_skeleton().
    void grow(StemCtx c, std::vector<StemCtx>& out);
};

void Grower::grow(StemCtx c, std::vector<StemCtx>& out) {
    if (c.level > levels || sk.nodes.size() >= p.max_nodes) return;
    const WeberLevel& L = p.level[std::min<uint32_t>(c.level, 3)];
    const uint32_t segs = std::max(1u, L.curve_res);
    const float seg_len = c.length / static_cast<float>(segs);

    // --- CURVATURE. The paper turns the stem by curve/curve_res per segment,
    // or, when curve_back is non-zero, one way over the first half and the
    // other over the second — which is the S-shaped bole a broadleaf actually
    // has and the thing a straight-axis generator can never produce.
    const bool s_curve = std::fabs(L.curve_back) > 1e-4f;

    // The stem's own frame. `side` is what curvature and splits rotate about.
    glm::vec3 side = norm_or(glm::cross(c.dir, glm::vec3{0.0f, 1.0f, 0.0f}),
                             glm::vec3{1.0f, 0.0f, 0.0f});

    // Where the children of the NEXT level will hang, collected as we walk so
    // they can be spawned against the finished stem.
    struct Site {
        glm::vec3 pos;
        glm::vec3 dir;
        int node;
        float offset_frac;
    };
    std::vector<Site> sites;

    glm::vec3 pos = c.pos;
    glm::vec3 dir = c.dir;
    int node = c.parent_node;
    // Split clones are grown after this stem finishes, so the trunk's own run
    // is contiguous in the node array (assign_pipe_radii wants children after
    // parents, and every grower in this zone keeps that property).
    std::vector<StemCtx> clones;

    // The paper's split error accumulator: `seg_splits` is fractional and the
    // running error makes "one segment in five" exact rather than a coin toss.
    float split_error = 0.0f;

    // --- THE AUTHORED-BOLE GUIDE (StemCtx::guide). Arc-parameterised: while
    // guide remains, a segment WALKS the drawn trunk and takes no curvature,
    // no lean and no splits of its own — the authored bole already carries the
    // sweep and the wind lean, and adding the model's on top is exactly the
    // double-count that put 94-100 % of branch bases off the drawn surface.
    const bool has_guide = c.guide != nullptr && c.guide_count >= 2;
    float guide_total = 0.0f;
    if (has_guide) {
        for (uint32_t i = 1; i < c.guide_count; ++i) {
            guide_total += glm::length(c.guide[i] - c.guide[i - 1]);
        }
    }
    auto guide_point = [&c](float a, glm::vec3& out_dir) {
        float acc = 0.0f;
        for (uint32_t i = 1; i < c.guide_count; ++i) {
            const glm::vec3 seg = c.guide[i] - c.guide[i - 1];
            const float l = glm::length(seg);
            if ((acc + l >= a && l > 1e-6f) || i + 1 == c.guide_count) {
                out_dir = l > 1e-6f ? seg / l : glm::vec3{0.0f, 1.0f, 0.0f};
                const float t = l > 1e-6f ? std::clamp((a - acc) / l, 0.0f, 1.0f)
                                          : 0.0f;
                return c.guide[i - 1] + seg * t;
            }
            acc += l;
        }
        out_dir = glm::vec3{0.0f, 1.0f, 0.0f};
        return c.guide[c.guide_count - 1];
    };
    float arc = 0.0f;
    bool on_guide = has_guide && guide_total > 1e-3f;
    bool base_forked = false;

    for (uint32_t s = 0; s < segs; ++s) {
        if (sk.nodes.size() >= p.max_nodes) break;
        const float u = static_cast<float>(s) / static_cast<float>(segs);

        glm::vec3 next;
        const bool seg_guided = on_guide;
        if (on_guide) {
            const float want = arc + seg_len;
            if (want < guide_total) {
                next = guide_point(want, dir);
            } else {
                // The bole ends inside this segment: finish it as FREE, DRAWN
                // wood continuing the bole's own direction.
                glm::vec3 end_dir;
                const glm::vec3 end = guide_point(guide_total, end_dir);
                dir = end_dir;
                next = clip(end + dir * (want - guide_total));
                on_guide = false;
            }
            arc = want;
        } else {
            // Declination change for this segment.
            float turn = 0.0f;
            if (s_curve) {
                turn = (u < 0.5f)
                    ? (L.curve * DEG / (static_cast<float>(segs) * 0.5f))
                    : (L.curve_back * DEG / (static_cast<float>(segs) * 0.5f));
            } else {
                turn = L.curve * DEG / static_cast<float>(segs);
            }
            turn += L.curve_v * DEG * rng.sym() / static_cast<float>(segs);
            dir = norm_or(rotate_about(dir, side, turn), dir);

            // ATTRACTION UP (the paper's AttractionUp). Applied from level 2
            // down, because the trunk does not need persuading and the main
            // limbs carry the declination the species asked for. It is what
            // turns a drooping limb up at its end — the single cheapest cue
            // that a branch is alive.
            if (c.level >= 2 && p.attraction_up != 0.0f) {
                const glm::vec3 up{0.0f, 1.0f, 0.0f};
                const float declination = std::acos(std::clamp(dir.y, -1.0f, 1.0f));
                const float orient = std::sin(declination);
                const float curve_up =
                    p.attraction_up * declination * orient / static_cast<float>(segs);
                const glm::vec3 axis_up = norm_or(glm::cross(dir, up), side);
                dir = norm_or(rotate_about(dir, axis_up, -curve_up), dir);
            }
            // The whole-tree wind lean, as in the other growers. NOT past the
            // guide either: a united stem's free run is the LEADER of a bole
            // that already leans, and re-applying the lean bends the tree
            // twice (the banana the user rejected on the boles).
            if (!has_guide && (p.lean.x != 0.0f || p.lean.y != 0.0f)) {
                dir = norm_or(dir + glm::vec3{p.lean.x, 0.0f, p.lean.y}
                                        * (0.30f / static_cast<float>(segs)),
                              dir);
            }

            next = clip(pos + dir * seg_len);
        }
        if (c.level > 0 && vetoed(next)) break; // shy: the stem stops here

        // TAPER along the stem, the paper's `nTaper`: 0 leaves a cylinder, 1
        // closes to a point, and the in-between is what a real stem does.
        const float t1 = static_cast<float>(s + 1) / static_cast<float>(segs);
        const float r1 = c.radius * std::max(0.05f, 1.0f - L.taper * t1);
        // On a guided stem only the GUIDED run is trunk (skipped by the mesh
        // side as already-drawn bole); the free run past the bole's end is the
        // central leader, which is real drawn wood.
        node = push(next, node, c.level, c.trunk && (!has_guide || seg_guided), r1);
        pos = next;
        side = norm_or(glm::cross(dir, glm::vec3{0.0f, 1.0f, 0.0f}), side);

        // --- BASE SPLITS AT THE BOLE'S END, DRAWN (united-bole path only). --
        // The paper forks the trunk at its base; our clear-bole contract never
        // allowed a ground fork, and the old answer — grow the forks invisibly
        // from the ground — is the two-trunk defect. The fork now happens
        // where the drawn bole ends, and the leaders are ordinary drawn wood.
        // The 0.16 rad floor is the old axes' outward lean, kept so a species
        // whose split_angle is small (willow, 3 deg) still opens visibly.
        if (has_guide && !on_guide && !base_forked) {
            base_forked = true;
            for (uint32_t b = 0; b < p.base_splits; ++b) {
                if (sk.nodes.size() >= p.max_nodes) break;
                const float ang = std::max(
                    (L.split_angle + L.split_angle_v * rng.sym()) * DEG, 0.16f);
                const float roll =
                    6.2831853f
                    * (static_cast<float>(b + 1) + rng.sym() * 0.25f)
                    / static_cast<float>(p.base_splits + 1);
                glm::vec3 cd = rotate_about(dir, side, ang);
                cd = norm_or(rotate_about(cd, dir, roll), cd);
                clones.push_back(StemCtx{node, pos, cd,
                                         c.length * (1.0f - t1) * 0.92f,
                                         r1 * 0.86f, c.level, false});
            }
        }

        // --- STEM SPLITTING -------------------------------------------------
        // The mechanism that gives a broadleaf its forked crown, and the one
        // our earlier growers had no equivalent of: a stem does not always
        // carry children, sometimes it BECOMES two stems of its own level.
        // Not on the guided run: the drawn bole cannot fork below its own top.
        if (L.seg_splits > 0.0f && s + 1 < segs && !seg_guided) {
            const float want = L.seg_splits + split_error;
            const auto n_split = static_cast<int>(std::floor(want + 0.5f));
            split_error = want - static_cast<float>(n_split);
            for (int k = 0; k < n_split; ++k) {
                const float ang = (L.split_angle + L.split_angle_v * rng.sym()) * DEG;
                // The paper rotates each clone about the PARENT's axis as well,
                // so a fork opens into space instead of staying in one plane —
                // a planar fork is the single most artificial-looking thing a
                // recursive tree can do.
                const float roll = 3.14159265f * (1.0f + rng.sym() * 0.4f);
                glm::vec3 cd = rotate_about(dir, side, ang);
                cd = norm_or(rotate_about(cd, dir, roll), cd);
                clones.push_back(StemCtx{node, pos, cd,
                                         c.length * (1.0f - t1) * 0.92f,
                                         r1 * 0.86f, c.level, false});
            }
        }

        // Child sites are recorded over the branched part of the stem only.
        const float base = (c.level == 0) ? p.base_size : 0.0f;
        if (t1 > base) sites.push_back(Site{pos, dir, node, t1});
    }

    // A stem of the deepest level carries the foliage. Anchored to its own tip,
    // so nothing can float (FloraSkeleton.h's invariant).
    if (c.level >= levels) {
        sk.leaf_sites.push_back(sk.nodes[static_cast<size_t>(node)].pos);
        sk.leaf_anchor.push_back(node);
        return;
    }

    // --- CHILDREN -----------------------------------------------------------
    const WeberLevel& C = p.level[std::min<uint32_t>(c.level + 1, 3)];
    if (C.branches == 0 || sites.empty()) {
        // A stem with no children is still a live tip.
        sk.leaf_sites.push_back(sk.nodes[static_cast<size_t>(node)].pos);
        sk.leaf_anchor.push_back(node);
    } else {
        const auto n_child = static_cast<int>(C.branches);
        float rot = rng.unit() * 360.0f;
        for (int i = 0; i < n_child; ++i) {
            if (sk.nodes.size() >= p.max_nodes) break;
            // Spread the children over the branched length. The paper places
            // them by offset; walking `sites` in order does the same and keeps
            // every child attached to a node that exists.
            const size_t si =
                std::min(sites.size() - 1,
                         static_cast<size_t>(static_cast<float>(i)
                                             / static_cast<float>(n_child)
                                             * static_cast<float>(sites.size())));
            const Site& st = sites[si];

            // DECLINATION. The negative-variation form is the paper's and it is
            // worth its own line: with down_angle_v < 0 the angle depends on
            // WHERE the child sits, so branches high on the parent point up and
            // low ones point out. That is a real tree's silhouette and it costs
            // one sign.
            float down = C.down_angle;
            if (C.down_angle_v < 0.0f) {
                const float r = std::clamp(st.offset_frac, 0.0f, 1.0f);
                down += std::fabs(C.down_angle_v)
                    * (1.0f - 2.0f * weber_shape_ratio(WeberShape::Conical, r));
            } else {
                down += C.down_angle_v * rng.sym();
            }
            rot += C.rotate + C.rotate_v * rng.sym();

            const glm::vec3 pdir = st.dir;
            glm::vec3 pside = norm_or(glm::cross(pdir, glm::vec3{0.0f, 1.0f, 0.0f}),
                                      glm::vec3{1.0f, 0.0f, 0.0f});
            glm::vec3 cd = rotate_about(pdir, pside, down * DEG);
            cd = norm_or(rotate_about(cd, pdir, rot * DEG), cd);

            // LENGTH. Level 1 reads the crown SHAPE — this is where a species'
            // outline comes from, and it comes from the branches being the
            // right lengths rather than from anything being cut to fit.
            float len_max = C.length + C.length_v * rng.sym();
            float child_len = 0.0f;
            if (c.level == 0) {
                const float base_len = p.base_size * c.length;
                const float denom = std::max(c.length - base_len, 0.01f);
                const float ratio = (c.length - st.offset_frac * c.length - base_len)
                    / denom;
                child_len = c.length * len_max * weber_shape_ratio(p.shape, ratio);
            } else {
                child_len = len_max * (c.length - 0.6f * st.offset_frac * c.length);
            }
            if (child_len < 0.05f) continue;

            // RADIUS, the pipe-model relative of the paper: a child's radius is
            // its parent's scaled by the length ratio to a power. The absolute
            // trunk radius stays the species contract (sim reads it).
            const float rad = std::max(
                sk.nodes[static_cast<size_t>(st.node)].radius
                    * std::pow(std::clamp(child_len / std::max(c.length, 0.01f), 0.05f, 1.0f),
                               p.ratio_power),
                0.005f);

            out.push_back(
                StemCtx{st.node, st.pos, cd, child_len, rad, c.level + 1, false});
        }
    }

    // A split CLONE is a stem of the same level, so it goes back into the same
    // generation rather than into the next one.
    for (const StemCtx& cl : clones) out.push_back(cl);
}

} // namespace

void weber_skeleton(Skeleton& sk, const WeberParams& p, uint64_t seed) {
    sk.nodes.clear();
    sk.leaf_sites.clear();
    sk.leaf_anchor.clear();
    Rng rng{seed * 0x9E3779B97F4A7C15ull + 0xB5026F5Aull};

    Grower g{sk, p, rng, p.levels};
    // The trunk's root node. Marked trunk so the mesh side does not draw the
    // bole twice (ProcFlora's emit_skeleton skips trunk-to-trunk segments).
    SkeletonNode root;
    root.pos = p.base;
    root.trunk = true;
    root.radius = std::max(p.height * p.ratio, 0.02f);
    sk.nodes.push_back(root);

    // --- BREADTH FIRST, AND IT IS NOT A STYLE CHOICE ------------------------
    // The model is naturally recursive and the first draft of this file was.
    // Recursion plus a node budget builds a BROKEN tree rather than a cheap
    // one: depth-first spends the whole budget on the first main limb and its
    // descendants, and every limb after it comes out bare. The budget has to
    // cut the DEEPEST LEVEL uniformly, not the far side of the tree, so the
    // generations are grown one at a time.
    //
    // And when a generation does not fit, it is SUBSAMPLED by stride rather
    // than truncated — the same reason: keeping the first 40 of 300 twigs
    // leaves one bald crown and one furnished one, while keeping every seventh
    // leaves a thinner crown that is still a crown.
    // BASE SPLITS: the trunk leaves the ground as `base_splits + 1` axes. They
    // are all level 0 and all `trunk`, so the bole mesh and sim's capsule still
    // see a trunk — there are simply several of it, which is what an oak is.
    // --- FITTING THE MODEL TO A BUDGET IT WAS NOT WRITTEN FOR ---------------
    // Weber & Penn get their density from the NUMBER of branches per level
    // (`CA Black Oak` carries 40 main limbs and 120 secondaries on each), and
    // they are not spending a triangle budget. We are, and at
    // TREE_TRI_BUDGET_MAX the crown affords about 35 nodes against a model that
    // wants some thousands.
    //
    // THE ANSWER IS THE STRIDE IN THE LOOP BELOW, and it is worth saying what
    // was tried instead: DROPPING THE DEEPEST LEVEL when the estimated cost
    // exceeded the budget. It sounds better than subsampling — a two-level oak
    // is a young oak, a three-level oak missing 95 % of its twigs is a broken
    // one — and it was implemented, measured and removed. It bought nothing at
    // the budgets we actually run (oak at 80 nodes: silhouette ambiguity 0.354
    // -> 0.315, i.e. worse; at 150: 0.424 -> 0.359, worse) and it silently cost
    // the GREAT oak a whole level, taking its wood from 8798 triangles to 2278
    // — the richest tree this zone has measured, thrown away by a heuristic
    // that was defending a budget the giant does not share.
    // Recorded rather than deleted quietly: the idea will occur to the next
    // agent too, and it has already been paid for.
    const uint32_t levels = p.levels;

    g.levels = levels;
    std::vector<StemCtx> current;
    if (p.bole != nullptr && p.bole_count >= 2) {
        // THE UNITED BOLE (WeberParams::bole): ONE level-0 axis, walking the
        // drawn trunk; base_splits fork at its end inside grow(), as drawn
        // leaders. The branch-carrying trunk and the trunk the eye sees are
        // one object — the whole point of the field.
        StemCtx c{0, p.base,
                  norm_or(p.bole[1] - p.bole[0], glm::vec3{0.0f, 1.0f, 0.0f}),
                  p.height, root.radius, 0, true};
        c.guide = p.bole;
        c.guide_count = p.bole_count;
        current.push_back(c);
    } else {
        const uint32_t axes = p.base_splits + 1;
        const float lean_out = (axes > 1) ? 0.16f : 0.0f;
        for (uint32_t a = 0; a < axes; ++a) {
            const float az = 6.2831853f * static_cast<float>(a)
                    / static_cast<float>(axes)
                + rng.unit() * 0.6f;
            const glm::vec3 d = norm_or(glm::vec3{std::cos(az) * lean_out, 1.0f,
                                                  std::sin(az) * lean_out},
                                        glm::vec3{0.0f, 1.0f, 0.0f});
            current.push_back(StemCtx{0, p.base, d, p.height, root.radius, 0, true});
        }
    }
    std::vector<StemCtx> next;
    for (uint32_t level = 0; level <= levels && !current.empty(); ++level) {
        next.clear();
        // What one stem of this generation is about to cost, so the stride can
        // be chosen BEFORE the budget is spent instead of discovered after.
        const uint32_t res = std::max(1u, p.level[std::min<uint32_t>(level, 3)].curve_res);
        const size_t room = (p.max_nodes > sk.nodes.size())
            ? (p.max_nodes - sk.nodes.size()) / res
            : 0;
        const size_t stride =
            (room > 0 && current.size() > room) ? (current.size() + room - 1) / room : 1;
        for (size_t i = 0; i < current.size(); i += stride) {
            if (sk.nodes.size() >= p.max_nodes) break;
            g.grow(current[i], next);
        }
        // Anything skipped by the stride is still a live tip on a real node, so
        // it carries foliage: dropping it silently is how a branch ends in
        // nothing, which is the one failure this zone's skeleton header is
        // about.
        if (stride > 1) {
            for (size_t i = 0; i < current.size(); ++i) {
                if (i % stride == 0) continue;
                sk.leaf_sites.push_back(
                    sk.nodes[static_cast<size_t>(current[i].parent_node)].pos);
                sk.leaf_anchor.push_back(current[i].parent_node);
            }
        }
        current.swap(next);
    }
}

} // namespace dfn::render
