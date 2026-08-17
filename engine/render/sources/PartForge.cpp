/*
Created: 16:08:2026 - 20:52:00
Last updated: 17:08:2026 - 17:28:41
Module: engine/render
File: engine/render/sources/PartForge.cpp

Responsibility:
- The building kit's geometry: every PartKind, cut to the size its params ask
  for, as closed volumes on the grid.

Dependencies:
- Uses: PartForge.h, ProcMesh.h (MeshData, tri/quad/pack).
- Used by: tools/forge_parts.cpp, tests/render/PartForgeTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ONE BUILDER, MANY PARTS. Nearly everything here is a hewn bar: a chamfered
  prism swept along an axis. A beam, a post, a plank, a stair's stringer and a
  wall's studs are that same bar at different sizes and angles. Adding a
  thirteenth part should mean a new arrangement of bars, not a new mesher.
- EVERY SIZE COMES FROM `_u` GRID UNITS, never from metres typed in place. The
  snapping promise in the header is only true while that holds: a part whose
  length is 1.37 m cannot be counted in by an agent placing on a 0.25 m grid.
- WEAR IS GEOMETRY, NOT A TINT. The reference frames are hand-hewn wood whose
  faces are not parallel and whose ends are split. Wear wobbles the section
  ring and jitters per-face colour; a part at wear 0 is a sawn plank and reads
  like one, which is why the kit ships both.
*/
/*
UPD:
- 16:08:2026 - 20:52:00: Создан вместе с PartForge.h.
- 16:08:2026 - 22:16:30: ЗАМКНУТАЯ ОБОЛОЧКА (пользователь по хутору: «дырки в доме, стены
  несплошные, окна глухие с имитацией вида насквозь») — механизм, не
  экземпляры: (1) дощатая стена получила сплошную подложку за досками, а весь
  пролёт вынесен к НАРУЖНОЙ плоскости рамы — утопленный пролёт читался чёрным
  провалом даже там, где дырки не было; (2) фронтон — замкнутая треугольная
  призма-ядро точно по линиям ската, ступени досок больше не сквозят; (3) окно
  — глухая тёпло-тёмная вставка PartMaterial::Pane за переплётом, с нахлёстом
  на раму; (4) цоколь — сплошная плита внутри кладки, швы камней остались
  тенями и перестали быть отверстиями; (5) дверное полотно — сплошная
  подложка с тыльной стороны. Мера — прибор dfn_scene_check --shell: до
  правки 216/18842 лучей наружу на хуторе.
- 16:08:2026 - 22:28:22: Мешер бруса (Rng/Material/tone/hewn_bar/block) вынесен ДОСЛОВНО
  в HewnBar.h — у кузницы утвари (PropForge) те же брусья, а вторая копия
  мешера — правило 39. Контроль: перепечка набора после выноса не изменила
  ни одного .dfo (git diff пуст) — геометрия та же до байта.
- 16:08:2026 - 22:40:03: ВТОРАЯ ПОЛОВИНА дырок — тёмный провал (лид, пункт 2: «дыра — это
  и то, что ЧИТАЕТСЯ как отверстие»). Найдено прибором measure_bay_contrast:
  стеновая панель была ОДНОСТОРОННЕЙ, и восточные стены хутора показывали
  улице голое тёмное ядро (yaw −90° разворачивает «наружную» плоскость
  внутрь), а подложка дверного полотна легла с УЛИЧНОЙ стороны досок. Теперь:
  доски двумя кожами у ОБЕИХ плоскостей стены (стену видно с улицы и из
  комнаты — интерьеры 1к1), ядро между ними; штукатурка — плита во всю
  толщину; подложка двери — со стороны накладок. Тёмные доски подмешивают
  0.60 тона рамы и несут широкий разброс: светлая рама вокруг ровно-тёмного
  поля — графический признак ОТВЕРСТИЯ. Числа (кадр восточной стены, обе
  руки из одного кадра): провал был 0.357 от рамы (до запечатывания 0.555),
  принятый пролёт 0.606; пол приёмки 0.50; стало 0.798 при принятом 0.945,
  оболочка осталась 0/18842.
- 17:08:2026 - 12:38:26: СЕМЬЯ СОЕДИНИТЕЛЕЙ (HOUSES.md §3-4): make_joint (призма
  4/6/8/24 граней, БЕЗ прогиба и БЕЗ конусности — грань обязана быть
  плоскостью, на неё панель садится заподлицо; ориентация граней — контракт с
  судьёй: при yaw 0 нормаль первой грани смотрит на +X), make_sleeper (лежень,
  у n4 плоская постель и плоское ложе), make_log_corner (перевязка торцов, шаг
  венца 0.23 — ритм срубной панели). Каталог: 384 стойки + 72 лежня + 8 углов.
  Имена несут рабочие свойства: joint-stone-d50-n8-h11-cap-w03.
- 17:08:2026 - 13:02:52: РАЗРЕЗ ПО СЕМЬЯМ (999 строк против предела 800, правило 21):
  кухня (material_of, m_of, tone, block, общие константы) — в
  PartForgeDetail.h; соединители — в PartForgeJoints.cpp ДОСЛОВНО; стены со
  стилями — PartForgeWalls.cpp. Контроль переноса: перепечка набора не
  изменила ни одного .dfo (git status по parts пуст). Материал Clay (глина по
  каркасу) — под волну вариантов стен.
- 17:08:2026 - 13:18:48: волна вариантов стен: диспетчер WallPanel по variant (0 —
  прежний пролёт с прежним именем байт-в-байт, иначе make_wall_styled),
  жетоны wall_style_token/opening_token в имени. kit_catalogue() вынесен
  ДОСЛОВНО в PartForgeCatalogue.cpp (файл снова упёрся в предел 800).
- 17:08:2026 - 13:23:56: волна крыш: make_roof перенесён дословно в PartForgeRoofs.cpp и
  расширен там; здесь — диспетчер RoofHip/SmokeVent и их имена (полувальма
  обязана нести -polu, иначе два варианта дерутся за одно имя файла).
- 17:08:2026 - 13:46:59: КРУТАЯ ЛЕСТНИЦА (пользователь: «более крутые, чтобы на второй этаж за
  длину этих доводили»): Stair variant 1 — проступь 1u при подъёме 1u = 45°,
  марш из N ступеней достигает N·0.25 м за N·0.25 м плана. Подъём НЕ трогали
  и не тронем: PLAYER_STEP_HEIGHT 0.35 — ступень 0.25 проходима, 0.50 нет,
  крутизна берётся только укорочением проступи. Имя несёт -steep; variant 0
  печётся байт-в-байт как раньше (going тот же m_of(2)).
- 17:08:2026 - 14:29:43: ТЕКСТУРЫ ДЕТАЛЯМ (заказ 17.08, п.3): skin_of() — таблица «материал ->
  колонка поверхности + ряд тона/износа», и forge_part кладёт геометрию в
  ПОТОК bark (текстурный канал .dfo; приложение уже кормит им и программу
  листвы, и треугольную коллизию для kind == "part", так что текстура не
  делает дом проходимым). Цветовая таблица материалов НЕ умерла — она
  контрольная рука и одновременно эталон: тайл, в который попадает материал,
  усредняется ровно в её цвет, то есть текстура меняет ПОВЕРХНОСТЬ, а не
  палитру принятой витрины. Геометрия не тронута: замкнутость, объём,
  сплошность стен и run/rise лестниц зелены теми же измерителями.
- 17:08:2026 - 14:56:52: ТЕКСТУРНАЯ ПЕЧЬ ДЕТАЛЕЙ ВЫКЛЮЧЕНА ПО УМОЛЧАНИЮ до подключения листа.
  Лист набора есть и геометрия несёт номера тайлов, но привязка листа к дому
  ещё не в дереве — а до неё текстурная деталь едет по пути ЛИСТВЫ и берёт
  тайлы из атласа листьев. Пользователь увидел это первым и назвал точно:
  «сейчас всё из листочков и текстур дерева сделано... надо вернуть прошлое
  состояние». Дверь DFN_PARTS_TEXTURED=1 включает новую форму тому, кто
  сажает привязку, и заодно делает две формы контрольной парой из одного
  бинарника. Умолчание перевернуть В ТОТ ЖЕ ДЕНЬ, когда лист привязан, и ни
  днём раньше: наполовину посаженная фича — не фича, а регрессия с планом.
- 17:08:2026 - 15:46:07: текстурность стала свойством ДЕТАЛИ (kit_textured_default() — одно
  определение умолчания). Была процессная дверь, читаемая внутри кузницы, и
  тест текстурного потока не мог её попросить: он проверял умолчание и
  покраснел в день, когда умолчание сменилось. Полка байт в байт прежняя.
- 17:08:2026 - 17:28:41: make_deck() и имя настила. Проём ОБВЯЗЫВАЕТСЯ четырьмя брусьями —
  обвязка не отделка, это геометрия, которую видят и судья, и тело
  столкновений, чего вычитание не даёт. Сплошной настил не пишет
  `-hole0x0x0x0-`: отсутствие объявления и объявление нуля не должны
  выглядеть одинаково.
*/

