/*
Created: 14:08:2026 - 23:36:19
Last updated: 17:08:2026 - 07:04:26
Module: engine/render
File: engine/render/sources/TreeForge.cpp

Responsibility:
- forge_tree(): the industry-shaped tree (see TreeForge.h for the architecture
  and its research citations), built as three registry streams.

Dependencies:
- Uses: TreeForge.h, FloraBuild.h (tube_segment, blob_cluster, Rng, helpers),
  FloraCards.h (emit_leaf_card, tones), ProcMesh.h (pack).
- Used by: dfn_render target, tools/forge_trees.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic: every draw comes from the params' seed through one Rng.
- Proportion constants here cite the research doc inline. They are the forge's
  own recipe, not NUMBERS rows yet: the user judges the gallery first, and
  numbers that survive his eye get promoted with their derivations (the same
  road every accepted look number has walked).
*/
/*
UPD:
- 14:08:2026 - 23:36:19: Created with TreeForge.h.
- 15:08:2026 - 00:24:00: v2 — скелет виден сквозь крону: каркасные ветви + вторичные ёлочкой,
  лапы-якоря на кончиках и серединах ветвей, КРЕСТ-накрест пары карточек на якорь
  (стандарт SpeedTree: одинокая плоскость исчезает в собственный профиль и мерцает
  при облёте), ядро — малая тёмная глубина за просветами, а не крона. Плотность
  поднята после первого же кадра v2: редкие помпоны — не референс.
- 15:08:2026 - 00:45:20: v3 — РЕКУРСИВНЫЙ гровер по пунктам пользователя: «ветки должны идти от
  ствола И от других веток» — дети покидают болу и родительские ветви с середины
  сегментов; «уменьшаться пропорционально» — трубная модель, ребёнок несёт ~половину
  радиуса родителя; «расти небольшими сегментами, каждый чуть в сторону, а может
  вниз» — каждый сегмент поворачивает: боковое блуждание, слабеющая с уровнем тяга
  вверх, каждый четвёртый провисает. Шар-ядро удалён. Хвойный путь: бола во всю
  высоту, мутовки по конусу, лапы ВДОЛЬ ветви (ель — не палка с помпоном), нормали
  лап следуют ветви сильнее купола.
- 15:08:2026 - 01:04:30: КОРА И МОХ пост-проходом по вершинам wood/ground (референсы-крупняки:
  вертикальные борозды чередованием гребень/борозда с блужданием по высоте, мох
  зелёным налётом до ~4 м и на корнях, пятнами по азимуту). ПОРЯДОК ЛИСТВЫ по
  §4.3 исследования: лапы ложатся почти горизонтальными ярусами — нормаль
  кланяется ВВЕРХ (лиственные 0.45 up, хвоя 0.8 up), roll ±0.25 вместо ±0.9
  («у нас снова беспорядок» снят направлением, не плотностью).
- 15:08:2026 - 02:14:30: СТВОЛЫ И СУЧЬЯ В ТЕКСТУРЕ (вердикт: «текстур на деревьях нет... мох —
  просто зеленушка»): bark_tube кладёт UV зеркальным повтором внутри тайла коры,
  комель и корни носят МШИСТЫЙ колорвей, бола и ветви — чистый; ветер занулён,
  просвечивание глушится весом качания в шейдере (дерево не светится насквозь).
  Дубовый габитус (низкие ветви горизонтальны всю длину), центр кроны заполнен
  внутренними якорями, изнанка кроны снизу смотрит вниз, roll свободный (параллельные
  линии убиты вращением В плоскости), лапы прибиты к веткам (джиттер 0.035),
  корни — дуга четырьмя хордами (колено невидимо в плоском тонировании).
- 15:08:2026 - 15:54:46: v6: хвоя пересобрана на ФРОНДЫ-ЛЕНТЫ (провисающая лента 4 сегментов, кончик вверх, голое дерево только внутренняя четверть); ЮБКА мёртвых сучьев 0.10-0.40h (оба фотоскана); корни — КОНТРФОРСЫ (старт внутри наплыва, выше и толще, медленный сбег); ветер честный: bark_tube несёт вес на обоих кольцах (иначе стык рвётся), вес = дистанция от опоры (лид 09f75eb: транзмит теперь по колонке UV).
- 15:08:2026 - 16:17:07: emit_frond умножает ширину на p.frond_width (лиственница).
- 16:08:2026 - 20:23:55: Развёртка трубы: зеркальная волна -> прямой повтор (тайл коры теперь торо-периодический).
- 16:08:2026 - 20:44:08: Якорь листвы на конце КАЖДОЙ ветви, включая скелетные (уровень 0): их концы оставались голыми крюками — видно на первом верном наземном кадре.
- 16:08:2026 - 20:52:47: По трём замечаниям галереи 20:42-44: (1) конусы вдвое короче (бола 7->12 сегментов, ветви 8/6/4) и обхват НА КАЖДОЕ КОЛЬЦО — кора перестала тянуться к тонкому концу и рваться на стыках; (2) листва ВИСИТ: верхняя кромка карты на веточке, плоскость почти вертикальна, азимут наружу кроны, ролл малый — «кучка мух» снята корреляцией ориентаций; (3) крест-пары 90°->~65° — рёберный пунктир реже; полный фейд по углу взгляда запрошен у лида (fs_foliage).
- 16:08:2026 - 21:45:26: По восьми замечаниям 21:05-21:20: ГНУТЫЕ ЛИСТЫ (дизайн пользователя, п.4) — 3×3-патч куполом с провисшими углами и помятостью, драпирован ПО ветке (п.1: сидит на ветке, не ниже), без плоской оси (п.2), один лист вместо пары карт (п.3: меньше и крупнее, якоря прорежены); рамка коры ПАРАЛЛЕЛЬНО ПЕРЕНОСИТСЯ вдоль ветви (п.6: борозды идут прямо по росту, не крутятся на стыках); веточки 2-го уровня в текстуре коры (п.7: чёрные концы на белой берёзе умерли); обломанные СУЧЬЯ на боле и ЖЁЛУДИ у дубов (п.5; дупло в очереди — нужен свой тёмный тайл).
- 16:08:2026 - 22:06:42: По пачке 21:50-57: (а) «узкие линии» оказались ВЕТОЧКАМИ-ВОЛОСКАМИ тоньше пикселя — пол радиуса 0.025->0.05 и последний уровень короче (0.7) — провода за листвой умерли; (б) кора по ветвям: v-координата = ДЛИНА ДУГИ вдоль ветви, не мировая высота (на горизонтальной ветви борозды мазались вбок — «в разные стороны растёт»); (в) чёрные палки берёзы — те же волоски. Прозрачность при движении и голые ели — полоса фейда лида 0.08/0.22, сужение запрошено (0.03/0.08 + ручки).
- 16:08:2026 - 22:40:39: ФРОНД СЛОЖЕН ДОМИКОМ (V-сечение, края ниже хребта, нормали половинок врозь): плоская горизонтальная лента с уровня глаз — линия в пиксель, ВЕСЬ хвойный ряд стоял голым (геометрия была в .dfo — найдено дампом + логом расстановки; три раза принимал снаги рассыпки за свои ели). Ленты шире (0.42 reach), листов на якорь больше при spray_per_branch>2 (колосс «листвы мало»), жёлуди крупнее и ниже листа.
- 16:08:2026 - 22:48:45: Реализация forge_bush/forge_fallen_log/forge_ground_prop (этап полянки).
- 17:08:2026 - 01:26:10: Пучку травы — крошечный корневой пенёк в потоке древесины: объект из одних карт расстановщик сцены не мог поставить (стояло 111 из 195).
- 17:08:2026 - 02:31:45: Ягоды на кончиках стволиков (двухконусные, 5.5-6.5 см, у листовой кромки); стелющийся хабитус; папоротник V-перьями; ЖЁЛУДИ-ЯБЛОКИ 11 см двухцветные с черешком («дуб волшебный»).
- 17:08:2026 - 02:51:54: Ягоды v2: каждый плод ВИСИТ на веточке (не парит), стили шарик/гроздь-рябина/капля, крапинка бусинками, полоски рёбрами, чашелистик-попка снизу.
- 17:08:2026 - 03:51:22: forge_path_light: факел — башмак/столб/железный обруч/бочонок обмотки/двухконусное пламя с горячим ядром; фонарь — столб с кронштейном и раскосом, подвесной короб: плиты, ТЁПЛОЕ стекло, клетка из четырёх стоек, колпак. Всё в wood-стриме: мебель не качается и не просвечивает.
- 17:08:2026 - 07:04:26: Читаемость мелкой флоры (утро 17.08): лезвия травы 3.4-5.2 см и 13-18 на пучок, венчики цветов от p.bloom (лепесток 14 см, сердцевина конусом), стебли толще.
*/

#include "engine/render/sources/TreeForge.h"

