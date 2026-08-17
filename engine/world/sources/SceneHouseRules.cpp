/*
Created: 17:08:2026 - 16:30:25
Last updated: 17:08:2026 - 17:01:54
Module: engine/world
File: engine/world/sources/SceneHouseRules.cpp

Responsibility:
- The building rules themselves (see SceneHouseRules.h). Reads every working
  property out of the REGISTRY NAME, so it judges parts it cannot forge.

Key items:
- VJoint / HJoint: a joint as the rules see it, vertical or lying down.
- wall_ends(): the §5 pair, moved from Scene.cpp, plus the two-post rule.
- deck_and_roof(): the new §8 rules — decks and slopes on horizontal joints.
- capacity(): one panel per facet, N panels on an N-gon, any number on a drum.

Dependencies:
- Uses: Scene.h, glm, std.
- Used by: Scene.cpp (check_scene), tests/core/SceneHouseRuleTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE SEATING CONVENTION IS ONE (§3.2): a panel meets a joint with its
  MID-THICKNESS PLANE ON THE JOINT'S AXIS. Vertical or horizontal makes no
  difference and must not: "rests on top of the joist" and "centred on the
  post" would be two conventions, and the first scene that mixed them would
  own a hairline seam nobody could name.
- A JOINT WITH NOTHING ON IT IS NEVER A FINDING («столбы можно ставить без
  стен, без ограничений»).
*/
/*
UPD:
- 17:08:2026 - 16:30:25: Создан: JointSeat/JointAngle перенесены из Scene.cpp дословно,
  рядом встали WallTwoJoints, JointCapacity, DeckOnJoints, RoofSeat.
- 17:08:2026 - 17:01:54: ШОВ «кого судим» (§8.3): правила постройки судят ЧЛЕНА
  ПОСТРОЙКИ. Там, где группа выключает вопрос к земле в Scene.cpp, включаются
  шарниры — и наоборот: одиночный образец витрины судится землёй, а не
  коньковым прогоном, которого под ним неоткуда взяться (9 находок витрины
  были все из них). И честный текст промаха: «промах 0.000 м» означало «мерить
  было не по чему» и посылало читателя искать миллиметр, которого нет.
*/

#include "engine/world/sources/SceneHouseRules.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <glm/geometric.hpp>
#include <map>
#include <string>
#include <vector>

