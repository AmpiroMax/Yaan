/*
Created: 21:08:2026 - 00:40:00
Last updated: 22:08:2026 - 23:00:32
Module: engine/world
File: engine/world/sources/HouseWalls.cpp

Responsibility:
- СТЕНА-ЦЕПОЧКА: шесть слоёв пролёта — пластина с прорезями и остеклением,
  венцы, обшивка/кладка по ЕДИНОЙ раскладке проёмов, завалинка, перерубы,
  ставни и крыльцо. Одна рама на всех, один источник дыр.

Key items:
- build_chain_surface; lay_out_span / push_opening_frame / build_wall_plate
  / build_log_courses / build_cladding / build_plinth_belt / build_log_ends
  / build_opening_trim; push_wall_slab, course_jitter, build_courses.

Dependencies:
- Uses: HouseMeshDetail.h, HouseStyle.h (раскладка)
- Used by: сборка build_house_mesh (HouseMesh.cpp) и соседние модули постройки.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
- ПО ФАЙЛУ НА АЛГОРИТМ (решение пользователя 21.08): модуль держит ОДИН
  алгоритм постройки; общие руки — в HouseMeshDetail.h.
*/
/*
UPD:
- 21:08:2026 - 00:40:00: Вырезан из HouseMesh.cpp (1942 строки, девять алгоритмов в одном файле).
- 21:08:2026 - 01:50:00: build_weathering: подтёки под проёмами и трещины-зигзаги по износу; косметика без коллайдера; трещины не висят в проёмах.
- 21:08:2026 - 02:45:00: Подтёки только на камне/штукатурке (на досках — «узор обоев»); трещины поверх кладки, 2-4 на пролёт.
- 22:08:2026 - 23:00:32: фаза плитки у досок обшивки (uv_shift через push_wall_slab): доски-клоны фасада получили разные волокна, дрожь той же course_jitter, что пол и кладка.
*/

#include "engine/world/sources/HouseMeshDetail.h"
#include "engine/world/sources/HouseStyle.h"

#include <algorithm>
#include <cmath>