#include "engine/render/sources/FloraBuild.h"
#include "engine/render/sources/ProcMesh.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace dfn::render {
namespace {

/// Vertex colour for textured wood on the foliage program: r = sway weight
/// (honest distance-from-support — the lead's 09f75eb shader derives all
/// three wind bands from this one weight), g = per-tree phase, b (value
/// jitter) = 0.5 neutral, a (sky vis) = 0.55.
[[nodiscard]] uint32_t pack_wind(float sway, float phase) {
    const auto to_byte = [](float v) {
        return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    // 0xAABBGGRR: a=0x8C (sky vis 0.55), b=0x80 (jitter 0.5).
    return 0x8C800000u | (to_byte(phase) << 8) | to_byte(sway);
}

/// One tapered tube segment with BARK UVs. Same geometry as tube_segment, but
/// each face maps into the given atlas tile rect: u runs around the
/// circumference, v along the segment's own length, both through a TRIANGLE
/// WAVE so the mapping mirror-repeats inside the tile and never crosses its
/// border into a neighbouring leaf tile (the atlas cannot wrap). The tile is
/// drawn mirror-symmetric, so the fold line is invisible by construction.
/// wind_c0 colours the p0 ring, wind_c1 the p1 ring (and the tip vertex):
/// the sway weight must be CONTINUOUS along a limb, or adjacent segments
/// translate by different amounts under wind and the joint cracks open.
/// u_hint PARALLEL-TRANSPORTS the texture frame along a limb (user, gallery
/// 21:18: «текстуры не прямые, везде по-разному идут; дерево растёт в одном
/// направлении, кора соответственно»): pass the previous segment's frame and
/// the furrows run straight down the limb instead of twisting at every joint
/// (perp_of() alone picks an arbitrary frame per segment).
void bark_tube(MeshData& m, glm::vec3 p0, glm::vec3 p1, glm::vec3 axis, float r0,
               float r1, int sides, glm::vec4 uv_rect, float v0_m, float circum_m,
               uint32_t wind_c0, uint32_t wind_c1, glm::vec3* u_hint = nullptr) {
    glm::vec3 u_axis;
    if (u_hint != nullptr && glm::length(*u_hint) > 1e-4f) {
        const glm::vec3 proj = *u_hint - axis * glm::dot(*u_hint, axis);
        u_axis = safe_normalize(proj, perp_of(axis));
        *u_hint = u_axis; // hand the transported frame back to the caller
    } else {
        u_axis = perp_of(axis);
    }
    const glm::vec3 v_axis = glm::cross(axis, u_axis);
    const float len = glm::length(p1 - p0);
    // PLAIN WRAP, not mirror: the bark tile is torus-periodic since the
    // FloraCards v2 field (mirror-repeat made every ridge a kaleidoscope
    // pair, which is what the user's «прямоугольнички» frame was showing).
    const auto tri_wave = [](float t) { return t - std::floor(t); };
    // Metres of trunk surface one full tile covers. ~2.6 m keeps the furrow
    // pitch believable on a 0.4 m oak and a 10 m colossus alike.
    constexpr float TILE_SPAN_M = 2.6f;
    const float du = uv_rect.z - uv_rect.x;
    const float dv = uv_rect.w - uv_rect.y;
    // EACH RING WEARS ITS OWN CIRCUMFERENCE (user, gallery 20:42: «кора везде
    // разная, где-то вытянута, где-то сжата»): a taper mapped with the butt
    // ring's girth stretches the tile toward the thin end, and the stretch
    // jumps at every joint. circum_m is the BUTT ring's girth; the tip ring
    // scales it by r1/r0, which is continuous across joints by construction.
    const float circum1_m = circum_m * ((r0 > 1e-5f) ? (r1 / r0) : 1.0f);
    for (int i = 0; i < sides; ++i) {
        const float a0 = TAU * static_cast<float>(i) / static_cast<float>(sides);
        const float a1 = TAU * static_cast<float>(i + 1) / static_cast<float>(sides);
        const glm::vec3 d0 = u_axis * std::cos(a0) + v_axis * std::sin(a0);
        const glm::vec3 d1 = u_axis * std::cos(a1) + v_axis * std::sin(a1);
        const float dr = r0 - r1;
        const float slope = (len > 1e-5f) ? (dr / len) : 0.0f;
        const glm::vec3 n0 = safe_normalize(d0 + axis * slope, d0);
        const glm::vec3 n1 = safe_normalize(d1 + axis * slope, d1);
        // Texture coordinates: circumference and height in METRES, folded.
        const float s0 = static_cast<float>(i) / static_cast<float>(sides);
        const float s1 = static_cast<float>(i + 1) / static_cast<float>(sides);
        const float cu0 = uv_rect.x + du * tri_wave(circum_m * s0 / TILE_SPAN_M);
        const float cu1 = uv_rect.x + du * tri_wave(circum_m * s1 / TILE_SPAN_M);
        const float cu0b = uv_rect.x + du * tri_wave(circum1_m * s0 / TILE_SPAN_M);
        const float cu1b = uv_rect.x + du * tri_wave(circum1_m * s1 / TILE_SPAN_M);
        const float cv0 = uv_rect.y + dv * tri_wave(v0_m / TILE_SPAN_M);
        const float cv1 = uv_rect.y + dv * tri_wave((v0_m + len) / TILE_SPAN_M);
        const auto base = static_cast<uint32_t>(m.vertices.size());
        if (r1 <= 1e-4f) {
            const glm::vec3 nt = safe_normalize(n0 + n1, axis);
            m.vertices.push_back({p0 + d0 * r0, n0, {cu0, cv0}, wind_c0});
            m.vertices.push_back({p1, nt, {(cu0b + cu1b) * 0.5f, cv1}, wind_c1});
            m.vertices.push_back({p0 + d1 * r0, n1, {cu1, cv0}, wind_c0});
            m.indices.insert(m.indices.end(), {base, base + 1, base + 2});
        } else {
            m.vertices.push_back({p0 + d0 * r0, n0, {cu0, cv0}, wind_c0});
            m.vertices.push_back({p1 + d0 * r1, n0, {cu0b, cv1}, wind_c1});
            m.vertices.push_back({p1 + d1 * r1, n1, {cu1b, cv1}, wind_c1});
            m.vertices.push_back({p0 + d1 * r0, n1, {cu1, cv0}, wind_c0});
            m.indices.insert(m.indices.end(),
                             {base, base + 1, base + 2, base, base + 2, base + 3});
        }
    }
}

} // namespace

RegistryObject forge_tree(const TreeForgeParams& p) {
    RegistryObject obj;
    obj.name = p.name;
    obj.kind = "tree";
    obj.source = "forge:v1 seed=" + std::to_string(p.seed);

    Rng rng(p.seed * 0x9E3779B97F4A7C15ull + 0x243F6A8885A308D3ull);
    const uint32_t bark = pack(p.bark);

    // BARK TILES (the textured-trunk wave): the clean colourway for the upper
    // bole and limbs, the MOSSY one for the flare and roots — moss lives where
    // the ground's damp does, which is the reference's own vertical story.
    // Wind: since the lead's 09f75eb the transmit gate is the atlas COLUMN
    // (u >= 0.8 is wood), so wood may carry an honest sway weight without
    // glowing — r is only sway now, all three wind bands derive from it.
    const LeafTone bark_row = p.bark.r > 0.6f ? LeafTone::BirchLight
                             : (p.conifer ? LeafTone::ConiferDark : LeafTone::OakMid);
    const LeafTone bark_moss_row = p.bark.r > 0.6f ? LeafTone::BirchPale
                                                    : LeafTone::OakSunlit;
    const glm::vec4 bark_uv = leaf_tile_uv(LeafShape::BarkPlate, bark_row);
    const glm::vec4 bark_moss_uv = leaf_tile_uv(LeafShape::BarkPlate, bark_moss_row);
    const float phase = rng.unit(); // one wind phase per tree
    const uint32_t wind_still = pack_wind(0.0f, phase);
    // Honest sway weight of a point on the wood: how far it stands from the
    // ground anchor, normalised by the tree's own height.
    const auto wood_sway = [&p](float y) {
        return 0.28f * std::pow(std::clamp(y / std::max(p.height, 1.0f), 0.0f, 1.0f),
                                1.6f);
    };

    const float crown_base = p.height * p.crown_base_frac;
    const float crown_top = p.height;
    const float crown_cy = (crown_base + crown_top) * 0.5f;
    const glm::vec3 crown_c{0.0f, crown_cy, 0.0f};
    const float crown_ry = (crown_top - crown_base) * 0.5f;

    // --- BOLE: near-vertical, zero-mean wander (§1.8: Curve=0, the wander is
    // sign-alternating and partially cancels). The lower third is rigid — the
    // measured boles hold their chord and bend only above (§1.7, frame 16).
    // 12, up from 7 (user, gallery 20:42: «видно в каких точках куски дерева
    // растут, вот эти конусы — надо чаще делать эти конусы, не такие
    // длинные»): shorter cones bend in smaller steps and the taper per
    // segment shrinks, so the joints stop reading as joints.
    const int bole_segments = 12;
    // A broadleaf bole ends inside its crown; a CONIFER's runs the whole
    // height — the leader IS the tree's spine, and every whorl hangs off it.
    const float bole_top_y = p.conifer ? p.height * 0.97f
                                       : crown_base + crown_ry * 0.7f;
    glm::vec3 pos{0.0f, FLARE_HEIGHT, 0.0f};
    glm::vec3 dir{0.0f, 1.0f, 0.0f};
    const float seg_len = (bole_top_y - FLARE_HEIGHT) / static_cast<float>(bole_segments);

    // Root flare + spurs, one axis with the bole (the one-tree stand's first
    // finding: a flare that does not share the bole's axis is a visible knee).
    const float flare_r = p.trunk_radius * 1.6f;
    bark_tube(obj.bark, glm::vec3{0.0f, -FLARE_DEPTH, 0.0f}, pos, dir, flare_r,
              p.trunk_radius, 7, bark_moss_uv, 0.0f, TAU * flare_r, wind_still,
              wind_still);
    for (int k = 0; k < ROOT_SPUR_COUNT; ++k) {
        const float az = TAU * (static_cast<float>(k) + 0.5f + rng.sym() * 0.3f)
                       / static_cast<float>(ROOT_SPUR_COUNT);
        const glm::vec3 rd{std::cos(az), 0.0f, std::sin(az)};
        const float reach = flare_r * (1.6f + rng.unit() * 1.0f);
        // BUTTRESS profile (passports §2, frame copy 8: the root is a RIDGE
        // of the flare that runs out along the ground, not a pipe leaning on
        // it). The spur starts INSIDE the flare, high and thick, and its
        // first chord is steep — the silhouette reads as one continuous
        // surface from bole to ground.
        const float r0 = std::max(p.trunk_radius * ROOT_SPUR_R_FRAC * 1.35f, 0.06f);
        const glm::vec3 pts[5] = {
            rd * (flare_r * 0.15f) + glm::vec3{0.0f, 0.62f, 0.0f},
            rd * (flare_r * 0.85f) + glm::vec3{0.0f, 0.26f, 0.0f},
            rd * (flare_r + reach * 0.35f) + glm::vec3{0.0f, ROOT_SPUR_RISE, 0.0f},
            rd * (flare_r + reach * 0.7f) + glm::vec3{0.0f, -0.05f, 0.0f},
            rd * (flare_r + reach) - glm::vec3{0.0f, ROOT_SPUR_SINK, 0.0f}};
        float rr = r0;
        for (int seg = 0; seg < 4; ++seg) {
            // Slow taper: a buttress keeps its section while it runs.
            const float nr = seg == 3 ? 0.0f
                                      : r0 * (1.0f - 0.22f * static_cast<float>(seg + 1));
            bark_tube(obj.ground, pts[seg], pts[seg + 1],
                      safe_normalize(pts[seg + 1] - pts[seg], rd), rr,
                      std::max(nr, 0.0f), 4, bark_moss_uv,
                      reach * 0.25f * static_cast<float>(seg), TAU * rr, wind_still,
                      wind_still);
            rr = std::max(nr, 0.02f);
        }
    }

    struct Ring {
        glm::vec3 pos;
        glm::vec3 dir;
        float radius;
    };
    std::vector<Ring> bole;
    bole.push_back({pos, dir, p.trunk_radius});
    glm::vec3 bole_frame = perp_of(dir); // transported: furrows run straight
    for (int s = 0; s < bole_segments; ++s) {
        const float t1 = static_cast<float>(s + 1) / static_cast<float>(bole_segments);
        if (s >= 2) { // the rigid lower third: wander only above it
            const float wob = 0.05f; // rad per segment, sign from the stream
            const glm::vec3 side{std::cos(rng.unit() * TAU), 0.0f,
                                 std::sin(rng.unit() * TAU)};
            dir = safe_normalize(dir + side * (rng.sym() * wob), dir);
        }
        const glm::vec3 next = pos + dir * seg_len;
        const float r0 = p.trunk_radius * std::pow(1.0f - (t1 - 1.0f / bole_segments) * 0.85f, 1.1f);
        const float r1 = p.trunk_radius * std::pow(1.0f - t1 * 0.85f, 1.1f);
        bark_tube(obj.bark, pos, next, dir, r0, std::max(r1, 0.05f), 7, bark_uv,
                  pos.y, TAU * r0, pack_wind(wood_sway(pos.y), phase),
                  pack_wind(wood_sway(next.y), phase), &bole_frame);
        pos = next;
        bole.push_back({pos, dir, std::max(r1, 0.05f)});
    }

    // --- THE BRANCH HIERARCHY, GROWN. The user's v2 verdict, point by point:
    // «ветки прямые, и их отсилы штуки 4-5» — branches now RECURSE (children
    // leave the bole AND other branches), each level thinner in proportion
    // (the pipe model: a child carries a fraction of its parent's section);
    // «должны расти небольшими сегментами, каждый чуть в сторону, а может и
    // вниз» — every branch walks in short segments, and every segment turns:
    // sideways wander, an upward pull that weakens with level, and an
    // occasional sag — which is how the referenced crowns actually move.
    struct SprayAnchor {
        glm::vec3 pos;
        glm::vec3 dir;
    };
    std::vector<SprayAnchor> anchors;

    if (!p.conifer) {
        // Recursive broadleaf grower. Levels: 0 scaffold, 1 branch, 2 twig.
        struct Grow {
            RegistryObject& obj;
            Rng& rng;
            std::vector<SprayAnchor>& anchors;
            uint32_t bark;
            glm::vec3 crown_c;
            float crown_rx, crown_ry;
            glm::vec4 bark_uv;
            float phase;
            float height;

            // A limb's sway weight: height plus horizontal reach from the
            // bole, both against the tree's own scale — the tip of a long
            // low oak limb sways as much as the crown top does.
            [[nodiscard]] uint32_t wind_at(glm::vec3 q) const {
                const float horiz = std::sqrt(q.x * q.x + q.z * q.z)
                                  / std::max(crown_rx, 0.5f);
                const float sway = std::clamp(
                    0.28f * std::pow(std::clamp(q.y / std::max(height, 1.0f),
                                                0.0f, 1.0f), 1.6f)
                        + 0.30f * horiz,
                    0.0f, 0.60f);
                const auto to_byte = [](float v) {
                    return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f)
                                                 * 255.0f + 0.5f);
                };
                return 0x8C800000u | (to_byte(phase) << 8) | to_byte(sway);
            }

            void run(glm::vec3 pos, glm::vec3 dir, float len, float radius, int level,
                     float arc = 0.0f) {
                // Shorter, more numerous cones (the same 20:42 remark).
                const int segs = level == 0 ? 8 : (level == 1 ? 6 : 4);
                const float seg = len / static_cast<float>(segs);
                glm::vec3 d = dir;
                glm::vec3 limb_frame = perp_of(d); // straight furrows per limb
                float r = radius;
                for (int si = 0; si < segs; ++si) {
                    // THE TURN, per segment: side wander always; up pull that
                    // fades with level (twigs stop caring about the sky); and
                    // one segment in ~4 dips DOWN — the sag real limbs show.
                    //
                    // THE OAK CLAUSE (user, on the colossus stand: «у дуба
                    // ветки не только вверх, ещё и вбок сильно... низкие ветки
                    // почти по всей длине параллельно земле идут»): a LOW
                    // scaffold keeps almost no upward pull for its whole
                    // length — the dome comes from the high scaffolds climbing
                    // while the low ones REACH.
                    const glm::vec3 side = safe_normalize(
                        glm::cross(d, glm::vec3{0.0f, 1.0f, 0.0f}),
                        glm::vec3{1.0f, 0.0f, 0.0f});
                    const bool low_limb = level == 0 && dir.y < 0.45f;
                    const float up_pull = (low_limb ? 0.05f : 0.22f)
                                        / static_cast<float>(1 + level);
                    const float dip = rng.unit() < 0.25f ? -0.18f : 0.0f;
                    d = safe_normalize(d + side * (rng.sym() * 0.30f)
                                         + glm::vec3{0.0f, up_pull + dip, 0.0f}, d);
                    const glm::vec3 np = pos + d * seg;
                    const float taper = 1.0f - 0.6f * (static_cast<float>(si + 1)
                                                       / static_cast<float>(segs));
                    // 0.05 floor, up from 0.025: a twig thinner than the
                    // frame's own pixel renders as an aliased HAIRLINE — the
                    // user's «узкие линии у листов» (21:53) were these wires,
                    // not card edges, and no foliage shader can fade geometry.
                    const float nr = std::max(radius * taper, 0.05f);
                    const int sides = level == 0 ? 5 : (level == 1 ? 4 : 3);
                    // EVERY level wears the bark texture — the vertex-coloured
                    // twigs read as BLACK sticks poking out of the birch's
                    // white limbs (user, gallery 21:20). The v-coordinate is
                    // ARC LENGTH along the limb, not world height (21:57: on a
                    // HORIZONTAL limb the height barely moves, so the furrows
                    // smeared sideways — «в разные стороны растёт»).
                    bark_tube(obj.bark, pos, np, d, r, nr, sides, bark_uv,
                              arc, TAU * r, wind_at(pos), wind_at(np),
                              &limb_frame);
                    arc += seg;

                    // CHILDREN leave mid-branch, alternating sides — from the
                    // second segment on, so the joint zone stays clean.
                    if (level < 2 && si >= 1 && si % 2 == 1) {
                        const int kids = level == 0 ? 2 : 1;
                        for (int c = 0; c < kids; ++c) {
                            if (rng.unit() < 0.3f) continue;
                            const float lr = ((si + c) % 2 == 0) ? 1.0f : -1.0f;
                            const glm::vec3 cd = safe_normalize(
                                d + side * (lr * (0.8f + rng.unit() * 0.6f))
                                  + glm::vec3{0.0f, rng.sym() * 0.35f, 0.0f}, side);
                            // The pipe model: the child takes ~half the parent's
                            // radius, and its length follows its section. The
                            // last level runs SHORTER (0.7): a long thin twig
                            // outruns its own leaf sheet and turns back into a
                            // bare wire past the foliage.
                            run(np, cd,
                                len * (0.5f + rng.unit() * 0.16f)
                                    * (level == 1 ? 0.7f : 1.0f),
                                nr * (0.52f + rng.unit() * 0.1f), level + 1);
                        }
                    }
                    pos = np;
                    r = nr;
                }
                // EVERY branch end carries foliage, INCLUDING the scaffold's
                // own tip: level 0 ended bare, and on the first correct
                // ground frame the scaffold tips read as naked hooks poking
                // out of the crown — the exact defect the header promises
                // dies by construction.
                anchors.push_back({pos, d});
                if (level == 2 && rng.unit() < 0.45f) {
                    anchors.push_back({pos - d * (len * 0.4f), d});
                }
                // THE CROWN'S INTERIOR (user: «в центре нет листвы... в центре
                // пустота»): foliage also grows along the INNER run of every
                // branch, not only at the rim its tips reach — an oak's crown
                // is full because leaves sprout wherever light does, and ours
                // sprouted only where recursion terminated.
                if (level >= 1 && rng.unit() < 0.35f) {
                    anchors.push_back({pos - d * (len * 0.75f), d});
                }
            }
        } grow{obj, rng, anchors, bark, crown_c, p.crown_radius, crown_ry,
               bark_uv, phase, p.height};

