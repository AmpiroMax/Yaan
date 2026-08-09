/*
Created: 09:08:2026 - 00:45:00
Last updated: 10:08:2026 - 00:04:04
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
*/

#include "engine/app/sources/App.h"

#include "engine/app/sources/Localization.h"

#include "engine/core/components/sources/Components.h"
#include "engine/world/sources/CoarseTerrain.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/physics/sources/TerrainCollision.h"
#include "engine/core/serialization/sources/ContentHash.h"
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
            << "palette=" << (cfg.palette_post ? 1 : 0) << '\n';
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

    if (!render_system_.init(*renderer_)) {
        return false;
    }
    // The map canvas rasterizes in internal-resolution pixels, so it must know
    // the settings.cfg-driven resolution to stay pixel-exact (render's note).
    render_system_.set_internal_resolution(config.internal_width, config.internal_height);

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

    camera_.set_projection(static_cast<float>(config::CAMERA_FOV_Y),
                           static_cast<float>(rp.framebuffer_width)
                               / static_cast<float>(rp.framebuffer_height),
                           static_cast<float>(config::CAMERA_NEAR),
                           static_cast<float>(config::CAMERA_FAR));

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

    // Rule 5: every user-facing string comes from here and nowhere else.
    // A missing file is loud and the game still runs, with every string drawn
    // as a visible placeholder rather than as nothing.
    (void)load_localization("games/daggerfall_n/assets/localization/ru.txt");

    if (render::Tour::enabled_by_env()) {
        const char* dir = std::getenv("DFN_TOUR_DIR");
        tour_.begin(render::Tour::testbed_steps(), dir ? dir : "screenshots",
                    [this](glm::vec2 p) { return chunks_.height_at(p).value_or(0.0f); });
    } else {
        input_->set_cursor_captured(true);
    }
    return true;
}

int App::run() {
    auto last = std::chrono::steady_clock::now();
    while (!window_->should_close()) {
        window_->poll_events();
        input_->update();
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

        gameplay::player_accumulate_input(world_, *input_); // per render frame (sim's contract)

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
                // The water callback is the authoritative source. Sampling the
                // terrain and subtracting, or reading the drawn water, would
                // let a primitive that extends past real water be swum in.
                gameplay::player_pre_step(world_, *physics_,
                    [this](glm::vec2 xz) { return chunks_.water_surface_at(xz); });
                physics_->step(static_cast<float>(timestep_.step_dt()));
                gameplay::player_post_step(world_, *physics_);

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
        if (tour_.active() && tour_.post_frame(*renderer_)) {
            window_->request_close(); // tour finished (render's contract)
        }
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
    if (physics_) {
        physics_->shutdown();
    }
    if (window_) {
        window_->shutdown();
    }
}

} // namespace dfn::app
