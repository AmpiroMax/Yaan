/*
Created: 09:08:2026 - 16:45:00
Last updated: 13:08:2026 - 18:59:13
Module: engine/world
File: engine/world/sources/WorldgenCarve.cpp

Responsibility:
- Carve SDF implementation: box cross-section corridors along polylines and
  rectangular chambers, unioned, plus the per-column vertical range the voxel
  builder needs to widen its active band.

Key items:
- carve_distance, carve_column_range, has_carves.

Dependencies:
- Uses: WorldgenCarve.h.
- Used by: dfn_world.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The cross-section is a BOX in (lateral, vertical) space measured against the
  segment's floor line: flat floor to stand on, flat ceiling overhead. The
  corridor floor follows the polyline's y, so a climbing segment is a ramp.
- Deterministic pure geometry.
*/
/*
UPD:
- 09:08:2026 - 16:45:00: Created — carve SDF for the crag tunnel and barrow.
- 09:08:2026 - 16:47:51: Created — carve distance fields and column ranges.
- 09:08:2026 - 17:36:42: §6.2: mouth walk (first station whose ceiling is under terrain) and derived-corridor overloads.
- 09:08:2026 - 21:37:57: NEW enclosure_darkness() — LANDSCAPE §6.3 authored darkness as the RULE, replacing the app-side stand-in that measured depth below the local surface (which calls a deep valley floor a cave). Both halves of design's rule are evaluated: ENCLOSED (inside carved air AND rock overhead) and EARNED (>= DARKNESS_DEPTH_MIN walked ALONG the corridor from the nearest mouth, not straight-line through rock — a switchback is dark because you walked it). Ramps over DARKNESS_FALLOFF_MIN. Measured seed 1: valley floor 0.000, barrow mouth 0.000, 20 m in 0.375, chamber 1.000, solid rock (not a place) 0.000.
- 10:08:2026 - 02:29:54: open_daylight_portals() implementation (STEP 1 m walk, 0.25 m floor clearance, 60 m cap). Seed 1: crag tunnel exit extended ~15 m to (761.3, 65.4, 254.5), floor +0.44 m over terrain — the exit portal exists again.
- 10:08:2026 - 21:27:14: Recorded (no behaviour change) that
  DARKNESS_FALLOFF_MIN is read as the whole ramp width while
  DARKNESS_FALLOFF_MAX has zero references anywhere. One of three orphaned
  range halves found in the constants census; needs a design ruling, not a
  local edit.
- 13:08:2026 - 16:45:00: МЕРЦАНИЕ В ПОДЗЕМЕЛЬЕ ПОЧИНЕНО (жалоба пользователя «темнеет в глазах, потом мигает»). Точка запроса поднимается на CARVE_QUERY_LIFT_M = VOXEL_SIZE перед ВСЕМИ проверками вхождения: запрос задаётся о низе капсулы, то есть о точке НА нарисованном полу, а нарисованный пол — реконструкция на решётке вокселя и стоит от аналитической плоскости в пределах вокселя, поэтому точный `>= 0` спрашивал о геометрии тоньше, чем геометрия умеет отвечать. Замер: 13 переключений ambient_darkness 0.000↔1.000 ЗА ОДИН КАДР на проход → 1; доля подземных тиков «по ту сторону границы» 3.6 % → 0.0 %. Подъём, а НЕ допуск на расстояние: изотропный допуск убрал мерцание и зачернил ОТКРЫТУЮ подходную выемку на 51 кадр (поймано дневной рукой). Вторая копия того же вопроса — свои ворота в corridor_path_from_mouth — держала 7 переключений из 13 после починки только первых. Новый enclosure_trace() — те же промежуточные величины ОДНОГО вычисления, чтобы прибор не мог разойтись с боевым кодом. Подробности и приёмка: docs/FINDING_DUNGEON_DARK.md.
- 13:08:2026 - 17:12:00: ВТОРАЯ ПОЛОВИНА ТОЙ ЖЕ ЖАЛОБЫ: путь до дневного света меряется от ПОРТАЛА — станции, где поднятый пол коридора пересекает поверхность, — а не от carve_mouth(), который отвечает на другой вопрос («где сомкнулся ПОТОЛОК») и у свитчбэка Равенскара стоит в 45 м ВНУТРИ тоннеля. Из-за этого на входе была чернота с первого шага вместо склона на DARKNESS_DEPTH_MIN, а на сотом метре — полный дневной свет под 12 м скалы (20.5 % подземных кадров); у коридора с двумя дневными концами признавался ровно один. Считаются ВСЕ пересечения, берётся ближайшее; carve_mouth намеренно не тронут (P4 выводит из него метки входов). Плюс берётся наиболее замкнутая из исходной и поднятой точки, иначе подъём выталкивал запрос на высоте глаз через свод 2.6-метрового прохода Бэкбарроу. Живьём: переключений 0↔1 за кадр 13 → 0, наибольшая покадровая ступень 1.000 → 0.0036, день под землёй 23.7 % → 7.4 % (и все они ближе 17 м пути от портала). Регрессия — tests/core/VoxelTests.cpp, рука до правки падает по всем трём проверкам.
- 13:08:2026 - 18:40:00: ТРЕТИЙ ДЕФЕКТ ТОГО ЖЕ ПРАВИЛА (жалоба пользователя «темнеет снаружи, когда ещё крыши нет никакой»): ворота замкнутости спрашивали про ТОЧКУ ЗАПРОСА, а надо про КРЫШУ. Врезанный в склон коридор — сначала открытая ТРАНШЕЯ: пол уже под поверхностью холма, потолок ещё на свету. Новый carve_roof_over(); тем же предикатом теперь определяется и портал в измерителе пути, иначе два определения «замкнуто» разъедутся (этот файл сегодня дважды платил ровно за это). Замер: из 2730 тиков с потолком ВЫШЕ рельефа 1673 несли тьму, худший — полная чернота при потолке на 1.31 м в воздухе; после правки 0 из 5511. Верный предикат всё это время лежал в carve_mouth этажом выше.
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
*/

