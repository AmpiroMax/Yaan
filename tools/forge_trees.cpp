/*
Created: 14:08:2026 - 23:36:19
Last updated: 16:08:2026 - 22:40:39
Module: tools
File: tools/forge_trees.cpp

Responsibility:
- The forge CLI (dfn_forge): builds the gallery of registry trees offline and
  writes them to assets/objects/trees (.dfo files), plus the human-readable index
  the registry contract asks for (manifests in git, в35). Prints per-object
  stats — wood/leaf triangles and the leaf share — because the research table
  (TREE_MODELS_RESEARCH §2) states its targets in exactly those columns.

Usage:
    dfn_forge [<out_dir>]     (default assets/objects/trees; run from repo root)

Dependencies:
- Uses: engine/render (TreeForge, ObjectRegistry).
- Used by: humans and agents producing registry objects.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- A bake that cannot round-trip its own file is refused OUT LOUD: every
  written object is read back and its hash compared before the tool reports
  success — a registry seeded with unreadable files indexes nothing.
*/
/*
UPD:
- 14:08:2026 - 23:36:19: Created — first gallery: three oaks, two birches.
- 15:08:2026 - 00:24:00: Гигант great-forge-oak в галерее (референс пользователя — дерево-поселение:
  «мне вот таких размеров деревья тоже нужны»): 46 м, крона 20 м радиуса, 10
  каркасных ветвей; лапы в АБСОЛЮТНЫХ метрах почти как у обычного дуба — листья
  гиганта не растут вместе с ним.
- 15:08:2026 - 00:45:20: Две ели (spruce-forge-a/b, запрошены дословно: «добавь на демку елки») —
  юбка почти до земли, 10-12 мутовок, хвойный тон; лапы ели крупнее (0.42).
- 15:08:2026 - 01:04:30: ЛИНЕЙКА РАЗМЕРОВ по docs/SKYRIM_TREES_RESEARCH.md §3: juniper 4.5 м,
  aspen 14 м, две лесные СОСНЫ 28/36 м с голой нижней болой (crown_base 0.45 —
  сосна, не большая ель), и КОЛОСС 200 м / крона 140 м (просьба «раза в 4-5
  больше гиганта») на собственной полке assets/objects/colossus и карте
  trees/colossus.
- 15:08:2026 - 02:14:30: ели ×2 плотнее (мутовки 15-18, ветви 9, лапы 2×0.5) — «антенна»; колосс —
  ДУБ: 150 м под кроной 170 м, ветви с 22 м (наука дуба из чата колосса).
- 15:08:2026 - 16:17:07: ЛИСТВЕННИЦА larch-forge-a (просьба пользователя дважды): фрондовая грамматика, светлая перистая хвоя WillowOlive, тонкие ленты 0.55, провис 0.14, крона прозрачная с 0.20h. Плюс снят древний -Wcomment в шапке.
- 16:08:2026 - 22:06:42: Листы гиганта/колосса не растут с деревом: spray_frac 0.15->0.09 и 0.10->0.05 — лист размером с крону штампует листья-одеяла.
- 16:08:2026 - 22:40:39: Ель 7 ветвей на мутовку (складка ×2 трисов); колосс/гигант spray_per_branch 3.
*/

