/*
Created: 09:08:2026 - 16:45:00
Last updated: 13:08:2026 - 18:59:13
Module: engine/world
File: engine/world/sources/WorldgenCarve.h

Responsibility:
- Pass P7 (3D terrain): the carved volumes that a heightfield cannot express —
  the switchback tunnel up Ravenscar Crag (LANDSCAPE §7 / user request в23,
  modelled on Skyrim's Seven Thousand Steps) and the Backbarrow interior.
  Expressed as signed distance fields subtracted from the terrain SDF.

Key items:
- CarveCorridor: a walkable corridor along a polyline (flat floor, flat
  ceiling, real headroom) — the shape both carves are built from.
- carve_distance(): the union SDF of every carved volume (negative inside).
- carve_column_range(): the y span a column's carves occupy, so the voxel
  builder can widen that column's active band.

Dependencies:
- Uses: TestbedLayout.h, glm.
- Used by: VoxelVolume (subtraction), validation, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- A corridor polyline may START AND END IN OPEN AIR on purpose: carving air out
  of air is a no-op, so the portal forms exactly where the path meets rock.
  That is how the mouths are made — do not "optimise" the outside segments away.
- FLAT FLOOR, not a tube. A capsule tunnel gives a rounded floor that reads as
  a burrow and walks badly; the cross-section here is a box (flat floor, flat
  ceiling) so the corridor reads as cut rock and the player stands upright.
- Deterministic: pure geometry, no rng.
*/
/*
UPD:
- 09:08:2026 - 16:45:00: Created — P7 carve pass for the 3D terrain stage.
- 09:08:2026 - 16:47:51: Created — P7 carve SDF: box cross-section corridors (flat floor, real headroom) and chambers, plus the per-column range the voxel builder needs to widen its band.
- 09:08:2026 - 17:36:42: §6.2: carve_mouth / site_carve_mouth (entrance markers derived from the mouth, never scored) and carve overloads taking derived corridors.
- 09:08:2026 - 21:37:57: NEW enclosure_darkness() — LANDSCAPE §6.3 authored darkness as the RULE, replacing the app-side stand-in that measured depth below the local surface (which calls a deep valley floor a cave). Both halves of design's rule are evaluated: ENCLOSED (inside carved air AND rock overhead) and EARNED (>= DARKNESS_DEPTH_MIN walked ALONG the corridor from the nearest mouth, not straight-line through rock — a switchback is dark because you walked it). Ramps over DARKNESS_FALLOFF_MIN. Measured seed 1: valley floor 0.000, barrow mouth 0.000, 20 m in 0.375, chamber 1.000, solid rock (not a place) 0.000.
- 10:08:2026 - 02:29:54: open_daylight_portals() — endpoints of flagged corridors pushed along their own leg (grade preserved) until the floor stands in open air; capped, and a corridor that cannot reach daylight is left as-is so the acceptance walk stays the alarm.
- 13:08:2026 - 16:45:00: NEW enclosure_trace() — те же промежуточные величины того же вычисления (какие ворота решили: вхождение, «над землёй», заработанный путь). enclosure_darkness() реализована как .darkness этого вызова, поэтому отладочной копии, способной разойтись с боевой, не существует. Заведено под разбор «темнеет, потом мигает»: результат сам по себе не отличает «не замкнуто» от «замкнуто, но ничего не заработано», а причины и правки у них разные.
- 13:08:2026 - 18:40:00: EnclosureTrace: above_ground -> open_to_sky + новое поле roof_y. Вторые ворота судят КРЫШУ прорезки против рельефа, а не точку запроса.
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
*/

#pragma once

#include "engine/world/sources/TestbedLayout.h"

#include <functional>
#include <glm/vec3.hpp>
#include <optional>
#include <vector>
#include <span>
#include <utility>

