/*
Created: 09:08:2026 - 00:45:00
Last updated: 22:08:2026 - 15:40:00
Module: engine/render
File: engine/render/sources/RenderSystem.cpp

Responsibility:
- RenderSystem implementation: frame orchestration (begin/end), terrain chunk
  meshes, interpolated ECS submissions (Rule 12).

Key items:
- RenderSystem::init/shutdown/render/upload_terrain/drop_terrain.

Dependencies:
- Uses: TerrainMesher, FirstPersonCamera, IRenderer, ecs::World, shared
  components (engine/core/components).
- Used by: dfn_render target; driven by engine/app.

Notes:
- Stage 2: the ECS RenderMesh path is implemented but inert — no asset
  pipeline exists yet, so mesh_cache_ has no entries and unresolved asset ids
  are skipped. Terrain + debug draw carry the stage-2 acceptance (Q51).
- Frustum culling deferred to stage 3 with core's math types (documented in
  the spec).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 9: IRenderer arrives as a parameter, never stored.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial implementation.
- 09:08:2026 - 11:03:00: Stage 3 — terrain splat atlas + water texture via
  ProcTexture (dense asset ids, cached by params), frame RenderEnvironment
  (Materials.h), water plane (set_water/clear_water, DFN_WATER debug env),
  visual clock for water scroll.
- 09:08:2026 - 11:57:20: Stage 3b — surface-truth terrain upload, scatter
  batches (trees + GRASS_VIEW_DISTANCE micro tiles), per-body water (lake
  planes + river ribbons), site placeholder meshes under blessed ids 1..7,
  ECS submissions on the "prop" program.
- 09:08:2026 - 17:33:00: Map screen: overlay quad + per-frame canvas texture
  (draw_overlay), map_.note_chunk on terrain upload, map_.note_site in the ECS
  pass (before the mesh lookup, so the mesh-less castle ids 8..11 still map),
  DFN_MAP=1 opens the map at init for the tour evidence shot.
- 09:08:2026 - 19:42:00: upload_terrain_voxel (see the header UPD).
- 09:08:2026 - 19:52:00: DFN_TIME re-applied per frame (see the header UPD).
- 09:08:2026 - 20:21:13: Foliage pass (see the header UPD): "foliage" program,
  leaf mask atlas upload, second scatter submit. Flora agent, lead-granted
  Rule 25 exception.
- 09:08:2026 - 20:46:00: collect_point_lights — CarriedLight + Transform become
  the frame's point lights (interpolated, hand offset rotated by body yaw,
  first two flagged for cube shadows), plus DFN_TORCH / DFN_DARK hooks.
- 09:08:2026 - 21:14:00: FRUSTUM CULLING for terrain and scatter, with the
  shadow-caster exemption (visible_or_casting): chunk bounds are measured at
  upload, and an off-screen mesh within LOOKDEV_SHADOW_CASTER_KEEP_M is still
  submitted because the backend double-submits opaques into the sun map — a
  naive cull would delete shadows along with the geometry.
- 09:08:2026 - 22:33:00: DFN_NO_SCATTER=1 — the trees-off half of a silhouette
  A/B. A landmark verdict taken with the forest in frame is a verdict on the
  forest.
- 09:08:2026 - 22:36:47: A MISSING MESH IS LOUD (lead's instruction). Both
  silent paths that hid the invisible castle now print: a blessed site id with
  no geometry at init, and an unregistered asset id in the ECS pass (once per
  id, not per frame).
- 09:08:2026 - 22:44:28: MAP REGRESSION FIX — upload_terrain_voxel records the
  explored chunk. Since the ferry moved to the voxel path the map had been
  recording only chunks WITHOUT a voxel mesh, i.e. almost none.
- 09:08:2026 - 23:50:06: HUD layer (transparent overlay + DFN_FONT_PROBE specimen),
  water bodies merged into world-grid buckets and frustum-culled, per-path
  upload accounting, and a LOUD first-failure report — a terrain chunk that
  fails to upload used to `return;` in silence, which is the same "absence
  looks neutral" family as the invisible castle.
- 10:08:2026 - 00:20:00: The font probe forces the HUD layer on instead of riding on
  hud_visible_. The app now owns that flag (it wires the real interaction
  prompt) and the first frame after it landed came back EMPTY, with nothing
  saying the hook had been switched off.
- 10:08:2026 - 03:08:00: CLOUDS (W4): apply_wind + apply_clouds driven from
  render() each frame (apply_wind had NO live call site — the 0.0 default
  read as a calm day: absence presenting as neutral, the invisible-castle
  family); DFN_CLOUD / DFN_VISTIME hooks for the acceptance shoot.
- 10:08:2026 - 20:17:40: RenderMesh::mesh_asset 0 is the documented "none"
  sentinel and is produced deliberately (hidden bone segment, empty item
  slot), so the missing-asset warning was firing on correct code every
  launch — Rule 38's failure mode in a log rather than a test. Skipped
  before the lookup; a genuinely unregistered id still warns once.
- 10:08:2026 - 22:54:58: DFN_WIND_FREEZE=<seconds> pins environment_.time_seconds for
  pixel-diff evidence. It was written because the canopy-speckle control was
  DIRTY and got WORSE with a longer settle (0.000 % at 240 frames, 1.990 % at
  700, same binary, same pinned pose) — the opposite of a streaming problem,
  which converges. The wind clock accumulates wall-clock frame_dt, so two runs
  reach the shutter with the crown in different positions. Consequence worth
  more than the hook (Rule 41): with the wind live, a pixel diff of a canopy
  CANNOT separate the shimmer the user reports from the rustle he likes.
- 10:08:2026 - 23:00:35: CORRECTION to the DFN_WIND_FREEZE entry above. It claimed a
  canopy pixel diff cannot separate the reported shimmer from the liked rustle.
  Measured with the hook that entry introduced: rustle alone (wind +1/120 s, eye
  still) is 0.008 % near canopy and 0.003 % treeline, against 0.864 % / 0.093 %
  for the running motion — 1 % and 3 % of the per-frame change. The two ARE
  separable and are not in tension, which is the opposite of what was written,
  and it matters because the wrong version invites softening the animation.
- 12:08:2026 - 00:52:40: GROUND TUFTS — the sparse near-field grass layer
  (GroundTufts.h). Spots are harvested once per chunk off the DRAWN voxel mesh
  in upload_terrain_voxel; the geometry is ONE eye-local mesh regrown only when
  the eye has walked TUFT_REBUILD_STEP_M, because at the Rule 33 view distance
  only a couple of hundred clumps are ever visible and baking a whole chunk of
  blades would spend tens of megabytes to draw a hundredth of them. Every
  setting is derived from an approved row (tuft_params(), Rule 14).
  DFN_NO_TUFTS=1 is the counterfactual arm.
- 13:08:2026 - 16:45:00: DFN_ENV_LOG=<путь> — по строке на каждый ПРЕДЪЯВЛЕННЫЙ кадр, снимается сразу ПОСЛЕ set_environment(), то есть пишет то, чем кадр РЕАЛЬНО нарисован: заливка (ambient_darkness), число точечных источников, солнце, общий свет, луна, глаз. Тот же класс прибора, что DFN_FRAME_LOG ведущего, и по той же причине: дефект «в подземелье темнеет, потом мигает» живёт в РАЗНОСТИ соседних кадров, а все наши двери съёмки его гасят.
- 13:08:2026 - 19:20:00: apply_clouds now receives the EYE. The cloud ceiling's height
  is a field of weather AND place (R3.4), and place needs a position; the
  place term's wavelength is two world widths, so this is a slow trend across
  the map, not something a walking player can watch move.
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
- 13:08:2026 - 20:37:12: render() prefers the TOLD visual time over its own steady_clock read.
  See RenderSystem.h, set_visual_time, for the measurement (67.466 % of the sky
  between two runs of one recipe, 0.000 % with the clock pinned).
- 13:08:2026 - 22:28:39: An entity with an ACTIVE CarriedLight no longer casts into
  point shadows (casts_in_point_shadows = false): the sconce's own mesh holds
  the flame, its head filled all six cube faces at 0.11-0.64 m, and the floor
  2.79 m from a burning torch read 0 of 255 across the whole frame. With the
  exemption the same box reads 4.49 luma; the defect arm
  (DFN_SELF_POINT_CAST=1, same binary) reproduces the black frame exactly
  (131312 lit pixels twice, bit for bit). The holder's true silhouette was
  degenerate in the map anyway -- centimetres from its own light.
- 14:08:2026 - 17:58:55: В28 pick_id STAMP — the ECS entity submit now sets
  DrawParams::pick_id = EntityId.index + 1, so center_pick() can NAME the
  entity under the crosshair (it was always 0 = "unnamed"). Index is the
  stable, mappable half of {index, generation}: a live pick's slot maps back
  to the entity via World's current generation, and generation only matters
  across destroy+reuse. +1 keeps a real slot-0 entity out of the "0 = unnamed"
  sentinel; the overlay inverts with (id-1). World geometry (terrain, scatter,
  water, path, tufts) and the viewmodel path — none currently — stay 0.
- 14:08:2026 - 19:34:00: ЛЕСТНИЦА ДЕТАЛИЗАЦИИ ФЛОРЫ НАКОНЕЦ ПОДКЛЮЧЕНА (рез ведущего на зону render). refresh_scatter_lod печёт НЕ БОЛЕЕ ОДНОГО чанка за кадр, ближний первым: перепечка стоит ровно столько же, сколько первая печь, поэтому проход без бюджета обменял бы ровный кадр на тот самый многосекундный ступор, от которого стриминг уже научился уходить. Якорь дистанции — БЛИЖАЙШАЯ ТОЧКА чанка, и это не деталь: CHUNK_SIZE 256 м, центр чанка может стоять в 181 м, когда ближний край под ногами, и банда по центру испекла бы дерево в пяти метрах силуэтом — тот самый дефект, ради предотвращения которого проход и написан, в одежде выигрыша. Чанк рождается сразу на своём уровне, а не печётся полным и потом понижается: полная печь для земли, до которой игроку далеко, платилась бы ровно во время стриминга, когда кадр и так нагружен. Замер на лесной демке, один бинарник: 7 695 612 → 2 396 252 треугольника, 600 кадров 75 с → 20 с (~8 → ~30 кадров/с), кадры расходятся на 0.265 % пикселей одним пятном дальнего древостоя при НУЛЕВОМ шуме — два прогона одной руки побитово равны. Кадры docs/acceptance/flora-lod-{before-full,after-banded}.png.
- 14:08:2026 - 23:36:19: Тело upload_prebuilt_scatter — ChunkScatterRes из готовых потоков.
- 15:08:2026 - 02:14:30: ревизия атласа в ключе кэша текстур (см. FloraCards.h).
- 15:08:2026 - 14:07:36: HUD и экран карты рисуются в СВОЕЙ сетке (см.
  RenderSystemResources): проверка показа спрашивает «есть ли у интерфейса
  холст», а не «совпадает ли он со сценой» — равенство прятало бы весь
  интерфейс на любом разрешении, кроме эталонного.
- 15:08:2026 - 16:10:00: лист нормалей коры (generate_leaf_normal_atlas зоны flora) грузится
  рядом с масочным атласом, тем же ключом ревизии — два листа пекутся вместе и
  не должны кэшироваться порознь, — и отдаётся каждому дро листвы через
  aux_texture. Один лист на весь атлас: тайл сам говорит, какой это материал.
- 17:08:2026 - 11:13:47: самосветящаяся геометрия и рой рисуются программой unlit после разброса.
- 17:08:2026 - 11:53:47: ЛЕНТА ТРОПЫ БОЛЬШЕ НЕ РИСУЕТСЯ. Две поверхности, построенные из одного
  поля разными кусками кода, расходятся везде, где хоть одна приближена, и
  лента всплывала или тонула. Это чинится не настройкой, а отсутствием второй
  поверхности. Осталась за дверью DFN_PATH_RIBBON=1 — контрольная рука из
  одного бинарника (правило 47), а не удалена.
- 17:08:2026 - 11:54:29: ЛЕНТА ТРОПЫ БОЛЬШЕ НЕ РИСУЕТСЯ. Две поверхности, построенные из одного
  поля разными кусками кода, расходятся везде, где хоть одна приближена, и лента
  всплывала или тонула. Это чинится не настройкой, а отсутствием второй
  поверхности. Осталась за дверью DFN_PATH_RIBBON=1 — контрольная рука из одного
  бинарника (правило 47), а не удалена.
- 17:08:2026 - 18:41:51: призрак рисуется последним из мировой геометрии, неосвещённым: это ОТВЕТ на
  мире, а не вещь в нём, и на закате он не должен выглядеть иначе.
- 18:08:2026 - 21:12:40: Тело постройки рисуется перед призраком: призрак — ответ поверх мира, дом — вещь в мире.
- 18:08:2026 - 22:26:40: Два потока постройки рисуются prop'ом; попытка отдать им плитки набора снята — читать их некому.
- 19:08:2026 - 02:34:20: Плитки набора для постройки вернулись (fs_prop теперь сэмплит); два потока идут со своими плитками.
- 19:08:2026 - 04:05:50: house_tile_asset — ленивая нарезка листа набора; submit потоков по материалам и дверей с поворотом вокруг петли.
- 19:08:2026 - 04:42:30: Нормальная плитка лениво под своим ключом; submit потоков и дверей с aux-листом рельефа.
- 20:08:2026 - 02:07:34: Листы набора печатаются ОДИН раз и живут полями (было: целый лист 9.4 МБ
  на каждый промах кэша плитки); ключ плитки несёт PARTS_ATLAS_REVISION и упакован без
  зазоров; shutdown() уничтожает меши светляков/свечения/призрака, потоки и двери
  постройки и программу оверлея; DFN_WIND_FREEZE читается один раз.
- 20:08:2026 - 18:40:00: Двери без demo_swing стоят закрытыми (приёмка: «все двери перекошены»).
- 22:08:2026 - 15:40:00: печётся и подаётся лист нормалей земли (PROC_KEY_TERRAIN_NORMALS,
  DrawParams::aux_texture у чанков и LOD-кольца). Дверь DFN_TERRAIN_NORMALS,
  0 = плоская земля прежнего кадра.
*/

