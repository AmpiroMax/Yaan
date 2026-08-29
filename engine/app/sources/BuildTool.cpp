/*
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
    case world::SceneRule::StairSeat:     return "build.no.stair_seat";
    case world::SceneRule::StairHeadroom: return "build.no.stair_head";
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

float place_support_y(const world::SceneDoc& doc, glm::vec3 aim, float ground_y,
                      const world::SceneWorld& world, std::string* on_what) {
    float support = ground_y;
    if (on_what != nullptr) {
        *on_what = "the ground";
    }
    if (world.object_top == nullptr || world.object_extent == nullptr) {
        // NO RULER, NO STACKING — and that is the honest answer rather than a
        // guessed height: an object whose top nobody can measure cannot be
        // stood on.
        return support;
    }
    // HOW MUCH ABOVE THE AIM STILL COUNTS AS "the surface I am pointing at".
    // One grid cell: the ray stops on the top face, and floating point plus the
    // 0.25 m snap put the aim a hair under or over it.
    constexpr float AIM_TOL_M = BUILD_GRID_M;
    for (const world::Placement& p : doc.placements) {
        float top = 0.0f;
        if (!world.object_top(world.ctx, p.object, top)) {
            continue;
        }
        const float top_y = p.position.y + top;
        if (top_y <= support || top_y > aim.y + AIM_TOL_M) {
            continue; // lower than what we have, or above where he pointed
        }
        // IS THE AIM OVER IT? The box when the object has one, the circle when
        // it does not — the same fallback the judge makes, and for the same
        // reason: a beam's origin is at one END, so a circle about it claims
        // ground the beam does not cover.
        const float dx = aim.x - p.position.x;
        const float dz = aim.z - p.position.z;
        glm::vec2 lo{0.0f};
        glm::vec2 hi{0.0f};
        bool inside = false;
        if (world.object_box != nullptr && world.object_box(world.ctx, p.object, lo, hi)) {
            // Into the placement's own frame: yaw turns the footprint with it.
            const float cs = std::cos(-p.yaw);
            const float sn = std::sin(-p.yaw);
            const float lx = cs * dx - sn * dz;
            const float lz = sn * dx + cs * dz;
            inside = lx >= lo.x && lx <= hi.x && lz >= lo.y && lz <= hi.y;
        } else {
            float radius = 0.0f;
            float bottom = 0.0f;
            if (!world.object_extent(world.ctx, p.object, radius, bottom)) {
                continue;
            }
            inside = dx * dx + dz * dz <= radius * radius;
        }
        if (!inside) {
            continue;
        }
        support = top_y;
        if (on_what != nullptr) {
            *on_what = p.object;
        }
    }
    return support;
}

} // namespace dfn::app