namespace dfn::world {

/// Signed distance to the union of all carved volumes at `world`
/// (negative = inside carved air). Returns a large positive value far away.
[[nodiscard]] float carve_distance(const TestbedLayout& layout, glm::vec3 world);

/// Same, including DERIVED corridors (the §6.2 entrance adits).
[[nodiscard]] float carve_distance(const TestbedLayout& layout,
                                   std::span<const CarveCorridor> extra, glm::vec3 world);

/// The vertical span carved volumes occupy in the column at `world_xz`, as
/// (lo, hi) in meters. Returns (1, -1) — an empty range — when the column
/// touches no carve. Used to widen the voxel band; a carve outside the band
/// would simply not exist.
[[nodiscard]] std::pair<float, float> carve_column_range(const TestbedLayout& layout,
                                                         glm::vec2 world_xz);
[[nodiscard]] std::pair<float, float> carve_column_range(const TestbedLayout& layout,
                                                         std::span<const CarveCorridor> extra,
                                                         glm::vec2 world_xz);

/// True if any carve exists in this layout (lets the builder skip the work).
[[nodiscard]] bool has_carves(const TestbedLayout& layout);

/// Terrain height sampler (macro + carve, WITHOUT pads — pads are what P4 is
/// still deciding when this is called).
using GroundSampler = std::function<float(glm::vec2)>;

/// Where a corridor stops being open to the sky and rock closes overhead: the
/// real entrance. `outward` points back out of the hill, i.e. the direction an
/// arriving player faces the opening from.
struct CarveMouth {
    glm::vec3 position{0.0f};
    glm::vec2 outward{0.0f, 1.0f};
};

/// Finds the mouth of `corridor`, or nullopt when the corridor never goes
/// under rock at all (a carve entirely in the open is not an entrance).
[[nodiscard]] std::optional<CarveMouth> carve_mouth(const CarveCorridor& corridor,
                                                    const GroundSampler& ground);

/// DERIVES the daylight ends of corridors flagged `daylight_portals`: each
/// endpoint is pushed outward along its own leg (grade preserved) until the
/// corridor floor stands in open air, so the portal genuinely forms where the
/// path meets rock. The switchback survey has been re-buried TWICE by reshapes
/// of the massif above it (the L0 lift, then the §2.8 banded contours) — an
/// endpoint that must sit in open air over terrain that keeps changing is a
/// derived quantity, not a survey point. Corridors that deliberately end
/// inside rock (the barrow passage -> chamber) carry the flag false and are
/// never touched. A corridor that cannot reach daylight within the cap is
/// left as-is: the acceptance walk stays the alarm, nothing silently "fixes".
void open_daylight_portals(TestbedLayout& layout, const GroundSampler& ground);

/// The mouth belonging to site `site_index`, or nullopt when that site has no
/// carve. THIS is what P4 uses: a carved entrance is derived, never scored.
[[nodiscard]] std::optional<CarveMouth> site_carve_mouth(const TestbedLayout& layout,
                                                         int site_index,
                                                         const GroundSampler& ground);

/// LANDSCAPE §6.3 authored darkness, as the RULE rather than a list of places:
/// darkness is EARNED by depth. Returns 0 (open daylight) .. 1 (pitch black)
/// for `world`, and it is 0 anywhere that is not genuinely enclosed — a deep
/// valley floor is not a cave, which is exactly the failure mode of measuring
/// "depth below the local surface" instead.
///
/// Both halves of the design rule are evaluated:
///   - ENCLOSED: the point is inside carved air AND there is rock overhead.
///   - EARNED:   the walk back to the nearest mouth is >= DARKNESS_DEPTH_MIN,
///               measured ALONG the corridor, not as a straight line (a
///               switchback is dark because you walked it, not because the
///               portal is close through solid rock).
/// The transition ramps over DARKNESS_FALLOFF_MIN rather than switching.
///
/// `ground` samples surface height; pass the same sampler the carve mouths
/// were derived with so the mouth positions agree.
[[nodiscard]] float enclosure_darkness(const TestbedLayout& layout,
                                       std::span<const CarveCorridor> extra,
                                       const GroundSampler& ground, glm::vec3 world);

/// The intermediates of the SAME evaluation, so a caller can say WHICH half
/// decided. Written for the "темнеет, потом мигает" investigation: the result
/// alone cannot distinguish "not enclosed" from "enclosed but nothing earned",
/// and those two have different causes and different fixes.
///
/// `enclosure_darkness()` is implemented as `.darkness` of this call — there is
/// ONE evaluation, not a debug copy that can drift from the shipping one
/// (Rule 32).
struct EnclosureTrace {
    float carve_distance = 0.0f;  ///< union carve SDF at the query point; >= 0 rejects
    float ground_y = 0.0f;        ///< surface height over the query column
    float roof_y = 0.0f;          ///< top of the carve containing the point
    bool open_to_sky = false;     ///< second rejection: the roof is not under the terrain
    float path_from_mouth = 0.0f; ///< metres walked along the corridor, after fallback
    bool path_measured = false;   ///< false = no mouth reachable, fallback used
    float darkness = 0.0f;
};

[[nodiscard]] EnclosureTrace enclosure_trace(const TestbedLayout& layout,
                                             std::span<const CarveCorridor> extra,
                                             const GroundSampler& ground, glm::vec3 world);

/// One wall torch: where it hangs and which way it faces.
struct CarveLightSite {
    glm::vec3 position{0.0f}; ///< the sconce, world space (y is explicit — a
                              ///< carved floor is not on the heightfield)
    float yaw = 0.0f;         ///< faces across the corridor, away from its wall
};

/// Torches on the walls of every carved corridor, wherever it is ENCLOSED.
///
/// The user's ruling, not a proposal: «факела точно должны висеть в той
/// пещере». Placement uses the SAME roof predicate as the darkness gate, so a
/// torch exists exactly where the place goes dark and nowhere along the open
/// approach cutting — one definition, two consumers (Rule 35).
///
/// `ground` must be the sampler the darkness query uses, or the lights and the
/// dark would be derived from two different worlds.
[[nodiscard]] std::vector<CarveLightSite> carve_wall_lights(const TestbedLayout& layout,
                                                            const GroundSampler& ground);

} // namespace dfn::world
