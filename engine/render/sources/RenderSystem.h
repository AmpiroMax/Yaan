/*
Created: 09:08:2026 - 00:16:00
Last updated: 28:08:2026 - 19:20:00
Module: engine/render
File: engine/render/sources/RenderSystem.h

Responsibility:
- The render facade: walks the ECS view of renderable entities and submits them
  to IRenderer with interpolated transforms (Rule 12). Owns the mapping from
  engine-level asset ids to renderer handles.

Key items:
- RenderSystem: render(world, renderer, camera, alpha), upload_terrain /
  drop_terrain (HeightFieldView -> terrain mesh, boundary agreed with core).

Dependencies:
- Uses: engine/core (ecs World, math HeightFieldView), engine/platform/render
  (IRenderer). Never bgfx (Rule 1).
- Used by: engine/app (main loop), engine/editor.

Notes:
- Rule 9: platform interfaces arrive as parameters and are never stored. The
  cached state here is resource bookkeeping (asset id -> MeshHandle/Texture-
  Handle), not game state; the ECS World stays the single source of truth
  (Rule 10).
- Renderable entities carry shared components (lead-owned, agreed stage 1):
  Transform + PreviousTransform (written by sim each fixed step), RenderMesh
  { mesh_asset, texture_asset : uint32 name hashes }, LocalBounds (model-space
  AABB). This header only forward-declares World; component includes appear in
  the stage-2 .cpp once engine/core/components exists.
- Terrain: core's ChunkManager emits ChunkLoaded/ChunkUnloaded; the app layer
  subscribes and calls upload_terrain/drop_terrain here (render cannot include
  world per the DAG — wiring agreed with core, confirmed at the sync).
- Culling (stage 2+): frustum vs LocalBounds transformed to world space; frustum
  math lives in engine/core/math.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Public contract, frozen for the stage (Rule 26): changes only via group sync.
- Keep submission logic here; never let gameplay call IRenderer directly.
*/
/*
UPD:
- 09:08:2026 - 00:16:00: Initial stage-1 contract (render zone).
- 09:08:2026 - 11:02:00: Stage 3 — procedural terrain atlas + water texture
  (ProcTexture, registry-assigned dense asset ids), RenderEnvironment per
  frame (Materials.h defaults, environment() accessor for tuning), water
  plane capability (set_water/clear_water + DFN_WATER=<height_m> debug env),
  render-side visual clock for water animation.
- 09:08:2026 - 11:57:20: Stage 3b (lead-approved batch): upload_terrain
  overload with SurfaceFieldView (surface-truth splat), upload_scatter/
  drop_scatter (batched P5 scatter with GRASS_VIEW_DISTANCE micro culling),
  set_water_bodies/clear_water_bodies (lake planes + river ribbons),
  placeholder site meshes registered under the blessed RenderMesh ids 1..7,
  ECS submissions moved to the lit+fogged "prop" program.
- 09:08:2026 - 17:33:00: Map screen (user request "миникарта как в скайриме"):
  toggle_map/set_map_open/map_open + set_internal_resolution, a MapScreen
  member fed by upload_terrain (explored chunks) and the ECS pass (site
  markers), and the generic draw_overlay path that blits a PixelCanvas over
  the frame as an unlit screen-filling quad (no IRenderer change, Rule 26).
- 09:08:2026 - 19:42:00: upload_terrain_voxel — render draws core's voxel
  surface, so carved interiors (tunnel, barrows) exist on screen at all.
- 09:08:2026 - 19:52:00: DFN_TIME now FREEZES the sky each frame (the app's
  clock would otherwise overwrite the screenshot hook every frame).
- 09:08:2026 - 20:21:13: FOLIAGE PASS: the "foliage" program, the procedural
  leaf mask atlas (flora's generate_leaf_atlas, uploaded like the terrain
  atlas) and a second per-chunk scatter mesh for the alpha-cutout leaf cards.
  EDITED BY THE FLORA AGENT under an explicit lead-granted Rule 25 exception
  while render's zone was unowned; wiring only — no shader, material, wind or
  backend change. Not a precedent. (Reviewed and KEPT by render, 20:25.)
- 09:08:2026 - 20:44:00: INTERIOR LIGHTING: collect_point_lights walks
  CarriedLight + Transform into the frame's point-light array (hand offset
  rotated by the carrier's interpolated rotation, first two lights flagged for
  cube shadows), plus the DFN_TORCH / DFN_DARK verification hooks.
- 09:08:2026 - 21:14:00: Frustum culling: per-chunk world bounds measured at
  upload (TerrainRes / ChunkScatterRes::bounds) and visible_or_casting, which
  keeps off-screen shadow casters alive.
- 09:08:2026 - 22:12:57: TERRAIN LOD, the drawing half. lod_* forwarding calls
  over a LodTerrain member: the app sets the world bounds and the streamed
  rectangle, ferries lod_to_load()/lod_to_release() to core's coarse-node
  calls, and hands meshes back through upload_lod_node. Nodes draw after the
  chunk terrain with DrawParams::fade carrying the cross-fade. Also: the
  resource/screen half of this class moved to RenderSystemResources.cpp, which
  is the same class split for the 800-line limit (Rule 21).
- 09:08:2026 - 22:33:00: DFN_NO_SCATTER verification hook (scatter_off_).
- 09:08:2026 - 22:36:47: warned_missing_meshes_ — the once-per-id missing-asset warning.
- 09:08:2026 - 22:39:28: lod_pending() — the accessor the app ferry retries against.
- 09:08:2026 - 22:44:28: upload_terrain_voxel takes the heightfield for the MAP
  (defaulted, source-compatible). Regression fix: the map recorded nothing at
  all once the ferry moved to the voxel path.
- 09:08:2026 - 23:50:06: hud()/set_hud_visible (the caller draws the interaction
  prompt; render owns the surface and the blit, never the string — Rule 5),
  overlay_program_, WaterBucket (bodies merged per CHUNK_SIZE cell with an AABB
  for culling), upload counters.
- 10:08:2026 - 02:30:08: register_mesh — caller-authored geometry enters the
  asset registry (character zone's body segments 34..49 via the app ferry;
  render cannot include engine/anim). Additive; refusals are loud.
- 10:08:2026 - 03:08:00: CLOUDS (W4): render() now drives apply_wind (no live
  call site existed anywhere — wind_strength sat at 0.0 and read as calm) and
  apply_clouds (the one coverage-field drift) each frame; DFN_CLOUD pins
  cover (0 = the pass's control), DFN_VISTIME pins the visual clock for the
  drift acceptance pair.
- 10:08:2026 - 20:15:40: lod_selected_count / lod_resident_count /
  lod_draw_count. A readout built on lod_pending() reads a HEALTHY ring as
  zero, because pending() is the awaiting-upload list; two zones read that
  zero as "the far-detail ring never populates" on the same day.
- 12:08:2026 - 00:52:40: GROUND TUFTS — the sparse near-field grass layer
  (GroundTufts.h). Spots are harvested once per chunk off the DRAWN voxel mesh
  in upload_terrain_voxel; the geometry is ONE eye-local mesh regrown only when
  the eye has walked TUFT_REBUILD_STEP_M, because at the Rule 33 view distance
  only a couple of hundred clumps are ever visible and baking a whole chunk of
  blades would spend tens of megabytes to draw a hundredth of them. Every
  setting is derived from an approved row (tuft_params(), Rule 14).
  DFN_NO_TUFTS=1 is the counterfactual arm.
- 13:08:2026 - 16:45:00: DFN_ENV_LOG (env_log_) — покадровый лог потреблённого окружения; см. .cpp.
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
- 13:08:2026 - 20:37:12: set_visual_time — THE SKY HAD TWO CLOCKS and only one of them was fixed.
  The sun and moon run off the app's game clock, which a tour advances by a fixed
  step per frame; the cloud drift and the wind envelope ran off a steady_clock
  read in render(). Measured, two runs of ONE binary on ONE recipe (sky probe):
  67.466 % of pixels differed with that clock free and 0.000 %, byte for byte,
  with it pinned. The defect is as old as the field; it SURFACED now because the
  sky gained enough structure for a few metres of drift to move a majority of it,
  which is worth stating plainly so nobody concludes that cloud volume broke the
  acceptance method. Additive and latched: until a caller tells the time, the
  wall-clock path is bit-identical to what shipped.
- 14:08:2026 - 19:34:00: ChunkScatterRes помнит свой уровень детализации и экземпляры, из которых испечён (рез ведущего). Экземпляры держим потому, что перепечка на смене полосы должна откуда-то взяться, а породившие их данные живут в engine/world — зоне, в которую render не может дотянуться назад; 24 байта на экземпляр против мегабайтов вершин, которые они заменяют.
- 14:08:2026 - 23:36:19: upload_prebuilt_scatter() — путь галереи реестра: приложение собирает
  потоки из .dfo и отдаёт сюда, чтобы объект реестра ехал ОБЫЧНОЙ отрисовкой
  скаттера (те же программы, атлас, ветер). Экземпляры не хранятся — банда
  детализации пропускает такой чанк по построению: уровни объекта реестра —
  дело КУЗНИЦЫ, запечённое в реестр, а не пересборка в кадре (в1).
- 15:08:2026 - 16:10:00: leaf_normal_asset_ — лист нормалей коры зоны flora, отдаётся дро листвы
  через DrawParams::aux_texture.
- 17:08:2026 - 10:53:33: ExtraLight + set_scene_lights/set_transient_lights — лампы карты и
  однокадровые огни (рой) идут в ТОТ ЖЕ бюджет и ТОТ ЖЕ отбор, что факелы.
  Второй отбор был бы вторым ответом на вопрос «какие восемь горят», и два
  ответа разошлись бы ровно тогда, когда игрок стоит между ними.
- 17:08:2026 - 10:56:32: у ExtraLight и у кандидата появилось СОБСТВЕННОЕ желание тени. Два
  теневых слота раздавались ДВУМ БЛИЖАЙШИМ независимо от того, просили они
  тень или нет.
- 17:08:2026 - 11:13:47: set_firefly_mesh / set_emissive_mesh — две порции геометрии, которая
  рисуется БЕЗ ОСВЕЩЕНИЯ: рой (перезаливается каждый кадр) и самосветящееся
  добро карты (пламя, стекло — заливается раз на карту). Мушка и пламя САМИ
  есть свет; затенять их ночью значит гасить ровно то, ради чего они есть.
- 17:08:2026 - 18:41:51: set_ghost_mesh — предпросмотр детали в руке строителя, каждый кадр, без света.
- 18:08:2026 - 12:06:07: только текст, поведение не тронуто: «129 отсчётов» у
  upload_lod_node названо своим именем — COARSE_NODE_RESOLUTION, решётка УЗЛА.
  HEIGHTMAP_RESOLUTION уехал 129 -> 257 (NUMBERS), и раньше два числа
  совпадали; голое «129» теперь читается как устаревший размер чанка, а не как
  живая константа узла, и правка ради этого совпадения сломала бы шов с core.
- 18:08:2026 - 21:12:40: set_house_mesh — слот постройки редактора: рисуется ОСВЕЩЁННОЙ (prop), заливается по изменению, а не по кадру.
- 18:08:2026 - 22:26:40: Слот постройки — два потока (каркас и полотна): материал сегодня цвет вершин, «prop» текстуру не читает.
- 19:08:2026 - 02:34:20: Поля плиток постройки; комментарий слота обновлён.
- 19:08:2026 - 04:05:50: HouseStream (поток на материал) и HouseDoor (петля + демонстрационный ход); плитка материала — лениво с кэшем.
- 19:08:2026 - 04:42:30: house_tile_asset(normal=true) — лист нормалей материала; потоки и двери несут его aux-текстурой.
- 20:08:2026 - 02:05:51: flame_phase_for/flame_at и house_tile_key вынесены в заголовок (чистые
  функции, у обеих появился прибор); листы набора кэшируются полями вместо печати
  9.4 МБ на каждый промах кэша плитки.
- 20:08:2026 - 18:40:00: HouseDoor.demo_swing — качается только выбранная в сессии дверь.
- 22:08:2026 - 15:40:00: terrain_normal_asset_ — лист нормалей земли для террейн-дро
  (aux_texture, как кора у листвы).
- 22:08:2026 - 16:20:00: set_air_override()/clear_air_override() — туман композиции,
  применяется внутри render() после всех перезаписей неба.
- 22:08:2026 - 18:40:00: HouseStreamGpu.bounds — мировой габарит потока построек для
  отсечения (профиль 22.08: 1.91 млн треугольников в кадре независимо от
  взгляда — пятно меша было размером с карту).
- 23:08:2026 - 23:10:00: set_interior_mesh + set_world_suspended — второй слот отрисовки и
  подвес экстерьера (И15 волна А). Город не выгружается: в подвесе кадр рисует
  интерьерный слот вместо городского, а всё остальное экстерьерное
  пропускается по условию СВОЕГО цикла. Дозой 0 (подвес выключен) кадр
  бит-в-бит прежний — гейт добавляет одно `false ||` на цикл.
- 22:08:2026 - 20:10:00: set_air_override принимает стартовую облачность карты — пишется
  один раз (расписание погоды и DFN_CLOUD выигрывают).
- 22:08:2026 - 21:00:00: interior у ExtraLight/PointLightCandidate — флаг доезжает до PointLight.
- 23:08:2026 - 00:30:00: set_path_classes — поле классов полотна от композиции; действует на
  чанки, воксельную землю и LOD одной упаковкой.
- 23:08:2026 - 01:40:00: коробка комнаты у ExtraLight/кандидата.
- 23:08:2026 - 07:20:00: set_ground_exclusions — пятна построек для травы; сброс tuft_built_
- 22:08:2026 - 22:52:34: softness у ExtraLight/PointLightCandidate — мягкость источника доезжает от сцены до PointLight.
- 22:08:2026 - 23:14:42: set_grass_lushness/grass_lushness_ — пышность травы за дверью DFN_GRASS_LUSH (0 = прежняя бит-в-бит).
- 22:08:2026 - 23:49:20: set_path_mask — маска троп мира (износ+класс во фрагмент, круг 5 «шевроны»); ленивая заливка текстуры на кадре.
- 23:08:2026 - 01:17:49: flicker у ExtraLight/PointLightCandidate — мерцание доезжает до модуляции цвета.
- 23:08:2026 - 18:11:16: emissive у HouseStream/HouseStreamGpu — самосветные потоки построек.
- 27:08:2026 - 14:30:00: HouseDoor.open_angle + set_house_door_open(index, radians) —
  обещанное шапкой HouseDoor «игровое открыто/закрыто как состояние симуляции».
  Угол называет зона app (тип двери, нажатие); render его рисует и НЕ помнит
  между заливками. Заливка сохраняет НОМЕР створки — пустая занимает своё
  место с mesh_id 0, иначе номер, по которому app открывает конкретное
  полотно, поехал бы, и игрок жал бы одну дверь, а открывалась соседняя.
- 27:08:2026 - 12:58:00: ScreenProp + set_screen_prop + overlay_depth_m — ОДИН
  ПРЕДМЕТ ПОВЕРХ ХОЛСТА ЭКРАНА, по заказу владельца 27.08 про объёмный герб
  главного меню. Слот, а не путь: у экрана меню нет ни мира, ни сущностей, и
  нужен ровно один меш, посланный ПОСЛЕ наложения холста (вид сцены у
  бэкенда Sequential, поэтому «после» и значит «поверх»). Двух отвергнутых
  вариантов не осталось в коде, поэтому они здесь: (1) послать ДО холста —
  страницы меню чистят его непрозрачным чёрным, и меш не был бы виден ни
  одним пикселем; (2) дыра в холсте — за ней обязано быть гарантированно
  пусто, а выход в главное меню ИЗ ИГРЫ мир не выгружает. overlay_depth_m
  вынесена из draw_overlay, потому что читателей стало двое: сам холст и
  зона app, которой надо поставить герб перед ним (правило 39).
  О МЕТКЕ: «Last updated» у трёх файлов зоны стояла на 14:30, то есть позже
  моих стенных часов (12:58). Чужую запись не правлю и под неё не
  подстраиваюсь — поле переставлено на честное время, и потому поехало
  НАЗАД; порядок в блоке из-за этого не хронологический.
- 28:08:2026 - 11:23:00: pane_glow у HouseStream/HouseStreamGpu — СИЛА свечения
  оконной вставки (свет из окна, 28.08). Величина, а не признак: окно светит
  небом, и полдень с полночью у него разные. Ноль — обычный поток, кадр
  бит-в-бит; едет в DrawParams::aux1. Только добавление.
  пересевает уже выращенное.
- 28:08:2026 - 12:49:39: ScreenProp::material — из чего сделан предмет экрана (зона
  МАТЕРИАЛЫ, 28.08). MATERIAL_NONE — прежний ламберт, кадр бит-в-бит.
- 28:08:2026 - 14:18:16: LooseProp / set_loose_props / set_loose_prop_transform —
  ТРЕТИЙ СЛОТ ПОСТРОЕЧНОЙ ГЕОМЕТРИИ: подвижный предмет со своей матрицей (зона
  ФИЗИКА ПРЕДМЕТОВ, заказ владельца 28.08 «поднимать объекты, держать,
  складывать»). Не поток построек, потому что поток — это запечённые в МИР
  вершины, а кувшин в руках переезжает шестьдесят раз в секунду; и не россыпь
  сцены, потому что россыпь рисует без материала плоским вершинным цветом
  (тот же довод, по которому 27.08 кровать уехала в потоки). Геометрия
  местная, место называет матрица, и приносит её физика — значит нарисованное
  и осязаемое не могут разъехаться по построению. Отсечение — ШАРОМ вокруг
  середины: предмет вертится, и ящик по осям мира верен ровно один кадр.
- 28:08:2026 - 17:31:25: HouseStream.material / HouseStreamGpu.material и
  program_for() (волна 3 зоны МАТЕРИАЛЫ). Здесь соединились два из четырёх словарей вещества,
  названных инвентаризацией: БЕЗЫМЯННЫЙ материал HouseStreamGpu (четыре
  поля, «уже готовый материал, у которого отняли имя») и одиннадцать ИМЁН
  PartMaterial, до которых из данных было не дотянуться. Плюс выбор
  программы переехал с ПОТОКА на ВЕЩЕСТВО. ScreenProp::material сменил тип
  на core::MaterialId — реестр уехал в engine/core, потому что render не
  виден ни world, ни платформе.
- 28:08:2026 - 19:20:00: set_parts_sheet_dose()/parts_sheet_dose() — доза
  структуры листа (дверь DFN_MAT_SHEET, волна 4 зоны МАТЕРИАЛЫ). Дверь ЧИТАЕТ
  ПРИЛОЖЕНИЕ и приносит ответ сюда: набор дверей требует, чтобы всякую строку
  таблицы читал файл engine/app, иначе это обещание, против которого можно
  написать рецепт, который никогда не откроется (именованное исключение там
  одно и занято DFN_TOUR). Спрашивают дозу в двух местах — плитка стены и свотч
  редактора, — и ответ обязан быть один (правило 39).
*/

