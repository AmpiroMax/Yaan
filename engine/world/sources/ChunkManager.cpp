/*
Created: 09:08:2026 - 00:42:03
Last updated: 17:08:2026 - 19:05:00
Module: engine/world
File: engine/world/sources/ChunkManager.cpp

Responsibility:
- Chunk streaming implementation: residency ring around the focus position,
  in-memory generation (stage 2), batch ECS spawn/destroy per chunk (Rule 11),
  ChunkLoaded/ChunkUnloaded events with the frozen lifetime ordering.

Key items:
- ChunkManager::open_generated / update / unload_all / queries.

Dependencies:
- Uses: ChunkManager.h, Worldgen (generate_chunk).
- Used by: dfn_world.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ChunkUnloaded is published BEFORE the chunk leaves the resident map and
  before its entity group is destroyed — consumers release meshes/bodies in the
  handler while heightfield(coord) is still valid. Keep that order.
- Batch ECS ops only on the streaming paths (Rule 11).
- A delivered coarse node is freed ONLY by release_coarse_node(). Adding an
  eviction policy here (age, count, distance) breaks the agreement with render,
  which drops its GPU mesh first and then calls release.
*/
/*
UPD:
- 09:08:2026 - 00:42:03: Stage 2 — in-memory generator streaming (open_generated),
  hysteresis load/unload ring, batch spawn/destroy, event protocol.
- 09:08:2026 - 11:05:22: Stage 3b — WorldGenContext built once at
  open_generated; surfacefield/scatter/water_bodies queries; site entities get
  Transform/PreviousTransform/RenderMesh/LocalBounds/SiteMarker prototypes via
  add_batch (Rule 11 — one pool visit per component type).
- 09:08:2026 - 14:41:26: Frame-05 bed fix: water_bodies().lakes now carries the lake plus one plane per surviving pond (additive, lead-blessed; render iterates the same span).
- 09:08:2026 - 16:30:44: Representation swap: voxel_mesh accessor.
- 09:08:2026 - 17:36:42: §6.2: honour ground_y when spawning site entities.
- 09:08:2026 - 18:19:09: Streaming LOAD BUDGET: at most CHUNK_LOAD_BUDGET chunks admitted per update, nearest-to-focus first with a deterministic tie-break, remainder deferred to following updates. Unbounded admission was the multi-second freeze (a cold ring is ~2 s of synchronous work at ~83 ms/chunk including sim's collision build). Nearest-first is what makes deferral safe: the ground under the player is distance 0, so it is always next and the queue cannot reorder into a hole beneath them.
- 09:08:2026 - 21:37:57: NEW darkness_at(world) — the §6.3 authored-darkness query wrapped at the ChunkManager level (lead's call) so the app holds only its one world handle and never the layout, the worldgen context or a GroundSampler; HOW darkness is computed stays in this zone and the sampler is guaranteed to be the one the carve mouths were derived with.
- 09:08:2026 - 22:10:12: water_surface_at(vec2) implemented over the analytic water_at, for sim's swim test.
- 09:08:2026 - 23:49:27: LOD STREAMING HALF. Coarse node residency (requested -> the one active build -> held until release_coarse_node), nearest-to-focus first, advanced only in updates that admitted NO chunk so two budgets never land in one frame. world_bounds_xz reports the extent the generator was OPENED with. Nothing leaves the held set on its own -- an eviction render did not ask for pulls the ground out from under a mesh it is still drawing.
- 10:08:2026 - 02:05:00: surface_class_at(vec2) — sampled-field point query (sim request; the world->sample decoder stays in this zone, Rule 35).
- 10:08:2026 - 11:37:17: path_surface() / stand_vantages() storage, flattened once per open.
- 13:08:2026 - 16:45:00: DFN_DARK_TRACE=<путь> — по строке на КАЖДЫЙ вызов darkness_at (приложение зовёт его раз в кадр) с разложением на ветви через enclosure_trace. Открывается ГРОМКО; выключен, пока переменная не названа. Этим прибором найдено, что ambient_darkness переключается 0↔1 за один кадр 13 раз за проход по тоннелю, каждый раз на пересечении carve_distance нуля в пределах 2 см.
- 13:08:2026 - 18:40:00: DFN_DARK_TRACE пишет roof_y и open_to_sky вместо above_ground — вслед за воротами, которые теперь судят крышу.
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
- 13:08:2026 - 20:58:50: DFN_TORCH_FLAME_UP — дверь дозы на высоту пламени настенного факела над его же сущностью; умолчание теперь строка TORCH_FLAME_ABOVE_GRIP, а не литерал 0.45 (правило 14/35: то же число строит палку в render). Заведена под доказанный замер: пол в 2.79 м от горящего подсвечника читает РОВНО 0 из 255, а с DFN_NO_POINT_SHADOW=1 — 5.71 из того же бинарника и той же точки, то есть свет обнуляет СОБСТВЕННАЯ теневая карта пламени. Единственное, что стоит в точке пламени, — меш самого подсвечника: у заглушки 52 границы -0.2..+0.9 по y, значит пламя на 0.45 сидит ВНУТРИ своей модели. Это правило 35 наоборот: у факела появился МЕШ, и смещение, верное пока он был только светом, стало светом, закопанным в геометрию.
- 13:08:2026 - 21:02:08: ГИПОТЕЗА ОПРОВЕРГНУТА СОБСТВЕННОЙ ДВЕРЬЮ, и опровержение записано у кода. Подъём пламени на 1.2 м — выше всей заглушки — оставляет пол под ногами РОВНО 0.00, побитово. Значит подсвечник не заслоняет сам себя, и настоящий заслоняющий пока не назван. Замер «тень обнуляет свет» (0.00 против 5.71 при DFN_NO_POINT_SHADOW=1) остаётся в силе со своим нулевым и положительным контролем; объяснение — нет. Правило 34: предпосылку проверяют ДО того, как она войдёт в файл, а если уже вошла — правят там же, где стоит.
- 14:08:2026 - 21:03:06: Transform.scale И PreviousTransform.scale ставятся site-плейсхолдерам (резка ведущего на этот файл, только это). Первая половина — site_placeholder_scale(), см. SiteComponents.h. Вторая найдена ЗАМЕРОМ первой и стоит отдельного упоминания: render рисует mix(prev.scale, curr.scale, alpha), а PreviousTransform::scale по умолчанию 1 — поэтому 0.2 факела приезжали как 0.6 при alpha 0.5 и, хуже, ДЫШАЛИ вместе с коэффициентом интерполяции каждый кадр. Правило 39 в точной форме: теневая копия структуры верна ровно до того дня, когда у оригинала появляется поле. Поймано тем, что кадр «до/после» дал усадку 0.6× там, где арифметика обещала 0.2×.
- 14:08:2026 - 21:22:22: Чтение испечённого мира. Ветка выбора одна: есть открытый .dfw — чанк ЧИТАЕТСЯ (2.3 мс), нет или в файле его нет — генерируется (84.0 мс), замер на боевом стенде в одном процессе. Открытие файла, который не открылся, ОТВЕРГАЕТСЯ, а не откатывается на генерацию: молчаливый откат дал бы менеджер, работающий ровно с той скоростью, ради ухода от которой файл и заведён, и отчитывающийся об успехе.
- 17:08:2026 - 19:05:00: Локальная перестройка земли под кисть рельефа: набор ГРЯЗНЫХ чанков в
  Impl, три вызова наружу, и ВЫНОС spawn_chunk_entities() из тела update()
  без единого изменения — чтобы поток стриминга и поток перестройки делили
  сотню строк, а не держали две копии, которые обязаны согласиться о том, что
  такое сущность сайта. Вторая копия разошлась бы в день, когда записи дадут
  новый компонент, и разошлась бы ТОЛЬКО на пути перестройки: предметы теряют
  поле ровно тогда, когда композитор правит землю под ними, — дефект, который
  никто не прочтёт как дублирование.
*/

