/*
Created: 17:08:2026 - 17:40:00
Last updated: 17:08:2026 - 17:40:00
Module: tools
File: tools/new_map.cpp

Responsibility:
- dfn_newmap: THE ONE WAY A MAP FOLDER COMES INTO EXISTENCE. Creates
  assets/maps/<zone>/ and a .map skeleton beside a .scene skeleton, and says
  out loud what it made.

WHY THIS EXISTS (user, 17.08.2026): «пустые папки, которые снова появились, с
картами - удалить (есть механизм который их постоянно создает, удалить его,
папки будут создавать агенты, по необходимости, сделать инструмент для создания
папки и карты в папке более явным и прописать, если не прописано)».

Two halves, and this is the second. The first was removing the code that made
directories BEFORE it had anything to write (the playtest bot and the tour both
did, so every configured-but-silent run left a folder behind). What is left is
this: a folder is made by someone who MEANT to make it, in one visible command,
and never as a side effect of opening a file for writing.

Dependencies:
- Uses: std::filesystem only. No engine libraries on purpose — making a folder
  and writing two text files must not depend on the world compiling.
- Used by: agents starting a new stand.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- REFUSES to overwrite. A map that exists is someone's work; this tool makes
  new ones and says so, it never "updates" one silently.
- The skeleton is DELIBERATELY INCOMPLETE where a human must choose: name and
  description are placeholders, built_commit is empty. A skeleton that looked
  finished would ship placeholders into the map list.
*/
/*
UPD:
- 17:08:2026 - 17:40:00: Создан — явное создание папки и карты (заказ 17.08).
*/

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

namespace fs = std::filesystem;

void usage() {
    std::fprintf(stderr,
                 "dfn_newmap <зона>/<имя> [--stand <Stand>] [--chunks <N>]\n"
                 "\n"
                 "  Создаёт:\n"
                 "    assets/maps/<зона>/<имя>.map    манифест карты\n"
                 "    assets/scenes/<имя>.scene       пустая композиция\n"
                 "\n"
                 "  --stand   стенд-основание, по умолчанию Gallery (свой\n"
                 "            небольшой стенд; правило пользователя 17.08 —\n"
                 "            не строить на большой обзорной карте)\n"
                 "  --chunks  размер в чанках, по умолчанию 1 (256 м)\n"
                 "\n"
                 "  Отказывается перезаписывать существующее.\n");
}

/// A .map skeleton. Placeholders are SPELLED as placeholders so a half-filled
/// map is visible in the map list instead of reading as finished.
[[nodiscard]] std::string map_skeleton(const std::string& leaf,
                                       const std::string& stand, int chunks) {
    std::string s;
    s += "name = ЗАПОЛНИ: что это за карта\n";
    s += "zone = ЗАПОЛНИ: чья зона\n";
    s += "size_chunks = " + std::to_string(chunks) + "\n";
    s += "source = stand:" + stand + "\n";
    s += "scene = assets/scenes/" + leaf + ".scene\n";
    s += "objects = assets/objects/parts\n";
    s += "description = ЗАПОЛНИ: что на ней стоит и зачем\n";
    s += "built_commit =\n";
    return s;
}

[[nodiscard]] std::string scene_skeleton(const std::string& leaf) {
    std::string s;
    s += "# Композиция карты " + leaf + ". Создана dfn_newmap.\n";
    s += "# Правила проверяются dfn_scene_check; каждая панель садится между\n";
    s += "# ДВУМЯ стойками (HOUSES.md §3.2), пол и потолок — на горизонтальные.\n";
    s += "\n";
    s += "world_span_m = 256\n";
    return s;
}

bool write_new(const fs::path& path, const std::string& body) {
    if (fs::exists(path)) {
        std::fprintf(stderr, "[newmap] %s уже есть -- ОТКАЗ\n",
                     path.string().c_str());
        return false;
    }
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) {
        std::fprintf(stderr, "[newmap] не могу создать %s: %s\n",
                     path.parent_path().string().c_str(), ec.message().c_str());
        return false;
    }
    std::ofstream out(path);
    if (!out) {
        std::fprintf(stderr, "[newmap] не могу записать %s\n",
                     path.string().c_str());
        return false;
    }
    out << body;
    std::printf("[newmap] создано %s\n", path.string().c_str());
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::string target;
    std::string stand = "Gallery";
    int chunks = 1;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--stand") == 0 && i + 1 < argc) {
            stand = argv[++i];
        } else if (std::strcmp(argv[i], "--chunks") == 0 && i + 1 < argc) {
            chunks = std::atoi(argv[++i]);
        } else if (argv[i][0] == '-') {
            std::fprintf(stderr, "[newmap] неизвестный аргумент \"%s\" -- ОТКАЗ\n",
                         argv[i]);
            usage();
            return 2;
        } else {
            target = argv[i];
        }
    }
    if (target.empty() || target.find('/') == std::string::npos) {
        usage();
        return 2;
    }
    if (chunks < 1) {
        std::fprintf(stderr, "[newmap] --chunks %d -- ОТКАЗ, минимум 1\n", chunks);
        return 2;
    }
    const std::string zone = target.substr(0, target.find('/'));
    const std::string leaf = target.substr(target.find('/') + 1);
    if (zone.empty() || leaf.empty()) {
        usage();
        return 2;
    }

    const fs::path map_path = fs::path("assets/maps") / zone / (leaf + ".map");
    const fs::path scene_path = fs::path("assets/scenes") / (leaf + ".scene");
    // BOTH OR NEITHER. A .map naming a .scene that does not exist is the
    // failure this tool is meant to prevent, so the scene is checked BEFORE
    // the map folder is made — otherwise a refusal here would leave behind
    // exactly the empty folder the user asked us to stop creating.
    if (fs::exists(map_path) || fs::exists(scene_path)) {
        std::fprintf(stderr, "[newmap] %s или %s уже есть -- ОТКАЗ\n",
                     map_path.string().c_str(), scene_path.string().c_str());
        return 1;
    }
    if (!write_new(scene_path, scene_skeleton(leaf))) {
        return 1;
    }
    if (!write_new(map_path, map_skeleton(leaf, stand, chunks))) {
        return 1;
    }
    std::printf("[newmap] заполни name/zone/description в %s, затем:\n"
                "         DFN_OPEN_MAP=%s/%s ./build_<зона>/engine/app/dfn_app\n",
                map_path.string().c_str(), zone.c_str(), leaf.c_str());
    return 0;
}
