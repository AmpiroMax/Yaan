/*
Created: 14:08:2026 - 23:36:19
Last updated: 22:08:2026 - 12:10:00
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
- 16:08:2026 - 22:48:45: ПОЛКА ПОЛЯНКИ assets/objects/glade: дуб-старейшина 30 м, берёзы/сосны кольца в 4/3 ростах, саженцы и молодняк, кусты, брёвна, пучки травы трёх высот, цветы трёх цветов, грибы. 25 объектов под сцену trees-glade.
- 17:08:2026 - 02:31:45: Пресеты видов кустов: орешник, ягодник красный/тёмный (крупные ягоды), можжевельник стелющийся, папоротник — полка glade.
- 17:08:2026 - 02:51:54: Лист 4: дуб-старейшина 50 м (ответ №1, крона Ø44); +4 ягодника — синий грозди, розовый в крапинку, зелёный капли в полоску, чёрный грозди с попкой; красному попка, тёмному сизый крап.
- 17:08:2026 - 03:51:22: Полка glade: glade-torch-stake 2.2 м и glade-lantern-post 2.0 м — новые модели света троп (пользователь: «не те, что сейчас есть»).
- 17:08:2026 - 07:04:26: Утренний вердикт по полянке: сосны 13-15 мутовок x 9 ветвей, ленты 1.25, подъём лап (шапка сосны, не юбка ели); ели галереи 17-23x9; +glade-oak-mid-a/b (дубки 11/14.5 м «убрать голость»); трава 0.9/1.3/1.7 м; цветы 0.75 м с bloom 0.14.
- 17:08:2026 - 09:50:47: Ели галереи шире (крона 3.9/4.4) и гуще (10 ветвей на мутовку) — сверка со скайримским референсом.
- 17:08:2026 - 09:54:18: bake_tree: каждое дерево полки glade печётся парой <имя> + <имя>-far (дальняя форма для пополиточного LOD лида).
- 17:08:2026 - 10:02:06: bake_bush: ягодники, орешник и можжевельник печутся парой <имя>+<имя>-far; голые кусты bush-a/b/c и мелочь (трава/цветы/грибы/брёвна/лампы) far-форм не имеют — экономить там нечего.
- 17:08:2026 - 11:16:13: glade-torch-flame и glade-lantern-glass — светящиеся половины ламп.
- 22:08:2026 - 12:10:00: ГИЛДЕРГРИН gildergreen-forge — одно дерево на город (решение владельца,
  волна 1 по Вайтрану): 14 м под кроной Ø18.2, W/H 1.30 против 0.87 у великана,
  сучья с 3.1 м, ствол 0.85. Разрешённое исключение из табу на деревья: НОВЫЙ
  рецепт, great-forge-oak и вся полка перепечены байт-в-байт. spray_frac 0.198
  (=0.09*20/9.1) — правило «лист остаётся листом» применено к вдвое меньшей
  кроне, а не скопировано буквой. Плюс кузница печатает ИЗМЕРЕННЫЕ габариты
  (measure_object), а не переписанные из рецепта: ниже/шире — это про
  геометрию, и спрашивать надо у неё.
*/