#include "engine/world/sources/ChunkManager.h"

#include "engine/world/sources/WorldFormat.h"

#include "engine/world/sources/WorldgenCarve.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenVantages.h"

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/SiteComponents.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <glm/gtc/quaternion.hpp>
#include <unordered_map>
#include <vector>

namespace dfn::world {

namespace {

// WHERE A SCONCE'S FLAME SITS ABOVE ITS OWN ENTITY, and the dose door on it
// (DFN_TORCH_FLAME_UP=<metres>).
//
// The default is TORCH_FLAME_ABOVE_GRIP, which is the registry's answer to
// "how far up the stick does the flame burn" and is read by render for the
// stick's length -- one number, two zones (Rule 35).
//
// THE DOOR EXISTS BECAUSE OF A MEASUREMENT, not for tuning -- AND IT THEN
// REFUTED THE HYPOTHESIS IT WAS OPENED FOR, which is why both halves are
// written here rather than only the surviving one.
//
// The measurement: standing 2.79 m from a lit sconce and looking at the floor
// at one's own feet, that floor reads EXACTLY 0 of 255 (palette shade step
// 19.99). Turning the light's cube shadow off with DFN_NO_POINT_SHADOW=1 --
// same binary, same vantage, same radius -- makes the same floor read 5.71.
// So the flame's own shadow map is what takes the light to zero rather than
// merely dimming it. That part stands, with its zero control (the same arm
// twice: 0.00 and 0.00) and its positive control (DFN_DARK=0 on the same box:
// 37.00, so the instrument can see light).
//
// The hypothesis: the only caster standing AT the flame is the sconce's own
// mesh -- the placeholder torch's local bounds run -0.2 .. +0.9 in y, so a
// flame 0.45 up the stick sits inside its own model, and a light inside a
// closed caster lights nothing. Rule 35's trigger fired in reverse: the torch
// GAINED A MESH and an offset correct for a bare light became a light buried
// in geometry.
//
// THE DOSE SAYS NO. Lifting the flame to 1.2 m -- clear of the whole mesh, one
// binary, same vantage -- leaves the floor at 0.00, bit for bit. The sconce is
// not the occluder, and the real one is still unnamed (measured 13.08.2026,
// reported to the lead). The door stays because the question it settles is
// permanent and because the next hypothesis needs the same arm.
[[nodiscard]] float wall_torch_flame_up() {
    static const float value = [] {
        const auto row = static_cast<float>(config::TORCH_FLAME_ABOVE_GRIP);
        if (const char* e = std::getenv("DFN_TORCH_FLAME_UP"); e != nullptr && *e != '\0') {
            float v = 0.0f;
            if (std::sscanf(e, "%f", &v) == 1 && v >= 0.0f) {
                std::fprintf(stderr, "[torch] DFN_TORCH_FLAME_UP=%.3f m (row %.3f)\n",
                             static_cast<double>(v), static_cast<double>(row));
                return v;
            }
            std::fprintf(stderr, "[torch] DFN_TORCH_FLAME_UP=\"%s\" is not a "
                                 "non-negative number -- REFUSED, using %.3f m\n",
                         e, static_cast<double>(row));
        }
        return row;
    }();
    return value;
}

} // namespace

struct ChunkManager::Impl {
    bool opened = false;
    WorldGenParams gen_params;
    ChunkStreamingParams params;
    const SaveDelta* delta = nullptr; // stage 3: overlay on load