#include "engine/render/sources/RenderSystem.h"

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/render/sources/BitmapFont.h"
#include "engine/render/sources/CloudModel.h"
#include "engine/render/sources/FloraCards.h"
#include "engine/render/sources/PartsAtlas.h"
#include "engine/render/sources/Materials.h"
#include "engine/render/sources/ProcMesh.h"
#include "engine/render/sources/ProcTexture.h"
#include "engine/render/sources/ScatterBatcher.h"
#include "engine/render/sources/SkyModel.h"
#include "engine/render/sources/TerrainMesher.h"
#include "engine/render/sources/VoxelMesher.h"
#include "engine/render/sources/Tour.h"
#include "engine/render/sources/WaterMesher.h"
#include "engine/render/sources/WindModel.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/matrix.hpp>

namespace dfn::render {

namespace {
/// DFN_PATH_RIBBON=1 brings the old over-the-ground path ribbon back, for a
/// side-by-side with the ground-borne path that replaced it. Default OFF: the
/// ribbon is the defect, not a feature with a bug in it.
[[nodiscard]] bool path_ribbon_on() {
    static const bool on = [] {
        const char* v = std::getenv("DFN_PATH_RIBBON");
        return v != nullptr && *v != '\0' && *v != '0';
    }();
    return on;
}

// Рельеф земли (22.08). ВКЛЮЧЁН по умолчанию — владелец прямо разрешил
// принципиальные изменения текстур; DFN_TERRAIN_NORMALS=0 — контрольная
// рука «плоская земля» из того же бинарника.
[[nodiscard]] bool terrain_normals_on() {
    static const bool on = [] {
        const char* v = std::getenv("DFN_TERRAIN_NORMALS");
        return v == nullptr || *v == '\0' || *v != '0';
    }();
    return on;
}
} // namespace

namespace {

// Cache keys for the procedural texture registry (params -> dense asset id).
constexpr uint64_t PROC_KEY_TERRAIN_ATLAS = 0x01;
constexpr uint64_t PROC_KEY_WATER = 0x02;
constexpr uint64_t PROC_KEY_LEAF_ATLAS = 0x03;
constexpr uint64_t PROC_KEY_LEAF_NORMALS = 0x07; // bark relief, same layout
constexpr uint64_t PROC_KEY_PATH_ATLAS = 0x04;
constexpr uint64_t PROC_KEY_HOUSE_TILE = 0x08; // одна плитка набора, seed = материал
constexpr uint64_t PROC_KEY_TERRAIN_NORMALS = 0x09; // рельеф земли, слои сплата

uint64_t proc_key(uint64_t kind, uint32_t size, uint32_t seed) {
    return (kind << 56) | (static_cast<uint64_t>(size) << 32) | seed;
}

glm::mat4 interpolated_transform(const components::PreviousTransform& prev,
                                 const components::Transform& curr, float alpha) {
    const glm::vec3 position = glm::mix(prev.position, curr.position, alpha);
    const glm::quat rotation = glm::slerp(prev.rotation, curr.rotation, alpha);
    const glm::vec3 scale = glm::mix(prev.scale, curr.scale, alpha);
    glm::mat4 m = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation);
    return glm::scale(m, scale);
}

math::Aabb bounds_of(const std::vector<platform::Vertex>& vertices) {
    math::Aabb box;
    for (const platform::Vertex& v : vertices) {
        box.expand(v.position);
    }
    return box;
}

} // namespace

bool RenderSystem::visible_or_casting(const math::Frustum& frustum,
                                      const math::Aabb& box, const glm::vec3& eye) {
    if (!box.valid()) {
        return true; // no bounds measured: never cull blind
    }
    if (frustum.visible(box)) {
        return true;
    }
    // Off screen, but still a sun caster? The backend renders every opaque
    // submit into the shadow map, so culling here would delete the shadow with
    // the mesh. Cheap conservative test: sphere around the box centre.
    const glm::vec3 d = box.center() - eye;
    const float reach = LOOKDEV_SHADOW_CASTER_KEEP_M
                      + glm::length(box.half_extents());
    return glm::dot(d, d) <= reach * reach;
}

size_t RenderSystem::ChunkKeyHash::operator()(const glm::ivec2& v) const {
    // 2D grid hash: large odd multipliers, good spread for small coords.
    const auto x = static_cast<uint64_t>(static_cast<uint32_t>(v.x));
    const auto y = static_cast<uint64_t>(static_cast<uint32_t>(v.y));
    return static_cast<size_t>(x * 0x9E3779B97F4A7C15ull ^ (y * 0xC2B2AE3D27D4EB4Full));
}

const PartsAtlas& RenderSystem::parts_sheet(bool normal) {
    // ОДИН ЛИСТ НА ВЕСЬ ЗАПУСК, А НЕ ОДИН НА ПРОМАХ. Печать листа это
    // 2304x1024 пикселей (9.4 МБ) процедурной работы, и она стояла ВНУТРИ
    // house_tile_asset: каждая новая пара (материал, тон) пекла лист целиком,
    // чтобы вырезать из него одну плитку 256x256 — 0.7% посчитанного. Дом из
    // девяти материалов платил это девять раз, и ещё девять за рельеф.
    //
    // Лениво: карта без построек не печатает ни одного листа. Освобождается в
    // shutdown() вместе с текстурами — 18.8 МБ на два листа, и держать их
    // после гибели рендерера незачем.
    PartsAtlas& sheet = normal ? parts_sheet_normal_ : parts_sheet_albedo_;
    bool& ready = normal ? parts_sheet_normal_ready_ : parts_sheet_albedo_ready_;
    if (!ready) {
        sheet = normal ? generate_parts_normal_atlas(PARTS_ATLAS_TILE_PX)
                       : generate_parts_atlas(PARTS_ATLAS_TILE_PX);
        ready = true;
    }
    return sheet;
}

uint32_t RenderSystem::house_tile_asset(platform::IRenderer& renderer, uint32_t surface,
                                        uint32_t tone, bool normal) {
    // ЛЕНИВО И С КЭШЕМ: ключ несёт материал, тон, лист И РЕВИЗИЮ НАБОРА
    // (house_tile_key, RenderSystem.h — там же довод про ревизию и про то,
    // почему прежняя упаковка держалась на арифметическом совпадении). uv
    // постройки считаны в метрах и повторяются wrap'ом — поэтому отдельная
    // плитка, а не атлас.
    const uint64_t key = proc_key(PROC_KEY_HOUSE_TILE, PARTS_ATLAS_TILE_PX,
                                  house_tile_key(surface, tone, normal,
                                                 PARTS_ATLAS_REVISION));
    if (const auto it = proc_texture_ids_.find(key); it != proc_texture_ids_.end()) {
        return it->second;
    }
    const PartsAtlas& sheet = parts_sheet(normal);
    if (sheet.pixels.empty()) {
        return 0; // лист не испёкся: рисовать нечем, и молчать об этом нельзя
    }
    std::vector<uint8_t> tile(static_cast<size_t>(PARTS_ATLAS_TILE_PX)
                              * PARTS_ATLAS_TILE_PX * 4u);
    const uint32_t x0 = surface * PARTS_ATLAS_TILE_PX;
    const uint32_t y0 = tone * PARTS_ATLAS_TILE_PX;
    for (uint32_t y = 0; y < PARTS_ATLAS_TILE_PX; ++y) {
        const uint8_t* src = sheet.pixels.data()
                           + (static_cast<size_t>(y0 + y) * sheet.width + x0) * 4u;
        std::copy(src, src + static_cast<size_t>(PARTS_ATLAS_TILE_PX) * 4u,
                  tile.begin() + static_cast<size_t>(y) * PARTS_ATLAS_TILE_PX * 4u);
    }
    return procedural_texture_asset(renderer, key, PARTS_ATLAS_TILE_PX,
                                    PARTS_ATLAS_TILE_PX, tile.data());
}