#pragma once

#include "engine/core/math/sources/Aabb.h"
#include "engine/core/math/sources/Frustum.h"
#include "engine/core/math/sources/SurfaceField.h"
#include "engine/core/math/sources/VoxelField.h"
#include "engine/platform/render/interfaces/IRenderer.h"
#include "engine/render/sources/FirstPersonCamera.h"
#include "engine/render/sources/GroundTufts.h"
#include "engine/render/sources/LodTerrain.h"
#include "engine/render/sources/MapScreen.h"
#include "engine/core/materials/sources/MaterialRegistry.h" // реестр веществ (28.08)
#include "engine/render/sources/PartsAtlas.h" // лист набора: тон, ревизия, размер плитки
#include "engine/render/sources/ScatterBatcher.h" // FloraLod, per-chunk bake level

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <glm/vec2.hpp>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dfn::ecs {
class World; // engine/core/ecs/sources/World.h (core zone)
}
namespace dfn::math {
struct HeightFieldView; // engine/core/math/sources/HeightField.h (core zone)
}

namespace dfn::render {

/// РАЗБРОС ФАЗ ПЛАМЕНИ ПО СВЕТИЛЬНИКАМ, В СЕКУНДАХ. Осциллятор ниже бьётся с
/// периодом 1.85 с (две составляющие, 0.91 и 1.45 Гц), поэтому сдвиг,
/// равномерно рассыпанный ровно по этому отрезку, покрывает цикл один раз: два
/// факела в коридоре дышат вразнобой, и ни одна пара не совпадает по
/// построению. Look-dev, в очереди на переезд в NUMBERS.md вместе с TORCH_COLOR.
inline constexpr float FLAME_PHASE_SPREAD_S = 1.85f;

/// ФАЗА МЕРЦАНИЯ ОДНОГО СВЕТИЛЬНИКА, посеянная от НЕГО САМОГО.
///
/// До 20.08 сюда приходил РАЗМЕР ПУЛА кандидатов на момент сбора, то есть
/// число, которое меняется каждый кадр от светляков и от того, сколько ламп
/// композиции попало в кадр. Фаза факела прыгала на ровном месте — это и была
/// открытая жалоба «свет мигает» в её второй половине (первая, растворение на
/// границе бюджета, чинилась отдельно и осталась).
///
/// Ключ обязан быть СТАБИЛЕН ВО ВРЕМЕНИ, а не только уникален: позиция для
/// НОСИМОГО факела меняется каждый кадр, поэтому сеять от координат нельзя —
/// это был бы тот же дефект с другим входом. Сеем от EntityId.packed().
[[nodiscard]] constexpr float flame_phase_for(std::uint64_t light_key) {
    // splitmix64: один проход даёт равномерные старшие биты для соседних
    // ключей (у сущностей ЕДИНИЦА разницы — самый частый случай).
    std::uint64_t h = light_key + 0x9E3779B97F4A7C15ull;
    h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ull;
    h = (h ^ (h >> 27)) * 0x94D049BB133111EBull;
    h ^= h >> 31;
    return static_cast<float>(h >> 40) * (1.0f / 16777216.0f) * FLAME_PHASE_SPREAD_S;
}

/// Одно дыхание пламени: множитель яркости около единицы и увод оттенка.
struct Flame {
    float intensity;
    float warmth; ///< >0 — к угольку, <0 — к бледному кончику
};

/// ДВА НЕСОИЗМЕРИМЫХ КОЛЕБАНИЯ, поэтому рисунок не повторяется. СТАВКИ НЕ В
/// ГЕРЦАХ, и эта строка когда-то утверждала обратное: `5.7f * 6.2831853f *
/// 0.1591549f` умножает на тау и на 1/тау, которые СОКРАЩАЮТСЯ, — значит
/// аргумент это 5.7 рад/с, пламя дышит на 0.91 и 1.45 Гц с биением 1.85 с, а не
/// на 5.7/9.1 Гц с биением 0.29 с. Найдено прибором, предсказавшим пару
/// отсчётов по написанным здесь числам и намерившим вдвое меньше
/// (RenderSystemTests, случай дыхания). ОСТАВЛЕНО КАК ИЗМЕРЕНО, а не
/// «исправлено»: с какой частотой должен трепетать факел — решение look-dev,
/// у которого есть хозяин.
///
/// Ведётся ВИЗУАЛЬНЫМИ часами (env.time_seconds), которые уже пришпиливают
/// DFN_VISTIME и DFN_WIND_FREEZE, — поэтому все рецепты съёмки проекта
/// остаются детерминированными.
///
/// АМПЛИТУДЫ (FLAME_*_SWING) живут в SkyModel.h рядом с TORCH_COLOR: с дышащим
/// пламенем цвет факела это ПОЛОСА, и каждый, кто её утверждает, обязан читать
/// ширину оттуда же, откуда её читает осциллятор.
[[nodiscard]] Flame flame_at(float t, float phase_s);

/// КЛЮЧ ПЛИТКИ НАБОРА: материал, тон, лист (цвет/нормали) и РЕВИЗИЯ ЛИСТА.
///
/// Ревизия здесь потому, что PartsAtlas.h требует этого прямым текстом: флора
/// потеряла заход на кэшированном листе в 4 колонки, который сэмплили uv на 5
/// колонок (белые хвойные при верном коде везде). Ключ без ревизии переживает
/// перепечатку листа и отдаёт вчерашние пиксели.
///
/// Упаковка ТЕСНАЯ И БЕЗ ЗАЗОРОВ: прежняя была `surface*16 + tone +
/// (normal?0x100)`, то есть закладывала 16 тонов там, где их четыре, и роняла
/// признак листа на фиксированный бит. Сегодня она не сталкивается — потому
/// что 9*16+3 < 0x100, а не потому, что так задумано; ряд тонов, доросший до
/// шестнадцатого, столкнул бы (0, 16) с (1, 0) молча.
[[nodiscard]] constexpr std::uint32_t house_tile_key(std::uint32_t surface,
                                                     std::uint32_t tone, bool normal,
                                                     std::uint32_t revision) {
    return (revision << 16) | (((surface * PARTS_ATLAS_TONES) + tone) << 1)
           | (normal ? 1u : 0u);
}


class RenderSystem {
public:
    // Prepares shared programs ("terrain", "unlit") via IRenderer::load_program.
    // Called once by the app after IRenderer::init.
    [[nodiscard]] bool init(platform::IRenderer& renderer);
    // Releases every handle this system created. Called before IRenderer::shutdown.
    void shutdown(platform::IRenderer& renderer);

