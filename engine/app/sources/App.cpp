/*
Created: 09:08:2026 - 00:45:00
Last updated: 10:08:2026 - 10:36:22
Module: engine/app
File: engine/app/sources/App.cpp

Responsibility:
- Composition root implementation: subsystem wiring, the fixed-step/interpolated
  main loop, and the chunk-event ferry (world -> render meshes + physics bodies).

Key items:
- App::init/run/shutdown; AppConfig::from_env (DFN_INTERNAL_RES, DFN_NULL_*).
- Chunk ferry: on ChunkLoaded converts uint16 heightfield to the float buffer
  TerrainDesc expects (kept alive per chunk until ChunkUnloaded).

Dependencies:
- Uses: platform factories (glfw/bgfx/jolt + null), core, world, render, gameplay.
- Used by: main.cpp.

Notes:
- Sim-zone seam: PlayerMovementSystem API per sim's stage-2 report; the three
  call sites are marked SIM-SEAM and adjusted at integration.
- Tour finished (post_frame == true) requests window close (render's contract).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Created for stage 2 integration.
- 09:08:2026 - 00:48:00: Adopted sim's confirmed free-function movement API
                         (spawn_player / accumulate_input / pre_step / post_step);
                         camera poses read from the player entity's components.
- 09:08:2026 - 10:16:00: ChunkCoord uses x/z (not x/y) — ferry fixed.
- 09:08:2026 - 10:20:00: Switched to core's open_generated() (open(file) is
                         stage 3) — the actual init failure after reboot.
- 09:08:2026 - 10:32:00: Tour vantages offset by spawn ground height (render's
                         underground-camera diagnosis; Rule 26 ack recorded).
- 09:08:2026 - 10:48:00: DFN_PALETTE=1 wired to RendererInitParams.palette_post
                         (stage-3 render batch, Rule 26).
- 09:08:2026 - 12:05:00: Stage-3b ferry: surface-aware terrain upload, scatter
                         upload/drop, per-body water; Tour v3 (testbed steps,
                         per-frame ground resolution, tour-driven streaming
                         focus, frozen player during tour).
- 09:08:2026 - 12:49:12: settings.cfg (graphics settings — user decision, sync #3):
                         internal_resolution + palette read from file,
                         auto-generated on first run; env overrides intact.
- 09:08:2026 - 15:07:13: CRITICAL fix (user report: fell through the world on
                         launch): terrain bodies were built here with
                         TerrainDesc::layer left at 0 — colliding with nothing.
                         Ferry now uses physics::create_terrain_body (owns the
                         decode and LAYER_STATIC). Also: pump the chunk events
                         before spawning, and stream before stepping.
- 09:08:2026 - 16:59:02: Voxel world: terrain collision ferried from
                         ChunkManager::voxel_mesh via create_terrain_mesh_body
                         (heightfield bodies cannot carry the tunnel ceiling).
- 09:08:2026 - 17:16:27: World-edge walls: past the generated extent there is
                         no terrain, and sprint speed made falling out of the
                         world a 20-second accident (sim's finding).
- 09:08:2026 - 17:32:38: Map screen wired: M toggles it, cursor released while
                         open, canvas told the internal resolution.
- 09:08:2026 - 19:12:24: Day/night clock wired: 48-minute day, T holds for a
                         50x debug run, lunar phase as a pure function of date.
- 09:08:2026 - 19:21:01: Terrain DRAWN from the voxel mesh (render's finding:
- 09:08:2026 - 20:27:13: ambient_darkness written per frame — a stand-in for
- 09:08:2026 - 20:38:09: ambient_darkness now asks core's enclosure query;
                         the app-side stand-in is deleted.
                         core's enclosure query so interiors are dark in play.
                         there was no voxel render path at all, so carves were
                         never submitted — the reported "saw the map from
                         inside the barrow" was missing geometry, not light).
- 09:08:2026 - 22:34:17: Взаимодействие подключено к игре: предметы, три пробных объекта (взять/открыть/использовать), столкновения с реквизитом, наведение, действия, переносимый свет, модель рук. Всё это существовало и не вызывалось ни разу.
- 09:08:2026 - 22:38:29: Настоящие номера моделей руки (32) и факела (33) вместо заглушек — render их завёл.
- 09:08:2026 - 22:47:13: Карта снова записывает разведанное: высотное поле едет вместе с воксельной выгрузкой (пометка кусков висела на старом пути и молча отвалилась). Плюс новая сигнатура действий игрока — выбрасывание предметов требует физики.
- 09:08:2026 - 22:49:12: Мир встаёт на паузу с открытым инвентарём (как в TES). Три системы продолжают работать — иначе из меню не выйти. Накопитель шагов сбрасывается, чтобы на выходе не выстрелить пачкой догоняющих тиков.
- 09:08:2026 - 23:30:34: Мир стал 2×2 км (WORLD_EXTENT_CHUNKS 8) — прямая просьба пользователя. Размер мира перестал быть голым числом в исходнике.
- 09:08:2026 - 23:50:20: Ферри дальней детализации: границы мира от core, прямоугольник по сетке чанков, обновление по КАДРОВОМУ времени, сбор по ожидающим узлам, меш уничтожается раньше поля.
- 10:08:2026 - 00:04:04: Подсказки взаимодействия рисуются на экране. Первый настоящий текст в игре: таблица строк грузится из данных, промах даёт заметную заглушку, а не пустоту.
- 10:08:2026 - 02:44:09: Большая проводка ландшафтного этапа: аудио (слушатель, ветер, шаги), контекст шага, тело от первого лица (ферри BodyDrive от часов шага sim), зеркальный двойник (DFN_MIRROR/DFN_SHOWCASE), автономный плейтест (DFN_PLAYTEST), связь угла обзора со скоростью, строка head_bob в настройках.
- 10:08:2026 - 10:36:22: Запуск через МЕНЮ: init() поднимает движок, enter_world() строит выбранную демо-карту. Стартовый экран, выбор карты, пауза по Esc. DFN_MENU=0/DFN_MAP для инструментов; тур и плейтест выключают меню сами.
*/