uint32_t RenderSystem::procedural_texture_asset(platform::IRenderer& renderer,
                                                uint64_t key, uint32_t width,
                                                uint32_t height,
                                                const uint8_t* pixels) {
    if (const auto it = proc_texture_ids_.find(key); it != proc_texture_ids_.end()) {
        return it->second; // cached by parameters
    }
    const platform::TextureHandle handle = renderer.create_texture(
        width, height, platform::TextureFormat::RGBA8,
        {pixels, static_cast<size_t>(width) * height * 4});
    if (!handle.valid()) {
        return 0;
    }
    const uint32_t asset_id = next_texture_asset_++;
    texture_cache_.emplace(asset_id, handle.id);
    proc_texture_ids_.emplace(key, asset_id);
    return asset_id;
}

bool RenderSystem::init(platform::IRenderer& renderer) {
    terrain_program_ = renderer.load_program("terrain").id;
    unlit_program_ = renderer.load_program("unlit").id;
    water_program_ = renderer.load_program("water").id;
    prop_program_ = renderer.load_program("prop").id;
    foliage_program_ = renderer.load_program("foliage").id;
    overlay_program_ = renderer.load_program("overlay").id;
    path_program_ = renderer.load_program("path").id;

    // A LOGICAL PROGRAM THAT FAILS TO LOAD DRAWS NOTHING, SILENTLY. The
    // backend's submit early-returns on an unknown program id, so a missing
    // shader looks exactly like geometry that was never built — which is how
    // an hour goes into hunting a mesher that was correct all along.
    {
        const struct { const char* name; uint32_t id; } required[] = {
            {"terrain", terrain_program_}, {"unlit", unlit_program_},
            {"water", water_program_},     {"prop", prop_program_},
            {"foliage", foliage_program_}, {"overlay", overlay_program_},
            {"path", path_program_},
        };
        for (const auto& r : required) {
            if (r.id == 0) {
                std::fprintf(stderr,
                             "[render] PROGRAM \"%s\" FAILED TO LOAD — every draw "
                             "that uses it is silently dropped.\n", r.name);
            }
        }
    }

    // Placeholder site meshes under the lead-blessed RenderMesh ids (1..12:
    // dwelling..tower_ruin, then the castle mass) — chunk streaming attaches
    // exactly these ids to site entities, so they render with no further
    // wiring.
    //
    // A GAP IN THIS RANGE IS LOUD NOW, AND THAT IS THE POINT. It used to
    // `continue`: build_site_mesh returned an empty mesh for every id above 7,
    // the loop skipped it without a word, and the ECS pass below swallowed the
    // resulting cache miss just as quietly — so Harrowward was invisible in the
    // world for an entire stage, and (because sim builds collision from these
    // same triangles) intangible with it. Nothing anywhere said so, because
    // "no mesh" and "nothing to draw" were the same code path. Absence
    // presenting as a neutral state is this project's most expensive recurring
    // bug; a blessed id with no geometry must never again be a silent skip.
    for (uint32_t id = SITE_MESH_ID_FIRST; id <= SITE_MESH_ID_LAST; ++id) {
        const MeshData data = build_site_mesh(id);
        if (data.vertices.empty()) {
            std::fprintf(stderr,
                         "[render] BLESSED SITE MESH ID %u HAS NO GEOMETRY — "
                         "worldgen attaches this id to entities that will be "
                         "invisible AND uncollidable. Add it to "
                         "build_site_mesh or shrink SITE_MESH_ID_LAST.\n",
                         id);
            continue;
        }
        const platform::MeshHandle handle =
            renderer.create_mesh(data.vertices, data.indices);
        if (handle.valid()) {
            mesh_cache_.emplace(id, handle.id);
        } else {
            std::fprintf(stderr,
                         "[render] site mesh id %u failed to upload\n", id);
        }
    }

    // Procedural textures (Q4в): generated in code, uploaded once, cached.
    {
        const auto atlas =
            generate_terrain_atlas(LOOKDEV_ATLAS_CELL_PX, LOOKDEV_TEXTURE_SEED);
        atlas_texture_asset_ = procedural_texture_asset(
            renderer,
            proc_key(PROC_KEY_TERRAIN_ATLAS, LOOKDEV_ATLAS_CELL_PX,
                     LOOKDEV_TEXTURE_SEED),
            LOOKDEV_ATLAS_CELL_PX * 2, LOOKDEV_ATLAS_CELL_PX * 2, atlas.data());

        // The terrain NORMAL atlas (22.08, «земля — крашеный ковёр рядом с
        // детализированной стеной»): same layout, same fields, tangent-space
        // relief for the ground the way the parts sheet does it for walls.
        const auto terrain_normals = generate_terrain_normal_atlas(
            LOOKDEV_ATLAS_CELL_PX, LOOKDEV_TEXTURE_SEED);
        terrain_normal_asset_ = procedural_texture_asset(
            renderer,
            proc_key(PROC_KEY_TERRAIN_NORMALS, LOOKDEV_ATLAS_CELL_PX,
                     LOOKDEV_TEXTURE_SEED),
            LOOKDEV_ATLAS_CELL_PX * 2, LOOKDEV_ATLAS_CELL_PX * 2,
            terrain_normals.data());

        // The §8.1 path atlas: cell index IS core's PathClass ordinal.
        const auto path_atlas =
            generate_path_atlas(LOOKDEV_ATLAS_CELL_PX, LOOKDEV_TEXTURE_SEED);
        path_atlas_asset_ = procedural_texture_asset(
            renderer,
            proc_key(PROC_KEY_PATH_ATLAS, LOOKDEV_ATLAS_CELL_PX,
                     LOOKDEV_TEXTURE_SEED),
            LOOKDEV_ATLAS_CELL_PX * 2, LOOKDEV_ATLAS_CELL_PX * 2, path_atlas.data());

        ProcTextureDesc water_desc;
        water_desc.kind = ProcTextureKind::WATER;
        water_desc.size = LOOKDEV_WATER_TEX_PX;
        water_desc.seed = LOOKDEV_TEXTURE_SEED;
        const auto water = generate_proc_texture(water_desc);
        water_texture_asset_ = procedural_texture_asset(
            renderer,
            proc_key(PROC_KEY_WATER, LOOKDEV_WATER_TEX_PX, LOOKDEV_TEXTURE_SEED),
            LOOKDEV_WATER_TEX_PX, LOOKDEV_WATER_TEX_PX, water.data());

        // The leaf mask atlas (flora's zone): tile columns are leaf SHAPES,
        // tile rows are leaf COLOURS, so a card's uv already carries both and
        // no vertex byte is spent on colour. A SEASON CHANGE IS THIS CALL
        // AGAIN with a different enum plus a re-upload — no mesh is rebuilt,
        // no chunk is re-baked, no per-card jitter is invalidated.
        const LeafAtlas leaves =
            generate_leaf_atlas(LEAF_ATLAS_TILE_PX, FloraSeason::Summer);
        leaf_texture_asset_ = procedural_texture_asset(
            renderer,
            // The SHAPE COUNT is part of the key: adding the bark column
            // changed every tile's uv rect, and a cached 4-column atlas under
            // 5-column uvs painted the conifers with birch tiles — foliage
            // went white while the code was correct everywhere.
            proc_key(PROC_KEY_LEAF_ATLAS,
                     LEAF_ATLAS_TILE_PX * 10000u + LEAF_ATLAS_SHAPES * 100u
                         + LEAF_ATLAS_REVISION,
                     static_cast<uint32_t>(FloraSeason::Summer)),
            leaves.width, leaves.height, leaves.pixels.data());

        // THE BARK NORMAL SHEET, from the same producer and the same layout
        // (flora, 15.08.2026). One sheet for the whole atlas rather than one
        // per object: the tile a fragment lands in already says which material
        // it is, so the sampler needs no per-draw bookkeeping at all. Its key
        // carries the same revision — the two sheets are baked together and
        // must never be cached apart.
        const LeafAtlas normals = generate_leaf_normal_atlas(LEAF_ATLAS_TILE_PX);
        leaf_normal_asset_ = procedural_texture_asset(
            renderer,
            proc_key(PROC_KEY_LEAF_NORMALS,
                     LEAF_ATLAS_TILE_PX * 10000u + LEAF_ATLAS_SHAPES * 100u
                         + LEAF_ATLAS_REVISION,
                     0u),
            normals.width, normals.height, normals.pixels.data());
    }

    // Screen overlay quad (map screen now, menus later): a unit quad in model
    // space, placed in front of the camera per frame by draw_overlay. Row 0 of
    // a canvas is the TOP of the screen, hence v = 0 on the +y corners.
    {
        const glm::vec3 face{0.0f, 0.0f, 1.0f};
        const std::array<platform::Vertex, 4> vertices{{
            {{-1.0f, 1.0f, 0.0f}, face, {0.0f, 0.0f}, 0xFFFFFFFFu},
            {{1.0f, 1.0f, 0.0f}, face, {1.0f, 0.0f}, 0xFFFFFFFFu},
            {{1.0f, -1.0f, 0.0f}, face, {1.0f, 1.0f}, 0xFFFFFFFFu},
            {{-1.0f, -1.0f, 0.0f}, face, {0.0f, 1.0f}, 0xFFFFFFFFu},
        }};
        const std::array<uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
        overlay_mesh_ = renderer.create_mesh(vertices, indices).id;
    }
    // Canvas size = the internal target, so one canvas pixel is one screen
    // pixel. The app should confirm it via set_internal_resolution (settings.cfg
    // can override both the constant and the env var).
    internal_res_ = Tour::internal_res_from_env(
        {static_cast<uint32_t>(config::INTERNAL_RES_W),
         static_cast<uint32_t>(config::INTERNAL_RES_H)});
    hud_.resize(internal_res_.x, internal_res_.y);
    // Verification hook (Rule 27): the screenshot tour cannot press M, so
    // DFN_MAP=1 opens the map from the first frame.
    if (const char* menv = std::getenv("DFN_MAP"); menv != nullptr && menv[0] == '1') {
        map_.set_open(true);
    }
    // Verification hook (Rule 27): DFN_FONT_PROBE=1 draws the font specimen
    // into the HUD every frame. It is the ONLY thing in engine/ that puts text
    // on screen without a caller, and it exists because the font's acceptance
    // frame must show every glyph plus the missing-glyph block plus unplated
    // text over real terrain. It stands down the instant the app draws a real
    // prompt, exactly like DFN_TORCH did.
    if (const char* fenv = std::getenv("DFN_FONT_PROBE");
        fenv != nullptr && fenv[0] == '1') {
        font_probe_ = true;
        hud_visible_ = true;
    }

    environment_ = make_default_environment();
    // Verification hook (Rule 27): DFN_TIME=<0..1 day fraction> freezes the sky
    // at an hour (0 = midnight, 0.5 = noon), DFN_MOON=<0..1> sets the phase.
    // In play the app drives the same function every frame from its clock.
    if (const char* tenv = std::getenv("DFN_TIME"); tenv != nullptr && *tenv != '\0') {
        float day = 0.5f;
        float phase = 0.5f;
        if (std::sscanf(tenv, "%f", &day) == 1) {
            if (const char* menv = std::getenv("DFN_MOON"); menv != nullptr) {
                std::sscanf(menv, "%f", &phase);
            }
            sky_frozen_ = true;
            frozen_day_ = day;
            frozen_moon_phase_ = phase;
            apply_sky_time(environment_, day, phase);
        }
    }
    // Verification hooks for the interior shoot (Rule 27). DFN_TORCH=1 lights a
    // carried flame at the camera's hand while the tour holds the player still;
    // DFN_DARK=<0..1> pins the authored darkness the app drives in play. Both
    // are screenshot hooks — the shipping paths are gameplay's CarriedLight and
    // the app's darkness ramp, and the torch hook stands down as soon as a real
    // CarriedLight exists.
    if (const char* tor = std::getenv("DFN_TORCH"); tor != nullptr && tor[0] != '\0') {
        torch_debug_ = tor[0] != '0';
        // "2" stands the flame off ahead of the camera (the brazier probe, see
        // collect_point_lights) instead of holding it at the hand.
        torch_ahead_m_ = tor[0] == '2' ? 6.0f : 0.0f;
    }
    // A/B hook: the same frame with the carried light's cube shadows OFF. A
    // shadow is only provable against its own absence, and this is also the
    // switch that measures what the cube pass costs.
    if (const char* nps = std::getenv("DFN_NO_POINT_SHADOW");
        nps != nullptr && nps[0] == '1') {
        point_shadows_off_ = true;
    }
    if (const char* denv = std::getenv("DFN_DARK"); denv != nullptr && *denv != '\0') {
        float dark = 0.0f;
        if (std::sscanf(denv, "%f", &dark) == 1) {
            dark_frozen_ = true;
            frozen_darkness_ = dark < 0.0f ? 0.0f : (dark > 1.0f ? 1.0f : dark);
        }
    }
    if (const char* ns = std::getenv("DFN_NO_SCATTER"); ns != nullptr && ns[0] == '1') {
        scatter_off_ = true;
    }
    // THE ENVIRONMENT LOG (DFN_ENV_LOG=<path>) — one line per PRESENTED frame,
    // written where the CONSUMER sits: immediately after set_environment(), so
    // it records the values this frame was actually drawn with rather than the
    // values some system computed. No readback, no settle, no freeze; it cannot
    // quiet what it is pointed at. Same instrument class as the lead's
    // DFN_FRAME_LOG (docs/FINDING_RUN_SMEAR.md) and for the same reason: the
    // defect this was opened for ("темнеет, потом мигает" underground) lives in
    // the DIFFERENCE between consecutive frames, and every capture door we own
    // either freezes the tick or waits for a flush.
    //
    // It opens LOUDLY: a run that logged nothing must not be mistakable for a
    // run that logged zeros.
    if (const char* el = std::getenv("DFN_ENV_LOG"); el != nullptr && *el != '\0') {
        env_log_ = std::fopen(el, "wb");
        if (env_log_ == nullptr) {
            std::fprintf(stderr, "[env_log] cannot open \"%s\" for writing\n", el);
        } else {
            std::fprintf(env_log_,
                         "# Daggerfall N per-frame ENVIRONMENT log -- one line per\n"
                         "# PRESENTED frame, taken at the set_environment() call.\n"
                         "# frame dark lights sun_x sun_y sun_z sun_lum amb_lum "
                         "moon_light eye_x eye_y eye_z light0_lum\n");
        }
    }
    // DFN_NO_TUFTS=1 — the ground-tuft layer's counterfactual arm (Rule 30).
    // It has to exist before the layer is worth measuring: "the grass helps"
    // and "the grass costs shimmer" are both claims about a DIFFERENCE, and a
    // difference needs an arm without it shot from the same standpoint by the
    // same binary.
    if (const char* nt = std::getenv("DFN_NO_TUFTS"); nt != nullptr && nt[0] == '1') {
        tufts_off_ = true;
    }
    // Cloud hooks (Rule 27/30). DFN_CLOUD=<0..1> pins the coverage amount:
    // 0 is the CONTROL of the whole pass — the sheet, the cumulus and the
    // ground shadows must all vanish in one move because they are one field
    // (a shadow surviving cover 0 would be the two-copies defect made
    // visible). DFN_VISTIME=<seconds> pins the visual clock, which pins the
    // drift: two runs 30 s of pinned time apart are the deterministic
    // acceptance pair proving coverage moves along the wind.
    if (const char* cenv = std::getenv("DFN_CLOUD"); cenv != nullptr && *cenv != '\0') {
        float cover = 0.0f;
        if (std::sscanf(cenv, "%f", &cover) == 1) {
            cloud_pinned_ = true;
            frozen_cloud_cover_ =
                cover < 0.0f ? 0.0f : (cover > 1.0f ? 1.0f : cover);
        }
    }
    if (const char* venv = std::getenv("DFN_VISTIME"); venv != nullptr && *venv != '\0') {
        float t = 0.0f;
        if (std::sscanf(venv, "%f", &t) == 1) {
            vis_time_frozen_ = true;
            frozen_vis_time_ = t;
        }
    }
    clock_start_ = std::chrono::steady_clock::now();

    // Debug water toggle (stage 3): DFN_WATER=<height_m> covers the testbed
    // area. Proper placement is design-doc-driven via set_water from app/editor.
    if (const char* wenv = std::getenv("DFN_WATER"); wenv != nullptr && *wenv != '\0') {
        float height = 0.0f;
        if (std::sscanf(wenv, "%f", &height) == 1) {
            const float chunk = static_cast<float>(config::CHUNK_SIZE);
            set_water(renderer, height, {chunk * 2.0f, chunk * 2.0f}, chunk * 4.0f);
        } else {
            std::fprintf(stderr, "[render] malformed DFN_WATER '%s' (want meters)\n",
                         wenv);
        }
    }

    // foliage_program_ is NOT required for init to succeed: the null backend
    // hands out valid ids for everything, but a platform without the embedded
    // shader pair would otherwise take the whole renderer down over leaves.
    return terrain_program_ != 0 && unlit_program_ != 0 && water_program_ != 0
        && prop_program_ != 0;
}

