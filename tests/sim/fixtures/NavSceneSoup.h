/*
Module: tests
File: tests/sim/fixtures/NavSceneSoup.h

Responsibility:
- ГЕОМЕТРИЯ СЦЕНЫ БЕЗ ОКНА для приборов сетки проходимости: дома сцены
  (.dfh через read_house + build_house_mesh, в мировые координаты по
  конвенции сцены), площадки [pad] как функция высоты рельефа. Тот же путь,
  что у SeatApproachTests: правило 17b — карту мира не открывать.

Key items:
- NavSceneSoup: позиции, индексы, площадки, охват, spawn.
- load_scene_soup(): читает .scene, собирает суп в мировых координатах.
- pad_ground(): высота по площадкам — самая низкая площадка как «натуральная»
  земля, каждая выше поднимает с плавным спадом на blend, объединение по max.
- soup_input(): NavInput поверх супа с агентом из реестра.

Dependencies:
- Uses: engine/world (Scene.h, HouseFile.h, HouseMesh.h), engine/gameplay
  (NavGrid.h), glm, std.
- Used by: tests/sim/NavGridTests.cpp, NavPathTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Это фикстура прибора, не второй сборщик геометрии приложения: в
  приложении NavInput — выход того же сборщика, что и коллайдер (условие 1
  синка 11.09); здесь — минимальный путь до тех же треугольников без App.
*/
#pragma once

#include "engine/gameplay/sources/NavGrid.h"
#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseGraph.h"
#include "engine/world/sources/HouseMesh.h"
#include "engine/world/sources/Scene.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace dfn::navtest {

struct NavSceneSoup {
    std::vector<glm::vec3> positions;
    std::vector<uint32_t> indices;
    std::vector<world::ScenePad> pads;
    std::vector<world::SceneSpawn> spawns;
    glm::vec2 min_xz{0.0f}, max_xz{0.0f};
    std::size_t houses = 0;
    std::size_t houses_read = 0;
    float natural_h = 0.0f;
    gameplay::NavMeshSoup soup; ///< указывает на positions/indices
};

inline float smoothstep01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float pad_ground(void* ctx, glm::vec2 p) {
    const auto* s = static_cast<const NavSceneSoup*>(ctx);
    float h = s->natural_h;
    for (const world::ScenePad& pad : s->pads) {
        float outside;
        if (pad.half_extents.x > 0.0f || pad.half_extents.y > 0.0f) {
            const glm::vec2 d = glm::abs(p - pad.center) - pad.half_extents;
            outside = glm::length(glm::max(d, glm::vec2{0.0f}));
        } else {
            outside = std::max(0.0f, glm::length(p - pad.center) - pad.radius);
        }
        const float w = pad.blend > 0.0f ? 1.0f - smoothstep01(outside / pad.blend) : (outside <= 0.0f ? 1.0f : 0.0f);
        h = std::max(h, s->natural_h + (pad.height - s->natural_h) * w);
    }
    return h;
}

/// Читает сцену; ложь — файл не прочитан. Дома без чертежа пропускаются
/// (считаются в houses, не в houses_read).
inline bool load_scene_soup(const std::string& path, NavSceneSoup& out, float margin_m = 4.0f) {
    out = NavSceneSoup{};
    world::SceneDoc doc;
    std::string err;
    if (!world::read_scene(path, doc, err)) {
        return false;
    }
    out.pads = doc.pads;
    out.spawns = doc.spawns;
    out.houses = doc.houses.size();
    std::map<std::string, world::HouseMesh> meshes;
    glm::vec2 lo{1.0e9f}, hi{-1.0e9f};
    for (const world::ScenePlacedHouse& H : doc.houses) {
        auto it = meshes.find(H.file);
        if (it == meshes.end()) {
            std::ifstream in(H.file);
            if (!in) {
                continue;
            }
            std::stringstream ss;
            ss << in.rdbuf();
            world::HouseGraph g;
            if (!world::read_house(ss.str(), g).ok) {
                continue;
            }
            it = meshes.emplace(H.file, world::build_house_mesh(g)).first;
        }
        const world::HouseMesh& m = it->second;
        const float c = std::cos(H.yaw);
        const float sn = std::sin(H.yaw);
        const uint32_t base = static_cast<uint32_t>(out.positions.size());
        for (const world::HouseVertex& v : m.vertices) {
            const glm::vec3 l = v.pos;
            const glm::vec3 w = H.position + glm::vec3{l.x * c + l.z * sn, l.y, -l.x * sn + l.z * c};
            out.positions.push_back(w);
            lo = glm::min(lo, glm::vec2{w.x, w.z});
            hi = glm::max(hi, glm::vec2{w.x, w.z});
        }
        for (const uint32_t i : m.indices) {
            out.indices.push_back(base + i);
        }
        ++out.houses_read;
    }
    out.natural_h = 1.0e9f;
    for (const world::ScenePad& pad : out.pads) {
        out.natural_h = std::min(out.natural_h, pad.height);
        const glm::vec2 he = pad.half_extents.x > 0.0f ? pad.half_extents : glm::vec2{pad.radius};
        lo = glm::min(lo, pad.center - he - glm::vec2{pad.blend});
        hi = glm::max(hi, pad.center + he + glm::vec2{pad.blend});
    }
    if (out.natural_h == 1.0e9f) {
        out.natural_h = 0.0f;
    }
    if (lo.x > hi.x) {
        lo = glm::vec2{0.0f};
        hi = glm::vec2{doc.world_span_m > 0.0f ? doc.world_span_m : 256.0f};
    }
    out.min_xz = lo - glm::vec2{margin_m};
    out.max_xz = hi + glm::vec2{margin_m};
    out.soup = gameplay::NavMeshSoup{out.positions, out.indices};
    return true;
}

/// Габарит чертежа дома в его координатах (для приборов: «стол по чертежу»).
inline bool house_bounds(const std::string& file, glm::vec3& lo, glm::vec3& hi) {
    std::ifstream in(file);
    if (!in) {
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    world::HouseGraph g;
    if (!world::read_house(ss.str(), g).ok) {
        return false;
    }
    const world::HouseMesh m = world::build_house_mesh(g);
    if (m.vertices.empty()) {
        return false;
    }
    lo = glm::vec3{1.0e9f};
    hi = glm::vec3{-1.0e9f};
    for (const world::HouseVertex& v : m.vertices) {
        lo = glm::min(lo, v.pos);
        hi = glm::max(hi, v.pos);
    }
    return true;
}

inline gameplay::NavInput soup_input(NavSceneSoup& s) {
    gameplay::NavInput in;
    in.meshes = std::span<const gameplay::NavMeshSoup>{&s.soup, 1};
    in.ground = &pad_ground;
    in.ground_ctx = &s;
    in.min_xz = s.min_xz;
    in.max_xz = s.max_xz;
    in.agent = gameplay::nav_agent_from_registry();
    return in;
}

} // namespace dfn::navtest
