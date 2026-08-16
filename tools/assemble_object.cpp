/*
Created: 16:08:2026 - 22:38:52
Last updated: 16:08:2026 - 22:50:41
Module: tools
File: tools/assemble_object.cpp

Responsibility:
- dfn_assemble: bakes a GROUP of a .scene into ONE registry object (.dfo). The
  second level of the building kit — small parts assembled into a wall panel, a
  floor panel, a porch, a corner of a log wall — frozen as a single object that
  is then placed whole.

Usage:
    dfn_assemble <file.scene> --group <name> --out <file.dfo>
                 [--objects <dir>[;<dir>...]] [--name <handle>]

Dependencies:
- Uses: engine/world (Scene), engine/render (ObjectRegistry, ProcMesh).
- Used by: the houses zone, building its assemblies.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- AN ASSEMBLY IS A COMPOSITION, FROZEN — never a new mesher. The .scene that
  produced it stays in git next to the .dfo, so an assembly can be re-opened,
  moved a piece and re-baked. The day this tool grows a "build me a wall"
  switch is the day the kit turned back into a house generator, which is the
  choice the user already made against (PartForge.h's header says why).
- THE ORIGIN IS THE GROUP'S FOOTING — min x, min y, min z of everything in it.
  Same convention every kit part follows (origin at the joint), because an
  assembly is placed by the same counting-in-grid-units rule its parts were.
- Streams keep their identity: wood stays wood, cards stay cards, bark stays
  bark, ground stays ground. Merging them would hand the leaf program a wall.
*/
/*
UPD:
- 16:08:2026 - 22:38:52: Создан по заданию пользователя: «пусть агент по строительству из
  мелких деталей будет собирать большие, чтобы меньше дырок было... стены пусть
  соберет, как панели, панели для пола соберет из нескольких брёвен... и
  зафиксируется как единый объект».
- 16:08:2026 - 22:50:41: --require-solid — печь только СПЛОШНУЮ сборку. Зовёт ту же
  check_panel_solid, что и судья ключом --solid: одна функция, двое зовущих,
  ни у одного своего мнения. Отказ стоит У ПЕЧИ, а не только у судьи, потому
  что мимо судьи можно испечь, а с этого момента течь перестаёт быть одним
  дефектом и становится по одному на каждую копию панели в городе — ровно то,
  ради чего пользователь и просил сборки. Поток cards в проверку НЕ идёт:
  листовые карточки альфа-вырезаны, и прибор объявил бы дырой каждую; сборка,
  которая держится замкнутой за счёт листвы, не замкнута.
*/