#include "engine/app/sources/App.h"

#include "engine/app/sources/Localization.h"

#include "engine/core/components/sources/Components.h"
#include "engine/world/sources/CoarseTerrain.h"
#include "engine/world/sources/WorldgenForest.h"
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
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace dfn::app {

namespace {

// Per-chunk physics state owned by the ferry: TerrainDesc does not promise the
// backend copies the height data, so the float conversion buffer stays alive
// for the lifetime of the body.
struct ChunkPhysics {
    std::vector<float> heights;
    platform::PhysicsBodyHandle body{};
};

uint64_t pack_coord(glm::ivec2 c) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(c.x)) << 32)
         | static_cast<uint64_t>(static_cast<uint32_t>(c.y));
}

} // namespace

// Ferry state lives here rather than in the header to keep App.h light.
static std::unordered_map<uint64_t, ChunkPhysics> g_chunk_physics;

namespace {

constexpr const char* SETTINGS_PATH = "settings.cfg";

// Reads key=value graphics settings; writes a commented default file on first
// run so the user always has something to edit (sync #3 decision: resolution
// and palette are user settings, not constants).
void load_or_create_settings(AppConfig& cfg) {
    std::ifstream in(SETTINGS_PATH);
    if (!in.is_open()) {
        std::ofstream out(SETTINGS_PATH);
        out << "# Daggerfall N graphics settings (auto-generated; edit freely)\n"
            << "# internal_resolution: rendering pixel grid, integer-upscaled to the\n"
            << "#   window. Presets: 640x360 (fine retro), 320x180 (chunky Daggerfall).\n"
            << "internal_resolution=" << cfg.internal_width << 'x' << cfg.internal_height
            << "\n"
            << "# palette: 1 = 64-color quantization + dithering (DOS look), 0 = off.\n"
            << "palette=" << (cfg.palette_post ? 1 : 0) << "\n"
            << "# head_bob: bob/dip/settle motion scale; 0 disables the motion\n"
            << "# entirely (footstep sound and animation still fire).\n"
            << "head_bob=" << cfg.head_bob << "\n"
            << "# show_menu: 1 = start in the menu and pick a demo map,\n"
            << "#            0 = drop straight into the world.\n"
            << "show_menu=" << (cfg.show_menu ? 1 : 0) << '\n';
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);
        if (key == "internal_resolution") {
            unsigned w = 0, h = 0;
            if (std::sscanf(value.c_str(), "%ux%u", &w, &h) == 2 && w > 0 && h > 0) {
                cfg.internal_width = w;
                cfg.internal_height = h;
            }
        } else if (key == "palette") {
            cfg.palette_post = !value.empty() && value[0] == '1';
        } else if (key == "show_menu") {
            cfg.show_menu = !value.empty() && value[0] == '1';
        } else if (key == "head_bob") {
            float v = 1.0f;
            if (std::sscanf(value.c_str(), "%f", &v) == 1 && v >= 0.0f && v <= 2.0f) {
                cfg.head_bob = v;
            }
        }
    }
}

} // namespace

AppConfig AppConfig::from_env() {
    AppConfig cfg;
    cfg.internal_width = static_cast<uint32_t>(config::INTERNAL_RES_W);
    cfg.internal_height = static_cast<uint32_t>(config::INTERNAL_RES_H);
    load_or_create_settings(cfg); // file first; env below overrides (tooling)
    if (const char* res = std::getenv("DFN_INTERNAL_RES")) {
        unsigned w = 0, h = 0;
        if (std::sscanf(res, "%ux%u", &w, &h) == 2 && w > 0 && h > 0) {
            cfg.internal_width = w;
            cfg.internal_height = h;
        }
    }
    if (const char* nr = std::getenv("DFN_NULL_RENDER"); nr && nr[0] == '1') {
        cfg.use_null_renderer = true;
    }
    if (const char* mn = std::getenv("DFN_MENU")) {
        cfg.show_menu = (mn[0] == '1');
    }
    if (const char* mp = std::getenv("DFN_MAP")) {
        const std::string m(mp);
        if (m == "forest") {
            cfg.start_stand = 1;
        } else if (m == "testbed" || m == "valley") {
            cfg.start_stand = 0;
        } else {
            cfg.start_stand = static_cast<uint32_t>(std::strtoul(mp, nullptr, 10));
        }
    }
    // Tooling never stops at a menu: nobody is there to press Enter, and a
    // tour that screenshots a menu is a tour that verified nothing.
    if (std::getenv("DFN_TOUR") != nullptr || std::getenv("DFN_PLAYTEST") != nullptr) {
        cfg.show_menu = false;
    }
    if (const char* na = std::getenv("DFN_NULL_AUDIO"); na && na[0] == '1') {
        cfg.use_null_audio = true;
    }
    if (const char* np = std::getenv("DFN_NULL_PHYSICS"); np && np[0] == '1') {
        cfg.use_null_physics = true;
    }
    if (const char* pal = std::getenv("DFN_PALETTE"); pal && pal[0] == '1') {
        cfg.palette_post = true;
    }
    return cfg;
}

App::App() : timestep_(config::SIM_DT, static_cast<uint32_t>(config::SIM_MAX_CATCHUP_STEPS)) {}

App::~App() = default;

