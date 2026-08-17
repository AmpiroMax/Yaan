/*
Created: 15:08:2026 - 16:24:04
Last updated: 17:08:2026 - 13:14:56
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
- 17:08:2026 - 11:35:28: чтение и запись [pad].
- 17:08:2026 - 12:33:08: реализация OffPath (пять проб по следу, а не по началу: дуб в двух
  метрах от полотна всё равно роняет на него крону) и OutsideBuildings
  (след постройки — ОБЪЕДИНЕНИЕ следов её деталей, а не выпуклая оболочка: у
  Г-образного дома в выемке двор, и бочка там стоять может).
- 17:08:2026 - 12:49:26: правила соединителей JointSeat/JointAngle (зона домов, HOUSES.md §5;
  правка чужого файла — исключение правила 26, только добавления). Торец
  панели мерится ПО УГЛАМ (середина внутри ничего не говорит об углах);
  стойку торцу даёт только СВОЯ группа (чужой столб ничего не связывает);
  угол мерится от граней САМОЙ стойки (её yaw), допуск выведен из ширины
  панели: atan(((w_f - T)/2) / r_in), а грань уже панели — дефект на любом
  угле. Панель/стойка узнаются по имени (wall-*, joint-*-dNN-nX-*).
- 17:08:2026 - 13:14:56: чтение и запись [river].
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
#include <map>
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

/// A joint post's working properties, read from its registry NAME
/// (joint-<mat>-d<cm>-n<4|6|8|r>-...). The name carries them by the kit's own
/// rule — «число граней входит в имя, чтобы композитор видел ограничение, не
/// открывая файл» — and the judge reads the same contract the composer does,
/// with no dependency on the forge.
struct JointName {
    float r_in_m = 0.0f; ///< inscribed (across-flats) radius
    int facets = 0;      ///< 4/6/8; 0 = round, any angle
};

[[nodiscard]] bool parse_joint_name(const std::string& name, JointName& out) {
    if (name.rfind("joint-", 0) != 0) {
        return false;
    }
    const std::size_t d_at = name.find("-d");
    if (d_at == std::string::npos) {
        return false;
    }
    int cm = 0;
    const char* first = name.data() + d_at + 2;
    const char* last = name.data() + name.size();
    const auto [ptr, ec] = std::from_chars(first, last, cm);
    if (ec != std::errc{} || cm <= 0) {
        return false;
    }
    out.r_in_m = static_cast<float>(cm) * 0.005f;
    if (name.find("-n4-", d_at) != std::string::npos) {
        out.facets = 4;
    } else if (name.find("-n6-", d_at) != std::string::npos) {
        out.facets = 6;
    } else if (name.find("-n8-", d_at) != std::string::npos) {
        out.facets = 8;
    } else if (name.find("-nr-", d_at) != std::string::npos) {
        out.facets = 0;
    } else {
        return false;
    }
    return true;
}

/// Is this placement a WALL PANEL — the thing whose ends the connector rules
/// judge? By name prefix, same contract as the joints: every kit panel and
/// every baked wall assembly is named wall-*.
[[nodiscard]] bool is_wall_panel_name(const std::string& name) {
    return name.rfind("wall-", 0) == 0;
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
    bool in_pad = false;
    bool in_river = false;
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
            in_pad = false;
            in_river = false;
            continue;
        }
        if (t == "[river]") {
            flush();
            in_placement = false;
            in_light = false;
            in_pad = false;
            in_river = true;
            out.rivers.emplace_back();
            continue;
        }
        if (t == "[pad]") {
            flush();
            in_placement = false;
            in_light = false;
            in_river = false;
            in_pad = true;
            out.pads.emplace_back();
            continue;
        }
        if (t == "[light]") {
            flush();
            in_placement = false;
            in_pad = false;
            in_river = false;
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
            in_pad = false;
            in_river = false;
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
        if (in_river) {
            SceneRiver& R = out.rivers.back();
            if (key == "point") {
                float x = 0.0f;
                float z = 0.0f;
                float w = 0.0f;
                if (std::sscanf(value.c_str(), "%f %f %f", &x, &z, &w) != 3) {
                    error = "line " + std::to_string(line_no)
                          + ": point wants three numbers \"x z water\"";
                    return false;
                }
                R.points.emplace_back(x, z, w);
            } else if (key == "width_m") {
                if (!parse_float(value, R.width_m)) {
                    error = "line " + std::to_string(line_no) + ": bad width_m";
                    return false;
                }
            } else if (key == "depth_m") {
                if (!parse_float(value, R.depth_m)) {
                    error = "line " + std::to_string(line_no) + ": bad depth_m";
                    return false;
                }
            } else if (key == "bank_m") {
                if (!parse_float(value, R.bank_m)) {
                    error = "line " + std::to_string(line_no) + ": bad bank_m";
                    return false;
                }
            } else if (key == "note") {
                R.note = value;
            }
            continue;
        }
        if (in_pad) {
            ScenePad& P = out.pads.back();
            const auto two = [&](glm::vec2& dst) {
                float a = 0.0f;
                float b = 0.0f;
                if (std::sscanf(value.c_str(), "%f %f", &a, &b) != 2) {
                    error = "line " + std::to_string(line_no)
                          + ": wants two numbers";
                    return false;
                }
                dst = {a, b};
                return true;
            };
            const auto one = [&](float& dst) {
                if (!parse_float(value, dst)) {
                    error = "line " + std::to_string(line_no) + ": \"" + value
                          + "\" is not a number";
                    return false;
                }
                return true;
            };
            if (key == "center") {
                if (!two(P.center)) return false;
            } else if (key == "half_extents") {
                if (!two(P.half_extents)) return false;
            } else if (key == "radius") {
                if (!one(P.radius)) return false;
            } else if (key == "blend") {
                if (!one(P.blend)) return false;
            } else if (key == "height") {
                if (!one(P.height)) return false;
            } else if (key == "note") {
                P.note = value;
            }
            continue;
        }
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
    for (const SceneRiver& R : doc.rivers) {
        out << "\n[river]\n"
            << "width_m = " << R.width_m << "\n"
            << "depth_m = " << R.depth_m << "\n"
            << "bank_m = " << R.bank_m << "\n";
        if (!R.note.empty()) {
            out << "note = " << R.note << "\n";
        }
        for (const glm::vec3& q : R.points) {
            out << "point = " << q.x << ' ' << q.y << ' ' << q.z << "\n";
        }
    }
    for (const ScenePad& P : doc.pads) {
        out << "\n[pad]\n"
            << "center = " << P.center.x << ' ' << P.center.y << "\n";
        if (P.half_extents.x > 0.0f || P.half_extents.y > 0.0f) {
            out << "half_extents = " << P.half_extents.x << ' ' << P.half_extents.y
                << "\n";
        } else {
            out << "radius = " << P.radius << "\n";
        }
        out << "blend = " << P.blend << "\n"
            << "height = " << P.height << "\n";
        if (!P.note.empty()) {
            out << "note = " << P.note << "\n";
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
        /// The same box built from the SOLID part only — what the overlap rule
        /// asks about. Equal to lo/hi when no solid box is supplied.
        glm::vec2 slo{0.0f};
        glm::vec2 shi{0.0f};
        bool known = false;
        bool has_top = false;
        bool solid = true; ///< no hook = everything is solid, as it always was
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
        if (s.known && world.object_solid != nullptr) {
            s.solid = world.object_solid(world.ctx, p.object);
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
        s.slo = s.lo;
        s.shi = s.hi;
        glm::vec2 qlo{0.0f};
        glm::vec2 qhi{0.0f};
        if (s.known && world.object_box_solid != nullptr
            && world.object_box_solid(world.ctx, p.object, qlo, qhi)) {
            const float c = std::cos(p.yaw);
            const float sn = std::sin(p.yaw);
            s.slo = glm::vec2{1e30f};
            s.shi = glm::vec2{-1e30f};
            for (int k = 0; k < 4; ++k) {
                const float lx = (k & 1) ? qhi.x : qlo.x;
                const float lz = (k & 2) ? qhi.y : qlo.y;
                const glm::vec2 w{p.position.x + (lx * c + lz * sn) * p.scale,
                                  p.position.z + (-lx * sn + lz * c) * p.scale};
                s.slo = glm::min(s.slo, w);
                s.shi = glm::max(s.shi, w);
            }
        }
    }

    /// Penetration of the SOLID footprints — what "inside each other" means.
    const auto solid_penetration = [&sizes](std::size_t a, std::size_t b) {
        const float x = std::min(sizes[a].shi.x, sizes[b].shi.x)
                      - std::max(sizes[a].slo.x, sizes[b].slo.x);
        const float z = std::min(sizes[a].shi.y, sizes[b].shi.y)
                      - std::max(sizes[a].slo.y, sizes[b].slo.y);
        return std::min(x, z);
    };

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

    // NOTHING ON A PATH. Asked of the SAME field the ground was worn by, so
    // the judge can never forbid building where the ground shows no path, nor
    // allow it where the ground shows one.
    //
    // MEASURED AT THE FOOTPRINT'S CORNERS AND CENTRE, not at the origin alone:
    // an oak whose trunk stands a metre off the tread still drops its crown
    // and its roots across it, and a wall whose corner clips the road still
    // blocks the road. Five probes are enough for the boxes this world has and
    // cheap enough to run on every placement of a two-thousand-object scene.
    if (world.path_clearance != nullptr) {
        for (std::size_t i = 0; i < doc.placements.size(); ++i) {
            if (!sizes[i].known) {
                continue;
            }
            const glm::vec2 lo = sizes[i].lo;
            const glm::vec2 hi = sizes[i].hi;
            const glm::vec2 probes[5] = {(lo + hi) * 0.5f, lo, {hi.x, lo.y},
                                         {lo.x, hi.y}, hi};
            float worst = 1e9f;
            for (const glm::vec2& p : probes) {
                float m = 1e9f;
                if (world.path_clearance(world.ctx, p, m)) {
                    worst = std::min(worst, m);
                }
            }
            if (worst < limits.path_clearance_m) {
                found.push_back({SceneRule::OffPath, i, doc.placements[i].object,
                                 limits.path_clearance_m - worst,
                                 worst < 0.0f ? "stands ON a path"
                                              : "crowds a path"});
            }
        }
    }

    // NOTHING INSIDE A BUILDING IT IS NOT PART OF. One rule, read from both
    // ends: a tree in a house and a house on a tree are the same overlap. The
    // building's footprint is the union of its members' — not a convex hull,
    // because an L-shaped house has a yard in its notch and a barrel may
    // stand there.
    {
        std::map<std::string, std::vector<std::size_t>> groups;
        for (std::size_t i = 0; i < doc.placements.size(); ++i) {
            if (sizes[i].known && !doc.placements[i].group.empty()) {
                groups[doc.placements[i].group].push_back(i);
            }
        }
        for (std::size_t i = 0; i < doc.placements.size(); ++i) {
            if (!sizes[i].known) {
                continue;
            }
            for (const auto& [name, members] : groups) {
                if (doc.placements[i].group == name) {
                    continue; // a member of the building is not intruding on it
                }
                float deepest = 0.0f;
                for (const std::size_t m : members) {
                    const float x = std::min(sizes[i].hi.x, sizes[m].hi.x)
                                  - std::max(sizes[i].lo.x, sizes[m].lo.x);
                    const float z = std::min(sizes[i].hi.y, sizes[m].hi.y)
                                  - std::max(sizes[i].lo.y, sizes[m].lo.y);
                    deepest = std::max(deepest, std::min(x, z));
                }
                if (deepest > limits.building_slack_m) {
                    found.push_back({SceneRule::OutsideBuildings, i,
                                     doc.placements[i].object,
                                     deepest - limits.building_slack_m,
                                     "stands inside \"" + name + "\""});
                }
            }
        }
    }

    // NOT INSIDE EACH OTHER. Slack because crowns legitimately mingle in a
    // wood; what this catches is two trunks in one hole.
    for (std::size_t i = 0; i < doc.placements.size(); ++i) {
        if (!sizes[i].known) continue;
        if (!sizes[i].solid) {
            continue; // ground cover: the player walks through it, so may others
        }
        for (std::size_t j = i + 1; j < doc.placements.size(); ++j) {
            if (!sizes[j].known || !sizes[j].solid) continue;
            // Two members of one built thing are ALLOWED to interpenetrate:
            // that is what a joint is. The rule still guards everything else,
            // including one house standing inside another.
            if (!doc.placements[i].group.empty()
                && doc.placements[i].group == doc.placements[j].group) {
                continue;
            }
            const float deep = solid_penetration(i, j);
            if (deep > limits.overlap_slack_m) {
                found.push_back({SceneRule::NoOverlap, i, doc.placements[i].object,
                                 deep - limits.overlap_slack_m,
                                 "stands inside " + doc.placements[j].object});
            }
        }
    }

    // СОЕДИНИТЕЛИ (зона домов, HOUSES.md §3-5): ПАНЕЛЬ НИКОГДА НЕ КАСАЕТСЯ
    // ПАНЕЛИ. Every wall panel's end must live inside a joint post of ITS OWN
    // group (a neighbour's post ties nothing), and on a faceted post the
    // panel's angle must land on a facet. Needs the local (unrotated) box, so
    // it runs only when the caller supplies object_box — the same door every
    // box-based rule above uses.
    if (world.object_box != nullptr) {
        struct JointAt {
            glm::vec2 axis{0.0f};
            float r_in = 0.0f;
            int facets = 0;
            float yaw = 0.0f;
            const std::string* group = nullptr;
        };
        std::vector<JointAt> joints;
        for (std::size_t i = 0; i < doc.placements.size(); ++i) {
            JointName jn;
            if (sizes[i].known && parse_joint_name(doc.placements[i].object, jn)) {
                const Placement& p = doc.placements[i];
                joints.push_back({{p.position.x, p.position.z},
                                  jn.r_in_m * p.scale, jn.facets, p.yaw, &p.group});
            }
        }
        for (std::size_t i = 0; i < doc.placements.size(); ++i) {
            const Placement& p = doc.placements[i];
            if (!sizes[i].known || !is_wall_panel_name(p.object)) {
                continue;
            }
            glm::vec2 blo{0.0f};
            glm::vec2 bhi{0.0f};
            if (!world.object_box(world.ctx, p.object, blo, bhi)) {
                continue;
            }
            // The kit's convention: a panel runs along its local +X, its
            // thickness is the local z extent.
            const float t_m = (bhi.y - blo.y) * p.scale;
            const float zc = (blo.y + bhi.y) * 0.5f;
            const float c = std::cos(p.yaw);
            const float sn = std::sin(p.yaw);
            // Local direction of the panel's length in the world, and the
            // lateral (thickness) direction — the same rotation every rule
            // above applies to box corners.
            const glm::vec2 along{c, -sn};
            const glm::vec2 lateral{sn, c};
            for (int end = 0; end < 2; ++end) {
                const float lx = end == 0 ? blo.x : bhi.x;
                const glm::vec2 at{p.position.x + (lx * c + zc * sn) * p.scale,
                                   p.position.z + (-lx * sn + zc * c) * p.scale};
                // The nearest joint of the same group judges this end; with
                // no joint at all the end is bare, which is the farmhouse's
                // exact defect (панели встык) and the rule's red hand.
                const JointAt* best = nullptr;
                float best_d = 1e30f;
                for (const JointAt& j : joints) {
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
                // Both vertical edges of the end face: corners at +-T/2 along
                // the lateral axis. HOUSES.md §5 names exactly these — the
                // end's MIDDLE being inside proves nothing about its corners.
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
                                  "allowed %.3f", which,
                                  static_cast<double>(worst),
                                  static_cast<double>(allowed));
                    found.push_back({SceneRule::JointSeat, i, p.object,
                                     worst - allowed, det});
                    continue;
                }
                if (best->facets <= 0) {
                    continue; // round: any angle is its rule
                }
                // Angle against the POST'S OWN facets. The tolerance is the
                // angle error at which the panel's exit band rides past the
                // facet's arris: lateral slack (w_f - T)/2 over the lever
                // r_in. A facet narrower than the panel fails at EVERY angle
                // — that pairing, not the yaw, is the defect then.
                constexpr float PI = 3.14159265359f;
                const float step = 2.0f * PI / static_cast<float>(best->facets);
                const float w_f = 2.0f * best->r_in
                                * std::tan(PI / static_cast<float>(best->facets));
                const float slack = (w_f - t_m) * 0.5f;
                if (slack <= 0.0f) {
                    char det[128];
                    std::snprintf(det, sizeof(det),
                                  "%s: facet %.3f m is narrower than the panel "
                                  "(%.3f m) — no angle can seat it", which,
                                  static_cast<double>(w_f),
                                  static_cast<double>(t_m));
                    found.push_back({SceneRule::JointAngle, i, p.object, -slack,
                                     det});
                    continue;
                }
                const float tol = std::atan(slack / best->r_in);
                // The exit direction folds onto the facet grid every `step`,
                // and every kit shape has an even facet count, so the near
                // and far ends fold identically.
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
                                  static_cast<double>(dev * 180.0f / PI),
                                  best->facets,
                                  static_cast<double>(tol * 180.0f / PI));
                    found.push_back({SceneRule::JointAngle, i, p.object, dev - tol,
                                     det});
                }
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
    case SceneRule::OffPath: rule = "off-path"; break;
    case SceneRule::OutsideBuildings: rule = "outside-buildings"; break;
    case SceneRule::JointSeat: rule = "joint-seat"; break;
    case SceneRule::JointAngle: rule = "joint-angle"; break;
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