        for (int b = 0; b < p.scaffold_count; ++b) {
            const float az = GOLDEN_ANGLE * static_cast<float>(b) + rng.sym() * 0.5f;
            const float attach_span = bole_top_y - crown_base * 0.85f;
            const float attach_y = crown_base * 0.85f
                                 + attach_span * (0.1f + 0.8f * rng.unit());
            const Ring* at = &bole.front();
            for (const Ring& ring : bole) {
                if (ring.pos.y <= attach_y) at = &ring;
            }
            const glm::vec3 out{std::cos(az), 0.0f, std::sin(az)};
            const float k = std::clamp((attach_y - crown_base)
                                           / std::max(attach_span, 0.1f), 0.0f, 1.0f);
            const float elev = 0.3f + 0.75f * k + rng.sym() * 0.12f;
            const glm::vec3 bd = safe_normalize(
                out * std::cos(elev) + glm::vec3{0.0f, std::sin(elev), 0.0f}, out);
            // Rooted INSIDE the bole ring, base thickened (§1.5's inflate).
            grow.run(at->pos + out * (at->radius * 0.4f), bd,
                     p.crown_radius * (0.9f + rng.unit() * 0.35f),
                     at->radius * (0.5f + rng.unit() * 0.12f), 0);
        }
        anchors.push_back({bole.back().pos + glm::vec3{0.0f, 0.4f, 0.0f},
                           glm::vec3{0.0f, 1.0f, 0.0f}});

