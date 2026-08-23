/*
Created: 23:08:2026 - 21:05:00
Last updated: 23:08:2026 - 21:05:00
Module: tools
File: tools/recipe_book.h

Responsibility:
- РЕЕСТР РЕЦЕПТОВ КУЗНИЦЫ и общий разбор ключей. Кузница объявляет ТАБЛИЦУ
  «имя -> пакет -> рука, которая печёт», разложенную по пакетам, а весь
  остальной обиход — выбор по маске (--only), перечень (--list), судья
  (--check), отчёт об отказах по связности и запись манифеста — здесь, один
  раз на обе кузницы.

Key items:
- Recipe: строка реестра (имя, файл, пакет, рука).
- add_recipe: заводит строку, ФАЙЛ ВЫВОДИТСЯ ИЗ ИМЕНИ.
- wildcard_match: маска с * и ? для --only.
- run_forge: разбор ключей, выпечка, отказы, манифест.

Dependencies:
- Uses: tools/house_manifest.h.
- Used by: tools/forge_houses.cpp, tools/forge_furniture.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ИМЯ — ЕДИНСТВЕННЫЙ ЛИТЕРАЛ. Путь получается из имени (SHELF/имя.dfh), а не
  пишется рядом второй строкой: в прежней кузнице путь стоял в вызове, имя —
  нигде, и сорок четыре литерала пути были единственным местом схватки для
  двенадцати городов, которые пекут на одну полку.
- ПАКЕТ ДОБАВЛЯЮТ СВОЕЙ СЕКЦИЕЙ: своя функция pack_<город>(book), вызванная в
  сборщике реестра. Общий city-* не трогают — иначе город правит городу.
- --only НЕ ТРОГАЕТ ЧУЖИЕ ФАЙЛЫ. Это его смысл: печь свой пакет, не
  перезаписывая соседский.
*/
/*
UPD:
- 23:08:2026 - 21:05:00: Создан — волна И3 эпохи «12 городов»: реестр рецептов
  вместо прямого списка вызовов в main() и ключ --only.
*/

#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "tools/house_manifest.h"

namespace dfn::forge {

/// Полка готовых построек. Одна на все кузницы и все города.
inline constexpr const char* SHELF = "assets/houses";

struct Recipe {
    /// Имя рецепта без расширения: «city-house-s», «furn-lamp-post».
    std::string name;
    /// Путь артефакта. Выводится из имени, руками не задаётся.
    std::string file;
    /// Пакет: city (общие), furn (обстановка), demo, будущие города.
    std::string family;
    /// Рука, которая печёт. Файл приходит ВХОДОМ, а не берётся из литерала
    /// внутри: иначе реестр обещал бы одно, а рука писала другое.
    std::function<void(const char*)> bake;
};

/// Заводит строку реестра. Файл — SHELF/<имя>.dfh.
inline void add_recipe(std::vector<Recipe>& book, const std::string& family,
                       const std::string& name, std::function<void(const char*)> bake) {
    Recipe r;
    r.name = name;
    r.file = std::string(SHELF) + "/" + name + ".dfh";
    r.family = family;
    r.bake = std::move(bake);
    book.push_back(std::move(r));
}

/// Маска с * (любая последовательность) и ? (любой один знак). Своя, а не
/// fnmatch: тот тянет POSIX-заголовок ради двадцати строк и на маске без
/// косых черт ведёт себя ровно так же.
[[nodiscard]] inline bool wildcard_match(const std::string& pat, const std::string& s) {
    std::size_t p = 0;
    std::size_t t = 0;
    std::size_t star = std::string::npos;
    std::size_t mark = 0;
    while (t < s.size()) {
        if (p < pat.size() && (pat[p] == '?' || pat[p] == s[t])) {
            ++p;
            ++t;
        } else if (p < pat.size() && pat[p] == '*') {
            star = p++;
            mark = t;
        } else if (star != std::string::npos) {
            p = star + 1;
            t = ++mark;
        } else {
            return false;
        }
    }
    while (p < pat.size() && pat[p] == '*') {
        ++p;
    }
    return p == pat.size();
}

/// РАЗБОР КЛЮЧЕЙ, ВЫПЕЧКА, ОТКАЗЫ, МАНИФЕСТ — один порядок на обе кузницы.
///
/// refused — ссылка на список отказов по связности САМОЙ кузницы: судья стоит
/// на каждом сохранении, и список читается ПОСЛЕ выпечки.
/// check — судья полки (--check); пустая функция значит «у этой кузницы судьи
/// нет», и ключ отвергается, а не притворяется сработавшим.
[[nodiscard]] inline int run_forge(int argc, char** argv, const char* tool,
                                   const std::vector<Recipe>& book,
                                   const std::vector<std::string>& refused,
                                   const std::function<int(const char*)>& check) {
    bool list_only = false;
    std::string only;
    std::string check_dir;
    bool want_check = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--check") {
            want_check = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                check_dir = argv[++i];
            }
        } else if (a == "--list") {
            list_only = true;
        } else if (a == "--only") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s: --only без маски\n", tool);
                return 2;
            }
            only = argv[++i];
        } else if (a.rfind("--only=", 0) == 0) {
            only = a.substr(7);
        } else {
            std::fprintf(stderr, "%s: неизвестный ключ \"%s\" — ОТКАЗ\n", tool,
                         a.c_str());
            return 2;
        }
    }

    if (want_check) {
        if (!check) {
            std::fprintf(stderr, "%s: судьи полки у этой кузницы нет\n", tool);
            return 2;
        }
        return check(check_dir.empty() ? SHELF : check_dir.c_str());
    }

    std::vector<const Recipe*> picked;
    for (const Recipe& r : book) {
        if (only.empty() || wildcard_match(only, r.name)) {
            picked.push_back(&r);
        }
    }

    if (list_only) {
        for (const Recipe* r : picked) {
            std::printf("%-24s %-8s %s\n", r->name.c_str(), r->family.c_str(),
                        r->file.c_str());
        }
        std::printf("%s: рецептов %zu из %zu\n", tool, picked.size(), book.size());
        return 0;
    }

    if (picked.empty()) {
        // МАСКА БЕЗ СОВПАДЕНИЙ — ОПЕЧАТКА, А НЕ ПУСТАЯ РАБОТА. Молчаливый
        // нулевой выход выглядел бы как «испеклось», и город собрался бы на
        // вчерашних файлах.
        std::fprintf(stderr, "%s: маска \"%s\" не совпала ни с одним рецептом — ОТКАЗ\n",
                     tool, only.c_str());
        return 2;
    }

    for (const Recipe* r : picked) {
        r->bake(r->file.c_str());
    }

    if (!refused.empty()) {
        std::fprintf(stderr, "forge: ОТКАЗАНО ПО СВЯЗНОСТИ, рецептов %zu:\n",
                     refused.size());
        for (const std::string& p : refused) {
            std::fprintf(stderr, "  %s\n", p.c_str());
        }
        return 3;
    }

    // МАНИФЕСТ ПИШЕТСЯ ВСЕГДА, И ВСЕГДА ПРО ВСЮ ПОЛКУ: он замеряет файлы, а не
    // то, что пеклось этим прогоном, поэтому после --only остаётся полным.
    if (!write_house_manifest(SHELF)) {
        return 4;
    }
    std::fprintf(stderr, "%s: испечено %zu рецептов\n", tool, picked.size());
    return 0;
}

} // namespace dfn::forge
