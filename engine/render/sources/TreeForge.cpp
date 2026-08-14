/*
Created: 14:08:2026 - 23:36:19
Last updated: 15:08:2026 - 00:24:00
Module: engine/render
File: engine/render/sources/TreeForge.cpp

Responsibility:
- forge_tree(): the industry-shaped tree (see TreeForge.h for the architecture
  and its research citations), built as three registry streams.

Dependencies:
- Uses: TreeForge.h, FloraBuild.h (tube_segment, blob_cluster, Rng, helpers),
  FloraCards.h (emit_leaf_card, tones), ProcMesh.h (pack).
- Used by: dfn_render target, tools/forge_trees.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic: every draw comes from the params' seed through one Rng.
- Proportion constants here cite the research doc inline. They are the forge's
  own recipe, not NUMBERS rows yet: the user judges the gallery first, and
  numbers that survive his eye get promoted with their derivations (the same
  road every accepted look number has walked).
*/
/*
UPD:
- 14:08:2026 - 23:36:19: Created with TreeForge.h.
- 15:08:2026 - 00:24:00: v2 — скелет виден сквозь крону: каркасные ветви + вторичные ёлочкой,
  лапы-якоря на кончиках и серединах ветвей, КРЕСТ-накрест пары карточек на якорь
  (стандарт SpeedTree: одинокая плоскость исчезает в собственный профиль и мерцает
  при облёте), ядро — малая тёмная глубина за просветами, а не крона. Плотность
  поднята после первого же кадра v2: редкие помпоны — не референс.
*/

#include "engine/render/sources/TreeForge.h"

#include "engine/render/sources/FloraBuild.h"
#include "engine/render/sources/ProcMesh.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace dfn::render {
namespace {

/// Vertical value gradient baked into the crown masses: dark under, lit top —
/// the polycount/Airborn "one dome of light" carried per vertex, so the flat-
/// shaded masses read as one volume under any sun (§1.2 bent normals, §1.3
/// projected normals; we bake the same statement into colour, which the prop
/// program already honours).
[[nodiscard]] uint32_t dome_color(glm::vec3 tone, float ny01, float jitter) {
    const float lit = 0.62f + 0.48f * ny01; // 0.62 under -> 1.10 sunward
    return pack(tone * lit * jitter);
}

/// Fibonacci direction k of n over the upper-biased sphere: even coverage
/// with no rings, no poles-first ordering (the reference masses sit uneven).
[[nodiscard]] glm::vec3 fib_dir(int k, int n) {
    const float t = (static_cast<float>(k) + 0.5f) / static_cast<float>(n);
    // Bias against the underside: y in [-0.25, 1) — a crown has a floor, and
    // masses hanging fully below centre belong to willows, not to this recipe.
    const float y = 1.0f - t * 1.25f;
    const float r = std::sqrt(std::max(0.0f, 1.0f - y * y));
    const float az = GOLDEN_ANGLE * static_cast<float>(k);
    return {r * std::cos(az), y, r * std::sin(az)};
}

} // namespace