#include "engine/render/sources/PartForge.h"

#include "engine/render/sources/HewnBar.h"
#include "engine/render/sources/PartForgeDetail.h"
#include "engine/render/sources/ProcMesh.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <initializer_list>
#include <vector>

namespace dfn::render {

// The forge's kitchen — material table, helpers, cross-TU make_* — lives in
// PartForgeDetail.h since the family split (Rule 21: 999 lines against the
// 800 hard limit). THE material table is defined here, once (Rule 39).
bool kit_textured_default() {
    // Read ONCE: a door that changed mid-bake would give one shelf two forms.
    static const bool on = [] {
        const char* v = std::getenv("DFN_PARTS_TEXTURED");
        return v != nullptr && *v != '\0' && *v != '0';
    }();
    return on;
}

namespace part_detail {

/// WHICH TILE A MATERIAL WEARS. The column is the SURFACE (how the material is
/// worked); the row is the TONE AND THE WEAR together. Two materials share a
/// column whenever their only difference is colour — brick and roof tile are
/// both fired clay, plaster and daub are both a trowelled skin — because a
/// column costs a whole row of tiles and a row costs one.
[[nodiscard]] PartSkin skin_of(PartMaterial m, float wear, bool textured) {
    // 0.55 is the fence between the catalogue's own two wear steps (0.3 and
    // 0.8): both sit clear of it, so no part lands on the fence and the row a
    // part gets is not decided by a rounding.
    const bool worn = wear >= 0.55f;
    PartSkin s;
    // TEXTURED ONLY WHEN THE RENDERER CAN DRAW IT. The parts sheet exists and
    // the geometry carries its tile numbers, but the binding that hands the
    // sheet to a house is not in yet — and until it is, a textured part rides
    // the FOLIAGE path and samples the LEAF atlas. The user saw exactly that
    // and named it exactly: «сейчас всё из листочков и текстур дерева сделано».
    //
    // So the switch defaults to OFF and the kit bakes the flat, vertex-coloured
    // form it has always had. DFN_PARTS_TEXTURED=1 turns it on for whoever is
    // landing the binding, which also makes the two forms a control pair out of
    // one binary (Rule 47). Flip the default the day the sheet is bound — and
    // not a day earlier, because a half-landed feature is not a feature, it is
    // a regression with a plan attached.
    s.textured = textured;
    s.span_m = PARTS_TILE_SPAN_M;
    const auto set = [&s](PartSurface side, PartTone tone, PartSurface end,
                          PartTone end_tone) {
        s.side = side;
        s.side_tone = tone;
        s.end = end;
        s.end_tone = end_tone;
    };
    switch (m) {
    case PartMaterial::Timber:
        set(PartSurface::HewnTimber, worn ? PartTone::Weathered : PartTone::Mid,
            PartSurface::EndGrain, worn ? PartTone::Weathered : PartTone::Mid);
        break;
    case PartMaterial::TimberDark:
        set(PartSurface::HewnTimber, PartTone::Dark, PartSurface::EndGrain,
            PartTone::Dark);
        break;
    case PartMaterial::Plaster:
        set(PartSurface::Plaster, worn ? PartTone::Weathered : PartTone::Light,
            PartSurface::Plaster, worn ? PartTone::Weathered : PartTone::Light);
        break;
    case PartMaterial::Clay:
        set(PartSurface::Plaster, worn ? PartTone::Weathered : PartTone::Mid,
            PartSurface::Plaster, worn ? PartTone::Weathered : PartTone::Mid);
        break;
    case PartMaterial::Stone:
        set(PartSurface::Stone, worn ? PartTone::Weathered : PartTone::Mid,
            PartSurface::Stone, worn ? PartTone::Weathered : PartTone::Mid);
        break;
    case PartMaterial::Brick:
        set(PartSurface::FiredClay, worn ? PartTone::Weathered : PartTone::Mid,
            PartSurface::FiredClay, worn ? PartTone::Weathered : PartTone::Mid);
        break;
    case PartMaterial::Tile:
        set(PartSurface::FiredClay, worn ? PartTone::Weathered : PartTone::Light,
            PartSurface::FiredClay, worn ? PartTone::Weathered : PartTone::Light);
        break;
    case PartMaterial::Thatch:
        set(PartSurface::Thatch, worn ? PartTone::Weathered : PartTone::Light,
            PartSurface::Thatch, worn ? PartTone::Weathered : PartTone::Light);
        break;
    case PartMaterial::Shingle:
        // Дранка is SPLIT wood, so it wears the sawn column and shows a wood
        // end — a shingle's butt is exactly what the eye reads on a roof.
        set(PartSurface::SawnBoard, worn ? PartTone::Weathered : PartTone::Dark,
            PartSurface::EndGrain, worn ? PartTone::Weathered : PartTone::Dark);
        break;
    case PartMaterial::Turf:
        set(PartSurface::Turf, worn ? PartTone::Weathered : PartTone::Mid,
            PartSurface::Turf, worn ? PartTone::Weathered : PartTone::Mid);
        break;
    case PartMaterial::Pane:
        set(PartSurface::Pane, PartTone::Mid, PartSurface::Pane, PartTone::Mid);
        break;
    }
    return s;
}

Material material_of(PartMaterial m, float wear, bool textured) {
    Material out = [m]() -> Material {
    switch (m) {
    // Weathered oak: the reference's timber is grey first and brown second.
    case PartMaterial::Timber: return {{0.44f, 0.37f, 0.29f}, 0.16f, 0.22f, 0.012f};
    case PartMaterial::TimberDark: return {{0.25f, 0.21f, 0.18f}, 0.14f, 0.22f, 0.012f};
    // Infill plaster is flat and pale; it does not wander, it cracks.
    case PartMaterial::Plaster: return {{0.71f, 0.67f, 0.57f}, 0.07f, 0.05f, 0.003f};
    // Field stone: the strongest wobble in the kit, because a footing course
    // that is a smooth box is the one thing that always reads as programmer art.
    case PartMaterial::Stone: return {{0.45f, 0.45f, 0.43f}, 0.13f, 0.30f, 0.030f};
    case PartMaterial::Thatch: return {{0.66f, 0.56f, 0.33f}, 0.18f, 0.10f, 0.020f};
    case PartMaterial::Shingle: return {{0.33f, 0.30f, 0.27f}, 0.15f, 0.08f, 0.006f};
    // The blind pane: dark and WARM — firelight-side-of-black, not void-black.
    // It imitates an interior; a neutral dark grey reads as a hole, which is
    // the exact complaint this material exists to close.
    case PartMaterial::Pane: return {{0.15f, 0.10f, 0.055f}, 0.10f, 0.0f, 0.0f};
    // Fired clay, weathered toward brown; laid in courses, so its read comes
    // from per-band tone, not from wobble.
    case PartMaterial::Brick: return {{0.42f, 0.24f, 0.17f}, 0.12f, 0.0f, 0.004f};
    case PartMaterial::Tile: return {{0.48f, 0.26f, 0.18f}, 0.14f, 0.06f, 0.005f};
    // Living turf: the greens sit close to the ground tufts', so a turf roof
    // belongs to the same world as the grass below it.
    case PartMaterial::Turf: return {{0.30f, 0.38f, 0.18f}, 0.16f, 0.12f, 0.030f};
    // Глина по каркасу: warm smoothed earth, flatter than plaster, browner.
    case PartMaterial::Clay: return {{0.60f, 0.50f, 0.38f}, 0.08f, 0.04f, 0.006f};
    }
    return {{0.5f, 0.5f, 0.5f}, 0.1f, 0.1f, 0.0f};
    }();
    // THE COLOUR TABLE ABOVE IS NOT DEAD once a part is textured: it is the
    // control arm. `skin.textured = false` reproduces the untextured kit
    // byte-for-byte out of THIS binary (Rule 47), and the atlas tile a
    // material maps to averages to this very colour — texturing changes the
    // surface, not the palette the showcase was accepted with.
    out.skin = skin_of(m, wear, textured);
    return out;
}

} // namespace part_detail

namespace {

// The short local names keep every make_* below reading as it always did.
using part_detail::Material;
using part_detail::Rng;
using part_detail::m_of;
using part_detail::material_of;
using part_detail::tone;
using part_detail::BOARD_GAP_M;
using part_detail::BOARD_W_M;
using part_detail::INFILL_THICK_M;
using part_detail::PANE_THICK_M;
using part_detail::WALL_CORE_M;

// This family's own dimensions, in metres. Proportions of a part, not world
// tuning: they decide what a board looks like, and every one of them was set
// against the reference frames in images_examples/houses_outdoors.
constexpr float PLANK_THICK_M = 0.06f;   ///< a sawn board
constexpr float LEAF_THICK_M = 0.07f;    ///< a door leaf
constexpr float STONE_W_M = 0.55f;       ///< a field stone in a footing course
constexpr float TREAD_M = 0.08f;
constexpr float NOSING_M = 0.04f;        ///< tread overhang; the step's shadow
/// A stair's rise and going, IN GRID UNITS: 0.25 m up per 0.50 m along, which
/// is 26.5 degrees. Both are integers on purpose — a flight of N steps then
/// lands exactly N units up and 2N along, so the deck it reaches can be
/// counted to rather than measured.
constexpr int STAIR_RISE_U = 1;
constexpr int STAIR_GOING_U = 2;
/// THE STEEP FLIGHT's going (Stair variant 1): 1u per 1u rise = 45°, the
/// indoor stair that reaches the next floor within its own run (user, 17.08:
/// «более крутые, чтобы на второй этаж за длину этих доводили»). The RISE
/// stays 1u for BOTH pitches, and that is a controller constraint rather than
/// taste: PLAYER_STEP_HEIGHT is 0.35 m, so a 0.25 riser is climbed and a 2u
/// riser (0.50) NEVER is — a flight made steep by doubling the rise would be
/// scenery no player can ascend. Steepness may only come from shortening the
/// going.
constexpr int STAIR_STEEP_GOING_U = 1;

// tone() and block() come from the kitchen (PartForgeDetail.h), same bodies
// they always had.
using part_detail::block;

// ---------------------------------------------------------------------------
// The parts. Each writes into `wood` (drawn with the prop program) and returns
// with its ORIGIN at the joint a composer places by: a beam's near end, a
// post's foot, a stair's bottom step, a roof's eaves.
// ---------------------------------------------------------------------------

void make_beam(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float len = m_of(p.length_u);
    hewn_bar(m, {0.0f, m_of(p.height_u) * 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f},
             {0.0f, 1.0f, 0.0f}, len, m_of(p.width_u) * 0.5f, m_of(p.height_u) * 0.5f,
             mat, p.wear, rng, std::max(2, p.length_u / 3));
}

void make_post(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    // Posts taper: every standing timber in the reference is a trunk, thicker
    // at the butt. A post with parallel sides is the thing that reads as CAD.
    const float taper = 1.0f - 0.10f * p.wear;
    hewn_bar(m, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
             m_of(p.length_u), m_of(p.width_u) * 0.5f, m_of(p.height_u) * 0.5f, mat,
             p.wear, rng, std::max(2, p.length_u / 3), taper);
}

void make_plank(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    Material sawn = mat;
    sawn.chamfer = 0.0f; // a board has square arrises; that is what makes it a board
    // Origin at the board's UNDERSIDE, not its middle: every part in this kit
    // is placed by the face it rests on, or a composer would have to know each
    // part's thickness to stack anything.
    hewn_bar(m, {0.0f, PLANK_THICK_M * 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f},
             {0.0f, 1.0f, 0.0f}, m_of(p.length_u), m_of(p.width_u) * 0.5f,
             PLANK_THICK_M * 0.5f, sawn, p.wear, rng, std::max(2, p.length_u / 4));
}

/// НАСТИЛ С ОБЪЯВЛЕННЫМ ПРОЁМОМ (HOUSES.md §9). Boards run along +X across a
/// width of +Z, origin at the corner and on the UNDERSIDE like every other
/// part of this kit, so a composer stacks it without knowing its thickness.
///
/// THE OPENING IS FRAMED, and the frame is the whole point rather than trim.
/// A void made by not laying a board is byte-for-byte the same thing as a
/// board somebody forgot — that is the defect this project already met — so
/// this part TRIMS the void with four beams: the two headers across the boards
/// and the two trimmers along them. A framed opening reads as built from
/// inside the game and, more importantly, the frame is geometry the judge and
/// the collision body can both see, which a subtraction is not.
void make_deck(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    Material sawn = mat;
    sawn.chamfer = 0.0f; // sawn boards have square arrises
    const float L = m_of(p.length_u);
    const float W = m_of(p.width_u);
    const float T = m_of(p.height_u);
    const bool holed = p.void_l_u > 0 && p.void_w_u > 0;
    const float hx0 = m_of(p.void_x_u);
    const float hx1 = hx0 + m_of(p.void_l_u);
    const float hz0 = m_of(p.void_z_u);
    const float hz1 = hz0 + m_of(p.void_w_u);
    const Material frame = material_of(PartMaterial::Timber, p.wear, p.textured);
    // Board width follows the shelf's plank, so a deck reads as the same timber
    // the rest of the kit is sawn from rather than as a slab with lines on it.
    const int boards = std::max(2, static_cast<int>(W / 0.25f + 0.5f));
    const float bw = W / static_cast<float>(boards);
    const float trim = std::min(m_of(1), T);   // the framing member's section
    for (int i = 0; i < boards; ++i) {
        const float z0 = static_cast<float>(i) * bw;
        const float zc = z0 + bw * 0.5f;
        // A board crossing the void is CUT at the headers, not omitted: the
        // stub each side of the opening is what carries the header.
        const bool crosses = holed && zc > hz0 && zc < hz1;
        const float runs[2][2] = {{0.0f, crosses ? hx0 : L}, {hx1, L}};
        for (int seg = 0; seg < (crosses ? 2 : 1); ++seg) {
            const float a = runs[seg][0];
            const float b = runs[seg][1];
            if (b - a < 0.01f) {
                continue; // the void reaches this edge: no stub to lay
            }
            hewn_bar(m, {a, T * 0.5f, zc}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
                     b - a, bw * 0.47f, T * 0.5f, sawn, p.wear, rng,
                     std::max(2, static_cast<int>((b - a) / 1.0f)));
        }
    }
    if (!holed) {
        return;
    }
    // ОБВЯЗКА ПРОЁМА: two headers across the boards, two trimmers along them.
    for (const float x : {hx0, hx1}) {
        if (x <= 0.01f || x >= L - 0.01f) {
            continue; // the void runs out to this edge; nothing to head off
        }
        hewn_bar(m, {x, T * 0.5f, hz0}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f},
                 hz1 - hz0, trim * 0.5f, T * 0.5f, frame, p.wear, rng, 2);
    }
    for (const float z : {hz0, hz1}) {
        if (z <= 0.01f || z >= W - 0.01f) {
            continue;
        }
        hewn_bar(m, {hx0, T * 0.5f, z}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
                 hx1 - hx0, trim * 0.5f, T * 0.5f, frame, p.wear, rng, 2);
    }
}

/// A wall bay: sill, head, two studs, and the infill between them. Timber
/// infill is BOARDS (each its own tone, each its own tiny gap) because a wall
/// drawn as one slab is the "cartoon" failure the tree work already paid for.
void make_wall(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float w = m_of(p.length_u);
    const float h = m_of(p.height_u);
    const float t = m_of(p.width_u);
    const Material frame = material_of(PartMaterial::Timber, p.wear, p.textured);
    const float fs = std::min(m_of(2), w * 0.25f); // frame member size

    // Frame first, so a wall reads as timber-frame even in silhouette.
    hewn_bar(m, {fs * 0.5f, fs * 0.5f, t * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
             h - fs, fs * 0.5f, t * 0.5f, frame, p.wear, rng, 3);
    hewn_bar(m, {w - fs * 0.5f, fs * 0.5f, t * 0.5f}, {0.0f, 1.0f, 0.0f},
             {0.0f, 0.0f, 1.0f}, h - fs, fs * 0.5f, t * 0.5f, frame, p.wear, rng, 3);
    hewn_bar(m, {0.0f, fs * 0.5f, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, w,
             t * 0.5f, fs * 0.5f, frame, p.wear, rng, 3);
    hewn_bar(m, {0.0f, h - fs * 0.5f, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
             w, t * 0.5f, fs * 0.5f, frame, p.wear, rng, 3);

    const float in_w = w - 2.0f * fs;
    const float in_h = h - 2.0f * fs;
    if (in_w <= 0.0f || in_h <= 0.0f) {
        return;
    }
    // A WALL IS SEEN FROM BOTH SIDES — the street and the room (interiors are
    // built 1:1 inside the same box), and half the placements in a real scene
    // turn the part's back to the camera (yaw flips which face is "outer").
    // A one-sided bay was measured doing exactly that: the farmhouse's east
    // walls showed their naked dark core to the street and read as pits while
    // the boards faced the furniture.
    //
    // Solid infill or boarded, decided by the MATERIAL and not by comparing
    // colours: two materials may one day share a tone, and a wall that quietly
    // changed construction because of that would be very hard to explain.
    if (p.material == PartMaterial::Plaster || p.material == PartMaterial::Stone) {
        // Full-depth slab, faces just shy of both wall planes: plaster set
        // into its frame, readable from either side, no through gap.
        block(m, {fs, fs, 0.02f}, {in_w, in_h, t - 0.04f}, mat, p.wear, rng, 2);
        return;
    }
    // THE SEALED CORE, Rule 52 + the zone's sealed-hull rule: a closed slab
    // BETWEEN the two board skins, so the shadow gap between two boards stays
    // a shadow and stops being a through-hole. Its tone is the wall's own,
    // darkened — what the eye reads in a real board gap is dark depth.
    Material core = mat;
    core.color *= 0.55f;
    core.wobble = 0.0f;
    block(m, {fs, fs, t * 0.5f - WALL_CORE_M * 0.5f},
          {in_w, in_h, WALL_CORE_M}, core, p.wear * 0.5f, rng, 2);
    // THE DARK-PIT HALF of the user's complaint: a bay much darker than its
    // own frame reads as an opening before the eye finds the boards — a light
    // frame around a flat dark field is the graphic signature of a HOLE. So
    // infill boards lean toward the frame's tone (measured on the farmhouse
    // east wall: pure TimberDark sat at 0.36 of the frame's luminance, the
    // accepted timber bay at 0.61, acceptance floor 0.50) and carry a wider
    // per-board spread so the field never reads as one flat pit.
    Material bmat = mat;
    bmat.color = glm::mix(mat.color, frame.color, 0.60f);
    bmat.jitter = mat.jitter * 1.6f;
    const int boards = std::max(2, static_cast<int>(in_w / BOARD_W_M + 0.5f));
    const float bw = in_w / static_cast<float>(boards);
    for (int i = 0; i < boards; ++i) {
        const float x = fs + static_cast<float>(i) * bw;
        const float gap = BOARD_GAP_M * (0.3f + 0.7f * p.wear);
        // One skin at each wall plane; the same board pattern on both, the
        // way a clad timber wall is actually built.
        hewn_bar(m, {x + bw * 0.5f, fs, t - INFILL_THICK_M * 0.5f}, {0.0f, 1.0f, 0.0f},
                 {0.0f, 0.0f, 1.0f}, in_h, (bw - gap) * 0.5f, INFILL_THICK_M * 0.5f,
                 bmat, p.wear, rng, 2);
        hewn_bar(m, {x + bw * 0.5f, fs, INFILL_THICK_M * 0.5f}, {0.0f, 1.0f, 0.0f},
                 {0.0f, 0.0f, 1.0f}, in_h, (bw - gap) * 0.5f, INFILL_THICK_M * 0.5f,
                 bmat, p.wear, rng, 2);
    }
}

/// The triangular end wall. A closed prism (both faces, three sides), boarded
/// vertically like the reference's gables, with a collar beam across it.
void make_gable(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float w = m_of(p.length_u);
    const float rise = m_of(p.height_u);
    const float t = m_of(p.width_u);
    // THE SEALED CORE: a closed triangular prism filling the gable exactly to
    // its rake lines. The boards in front are cut in whole-board steps, so
    // between each board's top and the roof line there was a triangular
    // through-hole (the user's farmhouse, hole class #3); the core is what the
    // eye now finds there — infill, not sky.
    {
        Material core = mat;
        core.color *= 0.75f;
        core.wobble = 0.0f;
        const float z0 = t * 0.30f;
        const float z1 = t * 0.70f;
        const glm::vec3 a0{0.0f, 0.0f, z0};
        const glm::vec3 b0{w, 0.0f, z0};
        const glm::vec3 c0{w * 0.5f, rise, z0};
        const glm::vec3 a1{0.0f, 0.0f, z1};
        const glm::vec3 b1{w, 0.0f, z1};
        const glm::vec3 c1{w * 0.5f, rise, z1};
        const uint32_t tn = tone(core, p.wear * 0.5f, rng);
        tri(m, a1, b1, c1, tn);            // front, +z
        tri(m, a0, c0, b0, tn);            // back, -z
        quad(m, a0, b0, b1, a1, tn);       // underside
        quad(m, a0, a1, c1, c0, tn);       // left rake
        quad(m, b0, c0, c1, b1, tn);       // right rake
    }
    const int boards = std::max(3, static_cast<int>(w / BOARD_W_M + 0.5f));
    const float bw = w / static_cast<float>(boards);
    for (int i = 0; i < boards; ++i) {
        const float x0 = static_cast<float>(i) * bw;
        const float xc = x0 + bw * 0.5f;
        // Each board is cut to the roof line it meets — the pitch is the part.
        const float frac = 1.0f - std::fabs(xc - w * 0.5f) / (w * 0.5f);
        const float bh = std::max(rise * frac, BUILD_GRID_M);
        hewn_bar(m, {xc, 0.0f, t * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, bh,
                 (bw - BOARD_GAP_M) * 0.5f, t * 0.5f, mat, p.wear, rng, 2);
    }
    const Material frame = material_of(PartMaterial::Timber, p.wear, p.textured);
    const float fs = m_of(1);
    // Collar beam at a third of the rise: the horizontal timber that makes a
    // Nordic gable read as a gable and not as a pile of boards.
    hewn_bar(m, {0.0f, rise * 0.33f, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
             w * 0.66f + w * 0.17f, t * 0.6f, fs * 0.5f, frame, p.wear, rng, 3);
    // The two barge boards along the rake, which is where the carved dragon
    // heads in the reference frames live.
    for (int s = 0; s < 2; ++s) {
        const glm::vec3 foot{s == 0 ? 0.0f : w, 0.0f, t * 0.5f};
        const glm::vec3 ridge{w * 0.5f, rise, t * 0.5f};
        const glm::vec3 along = ridge - foot;
        hewn_bar(m, foot, along, {0.0f, 0.0f, 1.0f}, glm::length(along), fs * 0.5f,
                 t * 0.55f, frame, p.wear, rng, 3);
    }
}

/// A flight, origin at the foot of the lowest riser. Rise is one grid unit
/// ALWAYS; the going picks the flight's duty — 2u = 26.5° street-and-terrace
/// stair, 1u (variant 1, `-steep` in the name) = 45° house stair that lands
/// on the next floor within its own run. Either way a flight of N steps lands
/// EXACTLY N units up and N·going units along, so the deck it reaches can be
/// counted to rather than measured.
void make_stair(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const int steps = std::max(1, p.height_u);
    const float w = m_of(p.width_u);
    const float rise = m_of(STAIR_RISE_U);
    const float going = m_of(p.variant == 1 ? STAIR_STEEP_GOING_U : STAIR_GOING_U);
    for (int i = 0; i < steps; ++i) {
        const float y = static_cast<float>(i) * rise;
        const float x = static_cast<float>(i) * going;
        // Tread overhangs its riser: the nosing shadow is how a stair reads as
        // steps rather than as a ramp with lines on it.
        hewn_bar(m, {x - NOSING_M, y + rise - TREAD_M * 0.5f, w * 0.5f},
                 {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, going + NOSING_M, w * 0.5f,
                 TREAD_M * 0.5f, mat, p.wear, rng, 2);
        // The riser box under it, so the flight is solid and not floating slabs.
        block(m, {x, y, 0.0f}, {going, rise - TREAD_M, w}, mat, p.wear * 0.6f, rng);
    }
    // Two stringers along the flight's diagonal.
    const glm::vec3 along = glm::normalize(
        glm::vec3{going * static_cast<float>(steps), rise * static_cast<float>(steps), 0.0f});
    const float slen = std::sqrt(std::pow(going * static_cast<float>(steps), 2.0f)
                                 + std::pow(rise * static_cast<float>(steps), 2.0f));
    const Material frame = material_of(PartMaterial::Timber, p.wear, p.textured);
    for (int s = 0; s < 2; ++s) {
        const float z = s == 0 ? m_of(1) * 0.5f : w - m_of(1) * 0.5f;
        hewn_bar(m, {0.0f, 0.0f, z}, along, {0.0f, 0.0f, 1.0f}, slen, m_of(1) * 0.5f,
                 m_of(1) * 0.5f, frame, p.wear, rng, std::max(2, steps));
    }
}

/// Jambs, lintel and threshold around an opening `length_u` wide and
/// `height_u` tall. The opening is EMPTY: the leaf is its own part, because a
/// door that cannot be left open is scenery.
void make_door_frame(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float w = m_of(p.length_u);
    const float h = m_of(p.height_u);
    const float t = m_of(p.width_u);
    const float j = m_of(1);
    hewn_bar(m, {-j * 0.5f, 0.0f, t * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, h,
             j * 0.5f, t * 0.5f, mat, p.wear, rng, 3);
    hewn_bar(m, {w + j * 0.5f, 0.0f, t * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
             h, j * 0.5f, t * 0.5f, mat, p.wear, rng, 3);
    hewn_bar(m, {-j, h + j * 0.5f, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
             w + 2.0f * j, t * 0.5f, j * 0.5f, mat, p.wear, rng, 3);
    hewn_bar(m, {-j, -j * 0.5f, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
             w + 2.0f * j, t * 0.5f, j * 0.5f, mat, p.wear, rng, 2);
}

/// Vertical boards on two ledgers, plus a diagonal brace: the plank door of
/// every frame the user supplied.
void make_door_leaf(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float w = m_of(p.length_u);
    const float h = m_of(p.height_u);
    // THE SEALED CORE: board gaps on a door are through-holes too. A closed
    // backing plate on the LEDGER side (between boards and ledgers, hidden by
    // both) keeps the plank read on the street face and the hull sealed. It
    // was first placed at z<0 — which is the STREET side of this part — and
    // hid the whole plank front behind a flat dark plate.
    {
        Material core = mat;
        core.color *= 0.6f;
        core.wobble = 0.0f;
        block(m, {0.0f, 0.0f, LEAF_THICK_M}, {w, h, 0.03f}, core, p.wear * 0.5f, rng, 1);
    }
    const int boards = std::max(2, static_cast<int>(w / BOARD_W_M + 0.5f));
    const float bw = w / static_cast<float>(boards);
    for (int i = 0; i < boards; ++i) {
        hewn_bar(m, {(static_cast<float>(i) + 0.5f) * bw, 0.0f, LEAF_THICK_M * 0.5f},
                 {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, h, (bw - BOARD_GAP_M) * 0.5f,
                 LEAF_THICK_M * 0.5f, mat, p.wear, rng, 2);
    }
    const Material iron = material_of(PartMaterial::TimberDark, p.wear, p.textured);
    for (int s = 0; s < 2; ++s) {
        const float y = s == 0 ? h * 0.18f : h * 0.80f;
        hewn_bar(m, {0.0f, y, LEAF_THICK_M}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, w,
                 LEAF_THICK_M * 0.6f, m_of(1) * 0.35f, iron, p.wear, rng, 2);
    }
    const glm::vec3 a{0.0f, h * 0.18f, LEAF_THICK_M};
    const glm::vec3 b{w, h * 0.80f, LEAF_THICK_M};
    hewn_bar(m, a, b - a, {0.0f, 0.0f, 1.0f}, glm::length(b - a), LEAF_THICK_M * 0.6f,
             m_of(1) * 0.30f, iron, p.wear, rng, 3);
}

/// Frame plus a cross of mullions — the small many-paned window in the
/// reference's gable.
void make_window(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float w = m_of(p.length_u);
    const float h = m_of(p.height_u);
    const float t = m_of(p.width_u);
    const float j = m_of(1) * 0.7f;
    hewn_bar(m, {0.0f, 0.0f, t * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, h,
             j * 0.5f, t * 0.5f, mat, p.wear, rng, 2);
    hewn_bar(m, {w, 0.0f, t * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, h, j * 0.5f,
             t * 0.5f, mat, p.wear, rng, 2);
    hewn_bar(m, {0.0f, 0.0f, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, w,
             t * 0.5f, j * 0.5f, mat, p.wear, rng, 2);
    hewn_bar(m, {0.0f, h, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, w, t * 0.5f,
             j * 0.5f, mat, p.wear, rng, 2);
    hewn_bar(m, {w * 0.5f, 0.0f, t * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, h,
             j * 0.3f, t * 0.4f, mat, p.wear, rng, 2);
    hewn_bar(m, {0.0f, h * 0.5f, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, w,
             t * 0.4f, j * 0.3f, mat, p.wear, rng, 2);
    // THE BLIND PANE (user: «окна глухие... с имитацией вида насквозь»): a
    // closed warm-dark insert behind the mullions, overlapping the frame all
    // round, so the window has an inside without having a hole. Its material
    // is Pane on purpose — findable, replaceable when real interiors arrive.
    block(m, {-j * 0.25f, -j * 0.25f, t * 0.30f - PANE_THICK_M * 0.5f},
          {w + j * 0.5f, h + j * 0.5f, PANE_THICK_M},
          material_of(PartMaterial::Pane, p.wear, p.textured), p.wear * 0.3f, rng, 1);
}

/// A course of field stones, origin at its near-bottom-left. Individual
/// stones, not one long block: the reference's dry-stone bases are read
/// entirely by the shadows between stones.
void make_footing(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float len = m_of(p.length_u);
    const float h = m_of(p.height_u);
    const float d = m_of(p.width_u);
    // THE SEALED CORE: the joints between stones read by their shadows, but
    // they must not read THROUGH — a footing is the bottom of the hull. Same
    // remedy as the boarded wall: a closed slab inside the course.
    {
        Material core = mat;
        core.color *= 0.55f;
        core.wobble = 0.0f;
        block(m, {0.0f, 0.0f, d * 0.25f}, {len, h, d * 0.5f}, core, p.wear * 0.5f,
              rng, 1);
    }
    const int rows = std::max(1, p.height_u / 2);
    const float rh = h / static_cast<float>(rows);
    for (int r = 0; r < rows; ++r) {
        // Every course is offset half a stone, the way a wall is actually laid.
        const float offset = (r % 2 == 0) ? 0.0f : -STONE_W_M * 0.5f;
        const int stones = std::max(1, static_cast<int>((len - offset) / STONE_W_M + 0.5f));
        const float sw = (len - offset) / static_cast<float>(stones);
        for (int i = 0; i < stones; ++i) {
            const float x = offset + static_cast<float>(i) * sw;
            const float x0 = std::max(x, 0.0f);
            const float x1 = std::min(x + sw, len);
            if (x1 - x0 < 0.02f) {
                continue;
            }
            // Same rule as the plank: the course's origin is its BED, so the
            // first course starts at y = 0 and the wall on top starts at h.
            hewn_bar(m, {(x0 + x1) * 0.5f, (static_cast<float>(r) + 0.5f) * rh, 0.0f},
                     {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, d, (x1 - x0) * 0.47f,
                     rh * 0.47f, mat, p.wear, rng, 2);
        }
    }
}

void make_fence(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float len = m_of(p.length_u);
    const float h = m_of(p.height_u);
    const float ps = m_of(1) * 0.8f;
    for (int s = 0; s < 2; ++s) {
        const float x = s == 0 ? 0.0f : len;
        hewn_bar(m, {x, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, h, ps * 0.5f,
                 ps * 0.5f, mat, p.wear, rng, 3, 0.85f);
    }
    const int rails = std::max(2, p.height_u / 2);
    for (int r = 0; r < rails; ++r) {
        const float y = h * (static_cast<float>(r) + 1.0f) / (static_cast<float>(rails) + 1.0f);
        hewn_bar(m, {0.0f, y, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, len,
                 ps * 0.35f, ps * 0.45f, mat, p.wear, rng, 3);
    }
}

[[nodiscard]] const char* kind_name(PartKind k) {
    switch (k) {
    case PartKind::Beam: return "beam";
    case PartKind::Post: return "post";
    case PartKind::Plank: return "plank";
    case PartKind::WallPanel: return "wall";
    case PartKind::Gable: return "gable";
    case PartKind::RoofSlope: return "roof";
    case PartKind::Stair: return "stair";
    case PartKind::DoorFrame: return "doorframe";
    case PartKind::DoorLeaf: return "door";
    case PartKind::WindowFrame: return "window";
    case PartKind::Footing: return "footing";
    case PartKind::Fence: return "fence";
    case PartKind::JointPost: return "joint";
    case PartKind::RoofHip: return "roofhip";
    case PartKind::SmokeVent: return "smokevent";
    case PartKind::Sleeper: return "sleeper";
    case PartKind::LogCorner: return "corner";
    case PartKind::Deck: return "deck";
    }
    return "part";
}

[[nodiscard]] const char* material_name(PartMaterial m) {
    switch (m) {
    case PartMaterial::Timber: return "timber";
    case PartMaterial::TimberDark: return "dark";
    case PartMaterial::Plaster: return "plaster";
    case PartMaterial::Stone: return "stone";
    case PartMaterial::Thatch: return "thatch";
    case PartMaterial::Shingle: return "shingle";
    case PartMaterial::Pane: return "pane";
    case PartMaterial::Brick: return "brick";
    case PartMaterial::Tile: return "tile";
    case PartMaterial::Turf: return "turf";
    case PartMaterial::Clay: return "clay";
    }
    return "mat";
}

/// The joint-shape token in a name: -n4/-n6/-n8 state the facet count, -nr
/// says round. In the NAME (not only in the params) by rule: the facet count
/// is the constraint a composer must see without opening the file.
[[nodiscard]] const char* facet_token(int facets) {
    switch (facets) {
    case 4: return "n4";
    case 6: return "n6";
    case 8: return "n8";
    default: return "nr";
    }
}

/// A styled wall's construction token (PartForgeWalls.cpp's dispatch order).
[[nodiscard]] const char* wall_style_token(int variant) {
    switch (variant) {
    case 1: return "log";
    case 2: return "framea";
    case 3: return "framex";
    case 4: return "framek";
    case 5: return "boardv";
    case 6: return "boardh";
    case 7: return "ashlar";
    case 8: return "rubble";
    case 9: return "brick";
    case 10: return "combo";
    default: return "plain";
    }
}

/// What is cut into a styled wall.
[[nodiscard]] const char* opening_token(int opening) {
    switch (opening) {
    case 1: return "win1";
    case 2: return "win2";
    case 3: return "door";
    default: return "blind";
    }
}

} // namespace

std::string part_name(const PartParams& p) {
    char buf[128];
    const int wear10 = static_cast<int>(p.wear * 10.0f + 0.5f);
    switch (p.kind) {
    // Connectors carry their WORKING properties instead of LxWxH: across-flats
    // diameter (d50 = 0.50 m), facet count (n4 = 90° step, nr = any angle),
    // then height/length in grid units. `-cap` marks a capital.
    case PartKind::JointPost:
        std::snprintf(buf, sizeof(buf), "joint-%s-d%d-%s-h%d%s-w%02d",
                      material_name(p.material), p.diameter_cm,
                      facet_token(p.facets), p.length_u,
                      p.variant == 1 ? "-cap" : "", wear10);
        return buf;
    case PartKind::Sleeper:
        std::snprintf(buf, sizeof(buf), "sleeper-%s-d%d-%s-%du-w%02d",
                      material_name(p.material), p.diameter_cm,
                      facet_token(p.facets), p.length_u, wear10);
        return buf;
    // Styled walls keep the wall- prefix (the judge's panel contract) and
    // spell construction and opening: wall-framex-clay-16x1x11-win2-w05.
    case PartKind::WallPanel:
        if (p.variant != 0) {
            std::snprintf(buf, sizeof(buf), "wall-%s-%s-%dx%dx%d-%s-w%02d",
                          wall_style_token(p.variant), material_name(p.material),
                          p.length_u, p.width_u, p.height_u,
                          opening_token(p.opening), wear10);
            return buf;
        }
        break;
    // The steep flight spells its pitch, or the 45° and 26.5° stairs of one
    // step count would fight over one file name — and a composer must see
    // "this one reaches the floor within its run" without opening the file.
    case PartKind::Stair:
        if (p.variant == 1) {
            std::snprintf(buf, sizeof(buf), "stair-steep-%s-%dx%dx%d-w%02d",
                          material_name(p.material), p.length_u, p.width_u,
                          p.height_u, wear10);
            return buf;
        }
        break;
    // The hip slope must spell its half-hip variant or the two would fight
    // over one file name; the vent is named by its base side alone.
    case PartKind::RoofHip:
        std::snprintf(buf, sizeof(buf), "roofhip-%s-%dx%dx%d%s-w%02d",
                      material_name(p.material), p.length_u, p.width_u,
                      p.height_u, p.variant == 1 ? "-polu" : "", wear10);
        return buf;
    case PartKind::SmokeVent:
        std::snprintf(buf, sizeof(buf), "smokevent-%s-%du-w%02d",
                      material_name(p.material), p.length_u, wear10);
        return buf;
    case PartKind::LogCorner:
        std::snprintf(buf, sizeof(buf), "corner-log-%s-h%d-w%02d",
                      material_name(p.material), p.length_u, wear10);
        return buf;
    // НАСТИЛ. The void rides IN THE NAME, because that is the only carrier the
    // judge and a months-old baked assembly can both read (HOUSES.md §4). A
    // solid deck spells no hole token at all rather than `-hole0x0x0x0-`: the
    // absence of a declaration and a declaration of nothing must not look the
    // same, or «панель забыли» and «здесь по проекту сплошь» become one string.
    case PartKind::Deck:
        if (p.void_l_u > 0 && p.void_w_u > 0) {
            std::snprintf(buf, sizeof(buf), "deck-%s-%dx%dx%d-hole%dx%dx%dx%d-w%02d",
                          material_name(p.material), p.length_u, p.width_u,
                          p.height_u, p.void_x_u, p.void_z_u, p.void_l_u,
                          p.void_w_u, wear10);
            return buf;
        }
        break;
    default: break;
    }
    std::snprintf(buf, sizeof(buf), "%s-%s-%dx%dx%d-w%02d", kind_name(p.kind),
                  material_name(p.material), p.length_u, p.width_u, p.height_u,
                  wear10);
    return buf;
}

RegistryObject forge_part(const PartParams& params) {
    RegistryObject obj;
    obj.name = params.name.empty() ? part_name(params) : params.name;
    obj.kind = "part";
    {
        char src[192];
        std::snprintf(src, sizeof(src), "kit:%s %s %dx%dx%du wear=%.2f seed=%llu",
                      kind_name(params.kind), material_name(params.material),
                      params.length_u, params.width_u, params.height_u,
                      static_cast<double>(params.wear),
                      static_cast<unsigned long long>(params.seed));
        obj.source = src;
    }
    Material mat = material_of(params.material, params.wear, params.textured);
    // A BOARD IS SAWN, A BEAM IS HEWN, and the kit tells them apart by the
    // PART rather than by the material — a plank and a post are the same oak.
    // Only these three kinds are boards through and through; the boarded WALL
    // skins say so at their own call site, because the same panel also carries
    // hewn framing.
    if (params.kind == PartKind::Plank || params.kind == PartKind::DoorLeaf
        || params.kind == PartKind::Fence) {
        part_detail::skin_as_board(mat);
    }
    // THE TEXTURED STREAM, and it is not a new stream: `bark` is the .dfo's
    // existing textured-mesh channel (ObjectRegistry.h), the app already feeds
    // it to the foliage program AND to the triangle collision body for
    // kind == "part" (App.cpp), so a part that gains a texture does not lose
    // its solidity. The plain `wood` stream stays empty for a textured part —
    // one surface may live in exactly one stream, or it z-fights itself.
    MeshData& out = mat.skin.textured ? obj.bark : obj.wood;
    Rng rng(params.seed);
    switch (params.kind) {
    case PartKind::Beam: make_beam(out, params, mat, rng); break;
    case PartKind::Post: make_post(out, params, mat, rng); break;
    case PartKind::Plank: make_plank(out, params, mat, rng); break;
    case PartKind::Deck: make_deck(out, params, mat, rng); break;
    case PartKind::WallPanel:
        if (params.variant != 0) {
            part_detail::make_wall_styled(out, params, mat, rng);
        } else {
            make_wall(out, params, mat, rng);
        }
        break;
    case PartKind::Gable: make_gable(out, params, mat, rng); break;
    case PartKind::RoofSlope: part_detail::make_roof(out, params, mat, rng); break;
    case PartKind::RoofHip:
        part_detail::make_roof_hip(out, params, mat, rng);
        break;
    case PartKind::SmokeVent:
        part_detail::make_smoke_vent(out, params, mat, rng);
        break;
    case PartKind::Stair: make_stair(out, params, mat, rng); break;
    case PartKind::DoorFrame: make_door_frame(out, params, mat, rng); break;
    case PartKind::DoorLeaf: make_door_leaf(out, params, mat, rng); break;
    case PartKind::WindowFrame: make_window(out, params, mat, rng); break;
    case PartKind::Footing: make_footing(out, params, mat, rng); break;
    case PartKind::Fence: make_fence(out, params, mat, rng); break;
    case PartKind::JointPost: part_detail::make_joint(out, params, mat, rng); break;
    case PartKind::Sleeper: part_detail::make_sleeper(out, params, mat, rng); break;
    case PartKind::LogCorner:
        part_detail::make_log_corner(out, params, mat, rng);
        break;
    }
    return obj;
}

} // namespace dfn::render