#include "engine/world/sources/WorldgenCarve.h"

#include "engine/core/config/sources/Constants.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace dfn::world {

namespace {

constexpr float FAR_AWAY = 1e9f;

/// HOW FAR A QUERY POINT RESTING ON THE FLOOR IS LIFTED before this file is
/// asked which side of a carve boundary it is on. This is the fix for
/// «темнеет, потом мигает», and it is the width of the question the
/// representation can answer rather than a fudge.
///
/// A player's position is the bottom of the capsule (IPhysics::
/// character_position — "the capsule bottom point"), i.e. a point ON THE FLOOR.
/// The floor the feet rest on is the DRAWN one: a surface-nets reconstruction
/// of this carve on a VOXEL_SIZE lattice. The floor in this file is the
/// ANALYTIC plane the carve was cut with. The two agree only to the lattice's
/// reconstruction error, so the SDF at the feet is a number hovering around
/// zero, and an exact `>= 0` test asks the geometry something finer than the
/// geometry can answer. Whichever side of zero the last bits land on, the whole
/// world is declared "cave" or "open daylight" for that frame.
///
/// MEASURED, live, walking the crag tunnel (8031 ticks, DFN_DARK_TRACE): with
/// the feet more than 1 m under the terrain surface, |carve_distance| is
/// 0.0302 m median, 0.4011 m at p90, 0.6904 m at worst — never as much as one
/// voxel, exactly as the reconstruction argument predicts — and 53.2 % of those
/// ticks sit within 5 cm of the boundary. 3.6 % land on the wrong side, and
/// each is a frame in which ambient light returns at FULL strength 18 m inside
/// a mountain: `ambient_darkness` stepped 0.000 <-> 1.000 in a SINGLE frame 13
/// times in one walk, at |carve_distance| between 0.0002 and 0.0108 m.
///
/// One voxel is how far the drawn surface can stand from the analytic one — the
/// same derivation and the same approved constant as render's
/// PATH_SURFACE_LIFT_M (Materials.h), which lifts drawn path quads for exactly
/// this reason. Not a new number and not a tuned one.
///
/// IT IS A LIFT AND NOT A TOLERANCE ON THE DISTANCE, and that distinction is
/// measured, not stylistic. The first version of this fix widened the carve by
/// one voxel in EVERY direction (`carve_distance >= tol`). It removed the
/// underground flicker and then produced a NEW defect on open ground: walking
/// the tunnel's outer approach cutting, where the feet read exactly at terrain
/// height and the corridor wall is 0.34…0.99 m away through air, the place was
/// declared enclosed and the picture went FULL BLACK for 51 frames in broad
/// daylight (arm `out_fixed`, 6185 frames, against 0.000000 darkness in the
/// control arm on the same route). Lifting the point instead widens nothing:
/// under a roof the lifted point is in the corridor's air, and in the open it
/// is in the sky, which is where the second gate then rejects it.
///
/// BOTH CONTAINMENT TESTS TAKE THE LIFTED POINT, and the second one is why the
/// first fix was not enough (Rule 32): `corridor_path_from_mouth` carries its
/// own `corridor_distance(...) > 0` gate, so a point one centimetre "outside"
/// its own floor was dropped from its own corridor and fell through to the
/// unmeasurable-mouth fallback — which answers with a straight line to a
/// DIFFERENT dungeon's mouth. Measured after fixing only the first test: 7 of
/// the 13 full-scale flips survived, every one of them this path.
///
/// The third copy of the question is in a GREEN TEST: tests/core/VoxelTests.cpp
/// asks the same containment at `p.y + 1.7` and `p.y - 0.5`, stepping away from
/// the plane the shipping query stands on — which is why no test ever saw this.
constexpr float CARVE_QUERY_LIFT_M = static_cast<float>(config::VOXEL_SIZE);

/// THE ROOF OVER A POINT INSIDE A CARVE: the top of the carved volume that
/// contains it, or -FAR_AWAY when no carve does. This is what "rock overhead"
/// has to be asked about, and asking it about the POINT instead was the second
/// half of «темнеет... снаружи, когда ещё крыши нет никакой».
///
/// A corridor cut into a hillside starts as an open TRENCH: its floor is below
/// the hill's surface while its ceiling is still out in the daylight. The
/// player's feet are then "under the terrain" and the sky is directly overhead.
/// Measured on a live walk before this fix: of 2730 ticks whose carve ceiling
/// stood ABOVE the terrain — no roof at all — 1673 (61.3 %) carried non-zero
/// darkness, and the worst was FULL BLACK where the ceiling stood 1.31 m above
/// the hillside.
///
/// The right predicate was already in this file and already documented, one
/// function up: `carve_mouth` finds "the first station whose CEILING is under
/// the terrain" and its comment names what comes before it — "the open approach
/// cutting". The mouth knew; the darkness gate did not ask.
[[nodiscard]] inline float carve_roof_over(const TestbedLayout& layout,
                                           std::span<const CarveCorridor> extra,
                                           glm::vec3 q) {
    float roof = -FAR_AWAY;
    const auto scan = [&](const CarveCorridor& c) {
        for (int i = 0; i + 1 < c.point_count; ++i) {
            const glm::vec3 a = c.points[i];
            const glm::vec3 b = c.points[i + 1];
            const glm::vec3 ab = b - a;
            const float len2 = glm::dot(ab, ab);
            if (len2 < 1e-6f) {
                continue;
            }
            const float t = std::clamp(glm::dot(q - a, ab) / len2, 0.0f, 1.0f);
            const glm::vec3 cp = a + ab * t;
            if (std::hypot(q.x - cp.x, q.z - cp.z) > c.half_width) {
                continue;
            }
            const float dy = q.y - cp.y;
            if (dy < 0.0f || dy > c.height) {
                continue;
            }
            roof = std::max(roof, cp.y + c.height);
        }
    };
    scan(layout.carves.crag_tunnel);
    scan(layout.carves.barrow_passage);
    scan(layout.carves.lakeshore_adit);
    for (const CarveCorridor& c : extra) {
        scan(c);
    }
    const CarveChamber& ch = layout.carves.barrow_chamber;
    if (ch.half_extent.x > 0.0f && std::fabs(q.x - ch.center.x) <= ch.half_extent.x
        && std::fabs(q.z - ch.center.z) <= ch.half_extent.z && q.y >= ch.center.y
        && q.y <= ch.center.y + ch.half_extent.y) {
        roof = std::max(roof, ch.center.y + ch.half_extent.y);
    }
    return roof;
}

/// The query point as this file must ask about it: lifted off the floor it
/// rests on. ONE definition, used by every containment test here, so the two
/// gates cannot drift apart again.
inline glm::vec3 lifted(glm::vec3 world) {
    return {world.x, world.y + CARVE_QUERY_LIFT_M, world.z};
}

/// Signed distance to one corridor segment's box cross-section. Negative
/// inside. `a`/`b` carry the FLOOR level; the box rises `height` above it.
float segment_distance(glm::vec3 a, glm::vec3 b, float half_width, float height,
                       glm::vec3 p) {
    const glm::vec3 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    const float t = len2 > 0.0f ? std::clamp(glm::dot(p - a, ab) / len2, 0.0f, 1.0f) : 0.0f;
    const glm::vec3 c = a + ab * t;
    // Lateral distance is measured horizontally so the corridor keeps its
    // width on a ramp; vertical is measured from the floor line.
    const float lateral = std::hypot(p.x - c.x, p.z - c.z);
    const float dy = p.y - c.y;
    // Box SDF in (lateral, vertical): outside distances combine, inside takes
    // the nearest face.
    const float dx = lateral - half_width;
    const float dv = std::max(-dy, dy - height);
    if (dx <= 0.0f && dv <= 0.0f) {
        return std::max(dx, dv); // inside: negative
    }
    const float ox = std::max(dx, 0.0f);
    const float ov = std::max(dv, 0.0f);
    return std::hypot(ox, ov);
}

float corridor_distance(const CarveCorridor& corridor, glm::vec3 p) {
    float best = FAR_AWAY;
    for (int i = 0; i + 1 < corridor.point_count; ++i) {
        best = std::min(best, segment_distance(corridor.points[i], corridor.points[i + 1],
                                               corridor.half_width, corridor.height, p));
    }
    return best;
}

float chamber_distance(const CarveChamber& chamber, glm::vec3 p) {
    if (chamber.half_extent.x <= 0.0f) {
        return FAR_AWAY;
    }
    const float dx = std::fabs(p.x - chamber.center.x) - chamber.half_extent.x;
    const float dz = std::fabs(p.z - chamber.center.z) - chamber.half_extent.z;
    const float dy_below = chamber.center.y - p.y;               // below the floor
    const float dy_above = p.y - (chamber.center.y + chamber.half_extent.y);
    const float dv = std::max(dy_below, dy_above);
    if (dx <= 0.0f && dz <= 0.0f && dv <= 0.0f) {
        return std::max(dx, std::max(dz, dv));
    }
    return std::hypot(std::hypot(std::max(dx, 0.0f), std::max(dz, 0.0f)),
                      std::max(dv, 0.0f));
}

/// Vertical span of a corridor over a column, or an empty range.
std::pair<float, float> corridor_column_range(const CarveCorridor& corridor,
                                              glm::vec2 xz) {
    float lo = FAR_AWAY;
    float hi = -FAR_AWAY;
    const float reach = corridor.half_width + 1.0f;
    for (int i = 0; i + 1 < corridor.point_count; ++i) {
        const glm::vec3 a = corridor.points[i];
        const glm::vec3 b = corridor.points[i + 1];
        // Horizontal distance from the column to the segment's ground track.
        const glm::vec2 a2{a.x, a.z};
        const glm::vec2 b2{b.x, b.z};
        const glm::vec2 ab = b2 - a2;
        const float len2 = glm::dot(ab, ab);
        const float t = len2 > 0.0f ? std::clamp(glm::dot(xz - a2, ab) / len2, 0.0f, 1.0f)
                                    : 0.0f;
        if (glm::length(xz - (a2 + ab * t)) > reach) {
            continue;
        }
        const float floor_y = a.y + (b.y - a.y) * t;
        lo = std::min(lo, floor_y - 1.0f);
        hi = std::max(hi, floor_y + corridor.height + 1.0f);
    }
    return {lo, hi};
}

} // namespace

