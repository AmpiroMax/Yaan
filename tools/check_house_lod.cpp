/*
Module: tools
File: tools/check_house_lod.cpp

Responsibility:
- dfn_house_lod: ЧЕМ ЛЕСТНИЦА ДАЛЬНИХ ФОРМ ПЛАТИТ И ЧТО ПОКУПАЕТ. Один прогон
  печёт КАЖДЫЙ чертёж тремя ступенями (полная / средняя / дальняя) и печатает
  треугольники всех трёх рядом — потому что порознь они лгут: «дальняя форма
  в двадцать раз дешевле» без числа полной формы не значит ничего.
- ТУТ ЖЕ СТОРОЖ ПОЛКИ: `--require-full <хэш>` требует, чтобы ПОЛНАЯ форма
  осталась прежней бит-в-бит. Лестница обязана быть только добавлением.

Usage:
    dfn_house_lod <чертёж.dfh | сцена.scene> [ещё ...]
                  [--bevel <метры>]      ширина фаски полной ступени
                  [--require-full <hex>] хэш полных форм; расхождение — отказ
                  [--csv <файл>]         числа по ступеням для отчёта
                  [--quiet]              только итог

  Запускать из корня репозитория. Сцена разбирается как город: каждый [house]
  строится своим чертежом, итог — сумма по городу.

Dependencies:
- Uses: engine/world (HouseFile, HouseMesh, HouseLod, Scene).
- Used by: рукава ctest house_lod_full_unchanged (+ контрольное плечо),
  отчёт artifacts/reports/house-lod.html, человек.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ТРИ СТУПЕНИ МЕРЯЮТСЯ ОДНИМ ПРОГОНОМ И ОДНИМ БИНАРНИКОМ (правило 47): «до» —
  это столбец «полная», а не прошлый коммит, иначе замер несёт заодно всё, что
  за сутки поменялось в дереве.
- ХЭШ ПОЛНОЙ ФОРМЫ СЧИТАЕТСЯ ПО ВЕРШИНАМ И ИНДЕКСАМ, а не по числу
  треугольников: равное число треугольников при разной геометрии — ровно тот
  случай, ради которого сторож и заведён.
*/

#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseLod.h"
#include "engine/world/sources/HouseMesh.h"
#include "engine/world/sources/Scene.h"

#include <array>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using dfn::world::HouseLod;
constexpr int TIERS = dfn::world::HOUSE_LOD_COUNT;