#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/TreeForge.h"

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
        std::fprintf(stderr, "[forge] cannot create %s: %s\n", out_dir.string().c_str(),
                     ec.message().c_str());
        return 1;
    }

    // The first gallery. Heights/crowns sit inside the species bands the
    // design already ratified (OAK/BIRCH_HEIGHT bands in NUMBERS.md); seeds
    // are arbitrary and PINNED — the gallery must re-bake byte-identical.
    std::vector<TreeForgeParams> gallery;
    for (int i = 0; i < 3; ++i) {
        TreeForgeParams oak;
        oak.seed = 101 + static_cast<uint64_t>(i);
        oak.name = "oak-forge-" + std::string(1, static_cast<char>('a' + i));
        oak.height = 15.0f + static_cast<float>(i) * 1.6f;
        oak.crown_radius = 5.2f + static_cast<float>(i) * 0.5f;
        oak.trunk_radius = 0.40f + static_cast<float>(i) * 0.04f;
        oak.tone = LeafTone::OakMid;
        oak.card_shape = LeafShape::RoundLobed;
        gallery.push_back(oak);
    }
    for (int i = 0; i < 2; ++i) {
        TreeForgeParams birch;
        birch.seed = 201 + static_cast<uint64_t>(i);
        birch.name = "birch-forge-" + std::string(1, static_cast<char>('a' + i));
        birch.height = 13.0f + static_cast<float>(i) * 1.5f;
        birch.crown_radius = 3.4f + static_cast<float>(i) * 0.4f;
        birch.crown_base_frac = 0.42f;
        birch.trunk_radius = 0.26f;
        birch.bark = {0.80f, 0.79f, 0.74f}; // birch bark: the brightest flora value
        birch.tone = LeafTone::BirchLight;
        birch.card_shape = LeafShape::OvalSpray;
        birch.scaffold_count = 4;
        birch.secondary_per_scaffold = 3;
        birch.spray_frac = 0.28f; // birch: fewer, slightly larger airy sprays
        gallery.push_back(birch);
    }
    for (int i = 0; i < 2; ++i) {
        // ЁЛКИ — the user's reference frames are half conifer forest, and he
        // asked for them by name («добавь на демку елки, как ты их видишь»).
        TreeForgeParams spruce;
        spruce.seed = 301 + static_cast<uint64_t>(i);
        spruce.name = "spruce-forge-" + std::string(1, static_cast<char>('a' + i));
        spruce.height = 17.0f + static_cast<float>(i) * 3.0f;
        spruce.crown_radius = 3.1f + static_cast<float>(i) * 0.4f;
        spruce.crown_base_frac = 0.10f; // the skirt nearly reaches the ground
        spruce.trunk_radius = 0.30f;
        spruce.bark = {0.23f, 0.16f, 0.11f}; // red-brown spruce bark
        spruce.tone = LeafTone::ConiferDark;
        spruce.card_shape = LeafShape::NeedleFan;
        spruce.conifer = true;
        spruce.whorl_count = 15 + i * 3;
        spruce.whorl_branches = 7; // folded fronds cost 2x tris
        spruce.droop = 0.30f;
        spruce.spray_per_branch = 2;
        spruce.spray_frac = 0.5f;
        gallery.push_back(spruce);
    }
    {
        // JUNIPER — the Reach's crooked shrub-tree (research §3: 4-6 m), the
        // small end of the ladder the user asked for («несколько мелких»).
        TreeForgeParams juniper;
        juniper.seed = 401;
        juniper.name = "juniper-forge-a";
        juniper.height = 4.5f;
        juniper.crown_radius = 2.6f;
        juniper.crown_base_frac = 0.30f;
        juniper.trunk_radius = 0.16f;
        juniper.bark = {0.42f, 0.40f, 0.38f}; // pale twisted grey
        juniper.tone = LeafTone::ConiferDark;
        juniper.card_shape = LeafShape::NeedleFan;
        juniper.scaffold_count = 4;
        juniper.spray_frac = 0.42f;
        gallery.push_back(juniper);
    }
    {
        // ASPEN — the Rift's tree (research §3: 12-18 m, pale bole, golden
        // shimmer). BirchPale is the closest ratified tone band.
        TreeForgeParams aspen;
        aspen.seed = 411;
        aspen.name = "aspen-forge-a";
        aspen.height = 14.0f;
        aspen.crown_radius = 3.6f;
        aspen.crown_base_frac = 0.4f;
        aspen.trunk_radius = 0.24f;
        aspen.bark = {0.74f, 0.74f, 0.68f};
        aspen.tone = LeafTone::BirchPale;
        aspen.card_shape = LeafShape::OvalSpray;
        aspen.scaffold_count = 5;
        aspen.spray_frac = 0.26f;
        gallery.push_back(aspen);
    }
    for (int i = 0; i < 2; ++i) {
        // TALL FOREST PINES (research §3: the vanilla forest pine is 25-40 m,
        // and «таких деревьев должно быть много»). Conifer grammar with a BARE
        // LOWER BOLE: the crown starts near half height, which is what makes a
        // forest pine a pine and not a big spruce.
        TreeForgeParams pine;
        pine.seed = 421 + static_cast<uint64_t>(i);
        pine.name = "pine-forge-" + std::string(1, static_cast<char>('a' + i));
        pine.height = 28.0f + static_cast<float>(i) * 8.0f;
        pine.crown_radius = 4.6f + static_cast<float>(i) * 0.6f;
        pine.crown_base_frac = 0.45f;
        pine.trunk_radius = 0.55f + static_cast<float>(i) * 0.1f;
        pine.bark = {0.30f, 0.19f, 0.12f}; // red-brown plated pine bark
        pine.tone = LeafTone::ConiferDark;
        pine.card_shape = LeafShape::NeedleFan;
        pine.conifer = true;
        pine.whorl_count = 8;
        pine.whorl_branches = 7;
        pine.droop = 0.22f;
        pine.spray_per_branch = 2;
        pine.spray_frac = 0.34f;
        gallery.push_back(pine);
    }
    {
        // LARCH — asked for by name, twice («хвойные деревья, листвинница»).
        // Same frond grammar as the spruce, different dress (passports queue):
        // LIGHT feathery needles (WillowOlive row — the warm light olive),
        // thin fronds, small droop, a crown the sky shows through.
        TreeForgeParams larch;
        larch.seed = 431;
        larch.name = "larch-forge-a";
        larch.height = 21.0f;
        larch.crown_radius = 4.4f;
        larch.crown_base_frac = 0.20f;
        larch.trunk_radius = 0.38f;
        larch.bark = {0.34f, 0.22f, 0.14f}; // reddish larch bark
        larch.tone = LeafTone::WillowOlive;
        larch.card_shape = LeafShape::NeedleFan;
        larch.conifer = true;
        larch.whorl_count = 13;
        larch.whorl_branches = 6;
        larch.droop = 0.14f;
        larch.spray_per_branch = 2;
        larch.frond_width = 0.55f;
        gallery.push_back(larch);
    }
    {
        // THE GIANT — the user's settlement-tree reference: «очень большое
        // дерево, на котором живут... мне вот таких размеров деревья тоже
        // нужны». Same recipe, landmark proportions; the dwellings are
        // gameplay's story, the TREE is the forge's.
        TreeForgeParams giant;
        giant.seed = 777;
        giant.name = "great-forge-oak";
        giant.height = 46.0f;
        giant.crown_radius = 20.0f;
        giant.crown_base_frac = 0.30f;
        giant.trunk_radius = 2.6f;
        giant.tone = LeafTone::OakDeep;
        giant.card_shape = LeafShape::RoundLobed;
        giant.scaffold_count = 10;
        giant.secondary_per_scaffold = 4;
        giant.spray_per_branch = 3;
        giant.spray_frac = 0.09f; // sheets stay leaf-scaled: giant leaves do not grow with the giant
        giant.core_frac = 0.30f;
        gallery.push_back(giant);
    }

    std::string index;
    index += "# Реестр объектов: деревья кузницы\n#\n";
    index += "# name | file | content_hash | wood_tris | leaf_tris | leaf_share\n";
    bool all_ok = true;
    for (const TreeForgeParams& params : gallery) {
        const RegistryObject obj = forge_tree(params);
        const fs::path file = out_dir / (params.name + ".dfo");
        if (!write_object(obj, file)) {
            std::fprintf(stderr, "[forge] %s: WRITE FAILED\n", params.name.c_str());
            all_ok = false;
            continue;
        }
        // Round trip before claiming success: a registry seeded with files
        // that cannot be read back indexes nothing.
        const auto back = read_object(file);
        if (!back || back->content_hash != obj.content_hash) {
            std::fprintf(stderr, "[forge] %s: ROUND TRIP FAILED\n", params.name.c_str());
            all_ok = false;
            continue;
        }
        const size_t wood_tris = obj.wood.indices.size() / 3 + obj.ground.indices.size() / 3;
        const size_t leaf_tris = obj.cards.indices.size() / 3;
        // The masses live in the wood STREAM (opaque program) but are LEAF by
        // subject; count them by colour would be fragile, so the honest split
        // reported here is stream-wise and the leaf-share row in the research
        // table is checked by eye on the gallery frames. Stated, not hidden.
        const double share = static_cast<double>(leaf_tris)
                           / static_cast<double>(wood_tris + leaf_tris);
        std::printf("[forge] %-14s %6zu wood+ground  %4zu card  (%.0f%% cards)  hash %016llx\n",
                    params.name.c_str(), wood_tris, leaf_tris, share * 100.0,
                    static_cast<unsigned long long>(obj.content_hash));
        char row[256];
        std::snprintf(row, sizeof(row), "%s | %s | %016llx | %zu | %zu | %.2f\n",
                      params.name.c_str(), file.filename().string().c_str(),
                      static_cast<unsigned long long>(obj.content_hash), wood_tris,
                      leaf_tris, share);
        index += row;
    }
    {
        // THE COLOSSUS — the user's ask, verbatim: «надо сделать прям
        // гигантское дерево, которое будет раза в 4-5 ещё больше чем это»
        // (settlement-scale, the Eldergleam class of research §3). Its crown
        // is wider than half the chunk, so it lives on its OWN SHELF and its
        // own map (trees/colossus) instead of crowding the gallery.
        TreeForgeParams colossus;
        colossus.seed = 999;
        colossus.name = "colossus-oak";
        // OAK PROPORTIONS (the colossus chat, 01:35: «колосс хочется чтобы был
        // дубом, а у них крона низко начинается»; oak habit: crown WIDER than
        // the tree is tall, dome from lateral scaffolds, base low): 150 m tall
        // under a 170 m crown, branches from 22 m up.
        colossus.height = 150.0f;
        colossus.crown_radius = 85.0f;
        colossus.crown_base_frac = 0.15f;
        colossus.trunk_radius = 10.0f;
        colossus.tone = LeafTone::OakDeep;
        colossus.card_shape = LeafShape::RoundLobed;
        colossus.scaffold_count = 16;
        colossus.secondary_per_scaffold = 4;
        colossus.spray_per_branch = 3; // «листвы мало» (16.08): more sheets, not bigger leaves
        colossus.spray_frac = 0.05f; // sheets ~4 m: a colossus' leaves stay leaves
        const fs::path shelf = out_dir.parent_path() / "colossus";
        std::error_code cec;
        fs::create_directories(shelf, cec);
        const RegistryObject obj = forge_tree(colossus);
        const fs::path file = shelf / (colossus.name + ".dfo");
        if (!write_object(obj, file) || !read_object(file)) {
            std::fprintf(stderr, "[forge] colossus: FAILED\n");
            all_ok = false;
        } else {
            std::printf("[forge] %-14s %6zu wood+ground  %4zu card  -> %s\n",
                        colossus.name.c_str(),
                        obj.wood.indices.size() / 3 + obj.ground.indices.size() / 3,
                        obj.cards.indices.size() / 3, shelf.string().c_str());
        }
    }

    const fs::path index_path = out_dir / "INDEX.md";
    {
        FILE* f = std::fopen(index_path.string().c_str(), "w");
        if (f != nullptr) {
            std::fwrite(index.data(), 1, index.size(), f);
            std::fclose(f);
        } else {
            std::fprintf(stderr, "[forge] cannot write %s\n", index_path.string().c_str());
            all_ok = false;
        }
    }
    std::printf("[forge] %zu objects -> %s\n", gallery.size(), out_dir.string().c_str());
    return all_ok ? 0 : 1;
}