void RenderSystem::report_upload_failure(const char* what) {
    // ONE line per run, not one per failure: the tour produced 13903 of these
    // and the useful information is the first one plus the totals.
    if (upload_failure_reported_) {
        return;
    }
    upload_failure_reported_ = true;
    std::fprintf(stderr,
                 "[render] A %s FAILED TO UPLOAD and will not be drawn. The GPU "
                 "buffer budget is spent; everything created after this point is "
                 "missing from the world, silently. Counts so far: terrain %llu, "
                 "voxel %llu, scatter chunks %llu, scatter meshes %llu.\n",
                 what,
                 static_cast<unsigned long long>(uploads_.terrain),
                 static_cast<unsigned long long>(uploads_.voxel),
                 static_cast<unsigned long long>(uploads_.scatter_chunks),
                 static_cast<unsigned long long>(uploads_.scatter_meshes));
}

void RenderSystem::shutdown(platform::IRenderer& renderer) {
    if (uploads_.failed > 0 || std::getenv("DFN_MESH_STATS") != nullptr) {
        std::fprintf(stderr,
                     "[render] uploads: terrain %llu, voxel %llu, scatter chunks "
                     "%llu (%llu meshes), FAILED %llu.\n",
                     static_cast<unsigned long long>(uploads_.terrain),
                     static_cast<unsigned long long>(uploads_.voxel),
                     static_cast<unsigned long long>(uploads_.scatter_chunks),
                     static_cast<unsigned long long>(uploads_.scatter_meshes),
                     static_cast<unsigned long long>(uploads_.failed));
    }
    if (env_log_ != nullptr) {
        std::fclose(env_log_);
        env_log_ = nullptr;
    }
    clear_water(renderer);
    clear_water_bodies(renderer);
    clear_path_surface(renderer);
    if (overlay_mesh_ != 0) {
        renderer.destroy_mesh(platform::MeshHandle{overlay_mesh_});
        overlay_mesh_ = 0;
    }
    if (overlay_texture_ != 0) {
        renderer.destroy_texture(platform::TextureHandle{overlay_texture_});
        overlay_texture_ = 0;
    }
    for (const auto& [coord, res] : terrain_meshes_) {
        renderer.destroy_mesh(platform::MeshHandle{res.mesh_id});
    }
    terrain_meshes_.clear();
    lod_.destroy_all(renderer);
    for (auto& [coord, scatter] : scatter_meshes_) {
        if (scatter.trees_mesh_id != 0) {
            renderer.destroy_mesh(platform::MeshHandle{scatter.trees_mesh_id});
        }
        if (scatter.foliage_mesh_id != 0) {
            renderer.destroy_mesh(platform::MeshHandle{scatter.foliage_mesh_id});
        }
        for (const MicroTileRes& tile : scatter.micro) {
            renderer.destroy_mesh(platform::MeshHandle{tile.mesh_id});
        }
    }
    scatter_meshes_.clear();
    if (tuft_mesh_id_ != 0) {
        renderer.destroy_mesh(platform::MeshHandle{tuft_mesh_id_});
        tuft_mesh_id_ = 0;
    }
    tuft_spots_.clear();
    tuft_built_ = false;
    // МЕШИ, КОТОРЫЕ ЖИВУТ НЕ В ЧАНКЕ И НЕ В КЭШЕ — и потому не сносились ничем.
    // У каждого есть свой сеттер, каждый сеттер честно освобождает ПРЕДЫДУЩУЮ
    // заливку, и ровно поэтому пропажа была невидима: течёт не поток заливок, а
    // ПОСЛЕДНЯЯ заливка каждого слота, переживающая рендерер.
    if (firefly_mesh_id_ != 0) {
        renderer.destroy_mesh(platform::MeshHandle{firefly_mesh_id_});
        firefly_mesh_id_ = 0;
    }
    if (emissive_mesh_id_ != 0) {
        renderer.destroy_mesh(platform::MeshHandle{emissive_mesh_id_});
        emissive_mesh_id_ = 0;
    }
    if (ghost_mesh_id_ != 0) {
        renderer.destroy_mesh(platform::MeshHandle{ghost_mesh_id_});
        ghost_mesh_id_ = 0;
    }
    for (const HouseStreamGpu& st : house_streams_) {
        renderer.destroy_mesh(platform::MeshHandle{st.mesh_id});
    }
    house_streams_.clear();
    for (const HouseDoorGpu& d : house_doors_) {
        renderer.destroy_mesh(platform::MeshHandle{d.mesh_id});
    }
    house_doors_.clear();
    // ЛИСТЫ НАБОРА: 18.8 МБ, которые незачем держать после рендерера.
    parts_sheet_albedo_ = {};
    parts_sheet_normal_ = {};
    parts_sheet_albedo_ready_ = false;
    parts_sheet_normal_ready_ = false;
    for (const auto& [asset, mesh_id] : mesh_cache_) {
        renderer.destroy_mesh(platform::MeshHandle{mesh_id});
    }
    mesh_cache_.clear();
    for (const auto& [asset, tex_id] : texture_cache_) {
        renderer.destroy_texture(platform::TextureHandle{tex_id});
    }
    texture_cache_.clear();
    proc_texture_ids_.clear();
    atlas_texture_asset_ = 0;
    water_texture_asset_ = 0;
    leaf_texture_asset_ = 0;
    leaf_normal_asset_ = 0;
    terrain_normal_asset_ = 0;
    next_texture_asset_ = 1;
    renderer.destroy_program(platform::ProgramHandle{terrain_program_});
    renderer.destroy_program(platform::ProgramHandle{unlit_program_});
    renderer.destroy_program(platform::ProgramHandle{path_program_});
    renderer.destroy_program(platform::ProgramHandle{water_program_});
    renderer.destroy_program(platform::ProgramHandle{prop_program_});
    renderer.destroy_program(platform::ProgramHandle{foliage_program_});
    // ПРОГРАММА ОВЕРЛЕЯ. Единственная из семи, которую init() загружает, а
    // shutdown() не уничтожал — и по ней рисуется весь интерфейс, то есть
    // пропущена была не редкая ветка, а та, что работает каждый кадр с
    // открытым экраном.
    renderer.destroy_program(platform::ProgramHandle{overlay_program_});
    terrain_program_ = 0;
    overlay_program_ = 0;
    unlit_program_ = 0;
    water_program_ = 0;
    path_program_ = 0;
    path_atlas_asset_ = 0;
    prop_program_ = 0;
    foliage_program_ = 0;
}