    // One render frame: begin_frame with the camera blended at alpha (Rule 12),
    // then iterate world.view<Transform, PreviousTransform, RenderMesh>(),
    // interpolate each transform, cull against LocalBounds, submit, end_frame.
    void render(ecs::World& world, platform::IRenderer& renderer,
                const FirstPersonCamera& camera, float alpha);

    // Terrain (boundary agreed with core, stage-1 sync) ------------------------
    // Registers caller-authored geometry under a RenderMesh asset id — the
    // seam for zones whose meshes render cannot build (the DAG forbids
    // render -> anim includes, so the app ferries engine/anim's body segments
    // through this at init). LOUD ON EVERY REFUSAL, silent never:
    //  - an id already registered is REFUSED (stderr + false): a collision is
    //    two zones disagreeing about the id map, and silently replacing a
    //    mesh is how that drift would hide. Live replacement is deliberately
    //    not offered until something needs it — ask, do not overload this.
    //  - ids inside ranges owned by other mechanisms are REFUSED even when
    //    free: 1..31 (site table + growth), 32..33 (view model), 64..127
    //    (items). A typo must not shadow a blessed id.
    // Draws on the "prop" program like every untextured mesh in the cache.
    [[nodiscard]] bool register_mesh(platform::IRenderer& renderer, uint32_t mesh_asset,
                                     std::span<const platform::Vertex> vertices,
                                     std::span<const uint32_t> indices);

    // Triangulates the heightfield (TerrainMesher, stage 2) and uploads the chunk
    // mesh. Idempotent per chunk_coord: re-upload replaces the previous mesh.
    void upload_terrain(platform::IRenderer& renderer, const math::HeightFieldView& field);
    // Stage 3b: same, with the chunk's SurfaceFieldView (core boundary, spec
    // Dependencies item 8) baking design-truth splat weights (sand by shore,
    // rock by surface class, water-bed darkening). surface may be nullptr.
    void upload_terrain(platform::IRenderer& renderer, const math::HeightFieldView& field,
                        const math::SurfaceFieldView* surface);
    // Destroys the chunk's mesh. Must run before core frees the heightmap
    // (ChunkUnloaded fires before the data is freed — agreed lifetime).
    // The VOXEL surface — the true world geometry, carves included. Terrain
    // was drawn from the heightfield, which is a function of (x, z) and cannot
    // represent a ceiling, so the inside of the crag tunnel and the barrows
    // was never submitted at all: walkable, invisible. Uploading the voxel
    // mesh for a chunk REPLACES its heightfield mesh (same key), so the app
    // ferry can switch over per chunk without a flicker of double geometry.
    // This is also the first step of terrain LOD — core's coarse nodes arrive
    // through the same VoxelMeshView shape.
    // `field` is NOT used for geometry — the voxel mesh is the geometry. It is
    // the MAP's source, and it is a defaulted parameter because leaving it out
    // is what broke the map screen: the app switched the terrain ferry to the
    // voxel path, `note_chunk` only ever ran on the heightfield path, and the
    // explored map quietly stopped recording anything. The map wants one
    // height per column, which is exactly a heightfield and is NOT derivable
    // from a surface mesh without re-deriving a world fact (the bug class this
    // zone is forbidden from re-entering). Pass the same view you would have
    // passed to upload_terrain; nullptr keeps the old behaviour.
    void upload_terrain_voxel(platform::IRenderer& renderer,
                              const math::VoxelMeshView& mesh,
                              const math::HeightFieldView* field = nullptr,
                              const math::SurfaceFieldView* surface = nullptr);
    void drop_terrain(platform::IRenderer& renderer, glm::ivec2 chunk_coord);

