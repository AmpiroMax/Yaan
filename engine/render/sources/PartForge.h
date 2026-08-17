/*
Created: 16:08:2026 - 20:52:00
Last updated: 17:08:2026 - 15:46:07
Module: engine/render
File: engine/render/sources/PartForge.h

Responsibility:
- THE BUILDING KIT: forges construction PARTS — beams, posts, planks, wall
  panels, gables, roof slopes, stairs, doors, windows, footings, fences — into
  the object registry (.dfo), so an agent builds a house by PLACING PIECES
  instead of writing a house-shaped function.

Key items:
- PartKind / PartParams / forge_part(): one part, made to size.
- kit_catalogue(): the whole numbered kit, expanded from a few families.

WHY PARTS AND NOT HOUSES (user, 16.08.2026: «надо чтобы агент мог сделать себе
несколько видов разных палок, стен, лестниц и тд, чтобы был набор из 500-та
различных строй материалов и их конфигураций, чтобы агент строил разные дома»):
a house generator produces the houses its author imagined; a KIT produces the
houses its USER imagines. The reference frames he supplied are Nordic timber
frame — posts and beams carrying the load, infill between them, a steep roof,
a stair to a raised floor — which is a kit by construction: the same dozen
pieces, cut to a few lengths, repeated.

EVERY PART SNAPS. Sizes are whole multiples of BUILD_GRID_M and origins sit at
a piece's natural joint (a beam's origin is its END, not its middle), so pieces
placed on the grid meet exactly instead of nearly. Placement by eye is what
makes assembled buildings look assembled.

Dependencies:
- Uses: ObjectRegistry.h (RegistryObject), FloraCards.h (bark/wood tiles).
- Used by: tools/forge_parts.cpp, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- PURE AND DETERMINISTIC: same params, same bytes, same content hash.
- Rule 52 applies to every part: closed volumes, never single planes. A wall
  you can see through the edge of is a wall nobody can build with.
*/
/*
UPD:
- 16:08:2026 - 20:52:00: Создан — набор строительных деталей для агента-строителя.
- 16:08:2026 - 22:16:30: PartMaterial::Pane — глухая вставка окна. Правило зоны с этого
  дня (пользователь про хутор: «стены несплошные, дырки в доме... окна глухие,
  с имитацией вида насквозь»): деталь ограждения ЗАМКНУТА, сквозных просветов
  не оставляет.
- 17:08:2026 - 12:38:26: СЕМЬЯ СОЕДИНИТЕЛЕЙ (HOUSES.md §3-4, [ЖДЁТ] -> код):
  JointPost (стойка-шарнир, 4/6/8/круг граней x d 35/50/75/100 см), Sleeper
  (лежень пола), LogCorner (перевязка торцов сруба); материалы Brick/Tile/Turf
  (кирпичный столб — слова пользователя; черепица и дёрн — под волну крыш).
  PartParams вырос полями facets/diameter_cm/variant — только добавления,
  старые детали переиспечены байт-в-байт.
- 17:08:2026 - 13:02:52: PartMaterial::Clay (глина по каркасу — «глина с каркасом» из
  списка вариантов стен пользователя 17.08).
- 17:08:2026 - 13:18:48: PartParams::opening (глухая/окно/два окна/дверной проём) —
  волна вариантов стен: 10 стилей (сруб, фахверк x3 рисунка раскосов, обшивка
  верт./гориз., камень тёсаный/бутовый, кирпич, комбо низ-камень) x проёмы,
  174 панели, имена wall-<стиль>-<мат>-LxTxH-<проём>-wNN.
- 17:08:2026 - 13:23:56: PartKind::RoofHip (вальма, вариант полувальмы) и SmokeVent (дымник)
  — волна вариантов крыш.
- 17:08:2026 - 13:46:59: Stair variant 1 = крутой марш 45° (проступь 1u, имя -steep) — «на
  второй этаж за длину этих доводили»; подъём остаётся 1u из-за
  PLAYER_STEP_HEIGHT (0.50 непроходим).
- 17:08:2026 - 15:46:07: текстурность стала свойством ДЕТАЛИ (kit_textured_default() — одно
  определение умолчания). Была процессная дверь, читаемая внутри кузницы, и
  тест текстурного потока не мог её попросить: он проверял умолчание и
  покраснел в день, когда умолчание сменилось. Полка байт в байт прежняя.
*/

