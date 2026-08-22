/*
Created: 18:08:2026 - 18:08:29
Last updated: 23:08:2026 - 01:40:00
Module: engine/app
File: engine/app/sources/AppWorld.cpp

Responsibility:
- ПОДЪЁМ МИРА: App::enter_world и то, что зовётся только оттуда. Вынесено из
  App.cpp по заказу пользователя 18.08: «enter_world точно можно в другом месте
  расписать. Ф-ция на 1300 строк... надо код подужать всё равно».

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ЭТО ТОТ ЖЕ КЛАСС, ДРУГОЙ ФАЙЛ. Заголовок App.h остаётся один; разложены
  РЕАЛИЗАЦИИ. Пользователь сформулировал это прямо: «app.h нормальный по
  размерам, может по разным cpp файлам реализацию класс раскидаем?».
- ПЕРЕНОС БЕЗ ЕДИНОГО ИЗМЕНЕНИЯ ТЕЛА. Правка вместе с переносом — это две
  правки в одном доказательстве: если после такого что-то сломается, никто не
  скажет, перенос виноват или правка. Сжатие тела — отдельным заходом.
*/
/*
UPD:
- 18:08:2026 - 18:08:29: enter_world перенесена из App.cpp как есть, без изменений тела.
- 20:08:2026 - 02:00:13: unload_world() — снос предыдущего мира в НАЧАЛЕ enter_world.
  До этого повторное открытие карты из браузера оставляло резидентными чанки,
  тела переправы, стены края мира, стволы галереи, водяные бакеты и путевую
  поверхность, вешало ВТОРУЮ подписку на ChunkLoaded и заводило ВТОРОГО игрока.
  Список сноса один на два вызова: shutdown() зовёт эту же функцию.
- 20:08:2026 - 15:30:00: enter_world зовёт load_scene_houses; unload_world чистит placed_houses_.
- 20:08:2026 - 17:30:00: Дверь DFN_PLAYTEST_ARRIVE=<м>.
- 21:08:2026 - 14:35:00: ДЕРЕВО СЦЕНЫ КОЛЛАЙДИТ СТВОЛОМ, НЕ ГАБАРИТОМ. Бокс по
  всему мешу у Гилдергрина (ветки кроны живут в потоке wood) дал невидимый куб
  53х53х53 с гранью z=123.66 — бот шести прогонов Вайтрана бился именно в него;
  найден переписью тел Jolt (id=5, x94..147 y30..83 z66..123.7). Сценические
  деревья (есть листва-карты) получают ствол-бокс в поясе 0.4..2.2 с крышкой
  12 м — как деревья галерейного грида; прочие объекты — прежний бокс.
- 22:08:2026 - 14:30:00: Дверь DFN_PLAYTEST_GLANCE=<0..1> — масштаб обзорного
  качания взгляда бота (0 = ровный взгляд операторской ленты). Разбор строгий.
- 22:08:2026 - 16:20:00: паром [air] из сцены в render_system_ (clear на каждой загрузке —
  воздух прошлой карты не наследуется).
- 22:08:2026 - 20:10:00: паром облачности [air] и её печать в строке [scene] air.
- 22:08:2026 - 21:00:00: паром SceneLight.interior в ExtraLight.
- 22:08:2026 - 21:30:00: спавн композиции: y = max(земля+0.2, y файла) — вверх
  желание композитора (спавн на полу дома), вниз запрет (закопать нельзя).
  Прежний «только с земли» ставил внутренний спавн ПОД пол постройки.
- 23:08:2026 - 00:30:00: паром классов троп рельефа в render_system_.set_path_classes.
- 23:08:2026 - 01:40:00: паром коробки комнаты из [light].
*/

#include "engine/app/sources/App.h"

#include "engine/app/sources/AppInternal.h"

#include "engine/app/sources/AppSettings.h"

#include "engine/app/sources/AppDoors.h"
#include "engine/app/sources/AppHud.h"

#include "engine/app/sources/AssetBake.h"

#include "engine/app/sources/Controls.h"
#include "engine/app/sources/EditorHud.h"
#include "engine/app/sources/HudScreen.h"
#include "engine/app/sources/Localization.h"
// The object menu's pictures. Included HERE and not in App.h on purpose: only
// wire_editor_panels() names it, and App.h is already the widest header in the
// tree.
#include "engine/editor/sources/EditorPaletteThumb.h"
#include "engine/editor/sources/EditorToolPath.h"
// Generated at BUILD time by tools/stamp_build_commit.cmake; carries
// DFN_BUILD_COMMIT into every state capture. See that script for why the
// configure-time version was a defect rather than a simplification.
#include "BuildInfo.h"

#include "engine/core/components/sources/Components.h"
#include "engine/world/sources/CoarseTerrain.h"
#include "engine/world/sources/WorldgenForest.h"
#include "engine/world/sources/LayoutLoad.h"
#include "engine/world/sources/Scene.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/physics/sources/TerrainCollision.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/anim/sources/Body.h"
#include "engine/anim/sources/BodyMesh.h"
#include "engine/gameplay/sources/HeldItem.h"
#include "engine/gameplay/sources/InteractableSpawn.h"
#include "engine/gameplay/sources/InteractionSystem.h"
#include "engine/gameplay/sources/InventoryScreen.h"
#include "engine/gameplay/sources/Item.h"
#include "engine/gameplay/sources/PlayerActions.h"
#include "engine/gameplay/sources/PlayerMovement.h" // sim's confirmed stage-2 API
#include "engine/gameplay/sources/PropCollision.h"
#include "engine/gameplay/sources/ViewModel.h"
#include "engine/render/sources/BitmapFont.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/SkyModel.h"
#include "engine/render/sources/TerrainLod.h"
#include "engine/gameplay/sources/StepEvents.h"
#include "engine/gameplay/sources/StepFeel.h"
#include "engine/platform/audio/sources/miniaudio/CreateMiniaudioAudio.h"
#include "engine/platform/audio/sources/null/CreateNullAudio.h"
#include "engine/platform/input/interfaces/IInput.h"
#include "engine/platform/input/sources/glfw/CreateGlfwInput.h"
#include "engine/platform/physics/interfaces/IPhysics.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"
#include "engine/platform/physics/sources/null/CreateNullPhysics.h"
#include "engine/platform/render/interfaces/IRenderer.h"
#include "engine/platform/render/sources/bgfx/CreateBgfxRenderer.h"
#include "engine/platform/render/sources/null/CreateNullRenderer.h"
#include "engine/platform/window/interfaces/IWindow.h"
#include "engine/platform/window/sources/glfw/CreateGlfwWindow.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <limits>
#include <string_view>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace dfn::app {

// СНОС МИРА, КОТОРЫЙ УЖЕ СТОИТ. Один список на два вызова: отсюда его зовёт
// enter_world (перед тем как построить следующий), и отсюда же его зовёт
// App::shutdown (правило 32 — вторая копия разъехалась бы в тот день, когда у
// мира появится седьмая принадлежность).
//
// ПОЧЕМУ ЭТО ВООБЩЕ ПОНАДОБИЛОСЬ: enter_world зовётся НЕ ОДИН РАЗ ЗА ЗАПУСК.
// Браузер карт зовёт её на каждое открытие (open_map), и до сегодня она
// строила поверх предыдущего мира: чанки оставались резидентными, тела
// переправы и стены края мира — живыми, водяные бакеты и путевая поверхность —
// залитыми, подписка на ChunkLoaded вешалась ВТОРЫМ обработчиком (каждый чанк
// заливался дважды, первая заливка терялась в бэкенде безымянной), а
// spawn_player заводил второго игрока рядом с первым.
//
// ПЕРВЫЙ ВХОД В МИР ПРОХОДИТ ЗДЕСЬ ЖЕ И НИЧЕГО НЕ ДЕЛАЕТ: карта переправы
// пуста, дескрипторы недействительны, подписки нулевые (unsubscribe по
// неизвестному id — no-op по контракту шины), игрок null. Ни одна ветка не
// требует, чтобы мир существовал.
void App::unload_world() {
    // Готовые постройки принадлежат карте: следующая карта прочитает свои.
    // Само тело house_body_ пересоберёт первый же upload_house_mesh.
    placed_houses_.clear();
    // ТЕЛА ПЕРЕПРАВЫ — ДО ВЫГРУЗКИ ЧАНКОВ, в том же порядке, в каком это делал
    // shutdown(): обработчик ChunkUnloaded ищет их в той же карте, и пустая
    // карта для него — законное состояние.
    if (physics_ != nullptr) {
        for (auto& [key, cp] : g_chunk_physics) {
            (void)key;
            physics_->destroy_body(cp.body);
        }
        g_chunk_physics.clear();
    }
    // ВЫГРУЗКА ЧАНКОВ ТРЕБУЕТ РЕНДЕРЕРА, потому что снимает меши земли и
    // рассыпи через обработчик события — ровно то условие, под которым эта
    // пара строк стоит в shutdown().
    if (renderer_ != nullptr) {
        chunks_.unload_all(world_, bus_);
        bus_.pump();
        // ВОДА И ТРОПЫ — ЦЕЛОМИРНЫЕ, а не чанковые: их не снимает ни один
        // ChunkUnloaded, поэтому бакеты прошлой карты рисовались бы поверх
        // следующей.
        render_system_.clear_water_bodies(*renderer_);
        render_system_.clear_path_surface(*renderer_);
        render_system_.set_scene_lights({});
        render_system_.set_transient_lights({});
        render_system_.set_emissive_mesh(*renderer_, {});
    }
    // ПОДПИСКИ СНИМАЮТСЯ ПОСЛЕ ВЫГРУЗКИ, иначе снимать меши было бы некому.
    bus_.unsubscribe(chunk_loaded_sub_);
    bus_.unsubscribe(chunk_unloaded_sub_);
    bus_.unsubscribe(landed_sub_);
    chunk_loaded_sub_ = {};
    chunk_unloaded_sub_ = {};
    landed_sub_ = {};
    if (physics_ != nullptr) {
        for (platform::PhysicsBodyHandle& w : world_edge_) {
            if (w.valid()) {
                physics_->destroy_body(w);
                w = {};
            }
        }
        // СТВОЛЫ И КОЛЛАЙДЕРЫ КОМПОЗИЦИИ. Раньше их сносила ветка галереи, то
        // есть только при переходе НА галерею: уход с галереи на любой другой
        // стенд оставлял лес твёрдых стволов посреди новой карты.
        for (const platform::PhysicsBodyHandle& b : gallery_bodies_) {
            physics_->destroy_body(b);
        }
    }
    gallery_bodies_.clear();
    // ИГРОК УХОДИТ ВМЕСТЕ С МИРОМ, вместе со своей капсулой, своими сегментами
    // тела и обеими половинами вида от первого лица. spawn_view_model
    // идемпотентна по НОСИТЕЛЮ, поэтому части мёртвого носителя не мешали бы
    // новым появиться — они просто остались бы висеть и рисоваться.
    if (world_.alive(player_)) {
        anim::destroy_body(world_, player_);
        std::vector<ecs::EntityId> doomed;
        world_.view<gameplay::ViewModelPart>().each(
            [&](ecs::EntityId id, gameplay::ViewModelPart& part) {
                if (part.carrier == player_) {
                    doomed.push_back(id);
                }
            });
        for (const ecs::EntityId id : doomed) {
            world_.destroy(id);
        }
        if (auto* ps = world_.get<gameplay::PlayerState>(player_);
            ps != nullptr && physics_ != nullptr) {
            physics_->destroy_character(ps->character);
        }
        world_.destroy(player_);
        world_.flush_destroyed();
    }
    player_ = {};
    if (world_.alive(mirror_puppet_)) {
        world_.destroy(mirror_puppet_);
        world_.flush_destroyed();
    }
    mirror_puppet_ = {};
}