bool has_carves(const TestbedLayout& layout) {
    return layout.carves.crag_tunnel.point_count > 1
        || layout.carves.barrow_passage.point_count > 1
        || layout.carves.barrow_chamber.half_extent.x > 0.0f;
}

float carve_distance(const TestbedLayout& layout, glm::vec3 world) {
    float d = corridor_distance(layout.carves.crag_tunnel, world);
    d = std::min(d, corridor_distance(layout.carves.barrow_passage, world));
    d = std::min(d, chamber_distance(layout.carves.barrow_chamber, world));
    return d;
}

std::optional<CarveMouth> carve_mouth(const CarveCorridor& corridor,
                                      const GroundSampler& ground) {
    // Walk from the outer end inward at fine steps; the mouth is the first
    // station whose CEILING is under the terrain. Everything before it is the
    // open approach cutting, which is why placing a marker at the polyline's
    // start puts it metres short of the actual opening.
    for (int i = 0; i + 1 < corridor.point_count; ++i) {
        const glm::vec3 a = corridor.points[i];
        const glm::vec3 b = corridor.points[i + 1];
        const float len = glm::length(b - a);
        const int steps = std::max(1, static_cast<int>(len / 0.5f));
        for (int s = 0; s <= steps; ++s) {
            const glm::vec3 p = a + (b - a) * (static_cast<float>(s) / steps);
            if (p.y + corridor.height < ground({p.x, p.z})) {
                const glm::vec2 heading = glm::normalize(glm::vec2{b.x - a.x, b.z - a.z});
                return CarveMouth{p, -heading}; // face back out of the hill
            }
        }
    }
    return std::nullopt;
}