        // NATURAL ORNAMENTS (user, gallery 21:15: «жёлуди... пару обломленных
        // веток, дупло и такие украшения естественные»; дупло queued — needs
        // its own dark tile). BROKEN STUBS: short snags low on the bole,
        // ending in a jagged kink — the scars a real crown leaves behind.
        {
            const int stubs = 2 + (rng.unit() < 0.5f ? 1 : 0);
            for (int s = 0; s < stubs; ++s) {
                const float y = crown_base * (0.50f + 0.55f * rng.unit());
                const Ring* at = &bole.front();
                for (const Ring& ring : bole) {
                    if (ring.pos.y <= y) at = &ring;
                }
                const float az = rng.unit() * TAU;
                const glm::vec3 out{std::cos(az), 0.0f, std::sin(az)};
                const glm::vec3 d1 = safe_normalize(
                    out + glm::vec3{0.0f, -0.10f - 0.30f * rng.unit(), 0.0f}, out);
                const float len = 0.25f + 0.45f * rng.unit();
                const float rr = std::max(at->radius * 0.16f, 0.03f);
                const glm::vec3 base_p = at->pos + out * (at->radius * 0.5f);
                const glm::vec3 mid = base_p + d1 * len;
                const uint32_t wsy = pack_wind(wood_sway(y), phase);
                bark_tube(obj.bark, base_p, mid, d1, rr, rr * 0.7f, 4, bark_uv,
                          y, TAU * rr, wsy, wsy);
                const glm::vec3 d2 = safe_normalize(
                    d1 + glm::vec3{rng.sym() * 0.6f, 0.4f * rng.sym(),
                                   rng.sym() * 0.6f}, d1);
                bark_tube(obj.bark, mid, mid + d2 * (len * 0.25f), d2, rr * 0.7f,
                          0.0f, 3, bark_uv, y + len, TAU * rr, wsy, wsy);
            }
        }
        // ACORNS, oaks only: little bipyramids in twos under the sheets —
        // readable up close, invisible at range, exactly like the real thing.
        if (p.card_shape == LeafShape::RoundLobed) {
            // APPLE-SIZED acorns (user, 17.08: «жёлуди как яблоки, дуб
            // волшебный и большой»): 11 cm two-tone fruit — ochre-green body
            // under a brown cap — hung well below the sheets so they read
            // against the sky, not against the leaves.
            const uint32_t body_c = pack(glm::vec3{0.62f, 0.55f, 0.20f});
            const uint32_t cap_c = pack(glm::vec3{0.38f, 0.26f, 0.12f});
            for (const SprayAnchor& a : anchors) {
                if (rng.unit() > 0.18f) continue;
                for (int k = 0; k < 3; ++k) {
                    const glm::vec3 ap = a.pos
                        + glm::vec3{rng.sym() * 0.35f, -0.42f - rng.unit() * 0.25f,
                                    rng.sym() * 0.35f};
                    const float ar = 0.11f;
                    const glm::vec3 upv{0.0f, 1.0f, 0.0f};
                    // Stemlet, fruit body (two cones), brown cap on top.
                    tube_segment(obj.wood, ap + upv * (ar * 1.1f),
                                 ap + upv * (ar * 1.6f), upv, 0.012f, 0.008f, 3,
                                 cap_c);
                    tube_segment(obj.wood, ap - upv * (ar * 1.4f), ap, upv,
                                 0.02f, ar, 4, body_c);
                    tube_segment(obj.wood, ap, ap + upv * (ar * 0.9f), upv,
                                 ar, ar * 0.55f, 4, body_c);
                    tube_segment(obj.wood, ap + upv * (ar * 0.9f),
                                 ap + upv * (ar * 1.15f), upv, ar * 0.58f, 0.0f, 4,
                                 cap_c);
                }
            }
        }
    } else {
        // --- THE CONIFER, v6 (passports §2.1-2.2): whorls down the cone, but
        // a branch is DRESSED AS A FROND RIBBON — a bent strip carrying the
        // painted branch-with-needles sheet (the Skyrim construction; the
        // fir/pine photoscans' own «twig» material) — not tubes with flat
        // cards pinned to anchors. Bare wood shows only near the trunk, in
        // the windows between tiers.
        const glm::vec4 needle_uv = leaf_tile_uv(LeafShape::NeedleFan, p.tone);
        const float phase_c = phase; // fronds share the tree's wind phase
        // One frond: a V-FOLDED sagging strip — spine down the middle, both
        // halves tilted DOWN like a real lapa's needle sheets. The flat
        // ribbon of the first cut vanished from eye level BY CONSTRUCTION
        // (a horizontal zero-thickness plane seen from its own height is a
        // one-pixel line: the whole conifer row read as naked poles, found
        // via the placement log + .dfo dump — the geometry was always there,
        // only the bearing was impossible). A folded sheet shows a face from
        // every bearing.
        const auto emit_frond = [&](glm::vec3 pos0, glm::vec3 dir0, float len,
                                    float half_w, float droop_rad) {
            const int segs = 4;
            glm::vec3 fpos = pos0;
            glm::vec3 fd = dir0;
            const glm::vec3 up{0.0f, 1.0f, 0.0f};
            const glm::vec3 side = safe_normalize(glm::cross(up, fd),
                                                  glm::vec3{1.0f, 0.0f, 0.0f});
            const float jit = 0.40f + rng.unit() * 0.26f;
            // Dihedral: how far the edges drop below the spine (rad-ish
            // fraction of the half-width). 0.35-0.55 reads as a living lapa.
            const float fold = 0.35f + rng.unit() * 0.20f;
            const auto base_v = static_cast<uint32_t>(obj.cards.vertices.size());
            const float v_mid = needle_uv.y + (needle_uv.w - needle_uv.y) * 0.5f;
            for (int s = 0; s <= segs; ++s) {
                const float t = static_cast<float>(s) / static_cast<float>(segs);
                if (s > 0) {
                    // Sag per segment; the LAST segment lifts — spruce tips
                    // turn up (frame 6).
                    const float sag = (s == segs ? 0.55f : -1.0f) * droop_rad
                                    / static_cast<float>(segs);
                    fd = safe_normalize(fd + up * sag + side * (rng.sym() * 0.05f), fd);
                    fpos += fd * (len / static_cast<float>(segs));
                }
                // Width: narrow butt, widest mid, tapering tip.
                const float ww = half_w
                    * (0.30f + 0.70f * std::sin(std::min(3.1416f * (0.18f + 0.82f * t),
                                                         3.1416f)));
                const float uu = needle_uv.x
                    + (needle_uv.z - needle_uv.x) * (0.04f + 0.92f * t);
                const float drop = ww * fold;
                const float sway = 0.15f + 0.85f * t; // honest: distance from support
                const uint32_t col = pack({sway, phase_c, jit});
                // Half-plane normals face up-and-outward on each side, so the
                // two faces shade differently — the fold reads even in flat light.
                const glm::vec3 nl = safe_normalize(up * 0.8f - side * fold, up);
                const glm::vec3 nr = safe_normalize(up * 0.8f + side * fold, up);
                obj.cards.vertices.push_back(
                    {fpos - side * ww - up * drop, nl,
                     {uu, needle_uv.y + (needle_uv.w - needle_uv.y) * 0.06f}, col});
                obj.cards.vertices.push_back(
                    {fpos, safe_normalize(nl + nr, up), {uu, v_mid}, col});
                obj.cards.vertices.push_back(
                    {fpos + side * ww - up * drop, nr,
                     {uu, needle_uv.y + (needle_uv.w - needle_uv.y) * 0.94f}, col});
            }
            for (int s = 0; s < segs; ++s) {
                const uint32_t a0 = base_v + static_cast<uint32_t>(s) * 3u;
                obj.cards.indices.insert(obj.cards.indices.end(),
                                         {a0, a0 + 3u, a0 + 4u, a0, a0 + 4u, a0 + 1u,
                                          a0 + 1u, a0 + 4u, a0 + 5u,
                                          a0 + 1u, a0 + 5u, a0 + 2u});
            }
        };

        for (int w = 0; w < p.whorl_count; ++w) {
            const float t = (static_cast<float>(w) + 0.5f)
                          / static_cast<float>(p.whorl_count);
            const float y = crown_base + (p.height - crown_base) * t;
            const float reach = p.crown_radius * (1.0f - 0.85f * t)
                              * (0.9f + rng.unit() * 0.2f);
            if (reach < 0.4f) continue;
            const Ring* at = &bole.front();
            for (const Ring& ring : bole) {
                if (ring.pos.y <= y) at = &ring;
            }
            const int count = p.whorl_branches;
            const float az0 = rng.unit() * TAU;
            for (int b = 0; b < count; ++b) {
                if (rng.unit() < 0.06f) continue; // ragged whorls, not a fan
                const float az = az0 + TAU * static_cast<float>(b)
                                     / static_cast<float>(count)
                               + rng.sym() * 0.3f;
                const glm::vec3 out{std::cos(az), 0.0f, std::sin(az)};
                const glm::vec3 bp = at->pos + out * (at->radius * 0.5f);
                const glm::vec3 d = safe_normalize(
                    out + glm::vec3{0.0f, 0.14f - p.droop * 0.45f, 0.0f}, out);
                // Bare wood: the inner quarter only — the rest is frond.
                const float r0 = std::max(at->radius * 0.30f, 0.03f);
                const float wood_len = reach * 0.28f;
                const glm::vec3 wp = bp + d * wood_len;
                bark_tube(obj.bark, bp, wp, d, r0, std::max(r0 * 0.55f, 0.02f), 4,
                          bark_uv, bp.y, TAU * std::max(r0, 0.05f),
                          pack_wind(wood_sway(bp.y), phase),
                          pack_wind(std::min(wood_sway(bp.y) + 0.12f, 0.6f), phase));
                // The ribbon overlaps the wood so the joint never shows.
                emit_frond(bp + d * (wood_len * 0.55f), d, reach * 0.9f,
                           std::min(reach * 0.42f, 2.0f) * (0.85f + rng.unit() * 0.3f)
                               * p.frond_width,
                           p.droop * (0.8f + rng.unit() * 0.4f));
            }
        }

        // THE DEAD-BRANCH SKIRT (both photoscans, every variant: bare broken
        // stubs at 0.10-0.40 of height, under the live crown). Only where the
        // live crown starts high enough to leave the band exposed.
        const float skirt_top = std::min(crown_base * 0.95f, p.height * 0.40f);
        const float skirt_bot = p.height * 0.10f;
        if (skirt_top - skirt_bot > 0.5f) {
            const int stubs = 6 + static_cast<int>(p.height * 0.35f);
            for (int s = 0; s < stubs; ++s) {
                const float y = skirt_bot + (skirt_top - skirt_bot) * rng.unit();
                const float az = rng.unit() * TAU;
                const glm::vec3 out{std::cos(az), 0.0f, std::sin(az)};
                const Ring* at = &bole.front();
                for (const Ring& ring : bole) {
                    if (ring.pos.y <= y) at = &ring;
                }
                const float len = p.crown_radius * (0.12f + rng.unit() * 0.22f);
                const glm::vec3 d = safe_normalize(
                    out + glm::vec3{0.0f, -0.15f - rng.unit() * 0.25f, 0.0f}, out);
                const float rr = std::max(at->radius * 0.12f, 0.015f);
                bark_tube(obj.bark, at->pos + out * (at->radius * 0.5f),
                          at->pos + out * (at->radius * 0.5f) + d * len, d, rr,
                          0.0f, 3, bark_uv, y, TAU * rr,
                          pack_wind(wood_sway(y), phase),
                          pack_wind(std::min(wood_sway(y) + 0.08f, 0.5f), phase));
            }
        }

        // The leader spike above the last whorl, and its own small crown.
        anchors.push_back({bole.back().pos + glm::vec3{0.0f, 0.3f, 0.0f},
                           glm::vec3{0.0f, 1.0f, 0.0f}});
    }

    // --- CURVED LEAF SHEETS ON THE ANCHORS (the user's 21:14 design). One
    // sheet per anchor, occasionally a second smaller one — NOT a count per
    // branch: fewer, bigger masses (21:12).
    for (const SprayAnchor& a : anchors) {
        // spray_per_branch > 2 asks for a LUSHER crown (the colossus: «листвы
        // мало» — more sheets per anchor, leaves stay leaf-sized).
        const int sprays = std::max(1, p.spray_per_branch - 1)
                         + (rng.unit() < 0.30f ? 1 : 0);
        for (int i = 0; i < sprays; ++i) {
            const glm::vec3 jitter{rng.sym() * 0.5f, rng.sym() * 0.35f,
                                   rng.sym() * 0.5f};
            // Jitter small enough that the card always OVERLAPS its anchor
            // (user: «часть листка летает в воздухе, ни к чему не
            // присоединено» — the old jitter could carry a spray clear of its
            // twig).
            // THE CURVED LEAF SHEET — the user's own design, gallery 21:14:
            // «не надо плоскими делать пачки листвы, пусть они будут как
            // ландшафт земли разных уровней, не плоскими, а изгибающимися».
            // One BENT 3x3 patch drapes OVER its twig (centre on the anchor —
            // 21:05: foliage must sit ON the branch, not hang below it), lies
            // near-horizontal with a free yaw and a modest tilt, its corners
            // sag, its surface undulates per-vertex. A curved sheet has no
            // edge-on bearing (the «полоски» die), no flat symmetry axis
            // (21:08), and ONE sheet replaces the old pair of crossed cards
            // (21:12: fewer, bigger masses — not a swarm of flies).
            const glm::vec3 up{0.0f, 1.0f, 0.0f};
            const float half_w = p.crown_radius * p.spray_frac
                               * (1.25f + rng.unit() * 0.55f)
                               * (i == 0 ? 1.0f : 0.6f);
            const glm::vec3 c = a.pos + jitter * (p.crown_radius * 0.025f);
            const float yaw = rng.unit() * TAU;
            glm::vec3 e1{std::cos(yaw), 0.0f, std::sin(yaw)};
            glm::vec3 e2{-std::sin(yaw), 0.0f, std::cos(yaw)};
            // Tilt the whole sheet a little toward the branch direction, so
            // canopy pads follow their limbs instead of a global plane.
            const float tilt = rng.sym() * 0.30f;
            e1 = safe_normalize(e1 + up * (tilt * 0.5f), e1);
            e2 = safe_normalize(e2 + up * (rng.sym() * 0.22f), e2);
            const float droop = half_w * (0.28f + rng.unit() * 0.18f);
            const float jit_v = 0.42f + rng.unit() * 0.22f;
            const glm::vec3 sway_origin{0.0f, crown_base, 0.0f};
            const float sway_span = p.crown_radius * 1.8f;
            const glm::vec4 uvr = leaf_tile_uv(p.card_shape, p.tone);
            glm::vec3 vp[3][3];
            for (int gj = 0; gj < 3; ++gj) {
                for (int gi = 0; gi < 3; ++gi) {
                    const float fx = static_cast<float>(gi - 1);
                    const float fz = static_cast<float>(gj - 1);
                    glm::vec3 q = c + e1 * (fx * half_w) + e2 * (fz * half_w);
                    const float r2 = (fx * fx + fz * fz) * 0.5f; // 0 centre, 1 corner
                    q.y -= droop * std::pow(r2, 1.3f); // corners sag — the dome
                    q.y += (gi == 1 && gj == 1 ? 0.10f : 0.0f) * half_w;
                    q.y += rng.sym() * 0.06f * half_w; // the terraced undulation
                    vp[gj][gi] = q;
                }
            }
            const auto sheet_base = static_cast<uint32_t>(obj.cards.vertices.size());
            for (int gj = 0; gj < 3; ++gj) {
                for (int gi = 0; gi < 3; ++gi) {
                    // Per-vertex normal from the curved surface itself.
                    const glm::vec3 du = vp[gj][std::min(gi + 1, 2)]
                                       - vp[gj][std::max(gi - 1, 0)];
                    const glm::vec3 dv = vp[std::min(gj + 1, 2)][gi]
                                       - vp[std::max(gj - 1, 0)][gi];
                    glm::vec3 n = safe_normalize(glm::cross(dv, du), up);
                    if (n.y < 0.0f) n = -n; // sheets face the sky
                    const float sway = std::clamp(
                        glm::length(vp[gj][gi] - sway_origin) / sway_span, 0.0f, 1.0f);
                    obj.cards.vertices.push_back(
                        {vp[gj][gi], n,
                         {uvr.x + (uvr.z - uvr.x) * (static_cast<float>(gi) * 0.5f),
                          uvr.y + (uvr.w - uvr.y) * (static_cast<float>(gj) * 0.5f)},
                         pack({sway, phase, jit_v})});
                }
            }
            for (int gj = 0; gj < 2; ++gj) {
                for (int gi = 0; gi < 2; ++gi) {
                    const uint32_t v00 = sheet_base + static_cast<uint32_t>(gj * 3 + gi);
                    obj.cards.indices.insert(obj.cards.indices.end(),
                                             {v00, v00 + 3u, v00 + 4u,
                                              v00, v00 + 4u, v00 + 1u});
                }
            }
        }
    }

    // --- BARK AND MOSS, AS A POST-PASS OVER THE WOOD (user, with the Skyrim
    // close-ups: «поработать над текстурой стволов... мох, корни у ствола
    // снизу»). The references show two things at trunk scale: deep VERTICAL
    // furrows (alternating lit ridge / dark groove) and a green moss film
    // creeping up the butt and over the root spurs. Both are value work, and
    // flat-shaded faces own their vertices — so the texture is painted into
    // vertex colour, which survives our palette where a normal map would not.
    const auto bark_pass = [&](MeshData& mesh) {
        for (platform::Vertex& v : mesh.vertices) {
            const uint32_t col = v.color_rgba;
            // Only wood-coloured vertices: crown masses (leaf tones) pass through.
            if (col != bark) continue;
            const float az = std::atan2(v.position.z, v.position.x);
            // FURROWS: value stripes around the circumference, wandering
            // slightly with height so the grooves read as grain, not as paint.
            const float stripe = std::sin(az * 9.0f + v.position.y * 0.35f)
                               + 0.5f * std::sin(az * 23.0f - v.position.y * 0.2f);
            const float furrow = 0.86f + 0.17f * std::clamp(stripe, -1.0f, 1.0f);
            // MOSS: strongest at the ground, gone by ~4 m, patchy by azimuth.
            const float moss_h = std::clamp(1.0f - v.position.y / 4.0f, 0.0f, 1.0f);
            const float patch = 0.5f + 0.5f * std::sin(az * 3.0f + 1.7f);
            const float moss = moss_h * patch * 0.55f;
            glm::vec3 c = p.bark * furrow;
            c = c * (1.0f - moss) + glm::vec3{0.22f, 0.34f, 0.13f} * moss;
            v.color_rgba = pack(glm::clamp(c, glm::vec3{0.0f}, glm::vec3{1.0f}));
        }
    };
    bark_pass(obj.wood); // only the flat-colour twigs remain in this stream;
                         // the textured bark/ground streams carry their own
                         // furrows and moss in the atlas tiles

    obj.content_hash = object_content_hash(obj);
    return obj;
}

