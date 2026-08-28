/*
Created: 28:08:2026 - 14:20:00
Last updated: 28:08:2026 - 14:20:00
Module: tools
File: tools/check_bevel.cpp

Responsibility:
- dfn_bevel_check: ПРИБОР КРИТЕРИЯ К4 ТЗ материалов («на объекте убранства не
  должно остаться ни одного ребра без фаски; доля рёбер с фаской = 100 %;
  проверка геометрическая, не по кадру») И СЧЁТ ТРЕУГОЛЬНИКОВ, которым эта
  доля куплена. Обе величины — на одном прогоне и по одному мешу, потому что
  порознь они лгут: сто процентов рёбер за впятеро больший бюджет — это не
  сдача, а счёт к оплате.

Usage:
    dfn_bevel_check <чертёж.dfh | сцена.scene> [ещё ...]
                    [--bevel <метры>]   ширина фаски (умолчание — движковое)
                    [--zero]            то же, что --bevel 0 (плечо «до»)
                    [--require-k4 <доля>]  ненулевой выход, если К4 ниже
                    [--k4-report <файл>]   числа для tools/quality/measure_surface.py
                    [--quiet]

  Запускать из корня репозитория. Сцена разбирается как город: каждый
  [house] строится своим чертежом и считается отдельно, итог — сумма.

Dependencies:
- Uses: engine/world (HouseFile, HouseMesh, house_edge_census, Scene).
- Used by: рукав ctest bevel_k4_stand, отчёт docs/reports/bevel.html, человек.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ПРИБОР ДЕРЖИТ ОБА ПЛЕЧА В ОДНОМ ДВОИЧНОМ ФАЙЛЕ. «До» — это `--zero`, а не
  прошлый коммит: сравнение с прошлым коммитом мерило бы заодно и всё
  остальное, что за сутки поменялось в дереве (правило 47).
- РУКА ПЕЧАТАЕТ ЧИСЛА, ПО КОТОРЫМ СУДИТ, а не только приговор (правило 29,
  выведенные правила рукавов): вердикт «К4 = 100 %» без числа рёбер не
  отличим от вердикта на пустом меше.
*/
/*
UPD:
- 28:08:2026 - 14:20:00: Создан вместе с фаской (волна материалов-1).
*/

#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseMesh.h"
#include "engine/world/sources/Scene.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

[[nodiscard]] bool read_graph(const std::string& path, dfn::world::HouseGraph& g) {
    std::ifstream in(path);
    if (!in.good()) {
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return dfn::world::read_house(ss.str(), g).ok;
}

struct Total {
    std::size_t tris = 0;
    std::size_t verts = 0;
    int convex = 0;
    int sharp = 0;
    float convex_len = 0.0f;
    float sharp_len = 0.0f;
    int graphs = 0;
};

void account(Total& t, const dfn::world::HouseMesh& m) {
    const dfn::world::HouseEdgeCensus c = dfn::world::house_edge_census(m);
    t.tris += m.triangle_count();
    t.verts += m.vertices.size();
    t.convex += c.convex;
    t.sharp += c.sharp;
    t.convex_len += c.convex_len_m;
    t.sharp_len += c.sharp_len_m;
    ++t.graphs;
}

[[nodiscard]] float share_of(const Total& t) {
    return t.convex == 0 ? 1.0f
                         : static_cast<float>(t.convex - t.sharp) / static_cast<float>(t.convex);
}

} // namespace

