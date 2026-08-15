/*
Created: 14:08:2026 - 23:36:19
Last updated: 15:08:2026 - 15:54:46
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
- 15:08:2026 - 02:14:30: СТВОЛЫ И СУЧЬЯ В ТЕКСТУРЕ (вердикт: «текстур на деревьях нет... мох —
  просто зеленушка»): bark_tube кладёт UV зеркальным повтором внутри тайла коры,
  комель и корни носят МШИСТЫЙ колорвей, бола и ветви — чистый; ветер занулён,
  просвечивание глушится весом качания в шейдере (дерево не светится насквозь).
  Дубовый габитус (низкие ветви горизонтальны всю длину), центр кроны заполнен
  внутренними якорями, изнанка кроны снизу смотрит вниз, roll свободный (параллельные
  линии убиты вращением В плоскости), лапы прибиты к веткам (джиттер 0.035),
  корни — дуга четырьмя хордами (колено невидимо в плоском тонировании).
- 15:08:2026 - 15:54:46: v6: хвоя пересобрана на ФРОНДЫ-ЛЕНТЫ (провисающая лента 4 сегментов, кончик вверх, голое дерево только внутренняя четверть); ЮБКА мёртвых сучьев 0.10-0.40h (оба фотоскана); корни — КОНТРФОРСЫ (старт внутри наплыва, выше и толще, медленный сбег); ветер честный: bark_tube несёт вес на обоих кольцах (иначе стык рвётся), вес = дистанция от опоры (лид 09f75eb: транзмит теперь по колонке UV).
*/

#include "engine/render/sources/TreeForge.h"