[[nodiscard]] bool read_graph(const std::string& path, dfn::world::HouseGraph& g) {
    std::ifstream in(path);
    if (!in.good()) {
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return dfn::world::read_house(ss.str(), g).ok;
}

struct Tally {
    std::array<std::size_t, TIERS> tris{};
    std::array<std::size_t, TIERS> verts{};
    std::array<std::size_t, TIERS> parts{};
    int graphs = 0;
};

void add(Tally& t, int tier, const dfn::world::HouseMesh& m) {
    t.tris[static_cast<std::size_t>(tier)] += m.triangle_count();
    t.verts[static_cast<std::size_t>(tier)] += m.vertices.size();
    t.parts[static_cast<std::size_t>(tier)] += m.parts.size();
}

/// FNV-1a по геометрии полной формы: вершины квантованы в 1/512 м — та же
/// сетка, которой AppHouse считает отпечаток для кэша AO.
void mix_mesh(std::uint64_t& h, const dfn::world::HouseMesh& m) {
    const auto mix = [&h](std::uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    mix(m.vertices.size());
    mix(m.indices.size());
    for (const dfn::world::HouseVertex& v : m.vertices) {
        mix(static_cast<std::uint64_t>(std::llround(v.pos.x * 512.0f)));
        mix(static_cast<std::uint64_t>(std::llround(v.pos.y * 512.0f)));
        mix(static_cast<std::uint64_t>(std::llround(v.pos.z * 512.0f)));
    }
    for (const std::uint32_t i : m.indices) {
        mix(i);
    }
}

[[nodiscard]] double pct(std::size_t part, std::size_t whole) {
    return whole == 0 ? 0.0 : 100.0 * static_cast<double>(part) / static_cast<double>(whole);
}

} // namespace

int main(int argc, char** argv) {
    float bevel = dfn::world::HOUSE_BEVEL_W_DEFAULT;
    bool quiet = false;
    std::string require_full;
    std::string csv;
    std::vector<std::string> inputs;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--bevel" && i + 1 < argc) {
            bevel = std::strtof(argv[++i], nullptr);
        } else if (a == "--require-full" && i + 1 < argc) {
            require_full = argv[++i];
        } else if (a == "--csv" && i + 1 < argc) {
            csv = argv[++i];
        } else if (a == "--quiet") {
            quiet = true;
        } else {
            inputs.push_back(a);
        }
    }
    if (inputs.empty()) {
        std::fprintf(stderr, "dfn_house_lod <чертёж.dfh | сцена.scene> ... "
                             "[--bevel <м>] [--require-full <hex>] [--csv <файл>] "
                             "[--quiet]\n");
        return 2;
    }

    Tally all;
    std::uint64_t full_hash = 1469598103934665603ull;
    std::vector<std::string> csv_rows;

    std::printf("# ступени дальней формы построек (И13). Фаска полной ступени %.4f м;\n"
                "# средняя входит с %.0f м, дальняя с %.0f м, перехлёст %.0f м.\n",
                static_cast<double>(bevel),
                static_cast<double>(dfn::world::HOUSE_LOD_MID_IN_M),
                static_cast<double>(dfn::world::HOUSE_LOD_FAR_IN_M),
                static_cast<double>(dfn::world::HOUSE_LOD_HYSTERESIS_M));
    std::printf("%-34s %10s %10s %10s %7s %7s\n", "предмет", "полная", "средняя",
                "дальняя", "ср,%", "дал,%");

    for (const std::string& in : inputs) {
        const bool is_scene = in.size() > 6 && in.rfind(".scene") == in.size() - 6;
        std::vector<std::pair<std::string, std::string>> graphs;
        if (is_scene) {
            dfn::world::SceneDoc doc;
            std::string err;
            if (!dfn::world::read_scene(in, doc, err)) {
                std::fprintf(stderr, "[lod] сцена %s не прочиталась: %s\n", in.c_str(),
                             err.c_str());
                return 3;
            }
            for (const dfn::world::ScenePlacedHouse& h : doc.houses) {
                graphs.emplace_back(std::filesystem::path(h.file).stem().string(), h.file);
            }
        } else {
            graphs.emplace_back(std::filesystem::path(in).stem().string(), in);
        }
        Tally per_input;
        for (const auto& [label, file] : graphs) {
            dfn::world::HouseGraph g;
            if (!read_graph(file, g)) {
                std::fprintf(stderr, "[lod] чертёж %s не прочитался\n", file.c_str());
                return 3;
            }
            std::array<std::size_t, TIERS> row{};
            for (int tier = 0; tier < TIERS; ++tier) {
                const dfn::world::HouseMesh m =
                    dfn::world::build_house_mesh(g, bevel, static_cast<HouseLod>(tier));
                if (tier == 0) {
                    mix_mesh(full_hash, m);
                }
                row[static_cast<std::size_t>(tier)] = m.triangle_count();
                add(per_input, tier, m);
                add(all, tier, m);
            }
            ++per_input.graphs;
            ++all.graphs;
            if (!quiet && !is_scene) {
                std::printf("%-34s %10zu %10zu %10zu %7.1f %7.1f\n", label.c_str(), row[0],
                            row[1], row[2], pct(row[1], row[0]), pct(row[2], row[0]));
            }
            if (!csv.empty() && !is_scene) {
                char buf[256];
                std::snprintf(buf, sizeof(buf), "%s,%zu,%zu,%zu", label.c_str(), row[0],
                              row[1], row[2]);
                csv_rows.emplace_back(buf);
            }
        }
        if (!quiet && is_scene) {
            std::printf("%-34s %10zu %10zu %10zu %7.1f %7.1f   (%d построек)\n",
                        std::filesystem::path(in).stem().string().c_str(),
                        per_input.tris[0], per_input.tris[1], per_input.tris[2],
                        pct(per_input.tris[1], per_input.tris[0]),
                        pct(per_input.tris[2], per_input.tris[0]), per_input.graphs);
            if (!csv.empty()) {
                char buf[256];
                std::snprintf(buf, sizeof(buf), "%s,%zu,%zu,%zu",
                              std::filesystem::path(in).stem().string().c_str(),
                              per_input.tris[0], per_input.tris[1], per_input.tris[2]);
                csv_rows.emplace_back(buf);
            }
        }
    }

    std::printf("ИТОГО: %d графов; трис полная %zu, средняя %zu (%.1f%%), дальняя %zu "
                "(%.1f%%); вершин %zu / %zu / %zu; частей %zu / %zu / %zu\n",
                all.graphs, all.tris[0], all.tris[1], pct(all.tris[1], all.tris[0]),
                all.tris[2], pct(all.tris[2], all.tris[0]), all.verts[0], all.verts[1],
                all.verts[2], all.parts[0], all.parts[1], all.parts[2]);
    char hash_hex[32];
    std::snprintf(hash_hex, sizeof(hash_hex), "%016llx",
                  static_cast<unsigned long long>(full_hash));
    std::printf("ХЭШ ПОЛНЫХ ФОРМ: %s\n", hash_hex);

    if (!csv.empty()) {
        std::ofstream out(csv);
        if (!out.good()) {
            std::fprintf(stderr, "[lod] не открылся файл %s\n", csv.c_str());
            return 3;
        }
        out << "# dfn_house_lod: треугольники по ступеням\n";
        out << "# имя,полная,средняя,дальняя\n";
        for (const std::string& r : csv_rows) {
            out << r << "\n";
        }
        out << "ИТОГО," << all.tris[0] << "," << all.tris[1] << "," << all.tris[2] << "\n";
        out << "# хэш полных форм " << hash_hex << "\n";
    }

    if (!require_full.empty() && require_full != hash_hex) {
        std::fprintf(stderr,
                     "[lod] ПОЛНАЯ ФОРМА ПОЛКИ ИЗМЕНИЛАСЬ: ждали %s, получили %s.\n"
                     "      Лестница дальних форм обязана быть ТОЛЬКО добавлением;\n"
                     "      если полная форма поменялась намеренно — обновите хэш\n"
                     "      в CMakeLists.txt и скажите об этом в UPD.\n",
                     require_full.c_str(), hash_hex);
        return 1;
    }
    return 0;
}