void open_daylight_portals(TestbedLayout& layout, const GroundSampler& ground) {
    // Derivation parameters (worldgen-internal, like the scatter margins):
    // STEP is the walk resolution, CLEARANCE how far below the corridor floor
    // the terrain must fall to count as open air (a floor grazing the grass
    // is not a portal), CAP the defensive bound — a corridor that cannot
    // reach daylight inside it is left alone so the acceptance walk stays red
    // and loud instead of silently rerouted across half the mountain.
    constexpr float STEP = 1.0f;
    constexpr float CLEARANCE = 0.25f;
    constexpr float CAP = 60.0f;
    for (CarveCorridor* c : {&layout.carves.crag_tunnel, &layout.carves.barrow_passage,
                             &layout.carves.lakeshore_adit}) {
        if (!c->daylight_portals || c->point_count < 2) {
            continue;
        }
        for (const int end : {0, c->point_count - 1}) {
            glm::vec3& p = c->points[end];
            const glm::vec3 inner = c->points[end == 0 ? 1 : c->point_count - 2];
            glm::vec3 dir = p - inner; // outward, grade preserved
            const float horiz = std::sqrt(dir.x * dir.x + dir.z * dir.z);
            if (horiz < 1e-3f) {
                continue;
            }
            dir /= horiz; // advance per horizontal meter
            float walked = 0.0f;
            glm::vec3 candidate = p;
            while (walked < CAP && ground({candidate.x, candidate.z}) > candidate.y - CLEARANCE) {
                candidate += dir * STEP;
                walked += STEP;
            }
            if (walked < CAP) {
                p = candidate; // reached open air: this is the daylight end
            }
        }
    }
}

