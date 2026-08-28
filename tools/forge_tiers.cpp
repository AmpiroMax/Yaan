/*
Created: 28:08:2026 - 22:50:00
Last updated: 28:08:2026 - 22:50:00
Module: tools
File: tools/forge_tiers.cpp

Responsibility:
- Кузница ЯРУСОВ ФЛОРЫ (dfn_forge_tiers): печёт в assets/objects/tiers то, чего
  композиционным сценам не хватает для ярусов записки №2 —
    * ПОДРОСТ: молодые деревца 1-3 м (ярус 3, единственный отсутствующий);
    * ПЛАТЫ КОВРА: моховой и травяной ковёр кусками 3x3 м, с камнями В покрове.

Usage:
    dfn_forge_tiers [<out_dir>]   (по умолчанию assets/objects/tiers; из корня)

ПОЧЕМУ ТРЕТИЙ БИНАРНИК, А НЕ РАЗДЕЛ В ДВУХ ПРЕЖНИХ. Ровно по доводу
forge_trees_v2.cpp: dfn_forge переписывает все 82 своих файла на каждом
прогоне, а dfn_forge2 сторожит своё имя («-v2-» в каждом рецепте). Этот пишет
СВОЮ полку и ни одного чужого имени — «не порти чужого» становится свойством
того, куда программа вообще может дотянуться.

ЧТО ЗДЕСЬ НЕ ИЗОБРЕТАЕТСЯ. Ни одной новой геометрии:
  * подрост — forge_tree_v2() с молодыми числами (крона до земли, ветви живые
    до низа, ни одного сухого сучка: сухой сук — примета возраста, §2.1);
  * мох, цветочный ковёр, камни — build_flora_mesh() с ВИДАМИ, которые в
    движке уже есть (MossPatch, FlowerCarpet, PebbleCluster): «мха нет» из
    записки — про ПОЛКУ композиционных сцен, а не про кузницу;
  * короткий пучок травы — forge_ground_prop(GrassTuft) с малой высотой.
Плата ковра — это СБОРКА этих кусков в один объект, и она нужна ровно потому,
что ковёр обязан быть сплошным: сеять его поштучно значило бы класть в .scene
десятки тысяч размещений там, где хватает двух тысяч плат.

Dependencies:
- Uses: engine/render (TreeForgeV2, TreeForge, ProcFlora, ObjectRegistry).
- Used by: рука; полка assets/objects/tiers; tools/gen_trees_v2.py.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ДЕТЕРМИНИРОВАН: тот же прогон — те же файлы и те же content_hash.
- НИ ОДНОГО ИМЕНИ ЧУЖОЙ ПОЛКИ. Страж внизу main() отказывает, если имя не
  начинается с "tier-".
*/
/*
UPD:
- 28:08:2026 - 22:50:00: Создан — первая волна ярусов флоры (подрост + ковёр).
*/

#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/ProcFlora.h"
#include "engine/render/sources/ProcMesh.h"
#include "engine/render/sources/TreeForge.h"
#include "engine/render/sources/TreeForgeV2.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using namespace dfn::render;

constexpr float TAU = 6.28318530718f;

/// Тот же смеситель, что в core (clump_detail::mix64) — здесь он раздаёт
/// раскладку внутри платы, и это не закон посева, а разброс внутри объекта.
uint64_t mix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(mix64(seed + 0x1234567u)) {}
    float unit() {
        s = mix64(s);
        return static_cast<float>(s >> 40) / 16777216.0f;
    }
    float range(float a, float b) { return a + (b - a) * unit(); }
    int pick(int n) { return static_cast<int>(unit() * static_cast<float>(n)) % n; }
};

/// Кладёт одно растение в плату, разложив его три потока.
void put(RegistryObject& into, const FloraMesh& m, glm::vec3 at, float yaw,
         float scale) {
    append_transformed(into.wood, m.wood, at, yaw, scale);
    append_transformed(into.cards, m.cards, at, yaw, scale);
    append_transformed(into.ground, m.ground, at, yaw, scale);
}

