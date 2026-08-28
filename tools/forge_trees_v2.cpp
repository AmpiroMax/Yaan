/*
Created: 28:08:2026 - 17:10:00
Last updated: 28:08:2026 - 17:10:00
Module: tools
File: tools/forge_trees_v2.cpp

Responsibility:
- The SECOND ITERATION's forge CLI (dfn_forge2): bakes the v2 recipes into
  assets/objects/trees BESIDE the first iteration's files and writes its own
  index, INDEX-V2.md.

Usage:
    dfn_forge2 [<out_dir>]    (default assets/objects/trees; run from repo root)

WHY A SECOND TOOL AND NOT A SECTION OF dfn_forge. dfn_forge rewrites every one
of the 82 files it owns on every run. Today those files are still written in
container v2 while the code emits v3 (a change from commit 9e2c8c9, the beds
wave), so running it would touch all 82 on disk for a reason that has nothing to
do with this wave. A separate tool writes ONLY the new names — «не порти
текущих» becomes a property of what the program can reach, not of the care taken
while running it.

Dependencies:
- Uses: engine/render (TreeForgeV2, ObjectRegistry).
- Used by: humans and agents producing v2 registry objects.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- A bake that cannot round-trip its own file is refused OUT LOUD.
- THIS TOOL MUST NEVER WRITE A NAME THE FIRST ITERATION OWNS. Every recipe
  below carries "-v2-" in its name, and the guard at the bottom of main()
  refuses the run if one does not.
*/
/*
UPD:
- 28:08:2026 - 17:10:00: Создан — три структурных закона по два габитуса и два
  посева: дуб-v2 (луговой), бук-v2 (лесной ярусный), акация-v2 (многоствольная).
*/

