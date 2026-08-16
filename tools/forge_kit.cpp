/*
Created: 16:08:2026 - 20:55:43
Last updated: 16:08:2026 - 20:55:43
Module: tools
File: tools/forge_kit.cpp

Responsibility:
- dfn_kit: forges THE BUILDING KIT into the object registry — every part of
  kit_catalogue() written as a .dfo under assets/objects/parts, plus the
  human-readable index an agent reads to find "a 2 m beam, 25 cm, weathered".

Usage:
    dfn_kit [<out_dir>]        (default assets/objects/parts; run from repo root)
    dfn_kit --list             (print the catalogue and stop, writing nothing)

Dependencies:
- Uses: engine/render (PartForge, ObjectRegistry).
- Used by: agents who build houses, and the human.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ROUND TRIP OR REFUSE, like dfn_forge: every part is read back and its hash
  compared before this tool reports success. A kit whose files do not open is
  a catalogue of nothing, and the failure would only surface later, inside a
  half-built house.
*/
/*
UPD:
- 16:08:2026 - 20:55:43: Создан — набор строительных деталей (запрос
  пользователя: «набор из 500-та различных строй материалов и их конфигураций,
  чтобы агент строил разные дома»).
*/

#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/PartForge.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    using namespace dfn::render;

    bool list_only = false;
    fs::path out_dir = "assets/objects/parts";
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--list") == 0) {
            list_only = true;
        } else if (argv[i][0] == '-') {
            std::fprintf(stderr, "[kit] unknown argument \"%s\" -- REFUSED\n", argv[i]);
            return 2;
        } else {
            out_dir = argv[i];
        }
    }

    const auto catalogue = kit_catalogue();
    std::map<std::string, int> per_kind;
    if (list_only) {
        for (const PartParams& p : catalogue) {
            std::printf("%s\n", part_name(p).c_str());
        }
        std::printf("[kit] %zu part(s) in the catalogue\n", catalogue.size());
        return 0;
    }

    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        std::fprintf(stderr, "[kit] cannot make %s: %s\n", out_dir.string().c_str(),
                     ec.message().c_str());
        return 1;
    }

    std::size_t tris = 0;
    std::size_t written = 0;
    std::string index;
    index += "# The building kit — every part the composer may place.\n";
    index += "# name  triangles  source\n";
    for (const PartParams& p : catalogue) {
        const RegistryObject obj = forge_part(p);
        if (obj.wood.indices.empty()) {
            std::fprintf(stderr, "[kit] %s produced NO GEOMETRY -- REFUSED\n",
                         obj.name.c_str());
            return 1;
        }
        const fs::path path = out_dir / (obj.name + ".dfo");
        if (!write_object(obj, path)) {
            std::fprintf(stderr, "[kit] cannot write %s\n", path.string().c_str());
            return 1;
        }
        // The round trip, part by part: a kit that cannot be read back is
        // worse than no kit, because the failure shows up mid-house.
        // write_object computes the identity; the object in hand still carries
        // the default 0, so the round trip is checked against the hash of the
        // PAYLOAD and not against a field nobody filled in.
        const uint64_t expect = object_content_hash(obj);
        const auto back = read_object(path);
        if (!back || back->content_hash != expect) {
            std::fprintf(stderr, "[kit] %s does not read back -- REFUSED\n",
                         path.string().c_str());
            return 1;
        }
        const std::size_t t = obj.wood.triangle_count();
        tris += t;
        ++written;
        ++per_kind[obj.source.substr(4, obj.source.find(' ', 4) - 4)];
        char row[320];
        std::snprintf(row, sizeof(row), "%-34s %6zu  %s\n", obj.name.c_str(), t,
                      obj.source.c_str());
        index += row;
    }

    {
        std::ofstream f(out_dir / "INDEX.txt", std::ios::trunc);
        if (!f) {
            std::fprintf(stderr, "[kit] cannot write the index\n");
            return 1;
        }
        f << index;
    }

    for (const auto& [kind, n] : per_kind) {
        std::printf("[kit]   %-12s %4d\n", kind.c_str(), n);
    }
    std::printf("[kit] %zu part(s), %zu triangles, %.0f avg -> %s\n", written, tris,
                static_cast<double>(tris) / static_cast<double>(written),
                out_dir.string().c_str());
    return 0;
}