    // Terrain LOD (the drawing half; core owns streaming and node meshes) ------
    //
    // THE PROBLEM THIS SOLVES IS NOT PERFORMANCE. Chunk streaming reaches
    // ~512 m while CAMERA_FAR is 8 km and the landmark rules are written for
    // 4 km, so the world simply STOPS EXISTING before the distance design
    // judges landmarks from — the 717 m acceptance frame of the massif could
    // not be photographed at all. LOD is what makes those frames exist.
    //
    // App wiring, in the order the app should call it:
    //   set_world_bounds(core's world_bounds_xz)          — once, after open
    //   set_lod_enabled(true)                             — once core answers
    //   set_streamed_rect(focus chunk +/- load_radius)    — when it changes
    //   update_lod(eye, dt)                               — every frame
    //   ferry lod_to_load()  -> core request_coarse_nodes
    //   ferry lod_to_release() -> core release_coarse_node + drop_lod_node
    //   upload_lod_node(...) when core's coarse_heightfield turns non-null
    // Nothing here is required for the app to run: with LOD disabled the
    // renderer behaves exactly as before.
    void set_world_bounds(glm::vec2 min_xz, glm::vec2 max_xz);
    // The ground core has streamed at FULL chunk detail. Coarse nodes are
    // excluded from it — see select_lod_nodes: a level-0 node is 1 m voxels
    // where a chunk is 2 m, so without this the two draw the same ground.
    void set_streamed_rect(glm::vec2 min_xz, glm::vec2 max_xz);
    void set_lod_enabled(bool enabled) { lod_.set_enabled(enabled); }
    [[nodiscard]] bool lod_enabled() const { return lod_.enabled(); }
    // Selection + fades. `dt_seconds` is the RENDER frame delta, not the sim
    // step: the cross-fade is a visual effect and must run at frame rate.
    void update_lod(const glm::vec3& eye, float dt_seconds);
    [[nodiscard]] std::span<const LodNode> lod_to_load() const { return lod_.to_load(); }
    [[nodiscard]] std::span<const LodNode> lod_to_release() const {
        return lod_.to_release();
    }
    // Selected but not yet delivered. THE FERRY RETRIES coarse_heightfield
    // AGAINST THIS, not against lod_to_load(): to_load is a per-frame diff that
    // names a node exactly once, while core admits nodes under a budget and
    // answers several frames later — a ferry built on to_load alone requests
    // nodes it then never collects, and the ground never appears.
    [[nodiscard]] std::span<const LodNode> lod_pending() const { return lod_.pending(); }
    // FOR A READOUT, USE lod_draw_count(), NOT lod_pending().size(). Earned:
    // the debug overlay showed "lod 0" from pending(), two zones read that as
    // "the far-detail ring never populates", and one of them (me) nearly
    // published it as a defect in a subsystem that was healthy — pending() is
    // the AWAITING-UPLOAD list, so zero is the steady state, and it reads as
    // absence exactly when everything is fine. The three counters answer three
    // different questions and only the last one goes to zero when the ring is
    // CULLED rather than missing:
    //   lod_selected_count() — what the policy asked for, from the eye alone
    //   lod_resident_count() — what core has actually delivered
    //   lod_draw_count()     — what survived the frustum and reached the GPU
    [[nodiscard]] size_t lod_selected_count() const { return lod_.selected_count(); }
    [[nodiscard]] size_t lod_resident_count() const { return lod_.resident_count(); }
    [[nodiscard]] size_t lod_draw_count() const { return lod_.last_draw_count(); }
    // Meshes one coarse node (COARSE_NODE_RESOLUTION = LOD_NODE_VOXELS + 1 =
    // 129 samples — the NODE lattice, NOT the chunk's, which is 257 since
    // 18:08:2026 — step = the level's voxel size) and uploads it. `surface` may be nullptr — core ships coarse surface fields
    // after the geometry, and slope-only splat is the agreed fallback.
    void upload_lod_node(platform::IRenderer& renderer, const LodNode& node,
                         const math::HeightFieldView& field,
                         const math::SurfaceFieldView* surface);
    void drop_lod_node(platform::IRenderer& renderer, const LodNode& node);
    [[nodiscard]] const LodTerrain& lod() const { return lod_; }

    // Scatter (stage 3b, data-only P5 instances — never entities) --------------
    // Bakes the chunk's scatter span into batched meshes (ScatterBatcher):
    // trees always drawn; bush/stone micro tiles culled by GRASS_VIEW_DISTANCE
    // from the eye. Idempotent per chunk_coord, mirrors upload_terrain.
    void upload_scatter(platform::IRenderer& renderer, glm::ivec2 chunk_coord,
                        std::span<const math::ScatterInstance> instances);
    void drop_scatter(platform::IRenderer& renderer, glm::ivec2 chunk_coord);

    // Water bodies (stage 3b) --------------------------------------------------
    // Builds one mesh per lake (ellipse plane) and per river segment (ribbon
    // along stations, width per station, surface descending source -> mouth;
    // segment i = stations [offsets[i], offsets[i+1])). Replaces any previous
    // bodies. The global set_water debug plane remains a separate fallback.
    void set_water_bodies(platform::IRenderer& renderer,
                          std::span<const math::LakePlane> lakes,
                          std::span<const math::RiverStation> river_stations,
                          std::span<const uint32_t> river_segment_offsets);
    void clear_water_bodies(platform::IRenderer& renderer);

    // --- The §8.1 path surface ------------------------------------------------
    //
    // The app ferries ChunkManager::path_surface() here once per world, the
    // same way it ferries water_bodies(): the network is whole-world and built
    // at open, not streamed. Render draws the tread; core routed it; flora
    // populates the margin (в24).
    void set_path_surface(platform::IRenderer& renderer,
                          std::span<const math::PathStation> stations,
                          std::span<const uint32_t> route_offsets);
    void clear_path_surface(platform::IRenderer& renderer);
    /// Drawable pieces currently resident (diagnostics and tests).
    [[nodiscard]] std::size_t path_piece_count() const { return path_meshes_.size(); }

    // Environment (stage 3) ----------------------------------------------------
    // The frame environment sent to IRenderer::set_environment each render.
    // Defaults from Materials.h; mutate to tune atmosphere/splat/water live.
    // time_seconds is overwritten each frame from the render-side visual clock.
    [[nodiscard]] platform::RenderEnvironment& environment() { return environment_; }

    /// THE VISUAL CLOCK, TOLD FROM OUTSIDE — and it exists because this project
    /// had TWO of them and fixed only one.
    ///
    /// Everything animated in a frame runs off a clock: the sun and moon off the
    /// app's `game_seconds_`, the cloud drift and the wind envelope off
    /// `environment_.time_seconds`, which this class reads from `steady_clock`.
    /// The app already made the FIRST one frame-deterministic during a tour
    /// (game_seconds_ += SIM_DT, with the reasoning written beside it: "a
    /// machine that runs the tour faster photographs a world at a different
    /// hour"). The second was left on the wall clock, so the same recipe run
    /// twice photographed the same hour with the CLOUDS SOMEWHERE ELSE.
    ///
    /// MEASURED, two runs of one binary on one recipe, sky probe: 67.466 % of
    /// pixels differ (max 137/255) with the clock free, and 0.000 % — byte for
    /// byte — with it pinned. The defect had been latent since the field was
    /// built; it surfaced when the sky gained enough structure for a few metres
    /// of drift to move a majority of it.
    ///
    /// Call once per frame with the same clock the sky is drawn from. Until a
    /// caller does, behaviour is bit-identical to before: the steady clock is
    /// used exactly as it was. DFN_VISTIME still overrides both.
    void set_visual_time(double seconds) {
        visual_time_told_ = true;
        told_visual_time_ = static_cast<float>(seconds);
    }

    /// THE COMPOSITION'S AIR ([air] в .scene, 22.08): a fog span sized to the
    /// map's own scale. Applied INSIDE render(), after every path that
    /// rewrites fog per frame (the app's day/night apply_sky_time and the
    /// DFN_TIME frozen-hour re-assert both stomp fog_start/end from the
    /// look-dev constants) — a load-time write would survive exactly one
    /// frame. Absolute overwrite, not a scale: idempotent by construction.
    /// `cloud_cover` — стартовая облачность карты (0..1; < 0 = не задана).
    /// Пишется ОДИН РАЗ здесь, а не покадрово: в отличие от тумана, облачность
    /// никто не переписывает каждый кадр, а будущее расписание погоды и
    /// DFN_CLOUD обязаны выигрывать у стартового значения карты.
    void set_air_override(float fog_start_m, float fog_end_m,
                          float cloud_cover = -1.0f) {
        air_override_set_ = true;
        air_fog_start_m_ = fog_start_m;
        air_fog_end_m_ = fog_end_m;
        if (cloud_cover >= 0.0f) {
            environment_.cloud_cover = std::min(cloud_cover, 1.0f);
        }
    }
    /// Back to the global constants (a map without [air] must not inherit the
    /// previous map's air).
    void clear_air_override() { air_override_set_ = false; }

    /// ПЯТНА ПОСТРОЕК для травы (cx, cz, hx, hz в плане): внутри них пучки не
    /// растут — «былинки сквозь пол» (владелец 23.08). Пишет AppHouse после
    /// заливки построек; фильтр живёт на выращивании, порядок загрузки чанков
    /// не важен.
    void set_ground_exclusions(std::vector<glm::vec4> rects) {
        ground_exclusions_ = std::move(rects);
        // Пересев принудительно: уже выращенная трава могла стоять в домах.
        tuft_built_ = false;
    }

    /// ПЫШНОСТЬ ТРАВЫ (владелец 23.08: «трава плохо выглядит — две
    /// травинки»). 0 — пол полосы реестра и прежние формы пучков бит-в-бит;
    /// 1 — середина полосы и формы 5/7/9/12. Пишет App по двери
    /// DFN_GRASS_LUSH.
    void set_grass_lushness(float lush) {
        if (grass_lushness_ != lush) {
            grass_lushness_ = lush;
            tuft_built_ = false; // пересев: плотность и формы изменились
        }
    }

    /// ПОЛЕ КЛАССОВ ПОЛОТНА (22.08, владелец: «тропинки опять плитами
    /// кладутся, а не тропинкой каменной»): материал троп композиции — в
    /// самой земле. Пишется приложением после чтения .relief; действует на
    /// чанки, воксельную землю и LOD-кольцо ОДНОЙ упаковкой альфы (см.
    /// TerrainMesher.h). Пустой вызов возвращает прежнюю упаковку.
    /// МАСКА ТРОП МИРА (круг 5: «шевроны»): растеризованные износ (R) и
    /// класс (G, 0/85/170/255) полотна, RGBA8 res x res, покрывает квадрат
    /// [origin, origin + span]. Пустой вектор или span <= 0 — маски нет,
    /// кромка полотна из вершинной альфы. Текстура заводится лениво на
    /// кадре (создание требует renderer).
    void set_path_mask(std::vector<uint8_t> rgba, uint32_t res,
                       glm::vec2 origin_xz, float span_m) {
        path_mask_pixels_ = std::move(rgba);
        path_mask_res_ = res;
        path_mask_pending_ = !path_mask_pixels_.empty() && span_m > 0.0f;
        path_mask_asset_ = path_mask_pending_ ? path_mask_asset_ : 0;
        environment_.path_mask_origin_xz = origin_xz;
        environment_.path_mask_span_m = path_mask_pending_ ? span_m : 0.0f;
        environment_.path_mask_res_px =
            path_mask_pending_ ? static_cast<float>(res) : 0.0f;
        if (!path_mask_pending_) {
            path_mask_asset_ = 0;
        }
    }

    void set_path_classes(PathClassField field) {
        path_classes_ = std::move(field);
        path_classes_set_ = !path_classes_.strokes.empty();
        lod_.set_path_classes(path_classes_set_ ? &path_classes_ : nullptr);
    }