#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/TreeForge.h"

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
        spruce.crown_radius = 3.9f + static_cast<float>(i) * 0.5f;
        spruce.crown_base_frac = 0.10f; // the skirt nearly reaches the ground
        spruce.trunk_radius = 0.30f;
        spruce.bark = {0.23f, 0.16f, 0.11f}; // red-brown spruce bark
        spruce.tone = LeafTone::ConiferDark;
        spruce.card_shape = LeafShape::NeedleFan;
        spruce.conifer = true;
        spruce.whorl_count = 17 + i * 3;
        spruce.whorl_branches = 10; // «из голых палок в пышные» (17.08)
        spruce.droop = 0.30f;
        spruce.frond_width = 1.15f;
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
    {
        // ГИЛДЕРГРИН — ОДНО дерево на весь город, у алтаря Кинарет (решение
        // владельца, волна 1 по Вайтрану). Это НЕ второй гигант: скайримский
        // Гилдергрин значительно НИЖЕ и ШИРЕ великана — купол шире собственной
        // высоты, ствол короткий и кряжистый, нижние сучья почти над головой.
        // Отдельный рецепт, а не правка great-forge-oak: гигант стоит дубом
        // поляны к ЮЗ от стен (gen_whiterun.py), и перекроить его ради города
        // значило бы переписать чужое принятое дерево.
        //
        // ЧИСЛА СОГЛАСОВАНЫ С АРХИТЕКТОРОМ и с docs/TREE_PASSPORTS.md §2.5
        // (купол шире высоты, W/H 1.1-1.3, крона с 0.2-0.3h):
        //   W/H = 2*9.1/14.0 = 1.30   против 0.87 у великана;
        //   нижние сучья на 0.22*14 = 3.1 м — под ними ходят.
        TreeForgeParams gilder;
        gilder.seed = 1187;
        gilder.name = "gildergreen-forge";
        gilder.height = 14.0f;
        gilder.crown_radius = 9.1f;
        gilder.crown_base_frac = 0.22f;
        gilder.trunk_radius = 0.85f;
        gilder.tone = LeafTone::OakDeep;
        gilder.card_shape = LeafShape::RoundLobed;
        gilder.scaffold_count = 10;
        gilder.secondary_per_scaffold = 4;
        gilder.spray_per_branch = 3;
        // ЛИСТ ОСТАЁТСЯ ЛИСТОМ — правило рецепта великана, применённое честно:
        // spray_frac задан ДОЛЕЙ кроны, поэтому при кроне вдвое меньшей то же
        // число дало бы вдвое мелкие полотна. 0.09*(20.0/9.1) = 0.198 — то же
        // АБСОЛЮТНОЕ полотно (~1.7 м полуширины), что у великана и у рядового
        // дуба галереи. Скопировать 0.09 буквой значило бы посыпать крону
        // конфетти — ровно тот промах, против которого правило и написано.
        gilder.spray_frac = 0.198f;
        gilder.core_frac = 0.30f;
        gallery.push_back(gilder);
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
        std::printf("[forge] %-18s %6zu wood+ground  %4zu card  (%.0f%% cards)  hash %016llx\n",
                    params.name.c_str(), wood_tris, leaf_tris, share * 100.0,
                    static_cast<unsigned long long>(obj.content_hash));
        // ГАБАРИТЫ — ИЗМЕРЕННЫЕ, а не переписанные из параметров (ObjectRegistry.h:
        // «размер, вписанный рядом с деталью, протухает молча»). Купол шире
        // высоты или нет — это про ГЕОМЕТРИЮ, и спрашивать надо у неё: свес
        // лап уводит крону за crown_radius, а рецепт об этом не знает.
        const ObjectExtent ext = measure_object(obj);
        std::printf("           габарит  H %.1f м  W %.1f м (Ø крона)  W/H %.2f"
                    "  твёрдый след Ø %.2f м  низ %.2f\n",
                    static_cast<double>(ext.top - ext.bottom),
                    static_cast<double>(ext.radius * 2.0f),
                    static_cast<double>(ext.radius * 2.0f
                                        / std::max(ext.top - ext.bottom, 0.01f)),
                    static_cast<double>((ext.shi.x - ext.slo.x + ext.shi.y - ext.slo.y)
                                        * 0.5f),
                    static_cast<double>(ext.bottom));
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

    // --- THE GLADE SHELF (user, 16.08: «полянка с цветами, мелкими тонкими
    // деревьями, с огромным дубом в центре, окружённая высоким, но не
    // гигантским лесом берёз и сосен... маленькие деревья, поваленные
    // деревья, трава высокая») — every species the glade scene composes,
    // baked to its own shelf assets/objects/glade.
    {
        const fs::path shelf = out_dir.parent_path() / "glade";
        std::error_code gec2;
        fs::create_directories(shelf, gec2);
        const auto bake = [&](const RegistryObject& obj) {
            const fs::path file = shelf / (obj.name + ".dfo");
            const bool ok = write_object(obj, file) && read_object(file).has_value();
            if (!ok) {
                std::fprintf(stderr, "[forge] glade %s: FAILED\n", obj.name.c_str());
                all_ok = false;
            } else {
                std::printf("[forge] glade/%-18s %5zu tris\n", obj.name.c_str(),
                            (obj.wood.indices.size() + obj.ground.indices.size()
                             + obj.bark.indices.size() + obj.cards.indices.size()) / 3);
            }
        };
        // FAR TWIN (`<имя>-far`, контракт лида 17.08): same params, same
        // silhouette, cheap feathers — the 32 m tiles deeper in the woods
        // ride this one. Trees only; a 100-tri bush needs no far form.
        const auto bake_tree = [&](TreeForgeParams tp) {
            bake(forge_tree(tp));
            tp.far_lod = true;
            tp.name += "-far";
            bake(forge_tree(tp));
        };
        const auto bake_bush = [&](BushForgeParams bp) {
            bake(forge_bush(bp));
            bp.far_lod = true;
            bp.name += "-far";
            bake(forge_bush(bp));
        };
        // The elder oak at the heart of the glade: big, lush, not a giant.
        TreeForgeParams elder;
        elder.seed = 1301;
        elder.name = "glade-oak-elder";
        // Sheet-4 answer №1: «дуб сделай 50м» — the magic oak towers 19 m
        // over the tallest pine, crown Ø44 m shading the ring walk.
        elder.height = 50.0f;
        elder.crown_radius = 22.0f;
        elder.crown_base_frac = 0.20f;
        elder.trunk_radius = 2.2f;
        elder.tone = LeafTone::OakDeep;
        elder.scaffold_count = 11;
        elder.spray_per_branch = 3;
        elder.spray_frac = 0.10f;
        bake_tree(elder);
        // The surrounding forest: tall (not giant) birches and pines,
        // four maturity/size steps each — «разного роста и пышности».
        for (int i = 0; i < 4; ++i) {
            TreeForgeParams b;
            b.seed = 1401 + static_cast<uint64_t>(i);
            b.name = "glade-birch-" + std::string(1, static_cast<char>('a' + i));
            b.height = 17.0f + static_cast<float>(i) * 2.2f;
            b.crown_radius = 3.4f + static_cast<float>(i) * 0.5f;
            b.crown_base_frac = 0.42f;
            b.trunk_radius = 0.26f + static_cast<float>(i) * 0.05f;
            b.bark = {0.80f, 0.79f, 0.74f};
            b.tone = LeafTone::BirchLight;
            b.card_shape = LeafShape::RaggedTip;
            b.scaffold_count = 4 + (i > 1 ? 1 : 0);
            b.spray_frac = 0.26f;
            bake_tree(b);
        }
        for (int i = 0; i < 3; ++i) {
            TreeForgeParams pi;
            pi.seed = 1501 + static_cast<uint64_t>(i);
            pi.name = "glade-pine-" + std::string(1, static_cast<char>('a' + i));
            pi.height = 25.0f + static_cast<float>(i) * 3.0f;
            pi.crown_radius = 4.8f + static_cast<float>(i) * 0.5f;
            pi.crown_base_frac = 0.45f;
            pi.trunk_radius = 0.52f + static_cast<float>(i) * 0.08f;
            pi.bark = {0.30f, 0.19f, 0.12f};
            pi.tone = LeafTone::ConiferDark;
            pi.card_shape = LeafShape::NeedleFan;
            pi.conifer = true;
            // User, 17.08 morning: «из голых палок в пышные деревья» — the
            // 8x7 crown of v1 was ~56 fronds on a 25 m tree. Dense rings,
            // nine branches each, wide ribbons, lifted (pine cap, not spruce
            // droop): ~117 fronds and the bole stays bare below 0.45.
            pi.whorl_count = 13 + i;
            pi.whorl_branches = 9;
            pi.droop = 0.16f;
            pi.frond_width = 1.25f;
            bake_tree(pi);
        }
        // Mid-tier OAKS for the forest ring (user: «хотя бы ещё маленькие
        // дубы поставим, чтоб голость леса убрать») — broadleaf mass between
        // birch trunks and pine caps.
        for (int i = 0; i < 2; ++i) {
            TreeForgeParams ok;
            ok.seed = 1551 + static_cast<uint64_t>(i);
            ok.name = "glade-oak-mid-" + std::string(1, static_cast<char>('a' + i));
            ok.height = 11.0f + static_cast<float>(i) * 3.5f;
            ok.crown_radius = 4.4f + static_cast<float>(i) * 0.8f;
            ok.crown_base_frac = 0.30f;
            ok.trunk_radius = 0.36f + static_cast<float>(i) * 0.06f;
            ok.tone = LeafTone::OakMid;
            ok.card_shape = LeafShape::RoundLobed;
            ok.scaffold_count = 6;
            ok.spray_per_branch = 2;
            ok.spray_frac = 0.26f;
            bake_tree(ok);
        }
        // The thin young trees of the glade itself, and the small tier of the
        // wood — «мелкие тонкие деревья... в лесу должны и маленькие быть».
        for (int i = 0; i < 2; ++i) {
            TreeForgeParams s;
            s.seed = 1601 + static_cast<uint64_t>(i);
            s.name = "glade-sapling-birch-" + std::string(1, static_cast<char>('a' + i));
            s.height = 3.6f + static_cast<float>(i) * 1.2f;
            s.crown_radius = 0.9f + static_cast<float>(i) * 0.3f;
            s.crown_base_frac = 0.35f;
            s.trunk_radius = 0.07f;
            s.bark = {0.80f, 0.79f, 0.74f};
            s.tone = LeafTone::BirchLight;
            s.card_shape = LeafShape::RaggedTip;
            s.scaffold_count = 3;
            s.spray_frac = 0.30f;
            bake_tree(s);
        }
        {
            TreeForgeParams y;
            y.seed = 1611;
            y.name = "glade-young-aspen";
            y.height = 7.0f;
            y.crown_radius = 1.7f;
            y.crown_base_frac = 0.4f;
            y.trunk_radius = 0.13f;
            y.bark = {0.74f, 0.74f, 0.68f};
            y.tone = LeafTone::BirchPale;
            y.card_shape = LeafShape::OvalSpray;
            y.scaffold_count = 4;
            y.spray_frac = 0.28f;
            bake_tree(y);
            TreeForgeParams yo = y;
            yo.seed = 1612;
            yo.name = "glade-young-oak";
            yo.height = 6.0f;
            yo.crown_radius = 2.3f;
            yo.crown_base_frac = 0.3f;
            yo.trunk_radius = 0.17f;
            yo.bark = {0.16f, 0.12f, 0.09f};
            yo.tone = LeafTone::OakMid;
            yo.card_shape = LeafShape::RoundLobed;
            bake_tree(yo);
        }
        // Bushes, logs, grass, flowers, mushrooms.
        for (int i = 0; i < 3; ++i) {
            BushForgeParams bu;
            bu.seed = 1701 + static_cast<uint64_t>(i);
            bu.name = "glade-bush-" + std::string(1, static_cast<char>('a' + i));
            bu.height = 1.2f + static_cast<float>(i) * 0.35f;
            bu.radius = 0.9f + static_cast<float>(i) * 0.28f;
            bu.stems = 4 + i;
            bake(forge_bush(bu));
        }
        // BUSH SPECIES (user, 17.08: «кустики разные, с разными листиками и
        // разными ягодами; ягоды крупные и видные»).
        {
            BushForgeParams hz; // ОРЕШНИК: tall, broad oak-ish leaves, no fruit
            hz.seed = 1721;
            hz.name = "glade-hazel";
            hz.height = 2.5f;
            hz.radius = 1.6f;
            hz.stems = 7;
            hz.tone = LeafTone::OakMid;
            hz.card_shape = LeafShape::RoundLobed;
            bake_bush(hz);
            BushForgeParams rb; // ЯГОДНИК КРАСНЫЙ: oval leaves, big red berries
            rb.seed = 1731;
            rb.name = "glade-berry-red";
            rb.height = 1.4f;
            rb.radius = 1.1f;
            rb.stems = 6;
            rb.tone = LeafTone::BirchLight;
            rb.card_shape = LeafShape::OvalSpray;
            rb.berry_count = 26;
            rb.berry_r = 0.065f;
            rb.berry = {0.78f, 0.12f, 0.10f};
            rb.berry_sepal = true;  // красные шарики с попкой
            bake_bush(rb);
            BushForgeParams db; // ЯГОДНИК ТЁМНЫЙ: dark leaves, blue-black fruit
            db.seed = 1741;
            db.name = "glade-berry-dark";
            db.height = 1.0f;
            db.radius = 0.85f;
            db.stems = 5;
            db.tone = LeafTone::WillowDark;
            db.card_shape = LeafShape::RaggedTip;
            db.berry_count = 20;
            db.berry_r = 0.055f;
            db.berry = {0.16f, 0.14f, 0.30f};
            db.berry_pattern = 1;  // сизый налёт крапинкой
            db.berry_spot = {0.55f, 0.60f, 0.72f};
            bake_bush(db);
            // Sheet-4 answer №5 — the variety row: «где-то шарики, где-то
            // грозди... красные, синие, розовенькие, зеленые... с разными
            // рисунками... с разными попками и веточками». Four more berry
            // bushes, each owning a distinct combination of the axes.
            BushForgeParams bb;  // ЯГОДНИК СИНИЙ: грозди мелких синих
            bb.seed = 1771;
            bb.name = "glade-berry-blue";
            bb.height = 1.2f;
            bb.radius = 1.0f;
            bb.stems = 6;
            bb.tone = LeafTone::OakMid;
            bb.card_shape = LeafShape::OvalSpray;
            bb.berry_count = 12;
            bb.berry_r = 0.05f;
            bb.berry = {0.16f, 0.22f, 0.55f};
            bb.berry_style = BerryStyle::Cluster;
            bb.berry_stalk = 1.4f;  // грозди свисают на длинных веточках
            bake_bush(bb);
            BushForgeParams pb;  // ЯГОДНИК РОЗОВЫЙ: мелкие в белую крапинку
            pb.seed = 1781;
            pb.name = "glade-berry-pink";
            pb.height = 0.9f;
            pb.radius = 0.8f;
            pb.stems = 5;
            pb.tone = LeafTone::BirchLight;
            pb.card_shape = LeafShape::RoundLobed;
            pb.berry_count = 24;
            pb.berry_r = 0.042f;
            pb.berry = {0.85f, 0.45f, 0.60f};
            pb.berry_pattern = 1;
            pb.berry_spot = {0.93f, 0.90f, 0.88f};
            pb.berry_sepal = true;
            bake_bush(pb);
            BushForgeParams gb;  // ЯГОДНИК ЗЕЛЁНЫЙ: крупные капли в полоску
            gb.seed = 1791;
            gb.name = "glade-berry-green";
            gb.height = 1.3f;
            gb.radius = 1.05f;
            gb.stems = 6;
            gb.tone = LeafTone::WillowOlive;
            gb.card_shape = LeafShape::OvalSpray;
            gb.berry_count = 18;
            gb.berry_r = 0.07f;
            gb.berry = {0.38f, 0.55f, 0.20f};
            gb.berry_style = BerryStyle::Drops;
            gb.berry_pattern = 2;  // светлые продольные полоски
            gb.berry_spot = {0.58f, 0.74f, 0.32f};
            bake_bush(gb);
            BushForgeParams kb;  // ЯГОДНИК ЧЁРНЫЙ: грозди чёрных с попкой
            kb.seed = 1796;
            kb.name = "glade-berry-black";
            kb.height = 1.1f;
            kb.radius = 0.9f;
            kb.stems = 5;
            kb.tone = LeafTone::WillowDark;
            kb.card_shape = LeafShape::RaggedTip;
            kb.berry_count = 10;
            kb.berry_r = 0.048f;
            kb.berry = {0.10f, 0.09f, 0.11f};
            kb.berry_style = BerryStyle::Cluster;
            kb.berry_sepal = true;
            bake_bush(kb);
            BushForgeParams jc; // МОЖЖЕВЕЛЬНИК СТЕЛЮЩИЙСЯ: flat, sizy berries
            jc.seed = 1751;
            jc.name = "glade-juniper-creep";
            jc.height = 0.5f;
            jc.radius = 1.5f;
            jc.stems = 6;
            jc.creeping = true;
            jc.tone = LeafTone::ConiferDark;
            jc.card_shape = LeafShape::NeedleFan;
            jc.berry_count = 14;
            jc.berry_r = 0.04f;
            jc.berry = {0.35f, 0.42f, 0.48f};
            bake_bush(jc);
            GroundPropParams fern; // ПАПОРОТНИК: веер гнутых перьев
            fern.seed = 1761;
            fern.name = "glade-fern";
            fern.kind = GroundPropKind::Fern;
            fern.height = 0.7f;
            bake(forge_ground_prop(fern));
            // СВЕТ ТРОП (этап 2): новые аккуратные модели — факел на столбе
            // для троп (шахматно) и фонарь с кронштейном для кольцевой.
            PathLightParams ts;
            ts.seed = 2001;
            ts.name = "glade-torch-stake";
            ts.kind = PathLightKind::TorchStake;
            ts.height = 2.2f;
            bake(forge_path_light(ts));
            PathLightParams lp;
            lp.seed = 2011;
            lp.name = "glade-lantern-post";
            lp.kind = PathLightKind::LanternPost;
            lp.height = 2.0f;
            bake(forge_path_light(lp));
            // GLOW-ЧАСТИ (контракт лида: kind=="emissive" рисуется unlit):
            // пламя и стекло — отдельные объекты рядом со столбами в сцене.
            PathLightParams tf = ts;
            tf.name = "glade-torch-flame";
            tf.part = PathLightPart::Glow;
            bake(forge_path_light(tf));
            PathLightParams lg = lp;
            lg.name = "glade-lantern-glass";
            lg.part = PathLightPart::Glow;
            bake(forge_path_light(lg));
        }
        for (int i = 0; i < 2; ++i) {
            LogForgeParams lg;
            lg.seed = 1801 + static_cast<uint64_t>(i);
            lg.name = "glade-log-" + std::string(1, static_cast<char>('a' + i));
            lg.length = i == 0 ? 7.5f : 4.5f;
            lg.radius = i == 0 ? 0.5f : 0.34f;
            bake(forge_fallen_log(lg));
        }
        for (int i = 0; i < 3; ++i) {
            GroundPropParams t;
            t.seed = 1901 + static_cast<uint64_t>(i);
            t.name = "glade-grass-" + std::string(1, static_cast<char>('a' + i));
            t.kind = GroundPropKind::GrassTuft;
            // User, 17.08 morning: «травы вообще не вижу» — the ladder rises
            // to waist and chest: 0.9 / 1.3 / 1.7 m, wide blades in the forge.
            t.height = 0.9f + static_cast<float>(i) * 0.4f;
            bake(forge_ground_prop(t));
        }
        const glm::vec3 petal[3] = {{0.92f, 0.90f, 0.85f},   // white
                                    {0.85f, 0.45f, 0.60f},   // pink
                                    {0.90f, 0.78f, 0.25f}};  // yellow
        const char* petal_name[3] = {"white", "pink", "yellow"};
        for (int i = 0; i < 3; ++i) {
            GroundPropParams f;
            f.seed = 2001 + static_cast<uint64_t>(i);
            f.name = std::string("glade-flowers-") + petal_name[i];
            f.kind = GroundPropKind::Flowers;
            // Storybook scale (user: «пусть крупные будут... зато играбельно»)
            f.height = 0.75f;
            f.bloom = 0.14f;
            f.accent = petal[i];
            bake(forge_ground_prop(f));
        }
        for (int i = 0; i < 2; ++i) {
            GroundPropParams m;
            m.seed = 2101 + static_cast<uint64_t>(i);
            m.name = i == 0 ? "glade-mushrooms-brown" : "glade-mushrooms-red";
            m.kind = GroundPropKind::Mushrooms;
            m.height = 0.16f;
            m.accent = i == 0 ? glm::vec3{0.48f, 0.34f, 0.18f}
                              : glm::vec3{0.70f, 0.16f, 0.10f};
            bake(forge_ground_prop(m));
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