RegistryObject forge_tree(const TreeForgeParams& p) {
    RegistryObject obj;
    obj.name = p.name;
    obj.kind = "tree";
    obj.source = "forge:v1 seed=" + std::to_string(p.seed);

    Rng rng(p.seed * 0x9E3779B97F4A7C15ull + 0x243F6A8885A308D3ull);
    const uint32_t bark = pack(p.bark);
    const glm::vec3 tone = leaf_tone_color(p.tone, FloraSeason::Summer);

    const float crown_base = p.height * p.crown_base_frac;
    const float crown_top = p.height;
    const float crown_cy = (crown_base + crown_top) * 0.5f;
    const glm::vec3 crown_c{0.0f, crown_cy, 0.0f};
    const float crown_ry = (crown_top - crown_base) * 0.5f;

    // --- BOLE: near-vertical, zero-mean wander (§1.8: Curve=0, the wander is
    // sign-alternating and partially cancels). The lower third is rigid — the
    // measured boles hold their chord and bend only above (§1.7, frame 16).
    const int bole_segments = 7;
    const float bole_top_y = crown_base + crown_ry * 0.7f; // ends inside the crown
    glm::vec3 pos{0.0f, FLARE_HEIGHT, 0.0f};
    glm::vec3 dir{0.0f, 1.0f, 0.0f};
    const float seg_len = (bole_top_y - FLARE_HEIGHT) / static_cast<float>(bole_segments);

    // Root flare + spurs, one axis with the bole (the one-tree stand's first
    // finding: a flare that does not share the bole's axis is a visible knee).
    const float flare_r = p.trunk_radius * 1.6f;
    tube_segment(obj.wood, glm::vec3{0.0f, -FLARE_DEPTH, 0.0f}, pos, dir, flare_r,
                 p.trunk_radius, 6, bark);
    for (int k = 0; k < ROOT_SPUR_COUNT; ++k) {
        const float az = TAU * (static_cast<float>(k) + 0.5f + rng.sym() * 0.3f)
                       / static_cast<float>(ROOT_SPUR_COUNT);
        const glm::vec3 rd{std::cos(az), 0.0f, std::sin(az)};
        const float reach = flare_r * (1.6f + rng.unit() * 1.0f);
        const float r0 = std::max(p.trunk_radius * ROOT_SPUR_R_FRAC, 0.05f);
        const glm::vec3 start = rd * (flare_r * 0.6f) + glm::vec3{0.0f, 0.3f, 0.0f};
        const glm::vec3 crest = rd * (flare_r + reach * 0.35f)
                              + glm::vec3{0.0f, ROOT_SPUR_RISE, 0.0f};
        const glm::vec3 tip = rd * (flare_r + reach) - glm::vec3{0.0f, ROOT_SPUR_SINK, 0.0f};
        tube_segment(obj.ground, start, crest, safe_normalize(crest - start, rd), r0,
                     r0 * 0.6f, 4, bark);
        tube_segment(obj.ground, crest, tip, safe_normalize(tip - crest, rd), r0 * 0.6f,
                     0.0f, 4, bark);
    }

    struct Ring {
        glm::vec3 pos;
        glm::vec3 dir;
        float radius;
    };
    std::vector<Ring> bole;
    bole.push_back({pos, dir, p.trunk_radius});
    for (int s = 0; s < bole_segments; ++s) {
        const float t1 = static_cast<float>(s + 1) / static_cast<float>(bole_segments);
        if (s >= 2) { // the rigid lower third: wander only above it
            const float wob = 0.05f; // rad per segment, sign from the stream
            const glm::vec3 side{std::cos(rng.unit() * TAU), 0.0f,
                                 std::sin(rng.unit() * TAU)};
            dir = safe_normalize(dir + side * (rng.sym() * wob), dir);
        }
        const glm::vec3 next = pos + dir * seg_len;
        const float r0 = p.trunk_radius * std::pow(1.0f - (t1 - 1.0f / bole_segments) * 0.85f, 1.1f);
        const float r1 = p.trunk_radius * std::pow(1.0f - t1 * 0.85f, 1.1f);
        tube_segment(obj.wood, pos, next, dir, r0, std::max(r1, 0.05f), 6, bark);
        pos = next;
        bole.push_back({pos, dir, std::max(r1, 0.05f)});
    }

    // --- THE BRANCH HIERARCHY, DRAWN. The user's Skyrim references rule this
    // stage: the skeleton is VISIBLE THROUGH the crown — scaffolds and second-
    // order branches are real geometry, and the foliage hangs ON them as
    // ragged sprays with sky in between. Not a solid ball (v1's verdict:
    // «мультяшный стиль»), not confetti (the old generator's verdict).
    //
    // Every branch remembers its outer points; the sprays attach THERE, so
    // foliage placement is the skeleton's own statement (§3 of the research:
    // leaves that do not grow from branches are the original complaint).
    struct SprayAnchor {
        glm::vec3 pos;
        glm::vec3 dir; ///< outward direction of the branch at the anchor
    };
    std::vector<SprayAnchor> anchors;

    const float phase = rng.unit();
    for (int b = 0; b < p.scaffold_count; ++b) {
        const float az = GOLDEN_ANGLE * static_cast<float>(b) + rng.sym() * 0.5f;
        const float attach_span = bole_top_y - (crown_base * 0.85f);
        const float attach_y = crown_base * 0.85f
                             + attach_span * (0.15f + 0.8f * rng.unit());
        const Ring* at = &bole.front();
        for (const Ring& r : bole) {
            if (r.pos.y <= attach_y) at = &r;
        }
        const glm::vec3 out{std::cos(az), 0.0f, std::sin(az)};
        const glm::vec3 root = at->pos + out * (at->radius * 0.4f); // embedded
        const float k = std::clamp((attach_y - crown_base) / std::max(attach_span, 0.1f),
                                   0.0f, 1.0f);
        // Low scaffolds reach out flat, high ones climb — the vase profile the
        // references show; the very top continues the leader.
        const float elev = 0.25f + 0.85f * k + rng.sym() * 0.12f;
        glm::vec3 bd = safe_normalize(
            out * std::cos(elev) + glm::vec3{0.0f, std::sin(elev), 0.0f}, out);
        const float branch_r = at->radius * (0.5f + rng.unit() * 0.12f);
        const float reach = p.crown_radius * (0.85f + rng.unit() * 0.35f);
        glm::vec3 bp = root;
        glm::vec3 cur = bd;
        float br = branch_r * 1.35f; // §1.5's inflate at the joint
        const int segs = 3;
        for (int sgi = 0; sgi < segs; ++sgi) {
            const float tt = static_cast<float>(sgi + 1) / segs;
            // Weber's sign-alternating wander plus a mild upward pull.
            const glm::vec3 side = safe_normalize(
                glm::cross(cur, glm::vec3{0.0f, 1.0f, 0.0f}), out);
            cur = safe_normalize(cur + glm::vec3{0.0f, 0.16f, 0.0f}
                                     + side * (rng.sym() * 0.25f), cur);
            const glm::vec3 np = bp + cur * (reach / segs);
            const float nr = branch_r * (1.0f - 0.68f * tt);
            tube_segment(obj.wood, bp, np, cur, br, std::max(nr, 0.035f), 5, bark);

            // SECOND-ORDER BRANCHES leave the outer two segments alternately
            // left/right — the herringbone every reference crown shows.
            if (sgi >= 1) {
                for (int c = 0; c < p.secondary_per_scaffold; ++c) {
                    if (rng.unit() < 0.12f) continue; // uneven, never a comb
                    const float lr = ((c + sgi) % 2 == 0) ? 1.0f : -1.0f;
                    const glm::vec3 sdir = safe_normalize(
                        cur + side * (lr * (0.7f + rng.unit() * 0.5f))
                            + glm::vec3{0.0f, 0.25f + rng.unit() * 0.3f, 0.0f},
                        side);
                    const float slen = reach * (0.3f + rng.unit() * 0.25f);
                    const glm::vec3 sp0 = bp + cur * (reach / segs) * (0.4f + 0.4f * rng.unit());
                    const glm::vec3 sp1 = sp0 + sdir * slen;
                    tube_segment(obj.wood, sp0, sp1, sdir, std::max(nr, 0.035f) * 0.6f,
                                 0.02f, 4, bark);
                    anchors.push_back({sp1, sdir});
                    if (rng.unit() < 0.9f) { // a mid-branch tuft anchor too
                        anchors.push_back({sp0 + sdir * (slen * 0.55f), sdir});
                    }
                }
            }
            bp = np;
            br = std::max(nr, 0.035f);
        }
        anchors.push_back({bp, cur}); // the scaffold tip itself
    }
    // The leader's top carries a crown of its own.
    anchors.push_back({bole.back().pos + glm::vec3{0.0f, 0.4f, 0.0f},
                       glm::vec3{0.0f, 1.0f, 0.0f}});

    // --- INNER SHADOW CORE: small and DARK — the depth glimpsed between the
    // sprays, not the crown itself. What the references show through the gaps
    // is shadow, and without this the gaps show SKY straight through the
    // middle of the tree, which reads hollow.
    if (p.core_frac > 0.0f) {
        MeshData core;
        blob_cluster(core, glm::vec3{0.0f},
                     {p.crown_radius * p.core_frac, crown_ry * p.core_frac * 0.9f,
                      p.crown_radius * p.core_frac},
                     6, 4, 0xFFFFFFFFu);
        const glm::vec3 shade = tone * 0.5f; // deep-shadow leaf value
        for (platform::Vertex& v : core.vertices) {
            v.color_rgba = pack(shade);
        }
        append_transformed(obj.wood, core, crown_c, 0.0f, 1.0f);
    }

    // --- LEAF SPRAYS ON THE ANCHORS. Each card is a ragged leafy branch tuft
    // (the atlas' spray masks), sized in crown fractions, its normal blended
    // between the branch's own outward direction and the radial from the crown
    // centre — enough dome for one light (the «наждачка» cure held from v1),
    // enough per-branch identity that sprays read as HANGING on their branch.
    for (const SprayAnchor& a : anchors) {
        const int sprays = p.spray_per_branch;
        for (int i = 0; i < sprays; ++i) {
            const glm::vec3 jitter{rng.sym() * 0.5f, rng.sym() * 0.35f,
                                   rng.sym() * 0.5f};
            const glm::vec3 c = a.pos + jitter * (p.crown_radius * 0.12f);
            const glm::vec3 radial = safe_normalize(c - crown_c, a.dir);
            const glm::vec3 n = safe_normalize(radial * 0.55f + a.dir * 0.45f, radial);
            LeafCardParams card;
            card.center = c;
            card.normal = n;
            card.half_width = p.crown_radius * p.spray_frac * (0.85f + rng.unit() * 0.45f);
            card.half_height = card.half_width * (0.65f + rng.unit() * 0.25f);
            card.roll = rng.sym() * 0.9f; // near-upright sprays, never fully spun
            card.shape = p.card_shape;
            card.tone = p.tone;
            card.value_jitter = 0.42f + rng.unit() * 0.22f; // narrow: v1's cure
            card.phase = phase;
            card.sway_origin = glm::vec3{0.0f, crown_base, 0.0f};
            card.sway_span = p.crown_radius * 1.8f;
            emit_leaf_card(obj.cards, card);
            // THE CROSSED PARTNER (SpeedTree's standard pair): the same tuft
            // seen edge-on stops vanishing — a lone plane disappears at its
            // own profile, and a sparse crown of lone planes flickers as the
            // camera orbits. Slightly smaller, rotated off the first.
            LeafCardParams cross = card;
            cross.normal = safe_normalize(glm::cross(n, glm::vec3{0.0f, 1.0f, 0.0f})
                                              + glm::vec3{0.0f, 0.3f * rng.sym(), 0.0f},
                                          a.dir);
            cross.half_width *= 0.85f;
            cross.half_height *= 0.85f;
            cross.roll = rng.sym() * 0.9f;
            cross.value_jitter = 0.42f + rng.unit() * 0.22f;
            emit_leaf_card(obj.cards, cross);
        }
    }

    obj.content_hash = object_content_hash(obj);
    return obj;
}

} // namespace dfn::render
