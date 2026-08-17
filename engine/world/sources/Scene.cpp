/*
Created: 15:08:2026 - 16:24:04
Last updated: 17:08:2026 - 10:53:33
Module: engine/world
File: engine/world/sources/Scene.cpp

Responsibility:
- The .scene text format's reader/writer and the rule checker declared in
  Scene.h.

Dependencies:
- Uses: Scene.h, std.
- Used by: tools/check_scene.cpp, the app, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The format is TEXT and its field order is fixed, because this file lives in
  git next to the map it composes: a human reads the diff, an agent rewrites
  the file, and both must see the same thing move.
- Every rule measures the OBJECT through SceneWorld, never a constant guessed
  here. A rule that invents a size is a rule that passes a tree and fails a
  boulder for no stated reason.
*/
/*
UPD:
- 15:08:2026 - 16:24:04: Created with Scene.h.
- 16:08:2026 - 21:08:52: Реализация групп: опора = земля ИЛИ верх другого члена группы, чей след
  пересекается в плане; отдельная ветвь «ВСТАВЛЕНО В» (окно несёт стена, а не
  то, что под ним — иначе каждое окно мира висит); след считается
  прямоугольником, когда object_box есть, и кругом, когда нет (дерево круглое,
  и старый вызывающий код обязан работать как раньше). fix_scene_ground НЕ
  трогает членов групп: посадить стропила на землю — снести дом.
- 16:08:2026 - 21:50:43: неизвестная СЕКЦИЯ пропускается, а не роняет чтение. Формат растёт
  (зона домов вносит [portal] под интерьеры), и читатель, умирающий на
  завтрашней секции, не прочтёт сегодняшний файл, написанный более новым
  инструментом. Та же позиция, что у неизвестного КЛЮЧА, — и она обязана быть
  той же, иначе обещание сдержано наполовину.
- 16:08:2026 - 22:40:23: split_shelves() реализован здесь же.
- 16:08:2026 - 22:45:34: check_panel_solid — сетка лучей поперёк тончайшей оси габарита,
  шаг 0.01 м (правило 50: самая узкая реальная дыра зоны — щель доски
  0.017 м; сетка грубее объявила бы стену из швов сплошной). Пересечение —
  math::ray_vs_triangle (тыльные грани сообщаются по контракту Intersect).
  Отчёт несёт СЧЁТ и АДРЕС первой дыры, не вердикт.
- 17:08:2026 - 03:09:30: чтение и запись spawn / spawn_yaw.
- 17:08:2026 - 10:53:33: чтение и запись [light]; неизвестный ключ внутри секции пропускается,
  кривое число — ошибка со строкой, как и везде в этом файле.
*/

#include "engine/world/sources/Scene.h"

#include "engine/core/math/sources/Intersect.h"
#include "engine/core/math/sources/Ray.h"

#include <algorithm>
#include <charconv>
#include <glm/common.hpp>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace dfn::world {
namespace {

[[nodiscard]] std::string trim(std::string_view v) {
    while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.remove_prefix(1);
    while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '\r')) {
        v.remove_suffix(1);
    }
    return std::string(v);
}