void put(RegistryObject& into, const RegistryObject& src, glm::vec3 at, float yaw,
         float scale) {
    append_transformed(into.wood, src.wood, at, yaw, scale);
    append_transformed(into.cards, src.cards, at, yaw, scale);
    append_transformed(into.ground, src.ground, at, yaw, scale);
    append_transformed(into.bark, src.bark, at, yaw, scale);
}

/// ПЛАТА КОВРА. Куски раскладываются НЕ равномерно по квадрату: у платы два
/// собственных сгустка, поэтому стык двух соседних плат не читается решёткой —
/// та же болезнь, от которой лечится и посев наверху, только внутри объекта.
///
/// half — половина стороны платы, м. reach — на сколько куски свешиваются за
/// край: без свеса шов между платами был бы полосой голой земли.
RegistryObject carpet_patch(const std::string& name, uint64_t seed, float half,
                            float reach, bool mossy, int count,
                            float pebble_share) {
    RegistryObject obj;
    obj.name = name;
    obj.kind = "prop";
    obj.source = std::string("forge:tiers carpet ") + (mossy ? "moss" : "sward")
                 + " seed=" + std::to_string(seed);
    Rng rng(seed);
    const float lim = half + reach;
    // Два сгустка платы.
    const float hx[2] = {rng.range(-half * 0.6f, half * 0.6f),
                         rng.range(-half * 0.6f, half * 0.6f)};
    const float hz[2] = {rng.range(-half * 0.6f, half * 0.6f),
                         rng.range(-half * 0.6f, half * 0.6f)};
    for (int i = 0; i < count; ++i) {
        const int lobe = rng.pick(2);
        float x = hx[lobe] + rng.range(-lim, lim) * 0.72f;
        float z = hz[lobe] + rng.range(-lim, lim) * 0.72f;
        x = std::clamp(x, -lim, lim);
        z = std::clamp(z, -lim, lim);
        const float yaw = rng.unit() * TAU;
        if (rng.unit() < pebble_share) {
            // КАМЕНЬ ЛЕЖИТ В ПОКРОВЕ, ТОГО ЖЕ РОСТА, ЧТО ПУЧКИ (§1.4) —
            // поэтому он часть платы, а не отдельный рассев поверх неё.
            FloraShape sh;
            sh.maturity = rng.range(0.30f, 0.55f);
            const FloraMesh peb = build_flora_mesh(FloraSpecies::PebbleCluster,
                                                   static_cast<uint32_t>(rng.pick(12)),
                                                   sh, FloraLod::Full);
            put(obj, peb, {x, 0.0f, z}, yaw, rng.range(0.35f, 0.62f));
            continue;
        }
        if (mossy) {
            FloraShape sh;
            sh.maturity = rng.range(0.55f, 1.05f);
            const FloraMesh moss = build_flora_mesh(FloraSpecies::MossPatch,
                                                    static_cast<uint32_t>(rng.pick(12)),
                                                    sh, FloraLod::Full);
            put(obj, moss, {x, 0.0f, z}, yaw, rng.range(0.5f, 0.85f));
        } else {
            // Травяной ковёр — КОРОТКИЙ пучок (0.16..0.30 м) плюс изредка
            // цветочный ковёр. Высокие злаки сюда не попадают: они акцент, и
            // у акцента своя плотность и свой порог света.
            if (rng.unit() < 0.22f) {
                FloraShape sh;
                sh.maturity = rng.range(0.5f, 0.9f);
                const FloraMesh fl = build_flora_mesh(FloraSpecies::FlowerCarpet,
                                                      static_cast<uint32_t>(rng.pick(12)),
                                                      sh, FloraLod::Full);
                put(obj, fl, {x, 0.0f, z}, yaw, rng.range(0.5f, 0.8f));
            } else {
                GroundPropParams gp;
                gp.seed = seed * 131u + static_cast<uint64_t>(i);
                gp.name = "blade";
                gp.kind = GroundPropKind::GrassTuft;
                gp.height = rng.range(0.16f, 0.30f);
                const RegistryObject tuft = forge_ground_prop(gp);
                put(obj, tuft, {x, 0.0f, z}, yaw, 1.0f);
            }
        }
    }
    obj.content_hash = object_content_hash(obj);
    return obj;
}

