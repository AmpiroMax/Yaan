/*
Created: 09:08:2026 - 11:05:22
Last updated: 17:08:2026 - 13:14:56
Module: engine/world
File: engine/world/sources/WorldgenSites.h

Responsibility:
- Worldgen v2 pass P4 (LANDSCAPE.md §6, §7.3): site placement — the hamlet's
  buildings around its common, shrine, dungeon entrances, tower ruin — each on
  a flattened building pad (BUILDING_PAD_SLOPE_MAX scorer), plus the corridor
  mask helper (§2.4) shared with P5/P6 and validation.

Key items:
- BuildingPad, SitesData, build_sites().
- pads_height(): the pad flatten stamp applied per sample.
- corridor_distance(): distance to the POI-chain corridor centerlines.

Dependencies:
- Uses: TestbedLayout.h, WorldgenHydrology.h, SiteComponents.h, Chunk.h
  (GeneratedEntityRecord), config.
- Used by: Worldgen.cpp, WorldgenScatter, WorldgenValidation, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM (Rule 13.1): placement rng = WorldGenRng stream STREAM_SITES;
  candidate loops have fixed attempt counts and deterministic tie-breaks.
  WorldEntityIds are sequential in (site, building) order — save deltas anchor
  to them (Q56); reordering sites is a worldgen_version bump.
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — P4 sites & pads.
- 09:08:2026 - 13:12:19: Stage 3b amendments: corridor_distance moved to TestbedLayout.h.
- 09:08:2026 - 15:18:34: Castle: SitesData carries the CastleBuild (its terrace is a separate square stamp with its own cut allowance).
- 09:08:2026 - 17:36:42: §6.2: EntranceWorks (mound + cut forecourt + derived adit) and entrance_works_height.
- 17:08:2026 - 11:35:28: BuildingPad стал прямоугольным (half_extents; ноль = прежний круг), и
  стамп вынесен в apply_pads — ОДНО определение, двое зовущих: площадки
  генератора и площадки КОМПОЗИЦИИ. Две копии стампа высоты были бы двумя
  ответами на «какая тут земля», и судья композитора мерил бы не тот мир, по
  которому ходит игрок. Терраса города 120x80 м, а круг такого сказать не
  может: вписанный теряет углы, через которые идут улицы, описанный ровняет то,
  что задумано нетронутым.
- 17:08:2026 - 13:14:56: RiverChannel + apply_rivers + river_water_surface — АВТОРСКИЙ ВОДОТОК.
  Полилиния несёт ОТМЕТКУ ВОДЫ на каждой станции: падение реки — решение
  дизайнера (где пороги, насколько канал ниже набережной), а не следствие земли,
  в которую река ещё не врезана. Одно описание даёт и ВРЕЗ, и ВОДУ: врезанная
  без воды река — сухая канава, вода без вреза — простыня на склоне.
*/

#pragma once

#include "engine/world/sources/Chunk.h"
#include "engine/world/sources/SiteComponents.h"
#include "engine/world/sources/TestbedLayout.h"
#include "engine/world/sources/WorldgenCastle.h"
#include "engine/world/sources/WorldgenHydrology.h"

#include <vector>