/// Parses a float and says so. A silent 0 here would place an object at the
/// world origin and look like a composition decision.
[[nodiscard]] bool parse_float(const std::string& text, float& out) {
    try {
        size_t used = 0;
        const float v = std::stof(text, &used);
        if (used == 0) {
            return false;
        }
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

bool read_scene(const std::filesystem::path& path, SceneDoc& out, std::string& error) {
    std::ifstream in(path);
    if (!in) {
        error = "cannot open " + path.string();
        return false;
    }
    out = SceneDoc{};
    std::string line;
    int line_no = 0;
    Placement current;
    bool in_placement = false;
    bool in_light = false;
    const auto flush = [&] {
        if (in_placement && !current.object.empty()) {
            out.placements.push_back(current);
        }
        current = Placement{};
    };
    while (std::getline(in, line)) {
        ++line_no;
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#') {
            continue;
        }
        if (t == "[place]") {
            flush();
            in_placement = true;
            in_light = false;
            continue;
        }
        if (t == "[light]") {
            flush();
            in_placement = false;
            in_light = true;
            out.lights.emplace_back();
            continue;
        }
        // ANY OTHER SECTION IS SKIPPED, not fatal. The format grows — the
        // interior work is adding a [portal] section right now — and a reader
        // that dies on tomorrow's section cannot read today's file written by
        // a newer tool. Same stance the unknown-KEY rule already takes, and it
        // has to be the same stance or the promise is only half kept.
        if (t.front() == '[' && t.back() == ']') {
            flush();
            in_placement = false;
            in_light = false;
            continue;
        }
        const auto eq = t.find('=');
        if (eq == std::string::npos) {
            error = "line " + std::to_string(line_no) + ": expected key = value";
            return false;
        }
        const std::string key = trim(t.substr(0, eq));
        const std::string value = trim(t.substr(eq + 1));
        const auto number = [&](float& dst) {
            if (!parse_float(value, dst)) {
                error = "line " + std::to_string(line_no) + ": \"" + value
                      + "\" is not a number";
                return false;
            }
            return true;
        };
        if (in_light) {
            SceneLight& L = out.lights.back();
            if (key == "pos") {
                float x = 0.0f;
                float y = 0.0f;
                float z = 0.0f;
                if (std::sscanf(value.c_str(), "%f %f %f", &x, &y, &z) != 3) {
                    error = "line " + std::to_string(line_no)
                          + ": pos wants three numbers \"x y z\"";
                    return false;
                }
                L.position = {x, y, z};
            } else if (key == "color") {
                float r = 0.0f;
                float g = 0.0f;
                float b = 0.0f;
                if (std::sscanf(value.c_str(), "%f %f %f", &r, &g, &b) != 3) {
                    error = "line " + std::to_string(line_no)
                          + ": color wants three numbers \"r g b\"";
                    return false;
                }
                L.color = {r, g, b};
            } else if (key == "radius_m") {
                if (!parse_float(value, L.radius_m)) {
                    error = "line " + std::to_string(line_no) + ": \"" + value
                          + "\" is not a number";
                    return false;
                }
            } else if (key == "casts_shadow") {
                L.casts_shadow = value == "1" || value == "true" || value == "yes";
            } else if (key == "note") {
                L.note = value;
            }
            continue;
        }
        if (!in_placement) {
            if (key == "map") {
                out.map = value;
            } else if (key == "world_span_m") {
                if (!number(out.world_span_m)) return false;
            } else if (key == "spawn") {
                float x = 0.0f;
                float y = 0.0f;
                float z = 0.0f;
                if (std::sscanf(value.c_str(), "%f %f %f", &x, &y, &z) != 3) {
                    error = "line " + std::to_string(line_no)
                          + ": spawn wants three numbers \"x y z\"";
                    return false;
                }
                out.spawn = {x, y, z};
                out.has_spawn = true;
            } else if (key == "spawn_yaw") {
                if (!number(out.spawn_yaw)) return false;
            }
            // Unknown header keys are skipped: the format will grow, and a
            // reader that refuses tomorrow's key cannot read today's file
            // written by a newer tool (Rule 7's spirit, in text).
            continue;
        }
        if (key == "object") {
            current.object = value;
        } else if (key == "note") {
            current.note = value;
        } else if (key == "group") {
            current.group = value;
        } else if (key == "yaw") {
            if (!number(current.yaw)) return false;
        } else if (key == "scale") {
            if (!number(current.scale)) return false;
        } else if (key == "pos") {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            if (std::sscanf(value.c_str(), "%f %f %f", &x, &y, &z) != 3) {
                error = "line " + std::to_string(line_no)
                      + ": pos wants three numbers \"x y z\"";
                return false;
            }
            current.position = {x, y, z};
        }
    }
    flush();
    return true;
}

bool write_scene(const SceneDoc& doc, const std::filesystem::path& path) {
    std::ostringstream out;
    out << "# Daggerfall N scene — what stands where on this map.\n"
        << "# Edited by hand and by agents; checked by dfn_scene_check.\n"
        << "map = " << doc.map << "\n"
        << "world_span_m = " << doc.world_span_m << "\n";
    if (doc.has_spawn) {
        out << "spawn = " << doc.spawn.x << ' ' << doc.spawn.y << ' ' << doc.spawn.z
            << "\n"
            << "spawn_yaw = " << doc.spawn_yaw << "\n";
    }
    for (const Placement& p : doc.placements) {
        out << "\n[place]\n"
            << "object = " << p.object << "\n"
            << "pos = " << p.position.x << ' ' << p.position.y << ' ' << p.position.z
            << "\n"
            << "yaw = " << p.yaw << "\n"
            << "scale = " << p.scale << "\n";
        if (!p.group.empty()) {
            out << "group = " << p.group << "\n";
        }
        if (!p.note.empty()) {
            out << "note = " << p.note << "\n";
        }
    }
    for (const SceneLight& L : doc.lights) {
        out << "\n[light]\n"
            << "pos = " << L.position.x << ' ' << L.position.y << ' ' << L.position.z
            << "\n"
            << "color = " << L.color.r << ' ' << L.color.g << ' ' << L.color.b << "\n"
            << "radius_m = " << L.radius_m << "\n";
        if (L.casts_shadow) {
            out << "casts_shadow = 1\n";
        }
        if (!L.note.empty()) {
            out << "note = " << L.note << "\n";
        }
    }
    const std::string text = out.str();
    // Atomic: a half-written scene is a map that opens to nothing.
    const std::filesystem::path tmp = path.string() + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            return false;
        }
        f.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!f) {
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    return !ec;
}