void RenderSystem::render(ecs::World& world, platform::IRenderer& renderer,
                          const FirstPersonCamera& camera, float alpha) {
    renderer.begin_frame(camera.view(alpha), camera.proj());

    // Frame environment: visual clock drives water/UV animation only (never
    // simulation — Rule 12 keeps gameplay off the wall clock; this is render).
    // ONE CLOCK IF THE CALLER HAS ONE, the wall clock if not. See
    // RenderSystem::set_visual_time for the measurement: with the app's own
    // frame-deterministic clock told to us, two runs of a recipe come out byte
    // for byte; with this steady_clock read they differed by 67.5 % of the sky.
    // The wall-clock branch stays because a caller that never says anything
    // must keep the behaviour it had.
    environment_.time_seconds =
        visual_time_told_
            ? told_visual_time_
            : std::chrono::duration<float>(
                  std::chrono::steady_clock::now() - clock_start_).count();
    // DFN_VISTIME pins the visual clock: wind envelope, water scroll and the
    // cloud drift all become pure functions of the pinned value, which is
    // what makes the two-timestamp drift pair deterministic evidence.
    if (vis_time_frozen_) {
        environment_.time_seconds = frozen_vis_time_;
    }
    // Screenshot determinism: the app writes the sky from its own clock every
    // frame, so the frozen hour has to be re-asserted here, after it.
    if (sky_frozen_) {
        apply_sky_time(environment_, frozen_day_, frozen_moon_phase_);
    }
    // THE SHARED WIND (W3) drives everything that moves: foliage sway, the
    // audio bed's gain (sim reads env.wind_strength), and the cloud drift.
    // Called HERE and not in the app: grep found no live apply_wind call
    // site anywhere — env.wind_strength sat at its 0.0 default and the zero
    // read as a calm day (absence presenting as neutral), so the model is
    // now driven from render's own frame path where it cannot be dropped.
    // DFN_WIND_FREEZE=<seconds>: pin the wind clock for pixel-diff evidence
    // (Rule 27/30 hook, never a shipping path).
    //
    // IT EXISTS BECAUSE IT INVALIDATED A NUMBER. tools/pngdiff.py's header
    // blames the tour's non-determinism on chunk streaming under a frame-count
    // wait (Rule 42), and for the tour route that is right — but on a SINGLE
    // PINNED probe, with nothing left to stream, the canopy-speckle control
    // still came back DIRTY and got WORSE the longer the shutter waited:
    // 0.000 % at 240 settle frames, 1.990 % at 700, near canopy, same binary,
    // same pose, nothing changed. That is not streaming, which converges; it is
    // THIS LINE. `time_seconds` accumulates wall-clock frame_dt, so two runs
    // reach the shutter with different gust phases and the crown is genuinely
    // in a different position in each. Longer settle = more divergence, which
    // is the exact opposite of the usual advice and is how it was caught.
    //
    // AND THEN THE HOOK ANSWERED THE QUESTION IT WAS BUILT TO DODGE. The first
    // version of this comment said a canopy diff "cannot separate the shimmer
    // the user reports from the rustle he says he likes — the same pixels
    // flipping by the same amount". That is true of ONE live-wind diff and
    // false of the zone, because a pinnable clock turns the rustle into an arm
    // you can hold still. Advancing the wind by exactly one 120 fps frame with
    // the eye STATIONARY, against the same 0.000 % control (near / treeline):
    //     rustle alone  (wind +1/120 s, eye still)   0.008 % / 0.003 %
    //     motion alone  (wind frozen, eye +0.05 m)   0.864 % / 0.093 %
    //     both          (the frame pair he sees)     1.001 % / 0.103 %
    // THE RUSTLE IS 1 % OF WHAT CHANGES PER FRAME UNDER THE CROWNS and 3 % at
    // the treeline; the running motion is ~108x and ~31x larger. So the two are
    // not in tension at all: whatever suppresses the running shimmer can be
    // pushed hard without spending the rustle, and an agent who softens the
    // canopy's ANIMATION to chase this number is trading away the thing the
    // user likes to buy one part in a hundred of the thing he complained about.
    // It pins time_seconds ITSELF and not just apply_wind's argument: the sway
    // in dfn_wind_offset runs off u_envTime, so freezing only the gust envelope
    // would leave the leaves moving and the control still dirty — a fix that
    // measures as a fix.
    // ЧИТАЕТСЯ ОДИН РАЗ, КАК У СОСЕДНИХ ДВЕРЕЙ. std::getenv стоял ЗДЕСЬ, то
    // есть в теле кадра: обход таблицы окружения шестьдесят раз в секунду ради
    // значения, которое не может измениться за время жизни процесса.
    static const bool wind_frozen = std::getenv("DFN_WIND_FREEZE") != nullptr;
    static const float wind_freeze_t = [] {
        const char* wf = std::getenv("DFN_WIND_FREEZE");
        return wf != nullptr ? static_cast<float>(std::atof(wf)) : 0.0f;
    }();
    if (wind_frozen) {
        environment_.time_seconds = wind_freeze_t;
    }
    apply_wind(environment_, environment_.time_seconds);
    // Clouds (W4): ONE coverage field, drifting along the wind just applied.
    // The offset written here is read by BOTH samplers (sky sheet + ground
    // shadow); the state tuple (cover/cumulus/shadow) stays whatever the app
    // or the "scattered" defaults put there.
    if (cloud_pinned_) {
        environment_.cloud_cover = frozen_cloud_cover_;
        // Cumulus follows the pin to zero so DFN_CLOUD=0 empties the WHOLE
        // sky (the Rule 30 control), not just the sheet.
        if (frozen_cloud_cover_ <= 0.0f) {
            environment_.cloud_cumulus = 0.0f;
        }
    }
    // THE EYE IS AN ARGUMENT NOW, because the ceiling's HEIGHT is a field of
    // place as well as of weather (R3.4, the user's own framing: «на разных
    // локациях будут на разных высотах, и в разную погоду на разных, будем
    // таким образом погоду и климат отображать»). The place term's wavelength
    // is two world widths, so this is a slow trend across the map and not
    // something a walking player can see move.
    const glm::vec3 cloud_eye = camera.interpolated_pose(alpha).position;
    apply_clouds(environment_, environment_.time_seconds,
                 glm::vec2(cloud_eye.x, cloud_eye.z));
    // Carried lights (the torch) are gathered from the ECS every frame, AFTER
    // the sky: apply_sky_time never touches the light array, and the light has
    // to be in the environment before set_environment or the backend builds
    // this frame's cube faces around a stale flame.
    collect_point_lights(world, camera, alpha);
    renderer.set_environment(environment_);
    if (env_log_ != nullptr) {
        const glm::vec3 eye = camera.interpolated_pose(alpha).position;
        const glm::vec3& sc = environment_.sun_color;
        const glm::vec3& ac = environment_.ambient_color;
        std::fprintf(env_log_,
                     "%llu %.6f %u %.4f %.4f %.4f %.4f %.4f %.4f %.3f %.3f %.3f %.5f\n",
                     static_cast<unsigned long long>(env_log_frame_++),
                     static_cast<double>(environment_.ambient_darkness),
                     environment_.point_light_count,
                     static_cast<double>(environment_.sun_direction.x),
                     static_cast<double>(environment_.sun_direction.y),
                     static_cast<double>(environment_.sun_direction.z),
                     static_cast<double>(0.2126f * sc.x + 0.7152f * sc.y + 0.0722f * sc.z),
                     static_cast<double>(0.2126f * ac.x + 0.7152f * ac.y + 0.0722f * ac.z),
                     static_cast<double>(environment_.moon_light),
                     static_cast<double>(eye.x), static_cast<double>(eye.y),
                     static_cast<double>(eye.z),
                     // The first point light's own brightness: without it a
                     // breathing flame can only be argued for by eye.
                     static_cast<double>(
                         environment_.point_light_count > 0
                             ? 0.2126f * environment_.point_lights[0].color.x
                                   + 0.7152f * environment_.point_lights[0].color.y
                                   + 0.0722f * environment_.point_lights[0].color.z
                             : 0.0f));
    }

    // Frustum culling (core's math). Culling is NOT free of consequences here:
    // see visible_or_casting — off-screen meshes near the eye are kept because
    // they still cast into the sun shadow map.
    const math::Frustum frustum =
        math::Frustum::from_view_proj(camera.proj() * camera.view(alpha));
    const glm::vec3 cull_eye = camera.interpolated_pose(alpha).position;

    // Terrain: world-space meshes, identity transform, splat atlas bound.
    const glm::mat4 identity(1.0f);
    const platform::ProgramHandle terrain{terrain_program_};
    platform::TextureHandle atlas{};
    if (const auto it = texture_cache_.find(atlas_texture_asset_);
        it != texture_cache_.end()) {
        atlas.id = it->second;
    }
    // The ground's relief sheet rides DrawParams::aux_texture exactly like
    // bark normals on foliage; params.w>0.5 is what lets fs_terrain perturb.
    // DFN_TERRAIN_NORMALS=0 is the flat-ground control arm out of one binary.
    platform::DrawParams terrain_params;
    if (terrain_normals_on()) {
        if (const auto it = texture_cache_.find(terrain_normal_asset_);
            it != texture_cache_.end()) {
            terrain_params.aux_texture.id = it->second;
        }
    }
    for (const auto& [coord, res] : terrain_meshes_) {
        if (!visible_or_casting(frustum, res.bounds, cull_eye)) {
            continue;
        }
        renderer.submit(platform::MeshHandle{res.mesh_id}, terrain, identity, atlas,
                        terrain_params);
    }

    // Coarse LOD nodes: the same program, the same atlas, the same splat — the
    // only difference is the sample step and the per-draw fade. They are
    // submitted AFTER the chunk terrain so that in the one case the two can
    // overlap (a streamed rectangle not aligned to the 128 m node grid) the
    // near, finer surface has already written depth.
    lod_.draw(renderer, frustum, terrain, atlas, terrain_params.aux_texture);

    // The §8.1 PATH SURFACE, drawn after the ground it lies on. Depth alone
    // would resolve the order (the tread sits PATH_GROOVE_DEPTH proud of the
    // flattened ground core sank for it), but the LOD nodes above may overlap
    // the chunk terrain, and a path is the one surface that must never lose
    // that tie. Never a shadow caster — see NON_CASTING_PROGRAMS.
    // THE RIBBON IS GONE, and the door is what proves it was the culprit.
    //
    // A path used to be drawn as a separate ribbon mesh laid over the terrain.
    // Two surfaces built from one field disagree wherever either is
    // approximated — the terrain is a voxel extraction, the ribbon a stack of
    // cross-sections — so the ribbon floated or sank, and no amount of tuning
    // fixes that: the second surface IS the defect. The user said so plainly
    // (17.08): «тропинки должны быть свойством земли, а не поверх нарисованной
    // текстурой — тогда проблем не будет».
    //
    // The path now rides the ground's own vertices (SurfaceFieldView::path_wear
    // -> vertex alpha -> fs_terrain), so it cannot hover over itself. This
    // submit stays behind a door rather than being deleted: it is the CONTROL
    // ARM out of one binary (Rule 47) for anyone who wants to see the old
    // defect, and the pieces are still built, so nothing else changes with it.
    if (path_ribbon_on() && !path_meshes_.empty()) {
        const platform::ProgramHandle path{path_program_};
        platform::TextureHandle path_atlas{};
        if (const auto it = texture_cache_.find(path_atlas_asset_);
            it != texture_cache_.end()) {
            path_atlas.id = it->second;
        }
        // aux0 carries the material's tiles per metre: the path atlas has its
        // own scale and lives on a mesh whose uv is already in metres, so it
        // cannot ride on u_terrainTiles (which is per CHUNK).
        platform::DrawParams params;
        params.aux0 = PATH_TILES_PER_M;
        for (const WaterBucket& piece : path_meshes_) {
            if (!frustum.visible(piece.bounds)) {
                continue;
            }
            renderer.submit(platform::MeshHandle{piece.mesh_id}, path, identity,
                            path_atlas, params);
        }
    }

    // Scatter batches (stage 3b): trees always; bush/stone micro tiles only
    // within GRASS_VIEW_DISTANCE of the eye (LANDSCAPE §2.3 micro contract).
    const platform::ProgramHandle prop{prop_program_};
    const glm::vec3 eye = camera.interpolated_pose(alpha).position;

    // GROUND TUFTS. Grown around the eye, drawn as ordinary opaque props: the
    // blades are geometry, not an alpha mask, so the coverage AA already
    // running on the internal target handles their edges and no mask, no
    // cutout and no mip chain is involved (GroundTufts.cpp says why that
    // choice is the anti-shimmer one). No frustum test: the whole layer lives
    // inside a 12 m ball around the eye, so the cull would cost more than the
    // draw it saves, and the shadow pass wants it anyway.
    refresh_ground_tufts(renderer, eye);
    // FLORA DETAIL BANDING. Sits beside the tuft refresh because it is the same
    // kind of thing — geometry re-grown around the eye — and because both must
    // see the eye of the frame being drawn, not the one before it.
    refresh_scatter_lod(renderer, eye);
    if (tuft_mesh_id_ != 0) {
        renderer.submit(platform::MeshHandle{tuft_mesh_id_}, prop, identity);
    }
    const auto micro_range = static_cast<float>(config::GRASS_VIEW_DISTANCE);
    const platform::ProgramHandle foliage{foliage_program_};
    platform::TextureHandle leaf_atlas{};
    if (const auto it = texture_cache_.find(leaf_texture_asset_);
        it != texture_cache_.end()) {
        leaf_atlas.id = it->second;
    }
    // The bark relief rides every foliage draw as the aux sheet. Neutral
    // everywhere but the bark column, so leaves are untouched by construction
    // and this needs no per-draw decision.
    platform::DrawParams foliage_params;
    if (const auto it = texture_cache_.find(leaf_normal_asset_);
        it != texture_cache_.end()) {
        foliage_params.aux_texture.id = it->second;
    }
    for (const auto& [coord, scatter] : scatter_meshes_) {
        const bool chunk_visible =
            visible_or_casting(frustum, scatter.bounds, cull_eye);
        if (chunk_visible && scatter.trees_mesh_id != 0) {
            renderer.submit(platform::MeshHandle{scatter.trees_mesh_id}, prop,
                            identity);
        }
        // Leaf cards: their own program (alpha test + wind + leaf
        // translucency) and their own texture, which the backend also binds on
        // the shadow-cutout caster so the canopy punches its holes through the
        // depth map instead of casting solid rectangles.
        if (chunk_visible && scatter.foliage_mesh_id != 0 && foliage_program_ != 0) {
            renderer.submit(platform::MeshHandle{scatter.foliage_mesh_id}, foliage,
                            identity, leaf_atlas, foliage_params);
        }
        for (const MicroTileRes& tile : scatter.micro) {
            if (!chunk_visible) {
                break; // the whole chunk is behind us
            }
            const glm::vec2 d = tile.center_xz - glm::vec2{eye.x, eye.z};
            const float max_dist = micro_range + tile.radius_m;
            if (glm::dot(d, d) <= max_dist * max_dist) {
                renderer.submit(platform::MeshHandle{tile.mesh_id}, prop, identity);
            }
        }
    }

    // THE SWARM, unlit and last among the world's geometry: the motes ARE
    // light, so shading them by the sun would put them out at exactly the hour
    // they exist for.
    if (emissive_mesh_id_ != 0 && unlit_program_ != 0) {
        renderer.submit(platform::MeshHandle{emissive_mesh_id_},
                        platform::ProgramHandle{unlit_program_}, identity);
    }
    if (firefly_mesh_id_ != 0 && unlit_program_ != 0) {
        renderer.submit(platform::MeshHandle{firefly_mesh_id_},
                        platform::ProgramHandle{unlit_program_}, identity);
    }
    // ПОСТРОЙКА РЕДАКТОРА — ОСВЕЩЁННАЯ, как и всякая вещь в мире. Перед
    // призраком: призрак рисуется поверх всего, потому что он ответ.
    // ПОСТРОЙКА РЕДАКТОРА — ОСВЕЩЁННАЯ, как и всякая вещь в мире. Два потока,
    // каркас и полотна: у них разный материал, и на сегодня материал — это
    // ЦВЕТ ВЕРШИН, а не текстура.
    //
    // ПОЧЕМУ НЕ ТЕКСТУРА (измерено 18.08, а не предположено). Программа «prop»
    // текстуру НЕ ЧИТАЕТ вовсе: её фрагмент — плоский цвет вершины. Я завёл
    // было две плитки из листа набора и передавал их в submit — картинка не
    // изменилась ни на пиксель, потому что читать их было некому. Чтобы дом
    // носил дерево и штукатурку, нужна программа, которая СЭМПЛИТ (своя
    // «built» или ветка в prop), и это работа в зоне платформы.
    if (prop_program_ != 0) {
        const auto tex_of = [&](uint32_t asset) {
            platform::TextureHandle t{};
            if (const auto it = texture_cache_.find(asset); it != texture_cache_.end()) {
                t.id = it->second;
            }
            return t;
        };
        for (const HouseStreamGpu& st : house_streams_) {
            // aux-лист несёт РЕЛЬЕФ: борозды бруса и зерно штукатурки читаются
            // per-pixel, и никакой цвет вершины их не заменит (см. fs_prop).
            platform::DrawParams dp;
            dp.aux_texture = tex_of(st.normal_asset);
            renderer.submit(platform::MeshHandle{st.mesh_id},
                            platform::ProgramHandle{prop_program_}, identity,
                            tex_of(st.texture_asset), dp);
        }
        // ДВЕРИ КАЧАЮТСЯ ВОКРУГ ПЕТЛИ. Ход демонстрационный (0..~85° и назад):
        // редактору важно ПОКАЗАТЬ, что петля стоит на выбранной паре; игровое
        // «открыто/закрыто» будет состоянием симуляции, а не синусом.
        house_door_phase_ += 0.008f;
        const float swing = (1.0f - std::cos(house_door_phase_)) * 0.5f * 1.48f;
        for (const HouseDoorGpu& d : house_doors_) {
            const glm::vec3 axis_v = d.hinge_b - d.hinge_a;
            const float axis_len = glm::length(axis_v);
            glm::mat4 xform(1.0f);
            // Качается ТОЛЬКО дверь с demo_swing (выбранная в сессии):
            // остальные полотна стоят закрытыми — приёмка 20.08 прочитала
            // общее качание как «все двери перекошены».
            if (d.demo_swing && axis_len > 1e-4f) {
                xform = glm::translate(glm::mat4(1.0f), d.hinge_a)
                      * glm::rotate(glm::mat4(1.0f), swing, axis_v / axis_len)
                      * glm::translate(glm::mat4(1.0f), -d.hinge_a);
            }
            platform::DrawParams dp;
            dp.aux_texture = tex_of(d.normal_asset);
            renderer.submit(platform::MeshHandle{d.mesh_id},
                            platform::ProgramHandle{prop_program_}, xform,
                            tex_of(d.texture_asset), dp);
        }
    }
    // THE BUILD GHOST last of the world's geometry, unlit: it is an ANSWER
    // painted on the world, not a thing in it, and shading it by the sun would
    // make the same verdict look different at dusk.
    if (ghost_mesh_id_ != 0 && unlit_program_ != 0) {
        renderer.submit(platform::MeshHandle{ghost_mesh_id_},
                        platform::ProgramHandle{unlit_program_}, identity);
    }

    // ECS renderables: interpolated fixed-step transforms (Rule 12). Site
    // entities carry the blessed placeholder mesh ids 1..7 (registered at
    // init); drawn lit+fogged via "prop".
    world.view<components::Transform, components::PreviousTransform,
               components::RenderMesh>()
        .each([&](ecs::EntityId id, components::Transform& curr,
                  components::PreviousTransform& prev, components::RenderMesh& rm) {
            // Map discovery: a site is remembered as soon as its chunk is
            // resident, BEFORE the mesh lookup, so a site with no mesh is
            // still discoverable on the map rather than doubly absent.
            map_.note_site(rm.mesh_asset, curr.position);
            // THE SENTINEL IS NOT A MISSING ASSET. `RenderMesh::mesh_asset`
            // documents 0 as "none" (engine/core/components), and it is
            // PRODUCED DELIBERATELY — a hidden bone segment, an empty item
            // slot. The warning below was firing on all of it, every launch,
            // which is Rule 38's failure mode moved from the test suite into
            // the log: a check that goes red on correct code does not get
            // argued with, it gets ignored, and then it cannot report the one
            // id that really is unregistered either. Drawing nothing for "no
            // mesh" is the correct outcome, so there is nothing to look up.
            if (rm.mesh_asset == 0) {
                return;
            }
            const auto mesh_it = mesh_cache_.find(rm.mesh_asset);
            if (mesh_it == mesh_cache_.end()) {
                // ONCE per id, never per frame: a per-frame warning at 60 fps
                // is noise nobody reads, which is the same silence with extra
                // steps. Once is enough to make an unregistered asset id
                // impossible to ship unnoticed.
                if (warned_missing_meshes_.insert(rm.mesh_asset).second) {
                    std::fprintf(stderr,
                                 "[render] entity wants mesh asset %u, which is "
                                 "not registered — it draws as NOTHING\n",
                                 rm.mesh_asset);
                }
                return;
            }
            platform::TextureHandle texture{};
            const auto tex_it = texture_cache_.find(rm.texture_asset);
            if (tex_it != texture_cache_.end()) {
                texture.id = tex_it->second;
            }
            platform::DrawParams params;
            // В28 CROSSHAIR PICK: name THIS entity under the reticle. The pick
            // id is a single uint32_t, but EntityId is {index, generation} — 64
            // bits — so it cannot carry both. It carries the INDEX, which is the
            // slot: for a thing under the crosshair RIGHT NOW (alive by
            // construction) the index alone maps back to the entity via World's
            // current generation for that slot; the generation only ever matters
            // across a destroy+reuse, which a live pick is not. STAMPED +1 so a
            // real entity is never mistaken for the "0 = unnamed" sentinel the
            // contract reserves — entity index 0 is a real slot, and letting it
            // collide with "unnamed" is exactly the absence-reads-as-neutral bug
            // this codebase keeps paying for. The overlay maps back with (id-1).
            params.pick_id = id.index + 1u;
            // A MESH THAT HOLDS A FLAME NEVER SHADOWS IT. The entity that
            // carries an active CarriedLight (sconce, held torch, lantern)
            // stands AT its own light, inside the light's near field, and the
            // cube map cannot represent an occluder that surrounds its own
            // origin: whatever fraction of the sphere the holder covers goes
            // to zero for the whole frame. Measured on the wall sconce: the
            // flame sits inside the mesh's head, every texel of all six faces
            // held the head's surfaces at 0.11-0.64 m, and the floor 2.79 m
            // from a burning torch read 0 of 255 across 230 400 pixels. The
            // cost of the exemption is only ever the holder's own silhouette
            // -- centimetres from the flame, degenerate in the map anyway.
            // DFN_SELF_POINT_CAST=1 is the counterfactual arm: the holder
            // casts again and the defect comes back, from this same binary
            // (Rule 47). Loud, like every dose door here.
            static const bool self_cast = [] {
                const char* e = std::getenv("DFN_SELF_POINT_CAST");
                const bool on = e != nullptr && *e == '1';
                if (on) {
                    std::fprintf(stderr,
                                 "[render] DFN_SELF_POINT_CAST=1: light "
                                 "holders cast into their own point shadows "
                                 "again (the defect arm)\n");
                }
                return on;
            }();
            if (const auto* cl = world.get<components::CarriedLight>(id);
                cl != nullptr && cl->active && !self_cast) {
                params.casts_in_point_shadows = false;
            }
            renderer.submit(platform::MeshHandle{mesh_it->second}, prop,
                            interpolated_transform(prev, curr, alpha), texture,
                            params);
        });

    // Water: transparent, so submitted after all opaques (the backend renders
    // the scene view sequentially and gives "water" a no-depth-write blend).
    platform::TextureHandle water_tex{};
    if (const auto it = texture_cache_.find(water_texture_asset_);
        it != texture_cache_.end()) {
        water_tex.id = it->second;
    }
    const platform::ProgramHandle water{water_program_};
    for (const WaterBucket& bucket : water_body_meshes_) {
        // Water never casts a sun shadow (transparent programs skip the depth
        // pass), so this is a plain frustum test with no caster exemption.
        if (!frustum.visible(bucket.bounds)) {
            continue;
        }
        renderer.submit(platform::MeshHandle{bucket.mesh_id}, water, identity,
                        water_tex);
    }
    if (water_mesh_ != 0) { // debug fallback plane (set_water / DFN_WATER)
        renderer.submit(platform::MeshHandle{water_mesh_}, water, identity,
                        water_tex);
    }

    // HUD: transparent, over the world, UNDER any full-screen screen. The
    // caller draws into hud() each frame; the probe hook is the only in-engine
    // author (see init()).
    // The probe forces the layer ON rather than riding on hud_visible_. The app
    // legitimately owns that flag — it sets it false when there is nothing to
    // prompt — and the first time it did, the font's acceptance frame came back
    // EMPTY with nothing anywhere saying the hook had been switched off. A
    // verification hook a peer's correct change can silently disable is a hook
    // that will lie to the next agent who trusts it.
    // The guard asks whether the HUD HAS a grid, not whether it matches the
    // scene's: since the interface keeps its own (see set_internal_resolution),
    // demanding equality here would hide the whole interface at any resolution
    // but the design one.
    const bool show_hud = (hud_visible_ || font_probe_) && hud_.width() > 0
                          && hud_.height() > 0;
    if (show_hud) {
        if (font_probe_) {
            hud_.clear_transparent();
            draw_font_specimen(hud_);
        }
        draw_overlay(renderer, hud_, camera, alpha, true);
    }

    // Map screen: last submit of the frame, opaque, covering everything. The
    // world behind it is still drawn (a few hundred microseconds at these
    // budgets) so toggling the map is instant and needs no app-loop change.
    if (map_.open()) {
        const CameraPose pose = camera.interpolated_pose(alpha);
        draw_overlay(renderer,
                     map_.compose(hud_.width(), hud_.height(), pose.position,
                                  pose.yaw),
                     camera, alpha);
    }

    renderer.end_frame();
}


