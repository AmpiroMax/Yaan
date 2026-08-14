/*
Created: 14:08:2026 - 23:36:19
Last updated: 15:08:2026 - 00:24:00
Module: tools
File: tools/forge_trees.cpp

Responsibility:
- The forge CLI (dfn_forge): builds the gallery of registry trees offline and
  writes them to assets/objects/trees/*.dfo, plus the human-readable index
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
        birch.spray_frac = 0.24f; // birch: fewer, slightly larger airy sprays
        gallery.push_back(birch);
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
        giant.spray_per_branch = 2;
        giant.spray_frac = 0.18f; // absolute sprays ~2.7 m: giant leaves do not scale
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
