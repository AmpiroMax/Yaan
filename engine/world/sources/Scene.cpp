/*
Created: 15:08:2026 - 16:24:04
Last updated: 27:08:2026 - 00:12:00
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
- 17:08:2026 - 15:12:10: yaw больше полного оборота — ВСЛУХ. Зона домов потеряла заход на том,
  что генератор писал ГРАДУСЫ в поле, объявленное радианами: сцена читалась,
  все объекты вставали, и неверна она была так, что по числам этого не видно
  (90 радиан — четырнадцать оборотов). Читатель мог сказать это одной строкой
  и теперь говорит. ПРЕДУПРЕЖДЕНИЕ, а не отказ: многооборотное значение
  законно, и читатель, отвергающий его, ошибался бы чаще, чем ловил.
- 17:08:2026 - 16:30:25: ПРАВИЛА ПОСТРОЙКИ УЕХАЛИ В SceneHouseRules.cpp — целиком, вместе с
  JointSeat/JointAngle и разбором имён стойки. Не «ещё одно добавление»:
  этот файл уже стоял на 1052 строках против правила 21, а заказ 17.08
  добавляет ещё четыре правила соединителей. Здесь остался ОДИН вызов, и
  отчёт у судьи по-прежнему один. Поведение не изменилось ни на находку:
  demo (206 расстановок) и showcase (109) остаются нулевыми до и после.
- 17:08:2026 - 17:28:41: check_stair_rules() встал рядом с check_house_rules(), и describe()
  знает stair-seat/stair-headroom. Лестница — своя тема со своим прибором
  (капсула игрока на носке каждой ступени), поэтому свой файл: HOUSES.md §9.
- 17:08:2026 - 19:05:00: чтение и запись ключа `relief` (зона кистей рельефа): одна строка с
  именем сиделки .relief. Необязательный ключ, его отсутствие — прежний файл
  до последнего бита.
- 20:08:2026 - 15:30:00: Чтение и запись [house].
- 22:08:2026 - 16:20:00: чтение и запись [air] (fog_start / fog_end).
- 22:08:2026 - 20:10:00: ключ cloud в [air] (необязателен; без него -1 «не задана»).
- 22:08:2026 - 21:00:00: ключ interior у [light].
- 23:08:2026 - 01:40:00: ключ room = cx cz hx hz у [light].
- 22:08:2026 - 22:51:38: ключ softness у [light] — мягкость источника 0..1.
- 23:08:2026 - 01:17:49: ключ flicker у [light] — мерцание живого огня 0..1.
- 23:08:2026 - 22:10:00: И15 волна А, шаг 1: чтение и запись [portal] и [spawn], ключ
  interior= у [house]. И РАЗБОР СЕКЦИЙ ПЕРЕПИСАН НА ПЕРЕЧИСЛЕНИЕ. Семь
  параллельных булей требовали от автора девятой секции дописать по строке
  сброса в КАЖДУЮ из восьми чужих веток; пропущенная строка не роняет чтение —
  она кладёт ключ новой секции в предыдущую, то есть даёт разобранный,
  правдоподобный и неверный документ. Перечисление делает это состояние
  непредставимым: секция — одно значение, присваивается один раз. Рука дозы 0
  (правило 47) в tests/core/SceneTests.cpp: боевой Вайтран читается без единого
  портала и без единого interior=, а два прохода записи сходятся побайтово.
- 23:08:2026 - 23:40:00: ключ ambient в [air] — общий свет локации (И15 шаг 4).
- 27:08:2026 - 00:12:00: ключ sealed у [house] (И15 волна Б): створка входит в
  коллайдер. Только добавление; сцена без ключа пишется и читается как раньше.
*/

#include "engine/world/sources/Scene.h"

#include "engine/world/sources/SceneHouseRules.h"
#include "engine/world/sources/SceneStairRules.h"

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

/// WHICH SECTION THE READER IS INSIDE. One value, not one bool per section.
///
/// The predecessor was seven parallel bools, and the cost was paid by whoever
/// added the EIGHTH section: every one of the seven existing branches had to
/// grow a line turning the newcomer off, and a forgotten line does not fail
/// loudly — it files the new section's keys into the previous section, which
/// parses, looks plausible and is wrong. Nine sections is 72 such lines; the
/// enum is one assignment.
enum class Section : uint8_t {
    Header, Place, River, Pad, Light, House, Air, Spawn, Portal, Unknown
};

} // namespace

