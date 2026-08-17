/*
Created: 15:08:2026 - 16:24:04
Last updated: 17:08:2026 - 13:14:56
Module: tools
File: tools/check_scene.cpp

Responsibility:
- dfn_scene_check: judges a composed map (.scene) against the rules in
  engine/world/sources/Scene.h, measuring against the REAL generated ground
  and the REAL registry objects. Exit code 1 when anything is wrong, so it can
  stand in a build or a hook.

Usage:
    dfn_scene_check <file.scene> [--stand Testbed|Forest|OneTree|Gallery]
                                 [--objects <dir>] [--fix]

  Run from the repo ROOT (it opens the shipped layout asset by relative path,
  exactly as the app and the tests do).

Dependencies:
- Uses: engine/world (Scene, Worldgen, layout), engine/render (ObjectRegistry).
- Used by: agents composing maps, the human, and whoever wires it into CI.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- MEASURE, NEVER GUESS: the ground comes from the generator and the object
  extents from the registry file. A checker that assumed a tree is "about 5 m"
  would pass a colossus and fail a shrub, and its report would be worth
  nothing.
- --fix moves objects onto the ground and NOTHING else, and it says how many
  it moved. Anything cleverer belongs in a tool a human watches.
*/
/*
UPD:
- 15:08:2026 - 16:24:04: Created — the composition tool's first half (the
  rules), per the user's ask for an application where agents and humans lay
  out the world by its own rules.
- 16:08:2026 - 21:08:52: Мерка детали расширена до верха и следа (object_top/object_box) — из той
  же сетки, что и всё остальное: высота опоры, вписанная руками, протухает
  первой при перековке детали. И --ground <x> <z> [<пролёт>]: строителю нужна
  земля ДО того, как он напишет, где лежит подошва, а --fix ему не поможет — он
  членов групп не двигает.
- 16:08:2026 - 21:50:43: --objects принимает список полок через ';' — судья обязан видеть ровно
  те же полки, что и игра, иначе он судит другую сцену.
- 16:08:2026 - 22:11:51: --shell — прибор ЗАМКНУТОЙ ОБОЛОЧКИ (правило зоны домов по
  замечанию пользователя: «стены несплошные, дырки в доме»). Из пробной точки
  ВНУТРИ каждой группы пускается веер лучей по сфере Фибоначчи; луч, ушедший
  из габарита постройки, не встретив ни одного треугольника её же деталей, —
  это сквозная дыра. Ноль ушедших — зелено. Меряется НАСТОЯЩАЯ геометрия
  (.dfo с полок, с yaw/scale размещения), а не имена и не габариты: щель в
  1.7 см между досками прибор видит, а глаз на кадре с одной стены — нет.
  Лучи ниже горизонта на -0.15 не пускаются: земляной пол — не дыра.
- 16:08:2026 - 22:45:34: --solid <группа> — просвет насквозь для сборок-панелей; сама
  проверка живёт в engine/world (check_panel_solid), судья лишь собирает
  суп с полок и печатает счёт и адрес дыры.
- 17:08:2026 - 11:35:28: судья мерит ТЕРРАСИРОВАННУЮ землю: без этого каждый дом на полке
  читался бы как закопанный на глубину самой полки, и отчёт стал бы шумом ровно
  там, где инструмент нужнее всего. И --ground с указанной сценой отвечает
  ПОСЛЕ её площадок (передай "-", чтобы спросить натуральную).
- 17:08:2026 - 12:33:08: судья спрашивает ТУ ЖЕ сеть троп, которой продавлена земля; «твёрдость»
  считается как высота твёрдой геометрии над PLAYER_STEP_HEIGHT — первый вариант
  «есть поток дерева» был побит настоящими данными: у пучка травы есть корневой
  пенёк в дереве, и каждая травинка становилась препятствием.
- 17:08:2026 - 13:14:56: судья мерит землю с врезанными руслами.
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/ProcMesh.h"
#include "engine/world/sources/LayoutLoad.h"
#include "engine/world/sources/Scene.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/world/sources/WorldgenForest.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <glm/geometric.hpp>
#include <filesystem>
#include <glm/common.hpp>
#include <map>
#include <optional>
#include <vector>
#include <string>

namespace {

struct Ctx {
    const dfn::world::WorldGenContext* gen = nullptr;
    /// Shelves, in the order the caller gave them; first hit wins. Several,
    /// because a town scene stands on parts, props and trees at once.
    std::vector<std::filesystem::path> shelves;
    /// name -> (radius, bottom, top). Memoised: a scene of 200 trees — or of
    /// 200 identical beams in one house — would otherwise read and hash the
    /// same .dfo two hundred times.
    struct Extent {
        float radius = 0.0f;
        float bottom = 0.0f;
        float top = 0.0f;
        bool solid = false; ///< has wood or bark: the streams a body is built from
        glm::vec2 lo{0.0f};   ///< local xz footprint, about the origin
        glm::vec2 hi{0.0f};
        glm::vec2 slo{0.0f};  ///< the same, of the SOLID streams only
        glm::vec2 shi{0.0f};
    };
    std::map<std::string, Extent> extents;
};

float ground_at(void* ctx, glm::vec2 p) {
    const auto* c = static_cast<Ctx*>(ctx);
    return dfn::world::terrain_height(*c->gen, p);
}

const Ctx::Extent* measure(Ctx* c, const std::string& name) {
    if (const auto it = c->extents.find(name); it != c->extents.end()) {
        return &it->second;
    }
    std::optional<dfn::render::RegistryObject> obj;
    for (const auto& shelf : c->shelves) {
        obj = dfn::render::read_object(shelf / (name + ".dfo"));
        if (obj) {
            break;
        }
    }
    if (!obj) {
        return nullptr;
    }
    Ctx::Extent e;
    const auto scan = [&](const dfn::render::MeshData& mesh) {
        for (const dfn::platform::Vertex& v : mesh.vertices) {
            e.radius = std::max(e.radius, std::sqrt(v.position.x * v.position.x
                                                    + v.position.z * v.position.z));
            e.bottom = std::min(e.bottom, v.position.y);
            e.top = std::max(e.top, v.position.y);
            e.lo = glm::min(e.lo, glm::vec2{v.position.x, v.position.z});
            e.hi = glm::max(e.hi, glm::vec2{v.position.x, v.position.z});
        }
    };
    const auto scan_solid = [&](const dfn::render::MeshData& mesh) {
        for (const dfn::platform::Vertex& v : mesh.vertices) {
            e.slo = glm::min(e.slo, glm::vec2{v.position.x, v.position.z});
            e.shi = glm::max(e.shi, glm::vec2{v.position.x, v.position.z});
        }
    };
    scan_solid(obj->wood);
    scan_solid(obj->bark);
    scan(obj->wood);
    scan(obj->cards);
    scan(obj->ground);
    scan(obj->bark);
    // AN OBSTACLE IS SOLID GEOMETRY TALLER THAN A STEP. "Has a wood stream"
    // was the first cut and it was defeated by real data: flora gives a grass
    // tuft a few-centimetre ROOT NUB in the wood stream so the placer will
    // render it at all, which made every blade of grass an obstacle and buried
    // the report under forty thousand meadow findings.
    //
    // The number is PLAYER_STEP_HEIGHT, not a guess: what a walker steps over
    // without noticing is not in his way, so two such things sharing ground is
    // a meadow and not a defect. Above it, they are two trunks in one hole.
    float solid_top = 0.0f;
    for (const dfn::render::MeshData* m : {&obj->wood, &obj->bark}) {
        for (const dfn::platform::Vertex& v : m->vertices) {
            solid_top = std::max(solid_top, v.position.y);
        }
    }
    e.solid = solid_top > static_cast<float>(dfn::config::PLAYER_STEP_HEIGHT);
    return &c->extents.emplace(name, e).first->second;
}

bool object_box_solid(void* ctx, const std::string& name, glm::vec2& lo,
                      glm::vec2& hi) {
    const Ctx::Extent* e = measure(static_cast<Ctx*>(ctx), name);
    if (e == nullptr) {
        return false;
    }
    lo = e->slo;
    hi = e->shi;
    return true;
}

bool object_solid(void* ctx, const std::string& name) {
    const Ctx::Extent* e = measure(static_cast<Ctx*>(ctx), name);
    return e != nullptr && e->solid;
}

/// Metres from the outer edge of the worn path surface, outward. THE SAME
/// network the ground was worn by (ctx->gen->paths): a judge that asked a
/// second source could forbid building where the ground shows no path and
/// permit it where the ground shows one.
bool path_clearance(void* ctx, glm::vec2 world, float& metres) {
    const auto* c = static_cast<Ctx*>(ctx);
    if (c->gen->paths.routes.empty()) {
        return false; // no network on this world: the rule cannot fire
    }
    metres = c->gen->paths.sample(world).dist_from_worn_edge;
    return true;
}

bool object_extent(void* ctx, const std::string& name, float& radius, float& bottom) {
    const Ctx::Extent* e = measure(static_cast<Ctx*>(ctx), name);
    if (e == nullptr) {
        return false;
    }
    radius = e->radius;
    bottom = e->bottom;
    return true;
}

/// The object's footprint about its own origin. A building part's origin is at
/// one END of it, so this is the only measurement that tells the truth about
/// where the part actually sits.
bool object_box(void* ctx, const std::string& name, glm::vec2& lo, glm::vec2& hi) {
    const Ctx::Extent* e = measure(static_cast<Ctx*>(ctx), name);
    if (e == nullptr) {
        return false;
    }
    lo = e->lo;
    hi = e->hi;
    return true;
}

/// How tall the object stands above its own origin — what another part rests
/// on. Measured from the same mesh as everything else: a support height typed
/// in by hand is the first thing to go stale when a part is re-forged.
bool object_top(void* ctx, const std::string& name, float& top) {
    const Ctx::Extent* e = measure(static_cast<Ctx*>(ctx), name);
    if (e == nullptr) {
        return false;
    }
    top = e->top;
    return true;
}

/// One placed object's triangles in WORLD space, merged into a soup. The
/// meshes are memoised per name — a house is two hundred copies of forty parts.
std::optional<dfn::render::MeshData> read_merged(Ctx* c, const std::string& name,
                                                 std::map<std::string, dfn::render::MeshData>& cache) {
    if (const auto it = cache.find(name); it != cache.end()) {
        return it->second;
    }
    std::optional<dfn::render::RegistryObject> obj;
    for (const auto& shelf : c->shelves) {
        obj = dfn::render::read_object(shelf / (name + ".dfo"));
        if (obj) {
            break;
        }
    }
    if (!obj) {
        return std::nullopt;
    }
    dfn::render::MeshData merged = obj->wood;
    dfn::render::append_transformed(merged, obj->bark, {0.0f, 0.0f, 0.0f}, 0.0f, 1.0f);
    dfn::render::append_transformed(merged, obj->cards, {0.0f, 0.0f, 0.0f}, 0.0f, 1.0f);
    dfn::render::append_transformed(merged, obj->ground, {0.0f, 0.0f, 0.0f}, 0.0f, 1.0f);
    cache.emplace(name, merged);
    return merged;
}

/// Moller-Trumbore. Returns true when the ray from `o` along unit `d` crosses
/// the triangle at t in (1e-4, t_max).
bool ray_hits_tri(const glm::vec3& o, const glm::vec3& d, const glm::vec3& a,
                  const glm::vec3& b, const glm::vec3& c, float t_max) {
    const glm::vec3 e1 = b - a;
    const glm::vec3 e2 = c - a;
    const glm::vec3 p = glm::cross(d, e2);
    const float det = glm::dot(e1, p);
    if (std::fabs(det) < 1e-9f) {
        return false;
    }
    const float inv = 1.0f / det;
    const glm::vec3 s = o - a;
    const float u = glm::dot(s, p) * inv;
    if (u < 0.0f || u > 1.0f) {
        return false;
    }
    const glm::vec3 q = glm::cross(s, e1);
    const float v = glm::dot(d, q) * inv;
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }
    const float t = glm::dot(e2, q) * inv;
    return t > 1e-4f && t < t_max;
}

/// THE SEALED-HULL INSTRUMENT. A probe inside the group casts a Fibonacci fan
/// of rays; every ray must die on the building's own triangles. An escaped ray
/// IS a through-hole — the count is the measurement, the directions name where
/// to look. Rays steeper than -0.15 down are not cast (an earthen floor is not
/// a hole); everything else must be roof, wall, gable, door or blind window.
int check_shell(Ctx* ctx, const dfn::world::SceneDoc& doc) {
    using dfn::render::MeshData;
    std::map<std::string, MeshData> cache;
    std::map<std::string, MeshData> soup_by_group;
    for (const auto& p : doc.placements) {
        if (p.group.empty()) {
            continue;
        }
        const auto merged = read_merged(ctx, p.object, cache);
        if (!merged) {
            std::fprintf(stderr, "[shell] %s: no such object on the shelves\n",
                         p.object.c_str());
            return -1;
        }
        dfn::render::append_transformed(soup_by_group[p.group], *merged, p.position,
                                        p.yaw, p.scale);
    }
    if (soup_by_group.empty()) {
        std::printf("[shell] no groups in this scene — nothing to seal\n");
        return 0;
    }
    int leaks_total = 0;
    for (const auto& [group, soup] : soup_by_group) {
        glm::vec3 lo{1e9f};
        glm::vec3 hi{-1e9f};
        for (const auto& v : soup.vertices) {
            lo = glm::min(lo, v.position);
            hi = glm::max(hi, v.position);
        }
        // The probe stands where a person would: group centre, eye height
        // over the ground the scene is judged against — the same generator
        // ground every other rule measures, not a guessed floor.
        const float cx = (lo.x + hi.x) * 0.5f;
        const float cz = (lo.z + hi.z) * 0.5f;
        const float ground_y = dfn::world::terrain_height(*ctx->gen, {cx, cz});
        const glm::vec3 probe{cx, ground_y + 1.4f, cz};
        const float t_max = glm::length(hi - lo) + 1.0f;
        // Rule 50: the fan's step must resolve the narrowest hole the rule is
        // about — a 1.7 cm board gap seen from ~2.3 m is ~0.4 deg wide but
        // 2 m long, so what matters is rays per SLIT, not per degree. 32768
        // rays put ~4 rays through each such slit; 4096 put 0.3 and the
        // instrument answered "sealed" about a wall of open seams.
        constexpr int RAYS = 32768;
        constexpr float MIN_DY = -0.15f;
        const float golden = 2.399963230f; // golden-angle increment
        int cast = 0;
        int escaped = 0;
        std::vector<glm::vec3> samples;
        for (int i = 0; i < RAYS; ++i) {
            const float y = 1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / RAYS;
            if (y < MIN_DY) {
                continue;
            }
            const float r = std::sqrt(std::max(0.0f, 1.0f - y * y));
            const float ang = golden * static_cast<float>(i);
            const glm::vec3 d{r * std::cos(ang), y, r * std::sin(ang)};
            ++cast;
            bool hit = false;
            const auto& idx = soup.indices;
            for (std::size_t k = 0; k + 2 < idx.size(); k += 3) {
                if (ray_hits_tri(probe, d, soup.vertices[idx[k]].position,
                                 soup.vertices[idx[k + 1]].position,
                                 soup.vertices[idx[k + 2]].position, t_max)) {
                    hit = true;
                    break;
                }
            }
            if (!hit) {
                ++escaped;
                if (samples.size() < 8) {
                    samples.push_back(d);
                }
            }
        }
        leaks_total += escaped;
        std::printf("[shell] group \"%s\": %d ray(s) cast from (%.2f, %.2f, %.2f), "
                    "%d escaped%s\n", group.c_str(), cast,
                    static_cast<double>(probe.x), static_cast<double>(probe.y),
                    static_cast<double>(probe.z), escaped,
                    escaped == 0 ? " — sealed" : "");
        for (const glm::vec3& d : samples) {
            std::printf("[shell]   through-hole toward azimuth %.0f deg, "
                        "elevation %+.0f deg\n",
                        static_cast<double>(std::atan2(d.z, d.x) * 57.2957795f),
                        static_cast<double>(std::asin(d.y) * 57.2957795f));
        }
    }
    return leaks_total;
}

/// THE PANEL INSTRUMENT (--solid <group>). A flat ASSEMBLY — a wall panel, a
/// floor deck — has no room to stand a probe in, so the hull fan is the wrong
/// instrument for it; what a panel promises is NO DAYLIGHT STRAIGHT THROUGH.
/// A grid of parallel rays is cast across the group's THINNEST axis (that is
/// the panel's normal, found from the bounding box, not from any property
/// under test — Rule 47), one ray per centimetre: the narrowest hole this
/// zone has ever shipped was a 1.7 cm board gap, and a coarser grid would
/// certify a wall of open seams (Rule 50). Zero rays through = solid.
int check_solid(Ctx* ctx, const dfn::world::SceneDoc& doc, const std::string& group) {
    using dfn::render::MeshData;
    std::map<std::string, MeshData> cache;
    MeshData soup;
    for (const auto& p : doc.placements) {
        if (p.group != group) {
            continue;
        }
        const auto merged = read_merged(ctx, p.object, cache);
        if (!merged) {
            std::fprintf(stderr, "[solid] %s: no such object on the shelves\n",
                         p.object.c_str());
            return -1;
        }
        dfn::render::append_transformed(soup, *merged, p.position, p.yaw, p.scale);
    }
    if (soup.vertices.empty()) {
        std::fprintf(stderr, "[solid] group \"%s\": nothing in it\n", group.c_str());
        return -1;
    }
    // ONE instrument, TWO callers (this judge and dfn_assemble's
    // --require-solid): the function lives in engine/world, and neither
    // caller holds an opinion of its own about what "solid" means.
    std::vector<glm::vec3> positions;
    positions.reserve(soup.vertices.size());
    for (const auto& v : soup.vertices) {
        positions.push_back(v.position);
    }
    const dfn::world::SolidReport r =
        dfn::world::check_panel_solid(positions, soup.indices);
    std::printf("[solid] group \"%s\": %d ray(s) across axis %c, %d through%s\n",
                group.c_str(), r.rays_cast, "xyz"[r.normal_axis], r.rays_through,
                r.rays_through == 0 ? " — solid" : "");
    if (r.rays_through > 0) {
        std::printf("[solid]   first daylight at (%.3f, %.3f, %.3f)\n",
                    static_cast<double>(r.first_hole.x),
                    static_cast<double>(r.first_hole.y),
                    static_cast<double>(r.first_hole.z));
    }
    return r.rays_through;
}

} // namespace

int main(int argc, char** argv) {
    using namespace dfn::world;
    if (argc < 2) {
        std::fprintf(stderr, "usage: dfn_scene_check <file.scene> [--stand <id>] "
                             "[--objects <dir>[;<dir>...]] [--fix]\n"
                             "       dfn_scene_check - --ground <x> <z> [<span>] "
                             "[--stand <id>]\n");
        return 2;
    }
    const std::filesystem::path scene_path = argv[1];
    std::string stand = "Gallery";
    std::string objects_dir = "assets/objects/trees";
    bool fix = false;
    bool shell = false;
    std::string solid_group;
    bool ground_query = false;
    float query_x = 0.0f;
    float query_z = 0.0f;
    float query_span = 8.0f;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fix") == 0) {
            fix = true;
        } else if (std::strcmp(argv[i], "--shell") == 0) {
            shell = true;
        } else if (std::strcmp(argv[i], "--solid") == 0 && i + 1 < argc) {
            solid_group = argv[++i];
        } else if (std::strcmp(argv[i], "--ground") == 0 && i + 2 < argc) {
            ground_query = true;
            query_x = std::strtof(argv[++i], nullptr);
            query_z = std::strtof(argv[++i], nullptr);
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                query_span = std::strtof(argv[++i], nullptr);
            }
        } else if (std::strcmp(argv[i], "--stand") == 0 && i + 1 < argc) {
            stand = argv[++i];
        } else if (std::strcmp(argv[i], "--objects") == 0 && i + 1 < argc) {
            objects_dir = argv[++i];
        } else {
            std::fprintf(stderr, "[scene] unknown argument \"%s\" -- REFUSED\n", argv[i]);
            return 2;
        }
    }

    SceneDoc doc;
    std::string error;
    // In a --ground query the scene is OPTIONAL (pass "-" for none), but when
    // one is given it is read: a builder asking "how high is the ground here"
    // while his own terraces are in the file wants the answer AFTER them.
    const bool want_scene = !ground_query || scene_path != "-";
    if (want_scene && !read_scene(scene_path, doc, error)) {
        if (ground_query) {
            std::fprintf(stderr, "[scene] %s: %s -- measuring the natural ground\n",
                         scene_path.string().c_str(), error.c_str());
            doc = SceneDoc{};
        } else
        {
            std::fprintf(stderr, "[scene] %s: %s\n", scene_path.string().c_str(),
                         error.c_str());
            return 1;
        }
    }

    WorldGenParams params;
    params.seed = 1;
    params.min_chunk = {0, 0};
    params.max_chunk = {7, 7};
    const auto lr = load_layout_file("games/daggerfall_n/assets/world/testbed_layout.json",
                                     params.layout);
    if (!lr.ok) {
        std::fprintf(stderr, "[scene] layout: %s\n", lr.error.c_str());
        return 1;
    }
    if (stand == "Forest") {
        params.layout = forest_stand_layout();
    } else if (stand == "OneTree") {
        params.layout = one_tree_stand_layout();
    } else if (stand == "Gallery") {
        params.layout = gallery_stand_layout();
    } else if (stand != "Testbed") {
        std::fprintf(stderr, "[scene] unknown stand \"%s\" -- REFUSED\n", stand.c_str());
        return 2;
    }
    // THE JUDGE MEASURES THE TERRACED GROUND, not the natural one. The pads a
    // composition authors are a pass of the height field, so a checker that
    // skipped them would report every house on a terrace as buried by the
    // terrace's own depth — and its report would be noise exactly where the
    // tool is needed most.
    for (const ScenePad& P : doc.pads) {
        BuildingPad pad;
        pad.center = P.center;
        pad.half_extents = P.half_extents;
        pad.radius = P.radius;
        pad.blend = P.blend;
        pad.height = P.height;
        params.composed_pads.push_back(pad);
    }
    for (const SceneRiver& R : doc.rivers) {
        RiverChannel ch;
        ch.points = R.points;
        ch.width_m = R.width_m;
        ch.depth_m = R.depth_m;
        ch.bank_m = R.bank_m;
        params.composed_rivers.push_back(std::move(ch));
    }
    const WorldGenContext gen = build_world_context(params);

    // ASKING THE WORLD A QUESTION, before there is a scene to check. A builder
    // needs to know the ground before he can write down where a footing goes,
    // and the alternative — placing at a guessed height and letting --fix
    // correct it — cannot work for a house, whose parts rest on each other and
    // are never sat down by the fixer.
    if (ground_query) {
        float lo = 1e9f;
        float hi = -1e9f;
        constexpr int STEPS = 16;
        for (int iz = 0; iz <= STEPS; ++iz) {
            for (int ix = 0; ix <= STEPS; ++ix) {
                const glm::vec2 at{
                    query_x + query_span * (static_cast<float>(ix) / STEPS - 0.5f),
                    query_z + query_span * (static_cast<float>(iz) / STEPS - 0.5f)};
                const float h = terrain_height(gen, at);
                lo = std::min(lo, h);
                hi = std::max(hi, h);
            }
        }
        std::printf("[ground] (%.2f, %.2f) = %.3f m; over %.1f m: %.3f..%.3f "
                    "(spread %.3f m)\n", static_cast<double>(query_x),
                    static_cast<double>(query_z),
                    static_cast<double>(terrain_height(gen, {query_x, query_z})),
                    static_cast<double>(query_span), static_cast<double>(lo),
                    static_cast<double>(hi), static_cast<double>(hi - lo));
        return 0;
    }


    Ctx ctx;
    ctx.gen = &gen;
    for (std::size_t at = 0; at <= objects_dir.size();) {
        const std::size_t sep = objects_dir.find(';', at);
        const std::size_t end = sep == std::string::npos ? objects_dir.size() : sep;
        std::string one = objects_dir.substr(at, end - at);
        while (!one.empty() && (one.front() == ' ' || one.front() == '\t')) {
            one.erase(one.begin());
        }
        while (!one.empty() && (one.back() == ' ' || one.back() == '\t')) {
            one.pop_back();
        }
        if (!one.empty()) {
            ctx.shelves.emplace_back(one);
        }
        if (sep == std::string::npos) {
            break;
        }
        at = sep + 1;
    }
    SceneWorld world;
    world.ground_at = &ground_at;
    world.object_extent = &object_extent;
    world.object_top = &object_top;
    world.object_box = &object_box;
    world.path_clearance = &path_clearance;
    world.object_solid = &object_solid;
    world.object_box_solid = &object_box_solid;
    world.ctx = &ctx;

    if (fix) {
        const std::size_t moved = fix_scene_ground(doc, world);
        if (moved > 0 && !write_scene(doc, scene_path)) {
            std::fprintf(stderr, "[scene] could not write %s\n",
                         scene_path.string().c_str());
            return 1;
        }
        std::printf("[scene] sat %zu placement(s) back on the ground\n", moved);
    }

    const auto findings = check_scene(doc, world);
    for (const SceneFinding& f : findings) {
        std::printf("%s\n", describe(f).c_str());
    }
    int leaks = 0;
    if (shell) {
        leaks = check_shell(&ctx, doc);
        if (leaks < 0) {
            return 1;
        }
    }
    if (!solid_group.empty()) {
        const int through = check_solid(&ctx, doc, solid_group);
        if (through != 0) {
            return 1;
        }
    }
    std::printf("[scene] %s: %zu placement(s), %zu finding(s)%s\n",
                scene_path.filename().string().c_str(), doc.placements.size(),
                findings.size(),
                shell ? (leaks == 0 ? ", hull sealed" : ", hull LEAKS") : "");
    return findings.empty() && leaks == 0 ? 0 : 1;
}