#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/TreeForgeV2.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    using namespace dfn::render;

    const fs::path out_dir = argc > 1 ? fs::path(argv[1]) : fs::path("assets/objects/trees");
    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        std::fprintf(stderr, "[forge2] cannot create %s: %s\n", out_dir.string().c_str(),
                     ec.message().c_str());
        return 1;
    }

    std::vector<TreeV2Params> shelf;

    // --- ЗАКОН 1: ЛУГОВОЙ ЛИСТВЕННЫЙ (дуб). Крона низко, шириной в рост,
    // глубиной 0.78 роста — «кочан», а не «леденец». Два посева на габитус,
    // потому что рощица из клонов — это витрина одного дерева.
    for (int i = 0; i < 2; ++i) {
        TreeV2Params oak;
        oak.seed = 2101 + static_cast<uint64_t>(i);
        oak.name = "oak-v2-luga-" + std::string(1, static_cast<char>('a' + i));
        oak.habit = TreeHabit::Solitary;
        oak.height = 14.0f + 1.6f * static_cast<float>(i);
        oak.trunk_radius = 0.46f + 0.04f * static_cast<float>(i);
        oak.bark = {0.17f, 0.13f, 0.10f};
        oak.card_shape = LeafShape::RoundLobed;
        oak.lobes = 7 + i;              // 7 и 8 долей — оба внутри полосы 5-9
        oak.lean_rad = 0.13f - 0.04f * static_cast<float>(i);
        oak.lean_dir = 0.7f + 2.1f * static_cast<float>(i);
        oak.curve_frac = 0.055f + 0.02f * static_cast<float>(i);
        oak.snags = 5 + i;
        shelf.push_back(oak);

        // ТОТ ЖЕ ВИД, ЛЕСНОЙ РЕЦЕПТ (разница №2): крона высоко и узко, бола
        // чистая на 0.58 роста, сучьев по ней больше. Это не тот же рецепт под
        // другим углом — это другая форма.
        TreeV2Params oakf = oak;
        oakf.seed = 2111 + static_cast<uint64_t>(i);
        oakf.name = "oak-v2-forest-" + std::string(1, static_cast<char>('a' + i));
        oakf.habit = TreeHabit::Forest;
        oakf.height = 19.0f + 2.5f * static_cast<float>(i);
        oakf.trunk_radius = 0.36f + 0.03f * static_cast<float>(i);
        oakf.lobes = 6 + i;
        oakf.lean_rad = 0.07f + 0.03f * static_cast<float>(i);
        oakf.snags = 8 + i;
        shelf.push_back(oakf);
    }

    // --- ЗАКОН 2: ЛЕСНОЙ ЯРУСНЫЙ (бук/ясень). Длинная гладкая бола, крона
    // смыкается потолком, лист мельче и округлее дубового (колонка OvalSpray).
    for (int i = 0; i < 2; ++i) {
        TreeV2Params beech;
        beech.seed = 2201 + static_cast<uint64_t>(i);
        beech.name = "beech-v2-forest-" + std::string(1, static_cast<char>('a' + i));
        beech.habit = TreeHabit::Forest;
        beech.height = 22.0f + 3.0f * static_cast<float>(i);
        beech.trunk_radius = 0.40f + 0.05f * static_cast<float>(i);
        beech.bark = {0.30f, 0.29f, 0.26f};   // гладкая серая кора бука
        beech.card_shape = LeafShape::OvalSpray;
        beech.lobes = 6;
        beech.lean_rad = 0.05f + 0.04f * static_cast<float>(i);
        beech.lean_dir = 3.9f - 1.4f * static_cast<float>(i);
        beech.curve_frac = 0.035f;
        beech.snags = 9;
        shelf.push_back(beech);

        TreeV2Params beechs = beech;
        beechs.seed = 2211 + static_cast<uint64_t>(i);
        beechs.name = "beech-v2-luga-" + std::string(1, static_cast<char>('a' + i));
        beechs.habit = TreeHabit::Solitary;
        beechs.height = 15.0f + 2.0f * static_cast<float>(i);
        beechs.trunk_radius = 0.50f + 0.05f * static_cast<float>(i);
        beechs.lobes = 8;
        beechs.lean_rad = 0.11f;
        beechs.snags = 4;
        shelf.push_back(beechs);
    }

    // --- ЗАКОН 3: МНОГОСТВОЛЬНАЯ (акация/ольха). Три-четыре ствола ОТ ЗЕМЛИ,
    // крона шире собственной высоты. Лист — узкий клин (RaggedTip).
    for (int i = 0; i < 2; ++i) {
        TreeV2Params ac;
        ac.seed = 2301 + static_cast<uint64_t>(i);
        ac.name = "acacia-v2-luga-" + std::string(1, static_cast<char>('a' + i));
        ac.habit = TreeHabit::MultiStem;
        ac.height = 11.0f + 1.5f * static_cast<float>(i);
        ac.trunk_radius = 0.40f;
        ac.bark = {0.21f, 0.17f, 0.12f};
        ac.card_shape = LeafShape::RaggedTip;
        ac.stems = 3 + i;                 // три и четыре ствола
        ac.lobes = 7;
        ac.lean_rad = 0.10f;
        ac.lean_dir = 1.9f * static_cast<float>(i);
        ac.curve_frac = 0.07f;
        ac.snags = 4;
        shelf.push_back(ac);

        TreeV2Params alder = ac;
        alder.seed = 2311 + static_cast<uint64_t>(i);
        alder.name = "alder-v2-forest-" + std::string(1, static_cast<char>('a' + i));
        alder.habit = TreeHabit::MultiStem;
        alder.height = 15.0f + 2.0f * static_cast<float>(i);
        // Многоствольная В ЛЕСУ: та же грамматика стволов, но крона поднята и
        // ужата — соседи не дают ей разойтись вширь.
        alder.crown_width_frac = 0.86f;
        alder.crown_depth_frac = 0.52f;
        alder.stems = 3;
        alder.lobes = 6;
        alder.bark = {0.24f, 0.22f, 0.19f};
        alder.snags = 7;
        shelf.push_back(alder);
    }

    std::string index;
    index += "# Реестр объектов: деревья ВТОРОЙ ИТЕРАЦИИ (28.08.2026)\n#\n";
    index += "# Полка общая с первой итерацией НАМЕРЕННО (распоряжение владельца\n"
             "# 28.08: «новые .dfo РЯДОМ со старыми»). Ни один файл первой\n"
             "# итерации этой кузницей не пишется — см. шапку forge_trees_v2.cpp.\n#\n";
    index += "# Габитус: luga — одиночное дерево (крона низко, чистой болы четверть\n"
             "# роста); forest — лесной рецепт ТОГО ЖЕ вида (крона высоко, бола\n"
             "# длинная и чистая). Это разница №2 записки docs/reports/trees-g3.\n#\n";
    index += "# W/H и глубина кроны — ИЗМЕРЕННЫЕ (measure_object), не рецептурные.\n#\n";
    index += "# name | file | habit | leaf_rows | content_hash | wood_tris | leaf_tris"
             " | H | W/H | crown_depth_h | clean_bole_h\n";

    bool all_ok = true;
    size_t written = 0;
    for (const TreeV2Params& p : shelf) {
        if (p.name.find("-v2-") == std::string::npos) {
            std::fprintf(stderr, "[forge2] %s: имя без \"-v2-\" — ОТКАЗ\n",
                         p.name.c_str());
            all_ok = false;
            continue;
        }
        const RegistryObject obj = forge_tree_v2(p);
        const fs::path file = out_dir / (p.name + ".dfo");
        if (!write_object(obj, file)) {
            std::fprintf(stderr, "[forge2] %s: WRITE FAILED\n", p.name.c_str());
            all_ok = false;
            continue;
        }
        const auto back = read_object(file);
        if (!back || back->content_hash != obj.content_hash) {
            std::fprintf(stderr, "[forge2] %s: ROUND TRIP FAILED\n", p.name.c_str());
            all_ok = false;
            continue;
        }
        ++written;
        const size_t wood_tris = obj.wood.indices.size() / 3
                               + obj.ground.indices.size() / 3
                               + obj.bark.indices.size() / 3;
        const size_t leaf_tris = obj.cards.indices.size() / 3;
        const ObjectExtent ext = measure_object(obj);
        const double H = static_cast<double>(ext.top - ext.bottom);
        const double W = static_cast<double>(ext.radius * 2.0f);
        // CROWN DEPTH, MEASURED OFF THE FOLIAGE ITSELF. Difference №1 of the
        // note is a claim about where the LEAVES are, not about a recipe field:
        // «крона занимает больше половины высоты и спускается по бокам». The
        // only honest instrument is the card stream's own vertical extent.
        float leaf_lo = 1e9f;
        float leaf_hi = -1e9f;
        for (const auto& v : obj.cards.vertices) {
            leaf_lo = std::min(leaf_lo, v.position.y);
            leaf_hi = std::max(leaf_hi, v.position.y);
        }
        const double crown_depth = (leaf_hi > leaf_lo)
                                 ? static_cast<double>(leaf_hi - leaf_lo) / std::max(H, 0.01)
                                 : 0.0;
        const double clean_bole = (leaf_lo < 1e8f)
                                ? static_cast<double>(leaf_lo) / std::max(H, 0.01) : 0.0;
        const char* habit = p.habit == TreeHabit::Forest ? "forest"
                          : (p.habit == TreeHabit::MultiStem ? "multi" : "luga");
        std::printf("[forge2] %-22s %-6s  %5zu wood  %4zu leaf  H %5.1f  W %5.1f"
                    "  W/H %.2f  крона %.2f h  чистой болы %.2f h  hash %016llx\n",
                    p.name.c_str(), habit, wood_tris, leaf_tris, H, W,
                    W / std::max(H, 0.01), crown_depth, clean_bole,
                    static_cast<unsigned long long>(obj.content_hash));
        char row[320];
        std::snprintf(row, sizeof(row),
                      "%s | %s | %s | %s+%s | %016llx | %zu | %zu | %.1f | %.2f"
                      " | %.2f | %.2f\n",
                      p.name.c_str(), file.filename().string().c_str(), habit,
                      leaf_tone_name(p.tone_rim), leaf_tone_name(p.tone_core),
                      static_cast<unsigned long long>(obj.content_hash), wood_tris,
                      leaf_tris, H, W / std::max(H, 0.01), crown_depth, clean_bole);
        index += row;
    }

    const fs::path index_file = out_dir / "INDEX-V2.md";
    if (FILE* f = std::fopen(index_file.string().c_str(), "wb")) {
        std::fwrite(index.data(), 1, index.size(), f);
        std::fclose(f);
    } else {
        std::fprintf(stderr, "[forge2] cannot write %s\n", index_file.string().c_str());
        all_ok = false;
    }

    std::printf("[forge2] %zu objects -> %s\n", written, out_dir.string().c_str());
    return all_ok ? 0 : 1;
}