bool portal_is_back(const ScenePortal& p) { return p.to == "^back"; }

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
    Section section = Section::Header;
    const auto flush = [&] {
        if (section == Section::Place && !current.object.empty()) {
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
        if (t.front() == '[' && t.back() == ']') {
            flush();
            if (t == "[place]") {
                section = Section::Place;
            } else if (t == "[river]") {
                section = Section::River;
                out.rivers.emplace_back();
            } else if (t == "[pad]") {
                section = Section::Pad;
                out.pads.emplace_back();
            } else if (t == "[light]") {
                section = Section::Light;
                out.lights.emplace_back();
            } else if (t == "[house]") {
                section = Section::House;
                out.houses.emplace_back();
            } else if (t == "[air]") {
                section = Section::Air;
                out.air.set = true;
            } else if (t == "[spawn]") {
                section = Section::Spawn;
                out.spawns.emplace_back();
            } else if (t == "[portal]") {
                section = Section::Portal;
                out.portals.emplace_back();
            } else {
                // ANY OTHER SECTION IS SKIPPED, not fatal. The format grows and
                // a reader that dies on tomorrow's section cannot read today's
                // file written by a newer tool. Same stance the unknown-KEY
                // rule takes, and it has to be the same stance or the promise
                // is only half kept. THIS IS THE CIRCULAR-RUN GUARANTEE the
                // interior wave leans on: yesterday's binary opens a scene
                // carrying [portal] and [spawn] and simply sees a city.
                //
                // ITS KEYS ARE SKIPPED TOO, and that is a CHANGE from the seven
                // bools, which turned themselves all off and thereby fed the
                // unknown section's keys to the HEADER — so a future section
                // with a `spawn` key would have silently moved the player.
                // No shipped scene has an unknown section, so the change is
                // invisible today and removes tomorrow's silent misparse.
                section = Section::Unknown;
            }
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
        // A three-number "x y z" value, said once (four sections want it).
        const auto three = [&](glm::vec3& dst, const char* what) {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            if (std::sscanf(value.c_str(), "%f %f %f", &x, &y, &z) != 3) {
                error = "line " + std::to_string(line_no) + ": " + what
                      + " wants three numbers \"x y z\"";
                return false;
            }
            dst = {x, y, z};
            return true;
        };
        if (section == Section::Unknown) {
            continue;
        }
        if (section == Section::Spawn) {
            SceneSpawn& S = out.spawns.back();
            if (key == "name") {
                S.name = value;
            } else if (key == "pos") {
                if (!three(S.position, "pos")) return false;
            } else if (key == "yaw") {
                if (!number(S.yaw)) return false;
            } else if (key == "note") {
                S.note = value;
            }
            continue;
        }
        if (section == Section::Portal) {
            ScenePortal& P = out.portals.back();
            if (key == "at") {
                if (!three(P.at, "at")) return false;
            } else if (key == "radius_m") {
                if (!number(P.radius_m)) return false;
            } else if (key == "to") {
                P.to = value;
            } else if (key == "to_spawn") {
                P.to_spawn = value;
            } else if (key == "note") {
                P.note = value;
            }
            continue;
        }
        if (section == Section::Air) {
            SceneAir& A = out.air;
            if (key == "fog_start") {
                if (!number(A.fog_start_m)) {
                    return false;
                }
            } else if (key == "fog_end") {
                if (!number(A.fog_end_m)) {
                    return false;
                }
            } else if (key == "cloud") {
                if (!number(A.cloud_cover)) {
                    return false;
                }
            } else if (key == "ambient") {
                if (!number(A.ambient)) {
                    return false;
                }
            }
            // Неизвестный ключ пропускается — та же позиция, что у секций.
            continue;
        }
        if (section == Section::House) {
            ScenePlacedHouse& H = out.houses.back();
            if (key == "file") {
                H.file = value;
            } else if (key == "pos") {
                float x = 0.0f;
                float y = 0.0f;
                float z = 0.0f;
                if (std::sscanf(value.c_str(), "%f %f %f", &x, &y, &z) != 3) {
                    error = "line " + std::to_string(line_no)
                          + ": pos wants three numbers \"x y z\"";
                    return false;
                }
                H.position = {x, y, z};
            } else if (key == "yaw") {
                if (!parse_float(value, H.yaw)) {
                    error = "line " + std::to_string(line_no) + ": bad yaw";
                    return false;
                }
            } else if (key == "interior") {
                H.interior = value;
            } else if (key == "sealed") {
                H.sealed = value == "1" || value == "true" || value == "yes";
            } else if (key == "note") {
                H.note = value;
            }
            continue;
        }
        if (section == Section::River) {
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
        if (section == Section::Pad) {
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
        if (section == Section::Light) {
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
            } else if (key == "interior") {
                L.interior = value == "1" || value == "true" || value == "yes";
            } else if (key == "softness") {
                L.softness = std::clamp(std::strtof(value.c_str(), nullptr),
                                        0.0f, 1.0f);
            } else if (key == "flicker") {
                L.flicker = std::clamp(std::strtof(value.c_str(), nullptr),
                                       0.0f, 1.0f);
            } else if (key == "room") {
                float cx = 0.0f, cz = 0.0f, hx = 0.0f, hz = 0.0f;
                if (std::sscanf(value.c_str(), "%f %f %f %f",
                                &cx, &cz, &hx, &hz) != 4) {
                    error = "line " + std::to_string(line_no)
                          + ": room ждёт четыре числа «cx cz hx hz»";
                    return false;
                }
                L.room_center = {cx, cz};
                L.room_half = {hx, hz};
            } else if (key == "note") {
                L.note = value;
            }
            continue;
        }
        if (section != Section::Place) {
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
            } else if (key == "relief") {
                out.relief = value;
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
            // A YAW BIGGER THAN A FULL TURN IS ALMOST CERTAINLY DEGREES.
            // The field is radians and says so, but a generator that wrote 90
            // instead of 1.5708 produces a scene that PARSES, places every
            // object, and is wrong in a way no one can see from the numbers —
            // 90 radians is fourteen turns, and it lands wherever it lands.
            // It cost a whole session to find, on a hand-built control box,
            // and the reader could have said it in one line.
            //
            // A WARNING, NOT A REFUSAL: a turntable animation or a deliberate
            // multi-turn value is legal, and a reader that refused it would be
            // wrong more often than the mistake it guards against.
            if (std::fabs(current.yaw) > 6.2832f) {
                std::fprintf(stderr,
                             "[scene] line %d: yaw = %g. The field is RADIANS "
                             "(%.0f full turns). Did you mean %g degrees, i.e. "
                             "%g radians?\n", line_no,
                             static_cast<double>(current.yaw),
                             static_cast<double>(std::fabs(current.yaw) / 6.2832f),
                             static_cast<double>(current.yaw),
                             static_cast<double>(current.yaw * 3.14159265f / 180.0f));
            }
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
    if (!doc.relief.empty()) {
        out << "relief = " << doc.relief << "\n";
    }
    if (doc.has_spawn) {
        out << "spawn = " << doc.spawn.x << ' ' << doc.spawn.y << ' ' << doc.spawn.z
            << "\n"
            << "spawn_yaw = " << doc.spawn_yaw << "\n";
    }
    // НАЗВАННЫЕ ТОЧКИ ВХОДА — сразу за заголовком: их читает человек, который
    // открыл файл, чтобы понять, куда эта локация впускает.
    for (const SceneSpawn& S : doc.spawns) {
        out << "\n[spawn]\n"
            << "name = " << S.name << "\n"
            << "pos = " << S.position.x << ' ' << S.position.y << ' '
            << S.position.z << "\n"
            << "yaw = " << S.yaw << "\n";
        if (!S.note.empty()) {
            out << "note = " << S.note << "\n";
        }
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
    for (const ScenePlacedHouse& H : doc.houses) {
        out << "\n[house]\n"
            << "file = " << H.file << "\n"
            << "pos = " << H.position.x << ' ' << H.position.y << ' '
            << H.position.z << "\n"
            << "yaw = " << H.yaw << "\n";
        if (!H.interior.empty()) {
            out << "interior = " << H.interior << "\n";
        }
        if (H.sealed) {
            out << "sealed = 1\n";
        }
        if (!H.note.empty()) {
            out << "note = " << H.note << "\n";
        }
    }
    // ПОРТАЛЫ ПОСЛЕ ПОСТРОЕК: дверь читается после дома, в который она ведёт.
    for (const ScenePortal& P : doc.portals) {
        out << "\n[portal]\n"
            << "at = " << P.at.x << ' ' << P.at.y << ' ' << P.at.z << "\n"
            << "radius_m = " << P.radius_m << "\n"
            << "to = " << P.to << "\n";
        if (!P.to_spawn.empty()) {
            out << "to_spawn = " << P.to_spawn << "\n";
        }
        if (!P.note.empty()) {
            out << "note = " << P.note << "\n";
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
        if (L.interior) {
            out << "interior = 1\n";
        }
        if (L.softness > 0.0f) {
            out << "softness = " << L.softness << "\n";
        }
        if (L.flicker > 0.0f) {
            out << "flicker = " << L.flicker << "\n";
        }
        if (L.room_half.x > 0.0f || L.room_half.y > 0.0f) {
            out << "room = " << L.room_center.x << ' ' << L.room_center.y << ' '
                << L.room_half.x << ' ' << L.room_half.y << "\n";
        }
        if (!L.note.empty()) {
            out << "note = " << L.note << "\n";
        }
    }
    if (doc.air.set) {
        out << "\n[air]\n"
            << "fog_start = " << doc.air.fog_start_m << "\n"
            << "fog_end = " << doc.air.fog_end_m << "\n";
        if (doc.air.cloud_cover >= 0.0f) {
            out << "cloud = " << doc.air.cloud_cover << "\n";
        }
        if (doc.air.ambient >= 0.0f) {
            out << "ambient = " << doc.air.ambient << "\n";
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

    // ПРАВИЛА ПОСТРОЙКИ (зона домов, HOUSES.md §3-5 и §8) живут в своём файле:
    // они переросли этот, а этот перерос правило 21 ещё до них. Один вызов —
    // один отчёт: судья у сцены остаётся один.
    check_house_rules(doc, world, limits, found);
    check_stair_rules(doc, world, limits, found);

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
    case SceneRule::WallTwoJoints: rule = "wall-two-joints"; break;
    case SceneRule::JointCapacity: rule = "joint-capacity"; break;
    case SceneRule::DeckOnJoints: rule = "deck-on-joints"; break;
    case SceneRule::RoofSeat: rule = "roof-seat"; break;
    case SceneRule::StairSeat: rule = "stair-seat"; break;
    case SceneRule::StairHeadroom: rule = "stair-headroom"; break;
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