bool App::init(const AppConfig& config) {
    config_ = config;

    window_ = platform::create_glfw_window();
    platform::WindowInitParams wp;
    wp.width = config.window_width;
    wp.height = config.window_height;
    wp.title = "Daggerfall N"; // bootstrap exception: replaced by loc lookup (sync #2 note)
    if (!window_ || !window_->init(wp)) {
        return false;
    }
    input_ = platform::create_glfw_input(*window_);

    renderer_ = config.use_null_renderer ? platform::create_null_renderer()
                                         : platform::create_bgfx_renderer();
    platform::RendererInitParams rp;
    rp.native_window_handle = window_->native_handle();
    rp.framebuffer_width = window_->framebuffer_size().x;
    rp.framebuffer_height = window_->framebuffer_size().y;
    rp.internal_width = config.internal_width;
    rp.internal_height = config.internal_height;
    rp.palette_post = config.palette_post;
    if (!renderer_ || !renderer_->init(rp)) {
        return false;
    }

    physics_ = config.use_null_physics ? platform::create_null_physics()
                                       : platform::create_jolt_physics();
    if (!physics_ || !physics_->init()) {
        return false;
    }

    // Audio: no device is a MODE, not an error (Rule 3) -- the game runs
    // silent-but-correct on the null backend.
    audio_ = config.use_null_audio ? platform::create_null_audio()
                                   : platform::create_miniaudio_audio();
    if (!audio_ || !audio_->init()) {
        audio_ = platform::create_null_audio();
        (void)audio_->init();
    }
    sfx_bus_ = audio_->create_bus({});
    sound_bank_ = gameplay::load_step_sound_bank(
        *audio_, "games/daggerfall_n/assets/audio", sfx_bus_);
    gameplay::wire_step_audio(bus_, *audio_, sound_bank_);
    wind_loop_ = gameplay::start_wind_loop(*audio_, sound_bank_);

    if (!render_system_.init(*renderer_)) {
        return false;
    }
    // The map canvas rasterizes in internal-resolution pixels, so it must know
    // the settings.cfg-driven resolution to stay pixel-exact (render's note).
    render_system_.set_internal_resolution(config.internal_width, config.internal_height);

    // Rule 5: every user-facing string comes from here and nowhere else.
    // A missing file is loud and the game still runs, with every string drawn
    // as a visible placeholder rather than as nothing.
    (void)load_localization("games/daggerfall_n/assets/localization/ru.txt");

    // The demo-map table. Adding a stand is one row here plus two localization
    // lines -- the menu itself never changes, which is the point of a table.
    // Stand ids belong to core's WorldGenParams; 0 is today's valley.
    menu_.set_maps({{static_cast<uint32_t>(world::StandId::Testbed),
                     "map.valley.name", "map.valley.blurb"},
                    {static_cast<uint32_t>(world::StandId::Forest),
                     "map.forest.name", "map.forest.blurb"}});
    {
        const auto fb = window_->framebuffer_size();
        camera_.set_projection(static_cast<float>(config::CAMERA_FOV_Y),
                               static_cast<float>(fb.x) / static_cast<float>(fb.y),
                               static_cast<float>(config::CAMERA_NEAR),
                               static_cast<float>(config::CAMERA_FAR));
    }

    // The world itself is NOT built here. Menu-first launch means the player
    // picks a demo map before any terrain exists, so world construction lives
    // in enter_world() and init() only raises the engine.
    if (config.show_menu) {
        mode_ = AppMode::Menu;
        input_->set_cursor_captured(false);
    } else {
        if (!enter_world(config.start_stand)) {
            return false;
        }
    }
    return true;
}