std::optional<CarveMouth> site_carve_mouth(const TestbedLayout& layout, int site_index,
                                           const GroundSampler& ground) {
    if (site_index < 0) {
        return std::nullopt;
    }
    if (site_index == layout.carves.barrow_site_index) {
        return carve_mouth(layout.carves.barrow_passage, ground);
    }
    if (site_index == layout.carves.lakeshore_site_index) {
        return carve_mouth(layout.carves.lakeshore_adit, ground);
    }
    return std::nullopt;
}

float carve_distance(const TestbedLayout& layout, std::span<const CarveCorridor> extra,
                     glm::vec3 world) {
    float d = carve_distance(layout, world);
    for (const CarveCorridor& c : extra) {
        d = std::min(d, corridor_distance(c, world));
    }
    return d;
}

std::pair<float, float> carve_column_range(const TestbedLayout& layout,
                                           std::span<const CarveCorridor> extra,
                                           glm::vec2 world_xz) {
    auto [lo, hi] = carve_column_range(layout, world_xz);
    for (const CarveCorridor& c : extra) {
        const auto [elo, ehi] = corridor_column_range(c, world_xz);
        if (elo <= ehi) {
            lo = lo > hi ? elo : std::min(lo, elo);
            hi = hi < lo ? ehi : std::max(hi, ehi);
        }
    }
    if (lo > hi) {
        return {1.0f, -1.0f};
    }
    return {lo, hi};
}

std::pair<float, float> carve_column_range(const TestbedLayout& layout,
                                           glm::vec2 world_xz) {
    auto [lo, hi] = corridor_column_range(layout.carves.crag_tunnel, world_xz);
    const auto [plo, phi] = corridor_column_range(layout.carves.barrow_passage, world_xz);
    lo = std::min(lo, plo);
    hi = std::max(hi, phi);
    const CarveChamber& ch = layout.carves.barrow_chamber;
    if (ch.half_extent.x > 0.0f
        && std::fabs(world_xz.x - ch.center.x) <= ch.half_extent.x + 1.0f
        && std::fabs(world_xz.y - ch.center.z) <= ch.half_extent.z + 1.0f) {
        lo = std::min(lo, ch.center.y - 1.0f);
        hi = std::max(hi, ch.center.y + ch.half_extent.y + 1.0f);
    }
    if (lo > hi) {
        return {1.0f, -1.0f}; // empty
    }
    return {lo, hi};
}