bool App::enter_world(uint32_t stand) {
    // ПЕРВОЙ СТРОКОЙ — СНОС ТОГО, ЧТО УЖЕ СТОИТ (см. unload_world выше).
    unload_world();
    active_stand_ = stand;
    // Chunk streaming: stage 2 serves the in-memory generated world (core's
    // open_generated path; .dfw file IO lands in stage 3). Testbed extent 4x4
    // chunks (Q45), fixed seed for reproducible screenshots (Rule 13.1).
    world::ChunkStreamingParams sp;
    sp.load_radius = static_cast<uint32_t>(config::CHUNK_LOAD_RADIUS);
    sp.unload_radius = static_cast<uint32_t>(config::CHUNK_UNLOAD_RADIUS);
    world::WorldGenParams gp;
    // THE COMPOSITION'S TERRACES MUST BE KNOWN BEFORE THE GROUND IS BUILT, so
    // the .scene is read HERE, before the generator context exists — its pads
    // are a pass of the height field, not a decoration laid on top of one. The
    // file is read once and kept; the placements below use the same document.
    //
    // A file that will not parse is reported and the map still opens on the
    // natural ground: a composition with a typo in it should be diagnosable
    // from inside the world it failed to shape.
    scene_doc_ = {};
    // Воздух прошлой карты не наследуется: карта без [air] живёт на
    // глобальных константах.
    render_system_.clear_air_override();
    if (!gallery_scene_.empty()) {
        std::string serr;
        if (!world::read_scene(gallery_scene_, scene_doc_, serr)) {
            std::fprintf(stderr, "[scene] %s: %s -- NOTHING PLACED\n",
                         gallery_scene_.c_str(), serr.c_str());
            scene_doc_ = {};
        }
        if (scene_doc_.air.set) {
            render_system_.set_air_override(scene_doc_.air.fog_start_m,
                                            scene_doc_.air.fog_end_m,
                                            scene_doc_.air.cloud_cover);
            std::fprintf(stderr, "[scene] air: fog %.0f..%.0f m, cloud %.2f\n",
                         static_cast<double>(scene_doc_.air.fog_start_m),
                         static_cast<double>(scene_doc_.air.fog_end_m),
                         static_cast<double>(scene_doc_.air.cloud_cover));
        }
        for (const world::ScenePad& P : scene_doc_.pads) {
            world::BuildingPad pad;
            pad.center = P.center;
            pad.half_extents = P.half_extents;
            pad.radius = P.radius;
            pad.blend = P.blend;
            pad.height = P.height;
            gp.composed_pads.push_back(pad);
        }
        for (const world::SceneRiver& R : scene_doc_.rivers) {
            world::RiverChannel ch;
            ch.points = R.points;
            ch.width_m = R.width_m;
            ch.depth_m = R.depth_m;
            ch.bank_m = R.bank_m;
            gp.composed_rivers.push_back(std::move(ch));
        }
        if (!scene_doc_.pads.empty()) {
            std::fprintf(stderr, "[scene] %zu authored pad(s) cut into the ground\n",
                         scene_doc_.pads.size());
        }
        // РУЧНАЯ ПРАВКА ЗЕМЛИ ЧИТАЕТСЯ ВМЕСТЕ СО СЦЕНОЙ. Ключ `relief` в .scene
        // существовал с 17.08, формат читался и писался и был проверен круговым
        // прогоном — а приложение не звало НИ read_relief, НИ write_relief ни
        // разу: холмы и тропы жили до выхода. Это чинится здесь и в SaveMap.
        relief_ = {};
        if (!scene_doc_.relief.empty()) {
            const std::filesystem::path side =
                std::filesystem::path(gallery_scene_).parent_path() / scene_doc_.relief;
            std::string rerr;
            if (world::read_relief(side, relief_, rerr)) {
                std::fprintf(stderr,
                             "[relief] %s: %zu правленых отсчётов, %zu троп\n",
                             side.string().c_str(), relief_.size(),
                             relief_.paths().size());
            } else {
                // ВСЛУХ И БЕЗ ОСТАНОВКИ КАРТЫ: потерянная правка земли иначе
                // выглядит как карта, которая сама поехала.
                std::fprintf(stderr, "[relief] %s -- ПРАВКИ НЕ ПРИМЕНЕНЫ\n",
                             rerr.c_str());
                relief_ = {};
            }
        }
        gp.composed_relief = relief_;
        // МАТЕРИАЛ ПОЛОТНА — В ЗЕМЛЮ (22.08, владелец: «тропинки опять
        // плитами кладутся, а не тропинкой каменной»). Классы троп рельефа
        // уезжают в рендер полем полилиний: мешеры пакуют класс в альфу
        // вершины, шейдер кладёт клетку путевого атласа САМОЙ землёй. Пустой
        // рельеф возвращает прежнюю упаковку — старые карты не меняются.
        {
            render::PathClassField pcf;
            pcf.strokes.reserve(relief_.paths().size());
            for (const world::ReliefPath& rp : relief_.paths()) {
                render::PathClassStroke st;
                st.points = world::relief_path_polyline(rp);
                st.half_width_m = rp.half_width_m;
                st.path_class = static_cast<uint8_t>(
                    std::clamp(rp.path_class, 0, 3));
                pcf.strokes.push_back(std::move(st));
            }
            render_system_.set_path_classes(std::move(pcf));
        }
    }
    // THE MAP IS CONTENT AND IT IS LOADED, NOT COMPILED IN (Rule 5). Core moved
    // 441 lines of ONE GAME'S survey -- Vaelmere, Ravenscar, Harrowward -- out
    // of `engine/world` and proved the asset reproduces the compiled defaults
    // exactly; this is the call that retires them. It was the largest Rule 5
    // violation in the repo and the single edit that turns "architecturally
    // reusable" into reusable, because until now the reusable engine knew the
    // name of this game's mountain.
    //
    // A FAILURE IS FATAL, DELIBERATELY. Falling back to the compiled defaults
    // would mean a missing or malformed asset produces a world that looks
    // almost right -- the fourth silent-zero of the day, and the most expensive
    // kind, because nobody would be looking for it. The file carries survey
    // coordinates and FRACTIONS only; the registry anchors and the scaling
    // transforms stay in the engine, so that moving a constant still moves the
    // world it is supposed to move (Rule 37).
    {
        const auto lr = world::load_layout_file(
            "games/daggerfall_n/assets/world/testbed_layout.json", gp.layout);
        if (!lr.ok) {
            std::fprintf(stderr, "[app] FATAL: layout asset: %s\n", lr.error.c_str());
            return false;
        }
    }
    gp.seed = 1u;
    gp.min_chunk = {0, 0};
    // 2x2 km (WORLD_EXTENT_CHUNKS 8 x CHUNK_SIZE 256), the user's direct and
    // twice-repeated request. Was a bare {3,3} here, which made the size of the
    // world unchangeable without editing source. The far-detail ladder is
    // already sized for the 10x10 km target and node ids sit on a fixed world
    // grid, so growing the world renumbers nothing already cached.
    gp.max_chunk = {static_cast<int>(config::WORLD_EXTENT_CHUNKS) - 1,
                    static_cast<int>(config::WORLD_EXTENT_CHUNKS) - 1};
    // The chosen demo map. Stand ids are core's; the app only selects.
    if (stand == static_cast<uint32_t>(world::StandId::Forest)) {
        gp.layout = world::forest_stand_layout();
    } else if (stand == static_cast<uint32_t>(world::StandId::OneTree)) {
        gp.layout = world::one_tree_stand_layout();
        // ONE chunk. The stand exists so the user can walk around a single
        // tree and name defects; seven more chunks of empty calm ground would
        // only add load time to a map whose whole point is opening instantly.
        gp.max_chunk = {0, 0};
    } else if (stand == static_cast<uint32_t>(world::StandId::Gallery)) {
        gp.layout = world::gallery_stand_layout();
        // The manifest sizes the stand: the tree gallery fits one chunk, the
        // colossus' branches reach ~120 m from its axis and need four.
        gp.max_chunk = {gallery_size_chunks_ - 1, gallery_size_chunks_ - 1};
    }
    chunks_.open_generated(gp, sp);

    // World edge (sim's finding): past the generated extent there is no terrain
    // and the player simply falls out of the world. At walking pace that took
    // minutes of deliberate effort; at sprint speed it is 20 seconds and looks
    // like a crash. Four static walls close the box until the world is bigger.
    {
        const float span = static_cast<float>(config::CHUNK_SIZE)
                         * static_cast<float>(gp.max_chunk.x - gp.min_chunk.x + 1);
        const float mid = span * 0.5f;
        const float h = 200.0f;   // tall enough that no terrain reaches over it
        const float t = 2.0f;     // wall thickness
        const glm::vec3 sides[4] = {{-t, 0.0f, mid}, {span + t, 0.0f, mid},
                                    {mid, 0.0f, -t}, {mid, 0.0f, span + t}};
        const glm::vec3 halves[4] = {{t, h, mid + t}, {t, h, mid + t},
                                     {mid + t, h, t}, {mid + t, h, t}};
        for (int i = 0; i < 4; ++i) {
            platform::StaticBoxDesc wall;
            wall.center = {sides[i].x, h * 0.5f, sides[i].z};
            wall.half_extents = halves[i];
            wall.layer = physics::LAYER_STATIC;
            world_edge_[static_cast<size_t>(i)] = physics_->create_static_box(wall);
        }
    }

    const auto wb = chunks_.water_bodies();
    // THE COMPOSITION'S OWN WATERCOURSES, appended to the generator's. Cutting
    // the channel was only half of a river: the ground below the water line is
    // marked covered and shaded as a bed, but the WATER ITSELF is a separate
    // surface built from stations, and without these lines the town's canal was
    // a dry brown ditch — which is exactly what the first frame showed.
    //
    // Sampled along each authored polyline at a fixed spacing rather than at
    // its corners: the ribbon is built between consecutive stations, so a
    // hundred-metre straight would otherwise be one enormous quad that sags
    // across the terrain it crosses.
    std::vector<math::RiverStation> rivers(wb.river_stations.begin(),
                                           wb.river_stations.end());
    std::vector<uint32_t> river_offsets(wb.river_segment_offsets.begin(),
                                        wb.river_segment_offsets.end());
    if (!scene_doc_.rivers.empty()) {
        constexpr float STATION_SPACING_M = 8.0f;
        if (river_offsets.empty()) {
            river_offsets.push_back(0);
        }
        for (const world::SceneRiver& R : scene_doc_.rivers) {
            if (R.points.size() < 2) {
                continue;
            }
            for (std::size_t i = 0; i + 1 < R.points.size(); ++i) {
                const glm::vec3 a = R.points[i];
                const glm::vec3 b = R.points[i + 1];
                const glm::vec2 a2{a.x, a.y};
                const glm::vec2 b2{b.x, b.y};
                const float len = glm::length(b2 - a2);
                const int steps = std::max(1, static_cast<int>(len / STATION_SPACING_M));
                // The last point of a segment is the first of the next, so it
                // is emitted once, by the next segment — except at the very end.
                for (int k = 0; k < steps; ++k) {
                    const float t = static_cast<float>(k) / static_cast<float>(steps);
                    math::RiverStation st;
                    st.position = a2 + (b2 - a2) * t;
                    st.surface_height = a.z + (b.z - a.z) * t;
                    st.half_width = R.width_m * 0.5f;
                    rivers.push_back(st);
                }
            }
            math::RiverStation last;
            last.position = {R.points.back().x, R.points.back().y};
            last.surface_height = R.points.back().z;
            last.half_width = R.width_m * 0.5f;
            rivers.push_back(last);
            river_offsets.push_back(static_cast<uint32_t>(rivers.size()));
        }
        std::fprintf(stderr, "[scene] %zu authored river(s), %zu station(s)\n",
                     scene_doc_.rivers.size(), rivers.size() - wb.river_stations.size());
    }
    render_system_.set_water_bodies(*renderer_, wb.lakes, rivers, river_offsets);

    // Path surfaces: whole-world, built at open, valid until re-open -- the
    // same lifetime as the water bodies above, so the same one-shot call site
    // is the right one. Empty on a stand with no paths, which is a valid
    // answer and needs no stand check.
    const auto ps = chunks_.path_surface();
    render_system_.set_path_surface(*renderer_, ps.stations, ps.route_offsets);

    // Subscribe the ferry BEFORE the first update so initial loads are seen.
    chunk_loaded_sub_ = bus_.subscribe<world::ChunkLoaded>([this](const world::ChunkLoaded& e) {
        world_changed_this_frame_ = true; // streaming quiescence, see run()
        auto view = chunks_.heightfield(e.coord);
        if (!view) {
            return;
        }
        // Terrain is DRAWN from the voxel mesh, not the heightfield. A
        // heightfield stores one height per column, so it is mathematically
        // incapable of a ceiling: inside a carve there was nothing to submit
        // at all, and a live player who walked into the barrow saw the world
        // from the inside. The heightfield upload remains as the fallback for
        // chunks that have no voxel mesh.
        // The heightfield still travels with the voxel upload, for the MAP and
        // only for the map. `note_chunk` used to hang off the heightfield path,
        // so the day terrain moved to the voxel mesh the map silently stopped
        // recording anything that HAD voxel geometry -- i.e. nearly everything.
        // An unexplored map is pixel-identical to a broken one, which is why it
        // went unnoticed for hours. Render will not re-derive one height per
        // column from a surface mesh; the app already holds the field, so the
        // app passes it.
        const auto voxel = chunks_.voxel_mesh(e.coord);
        auto sf = chunks_.surfacefield(e.coord);
        if (voxel) {
            render_system_.upload_terrain_voxel(*renderer_, *voxel, &*view,
                                                sf ? &*sf : nullptr);
        } else {
            render_system_.upload_terrain(*renderer_, *view, sf ? &*sf : nullptr);
        }
        render_system_.upload_scatter(*renderer_, {e.coord.x, e.coord.z},
                                      chunks_.scatter(e.coord));

        // Terrain collision comes from the VOXEL surface, not the heightfield:
        // a heightfield body cannot represent the crag tunnel's ceiling, so the
        // player would walk over the mountain instead of through it. An invalid
        // handle means "empty chunk, no body needed" and is not an error.
        // sim's helper sets LAYER_STATIC (hand-rolling that once left `layer`
        // at 0 — a body colliding with nothing, and the player fell through).
        if (voxel) {
            ChunkPhysics cp;
            cp.body = physics::create_terrain_mesh_body(*physics_, *voxel, 0);
            if (cp.body.valid()) {
                g_chunk_physics[pack_coord({e.coord.x, e.coord.z})] = std::move(cp);
            }
        }
    });
    chunk_unloaded_sub_ = bus_.subscribe<world::ChunkUnloaded>([this](const world::ChunkUnloaded& e) {
        world_changed_this_frame_ = true; // streaming quiescence, see run()
        render_system_.drop_terrain(*renderer_, {e.coord.x, e.coord.z});
        render_system_.drop_scatter(*renderer_, {e.coord.x, e.coord.z});
        auto it = g_chunk_physics.find(pack_coord({e.coord.x, e.coord.z}));
        if (it != g_chunk_physics.end()) {
            physics_->destroy_body(it->second.body);
            g_chunk_physics.erase(it);
        }
    });

    // Spawn at the center of chunk (0,0), on the ground. The chunk events are
    // QUEUED (post/pump), so the pump here is load-bearing: without it the
    // terrain collision bodies would not exist yet and the player would spawn
    // into empty space and fall through the world.
    const float mid = static_cast<float>(config::CHUNK_SIZE) * 0.5f;
    chunks_.update({mid, 0.0f, mid}, world_, bus_);
    bus_.pump();
    const float ground = chunks_.height_at({mid, mid}).value_or(0.0f);
    const glm::vec3 spawn{mid, ground + 0.2f, mid};
    scene_spawn_.reset(); // a previous map's composition must not follow us here
    scene_tiles_.clear();
    scene_objects_.clear();
    scene_collision_debug_.clear();
    collider_debug_ = [] {
        const char* v = door_value("DFN_DRAW_COLLIDERS");
        return v != nullptr && *v != '\0' && *v != '0';
    }();
    render_system_.set_scene_lights({});
    render_system_.set_transient_lights({});
    render_system_.set_emissive_mesh(*renderer_, {});
    {
        // ONE SEED PER MAP, from the world seed: two runs of the same map must
        // put the same mote in the same place, or no acceptance frame of a
        // night can ever be compared with another (Rule 30).
        render::FireflyParams fp;
        fp.seed = gp.seed ^ 0x5EEDF11E5ull;
        fp.world_span = static_cast<float>(config::CHUNK_SIZE)
                        * static_cast<float>(std::max(1, gallery_size_chunks_));
        fireflies_.init(fp);
    }

    // THE GALLERY'S EXHIBITS: registry objects (.dfo), read and PLACED — the
    // world generated bare ground and knows nothing about them (в1: the game
    // only reads). A row east of the spawn, one object every GALLERY_STEP_M,
    // each standing on the ground the streamer just built. Assembled into two
    // combined streams and handed to the ordinary scatter draw, so a registry
    // tree gets the same programs, atlas and wind as a scattered one — the
    // gallery judges the OBJECT, not a special-case renderer.
    if (stand == static_cast<uint32_t>(world::StandId::Gallery)) {
        namespace fs = std::filesystem;
        // A MAP WITH A COMPOSITION DOES NOT ALSO GET THE AUTO-GRID, and this
        // flag is what says so. The hole was mine: the scene branch used to
        // spawn the player and RETURN, and when the early return went (it was
        // skipping the character rig) the grid below stopped being
        // unreachable — so every composed map quietly grew a second, uninvited
        // row of every object on its shelf, standing on top of the
        // composition. A flag rather than another return: returning is exactly
        // what caused the previous bug.
        const bool composed = !gallery_scene_.empty();
        std::vector<fs::path> files;
        std::error_code gec;
        for (auto& body : gallery_bodies_) { // the previous gallery's trunks
            physics_->destroy_body(body);
        }
        gallery_bodies_.clear();
        // THE COMPOSITION FILE WINS. When the map names a .scene, the objects
        // stand where that file says — the same file dfn_scene_check judges and
        // a human edits in git. The auto-grid below stays for the shelves that
        // have no composition yet: a gallery is "show me everything on this
        // shelf", a scene is "this is the place I built".
        if (composed) {
            // ALREADY READ, above, before the ground was built: the pads had
            // to reach the generator. Reading it a second time here would be
            // a second answer to "what does this file say" — and the two could
            // differ if the file changed between the reads.
            const world::SceneDoc& doc = scene_doc_;
            // ONE MESH PER TILE, NOT ONE MESH PER MAP. The first version of
            // this loader built a single combined mesh, which was right for a
            // gallery of fourteen exhibits and WRONG the moment a real
            // composition arrived: flora's glade is 2432 placements and ~7 M
            // triangles, and one mesh means one bounding volume spanning the
            // whole map — so the frustum cull can never reject anything, the
            // LOD ladder has nothing to step on, and the GPU re-reads the
            // entire forest for every shadow pass. The renderer already culls
            // and drops PER ENTRY of its scatter map; it only ever needed the
            // scene handed to it in pieces.
            //
            // The tile is 32 m: small enough that a wall of trees behind the
            // camera is actually rejected, large enough that a 256 m map is 64
            // meshes rather than thousands of draw calls.
            scene_tiles_.clear();
            scene_objects_.clear();
            // Keyed away from the streamer's own chunk coordinates: these are
            // SCENE tiles, not world chunks, and a collision would have the
            // streamer's next bake silently drop half the composition.
            struct Tile {
                std::vector<world::Placement> parts;
            };
            std::map<std::pair<int, int>, Tile> tiles;
            render::MeshData scene_emissive;
            /// Collision triangles per tile, in WORLD space. Built once from
            /// the NEAR forms and never rebuilt: the detail ladder swaps what
            /// is drawn, and a wall that got thinner when the player walked
            /// away would be a wall he could then walk through.
            std::map<std::pair<int, int>, DebugCollision> tile_collision;
            auto& loaded = scene_objects_;
            int placed = 0;
            for (const world::Placement& p : doc.placements) {
                auto it = loaded.find(p.object);
                if (it == loaded.end()) {
                    // SEVERAL SHELVES, in the manifest's order: a town stands
                    // on building parts, street props and flora's trees at
                    // once, and one shelf per map would mean copying .dfo
                    // files between directories — which is how two copies of
                    // one object start drifting under the same name.
                    std::optional<render::RegistryObject> obj;
                    for (const std::string& shelf : gallery_shelves_) {
                        obj = render::read_object(fs::path(shelf) / (p.object + ".dfo"));
                        if (obj) {
                            break;
                        }
                    }
                    if (!obj) {
                        std::fprintf(stderr, "[scene] no object \"%s\" on any shelf "
                                             "of %s -- SKIPPED\n", p.object.c_str(),
                                     gallery_objects_dir_.c_str());
                        continue;
                    }
                    it = loaded.emplace(p.object, std::move(*obj)).first;
                }
                const render::RegistryObject& obj = it->second;
                // The whole object goes into ONE tile, chosen by its ORIGIN:
                // splitting an object across tiles would cut a tree in half at
                // a boundary and let one half be culled without the other.
                // SELF-LIT OBJECTS NEVER ENTER A TILE. Which objects those are
                // is the OBJECT'S OWN business — kind == "emissive" in the
                // .dfo — a convention in the file rather than a list in this
                // code, exactly like `-far`, so the zone that forges a flame
                // decides that it glows without asking me.
                //
                // They are kept OUT of the tiles on purpose: a tile is re-baked
                // whenever its detail form changes, and a flame appended on
                // every re-bake would multiply itself as the player walks.
                if (obj.kind == "emissive") {
                    render::append_transformed(scene_emissive, obj.wood, p.position,
                                               p.yaw, p.scale);
                    render::append_transformed(scene_emissive, obj.cards, p.position,
                                               p.yaw, p.scale);
                    render::append_transformed(scene_emissive, obj.bark, p.position,
                                               p.yaw, p.scale);
                    render::append_transformed(scene_emissive, obj.ground, p.position,
                                               p.yaw, p.scale);
                    ++placed;
                    continue;
                }
                tiles[{static_cast<int>(std::floor(p.position.x / SCENE_TILE_M)),
                       static_cast<int>(std::floor(p.position.z / SCENE_TILE_M))}]
                    .parts.push_back(p);
                // ARCHITECTURE COLLIDES BY ITS TRIANGLES, NOT BY ITS BOX.
                //
                // Everything used to get ONE axis-aligned box around its whole
                // mesh. For a tree trunk that is right. For a building it is
                // catastrophic, and the user found all three faces of it in one
                // walk: «есть стенка без двери одна, я в дверь пройти не могу,
                // она как стена тоже» (the opening was inside the box),
                // «не могу бегать по угловой крыше, у неё хитбокс что
                // прямоугольник здоровый?» (a pitched roof became a solid
                // block), «не могу ходить по лестнице» (a flight became a
                // ramp-shaped wall). One cause, three symptoms.
                //
                // Kit parts and assemblies therefore contribute their SOLID
                // TRIANGLES to a per-tile mesh body. Trees keep the trunk box
                // and the root disc on purpose: walking UNDER a canopy is the
                // point of a tree, and a triangle mesh would hang the player on
                // the first low branch.
                if (obj.kind == "part" || obj.kind == "assembly") {
                    auto& tri = tile_collision[{static_cast<int>(std::floor(p.position.x / SCENE_TILE_M)),
                                                static_cast<int>(std::floor(p.position.z / SCENE_TILE_M))}];
                    const float c = std::cos(p.yaw);
                    const float sn2 = std::sin(p.yaw);
                    const auto feed = [&](const render::MeshData& m) {
                        const uint32_t base = static_cast<uint32_t>(tri.positions.size());
                        for (const platform::Vertex& v : m.vertices) {
                            const glm::vec3 l = v.position * p.scale;
                            tri.positions.push_back({p.position.x + l.x * c + l.z * sn2,
                                                     p.position.y + l.y,
                                                     p.position.z - l.x * sn2 + l.z * c});
                        }
                        for (const uint32_t i : m.indices) {
                            tri.indices.push_back(base + i);
                        }
                    };
                    feed(obj.wood);
                    feed(obj.bark);
                    feed(obj.ground);
                    ++placed;
                    continue;
                }
                // ДЕРЕВО СЦЕНЫ (есть листва-карты): тело — СТВОЛ, не габарит.
                // У Гилдергрина деревянные ветки кроны живут в потоке wood, и
                // «бокс по всему мешу» стал невидимым кубом 53х53х53 с гранью
                // z=123.66 — бот всех прогонов бился именно в него (перепись
                // тел Jolt, 21.08). Ствол меряется в поясе 0.4..2.2 и режется
                // по высоте 12 — как у деревьев галерейного грида ниже.
                if (!obj.cards.vertices.empty()) {
                    float trunk_r = 0.15f;
                    float wood_top = 2.0f;
                    const auto girth = [&](const render::MeshData& m) {
                        for (const platform::Vertex& v : m.vertices) {
                            wood_top = std::max(wood_top, v.position.y * p.scale);
                            const float yy = v.position.y * p.scale;
                            if (yy > 0.4f && yy < 2.2f) {
                                trunk_r = std::max(
                                    trunk_r,
                                    p.scale
                                        * std::sqrt(v.position.x * v.position.x
                                                    + v.position.z * v.position.z));
                            }
                        }
                    };
                    girth(obj.bark);
                    girth(obj.wood);
                    platform::StaticBoxDesc trunk;
                    const float bh = std::min(wood_top, 12.0f);
                    trunk.center = {p.position.x, p.position.y + bh * 0.5f,
                                    p.position.z};
                    trunk.half_extents = {trunk_r * 0.75f, bh * 0.5f,
                                          trunk_r * 0.75f};
                    trunk.layer = physics::LAYER_STATIC;
                    const auto tb = physics_->create_static_box(trunk);
                    if (tb.valid()) {
                        gallery_bodies_.push_back(tb);
                    }
                    ++placed;
                    continue;
                }
                glm::vec3 lo{1e9f};
                glm::vec3 hi{-1e9f};
                const auto grow = [&](const render::MeshData& m) {
                    for (const platform::Vertex& v : m.vertices) {
                        lo = glm::min(lo, v.position);
                        hi = glm::max(hi, v.position);
                    }
                };
                grow(obj.wood);
                grow(obj.bark);
                if (hi.x < lo.x) {
                    // NOTHING SOLID TO STAND ON — a tuft of grass, a flower, a
                    // mushroom: cards only, and a card is not a wall. It gets
                    // no body, and that is correct.
                    //
                    // IT IS STILL PLACED, AND THE COUNTER MUST SAY SO. This
                    // `continue` used to skip ++placed as well, so the loader
                    // reported "2424 of 2432 standing" for a scene where all
                    // 2432 stood — and flora spent a message hunting eight
                    // objects that were never missing. A count that lies about
                    // its own success is worse than no count: it sends someone
                    // looking for a defect that is not there.
                    ++placed;
                    continue;
                }
                const float cs = std::fabs(std::cos(p.yaw));
                const float sn = std::fabs(std::sin(p.yaw));
                const glm::vec3 half = (hi - lo) * 0.5f * p.scale;
                const glm::vec3 mid_local = (hi + lo) * 0.5f * p.scale;
                platform::StaticBoxDesc box;
                box.center = {p.position.x + mid_local.x * std::cos(p.yaw)
                                  + mid_local.z * std::sin(p.yaw),
                              p.position.y + mid_local.y,
                              p.position.z - mid_local.x * std::sin(p.yaw)
                                  + mid_local.z * std::cos(p.yaw)};
                box.half_extents = {half.x * cs + half.z * sn, half.y,
                                    half.x * sn + half.z * cs};
                box.layer = physics::LAYER_STATIC;
                const auto body = physics_->create_static_box(box);
                if (body.valid()) {
                    gallery_bodies_.push_back(body);
                }
                ++placed;
            }
            // THE CHEAPER FORM OF EVERY OBJECT, if the shelf has one. The
            // convention is `<name>-far`, and its ABSENCE IS LEGAL: an object
            // with no far form simply rides its near form at every distance,
            // so old shelves and cheap props (grass, flowers — a few hundred
            // triangles) need no ladder at all.
            for (const auto& [name, obj] : std::map<std::string, render::RegistryObject>(loaded)) {
                const std::string far_name = name + "-far";
                if (loaded.count(far_name) != 0) {
                    continue;
                }
                for (const std::string& shelf : gallery_shelves_) {
                    if (auto far_obj = render::read_object(fs::path(shelf)
                                                           / (far_name + ".dfo"))) {
                        loaded.emplace(far_name, std::move(*far_obj));
                        break;
                    }
                }
            }
            std::size_t with_far = 0;
            for (const auto& [name, obj] : loaded) {
                if (name.size() > 4 && name.compare(name.size() - 4, 4, "-far") == 0) {
                    ++with_far;
                }
            }

            scene_tiles_.reserve(tiles.size());
            for (auto& [key, tile] : tiles) {
                SceneTile st;
                st.key = {SCENE_TILE_KEY_BASE + key.first, SCENE_TILE_KEY_BASE + key.second};
                st.min_xz = {static_cast<float>(key.first) * SCENE_TILE_M,
                             static_cast<float>(key.second) * SCENE_TILE_M};
                st.max_xz = st.min_xz + glm::vec2{SCENE_TILE_M, SCENE_TILE_M};
                st.parts = std::move(tile.parts);
                scene_tiles_.push_back(std::move(st));
            }
            std::size_t scene_tris = 0;
            for (SceneTile& st : scene_tiles_) {
                // Everything opens in its NEAR form. The first frames decide
                // the ladder from the real eye, one tile at a time; opening a
                // map already coarse would show the cheap forms to a player
                // standing among them.
                bake_scene_tile(st, false);
                scene_tris += st.parts.size();
            }
            std::fprintf(stderr, "[scene] %zu of %zu object(s) have a -far form\n",
                         with_far, loaded.size() - with_far);

            // THE COMPOSITION'S LAMPS. Handed to the renderer WHOLE — all of
            // them, however many — because the file says what EXISTS and the
            // renderer decides what is LIT: it keeps the nearest eight and
            // fades the eighth out as a ninth crosses it. Trimming here to the
            // budget would delete far lamps instead of dimming them, and the
            // player would walk toward a dark post that lights up when he
            // arrives.
            std::vector<render::RenderSystem::ExtraLight> lamps;
            lamps.reserve(doc.lights.size());
            std::size_t shadowing = 0;
            for (const world::SceneLight& L : doc.lights) {
                if (L.radius_m <= 0.0f) {
                    continue; // an unlit lamp is a decision, not a defect
                }
                lamps.push_back({L.position, L.color, L.radius_m,
                                 L.casts_shadow, L.interior, L.room_center,
                                 L.room_half});
                shadowing += L.casts_shadow ? 1u : 0u;
            }
            // ONE MESH BODY PER TILE. Per placement would be thousands of
            // bodies for a town; per map would be one body rebuilt whenever
            // anything changed. The tile is the same unit the drawing uses,
            // which keeps the two from disagreeing about what is where.
            std::size_t collision_tris = 0;
            for (auto& [key, tri] : tile_collision) {
                if (tri.indices.empty()) {
                    continue;
                }
                platform::TerrainMeshDesc desc;
                desc.positions = tri.positions;
                desc.indices = tri.indices;
                desc.layer = physics::LAYER_STATIC;
                const auto body = physics_->create_terrain_mesh(desc);
                if (body.valid()) {
                    gallery_bodies_.push_back(body);
                    collision_tris += tri.indices.size() / 3;
                }
                if (collider_debug_) {
                    scene_collision_debug_.push_back(std::move(tri));
                }
            }
            if (collision_tris > 0) {
                std::fprintf(stderr, "[scene] %zu collision triangle(s) in %zu "
                                     "mesh bod%s\n", collision_tris,
                             tile_collision.size(),
                             tile_collision.size() == 1 ? "y" : "ies");
            }
            render_system_.set_emissive_mesh(*renderer_, scene_emissive);
            if (!scene_emissive.indices.empty()) {
                std::fprintf(stderr, "[scene] %zu self-lit triangle(s) drawn unlit\n",
                             scene_emissive.triangle_count());
            }
            render_system_.set_scene_lights(std::move(lamps));
            if (!doc.lights.empty()) {
                std::fprintf(stderr, "[scene] %zu lamp(s), %zu asking for a shadow; "
                                     "%u light the frame, %u of those cast\n",
                             doc.lights.size(), shadowing,
                             platform::MAX_POINT_LIGHTS,
                             platform::MAX_SHADOW_POINT_LIGHTS);
            }
            std::fprintf(stderr, "[scene] %s: %d of %zu placement(s) standing, "
                                 "%zu distinct object(s), %zu tile(s)\n",
                         gallery_scene_.c_str(), placed, doc.placements.size(),
                         loaded.size(), scene_tiles_.size());
            (void)scene_tris;
            // THE COMPOSITION MAY SAY WHERE THE PLAYER STANDS. The stand only
            // knows the middle of its chunk; "the middle of the stone path,
            // facing the great oak" is a statement about what was BUILT, so it
            // lives with the build. The ground is taken from the streamer and
            // not from the file's y, so a composer who moves the spawn does not
            // have to re-measure the terrain under it — and cannot bury it.
            //
            // IT ONLY RECORDS THE WISH; the player is spawned by the ONE call
            // at the end of this function, like on every other map. The first
            // version spawned him here and RETURNED, which quietly skipped
            // everything enter_world does afterwards — the character rig above
            // all. The symptoms the user reported were "в третьем лице вообще
            // тела нет" and "не могу бегать по карте", and neither mentions a
            // scene: a shortcut that returns early does not announce which
            // twenty things it stopped doing.
            if (doc.has_spawn) {
                // ВВЕРХ — ЖЕЛАНИЕ КОМПОЗИТОРА, ВНИЗ — ЗАПРЕТ (22.08). Прежняя
                // строка брала y ТОЛЬКО с земли, и спавн «на полу таверны»
                // вставал под пол дома (пол — коллайдер постройки, не
                // террейн; цоколь 0.5 м, игрок застревал под ним — замер
                // кузнеца). max() уважает файл, когда он просит ВЫШЕ земли
                // (капсула сама осядет на пол), и по-прежнему не даёт
                // закопать: y ниже земли читается как раньше — с земли.
                const float ground_y =
                    chunks_.height_at({doc.spawn.x, doc.spawn.z}).value_or(ground)
                    + 0.2f;
                scene_spawn_ = glm::vec3{
                    doc.spawn.x, std::max(ground_y, doc.spawn.y), doc.spawn.z};
                scene_spawn_yaw_ = doc.spawn_yaw;
                std::fprintf(stderr, "[scene] spawn from the composition: "
                                     "(%.1f, %.1f, %.1f) yaw %.2f\n",
                             static_cast<double>(scene_spawn_->x),
                             static_cast<double>(scene_spawn_->y),
                             static_cast<double>(scene_spawn_->z),
                             static_cast<double>(doc.spawn_yaw));
            }
        }
        // The auto-grid shows ONE shelf: it is "show me everything here", and
        // a multi-shelf map without a composition has not said which "here".
        const std::string grid_shelf = gallery_shelves_.empty()
                                         ? gallery_objects_dir_ : gallery_shelves_.front();
        for (const auto& e :
             composed ? fs::directory_iterator() : fs::directory_iterator(grid_shelf, gec)) {
            if (e.path().extension() != ".dfo") {
                continue;
            }
            // A `-far` twin is the SAME exhibit made cheaper, not another one.
            // Showing both doubled the shelf and pushed the real objects off
            // the end of the grid.
            const std::string stem = e.path().stem().string();
            if (stem.size() > 4 && stem.compare(stem.size() - 4, 4, "-far") == 0) {
                continue;
            }
            files.push_back(e.path());
        }
        // Sorted by name: the row order must be the INDEX's order, not the
        // directory iterator's mood.
        std::sort(files.begin(), files.end());
        render::MeshData row_wood;
        render::MeshData row_cards;
        // The row is spaced by each object's OWN measured footprint, because
        // the registry holds both a birch and a settlement-scale giant: a
        // fixed step either wastes the walk or buries the birch in the
        // giant's crown. The mesh is the truth about how wide an object is.
        // A WRAPPING GRID, and it stays INSIDE the chunk (user: «твои деревья
        // с карты уходят... за границу не ставь»): rows fill eastward from the
        // spawn and wrap north when the next exhibit would cross the margin.
        // Spacing is still each object's measured footprint.
        const float world_span = static_cast<float>(config::CHUNK_SIZE)
                               * static_cast<float>(gallery_size_chunks_);
        // THE WHOLE WIDTH OF THE MAP, not the half east of the spawn. The grid
        // used to start at mid + 8 and so had 108 m of the 256 to work with;
        // three oaks of the tree gallery and several of the glade catalogue ran
        // off the end and were SKIPPED — which is how a shelf silently became a
        // shorter shelf. Rows still march NORTH of the spawn (the user's own
        // arrangement: he walks out and the exhibits are ahead of him).
        const float row_min_x = 12.0f;
        const float edge_max = world_span - 12.0f;
        float cursor = row_min_x;
        float row_z = mid - 8.0f;
        float prev_half = 0.0f;
        float row_max_half = 0.0f;
        int shown = 0;
        for (const fs::path& f : files) {
            const auto obj = render::read_object(f);
            if (!obj) {
                std::fprintf(stderr, "[gallery] %s refused (see [dfo] above)\n",
                             f.string().c_str());
                continue;
            }
            float half = 3.0f;
            for (const platform::Vertex& v : obj->wood.vertices) {
                half = std::max(half, std::max(std::fabs(v.position.x),
                                               std::fabs(v.position.z)));
            }
            for (const platform::Vertex& v : obj->cards.vertices) {
                half = std::max(half, std::max(std::fabs(v.position.x),
                                               std::fabs(v.position.z)));
            }
            float x = cursor + prev_half + half + 5.0f;
            if (files.size() == 1) {
                // One exhibit owns the map: it stands at the WORLD's centre,
                // and the walk from the spawn to it is the exhibition.
                x = world_span * 0.5f;
                row_z = world_span * 0.5f;
            }
            if (x + half > edge_max && shown > 0) {
                // Wrap north: the new row clears the tallest crown of the last.
                row_z -= row_max_half + half + 8.0f;
                cursor = row_min_x;
                prev_half = 0.0f;
                row_max_half = 0.0f;
                x = cursor + half;
            }
            // The chunk's north edge is a hard wall too; past it, refuse loudly
            // rather than plant a tree half off the world.
            if (row_z - half < 12.0f) {
                std::fprintf(stderr, "[gallery] %s SKIPPED: the grid is out of "
                                     "room inside the chunk\n", f.string().c_str());
                continue;
            }
            cursor = x;
            prev_half = half;
            row_max_half = std::max(row_max_half, half);
            const float y = chunks_.height_at({x, row_z}).value_or(ground);
            const glm::vec3 at{x, y, row_z};
            render::append_transformed(row_wood, obj->wood, at, 0.0f, 1.0f);
            render::append_transformed(row_cards, obj->cards, at, 0.0f, 1.0f);
            // Textured wood (bark + rooted ground) rides the FOLIAGE batch:
            // its albedo lives in the atlas and its wind weights are zero.
            render::append_transformed(row_cards, obj->bark, at, 0.0f, 1.0f);
            render::append_transformed(row_cards, obj->ground, at, 0.0f, 1.0f);
            // A SOLID TRUNK (user: «не давать сквозь них ходить»). The body is
            // sized from the MESH — the widest wood within reach height — the
            // same "the mesh is the truth" rule the row spacing follows. A box,
            // not the crown: walking under a canopy is the point of a tree.
            {
                float trunk_r = 0.15f;
                float wood_top = 2.0f;
                for (const platform::Vertex& v : obj->bark.vertices) {
                    wood_top = std::max(wood_top, v.position.y);
                    if (v.position.y > 0.4f && v.position.y < 2.2f) {
                        trunk_r = std::max(trunk_r,
                                           std::sqrt(v.position.x * v.position.x
                                                     + v.position.z * v.position.z));
                    }
                }
                for (const platform::Vertex& v : obj->wood.vertices) {
                    wood_top = std::max(wood_top, v.position.y);
                    if (v.position.y > 0.4f && v.position.y < 2.2f) {
                        trunk_r = std::max(trunk_r,
                                           std::sqrt(v.position.x * v.position.x
                                                     + v.position.z * v.position.z));
                    }
                }
                platform::StaticBoxDesc trunk;
                const float bh = std::min(wood_top, 12.0f);
                trunk.center = {at.x, at.y + bh * 0.5f, at.z};
                trunk.half_extents = {trunk_r * 0.75f, bh * 0.5f, trunk_r * 0.75f};
                trunk.layer = physics::LAYER_STATIC;
                const auto body = physics_->create_static_box(trunk);
                if (body.valid()) {
                    gallery_bodies_.push_back(body);
                }
                // THE ROOTS ARE SOLID TOO (user: «ствол может и физичен, а
                // корни нет») — as a LOW disc of ground, not as thin cones: a
                // walker STEPS UP onto the root spread and off again, which is
                // what boots on real roots do; per-cone bodies would be a
                // stumble field, the argument that kept them out of the solid
                // bole originally.
                // THE SPREAD IS MEASURED, NOT DERIVED. It used to be read from
                // the `ground` stream alone, and that was the bug the user hit
                // on the giant («корни у большого дерева — не физический
                // объект, прохожу насквозь»): the colossus carries its buttress
                // roots in the BARK stream, so the reach came out 0 and no body
                // was made at all. What a walker steps on is every piece of
                // wood near the ground, whichever stream drew it — so all three
                // are scanned, below ROOT_BAND_M, and flora needs no new field
                // in the .dfo for it.
                constexpr float ROOT_BAND_M = 1.0f;
                constexpr float ROOT_STEP_M = 0.25f; // half-height; <= PLAYER_STEP_HEIGHT
                float root_reach = 0.0f;
                const auto reach_of = [&](const render::MeshData& mesh, float band) {
                    for (const platform::Vertex& v : mesh.vertices) {
                        if (v.position.y > band) {
                            continue;
                        }
                        root_reach = std::max(root_reach,
                                              std::sqrt(v.position.x * v.position.x
                                                        + v.position.z * v.position.z));
                    }
                };
                // The `ground` stream IS the root spread by construction, so it
                // is taken WHOLE: a buttress that climbs two metres up the bole
                // is still a root, and banding it would measure the tree's
                // waist instead of its feet. The bole streams are banded,
                // because there the same scan would return the crown.
                reach_of(obj->ground, std::numeric_limits<float>::max());
                reach_of(obj->bark, ROOT_BAND_M);
                reach_of(obj->wood, ROOT_BAND_M);
                if (root_reach > 0.5f) {
                    platform::StaticBoxDesc roots;
                    roots.center = {at.x, at.y + ROOT_STEP_M, at.z};
                    roots.half_extents = {root_reach * 0.8f, ROOT_STEP_M, root_reach * 0.8f};
                    roots.layer = physics::LAYER_STATIC;
                    const auto rb = physics_->create_static_box(roots);
                    if (rb.valid()) {
                        gallery_bodies_.push_back(rb);
                    }
                }
                // SAY THE NUMBER, both ways. "The roots are solid" is a claim;
                // "the root disc is 8.7 m" is a measurement, and a silent zero
                // here is exactly how the giant went walk-through unnoticed.
                std::fprintf(stderr, "[gallery] %s root disc %.2f m%s\n",
                             obj->name.c_str(), static_cast<double>(root_reach * 0.8f),
                             root_reach > 0.5f ? "" : " -- NONE (no wood near the ground)");
            }
            std::fprintf(stderr, "[gallery] %s at (%.0f, %.0f) half %.1f m hash %016llx\n",
                         obj->name.c_str(), static_cast<double>(x),
                         static_cast<double>(row_z), static_cast<double>(half),
                         static_cast<unsigned long long>(obj->content_hash));
            ++shown;
        }
        if (shown > 0) {
            render_system_.upload_prebuilt_scatter(*renderer_, {0, 0}, row_wood,
                                                   row_cards);
        } else if (!composed) {
            std::fprintf(stderr, "[gallery] no readable objects in %s -- run "
                                 "dfn_forge first\n", gallery_objects_dir_.c_str());
        }
    }

    player_ = gameplay::spawn_player(world_, *physics_,
                                     scene_spawn_.value_or(spawn));
    if (!world_.alive(player_)) {
        return false;
    }
    if (scene_spawn_) {
        if (auto* ps = world_.get<gameplay::PlayerState>(player_)) {
            ps->yaw = scene_spawn_yaw_;
        }
    }
    // THE INSPECTION STAND OPENS ON ITS SUBJECT. The tree stands east of the
    // spawn (ONE_TREE_STAND_X/Z); yaw 0 looks north (forward = {sin, 0, -cos}),
    // so without this the stand opens on empty grass and the first act of every
    // inspection is hunting for the exhibit.
    // ...BUT A COMPOSITION THAT ASKED FOR A DIRECTION OUTRANKS IT. This default
    // is for a bare stand, where the auto-grid puts exhibits east of the spawn.
    // A .scene names its own spawn AND its own yaw, and it does so because the
    // author aimed it at something; overriding that made spawn_yaw a field that
    // parses, prints to the log and changes nothing — the worst kind, because
    // it reads as working. Every houses and flora stand is a Gallery, so this
    // silently ate EVERY composed yaw the project has ever written.
    if (world::stand_is_inspection(static_cast<world::StandId>(stand)) &&
        !scene_spawn_) {
        if (auto* ps = world_.get<gameplay::PlayerState>(player_)) {
            ps->yaw = glm::half_pi<float>(); // east, straight at the exhibits
        }
    }

    // TESTBED CONTENT (Rule 5 exception, same standing as the fixed seed and
    // the extent walls above): items and placements are data and move to the
    // content loader the day core's JSON reader lands. Ids follow story's
    // convention and are hashed, never spelled in C++ logic.
    //
    // This block exists because the interaction, inventory and held-item
    // systems were written, tested and NEVER CALLED by the running game --
    // which is why "рук нет и трогать нечего" was a bug report rather than a
    // feature request. Same class as the terrain ferry and the unpumped chunk
    // events: the subsystem was correct and the composition root ignored it.
    {
        gameplay::ItemDatabase items;
        gameplay::ItemDef torch;
        torch.id = {serialization::fnv1a64("item.tool.torch")};
        torch.display_name_key = "item.tool.torch.name";
        torch.light_source = true;
        torch.mesh_id = 33; // render's registry: 32 hand, 33 torch
        items.add(torch);
        world_.add_resource(std::move(items));

        world_.add(player_, gameplay::Inventory{});
        world_.add(player_, gameplay::HeldItem{});
        // A TORCH IN HAND AT SPAWN -- and this is a TESTBED CROSSBAR, marked as
        // one so nobody mistakes it for design.
        //
        // The user walked into the mountain tunnel and reported "абсолютная
        // тьма, ничего совершенно не видно". sim measured that the whole torch
        // chain works end to end -- pick up, hold, light, carried light, point
        // light in the frame -- and that the world contains exactly ONE torch,
        // a pickup two metres from the spawn, roughly 600 m from the tunnel
        // mouth, with the inventory starting empty. He went in empty-handed
        // because there was no other outcome available to him.
        //
        // In a shipped game "find something to burn" is CONTENT and belongs in
        // the dungeon's approach. Here it is the difference between a place
        // that can be played and one that cannot, so the stand hands him one.
        if (auto* inv = world_.get<gameplay::Inventory>(player_)) {
            const auto& db = world_.resource<gameplay::ItemDatabase>();
            (void)gameplay::add_item(*inv, db, torch.id, 1);
        }
        // THE VIEW MODEL'S HAND IS DECLARED ABSENT, ON PURPOSE, and this line
        // is a fix rather than a disabling.
        //
        // It used to say 32 (VIEWMODEL_MESH_ID_HAND). Nothing has ever built a
        // mesh for that id: render reserves 32..33 for a view-model mechanism
        // that does not exist yet, and `register_mesh` REFUSES the id for that
        // very reason -- so the app named an asset nobody supplies, and the
        // first-person hand has drawn as NOTHING since the day it was wired.
        // Render's loud unregistered-asset report is what surfaced it; it was
        // firing every launch, next to the `mesh_asset = 0` sentinel warning
        // that made it easy to dismiss as noise.
        //
        // 0 is the documented "none", so this states the absence instead of
        // producing it by accident, and the warning goes quiet because there is
        // nothing missing to warn about.
        //
        // WHY NOT POINT IT AT THE RIG'S HAND INSTEAD: the first-person BODY
        // already draws a real HandR, placed by the rig. A second hand placed
        // by the view model's own sway would be the same hand in two places.
        // Whether a view-model hand should exist at all now that a full body
        // does is a design question, not a wiring one -- raised, not decided
        // here. The item slot is untouched and a held torch still draws.
        world_.add_resource(gameplay::ViewModelAssets{.hand_mesh = 0});
        gameplay::spawn_view_model(world_, player_);

        // Three props, not one: take, open and use are three different verb
        // paths, and a lone pickup would leave two of them as untested in the
        // real game as they were before this block existed.
        // THE PROPS' PLACEHOLDER MESHES (sim's geometry, app ferry -- the same
        // shape as the body-segment ferry below). Without this the three demo
        // props spawn with a RenderMesh id nothing has uploaded and draw as
        // NOTHING, which is how a 1.8 x 2.0 m door stood invisible 2.5 m in
        // front of the spawn with the whole hover chain working correctly
        // around it: the ray hit its physics box, HoverTarget filled honestly,
        // and "Открыть" was drawn over empty grass.
        for (const gameplay::InteractableMesh& m : gameplay::interactable_meshes()) {
            if (!render_system_.register_mesh(*renderer_, m.mesh_asset, m.vertices,
                                              m.indices)) {
                std::fprintf(stderr,
                             "[app] interactable mesh %u refused by the registry -- "
                             "that prop will be INVISIBLE\n", m.mesh_asset);
            }
        }

        // NOT ON THE INSPECTION STAND. Its contract is "exactly one tree and
        // nothing else": the door prop spawns 2.5 m from the eye and is the
        // first thing the frame shows instead of the exhibit (measured — the
        // stand's first capture was a door filling the view, «Открыть» over
        // it, tree off-screen). The torch stays IN THE INVENTORY above: it is
        // not standing in the world.
        if (!world::stand_is_inspection(static_cast<world::StandId>(stand))) {
        gameplay::InteractableDesc take;
        take.kind = gameplay::InteractableKind::Pickup;
        // HEIGHT IS PART OF PLACEMENT, and 0.5 m was below the game. sim
        // measured it: eye at 1.7 m, prop at 0.5 m, 2.3 m away -- the crosshair
        // sits 31 degrees ABOVE both, so a player walking and looking ahead
        // never even gets the prompt. Its bot never once hovered them in 90
        // seconds for the same reason. The door, at 15.6 degrees down, was
        // always caught, which is why the complaint read as "the door works,
        // the other two do nothing" -- two different failures wearing one
        // sentence. 1.3 m is where a wall sconce and a wall lever live anyway.
        take.position = spawn + glm::vec3{2.0f, 1.45f, 0.0f};
        take.prompt_key = "prompt.take";
        take.item = torch.id;
        (void)gameplay::spawn_interactable(world_, *physics_, take);

        gameplay::InteractableDesc lever;
        lever.kind = gameplay::InteractableKind::Usable;
        lever.position = spawn + glm::vec3{-2.0f, 1.45f, 0.0f};
        lever.prompt_key = "prompt.use";
        lever.action = serialization::fnv1a64("use.testbed.lever");
        (void)gameplay::spawn_interactable(world_, *physics_, lever);

        gameplay::InteractableDesc door;
        door.kind = gameplay::InteractableKind::Openable;
        door.position = spawn + glm::vec3{0.0f, 1.0f, -2.5f};
        door.half_extents = {0.9f, 1.0f, 0.1f};
        door.prompt_key = "prompt.open";
        (void)gameplay::spawn_interactable(world_, *physics_, door);
        } // stand != OneTree (the three testbed props)
    }

    // FIRST-PERSON BODY (character's zone, wired here). Rigid segments through
    // the ordinary render path; the head MESH is hidden because the camera
    // sits inside the skull.
    body_rig_ = anim::Rig::build(anim::RigProportions::from_config());
    for (uint32_t b = 0; b < anim::BONE_COUNT; ++b) {
        const auto bone = static_cast<anim::Bone>(b);
        const auto seg = anim::build_body_segment_mesh(bone, body_rig_.proportions);
        if (!render_system_.register_mesh(*renderer_, anim::body_segment_mesh_id(bone),
                                          seg.vertices, seg.indices)) {
            std::fprintf(stderr, "[app] body segment mesh %u refused by the registry\n",
                         anim::body_segment_mesh_id(bone));
        }
    }
    anim::spawn_body(world_, player_, body_rig_, /*hide_head=*/true);

    // Landing dip rides sim's measured impact, not a guess (their event).
    landed_sub_ = bus_.subscribe<gameplay::Landed>([this](const gameplay::Landed& e) {
        anim::note_landed(world_, e.walker, e.impact_speed);
    });

    // MIRROR PUPPET (grill v11). DFN_MIRROR=1: the double stands 3 m ahead and
    // mirrors you. DFN_SHOWCASE=1: it floats and cycles the clip reel instead.
    // Placement literals live here under the testbed block's Rule 5 exception.
    {
        const char* mirror_env = door_value("DFN_MIRROR");
        const char* showcase_env = door_value("DFN_SHOWCASE");
        // THE PROBE COUNTS AS WANTING A DOUBLE, and leaving it out cost
        // character a whole shoot. `DFN_BODY_PROBE=mirror|showcase|profile|
        // plant|gait` selects the camera BEHAVIOUR and every one of those modes
        // aims at `mirror_puppet_` -- so without the puppet the probe framed an
        // empty clearing, and the resulting frame reads as "the body is not
        // drawing" rather than as "the subject was never spawned".
        //
        // THIRD SILENT ZERO OF THE DAY AND THE SAME BUG ALL THREE TIMES: a
        // PRECONDITION written as a list of the callers who happened to need it
        // when it was written. The menu skip named two env vars and four more
        // arrived; the unregistered-mesh warning named ids and the sentinel
        // arrived; this names two and the probe arrived. A list of names cannot
        // notice that a new caller has the same requirement -- only the
        // requirement can, and the requirement here is "this run aims a camera
        // at the double".
        const char* probe_env = door_value("DFN_BODY_PROBE");
        const bool want_mirror = (mirror_env && *mirror_env == '1')
                              || (showcase_env && *showcase_env == '1')
                              || (probe_env != nullptr && *probe_env != '\0');
        if (want_mirror) {
            const glm::vec3 mirror_pt = spawn + glm::vec3{0.0f, 0.0f, -3.0f};
            const auto puppet = anim::spawn_mirror_puppet(world_, body_rig_, player_,
                                                          mirror_pt, {0.0f, 1.0f});
            if (showcase_env && *showcase_env == '1') {
                if (auto* mp = world_.get<anim::MirrorPuppet>(puppet)) {
                    mp->showcase = true;
                    mp->hover_height_m = 1.2f;
                    mp->clip_seconds = 4.0f;
                }
            }
            mirror_puppet_ = puppet;
        }
    }

    // BODY PROBE (Rule 27 evidence; see App.h). The Tour freezes the tick, so
    // an animated subject cannot be photographed by it at all. Here the world
    // RUNS and the shot is triggered off simulation state.
    if (const char* bp = door_value("DFN_BODY_PROBE"); bp != nullptr && *bp != '\0') {
        BodyProbe probe;
        probe.mode = bp;
        const char* d = door_value("DFN_BODY_PROBE_DIR");
        probe.dir = d ? d : ("screenshots/body_" + probe.mode);
        std::filesystem::create_directories(probe.dir);
        probe.warmup_s = 4.0f;
        if (probe.mode == "stride" || probe.mode == "gait") {
            // The four quarters of ONE stride, in crossing order. This is the
            // Rule 27 range clause: FOOTFALL_PHASE_LEFT/RIGHT are where a foot
            // MUST be planted, and 0.0/0.5 are where one MUST be in the air —
            // a set that can only pass if the plant timing is actually right.
            probe.targets = {static_cast<float>(config::FOOTFALL_PHASE_LEFT), 0.5f,
                             static_cast<float>(config::FOOTFALL_PHASE_RIGHT), 0.0f};
            // "stride" looks down at its own feet; "gait" watches the MIRROR
            // double instead, because a walker cannot photograph its own legs
            // from inside its own skull. Same trigger, outside vantage.
            probe.pitch = probe.mode == "gait" ? -0.10f : -1.15f;
            if (const char* p = door_value("DFN_BODY_PITCH")) {
                probe.pitch = std::strtof(p, nullptr);
            }
        } else if (probe.mode == "showcase") {
            // Mid-clip of each of the six reel entries (4 s per clip); the run
            // starts one whole clip in so the warm-up cannot eat a shot.
            probe.targets = {6.0f, 10.0f, 14.0f, 18.0f, 22.0f, 26.0f};
            probe.pitch = 0.15f; // the double floats at 1.2 m
        } else if (probe.mode == "profile") {
            // THE SIDE BEARING, and it exists because the front one cannot
            // fail: a fore-aft leg scissor projects to nothing when the subject
            // faces the lens, so a frontal walk frame looks identical whether
            // the legs swing or not. The mirror double can never supply this —
            // it reflects the camera's OWN facing, so it turns to face you
            // whichever way you turn. Only the showcase double, whose facing is
            // independent, can be walked around and seen in profile.
            probe.targets = {6.0f, 10.0f, 26.0f}; // walk, run, idle
            probe.warmup_s = 5.0f;
            probe.pitch = 0.10f;
        } else if (probe.mode == "plant") {
            // THE FOOTFALL FRAME. Same side vantage, but the shots are the four
            // quarters of one walk cycle: the phase rows FOOTFALL_PHASE_LEFT
            // and _RIGHT, where a foot MUST be down, and 0.5 / 1.0, where the
            // legs MUST be passing. The clip's own clock is a pure function of
            // WALK_SPEED and the step-length pair, so the times are derived
            // here from those rows rather than typed in.
            const auto v = static_cast<float>(config::WALK_SPEED);
            const float step = static_cast<float>(config::STEP_LENGTH_BASE)
                             + static_cast<float>(config::STEP_LENGTH_PER_MPS) * v;
            const float period = 2.0f * step / v;   // seconds per full stride
            const float clip = 4.0f;                // the walk clip starts here
            probe.targets = {clip + (2.0f + static_cast<float>(config::FOOTFALL_PHASE_LEFT))
                                        * period,
                             clip + 2.5f * period,
                             clip + (2.0f + static_cast<float>(config::FOOTFALL_PHASE_RIGHT))
                                        * period,
                             clip + 3.0f * period};
            probe.warmup_s = 4.0f;
            probe.pitch = 0.10f;
        } else { // mirror
            // Turn LEFT by these offsets; the double must turn the other way.
            probe.targets = {0.0f, -0.25f, -0.5f};
            probe.direction = -1;
            probe.pitch = 0.0f;
        }
        body_probe_ = std::move(probe);
    }

    // STEP CONTEXT: who publishes, whose ground, the user's bob setting.
    step_ctx_.events = &bus_;
    step_ctx_.surface_class_at = [this](glm::vec2 xz) {
        return chunks_.surface_class_at(xz);
    };
    step_ctx_.bob_scale = config_.head_bob;

    // AUTONOMOUS PLAYTEST (sim's spec, engine/gameplay/docs/PLAYTEST.md).
    // DFN_PLAYTEST=patrol|explore|soak. The bot writes the same input intents
    // human keys write; incidents screenshot and gate the exit code.
    // A ROUTE IMPLIES A PLAYTEST. Naming waypoints and getting a still player is
    // the silent-no-op failure this harness exists to prevent: the dungeon zone
    // lost a 150-second run to exactly that, standing on the spawn while the
    // route sat parsed and unused inside a branch it never entered. There is no
    // second reading of "here is the route to walk", so the value carries the
    // intent and the mode follows it.
    const char* route_env = door_value("DFN_PLAYTEST_ROUTE");
    const bool route_given = route_env != nullptr && *route_env != '\0';
    const char* pt_env = door_value("DFN_PLAYTEST");
    if (route_given && (pt_env == nullptr || *pt_env == '\0')) {
        std::fprintf(stderr, "[playtest] DFN_PLAYTEST_ROUTE given without "
                             "DFN_PLAYTEST -- running patrol\n");
    }
    if (const char* pt = (pt_env != nullptr && *pt_env != '\0') ? pt_env
                                                                : (route_given ? "patrol" : nullptr);
        pt != nullptr && *pt != '\0') {
        gameplay::PlaytestConfig ptc;
        const std::string mode(pt);
        if (mode == "patrol") {
            ptc.mode = gameplay::BotMode::WaypointPatrol;
            // v1 route: the three testbed props and home.
            ptc.waypoints = {{spawn.x + 2.0f, spawn.z}, {spawn.x - 2.0f, spawn.z},
                             {spawn.x, spawn.z - 2.5f}, {spawn.x, spawn.z}};
            ptc.loop_waypoints = true;
        } else if (mode == "explore") {
            ptc.mode = gameplay::BotMode::RandomExplorer;
        } else {
            ptc.mode = gameplay::BotMode::Soak;
        }
        if (const char* sd = door_value("DFN_PLAYTEST_SEED")) {
            ptc.seed = std::strtoull(sd, nullptr, 10);
        }
        if (const char* sec = door_value("DFN_PLAYTEST_SECONDS")) {
            ptc.duration_seconds = std::strtof(sec, nullptr);
        }
        // DFN_PLAYTEST_GAIT=walk|jog|run. The bot already carries the gear
        // (PlaytestConfig::gait); without a door to it the harness can only
        // ever measure WALK, and every step-feel quantity is a function of
        // speed -- so the gears that are not the default are exactly the ones
        // no automated run has ever visited.
        if (const char* g = door_value("DFN_PLAYTEST_GAIT"); g != nullptr && *g != '\0') {
            //
            // AN UNKNOWN VALUE IS REFUSED OUT LOUD, not folded into walk. A
            // typo ("jgo") falling through to Walk would silently reproduce the
            // exact defect this door was opened to fix: a run that reports
            // itself as a jog measurement while measuring a walk. The default
            // is the dangerous branch here precisely because it is also the
            // correct spelling of a real gear.
            const std::string gait(g);
            if (gait == "run") {
                ptc.gait = gameplay::Gait::Run;
            } else if (gait == "jog") {
                ptc.gait = gameplay::Gait::Jog;
            } else if (gait == "walk") {
                ptc.gait = gameplay::Gait::Walk;
            } else {
                std::fprintf(stderr,
                             "[playtest] DFN_PLAYTEST_GAIT=\"%s\" is not "
                             "walk|jog|run -- REFUSING to run, because a run "
                             "that quietly measured walk would be reported as "
                             "measuring \"%s\"\n",
                             g, g);
                return false;
            }
        }
        // DFN_PLAYTEST_ROUTE="x,z;x,z;..." -- ABSOLUTE world coordinates.
        //
        // Why this door exists: patrol's route was hardwired to four points two
        // metres around the spawn, and PLAYTEST.md names patrol's own purpose as
        // "scripted acceptance walks (the crag tunnel, the castle ford)". The
        // route was always meant to be given; it simply was never exposed, so no
        // automated run has ever been INSIDE anything -- explorer picks random
        // goals, soak circles the spawn, and the dungeons sit 500 m away.
        //
        // REFUSED OUT LOUD on a malformed value, for the same reason as
        // DFN_PLAYTEST_GAIT above and one degree worse: folding a typo back to
        // the spawn ring would produce a run that reports "walked the tunnel"
        // having measured a lawn. A wrong measurement that looks like a right
        // one is the failure mode this whole harness is built against.
        if (const char* rt = door_value("DFN_PLAYTEST_ROUTE");
            rt != nullptr && *rt != '\0') {
            std::vector<glm::vec2> route;
            const std::string spec(rt);
            size_t pos = 0;
            bool ok = true;
            while (pos < spec.size() && ok) {
                const size_t end = std::min(spec.find(';', pos), spec.size());
                const std::string pair = spec.substr(pos, end - pos);
                pos = end + 1;
                if (pair.empty()) {
                    continue; // a trailing ';' is not an error
                }
                const size_t comma = pair.find(',');
                if (comma == std::string::npos) {
                    ok = false;
                    break;
                }
                char* xe = nullptr;
                char* ze = nullptr;
                const std::string xs = pair.substr(0, comma);
                const std::string zs = pair.substr(comma + 1);
                const float x = std::strtof(xs.c_str(), &xe);
                const float z = std::strtof(zs.c_str(), &ze);
                // strtof reports "no conversion" by leaving the end pointer at
                // the start -- checking that is what separates "0" from "oops".
                if (xe == xs.c_str() || ze == zs.c_str()) {
                    ok = false;
                    break;
                }
                route.push_back({x, z});
            }
            if (!ok || route.empty()) {
                std::fprintf(stderr,
                             "[playtest] DFN_PLAYTEST_ROUTE=\"%s\" is not "
                             "\"x,z;x,z;...\" -- REFUSING to run, because a run "
                             "that quietly walked the spawn ring would be "
                             "reported as walking that route\n",
                             rt);
                return false;
            }
            ptc.mode = gameplay::BotMode::WaypointPatrol;
            ptc.waypoints = std::move(route);
            ptc.loop_waypoints = true;
            std::fprintf(stderr, "[playtest] route: %zu waypoints, first (%.1f, %.1f)\n",
                         ptc.waypoints.size(),
                         static_cast<double>(ptc.waypoints.front().x),
                         static_cast<double>(ptc.waypoints.front().y));
        }
        // Точность прибытия (DFN_PLAYTEST_ARRIVE=<м>): маршруты через дверные
        // проёмы требуют дойти ДО оси проёма, а не «почти дойти».
        if (const char* am = door_value("DFN_PLAYTEST_ARRIVE"); am != nullptr && *am != '\0') {
            ptc.arrive_m = std::strtof(am, nullptr);
            if (ptc.arrive_m <= 0.0f) {
                std::fprintf(stderr, "[playtest] DFN_PLAYTEST_ARRIVE=\"%s\" is not a "
                                     "positive distance -- REFUSING to run\n", am);
                return false;
            }
        }
        // Обзорная развёртка бота (DFN_PLAYTEST_GLANCE=<0..1>): 0 — ровный
        // взгляд для операторской ленты (кадры вида города), 1 — штатное
        // качание вниз, которым бот находит предметы ниже глаз. Разбор
        // строгий: опечатка, тихо ставшая нулём, отключила бы у соака три
        // глагола из четырёх и «доказала» бы, что Take/Use не работают.
        if (const char* gl = door_value("DFN_PLAYTEST_GLANCE"); gl != nullptr && *gl != '\0') {
            char* end = nullptr;
            ptc.glance_scale = std::strtof(gl, &end);
            if (end == gl || *end != '\0' || ptc.glance_scale < 0.0f) {
                std::fprintf(stderr, "[playtest] DFN_PLAYTEST_GLANCE=\"%s\" is not a "
                                     "scale >= 0 -- REFUSING to run\n", gl);
                return false;
            }
        }
        const glm::vec4 wbz = chunks_.world_bounds_xz();
        ptc.world_min = {wbz.x + 16.0f, wbz.y + 16.0f};
        ptc.world_max = {wbz.z - 16.0f, wbz.w - 16.0f};
        playtest_ = gameplay::make_playtest(ptc);
        pt_env_.terrain_height = [this](glm::vec2 xz) { return chunks_.height_at(xz); };
        pt_env_.water_analytic = [this](glm::vec2 xz) { return chunks_.water_surface_at(xz); };
        pt_env_.water_drawn = [this](glm::vec2 xz) -> std::optional<float> {
            const auto bodies = chunks_.water_bodies();
            for (const auto& l : bodies.lakes) {
                const glm::vec2 dd = (xz - l.center) / l.half_extent;
                if (glm::dot(dd, dd) <= 1.0f) {
                    return l.surface_height;
                }
            }
            std::optional<float> best;
            float best_d = 1e9f;
            for (const auto& st : bodies.river_stations) {
                const float dist = glm::length(xz - st.position);
                if (dist <= st.half_width && dist < best_d) {
                    best_d = dist;
                    best = st.surface_height;
                }
            }
            return best;
        };
        pt_env_.world_floor_y = -60.0f; // below every legitimate carve
        const char* dir = door_value("DFN_PLAYTEST_DIR");
        pt_dir_ = dir ? dir : ("screenshots/playtest_" + mode);
        // NO create_directories HERE. playtest_write_artifacts() makes the
        // directory when it has something to put in it; making it now means
        // every run that starts the bot and writes nothing leaves an empty
        // folder behind, and those folders came back often enough that the
        // user asked for the MECHANISM to go, not the folders. A directory
        // should be evidence that something was written.
        // The bot needs the world, not the cursor; the player is NOT frozen.
    }

    {
        const auto fb = window_->framebuffer_size();
        camera_.set_projection(static_cast<float>(config::CAMERA_FOV_Y),
                               static_cast<float>(fb.x) / static_cast<float>(fb.y),
                               static_cast<float>(config::CAMERA_NEAR),
                               static_cast<float>(config::CAMERA_FAR));
    }

    // FAR DETAIL. Chunk streaming reaches CHUNK_LOAD_RADIUS chunks from wherever
    // the player stands while CAMERA_FAR is 8 km, so without this the world ends
    // a few hundred metres away in every direction. Bounds come from CORE rather
    // than from generated config, because the configured extent and the
    // generated extent have already disagreed once this stage.
    //
    // Unconditional, with DFN_NO_LOD=1 as a tooling escape rather than a user
    // setting: with far detail off the world simply stops, which is a broken
    // game and not a quality preference, and a graphics option nobody sets is
    // an untested code path.
    {
        const glm::vec4 wb = chunks_.world_bounds_xz();
        render_system_.set_world_bounds({wb.x, wb.y}, {wb.z, wb.w});
        const char* no_lod = door_value("DFN_NO_LOD");
        render_system_.set_lod_enabled(!(no_lod != nullptr && *no_lod == '1'));
    }

    if (render::Tour::enabled_by_env()) {
        const char* dir = door_value("DFN_TOUR_DIR");
        // ТОЧКИ ТУРА ДЛЯ КАРТЫ-КОМПОЗИЦИИ, выведенные ИЗ САМОЙ СЦЕНЫ.
        //
        // Заказ пользователя 18.08: «кто-то запускает демку игры на старой
        // большой карте... переделай скрипт демки так, чтобы она на карте с
        // демкой домиков запускалась». Скрипта оказалось МАЛО, и это стоит
        // записать: маршрут тура не назывался вместе с картой. Стенды публикуют
        // свои точки сами, а карта-композиция — нет, поэтому список приходил
        // ПУСТЫМ и тур молча падал на зашитый маршрут сгенерированного мира —
        // «переправа», «озёрный обрыв», «подход к хутору», координаты x 320..830
        // при карте в полсотни метров. Кадры выходили пустые: вода и небо.
        // Молчаливый откат на другой маршрут неотличим от работающего тура,
        // пока не откроешь картинку.
        //
        // Точки считаются от ОХВАТА расстановок, а не от точки появления: сцена
        // может стоять в стороне от спавна, и тогда тур снимал бы траву рядом с
        // домами. Четыре стороны света, отход в полтора охвата — чтобы дома
        // помещались в кадр целиком, — и взгляд в середину сцены.
        const auto stand_v = chunks_.stand_vantages();
        std::vector<math::StandVantage> tour_vantages(stand_v.begin(), stand_v.end());
        if (tour_vantages.empty() && !scene_doc_.placements.empty()) {
            glm::vec2 lo{std::numeric_limits<float>::max()};
            glm::vec2 hi{std::numeric_limits<float>::lowest()};
            for (const world::Placement& p : scene_doc_.placements) {
                lo = glm::min(lo, glm::vec2{p.position.x, p.position.z});
                hi = glm::max(hi, glm::vec2{p.position.x, p.position.z});
            }
            const glm::vec2 mid = 0.5f * (lo + hi);
            const float span = std::max(glm::length(hi - lo), 8.0f);
            const float back = span * 0.75f;
            struct Side {
                const char* name;
                glm::vec2 dir;
            };
            constexpr Side SIDES[] = {{"scene_south", {0.0f, 1.0f}},
                                      {"scene_west", {-1.0f, 0.0f}},
                                      {"scene_north", {0.0f, -1.0f}},
                                      {"scene_east", {1.0f, 0.0f}}};
            for (const Side& side : SIDES) {
                math::StandVantage v;
                v.label = side.name;
                v.position = mid + side.dir * back;
                v.subject = mid;
                v.eye_offset = 1.7f;
                const glm::vec2 d = v.subject - v.position;
                v.yaw = std::atan2(d.x, -d.y);
                v.pitch = -0.05f;
                tour_vantages.push_back(std::move(v));
            }
            std::fprintf(stderr,
                         "[tour] карта-композиция: %zu точек вокруг сцены "
                         "(середина %.1f, %.1f; охват %.1f м)\n",
                         tour_vantages.size(), static_cast<double>(mid.x),
                         static_cast<double>(mid.y), static_cast<double>(span));
        }
        // The stand publishes its own vantages, so a tour on the forest stand
        // photographs the forest instead of shooting one frame at a testbed
        // coordinate and stopping. An empty vantage list falls through to the
        // testbed route, so nothing about the old stand changes.
        tour_.begin(render::Tour::stand_steps(tour_vantages),
                    dir ? dir : "screenshots",
                    [this](glm::vec2 p) { return chunks_.height_at(p).value_or(0.0f); });
    } else {
        // NO UNATTENDED RUN OWNS THE MOUSE. This exemption was written for the
        // body probe alone, so every other automated door -- tour, playtest,
        // capture, restore -- still grabbed the desktop pointer and threw the
        // user into the game while he was working. The exemption belonged to
        // the PROPERTY (nobody is aiming), not to one door that happened to
        // have it.
        input_->set_cursor_captured(!unattended_run());
    }

    // A pending restore is applied LAST, once the world it describes exists and
    // the player is in it. Consumed rather than kept: re-entering a map from
    // the menu later should start fresh, not silently teleport to a pose from
    // a command line the player has long forgotten typing.
    if (restore_) {
        apply_restore(*restore_);
        restore_.reset();
    }

    // ГОТОВЫЕ ПОСТРОЙКИ КАРТЫ (секция [house], 20.08): графы из .dfh встают в
    // общие потоки и общий коллайдер постройки. После физики и сцены — им
    // нужны обе.
    load_scene_houses();

    // A DFN_TRAJ_REC run begins recording as soon as the world (and the stand it
    // stamps into the file) exists. The seed is this function's fixed worldgen
    // seed, recorded so a replay is checked against the world it was taken in.
    if (traj_rec_arm_ && !traj_rec_.active()) {
        traj_rec_.begin(active_stand_, 1u);
    }
    return true;
}

} // namespace dfn::app