namespace dfn::world {
namespace {

constexpr float PI = 3.14159265359f;

/// The kit's panel thickness, used as the STAND-IN when a part's slab depth
/// is not in its name (roof coverings: straw is 0.35 deep, tile 0.09, and the
/// name spells run/depth/rise, never the covering). One grid unit is the
/// thickness every wall panel in the shelf actually has, so a slope judged
/// against it is judged against the panel the facet rule was derived for.
constexpr float NOMINAL_PANEL_T_M = 0.25f;

// ---------------------------------------------------------------------------
// NAMES. Every working property comes from here (HOUSES.md §4): the judge
// reads the same contract the composer does and depends on no forge.
// ---------------------------------------------------------------------------

[[nodiscard]] bool int_after(const std::string& s, std::size_t at, int& out) {
    const char* first = s.data() + at;
    const char* last = s.data() + s.size();
    const auto [ptr, ec] = std::from_chars(first, last, out);
    return ec == std::errc{};
}

/// Facet token of a connector name: n4 / n6 / n8 / nr. Returns false when the
/// name carries none, which is how a non-connector is told apart.
[[nodiscard]] bool facets_after(const std::string& name, std::size_t from,
                                int& facets) {
    if (name.find("-n4-", from) != std::string::npos) { facets = 4; return true; }
    if (name.find("-n6-", from) != std::string::npos) { facets = 6; return true; }
    if (name.find("-n8-", from) != std::string::npos) { facets = 8; return true; }
    if (name.find("-nr-", from) != std::string::npos) { facets = 0; return true; }
    return false;
}

/// joint-<mat>-d<cm>-n<N>-h<u>[-cap]-w<NN>: an upright post.
[[nodiscard]] bool parse_post(const std::string& name, float& r_in, int& facets,
                              float& height_m) {
    if (name.rfind("joint-", 0) != 0) return false;
    const std::size_t d_at = name.find("-d");
    if (d_at == std::string::npos) return false;
    int cm = 0;
    if (!int_after(name, d_at + 2, cm) || cm <= 0) return false;
    r_in = static_cast<float>(cm) * 0.005f;
    if (!facets_after(name, d_at, facets)) return false;
    const std::size_t h_at = name.find("-h", d_at);
    int hu = 0;
    height_m = (h_at != std::string::npos && int_after(name, h_at + 2, hu) && hu > 0)
                 ? static_cast<float>(hu) * 0.25f
                 : 0.0f;
    return true;
}

/// sleeper-<mat>-d<cm>-n<N>-<L>u-w<NN>: THE HORIZONTAL JOINT. Its axis runs
/// along the part's local +X at r_in above its origin (the underside is the
/// face it beds on — the kit's stacking convention, PartForgeJoints.cpp).
[[nodiscard]] bool parse_sleeper(const std::string& name, float& r_in, int& facets,
                                 float& length_m) {
    if (name.rfind("sleeper-", 0) != 0) return false;
    const std::size_t d_at = name.find("-d");
    if (d_at == std::string::npos) return false;
    int cm = 0;
    if (!int_after(name, d_at + 2, cm) || cm <= 0) return false;
    r_in = static_cast<float>(cm) * 0.005f;
    if (!facets_after(name, d_at, facets)) return false;
    // ...-<L>u-  : the last "<digits>u-" run in the name.
    const std::size_t u_at = name.rfind("u-");
    if (u_at == std::string::npos || u_at == 0) return false;
    std::size_t start = u_at;
    while (start > 0 && std::isdigit(static_cast<unsigned char>(name[start - 1])) != 0) {
        --start;
    }
    int lu = 0;
    if (start == u_at || !int_after(name, start, lu) || lu <= 0) return false;
    length_m = static_cast<float>(lu) * 0.25f;
    return true;
}

/// The `-<A>x<B>x<C>-` block every dimensioned kit part carries. What the
/// three numbers MEAN is the part's business (a slope reads run/depth/rise, a
/// gable base/thickness/rise); this only finds them.
[[nodiscard]] bool parse_dims(const std::string& name, int& a, int& b, int& c) {
    for (std::size_t at = name.find('-'); at != std::string::npos;
         at = name.find('-', at + 1)) {
        const char* p = name.data() + at + 1;
        const char* end = name.data() + name.size();
        auto r1 = std::from_chars(p, end, a);
        if (r1.ec != std::errc{} || r1.ptr == end || *r1.ptr != 'x') continue;
        auto r2 = std::from_chars(r1.ptr + 1, end, b);
        if (r2.ec != std::errc{} || r2.ptr == end || *r2.ptr != 'x') continue;
        auto r3 = std::from_chars(r2.ptr + 1, end, c);
        if (r3.ec != std::errc{}) continue;
        return true;
    }
    return false;
}

[[nodiscard]] bool starts(const std::string& s, const char* p) {
    return s.rfind(p, 0) == 0;
}

// ---------------------------------------------------------------------------
// GEOMETRY. One transform, the same one every box rule in Scene.cpp uses:
// local +X lands on (cos yaw, -sin yaw), local +Z on (sin yaw, cos yaw).
// ---------------------------------------------------------------------------

struct Frame {
    glm::vec3 origin{0.0f};
    glm::vec3 along{1.0f, 0.0f, 0.0f};   ///< local +X in the world
    glm::vec3 lateral{0.0f, 0.0f, 1.0f}; ///< local +Z in the world
    float scale = 1.0f;
    [[nodiscard]] glm::vec3 at(float lx, float ly, float lz) const {
        return origin + (along * lx + glm::vec3{0.0f, ly, 0.0f} + lateral * lz) * scale;
    }
};

[[nodiscard]] Frame frame_of(const Placement& p) {
    Frame f;
    f.origin = p.position;
    const float c = std::cos(p.yaw);
    const float s = std::sin(p.yaw);
    f.along = {c, 0.0f, -s};
    f.lateral = {s, 0.0f, c};
    f.scale = p.scale;
    return f;
}

/// An upright post as the rules see it.
struct VJoint {
    std::size_t index = 0;
    glm::vec2 axis{0.0f};
    float r_in = 0.0f;
    int facets = 0;
    float yaw = 0.0f;
    const std::string* group = nullptr;
};

/// A lying joint — лежень, мауэрлат, коньковый прогон. `a` is the axis's near
/// end, `dir` its unit direction, `len` its length.
struct HJoint {
    std::size_t index = 0;
    glm::vec3 a{0.0f};
    glm::vec3 dir{1.0f, 0.0f, 0.0f};
    glm::vec3 lateral{0.0f, 0.0f, 1.0f}; ///< local +Z: the facet basis's first axis
    float len = 0.0f;
    float r_in = 0.0f;
    int facets = 0;
    const std::string* group = nullptr;
};

/// How far off the joint's axis this point sits, and where along it.
[[nodiscard]] float off_axis(const HJoint& j, glm::vec3 p, float& t) {
    const glm::vec3 d = p - j.a;
    t = glm::dot(d, j.dir);
    return glm::length(d - j.dir * t);
}

/// Angle of an attachment direction in the joint's OWN facet basis, radians.
/// Facet k's outward normal sits at k * 2pi/N in this basis — the orientation
/// contract both forges spell in their headers.
[[nodiscard]] float facet_angle(const HJoint& j, glm::vec3 body) {
    const glm::vec3 perp = body - j.dir * glm::dot(body, j.dir);
    return std::atan2(perp.y, glm::dot(perp, j.lateral));
}

/// One thing hanging on one joint. Collected by both halves of the file and
/// spent by the capacity rule, so that "how many panels may a post carry"
/// has ONE answer rather than one per orientation.
///
/// `t0..t1` is HOW MUCH OF THE JOINT this attachment occupies, measured along
/// the joint's own axis (world height for a post, length for a sleeper). The
/// facet limit is a limit AT A STATION, not over a whole joint, and it has to
/// be: a two-storey house stands two wall panels on one facet of one post, one
/// above the other, and a ridge purlin carries a row of slope panels down its
/// length. Both are carpentry; a rule that counted them would have made the
/// first pass over the demo report 38 defects that were not there (it did).
struct Attach {
    std::size_t joint_placement = 0;
    int facets = 0;
    float angle = 0.0f;   ///< in the joint's facet basis
    float t0 = 0.0f;      ///< along the joint's axis, metres
    float t1 = 0.0f;
    std::size_t by = 0;   ///< the placement that hangs on it
};

/// Do two attachments share any of the joint's length? Touching end to end is
/// not sharing: two panels butting over one post rest on different halves of
/// the facet, which is how a wall course is built.
[[nodiscard]] bool overlaps(const Attach& a, const Attach& b) {
    return std::min(a.t1, b.t1) - std::max(a.t0, b.t0) > 1e-3f;
}

/// Deviation from the nearest facet, and which facet that is. (The deviation
/// is the JointAngle rule's business and is not always spent; `which` is what
/// the capacity rule needs, so the return is deliberately not [[nodiscard]].)
float facet_dev(float angle, int facets, int& which) {
    const float step = 2.0f * PI / static_cast<float>(facets);
    float k = std::round(angle / step);
    which = static_cast<int>(k) % facets;
    if (which < 0) {
        which += facets;
    }
    return std::fabs(angle - k * step);
}

} // namespace

void check_house_rules(const SceneDoc& doc, const SceneWorld& world,
                       const SceneLimits& limits,
                       std::vector<SceneFinding>& found) {
    if (world.object_box == nullptr) {
        return; // same door every box-based rule uses
    }
    const std::size_t n = doc.placements.size();
    std::vector<VJoint> posts;
    std::vector<HJoint> lying;
    std::vector<Attach> attached;

    for (std::size_t i = 0; i < n; ++i) {
        const Placement& p = doc.placements[i];
        float r_in = 0.0f;
        int facets = 0;
        float span = 0.0f;
        if (parse_post(p.object, r_in, facets, span)) {
            posts.push_back({i, {p.position.x, p.position.z}, r_in * p.scale, facets,
                             p.yaw, &p.group});
        } else if (parse_sleeper(p.object, r_in, facets, span)) {
            const Frame f = frame_of(p);
            HJoint j;
            j.index = i;
            j.a = f.at(0.0f, r_in, 0.0f);
            j.dir = f.along;
            j.lateral = f.lateral;
            j.len = span * p.scale;
            j.r_in = r_in * p.scale;
            j.facets = facets;
            j.group = &p.group;
            lying.push_back(j);
        }
    }

    // ЭТИ ПРАВИЛА СУДЯТ ЧЛЕНОВ ПОСТРОЙКИ, И ГРУППА — НЕ ЛАЗЕЙКА, А ШОВ.
    //
    // Что делает `group` в Scene.cpp: она ОТКЛЮЧАЕТ вопрос к земле. Одинокая
    // расстановка обязана стоять на грунте (OnGround мерит зазор до
    // ground_at), а члену группы разрешено стоять на другом члене — иначе
    // каждая балка выше подошвы читалась бы как висящая. Ровно там, где
    // выключается земля, и обязаны включаться шарниры: иначе у стропила не
    // осталось бы НИ ОДНОГО судьи, а это и есть та дыра, ради которой правила
    // писались.
    //
    // Отсюда и обратное: расстановка БЕЗ группы судится землёй и не судится
    // здесь. Это не поблажка — 43 образца витрины лежат на полке стенда
    // поодиночке, без дома, и коньковому прогону под ними взяться неоткуда;
    // «скат обязан висеть на двух лежнях» — утверждение о ДОМЕ, а образец не
    // дом. И снять группу, чтобы замолчать судью, не выйдет: снятая группа
    // возвращает вопрос к земле, и скат, который висел на высоте конька,
    // немедленно краснеет как hovers (контрфакт в SceneHouseRuleTests).
    const auto is_built = [](const Placement& p) { return !p.group.empty(); };

    // ======================================================================
    // ВЕРТИКАЛЬНЫЕ СВЯЗИ: ПАНЕЛЬ НИКОГДА НЕ КАСАЕТСЯ ПАНЕЛИ (§3, §5).
    // Moved from Scene.cpp unchanged except for the two-post rule below it.
    // ======================================================================
    for (std::size_t i = 0; i < n; ++i) {
        const Placement& p = doc.placements[i];
        if (!starts(p.object, "wall-") || !is_built(p)) {
            continue;
        }
        glm::vec2 blo{0.0f};
        glm::vec2 bhi{0.0f};
        if (!world.object_box(world.ctx, p.object, blo, bhi)) {
            continue;
        }
        // The kit's convention: a panel runs along its local +X, its thickness
        // is the local z extent.
        const float t_m = (bhi.y - blo.y) * p.scale;
        const float zc = (blo.y + bhi.y) * 0.5f;
        const float c = std::cos(p.yaw);
        const float sn = std::sin(p.yaw);
        const glm::vec2 along{c, -sn};
        const glm::vec2 lateral{sn, c};
        // How much of the post this panel takes: its own height band. Without
        // a top hook every panel claims a zero band and two storeys never
        // conflict — the lenient direction, which is the right one for a hook
        // a caller may not supply.
        float wall_h = 0.0f;
        if (world.object_top != nullptr) {
            world.object_top(world.ctx, p.object, wall_h);
        }
        const float wall_y0 = p.position.y;
        const float wall_y1 = p.position.y + wall_h * p.scale;
        const VJoint* seated[2] = {nullptr, nullptr};
        for (int end = 0; end < 2; ++end) {
            const float lx = end == 0 ? blo.x : bhi.x;
            const glm::vec2 at{p.position.x + (lx * c + zc * sn) * p.scale,
                               p.position.z + (-lx * sn + zc * c) * p.scale};
            const VJoint* best = nullptr;
            float best_d = 1e30f;
            for (const VJoint& j : posts) {
                if (*j.group != p.group) {
                    continue;
                }
                const float d = glm::length(j.axis - at);
                if (d < best_d) {
                    best_d = d;
                    best = &j;
                }
            }
            const char* which = end == 0 ? "near end" : "far end";
            if (best == nullptr) {
                found.push_back({SceneRule::JointSeat, i, p.object, 0.0f,
                                 std::string(which)
                                     + ": no joint post in this group at all"});
                continue;
            }
            float worst = 0.0f;
            for (const float side : {-0.5f, 0.5f}) {
                const glm::vec2 corner = at + lateral * (t_m * side);
                worst = std::max(worst, glm::length(corner - best->axis));
            }
            const float allowed = best->r_in - limits.joint_seat_margin_m;
            if (worst > allowed) {
                char det[128];
                std::snprintf(det, sizeof(det),
                              "%s: end corner %.3f m from the joint axis, "
                              "allowed %.3f", which, static_cast<double>(worst),
                              static_cast<double>(allowed));
                found.push_back({SceneRule::JointSeat, i, p.object, worst - allowed,
                                 det});
                continue;
            }
            seated[end] = best;
            // Which way the panel's BODY runs from this post: the near end
            // hands it +along, the far end -along. That direction, not the
            // panel's yaw, is what a facet has to carry.
            const glm::vec2 body = end == 0 ? along : -along;
            attached.push_back({best->index, best->facets,
                                std::atan2(-body.y, body.x) - best->yaw, wall_y0,
                                wall_y1, i});
            if (best->facets <= 0) {
                continue; // round: any angle is its rule
            }
            const float step = 2.0f * PI / static_cast<float>(best->facets);
            const float w_f = 2.0f * best->r_in
                            * std::tan(PI / static_cast<float>(best->facets));
            const float slack = (w_f - t_m) * 0.5f;
            if (slack <= 0.0f) {
                char det[128];
                std::snprintf(det, sizeof(det),
                              "%s: facet %.3f m is narrower than the panel "
                              "(%.3f m) — no angle can seat it", which,
                              static_cast<double>(w_f), static_cast<double>(t_m));
                found.push_back({SceneRule::JointAngle, i, p.object, -slack, det});
                continue;
            }
            const float tol = std::atan(slack / best->r_in);
            const float dir = std::atan2(-along.y, along.x);
            float dev = std::fmod(dir - best->yaw, step);
            if (dev < 0.0f) {
                dev += step;
            }
            dev = std::min(dev, step - dev);
            if (dev > tol) {
                char det[160];
                std::snprintf(det, sizeof(det),
                              "%s: %.1f deg off the nearest facet of an n%d "
                              "joint (tolerance %.1f deg)", which,
                              static_cast<double>(dev * 180.0f / PI), best->facets,
                              static_cast<double>(tol * 180.0f / PI));
                found.push_back({SceneRule::JointAngle, i, p.object, dev - tol, det});
            }
        }
        // КАЖДЫЙ МОДУЛЬ СТЕНЫ — НА ДВУХ ШАРНИРАХ, ОБЯЗАТЕЛЬНО (пользователь,
        // 17.08). Two ends seated in ONE post is a panel pinned at a point: it
        // has an angle but no span, and nothing decides where its far end
        // lives. The seat rule alone never notices — it judges each end apart.
        if (seated[0] != nullptr && seated[0] == seated[1]) {
            found.push_back({SceneRule::WallTwoJoints, i, p.object, 0.0f,
                             "both ends seat in the SAME joint post — a wall "
                             "module must hang on two"});
        }
    }

    // ======================================================================
    // ГОРИЗОНТАЛЬНЫЕ СВЯЗИ: ПОЛ, ПОТОЛОК И КРЫША (§8, заказ 17.08).
    // ======================================================================
    /// Seats one edge (two world endpoints at MID-THICKNESS) in the nearest
    /// lying joint of the same group. Returns nullptr when nothing carries it.
    const auto seat_edge = [&](const std::string& group, glm::vec3 e0, glm::vec3 e1,
                               float& off, float& t_lo, float& t_hi,
                               int& candidates) -> const HJoint* {
        const HJoint* best = nullptr;
        // NOT 1e30: "hangs 1000000015047466219876688855040 m off" is a number
        // nobody can act on, and the honest reading of "no lying joint in this
        // group at all" is that the miss is not a distance. That is exactly
        // why `candidates` goes out with it: a report that printed «промах
        // 0.000 м» for «мерить было не по чему» would be a number that LIES,
        // and the reader would go hunting a millimetre that does not exist.
        float best_off = 0.0f;
        t_lo = 0.0f;
        t_hi = 0.0f;
        candidates = 0;
        for (const HJoint& j : lying) {
            if (*j.group != group) {
                continue;
            }
            float t0 = 0.0f;
            float t1 = 0.0f;
            const float d0 = off_axis(j, e0, t0);
            const float d1 = off_axis(j, e1, t1);
            const float worst = std::max(d0, d1);
            // Past either butt the joint is not there at all, whatever the
            // radial distance says — an edge floating a metre beyond a joist
            // is on no joist.
            const float lo = -j.r_in;
            const float hi = j.len + j.r_in;
            if (t0 < lo || t0 > hi || t1 < lo || t1 > hi) {
                continue;
            }
            ++candidates;
            if (best == nullptr || worst < best_off) {
                best_off = worst;
                best = &j;
                t_lo = std::min(t0, t1);
                t_hi = std::max(t0, t1);
            }
        }
        off = best_off;
        return (best != nullptr && best_off <= best->r_in - limits.joint_seat_margin_m)
                 ? best
                 : nullptr;
    };

    /// How the miss reads in the report. Two different failures wear two
    /// different sentences on purpose: «промахнулся на 0.31 м» is a job for
    /// the composer (move it), «лежня нет вовсе» is a job for the builder
    /// (add the purlin). One sentence for both would send the second reader
    /// looking for a distance to close.
    const auto miss_text = [](int candidates, float off) -> std::string {
        char buf[96];
        if (candidates == 0) {
            return "no lying joint of this group crosses it AT ALL";
        }
        std::snprintf(buf, sizeof(buf),
                      "nearest of %d lying joint(s) misses by %.3f m", candidates,
                      static_cast<double>(off));
        return buf;
    };

    /// УГОЛ И НА ЛЕЖАЩЕМ ШАРНИРЕ ТОЖЕ (§4: the facet rule is about joints, not
    /// about uprights). The panel's thickness is not in a roof's name — the
    /// name spells run/depth/rise and the covering decides the slab — so the
    /// tolerance is derived against the kit's own panel thickness. That is a
    /// STAND-IN and it is named one: straw is deeper than 0.25 and tile is
    /// thinner, and the day a covering's depth reaches the registry this line
    /// asks it instead.
    const auto check_h_angle = [&](const HJoint& j, glm::vec3 body, std::size_t at,
                                   const std::string& object) {
        if (j.facets <= 0) {
            return; // a drum takes any angle — that is what a drum is for
        }
        const float w_f = 2.0f * j.r_in * std::tan(PI / static_cast<float>(j.facets));
        const float slack = (w_f - NOMINAL_PANEL_T_M) * 0.5f;
        if (slack <= 0.0f) {
            char det[160];
            std::snprintf(det, sizeof(det),
                          "facet %.3f m of this n%d lying joint is narrower than "
                          "a panel (%.2f m) — no angle can seat it",
                          static_cast<double>(w_f), j.facets,
                          static_cast<double>(NOMINAL_PANEL_T_M));
            found.push_back({SceneRule::JointAngle, at, object, -slack, det});
            return;
        }
        const float tol = std::atan(slack / j.r_in);
        int which = 0;
        const float dev = facet_dev(facet_angle(j, body), j.facets, which);
        if (dev > tol) {
            char det[176];
            std::snprintf(det, sizeof(det),
                          "%.1f deg off facet %d of an n%d lying joint "
                          "(tolerance %.1f deg) — a slope at this pitch wants a "
                          "round one", static_cast<double>(dev * 180.0f / PI), which,
                          j.facets, static_cast<double>(tol * 180.0f / PI));
            found.push_back({SceneRule::JointAngle, at, object, dev - tol, det});
        }
    };

    for (std::size_t i = 0; i < n; ++i) {
        const Placement& p = doc.placements[i];
        if (!is_built(p)) {
            continue; // судится землёй, а не шарнирами — см. is_built выше
        }
        const Frame f = frame_of(p);
        int a = 0;
        int b = 0;
        int c = 0;

        // --- НАКЛОННАЯ ПАНЕЛЬ: низ на одном шарнире, верх на другом ---------
        // roof-<mat>-<run>x<depth>x<rise>: bottom and top edges are PARALLEL,
        // which is the whole family the user named — прямоугольная,
        // трапециевидная, «и всякая, у которой нижняя грань параллельна
        // верхней». Из них под разными углами собираются арки и навесы.
        if (starts(p.object, "roof-") && parse_dims(p.object, a, b, c)) {
            const float run = static_cast<float>(a) * 0.25f;
            const float depth = static_cast<float>(b) * 0.25f;
            const float rise = static_cast<float>(c) * 0.25f;
            const glm::vec3 low0 = f.at(0.0f, 0.0f, 0.0f);
            const glm::vec3 low1 = f.at(0.0f, 0.0f, depth);
            const glm::vec3 top0 = f.at(run, rise, 0.0f);
            const glm::vec3 top1 = f.at(run, rise, depth);
            const glm::vec3 up_slope = glm::normalize(f.along * run
                                                    + glm::vec3{0.0f, rise, 0.0f});
            float off_low = 0.0f;
            float off_top = 0.0f;
            float tl0 = 0.0f;
            float tl1 = 0.0f;
            float tt0 = 0.0f;
            float tt1 = 0.0f;
            int cl = 0;
            int ct = 0;
            const HJoint* jl = seat_edge(p.group, low0, low1, off_low, tl0, tl1, cl);
            const HJoint* jt = seat_edge(p.group, top0, top1, off_top, tt0, tt1, ct);
            if (jt == nullptr) {
                found.push_back({SceneRule::RoofSeat, i, p.object, off_top,
                                 "upper edge is carried by nothing: "
                                     + miss_text(ct, off_top)});
            } else {
                attached.push_back({jt->index, jt->facets,
                                    facet_angle(*jt, -up_slope), tt0, tt1, i});
                check_h_angle(*jt, -up_slope, i, p.object);
            }
            if (jl != nullptr) {
                attached.push_back({jl->index, jl->facets,
                                    facet_angle(*jl, up_slope), tl0, tl1, i});
                check_h_angle(*jl, up_slope, i, p.object);
            } else {
                // КОЗЫРЁК. «Крыша может иметь козырёк, свисающий над землёй
                // вне дома: краем упираться в шарнир не обязательно.» The
                // licence is not "the low edge is free" — it is "the low edge
                // is OUTSIDE": past the footprint of this group's posts there
                // is nothing left to seat it on, and a свес that reached back
                // over the walls would just be an unseated roof.
                glm::vec2 lo{1e30f};
                glm::vec2 hi{-1e30f};
                int posts_here = 0;
                for (const VJoint& j : posts) {
                    if (*j.group != p.group) continue;
                    lo = glm::min(lo, j.axis);
                    hi = glm::max(hi, j.axis);
                    ++posts_here;
                }
                const glm::vec2 mid{(low0.x + low1.x) * 0.5f, (low0.z + low1.z) * 0.5f};
                const bool outside = posts_here > 0
                                  && (mid.x < lo.x || mid.x > hi.x || mid.y < lo.y
                                      || mid.y > hi.y);
                if (!outside) {
                    char tail[96];
                    std::snprintf(tail, sizeof(tail),
                                  " and is NOT a козырёк — it is over this group's "
                                  "own posts, %d of them", posts_here);
                    found.push_back({SceneRule::RoofSeat, i, p.object, off_low,
                                     "lower edge is carried by nothing: "
                                         + miss_text(cl, off_low) + tail});
                }
            }
            continue;
        }

        // --- ТРЕУГОЛЬНАЯ: ВЕРХНИМ УГЛОМ В ТОЧКУ НА ШАРНИРЕ -----------------
        // gable-<mat>-<base>x<thick>x<rise>, roofhip-<mat>-<depth>x<run>x<rise>.
        // A triangle has no upper EDGE to lay on a joint, so what the rule can
        // ask of it — and the user asked exactly this — is that its apex is on
        // one.
        const bool gable = starts(p.object, "gable-");
        const bool hip = starts(p.object, "roofhip-");
        if ((gable || hip) && parse_dims(p.object, a, b, c)) {
            glm::vec3 apex{0.0f};
            if (gable) {
                apex = f.at(static_cast<float>(a) * 0.125f,
                            static_cast<float>(c) * 0.25f,
                            static_cast<float>(b) * 0.125f);
            } else {
                apex = f.at(static_cast<float>(b) * 0.25f,
                            static_cast<float>(c) * 0.25f,
                            static_cast<float>(a) * 0.125f);
            }
            float off = 0.0f;
            float ta0 = 0.0f;
            float ta1 = 0.0f;
            int ca = 0;
            const HJoint* j = seat_edge(p.group, apex, apex, off, ta0, ta1, ca);
            if (j == nullptr) {
                found.push_back({SceneRule::RoofSeat, i, p.object, off,
                                 "apex hangs on nothing: " + miss_text(ca, off)});
            } else {
                attached.push_back({j->index, j->facets,
                                    facet_angle(*j, {0.0f, -1.0f, 0.0f}), ta0, ta1, i});
            }
            continue;
        }

        // --- ПОЛ И ПОТОЛОК: ОТ ДВУХ ДО ЧЕТЫРЁХ ЛЕЖНЕЙ ----------------------
        // «Пол и потолок не могут висеть в пространстве — только на
        // горизонтальных шарнирах; шарниров от 2 до 4.»
        if (!starts(p.object, "floor-")) {
            continue;
        }
        glm::vec2 blo{0.0f};
        glm::vec2 bhi{0.0f};
        if (!world.object_box(world.ctx, p.object, blo, bhi)) {
            continue;
        }
        float thick = 0.0f;
        if (world.object_top != nullptr) {
            world.object_top(world.ctx, p.object, thick);
        }
        const float mid_y = p.position.y + thick * p.scale * 0.5f;
        // Every lying joint of the group whose axis LIES IN the deck's slab
        // and crosses its footprint: the joists this deck is let into. A joist
        // beside the deck, or a metre below it, carries nothing.
        struct Carrier {
            const HJoint* j = nullptr;
            float t0 = 0.0f; ///< how much of the joist this deck covers
            float t1 = 0.0f;
        };
        std::vector<Carrier> carriers;
        for (const HJoint& j : lying) {
            if (*j.group != p.group) continue;
            if (std::fabs(j.a.y - mid_y) > thick * p.scale * 0.5f + j.r_in) continue;
            // Sample the axis across the deck's own local footprint.
            constexpr int SAMPLES = 32;
            float t0 = 1e30f;
            float t1 = -1e30f;
            for (int k = 0; k <= SAMPLES; ++k) {
                const float t = j.len * static_cast<float>(k)
                              / static_cast<float>(SAMPLES);
                const glm::vec3 d = j.a + j.dir * t - p.position;
                const float lx = glm::dot(d, f.along) / p.scale;
                const float lz = glm::dot(d, f.lateral) / p.scale;
                if (lx >= blo.x && lx <= bhi.x && lz >= blo.y && lz <= bhi.y) {
                    t0 = std::min(t0, t);
                    t1 = std::max(t1, t);
                }
            }
            if (t1 >= t0) {
                carriers.push_back({&j, t0, t1});
            }
        }
        if (carriers.size() < 2) {
            char det[160];
            std::snprintf(det, sizeof(det),
                          "a deck on %zu lying joint(s): пол и потолок не могут "
                          "висеть в пространстве, нужно не меньше двух",
                          carriers.size());
            found.push_back({SceneRule::DeckOnJoints, i, p.object,
                             static_cast<float>(2 - static_cast<int>(carriers.size())),
                             det});
        } else if (carriers.size() > 4) {
            char det[160];
            std::snprintf(det, sizeof(det),
                          "a deck on %zu lying joints, the cap is 4 — a panel is "
                          "framed on its sides, a plank floor is not a panel",
                          carriers.size());
            found.push_back({SceneRule::DeckOnJoints, i, p.object,
                             static_cast<float>(carriers.size() - 4), det});
        } else {
            // НЕ ВИСИТ: the deck's own centre must lie BETWEEN its outermost
            // carriers. Two joists under one half hold a deck the way two legs
            // under one end hold a table.
            const glm::vec3 centre = f.at((blo.x + bhi.x) * 0.5f, 0.0f,
                                          (blo.y + bhi.y) * 0.5f);
            const glm::vec3 across = glm::normalize(
                glm::cross(carriers.front().j->dir, glm::vec3{0.0f, 1.0f, 0.0f}));
            float lo = 1e30f;
            float hi = -1e30f;
            for (const Carrier& q : carriers) {
                const float s = glm::dot(q.j->a - centre, across);
                lo = std::min(lo, s);
                hi = std::max(hi, s);
            }
            if (lo > 0.0f || hi < 0.0f) {
                char det[160];
                std::snprintf(det, sizeof(det),
                              "every carrying joint is on ONE side of the deck's "
                              "centre (%.2f..%.2f m) — the rest of it hangs",
                              static_cast<double>(lo), static_cast<double>(hi));
                found.push_back({SceneRule::DeckOnJoints, i, p.object,
                                 std::min(std::fabs(lo), std::fabs(hi)), det});
            }
        }
        for (const Carrier& q : carriers) {
            attached.push_back({q.j->index, q.j->facets,
                                facet_angle(*q.j, {0.0f, 1.0f, 0.0f}), q.t0, q.t1, i});
        }
    }

    // ======================================================================
    // ЁМКОСТЬ ШАРНИРА: СКОЛЬКО ГРАНЕЙ — СТОЛЬКО И ПАНЕЛЕЙ (§4, заказ 17.08).
    // «К круглым сколько угодно, к квадратным ровно 4 (по одной на грань), к
    // N-угольным не больше N.» The limit is not a budget somebody chose: a
    // panel seats FLUSH ON A FACET, and a facet already carrying a panel has
    // nothing left to offer the next one.
    // ======================================================================
    std::map<std::size_t, std::vector<const Attach*>> by_joint;
    for (const Attach& at : attached) {
        by_joint[at.joint_placement].push_back(&at);
    }
    for (const auto& [joint_i, list] : by_joint) {
        const int facets = list.front()->facets;
        if (facets <= 0) {
            continue; // a drum has no facets to run out of
        }
        const std::string& jname = doc.placements[joint_i].object;
        // ONE FACET, ONE PANEL — where they meet on the joint's length. Two
        // panels on one facet at different stations are a wall course or a
        // second storey, and the joint has as much facet for the second as it
        // had for the first.
        bool crowded = false;
        for (std::size_t x = 0; x < list.size(); ++x) {
            int at_station = 1;
            for (std::size_t y = 0; y < list.size(); ++y) {
                if (x == y || !overlaps(*list[x], *list[y])) {
                    continue;
                }
                ++at_station;
                if (y < x) {
                    continue; // report each pair once
                }
                int fx = 0;
                int fy = 0;
                facet_dev(list[x]->angle, facets, fx);
                facet_dev(list[y]->angle, facets, fy);
                if (fx == fy) {
                    char det[192];
                    std::snprintf(det, sizeof(det),
                                  "facet %d of this n%d joint carries two panels "
                                  "at the same station (#%zu and #%zu) — one "
                                  "facet, one panel", fx, facets, list[x]->by,
                                  list[y]->by);
                    found.push_back({SceneRule::JointCapacity, joint_i, jname, 1.0f,
                                     det});
                }
            }
            if (at_station > facets && !crowded) {
                crowded = true;
                char det[176];
                std::snprintf(det, sizeof(det),
                              "%d panels meet on this n%d joint at one station — "
                              "it has %d facets and a facet carries one",
                              at_station, facets, facets);
                found.push_back({SceneRule::JointCapacity, joint_i, jname,
                                 static_cast<float>(at_station - facets), det});
            }
        }
    }
}

} // namespace dfn::world
