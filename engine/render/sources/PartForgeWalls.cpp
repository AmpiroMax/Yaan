/*
Created: 17:08:2026 - 13:09:29
Last updated: 17:08:2026 - 15:46:07
Module: engine/render
File: engine/render/sources/PartForgeWalls.cpp

Responsibility:
- THE WALL VARIANTS (user, 17.08: «я также жду от агента кучу разных вариантов
  стен»): ten construction styles — сруб, фахверк с тремя рисунками раскосов,
  обшивка вертикальная и горизонтальная, камень тёсаный и бутовый, кирпич,
  глина по каркасу, низ-камень-верх-дерево — each blind or cut with a window,
  two windows, or a door opening. One TU of the family split (Rule 21).

Key items:
- part_detail::make_wall_styled(): dispatch over PartParams::variant.
- Hole/holes_of(): the opening cut, shared by every style.
- course walls (log/ashlar/rubble/brick) vs skin walls (boards) vs frame.

Dependencies:
- Uses: PartForgeDetail.h (material table, helpers), HewnBar.h.
- Used by: PartForge.cpp (forge_part dispatch).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- SEALED BY CONSTRUCTION (HOUSES.md §2): every course bites 1 cm into the
  next and every skin has its core, so no style can ship a through-gap. The
  ONE sanctioned through-hole is the DOOR opening — a wall part with a door
  hole is legal, the HOUSE must close it with a leaf; the wall tests assert
  daylight there and nowhere else.
- A wall is seen from BOTH sides (the farmhouse east-wall lesson): masonry is
  full-thickness, skins come in pairs, infill slabs read from either face.
*/
/*
UPD:
- 17:08:2026 - 13:09:29: Создан — волна вариантов стен (10 стилей x 4 проёма).
- 17:08:2026 - 14:29:43: обшивка носит ПИЛЁНУЮ колонку атласа (skin_as_board), ядро за ней —
  свою: доска и брус — разные поверхности одного дерева. material_of получил
  wear (ряд атласа = тон и износ вместе).
- 17:08:2026 - 15:46:07: текстурность стала свойством ДЕТАЛИ (kit_textured_default() — одно
  определение умолчания). Была процессная дверь, читаемая внутри кузницы, и
  тест текстурного потока не мог её попросить: он проверял умолчание и
  покраснел в день, когда умолчание сменилось. Полка байт в байт прежняя.
*/