std::vector<SceneFinding> check_scene(const SceneDoc& doc, const SceneWorld& world,
                                      const SceneLimits& limits) {
    std::vector<SceneFinding> found;
    if (world.ground_at == nullptr || world.object_extent == nullptr) {
        found.push_back({SceneRule::KnownObject, 0, "",
                         0.0f, "the checker was given no world to measure against"});
        return found;
    }
    struct Sized {
        float radius = 0.0f;
        float bottom = 0.0f;
        float top = 0.0f;
        /// The WORLD-space footprint: min/max in x and z, already scaled,
        /// turned by yaw and moved into place.
        glm::vec2 lo{0.0f};
        glm::vec2 hi{0.0f};
        bool known = false;
        bool has_top = false;
    };
    std::vector<Sized> sizes(doc.placements.size());
    for (std::size_t i = 0; i < doc.placements.size(); ++i) {
        const Placement& p = doc.placements[i];
        Sized& s = sizes[i];
        s.known = world.object_extent(world.ctx, p.object, s.radius, s.bottom);
        s.radius *= p.scale;
        s.bottom *= p.scale;
        if (s.known && world.object_top != nullptr
            && world.object_top(world.ctx, p.object, s.top)) {
            s.top *= p.scale;
            s.has_top = true;
        }
        glm::vec2 blo{0.0f};
        glm::vec2 bhi{0.0f};
        if (s.known && world.object_box != nullptr
            && world.object_box(world.ctx, p.object, blo, bhi)) {
            // Turn the four corners and take what encloses them: a turned box
            // is not a box, and the enclosing one is the honest conservative
            // reading — it never lets an overlap through unseen.
            const float c = std::cos(p.yaw);
            const float sn = std::sin(p.yaw);
            s.lo = glm::vec2{1e30f};
            s.hi = glm::vec2{-1e30f};
            for (int k = 0; k < 4; ++k) {
                const float lx = (k & 1) ? bhi.x : blo.x;
                const float lz = (k & 2) ? bhi.y : blo.y;
                const glm::vec2 w{p.position.x + (lx * c + lz * sn) * p.scale,
                                  p.position.z + (-lx * sn + lz * c) * p.scale};
                s.lo = glm::min(s.lo, w);
                s.hi = glm::max(s.hi, w);
            }
        } else {
            // The circle, as before: right for a tree, and the only thing a
            // caller that predates boxes can supply.
            s.lo = {p.position.x - s.radius, p.position.z - s.radius};
            s.hi = {p.position.x + s.radius, p.position.z + s.radius};
        }
    }

    /// How deep two footprints interpenetrate in xz, metres. <= 0 = apart.
    const auto penetration = [&sizes](std::size_t a, std::size_t b) {
        const float x = std::min(sizes[a].hi.x, sizes[b].hi.x)
                      - std::max(sizes[a].lo.x, sizes[b].lo.x);
        const float z = std::min(sizes[a].hi.y, sizes[b].hi.y)
                      - std::max(sizes[a].lo.y, sizes[b].lo.y);
        return std::min(x, z);
    };

    for (std::size_t i = 0; i < doc.placements.size(); ++i) {
        const Placement& p = doc.placements[i];
        const Sized& s = sizes[i];
        if (!s.known) {
            found.push_back({SceneRule::KnownObject, i, p.object, 0.0f,
                             "no object by this name in the registry"});
            continue; // every other rule needs its size
        }

        // STANDS ON SOMETHING. The rule the user asked this tool for: an
        // object whose bottom is not where its support is hovers or is buried,
        // and both look like a modelling defect from inside the game.
        //
        // The support is the TERRAIN, or — for a member of a group — the top
        // of another member it sits over. Without that second half a house
        // could never pass: every beam above the sill would read as hovering,
        // the report would be all noise, and a report nobody reads is a rule
        // that guards nothing.
        //
        // The ORIGIN is what is measured, not the lowest vertex: an object's
        // origin is its footing by convention (a tree's roots dive below it on
        // purpose, a beam's origin is its bottom face). Measuring the lowest
        // vertex instead would report every rooted tree as buried.
        const float ground = world.ground_at(world.ctx, {p.position.x, p.position.z});
        const float bottom = p.position.y;
        float support = ground;
        std::string on = "the ground";
        if (!p.group.empty()) {
            for (std::size_t j = 0; j < doc.placements.size(); ++j) {
                if (j == i || doc.placements[j].group != p.group || !sizes[j].known
                    || !sizes[j].has_top) {
                    continue;
                }
                const float top_j = doc.placements[j].position.y + sizes[j].top;
                // SET INTO, not standing on. A window is carried by the wall
                // it is cut into, not by anything beneath it; so is a door, and
                // so is a beam let into a post. When another member's own
                // vertical span contains this part's foot, that member holds
                // it — asking what is UNDER a window would report every window
                // in the world as hovering.
                if (doc.placements[j].position.y <= bottom && top_j > bottom
                    && penetration(i, j) > 0.0f) {
                    support = bottom;
                    on = doc.placements[j].object;
                    break;
                }
                if (top_j > bottom + limits.ground_tolerance_m || top_j <= support) {
                    continue; // above this part, or lower than what we have
                }
                // ANY footprint contact counts as support, deliberately
                // generous: a beam that spans two posts must count as resting
                // on them, and the cost of being generous is a missed hover,
                // while the cost of being strict is every carpentry joint
                // reported as a defect.
                if (penetration(i, j) > 0.0f) {
                    support = top_j;
                    on = doc.placements[j].object;
                }
            }
        }
        const float gap = bottom - support;
        const float allow_down = (!p.group.empty() && on == "the ground")
                                   ? limits.bury_tolerance_m
                                   : limits.ground_tolerance_m;
        if (gap > limits.ground_tolerance_m || gap < -allow_down) {
            found.push_back({SceneRule::OnGround, i, p.object, gap,
                             (gap > 0.0f ? "hovers above " : "is buried in ") + on});
        }

        // INSIDE THE MAP, measured with the object's own footprint — an oak
        // whose origin is inside but whose crown hangs over the edge is still
        // half off the world.
        if (doc.world_span_m > 0.0f) {
            const float lo = limits.edge_margin_m + s.radius;
            const float hi = doc.world_span_m - limits.edge_margin_m - s.radius;
            const float over_x = std::max(lo - p.position.x, p.position.x - hi);
            const float over_z = std::max(lo - p.position.z, p.position.z - hi);
            const float over = std::max(over_x, over_z);
            if (over > 0.0f) {
                found.push_back({SceneRule::InsideBounds, i, p.object, over,
                                 "reaches past the map edge"});
            }
        }
    }

    // NOT INSIDE EACH OTHER. Slack because crowns legitimately mingle in a
    // wood; what this catches is two trunks in one hole.
    for (std::size_t i = 0; i < doc.placements.size(); ++i) {
        if (!sizes[i].known) continue;
        for (std::size_t j = i + 1; j < doc.placements.size(); ++j) {
            if (!sizes[j].known) continue;
            // Two members of one built thing are ALLOWED to interpenetrate:
            // that is what a joint is. The rule still guards everything else,
            // including one house standing inside another.
            if (!doc.placements[i].group.empty()
                && doc.placements[i].group == doc.placements[j].group) {
                continue;
            }
            const float deep = penetration(i, j);
            if (deep > limits.overlap_slack_m) {
                found.push_back({SceneRule::NoOverlap, i, doc.placements[i].object,
                                 deep - limits.overlap_slack_m,
                                 "stands inside " + doc.placements[j].object});
            }
        }
    }
    return found;
}

