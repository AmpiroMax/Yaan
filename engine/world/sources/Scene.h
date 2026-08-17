/*
Created: 15:08:2026 - 16:24:04
Last updated: 17:08:2026 - 12:33:08
Module: engine/world
File: engine/world/sources/Scene.h

Responsibility:
- THE COMPOSITION FILE (.scene) and the RULES that judge it: what stands where
  on a map, as data both a human and an agent edit, plus the machine-checkable
  invariants a composed world must satisfy. The fourth tool of the pivot,
  after the map browser, the world baker and the object registry.

Key items:
- Placement / SceneDoc: one object of the registry, placed.
- read_scene / write_scene: the text format (in git, diffable, mergeable).
- SceneRule / check_scene(): the rules, and the report they produce.

Dependencies:
- Uses: engine/core/math, std. NOT engine/render: a scene is data about WHAT
  stands where, and it must be checkable in a tool with no window.
- Used by: tools/check_scene.cpp, the app's gallery/composition loading, the
  editor's placement UI (later), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE RULES EXIST BECAUSE MEMORY DOES NOT. The user's own words when he asked
  for this tool: «это поможет избегать ошибок по типу висящих в воздухе
  тропинок». Every rule here was bought by a real defect in this repository —
  an object hovering over ground it was placed on, a tree planted past the map
  edge, exhibits standing inside each other. A rule with no defect behind it
  does not belong in this file; a defect met twice does.
- A CHECK REPORTS, IT DOES NOT REPAIR. Silent repair is how a broken scene
  becomes a scene nobody knows is broken: the report names the placement, the
  rule and the number, and the caller decides. `fix_scene` exists separately
  and is explicit.
- The reader is TOTAL: an unknown key is skipped, not fatal (the format will
  grow), but a MALFORMED number is an error with a line, because "0 by
  accident" is the failure mode this project keeps paying for.
*/
/*
UPD:
- 15:08:2026 - 16:24:04: Создан по заданию пользователя: «надо сделать
  приложение, где агенты смогут объекты и пространство оформлять по внутренним
  правилам... чтобы инструментом могли как агенты, так и человек пользоваться».
- 16:08:2026 - 21:08:52: ГРУППЫ И ОПОРА — по заданию пользователя про инструмент, где агенты
  СОБИРАЮТ ДОМА из готовых деталей. Placement::group («farmhouse») меняет два
  правила: члены одной постройки могут пересекаться (это стык, а не дефект) и
  могут стоять ДРУГ НА ДРУГЕ, а не только на земле — без второго дома нельзя
  проверить в принципе: каждая балка выше подошвы читалась бы как висящая, и
  отчёт стал бы шумом. SceneWorld дорос двумя необязательными крюками (правило
  26, только добавления): object_top — насколько деталь возвышается над своим
  началом (то, на что встаёт следующая), object_box — СЛЕД детали как
  прямоугольник. Второй куплен ошибкой: начало строительной детали лежит у её
  КРАЯ, поэтому круг радиуса от начала у фронтона 4 м даёт 4.5 м вокруг угла, и
  забор «стоял внутри» дома через три метра пустой травы. И асимметричный
  допуск bury_tolerance_m: постройка ВРЕЗАЕТСЯ в склон подошвой (так кладут
  камень), а висит в воздухе — всегда ошибка; одинокому дереву поблажки нет.
- 16:08:2026 - 22:40:23: split_shelves() — разбор списка полок реестра. ОДНО определение: три
  инструмента уже разбирали его своей копией, а список полок, означающий в игре
  и в её судье разное, — это судья другого мира (правило 35).
- 16:08:2026 - 22:45:34: check_panel_solid/SolidReport — прибор сплошной ПАНЕЛИ (сборки
  пользователя: «из мелких деталей собирать большие, чтобы меньше дырок»).
  Голая геометрия на входе — заголовок остаётся без engine/render; одна
  функция, двое зовущих: судья (--solid) и пекарь сборок (--require-solid).
- 17:08:2026 - 03:09:30: СПАВН В КОМПОЗИЦИИ (spawn / spawn_yaw, необязательные). Запрос зоны
  flora: пользователь перенёс точку входа на полянку в середину каменной тропы
  лицом к дубу. Это принадлежит КОМПОЗИЦИИ, а не стенду: «встань здесь и смотри
  туда» — утверждение о том, что ПОСТРОЕНО, а стенд знает только середину
  своего чанка. Тем же ключом потом встанет спавн у двери внутри дома.
- 17:08:2026 - 10:53:33: СЕКЦИЯ [light] — лампы композиции (позиция пламени, цвет, радиус,
  просьба о тени, заметка). Свет НЕ объект: объект это то, во что можно
  упереться, а лампа нет; и один столб горит ночью и не горит днём, поэтому
  композитор обязан двигать пламя отдельно от столба. Ламп может быть сколько
  угодно — файл говорит, что СУЩЕСТВУЕТ, рендер решает, что ГОРИТ.
- 17:08:2026 - 11:35:28: СЕКЦИЯ [pad] — та самая правка карты высот, которую просил пользователь
  («редактировать масштаб и карту высот»). Площадка это УТВЕРЖДЕНИЕ, а не мазок
  кистью: «здесь земля такой высоты, растушёвка столько метров», — поэтому её
  можно двигать, перечитывать и судить, чего нарисованное поле высот не умеет.
- 17:08:2026 - 12:33:08: ПРАВИЛА РАЗМЕЩЕНИЯ по заданию пользователя («запретим ставить деревья
  на любые тропы... как например нельзя дерево в доме ставить, дом поверх
  дерева ставить»): OffPath и OutsideBuildings. Второе — ОДНО правило с двух
  концов: дерево в доме и дом поверх дерева это одно и то же пересечение,
  увиденное с разных сторон, и два правила разошлись бы в первом же спорном
  случае. Плюс два крюка, которые делают их не шумными: object_solid (препятствие
  — это твёрдая геометрия ВЫШЕ ШАГА игрока; трава и цветы им не являются) и
  object_box_solid (пересечение меряется по СТВОЛАМ, а не по кронам: две берёзы
  в двух метрах — это лес, а не дефект).
*/