/// Walk distance along a corridor from the NEAREST place it opens to daylight
/// to the point on its polyline nearest `world`. Returns a large value when the
/// point is not in this corridor at all.
namespace {

float corridor_path_from_mouth(const CarveCorridor& c, const GroundSampler& ground,
                               glm::vec3 world) {
    if (c.point_count < 2) {
        return 1e9f;
    }
    if (std::min(corridor_distance(c, world), corridor_distance(c, lifted(world))) > 0.0f) {
        return 1e9f; // not inside this corridor (see CARVE_QUERY_LIFT_M)
    }
    // Arc length to the projection of `q`, measured from point 0.
    const auto arc_to = [&](glm::vec3 q) {
        float best = 0.0f;
        float best_d2 = 1e30f;
        float run = 0.0f;
        for (int i = 0; i + 1 < c.point_count; ++i) {
            const glm::vec3 a2 = c.points[i];
            const glm::vec3 b2 = c.points[i + 1];
            const glm::vec3 ab = b2 - a2;
            const float len2 = glm::dot(ab, ab);
            const float u = len2 > 1e-6f ? std::clamp(glm::dot(q - a2, ab) / len2, 0.0f, 1.0f) : 0.0f;
            const glm::vec3 proj = a2 + ab * u;
            const glm::vec3 d = q - proj;
            const float d2 = glm::dot(d, d);
            if (d2 < best_d2) {
                best_d2 = d2;
                best = run + std::sqrt(len2) * u;
            }
            run += std::sqrt(len2);
        }
        return best;
    };

    // WHERE THE DAYLIGHT IS, and this is the second defect of «темнеет в
    // глазах, потом мигает» — the half that is not the flicker.
    //
    // What stood here measured the walk from carve_mouth(), which answers a
    // DIFFERENT question: the first station whose CEILING has gone under the
    // terrain, i.e. where the hill has closed over the corridor. On the
    // Ravenscar switchback that station is 109.7 m along the polyline, while a
    // player walks under rock at 63 m — so the walk was measured from a point
    // 45 m INSIDE the tunnel, unsigned. Two consequences, both measured on a
    // live walk and both exactly what the user described:
    //   - at the entrance the "walk" is already 45 m, so the picture goes
    //     PITCH BLACK on the first step instead of ramping over
    //     DARKNESS_DEPTH_MIN (25 m), which is what §6.3 promises;
    //   - walking DEEPER approaches that point, so the walk falls to zero and
    //     the tunnel returns to FULL DAYLIGHT under 12 m of rock. 20.5 % of
    //     underground frames on the fixed build were full daylight, 761 of them
    //     under more than 5 m of rock.
    // And a corridor with two daylight ends (`daylight_portals`) had only ONE
    // of them recognised at all, so standing two metres inside the far exit
    // counted as 150 m deep, in sight of the sky.
    //
    // A PORTAL is where the corridor stops being enclosed for the player: the
    // station where the LIFTED floor crosses the terrain surface — the same
    // enclosure test the caller applies to the player, so the ramp starts
    // exactly where the darkness gate starts. Every crossing counts (both ends,
    // and any shaft the polyline pops out of), and the walk is measured from
    // the NEAREST one. carve_mouth() is deliberately left alone: P4 derives the
    // entrance MARKERS from it, and "where the roof closes" is the right answer
    // for a marker even though it is the wrong one for a walk.
    constexpr float STEP_M = 0.5f;
    float portal_arcs[64];
    int portal_count = 0;
    float run = 0.0f;
    bool prev_enclosed = false;
    bool have_prev = false;
    for (int i = 0; i + 1 < c.point_count && portal_count < 64; ++i) {
        const glm::vec3 a = c.points[i];
        const glm::vec3 b = c.points[i + 1];
        const float len = glm::length(b - a);
        const int steps = std::max(1, static_cast<int>(len / STEP_M));
        for (int s = 0; s <= steps && portal_count < 64; ++s) {
            const float u = static_cast<float>(s) / static_cast<float>(steps);
            const glm::vec3 p = a + (b - a) * u;
            // THE SAME PREDICATE AS THE GATE ABOVE, deliberately: a portal is
            // where the player stops being under the sky, so it is the station
            // where this corridor's ROOF goes under the terrain. Using the
            // floor here (as this did) put the ramp's foot 45 m out in the
            // open cutting, which is the defect the roof test above fixes; two
            // definitions of "enclosed" would have re-opened it from the side.
            const float station_roof = p.y + c.height;
            const bool enclosed = station_roof < ground({p.x, p.z});
            if (have_prev && enclosed != prev_enclosed) {
                portal_arcs[portal_count++] = run + len * u;
            }
            prev_enclosed = enclosed;
            have_prev = true;
        }
        run += len;
    }
    const float here = arc_to(world);
    if (portal_count == 0) {
        // The corridor never crosses the surface inside its own length: it
        // begins already under rock (the barrow passage does). Its own start is
        // then the deepest thing we can honestly call an entrance, which is
        // what carve_mouth reports for such a corridor.
        const auto mouth = carve_mouth(c, ground);
        if (!mouth) {
            return 1e9f; // never goes under rock: not an entrance, nothing earned
        }
        return std::fabs(here - arc_to(mouth->position));
    }
    float best = 1e9f;
    for (int i = 0; i < portal_count; ++i) {
        best = std::min(best, std::fabs(here - portal_arcs[i]));
    }
    return best;
}

} // namespace