std::size_t fix_scene_ground(SceneDoc& doc, const SceneWorld& world,
                             const SceneLimits& limits) {
    if (world.ground_at == nullptr) {
        return 0;
    }
    std::size_t moved = 0;
    for (Placement& p : doc.placements) {
        // NEVER a member of a group: sitting a house's rafters on the terrain
        // would demolish the house. What rests on what inside a built thing is
        // the composer's design, and a repair tool does not get a vote on it.
        if (!p.group.empty()) {
            continue;
        }
        const float ground = world.ground_at(world.ctx, {p.position.x, p.position.z});
        if (std::fabs(p.position.y - ground) > limits.ground_tolerance_m) {
            p.position.y = ground;
            ++moved;
        }
    }
    return moved;
}

std::vector<std::string> split_shelves(const std::string& list) {
    std::vector<std::string> out;
    for (std::size_t at = 0; at <= list.size();) {
        const std::size_t sep = list.find(';', at);
        const std::size_t end = sep == std::string::npos ? list.size() : sep;
        std::string one = trim(std::string_view(list).substr(at, end - at));
        if (!one.empty()) {
            out.push_back(std::move(one));
        }
        if (sep == std::string::npos) {
            break;
        }
        at = sep + 1;
    }
    return out;
}

std::string describe(const SceneFinding& f) {
    const char* rule = "?";
    switch (f.rule) {
    case SceneRule::OnGround: rule = "on-ground"; break;
    case SceneRule::InsideBounds: rule = "inside-bounds"; break;
    case SceneRule::NoOverlap: rule = "no-overlap"; break;
    case SceneRule::KnownObject: rule = "known-object"; break;
    }
    char buf[256];
    std::snprintf(buf, sizeof(buf), "[%s] #%zu %s: %s (%+.2f m)", rule,
                  f.placement_index, f.object.c_str(), f.detail.c_str(),
                  static_cast<double>(f.amount_m));
    return buf;
}