#pragma once

#include "engine/render/sources/ObjectRegistry.h"

#include <cstdint>
#include <string>
#include <vector>

namespace dfn::render {

/// The build grid. Every part's size is a whole multiple of it and every
/// origin sits on it, which is what lets an agent place by integer counts and
/// have the pieces MEET. 0.25 m: fine enough for a step's rise and a plank's
/// width, coarse enough that a whole house is a few dozen integers.
inline constexpr float BUILD_GRID_M = 0.25f;

enum class PartKind : uint8_t {
    Beam,       ///< horizontal timber, origin at its near end
    Post,       ///< vertical timber, origin at its foot
    Plank,      ///< thin board for cladding and decks
    WallPanel,  ///< timber frame + infill, one bay wide
    Gable,      ///< the triangular end wall under a roof
    RoofSlope,  ///< one pitched roof plane with its ridge beam
    /// A flight, origin at the foot of the lowest step. variant 0 = 26.5°
    /// (going 2u — streets, terraces), variant 1 = STEEP 45° (going 1u,
    /// `-steep` in the name) — the house stair that reaches the next floor
    /// within its own run. Rise is 1u for both: PLAYER_STEP_HEIGHT 0.35 m
    /// passes a 0.25 riser and refuses a 0.50 one, so pitch may only come
    /// from the going (a controller constraint, not a preference).
    Stair,
    DoorFrame,  ///< opening with jambs and lintel (the door leaf is its own part)
    DoorLeaf,
    WindowFrame,
    Footing,    ///< the stone block a timber building stands on
    Fence,      ///< one section of rail fence
    /// СТОЙКА-ШАРНИР (HOUSES.md §3-4): the connector post every panel ends at.
    /// A vertical N-gon prism, origin at its foot, AXIS at the origin — panels
    /// are measured centre-of-post to centre-of-post, so the composer places
    /// this first and counts panels from it. Its facet count is the JOINT'S
    /// working property: as many facets, as many directions it can hand a
    /// panel to (see PartParams::facets).
    JointPost,
    /// ЛЕЖЕНЬ: the horizontal bedding log a floor deck rests on and meets the
    /// walls through (HOUSES.md §3: floor-to-wall is a connector too, never a
    /// butt joint). Origin at the near end's UNDERSIDE, axis along +X.
    Sleeper,
    /// ПЕРЕВЯЗКА ТОРЦОВ СРУБА: the interlocked crossing log ends at a log
    /// house's corner — alternating stubs along +X and +Z, origin at the
    /// corner axis' foot. What makes a log wall read as BUILT at its corner
    /// instead of two panels colliding.
    LogCorner,
    /// ВАЛЬМОВЫЙ СКАТ: the triangular end slope of a hipped roof (variant 1 =
    /// полувальма, the trapezoid that pinches only the gable's top). Origin
    /// at the eaves' near corner, eaves along +Z, rise toward +X.
    RoofHip,
    /// ДЫМНИК: the louvred ridge hood. Origin at its BASE CENTRE so it sets
    /// astride a ridge by the ridge's own coordinates.
    SmokeVent,
};

/// What a part is made OF, in the reference's own terms: the frames the user
/// gave show three materials and nothing else — weathered timber, pale infill
/// plaster, and grey stone footings — plus thatch on the roofs.
enum class PartMaterial : uint8_t {
    Timber,
    TimberDark,
    Plaster,
    Stone,
    Thatch,
    Shingle,
    /// A BLIND window pane: the closed dark-warm insert that IMITATES a view
    /// into the house (user: «окна глухие, без сплошного просвета, с имитацией
    /// вида насквозь»). Its own material so the registry can tell an insert
    /// from wood and a later real-interior pass can find and replace it.
    Pane,
    /// Кирпич: the masonry column and wall material the user named for
    /// connectors («кирпичный столб») — WHITERUN_RESEARCH.md §6 sanctioned it
    /// for the street layer.
    Brick,
    /// Черепица: fired-clay roof tile, the rich house's roof.
    Tile,
    /// Дёрн: the living turf roof of the poorest houses (roof variants wave).
    Turf,
    /// Глина по каркасу: the smoothed earthen infill of the poorer timber
    /// frame (wall variants wave — «глина с каркасом» in the user's list).
    Clay,
};

/// Whether the kit bakes its TEXTURED form. Reads DFN_PARTS_TEXTURED once.
/// ONE DEFINITION of the answer: the catalogue, the first-run bake and the
/// tests all ask here, so none of them can drift from the others.
[[nodiscard]] bool kit_textured_default();

struct PartParams {
    uint64_t seed = 1;
    std::string name = "part";
    PartKind kind = PartKind::Beam;
    PartMaterial material = PartMaterial::Timber;
    /// Size in GRID UNITS, not metres — the unit an agent counts in.
    int length_u = 8;  ///< along the part's own axis
    int width_u = 1;
    int height_u = 1;
    /// 0 = crisp and new, 1 = weathered: axe marks, sag, split ends. The
    /// reference is old wood, so the kit's default is not zero.
    float wear = 0.5f;
    /// JOINT SHAPE (JointPost/Sleeper): how many FLAT facets the connector
    /// offers — 4 (шаг угла 90°), 6 (60°), 8 (45°), 0 = круглая (any angle).
    /// The reason is one rule for the whole row (user, 17.08): a panel seats
    /// FLUSH ON A FACET, so a joint hands out exactly as many directions as it
    /// has facets; between facets the panel rides over the arris and the gap
    /// returns. The count is a WORKING property, carried in the name (-n4/-nr)
    /// so a composer sees the constraint without opening the file.
    int facets = 0;
    /// JOINT WORKING SIZE (JointPost/Sleeper), across-flats, centimetres.
    /// Row 35/50/75/100: derived from panel thickness T as d >= T + 0.1 — at
    /// centre-to-centre placement the facet must overhang the panel's face by
    /// >= 5 cm each side, or the seam line stays visible and floating point
    /// turns a touch into a hairline gap (HOUSES.md §3.1). Not `_u` grid
    /// units on purpose: 0.35 is not a grid multiple, and the grid owns
    /// LENGTHS, not section sizes (§3.2).
    int diameter_cm = 0;
    /// TEXTURED FORM, per part. Defaults to the process-wide door so nothing
    /// changes for the catalogue; a caller that wants ONE form regardless —
    /// a test of the textured stream, or the sign shelf that must stay flat —
    /// says so here instead of reaching for an environment variable it cannot
    /// change once the first part has been forged.
    bool textured = kit_textured_default();
    /// STYLE VARIANT inside one kind (wall bonds, roof shapes, joint
    /// capitals). 0 = the kind's default; meanings are per-kind and each
    /// variant spells its token into the part's name.
    int variant = 0;
    /// WHAT IS CUT INTO a styled wall: 0 глухая, 1 окно, 2 два окна,
    /// 3 дверной проём. Windows are BLIND by §2 (a sealed Pane insert); the
    /// door opening is the kit's ONE sanctioned through-hole — the house
    /// closes it with a leaf, and the wall tests assert daylight there and
    /// nowhere else.
    int opening = 0;
};

/// The kit's naming rule: kind-material-LxWxH-wNN, e.g. "beam-timber-8x2x1-w06".
/// Derived from the params alone, so a part's file name states its size and an
/// agent can ASK for a part by describing it instead of reading an index.
[[nodiscard]] std::string part_name(const PartParams& params);

/// Forges one part. Ready for write_object().
[[nodiscard]] RegistryObject forge_part(const PartParams& params);

/// THE KIT: every part the catalogue declares, expanded from its families
/// (kind x size x material x wear). This is the "500 pieces" — produced by
/// rule rather than typed out, so adding a length adds a row everywhere.
[[nodiscard]] std::vector<PartParams> kit_catalogue();

} // namespace dfn::render