/// ПОДРОСТ. Один рецепт, одна ручка: возраст. Молодое дерево — это НЕ взрослое
/// уменьшенное (масштаб оставил бы ствол толщиной с взрослый и сухие сучья на
/// двухметровом деревце). Это тот же строитель с молодыми числами:
///   * крона занимает почти весь рост (0.86 против 0.63-0.71 у взрослого) —
///     «конус до самой земли, ветви живые до низа» (§2.1);
///   * бола тонкая: 0.030-0.045 радиуса при 1.4-3.0 м роста;
///   * ни одного сухого сучка: сук — примета ВОЗРАСТА;
///   * долей мало (4-5): у молодого дерева ещё нет массы, которую надо рвать.
RegistryObject sapling(const std::string& name, uint64_t seed, float height,
                       LeafShape shape, glm::vec3 bark, float lean, float lean_dir,
                       bool far_form = false) {
    TreeV2Params p;
    p.seed = seed;
    p.name = name;
    p.habit = TreeHabit::Solitary;
    p.height = height;
    p.crown_depth_frac = 0.86f;
    p.crown_width_frac = 0.62f;   // молодое узкое: света ему хватает сверху
    p.trunk_radius = 0.030f + 0.005f * height;
    p.bark = bark;
    p.card_shape = shape;
    p.lobes = 4 + (static_cast<int>(seed) % 2);
    p.lean_rad = lean;
    p.lean_dir = lean_dir;
    p.curve_frac = 0.09f;         // молодой ствол гибче взрослого
    p.snags = 0;
    p.far_lod = far_form;
    return forge_tree_v2(p);
}

} // namespace