SolidReport check_panel_solid(const std::vector<glm::vec3>& positions,
                              const std::vector<uint32_t>& indices, float step_m,
                              float rim_m) {
    SolidReport report;
    if (positions.empty() || indices.size() < 3) {
        return report;
    }
    glm::vec3 lo{1e9f};
    glm::vec3 hi{-1e9f};
    for (const glm::vec3& p : positions) {
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    const glm::vec3 ext = hi - lo;
    int n_axis = 0;
    if (ext.y < ext[static_cast<glm::length_t>(n_axis)]) {
        n_axis = 1;
    }
    if (ext.z < ext[static_cast<glm::length_t>(n_axis)]) {
        n_axis = 2;
    }
    report.normal_axis = static_cast<uint8_t>(n_axis);
    const int a_axis = n_axis == 0 ? 1 : 0;
    const int b_axis = n_axis == 2 ? 1 : 2;
    glm::vec3 dir{0.0f};
    dir[static_cast<glm::length_t>(n_axis)] = 1.0f;
    const float t_max = ext[static_cast<glm::length_t>(n_axis)] + 2.0f;
    const float a0 = lo[static_cast<glm::length_t>(a_axis)] + rim_m;
    const float a1 = hi[static_cast<glm::length_t>(a_axis)] - rim_m;
    const float b0 = lo[static_cast<glm::length_t>(b_axis)] + rim_m;
    const float b1 = hi[static_cast<glm::length_t>(b_axis)] - rim_m;
    for (float a = a0; a <= a1; a += step_m) {
        for (float b = b0; b <= b1; b += step_m) {
            math::Ray ray;
            ray.origin[static_cast<glm::length_t>(n_axis)] =
                lo[static_cast<glm::length_t>(n_axis)] - 1.0f;
            ray.origin[static_cast<glm::length_t>(a_axis)] = a;
            ray.origin[static_cast<glm::length_t>(b_axis)] = b;
            ray.direction = dir;
            ++report.rays_cast;
            bool hit = false;
            for (std::size_t k = 0; k + 2 < indices.size(); k += 3) {
                if (math::ray_vs_triangle(ray, positions[indices[k]],
                                          positions[indices[k + 1]],
                                          positions[indices[k + 2]], t_max)) {
                    hit = true;
                    break;
                }
            }
            if (!hit) {
                if (report.rays_through == 0) {
                    glm::vec3 at = ray.origin;
                    at[static_cast<glm::length_t>(n_axis)] =
                        (lo[static_cast<glm::length_t>(n_axis)]
                         + hi[static_cast<glm::length_t>(n_axis)]) * 0.5f;
                    report.first_hole = at;
                }
                ++report.rays_through;
            }
        }
    }
    return report;
}

} // namespace dfn::world
