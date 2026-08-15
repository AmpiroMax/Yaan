/*
Created: 12:08:2026 - 22:52:00
Last updated: 15:08:2026 - 14:07:36
Module: engine/world
File: engine/world/sources/WorldgenPlacement.h

Responsibility:
- The placement gates SHARED by more than one placing pass: the §1.3 L0 sight
  wedges, the building-pad ring and the §6.2 entrance exclusion ring.

Key items:
- SightWedges / build_sight_wedges(): C4 enforcement, one construction.
- on_building_pad(), near_entrance_works().
- screen_px_per_rad() / readable_distance_m(): §1.5's read-distance ladder,
  which every siting rule in this zone needs and three files had open-coded.

Dependencies:
- Uses: TestbedLayout.h, WorldgenHydrology.h, WorldgenMacro.h, WorldgenSites.h,
  config.
- Used by: WorldgenScatter.cpp, WorldgenGreatOak.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THIS FILE EXISTS BECAUSE OF RULE 32. These three gates lived inside
  WorldgenScatter.cpp's ScatterCtx, so the second pass that needed them (the
  great oak) could only have them by copying — and a copied exclusion rule
  drifts silently: the copy keeps admitting what the original learned to
  reject. Anything that decides WHERE a thing may stand belongs here the moment
  it has two callers.
*/
/*
UPD:
- 12:08:2026 - 22:52:00: Created by extraction from WorldgenScatter.cpp — the
  wedge struct, its construction and the two exclusion rings, unchanged.
- 12:08:2026 - 22:54:00: screen_px_per_rad / readable_distance_m — the same
  extraction for the read-distance ladder, which had two copies in this zone.
- 15:08:2026 - 14:07:36: screen_px_per_rad() читает DESIGN_RES_H вместо INTERNAL_RES_H —
  форма мира не зависит от графической настройки игрока (см. строку NUMBERS).
*/

#pragma once

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/TestbedLayout.h"
#include "engine/world/sources/WorldgenHydrology.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenSites.h"

#include <cmath>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include <vector>

namespace dfn::world {

/// §1.5/§10.4 — THE READ-DISTANCE LADDER, stated once.
///
/// Vertical angular resolution of the shipped frame. It had three copies in
/// this repository (validation's readability thresholds, the §10.4 mid-field
/// count in the suite, and render's own LOD switch) and the moment
/// INTERNAL_RES or CAMERA_FOV_Y moves they stop agreeing about what "reads"
/// means — a Rule 32 shadow copy wearing a trigonometric costume. Render's is
/// still render's; the two in this zone are gone.
[[nodiscard]] inline float screen_px_per_rad() {
    // DESIGN_RES_H, NOT the user's INTERNAL_RES — and the difference is a
    // rule, not a detail. Resolution became a graphics SETTING (sync #3, and
    // the default rose to 1920x1080 on 15.08.2026); the shape of the world
    // must not change when a player picks a different one. With the shared
    // constant, raising the default added a third co-equal landmark beside
    // the crag and turned the §1.3 hierarchy test red — a settings slider was
    // rewriting the landscape. The thresholds here were derived and filmed in
    // a 360-line frame, so that frame is what they keep measuring in.
    return static_cast<float>(config::DESIGN_RES_H) / static_cast<float>(config::CAMERA_FOV_Y);
}

/// Distance (m) at which an object `size_m` across stops being an OBJECT and
/// becomes texture: it subtends SILHOUETTE_MIN_PX there. §10.4 states this as
/// «d = 30 x S», which is this expression at today's constants.
[[nodiscard]] inline float readable_distance_m(float size_m) {
    return size_m * screen_px_per_rad() / static_cast<float>(config::SILHOUETTE_MIN_PX);
}

/// L0 sight wedges (§1.3 C4 enforcement): 2D wedges from each POI standpoint
/// to the L0 footprint; an occluder inside a wedge whose top would subtend
/// >= L0_angle / LANDMARK_CLEARANCE_FACTOR from the standpoint is rejected.
/// Angle comparisons use tangents (angles here are < 0.2 rad; documented
/// small-angle equivalence).
struct SightWedges {
    struct Standpoint {
        glm::vec2 pos;
        float eye_y;
        glm::vec2 dir; ///< toward the crag center, normalized
        float dist;    ///< to the crag center
        float t_l0;    ///< tangent of the L0's elevation angle
    };
    std::vector<Standpoint> points;
    float crag_radius = 0.0f;

