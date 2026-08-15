/*
Created: 15:08:2026 - 16:24:04
Last updated: 15:08:2026 - 16:24:04
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
    void* ctx = nullptr;
};

/// Tolerances. Defaults are the numbers today's defects were measured at; a
/// caller may tighten them, and the checker reports which value it used.
struct SceneLimits {
    float ground_tolerance_m = 0.05f; ///< hover/bury beyond this is a finding
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

/// Human-readable one-liner for a finding (the tool's output, and the text an
/// agent pastes into a map chat).
[[nodiscard]] std::string describe(const SceneFinding& finding);

} // namespace dfn::world
