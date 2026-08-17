/*
Created: 17:08:2026 - 17:16:17
Last updated: 17:08:2026 - 17:16:17
Module: engine/world
File: engine/world/sources/SceneStairRules.cpp

Responsibility:
- The two stair rules themselves (see SceneStairRules.h): StairSeat and
  StairHeadroom, plus the derived opening the generator asks for.

Key items:
- opening_length_m / opening_start_m: КАЛЬКУЛЯТОР.
- capsule_half_width(): the player's horizontal half-width at a height above
  his own feet — a capsule, NOT a cylinder, and that difference is the whole
  second term of the formula.
- check_stair_rules(): СУДЬЯ — the capsule on every tread in turn.

Dependencies:
- Uses: Scene.h, glm, std.
- Used by: Scene.cpp (check_scene), tests/core/SceneStairRuleTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ЭТОТ ФАЙЛ МЕРИТ, А НЕ СЧИТАЕТ. check_stair_rules() НЕ ЗОВЁТ
  opening_length_m() и не должен: две независимые дороги к одному числу —
  единственное, что вообще способно поймать ошибку в любой из них. Сведи их в
  одну, и обе ошибутся одинаково.
*/
/*
UPD:
- 17:08:2026 - 17:16:17: Создан: лестница на шарнирах и проём, измеренный капсулой.
*/

#include "engine/world/sources/SceneStairRules.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <glm/geometric.hpp>
#include <string>
#include <vector>