#pragma once

#include <cstdint>
#include <filesystem>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace dfn::world {

/// One placed object: a name in the object registry plus where it stands.
/// Positions are WORLD METRES and heights are ABSOLUTE — not offsets from a
/// ground the file cannot see. A scene that stored "0.2 above the ground"
/// would be a scene whose meaning changed when the terrain did.
struct Placement {
    std::string object;      ///< registry object name, e.g. "oak-forge-a"
    glm::vec3 position{0.0f};
    float yaw = 0.0f;        ///< radians
    float scale = 1.0f;
    /// Free-text note from whoever placed it — the human's «зачем оно тут».
    /// Carried through read/write untouched, so a tool never eats a comment.
    std::string note;
    /// WHAT THIS PART BELONGS TO, e.g. "farmhouse". Empty = it stands alone.
    /// A group is one built thing, and it changes two rules:
    ///   - members may INTERSECT each other (a beam sits in a notch; a rafter
    ///     passes through a wall plate — that is carpentry, not a defect);
    ///   - a member may rest on ANOTHER MEMBER instead of on the terrain, so
    ///     the second storey of a house is not reported as hovering.
    /// Without groups the checker could only ever judge things standing on
    /// open ground, which is trees and nothing else.
    std::string group;
};

/// A LAMP THE COMPOSITION HANGS. Not an object — an object is a thing you can
/// walk into, and a light is not. The lamp POST is a Placement like any other;
/// this is the flame on it, and the two are separate rows on purpose: the same
/// post carries a lit lamp at night and an unlit one by day, and a composer
/// must be able to move one without the other.
///
/// A scene may declare far more lamps than the renderer can light at once
/// (eight in a frame, two of them casting). That is deliberate and not a
/// budget to police here: the file says what EXISTS, the renderer decides what
/// is LIT, and it picks the nearest by distance with a fade at the edge.
struct SceneLight {
    glm::vec3 position{0.0f};   ///< world metres, the flame itself
    glm::vec3 color{1.0f, 0.85f, 0.55f}; ///< linear; the default is a flame
    float radius_m = 6.0f;      ///< 0 = off, and an off lamp is not an error
    bool casts_shadow = false;  ///< honoured for the two nearest that ask
    std::string note;
};

