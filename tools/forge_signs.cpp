/*
Created: 17:08:2026 - 14:46:25
Last updated: 17:08:2026 - 15:58:02
Module: tools
File: tools/forge_signs.cpp

Responsibility:
- dfn_signs: bakes TABLICHKI from a text file into the object registry — one
  .dfo per sign, plus the index a composer reads to find the name of a sign by
  what it says.

Usage:
    dfn_signs <file.signs>... [<out_dir>]  (default assets/objects/signs)
    dfn_signs --list <file.signs>...       (print names and stop, writing nothing)
    dfn_signs --flat <file.signs>...       (untextured arm, see below)

Any argument ending in .signs is an INPUT; a single other argument is the
output directory. All inputs land on one shelf and in one index.

WHY A DATA FILE AND NOT A CATALOGUE IN C++ (Rule 5, Rule 6). The text on a sign
IS content: it is the sentence a player reads standing in front of a house, it
will be translated, and it changes far more often than any code here. So the
forge takes a string and this tool reads the strings from a file — adding a
sign is editing a text file, never a rebuild.

File format (the .scene family's own syntax, so an agent who can read one can
read the other). THE PARSER IS NOT HERE: it is render::read_signs_file(), so
that the game's first-run bake reads exactly what this tool reads.

    [sign]
    shape = post | hanging | wall      # how it is held up
    cap = 0.06                         # letter cap height, metres
    board = timber | dark | stone | plaster
    ink = dark | timber | stone
    name = my-sign                     # optional; else a hash of the text
    line = Первая строка
    line = вторая строка

Dependencies:
- Uses: engine/render (SignForge, ObjectRegistry).
- Used by: the showcase and demo generators, and the human.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ROUND TRIP OR REFUSE, like dfn_kit: every sign is read back and its hash
  compared before this tool reports success.
- NO USER-FACING STRING IN THIS FILE. Every word a player sees comes out of the
  input file.
*/
/*
UPD:
- 17:08:2026 - 14:46:25: Создан — работа 4 заказа 17.08 (таблички; текст приходит файлом).
- 17:08:2026 - 14:55:07: Разбор .signs уехал в библиотеку (read_signs_file) — печь приложения
  не может заглянуть в инструмент, а второй разбор разошёлся бы с этим.
- 17:08:2026 - 15:58:02: Несколько .signs за один прогон: индекс описывает ВСЮ полку, и печь
  в два захода оставляла бы индекс, называющий только второй файл.
*/

#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/SignForge.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace dfn::render;

int main(int argc, char** argv) {
    bool list_only = false;
    bool flat = false;
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--list") == 0) {
            list_only = true;
        } else if (std::strcmp(argv[i], "--flat") == 0) {
            // The untextured arm: flat vertex-colour albedo, which is what the
            // whole kit still looks like until the renderer binds the parts
            // sheet. A textured sign among untextured houses previews nothing.
            flat = true;
        } else {
            args.emplace_back(argv[i]);
        }
    }
    // EVERY .signs ON THE COMMAND LINE, ONE SHELF. The index this tool writes
    // describes the WHOLE directory, so baking two files in two runs would
    // leave an index that names only the second one — and a shelf whose index
    // lies is worse than a shelf with no index.
    std::vector<fs::path> inputs;
    fs::path out_dir = "assets/objects/signs";
    for (const std::string& a : args) {
        if (a.size() > 6 && a.compare(a.size() - 6, 6, ".signs") == 0) {
            inputs.emplace_back(a);
        } else {
            out_dir = a;
        }
    }
    if (inputs.empty()) {
        std::fprintf(stderr,
                     "usage: dfn_signs [--list] [--flat] <file.signs>... [<out_dir>]\n");
        return 2;
    }

    // ONE PARSER FOR TWO CONSUMERS (Rule 32): this tool and the game's own
    // first-run bake read the same .signs through read_signs_file().
    std::vector<SignParams> signs;
    bool ok = true;
    for (const fs::path& input : inputs) {
        if (!read_signs_file(input.string(), signs)) {
            std::fprintf(stderr, "[signs] %s: refused (see above)\n",
                         input.string().c_str());
            ok = false;
        }
    }
    for (SignParams& s : signs) {
        s.textured = !flat;
    }
    if (!ok) {
        return 1;
    }
    if (signs.empty()) {
        std::fprintf(stderr, "[signs] no [sign] blocks in any input\n");
        return 1;
    }

    if (list_only) {
        for (const SignParams& s : signs) {
            const glm::vec2 size = sign_board_size(s);
            std::printf("%-44s %5.2f x %4.2f m  %zu line(s)\n", sign_name(s).c_str(),
                        static_cast<double>(size.x), static_cast<double>(size.y),
                        s.lines.size());
        }
        return 0;
    }

    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        std::fprintf(stderr, "[signs] cannot create %s: %s\n",
                     out_dir.string().c_str(), ec.message().c_str());
        return 1;
    }

    std::string index = "# Signs — the name of a sign is a hash of what it says.\n";
    std::size_t tris = 0;
    for (const SignParams& s : signs) {
        if (s.lines.empty()) {
            std::fprintf(stderr, "[signs] a [sign] with no lines is a board -- REFUSED\n");
            return 1;
        }
        const RegistryObject obj = forge_sign(s);
        const fs::path path = out_dir / (obj.name + ".dfo");
        if (!write_object(obj, path)) {
            std::fprintf(stderr, "[signs] cannot write %s\n", path.string().c_str());
            return 1;
        }
        const uint64_t expect = object_content_hash(obj);
        const auto back = read_object(path);
        if (!back || back->content_hash != expect) {
            std::fprintf(stderr, "[signs] %s does not read back -- REFUSED\n",
                         path.string().c_str());
            return 1;
        }
        const std::size_t t = obj.wood.triangle_count() + obj.bark.triangle_count();
        tris += t;
        char row[512];
        std::snprintf(row, sizeof(row), "%-44s %6zu  %s\n", obj.name.c_str(), t,
                      s.lines.front().c_str());
        index += row;
    }
    std::ofstream idx(out_dir / "INDEX.txt");
    idx << index;
    std::printf("[signs] %zu sign(s), %zu triangles -> %s\n", signs.size(), tris,
                out_dir.string().c_str());
    return 0;
}