int main(int argc, char** argv) {
    namespace fs = std::filesystem;

    const fs::path out_dir = argc > 1 ? fs::path(argv[1]) : fs::path("assets/objects/tiers");
    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        std::fprintf(stderr, "[tiers] cannot create %s: %s\n", out_dir.string().c_str(),
                     ec.message().c_str());
        return 1;
    }

    std::vector<RegistryObject> shelf;

    // --- ЯРУС 3: ПОДРОСТ. Шесть деревец 1.4..3.0 м, три силуэта листа.
    {
        const struct { const char* suffix; float h; LeafShape shape; glm::vec3 bark; }
        rows[] = {
            // ЧИСЛО В СТРОКЕ — ЗАКАЗ СТРОИТЕЛЮ, А ЗАМЕР — В INDEX.md, и они не
            // совпадают: крона строится ВЫШЕ заказанной высоты боли. Полоса
            // записки (1-3 м) держится по ЗАМЕРУ, поэтому заказы подобраны под
            // него, а не переписаны из полосы.
            {"oak-a",   0.62f, LeafShape::RoundLobed, {0.17f, 0.13f, 0.10f}},
            {"oak-b",   1.90f, LeafShape::RoundLobed, {0.18f, 0.14f, 0.10f}},
            {"beech-a", 1.30f, LeafShape::OvalSpray,  {0.30f, 0.29f, 0.26f}},
            {"beech-b", 2.05f, LeafShape::OvalSpray,  {0.31f, 0.30f, 0.27f}},
            {"alder-a", 1.05f, LeafShape::RaggedTip,  {0.21f, 0.17f, 0.12f}},
            {"alder-b", 2.05f, LeafShape::RaggedTip,  {0.22f, 0.18f, 0.13f}},
        };
        uint64_t s = 3101;
        for (const auto& r : rows) {
            const std::string nm = std::string("tier-sapling-") + r.suffix;
            const float ln = 0.10f + 0.03f * static_cast<float>(s % 5);
            const float ld = static_cast<float>(s % 7) * 0.9f;
            shelf.push_back(sapling(nm, s, r.h, r.shape, r.bark, ln, ld, false));
            // ДАЛЬНЯЯ ФОРМА — ТОТ ЖЕ РЕЦЕПТ, дешевле (far_lod строителя v2), а
            // не другое деревце: приложение подменяет `<имя>-far` по расстоянию.
            shelf.push_back(sapling(nm + "-far", s, r.h, r.shape, r.bark, ln, ld,
                                    true));
            ++s;
        }
    }

    // --- ЯРУС 5а: КОВЁР. Четыре платы мха и четыре травяных, 3x3 м со свесом
    // 0.35 м. Четыре, а не одна: плата, повторённая тысячу раз с одним и тем
    // же расположением кусков, — это штамп, и его видно ровно тем же глазом,
    // каким видно равномерный рассев.
    {
        const float HALF = 1.05f;
        const float REACH = 0.15f;
        // ДАЛЬНЯЯ ФОРМА ПЛАТЫ — ТА ЖЕ ПЛАТА С ОБРЕЗАННЫМ ХВОСТОМ. Семя то же,
        // порядок кусков тот же, поэтому первые 45% кусков стоят ТАМ ЖЕ: при
        // подмене плата редеет, а не переезжает. Это и есть лестница дальних
        // форм, которой ковёр обязан больше всех: он лежит на всей плитке.
        const int MOSS_N = 16;
        const int SWARD_N = 13;
        for (int i = 0; i < 4; ++i) {
            const char c = static_cast<char>('a' + i);
            const uint64_t sm = 4201 + static_cast<uint64_t>(i);
            const uint64_t ss = 4301 + static_cast<uint64_t>(i);
            shelf.push_back(carpet_patch(std::string("tier-moss-") + c, sm,
                                         HALF, REACH, true, MOSS_N, 0.12f));
            shelf.push_back(carpet_patch(std::string("tier-moss-") + c + "-far", sm,
                                         HALF, REACH, true, MOSS_N * 45 / 100, 0.12f));
            shelf.push_back(carpet_patch(std::string("tier-sward-") + c, ss,
                                         HALF, REACH, false, SWARD_N, 0.10f));
            shelf.push_back(carpet_patch(std::string("tier-sward-") + c + "-far", ss,
                                         HALF, REACH, false, SWARD_N * 45 / 100, 0.10f));
        }
    }

    // --- СТРАЖ ИМЕНИ. Ни одного чужого.
    for (const RegistryObject& o : shelf) {
        if (o.name.rfind("tier-", 0) != 0) {
            std::fprintf(stderr, "[tiers] \"%s\" — имя не этой полки, ОТКАЗ\n",
                         o.name.c_str());
            return 1;
        }
    }

    std::string index =
        "# ЯРУСЫ ФЛОРЫ — полка dfn_forge_tiers\n"
        "# Печётся `dfn_forge_tiers assets/objects/tiers`; правится в\n"
        "# tools/forge_tiers.cpp. Подрост — ярус 3 записки №2; платы ковра —\n"
        "# ярус 5а. Ни одной новой геометрии: рецепты движка с другими числами.\n"
        "#\n"
        "# имя | файл | вид | content_hash | трис | высота, м\n";
    int written = 0;
    for (const RegistryObject& o : shelf) {
        const fs::path path = out_dir / (o.name + ".dfo");
        if (!write_object(o, path)) {
            std::fprintf(stderr, "[tiers] не записан %s\n", path.string().c_str());
            return 1;
        }
        const ObjectExtent e = measure_object(o);
        const std::size_t tris = o.wood.triangle_count() + o.cards.triangle_count()
                               + o.ground.triangle_count() + o.bark.triangle_count();
        char row[512];
        std::snprintf(row, sizeof(row), "%s | %s.dfo | %s | %016llx | %zu | %.2f |\n",
                      o.name.c_str(), o.name.c_str(), o.kind.c_str(),
                      static_cast<unsigned long long>(o.content_hash), tris,
                      static_cast<double>(e.top - e.bottom));
        index += row;
        ++written;
    }
    const fs::path index_path = out_dir / "INDEX.md";
    FILE* f = std::fopen(index_path.string().c_str(), "wb");
    if (f == nullptr) {
        std::fprintf(stderr, "[tiers] не записан %s\n", index_path.string().c_str());
        return 1;
    }
    std::fwrite(index.data(), 1, index.size(), f);
    std::fclose(f);

    std::printf("[tiers] %d объектов в %s\n", written, out_dir.string().c_str());
    return 0;
}
