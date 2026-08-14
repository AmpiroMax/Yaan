/*
Created: 14:08:2026 - 23:36:19
Last updated: 14:08:2026 - 23:36:19
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

    // --- SCAFFOLDS: order-1 branches, each STARTING ON THE DRAWN BOLE with a
    // thickened, embedded base (§1.5: the joint is a swelling, not a butt).
    // They curve upward and END INSIDE the crown masses — nothing pokes out
    // bare, which retires «витые палки» structurally.
    for (int b = 0; b < p.scaffold_count; ++b) {
        // Attachment: spiral azimuths (golden step + jitter), heights spread
        // over the crown's lower half — where real scaffolds live.
        const float az = GOLDEN_ANGLE * static_cast<float>(b) + rng.sym() * 0.5f;
        const float hy = crown_base + crown_ry * (0.05f + 0.75f * rng.unit()) - crown_ry;
        const float attach_y = std::clamp(hy + crown_ry * 0.5f, FLARE_HEIGHT + 1.0f,
                                          bole_top_y - 0.5f);
        // Find the bole ring at that height and root the branch INSIDE it.
        const Ring* at = &bole.front();
        for (const Ring& r : bole) {
            if (r.pos.y <= attach_y) at = &r;
        }
        const glm::vec3 out{std::cos(az), 0.0f, std::sin(az)};
        const glm::vec3 root = at->pos + out * (at->radius * 0.4f); // embedded
        // Elevation: steep low scaffolds, flatter high ones — the classic vase.
        const float k = (attach_y - crown_base + crown_ry) / (2.0f * crown_ry);
        const float elev = 0.9f - 0.55f * k + rng.sym() * 0.1f;
        glm::vec3 bd = safe_normalize(
            out * std::cos(elev) + glm::vec3{0.0f, std::sin(elev), 0.0f}, out);
        const float branch_r = at->radius * (0.55f + rng.unit() * 0.15f);
        const float reach = p.crown_radius * (0.75f + rng.unit() * 0.35f);
        glm::vec3 bp = root;
        float br = branch_r * 1.35f; // §1.5's inflate: the base is a swelling
        const int segs = 3;
        for (int s = 0; s < segs; ++s) {
            const float tt = static_cast<float>(s + 1) / segs;
            // Curve upward as it goes (AttractionUp of the model, spent here
            // as a per-segment pull instead of a parameter).
            bd = safe_normalize(bd + glm::vec3{0.0f, 0.28f * tt, 0.0f}, bd);
            const glm::vec3 np = bp + bd * (reach / segs);
            const float nr = branch_r * (1.0f - 0.75f * tt);
            tube_segment(obj.wood, bp, np, bd, br, std::max(nr, 0.04f), 5, bark);
            bp = np;
            br = std::max(nr, 0.04f);
        }
    }

    // --- CROWN MASSES: the core plus satellites — camp (а) of §1.7, where the
    // measured models spend 87-91 % of their triangles on SOLID leaf masses.
    // Faceted on purpose (the reference look), top-lit by baked colour.
    const auto mass = [&](glm::vec3 c, glm::vec3 radii, int slices, int bands) {
        MeshData blob;
        blob_cluster(blob, glm::vec3{0.0f}, radii, slices, bands, 0xFFFFFFFFu);
        // Re-colour per vertex by the DOME rule before merging: normal.y of a
        // faceted blob is its facet's tilt, and lighting-by-colour survives the
        // palette where lighting-by-normal alone dies at this pixel scale.
        const float jit = 0.92f + rng.unit() * 0.16f; // narrow: §3's verdict on
                                                      // inter-card noise
        for (platform::Vertex& v : blob.vertices) {
            const float ny01 = std::clamp(v.normal.y * 0.5f + 0.5f, 0.0f, 1.0f);
            const float local = 0.9f + 0.2f * std::clamp(v.position.y / (radii.y + 1e-3f)
                                                             * 0.5f + 0.5f,
                                                         0.0f, 1.0f);
            v.color_rgba = dome_color(tone, ny01 * local * 0.9f, jit);
        }
        append_transformed(obj.wood, blob, c, 0.0f, 1.0f);
    };
    mass(crown_c, {p.crown_radius * 0.62f, crown_ry * 0.72f, p.crown_radius * 0.62f}, 7, 5);
    for (int i = 0; i < p.mass_count; ++i) {
        const glm::vec3 d = fib_dir(i, p.mass_count);
        const glm::vec3 c = crown_c
                          + glm::vec3{d.x * p.crown_radius * 0.55f, d.y * crown_ry * 0.55f,
                                      d.z * p.crown_radius * 0.55f};
        const float s = 0.34f + rng.unit() * 0.16f; // 0.34-0.50 of the crown —
                                                    // Marc Solà's clump scale
        mass(c, {p.crown_radius * s, crown_ry * s * (0.8f + rng.unit() * 0.3f),
                 p.crown_radius * s}, 6, 4);
    }

    // --- BIG RIM CARDS: SpeedTree clusters at our budget mean each card is a
    // BRANCH-WITH-LEAVES, roughly half the crown across (§1.1, §1.6, §2's
    // element-size row). Normals point FROM the crown centre — the Airborn
    // projection — so every card shades as part of one dome.
    const float phase = rng.unit();
    for (int i = 0; i < p.card_count; ++i) {
        const glm::vec3 d = fib_dir(i, p.card_count);
        const glm::vec3 dir3 = safe_normalize(
            glm::vec3{d.x, d.y * 0.8f, d.z}, glm::vec3{0.0f, 1.0f, 0.0f});
        LeafCardParams card;
        card.center = crown_c + glm::vec3{dir3.x * p.crown_radius * 0.78f,
                                          dir3.y * crown_ry * 0.78f,
                                          dir3.z * p.crown_radius * 0.78f};
        card.normal = dir3; // FROM the centre: the projected-normal dome
        card.half_width = p.crown_radius * (0.5f + rng.unit() * 0.12f);
        card.half_height = p.crown_radius * (0.4f + rng.unit() * 0.1f);
        card.roll = rng.unit() * TAU;
        card.shape = p.card_shape;
        card.tone = p.tone;
        // NARROW value band (§3: inter-card noise at card frequency IS the
        // «наждачка»; variation belongs INSIDE the texture, not between cards).
        card.value_jitter = 0.45f + rng.unit() * 0.2f;
        card.phase = phase;
        card.sway_origin = glm::vec3{0.0f, crown_base, 0.0f};
        card.sway_span = p.crown_radius * 1.8f;
        emit_leaf_card(obj.cards, card);
    }

    obj.content_hash = object_content_hash(obj);
    return obj;
}

} // namespace dfn::render