namespace dfn::world {

namespace {

/// Числа обшивки. Здесь, а не в NUMBERS.md: это толщины ДЕКОРА в метрах,
/// видимые глазом на стене, — как радиус шарика якоря, они описывают вид, а
/// не участвуют в расчётах мира.
inline constexpr float HOUSE_BOARD_TH_M = 0.035f;  ///< вынос доски от пластины
inline constexpr float HOUSE_BRACE_W_M = 0.11f;    ///< ширина раскоса
inline constexpr float HOUSE_BRACE_TH_M = 0.05f;   ///< вынос раскоса
inline constexpr float HOUSE_FRAME_W_M = 0.07f;    ///< ширина рамы проёма
inline constexpr float HOUSE_FRAME_TH_M = 0.06f;   ///< вынос рамы проёма

/// ЧЕТЫРЁХУГОЛЬНАЯ ПЛАШКА НА ПЛОСКОСТИ СТЕНЫ, выдавленная наружу. Углы приходят
/// в координатах стены (u вдоль, v вверх); порядок обхода выправляется здесь по
/// знаку площади — вызывающий думает о раскладке, а не о правиле правой руки.
static void push_wall_slab(MeshBuilder& mb, HouseMesh& mesh, ElementId owner,
                           const glm::vec3& a, const glm::vec3& dir, const glm::vec3& face_n,
                           float base_out, float thickness, const glm::vec2 quad[4],
                           float tex_deg, glm::vec2 uv_shift = {}) {
    float area2 = 0.0f;
    for (int i = 0; i < 4; ++i) {
        const glm::vec2& p0 = quad[i];
        const glm::vec2& p1 = quad[(i + 1) % 4];
        area2 += p0.x * p1.y - p1.x * p0.y;
    }
    glm::vec3 loop[4];
    for (int i = 0; i < 4; ++i) {
        // Площадь < 0 — обход был по часовой, читаем углы задом наперёд.
        const glm::vec2& c = quad[area2 < 0.0f ? 3 - i : i];
        loop[i] = a + dir * c.x + glm::vec3{0.0f, c.y, 0.0f} + face_n * base_out;
    }
    const std::uint32_t tris[6] = {0, 1, 2, 0, 2, 3};
    push_prism(mb, loop, tris, face_n * thickness, tex_deg, mesh, owner, uv_shift);
}


} // namespace

/// ДЕТЕРМИНИРОВАННАЯ ДРОЖЬ ГЛУБИНЫ КУСКА КЛАДКИ. Хэш ряда и колонки, а не
/// случайность: две сборки одного графа обязаны дать побайтово один меш (на
/// этом стоит рукав), а глубина, разная у соседей, и есть «объём» кладки.
float course_jitter(int row, int col) {
    std::uint32_t h = static_cast<std::uint32_t>(row * 73856093) ^
                      static_cast<std::uint32_t>(col * 19349663);
    h = (h ^ (h >> 13)) * 0x85ebca6bu;
    return static_cast<float>((h >> 16) & 0xFFu) / 255.0f; // 0..1
}

namespace {

/// КЛАДКА РЯДАМИ: кирпичи или каменные блоки с перевязкой. Каждый кусок —
/// отдельная плашка со своей глубиной; проёмы обходятся, как и у обшивки.
static void build_courses(const Element& e, const ElementParams& p, const glm::vec3& a,
                          const glm::vec3& dir, float seg_len, const glm::vec3& face_n,
                          float half, bool stone, MeshBuilder& mb, HouseMesh& mesh,
                          std::span<const OpeningPlacement> openings) {
    // Кирпич 25x6.5 см с швом 1 см; блок 45x22 см с швом 1.5 см. Числа —
    // ходовые размеры кладки, вид, а не расчёт мира.
    const float unit_l = stone ? 0.45f : 0.25f;
    const float unit_h = stone ? 0.22f : 0.065f;
    const float gap = stone ? 0.015f : 0.010f;
    const float th = stone ? 0.045f : 0.030f; // вынос от пластины
    int row = 0;
    for (float v = 0.0f; v + unit_h * 0.5f < p.height; v += unit_h + gap, ++row) {
        const float v1 = std::min(v + unit_h, p.height);
        // ПЕРЕВЯЗКА: каждый второй ряд сдвинут на полкуска — то, что отличает
        // кладку от плитки.
        float u = (row % 2 == 0) ? 0.0f : -unit_l * 0.5f;
        int col = 0;
        for (; u < seg_len; u += unit_l + gap, ++col) {
            const float u0 = std::max(u, 0.0f);
            const float u1 = std::min(u + unit_l, seg_len);
            if (u1 - u0 < gap) {
                continue;
            }
            bool blocked = false;
            for (const OpeningPlacement& op : openings) {
                if (u0 < op.u1 && u1 > op.u0 && v < op.v1 && v1 > op.v0) {
                    blocked = true;
                    break;
                }
            }
            if (blocked) {
                continue;
            }
            // ИЗНОС: у старой кладки дрожь глубже, а отдельные куски ВЫПАЛИ
            // (по хэшу — две сборки дают один меш; дыры читаются щербинами).
            if (p.wear > 0.0f && course_jitter(row * 7 + 3, col * 5 + 1) < p.wear * 0.12f) {
                continue;
            }
            glm::vec2 quad[4] = {{u0, v}, {u1, v}, {u1, v1}, {u0, v1}};
            // СКОЛОТЫЙ УГОЛ (20.08: «к износу добавить сколы»): у старого
            // куска один угол съеден — вершина сдвигается внутрь по обеим
            // осям. Квад остаётся квадом, скос читается сколом.
            if (p.wear > 0.0f
                && course_jitter(row * 17 + 9, col * 13 + 5) < p.wear * 0.3f) {
                const int corner =
                    static_cast<int>(course_jitter(col * 19 + 3, row * 23 + 7) * 3.99f);
                const float bx = (u1 - u0) * (0.25f + 0.3f * course_jitter(row, col + 40));
                const float by = (v1 - v) * (0.3f + 0.35f * course_jitter(col, row + 40));
                const float sx = (corner == 0 || corner == 3) ? bx : -bx;
                const float sy = (corner == 0 || corner == 1) ? by : -by;
                quad[corner].x += sx;
                quad[corner].y += sy;
            }
            const float depth =
                th * (0.7f + (0.3f + 0.5f * p.wear) * course_jitter(row, col));
            push_wall_slab(mb, mesh, e.id, a, dir, face_n, half, depth, quad, p.tex_deg);
        }
    }
}

/// ОБШИВКА ОДНОГО ПРОЛЁТА СТЕНЫ по раскладке HouseStyle. Раскладка считает
/// СПИСОК (доски, раскосы, проёмы) в координатах стены и говорит вслух, сколько
/// проёмов не влезло; здесь список превращается в плашки на ЛИЦЕ пролёта.
/// Лицо — то же, что у несущей пластины (facing_flipped уже учтён вызывающим).
/// ЕДИНАЯ РАСКЛАДКА ПРОЁМОВ ПРОЛЁТА — один источник дыр для пластины,
/// кладки, венцов, рам, ставней и крыльца (аудит #3, находка 3: спека
/// собиралась дважды руками, и рама с дырой разъехались бы при первой правке
/// lay_out_wall). Правило «дверь берёт верх над окнами» живёт ЗДЕСЬ и только
/// здесь; findings говорятся, когда say != nullptr — один раз на пролёт.
static WallLayout lay_out_span(const Element& e, const ElementParams& p,
                               float seg_len, HouseMesh* say) {
    WallStyle style;
    WallSpec spec;
    spec.length = seg_len;
    spec.height = p.height;
    if (static_cast<int>(p.doors) > 0) {
        style.opening = OpeningKind::Door;
        style.opening_w = HOUSE_DOOR_W_DEFAULT;
        style.opening_h = HOUSE_DOOR_H_DEFAULT;
        style.opening_sill = 0.0f;
        spec.openings = static_cast<int>(p.doors);
        if (say != nullptr && static_cast<int>(p.windows) > 0) {
            say->findings.push_back({e.id, MeshIssue::CladdingSaid, 0.0f,
                                     "окна и дверь разом раскладка пока не умеет: "
                                     "дверной проём взял верх"});
        }
    } else if (static_cast<int>(p.windows) > 0) {
        style.opening = OpeningKind::Window;
        spec.openings = static_cast<int>(p.windows);
    }
    WallLayout lay = lay_out_wall(spec, style);
    if (say != nullptr) {
        for (const LayoutFinding& f : lay.findings) {
            say->findings.push_back({e.id, MeshIssue::CladdingSaid, f.value, f.what});
        }
    }
    return lay;
}

/// РАМА ПРОЁМА — ОДНА НА ВСЕХ (аудит #3, находка 2: подоконник, перемычка и
/// две стойки были скопированы трижды дословно). base_out — с какой глубины
/// рама выступает: у венцов она сидит поверх бруса.
static void push_opening_frame(MeshBuilder& mb, HouseMesh& mesh, ElementId owner,
                               const glm::vec3& a, const glm::vec3& dir,
                               const glm::vec3& face_n, float base_out,
                               const OpeningPlacement& op, float tex_deg) {
    const float w = HOUSE_FRAME_W_M;
    const glm::vec2 frames[4][4] = {
        {{op.u0 - w, op.v0 - w}, {op.u1 + w, op.v0 - w},
         {op.u1 + w, op.v0}, {op.u0 - w, op.v0}}, // подоконник
        {{op.u0 - w, op.v1}, {op.u1 + w, op.v1},
         {op.u1 + w, op.v1 + w}, {op.u0 - w, op.v1 + w}}, // перемычка
        {{op.u0 - w, op.v0}, {op.u0, op.v0}, {op.u0, op.v1}, {op.u0 - w, op.v1}},
        {{op.u1, op.v0}, {op.u1 + w, op.v0}, {op.u1 + w, op.v1}, {op.u1, op.v1}},
    };
    for (const auto& f : frames) {
        push_wall_slab(mb, mesh, owner, a, dir, face_n, base_out, HOUSE_FRAME_TH_M,
                       f, tex_deg);
    }
}

/// ОБШИВКА ПРОЛЁТА по ГОТОВОЙ раскладке: фахверк (доски+раскосы+рамы),
/// кладка (куски+рамы) или рамы венцов. Раскладку считает вызывающий —
/// у этой функции нет права на вторую правду о проёмах.
static void build_cladding(const Element& e, const ElementParams& p, const glm::vec3& a,
                           const glm::vec3& dir, float seg_len, const glm::vec3& face_n,
                           float half, const WallLayout& lay, MeshBuilder& mb,
                           HouseMesh& mesh) {
    const WallFill fill = fill_kind(p);
    if (fill == WallFill::Logs) {
        // Венцам из обшивки принадлежат ТОЛЬКО рамы (сайдинг поверх венцов —
        // приёмка кадров 20.08); сами венцы строит build_log_courses.
        mb.set_material(0, 2);
        for (const OpeningPlacement& op : lay.openings) {
            push_opening_frame(mb, mesh, e.id, a, dir, face_n, half + 0.11f, op,
                               p.tex_deg);
        }
        mb.set_material(-1, -1);
        return;
    }
    if (fill == WallFill::Brick || fill == WallFill::Block) {
        mb.set_material(fill == WallFill::Block ? 3 : 4, -1);
        build_courses(e, p, a, dir, seg_len, face_n, half, fill == WallFill::Block,
                      mb, mesh, lay.openings);
        mb.set_material(0, 1);
        for (const OpeningPlacement& op : lay.openings) {
            push_opening_frame(mb, mesh, e.id, a, dir, face_n, half, op, p.tex_deg);
        }
        mb.set_material(-1, -1);
        return;
    }
    // Фахверк: доски, раскосы и рамы — тёсаный брус поверх элементного фона.
    mb.set_material(0, 1);
    // ДОСКИ-КЛОНЫ (владелец 23.08: «на досках всё ещё текстуры одинаковые»):
    // рамка uv центрируется на грани, и каждая доска обшивки сэмплила плитку
    // с одного места — фасад читался ксерокопией. Фаза от положения доски на
    // стене (та же дрожь course_jitter, что у пола и кладки): две сборки
    // одного графа дают побайтово один меш, соседние доски — разные волокна.
    for (const BoardRun& b : lay.boards) {
        const glm::vec2 quad[4] = {{b.u0, b.v0}, {b.u1, b.v0}, {b.u1, b.v1}, {b.u0, b.v1}};
        const int bu = static_cast<int>(std::lround(b.u0 * 8.0f));
        const int bv = static_cast<int>(std::lround(b.v0 * 8.0f));
        const glm::vec2 phase{course_jitter(bu * 13 + 5, bv * 17 + 7) * 0.731f,
                              course_jitter(bu * 19 + 11, bv * 23 + 3) * 0.517f};
        push_wall_slab(mb, mesh, e.id, a, dir, face_n, half, HOUSE_BOARD_TH_M, quad,
                       p.tex_deg, phase);
    }
    for (const BracePlacement& br : lay.braces) {
        const glm::vec2 low{br.u_low, br.v_low};
        const glm::vec2 high{br.u_high, br.v_high};
        glm::vec2 e2 = high - low;
        const float len = std::sqrt(e2.x * e2.x + e2.y * e2.y);
        if (len < HOUSE_GEOM_EPS) {
            continue;
        }
        const glm::vec2 n2 = glm::vec2{-e2.y, e2.x} / len * (HOUSE_BRACE_W_M * 0.5f);
        // Раскос выступает дальше досок: он каркас, а не обшивка.
        const glm::vec2 quad[4] = {low + n2, low - n2, high - n2, high + n2};
        push_wall_slab(mb, mesh, e.id, a, dir, face_n, half + HOUSE_BOARD_TH_M,
                       HOUSE_BRACE_TH_M, quad, p.tex_deg);
    }
    for (const OpeningPlacement& op : lay.openings) {
        push_opening_frame(mb, mesh, e.id, a, dir, face_n, half, op, p.tex_deg);
    }
    mb.set_material(-1, -1);
}

// ---------------------------------------------------------------------------
// Стена-цепочка: шесть слоёв одного пролёта (разрез build_chain_surface,
// аудит #3, находка 4: 240 строк семи обязанностей в одной функции).
// Общий словарь: a — низ начала пролёта, dir — единичное вдоль, seg_len —
// длина, face_n — лицо, half — полутолщина, holes — раскладка проёмов.
// ---------------------------------------------------------------------------

/// НЕСУЩАЯ ПЛАСТИНА с прорезями: простенки, подоконные короба, перемычки;
/// в оконные проёмы — лист остекления (Pane).
static void build_wall_plate(const Element& e, const ElementParams& p,
                             const glm::vec3& a, const glm::vec3& dir, float seg_len,
                             const glm::vec3& face_n, float half,
                             std::span<const OpeningPlacement> holes, bool windows,
                             MeshBuilder& mb, HouseMesh& mesh) {
    const std::uint32_t quad[6] = {0, 1, 2, 0, 2, 3};
    const glm::vec3 up{0.0f, p.height, 0.0f};
    const glm::vec3 h = face_n * half;
    if (holes.empty()) {
        const glm::vec3 b = a + dir * seg_len;
        const glm::vec3 loop[4] = {a - h, a + h, b + h, b - h};
        push_prism(mb, loop, quad, up, p.tex_deg, mesh, e.id);
        return;
    }
    const auto box = [&](float u0, float u1, float v0, float v1) {
        if (u1 - u0 < HOUSE_GEOM_EPS || v1 - v0 < HOUSE_GEOM_EPS) {
            return;
        }
        const auto at = [&](float u, float v) {
            return a + dir * u + glm::vec3{0.0f, v, 0.0f} - h;
        };
        const glm::vec3 loop[4] = {at(u0, v0), at(u1, v0), at(u1, v1), at(u0, v1)};
        push_prism(mb, loop, quad, face_n * (half * 2.0f), p.tex_deg, mesh, e.id);
    };
    float u_at = 0.0f;
    for (const OpeningPlacement& op : holes) {
        box(u_at, op.u0, 0.0f, p.height);   // простенок слева
        box(op.u0, op.u1, 0.0f, op.v0);     // под подоконником (у двери 0)
        box(op.u0, op.u1, op.v1, p.height); // перемычка сверху
        u_at = op.u1;
    }
    box(u_at, seg_len, 0.0f, p.height);
    if (windows) {
        // ОСТЕКЛЕНИЕ: тонкий лист «глухого окна» в срединной плоскости —
        // тёплая глубина панели читается стеклом с обеих сторон.
        mb.set_material(8, -1);
        for (const OpeningPlacement& op : holes) {
            const auto at_mid = [&](float u, float v) {
                return a + dir * u + glm::vec3{0.0f, v, 0.0f} - face_n * 0.01f;
            };
            const glm::vec3 loop[4] = {at_mid(op.u0, op.v0), at_mid(op.u1, op.v0),
                                       at_mid(op.u1, op.v1), at_mid(op.u0, op.v1)};
            push_prism(mb, loop, quad, face_n * 0.02f, p.tex_deg, mesh, e.id);
        }
        mb.set_material(-1, -1);
    }
}

/// ВЕНЦЫ СРУБА (fill=4): ряды горизонтального бруса на лицевой стороне со
/// швом; проёмы выкусывают свои диапазоны; износ углубляет дрожь.
static void build_log_courses(const Element& e, const ElementParams& p,
                              const glm::vec3& a, const glm::vec3& dir, float seg_len,
                              const glm::vec3& face_n, float half,
                              std::span<const OpeningPlacement> holes,
                              MeshBuilder& mb, HouseMesh& mesh) {
    mb.set_material(0, -1);
    const float row_h = 0.26f;
    const float seam = 0.035f;
    int lrow = 0;
    for (float y = 0.02f; y + row_h < p.height + row_h * 0.5f;
         y += row_h + seam, ++lrow) {
        const float y1 = std::min(y + row_h, p.height - 0.02f);
        if (y1 - y < 0.05f) {
            continue;
        }
        float u_at = 0.0f;
        const auto log_run = [&](float u0, float u1) {
            if (u1 - u0 < 0.08f) {
                return;
            }
            const float depth =
                0.075f + 0.035f * course_jitter(lrow, static_cast<int>(u0 * 7));
            const glm::vec2 q[4] = {{u0, y}, {u1, y}, {u1, y1}, {u0, y1}};
            push_wall_slab(mb, mesh, e.id, a, dir, face_n, half,
                           depth * (1.0f + 0.4f * p.wear), q, p.tex_deg);
        };
        for (const OpeningPlacement& op : holes) {
            if (op.v0 < y1 && op.v1 > y) {
                log_run(u_at, op.u0);
                u_at = op.u1;
            }
        }
        log_run(u_at, seg_len);
    }
    mb.set_material(-1, -1);
}

/// ЗАВАЛИНКА: каменный пояс вдоль низа лицевой стороны; дверь обходит.
static void build_plinth_belt(const Element& e, const ElementParams& p,
                              const glm::vec3& a, const glm::vec3& dir, float seg_len,
                              const glm::vec3& face_n, float half,
                              std::span<const OpeningPlacement> holes, bool doors,
                              MeshBuilder& mb, HouseMesh& mesh) {
    mb.set_material(3, 1);
    const auto belt = [&](float u0, float u1) {
        if (u1 - u0 < 0.05f) {
            return;
        }
        // Секциями по ~0.8 м: мелкая грануляция принимает вершинную грязь и
        // мох (на цельной ленте угловая интерполяция их растворяла).
        for (float u = u0; u < u1; u += 0.8f) {
            const float ue = std::min(u + 0.78f, u1);
            const glm::vec2 q[4] = {{u, 0.0f}, {ue, 0.0f}, {ue, 0.28f}, {u, 0.28f}};
            push_wall_slab(mb, mesh, e.id, a, dir, face_n, half, 0.12f, q,
                           p.tex_deg);
        }
    };
    float u_at = -0.05f;
    if (doors) {
        for (const OpeningPlacement& op : holes) {
            belt(u_at, op.u0 - 0.05f);
            u_at = op.u1 + 0.05f;
        }
    }
    belt(u_at, seg_len + 0.05f);
    mb.set_material(-1, -1);
}

/// ПЕРЕРУБЫ: торцы брёвен за обоими концами пролёта — сруб читается углами.
static void build_log_ends(const Element& e, const ElementParams& p,
                           const glm::vec3& a, const glm::vec3& dir, float seg_len,
                           const glm::vec3& face_n, MeshBuilder& mb, HouseMesh& mesh) {
    mb.set_material(2, -1); // торец с годовыми кольцами
    const glm::vec3 up1{0.0f, 1.0f, 0.0f};
    for (float y = 0.15f; y < p.height - 0.05f; y += 0.30f) {
        for (const float u_end : {0.0f, seg_len}) {
            const glm::vec3 start =
                a + dir * (u_end - 0.26f) + glm::vec3{0.0f, y, 0.0f};
            const std::vector<glm::vec3> ring =
                profile_ring(start, up1, face_n, 0.11f, 8);
            std::vector<std::uint32_t> fan;
            for (int k = 1; k + 1 < 8; ++k) {
                fan.push_back(0);
                fan.push_back(static_cast<std::uint32_t>(k));
                fan.push_back(static_cast<std::uint32_t>(k + 1));
            }
            push_prism(mb, ring, fan, dir * 0.52f, p.tex_deg, mesh, e.id);
        }
    }
    mb.set_material(-1, -1);
}

/// СТАВНИ С ПОДОКОННИКОМ у окон и КРЫЛЬЦО-СТУПЕНЬ у двери — по ТОЙ ЖЕ
/// раскладке, что резала проёмы.
static void build_opening_trim(const Element& e, const ElementParams& p,
                               const glm::vec3& a, const glm::vec3& dir,
                               const glm::vec3& face_n, float half,
                               std::span<const OpeningPlacement> holes, bool doors,
                               bool windows, MeshBuilder& mb, HouseMesh& mesh) {
    for (const OpeningPlacement& op : holes) {
        if (p.shutters > 0.5f && windows) {
            mb.set_material(1, 2); // тёмная доска
            const glm::vec2 left[4] = {{op.u0 - 0.50f, op.v0},
                                       {op.u0 - 0.10f, op.v0},
                                       {op.u0 - 0.10f, op.v1},
                                       {op.u0 - 0.50f, op.v1}};
            const glm::vec2 right[4] = {{op.u1 + 0.10f, op.v0},
                                        {op.u1 + 0.50f, op.v0},
                                        {op.u1 + 0.50f, op.v1},
                                        {op.u1 + 0.10f, op.v1}};
            const glm::vec2 sill[4] = {{op.u0 - 0.08f, op.v0 - 0.05f},
                                       {op.u1 + 0.08f, op.v0 - 0.05f},
                                       {op.u1 + 0.08f, op.v0},
                                       {op.u0 - 0.08f, op.v0}};
            push_wall_slab(mb, mesh, e.id, a, dir, face_n, half, 0.05f, left,
                           p.tex_deg);
            push_wall_slab(mb, mesh, e.id, a, dir, face_n, half, 0.05f, right,
                           p.tex_deg);
            push_wall_slab(mb, mesh, e.id, a, dir, face_n, half, 0.09f, sill,
                           p.tex_deg);
            mb.set_material(-1, -1);
        }
        if (p.porch > 0.5f && doors) {
            mb.set_material(3, 1); // камень среднего тона, как завалинка
            const glm::vec3 p1 = a + dir * (op.u0 - 0.2f) + face_n * half;
            const glm::vec3 p2 = a + dir * (op.u1 + 0.2f) + face_n * half;
            const glm::vec3 out = face_n * 0.55f;
            const std::vector<glm::vec3> step_loop = {p1, p2, p2 + out, p1 + out};
            const std::vector<std::uint32_t> two = {0, 1, 2, 0, 2, 3};
            push_prism(mb, step_loop, two, glm::vec3{0.0f, 0.15f, 0.0f}, p.tex_deg,
                       mesh, e.id);
            mb.set_material(-1, -1);
        }
    }
}


} // namespace

/// ПОДТЁКИ И ТРЕЩИНЫ ИЗНОСА (EXTERIOR_CATALOG.md: потёки — под
/// подоконниками и кромками; трещины ломаными по штукатурке и кладке).
/// Подтёк — узкая плашка элементного материала в ВЫВЕТРЕННОМ тоне вниз от
/// нижних углов проёма; трещина — зигзаг из тонких тёмных сегментов от
/// верхней кромки. Всё по хэшу: сборка детерминирована.
static void build_weathering(const Element& e, const ElementParams& p,
                             const glm::vec3& a, const glm::vec3& dir, float seg_len,
                             const glm::vec3& face_n, float half,
                             std::span<const OpeningPlacement> holes,
                             MeshBuilder& mb, HouseMesh& mesh) {
    if (p.wear < 0.25f) {
        return;
    }
    mb.collider = false; // косметика: в коллайдер не идёт
    // Подтёки: ПОВЕРХ кладки и обшивки (калибровка глаз 21.08: плашка на
    // плоскости пластины пряталась ПОД кирпичами), шире и в тёмном ряду.
    const WallFill wf = fill_kind(p);
    const float streak_out =
        (wf == WallFill::Brick || wf == WallFill::Block) ? half + 0.09f
                                                         : half + 0.012f;
    // Подтёки — только на камне и штукатурке: на досках обшивки они легли
    // «узором обоев» (калибровка №2), дерево мокнет иначе.
    const bool streaks_ok =
        p.clad < 0.5f && wf != WallFill::Logs;
    mb.set_material(-1, 2); // элементный материал, тёмный ряд
    for (const OpeningPlacement& op : streaks_ok
                                          ? holes
                                          : std::span<const OpeningPlacement>{}) {
        for (const float ux : {op.u0 + 0.02f, op.u1 - 0.11f}) {
            const float len =
                0.5f + 1.2f * p.wear
                * course_jitter(static_cast<int>(ux * 13), static_cast<int>(op.v0 * 7));
            const float v1 = op.v0 - 0.02f;
            const float v0 = std::max(0.02f, v1 - len);
            if (v1 - v0 < 0.08f) {
                continue;
            }
            const glm::vec2 q[4] = {{ux, v0}, {ux + 0.09f, v0},
                                    {ux + 0.09f, v1}, {ux, v1}};
            push_wall_slab(mb, mesh, e.id, a, dir, face_n, streak_out, 0.006f, q,
                           p.tex_deg);
        }
    }
    // Трещины: две-четыре на пролёт при wear>=0.35, ПОВЕРХ кладки (под
    // кирпичами их было видно только в швах — «светлые царапины»).
    if (p.wear >= 0.35f) {
        const int n_cracks =
            2 + static_cast<int>(course_jitter(static_cast<int>(seg_len * 5), 3)
                                 * 3.0f * p.wear);
        mb.set_material(-1, 2); // тёмный ряд элементного материала
        for (int c = 0; c < n_cracks; ++c) {
            float u = seg_len * (0.2f + 0.6f * course_jitter(c * 17 + 5,
                                                             static_cast<int>(seg_len)));
            // Трещина не живёт в воздухе проёма: старт над дырой уводится в
            // ближайший простенок.
            for (const OpeningPlacement& op : holes) {
                if (u > op.u0 - 0.12f && u < op.u1 + 0.12f) {
                    u = (u - op.u0 < op.u1 - u) ? std::max(0.06f, op.u0 - 0.18f)
                                                : std::min(seg_len - 0.06f,
                                                           op.u1 + 0.18f);
                }
            }
            float v_top = p.height - 0.03f;
            // Короткие частые изломы, к концу тоньше — длинная прямая читалась
            // царапиной поверх текстуры (калибровка глаз 21.08).
            const int segs = 5 + static_cast<int>(4.0f * p.wear);
            const float fall = (p.height * (0.3f + 0.3f * p.wear))
                             / static_cast<float>(segs);
            for (int k = 0; k < segs; ++k) {
                const float t = static_cast<float>(k) / static_cast<float>(segs);
                const float wgt = 0.018f * (1.0f - t) + 0.007f * t;
                const float du =
                    (course_jitter(c * 29 + k, 11) - 0.5f) * 0.12f;
                const float v_lo = v_top - fall;
                const glm::vec2 q[4] = {{u, v_lo}, {u + wgt, v_lo},
                                        {u + du + wgt, v_top}, {u + du, v_top}};
                push_wall_slab(mb, mesh, e.id, a, dir, face_n, streak_out, 0.004f,
                               q, p.tex_deg);
                u += du;
                v_top = v_lo;
            }
        }
    }
    mb.set_material(-1, -1);
    mb.collider = true;
}

void build_chain_surface(const Element& e, const ElementParams& p, std::span<const glm::vec3> pts,
                         MeshBuilder& mb, HouseMesh& mesh) {
    if (p.height <= HOUSE_GEOM_EPS) {
        mesh.findings.push_back(
            {e.id, MeshIssue::ChainNeedsHeight, 0.0f, "цепочка без height: стены нет"});
        return;
    }
    const float half = std::max(p.thickness, HOUSE_GEOM_EPS) * 0.5f;
    const bool doors = static_cast<int>(p.doors) > 0;
    const bool windows = !doors && static_cast<int>(p.windows) > 0;
    const WallFill fill = fill_kind(p);
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        const glm::vec3& a = pts[i];
        const glm::vec3& b = pts[i + 1];
        const glm::vec3 d = b - a;
        const float seg_len = std::sqrt(d.x * d.x + d.z * d.z);
        if (seg_len < HOUSE_GEOM_EPS) {
            mesh.findings.push_back({e.id, MeshIssue::ChainSegmentVertical,
                                     static_cast<float>(i),
                                     "отрезок цепочки вертикален: выдавливать некуда"});
            continue;
        }
        // ЛИЦО ОТРЕЗКА — по правилу правой руки от порядка вершин и вертикали.
        // КАЖДЫЙ ОТРЕЗОК — САМОСТОЯТЕЛЬНОЕ ТЕЛО: в углу стоит столб, стены
        // упираются в его ось, нахлёст — норма (§7.0).
        glm::vec3 face_n = glm::normalize(glm::cross(d, glm::vec3{0.0f, 1.0f, 0.0f}));
        if (e.facing_flipped) {
            face_n = -face_n;
        }
        const glm::vec3 dir = glm::vec3{d.x, 0.0f, d.z} / seg_len;
        // ОДНА раскладка на пролёт; findings — только у первого пролёта,
        // иначе трёхзвенная цепочка говорила бы каждую находку трижды.
        const WallLayout lay = lay_out_span(e, p, seg_len, i == 0 ? &mesh : nullptr);
        const std::span<const OpeningPlacement> holes =
            (doors || windows) ? std::span<const OpeningPlacement>(lay.openings)
                               : std::span<const OpeningPlacement>{};
        build_wall_plate(e, p, a, dir, seg_len, face_n, half, holes, windows, mb,
                         mesh);
        if (fill == WallFill::Logs) {
            build_log_courses(e, p, a, dir, seg_len, face_n, half, holes, mb, mesh);
        }
        if (p.clad > 0.5f || fill == WallFill::Brick || fill == WallFill::Block
            || fill == WallFill::Logs) {
            build_cladding(e, p, a, dir, seg_len, face_n, half, lay, mb, mesh);
        }
        if (p.plinth > 0.5f) {
            build_plinth_belt(e, p, a, dir, seg_len, face_n, half, holes, doors, mb,
                              mesh);
        }
        if (p.logends > 0.5f) {
            build_log_ends(e, p, a, dir, seg_len, face_n, mb, mesh);
        }
        if ((p.shutters > 0.5f && windows) || (p.porch > 0.5f && doors)) {
            build_opening_trim(e, p, a, dir, face_n, half, holes, doors, windows, mb,
                               mesh);
        }
        build_weathering(e, p, a, dir, seg_len, face_n, half, holes, mb, mesh);
    }
}


} // namespace dfn::world