/// A FLAT THE COMPOSITION CUTS INTO THE GROUND — the terrace of a town, the
/// pad under a house, the shelf a market square stands on. Rectangular when
/// half_extents is set, circular on `radius` otherwise.
///
/// This is the "edit the heightmap" half of the tool the user asked for. It is
/// a STATEMENT, not a brush stroke: a pad says "here the ground is this high,
/// blending back over this many metres", so it can be moved, re-read and
/// judged — which a painted heightfield could not be.
struct ScenePad {
    glm::vec2 center{0.0f};
    glm::vec2 half_extents{0.0f}; ///< rectangle; zero = use radius
    float radius = 0.0f;
    float blend = 8.0f;           ///< metres to fade back into the natural ground
    float height = 0.0f;          ///< absolute metres
    std::string note;
};

/// One composed scene: the placements of one map.
struct SceneDoc {
    std::string map;         ///< "category/stem" this scene composes
    /// World extent in metres, for the bounds rule. 0 = unknown (the checker
    /// then says so instead of passing the rule silently).
    float world_span_m = 0.0f;
    /// WHERE THE PLAYER STANDS when this map opens, and which way he looks.
    /// Optional: without it the stand's own spawn is used, exactly as before.
    /// It belongs to the COMPOSITION and not to the stand, because "stand here
    /// and look at that" is a statement about what was BUILT — the middle of a
    /// stone path facing the great oak, or just inside a house's door. A stand
    /// only knows where the middle of its chunk is.
    bool has_spawn = false;
    glm::vec3 spawn{0.0f};
    float spawn_yaw = 0.0f;   ///< radians; 0 looks north (forward = {sin,0,-cos})
    std::vector<Placement> placements;
    std::vector<SceneLight> lights;
    std::vector<ScenePad> pads;
};

/// Which rule a finding broke. Named, not numbered: a report a human reads.
enum class SceneRule : uint8_t {
    OnGround,      ///< the object neither hovers nor is buried
    InsideBounds,  ///< the whole object stays inside the map
    NoOverlap,     ///< two objects do not stand inside each other
    KnownObject,   ///< the registry has an object by this name
    /// NOTHING STANDS ON A PATH. A road with a tree growing out of it is not a
    /// road, and the path is now the ground's own property — so an object over
    /// it is not merely ugly, it contradicts what the ground says it is.
    OffPath,
    /// NOTHING STANDS INSIDE A BUILDING THAT IS NOT PART OF IT — and the rule
    /// reads both ways, which is why it is one rule and not two: a tree in a
    /// house and a house on a tree are the same overlap seen from two ends.
    /// The user named both (17.08): «нельзя дерево в доме ставить / дом поверх
    /// дерева ставить».
    OutsideBuildings,
};

/// One violation. Carries the NUMBER, not just a verdict — "hovers" is an
/// opinion, "hovers by 0.42 m" is a measurement somebody can act on.
struct SceneFinding {
    SceneRule rule = SceneRule::OnGround;
    std::size_t placement_index = 0;
    std::string object;
    float amount_m = 0.0f;   ///< how far past the rule (metres), signed
    std::string detail;
};

