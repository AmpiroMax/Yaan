/*
Created: 17:08:2026 - 21:03:54
Last updated: 17:08:2026 - 21:03:54
Module: tests/app
File: tests/app/EditorPaletteFixture.h

Responsibility:
- The two shelves the object menu's suites run on: the real baked kit when it is
  there, and a small hand-made one that is always there.

WHY A HEADER AND NOT A COPY: the axes suite needs exactly the same two, and a
second copy of `toy_shelf` would drift the first time a part was added to one of
them — and it would drift SILENTLY, because both suites would still be green
about different shelves (Rule 39).

Dependencies:
- Uses: std::filesystem.
- Used by: tests/app/EditorPaletteTests.cpp, tests/app/EditorPaletteAxesTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- AN ARM THAT SKIPS ON AN UNBAKED SHELF PROVES NOTHING. Anything held only by
  the real kit needs a companion that runs on the toy one.
*/
/*
UPD:
- 17:08:2026 - 21:03:54: Выделен из EditorPaletteTests.cpp при разрезе по правилу 21.
*/

#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr const char* SHELF = "assets/objects/parts";

/// The shelf's names, or an empty vector when nothing has been baked yet.
std::vector<std::string> shelf_names() {
    std::vector<std::string> out;
    std::error_code ec;
    if (!fs::is_directory(SHELF, ec)) {
        return out;
    }
    for (const auto& e : fs::directory_iterator(SHELF, ec)) {
        if (e.path().extension() != ".dfo") {
            continue;
        }
        std::string n = e.path().stem().string();
        if (n.size() > 4 && n.compare(n.size() - 4, 4, "-far") == 0) {
            continue;
        }
        out.push_back(std::move(n));
    }
    std::sort(out.begin(), out.end());
    return out;
}

/// A small hand-made shelf. Every arm that does not need the real kit runs on
/// this one, so the suite discriminates on a machine that has never baked.
std::vector<std::string> toy_shelf() {
    return {
        "wall-log-timber-12x1x13-blind-w03",
        "wall-log-timber-12x1x13-door-w08",
        "wall-ashlar-stone-16x1x11-win1-w05",
        "joint-timber-d50-n4-h13-w03",
        "joint-stone-d75-n8-h13-cap-w05",
        "joint-timber-d35-nr-h11-w08",
        "beam-dark-4x1x1-w03",
        "stair-steep-timber-1x4x13-w03",
    };
}

} // namespace