void RenderSystem::set_world_bounds(glm::vec2 min_xz, glm::vec2 max_xz) {
    lod_.set_world_bounds(min_xz, max_xz);
}

void RenderSystem::set_streamed_rect(glm::vec2 min_xz, glm::vec2 max_xz) {
    lod_.set_resident_rect(min_xz, max_xz);
}

void RenderSystem::update_lod(const glm::vec3& eye, float dt_seconds) {
    lod_.update(eye, dt_seconds);
}

void RenderSystem::upload_lod_node(platform::IRenderer& renderer, const LodNode& node,
                                   const math::HeightFieldView& field,
                                   const math::SurfaceFieldView* surface) {
    lod_.upload(renderer, node, field, surface);
}

void RenderSystem::drop_lod_node(platform::IRenderer& renderer, const LodNode& node) {
    lod_.drop(renderer, node);
}

void RenderSystem::upload_terrain(platform::IRenderer& renderer,
                                  const math::HeightFieldView& field) {
    upload_terrain(renderer, field, nullptr);
}

void RenderSystem::upload_terrain(platform::IRenderer& renderer,
                                  const math::HeightFieldView& field,
                                  const math::SurfaceFieldView* surface) {
    // Explored map: the chunk is baked into the map the moment it streams in,
    // and stays there after unload (drop_terrain frees the GPU mesh only).
    map_.note_chunk(field, surface);
    const TerrainMeshData data = build_terrain_mesh(field, surface);
    if (data.vertices.empty()) {
        return;
    }
    const platform::MeshHandle handle = renderer.create_mesh(data.vertices, data.indices);
    if (!handle.valid()) {
        ++uploads_.failed;
        report_upload_failure("terrain chunk");
        return;
    }
    ++uploads_.terrain;
    // Idempotent per coord: replace (and free) any previous upload.
    const TerrainRes res{handle.id, bounds_of(data.vertices)};
    const auto it = terrain_meshes_.find(field.chunk_coord);
    if (it != terrain_meshes_.end()) {
        renderer.destroy_mesh(platform::MeshHandle{it->second.mesh_id});
        it->second = res;
    } else {
        terrain_meshes_.emplace(field.chunk_coord, res);
    }
}