/// What the checker needs to know about the world it is judging. Supplied by
/// the caller so this header depends on no generator: a tool passes worldgen's
/// sampler, a test passes a flat plane, and both exercise the same rules.
struct SceneWorld {
    /// Ground height at a world x/z, metres. Required.
    float (*ground_at)(void* ctx, glm::vec2 world_xz) = nullptr;
    /// Footprint radius of a registry object, metres, and its lowest point
    /// relative to its origin (negative for roots that dive). Required: the
    /// rules measure the OBJECT, never a guessed size.
    bool (*object_extent)(void* ctx, const std::string& name, float& radius_m,
                          float& bottom_m) = nullptr;
    /// How tall the object is above its own origin, metres. OPTIONAL: when it
    /// is null nothing can rest on anything and the ground rule measures
    /// against the terrain alone, exactly as before groups existed. Supplied
    /// separately rather than as a fourth out-parameter of object_extent so
    /// that every caller written against the old shape keeps compiling
    /// (Rule 26: contracts grow, they do not change).
    bool (*object_top)(void* ctx, const std::string& name, float& top_m) = nullptr;
    /// The object's FOOTPRINT as a box in its own local space, relative to its
    /// origin. OPTIONAL: without it the checker falls back to the radius
    /// circle above, which is right for a tree — round, centred on its trunk —
    /// and badly wrong for a building part, whose origin is at one END. A 4 m
    /// gable measured as a circle claims a 4.5 m radius about its corner and
    /// then "stands inside" everything in the yard.
    bool (*object_box)(void* ctx, const std::string& name, glm::vec2& min_xz,
                       glm::vec2& max_xz) = nullptr;
    /// Metres from the OUTER EDGE of the worn path surface, outward; negative
    /// ON the trodden surface. OPTIONAL — a world with no paths supplies none,
    /// and then the path rule simply never fires.
    ///
    /// It asks the SAME field the ground was worn by. Asking a second source
    /// would let the judge forbid building where the ground shows no path, and
    /// permit it where the ground shows one.
    bool (*path_clearance)(void* ctx, glm::vec2 world_xz, float& metres) = nullptr;
    /// Is this object SOLID — does it have geometry a body is built from?
    /// OPTIONAL; without it everything counts as solid, which is what the rule
    /// assumed before the question could be asked.
    ///
    /// It is the criterion the GAME already uses: an object with no solid
    /// stream gets no collision body and the player walks through it. Grass,
    /// flowers and mushrooms are such objects, and two of them sharing a
    /// patch of ground is a meadow, not a defect — which is why the overlap
    /// rule must not fire on them. Deciding this by a LIST OF SPECIES would
    /// have been a second definition of "solid" that drifts from the one the
    /// player's knees already know.
    bool (*object_solid)(void* ctx, const std::string& name) = nullptr;
    /// The footprint of the object's SOLID part only — its trunk and its
    /// walls, not its crown. OPTIONAL; without it the whole footprint is used,
    /// as before.
    ///
    /// The overlap rule needs this and the support rule must NOT have it. Two
    /// birches two metres apart have mingling crowns and separate trunks: that
    /// is a wood, and measuring their crowns called it a defect forty thousand
    /// times on one map. A beam resting on a post, on the other hand, rests on
    /// whatever part of it is under it. Same objects, two questions, two
    /// footprints.
    bool (*object_box_solid)(void* ctx, const std::string& name, glm::vec2& min_xz,
                             glm::vec2& max_xz) = nullptr;
    void* ctx = nullptr;
};

/// Tolerances. Defaults are the numbers today's defects were measured at; a
/// caller may tighten them, and the checker reports which value it used.
struct SceneLimits {
    float ground_tolerance_m = 0.05f; ///< hover/bury beyond this is a finding
    /// How far a GROUP MEMBER may be dug into the terrain before it counts.
    /// Asymmetric with the hover tolerance on purpose: a building sits on a
    /// slope by burying the uphill side of its footing course — that is how
    /// masonry works — while a part hovering by the same amount is always a
    /// mistake. Loose objects (no group) get no such licence: a tree buried
    /// half a metre is a defect, not a design.
    float bury_tolerance_m = 0.5f;
    float edge_margin_m = 2.0f;       ///< keep this much of the map beyond it
    float overlap_slack_m = 0.5f;     ///< crowns may mingle by this much
    /// How far an object must keep from the worn edge of a path. Not zero: a
    /// trunk exactly at the edge still drops its crown and its roots over the
    /// tread, and a walker still has to step around it.
    float path_clearance_m = 0.5f;
    /// How far a loose object must keep out of a building's footprint. Small
    /// on purpose — a barrel against a wall is a barrel against a wall, not a
    /// defect; what this rule is for is a tree in the middle of a room.
    float building_slack_m = 0.25f;
};

