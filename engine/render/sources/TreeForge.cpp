/*
Created: 14:08:2026 - 23:36:19
Last updated: 15:08:2026 - 01:04:30
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
- 15:08:2026 - 00:45:20: v3 — РЕКУРСИВНЫЙ гровер по пунктам пользователя: «ветки должны идти от
  ствола И от других веток» — дети покидают болу и родительские ветви с середины
  сегментов; «уменьшаться пропорционально» — трубная модель, ребёнок несёт ~половину
  радиуса родителя; «расти небольшими сегментами, каждый чуть в сторону, а может
  вниз» — каждый сегмент поворачивает: боковое блуждание, слабеющая с уровнем тяга
  вверх, каждый четвёртый провисает. Шар-ядро удалён. Хвойный путь: бола во всю
  высоту, мутовки по конусу, лапы ВДОЛЬ ветви (ель — не палка с помпоном), нормали
  лап следуют ветви сильнее купола.
- 15:08:2026 - 01:04:30: КОРА И МОХ пост-проходом по вершинам wood/ground (референсы-крупняки:
  вертикальные борозды чередованием гребень/борозда с блужданием по высоте, мох
  зелёным налётом до ~4 м и на корнях, пятнами по азимуту). ПОРЯДОК ЛИСТВЫ по
  §4.3 исследования: лапы ложатся почти горизонтальными ярусами — нормаль
  кланяется ВВЕРХ (лиственные 0.45 up, хвоя 0.8 up), roll ±0.25 вместо ±0.9
  («у нас снова беспорядок» снят направлением, не плотностью).
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
    // A broadleaf bole ends inside its crown; a CONIFER's runs the whole
    // height — the leader IS the tree's spine, and every whorl hangs off it.
    const float bole_top_y = p.conifer ? p.height * 0.97f
                                       : crown_base + crown_ry * 0.7f;
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

    // --- THE BRANCH HIERARCHY, GROWN. The user's v2 verdict, point by point:
    // «ветки прямые, и их отсилы штуки 4-5» — branches now RECURSE (children
    // leave the bole AND other branches), each level thinner in proportion
    // (the pipe model: a child carries a fraction of its parent's section);
    // «должны расти небольшими сегментами, каждый чуть в сторону, а может и
    // вниз» — every branch walks in short segments, and every segment turns:
    // sideways wander, an upward pull that weakens with level, and an
    // occasional sag — which is how the referenced crowns actually move.
    struct SprayAnchor {
        glm::vec3 pos;
        glm::vec3 dir;
    };
    std::vector<SprayAnchor> anchors;
    const float phase = rng.unit();

    if (!p.conifer) {
        // Recursive broadleaf grower. Levels: 0 scaffold, 1 branch, 2 twig.
        struct Grow {
            RegistryObject& obj;
            Rng& rng;
            std::vector<SprayAnchor>& anchors;
            uint32_t bark;
            glm::vec3 crown_c;
            float crown_rx, crown_ry;

            void run(glm::vec3 pos, glm::vec3 dir, float len, float radius, int level) {
                const int segs = level == 0 ? 5 : (level == 1 ? 4 : 3);
                const float seg = len / static_cast<float>(segs);
                glm::vec3 d = dir;
                float r = radius;
                for (int si = 0; si < segs; ++si) {
                    // THE TURN, per segment: side wander always; up pull that
                    // fades with level (twigs stop caring about the sky); and
                    // one segment in ~4 dips DOWN — the sag real limbs show.
                    const glm::vec3 side = safe_normalize(
                        glm::cross(d, glm::vec3{0.0f, 1.0f, 0.0f}),
                        glm::vec3{1.0f, 0.0f, 0.0f});
                    const float up_pull = 0.22f / static_cast<float>(1 + level);
                    const float dip = rng.unit() < 0.25f ? -0.18f : 0.0f;
                    d = safe_normalize(d + side * (rng.sym() * 0.30f)
                                         + glm::vec3{0.0f, up_pull + dip, 0.0f}, d);
                    const glm::vec3 np = pos + d * seg;
                    const float taper = 1.0f - 0.6f * (static_cast<float>(si + 1)
                                                       / static_cast<float>(segs));
                    const float nr = std::max(radius * taper, 0.025f);
                    const int sides = level == 0 ? 5 : (level == 1 ? 4 : 3);
                    tube_segment(obj.wood, pos, np, d, r, nr, sides, bark);

                    // CHILDREN leave mid-branch, alternating sides — from the
                    // second segment on, so the joint zone stays clean.
                    if (level < 2 && si >= 1) {
                        const int kids = level == 0 ? 2 : 1;
                        for (int c = 0; c < kids; ++c) {
                            if (rng.unit() < 0.3f) continue;
                            const float lr = ((si + c) % 2 == 0) ? 1.0f : -1.0f;
                            const glm::vec3 cd = safe_normalize(
                                d + side * (lr * (0.8f + rng.unit() * 0.6f))
                                  + glm::vec3{0.0f, rng.sym() * 0.35f, 0.0f}, side);
                            // The pipe model: the child takes ~half the parent's
                            // radius, and its length follows its section.
                            run(np, cd, len * (0.5f + rng.unit() * 0.16f),
                                nr * (0.52f + rng.unit() * 0.1f), level + 1);
                        }
                    }
                    pos = np;
                    r = nr;
                }
                if (level >= 1) {
                    anchors.push_back({pos, d}); // tips of branches and twigs
                }
                if (level == 2 && rng.unit() < 0.7f) {
                    anchors.push_back({pos - d * (len * 0.4f), d});
                }
            }
        } grow{obj, rng, anchors, bark, crown_c, p.crown_radius, crown_ry};

        for (int b = 0; b < p.scaffold_count; ++b) {
            const float az = GOLDEN_ANGLE * static_cast<float>(b) + rng.sym() * 0.5f;
            const float attach_span = bole_top_y - crown_base * 0.85f;
            const float attach_y = crown_base * 0.85f
                                 + attach_span * (0.1f + 0.8f * rng.unit());
            const Ring* at = &bole.front();
            for (const Ring& ring : bole) {
                if (ring.pos.y <= attach_y) at = &ring;
            }
            const glm::vec3 out{std::cos(az), 0.0f, std::sin(az)};
            const float k = std::clamp((attach_y - crown_base)
                                           / std::max(attach_span, 0.1f), 0.0f, 1.0f);
            const float elev = 0.3f + 0.75f * k + rng.sym() * 0.12f;
            const glm::vec3 bd = safe_normalize(
                out * std::cos(elev) + glm::vec3{0.0f, std::sin(elev), 0.0f}, out);
            // Rooted INSIDE the bole ring, base thickened (§1.5's inflate).
            grow.run(at->pos + out * (at->radius * 0.4f), bd,
                     p.crown_radius * (0.9f + rng.unit() * 0.35f),
                     at->radius * (0.5f + rng.unit() * 0.12f), 0);
        }
        anchors.push_back({bole.back().pos + glm::vec3{0.0f, 0.4f, 0.0f},
                           glm::vec3{0.0f, 1.0f, 0.0f}});
    } else {
        // --- THE CONIFER (ёлка): whorls of near-horizontal branches down a
        // cone, drooping as they reach, needle sprays along the outer half.
        // The bole for a conifer runs the WHOLE height (built above), and the
        // crown starts low — crown_base_frac is the skirt, not a canopy base.
        for (int w = 0; w < p.whorl_count; ++w) {
            const float t = (static_cast<float>(w) + 0.5f)
                          / static_cast<float>(p.whorl_count);
            const float y = crown_base + (p.height - crown_base) * t;
            // Branch reach follows the cone: long at the skirt, short at the top.
            const float reach = p.crown_radius * (1.0f - 0.85f * t)
                              * (0.9f + rng.unit() * 0.2f);
            if (reach < 0.4f) continue;
            const Ring* at = &bole.front();
            for (const Ring& ring : bole) {
                if (ring.pos.y <= y) at = &ring;
            }
            const int count = p.whorl_branches;
            const float az0 = rng.unit() * TAU;
            for (int b = 0; b < count; ++b) {
                if (rng.unit() < 0.15f) continue; // ragged whorls, not a fan
                const float az = az0 + TAU * static_cast<float>(b)
                                     / static_cast<float>(count)
                               + rng.sym() * 0.3f;
                const glm::vec3 out{std::cos(az), 0.0f, std::sin(az)};
                glm::vec3 bp = at->pos + out * (at->radius * 0.5f);
                glm::vec3 d = safe_normalize(
                    out + glm::vec3{0.0f, 0.15f - p.droop * 0.5f, 0.0f}, out);
                float r = std::max(at->radius * 0.32f, 0.03f);
                const int segs = 3;
                for (int si = 0; si < segs; ++si) {
                    // Droop grows toward the tip — the spruce sag.
                    d = safe_normalize(d + glm::vec3{0.0f, -p.droop / segs, 0.0f}
                                         + out * 0.2f, d);
                    const glm::vec3 np = bp + d * (reach / segs);
                    const float nr = std::max(r * 0.55f, 0.02f);
                    tube_segment(obj.wood, bp, np, d, r, nr, 3, bark);
                    // Needle sprays sit ALONG the branch's outer segments, not
                    // only at the tip — a spruce limb is a frond, not a stick
                    // with a pom-pom.
                    if (si >= 1) {
                        anchors.push_back({(bp + np) * 0.5f, d});
                    }
                    bp = np;
                    r = nr;
                }
                anchors.push_back({bp, d});
            }
        }
        // The leader spike above the last whorl, and its own small crown.
        anchors.push_back({bole.back().pos + glm::vec3{0.0f, 0.3f, 0.0f},
                           glm::vec3{0.0f, 1.0f, 0.0f}});
    }

    // --- LEAF SPRAYS ON THE ANCHORS: crossed pairs, dome-blended normals —
    // held from v2 (its cure for card flicker and for the «наждачка»).
    for (const SprayAnchor& a : anchors) {
        const int sprays = p.spray_per_branch;
        for (int i = 0; i < sprays; ++i) {
            const glm::vec3 jitter{rng.sym() * 0.5f, rng.sym() * 0.35f,
                                   rng.sym() * 0.5f};
            const glm::vec3 c = a.pos + jitter * (p.crown_radius * 0.1f);
            const glm::vec3 radial = safe_normalize(c - crown_c, a.dir);
            // THE GROWTH DIRECTION (research §4.3, the user's aspen and pine
            // frames): foliage lies in near-HORIZONTAL layers along its branch
            // with an upward pull — broadleaf clumps face up-and-out, conifer
            // fronds lie almost flat and sag at the tip. The chaos the user
            // called out was the roll and the dome-heavy normals; both bow to
            // UP now.
            const glm::vec3 up{0.0f, 1.0f, 0.0f};
            const glm::vec3 n = p.conifer
                ? safe_normalize(up * 0.8f + a.dir * 0.15f + radial * 0.05f, up)
                : safe_normalize(up * 0.45f + radial * 0.3f + a.dir * 0.25f, up);
            LeafCardParams card;
            card.center = c;
            card.normal = n;
            card.half_width = p.crown_radius * p.spray_frac * (0.85f + rng.unit() * 0.45f);
            card.half_height = card.half_width * (p.conifer ? 0.55f : 0.7f);
            card.roll = rng.sym() * 0.25f; // layers, not chaos (§4.3)
            card.shape = p.card_shape;
            card.tone = p.tone;
            card.value_jitter = 0.42f + rng.unit() * 0.22f;
            card.phase = phase;
            card.sway_origin = glm::vec3{0.0f, crown_base, 0.0f};
            card.sway_span = p.crown_radius * 1.8f;
            emit_leaf_card(obj.cards, card);
            LeafCardParams cross = card;
            cross.normal = safe_normalize(glm::cross(n, glm::vec3{0.0f, 1.0f, 0.0f})
                                              + glm::vec3{0.0f, 0.3f * rng.sym(), 0.0f},
                                          a.dir);
            cross.half_width *= 0.85f;
            cross.half_height *= 0.85f;
            cross.roll = rng.sym() * 0.25f;
            cross.value_jitter = 0.42f + rng.unit() * 0.22f;
            emit_leaf_card(obj.cards, cross);
        }
    }

    // --- BARK AND MOSS, AS A POST-PASS OVER THE WOOD (user, with the Skyrim
    // close-ups: «поработать над текстурой стволов... мох, корни у ствола
    // снизу»). The references show two things at trunk scale: deep VERTICAL
    // furrows (alternating lit ridge / dark groove) and a green moss film
    // creeping up the butt and over the root spurs. Both are value work, and
    // flat-shaded faces own their vertices — so the texture is painted into
    // vertex colour, which survives our palette where a normal map would not.
    const auto bark_pass = [&](MeshData& mesh) {
        for (platform::Vertex& v : mesh.vertices) {
            const uint32_t col = v.color_rgba;
            // Only wood-coloured vertices: crown masses (leaf tones) pass through.
            if (col != bark) continue;
            const float az = std::atan2(v.position.z, v.position.x);
            // FURROWS: value stripes around the circumference, wandering
            // slightly with height so the grooves read as grain, not as paint.
            const float stripe = std::sin(az * 9.0f + v.position.y * 0.35f)
                               + 0.5f * std::sin(az * 23.0f - v.position.y * 0.2f);
            const float furrow = 0.86f + 0.17f * std::clamp(stripe, -1.0f, 1.0f);
            // MOSS: strongest at the ground, gone by ~4 m, patchy by azimuth.
            const float moss_h = std::clamp(1.0f - v.position.y / 4.0f, 0.0f, 1.0f);
            const float patch = 0.5f + 0.5f * std::sin(az * 3.0f + 1.7f);
            const float moss = moss_h * patch * 0.55f;
            glm::vec3 c = p.bark * furrow;
            c = c * (1.0f - moss) + glm::vec3{0.22f, 0.34f, 0.13f} * moss;
            v.color_rgba = pack(glm::clamp(c, glm::vec3{0.0f}, glm::vec3{1.0f}));
        }
    };
    bark_pass(obj.wood);
    bark_pass(obj.ground); // the root spurs wear the same moss

    obj.content_hash = object_content_hash(obj);
    return obj;
}

} // namespace dfn::render