    WorldGenContext gen_ctx;                      // built once per open_generated
    std::vector<math::LakePlane> lakes;           // water_bodies() storage
    // path_surface() storage — flattened once per open, same lifetime as lakes.
    std::vector<math::PathStation> path_stations;
    std::vector<uint32_t> path_route_offsets;
    std::vector<math::PathGoalMark> path_goals;
    std::vector<int32_t> path_hidden_station;
    std::vector<int32_t> path_visible_station;
    std::vector<float> path_hidden_run_m;
    std::vector<math::StandVantage> vantages;      // stand_vantages() storage
    /// THE BAKED SOURCE, when there is one. Null means "generate on load",
    /// which is what every caller did before the baker existed and what a
    /// stand-backed demo still does. It is a POINTER and not a flag beside a
    /// reader, so "we have a file" and "the file is open" cannot disagree.
    std::unique_ptr<WorldFileReader> baked;
    std::unordered_map<uint64_t, Chunk> resident; // key = chunk_group(coord)
    std::vector<ChunkCoord> loaded_coords;        // cache for loaded_chunks()

    // DFN_DARK_TRACE (see darkness_at): opened on first use, closed with the
    // manager. mutable-by-pointer: darkness_at is const and stays const.
    std::FILE* dark_trace = nullptr;
    bool dark_trace_tried = false;
    uint64_t dark_trace_call = 0;

    // --- Coarse LOD nodes ----------------------------------------------------
    // Three states, and the split is the contract with render: `requested` is
    // asked for but untouched, `active` is the ONE node under construction, and
    // `coarse` holds finished nodes until release_coarse_node. Nothing moves out
    // of `coarse` on its own — an eviction render did not ask for would pull the
    // ground out from under a mesh it is still drawing.
    std::vector<CoarseNode> requested;
    std::optional<CoarseNodeData> active;
    std::unordered_map<uint64_t, CoarseNodeData> coarse;

    /// Shortest distance from the focus to the node's footprint on xz (0 when
    /// the focus is inside it) — the same measure render selects nodes with.
    [[nodiscard]] static float coarse_distance(const CoarseNode& n, glm::vec2 focus) {
        const glm::vec2 o = coarse_node_origin_m(n);
        const float s = coarse_node_size_m(n.level);
        const float dx = std::max({o.x - focus.x, 0.0f, focus.x - (o.x + s)});
        const float dz = std::max({o.y - focus.y, 0.0f, focus.y - (o.y + s)});
        return std::sqrt(dx * dx + dz * dz);
    }

    /// Advances the ONE node under construction, starting the nearest requested
    /// node when there is none. Nearest-first for the same reason the chunk
    /// queue is nearest-first: what the player is looking at should arrive
    /// before what is behind them, and the order must not depend on the
    /// enumeration order of a hash map.
    void advance_coarse(glm::vec2 focus) {
        if (!active) {
            if (requested.empty()) {
                return;
            }
            auto best = requested.begin();
            float best_d = coarse_distance(*best, focus);
            for (auto it = requested.begin() + 1; it != requested.end(); ++it) {
                const float d = coarse_distance(*it, focus);
                // Deterministic tie-break: a coarser level first (it covers more
                // ground for the same cost), then node coords.
                bool better = d < best_d;
                if (!better && d == best_d) {
                    if (it->level != best->level) {
                        better = it->level > best->level;
                    } else if (it->x != best->x) {
                        better = it->x < best->x;
                    } else {
                        better = it->z < best->z;
                    }
                }
                if (better) {
                    best = it;
                    best_d = d;
                }
            }
            active = begin_coarse_node(*best);
            requested.erase(best);
        }
        build_coarse_rows(gen_ctx, *active, COARSE_NODE_ROW_BUDGET);
        if (active->complete()) {
            coarse.insert_or_assign(coarse_node_key(active->node), std::move(*active));
            active.reset();
        }
    }

    [[nodiscard]] bool coarse_known(const CoarseNode& n) const {
        if (coarse.contains(coarse_node_key(n))) {
            return true;
        }
        if (active && active->node == n) {
            return true;
        }
        return std::find(requested.begin(), requested.end(), n) != requested.end();
    }

    [[nodiscard]] bool in_extent(ChunkCoord c) const {
        return c.x >= gen_params.min_chunk.x && c.x <= gen_params.max_chunk.x
            && c.z >= gen_params.min_chunk.z && c.z <= gen_params.max_chunk.z;
    }

    /// Chunks whose GROUND is stale because the composer painted on it.
    /// A SET RATHER THAN AN IMMEDIATE REBUILD: a stroke covers the same chunk
    /// a hundred times as the mouse moves, and rebuilding on each would spend
    /// the cost of the whole stroke a hundred times over.
    std::vector<ChunkCoord> dirty;