    /// True if an occluder of top height `top_y` at `p` would violate the
    /// clearance factor inside any wedge.
    [[nodiscard]] bool rejects(glm::vec2 p, float top_y) const {
        for (const Standpoint& sp : points) {
            const glm::vec2 rel = p - sp.pos;
            const float proj = glm::dot(rel, sp.dir);
            if (proj < 10.0f || proj > sp.dist) continue;
            const float perp = std::fabs(rel.x * sp.dir.y - rel.y * sp.dir.x);
            if (perp > crag_radius * proj / sp.dist) continue;
            const float t_tree = (top_y - sp.eye_y) / proj;
            if (t_tree * static_cast<float>(config::LANDMARK_CLEARANCE_FACTOR) >= sp.t_l0) {
                return true;
            }
        }
        return false;
    }

    /// THE SAME TEST FOR A THING WITH WIDTH, and it is not a convenience.
    /// rejects() asks about a COLUMN, which is right for a 12 m crown and
    /// wrong by 48 m for a crown as wide as the tree is tall: a giant whose
    /// trunk sits a metre outside a wedge still curtains the massif with half
    /// its canopy. Sampling the crown's own rim is the cheapest honest form of
    /// "does this SILHOUETTE enter the wedge" (GIANT_OAKS §1's consequence:
    /// «огибающая загораживания обязана знать этот силуэт»).
    [[nodiscard]] bool rejects_disc(glm::vec2 p, float radius, float top_y,
                                    int samples = 12) const {
        if (rejects(p, top_y)) return true;
        constexpr float TAU = 6.28318530717958647692f;
        for (int i = 0; i < samples; ++i) {
            const float a = TAU * static_cast<float>(i) / static_cast<float>(samples);
            if (rejects(p + glm::vec2{std::cos(a), std::sin(a)} * radius, top_y)) {
                return true;
            }
        }
        return false;
    }
};

/// Builds the wedges from the layout's POI standpoints toward the L0.
/// `ground_at` is the pre-P4 sampler (macro + water carve) the wedges have
/// always used for eye heights.
template <typename GroundFn>
[[nodiscard]] SightWedges build_sight_wedges(const TestbedLayout& layout, GroundFn&& ground_at) {
    SightWedges wedges;
    wedges.crag_radius = layout.crag.radius;
    const float l0_top = ground_at(layout.crag.center) + L0_AIM_ABOVE_PEAK;
    const auto add_standpoint = [&](glm::vec2 pos) {
        const glm::vec2 to_crag = layout.crag.center - pos;
        const float dist = glm::length(to_crag);
        if (dist < layout.crag.radius) return; // standing on the L0 itself
        SightWedges::Standpoint sp;
        sp.pos = pos;
        sp.eye_y = ground_at(pos) + static_cast<float>(config::PLAYER_EYE_HEIGHT);
        sp.dir = to_crag / dist;
        sp.dist = dist;
        sp.t_l0 = (l0_top - sp.eye_y) / dist;
        wedges.points.push_back(sp);
    };
    for (const SiteLayout& site : layout.sites) add_standpoint(site.position);
    add_standpoint(layout.watchpoint);
    return wedges;
}

/// Building pads (§2.4): nothing natural stands on graded ground. `extra` is
/// the caller's own reach — a crown radius for a tree wide enough to curtain
/// what the pad carries, 0 for a trunk-sized thing.
[[nodiscard]] inline bool on_building_pad(const SitesData& sites, glm::vec2 p,
                                          float extra = 0.0f) {
    for (const BuildingPad& pad : sites.pads) {
        if (glm::length(p - pad.center) < pad.radius + pad.blend + 2.0f + extra) return true;
    }
    return false;
}

/// §6.2 exclusion ring: nothing natural grows over an entrance. The mound
/// exists to make a silhouette a hole in flat ground cannot have, and a stand
/// of oaks on top of it destroys exactly that.
[[nodiscard]] inline bool near_entrance_works(const SitesData& sites, glm::vec2 p,
                                              float extra = 0.0f) {
    const float margin = static_cast<float>(config::ENTRANCE_SCATTER_EXCLUSION_MARGIN) + extra;
    for (const EntranceWorks& w : sites.entrances) {
        if (!w.valid) continue;
        if (glm::length(p - w.center) < w.mound_radius + margin) return true;
        if (glm::length(p - w.portal) < w.forecourt_length + margin) return true;
    }
    // Hand-authored entrances have no works; keep their approach clear too.
    for (std::size_t i = 0; i < sites.entities.size(); ++i) {
        if (sites.types[i] != SiteType::DungeonEntrance) continue;
        if (glm::length(p - sites.entities[i].position_xz) < margin + 4.0f) return true;
    }
    return false;
}

} // namespace dfn::world