    // Water plane capability (stage 3) -----------------------------------------
    // Creates (or replaces) a flat water plane at world height `height_m`
    // covering the square [center - half_extent, center + half_extent] on x/z.
    // Also raises the terrain sand line to just above the waterline. Placement
    // is data-driven later (design doc); the app/editor wires real calls.
    // Debug: DFN_WATER=<height_m> in init() enables a testbed-covering plane.
    void set_water(platform::IRenderer& renderer, float height_m,
                   glm::vec2 center_xz, float half_extent_m);
    void clear_water(platform::IRenderer& renderer);
    [[nodiscard]] bool water_enabled() const { return water_mesh_ != 0; }

    // Map screen (user request: a Skyrim-style map on a key) -------------------
    // The map fills the frame as an opaque overlay while open; the world keeps
    // rendering behind it (no app-loop change, and reopening costs nothing).
    // The app owns the key binding — it calls toggle_map() on Key::M.
    // Debug/verification: DFN_MAP=1 opens the map at init (tour evidence).
    void toggle_map() { map_.toggle(); }
    /// ДОЗА СТРУКТУРЫ ЛИСТА НАБОРА (дверь DFN_MAT_SHEET, волна 4 зоны
    /// МАТЕРИАЛЫ). Сторона плитки берётся у неё же (parts_tile_px) и входит в
    /// ключ кэша — доза, сменившая сторону, обязана промахнуться мимо вчерашней
    /// плитки.
    ///
    /// ДВЕРЬ ЧИТАЕТ ПРИЛОЖЕНИЕ, А НЕ ЭТОТ МОДУЛЬ, и это требование набора, а не
    /// вкус: дверь, которую не читает ни один файл engine/app, — это строка,
    /// против которой можно написать рецепт, который никогда не откроется
    /// (tests/app/DoorsTests.cpp, «no door is described that nothing reads»;
    /// именованное исключение там ровно одно и занято DFN_TOUR). Ставится ОДИН
    /// раз на заливке карты, рядом с дозой DFN_MAT, — до того, как напечатан
    /// первый лист: доза, сменившаяся между двумя постройками, дала бы дом с
    /// зерном рядом с домом без него.
    static void set_parts_sheet_dose(PartsSheetDose dose);

    /// Значение, поставленное выше; по умолчанию Flat — лист волны 3.
    /// ОДНО МЕСТО, ГДЕ ДОЗУ СПРАШИВАЮТ (правило 39): свотчи редактора
    /// печатаются из того же листа, что носят стены, и второй источник ответа
    /// развёл бы их в первый же день.
    [[nodiscard]] static PartsSheetDose parts_sheet_dose();

    void set_map_open(bool open) { map_.set_open(open); }
    [[nodiscard]] bool map_open() const { return map_.open(); }
    // The internal (low-res) target size the overlay canvas is drawn at, so
    // one canvas pixel is one screen pixel. Defaults to INTERNAL_RES /
    // DFN_INTERNAL_RES; the app should pass the value it actually gave
    // RendererInitParams (settings.cfg may override both).
    void set_internal_resolution(uint32_t width, uint32_t height);
    [[nodiscard]] const MapScreen& map() const { return map_; }

    // HUD layer (the interaction prompt, and later the crosshair / status) ----
    // A TRANSPARENT screen-space canvas the game draws into every frame, blitted
    // over the world through the same overlay path as the map but with an alpha
    // blend. The caller owns its contents: clear it, draw into it with
    // BitmapFont.h's draw_text, and render() composites it.
    //
    // WHY THE CALLER DRAWS AND NOT render: the prompt's TEXT is a localization
    // string resolved from components::Highlightable::prompt_key (Rule 5).
    // render must never contain a user-facing string, so it cannot own the
    // content — only the surface and the blit.
    //
    // The canvas is sized to the internal resolution (one canvas pixel = one
    // screen pixel) by init() and set_internal_resolution(); do not resize it.
    [[nodiscard]] PixelCanvas& hud() { return hud_; }
    void set_hud_visible(bool visible) { hud_visible_ = visible; }
    [[nodiscard]] bool hud_visible() const { return hud_visible_; }

    /// ОДИН ПРЕДМЕТ ПОВЕРХ ХОЛСТА ЭКРАНА — сегодня объёмный герб главного меню
    /// (заказ владельца 27.08). Слот, а не отдельный путь: у экрана меню нет
    /// ни мира, ни сущностей, а нужен ровно один меш, нарисованный ПОСЛЕ
    /// холста.
    ///
    /// ПОЧЕМУ ПОСЛЕ, А НЕ ДО. Страницы меню чистят холст непрозрачным чёрным
    /// (Menu.cpp), то есть наложение кроет кадр целиком; меш, посланный до
    /// него, не был бы виден ни одним пикселем. Второй вариант — дыра в
    /// холсте — потребовал бы, чтобы за дырой гарантированно ничего не было,
    /// а выход в главное меню ИЗ ИГРЫ оставляет мир загруженным. Вид сцены
    /// у бэкенда Sequential, поэтому «после» здесь значит именно «поверх».
    ///
    /// Матрица задаётся В ОСЯХ КАМЕРЫ (камера в нуле, смотрит в −Z): так
    /// вызывающему не нужно знать, где стоит камера, и предмет не уезжает,
    /// если меню открыли посреди мира.
    struct ScreenProp {
        uint32_t mesh = 0;    ///< MeshHandle::id
        uint32_t program = 0; ///< ProgramHandle::id
        glm::mat4 in_camera{1.0f};
        /// ИЗ ЧЕГО ЭТОТ ПРЕДМЕТ СДЕЛАН (зона МАТЕРИАЛЫ, 28.08). MATERIAL_NONE
        /// — «не сказано», то есть прежний ламберт по цвету вершины, кадр
        /// бит-в-бит. Первый клиент — золотой герб: у металла блик окрашен
        /// им самим, и никакой цвет вершины этого не выражает.
        core::MaterialId material = core::MATERIAL_NONE;
        [[nodiscard]] bool valid() const { return mesh != 0 && program != 0; }
    };
    /// Живёт ОДИН кадр и снимается сразу после отправки — как временные
    /// светы: игровой кадр, который его не ставил, не должен унаследовать
    /// герб от последнего кадра меню.
    void set_screen_prop(const ScreenProp& prop) { screen_prop_ = prop; }

    /// НА КАКОМ РАССТОЯНИИ ОТ ГЛАЗА ВИСИТ ХОЛСТ ЭКРАНА. Одно определение на
    /// весь движок (правило 39): его читает и draw_overlay, который холст
    /// туда кладёт, и app, которому надо поставить герб ПЕРЕД холстом. Копия
    /// этой арифметики у вызывающего разошлась бы с настоящей при первой же
    /// правке ближней плоскости, и разошлась бы молча.
    [[nodiscard]] static float overlay_depth_m(const FirstPersonCamera& camera);

private:
    struct ChunkKeyHash {
        size_t operator()(const glm::ivec2& v) const;
    };

    // Registry-assigned dense id for a procedural texture, cached by params
    // (stage-1 registry decision); creates and uploads on first use.
    uint32_t procedural_texture_asset(platform::IRenderer& renderer, uint64_t key,
                                      uint32_t width, uint32_t height,
                                      const uint8_t* pixels);

    // Collects every entity with CarriedLight + Transform into the frame
    // environment's point-light array (torches, lanterns, an NPC's lamp).
    // Gameplay owns WHETHER a light is on; this owns how it looks and which
    // ones get a shadow map. The flame sits at CarriedLight::offset in CARRIER
    // space — a light at the eye casts no visible shadow by construction, so
    // the offset is the feature, not a detail.
public:
    /// A light that is NOT carried by an entity: a lamp the composition hung
    /// on the map, or a swarm's glow this frame. Fed into the SAME budget as
    /// the torches and sorted by the same rule — a second selection would be a
    /// second answer to "which eight are lit" (Rule 32), and the two would
    /// disagree exactly when it matters, with the player between them.
    struct ExtraLight {
        glm::vec3 position{0.0f};
        glm::vec3 color{0.0f};
        float radius_m = 0.0f;
        /// Whether this light ASKS for a shadow map. There are two slots for
        /// the whole frame, so most lights must say no — and saying no has to
        /// MEAN no, or the field is decoration.
        bool casts_shadow = false;
        /// ИНТЕРЬЕРНЫЙ свет гейтится небесной видимостью приёмника (см.
        /// SceneLight::interior): без теневого слота он иначе светит улице
        /// сквозь кладку.
        bool interior = false;
        /// Коробка комнаты (план): см. platform::PointLight.
        glm::vec2 room_center_xz{0.0f};
        glm::vec2 room_half_xz{0.0f};
        /// Мягкость источника 0..1 — см. platform::PointLight::softness.
        float softness = 0.0f;
        /// Мерцание живого огня 0..1: модуляция яркости на CPU от времени
        /// сцены и позиции (детерминированная — заморозка часа замирает и её).
        float flicker = 0.0f;
    };
    /// Lamps the map's composition owns. Set once when a map opens; survives
    /// until the next one. A composition of a hundred lamps is legal — only
    /// eight are ever lit, and which eight is the budget's business.
    void set_scene_lights(std::vector<ExtraLight> lights);
    /// Лампы, которые СЕЙЧАС считаются лампами карты. Читается на входе в
    /// интерьер, чтобы вернуть их на выходе: пересобрать их заново значило бы
    /// завести ВТОРОЕ место, где из [light] делается ExtraLight (правило 39).
    [[nodiscard]] const std::vector<ExtraLight>& scene_lights() const {
        return scene_lights_;
    }
    /// THE SWARM'S BILLBOARDS for this frame. Uploaded as one small mesh and
    /// drawn UNLIT: a firefly is its own light source, and a mote that the
    /// canopy shades goes dark exactly where it is wanted. An empty mesh is
    /// the ordinary daytime state and costs one branch.
    void set_firefly_mesh(platform::IRenderer& renderer, const MeshData& mesh);

    /// THE MAP'S SELF-LIT GEOMETRY: flames, glowing glass, embers. Set once
    /// when a composition loads, drawn by the same unlit program the swarm
    /// uses — a flame that the night shades into black is a light source that
    /// is not lit, which is the first thing anyone notices.
    void set_emissive_mesh(platform::IRenderer& renderer, const MeshData& mesh);

