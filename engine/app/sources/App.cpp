/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 10:48:00
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
*/

#include "engine/app/sources/App.h"

#include "engine/core/components/sources/Components.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/gameplay/sources/PlayerMovement.h" // sim's confirmed stage-2 API
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
#include <cstdlib>
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

AppConfig AppConfig::from_env() {
    AppConfig cfg;
    cfg.internal_width = static_cast<uint32_t>(config::INTERNAL_RES_W);
    cfg.internal_height = static_cast<uint32_t>(config::INTERNAL_RES_H);
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

    // Chunk streaming: stage 2 serves the in-memory generated world (core's
    // open_generated path; .dfw file IO lands in stage 3). Testbed extent 4x4
    // chunks (Q45), fixed seed for reproducible screenshots (Rule 13.1).
    world::ChunkStreamingParams sp;
    sp.load_radius = static_cast<uint32_t>(config::CHUNK_LOAD_RADIUS);
    sp.unload_radius = static_cast<uint32_t>(config::CHUNK_UNLOAD_RADIUS);
    world::WorldGenParams gp;
    gp.seed = 1u;
    gp.min_chunk = {0, 0};
    gp.max_chunk = {3, 3};
    chunks_.open_generated(gp, sp);

    // Subscribe the ferry BEFORE the first update so initial loads are seen.
    bus_.subscribe<world::ChunkLoaded>([this](const world::ChunkLoaded& e) {
        auto view = chunks_.heightfield(e.coord);
        if (!view) {
            return;
        }
        render_system_.upload_terrain(*renderer_, *view);

        ChunkPhysics cp;
        const uint32_t n = view->resolution;
        cp.heights.resize(static_cast<size_t>(n) * n);
        for (uint32_t z = 0; z < n; ++z) {
            for (uint32_t x = 0; x < n; ++x) {
                cp.heights[static_cast<size_t>(z) * n + x] = view->height_at(x, z);
            }
        }
        platform::TerrainDesc td;
        td.origin = {view->origin.x, 0.0f, view->origin.y};
        td.sample_count_x = n;
        td.sample_count_z = n;
        td.sample_spacing = view->step;
        td.heights = cp.heights;
        cp.body = physics_->create_terrain(td);
        g_chunk_physics[pack_coord({e.coord.x, e.coord.z})] = std::move(cp);
    });
    bus_.subscribe<world::ChunkUnloaded>([this](const world::ChunkUnloaded& e) {
        render_system_.drop_terrain(*renderer_, {e.coord.x, e.coord.z});
        auto it = g_chunk_physics.find(pack_coord({e.coord.x, e.coord.z}));
        if (it != g_chunk_physics.end()) {
            physics_->destroy_body(it->second.body);
            g_chunk_physics.erase(it);
        }
    });

    // Spawn at the center of chunk (0,0), on the ground.
    const float mid = static_cast<float>(config::CHUNK_SIZE) * 0.5f;
    chunks_.update({mid, 0.0f, mid}, world_, bus_);
    const float ground = chunks_.height_at({mid, mid}).value_or(0.0f);
    const glm::vec3 spawn{mid, ground + 0.2f, mid};

    player_ = gameplay::spawn_player(world_, *physics_, spawn);
    if (!world_.alive(player_)) {
        return false;
    }

    camera_.set_projection(static_cast<float>(config::CAMERA_FOV_Y),
                           static_cast<float>(rp.framebuffer_width)
                               / static_cast<float>(rp.framebuffer_height),
                           static_cast<float>(config::CAMERA_NEAR),
                           static_cast<float>(config::CAMERA_FAR));

    if (render::Tour::enabled_by_env()) {
        const char* dir = std::getenv("DFN_TOUR_DIR");
        tour_.begin(render::Tour::default_steps(ground), dir ? dir : "screenshots");
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

        const auto now = std::chrono::steady_clock::now();
        const double frame_dt = std::chrono::duration<double>(now - last).count();
        last = now;

        gameplay::player_accumulate_input(world_, *input_); // per render frame (sim's contract)

        const uint32_t steps = timestep_.accumulate(frame_dt);
        for (uint32_t i = 0; i < steps; ++i) {
            gameplay::player_pre_step(world_, *physics_);
            physics_->step(static_cast<float>(timestep_.step_dt()));
            gameplay::player_post_step(world_, *physics_);
            if (const auto* t = world_.get<components::Transform>(player_)) {
                chunks_.update(t->position, world_, bus_);
            }
            bus_.pump();
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
