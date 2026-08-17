/*
Created: 09:08:2026 - 00:42:03
Last updated: 17:08:2026 - 11:54:29
Module: engine/world
File: engine/world/sources/Worldgen.cpp

Responsibility:
- Worldgen v2 orchestration (Rule 13.1: same seed, byte-identical output):
  world context (P2 hydrology + P4 sites over the P1 macro field), per-chunk
  generation — final heights quantized in the shared WORLDGEN_MAX_HEIGHT
  range, P3 surface arrays, P4 entity records, P5 scatter instances.

Key items:
- WorldGenRng (SplitMix64), build_world_context, terrain_height,
  surface_point, generate_chunk, generate_world (file output still deferred).

Dependencies:
- Uses: Worldgen.h, WorldgenMacro/Hydrology/Sites/Scatter, WorldgenNoise,
  generated constants.
- Used by: dfn_world, worldgen tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM IS NON-NEGOTIABLE (Rule 13.1): all randomness flows from mix64
  streams; no std::rand, no platform-dependent paths.
- All chunks share ONE quantization range (offset 0, scale
  WORLDGEN_MAX_HEIGHT/65535) so shared edge samples decode identically across
  neighbors — exact-stitch guarantee of the HeightFieldView contract. Do not
  "optimize" to per-chunk min/max without a group sync (it would break edge
  equality). Range raised 31.5 -> 64 m at stage 3b (lead-acked, sync note).
- Heights/surface/classification are pure functions of (params, world_xz) —
  chunk-independent by construction. Keep it that way.
*/
/*
UPD:
- 09:08:2026 - 00:42:03: Stage 2 — value-noise gentle hills, SplitMix64 rng;
  generate_world returns a deferred-IO error until stage 3.
- 09:08:2026 - 11:05:22: Stage 3b — worldgen v2: P1 stamps via WorldgenMacro
  (octaves from dfn::config, local table deleted), P2 hydrology, P3 surface
  arrays, P4 site records, P5 scatter; WORLDGEN_MAX_HEIGHT quantization.
- 09:08:2026 - 13:12:19: Stage 3b amendments: grid-pass generate_chunk (water/heights once per node, slope from the grid, analytic border) — bit-identical to surface_point, ~5x fewer field evals; equality pinned by test.
- 09:08:2026 - 16:30:44: Representation swap: generate_chunk builds the voxel volume from the heightmap it just wrote, extracts the surface, and drops the volume.
- 09:08:2026 - 16:47:51: P7: carves passed to the volume build.
- 09:08:2026 - 17:36:42: §6.2: entrance works applied between hydrology and pads; derived adits passed to the voxel build.
- 09:08:2026 - 19:55:17: Barrow re-siting (design ruling): swing_barrow_into_couloir searches the arc for a re-entrant fold at the same radius and rigidly rotates site, passage and chamber together. On Ravenscar it finds nothing — the stamp is a smooth radial cone with no angular structure — so the barrow stays authored and its mouth test stays red. Design's high-shoulder fallback was implemented, MEASURED and then removed: it broke story's hard constraint (mouth visible from 26 of 39 Vaelmere standpoints) and put the lifted chamber through the crag tunnel (10 stations with no floor).
- 10:08:2026 - 02:29:54: build_world_context derives daylight portals (open_daylight_portals) after the couloir swing, against the pre-P4 sampler (macro + water carve) — same layout copy every consumer reads, so the extended corridor is one fact.
- 10:08:2026 - 02:59:28: Stand selector (§8.1): build_world_context branches for StandId::Forest — empty hydrology (a waterless stand's VALID P2, ok=true), empty sites; the stand's own passes land with the erosion/path commits. Testbed path untouched.
- 10:08:2026 - 10:40:28: LF-8 wired: build_world_context bakes the erosion delta for
  the forest stand against its own P1 field, and terrain_height composes
  macro + delta on that stand's pass stack (no water carve / entrance works /
  pads — the stand declares none of them, and running their no-ops here would
  invite one to stop being a no-op unnoticed). Testbed path untouched.
- 10:08:2026 - 10:52:15: §8.1 paths wired: the network routes on the ground it will be
  BUILT ON (macro + LF-8 erosion, not macro alone — routing on the pre-erosion
  field would lay treads across gullies the router never saw), and
  terrain_height composes macro + erosion + the flatten delta.
- 10:08:2026 - 10:55:03: BR-6 find layer built on the FINISHED ground (macro + erosion
  + path flattening) — occlusion siting against terrain that does not ship is
  siting against nothing.
- 10:08:2026 - 19:45:47: THE DRAWN GROUND AND THE PLACED GROUND WERE TWO
  GROUNDS. generate_chunk carried its own copy of the pass stack, and when the
  forest stand's branch (LF-8 erosion, then the path flatten) landed in
  terrain_height, the copy was never told: everything PLACED read
  terrain_height, everything DRAWN and COLLIDED came from the heightmap written
  here. Measured on stand 0, chunk (1,2): stored heightmap minus placed field
  -1.5000..+1.5028 m (the erosion overlay's own clamp, appearing verbatim, which
  is the control) plus a further +0.166 m median on every path tread (exactly
  PATH_GROOVE_DEPTH -- the groove was not in the drawn world at all), and the
  worst tread clearance against the drawn surface -0.663 m. Fixed by extracting
  compose_passes() as the ONE statement of what the finished ground is; both
  terrain_height and generate_chunk call it, the second passing the water sample
  it already has so no macro evaluation is added. After: heightmap agrees with
  the placed field to +-0.0031 m (one quantization step), worst tread clearance
  +0.100 m, median +0.146 m against a 0.15 m groove. Rule 35's state clause --
  two copies drift whether they are numbers or passes.
  NOT A VOXEL DEFECT: VOXEL_SIZE 1.0 m vs PATH_GROOVE_DEPTH 0.15 m was the
  standing diagnosis and it was wrong. Surface nets reproduces the height field
  to +-0.03 m on open ground; a vertex-snap refinement of the extractor was
  written, measured against the un-refined arm, found identical to four decimals
  and DROPPED rather than shipped as a fix for nothing.
- 11:08:2026 - 15:15:55: §2.7 general relief and §10.5 B2 outcrops compose HERE, once, before the works and pads. AND surface_point() was a FOURTH open-coded copy of the pass stack -- the one that classifies the ground's MATERIAL, so its drift would have read as a splat seam rather than as a failing height test. It calls compose_passes now.
- 12:08:2026 - 22:55:00: place_great_oaks() runs LAST in build_world_context: the
  giant's gates read the pads, the entrance works and the L0 sight wedges, so it
  must be sited against the finished world rather than against the field the
  sites themselves were placed on.
- 13:08:2026 - 16:35:00: §10.1.3 THE FORMS composed here (WorldgenForms.h),
  between the meso tier and the micro tier, with two clauses that were both
  bought by a measurement. (1) The forms are ADDENDA to the unformed ground and
  never a re-derivation of it, so at strength 0 the expression collapses to the
  old one BIT FOR BIT and the pinned testbed digest still holds — regrouping
  (meso+micro)*mask into meso*mask+micro*mask moved that digest by itself, and a
  control that has already moved certifies nothing. (2) The forms' mask is the
  relief mask TIMES meso_scale, because meso_scale is where в9's authored glade
  taper lives and a calm plain is exactly where a scarp must not grow (measured
  when it did: glade relief 3.18 m against a 3.0 bound). Plus the flood guard,
  conditioned on the ground having been DRY — written without that clause it
  became a floor rather than a guard and silently deleted every cut in the
  world.
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
- 13:08:2026 - 00:40:00: the drainage is built on the MACRO landform (the same field hydrology solved on, so valleys and rivers agree which way the country drains) and applied in compose_passes BEFORE the draws and benches, carrying the same mask, so those two operators bank the valley instead of competing with it. It does NOT feed back into pond levels, ford depths or pads -- those were solved against the un-incised field, exactly the rule the forms went in under.
- 14:08:2026 - 22:27:28: Ветки Forest переведены на stand_is_floral() (OneTree едет тем же путём);
  для OneTree контекст возвращается сразу после пустой гидрологии — без эрозии,
  троп и находок: пустые результаты — валидные результаты этого стенда, и ни
  одному потребителю не нужна проверка стенда (правило 32). Тестбед побитово
  не тронут (страж — закреплённый дайджест карты высот).
- 17:08:2026 - 11:35:28: площадки композиции применяются ПОСЛЕДНИМИ и НА ВСЕХ СТЕНДАХ, в
  отличие от площадок генератора: город компонуется на том стенде, который
  назвала его карта, и фича, тихо не работающая на половине стендов, была бы
  обнаружена тем, кто строит город не на том. При пустом списке строка — no-op
  бит в бит, поэтому закреплённый дайджест тестбеда цел.
- 17:08:2026 - 11:53:47: износ тропы пишется в поверхность чанка ТЕМ ЖЕ сэмплом сети, которым
  compose_passes продавливает землю: взять его из другого места значило бы
  положить цвет тропы РЯДОМ с её формой, а не на неё. И число маршрутов
  печатается вслух — «тропы нет» и «тропа не нарисована» на кадре неразличимы,
  и зоны уже потратили на это два круга.
- 17:08:2026 - 11:54:29: износ тропы пишется в поверхность чанка ТЕМ ЖЕ сэмплом сети, которым
  compose_passes продавливает землю: взять его из другого места значило бы
  положить цвет тропы РЯДОМ с её формой, а не на неё. И число маршрутов
  печатается вслух — «тропы нет» и «тропа не нарисована» на кадре неразличимы.
*/