void RenderSystem::upload_terrain_voxel(platform::IRenderer& renderer,
                                        const math::VoxelMeshView& mesh,
                                        const math::HeightFieldView* field,
                                        const math::SurfaceFieldView* surface) {
    // THE MAP IS RECORDED HERE FIRST, and before any early-out below. A chunk
    // the player streamed in is explored whether or not its voxel mesh turned
    // out to be empty — and putting this after the early-out is a smaller
    // version of the bug it fixes.
    if (field != nullptr) {
        map_.note_chunk(*field, surface);
    }
    const TerrainMeshData data = build_voxel_terrain_mesh(mesh, surface);
    if (data.vertices.empty()) {
        return; // solid or empty chunk
    }
    const platform::MeshHandle handle = renderer.create_mesh(data.vertices, data.indices);
    if (!handle.valid()) {
        ++uploads_.failed;
        report_upload_failure("voxel chunk");
        return;
    }
    ++uploads_.voxel;
    // GROUND TUFTS: harvested off the mesh WE JUST BUILT, which is the ground
    // the player sees. Doing it here rather than from the height field is the
    // whole reason the tufts sit on the surface instead of hovering over it or
    // sinking into it (GroundTufts.h explains the 0.10-0.15 m the two surfaces
    // still disagree by). Cheap and once per chunk.
    if (!tufts_off_) {
        std::vector<TuftSpot> spots = harvest_tuft_spots(mesh, tuft_params());
        if (spots.empty()) {
            tuft_spots_.erase(mesh.chunk_coord);
        } else {
            tuft_spots_[mesh.chunk_coord] = std::move(spots);
        }
        tuft_built_ = false; // the world under the eye changed; regrow
    }
    // Same key as the heightfield upload: whichever source ran last owns the
    // chunk, so switching the ferry over never draws both.
    const TerrainRes res{handle.id, bounds_of(data.vertices)};
    const auto it = terrain_meshes_.find(mesh.chunk_coord);
    if (it != terrain_meshes_.end()) {
        renderer.destroy_mesh(platform::MeshHandle{it->second.mesh_id});
        it->second = res;
    } else {
        terrain_meshes_.emplace(mesh.chunk_coord, res);
    }
}

// THE TUFT SETTINGS, AND EVERY ONE OF THEM IS DERIVED FROM A ROW THAT ALREADY
// EXISTS (Rule 14). Nothing here is a taste value:
//
//  * density = GRASS_DENSITY_MIN. The approved range is 0.5-1.5 /m² and the
//    user asked for «не много», so the layer sits on the FLOOR of the range
//    rather than inventing a number under it.
//  * height  = GRASS_HEIGHT_MAX, the §2.3 cap that keeps grass from hiding an
//    interactable.
//  * slope   = SLOPE_GRASS_MAX, the same threshold the splat uses, so a tuft
//    can never stand on ground the shader is drawing as rock.
//  * VIEW DISTANCE = RULE 33, COMPUTED, and it is the interesting one. A
//    silhouette needs SILHOUETTE_MIN_PX to be an object; at INTERNAL_RES_H
//    over CAMERA_FOV_Y that is a fixed angular size, so an object of height h
//    stops being one at h * (focal px / min px). For a 0.4 m tuft on a
//    640x360 frame that lands near 12 m. Past it a tuft is not small — it is
//    ABSENT, and drawing it there buys nothing while manufacturing the running
//    shimmer this project has already fought twice. Tufts cannot fix the
//    middle distance; their band is the near metres, where the ground is bare.
GroundTuftParams RenderSystem::tuft_params() {
    GroundTuftParams p;
    p.density_per_m2 = static_cast<float>(config::GRASS_DENSITY_MIN);
    p.height_max_m = static_cast<float>(config::GRASS_HEIGHT_MAX);
    p.slope_max_rad = static_cast<float>(config::SLOPE_GRASS_MAX);
    const float focal_px = 0.5f * static_cast<float>(config::INTERNAL_RES_H)
                         / std::tan(0.5f * static_cast<float>(config::CAMERA_FOV_Y));
    p.view_distance_m = p.height_max_m * focal_px
                      / static_cast<float>(config::SILHOUETTE_MIN_PX);
    p.seed = 0x67C5u;
    return p;
}