float enclosure_darkness(const TestbedLayout& layout, std::span<const CarveCorridor> extra,
                         const GroundSampler& ground, glm::vec3 world) {
    return enclosure_trace(layout, extra, ground, world).darkness;
}

EnclosureTrace enclosure_trace(const TestbedLayout& layout,
                               std::span<const CarveCorridor> extra,
                               const GroundSampler& ground, glm::vec3 world) {
    EnclosureTrace tr;
    // Half one: ENCLOSED. Inside carved air, with rock actually overhead.
    //
    // Every test below asks about the LIFTED point (CARVE_QUERY_LIFT_M): the
    // caller's position is a point resting ON the drawn floor, and the drawn
    // floor stands within one voxel of this analytic one rather than on it.
    // Asking about the surface a body touches instead of the air it stands in
    // is what made this a coin flip, once per frame, underground.
    // THE MOST ENCLOSED OF THE TWO, so the query is robust at BOTH ends of a
    // corridor's cross-section. The lift alone answers "is the air above my
    // feet carved", which is the question for a point resting on the floor —
    // but it pushes a query taken at EYE height (1.7 m) through the ceiling of
    // the 2.6 m barrow passage, and that caller would then be told the barrow
    // is open daylight. Taking the minimum keeps the floor's answer stable
    // without inventing enclosure anywhere: outside a carve BOTH distances are
    // positive, and the second gate below still rejects on the LIFTED point, so
    // the open-ground blackout this fix's first version produced cannot return.
    const glm::vec3 probe = lifted(world);
    tr.carve_distance = std::min(carve_distance(layout, extra, world),
                                 carve_distance(layout, extra, probe));
    tr.ground_y = ground({world.x, world.z});
    tr.roof_y = carve_roof_over(layout, extra, probe);
    // THE SECOND GATE ASKS ABOUT THE ROOF, NOT ABOUT THE FEET. A corridor cut
    // into a hillside is an open TRENCH before it is a tunnel: the floor is
    // already under the hill's surface while the ceiling is still in daylight.
    // Testing the query point put the player "inside" there, and the picture
    // went dark in the open air -- the user's own words, «темнеет снаружи,
    // когда ещё крыши нет никакой». See carve_roof_over for the measurement.
    tr.open_to_sky = tr.roof_y >= tr.ground_y;
    if (tr.carve_distance >= 0.0f) {
        return tr; // not in a carve at all -- a valley floor is not a cave
    }
    if (tr.open_to_sky) {
        return tr; // no rock overhead: an open cutting, or a shaft to the sky
    }

    // Half two: EARNED. Shortest walk back to any mouth, along the corridors.
    float path = 1e9f;
    path = std::min(path, corridor_path_from_mouth(layout.carves.crag_tunnel, ground, world));
    path = std::min(path, corridor_path_from_mouth(layout.carves.barrow_passage, ground, world));
    path = std::min(path, corridor_path_from_mouth(layout.carves.lakeshore_adit, ground, world));
    for (const CarveCorridor& c : extra) {
        path = std::min(path, corridor_path_from_mouth(c, ground, world));
    }
    // (corridor_path_from_mouth lifts the point itself, so the containment test
    // there and the one above are the same test.)
    tr.path_measured = path <= 1e8f;
    if (path > 1e8f) {
        // Inside carved air but reachable from no mouth we can measure -- the
        // barrow CHAMBER is the real case: it hangs off the end of its
        // passage. Fall back to the passage's full length plus the distance
        // in, which is the walk a player actually makes.
        const CarveCorridor& psg = layout.carves.barrow_passage;
        const auto mouth = carve_mouth(psg, ground);
        if (!mouth || psg.point_count < 2) {
            tr.path_from_mouth = path;
            tr.darkness = 1.0f; // sealed and unmeasurable: dark is the safe answer
            return tr;
        }
        path = glm::length(world - mouth->position);
    }

    const float depth_min = static_cast<float>(config::DARKNESS_DEPTH_MIN);
    // ORPHANED RANGE HALF, measured 10.08.2026 and recorded here rather than in
    // a message, because this line is where the person who can fix it stands.
    // DARKNESS_FALLOFF_MIN has 4 references, all of them this one and its
    // documentation; DARKNESS_FALLOFF_MAX has ZERO, in engine/, tests/, games/
    // and tools/ alike. So a registry RANGE is being read as if it were a
    // VALUE: `falloff` is not a minimum here, it is the entire ramp width, and
    // the name says otherwise (Rule 44 — a constant no longer meaning what its
    // name says). Whoever authored the pair intended a band and got a floor.
    //
    // NOT fixed in place, deliberately: picking MAX, or the midpoint, or a
    // per-seed draw across the band are three different design answers with
    // three different looks underground, and that is design's call rather than
    // this zone's. Two siblings are in the same state and want one ruling
    // together: FORD_SPACING_MIN (0 refs, its _MAX has 8 — see
    // WorldgenHydrology.cpp) and L0_ARETE_COUNT_MAX (0 refs, its _MIN has 3 —
    // see TestbedLayout.h, which already argues at length about not reading one
    // bound of a range as the range).
    const float falloff = static_cast<float>(config::DARKNESS_FALLOFF_MIN);
    // Ramp UP TO full darkness at DARKNESS_DEPTH_MIN, so the threshold is
    // where it becomes pitch black rather than where it starts to dim.
    tr.path_from_mouth = path;
    tr.darkness = std::clamp((path - (depth_min - falloff)) / falloff, 0.0f, 1.0f);
    return tr;
}