namespace dfn::world {

/// One flattened building pad (footprint x 1.5, LANDSCAPE §6): terrain inside
/// `radius` is leveled to `height`, blending back over `blend` meters.
struct BuildingPad {
    glm::vec2 center{0.0f};
    float radius = 0.0f;
    float blend = 0.0f;
    float height = 0.0f;
    /// RECTANGULAR when either half-extent is positive; `radius` is then
    /// ignored. A town terrace is 120 x 80 m and a circle cannot say that:
    /// inscribed it loses the corners the streets run through, circumscribed
    /// it flattens ground the design wanted left alone. Zero keeps the circle,
    /// so every pad the generator already places behaves exactly as before
    /// (Rule 26: the field grew, the meaning did not).
    glm::vec2 half_extents{0.0f};
};

/// Entrance works for one dungeon (LANDSCAPE §6.2). The generator MAKES THE
/// RELIEF IT NEEDS: on flat ground a chambered-barrow mound plus a cut
/// forecourt restores a hillside to face out of, so the same adit logic then
/// applies inside the mound flank. On ground that already has relief, only the
/// adit is cut. Entrances never use the building-pad scorer — flat and dry is
/// exactly where a cave mouth cannot exist.
struct EntranceWorks {
    bool valid = false;
    bool mounded = false;      ///< false = natural relief was sufficient
    glm::vec2 center{0.0f};    ///< mound centre / site anchor
    glm::vec2 outward{0.0f, 1.0f}; ///< approach direction, portal faces this way
    float mound_radius = 0.0f;
    float mound_height = 0.0f;
    glm::vec2 portal{0.0f};    ///< where the lintel sits, on the flank
    float portal_floor = 0.0f; ///< absolute floor height at the portal
    float forecourt_length = 0.0f;
    float forecourt_half_width = 0.0f;
    CarveCorridor adit{};      ///< derived passage in, mouth at the portal
};

/// P4 output: pads (terrain stamps) + site entity records with their types.
/// entities[i] pairs with types[i]; WorldEntityIds are 1-based sequential.
struct SitesData {
    std::vector<BuildingPad> pads;
    std::vector<GeneratedEntityRecord> entities;
    std::vector<SiteType> types;
    /// The castle (§6.1): its terrace is a separate square stamp with its own
    /// cut allowance, and its elements are appended to `entities`/`types`.
    CastleBuild castle;
    /// One per dungeon entrance (§6.2), in layout site order.
    std::vector<EntranceWorks> entrances;
};

/// Places all layout sites. Heights are sampled from macro + hydrology carve
/// (pads must not fight the river; buildings keep BUILDING_WATER_MARGIN).
[[nodiscard]] SitesData build_sites(uint64_t seed, const TestbedLayout& layout,
                                    const HydrologyData& hydro);

/// Applies the §6.2 entrance works (mound + cut forecourt) to terrain height
/// `h`. Runs BEFORE building pads and AFTER hydrology, so the mound is real
/// ground that scatter, slope and the heightfield all see.
[[nodiscard]] float entrance_works_height(const SitesData& sites, glm::vec2 world, float h);

/// Applies pad flattening to terrain height `h` at `world` (P4 stamp).
/// (corridor_distance moved to TestbedLayout.h — pure layout geometry, now
/// also consumed by hydrology ford beds.)
[[nodiscard]] float pads_height(const SitesData& sites, glm::vec2 world, float h);

/// The pad stamp itself, over an arbitrary list. ONE definition, two callers:
/// the generator's own pads (through pads_height) and the pads a COMPOSITION
/// authors. Two copies of a height stamp would be two answers to "how high is
/// the ground here", and the composer's judge would measure a different world
/// from the one the player walks (Rule 32).
[[nodiscard]] float apply_pads(const std::vector<BuildingPad>& pads, glm::vec2 world,
                               float h);

/// AN AUTHORED WATERCOURSE. `points` is a polyline in (x, z, WATER HEIGHT):
/// the third component is the surface of the water at that station, and it is
/// authored rather than derived because a river's fall is a design decision —
/// where the rapids are, how deep the town's canal sits below its quay.
///
/// Two things come out of one description, and that is the whole point: the
/// channel CUT into the ground and the WATER standing in it. A river authored
/// as terrain alone is a dry ditch; authored as water alone it is a sheet
/// lying on a hillside. They must be one statement or they drift.
struct RiverChannel {
    std::vector<glm::vec3> points; ///< x, z, water surface height (m)
    float width_m = 6.0f;   ///< of the channel bed
    float depth_m = 1.0f;   ///< bed below the water surface
    float bank_m = 6.0f;    ///< blend from the channel lip back to natural ground
};

/// Distance from `world` to the polyline, and the water height interpolated
/// along it at the nearest point. Returns false for a degenerate river.
[[nodiscard]] bool river_nearest(const RiverChannel& river, glm::vec2 world,
                                 float& distance_m, float& water_height);

/// Cuts every channel into height `h`. Applied AFTER the pads: a river runs
/// through a terrace, not under it — the user asked for one arm through the
/// town itself, and a pad that won over the water would fill its own canal.
[[nodiscard]] float apply_rivers(const std::vector<RiverChannel>& rivers,
                                 glm::vec2 world, float h);

/// The water surface at `world`, or math::NO_WATER where no channel reaches.
/// Read by the chunk builder so the sample is marked covered and the water is
/// actually drawn — the cut alone would only dig a dry ditch.
[[nodiscard]] float river_water_surface(const std::vector<RiverChannel>& rivers,
                                        glm::vec2 world);

} // namespace dfn::world