#include "engine/world/sources/Worldgen.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenCarve.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenForest.h"
#include "engine/world/sources/WorldgenForms.h"
#include "engine/world/sources/WorldgenOutcrop.h"
#include "engine/world/sources/WorldgenRelief.h"
#include "engine/world/sources/WorldgenNoise.h"
#include "engine/world/sources/VoxelMesh.h"
#include "engine/world/sources/VoxelVolume.h"
#include "engine/world/sources/WorldgenScatter.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <glm/geometric.hpp>
#include <vector>

namespace dfn::world {

namespace {

constexpr uint32_t RESOLUTION = static_cast<uint32_t>(config::HEIGHTMAP_RESOLUTION);
constexpr float STEP_M = static_cast<float>(config::HEIGHTMAP_STEP);
constexpr float CHUNK_SIZE_M = static_cast<float>(config::CHUNK_SIZE);
constexpr float MAX_HEIGHT_M = static_cast<float>(config::WORLDGEN_MAX_HEIGHT);
constexpr float SLOPE_GRASS = static_cast<float>(config::SLOPE_GRASS_MAX);
constexpr float SLOPE_ROCK = static_cast<float>(config::SLOPE_ROCK_MIN);
constexpr float SAND_DIST = static_cast<float>(config::SHORE_SAND_DIST);
constexpr float SAND_HEIGHT = static_cast<float>(config::SHORE_SAND_HEIGHT);

} // namespace

// --- WorldGenRng --------------------------------------------------------------

WorldGenRng WorldGenRng::for_chunk(uint64_t seed, ChunkCoord coord, uint32_t pass_tag) {
    uint64_t s = noise::mix64(seed ^ 0x8BADF00D5EEDC0DEull);
    s = noise::mix64(s ^ static_cast<uint64_t>(static_cast<uint32_t>(coord.x)));
    s = noise::mix64(s ^ static_cast<uint64_t>(static_cast<uint32_t>(coord.z)));
    s = noise::mix64(s ^ pass_tag);
    return WorldGenRng{s};
}

uint64_t WorldGenRng::next_u64() {
    state += 0x9E3779B97F4A7C15ull;
    uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

float WorldGenRng::next_float01() {
    return static_cast<float>(next_u64() >> 40) * (1.0f / 16777216.0f);
}

uint32_t WorldGenRng::next_range(uint32_t min, uint32_t max) {
    const uint64_t span = static_cast<uint64_t>(max) - min + 1;
    // Rejection sampling: no modulo bias.
    const uint64_t limit = UINT64_MAX - (UINT64_MAX % span);
    uint64_t v = next_u64();
    while (v >= limit) {
        v = next_u64();
    }
    return min + static_cast<uint32_t>(v % span);
}

// --- World-level context -------------------------------------------------------

namespace {

/// §7.1 coordinates are stamps against a SPECIFIC terrain state: anything sited
/// on a landmark's slopes carries an implicit dependency on that landmark's
/// relief. Raising Ravenscar 52 -> 115 m buried the Backbarrow under 20-44 m of
/// its own mountain. Design's ruling: the barrow does not move OUTWARD (that
/// drags the castle with it and cascades five ways), it moves AROUND — into a
/// couloir, one of the re-entrant folds a ridged stamp produces between its
/// buttress ridges, where ground at the SAME radius is still near valley level.
/// A rigid rotation about the crag centre carries the authored passage and
/// chamber with the site, so the barrow keeps its designed shape.
void swing_barrow_into_couloir(TestbedLayout& layout, uint64_t seed,
                               const HydrologyData& hydro) {
    const int si = layout.carves.barrow_site_index;
    if (si < 0 || si >= static_cast<int>(std::size(layout.sites))) {
        return;
    }
    const glm::vec2 cc = layout.crag.center;
    const glm::vec2 rel = layout.sites[si].position - cc;
    const float radius = glm::length(rel);
    if (radius < 1.0f || radius > layout.crag.radius) {
        return; // not sited on the massif
    }
    const auto ground = [&](glm::vec2 q) {
        return carve_height(hydro, layout, q, macro_height(seed, layout, q));
    };
    // Valley reference along this bearing, just outside the stamp.
    const glm::vec2 dir = rel / radius;
    const float valley = ground(cc + dir * (layout.crag.radius + 40.0f));
    const float threshold = valley + 8.0f;
    if (ground(layout.sites[si].position) <= threshold) {
        return; // already clear; nothing to swing
    }
    // Search the arc outward from the authored bearing so the FIRST hit is the
    // nearest one — the barrow moves as little as the mountain allows.
    const float base = std::atan2(rel.y, rel.x);
    float found = 0.0f;
    bool ok = false;
    for (float dev = 2.0f * 0.0174532925f; dev <= 30.0f * 0.0174532925f && !ok;
         dev += 2.0f * 0.0174532925f) {
        for (const float sign : {1.0f, -1.0f}) {
            const float ang = base + sign * dev;
            const glm::vec2 probe = cc + glm::vec2{std::cos(ang), std::sin(ang)} * radius;
            if (ground(probe) <= threshold) {
                found = sign * dev;
                ok = true;
                break;
            }
        }
    }
    if (!ok) {
        // NO COULOIR EXISTS on this massif, and design's high-shoulder
        // fallback is NOT taken, because measuring it showed it breaks two
        // things rather than one:
        //   (a) story's hard constraint — a mouth lifted to ~39 m is visible
        //       from 26 of 39 Vaelmere standpoints, and MQ1 depends on the
        //       player FINDING the grave rather than seeing it from town;
        //   (b) the lifted chamber intersects the crag switchback tunnel,
        //       leaving 10 stations of ascent with no floor underfoot.
        // The couloir search itself is correct and stays: Ravenscar simply has
        // no angular structure to search — its stamp is a smooth radial cone
        // (measured: identical lobe ratio at every height), so re-entrant folds
        // do not exist yet. Once the pending crag SHAPE ruling adds angular
        // ridge modulation, couloirs appear and this finds one. Until then the
        // barrow stays where it was authored and its mouth test stays red,
        // which is the honest state.
        return;
    }
    // Rigid rotation about the crag centre: site, passage and chamber together.
    const float cs = std::cos(found);
    const float sn = std::sin(found);
    const auto swing = [&](glm::vec2 q) {
        const glm::vec2 r = q - cc;
        return cc + glm::vec2{r.x * cs - r.y * sn, r.x * sn + r.y * cs};
    };
    layout.sites[si].position = swing(layout.sites[si].position);
    CarveCorridor& passage = layout.carves.barrow_passage;
    for (int i = 0; i < passage.point_count; ++i) {
        const glm::vec2 xz = swing({passage.points[i].x, passage.points[i].z});
        passage.points[i].x = xz.x;
        passage.points[i].z = xz.y;
    }
    CarveChamber& ch = layout.carves.barrow_chamber;
    const glm::vec2 cxz = swing({ch.center.x, ch.center.z});
    ch.center.x = cxz.x;
    ch.center.z = cxz.y;
}

} // namespace

WorldGenContext build_world_context(const WorldGenParams& params) {
    WorldGenContext ctx;
    ctx.params = params;
    const glm::vec2 domain_min{static_cast<float>(params.min_chunk.x) * CHUNK_SIZE_M,
                               static_cast<float>(params.min_chunk.z) * CHUNK_SIZE_M};
    const glm::vec2 domain_max{static_cast<float>(params.max_chunk.x + 1) * CHUNK_SIZE_M,
                               static_cast<float>(params.max_chunk.z + 1) * CHUNK_SIZE_M};
    if (stand_is_floral(params.layout.stand)) {
        // §8.1: the floral stands declare NO water landform (LF-3/LF-6 absent
        // from their composition), so P2 stays empty — water_at then passes
        // heights through and reports far-field distance everywhere. ok=true
        // because an empty hydrology is these stands' VALID hydrology, not a
        // failed trace. P4 sites stay empty too: the forest stand's goals
        // belong to the §8.1 path network (built in the stand passes below),
        // not to the testbed site table.
        ctx.hydrology.ok = true;
        if (params.layout.stand == StandId::OneTree) {
            // The inspection stand keeps nothing else: no erosion (declared
            // off in its layout — the zero grid answers 0), no paths, no
            // finds (the empty network answers "far from any path"). Empty
            // results are this stand's VALID results, so no consumer needs a
            // stand check (Rule 32).
            return ctx;
        }
        // LF-8 (в17): the overlay runs against the stand's P1 field, ONCE, and
        // is baked. `layout.erosion == false` returns the zero grid from the
        // same entry point — the dictionary's named control.
        {
            const uint64_t seed = params.seed;
            const TestbedLayout& lay = ctx.params.layout;
            ctx.erosion = build_erosion(
                seed, domain_min, domain_max, ErosionParams{},
                [&](glm::vec2 p) { return macro_height(seed, lay, p); }, lay.erosion);
            // The path network routes on the GROUND IT WILL BE BUILT ON —
            // macro + erosion, not macro alone. Routing on the pre-erosion
            // field would put treads across gullies that the shipped terrain
            // has and the router never saw.
            const ErosionGrid& ero = ctx.erosion;
            ctx.paths = build_path_network(seed, lay, domain_min, domain_max, PathParams{},
                                           [&](glm::vec2 p) {
                                               return macro_height(seed, lay, p) + ero.sample(p);
                                           });
            // SAY WHETHER THERE ARE ANY. A path that does not exist and a path
            // that is not drawn look identical from a screenshot, and the zones
            // have now spent two rounds on which of the two they were seeing.
            std::fprintf(stderr, "[paths] %zu route(s) on this world\n",
                         ctx.paths.routes.size());
            // BR-6 (в20): the find layer is seeded on the FINISHED ground —
            // the occlusion siting must see the erosion and the path treads,
            // or a find is placed against terrain that does not ship.
            const PathNetwork& pn = ctx.paths;
            ctx.finds = build_finds(seed, pn, domain_min, domain_max, FindParams{},
                                    [&](glm::vec2 p) {
                                        const float e = macro_height(seed, lay, p)
                                                      + ero.sample(p);
                                        return e + pn.flatten_at(p, e);
                                    });
        }
        return ctx;
    }
    ctx.hydrology = build_hydrology(params.seed, params.layout, domain_min, domain_max);
    // THE DRAINAGE, and the position in this sequence is a decision rather than
    // a convenience. It is built on the MACRO LANDFORM — the same field
    // hydrology solved on — because the valleys have to agree with the rivers
    // about which way this country drains; building it on the post-carve field
    // would let the carve's own trench recruit the whole catchment into itself.
    //
    // It is built AFTER hydrology and applied only in compose_passes, i.e. it
    // does NOT feed back into pond levels, ford depths or site pads. That is
    // deliberate for today: the flat-reach and ford contracts were solved
    // against the un-incised field, and a pass that moved them would have to
    // re-derive all of them in the same commit. The forms went in under exactly
    // this rule and it held.
    {
        const uint64_t seed = params.seed;
        const TestbedLayout& lay = ctx.params.layout;
        ctx.flow = build_flow(seed, domain_min, domain_max, FlowParams{},
                              [&](glm::vec2 p) { return macro_height(seed, lay, p); },
                              std::getenv("DFN_FLOW_OFF") == nullptr);
    }
    // Re-validate placements that sit on the L0's slopes BEFORE anything is
    // sited against them (design's durable rule: re-validation is part of a
    // landmark change, not a follow-up).
    swing_barrow_into_couloir(ctx.params.layout, params.seed, ctx.hydrology);
    // Derive the daylight ends of flagged carve corridors against the terrain
    // that actually ships (macro + water carve — the same pre-P4 sampler the
    // carve mouths use). The §2.8 massif re-buried the surveyed tunnel exit;
    // an endpoint that must stand in open air is derived, never re-surveyed.
    {
        const uint64_t seed = params.seed;
        const TestbedLayout& lay = ctx.params.layout;
        const HydrologyData& hydro = ctx.hydrology;
        open_daylight_portals(ctx.params.layout, [&](glm::vec2 p) {
            return carve_height(hydro, lay, p, macro_height(seed, lay, p));
        });
    }
    ctx.sites = build_sites(params.seed, ctx.params.layout, ctx.hydrology);
    // GIANT_OAKS §2 — LAST, and the order is the whole reason it is here and
    // not in the scatter pass: the giant's own gates read the pads, the
    // entrance works and the L0 sight wedges, so it must be sited against the
    // finished world rather than against the field the sites were placed on.
    ctx.great_oaks = place_great_oaks(ctx);
    // TORCHES ON THE CORRIDOR WALLS (the user's ruling). LAST, and against the
    // FINISHED height field: they are placed by the same roof predicate the
    // darkness gate uses, and that gate reads terrain_height(ctx, ...), so
    // deriving them from anything earlier would light a different world than
    // the one that goes dark.
    {
        const auto lights = carve_wall_lights(ctx.params.layout, [&ctx](glm::vec2 p) {
            return terrain_height(ctx, p);
        });
        const uint64_t archetype = serialization::fnv1a64(
            site_archetype(SiteType::WallTorch).content_id);
        for (const CarveLightSite& l : lights) {
            GeneratedEntityRecord rec;
            rec.world_id = static_cast<WorldEntityId>(ctx.sites.entities.size() + 1);
            rec.archetype = archetype;
            rec.position_xz = {l.position.x, l.position.z};
            rec.yaw = l.yaw;
            // EXPLICIT Y, and this is the case that field exists for: a sconce
            // hangs on a wall inside the rock, and the heightfield does not
            // know carves exist.
            rec.ground_y = l.position.y;
            ctx.sites.entities.push_back(rec);
            ctx.sites.types.push_back(SiteType::WallTorch);
        }
    }
    return ctx;
}

/// THE PASS STACK — see the contract in Worldgen.h. It is a function rather
/// than three copies because it WAS three copies (chunk builder, coarse node
/// builder, and this), and two of them were never told when the forest stand's
/// branch landed.
float compose_passes(const WorldGenContext& ctx, glm::vec2 world, float macro,
                     const WaterSample& water) {
    // §2.7 THE GENERAL GROUND RELIEF, and it goes HERE for a reason that was
    // measured rather than chosen. Inside the macro step it perturbs the field
    // hydrology is solved on, which is how the first attempt moved the
    // shoreline; after the carve it cannot, and the shore taper keeps the bank
    // itself flat. Before the works and the pads, so anything cut flat on
    // purpose still wins (§10.1.2 exempts exactly that list from the σ floor).
    const float meso_scale = stand_is_floral(ctx.params.layout.stand)
                                 ? glade_factor(ctx.params.layout, world)
                                 : 1.0f;
    // SPLIT INTO TIERS because §10.1.3's bench/riser operator goes BETWEEN
    // them (WorldgenForms.h): it reshapes the meso tier and the macro under it,
    // and the micro tier is laid on the result. `relief` below is the
    // structural half — meso + rock — and micro is added after the forms.
    const ReliefTiers tiers = ground_relief_tiers(ctx.params.seed, ctx.params.layout, world,
                                                  water.dist_to_water, meso_scale);
    // GROUPED EXACTLY AS ground_relief() GROUPS IT — (meso + micro) * mask, one
    // multiply — and that is a bit-level requirement rather than a style: with
    // the forms at identity this function must reproduce the pinned testbed
    // heightmap BYTE FOR BYTE, which is the only thing that proves a terrain
    // difference belongs to the forms and not to the plumbing. Regrouping it as
    // meso*mask + micro*mask moved the digest by itself (float addition is not
    // associative), and a control that has already moved cannot certify
    // anything.
    float relief = (tiers.meso + tiers.micro) * tiers.mask;
    float rock = 0.0f;
    // §10.5 B2 — the rock. It is terrain and not scatter (§10.2's seam: the
    // heightmap owns 4 m and up), so it composes here with everything else that
    // decides what the ground IS. Multiplied by the same shore mask the relief
    // carries, because a rock rising out of the bank has the same effect on the
    // §3.3 bed/mud cap that a 0.5 m dip had.
    {
        const float shore_ok =
            std::clamp(water.dist_to_water / static_cast<float>(config::SHORE_SAND_DIST), 0.0f,
                       1.0f);
        // meso_scale carries в9's glade taper here too: a 5 m boss inside the
        // ONE authored calm plain is exactly what that plain exists not to be,
        // and §10.1.2's own exemption logic says a place an approved rule keeps
        // flat is not where the bumpiness contract binds.
        rock = outcrop_height(ctx.params.seed, ctx.params.layout, world) * shore_ok * meso_scale;
        relief += rock;
    }

    // §10.1.3 THE FORMS — benches and their risers. It is applied to the
    // STRUCTURAL ground (everything that decides where the land lies) and
    // BEFORE the micro tier, the works and the pads: an authored flat still
    // wins, and §2.7's "flat, not sterile" octave survives on top of a bench
    // instead of being ironed into it. One application site, exactly as the
    // relief it reshapes has one.
    //
    // THE FORMS' MASK IS THE RELIEF MASK **TIMES meso_scale**, and the second
    // factor is not decoration: meso_scale carries в9's authored glade taper,
    // and a glade that an approved rule keeps calm is exactly where a scarp
    // must not appear. Measured when it did — the forest stand's glade relief
    // read 3.18 m against its own 3.0 m bound and the LF-2 control 2.50 against
    // 2.00, i.e. the calm plain stopped being calm. §10.1.2's exemption list
    // and this mask are the same list, which is the property that has to hold.
    const float form_mask = tiers.mask * std::clamp(meso_scale, 0.0f, 1.0f);
    const float ground = stand_is_floral(ctx.params.layout.stand)
                             ? macro + ctx.erosion.sample(world)
                             : water.height;
    // THE UNFORMED GROUND — the world exactly as it was before this pass. The
    // two forms are ADDENDA to it, never a re-derivation of it, so that at
    // strength 0 the sum collapses to this expression unchanged.
    const float unformed = ground + relief;
    // What the bench operator READS is the structural half without the micro
    // tier: the operator multiplies a bench's gradient by (1 - strength), and
    // §2.7's "flat, not sterile" octave must survive on top of a bench rather
    // than be ironed into it. Reading a different height from the one it is
    // added to is deliberate and is why the identity above still holds.
    const float structural = ground + tiers.meso * tiers.mask + rock;
    // THE DRAINAGE INCISION. It carries the SAME mask the forms carry, so the
    // shore band, the graded corridors, the massif hem and в9's authored glade
    // are exempt from a valley exactly as they are exempt from a bench — one
    // exemption list, not two that have to be kept in step.
    //
    // It is applied BEFORE the draws and the benches on purpose: those two
    // operators sharpen whatever relief they are given, so running them on the
    // incised ground makes them bank the valley instead of competing with it.
    const float d_flow = ctx.flow.sample(world) * form_mask;
    const float d_draw = draw_forms(ctx.params.seed, world, form_mask);
    const float d_terrace =
        terrace_forms(ctx.params.seed, world, structural + d_flow + d_draw, form_mask);
    float benched = unformed + d_flow + d_draw + d_terrace;

    // A FORM NEVER CUTS THE GROUND BELOW THE WATER STANDING BESIDE IT.
    //
    // The shore mask tapers the forms out across SHORE_SAND_DIST, which is the
    // right rule for AMPLITUDE and not a sufficient one for a CUT: a 2.6 m
    // incision seven metres from a bank reaches under the surface, and the
    // result is not a subtle one — it is a scatter instance standing in water
    // and a pond with a trench leaving it (measured: WorldgenV2Tests' "nothing
    // scattered stands in water" went red with exactly one instance under a
    // plane). So the clamp is stated against the water level itself rather
    // than against a distance that only correlates with it.
    //
    // THE GUARD IS CONDITIONED ON THE GROUND HAVING BEEN DRY, and that clause is
    // the whole rule rather than a refinement of it. Written without it — as a
    // plain max() against the nearest body's level — it stopped being a flood
    // guard and became a FLOOR: near_level is reported for the nearest body
    // whatever the distance, so every point of a lowland lying below the lake's
    // surface got raised to its own unformed height, which is to say every cut
    // in the world was deleted. It read as the pass having no effect at all
    // (A1 median fell 2 -> 0 with the operator at full strength), and it took a
    // re-measure to catch, because nothing about it looks wrong.
    if (water.near_level != math::NO_WATER && unformed > water.near_level) {
        benched = std::max(benched, water.near_level);
    }

    float ground_final = 0.0f;
    if (stand_is_floral(ctx.params.layout.stand)) {
        // The floral pass stack: P1 + LF-8 erosion + §2.7 relief + the path
        // flatten. No water carve, no entrance works, no generator pads —
        // these stands declare none of them, and running their no-ops here
        // would only invite one to stop being a no-op unnoticed. For OneTree
        // every term but P1 is the empty pass answering zero, by construction.
        ground_final = benched + ctx.paths.flatten_at(world, benched);
    } else {
        const float worked = entrance_works_height(ctx.sites, world, benched);
        ground_final = pads_height(ctx.sites, world, worked);
    }
    // THE COMPOSITION'S OWN PADS, LAST AND ON EVERY STAND. An authored terrace
    // is the strongest statement anyone makes about the ground — a composer who
    // cut a flat there meant it — so nothing downstream may argue with it.
    //
    // It is applied to BOTH branches deliberately, unlike the generator's pads:
    // a town is composed on whatever stand its map names, and a feature that
    // silently did nothing on half the stands would be discovered by someone
    // building a city on the wrong one. With an empty list this line is a
    // no-op bit for bit, which is what keeps the pinned testbed digest intact.
    ground_final = apply_pads(ctx.params.composed_pads, world, ground_final);
    return std::clamp(ground_final, 0.0f, MAX_HEIGHT_M);
}

float terrain_height(const WorldGenContext& ctx, glm::vec2 world) {
    const float macro = macro_height(ctx.params.seed, ctx.params.layout, world);
    return compose_passes(ctx, world, macro,
                          water_at(ctx.hydrology, ctx.params.layout, world, macro));
}

float terrain_slope(const WorldGenContext& ctx, glm::vec2 world) {
    // Central differences of the FINAL height field (position-based — identical
    // on shared chunk edges even though neighbors lie outside the chunk being
    // generated, and identical at every LOD level for the same reason).
    const float hx = terrain_height(ctx, {world.x + STEP_M, world.y})
                   - terrain_height(ctx, {world.x - STEP_M, world.y});
    const float hz = terrain_height(ctx, {world.x, world.y + STEP_M})
                   - terrain_height(ctx, {world.x, world.y - STEP_M});
    return std::atan(std::sqrt(hx * hx + hz * hz) / (2.0f * STEP_M));
}

math::SurfaceClass classify_surface(const TestbedLayout& layout, glm::vec2 world,
                                    float height, const WaterSample& water,
                                    float slope_rad) {
    // Priority rules per LANDSCAPE §4 (first match wins).
    const bool covered = water.water_surface != math::NO_WATER && height < water.water_surface;
    if (covered) {
        return math::SurfaceClass::WaterBed;
    }
    if (water.dist_to_water <= SAND_DIST && water.near_level != math::NO_WATER
        && height - water.near_level <= SAND_HEIGHT) {
        return math::SurfaceClass::Sand;
    }
    if (slope_rad >= SLOPE_ROCK
        || (crag_distance(layout, world) < layout.crag.radius
            && height >= layout.crag.rockline)) {
        return math::SurfaceClass::Rock;
    }
    if (slope_rad >= SLOPE_GRASS) {
        return math::SurfaceClass::GrassRockBlend;
    }
    return math::SurfaceClass::Grass;
}

SurfacePoint surface_point(const WorldGenContext& ctx, glm::vec2 world) {
    const TestbedLayout& layout = ctx.params.layout;
    const float macro = macro_height(ctx.params.seed, layout, world);
    const WaterSample water = water_at(ctx.hydrology, layout, world, macro);
    // A FOURTH COPY OF THE PASS STACK LIVED HERE, and the extraction that
    // deduplicated the other three never looked at it. It said
    // "water -> works -> pads -> clamp" — right for the testbed on the day it
    // was written, silently wrong on the forest stand (no erosion, no path
    // tread) and wrong everywhere once §2.7's relief landed. It is what
    // classifies the ground's MATERIAL, so the drift showed up as splat
    // disagreeing with geometry rather than as a failing height test.
    const float h = compose_passes(ctx, world, macro, water);

    SurfacePoint out;
    out.height = h;
    out.dist_to_water = water.dist_to_water;
    const bool covered = water.water_surface != math::NO_WATER && h < water.water_surface;
    out.water_surface = covered ? water.water_surface : math::NO_WATER;
    out.surface_class = classify_surface(layout, world, h, water, terrain_slope(ctx, world));
    return out;
}

// --- Generation ----------------------------------------------------------------

Chunk generate_chunk(const WorldGenContext& ctx, ChunkCoord coord) {
    Chunk chunk;
    chunk.coord = coord;

    Heightmap& hm = chunk.heightmap;
    // The SHARED quantization (Chunk.h): every chunk and every coarse LOD node
    // decode with the same offset/scale, which is what makes a sample two grids
    // share bit-identical rather than merely close.
    hm.height_offset = HEIGHT_QUANT_OFFSET;
    hm.height_scale = HEIGHT_QUANT_SCALE;
    const std::size_t sample_count = static_cast<std::size_t>(RESOLUTION) * RESOLUTION;
    hm.samples.resize(sample_count);
    chunk.surface.dist_to_water.resize(sample_count);
    chunk.surface.water_surface.resize(sample_count);
    chunk.surface.surface_class.resize(sample_count);
    // Sized only when this world HAS a path network: an empty vector is the
    // documented "no paths here", and every reader treats it as all-zero.
    if (!ctx.paths.routes.empty()) {
        chunk.surface.path_wear.resize(sample_count);
    }

    const glm::vec2 origin{static_cast<float>(coord.x) * CHUNK_SIZE_M,
                           static_cast<float>(coord.z) * CHUNK_SIZE_M};
    const auto world_at = [&](uint32_t x, uint32_t z) {
        return origin
             + glm::vec2{static_cast<float>(x) * STEP_M, static_cast<float>(z) * STEP_M};
    };
    const TestbedLayout& layout = ctx.params.layout;

    // Grid-pass generation (bit-identical to per-sample surface_point calls —
    // every value below is the same pure position-based function, evaluated
    // once instead of five times per sample):
    // pass A: water samples (carve heights) + final heights per grid node.
    std::vector<WaterSample> water(sample_count);
    std::vector<float> final_h(sample_count);
    for (uint32_t z = 0; z < RESOLUTION; ++z) {
        for (uint32_t x = 0; x < RESOLUTION; ++x) {
            const glm::vec2 world = world_at(x, z);
            const std::size_t i = static_cast<std::size_t>(z) * RESOLUTION + x;
            const float macro = macro_height(ctx.params.seed, layout, world);
            water[i] = water_at(ctx.hydrology, layout, world, macro);
            // THE PASS STACK HAS ONE COPY (compose_passes), and this is why.
            // It used to be spelled out here as well, and when the forest
            // stand's branch (LF-8 erosion, then the path flatten) landed in
            // terrain_height, THIS copy was not told. The result was two
            // different grounds: everything PLACED by height read
            // terrain_height, and everything DRAWN and COLLIDED came from the
            // heightmap written here — measured on stand 0 as the stored
            // heightmap standing -1.50..+1.50 m off the placed field (exactly
            // the erosion overlay's clamp, which is the control) plus a further
            // +0.166 m median on every path tread (exactly PATH_GROOVE_DEPTH).
            // Rule 35's state clause: two copies drift whether they are numbers
            // or passes.
            final_h[i] = compose_passes(ctx, world, macro, water[i]);
        }
    }
    // pass B: quantize + classify. Slope uses the grid where the +-STEP
    // neighbor is inside the chunk and the analytic field on the border —
    // identical floats either way (position-based), so shared edges agree.
    for (uint32_t z = 0; z < RESOLUTION; ++z) {
        for (uint32_t x = 0; x < RESOLUTION; ++x) {
            const glm::vec2 world = world_at(x, z);
            const std::size_t i = static_cast<std::size_t>(z) * RESOLUTION + x;
            const float h = final_h[i];
            hm.samples[i] = quantize_height(h);

            const auto h_at = [&](int32_t nx, int32_t nz) {
                if (nx >= 0 && nz >= 0 && nx < static_cast<int32_t>(RESOLUTION)
                    && nz < static_cast<int32_t>(RESOLUTION)) {
                    return final_h[static_cast<std::size_t>(nz) * RESOLUTION
                                   + static_cast<std::size_t>(nx)];
                }
                return terrain_height(ctx, {world.x + static_cast<float>(nx - static_cast<int32_t>(x)) * STEP_M,
                                            world.y + static_cast<float>(nz - static_cast<int32_t>(z)) * STEP_M});
            };
            const int32_t ix = static_cast<int32_t>(x);
            const int32_t iz = static_cast<int32_t>(z);
            const float hx = h_at(ix + 1, iz) - h_at(ix - 1, iz);
            const float hz = h_at(ix, iz + 1) - h_at(ix, iz - 1);
            const float slope = std::atan(std::sqrt(hx * hx + hz * hz) / (2.0f * STEP_M));

            const WaterSample& w = water[i];
            const bool covered = w.water_surface != math::NO_WATER && h < w.water_surface;
            chunk.surface.dist_to_water[i] = w.dist_to_water;
            chunk.surface.water_surface[i] = covered ? w.water_surface : math::NO_WATER;

            chunk.surface.surface_class[i] =
                static_cast<uint8_t>(classify_surface(layout, world, h, w, slope));
            if (!chunk.surface.path_wear.empty()) {
                // THE SAME SAMPLE THE GROUND WAS FLATTENED BY. compose_passes
                // grooves the terrain with this network; taking the wear from
                // anywhere else would put the trodden colour beside the trodden
                // shape instead of on it.
                chunk.surface.path_wear[i] = ctx.paths.sample(world).wear;
            }
        }
    }

    // P4 entities whose position falls inside this chunk (half-open bounds —
    // no duplicates across neighbors). Order preserved => deterministic ids.
    const glm::vec2 chunk_max = origin + glm::vec2{CHUNK_SIZE_M, CHUNK_SIZE_M};
    for (const GeneratedEntityRecord& rec : ctx.sites.entities) {
        if (rec.position_xz.x >= origin.x && rec.position_xz.x < chunk_max.x
            && rec.position_xz.y >= origin.y && rec.position_xz.y < chunk_max.y) {
            chunk.entities.push_back(rec);
        }
    }

    // P5 scatter instances for this chunk.
    chunk.scatter = build_scatter(ctx, origin, chunk_max);

    // 3D terrain: build the voxel volume from the heightmap just written, then
    // extract its surface and DROP the volume — the world is not destructible,
    // so only the geometry stays resident.
    {
        std::vector<CarveCorridor> derived;
        for (const EntranceWorks& w : ctx.sites.entrances) {
            if (w.valid && w.adit.point_count > 1) {
                derived.push_back(w.adit);
            }
        }
        const VoxelVolume volume = build_voxel_volume(
            chunk, [&ctx](glm::vec2 p) { return terrain_height(ctx, p); },
            ctx.params.layout, derived);
        VoxelMeshData mesh = extract_surface_nets(volume);
        chunk.voxels.positions = std::move(mesh.positions);
        chunk.voxels.normals = std::move(mesh.normals);
        chunk.voxels.materials = std::move(mesh.materials);
        chunk.voxels.indices = std::move(mesh.indices);
    }
    return chunk;
}

Chunk generate_chunk(const WorldGenParams& params, ChunkCoord coord) {
    return generate_chunk(build_world_context(params), coord);
}

WorldGenResult generate_world(const WorldGenParams& params,
                              const std::filesystem::path& out_file) {
    (void)params;
    (void)out_file;
    // World file IO is deferred (lead directive: streaming uses the in-memory
    // generator via ChunkManager::open_generated; WorldFormat stays headers-only).
    return WorldGenResult{false,
                          "generate_world: .dfw output arrives with world file IO; use "
                          "ChunkManager::open_generated (in-memory chunks)"};
}

} // namespace dfn::world