    void rebuild_coord_cache() {
        loaded_coords.clear();
        loaded_coords.reserve(resident.size());
        for (const auto& [key, chunk] : resident) {
            loaded_coords.push_back(chunk.coord);
        }
    }
};

namespace {
[[nodiscard]] uint32_t chebyshev(ChunkCoord a, ChunkCoord b) {
    const int32_t dx = std::abs(a.x - b.x);
    const int32_t dz = std::abs(a.z - b.z);
    return static_cast<uint32_t>(dx > dz ? dx : dz);
}
} // namespace

namespace {

/// SPAWNS ONE CHUNK'S GENERATED ENTITIES. Extracted from update()'s load pass
/// with nothing changed, so that the streaming path and the REBUILD path below
/// share it instead of holding two copies of a hundred lines that must agree
/// about what a site entity is. A second copy would drift the first time
/// somebody gave a record a new component, and it would drift on the rebuild
/// path only — visible as props that lose a field the moment a composer edits
/// the ground under them, which nobody would read as a duplication defect.
void spawn_chunk_entities(const Chunk& chunk, ChunkCoord coord, ecs::World& ecs) {
    const uint64_t key = chunk_group(coord);
    // Batch entity spawn for the chunk's generated records (Rule 11):
    // one spawn_batch + one add_batch per component type. Stage 3b
    // records are P4 sites — placeholder RenderMesh ids + SiteMarker
    // (render maps the ids; real archetype instantiation from content
    // files is gameplay/lead wiring, a later stage).
    if (!chunk.entities.empty()) {
        const std::size_t n = chunk.entities.size();
        std::vector<ecs::EntityId> ids(n);
        ecs.spawn_batch(ids, key);

        std::vector<components::Transform> transforms(n);
        std::vector<components::RenderMesh> meshes(n);
        std::vector<components::LocalBounds> bounds(n);
        std::vector<SiteMarker> markers(n);
        for (std::size_t i = 0; i < n; ++i) {
            const GeneratedEntityRecord& rec = chunk.entities[i];
            const float y = rec.ground_y != NO_GROUND_Y
                              ? rec.ground_y
                              : chunk.heightmap.sample_world(coord, rec.position_xz);
            transforms[i].position = {rec.position_xz.x, y, rec.position_xz.y};
            transforms[i].rotation =
                glm::angleAxis(rec.yaw, glm::vec3{0.0f, 1.0f, 0.0f});
            if (const auto type = site_type_from_archetype(rec.archetype)) {
                const SiteArchetype& a = site_archetype(*type);
                // SCALE MAPS THE MESH'S OWN SPACE ONTO ITS DECLARED
                // BOX, and its absence is why a wall sconce was a 2 m
                // cube standing half inside the rock. Every row
                // authored in metres returns exactly 1, so nothing but
                // the torch moves (SiteComponents.h). Same line as
                // InteractableSpawn.cpp's, and for the same reason:
                // the drawn prop and the declared prop are one object
                // or they are two that will drift.
                transforms[i].scale = site_placeholder_scale(a);
                meshes[i] = components::RenderMesh{a.mesh_id, 0};
                bounds[i] = components::LocalBounds{a.bounds_min, a.bounds_max};
                markers[i] = SiteMarker{*type};
            }
        }
        ecs.add_batch<components::Transform>(ids, std::span<const components::Transform>{transforms});
        std::vector<components::PreviousTransform> prev(n);
        for (std::size_t i = 0; i < n; ++i) {
            prev[i].position = transforms[i].position;
            prev[i].rotation = transforms[i].rotation;
            // AND THE SCALE, which this loop had no reason to copy
            // until one line above gave a placeholder a scale at all.
            // render draws mix(prev.scale, curr.scale, alpha), and
            // PreviousTransform::scale defaults to 1 — so the torch's
            // 0.2 arrived as 0.6 at alpha 0.5 and, worse, BREATHED
            // with the interpolation factor every frame. Rule 39 in
            // its exact shape: a shadow copy of a struct is correct
            // until the original gains a field.
            prev[i].scale = transforms[i].scale;
        }
        ecs.add_batch<components::PreviousTransform>(
            ids, std::span<const components::PreviousTransform>{prev});
        ecs.add_batch<components::RenderMesh>(
            ids, std::span<const components::RenderMesh>{meshes});
        ecs.add_batch<components::LocalBounds>(
            ids, std::span<const components::LocalBounds>{bounds});
        ecs.add_batch<SiteMarker>(ids, std::span<const SiteMarker>{markers});
        // A WALL TORCH IS A LIGHT, and this is where it gets one.
        // components::CarriedLight is the only thing render collects
        // into the frame's point-light array; the name says "carried"
        // but the contract is "a flame at an entity", which a sconce is
        // (IRenderer's own list: "torch, braziers, lit windows").
        // Added after the batches on purpose: only some records are
        // torches, so this is not a batch (Rule 9's structural-change
        // discipline is satisfied — the view above has been consumed).
        for (std::size_t i = 0; i < n; ++i) {
            if (markers[i].type != SiteType::WallTorch) {
                continue;
            }
            ecs.add(ids[i], components::CarriedLight{
                                .active = true,
                                .radius_m = 0.0f, // render's default torch radius
                                .color_rgb = 0,   // render's default warm flame
                                // Up the stick from the sconce, the same
                                // way a held torch's flame sits above the
                                // grip. DFN_TORCH_FLAME_UP is the dose
                                // door on this one number — see
                                // wall_torch_flame_up() for what it was
                                // opened to measure.
                                .offset = {0.0f, wall_torch_flame_up(), 0.0f}});
        }
    }
}

} // namespace

ChunkManager::ChunkManager() : impl_(std::make_unique<Impl>()) {}
ChunkManager::~ChunkManager() {
    if (impl_ && impl_->dark_trace != nullptr) {
        std::fclose(impl_->dark_trace);
        impl_->dark_trace = nullptr;
    }
}

bool ChunkManager::open(const std::filesystem::path& world_file,
                        const WorldGenParams& gen_params, const SaveDelta* delta,
                        ChunkStreamingParams params) {
    auto reader = std::make_unique<WorldFileReader>();
    if (!reader->open(world_file)) {
        // REFUSED, NOT FALLEN BACK. A manager that quietly generated the world
        // when its file failed to open would run at the speed the file exists
        // to avoid while reporting success, and the missing file would be found
        // by somebody profiling something else weeks later.
        return false;
    }
    // The world-level passes are built ANYWAY, and this is honest rather than
    // wasteful: the .dfw carries chunks, not the lake planes, the path network
    // or the stand's vantages, and those are what render and gameplay ask this
    // manager for between chunk loads. Baking them too is the next step and it
    // is a FORMAT change, so it does not get smuggled in as a code change.
    open_generated(gen_params, params);
    impl_->baked = std::move(reader);
    impl_->delta = delta;
    return true;
}

void ChunkManager::open_generated(const WorldGenParams& gen_params,
                                  ChunkStreamingParams params) {
    impl_->opened = true;
    // A stand-backed open must not inherit a previously opened file: open()
    // calls THIS function and then attaches its reader, so clearing here is
    // what keeps the two entry points from silently sharing a source.
    impl_->baked.reset();
    impl_->gen_params = gen_params;
    impl_->params = params;
    impl_->delta = nullptr;
    impl_->resident.clear();
    impl_->loaded_coords.clear();
    // A new world invalidates every coarse node: the ids are on a fixed world
    // grid, but what the field says at those coordinates is not the same world.
    impl_->requested.clear();
    impl_->active.reset();
    impl_->coarse.clear();
    // World-level passes once per open (deterministic; chunks stay independent).
    impl_->gen_ctx = build_world_context(gen_params);
    // Drawable water bodies: the lake plus every surviving pond, so no
    // water-covered sample is left without a body render can draw over it.
    impl_->lakes.assign(1, impl_->gen_ctx.hydrology.lake);
    impl_->lakes.insert(impl_->lakes.end(), impl_->gen_ctx.hydrology.pond_planes.begin(),
                        impl_->gen_ctx.hydrology.pond_planes.end());
    // The §8.1 path network flattened for render. Unconditional: a stand with
    // no paths flattens to zero stations and a one-element offsets array, which
    // is the same code path rather than a branch (Rule 32).
    path_render_stations(impl_->gen_ctx.paths, impl_->path_stations,
                         impl_->path_route_offsets, impl_->path_goals);
    impl_->path_hidden_station.clear();
    impl_->path_visible_station.clear();
    impl_->path_hidden_run_m.clear();
    for (const PathRoute& r : impl_->gen_ctx.paths.routes) {
        impl_->path_hidden_station.push_back(r.hidden_station);
        impl_->path_visible_station.push_back(r.visible_station);
        impl_->path_hidden_run_m.push_back(r.longest_hidden_run_m);
    }
    // The stand's own acceptance standpoints. Gated on the STAND, not on
    // emptiness: the testbed publishes none because its §7.1 route is authored
    // in render's own testbed_steps(), and inventing forest vantages for it
    // would emit frames aimed at ground that carries no such claim.
    impl_->vantages.clear();
    if (impl_->gen_ctx.params.layout.stand == StandId::Forest) {
        impl_->vantages = forest_vantages(impl_->gen_ctx.params.seed,
                                          impl_->gen_ctx.params.layout, impl_->gen_ctx.paths,
                                          impl_->gen_ctx.finds);
    }
}

void ChunkManager::update(const glm::vec3& focus_position, ecs::World& ecs,
                          events::EventBus& bus) {
    if (!impl_->opened) {
        return;
    }
    const ChunkCoord focus = chunk_at_position({focus_position.x, focus_position.z});
    bool changed = false;

    // --- Unload pass: residents beyond the unload radius (hysteresis). --------
    std::vector<ChunkCoord> to_unload;
    for (const auto& [key, chunk] : impl_->resident) {
        if (chebyshev(chunk.coord, focus) > impl_->params.unload_radius) {
            to_unload.push_back(chunk.coord);
        }
    }
    for (const ChunkCoord coord : to_unload) {
        // Order is contract: publish while data is valid, then destroy the
        // entity group (one batch, Rule 11), then free the chunk.
        bus.publish(ChunkUnloaded{coord});
        ecs.destroy_group(chunk_group(coord));
        impl_->resident.erase(chunk_group(coord));
        changed = true;
    }

    // --- Load pass: missing chunks within the load radius, clipped to extent,
    // NEAREST FIRST and rate-limited to CHUNK_LOAD_BUDGET per update.
    //
    // Admitting every missing chunk in one update is what produced the
    // multi-second freezes: a chunk costs ~14.5 ms here plus sim's ~68 ms
    // collision build, so a cold 5x5 ring was ~2 s of synchronous work inside
    // a single frame. Deferring the remainder spreads that over following
    // updates. Nearest-to-focus ordering is what makes deferral safe: the
    // ground under the player is by definition distance 0, so it is always the
    // next chunk admitted and the queue can never reorder into a hole beneath
    // them. Every update admits at least one chunk, so nothing starves.
    const int32_t r = static_cast<int32_t>(impl_->params.load_radius);
    struct Pending {
        ChunkCoord coord;
        int32_t ring;    ///< Chebyshev distance: the streaming ring it sits in
        int64_t dist_sq; ///< tie-break within a ring: true distance
    };
    std::vector<Pending> pending;
    for (int32_t dz = -r; dz <= r; ++dz) {
        for (int32_t dx = -r; dx <= r; ++dx) {
            const ChunkCoord coord{focus.x + dx, focus.z + dz};
            if (!impl_->in_extent(coord) || impl_->resident.contains(chunk_group(coord))) {
                continue;
            }
            pending.push_back({coord, static_cast<int32_t>(chebyshev(coord, focus)),
                               static_cast<int64_t>(dx) * dx + static_cast<int64_t>(dz) * dz});
        }
    }
    std::sort(pending.begin(), pending.end(), [](const Pending& a, const Pending& b) {
        if (a.ring != b.ring) return a.ring < b.ring;
        if (a.dist_sq != b.dist_sq) return a.dist_sq < b.dist_sq;
        // Deterministic final tie-break so load order never depends on the
        // enumeration order of the resident map.
        if (a.coord.x != b.coord.x) return a.coord.x < b.coord.x;
        return a.coord.z < b.coord.z;
    });
    const std::size_t budget = static_cast<std::size_t>(config::CHUNK_LOAD_BUDGET);
    if (pending.size() > budget) {
        pending.resize(budget);
    }

    for (const Pending& entry : pending) {
        {
            const ChunkCoord coord = entry.coord;
            const uint64_t key = chunk_group(coord);
            // READ IT IF IT IS BAKED, generate it if it is not. Measured on
            // the shipping testbed, same process: 2.3 ms to read a chunk
            // against 84.0 ms to generate one. That 84 ms is the user's
            // «чанки грузятся неадекватно долго», and it is the reason this
            // branch exists at all.
            //
            // A chunk MISSING from an open file falls through to generation
            // rather than leaving a hole: a bake covers an extent, and a demo
            // that walks one chunk past its baked edge should meet ground, not
            // sky. The cost of being wrong here is a hitch; the cost of the
            // other choice is a player falling through the world.
            std::optional<Chunk> baked_chunk;
            if (impl_->baked != nullptr) {
                baked_chunk = impl_->baked->load_chunk(coord);
            }
            Chunk chunk = baked_chunk.has_value() ? std::move(*baked_chunk)
                                                  : generate_chunk(impl_->gen_ctx, coord);
            // Stage 3: apply impl_->delta overlay here before spawning (Q56).

            spawn_chunk_entities(chunk, coord, ecs);

            impl_->resident.emplace(key, std::move(chunk));
            bus.publish(ChunkLoaded{coord});
            changed = true;
        }
    }

    // --- Coarse LOD pass: far terrain is built with the update's LEFTOVER ----
    //
    // Only when no chunk was admitted. Two budgets spent in one update is two
    // budgets' worth of hitch, and the ground under the player outranks the
    // ground on the horizon by definition: a chunk admission costs ~83 ms
    // including sim's collision build, which is already the whole frame. Chunk
    // admissions are bursty (a ring fills over a handful of updates and then
    // nothing), so this starves only while the player is crossing into new
    // ground, which is measured in frames.
    if (pending.empty()) {
        impl_->advance_coarse({focus_position.x, focus_position.z});
    }

    if (changed) {
        impl_->rebuild_coord_cache();
    }
}

void ChunkManager::set_composed_relief(const ReliefLayer& relief) {
    // ONLY THE LAYER, NOT THE WHOLE CONTEXT. build_world_context re-runs
    // hydrology, sites, erosion, the drainage and the path network — hundreds
    // of milliseconds — and NONE of them read this layer: it is applied last in
    // compose_passes and consulted by classify_surface, and nothing upstream of
    // those two can see it. So replacing the field alone is not a shortcut past
    // a rebuild, it is the whole rebuild, and it is correct by construction
    // rather than by anyone remembering to keep it that way.
    impl_->gen_params.composed_relief = relief;
    impl_->gen_ctx.params.composed_relief = relief;
}

std::size_t ChunkManager::invalidate_area(glm::vec2 min_xz, glm::vec2 max_xz) {
    const auto size = static_cast<float>(config::CHUNK_SIZE);
    std::size_t marked = 0;
    for (const auto& [key, chunk] : impl_->resident) {
        const glm::vec2 lo{static_cast<float>(chunk.coord.x) * size,
                           static_cast<float>(chunk.coord.z) * size};
        const glm::vec2 hi = lo + glm::vec2{size, size};
        if (hi.x < min_xz.x || lo.x > max_xz.x || hi.y < min_xz.y || lo.y > max_xz.y) {
            continue;
        }
        if (std::find(impl_->dirty.begin(), impl_->dirty.end(), chunk.coord)
            == impl_->dirty.end()) {
            impl_->dirty.push_back(chunk.coord);
            ++marked;
        }
    }
    // A chunk OUTSIDE the resident set needs no marking and gets none: it will
    // be generated from the current layer whenever streaming reaches it. Saying
    // "0 chunks" for an edit past the streamed ring is the truth, and a caller
    // that wanted to know is better served by that than by a queued rebuild of
    // ground nobody is looking at.
    return marked;
}

std::size_t ChunkManager::rebuild_dirty(ecs::World& ecs, events::EventBus& bus,
                                        std::size_t budget) {
    if (!impl_->opened || budget == 0) {
        return impl_->dirty.size();
    }
    std::size_t done = 0;
    while (done < budget && !impl_->dirty.empty()) {
        const ChunkCoord coord = impl_->dirty.front();
        impl_->dirty.erase(impl_->dirty.begin());
        const uint64_t key = chunk_group(coord);
        if (!impl_->resident.contains(key)) {
            continue; // streamed out while the composer was painting: no work
        }
        // GENERATED FIRST, SWAPPED SECOND. The obvious implementation — unload
        // it and let the next update() stream it back — is one line and leaves
        // a 256 m hole in the world for a frame: the composer watches the
        // ground he is sculpting blink out from under him on every stroke.
        // Building the replacement before announcing the loss costs the same
        // milliseconds and never shows a hole.
        //
        // THE BAKED FILE IS DELIBERATELY NOT CONSULTED. A .dfw was baked before
        // anybody painted; reading the chunk back from it would hand back
        // exactly the ground the composer just changed, and the edit would look
        // like it did not take.
        Chunk fresh = generate_chunk(impl_->gen_ctx, coord);

        // The unload/load protocol in full, in the order the frozen lifetime
        // contract states it: consumers drop their mesh and their collision
        // body while the old data is still valid, then the new data goes in,
        // then they are told to build again.
        bus.publish(ChunkUnloaded{coord});
        ecs.destroy_group(key);
        impl_->resident.erase(key);
        spawn_chunk_entities(fresh, coord, ecs);
        impl_->resident.emplace(key, std::move(fresh));
        bus.publish(ChunkLoaded{coord});
        ++done;
    }
    if (done > 0) {
        impl_->rebuild_coord_cache();
    }
    return impl_->dirty.size();
}

void ChunkManager::unload_all(ecs::World& ecs, events::EventBus& bus) {
    // Same per-chunk protocol as streaming unload.
    std::vector<ChunkCoord> coords = impl_->loaded_coords;
    for (const ChunkCoord coord : coords) {
        bus.publish(ChunkUnloaded{coord});
        ecs.destroy_group(chunk_group(coord));
        impl_->resident.erase(chunk_group(coord));
    }
    impl_->rebuild_coord_cache();
}

bool ChunkManager::is_loaded(ChunkCoord coord) const {
    return impl_->resident.contains(chunk_group(coord));
}

std::span<const ChunkCoord> ChunkManager::loaded_chunks() const {
    return impl_->loaded_coords;
}

std::optional<math::HeightFieldView> ChunkManager::heightfield(ChunkCoord coord) const {
    const auto it = impl_->resident.find(chunk_group(coord));
    if (it == impl_->resident.end()) {
        return std::nullopt;
    }
    return it->second.heightmap.view(coord);
}

std::optional<math::SurfaceFieldView> ChunkManager::surfacefield(ChunkCoord coord) const {
    const auto it = impl_->resident.find(chunk_group(coord));
    if (it == impl_->resident.end()) {
        return std::nullopt;
    }
    return it->second.surface.view(coord);
}

std::optional<math::VoxelMeshView> ChunkManager::voxel_mesh(ChunkCoord coord) const {
    const auto it = impl_->resident.find(chunk_group(coord));
    if (it == impl_->resident.end()) {
        return std::nullopt;
    }
    return it->second.voxels.view(coord);
}

std::span<const math::ScatterInstance> ChunkManager::scatter(ChunkCoord coord) const {
    const auto it = impl_->resident.find(chunk_group(coord));
    if (it == impl_->resident.end()) {
        return {};
    }
    return it->second.scatter;
}

ChunkManager::WaterBodies ChunkManager::water_bodies() const {
    if (!impl_->opened) {
        return {};
    }
    return WaterBodies{impl_->lakes, impl_->gen_ctx.hydrology.stations,
                       impl_->gen_ctx.hydrology.segment_offsets};
}

ChunkManager::PathSurface ChunkManager::path_surface() const {
    if (!impl_->opened) {
        return {};
    }
    PathSurface ps;
    ps.stations = impl_->path_stations;
    ps.route_offsets = impl_->path_route_offsets;
    ps.goals = impl_->path_goals;
    ps.rich_edge_band_m = impl_->gen_ctx.paths.rich_edge_band_m;
    ps.hidden_station = impl_->path_hidden_station;
    ps.visible_station = impl_->path_visible_station;
    ps.hidden_run_m = impl_->path_hidden_run_m;
    return ps;
}

std::span<const math::StandVantage> ChunkManager::stand_vantages() const {
    if (!impl_->opened) {
        return {};
    }
    return impl_->vantages;
}

const Chunk* ChunkManager::chunk(ChunkCoord coord) const {
    const auto it = impl_->resident.find(chunk_group(coord));
    return it == impl_->resident.end() ? nullptr : &it->second;
}

std::optional<float> ChunkManager::height_at(glm::vec2 world_xz) const {
    const ChunkCoord coord = chunk_at_position(world_xz);
    const auto it = impl_->resident.find(chunk_group(coord));
    if (it == impl_->resident.end()) {
        return std::nullopt;
    }
    return it->second.heightmap.sample_world(coord, world_xz);
}

// --- Coarse terrain (far LOD) --------------------------------------------------

glm::vec4 ChunkManager::world_bounds_xz() const {
    if (!impl_->opened) {
        return glm::vec4{0.0f};
    }
    // THE GENERATED extent, derived from the params the generator was opened
    // with — not from configured constants, which describe an intent that has
    // already diverged from what exists once this stage. max_chunk is
    // INCLUSIVE, so the far edge is (max_chunk + 1) * CHUNK_SIZE.
    const float size = static_cast<float>(config::CHUNK_SIZE);
    const WorldGenParams& p = impl_->gen_params;
    return glm::vec4{static_cast<float>(p.min_chunk.x) * size,
                     static_cast<float>(p.min_chunk.z) * size,
                     static_cast<float>(p.max_chunk.x + 1) * size,
                     static_cast<float>(p.max_chunk.z + 1) * size};
}

void ChunkManager::request_coarse_nodes(std::span<const CoarseNode> nodes) {
    if (!impl_->opened) {
        return;
    }
    for (const CoarseNode& node : nodes) {
        // Idempotent: render may pass the same standing set every frame, and a
        // duplicate request must not rebuild a node that is already delivered
        // (which would hand render a new view while it draws the old one).
        if (!impl_->coarse_known(node)) {
            impl_->requested.push_back(node);
        }
    }
}

std::optional<math::HeightFieldView>
ChunkManager::coarse_heightfield(const CoarseNode& node) const {
    const auto it = impl_->coarse.find(coarse_node_key(node));
    if (it == impl_->coarse.end()) {
        return std::nullopt; // never requested, or still being built
    }
    return it->second.height_view();
}

std::optional<math::SurfaceFieldView>
ChunkManager::coarse_surfacefield(const CoarseNode& node) const {
    const auto it = impl_->coarse.find(coarse_node_key(node));
    if (it == impl_->coarse.end()) {
        return std::nullopt;
    }
    return it->second.surface_view();
}

void ChunkManager::release_coarse_node(const CoarseNode& node) {
    if (impl_->coarse.erase(coarse_node_key(node)) > 0) {
        return;
    }
    // Cancelling something not yet delivered: the node under construction, or
    // one still queued. Render deselects nodes it never received (it moved on),
    // and without these two branches the work would be finished for nobody.
    if (impl_->active && impl_->active->node == node) {
        impl_->active.reset();
        return;
    }
    const auto it = std::find(impl_->requested.begin(), impl_->requested.end(), node);
    if (it != impl_->requested.end()) {
        impl_->requested.erase(it);
    }
}

std::size_t ChunkManager::coarse_resident_count() const {
    return impl_->coarse.size();
}

std::size_t ChunkManager::coarse_pending_count() const {
    return impl_->requested.size() + (impl_->active ? 1u : 0u);
}

std::optional<float> ChunkManager::water_surface_at(glm::vec2 world_xz) const {
    const WorldGenContext& ctx = impl_->gen_ctx;
    // Macro height first: water_at needs the pre-carve terrain at this column,
    // and it must be the same value the height pipeline uses or the carve and
    // the water surface disagree at the shoreline.
    const float h = macro_height(ctx.params.seed, ctx.params.layout, world_xz);
    const WaterSample s = water_at(ctx.hydrology, ctx.params.layout, world_xz, h);
    if (s.water_surface <= math::NO_WATER) {
        return std::nullopt;
    }
    return s.water_surface;
}

std::optional<math::SurfaceClass> ChunkManager::surface_class_at(glm::vec2 world_xz) const {
    const auto view = surfacefield(chunk_at_position(world_xz));
    if (!view.has_value() || view->resolution == 0) {
        return std::nullopt;
    }
    // Nearest sample of the drawn field: the world->sample convention (origin,
    // step, row-major x fastest) lives HERE so no consumer grows a second
    // decoder copy of it (Rule 35, state form).
    const float limit = static_cast<float>(view->resolution - 1);
    const auto x = static_cast<uint32_t>(
        std::clamp(std::round((world_xz.x - view->origin.x) / view->step), 0.0f, limit));
    const auto z = static_cast<uint32_t>(
        std::clamp(std::round((world_xz.y - view->origin.y) / view->step), 0.0f, limit));
    return static_cast<math::SurfaceClass>(
        view->surface_class[static_cast<std::size_t>(z) * view->resolution + x]);
}

float ChunkManager::darkness_at(glm::vec3 world) const {
    // The GroundSampler is built from the SAME context the carve mouths were
    // derived with -- that guarantee is the reason this wrapper exists rather
    // than the app assembling the call itself.
    const WorldGenContext& ctx = impl_->gen_ctx;
    const GroundSampler ground = [&ctx](glm::vec2 p) { return terrain_height(ctx, p); };
    // DFN_DARK_TRACE=<path>: one line per CALL (the app calls this once per
    // frame), naming WHICH half of the rule decided. The result alone cannot
    // tell "not enclosed" from "enclosed but nothing earned" apart, and the
    // "темнеет, потом мигает" run needs exactly that split. Off unless the
    // variable names a file; opens loudly, and the shipping path below is the
    // SAME evaluation, not a second one.
    if (!impl_->dark_trace_tried) {
        impl_->dark_trace_tried = true;
        if (const char* dt = std::getenv("DFN_DARK_TRACE"); dt != nullptr && *dt != '\0') {
            impl_->dark_trace = std::fopen(dt, "wb");
            if (impl_->dark_trace == nullptr) {
                std::fprintf(stderr, "[dark_trace] cannot open \"%s\" for writing\n", dt);
            } else {
                std::fprintf(impl_->dark_trace,
                             "# call qx qy qz carve_dist ground_y open_to_sky "
                             "path_m path_measured darkness roof_y\n");
            }
        }
    }
    if (impl_->dark_trace != nullptr) {
        const EnclosureTrace tr = enclosure_trace(ctx.params.layout, {}, ground, world);
        std::fprintf(impl_->dark_trace,
                     "%llu %.3f %.3f %.3f %+.6f %.3f %d %.3f %d %.6f %.3f\n",
                     static_cast<unsigned long long>(impl_->dark_trace_call++),
                     static_cast<double>(world.x), static_cast<double>(world.y),
                     static_cast<double>(world.z),
                     static_cast<double>(tr.carve_distance),
                     static_cast<double>(tr.ground_y), tr.open_to_sky ? 1 : 0,
                     static_cast<double>(tr.path_from_mouth), tr.path_measured ? 1 : 0,
                     static_cast<double>(tr.darkness), static_cast<double>(tr.roof_y));
        return tr.darkness;
    }
    return enclosure_darkness(ctx.params.layout, {}, ground, world);
}

} // namespace dfn::world
