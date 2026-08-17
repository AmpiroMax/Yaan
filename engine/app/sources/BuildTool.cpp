/*
Created: 17:08:2026 - 19:05:00
Last updated: 17:08:2026 - 19:05:00
Module: engine/app
File: engine/app/sources/BuildTool.cpp

Responsibility:
- The build hand declared in BuildTool.h.

Dependencies:
- Uses: BuildTool.h, engine/world (Scene), std::filesystem.
- Used by: App (editor mode), tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE REASONS BELOW ARE TRANSLATIONS, NOT DECISIONS. Every branch answers a
  rule the judge already applied; none of them decides anything on its own. If
  you catch yourself writing `if (too_close) return {false, ...}` here, the
  rule belongs in the judge.
*/
/*
UPD:
- 17:08:2026 - 19:05:00: Создан вместе с BuildTool.h.
*/

#include "engine/app/sources/BuildTool.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>

namespace dfn::app {
namespace {

namespace fs = std::filesystem;

/// The builder-facing sentence for a rule. Rule 5: these are content and live
/// in the localization table; the keys are spelled here because the mapping
/// rule -> key IS logic, and a table of keys is what makes a missing case
/// visible at compile time rather than as a blank line on screen.
[[nodiscard]] const char* reason_key(world::SceneRule rule) {
    switch (rule) {
    case world::SceneRule::OnGround:      return "build.no.ground";
    case world::SceneRule::InsideBounds:  return "build.no.bounds";
    case world::SceneRule::NoOverlap:     return "build.no.overlap";
    case world::SceneRule::KnownObject:   return "build.no.unknown";
    case world::SceneRule::OffPath:       return "build.no.path";
    case world::SceneRule::OutsideBuildings: return "build.no.inside";
    case world::SceneRule::JointSeat:     return "build.no.joint_seat";
    case world::SceneRule::JointAngle:    return "build.no.joint_angle";
    case world::SceneRule::WallTwoJoints: return "build.no.two_joints";
    case world::SceneRule::JointCapacity: return "build.no.capacity";
    case world::SceneRule::DeckOnJoints:  return "build.no.deck";
    case world::SceneRule::RoofSeat:      return "build.no.roof";
    }
    return "build.no.other";
}

/// "wall-log-timber-12x1x13-blind-w05" -> "wall". The kit spells its family
/// into the name on purpose (HOUSES.md), so the menu's grouping comes from the
/// shelf itself and cannot fall behind it.
[[nodiscard]] std::string family_of(const std::string& name) {
    const std::size_t dash = name.find('-');
    return dash == std::string::npos ? name : name.substr(0, dash);
}

} // namespace

glm::vec3 snap_to_grid(glm::vec3 world_position) {
    const auto snap = [](float v) {
        return std::round(v / BUILD_GRID_M) * BUILD_GRID_M;
    };
    // Y IS DELIBERATELY UNTOUCHED. The ground is continuous; a part that
    // jumped to the nearest 25 cm would hover or sink for a reason invisible
    // to the person holding it, and the judge would then call it wrong.
    return {snap(world_position.x), world_position.y, snap(world_position.z)};
}

BuildVerdict verdict_from_findings(const std::vector<world::SceneFinding>& findings,
                                   std::size_t candidate_index) {
    for (const world::SceneFinding& f : findings) {
        // ONLY THE GHOST'S OWN FINDINGS. A composition with an old problem
        // three houses away must not paint this ghost red: a build tool that
        // refuses everything until the whole map is clean is a build tool
        // nobody can start using.
        if (f.placement_index != candidate_index) {
            continue;
        }
        return {false, reason_key(f.rule)};
    }
    return {true, {}};
}

std::vector<BuildGroup> build_palette(const std::string& shelves) {
    // Ordered by family name so the menu is stable between runs: a palette
    // whose rows move because a directory listing came back in another order
    // teaches the builder nothing he can keep.
    std::map<std::string, std::vector<std::string>> by_family;
    std::size_t start = 0;
    while (start <= shelves.size()) {
        const std::size_t semi = shelves.find(';', start);
        const std::string dir = shelves.substr(
            start, semi == std::string::npos ? std::string::npos : semi - start);
        if (!dir.empty()) {
            std::error_code ec;
            if (fs::is_directory(dir, ec)) {
                for (const auto& e : fs::directory_iterator(dir, ec)) {
                    if (e.path().extension() != ".dfo") {
                        continue;
                    }
                    const std::string name = e.path().stem().string();
                    // The far LOD form is the same exhibit seen from afar, not
                    // a second thing to place (the gallery learned this the
                    // hard way and showed `-far` twins as separate exhibits).
                    if (name.size() > 4 && name.compare(name.size() - 4, 4, "-far") == 0) {
                        continue;
                    }
                    by_family[family_of(name)].push_back(name);
                }
            }
        }
        if (semi == std::string::npos) {
            break;
        }
        start = semi + 1;
    }

    std::vector<BuildGroup> groups;
    groups.reserve(by_family.size());
    for (auto& [family, names] : by_family) {
        std::sort(names.begin(), names.end());
        groups.push_back({family, std::move(names)});
    }
    return groups;
}

} // namespace dfn::app