    /// THE BUILD GHOST: the part the editor is holding, drawn where it would
    /// land. Set EVERY FRAME while the build hand is open (it follows the
    /// crosshair) and cleared with an empty mesh when it is not — same shape
    /// as the swarm's slot, and for the same reason: this geometry moves.
    /// Drawn UNLIT so its tint reads as an answer (green allowed, red refused)
    /// rather than as a material the builder has to interpret through the sun.
    void set_ghost_mesh(platform::IRenderer& renderer, const MeshData& mesh);

    /// ПОСТРОЙКА, КОТОРУЮ ПРАВЯТ ПРЯМО СЕЙЧАС: дом из графа редактора, телом.
    ///
    /// Четвёртый слот того же вида, что светлячки, самосветящаяся геометрия и
    /// призрак, но с двумя отличиями, и оба существенные:
    ///
    /// 1. РИСУЕТСЯ ОСВЕЩЁННЫМ, программой «prop». Призрак не освещают нарочно —
    ///    он ответ, нарисованный поверх мира. Дом ответом не является: он вещь
    ///    в мире, и стена, не темнеющая к вечеру, читается как декорация.
    /// 2. ЗАЛИВАЕТСЯ НЕ КАЖДЫЙ КАДР, а когда постройка изменилась. У призрака
    ///    геометрия едет за прицелом, у дома — стоит; перезаливать тысячи
    ///    треугольников на каждый кадр значило бы платить за неподвижное.
    ///    Когда заливать — знает вызывающий, здесь только «замени на это».
    ///
    /// Пустой меш очищает слот: карта без построек — обычное состояние.
    /// ТЕЛО ПОСТРОЙКИ — ПОТОК НА МАТЕРИАЛ. Материал приходит текстурой
    /// draw-вызова, поэтому сколько разных материалов выбрал человек, столько
    /// потоков и рисуется (заказ 19.08: «выбирать текстуры для палок, стен»).
    /// Плитка каждого материала вырезается из листа набора ЛЕНИВО и кэшируется
    /// под своим ключом: заплатили только за то, что реально носится.
    struct HouseStream {
        MeshData mesh;
        std::uint32_t surface = 0; ///< PartSurface ordinal
        std::uint32_t tone = 1;    ///< PartTone ordinal
        /// ИЗ ЧЕГО СДЕЛАН ПОТОК (волна 3 зоны МАТЕРИАЛЫ). Здесь соединились
        /// два из четырёх словарей вещества, названных инвентаризацией:
        /// БЕЗЫМЯННЫЙ материал `HouseStreamGpu{texture, normal, emissive,
        /// pane_glow}` — «уже готовый материал, у которого отняли имя» — и
        /// одиннадцать ИМЁН PartMaterial, до которых из данных было не
        /// дотянуться. Теперь имя приезжает из рецепта или из .dfo, а числа
        /// живут в реестре.
        ///
        /// MATERIAL_NONE — «не сказано», и это по-прежнему подавляющее
        /// большинство потоков города: пара (surface, tone) кроет их так же,
        /// как кроила. Кадр от этого поля не меняется, пока вещество не
        /// названо, — и это ровно контрольная рука дозы DFN_MAT.
        core::MaterialId material = core::MATERIAL_NONE;
        /// Самосветный поток (24.08): рисуется без освещения (пламя, стекло).
        bool emissive = false;
        /// СИЛА СВЕЧЕНИЯ ОКОННОЙ ВСТАВКИ (28.08), 0 — обычный поток. Читается
        /// только вместе с emissive и едет в DrawParams::aux1. Величина, а не
        /// признак, потому что окно светит НЕБОМ: полдень и полночь у него
        /// разные, и час суток называет зона app на входе в дом.
        float pane_glow = 0.0f;
    };
    /// ДВЕРЬ — СВОЙ ПОТОК С ПЕТЛЁЙ (заказ 19.08: стена со свойством «дверь»
    /// получает анимацию вокруг выбранной пары якорей). Рисуется той же
    /// освещённой программой, но с поворотом вокруг оси петли; демонстрационный
    /// ход двери считает render — редактору важно ПОКАЗАТЬ, что петля выбрана
    /// верно, игровое «открыто/закрыто» будет состоянием симуляции.
    struct HouseDoor {
        MeshData mesh;
        glm::vec3 hinge_a{0.0f};
        glm::vec3 hinge_b{0.0f};
        std::uint32_t surface = 0;
        std::uint32_t tone = 1;
        /// Демонстрационное качание — ТОЛЬКО у двери, выбранной в сессии
        /// редактирования (показать петлю). Остальные стоят ЗАКРЫТЫМИ:
        /// приёмка 20.08 прочитала качающиеся полотна готовых домов как
        /// «все двери перекошены и шире проёма».
        bool demo_swing = false;
        /// Угол открытой створки (радианы). Заливка ставит закрытую: «эта
        /// дверь открыта» — состояние мира, и его называет app.
        float open_angle = 0.0f;
    };

    void set_house_mesh(platform::IRenderer& renderer, std::vector<HouseStream> streams,
                        std::vector<HouseDoor> doors);

    /// УГОЛ ОТКРЫТОЙ СТВОРКИ ГОРОДА, радианы вокруг её петли. Это обещанное
    /// шапкой HouseDoor «игровое открыто/закрыто как состояние симуляции»:
    /// решает его ЗОНА APP (тип двери, нажатие, кто её открыл), а render
    /// только рисует названный угол — и не помнит его между заливками, потому
    /// что заливка означает другую карту либо другую постройку.
    /// `index` — номер створки в списке, отданном set_house_mesh; чужой номер
    /// молча игнорируется: заливка могла смениться под руками у app.
    void set_house_door_open(std::size_t index, float radians);

    /// ВТОРОЙ СЛОТ ПОСТРОЕК — ИНТЕРЬЕР-ЛОКАЦИЯ (И15). Тот же вид геометрии,
    /// та же программа, свой набор буферов. Отдельный слот, а не повторный
    /// set_house_mesh, ровно потому, что город обязан ПЕРЕЖИТЬ вход в дом:
    /// перезалив слота построек стоил бы на выходе второй загрузки города,
    /// а выход по своду обязан укладываться в 0.05 с.
    void set_interior_mesh(platform::IRenderer& renderer,
                           std::vector<HouseStream> streams,
                           std::vector<HouseDoor> doors);

    /// ПОДВИЖНЫЙ ПРЕДМЕТ — СВОЙ ДРО СО СВОЕЙ МАТРИЦЕЙ (зона ФИЗИКА ПРЕДМЕТОВ,
    /// заказ владельца 28.08 «поднимать объекты, держать, складывать»).
    ///
    /// ПОЧЕМУ ОТДЕЛЬНЫЙ СЛОТ, А НЕ ПОТОК ПОСТРОЕК. Потоки построек — это
    /// ЗАПЕЧЁННЫЕ В МИР вершины: кувшин, влитый в поток, стоит там, где его
    /// влили, до следующей заливки карты, а кувшин, который несут в руках,
    /// переезжает шестьдесят раз в секунду. Перезаливать ради него поток
    /// размером с квартал — это перестраивать дом, чтобы подвинуть миску.
    ///
    /// ПОЧЕМУ НЕ РОССЫПЬ СЦЕНЫ. Россыпь рисует БЕЗ материала — плоским
    /// вершинным цветом (та самая причина, по которой кровать поехала в
    /// потоки построек 27.08): уехавший туда кувшин потерял бы глину на
    /// глазах у игрока. Здесь те же плитки листа набора, что у постройки,
    /// и та же программа — отличается только матрица.
    ///
    /// Геометрия приходит в МЕСТНЫХ координатах предмета (та же система, в
    /// которой её мерит реестр), а место называет матрица: одна и та же
    /// матрица приходит из физики, поэтому нарисованное и осязаемое не могут
    /// разъехаться по построению.
    struct LooseProp {
        std::vector<HouseStream> streams; ///< местные координаты предмета
        /// В КАКОМ СЛОТЕ ЖИВЁТ: интерьерный предмет рисуется в подвесе,
        /// городской — снаружи. Один список на оба, потому что список этот
        /// короткий, а два списка разошлись бы правкой одного из них.
        bool interior = false;
    };
    /// Заливает список подвижных предметов (старые буферы освобождаются).
    void set_loose_props(platform::IRenderer& renderer, std::vector<LooseProp> props);
    /// Ставит матрицу предмету по его номеру в списке. Чужой номер молча
    /// игнорируется — по той же причине, что у set_house_door_open: заливка
    /// могла смениться под руками у app.
    void set_loose_prop_transform(std::size_t index, const glm::mat4& transform);
    [[nodiscard]] std::size_t loose_prop_count() const { return loose_props_.size(); }
    void clear_loose_props(platform::IRenderer& renderer);

    /// ПОДВЕС ЭКСТЕРЬЕРА. true — кадр рисует ТОЛЬКО интерьерный слот (и всё,
    /// что не является миром: тело игрока, предмет в руке, HUD); земля,
    /// дальний рельеф, трава, рассев, вода и потоки городских построек
    /// пропускаются. Город при этом остаётся резидентным — ни один буфер не
    /// освобождается, ни один чанк не выгружается.
    void set_world_suspended(bool suspended) { world_suspended_ = suspended; }
    [[nodiscard]] bool world_suspended() const { return world_suspended_; }