/// Reads a .scene file. Returns false and fills `error` (with the line number)
/// on a malformed number or a missing required key.
[[nodiscard]] bool read_scene(const std::filesystem::path& path, SceneDoc& out,
                              std::string& error);

/// Writes it back, atomically, in a stable order — so a diff shows what a
/// human changed and not what a container reordered.
[[nodiscard]] bool write_scene(const SceneDoc& doc, const std::filesystem::path& path);

/// Judges the scene. Returns every finding; an empty result is a clean scene.
[[nodiscard]] std::vector<SceneFinding> check_scene(const SceneDoc& doc,
                                                    const SceneWorld& world,
                                                    const SceneLimits& limits = {});

/// Sits every hovering or buried placement back on the ground and returns how
/// many it moved. SEPARATE from check_scene and explicit on purpose: a checker
/// that repairs is a checker whose report nobody reads.
[[nodiscard]] std::size_t fix_scene_ground(SceneDoc& doc, const SceneWorld& world,
                                           const SceneLimits& limits = {});

/// Splits a shelf list ("a;b;c") into its directories, trimmed, empties
/// dropped. ONE definition because three tools already need it — the app, the
/// checker and the assembler — and a shelf list that means different things in
/// the game and in its judge is a judge of a different world (Rule 35).
[[nodiscard]] std::vector<std::string> split_shelves(const std::string& list);

/// Human-readable one-liner for a finding (the tool's output, and the text an
/// agent pastes into a map chat).
[[nodiscard]] std::string describe(const SceneFinding& finding);

/// Report of the panel-solidity grid (check_panel_solid). Carries the COUNT
/// and the ADDRESS, not a verdict: «дыра на x=1.93 y=0.46» is actionable,
/// «панель дырявая» is an opinion (the gable lesson, in the instrument).
struct SolidReport {
    int rays_cast = 0;
    int rays_through = 0;        ///< 0 = solid
    uint8_t normal_axis = 0;     ///< 0=x 1=y 2=z: the thinnest bbox axis
    /// World-space point where the FIRST through-ray crossed the panel slab's
    /// mid-plane — where to look for the hole.
    glm::vec3 first_hole{0.0f};
};

/// THE PANEL INSTRUMENT. A flat assembly (wall panel, floor deck) has no
/// interior to stand a probe in, so the sealed-hull fan cannot judge it; what
/// a panel promises is NO DAYLIGHT STRAIGHT THROUGH. Casts a grid of parallel
/// rays across the triangles' THINNEST bounding-box axis (the panel's normal,
/// found from geometry, never from the property under test), one ray per
/// `step_m`; the default 0.01 m is set by the narrowest real hole this zone
/// has shipped — a 0.017 m board gap — which a coarser grid would certify as
/// solid. `rim_m` trims the border, where the panel's edge is the frame's
/// business and a boundary ray measures floating point, not wood.
///
/// Takes BARE world-space triangles (positions + indices) so this header
/// stays free of engine/render: the scene judge and the assembly baker each
/// build the soup from the shelves they already read, and both call THIS
/// function — one instrument, two callers, no second opinion.
[[nodiscard]] SolidReport check_panel_solid(const std::vector<glm::vec3>& positions,
                                            const std::vector<uint32_t>& indices,
                                            float step_m = 0.01f,
                                            float rim_m = 0.02f);

} // namespace dfn::world