RegistryObject forge_bush(const BushForgeParams& p) {
    RegistryObject obj;
    obj.name = p.name;
    obj.kind = "bush";
    obj.source = "forge:bush seed=" + std::to_string(p.seed);
    Rng rng(p.seed * 0x9E3779B97F4A7C15ull + 0xB5297A4D3F84D5B5ull);
    const LeafTone bark_row = LeafTone::OakMid;
    const glm::vec4 bark_uv = leaf_tile_uv(LeafShape::BarkPlate, bark_row);
    const glm::vec4 leaf_uv = leaf_tile_uv(p.card_shape, p.tone);
    const float phase = rng.unit();
    const glm::vec3 up{0.0f, 1.0f, 0.0f};
    // Curved sheet helper, small: 3x3 patch, corners sagging.
    const auto sheet = [&](glm::vec3 c, float half_w) {
        const float yaw = rng.unit() * TAU;
        glm::vec3 e1{std::cos(yaw), 0.0f, std::sin(yaw)};
        glm::vec3 e2{-std::sin(yaw), 0.0f, std::cos(yaw)};
        e1 = safe_normalize(e1 + up * (rng.sym() * 0.25f), e1);
        e2 = safe_normalize(e2 + up * (rng.sym() * 0.25f), e2);
        const float droop = half_w * (0.30f + rng.unit() * 0.2f);
        const float jit = 0.40f + rng.unit() * 0.26f;
        glm::vec3 vp[3][3];
        for (int gj = 0; gj < 3; ++gj) {
            for (int gi = 0; gi < 3; ++gi) {
                const float fx = static_cast<float>(gi - 1);
                const float fz = static_cast<float>(gj - 1);
                glm::vec3 q = c + e1 * (fx * half_w) + e2 * (fz * half_w);
                q.y -= droop * std::pow((fx * fx + fz * fz) * 0.5f, 1.3f);
                q.y += rng.sym() * 0.05f * half_w;
                if (q.y < 0.05f) q.y = 0.05f; // the ground is not fair game
                vp[gj][gi] = q;
            }
        }
        const auto base_v = static_cast<uint32_t>(obj.cards.vertices.size());
        for (int gj = 0; gj < 3; ++gj) {
            for (int gi = 0; gi < 3; ++gi) {
                const glm::vec3 du = vp[gj][std::min(gi + 1, 2)]
                                   - vp[gj][std::max(gi - 1, 0)];
                const glm::vec3 dv = vp[std::min(gj + 1, 2)][gi]
                                   - vp[std::max(gj - 1, 0)][gi];
                glm::vec3 n = safe_normalize(glm::cross(dv, du), up);
                if (n.y < 0.0f) n = -n;
                const float sway = std::clamp(vp[gj][gi].y / std::max(p.height, 0.3f),
                                              0.1f, 1.0f);
                obj.cards.vertices.push_back(
                    {vp[gj][gi], n,
                     {leaf_uv.x + (leaf_uv.z - leaf_uv.x) * (static_cast<float>(gi) * 0.5f),
                      leaf_uv.y + (leaf_uv.w - leaf_uv.y) * (static_cast<float>(gj) * 0.5f)},
                     pack({sway, phase, jit})}); // b is PER SHEET by contract
            }
        }
        for (int gj = 0; gj < 2; ++gj) {
            for (int gi = 0; gi < 2; ++gi) {
                const uint32_t v00 = base_v + static_cast<uint32_t>(gj * 3 + gi);
                obj.cards.indices.insert(obj.cards.indices.end(),
                                         {v00, v00 + 3u, v00 + 4u, v00, v00 + 4u, v00 + 1u});
            }
        }
    };
    std::vector<glm::vec3> tips; // stem ends — where the berries hang
    for (int s = 0; s < p.stems; ++s) {
        const float az = TAU * (static_cast<float>(s) + rng.unit() * 0.5f)
                       / static_cast<float>(p.stems);
        const glm::vec3 out{std::cos(az), 0.0f, std::sin(az)};
        glm::vec3 pos = out * (p.radius * 0.12f);
        // A CREEPING bush (можжевельник) hugs the ground: stems run nearly
        // flat; an upright bush fountains.
        glm::vec3 d = p.creeping ? safe_normalize(out + up * 0.18f, out)
                                 : safe_normalize(out * 0.55f + up, up);
        float r = 0.028f + rng.unit() * 0.014f;
        const float stem_len = (p.creeping ? p.radius * 1.1f : p.height)
                             * (0.75f + rng.unit() * 0.35f);
        glm::vec3 frame = perp_of(d);
        float arc = 0.0f;
        const int segs = 3;
        for (int si = 0; si < segs; ++si) {
            d = p.creeping
                ? safe_normalize(d + out * 0.25f + up * (si == 0 ? 0.02f : -0.10f), d)
                : safe_normalize(d + out * 0.18f + up * (si == 0 ? 0.1f : -0.05f), d);
            const glm::vec3 np = pos + d * (stem_len / segs);
            const float nr = std::max(r * 0.6f, 0.012f);
            bark_tube(obj.bark, pos, np, d, r, nr, 3, bark_uv, arc, TAU * r,
                      pack_wind(0.2f + 0.5f * si / segs, phase),
                      pack_wind(0.2f + 0.5f * (si + 1) / segs, phase), &frame);
            arc += stem_len / segs;
            // Sheets ride the OUTER half of every stem, down to the grass.
            if (si >= 1 || rng.unit() < 0.5f) {
                sheet((pos + np) * 0.5f, p.radius * (0.42f + rng.unit() * 0.2f));
            }
            pos = np;
            r = nr;
        }
        sheet(pos, p.radius * (0.5f + rng.unit() * 0.2f));
        tips.push_back(pos);
    }
    // BERRIES (user: «крупными и их должно быть видно»): closed bipyramids in
    // little clusters at the stem tips, hanging just under the sheet rims —
    // against the leaf mass, not buried in it.
    if (p.berry_count > 0) {
        const uint32_t berry_c = pack(p.berry);
        const uint32_t spot_c = pack(p.berry_spot);
        const uint32_t stalk_c = pack(p.bark * 0.75f);
        const uint32_t calyx_c = pack(glm::vec3{0.16f, 0.20f, 0.10f});
        // One fruit body at `at`; elong stretches Drops pole to pole. The
        // pattern and the calyx are raised geometry on purpose: paint-only
        // detail dies at 4-7 cm, ribs and specks survive.
        const auto fruit = [&](const glm::vec3& at, float br, float elong) {
            tube_segment(obj.wood, at - up * (br * elong), at, up, br * 0.25f,
                         br * 0.95f, 4, berry_c);
            tube_segment(obj.wood, at, at + up * (br * 0.8f * elong), up,
                         br * 0.95f, 0.0f, 4, berry_c);
            if (p.berry_pattern == 1) {  // крапинка
                for (int k = 0; k < 3; ++k) {
                    const float a = TAU * (static_cast<float>(k)
                                           + rng.unit() * 0.5f) / 3.0f;
                    const glm::vec3 sp = at
                        + glm::vec3{std::cos(a), rng.sym() * 0.2f, std::sin(a)}
                          * (br * 0.78f);
                    tube_segment(obj.wood, sp - up * (br * 0.16f),
                                 sp + up * (br * 0.16f), up, br * 0.03f,
                                 br * 0.16f, 3, spot_c);
                }
            } else if (p.berry_pattern == 2) {  // полоски
                for (int k = 0; k < 2; ++k) {
                    const float a = TAU * 0.5f * static_cast<float>(k)
                                  + rng.unit() * 0.8f;
                    const glm::vec3 rib{std::cos(a), 0.0f, std::sin(a)};
                    tube_segment(obj.wood,
                                 at - up * (br * 0.55f * elong) + rib * (br * 0.62f),
                                 at + up * (br * 0.55f * elong) + rib * (br * 0.62f),
                                 up, br * 0.10f, br * 0.10f, 3, spot_c);
                }
            }
            if (p.berry_sepal) {  // попка-чашелистик под плодом
                tube_segment(obj.wood, at - up * (br * (elong + 0.5f)),
                             at - up * (br * elong * 0.6f), up, br * 0.06f,
                             br * 0.42f, 4, calyx_c);
            }
        };
        for (int bi = 0; bi < p.berry_count; ++bi) {
            const glm::vec3& tip = tips[static_cast<size_t>(bi) % tips.size()];
            const float br = p.berry_r * (0.8f + rng.unit() * 0.4f);
            const float hang = p.berry_r * p.berry_stalk
                             + 0.06f + rng.unit() * 0.10f;
            const glm::vec3 at = tip
                + glm::vec3{rng.sym() * 0.22f, -hang, rng.sym() * 0.22f};
            // ВЕТОЧКА: the stalk that actually carries the fruit — the user
            // asked for visible attachment, not floating spheres.
            tube_segment(obj.wood, tip, at + up * (br * 0.7f), up, br * 0.13f,
                         br * 0.10f, 3, stalk_c);
            switch (p.berry_style) {
            case BerryStyle::Balls:
                fruit(at, br, 1.0f);
                break;
            case BerryStyle::Drops:
                fruit(at, br * 0.82f, 1.6f);
                break;
            case BerryStyle::Cluster: {
                // Rowan-style bunch: 5-7 smaller fruits fan out and down
                // from the stalk's end on their own short stalklets.
                const int n = 5 + static_cast<int>(rng.unit() * 2.99f);
                for (int k = 0; k < n; ++k) {
                    const float a = TAU * static_cast<float>(k)
                                  / static_cast<float>(n) + rng.unit() * 0.6f;
                    const glm::vec3 off{std::cos(a) * br * 0.9f,
                                        -br * (0.5f + rng.unit() * 1.1f),
                                        std::sin(a) * br * 0.9f};
                    tube_segment(obj.wood, at + up * (br * 0.5f), at + off,
                                 up, br * 0.07f, br * 0.05f, 3, stalk_c);
                    fruit(at + off, br * 0.55f, 1.0f);
                }
                break;
            }
            }
        }
    }
    obj.content_hash = object_content_hash(obj);
    return obj;
}