    /// Lights that live for ONE frame (the firefly swarm). Replaced every
    /// frame; an empty list is the normal daytime state, not an error.
    void set_transient_lights(std::vector<ExtraLight> lights);

private:
    std::vector<ExtraLight> scene_lights_;
    uint32_t firefly_mesh_id_ = 0;
    uint32_t emissive_mesh_id_ = 0;
    uint32_t ghost_mesh_id_ = 0;
    struct HouseStreamGpu {
        uint32_t mesh_id = 0;
        uint32_t texture_asset = 0;
        uint32_t normal_asset = 0; ///< лист нормалей материала (рельеф)
        /// Мировой габарит потока — для отсечения по пирамиде. Замер 22.08
        /// (профиль кузнеца, 59905 кадров): при потоках «материал на весь
        /// город» кадр слал 1.9 млн треугольников НЕЗАВИСИМО от направления
        /// взгляда — отсечение исправно, но пятно меша было размером с карту
        /// и отсекать было нечего. Пространственная нарезка потоков (см.
        /// AppHouse) возвращает отсечению предмет работы.
        math::Aabb bounds{};
        bool emissive = false; ///< самосветный поток — DrawParams.emissive
        float pane_glow = 0.0f; ///< сила оконной вставки — DrawParams.aux1
        /// ВЕЩЕСТВО ПОТОКА — DrawParams.material. Пятое поле у структуры,
        /// которую инвентаризация назвала «безымянным материалом, самой
        /// близкой к цели точкой сегодняшнего кода»: у неё было отнято имя, и
        /// это оно.
        core::MaterialId material = core::MATERIAL_NONE;
    };
    struct HouseDoorGpu {
        uint32_t mesh_id = 0;
        uint32_t texture_asset = 0;
        uint32_t normal_asset = 0;
        glm::vec3 hinge_a{0.0f};
        glm::vec3 hinge_b{0.0f};
        bool demo_swing = false;
        float open_angle = 0.0f;
    };
    /// ОДИН ПОДВИЖНЫЙ ПРЕДМЕТ В ВИДЕОПАМЯТИ: свои потоки плюс матрица.
    struct LoosePropGpu {
        std::vector<HouseStreamGpu> streams;
        glm::mat4 transform{1.0f};
        /// Габарит В МЕСТНЫХ осях: мировой считается матрицей на кадре.
        /// Держать мировой было бы вторым описанием того же места, и оно
        /// протухало бы ровно на тик (правило 39).
        glm::vec3 local_centre{0.0f};
        float local_radius = 0.0f;
        bool interior = false;
    };
    std::vector<LoosePropGpu> loose_props_;

    /// Заливка ОДНОГО слота построек — общее тело set_house_mesh и
    /// set_interior_mesh. Объявлена ЗДЕСЬ, а не рядом с ними: её параметры
    /// названы типами, которые определены строкой выше.
    void fill_house_slot(platform::IRenderer& renderer,
                         std::vector<HouseStream> streams,
                         std::vector<HouseDoor> doors,
                         std::vector<HouseStreamGpu>& out_streams,
                         std::vector<HouseDoorGpu>& out_doors);
    std::vector<HouseStreamGpu> house_streams_;
    std::vector<HouseDoorGpu> house_doors_;
    /// Второй слот — интерьер-локация (И15). Живёт независимо от городского.
    std::vector<HouseStreamGpu> interior_streams_;
    std::vector<HouseDoorGpu> interior_doors_;
    bool world_suspended_ = false;
    float house_door_phase_ = 0.0f; // демонстрационный ход двери
    /// Плитка материала набора, ленивo и с кэшем (см. set_house_mesh).
    /// `normal` = true — лист НОРМАЛЕЙ той же плитки: рельеф стен и столбов
    /// идёт тем же механизмом, что у собранных деталей (aux-лист, fs_prop).
    uint32_t house_tile_asset(platform::IRenderer& renderer, uint32_t surface,
                              uint32_t tone, bool normal = false);
    /// ЛИСТЫ НАБОРА, СПЕЧЁННЫЕ ПО ОДНОМУ РАЗУ. house_tile_asset печатал ЦЕЛЫЙ
    /// лист 2304x1024 (9.4 МБ) на КАЖДЫЙ промах кэша плитки — ради одной
    /// вырезанной плитки 256x256, то есть 0.7% того, что посчитали. Постройка
    /// из девяти материалов платила это девять раз, а с листом нормалей —
    /// восемнадцать. Лист печётся лениво (карта без построек не платит
    /// ничего) и живёт до shutdown(), где освобождается вместе с текстурами.
    [[nodiscard]] const PartsAtlas& parts_sheet(bool normal);
    PartsAtlas parts_sheet_albedo_;
    PartsAtlas parts_sheet_normal_;
    bool parts_sheet_albedo_ready_ = false;
    bool parts_sheet_normal_ready_ = false;
    std::vector<ExtraLight> transient_lights_;

    /// One flame gathered this frame, before the eight slots are handed out.
    struct PointLightCandidate {
        glm::vec3 position;
        glm::vec3 color;
        float radius;
        float d2; ///< squared distance to the eye — the only thing slots are sorted by
        /// Does this light WANT a shadow map? A carried flame does by default
        /// (its whole point is the moving shadow it throws); a lamp hung by a
        /// composition says so for itself. The two caster slots are handed out
        /// among the lights that ask, in distance order — before this field
        /// they went to the nearest two REGARDLESS, so a composition that
        /// asked for no shadows got one anyway, from whichever lamp happened
        /// to be closest.
        bool wants_shadow = true;
        /// См. ExtraLight::interior — доезжает до PointLight и до шейдера.
        bool interior = false;
        glm::vec2 room_center_xz{0.0f};
        glm::vec2 room_half_xz{0.0f};
        float softness = 0.0f;
        float flicker = 0.0f;
    };
    void publish_point_lights(std::vector<PointLightCandidate>& candidates);
    void collect_point_lights(ecs::World& world, const FirstPersonCamera& camera,
                              float alpha);

    // Blits a CPU screen canvas over the frame: uploads it as one RGBA8
    // texture and draws the unlit quad that exactly fills the frustum just
    // past the near plane. Generic on purpose — the future menu screen draws
    // through the same path (no IRenderer contract change, Rule 26).
    // `blended` picks the alpha-blended "overlay" program (HUD) over the opaque
    // "unlit" one (full-screen screens like the map).
    void draw_overlay(platform::IRenderer& renderer, const PixelCanvas& canvas,
                      const FirstPersonCamera& camera, float alpha,
                      bool blended = false);

    // A resident chunk mesh plus the world AABB it occupies. The bounds are
    // measured at upload (the mesher's vertices are right there) and exist for
    // frustum culling — recomputing them per frame from the GPU is impossible
    // and storing them is 24 bytes.
    struct TerrainRes {
        uint32_t mesh_id = 0;
        math::Aabb bounds{};
    };

    // One micro-scatter tile resident on the GPU (culling data + mesh).
    struct MicroTileRes {
        glm::vec2 center_xz{0.0f};
        float radius_m = 0.0f;
        uint32_t mesh_id = 0; // MeshHandle.id
    };
    struct ChunkScatterRes {
        uint32_t trees_mesh_id = 0;   // 0 = no trees in this chunk
        uint32_t foliage_mesh_id = 0; // alpha-cutout leaf cards ("foliage")
        math::Aabb bounds{};          // trees + cards, for frustum culling
        std::vector<MicroTileRes> micro;
        /// The detail level THIS chunk's flora is currently baked at, and the
        /// instances it was baked from. The instances are kept because a band
        /// crossing re-bakes the chunk and the world data that produced it lives
        /// in engine/world, a zone render cannot reach back into. They are
        /// small — a ScatterInstance is 24 bytes and the whole forest stand is
        /// ~13 500 of them — against the megabytes of vertex data they replace.
        FloraLod lod = FloraLod::Full;
        std::vector<math::ScatterInstance> instances;
        glm::vec2 center_xz{0.0f}; ///< chunk centre, the banding's distance anchor
    };

    // True if the box should be drawn: inside the frustum, OR close enough to
    // the eye that it is still a SUN SHADOW CASTER. The second half is not an
    // optimization detail — the backend double-submits every opaque draw into
    // the shadow map, so a box culled here loses its shadow too, and a tree
    // just off the left edge of the screen would stop shading the ground the
    // player is looking at.
    [[nodiscard]] static bool visible_or_casting(const math::Frustum& frustum,
                                                 const math::Aabb& box,
                                                 const glm::vec3& eye);

    // Resource bookkeeping only — never game state (Rule 10).
    std::unordered_map<glm::ivec2, TerrainRes, ChunkKeyHash> terrain_meshes_;
    std::unordered_map<glm::ivec2, ChunkScatterRes, ChunkKeyHash> scatter_meshes_;
    /// Last eye position the scatter banding saw, horizontal only. Held because
    /// upload_scatter is called from the chunk ferry, which has no camera: a
    /// chunk arriving mid-walk must be born at the level its distance calls
    /// for, not at Full and then walked down over the next frames.
    glm::vec2 scatter_eye_{0.0f};

    // GROUND TUFTS (GroundTufts.h): the sparse near-field grass. Two halves,
    // and the split is the whole design.
    //
    // The SPOTS are harvested once per chunk upload, off the drawn mesh, and
    // are cheap to keep (a 256 m chunk at the design floor density is a few
    // hundred KB). The GEOMETRY is one eye-local mesh rebuilt only when the eye
    // has walked TUFT_REBUILD_STEP_M, because at the Rule 33 view distance only
    // a couple of hundred tufts are ever visible — baking blades for a whole
    // chunk would spend tens of megabytes to draw a hundredth of them.
    /// The tuft settings, DERIVED from approved rows rather than typed
    /// (Rule 14). Defined in the .cpp beside the Rule 33 arithmetic.
    // НЕ static с 23.08: пышность травы — состояние системы (grass_lushness_).
    [[nodiscard]] GroundTuftParams tuft_params() const;
    /// Regrows the eye-local tuft mesh if the eye has moved far enough.
    void refresh_ground_tufts(platform::IRenderer& renderer, glm::vec3 eye);

    /// Re-bakes AT MOST ONE resident scatter chunk per frame at the detail
    /// level its distance now calls for (FLORA_LOD_REDUCED_M /
    /// FLORA_LOD_SILHOUETTE_M, with FLORA_LOD_HYSTERESIS_M of overlap so a
    /// player standing on a band edge does not re-bake every frame).
    ///
    /// ONE PER FRAME IS THE WHOLE SAFETY ARGUMENT. A re-bake costs what the
    /// chunk's first bake cost, so an unbudgeted pass that re-levelled a ring
    /// at once would trade a steady frame cost for exactly the multi-second
    /// freeze chunk streaming already learned to spread out. Nearest first,
    /// for the same reason streaming admits nearest first: the chunk the
    /// player is looking at is the one whose detail is wrong most visibly.
    void refresh_scatter_lod(platform::IRenderer& renderer, glm::vec3 eye);