int main(int argc, char** argv) {
    float bevel = dfn::world::HOUSE_BEVEL_W_DEFAULT;
    float require_k4 = -1.0f;
    bool quiet = false;
    std::string k4_report;
    std::vector<std::string> inputs;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--bevel" && i + 1 < argc) {
            bevel = std::strtof(argv[++i], nullptr);
        } else if (a == "--zero") {
            bevel = 0.0f;
        } else if (a == "--require-k4" && i + 1 < argc) {
            require_k4 = std::strtof(argv[++i], nullptr);
        } else if (a == "--k4-report" && i + 1 < argc) {
            k4_report = argv[++i];
        } else if (a == "--quiet") {
            quiet = true;
        } else {
            inputs.push_back(a);
        }
    }
    if (inputs.empty()) {
        std::fprintf(stderr,
                     "dfn_bevel_check <чертёж.dfh | сцена.scene> ... [--bevel <м>] "
                     "[--zero] [--require-k4 <доля>] [--k4-report <файл>]\n");
        return 2;
    }

    Total all;
    std::printf("# фаска %.4f м; К4 — доля выпуклых рёбер с изломом не круче %.0f°\n", bevel,
                static_cast<double>(dfn::world::HOUSE_BEVEL_TURN_DEG));
    std::printf("%-38s %8s %8s %8s %8s %7s\n", "предмет", "трис", "рёбер", "острых", "К4,%",
                "макс°");
    for (const std::string& in : inputs) {
        const bool is_scene = in.size() > 6 && in.rfind(".scene") == in.size() - 6;
        std::vector<std::pair<std::string, std::string>> graphs; // подпись -> файл
        if (is_scene) {
            dfn::world::SceneDoc doc;
            std::string err;
            if (!dfn::world::read_scene(in, doc, err)) {
                std::fprintf(stderr, "[фаска] сцена %s не прочиталась: %s\n", in.c_str(),
                             err.c_str());
                return 3;
            }
            for (const dfn::world::ScenePlacedHouse& h : doc.houses) {
                graphs.emplace_back(std::filesystem::path(h.file).stem().string(), h.file);
            }
        } else {
            graphs.emplace_back(std::filesystem::path(in).stem().string(), in);
        }
        Total per_input;
        for (const auto& [label, file] : graphs) {
            dfn::world::HouseGraph g;
            if (!read_graph(file, g)) {
                std::fprintf(stderr, "[фаска] чертёж %s не прочитался\n", file.c_str());
                return 3;
            }
            const dfn::world::HouseMesh m = dfn::world::build_house_mesh(g, bevel);
            const dfn::world::HouseEdgeCensus c = dfn::world::house_edge_census(m);
            if (!quiet && !is_scene) {
                std::printf("%-38s %8zu %8d %8d %8.1f %7.1f\n", label.c_str(),
                            m.triangle_count(), c.convex, c.sharp,
                            static_cast<double>(c.share() * 100.0f),
                            static_cast<double>(c.worst_deg));
            }
            account(per_input, m);
            account(all, m);
        }
        if (!quiet && is_scene) {
            std::printf("%-38s %8zu %8d %8d %8.1f %7s   (%d построек)\n",
                        std::filesystem::path(in).stem().string().c_str(), per_input.tris,
                        per_input.convex, per_input.sharp,
                        static_cast<double>(share_of(per_input) * 100.0f), "-",
                        per_input.graphs);
        }
    }
    const float k4 = share_of(all);
    std::printf("ИТОГО: %d графов, трис %zu, вершин %zu, выпуклых рёбер %d, острых %d, "
                "К4 %.2f%%, длина острых %.2f м из %.2f м\n",
                all.graphs, all.tris, all.verts, all.convex, all.sharp,
                static_cast<double>(k4 * 100.0f), static_cast<double>(all.sharp_len),
                static_cast<double>(all.convex_len));

    if (!k4_report.empty()) {
        std::ofstream out(k4_report);
        if (!out.good()) {
            std::fprintf(stderr, "[фаска] не открылся файл отчёта %s\n", k4_report.c_str());
            return 3;
        }
        out << "# dfn_bevel_check: числа критерия К4\n";
        out << "bevel_m " << bevel << "\n";
        out << "graphs " << all.graphs << "\n";
        out << "tris " << all.tris << "\n";
        out << "convex_edges " << all.convex << "\n";
        out << "sharp_edges " << all.sharp << "\n";
        out << "k4_share " << k4 << "\n";
    }

    if (require_k4 >= 0.0f && k4 + 1e-6f < require_k4) {
        std::fprintf(stderr, "[фаска] К4 %.2f%% ниже требуемых %.2f%% — %d острых рёбер\n",
                     static_cast<double>(k4 * 100.0f), static_cast<double>(require_k4 * 100.0f),
                     all.sharp);
        return 1;
    }
    return 0;
}