RegistryObject forge_fallen_log(const LogForgeParams& p) {
    RegistryObject obj;
    obj.name = p.name;
    obj.kind = "log";
    obj.source = "forge:log seed=" + std::to_string(p.seed);
    Rng rng(p.seed * 0x9E3779B97F4A7C15ull + 0x94D049BB133111EBull);
    const glm::vec4 uv = leaf_tile_uv(LeafShape::BarkPlate,
                                      p.mossy ? LeafTone::OakDeep : LeafTone::OakMid);
    const uint32_t still = pack_wind(0.0f, rng.unit());
    // The trunk lies along +X, one third sunk (design §5.10: half-sunk), with
    // a gentle sag where it meets the ground mid-span.
    const int segs = 6;
    glm::vec3 pos{0.0f, p.radius * 0.62f, 0.0f};
    glm::vec3 d{1.0f, 0.0f, 0.0f};
    glm::vec3 frame{0.0f, 1.0f, 0.0f};
    float r = p.radius;
    float arc = 0.0f;
    for (int s = 0; s < segs; ++s) {
        const float t1 = static_cast<float>(s + 1) / segs;
        d = safe_normalize(glm::vec3{1.0f, -0.02f + 0.03f * rng.sym(),
                                     rng.sym() * 0.12f}, d);
        const glm::vec3 np = pos + d * (p.length / segs);
        // The BROKEN END: the last segment collapses to a ragged point.
        const float nr = s == segs - 1 ? 0.06f : p.radius * (1.0f - 0.35f * t1);
        bark_tube(obj.ground, pos, np, d, r, nr, 6, uv, arc, TAU * r, still, still,
                  &frame);
        arc += p.length / segs;
        pos = np;
        r = nr;
    }
    // The ROOT PLATE at the butt: a short flare stub + radial spurs — the
    // «несколько корней из пары многоугольников» recipe of rule 52, standing
    // upright the way a downed tree presents its plate.
    for (int k = 0; k < 6; ++k) {
        const float az = TAU * (static_cast<float>(k) + 0.5f + rng.sym() * 0.2f) / 6.0f;
        // Spurs fan in the Y/Z plane (the plate faces -X).
        const glm::vec3 rd{-0.18f, std::sin(az), std::cos(az)};
        const glm::vec3 base{0.0f, p.radius * 0.62f, 0.0f};
        const float reach = p.radius * (1.5f + rng.unit() * 0.9f);
        glm::vec3 f2 = perp_of(safe_normalize(rd, {0, 1, 0}));
        bark_tube(obj.ground, base, base + safe_normalize(rd, {0.0f, 1.0f, 0.0f}) * reach,
                  safe_normalize(rd, {0.0f, 1.0f, 0.0f}),
                  p.radius * 0.34f, 0.02f, 4, uv, 0.0f,
                  TAU * p.radius * 0.34f, still, still, &f2);
    }
    obj.content_hash = object_content_hash(obj);
    return obj;
}