// Builds (or rebuilds) the world for one demo map. Everything that depends on
// terrain existing lives here: streaming, edge walls, the chunk ferry, the
// player, the testbed content, the body, the mirror puppet and the playtest.
bool App::enter_world(uint32_t stand) {
    active_stand_ = stand;
    // Chunk streaming: stage 2 serves the in-memory generated world (core's
    // open_generated path; .dfw file IO lands in stage 3). Testbed extent 4x4
    // chunks (Q45), fixed seed for reproducible screenshots (Rule 13.1).
    world::ChunkStreamingParams sp;
    sp.load_radius = static_cast<uint32_t>(config::CHUNK_LOAD_RADIUS);
    sp.unload_radius = static_cast<uint32_t>(config::CHUNK_UNLOAD_RADIUS);
    world::WorldGenParams gp;
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
    render_system_.set_water_bodies(*renderer_, wb.lakes, wb.river_stations,
                                    wb.river_segment_offsets);

    // Subscribe the ferry BEFORE the first update so initial loads are seen.
    bus_.subscribe<world::ChunkLoaded>([this](const world::ChunkLoaded& e) {
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
    bus_.subscribe<world::ChunkUnloaded>([this](const world::ChunkUnloaded& e) {
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

    player_ = gameplay::spawn_player(world_, *physics_, spawn);
    if (!world_.alive(player_)) {
        return false;
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
        world_.add_resource(gameplay::ViewModelAssets{.hand_mesh = 32});
        gameplay::spawn_view_model(world_, player_);

        // Three props, not one: take, open and use are three different verb
        // paths, and a lone pickup would leave two of them as untested in the
        // real game as they were before this block existed.
        gameplay::InteractableDesc take;
        take.kind = gameplay::InteractableKind::Pickup;
        take.position = spawn + glm::vec3{2.0f, 0.5f, 0.0f};
        take.prompt_key = "prompt.take";
        take.item = torch.id;
        (void)gameplay::spawn_interactable(world_, *physics_, take);

        gameplay::InteractableDesc lever;
        lever.kind = gameplay::InteractableKind::Usable;
        lever.position = spawn + glm::vec3{-2.0f, 0.5f, 0.0f};
        lever.prompt_key = "prompt.use";
        lever.action = serialization::fnv1a64("use.testbed.lever");
        (void)gameplay::spawn_interactable(world_, *physics_, lever);

        gameplay::InteractableDesc door;
        door.kind = gameplay::InteractableKind::Openable;
        door.position = spawn + glm::vec3{0.0f, 1.0f, -2.5f};
        door.half_extents = {0.9f, 1.0f, 0.1f};
        door.prompt_key = "prompt.open";
        (void)gameplay::spawn_interactable(world_, *physics_, door);
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
    bus_.subscribe<gameplay::Landed>([this](const gameplay::Landed& e) {
        anim::note_landed(world_, e.walker, e.impact_speed);
    });

    // MIRROR PUPPET (grill v11). DFN_MIRROR=1: the double stands 3 m ahead and
    // mirrors you. DFN_SHOWCASE=1: it floats and cycles the clip reel instead.
    // Placement literals live here under the testbed block's Rule 5 exception.
    {
        const char* mirror_env = std::getenv("DFN_MIRROR");
        const char* showcase_env = std::getenv("DFN_SHOWCASE");
        const bool want_mirror = (mirror_env && *mirror_env == '1')
                              || (showcase_env && *showcase_env == '1');
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
    if (const char* bp = std::getenv("DFN_BODY_PROBE"); bp != nullptr && *bp != '\0') {
        BodyProbe probe;
        probe.mode = bp;
        const char* d = std::getenv("DFN_BODY_PROBE_DIR");
        probe.dir = d ? d : ("screenshots/body_" + probe.mode);
        std::filesystem::create_directories(probe.dir);
        probe.warmup_s = 4.0f;
        if (probe.mode == "stride") {
            // The four quarters of ONE stride, in crossing order. This is the
            // Rule 27 range clause: FOOTFALL_PHASE_LEFT/RIGHT are where a foot
            // MUST be planted, and 0.0/0.5 are where one MUST be in the air —
            // a set that can only pass if the plant timing is actually right.
            probe.targets = {static_cast<float>(config::FOOTFALL_PHASE_LEFT), 0.5f,
                             static_cast<float>(config::FOOTFALL_PHASE_RIGHT), 0.0f};
            probe.pitch = -1.15f; // look at your own feet
            if (const char* p = std::getenv("DFN_BODY_PITCH")) {
                probe.pitch = std::strtof(p, nullptr);
            }
        } else if (probe.mode == "showcase") {
            // Mid-clip of each of the six reel entries (4 s per clip).
            probe.targets = {2.0f, 6.0f, 10.0f, 14.0f, 18.0f, 22.0f};
            probe.pitch = 0.15f; // the double floats at 1.2 m
        } else { // mirror
            // Turn LEFT by these offsets; the double must turn the other way.
            probe.targets = {0.0f, -0.25f, -0.5f};
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
    if (const char* pt = std::getenv("DFN_PLAYTEST"); pt != nullptr && *pt != '\0') {
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
        if (const char* sd = std::getenv("DFN_PLAYTEST_SEED")) {
            ptc.seed = std::strtoull(sd, nullptr, 10);
        }
        if (const char* sec = std::getenv("DFN_PLAYTEST_SECONDS")) {
            ptc.duration_seconds = std::strtof(sec, nullptr);
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
        const char* dir = std::getenv("DFN_PLAYTEST_DIR");
        pt_dir_ = dir ? dir : ("screenshots/playtest_" + mode);
        std::filesystem::create_directories(pt_dir_);
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
        const char* no_lod = std::getenv("DFN_NO_LOD");
        render_system_.set_lod_enabled(!(no_lod != nullptr && *no_lod == '1'));
    }

    if (render::Tour::enabled_by_env()) {
        const char* dir = std::getenv("DFN_TOUR_DIR");
        tour_.begin(render::Tour::testbed_steps(), dir ? dir : "screenshots",
                    [this](glm::vec2 p) { return chunks_.height_at(p).value_or(0.0f); });
    } else {
        // The body probe drives the look itself; grabbing the cursor for it
        // would only steal the desktop's pointer for the length of the run.
        input_->set_cursor_captured(!body_probe_.has_value());
    }
    return true;
}

namespace {

// Did the swept interval a -> b pass `target`? `wrapping` treats the values as
// a cycle in [0,1) advancing forward (the stride phase); otherwise a plain
// interval test that also fires when an endpoint IS the target.
[[nodiscard]] bool swept_past(float a, float b, float target, bool wrapping) {
    if (wrapping) {
        const float span = std::fmod(b - a + 1.0f, 1.0f);
        const float to_target = std::fmod(target - a + 1.0f, 1.0f);
        return span > 0.0f && to_target <= span;
    }
    return (a - target) * (b - target) <= 0.0f;
}

} // namespace

void App::body_probe_drive() {
    if (!body_probe_) {
        return;
    }
    BodyProbe& p = *body_probe_;
    p.elapsed_s += static_cast<float>(config::SIM_DT);
    auto* ps = world_.get<gameplay::PlayerState>(player_);
    if (ps == nullptr) {
        return;
    }
    if (p.mode == "stride") {
        // The bot walks; the probe only aims the eye at its own feet.
        ps->pitch = p.pitch;
        return;
    }
    // Showcase and mirror: stand still, face the double. The mirror run then
    // turns LEFT at a fixed rate, and the double must answer to ITS left.
    ps->move_axes = {0.0f, 0.0f};
    ps->run = false;
    ps->pending_look = {0.0f, 0.0f};
    if (!p.aimed) {
        const auto* self = world_.get<components::Transform>(player_);
        const auto* other = world_.get<components::Transform>(mirror_puppet_);
        if (self != nullptr && other != nullptr) {
            const glm::vec2 d{other->position.x - self->position.x,
                              other->position.z - self->position.z};
            if (glm::dot(d, d) > 1.0e-6f) {
                p.aim_yaw = std::atan2(d.x, -d.y);
            }
        }
        p.aimed = p.elapsed_s >= p.warmup_s;
    }
    float offset = 0.0f;
    if (p.mode == "mirror" && p.elapsed_s > p.warmup_s) {
        offset = std::max(-0.5f, -0.25f * (p.elapsed_s - p.warmup_s));
    }
    ps->yaw = p.aim_yaw + offset;
    ps->pitch = p.pitch;
}

void App::body_probe_frame(float alpha, float frame_dt) {
    if (!body_probe_ || renderer_ == nullptr) {
        return;
    }
    (void)alpha;
    (void)frame_dt;
    BodyProbe& p = *body_probe_;
    if (p.next >= p.targets.size() || p.elapsed_s > p.warmup_s + 60.0f) {
        if (!p.log.empty()) {
            if (std::FILE* f = std::fopen((p.dir + "/probe_log.txt").c_str(), "w")) {
                std::fwrite(p.log.data(), 1, p.log.size(), f);
                std::fclose(f);
            }
            p.log.clear();
        }
        window_->request_close();
        return;
    }
    if (p.cooldown > 0) {
        --p.cooldown;
        return;
    }

    // What this probe is watching, and the line the shot must land on.
    float now = 0.0f;
    bool wrapping = false;
    float speed = 0.0f;
    float step_len = 0.0f;
    if (p.mode == "stride") {
        const auto* drive = world_.get<anim::BodyDrive>(player_);
        if (drive == nullptr) {
            return;
        }
        now = drive->stride_phase;
        speed = drive->speed_mps;
        step_len = drive->step_length_m;
        wrapping = true;
        if (speed < 0.5f || !drive->grounded) {
            p.prev_value = now; // a standing frame proves nothing about a stride
            return;
        }
    } else if (p.mode == "showcase") {
        const auto* drive = world_.get<anim::BodyDrive>(mirror_puppet_);
        if (drive == nullptr) {
            return;
        }
        now = drive->showcase_time_s;
    } else {
        const auto* ps = world_.get<gameplay::PlayerState>(player_);
        if (ps == nullptr) {
            return;
        }
        now = ps->yaw - p.aim_yaw;
    }
    if (p.elapsed_s < p.warmup_s) {
        p.prev_value = now;
        return;
    }

    // The backend captures into the NEXT rendered frame, so the target is
    // tested against where the subject will BE, not where it is.
    const float delta = wrapping ? std::fmod(now - p.prev_value + 1.0f, 1.0f)
                                 : now - p.prev_value;
    const float predicted = wrapping ? std::fmod(now + delta, 1.0f) : now + delta;
    p.prev_value = now;
    if (!p.primed) {
        p.value = predicted;
        p.primed = true;
        return;
    }
    const float before = p.value;
    p.value = predicted;
    if (!swept_past(before, predicted, p.targets[p.next], wrapping)) {
        return;
    }

    char name[96];
    std::snprintf(name, sizeof(name), "%02zu_%s_%.3f.png", p.next, p.mode.c_str(),
                  static_cast<double>(p.targets[p.next]));
    (void)renderer_->save_screenshot(p.dir + "/" + name);
    char line[256];
    std::snprintf(line, sizeof(line),
                  "%s target=%.3f captured=%.3f speed=%.2f step=%.2f t=%.1f\n", name,
                  static_cast<double>(p.targets[p.next]),
                  static_cast<double>(predicted), static_cast<double>(speed),
                  static_cast<double>(step_len), static_cast<double>(p.elapsed_s));
    p.log += line;
    std::fprintf(stderr, "[body_probe] %s", line);
    ++p.next;
    p.cooldown = 4; // let the backend flush before another shot is scheduled
}

int App::run() {
    auto last = std::chrono::steady_clock::now();
    while (!window_->should_close()) {
        window_->poll_events();
        input_->update();

        // MENU MODE: the engine is up, the world may not exist yet. Nothing
        // simulates here -- the menu is drawn over whatever the last frame was
        // (a dimmed world when paused, a plain ground before any world).
        if (mode_ == AppMode::Menu) {
            if (input_->was_pressed(platform::Key::UP)) {
                menu_.move(-1);
            }
            if (input_->was_pressed(platform::Key::DOWN)) {
                menu_.move(1);
            }
            MenuAction action = MenuAction::None;
            if (input_->was_pressed(platform::Key::ENTER)) {
                action = menu_.activate();
            } else if (input_->was_pressed(platform::Key::ESCAPE)) {
                action = menu_.back();
            }
            switch (action) {
            case MenuAction::EnterWorld:
                if (!enter_world(menu_.chosen_stand())) {
                    return 1;
                }
                mode_ = AppMode::Playing;
                input_->set_cursor_captured(true);
                break;
            case MenuAction::Resume:
                mode_ = AppMode::Playing;
                input_->set_cursor_captured(true);
                break;
            case MenuAction::Quit:
                window_->request_close();
                break;
            case MenuAction::ToRoot:
            case MenuAction::None:
                break;
            }
            if (window_->should_close()) {
                break;
            }
            // Drawn through the PUBLIC hud layer rather than a new render API:
            // clear() writes alpha 255, so a fully cleared canvas covers the
            // frame exactly like an opaque screen, and the pause page clears
            // transparent to keep the world visible underneath.
            draw_menu(render_system_.hud(), menu_);
            render_system_.set_hud_visible(true);
            render_system_.render(world_, *renderer_, camera_, 0.0f);
            // VERIFICATION HOOK (Rule 27): a menu nobody can photograph is a
            // menu nobody can verify. DFN_MENU_SHOT=<path> captures one frame
            // of whichever page is showing and closes.
            if (const char* shot = std::getenv("DFN_MENU_SHOT");
                shot != nullptr && *shot != '\0') {
                // The backend captures AFTER the current end_frame and needs a
                // few frames to flush (the tour learned this the hard way), so
                // shoot once and keep drawing until the flush lands.
                if (menu_shot_frames_ == 0) {
                    (void)renderer_->save_screenshot(shot);
                }
                if (++menu_shot_frames_ > 4) {
                    window_->request_close();
                }
            }
            last = std::chrono::steady_clock::now(); // no frame_dt spike on resume
            continue;
        }
        if (window_->consume_resize()) {
            const auto fb = window_->framebuffer_size();
            renderer_->resize(fb.x, fb.y);
            camera_.set_projection(camera_.fov_y(),
                                   static_cast<float>(fb.x) / static_cast<float>(fb.y),
                                   camera_.near_plane(), camera_.far_plane());
        }
        if (input_->was_pressed(platform::Key::ESCAPE)) {
            window_->request_close();
        }
        // ESC pauses. Cursor is released so the pointer is usable, and the
        // world stops ticking because Menu mode skips the whole simulation.
        if (input_->was_pressed(platform::Key::ESCAPE)) {
            if (render_system_.map_open()) {
                render_system_.set_map_open(false);
            } else {
                menu_.open(MenuPage::Pause);
                mode_ = AppMode::Menu;
                input_->set_cursor_captured(false);
                continue;
            }
        }
        if (input_->was_pressed(platform::Key::M)) {
            render_system_.toggle_map();
            // Free the cursor while the map is up: mouse-look under a fullscreen
            // plate spins the world behind it for no reason.
            input_->set_cursor_captured(!render_system_.map_open());
        }

        const auto now = std::chrono::steady_clock::now();
        const double frame_dt = std::chrono::duration<double>(now - last).count();
        last = now;

        // In-game clock (в67): DAY_LENGTH_SECONDS per day, with a debug key that
        // runs it DEBUG_TIME_SCALE faster so shadows can be watched sweeping.
        const double time_scale = input_->is_down(platform::Key::T)
                                      ? static_cast<double>(config::DEBUG_TIME_SCALE)
                                      : 1.0;
        game_seconds_ += frame_dt * time_scale;
        const double day_len = static_cast<double>(config::DAY_LENGTH_SECONDS);
        const double days = game_seconds_ / day_len;
        const float day_fraction = static_cast<float>(days - std::floor(days));
        // The lunar phase is a PURE function of the date — no accumulated state,
        // so the moon is knowable for any past or future day (в69: werewolves,
        // vampires and lunar magic will depend on it).
        const double lunar = days / static_cast<double>(config::LUNAR_MONTH_DAYS);
        const float lunar_phase = static_cast<float>(lunar - std::floor(lunar));
        render::apply_sky_time(render_system_.environment(), day_fraction, lunar_phase);

        // Authored darkness (LANDSCAPE §6.3). The rule has two halves —
        // ENCLOSED (rock actually overhead, so a shaft open to the sky is not
        // dark) and EARNED (>= DARKNESS_DEPTH_MIN walked ALONG the corridor
        // from the nearest mouth, not straight-line through rock). Both live in
        // worldgen, which is why the app asks rather than computes: an app-side
        // approximation redefined "cave" as "low ground" and would have kept
        // the whole switchback tunnel lit, since its portal is 15 m away
        // through stone but 60 m away on foot.
        if (const auto* t = world_.get<components::Transform>(player_)) {
            render_system_.environment().ambient_darkness = chunks_.darkness_at(t->position);
        }

        if (!playtest_) {
            gameplay::player_accumulate_input(world_, *input_); // per render frame (sim's contract)
        }

        // THE WORLD PAUSES WHILE THE INVENTORY IS OPEN (в70: "инвентарь как в
        // скайриме" -- Skyrim, Oblivion and Morrowind all pause, and the pause
        // is what makes a menu with a 3D preview usable at all: you study an
        // object without being hit while you do it).
        //
        // Three of these systems MUST keep running, and sim restructured their
        // zone so that they can. A pause that skipped the whole block would
        // have locked the player in the menu forever, because the key that
        // CLOSES the screen is consumed by player_actions_step -- and the
        // preview turntable lived in the movement path, so "skip movement"
        // would have frozen the one thing on screen. The guard compiles either
        // way; only the restructure makes it correct.
        const bool paused =
            world_.has_resource<gameplay::InventoryScreen>()
            && world_.resource<gameplay::InventoryScreen>().open;

        if (paused) {
            // reset() rather than simply skipping the loop: accumulate() would
            // bank a step per frame while paused and spend the backlog in one
            // burst on unpause, teleporting the player. SIM_MAX_CATCHUP_STEPS
            // bounds that but does not make it right.
            timestep_.reset();
            gameplay::player_actions_step(world_, bus_, *physics_);
            gameplay::update_carried_lights(world_);
            gameplay::update_view_model(world_);
            bus_.pump();
        }

        const uint32_t steps = paused ? 0u : timestep_.accumulate(frame_dt);
        for (uint32_t i = 0; i < steps; ++i) {
            // Streaming runs BEFORE the physics step: a step must never execute
            // against a world whose collision bodies are one tick stale, or the
            // player falls through terrain that has not been created yet.
            glm::vec3 focus{0.0f};
            if (tour_.active()) {
                focus = tour_.focus_position();
            } else if (const auto* t = world_.get<components::Transform>(player_)) {
                focus = t->position;
            }
            chunks_.update(focus, world_, bus_);
            bus_.pump();

            // Prop collision goes AFTER streaming and BEFORE the step, for the
            // same reason the terrain ferry does: chunk residency and the site
            // entities ChunkManager spawns must both exist, and their bodies
            // must be in the world before step() runs. One tick late means the
            // player walks through a house once.
            gameplay::update_prop_collision(world_, *physics_, chunks_);

            if (!tour_.active()) { // frozen player during the tour: deterministic frames
                // The bot steers by writing the same input intents human keys
                // write -- BEFORE pre_step, per sim's playtest contract.
                if (playtest_ && !playtest_->finished) {
                    gameplay::playtest_drive(*playtest_, world_);
                }
                // AFTER the bot (it owns yaw; the probe owns the rest) and
                // BEFORE pre_step, which is where a look intent is consumed.
                body_probe_drive();
                // The water callback is the authoritative source. Sampling the
                // terrain and subtracting, or reading the drawn water, would
                // let a primitive that extends past real water be swum in.
                gameplay::player_pre_step(world_, *physics_,
                    [this](glm::vec2 xz) { return chunks_.water_surface_at(xz); },
                    step_ctx_);
                physics_->step(static_cast<float>(timestep_.step_dt()));
                gameplay::player_post_step(world_, *physics_, step_ctx_);

                // BODY FERRY (character's zone reads, the app writes): sim's
                // stride clock drives the leg clips, so the visual foot-plant
                // and the footstep sound land on the same tick by construction.
                if (auto* drive = world_.get<anim::BodyDrive>(player_)) {
                    if (const auto* ps = world_.get<gameplay::PlayerState>(player_)) {
                        drive->stride_phase = ps->stride_phase;
                        drive->step_length_m = gameplay::step_length(ps->stride_speed);
                        drive->speed_mps = ps->stride_speed;
                        drive->facing_yaw = ps->yaw;
                        drive->grounded = !ps->airborne;
                        drive->vertical_velocity = ps->vertical_velocity;
                        drive->crouch_blend = ps->crouch_blend;
                    }
                }
                anim::update_bodies(world_, body_rig_);

                // Invariant checks AFTER post_step; an incident screenshots.
                if (playtest_ && !playtest_->finished) {
                    if (const size_t n = gameplay::playtest_check(*playtest_, world_, pt_env_);
                        n > 0 && pt_shots_ < 20) {
                        (void)renderer_->save_screenshot(
                            pt_dir_ + "/incident_" + std::to_string(pt_shots_++) + ".png");
                    }
                }

                // Hover AFTER post_step: the crosshair ray must use THIS tick's
                // eye pose. Hovering from last tick's pose acts on what you were
                // looking at a frame ago -- invisible standing still, wrong
                // while turning.
                gameplay::update_hover(world_, *physics_);
                // Actions AFTER the hover they act on: E interact, F light, I bag.
                gameplay::player_actions_step(world_, bus_, *physics_);
                // Carriers without a view model (NPCs with lanterns).
                gameplay::update_carried_lights(world_);
                // LAST: reads the CameraPose post_step wrote and the HeldItem
                // the actions may have just changed, so a torch picked up this
                // tick is in hand this tick rather than next.
                gameplay::update_view_model(world_);
                bus_.pump(); // deliver the interaction events published above
            }
        }

        const float alpha = static_cast<float>(timestep_.alpha());
        const auto* pose = world_.get<components::CameraPose>(player_);
        const auto* prev_pose = world_.get<components::PreviousCameraPose>(player_);
        if (pose != nullptr && prev_pose != nullptr) {
            camera_.set_poses({prev_pose->position, prev_pose->yaw, prev_pose->pitch},
                              {pose->position, pose->yaw, pose->pitch});
            // Speed-coupled FOV (sim writes fov_scale at fixed tick; the app
            // interpolates and applies -- default 1.0 changes nothing).
            const float fs = prev_pose->fov_scale
                           + (pose->fov_scale - prev_pose->fov_scale) * alpha;
            camera_.set_projection(static_cast<float>(config::CAMERA_FOV_Y) * fs,
                                   camera_.aspect_ratio(), camera_.near_plane(),
                                   camera_.far_plane());
        }

        // Audio follows the eye; the wind bed follows the ONE wind model the
        // foliage bends to (Rule 35 -- same gust envelope for ear and eye).
        {
            const auto eye = camera_.interpolated_pose(alpha);
            const float cp = std::cos(eye.pitch);
            const platform::ListenerPose lp{
                eye.position,
                {std::sin(eye.yaw) * cp, std::sin(eye.pitch), -std::cos(eye.yaw) * cp},
                {0.0f, 1.0f, 0.0f}};
            audio_->update(lp);
            gameplay::update_wind_loop(*audio_, wind_loop_,
                                       render_system_.environment().wind_strength);
        }
        if (playtest_ && !playtest_->finished) {
            gameplay::playtest_note_frame(*playtest_, static_cast<float>(frame_dt));
        }
        if (tour_.active()) {
            tour_.apply(camera_);
        }

        // FAR-DETAIL FERRY. Four things here are load-bearing and were paid for
        // in measurements rather than opinion:
        //  - the streamed rect is CHUNK-ALIGNED, not eye +/- radius in metres.
        //    Render's descent tests inside/outside against it, and core measured
        //    an unaligned rect costing 71 nodes where the aligned one costs 46.
        //    It is also a correctness matter, not a saving: a level-0 node is
        //    1 m where a chunk heightfield is 2 m, so without the rect the two
        //    systems draw the same ground twice at slightly different heights.
        //  - update_lod takes the RENDER delta, never SIM_DT. The cross-fade is
        //    a visual effect; at the fixed rate a 0.6 s dissolve steps in 16
        //    chunks and reads as a flicker rather than a fade.
        //  - the ferry collects against lod_pending(), NOT lod_to_load().
        //    to_load names a node exactly once, while core answers several
        //    frames later under its row budget, so a ferry built on to_load
        //    requests nodes it never collects and the ground never appears.
        //  - drop the mesh BEFORE releasing the field it was built from, the
        //    same lifetime rule as ChunkUnloaded.
        if (render_system_.lod_enabled()) {
            const glm::vec3 eye = camera_.interpolated_pose(alpha).position;
            const float cs = static_cast<float>(config::CHUNK_SIZE);
            const float r = static_cast<float>(config::CHUNK_LOAD_RADIUS);
            // Same focus the streaming loop used this frame: the tour drives it
            // during a tour, the player otherwise.
            glm::vec3 lod_focus{0.0f};
            if (tour_.active()) {
                lod_focus = tour_.focus_position();
            } else if (const auto* t = world_.get<components::Transform>(player_)) {
                lod_focus = t->position;
            }
            const glm::vec2 fc{std::floor(lod_focus.x / cs), std::floor(lod_focus.z / cs)};
            render_system_.set_streamed_rect({(fc.x - r) * cs, (fc.y - r) * cs},
                                             {(fc.x + r + 1.0f) * cs,
                                              (fc.y + r + 1.0f) * cs});
            render_system_.update_lod(eye, static_cast<float>(frame_dt));

            const auto to_world = [](const render::LodNode& n) {
                return world::CoarseNode{n.level, n.x, n.z};
            };
            std::vector<world::CoarseNode> wanted;
            wanted.reserve(render_system_.lod_to_load().size());
            for (const auto& n : render_system_.lod_to_load()) {
                wanted.push_back(to_world(n));
            }
            if (!wanted.empty()) {
                chunks_.request_coarse_nodes(wanted);
            }
            for (const auto& n : render_system_.lod_to_release()) {
                render_system_.drop_lod_node(*renderer_, n);
                chunks_.release_coarse_node(to_world(n));
            }
            for (const auto& n : render_system_.lod_pending()) {
                const auto wn = to_world(n);
                if (auto hf = chunks_.coarse_heightfield(wn)) {
                    auto sf = chunks_.coarse_surfacefield(wn);
                    render_system_.upload_lod_node(*renderer_, n, *hf,
                                                   sf ? &*sf : nullptr);
                }
            }
        }

        // INTERACTION PROMPT. The cheapest visible thing in the project: the
        // hover path, the verbs and the keys have all existed for hours and
        // could not draw a pixel without glyphs. Shadow is not decoration --
        // at five pixels tall, unshadowed text vanishes over grass.
        {
            render::PixelCanvas& hud = render_system_.hud();
            hud.clear_transparent();
            bool any = false;
            if (world_.has_resource<components::HoverTarget>()) {
                const auto& hover = world_.resource<components::HoverTarget>();
                if (hover.prompt_key != 0) {
                    const std::string_view text = localized(hover.prompt_key);
                    const int w = static_cast<int>(hud.width());
                    const int h = static_cast<int>(hud.height());
                    render::draw_text(hud, (w - render::text_width_px(text)) / 2,
                                      h - 40, text, render::Color{232, 228, 214},
                                      /*shadow=*/true);
                    any = true;
                }
            }
            // VERIFICATION HOOK (Rule 27, gated): draws a real prompt and a
            // deliberate MISS side by side, so the placeholder is proved to be
            // unmistakable rather than assumed to be.
            if (const char* probe = std::getenv("DFN_HUD_PROBE");
                probe != nullptr && *probe == '1') {
                const int w = static_cast<int>(hud.width());
                const int h = static_cast<int>(hud.height());
                const std::string_view hit = localized(serialization::fnv1a64("prompt.take"));
                const std::string_view miss = localized(serialization::fnv1a64("prompt.nonexistent"));
                render::draw_text(hud, (w - render::text_width_px(hit)) / 2, h - 40,
                                  hit, render::Color{232, 228, 214}, true);
                render::draw_text(hud, (w - render::text_width_px(miss)) / 2, h - 24,
                                  miss, render::Color{232, 228, 214}, true);
                any = true;
            }
            render_system_.set_hud_visible(any);
        }

        render_system_.render(world_, *renderer_, camera_, alpha);
        body_probe_frame(alpha, static_cast<float>(frame_dt));
        if (tour_.active() && tour_.post_frame(*renderer_)) {
            window_->request_close(); // tour finished (render's contract)
        }
        if (playtest_ && playtest_->finished && pt_artifacts_pending_) {
            gameplay::playtest_write_artifacts(*playtest_, pt_dir_);
            pt_artifacts_pending_ = false;
            window_->request_close();
        }
    }
    // Gate: a playtest run with incidents exits nonzero (Main passes it through).
    if (playtest_) {
        return playtest_->incidents.empty() ? 0 : 1;
    }
    return 0;
}

void App::shutdown() {
    if (physics_) {
        for (auto& [key, cp] : g_chunk_physics) {
            physics_->destroy_body(cp.body);
        }
        g_chunk_physics.clear();
    }
    if (renderer_) {
        chunks_.unload_all(world_, bus_);
        bus_.pump();
        render_system_.shutdown(*renderer_);
        renderer_->shutdown();
    }
    if (audio_) {
        audio_->shutdown();
    }
    if (physics_) {
        physics_->shutdown();
    }
    if (window_) {
        window_->shutdown();
    }
}

} // namespace dfn::app