#include "engine/render/sources/FloraBuild.h"
#include "engine/render/sources/ProcMesh.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace dfn::render {
namespace {

/// Vertex colour for textured wood on the foliage program: r = sway weight
/// (honest distance-from-support — the lead's 09f75eb shader derives all
/// three wind bands from this one weight), g = per-tree phase, b (value
/// jitter) = 0.5 neutral, a (sky vis) = 0.55.
[[nodiscard]] uint32_t pack_wind(float sway, float phase) {
    const auto to_byte = [](float v) {
        return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    // 0xAABBGGRR: a=0x8C (sky vis 0.55), b=0x80 (jitter 0.5).
    return 0x8C800000u | (to_byte(phase) << 8) | to_byte(sway);
}

/// One tapered tube segment with BARK UVs. Same geometry as tube_segment, but
/// each face maps into the given atlas tile rect: u runs around the
/// circumference, v along the segment's own length, both through a TRIANGLE
/// WAVE so the mapping mirror-repeats inside the tile and never crosses its
/// border into a neighbouring leaf tile (the atlas cannot wrap). The tile is
/// drawn mirror-symmetric, so the fold line is invisible by construction.
/// wind_c0 colours the p0 ring, wind_c1 the p1 ring (and the tip vertex):
/// the sway weight must be CONTINUOUS along a limb, or adjacent segments
/// translate by different amounts under wind and the joint cracks open.
void bark_tube(MeshData& m, glm::vec3 p0, glm::vec3 p1, glm::vec3 axis, float r0,
               float r1, int sides, glm::vec4 uv_rect, float v0_m, float circum_m,
               uint32_t wind_c0, uint32_t wind_c1) {
    const glm::vec3 u_axis = perp_of(axis);
    const glm::vec3 v_axis = glm::cross(axis, u_axis);
    const float len = glm::length(p1 - p0);
    const auto tri_wave = [](float t) { return 1.0f - std::fabs(1.0f - 2.0f * (t - std::floor(t))); };
    // Metres of trunk surface one full tile covers. ~2.6 m keeps the furrow
    // pitch believable on a 0.4 m oak and a 10 m colossus alike.
    constexpr float TILE_SPAN_M = 2.6f;
    const float du = uv_rect.z - uv_rect.x;
    const float dv = uv_rect.w - uv_rect.y;
    for (int i = 0; i < sides; ++i) {
        const float a0 = TAU * static_cast<float>(i) / static_cast<float>(sides);
        const float a1 = TAU * static_cast<float>(i + 1) / static_cast<float>(sides);
        const glm::vec3 d0 = u_axis * std::cos(a0) + v_axis * std::sin(a0);
        const glm::vec3 d1 = u_axis * std::cos(a1) + v_axis * std::sin(a1);
        const float dr = r0 - r1;
        const float slope = (len > 1e-5f) ? (dr / len) : 0.0f;
        const glm::vec3 n0 = safe_normalize(d0 + axis * slope, d0);
        const glm::vec3 n1 = safe_normalize(d1 + axis * slope, d1);
        // Texture coordinates: circumference and height in METRES, folded.
        const float cu0 = uv_rect.x + du * tri_wave(circum_m * (static_cast<float>(i)
                              / static_cast<float>(sides)) / TILE_SPAN_M);
        const float cu1 = uv_rect.x + du * tri_wave(circum_m * (static_cast<float>(i + 1)
                              / static_cast<float>(sides)) / TILE_SPAN_M);
        const float cv0 = uv_rect.y + dv * tri_wave(v0_m / TILE_SPAN_M);
        const float cv1 = uv_rect.y + dv * tri_wave((v0_m + len) / TILE_SPAN_M);
        const auto base = static_cast<uint32_t>(m.vertices.size());
        if (r1 <= 1e-4f) {
            const glm::vec3 nt = safe_normalize(n0 + n1, axis);
            m.vertices.push_back({p0 + d0 * r0, n0, {cu0, cv0}, wind_c0});
            m.vertices.push_back({p1, nt, {(cu0 + cu1) * 0.5f, cv1}, wind_c1});
            m.vertices.push_back({p0 + d1 * r0, n1, {cu1, cv0}, wind_c0});
            m.indices.insert(m.indices.end(), {base, base + 1, base + 2});
        } else {
            m.vertices.push_back({p0 + d0 * r0, n0, {cu0, cv0}, wind_c0});
            m.vertices.push_back({p1 + d0 * r1, n0, {cu0, cv1}, wind_c1});
            m.vertices.push_back({p1 + d1 * r1, n1, {cu1, cv1}, wind_c1});
            m.vertices.push_back({p0 + d1 * r0, n1, {cu1, cv0}, wind_c0});
            m.indices.insert(m.indices.end(),
                             {base, base + 1, base + 2, base, base + 2, base + 3});
        }
    }
}

} // namespace

RegistryObject forge_tree(const TreeForgeParams& p) {
    RegistryObject obj;
    obj.name = p.name;
    obj.kind = "tree";
    obj.source = "forge:v1 seed=" + std::to_string(p.seed);

    Rng rng(p.seed * 0x9E3779B97F4A7C15ull + 0x243F6A8885A308D3ull);
    const uint32_t bark = pack(p.bark);

    // BARK TILES (the textured-trunk wave): the clean colourway for the upper
    // bole and limbs, the MOSSY one for the flare and roots — moss lives where
    // the ground's damp does, which is the reference's own vertical story.
    // Wind: since the lead's 09f75eb the transmit gate is the atlas COLUMN
    // (u >= 0.8 is wood), so wood may carry an honest sway weight without
    // glowing — r is only sway now, all three wind bands derive from it.
    const LeafTone bark_row = p.bark.r > 0.6f ? LeafTone::BirchLight
                             : (p.conifer ? LeafTone::ConiferDark : LeafTone::OakMid);
    const LeafTone bark_moss_row = p.bark.r > 0.6f ? LeafTone::BirchPale
                                                    : LeafTone::OakSunlit;
    const glm::vec4 bark_uv = leaf_tile_uv(LeafShape::BarkPlate, bark_row);
    const glm::vec4 bark_moss_uv = leaf_tile_uv(LeafShape::BarkPlate, bark_moss_row);
    const float phase = rng.unit(); // one wind phase per tree
    const uint32_t wind_still = pack_wind(0.0f, phase);
    // Honest sway weight of a point on the wood: how far it stands from the
    // ground anchor, normalised by the tree's own height.
    const auto wood_sway = [&p](float y) {
        return 0.28f * std::pow(std::clamp(y / std::max(p.height, 1.0f), 0.0f, 1.0f),
                                1.6f);
    };

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
    bark_tube(obj.bark, glm::vec3{0.0f, -FLARE_DEPTH, 0.0f}, pos, dir, flare_r,
              p.trunk_radius, 7, bark_moss_uv, 0.0f, TAU * flare_r, wind_still,
              wind_still);
    for (int k = 0; k < ROOT_SPUR_COUNT; ++k) {
        const float az = TAU * (static_cast<float>(k) + 0.5f + rng.sym() * 0.3f)
                       / static_cast<float>(ROOT_SPUR_COUNT);
        const glm::vec3 rd{std::cos(az), 0.0f, std::sin(az)};
        const float reach = flare_r * (1.6f + rng.unit() * 1.0f);
        // BUTTRESS profile (passports §2, frame copy 8: the root is a RIDGE
        // of the flare that runs out along the ground, not a pipe leaning on
        // it). The spur starts INSIDE the flare, high and thick, and its
        // first chord is steep — the silhouette reads as one continuous
        // surface from bole to ground.
        const float r0 = std::max(p.trunk_radius * ROOT_SPUR_R_FRAC * 1.35f, 0.06f);
        const glm::vec3 pts[5] = {
            rd * (flare_r * 0.15f) + glm::vec3{0.0f, 0.62f, 0.0f},
            rd * (flare_r * 0.85f) + glm::vec3{0.0f, 0.26f, 0.0f},
            rd * (flare_r + reach * 0.35f) + glm::vec3{0.0f, ROOT_SPUR_RISE, 0.0f},
            rd * (flare_r + reach * 0.7f) + glm::vec3{0.0f, -0.05f, 0.0f},
            rd * (flare_r + reach) - glm::vec3{0.0f, ROOT_SPUR_SINK, 0.0f}};
        float rr = r0;
        for (int seg = 0; seg < 4; ++seg) {
            // Slow taper: a buttress keeps its section while it runs.
            const float nr = seg == 3 ? 0.0f
                                      : r0 * (1.0f - 0.22f * static_cast<float>(seg + 1));
            bark_tube(obj.ground, pts[seg], pts[seg + 1],
                      safe_normalize(pts[seg + 1] - pts[seg], rd), rr,
                      std::max(nr, 0.0f), 4, bark_moss_uv,
                      reach * 0.25f * static_cast<float>(seg), TAU * rr, wind_still,
                      wind_still);
            rr = std::max(nr, 0.02f);
        }
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
        bark_tube(obj.bark, pos, next, dir, r0, std::max(r1, 0.05f), 7, bark_uv,
                  pos.y, TAU * r0, pack_wind(wood_sway(pos.y), phase),
                  pack_wind(wood_sway(next.y), phase));
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

    if (!p.conifer) {
        // Recursive broadleaf grower. Levels: 0 scaffold, 1 branch, 2 twig.
        struct Grow {
            RegistryObject& obj;
            Rng& rng;
            std::vector<SprayAnchor>& anchors;
            uint32_t bark;
            glm::vec3 crown_c;
            float crown_rx, crown_ry;
            glm::vec4 bark_uv;
            float phase;
            float height;

            // A limb's sway weight: height plus horizontal reach from the
            // bole, both against the tree's own scale — the tip of a long
            // low oak limb sways as much as the crown top does.
            [[nodiscard]] uint32_t wind_at(glm::vec3 q) const {
                const float horiz = std::sqrt(q.x * q.x + q.z * q.z)
                                  / std::max(crown_rx, 0.5f);
                const float sway = std::clamp(
                    0.28f * std::pow(std::clamp(q.y / std::max(height, 1.0f),
                                                0.0f, 1.0f), 1.6f)
                        + 0.30f * horiz,
                    0.0f, 0.60f);
                const auto to_byte = [](float v) {
                    return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f)
                                                 * 255.0f + 0.5f);
                };
                return 0x8C800000u | (to_byte(phase) << 8) | to_byte(sway);
            }

            void run(glm::vec3 pos, glm::vec3 dir, float len, float radius, int level) {
                const int segs = level == 0 ? 5 : (level == 1 ? 4 : 3);
                const float seg = len / static_cast<float>(segs);
                glm::vec3 d = dir;
                float r = radius;
                for (int si = 0; si < segs; ++si) {
                    // THE TURN, per segment: side wander always; up pull that
                    // fades with level (twigs stop caring about the sky); and
                    // one segment in ~4 dips DOWN — the sag real limbs show.
                    //
                    // THE OAK CLAUSE (user, on the colossus stand: «у дуба
                    // ветки не только вверх, ещё и вбок сильно... низкие ветки
                    // почти по всей длине параллельно земле идут»): a LOW
                    // scaffold keeps almost no upward pull for its whole
                    // length — the dome comes from the high scaffolds climbing
                    // while the low ones REACH.
                    const glm::vec3 side = safe_normalize(
                        glm::cross(d, glm::vec3{0.0f, 1.0f, 0.0f}),
                        glm::vec3{1.0f, 0.0f, 0.0f});
                    const bool low_limb = level == 0 && dir.y < 0.45f;
                    const float up_pull = (low_limb ? 0.05f : 0.22f)
                                        / static_cast<float>(1 + level);
                    const float dip = rng.unit() < 0.25f ? -0.18f : 0.0f;
                    d = safe_normalize(d + side * (rng.sym() * 0.30f)
                                         + glm::vec3{0.0f, up_pull + dip, 0.0f}, d);
                    const glm::vec3 np = pos + d * seg;
                    const float taper = 1.0f - 0.6f * (static_cast<float>(si + 1)
                                                       / static_cast<float>(segs));
                    const float nr = std::max(radius * taper, 0.025f);
                    const int sides = level == 0 ? 5 : (level == 1 ? 4 : 3);
                    if (level <= 1) { // heavy limbs wear the bark texture
                        bark_tube(obj.bark, pos, np, d, r, nr, sides, bark_uv,
                                  pos.y, TAU * r, wind_at(pos), wind_at(np));
                    } else {
                        tube_segment(obj.wood, pos, np, d, r, nr, sides, bark);
                    }

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
                // THE CROWN'S INTERIOR (user: «в центре нет листвы... в центре
                // пустота»): foliage also grows along the INNER run of every
                // branch, not only at the rim its tips reach — an oak's crown
                // is full because leaves sprout wherever light does, and ours
                // sprouted only where recursion terminated.
                if (level >= 1 && rng.unit() < 0.6f) {
                    anchors.push_back({pos - d * (len * 0.75f), d});
                }
            }
        } grow{obj, rng, anchors, bark, crown_c, p.crown_radius, crown_ry,
               bark_uv, phase, p.height};

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
        // --- THE CONIFER, v6 (passports §2.1-2.2): whorls down the cone, but
        // a branch is DRESSED AS A FROND RIBBON — a bent strip carrying the
        // painted branch-with-needles sheet (the Skyrim construction; the
        // fir/pine photoscans' own «twig» material) — not tubes with flat
        // cards pinned to anchors. Bare wood shows only near the trunk, in
        // the windows between tiers.
        const glm::vec4 needle_uv = leaf_tile_uv(LeafShape::NeedleFan, p.tone);
        const float phase_c = phase; // fronds share the tree's wind phase
        // One frond ribbon: sagging centre-line, tip lifting back up (the
        // frame-6 silhouette), stem of the sheet running along the strip.
        const auto emit_frond = [&](glm::vec3 pos0, glm::vec3 dir0, float len,
                                    float half_w, float droop_rad) {
            const int segs = 4;
            glm::vec3 fpos = pos0;
            glm::vec3 fd = dir0;
            const glm::vec3 up{0.0f, 1.0f, 0.0f};
            const glm::vec3 side = safe_normalize(glm::cross(up, fd),
                                                  glm::vec3{1.0f, 0.0f, 0.0f});
            const float jit = 0.40f + rng.unit() * 0.26f;
            const auto base_v = static_cast<uint32_t>(obj.cards.vertices.size());
            for (int s = 0; s <= segs; ++s) {
                const float t = static_cast<float>(s) / static_cast<float>(segs);
                if (s > 0) {
                    // Sag per segment; the LAST segment lifts — spruce tips
                    // turn up (frame 6).
                    const float sag = (s == segs ? 0.55f : -1.0f) * droop_rad
                                    / static_cast<float>(segs);
                    fd = safe_normalize(fd + up * sag + side * (rng.sym() * 0.05f), fd);
                    fpos += fd * (len / static_cast<float>(segs));
                }
                // Width: narrow butt, widest mid, tapering tip.
                const float ww = half_w
                    * (0.30f + 0.70f * std::sin(std::min(3.1416f * (0.18f + 0.82f * t),
                                                         3.1416f)));
                const float uu = needle_uv.x
                    + (needle_uv.z - needle_uv.x) * (0.04f + 0.92f * t);
                const glm::vec3 n = safe_normalize(up * 0.85f + fd * 0.15f, up);
                const float sway = 0.15f + 0.85f * t; // honest: distance from support
                obj.cards.vertices.push_back(
                    {fpos - side * ww, n,
                     {uu, needle_uv.y + (needle_uv.w - needle_uv.y) * 0.06f},
                     pack({sway, phase_c, jit})});
                obj.cards.vertices.push_back(
                    {fpos + side * ww, n,
                     {uu, needle_uv.y + (needle_uv.w - needle_uv.y) * 0.94f},
                     pack({sway, phase_c, jit})});
            }
            for (int s = 0; s < segs; ++s) {
                const uint32_t a0 = base_v + static_cast<uint32_t>(s) * 2u;
                obj.cards.indices.insert(obj.cards.indices.end(),
                                         {a0, a0 + 2u, a0 + 3u, a0, a0 + 3u, a0 + 1u});
            }
        };

        for (int w = 0; w < p.whorl_count; ++w) {
            const float t = (static_cast<float>(w) + 0.5f)
                          / static_cast<float>(p.whorl_count);
            const float y = crown_base + (p.height - crown_base) * t;
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
                if (rng.unit() < 0.06f) continue; // ragged whorls, not a fan
                const float az = az0 + TAU * static_cast<float>(b)
                                     / static_cast<float>(count)
                               + rng.sym() * 0.3f;
                const glm::vec3 out{std::cos(az), 0.0f, std::sin(az)};
                const glm::vec3 bp = at->pos + out * (at->radius * 0.5f);
                const glm::vec3 d = safe_normalize(
                    out + glm::vec3{0.0f, 0.14f - p.droop * 0.45f, 0.0f}, out);
                // Bare wood: the inner quarter only — the rest is frond.
                const float r0 = std::max(at->radius * 0.30f, 0.03f);
                const float wood_len = reach * 0.28f;
                const glm::vec3 wp = bp + d * wood_len;
                bark_tube(obj.bark, bp, wp, d, r0, std::max(r0 * 0.55f, 0.02f), 4,
                          bark_uv, bp.y, TAU * std::max(r0, 0.05f),
                          pack_wind(wood_sway(bp.y), phase),
                          pack_wind(std::min(wood_sway(bp.y) + 0.12f, 0.6f), phase));
                // The ribbon overlaps the wood so the joint never shows.
                emit_frond(bp + d * (wood_len * 0.55f), d, reach * 0.9f,
                           std::min(reach * 0.30f, 1.7f) * (0.85f + rng.unit() * 0.3f),
                           p.droop * (0.8f + rng.unit() * 0.4f));
            }
        }

        // THE DEAD-BRANCH SKIRT (both photoscans, every variant: bare broken
        // stubs at 0.10-0.40 of height, under the live crown). Only where the
        // live crown starts high enough to leave the band exposed.
        const float skirt_top = std::min(crown_base * 0.95f, p.height * 0.40f);
        const float skirt_bot = p.height * 0.10f;
        if (skirt_top - skirt_bot > 0.5f) {
            const int stubs = 6 + static_cast<int>(p.height * 0.35f);
            for (int s = 0; s < stubs; ++s) {
                const float y = skirt_bot + (skirt_top - skirt_bot) * rng.unit();
                const float az = rng.unit() * TAU;
                const glm::vec3 out{std::cos(az), 0.0f, std::sin(az)};
                const Ring* at = &bole.front();
                for (const Ring& ring : bole) {
                    if (ring.pos.y <= y) at = &ring;
                }
                const float len = p.crown_radius * (0.12f + rng.unit() * 0.22f);
                const glm::vec3 d = safe_normalize(
                    out + glm::vec3{0.0f, -0.15f - rng.unit() * 0.25f, 0.0f}, out);
                const float rr = std::max(at->radius * 0.12f, 0.015f);
                bark_tube(obj.bark, at->pos + out * (at->radius * 0.5f),
                          at->pos + out * (at->radius * 0.5f) + d * len, d, rr,
                          0.0f, 3, bark_uv, y, TAU * rr,
                          pack_wind(wood_sway(y), phase),
                          pack_wind(std::min(wood_sway(y) + 0.08f, 0.5f), phase));
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
            // Jitter small enough that the card always OVERLAPS its anchor
            // (user: «часть листка летает в воздухе, ни к чему не
            // присоединено» — the old jitter could carry a spray clear of its
            // twig).
            const glm::vec3 c = a.pos + jitter * (p.crown_radius * 0.035f);
            const glm::vec3 radial = safe_normalize(c - crown_c, a.dir);
            // THE GROWTH DIRECTION (research §4.3, the user's aspen and pine
            // frames): foliage lies in near-HORIZONTAL layers along its branch
            // with an upward pull — broadleaf clumps face up-and-out, conifer
            // fronds lie almost flat and sag at the tip. The chaos the user
            // called out was the roll and the dome-heavy normals; both bow to
            // UP now.
            const glm::vec3 up{0.0f, 1.0f, 0.0f};
            // THE CANOPY HAS AN UNDERSIDE (user, looking up the colossus: «вектор
            // взгляда лежит в плоскости листьев, а должен быть перпендикулярен» —
            // from below he was seeing every layer edge-on). Cards on the lower
            // hemisphere of the crown bow DOWNWARD instead of up: a viewer under
            // the tree now faces leaf planes, exactly as a real canopy shows its
            // underleaves.
            const float below = std::clamp((crown_c.y - c.y)
                                               / std::max(crown_ry, 0.1f), 0.0f, 1.0f);
            const glm::vec3 vertical = below > 0.35f ? -up : up;
            const glm::vec3 n = p.conifer
                ? safe_normalize(vertical * 0.8f + a.dir * 0.15f + radial * 0.05f, vertical)
                : safe_normalize(vertical * 0.45f + radial * 0.3f + a.dir * 0.25f, vertical);
            LeafCardParams card;
            card.center = c;
            card.normal = n;
            card.half_width = p.crown_radius * p.spray_frac * (0.85f + rng.unit() * 0.45f);
            card.half_height = card.half_width * (p.conifer ? 0.55f : 0.7f);
            // FULL free roll (user: «слишком много параллельных линий, видно
            // что искусственное; линии под разными углами»): roll spins the
            // card IN its plane, so the layering the normals build survives —
            // the ±14° cap was killing the edge-angle variety, not the chaos.
            card.roll = rng.unit() * TAU;
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
            cross.roll = rng.unit() * TAU;
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
    bark_pass(obj.wood); // only the flat-colour twigs remain in this stream;
                         // the textured bark/ground streams carry their own
                         // furrows and moss in the atlas tiles

    obj.content_hash = object_content_hash(obj);
    return obj;
}

} // namespace dfn::render