RegistryObject forge_ground_prop(const GroundPropParams& p) {
    RegistryObject obj;
    obj.name = p.name;
    obj.kind = "prop";
    obj.source = "forge:prop seed=" + std::to_string(p.seed);
    Rng rng(p.seed * 0x9E3779B97F4A7C15ull + 0xDA3E39CB94B95BDBull);
    const glm::vec3 up{0.0f, 1.0f, 0.0f};
    const float phase = rng.unit();
    if (p.kind == GroundPropKind::GrassTuft) {
        // A tuft: 10-14 folded BLADES fanning from one root — rule 52's
        // small-flora exemption, with a concrete shape (a bent taper, not a
        // rectangle). Leaf material: it sways and it transmits.
        // A TINY ROOT NUB in the wood stream: a cards-only object has no
        // solid footprint, and the scene placer refused to stand all 84
        // tufts of the glade («111 of 195 standing») — the nub is buried in
        // the blades and gives the mesh its truth about where it stands.
        tube_segment(obj.wood, glm::vec3{0.0f, -0.02f, 0.0f},
                     glm::vec3{0.0f, 0.06f, 0.0f}, up, 0.025f, 0.015f, 3,
                     pack(glm::vec3{0.20f, 0.24f, 0.10f}));
        const glm::vec4 uv = leaf_tile_uv(LeafShape::RaggedTip, LeafTone::BirchLight);
        const int blades = 13 + static_cast<int>(rng.unit() * 6.0f);
        for (int b = 0; b < blades; ++b) {
            const float az = TAU * static_cast<float>(b) / blades + rng.sym() * 0.3f;
            const glm::vec3 out{std::cos(az), 0.0f, std::sin(az)};
            const float h = p.height * (0.6f + rng.unit() * 0.5f);
            // Wide blades (user, 17.08: «травы вообще не вижу») — a 2 cm
            // blade vanishes at ten paces; playability beats botany.
            const float w0 = 0.034f + rng.unit() * 0.018f;
            glm::vec3 pos{out.x * 0.03f, 0.0f, out.z * 0.03f};
            glm::vec3 d = safe_normalize(up + out * (0.15f + rng.unit() * 0.25f), up);
            const glm::vec3 side = safe_normalize(glm::cross(up, out),
                                                  glm::vec3{1.0f, 0.0f, 0.0f});
            const auto base_v = static_cast<uint32_t>(obj.cards.vertices.size());
            const int segs = 3;
            for (int s = 0; s <= segs; ++s) {
                const float t = static_cast<float>(s) / segs;
                if (s > 0) {
                    d = safe_normalize(d + out * 0.35f * t, d); // the blade bows
                    pos += d * (h / segs);
                }
                const float w = w0 * (1.0f - 0.85f * t);
                const glm::vec3 n = safe_normalize(glm::cross(d, side), up);
                const uint32_t col = pack({0.25f + 0.75f * t, phase, 0.5f});
                const float uu = uv.x + (uv.z - uv.x) * (0.15f + 0.7f * t);
                obj.cards.vertices.push_back({pos - side * w, n,
                                              {uu, uv.y + (uv.w - uv.y) * 0.30f}, col});
                obj.cards.vertices.push_back({pos + side * w, n,
                                              {uu, uv.y + (uv.w - uv.y) * 0.70f}, col});
            }
            for (int s = 0; s < segs; ++s) {
                const uint32_t a0 = base_v + static_cast<uint32_t>(s) * 2u;
                obj.cards.indices.insert(obj.cards.indices.end(),
                                         {a0, a0 + 2u, a0 + 3u, a0, a0 + 3u, a0 + 1u});
            }
        }
    } else if (p.kind == GroundPropKind::Flowers) {
        // 3-5 stems, each a thin green tube crowned by a petal fan: petals
        // are vertex-coloured little quads on the PROP program (the atlas has
        // no petal hue), the head a tiny dark heart.
        const uint32_t stem_c = pack(glm::vec3{0.22f, 0.34f, 0.12f});
        const uint32_t petal_c = pack(p.accent);
        const uint32_t heart_c = pack(glm::vec3{0.55f, 0.42f, 0.10f});
        const int stems = 4 + static_cast<int>(rng.unit() * 4.0f);
        for (int s = 0; s < stems; ++s) {
            const float az = TAU * rng.unit();
            const glm::vec3 at{std::cos(az) * 0.18f * (1.0f + rng.unit()), 0.0f,
                               std::sin(az) * 0.18f * (1.0f + rng.unit())};
            const float h = p.height * (0.7f + rng.unit() * 0.5f);
            tube_segment(obj.wood, at, at + up * h, up, 0.014f, 0.009f, 3, stem_c);
            const glm::vec3 head = at + up * h;
            const int petals = 5 + (rng.unit() < 0.4f ? 1 : 0);
            for (int q = 0; q < petals; ++q) {
                const float pa = TAU * static_cast<float>(q) / petals + phase;
                const glm::vec3 pd{std::cos(pa), 0.35f, std::sin(pa)};
                const glm::vec3 pu = safe_normalize(pd, up) * p.bloom;
                const glm::vec3 pv = safe_normalize(glm::cross(up, pd),
                                                    glm::vec3{1.0f, 0.0f, 0.0f})
                                   * (p.bloom * 0.5f);
                const auto b = static_cast<uint32_t>(obj.wood.vertices.size());
                const glm::vec3 n = safe_normalize(up + pd * 0.3f, up);
                obj.wood.vertices.push_back({head + pv * 0.4f, n, {0, 0}, petal_c});
                obj.wood.vertices.push_back({head - pv * 0.4f, n, {0, 1}, petal_c});
                obj.wood.vertices.push_back({head + pu - pv, n, {1, 1}, petal_c});
                obj.wood.vertices.push_back({head + pu + pv, n, {1, 0}, petal_c});
                obj.wood.indices.insert(obj.wood.indices.end(),
                                        {b, b + 1u, b + 2u, b, b + 2u, b + 3u});
            }
            tube_segment(obj.wood, head, head + up * (p.bloom * 0.28f), up,
                         p.bloom * 0.30f, p.bloom * 0.20f, 4, heart_c);
        }
    } else if (p.kind == GroundPropKind::Fern) {
        // FERN: a fan of 7-9 arcing V-folded fronds from one crown point —
        // the pinnate needle sheet reads as a fern leaf at this scale, and
        // the fold keeps it visible edge-on (the same lesson the conifer
        // lapa taught). Leaf material: sways, transmits backlight.
        const glm::vec4 uv = leaf_tile_uv(LeafShape::NeedleFan, LeafTone::WillowOlive);
        const int fronds = 7 + static_cast<int>(rng.unit() * 3.0f);
        for (int f = 0; f < fronds; ++f) {
            const float az = TAU * static_cast<float>(f) / fronds + rng.sym() * 0.25f;
            const glm::vec3 out{std::cos(az), 0.0f, std::sin(az)};
            const glm::vec3 side = safe_normalize(glm::cross(up, out),
                                                  glm::vec3{1.0f, 0.0f, 0.0f});
            glm::vec3 fpos{out.x * 0.03f, 0.04f, out.z * 0.03f};
            glm::vec3 fd = safe_normalize(out * 0.55f + up, up); // rises, then arcs
            const float len = p.height * (0.85f + rng.unit() * 0.4f);
            const float half_w = len * 0.16f;
            const float fold = 0.45f;
            const int fsegs = 3;
            const float v_mid = uv.y + (uv.w - uv.y) * 0.5f;
            const auto base_v = static_cast<uint32_t>(obj.cards.vertices.size());
            for (int s2 = 0; s2 <= fsegs; ++s2) {
                const float t = static_cast<float>(s2) / fsegs;
                if (s2 > 0) {
                    fd = safe_normalize(fd + out * 0.30f - up * (0.42f * t), fd);
                    fpos += fd * (len / fsegs);
                }
                const float ww = half_w
                    * (0.35f + 0.65f * std::sin(std::min(3.1416f * (0.2f + 0.8f * t),
                                                         3.1416f)));
                const float uu = uv.x + (uv.z - uv.x) * (0.06f + 0.88f * t);
                const float drop = ww * fold;
                const uint32_t col = pack({0.3f + 0.7f * t, phase, 0.5f});
                const glm::vec3 nl = safe_normalize(up * 0.8f - side * fold, up);
                const glm::vec3 nr = safe_normalize(up * 0.8f + side * fold, up);
                obj.cards.vertices.push_back(
                    {fpos - side * ww - up * drop, nl,
                     {uu, uv.y + (uv.w - uv.y) * 0.06f}, col});
                obj.cards.vertices.push_back(
                    {fpos, safe_normalize(nl + nr, up), {uu, v_mid}, col});
                obj.cards.vertices.push_back(
                    {fpos + side * ww - up * drop, nr,
                     {uu, uv.y + (uv.w - uv.y) * 0.94f}, col});
            }
            for (int s2 = 0; s2 < fsegs; ++s2) {
                const uint32_t a0 = base_v + static_cast<uint32_t>(s2) * 3u;
                obj.cards.indices.insert(obj.cards.indices.end(),
                                         {a0, a0 + 3u, a0 + 4u, a0, a0 + 4u, a0 + 1u,
                                          a0 + 1u, a0 + 4u, a0 + 5u,
                                          a0 + 1u, a0 + 5u, a0 + 2u});
            }
        }
        // The rootstock nub (same reason as the grass tuft: the scene placer
        // needs a solid footprint).
        tube_segment(obj.wood, glm::vec3{0.0f, -0.02f, 0.0f},
                     glm::vec3{0.0f, 0.08f, 0.0f}, up, 0.03f, 0.02f, 3,
                     pack(glm::vec3{0.25f, 0.20f, 0.12f}));
    } else {
        // MUSHROOMS: stem + cap, both closed volumes (rule 52 has no
        // exemption for a mushroom). A family of 3-5, sizes staggered.
        const uint32_t stem_c = pack(glm::vec3{0.78f, 0.72f, 0.60f});
        const uint32_t cap_c = pack(p.accent);
        const int caps = 3 + static_cast<int>(rng.unit() * 3.0f);
        for (int m = 0; m < caps; ++m) {
            const float az = TAU * rng.unit();
            const float dist = 0.05f + rng.unit() * 0.16f;
            const glm::vec3 at{std::cos(az) * dist, 0.0f, std::sin(az) * dist};
            const float h = p.height * (0.5f + rng.unit() * 0.6f);
            const float cap_r = h * (0.55f + rng.unit() * 0.3f);
            tube_segment(obj.wood, at, at + up * h, up, cap_r * 0.28f, cap_r * 0.22f,
                         4, stem_c);
            // The cap: a squat cone down over the stem head, then a tip cone.
            tube_segment(obj.wood, at + up * (h + cap_r * 0.42f), at + up * (h - cap_r * 0.10f),
                         -up, 0.02f, cap_r, 5, cap_c);
            tube_segment(obj.wood, at + up * (h - cap_r * 0.10f),
                         at + up * (h - cap_r * 0.16f), -up, cap_r, cap_r * 0.5f, 5,
                         cap_c);
        }
    }
    obj.content_hash = object_content_hash(obj);
    return obj;
}