// How far the eye may walk before the tufts are regrown. Half the smallest
// clump is invisible as a pop and keeps the rebuild off the per-frame path;
// the layer is deterministic, so a regrown tuft lands exactly where the old
// one stood and only the SET changes at the rim of the view distance.
constexpr float TUFT_REBUILD_STEP_M = 2.0f;

namespace {

/// Horizontal centre of a chunk, in world metres. Kept for the record a chunk
/// carries; the BANDING does not use it — see chunk_distance_xz.
[[nodiscard]] glm::vec2 chunk_center_xz(glm::ivec2 chunk_coord) {
    const auto size = static_cast<float>(config::CHUNK_SIZE);
    return {(static_cast<float>(chunk_coord.x) + 0.5f) * size,
            (static_cast<float>(chunk_coord.y) + 0.5f) * size};
}

/// Distance from the eye to the NEAREST POINT of a chunk's footprint (0 when
/// standing on it), horizontal only.
///
/// THE NEAREST POINT AND NOT THE CENTRE, and the difference is not a detail:
/// CHUNK_SIZE is 256 m, so a chunk's centre can be 181 m away while its near
/// edge is under the player's feet. Banding on the centre would bake a tree
/// five metres in front of the player as a silhouette because the rest of its
/// chunk is far — the exact defect this whole pass exists to avoid, wearing a
/// performance win's clothes. Anchoring on the nearest point makes the level a
/// chunk is baked at never coarser than its CLOSEST tree can afford.
///
/// The cost of that honesty is coarseness: one level per 256 m chunk means a
/// chunk keeps full detail because of its nearest corner while its far corner
/// pays for it. Finer granularity (per micro tile, 64 m) is the next step and
/// is deliberately not taken here — it multiplies draw calls, and draw calls
/// are the other half of the frame's budget.
[[nodiscard]] float chunk_distance_xz(glm::vec2 eye_xz, glm::ivec2 chunk_coord) {
    const auto size = static_cast<float>(config::CHUNK_SIZE);
    const glm::vec2 lo{static_cast<float>(chunk_coord.x) * size,
                       static_cast<float>(chunk_coord.y) * size};
    const glm::vec2 hi = lo + glm::vec2{size, size};
    const glm::vec2 nearest{std::clamp(eye_xz.x, lo.x, hi.x),
                            std::clamp(eye_xz.y, lo.y, hi.y)};
    return glm::distance(eye_xz, nearest);
}

} // namespace

void RenderSystem::refresh_scatter_lod(platform::IRenderer& renderer, glm::vec3 eye) {
    scatter_eye_ = {eye.x, eye.z};
    if (scatter_off_ || flora_lod_forced()) {
        return; // the force door is a CONTROL arm: banding must not run under it
    }
    // Pick the NEAREST chunk whose baked level disagrees with its distance, and
    // re-bake that one. Nearest first because the mismatch that matters is the
    // one in front of the player; one per frame because a re-bake costs a bake.
    const glm::ivec2* worst = nullptr;
    float worst_distance = 0.0f;
    FloraLod worst_lod = FloraLod::Full;
    for (const auto& [coord, res] : scatter_meshes_) {
        if (res.instances.empty()) {
            continue;
        }
        const float d = chunk_distance_xz(scatter_eye_, coord);
        const FloraLod want = flora_lod_for_distance(d, res.lod);
        if (want == res.lod) {
            continue;
        }
        if (worst == nullptr || d < worst_distance) {
            worst = &coord;
            worst_distance = d;
            worst_lod = want;
        }
    }
    if (worst == nullptr) {
        return;
    }
    // The instances have to be COPIED out before the re-bake: bake_scatter
    // drops the chunk's entry first, and the span would then point into a
    // destroyed vector. This is the one place the kept instances are paid for.
    const glm::ivec2 coord = *worst;
    std::vector<math::ScatterInstance> instances = scatter_meshes_.at(coord).instances;
    bake_scatter(renderer, coord, instances, worst_lod);
}

void RenderSystem::refresh_ground_tufts(platform::IRenderer& renderer, glm::vec3 eye) {
    if (tufts_off_) {
        return;
    }
    if (tuft_built_ && glm::distance(eye, tuft_built_at_) < TUFT_REBUILD_STEP_M) {
        return;
    }
    const GroundTuftParams params = tuft_params();
    MeshData grown;
    for (const auto& [coord, spots] : tuft_spots_) {
        MeshData part = build_ground_tufts(spots, eye, params);
        if (part.vertices.empty()) {
            continue;
        }
        const auto base = static_cast<uint32_t>(grown.vertices.size());
        grown.vertices.insert(grown.vertices.end(), part.vertices.begin(),
                              part.vertices.end());
        for (uint32_t i : part.indices) {
            grown.indices.push_back(base + i);
        }
    }
    // The old mesh is destroyed AFTER the new one is built but BEFORE it is
    // uploaded, so the handle budget never holds two full sets at once.
    if (tuft_mesh_id_ != 0) {
        renderer.destroy_mesh(platform::MeshHandle{tuft_mesh_id_});
        tuft_mesh_id_ = 0;
    }
    tuft_built_ = true;
    tuft_built_at_ = eye;
    if (grown.vertices.empty()) {
        return;
    }
    const platform::MeshHandle handle =
        renderer.create_mesh(grown.vertices, grown.indices);
    if (!handle.valid()) {
        ++uploads_.failed;
        report_upload_failure("ground tufts");
        return;
    }
    tuft_mesh_id_ = handle.id;
}

void RenderSystem::drop_terrain(platform::IRenderer& renderer, glm::ivec2 chunk_coord) {
    tuft_spots_.erase(chunk_coord);
    tuft_built_ = false;
    const auto it = terrain_meshes_.find(chunk_coord);
    if (it != terrain_meshes_.end()) {
        renderer.destroy_mesh(platform::MeshHandle{it->second.mesh_id});
        terrain_meshes_.erase(it);
    }
}

void RenderSystem::upload_scatter(platform::IRenderer& renderer,
                                  glm::ivec2 chunk_coord,
                                  std::span<const math::ScatterInstance> instances) {
    drop_scatter(renderer, chunk_coord); // idempotent per coord
    // DFN_NO_SCATTER=1: verification hook (Rule 27), never a shipping path.
    // It exists because A LANDMARK'S SILHOUETTE CANNOT BE JUDGED WITHOUT IT.
    // Core measured the conservative canopy envelope owning 54-79 % of the
    // skyline at the crag's acceptance distances, which means every shape
    // verdict taken so far may have been a verdict on a pine stand. Shooting
    // the same vantage twice, trees on and trees off, separates "the mountain
    // is a dome" from "the mountain is behind a forest" — and those two have
    // completely different owners.
    if (scatter_off_ || instances.empty()) {
        return;
    }
    // The level a chunk is BORN at is the level its distance already calls for.
    // Baking every new chunk at Full and letting the pass below walk it down
    // would pay the most expensive bake for ground the player is nowhere near —
    // and pay it exactly during streaming, when the frame is already loaded.
    bake_scatter(renderer, chunk_coord, instances,
                 flora_lod_for_distance(chunk_distance_xz(scatter_eye_, chunk_coord),
                                        FloraLod::Full));
}

void RenderSystem::bake_scatter(platform::IRenderer& renderer, glm::ivec2 chunk_coord,
                                std::span<const math::ScatterInstance> instances,
                                FloraLod lod) {
    drop_scatter(renderer, chunk_coord); // idempotent per coord; also the re-bake path
    const auto chunk_size = static_cast<float>(config::CHUNK_SIZE);
    const glm::vec2 origin{static_cast<float>(chunk_coord.x) * chunk_size,
                           static_cast<float>(chunk_coord.y) * chunk_size};
    ScatterBatches batches = build_scatter_batches(instances, origin, chunk_size,
                                                   /*micro_tiles_per_axis=*/4, lod);

    ChunkScatterRes res;
    res.lod = lod;
    res.center_xz = chunk_center_xz(chunk_coord);
    res.instances.assign(instances.begin(), instances.end());
    res.bounds.expand(bounds_of(batches.trees.vertices));
    res.bounds.expand(bounds_of(batches.foliage.vertices));
    ++uploads_.scatter_chunks;
    const auto upload_batch = [&](const MeshData& mesh) -> uint32_t {
        const platform::MeshHandle handle =
            renderer.create_mesh(mesh.vertices, mesh.indices);
        if (!handle.valid()) {
            ++uploads_.failed;
            report_upload_failure("scatter batch");
            return 0;
        }
        ++uploads_.scatter_meshes;
        return handle.id;
    };
    if (!batches.trees.vertices.empty()) {
        res.trees_mesh_id = upload_batch(batches.trees);
    }
    if (!batches.foliage.vertices.empty()) {
        res.foliage_mesh_id = upload_batch(batches.foliage);
    }
    for (const MicroTile& tile : batches.micro) {
        const uint32_t id = upload_batch(tile.mesh);
        if (id != 0) {
            res.micro.push_back({tile.center_xz, tile.radius_m, id});
        }
    }
    if (res.trees_mesh_id != 0 || res.foliage_mesh_id != 0 || !res.micro.empty()) {
        scatter_meshes_.emplace(chunk_coord, std::move(res));
    }
}

void RenderSystem::upload_prebuilt_scatter(platform::IRenderer& renderer,
                                           glm::ivec2 chunk_coord,
                                           const MeshData& trees,
                                           const MeshData& foliage) {
    drop_scatter(renderer, chunk_coord);
    ChunkScatterRes res;
    res.bounds.expand(bounds_of(trees.vertices));
    res.bounds.expand(bounds_of(foliage.vertices));
    ++uploads_.scatter_chunks;
    const auto upload_batch = [&](const MeshData& mesh) -> uint32_t {
        const platform::MeshHandle handle = renderer.create_mesh(mesh.vertices, mesh.indices);
        if (!handle.valid()) {
            ++uploads_.failed;
            report_upload_failure("prebuilt scatter batch");
            return 0;
        }
        ++uploads_.scatter_meshes;
        return handle.id;
    };
    if (!trees.vertices.empty()) {
        res.trees_mesh_id = upload_batch(trees);
    }
    if (!foliage.vertices.empty()) {
        res.foliage_mesh_id = upload_batch(foliage);
    }
    if (res.trees_mesh_id != 0 || res.foliage_mesh_id != 0) {
        scatter_meshes_.emplace(chunk_coord, std::move(res));
    }
}

void RenderSystem::drop_scatter(platform::IRenderer& renderer, glm::ivec2 chunk_coord) {
    const auto it = scatter_meshes_.find(chunk_coord);
    if (it == scatter_meshes_.end()) {
        return;
    }
    if (it->second.trees_mesh_id != 0) {
        renderer.destroy_mesh(platform::MeshHandle{it->second.trees_mesh_id});
    }
    if (it->second.foliage_mesh_id != 0) {
        renderer.destroy_mesh(platform::MeshHandle{it->second.foliage_mesh_id});
    }
    for (const MicroTileRes& tile : it->second.micro) {
        renderer.destroy_mesh(platform::MeshHandle{tile.mesh_id});
    }
    scatter_meshes_.erase(it);
}

} // namespace dfn::render