// TORCHES ON THE WALLS OF A CARVED CORRIDOR.
//
// SPACING, HEIGHT AND INSET ARE LOOK-DEV AND WANT ROWS (Rule 14). 10 m apart
// alternating walls means the player is never more than 5 m from a flame, which
// is the torch's own useful radius in the dark (TORCH_RADIUS_DARK 4 m plus the
// carried one); 1.8 m is a sconce at head height; 0.25 m keeps the stick clear
// of the wall it hangs on. Marked as placeholders rather than smuggled in, and
// requested from the lead with the falloff numbers.
namespace {
constexpr float WALL_TORCH_SPACING_M = 10.0f;
constexpr float WALL_TORCH_HEIGHT_M = 1.8f;
constexpr float WALL_TORCH_INSET_M = 0.25f;
} // namespace

std::vector<CarveLightSite> carve_wall_lights(const TestbedLayout& layout,
                                              const GroundSampler& ground) {
    std::vector<CarveLightSite> out;
    const auto walk = [&](const CarveCorridor& c) {
        if (c.point_count < 2) {
            return;
        }
        float since = WALL_TORCH_SPACING_M; // the first enclosed station gets one
        bool left = true;
        for (int i = 0; i + 1 < c.point_count; ++i) {
            const glm::vec3 a = c.points[i];
            const glm::vec3 b = c.points[i + 1];
            const glm::vec3 ab = b - a;
            const float len = glm::length(ab);
            if (len < 1e-3f) {
                continue;
            }
            const glm::vec2 dir = glm::normalize(glm::vec2{ab.x, ab.z});
            const glm::vec2 side{-dir.y, dir.x};
            const int steps = std::max(1, static_cast<int>(len / 0.5f));
            for (int s = 0; s < steps; ++s) {
                const float u = static_cast<float>(s) / static_cast<float>(steps);
                const glm::vec3 p = a + ab * u;
                // THE SAME PREDICATE AS THE DARKNESS GATE: a torch belongs where
                // the roof has gone under the terrain, never in the open cutting.
                if (p.y + c.height >= ground({p.x, p.z})) {
                    since = WALL_TORCH_SPACING_M; // re-arm: light the first step in
                    continue;
                }
                since += len / static_cast<float>(steps);
                if (since < WALL_TORCH_SPACING_M) {
                    continue;
                }
                since = 0.0f;
                const float lat = c.half_width - WALL_TORCH_INSET_M;
                const glm::vec2 at = glm::vec2{p.x, p.z} + side * (left ? lat : -lat);
                CarveLightSite site;
                site.position = {at.x, p.y + WALL_TORCH_HEIGHT_M, at.y};
                // Faces across the corridor, i.e. away from its own wall.
                const glm::vec2 face = left ? -side : side;
                site.yaw = std::atan2(face.x, face.y);
                out.push_back(site);
                left = !left;
            }
        }
    };
    walk(layout.carves.crag_tunnel);
    walk(layout.carves.barrow_passage);
    walk(layout.carves.lakeshore_adit);
    return out;
}

} // namespace dfn::world