RegistryObject forge_path_light(const PathLightParams& p) {
    RegistryObject obj;
    obj.name = p.name;
    obj.kind = "lamp";
    obj.source = "forge:lamp seed=" + std::to_string(p.seed);
    Rng rng(p.seed * 0x9E3779B97F4A7C15ull + 0xC2B2AE3D27D4EB4Full);
    const glm::vec3 up{0.0f, 1.0f, 0.0f};
    const uint32_t wood_c = pack(p.wood);
    const uint32_t iron_c = pack(p.iron);
    const uint32_t flame_c = pack(p.flame);
    const uint32_t core_c = pack(glm::vec3{1.0f, 0.92f, 0.55f});
    // Everything sits in the WOOD stream (prop program): a lamp must not sway
    // and must not transmit — it is furniture, not foliage.
    if (p.kind == PathLightKind::TorchStake) {
        const float post_h = p.height - 0.42f;
        // Boot at the ground so the stake reads planted, not stuck.
        tube_segment(obj.wood, -up * 0.04f, up * 0.14f, up, 0.065f, 0.048f, 8,
                     wood_c);
        tube_segment(obj.wood, up * 0.14f, up * post_h, up, 0.048f, 0.034f, 8,
                     wood_c);
        // Iron collar under the head keeps the wrap from reading as wood.
        tube_segment(obj.wood, up * (post_h - 0.02f), up * (post_h + 0.045f), up,
                     0.052f, 0.052f, 8, iron_c);
        // The wrapped head: two stacked segments make a slight barrel.
        tube_segment(obj.wood, up * (post_h + 0.045f), up * (post_h + 0.15f), up,
                     0.058f, 0.078f, 8, pack(glm::vec3{0.22f, 0.12f, 0.07f}));
        tube_segment(obj.wood, up * (post_h + 0.15f), up * (post_h + 0.24f), up,
                     0.078f, 0.052f, 8, pack(glm::vec3{0.26f, 0.14f, 0.08f}));
        // FLAME: closed bipyramid, hot core over orange skirt — vertex colour
        // bright enough to read unlit; the real light is the lead's [light].
        const float fb = post_h + 0.24f;
        tube_segment(obj.wood, up * fb, up * (fb + 0.10f), up, 0.020f, 0.055f, 6,
                     flame_c);
        tube_segment(obj.wood, up * (fb + 0.10f), up * (fb + 0.26f), up, 0.055f,
                     0.0f, 6, flame_c);
        tube_segment(obj.wood, up * (fb + 0.04f), up * (fb + 0.15f), up, 0.028f,
                     0.0f, 6, core_c);
    } else {  // LanternPost
        const float post_h = p.height - 0.10f;
        tube_segment(obj.wood, -up * 0.04f, up * 0.16f, up, 0.075f, 0.056f, 8,
                     wood_c);
        tube_segment(obj.wood, up * 0.16f, up * post_h, up, 0.056f, 0.042f, 8,
                     wood_c);
        // Arm reaches out; the lantern hangs from its end on a short link.
        const glm::vec3 arm_dir{1.0f, 0.0f, 0.0f};
        const glm::vec3 arm_root = up * (post_h - 0.06f);
        const glm::vec3 arm_end = arm_root + arm_dir * 0.34f + up * 0.05f;
        tube_segment(obj.wood, arm_root, arm_end, arm_dir, 0.030f, 0.024f, 6,
                     wood_c);
        // Diagonal brace so the arm is built, not glued (rule 52 in spirit).
        tube_segment(obj.wood, up * (post_h - 0.30f), arm_root + arm_dir * 0.22f,
                     arm_dir, 0.018f, 0.014f, 5, wood_c);
        tube_segment(obj.wood, arm_end, arm_end - up * 0.07f, -up, 0.010f, 0.010f,
                     5, iron_c);
        // The lantern box, hung under the arm's end.
        const glm::vec3 c = arm_end - up * 0.07f;
        // top plate, glass barrel (WARM — reads lit even before it is), bottom
        // plate, cap cone with a finial ring.
        tube_segment(obj.wood, c - up * 0.02f, c - up * 0.055f, -up, 0.095f,
                     0.095f, 8, iron_c);
        tube_segment(obj.wood, c - up * 0.055f, c - up * 0.215f, -up, 0.085f,
                     0.085f, 8, pack(glm::vec3{0.98f, 0.82f, 0.42f}));
        tube_segment(obj.wood, c - up * 0.215f, c - up * 0.255f, -up, 0.098f,
                     0.098f, 8, iron_c);
        tube_segment(obj.wood, c - up * 0.255f, c - up * 0.285f, -up, 0.098f,
                     0.040f, 8, iron_c);
        tube_segment(obj.wood, c - up * 0.02f, c + up * 0.045f, up, 0.075f,
                     0.012f, 8, iron_c);
        // Four corner posts of the cage in front of the glass.
        for (int k = 0; k < 4; ++k) {
            const float a = TAU * (static_cast<float>(k) + 0.5f) / 4.0f;
            const glm::vec3 off{std::cos(a) * 0.088f, 0.0f, std::sin(a) * 0.088f};
            tube_segment(obj.wood, c + off - up * 0.055f, c + off - up * 0.215f,
                         -up, 0.008f, 0.008f, 4, iron_c);
        }
    }
    (void)rng;
    obj.content_hash = object_content_hash(obj);
    return obj;
}

} // namespace dfn::render