namespace dfn::world {
namespace {

/// The kit's own stair geometry, read out of the name and its one variant
/// token: `stair[-steep]-<mat>-<going_u>x<width_u>x<steps>-w<NN>`, origin at
/// the foot of the lowest step, climbing along local +X, width along +Z. Rise
/// is 1u for every flight in the shelf and may not be otherwise —
/// PLAYER_STEP_HEIGHT 0.35 passes a 0.25 riser and refuses a 0.50 one.
constexpr float GRID_M = 0.25f;

struct Flight {
    float going = 0.25f; ///< horizontal per step
    float width = 1.0f;
    int steps = 1;
};

[[nodiscard]] bool int_at(const std::string& s, std::size_t at, int& out) {
    const auto [ptr, ec] = std::from_chars(s.data() + at, s.data() + s.size(), out);
    (void)ptr;
    return ec == std::errc{};
}

[[nodiscard]] bool parse_flight(const std::string& name, Flight& f) {
    if (name.rfind("stair-", 0) != 0) {
        return false;
    }
    int a = 0;
    int b = 0;
    int c = 0;
    bool got = false;
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
        got = true;
        break;
    }
    if (!got || c <= 0) {
        return false;
    }
    f.going = static_cast<float>(a) * GRID_M;
    f.width = static_cast<float>(b) * GRID_M;
    f.steps = c;
    return true;
}

/// НАСТИЛ И ЕГО ОБЪЯВЛЕННАЯ ПУСТОТА:
/// `deck-<mat>-<L>x<W>x<T>[-hole<x>x<z>x<l>x<w>]-w<NN>`.
struct Deck {
    float len = 0.0f;   ///< along local +X
    float wide = 0.0f;  ///< along local +Z
    float thick = 0.0f;
    bool declared = false; ///< does this panel DECLARE a void at all
    float vx0 = 0.0f;
    float vx1 = 0.0f;
    float vz0 = 0.0f;
    float vz1 = 0.0f;
};

[[nodiscard]] bool parse_deck(const std::string& name, Deck& d) {
    if (name.rfind("deck-", 0) != 0) {
        return false;
    }
    int a = 0;
    int b = 0;
    int c = 0;
    const std::size_t dash = name.find('-', 5);
    if (dash == std::string::npos) {
        return false;
    }
    const char* p = name.data() + dash + 1;
    const char* end = name.data() + name.size();
    auto r1 = std::from_chars(p, end, a);
    if (r1.ec != std::errc{} || r1.ptr == end || *r1.ptr != 'x') return false;
    auto r2 = std::from_chars(r1.ptr + 1, end, b);
    if (r2.ec != std::errc{} || r2.ptr == end || *r2.ptr != 'x') return false;
    auto r3 = std::from_chars(r2.ptr + 1, end, c);
    if (r3.ec != std::errc{}) return false;
    d.len = static_cast<float>(a) * GRID_M;
    d.wide = static_cast<float>(b) * GRID_M;
    d.thick = static_cast<float>(c) * GRID_M;
    const std::size_t h = name.find("-hole");
    if (h == std::string::npos) {
        d.declared = false;
        return true;
    }
    int vx = 0;
    int vz = 0;
    int vl = 0;
    int vw = 0;
    const char* q = name.data() + h + 5;
    auto s1 = std::from_chars(q, end, vx);
    if (s1.ec != std::errc{} || s1.ptr == end || *s1.ptr != 'x') return true;
    auto s2 = std::from_chars(s1.ptr + 1, end, vz);
    if (s2.ec != std::errc{} || s2.ptr == end || *s2.ptr != 'x') return true;
    auto s3 = std::from_chars(s2.ptr + 1, end, vl);
    if (s3.ec != std::errc{} || s3.ptr == end || *s3.ptr != 'x') return true;
    auto s4 = std::from_chars(s3.ptr + 1, end, vw);
    if (s4.ec != std::errc{}) return true;
    d.declared = vl > 0 && vw > 0;
    d.vx0 = static_cast<float>(vx) * GRID_M;
    d.vx1 = d.vx0 + static_cast<float>(vl) * GRID_M;
    d.vz0 = static_cast<float>(vz) * GRID_M;
    d.vz1 = d.vz0 + static_cast<float>(vw) * GRID_M;
    return true;
}

struct Frame {
    glm::vec3 origin{0.0f};
    glm::vec3 along{1.0f, 0.0f, 0.0f};
    glm::vec3 lateral{0.0f, 0.0f, 1.0f};
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

/// ИГРОК — КАПСУЛА, А НЕ СТОЛБИК. Half-width at `h` metres above his feet:
/// the radius in the cylindrical middle, and a shrinking circle inside each
/// hemispherical cap. This is the ONE place the second term of the derived
/// formula comes from, and it is here as geometry rather than as a constant so
/// that the judge and the calculator agree by having reasoned the same way,
/// not by having copied the same number.
[[nodiscard]] float capsule_half_width(float h, float height, float radius) {
    if (h < 0.0f || h > height) {
        return 0.0f;
    }
    if (h < radius) {
        const float dy = radius - h;
        return std::sqrt(std::max(0.0f, radius * radius - dy * dy));
    }
    const float top = height - radius;
    if (h > top) {
        const float dy = h - top;
        return std::sqrt(std::max(0.0f, radius * radius - dy * dy));
    }
    return radius;
}

/// Widest the capsule gets anywhere inside the height band [`lo`, `hi`].
[[nodiscard]] float capsule_widest_in(float lo, float hi, float height,
                                      float radius) {
    lo = std::max(lo, 0.0f);
    hi = std::min(hi, height);
    if (hi < lo) {
        return 0.0f;
    }
    // The half-width rises to `radius` over [0, radius] and falls again over
    // [height - radius, height], so the widest point of any band is whichever
    // of its ends (or the cylinder, if the band reaches it) is widest.
    const float mid = std::clamp(radius, lo, hi);
    return std::max({capsule_half_width(lo, height, radius),
                     capsule_half_width(hi, height, radius),
                     capsule_half_width(mid, height, radius)});
}

} // namespace

float opening_length_m(float t, float thick, float height, float radius) {
    if (t <= 0.0f) {
        return 0.0f;
    }
    return (height + thick) / t + radius * t / (std::sqrt(1.0f + t * t) + 1.0f);
}

float opening_start_m(float t, float thick, float height, float radius,
                      float storey_m) {
    if (t <= 0.0f) {
        return 0.0f;
    }
    const float h_low = storey_m - thick;
    return (h_low - height) / t - radius * t / (std::sqrt(1.0f + t * t) + 1.0f);
}

void check_stair_rules(const SceneDoc& doc, const SceneWorld& world,
                       const SceneLimits& limits,
                       std::vector<SceneFinding>& found) {
    if (world.object_box == nullptr) {
        return;
    }
    const float H = limits.player_capsule_height_m;
    const float R = limits.player_capsule_radius_m;
    const std::size_t n = doc.placements.size();

    // Every deck of the scene, resolved once: a flight has to ask about all of
    // them, and parsing per step would parse the same name a hundred times.
    struct PlacedDeck {
        std::size_t index = 0;
        Deck d;
        Frame f;
        float y0 = 0.0f; ///< underside, world
        float y1 = 0.0f; ///< top, world
        const std::string* group = nullptr;
    };
    std::vector<PlacedDeck> decks;
    for (std::size_t i = 0; i < n; ++i) {
        const Placement& p = doc.placements[i];
        Deck d;
        if (!parse_deck(p.object, d)) {
            continue;
        }
        PlacedDeck pd;
        pd.index = i;
        pd.d = d;
        pd.f = frame_of(p);
        pd.y0 = p.position.y;
        pd.y1 = p.position.y + d.thick * p.scale;
        pd.group = &p.group;
        decks.push_back(pd);
    }

    // ЛЕЖАЩИЕ ШАРНИРЫ, теми же именами, что читает SceneHouseRules: лестница —
    // ТРЕТИЙ их клиент, и клиент обязан пользоваться тем же контрактом, а не
    // своим похожим.
    struct Lying {
        glm::vec3 a{0.0f};
        glm::vec3 dir{1.0f, 0.0f, 0.0f};
        float len = 0.0f;
        float r_in = 0.0f;
        const std::string* group = nullptr;
    };
    std::vector<Lying> lying;
    for (std::size_t i = 0; i < n; ++i) {
        const Placement& p = doc.placements[i];
        if (p.object.rfind("sleeper-", 0) != 0) {
            continue;
        }
        const std::size_t d_at = p.object.find("-d");
        const std::size_t u_at = p.object.rfind("u-");
        if (d_at == std::string::npos || u_at == std::string::npos || u_at == 0) {
            continue;
        }
        int cm = 0;
        if (!int_at(p.object, d_at + 2, cm) || cm <= 0) {
            continue;
        }
        std::size_t start = u_at;
        while (start > 0 && std::isdigit(static_cast<unsigned char>(p.object[start - 1])) != 0) {
            --start;
        }
        int lu = 0;
        if (start == u_at || !int_at(p.object, start, lu) || lu <= 0) {
            continue;
        }
        const float r_in = static_cast<float>(cm) * 0.005f;
        const Frame f = frame_of(p);
        Lying j;
        j.a = f.at(0.0f, r_in, 0.0f);
        j.dir = f.along;
        j.len = static_cast<float>(lu) * GRID_M * p.scale;
        j.r_in = r_in * p.scale;
        j.group = &p.group;
        lying.push_back(j);
    }

    /// Is this world point inside some lying joint of `group`?
    const auto on_joint = [&](const std::string& group, glm::vec3 pt,
                              float& best_off) -> bool {
        bool any = false;
        best_off = 0.0f;
        for (const Lying& j : lying) {
            if (*j.group != group) {
                continue;
            }
            const glm::vec3 d = pt - j.a;
            const float t = glm::dot(d, j.dir);
            if (t < -j.r_in || t > j.len + j.r_in) {
                continue;
            }
            const float off = glm::length(d - j.dir * t);
            if (!any || off < best_off) {
                best_off = off;
                any = true;
            }
            if (off <= j.r_in - limits.joint_seat_margin_m) {
                return true;
            }
        }
        return false;
    };

    for (std::size_t i = 0; i < n; ++i) {
        const Placement& p = doc.placements[i];
        Flight fl;
        if (!parse_flight(p.object, fl) || p.group.empty()) {
            continue; // a flight on open ground is judged by OnGround (§8.3)
        }
        const Frame f = frame_of(p);
        const float rise = GRID_M; // 1u for every flight in the shelf

        // --- StairSeat: НИЗ НА ШАРНИР НИЖНЕГО УРОВНЯ, ВЕРХ НА ШАРНИР ВЕРХНЕГО.
        // Mid-width on each end, at the flight's own level: the same
        // mid-thickness-on-the-axis convention every panel obeys (§3.2).
        const glm::vec3 foot = f.at(0.0f, 0.0f, fl.width * 0.5f);
        const glm::vec3 headw = f.at(static_cast<float>(fl.steps) * fl.going,
                                     static_cast<float>(fl.steps) * rise,
                                     fl.width * 0.5f);
        float off = 0.0f;
        for (int end = 0; end < 2; ++end) {
            const glm::vec3 pt = end == 0 ? foot : headw;
            if (on_joint(p.group, pt, off)) {
                continue;
            }
            char det[176];
            std::snprintf(det, sizeof(det),
                          "%s of the flight is on no lying joint of this group "
                          "(nearest miss %.3f m) — a stair hangs off пол-потолок "
                          "at BOTH ends", end == 0 ? "the foot" : "the head",
                          static_cast<double>(off));
            found.push_back({SceneRule::StairSeat, i, p.object, off, det});
        }

        // --- StairHeadroom: КАПСУЛА НА КАЖДУЮ СТУПЕНЬ ------------------------
        // The judge, and it never asks the formula (see the file header). It
        // stands the player on tread k and asks what the capsule touches.
        // МЕРИТСЯ ПО НОСКУ СТУПЕНИ, и это не выбор из удобства: носок — та
        // единственная точка ступени, которая лежит НА ЛИНИИ УКЛОНА
        // (x = (k+1)*going, y = (k+1)*rise, отношение ровно t). Так меряют
        // высоту прохода над лестницей и в настоящих нормах, и так судья
        // остаётся сравнимым с калькулятором, который знает про марш только
        // его уклон. Мерить по середине проступи значило бы подмешивать к
        // ответу полпроступи и получать расхождение, которого в геометрии нет.
        for (int k = 0; k < fl.steps; ++k) {
            const float feet_y = static_cast<float>(k + 1) * rise * p.scale;
            const glm::vec3 stand = f.at(static_cast<float>(k + 1) * fl.going,
                                         feet_y, fl.width * 0.5f);
            // ДВА РАЗНЫХ ФАКТА, и путать их нельзя: «на этой высоте у постройки
            // ЕСТЬ настил» и «настил ЕСТЬ НАД НИМ». Первое без второго — это
            // щель между панелями, то есть НЕОБЪЯВЛЕННАЯ пустота.
            bool level_exists = false;
            bool covered = false;
            const PlacedDeck* hit_solid = nullptr;
            float worst = 0.0f;
            const char* why = "";
            for (const PlacedDeck& pd : decks) {
                if (*pd.group != p.group) {
                    continue;
                }
                const float lo = pd.y0 - stand.y;
                const float hi = pd.y1 - stand.y;
                // ПОТОЛОК — ЭТО ТО, ЧТО ВЫШЕ НОГ. A slab whose underside is at
                // or below his feet is the FLOOR HE IS ARRIVING ON, not a
                // ceiling: the last treads of any flight bring him level with
                // the deck by design, and a rule that counted the landing as an
                // obstruction would report every correct stairwell in the world.
                if (lo <= 0.001f || lo >= H) {
                    continue;
                }
                (void)hi;
                level_exists = true;
                const float r = capsule_widest_in(lo, std::min(hi, H), H, R);
                // Into the deck's own frame: the void is declared there.
                const glm::vec3 rel = stand - pd.f.origin;
                const float lx = glm::dot(rel, pd.f.along) / pd.f.scale;
                const float lz = glm::dot(rel, pd.f.lateral) / pd.f.scale;
                if (lx < -r || lx > pd.d.len + r || lz < -r || lz > pd.d.wide + r) {
                    continue; // this panel is not over him at all
                }
                covered = true;
                if (!pd.d.declared) {
                    // Solid panel over the flight: no declaration, no way up.
                    if (r > worst) {
                        worst = r;
                        hit_solid = &pd;
                        why = "СПЛОШНАЯ ПАНЕЛЬ над маршем: проёма в ней не "
                              "объявлено вовсе";
                    }
                    continue;
                }
                // How far he stands from the nearest SOLID timber of this
                // panel. A void side that runs out to the panel's own edge is
                // not a wall of anything and must not be measured against.
                float clear = 1e30f;
                if (pd.d.vx0 > 0.001f) clear = std::min(clear, lx - pd.d.vx0);
                if (pd.d.vx1 < pd.d.len - 0.001f) clear = std::min(clear, pd.d.vx1 - lx);
                if (pd.d.vz0 > 0.001f) clear = std::min(clear, lz - pd.d.vz0);
                if (pd.d.vz1 < pd.d.wide - 0.001f) clear = std::min(clear, pd.d.vz1 - lz);
                const float bite = r - clear;
                if (bite > 0.001f && bite > worst) {
                    worst = bite;
                    hit_solid = &pd;
                    why = "проём есть, но игрок в него не помещается";
                }
            }
            if (hit_solid != nullptr) {
                // ЧИСЛО ИДЁТ ПЕРВЫМ, а имя детали последним, и это не вкус:
                // имя настила с проёмом длинное, и в первой версии оно съедало
                // хвост строки вместе с миллиметрами — то есть ровно то, ради
                // чего находку и печатают.
                char det[320];
                std::snprintf(det, sizeof(det),
                              "капсула (%.2f x %.2f м) входит в настил на "
                              "%.3f м — ступень %d из %d: %s (\"%s\")",
                              static_cast<double>(R * 2.0f), static_cast<double>(H),
                              static_cast<double>(worst), k + 1, fl.steps, why,
                              doc.placements[hit_solid->index].object.c_str());
                found.push_back({SceneRule::StairHeadroom, i, p.object, worst, det});
                break; // one finding per flight: the first step that fails
            }
            if (level_exists && !covered) {
                // НЕОБЪЯВЛЕННАЯ ПУСТОТА. Over his head there is nothing — but
                // is that because the design says so, or because a panel was
                // never laid? The group HAS a deck at this height; it simply is
                // not over him. That is a gap between panels, and a gap is
                // exactly what «просто не положенная панель» looks like from
                // below. A building with no deck at this height at all is NOT
                // judged: an outside flight to a terrace is a stair.
                char det[256];
                std::snprintf(det, sizeof(det),
                              "ступень %d из %d: над ней ПУСТОТА, которую никто "
                              "не объявлял — марш идёт сквозь щель между "
                              "настилами, а не сквозь ПРОЁМ", k + 1, fl.steps);
                found.push_back({SceneRule::StairHeadroom, i, p.object, 0.0f, det});
                break;
            }
        }
    }
}

} // namespace dfn::world