#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/ProcMesh.h"
#include "engine/world/sources/Scene.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    using namespace dfn::render;
    using namespace dfn::world;

    if (argc < 2) {
        std::fprintf(stderr,
                     "usage: dfn_assemble <file.scene> --group <name> --out <file.dfo>\n"
                     "                    [--objects <dir>[;<dir>...]] [--name <handle>]\n");
        return 2;
    }
    const fs::path scene_path = argv[1];
    std::string group;
    fs::path out_path;
    std::string shelves_arg = "assets/objects/parts";
    std::string handle;
    bool require_solid = false;
    for (int i = 2; i < argc; ++i) {
        const auto next = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "[assemble] %s wants a value -- REFUSED\n", what);
                std::exit(2);
            }
            return argv[++i];
        };
        if (std::strcmp(argv[i], "--group") == 0) {
            group = next("--group");
        } else if (std::strcmp(argv[i], "--out") == 0) {
            out_path = next("--out");
        } else if (std::strcmp(argv[i], "--objects") == 0) {
            shelves_arg = next("--objects");
        } else if (std::strcmp(argv[i], "--name") == 0) {
            handle = next("--name");
        } else if (std::strcmp(argv[i], "--require-solid") == 0) {
            require_solid = true;
        } else {
            std::fprintf(stderr, "[assemble] unknown argument \"%s\" -- REFUSED\n", argv[i]);
            return 2;
        }
    }
    if (group.empty() || out_path.empty()) {
        std::fprintf(stderr, "[assemble] --group and --out are required -- REFUSED\n");
        return 2;
    }

    SceneDoc doc;
    std::string error;
    if (!read_scene(scene_path, doc, error)) {
        std::fprintf(stderr, "[assemble] %s: %s\n", scene_path.string().c_str(),
                     error.c_str());
        return 1;
    }
    const std::vector<std::string> shelves = split_shelves(shelves_arg);

    // Read every distinct object ONCE: an assembly is a dozen copies of three
    // parts, and re-reading (and re-hashing) each copy would be the tool's
    // whole cost.
    std::map<std::string, RegistryObject> loaded;
    std::vector<const Placement*> members;
    for (const Placement& p : doc.placements) {
        if (p.group != group) {
            continue;
        }
        if (loaded.find(p.object) == loaded.end()) {
            std::optional<RegistryObject> obj;
            for (const std::string& shelf : shelves) {
                obj = read_object(fs::path(shelf) / (p.object + ".dfo"));
                if (obj) {
                    break;
                }
            }
            if (!obj) {
                // REFUSED, not skipped. A wall panel silently missing one of
                // its logs is a wall panel with a hole in it, and the hole
                // would be baked in and shipped.
                std::fprintf(stderr, "[assemble] no object \"%s\" on any shelf of %s"
                                     " -- REFUSED\n", p.object.c_str(),
                             shelves_arg.c_str());
                return 1;
            }
            loaded.emplace(p.object, std::move(*obj));
        }
        members.push_back(&p);
    }
    if (members.empty()) {
        std::fprintf(stderr, "[assemble] group \"%s\" has no placements in %s"
                             " -- REFUSED\n", group.c_str(), scene_path.string().c_str());
        return 1;
    }

    // THE ORIGIN: the group's own footing corner, found from the placed
    // MESHES rather than from the placement positions — a part's origin is at
    // its joint, so the lowest, nearest corner of the assembly is generally
    // not any one part's origin.
    glm::vec3 lo{std::numeric_limits<float>::max()};
    for (const Placement* p : members) {
        const RegistryObject& obj = loaded.at(p->object);
        const float c = std::cos(p->yaw);
        const float s = std::sin(p->yaw);
        const auto scan = [&](const MeshData& mesh) {
            for (const dfn::platform::Vertex& v : mesh.vertices) {
                const glm::vec3 w{
                    p->position.x + (v.position.x * c + v.position.z * s) * p->scale,
                    p->position.y + v.position.y * p->scale,
                    p->position.z + (-v.position.x * s + v.position.z * c) * p->scale};
                lo = glm::min(lo, w);
            }
        };
        scan(obj.wood);
        scan(obj.cards);
        scan(obj.ground);
        scan(obj.bark);
    }

    RegistryObject out;
    out.name = handle.empty() ? group : handle;
    out.kind = "assembly";
    {
        char src[256];
        std::snprintf(src, sizeof(src), "assemble:%s#%s parts=%zu",
                      scene_path.filename().string().c_str(), group.c_str(),
                      members.size());
        out.source = src;
    }
    for (const Placement* p : members) {
        const RegistryObject& obj = loaded.at(p->object);
        const glm::vec3 at = p->position - lo;
        append_transformed(out.wood, obj.wood, at, p->yaw, p->scale);
        append_transformed(out.cards, obj.cards, at, p->yaw, p->scale);
        append_transformed(out.ground, obj.ground, at, p->yaw, p->scale);
        append_transformed(out.bark, obj.bark, at, p->yaw, p->scale);
    }

    // SOLID OR NOT BAKED, when asked. The same check the judge runs under
    // --solid, called here through its ONE definition in engine/world: a
    // panel that leaks must not become a .dfo, because from that moment the
    // leak stops being one defect and becomes one per copy placed in the town.
    // Refusing at the oven is the whole reason the user asked for assemblies
    // («чтобы меньше дырок было»); refusing only in the judge would leave a
    // way to bake past him.
    if (require_solid) {
        std::vector<glm::vec3> positions;
        std::vector<uint32_t> indices;
        const auto feed = [&](const MeshData& mesh) {
            const uint32_t base = static_cast<uint32_t>(positions.size());
            for (const dfn::platform::Vertex& v : mesh.vertices) {
                positions.push_back(v.position);
            }
            for (const uint32_t i : mesh.indices) {
                indices.push_back(base + i);
            }
        };
        feed(out.wood);
        feed(out.bark);
        feed(out.ground);
        // `cards` is deliberately NOT fed: leaf cards are alpha-cut sheets and
        // a solidity test would call every one of them a hole. An assembly
        // that leans on foliage to be closed is not closed.
        const SolidReport r = check_panel_solid(positions, indices);
        if (r.rays_through > 0) {
            std::fprintf(stderr,
                         "[assemble] %s LEAKS: %d of %d ray(s) pass through "
                         "(axis %u), first hole at (%.2f, %.2f, %.2f) -- REFUSED, "
                         "nothing written\n", group.c_str(), r.rays_through,
                         r.rays_cast, static_cast<unsigned>(r.normal_axis),
                         static_cast<double>(r.first_hole.x),
                         static_cast<double>(r.first_hole.y),
                         static_cast<double>(r.first_hole.z));
            return 1;
        }
        std::printf("[assemble]   solid: 0 of %d ray(s) through\n", r.rays_cast);
    }

    std::error_code ec;
    fs::create_directories(out_path.parent_path(), ec);
    if (!write_object(out, out_path)) {
        std::fprintf(stderr, "[assemble] cannot write %s\n", out_path.string().c_str());
        return 1;
    }
    // Round trip or refuse, like every other tool that writes to the registry:
    // an assembly that cannot be read back would be discovered inside a house.
    const uint64_t expect = object_content_hash(out);
    const auto back = read_object(out_path);
    if (!back || back->content_hash != expect) {
        std::fprintf(stderr, "[assemble] %s does not read back -- REFUSED\n",
                     out_path.string().c_str());
        return 1;
    }
    std::printf("[assemble] %s: %zu part(s) of %zu kind(s) -> %s\n"
                "[assemble]   %zu wood + %zu card + %zu bark + %zu ground triangles,"
                " hash %016llx\n"
                "[assemble]   origin at the group's footing corner (%.2f, %.2f, %.2f)\n",
                group.c_str(), members.size(), loaded.size(),
                out_path.string().c_str(), out.wood.triangle_count(),
                out.cards.triangle_count(), out.bark.triangle_count(),
                out.ground.triangle_count(),
                static_cast<unsigned long long>(expect),
                static_cast<double>(lo.x), static_cast<double>(lo.y),
                static_cast<double>(lo.z));
    return 0;
}