#include "engine/render/sources/PartForgeDetail.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace dfn::render::part_detail {
namespace {

/// This family's dimensions, metres. Set against the same reference frames
/// as the base kit's (images_examples/houses_outdoors).
constexpr float WIN_W_M = 1.0f;      ///< a window opening's width
constexpr float WIN_H_M = 1.0f;      ///< ... and height
constexpr float WIN_SILL_M = 1.1f;   ///< sill height: eye-level glass, 1.7 eye
constexpr float DOOR_W_M = 1.0f;     ///< a door opening, PLAYER_CAPSULE + jambs
constexpr float DOOR_H_M = 2.05f;    ///< above PLAYER_CAPSULE_HEIGHT 1.8
constexpr float COURSE_BITE_M = 0.01f; ///< a course's overlap into the next
constexpr float LOG_COURSE_M = 0.23f;  ///< the log wall rhythm (= corner part's)
constexpr float ASHLAR_COURSE_M = 0.40f; ///< тёсаный камень: tall regular courses
constexpr float RUBBLE_COURSE_M = 0.28f; ///< бут: lower, rougher
constexpr float BRICK_COURSE_M = 0.16f;  ///< кирпич: thin banded courses
constexpr float FRAME_MEMBER_M = 0.15f;  ///< фахверк: visible timber's section
constexpr float COMBO_STONE_H_M = 1.0f;  ///< низ-камень: the stone belt's height

/// One rectangular opening in the wall's face plane (x along, y up).
struct Hole {
    float x0, x1, y0, y1;
    bool pane; ///< true = window (gets a blind pane); false = door (stays open)
};

[[nodiscard]] std::vector<Hole> holes_of(const PartParams& p, float w, float h) {
    std::vector<Hole> out;
    const float ww = std::min(WIN_W_M, w * 0.30f);
    const float wh = std::min(WIN_H_M, h - WIN_SILL_M - 0.5f);
    switch (p.opening) {
    case 1: // одно окно, centered
        out.push_back({w * 0.5f - ww * 0.5f, w * 0.5f + ww * 0.5f, WIN_SILL_M,
                       WIN_SILL_M + wh, true});
        break;
    case 2: // два окна at the third points
        out.push_back({w * 0.30f - ww * 0.5f, w * 0.30f + ww * 0.5f, WIN_SILL_M,
                       WIN_SILL_M + wh, true});
        out.push_back({w * 0.70f - ww * 0.5f, w * 0.70f + ww * 0.5f, WIN_SILL_M,
                       WIN_SILL_M + wh, true});
        break;
    case 3: // дверной проём, centered, floor-up
        out.push_back({w * 0.5f - DOOR_W_M * 0.5f, w * 0.5f + DOOR_W_M * 0.5f,
                       0.0f, DOOR_H_M, false});
        break;
    default: break;
    }
    return out;
}

/// The share of [x0..x1] x [y0..y1] NOT covered by holes, as blocks of depth
/// `thick` starting at `z0`. The wall body around its openings — every style
/// calls this for its core or its slab.
void slab_around(MeshData& m, const std::vector<Hole>& holes, float x0, float x1,
                 float y0, float y1, float z0, float thick, const Material& mat,
                 float wear, Rng& rng) {
    // A sealing slab is a TRUE BOX. The material's chamfer would cut its
    // corners (stone cuts 30% of a half-section), and on a 2 m wide slab that
    // corner cut is a 40 cm daylight wedge — the panel instrument caught
    // exactly that on three styles' first bake (first holes at y=0.02 rims
    // and course seams). Wobble off for the same reason.
    Material slab = mat;
    slab.chamfer = 0.0f;
    slab.wobble = 0.0f;
    std::vector<float> cuts{x0, x1};
    for (const Hole& hh : holes) {
        if (hh.x1 > x0 && hh.x0 < x1) {
            cuts.push_back(std::clamp(hh.x0, x0, x1));
            cuts.push_back(std::clamp(hh.x1, x0, x1));
        }
    }
    std::sort(cuts.begin(), cuts.end());
    for (std::size_t i = 0; i + 1 < cuts.size(); ++i) {
        const float sx0 = cuts[i];
        const float sx1 = cuts[i + 1];
        if (sx1 - sx0 < 0.01f) {
            continue;
        }
        const float mid = (sx0 + sx1) * 0.5f;
        // y-intervals of this strip not covered by a hole.
        std::vector<std::pair<float, float>> spans{{y0, y1}};
        for (const Hole& hh : holes) {
            if (mid < hh.x0 || mid > hh.x1) {
                continue;
            }
            std::vector<std::pair<float, float>> next;
            for (const auto& s : spans) {
                if (hh.y1 <= s.first || hh.y0 >= s.second) {
                    next.push_back(s);
                    continue;
                }
                if (hh.y0 > s.first) {
                    next.push_back({s.first, hh.y0});
                }
                if (hh.y1 < s.second) {
                    next.push_back({hh.y1, s.second});
                }
            }
            spans = std::move(next);
        }
        for (const auto& s : spans) {
            if (s.second - s.first < 0.02f) {
                continue;
            }
            block(m, {sx0, s.first, z0}, {sx1 - sx0, s.second - s.first, thick},
                  slab, wear, rng, 2);
        }
    }
}

/// Trim around every opening: jambs, sill/lintel, and for a window the blind
/// pane with its mullion cross (§2: окна глухие, с имитацией вида насквозь).
void trim_holes(MeshData& m, const std::vector<Hole>& holes, float t,
                const Material& trim, const PartParams& p, Rng& rng) {
    const float j = 0.10f; // trim member half-size
    for (const Hole& hh : holes) {
        const float yb = std::max(hh.y0, 0.0f);
        // Jambs, full thickness and a shade proud of both faces.
        hewn_bar(m, {hh.x0 - j * 0.5f, yb, t * 0.5f}, {0.0f, 1.0f, 0.0f},
                 {0.0f, 0.0f, 1.0f}, hh.y1 - yb + j, j * 0.5f, t * 0.55f, trim,
                 p.wear, rng, 2);
        hewn_bar(m, {hh.x1 + j * 0.5f, yb, t * 0.5f}, {0.0f, 1.0f, 0.0f},
                 {0.0f, 0.0f, 1.0f}, hh.y1 - yb + j, j * 0.5f, t * 0.55f, trim,
                 p.wear, rng, 2);
        // Lintel always; sill only when the hole does not start at the floor.
        hewn_bar(m, {hh.x0 - j, hh.y1 + j * 0.5f, t * 0.5f}, {1.0f, 0.0f, 0.0f},
                 {0.0f, 1.0f, 0.0f}, hh.x1 - hh.x0 + 2.0f * j, t * 0.55f, j * 0.5f,
                 trim, p.wear, rng, 2);
        if (hh.y0 > 0.05f) {
            hewn_bar(m, {hh.x0 - j, hh.y0 - j * 0.5f, t * 0.5f}, {1.0f, 0.0f, 0.0f},
                     {0.0f, 1.0f, 0.0f}, hh.x1 - hh.x0 + 2.0f * j, t * 0.55f,
                     j * 0.5f, trim, p.wear, rng, 2);
        }
        if (hh.pane) {
            // The blind insert, overlapping the trim all round, plus the
            // mullion cross that makes it read as a casement.
            block(m, {hh.x0 - 0.03f, hh.y0 - 0.03f, t * 0.5f - PANE_THICK_M * 0.5f},
                  {hh.x1 - hh.x0 + 0.06f, hh.y1 - hh.y0 + 0.06f, PANE_THICK_M},
                  material_of(PartMaterial::Pane, p.wear, p.textured), p.wear * 0.3f, rng, 1);
            const float cx = (hh.x0 + hh.x1) * 0.5f;
            const float cy = (hh.y0 + hh.y1) * 0.5f;
            hewn_bar(m, {cx, hh.y0, t * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
                     hh.y1 - hh.y0, 0.03f, t * 0.45f, trim, p.wear, rng, 2);
            hewn_bar(m, {hh.x0, cy, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
                     hh.x1 - hh.x0, t * 0.45f, 0.03f, trim, p.wear, rng, 2);
        }
    }
}

/// COURSE WALLS: full-thickness horizontal courses biting into each other —
/// сруб, тёсаный камень, бут, кирпич are all this loop with different rhythm,
/// segmentation and material.
void make_courses(MeshData& m, const PartParams& p, const Material& mat, Rng& rng,
                  float course_h, float seg_w, float seg_jitter, bool offset_rows,
                  const std::vector<Hole>& holes, float w, float h, float t) {
    const int courses = std::max(1, static_cast<int>(h / course_h + 0.5f));
    for (int i = 0; i < courses; ++i) {
        const float y0 = static_cast<float>(i) * course_h;
        const float y1 = std::min(y0 + course_h + COURSE_BITE_M, h);
        const float yc = (y0 + y1) * 0.5f;
        // x-runs of this course not covered by a hole.
        std::vector<std::pair<float, float>> runs{{0.0f, w}};
        for (const Hole& hh : holes) {
            if (yc < hh.y0 || yc > hh.y1) {
                continue;
            }
            std::vector<std::pair<float, float>> next;
            for (const auto& r : runs) {
                if (hh.x1 <= r.first || hh.x0 >= r.second) {
                    next.push_back(r);
                    continue;
                }
                if (hh.x0 > r.first) {
                    next.push_back({r.first, hh.x0});
                }
                if (hh.x1 < r.second) {
                    next.push_back({hh.x1, r.second});
                }
            }
            runs = std::move(next);
        }
        const float offset = (offset_rows && i % 2 == 1) ? seg_w * 0.5f : 0.0f;
        for (const auto& r : runs) {
            if (r.second - r.first < 0.03f) {
                continue;
            }
            if (seg_w <= 0.0f) {
                // One log the whole run: сруб.
                hewn_bar(m, {r.first, yc, t * 0.5f}, {1.0f, 0.0f, 0.0f},
                         {0.0f, 1.0f, 0.0f}, r.second - r.first, t * 0.5f,
                         (y1 - y0) * 0.5f, mat, p.wear, rng, 3);
                continue;
            }
            // Segmented masonry: stones/bricks with alternating row offset;
            // each segment overlaps its neighbour 1 cm, so a joint is a
            // shadow, never a slit.
            float x = r.first - offset;
            while (x < r.second) {
                const float jw = seg_w * (1.0f + seg_jitter * (rng.unit() - 0.5f));
                const float sx0 = std::max(x, r.first);
                const float sx1 = std::min(x + jw + COURSE_BITE_M, r.second);
                x += jw;
                if (sx1 - sx0 < 0.03f) {
                    continue;
                }
                hewn_bar(m, {sx0, yc, t * 0.5f}, {1.0f, 0.0f, 0.0f},
                         {0.0f, 1.0f, 0.0f}, sx1 - sx0, t * 0.485f,
                         (y1 - y0) * 0.485f, mat, p.wear, rng, 2);
            }
        }
    }
    // The masonry seams stay shadows and never daylight: the sealed slab
    // inside the wall, same remedy as the base kit's footing.
    Material core = mat;
    core.color *= 0.55f;
    core.wobble = 0.0f;
    // BOARDS ARE SAWN, and the wall says so: the skin wears the sawn column
    // while the core behind it keeps the material's own surface (it is only
    // ever seen through a reveal).
    Material skin = mat;
    skin_as_board(skin);
    slab_around(m, holes, 0.0f, w, 0.0f, h, t * 0.5f - WALL_CORE_M * 0.5f,
                WALL_CORE_M, core, p.wear * 0.5f, rng);
}

/// SKIN WALLS: two board skins over a sealed core — обшивка вертикальная и
/// горизонтальная.
void make_boarded(MeshData& m, const PartParams& p, const Material& mat, Rng& rng,
                  bool vertical, const std::vector<Hole>& holes, float w, float h,
                  float t) {
    Material core = mat;
    core.color *= 0.55f;
    core.wobble = 0.0f;
    // BOARDS ARE SAWN, and the wall says so: the skin wears the sawn column
    // while the core behind it keeps the material's own surface (it is only
    // ever seen through a reveal).
    Material skin = mat;
    skin_as_board(skin);
    slab_around(m, holes, 0.0f, w, 0.0f, h, t * 0.5f - WALL_CORE_M * 0.5f,
                WALL_CORE_M, core, p.wear * 0.5f, rng);
    const float span = vertical ? w : h;
    const int boards = std::max(2, static_cast<int>(span / BOARD_W_M + 0.5f));
    const float bw = span / static_cast<float>(boards);
    for (int side = 0; side < 2; ++side) {
        const float z = side == 0 ? INFILL_THICK_M * 0.5f : t - INFILL_THICK_M * 0.5f;
        for (int i = 0; i < boards; ++i) {
            const float c0 = static_cast<float>(i) * bw;
            const float mid = c0 + bw * 0.5f;
            // Board runs not covered by a hole (in the board's own direction).
            std::vector<std::pair<float, float>> runs{
                {0.0f, vertical ? h : w}};
            for (const Hole& hh : holes) {
                const float lo = vertical ? hh.y0 : hh.x0;
                const float hi = vertical ? hh.y1 : hh.x1;
                const float p0 = vertical ? hh.x0 : hh.y0;
                const float p1 = vertical ? hh.x1 : hh.y1;
                if (mid < p0 || mid > p1) {
                    continue;
                }
                std::vector<std::pair<float, float>> next;
                for (const auto& r : runs) {
                    if (hi <= r.first || lo >= r.second) {
                        next.push_back(r);
                        continue;
                    }
                    if (lo > r.first) {
                        next.push_back({r.first, lo});
                    }
                    if (hi < r.second) {
                        next.push_back({hi, r.second});
                    }
                }
                runs = std::move(next);
            }
            for (const auto& r : runs) {
                if (r.second - r.first < 0.05f) {
                    continue;
                }
                const float bhw = (bw - BOARD_GAP_M * (0.3f + 0.7f * p.wear)) * 0.5f;
                if (vertical) {
                    hewn_bar(m, {mid, r.first, z}, {0.0f, 1.0f, 0.0f},
                             {0.0f, 0.0f, 1.0f}, r.second - r.first, bhw,
                             INFILL_THICK_M * 0.5f, skin, p.wear, rng, 2);
                } else {
                    hewn_bar(m, {r.first, mid, z}, {1.0f, 0.0f, 0.0f},
                             {0.0f, 1.0f, 0.0f}, r.second - r.first,
                             INFILL_THICK_M * 0.5f, bhw, skin, p.wear, rng, 2);
                }
            }
        }
    }
}

/// FRAME WALLS (фахверк): visible timber skeleton proud of a full-depth
/// infill slab. `braces`: 1 = одинарный раскос, 2 = андреевский крест,
/// 3 = К-образный (средник + два раскоса в его середину).
void make_frame(MeshData& m, const PartParams& p, const Material& infill, Rng& rng,
                int braces, const std::vector<Hole>& holes, float w, float h,
                float t) {
    const Material timber = material_of(PartMaterial::TimberDark, p.wear, p.textured);
    const float fs = FRAME_MEMBER_M;
    // The slab first: full thickness, faces just shy of both wall planes.
    slab_around(m, holes, 0.0f, w, 0.0f, h, 0.02f, t - 0.04f, infill, p.wear, rng);
    // Sill, head, corner studs.
    hewn_bar(m, {0.0f, fs * 0.5f, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
             w, t * 0.55f, fs * 0.5f, timber, p.wear, rng, 3);
    hewn_bar(m, {0.0f, h - fs * 0.5f, t * 0.5f}, {1.0f, 0.0f, 0.0f},
             {0.0f, 1.0f, 0.0f}, w, t * 0.55f, fs * 0.5f, timber, p.wear, rng, 3);
    hewn_bar(m, {fs * 0.5f, 0.0f, t * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
             h, fs * 0.5f, t * 0.55f, timber, p.wear, rng, 3);
    hewn_bar(m, {w - fs * 0.5f, 0.0f, t * 0.5f}, {0.0f, 1.0f, 0.0f},
             {0.0f, 0.0f, 1.0f}, h, fs * 0.5f, t * 0.55f, timber, p.wear, rng, 3);
    // Braces. With an opening they retreat to the side margins — a brace
    // through a window is not carpentry, it is a mistake with a diagonal.
    const auto brace = [&](glm::vec3 a, glm::vec3 b) {
        const glm::vec3 d = b - a;
        hewn_bar(m, a, d, {0.0f, 0.0f, 1.0f}, glm::length(d), fs * 0.4f, t * 0.55f,
                 timber, p.wear, rng, 3);
    };
    if (holes.empty()) {
        if (braces == 1) {
            brace({fs, fs, t * 0.5f}, {w - fs, h - fs, t * 0.5f});
        } else if (braces == 2) {
            brace({fs, fs, t * 0.5f}, {w - fs, h - fs, t * 0.5f});
            brace({fs, h - fs, t * 0.5f}, {w - fs, fs, t * 0.5f});
        } else {
            hewn_bar(m, {w * 0.5f, 0.0f, t * 0.5f}, {0.0f, 1.0f, 0.0f},
                     {0.0f, 0.0f, 1.0f}, h, fs * 0.5f, t * 0.55f, timber, p.wear,
                     rng, 3);
            brace({fs, fs, t * 0.5f}, {w * 0.5f, h * 0.5f, t * 0.5f});
            brace({w - fs, fs, t * 0.5f}, {w * 0.5f, h * 0.5f, t * 0.5f});
        }
        return;
    }
    const float left = holes.front().x0;
    const float right = holes.back().x1;
    if (left > 0.8f) {
        brace({fs, fs, t * 0.5f}, {left - 0.05f, h - fs, t * 0.5f});
    }
    if (w - right > 0.8f) {
        brace({w - fs, fs, t * 0.5f}, {right + 0.05f, h - fs, t * 0.5f});
    }
}

} // namespace

/// The styled walls' dispatch. variant: 1 log, 2 frame-a, 3 frame-x,
/// 4 frame-k, 5 board-v, 6 board-h, 7 ashlar, 8 rubble, 9 brick, 10 combo.
void make_wall_styled(MeshData& m, const PartParams& p, const Material& mat,
                      Rng& rng) {
    const float w = m_of(p.length_u);
    const float h = m_of(p.height_u);
    const float t = m_of(p.width_u);
    const std::vector<Hole> holes = holes_of(p, w, h);
    const bool masonry = p.variant >= 7 && p.variant <= 10;
    const Material trim =
        material_of(masonry ? PartMaterial::Stone : PartMaterial::Timber, p.wear, p.textured);
    switch (p.variant) {
    case 1: // СРУБ: full-run logs at the corner part's own rhythm.
        make_courses(m, p, mat, rng, LOG_COURSE_M, 0.0f, 0.0f, false, holes, w, h, t);
        break;
    case 2:
    case 3:
    case 4:
        make_frame(m, p, mat, rng, p.variant - 1, holes, w, h, t);
        break;
    case 5:
        make_boarded(m, p, mat, rng, true, holes, w, h, t);
        break;
    case 6:
        make_boarded(m, p, mat, rng, false, holes, w, h, t);
        break;
    case 7: { // ТЁСАНЫЙ КАМЕНЬ: tall regular courses, crisp arrises.
        Material ashlar = mat;
        ashlar.wobble *= 0.25f;
        ashlar.chamfer = 0.10f;
        make_courses(m, p, ashlar, rng, ASHLAR_COURSE_M, 0.75f, 0.15f, true, holes,
                     w, h, t);
        break;
    }
    case 8: // БУТ: low rough courses, wild widths, the kit stone's full wobble.
        make_courses(m, p, mat, rng, RUBBLE_COURSE_M, 0.45f, 0.75f, true, holes, w,
                     h, t);
        break;
    case 9: // КИРПИЧ: thin courses, running bond by half-offset rows.
        make_courses(m, p, mat, rng, BRICK_COURSE_M, 0.45f, 0.05f, true, holes, w,
                     h, t);
        break;
    case 10: { // НИЗ КАМЕНЬ — ВЕРХ ДЕРЕВО, with a belt beam on the seam.
        std::vector<Hole> lower;
        std::vector<Hole> upper;
        for (const Hole& hh : holes) {
            if (hh.y0 < COMBO_STONE_H_M) {
                lower.push_back({hh.x0, hh.x1, hh.y0,
                                 std::min(hh.y1, COMBO_STONE_H_M), hh.pane});
            }
            if (hh.y1 > COMBO_STONE_H_M) {
                upper.push_back({hh.x0, hh.x1,
                                 std::max(hh.y0, COMBO_STONE_H_M) - COMBO_STONE_H_M,
                                 hh.y1 - COMBO_STONE_H_M, hh.pane});
            }
        }
        Material stone = material_of(PartMaterial::Stone, p.wear, p.textured);
        // The stone belt (its own courses)...
        {
            MeshData belt;
            make_courses(belt, p, stone, rng, RUBBLE_COURSE_M, 0.5f, 0.5f, true,
                         lower, w, COMBO_STONE_H_M, t);
            append_transformed(m, belt, {0.0f, 0.0f, 0.0f}, 0.0f, 1.0f);
        }
        // ...the belt beam every reference house wears on that seam...
        hewn_bar(m, {0.0f, COMBO_STONE_H_M + 0.05f, t * 0.5f}, {1.0f, 0.0f, 0.0f},
                 {0.0f, 1.0f, 0.0f}, w, t * 0.55f, 0.06f,
                 material_of(PartMaterial::Timber, p.wear, p.textured), p.wear, rng, 3);
        // ...and the boarded top, shifted up.
        {
            MeshData top;
            PartParams tp = p;
            make_boarded(top, tp, mat, rng, true, upper, w, h - COMBO_STONE_H_M, t);
            append_transformed(m, top, {0.0f, COMBO_STONE_H_M, 0.0f}, 0.0f, 1.0f);
        }
        break;
    }
    default:
        // Unknown style: the base bay (never silent geometry-less output).
        make_boarded(m, p, mat, rng, true, holes, w, h, t);
        break;
    }
    trim_holes(m, holes, t, trim, p, rng);
}

} // namespace dfn::render::part_detail