    /// Bakes and uploads one chunk's scatter at `lod`, replacing whatever was
    /// there. The shared body of the first upload and of every re-bake — two
    /// copies of this would drift the day one of them learned something.
    void bake_scatter(platform::IRenderer& renderer, glm::ivec2 chunk_coord,
                      std::span<const math::ScatterInstance> instances, FloraLod lod);

public:
    /// Uploads PREBUILT scatter streams for a chunk — the object-registry
    /// gallery's path: the app assembles wood/foliage buffers from .dfo
    /// objects and hands them here to ride the ordinary scatter draw (same
    /// programs, same leaf atlas, same wind). No instances are kept, so the
    /// LOD banding pass skips these by construction: a registry object's
    /// levels are the FORGE's business, baked into the registry, not re-baked
    /// in the frame (в1).
    void upload_prebuilt_scatter(platform::IRenderer& renderer, glm::ivec2 chunk_coord,
                                 const MeshData& trees, const MeshData& foliage);

private:

    std::unordered_map<glm::ivec2, std::vector<TuftSpot>, ChunkKeyHash> tuft_spots_;
    uint32_t tuft_mesh_id_ = 0;
    glm::vec3 tuft_built_at_{0.0f};
    bool tuft_built_ = false;
    float grass_lushness_ = 0.0f;
    // Маска троп: пиксели до ленивой заливки в текстуру + её ассет.
    std::vector<uint8_t> path_mask_pixels_;
    uint32_t path_mask_res_ = 0;
    bool path_mask_pending_ = false;
    uint32_t path_mask_asset_ = 0;
    bool tufts_off_ = false; // DFN_NO_TUFTS=1 — the counterfactual arm
    std::unordered_map<uint32_t, uint32_t> mesh_cache_;    // mesh_asset id -> MeshHandle.id
    std::unordered_map<uint32_t, uint32_t> texture_cache_; // texture_asset id -> TextureHandle.id
    std::unordered_map<uint64_t, uint32_t> proc_texture_ids_; // params key -> asset id
    // Water bodies are MERGED INTO WORLD-GRID BUCKETS, not uploaded one mesh
    // per body. Core's hydrology emits a LakePlane per pond: 17336 of them on
    // the 2x2 km testbed, which spent every one of bgfx's 4096 vertex-buffer
    // handles before a single chunk of terrain could upload, and cost 4078 draw
    // calls a frame besides. One bucket per CHUNK_SIZE cell keeps the count
    // proportional to world area / chunk area and gives the frustum something
    // to cull.
    struct WaterBucket {
        uint32_t mesh_id = 0;
        math::Aabb bounds{};
    };
    std::vector<WaterBucket> water_body_meshes_;
    // A path piece is one class over ~128 m of tread; the class picks the atlas
    // cell and is carried in the vertices, so this only needs the mesh and its
    // bounds — same shape as a water bucket, same reason (something to cull).
    std::vector<WaterBucket> path_meshes_;
    uint32_t next_texture_asset_ = 1; // dense id allocator (0 = none)
    uint32_t terrain_program_ = 0; // ProgramHandle.id
    uint32_t unlit_program_ = 0;   // ProgramHandle.id
    uint32_t water_program_ = 0;   // ProgramHandle.id
    uint32_t prop_program_ = 0;    // ProgramHandle.id (lit+fog vertex color)
    uint32_t foliage_program_ = 0; // ProgramHandle.id (alpha-cutout leaf cards)

    /// КАКОЙ ПРОГРАММОЙ РИСОВАТЬ ЭТО ВЕЩЕСТВО (волна 3 зоны МАТЕРИАЛЫ).
    /// Единственное место, где выбор программы делается ЗДЕСЬ, и делается он
    /// не по потоку файла, а по записи вещества: качается ли оно и вырезается
    /// ли. Само правило живёт в render::material_program (PartsMaterial.h) —
    /// сюда оно приходит именем, а имя переводится в загруженный
    /// идентификатор.
    ///
    /// ЧТО ЭТО ЧИНИТ. До волны 3 «чтобы брус стал кирпичом, надо было перепечь
    /// меш В ДРУГОЙ ПОТОК»: WOOD ехал prop, CARD и BARK — foliage, HOUS —
    /// prop с вырезанной плиткой. Программа была следствием того, куда
    /// кузница положила треугольники, а не того, из чего они сделаны.
    [[nodiscard]] uint32_t program_for(core::MaterialId material) const;
    uint32_t overlay_program_ = 0; // ProgramHandle.id ("unlit" + alpha blend)
    uint32_t path_program_ = 0;    // ProgramHandle.id (§8.1 path surface)
    uint32_t atlas_texture_asset_ = 0; // terrain splat atlas (engine asset id)
    uint32_t water_texture_asset_ = 0; // water surface texture (engine asset id)
    uint32_t leaf_texture_asset_ = 0;  // leaf mask atlas (engine asset id)
    /// The BARK NORMAL sheet, same layout as the mask atlas (flora bakes both
    /// from one field, so a crack in the light cannot part company with the
    /// crack in the colour). Handed to every foliage draw as
    /// DrawParams::aux_texture; 0 until the atlas is built.
    uint32_t leaf_normal_asset_ = 0;
    // Пятна построек для травы (см. set_ground_exclusions()).
    std::vector<glm::vec4> ground_exclusions_;
    // Поле классов полотна троп (см. set_path_classes()).
    PathClassField path_classes_;
    bool path_classes_set_ = false;
    // Воздух композиции ([air]): см. set_air_override().
    bool air_override_set_ = false;
    float air_fog_start_m_ = 0.0f;
    float air_fog_end_m_ = 0.0f;
    /// The terrain NORMAL atlas, same 2x2 layout as the splat atlas and baked
    /// from the same pre-ramp fields (ProcTexture::terrain_field_t) — the
    /// ground's own crack cannot part company with its shading. Handed to
    /// terrain draws (chunks + LOD) as DrawParams::aux_texture.
    uint32_t terrain_normal_asset_ = 0;
    uint32_t path_atlas_asset_ = 0;    // §8.1 path surface atlas (engine asset id)
    uint32_t water_mesh_ = 0;          // MeshHandle.id, 0 = no debug water plane
    uint32_t overlay_mesh_ = 0;        // MeshHandle.id, screen-filling quad
    uint32_t overlay_texture_ = 0;     // TextureHandle.id, re-uploaded per frame
    glm::uvec2 internal_res_{0, 0};    // overlay canvas size (internal pixels)
    // DFN_TIME/DFN_MOON freeze the sky for deterministic screenshots. Re-applied
    // every frame, not just at init: the app drives apply_sky_time from its own
    // clock now, so an init-time value would be overwritten before frame one.
    bool sky_frozen_ = false;
    float frozen_day_ = 0.5f;
    float frozen_moon_phase_ = 0.5f;
    // DFN_CLOUD pins cloud cover (0 = the pass's Rule 30 control: sheet,
    // cumulus AND ground shadows must vanish together — one field). Re-applied
    // per frame like the sky freeze so a future app-side schedule write cannot
    // overrule a screenshot pin.
    bool cloud_pinned_ = false;
    float frozen_cloud_cover_ = 0.0f;
    // DFN_VISTIME pins the visual clock (wind envelope, water scroll, cloud
    // drift): the deterministic half of the drift acceptance pair.
    bool vis_time_frozen_ = false;
    float frozen_vis_time_ = 0.0f;
    // set_visual_time's latch. Two members and not an optional: this is read
    // once per frame in the hot path and the flag is what keeps the "nobody
    // told me" case bit-identical to the wall-clock behaviour that shipped.
    bool visual_time_told_ = false;
    float told_visual_time_ = 0.0f;
    // Verification hooks for the interior shoot (Rule 27), NOT the feature.
    // DFN_TORCH=1 lights a carried flame at the CAMERA's hand position, because
    // the tour freezes the player and no gameplay entity carries a torch during
    // a screenshot run. The shipping path is components::CarriedLight, written
    // by gameplay; if any entity carries one, it wins and this hook stands down.
    bool torch_debug_ = false;
    // DFN_TORCH=2: metres AHEAD of the camera to stand the debug flame off, so
    // casters fall between eye and light and the cube map's work is visible in
    // an open-ground frame. 0 = the flame is at the hand (DFN_TORCH=1).
    float torch_ahead_m_ = 0.0f;
    // DFN_NO_POINT_SHADOW=1 keeps the lights and drops their cube maps — the
    // A/B half of the acceptance shoot, and the cost measurement.
    bool point_shadows_off_ = false;
    // DFN_DARK=<0..1> pins ambient_darkness (the app drives it in play).
    // DFN_NO_SCATTER=1 drops every scatter batch at upload (see upload_scatter):
    // the trees-off half of a landmark-silhouette A/B, and nothing else.
    // Asset ids already reported missing, so the warning fires once rather
    // than 60 times a second (see the ECS pass).
    std::unordered_set<uint32_t> warned_missing_meshes_;
    bool scatter_off_ = false;
    bool dark_frozen_ = false;
    float frozen_darkness_ = 0.0f;
    // DFN_ENV_LOG=<path>: one line per PRESENTED frame, written at the
    // set_environment() call — what the frame was actually DRAWN with, not what
    // some system computed. Between-frames motion of the lighting becomes
    // arithmetic on adjacent lines. Off unless the variable names a file; the
    // handle is closed in shutdown().
    std::FILE* env_log_ = nullptr;
    uint64_t env_log_frame_ = 0;
    MapScreen map_;
    PixelCanvas hud_;              // transparent HUD layer, drawn by the caller
    bool hud_visible_ = false;
    ScreenProp screen_prop_;       // герб меню; один кадр, см. set_screen_prop
    bool font_probe_ = false;      // DFN_FONT_PROBE: draw the glyph specimen

    // Upload accounting. GPU buffer handles are a HARD, SMALL budget (bgfx
    // hands out 4096) and every path below spends them; when the budget ran out
    // the only symptom was a crash at exit. Counted per entry point so the next
    // report says WHICH path is spending, not just that spending happened.
    struct UploadCounts {
        uint64_t terrain = 0;
        uint64_t voxel = 0;
        uint64_t scatter_chunks = 0;
        uint64_t scatter_meshes = 0;
        uint64_t failed = 0;
    } uploads_;
    bool upload_failure_reported_ = false;

    // Prints the FIRST upload failure of the run with the counts that caused
    // it. Silent afterwards.
    void report_upload_failure(const char* what);
    LodTerrain lod_; // coarse terrain beyond the streamed chunk ring
    platform::RenderEnvironment environment_{};
    std::chrono::steady_clock::time_point clock_start_{}; // visual time origin
};

} // namespace dfn::render
