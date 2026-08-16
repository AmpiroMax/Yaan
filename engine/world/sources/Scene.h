/*
Created: 15:08:2026 - 16:24:04
Last updated: 16:08:2026 - 22:40:23
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

/// One composed scene: the placements of one map.
struct SceneDoc {
    std::string map;         ///< "category/stem" this scene composes
    /// World extent in metres, for the bounds rule. 0 = unknown (the checker
    /// then says so instead of passing the rule silently).
    float world_span_m = 0.0f;
    std::vector<Placement> placements;
};

/// Which rule a finding broke. Named, not numbered: a report a human reads.
enum class SceneRule : uint8_t {
    OnGround,      ///< the object neither hovers nor is buried
    InsideBounds,  ///< the whole object stays inside the map
    NoOverlap,     ///< two objects do not stand inside each other
    KnownObject,   ///< the registry has an object by this name
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

} // namespace dfn::world
